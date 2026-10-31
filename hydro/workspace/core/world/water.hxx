#pragma once

#include <workspace/core/world/wave.hxx>
#include <workspace/utility/logger.hxx>

#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

// Ocean surface, reversed from VuWaterOceanWave in HydroThunder.exe. The
// functions below are named in the IDB:
//
//   VuWaterOceanWave_Update               0x59ccb0
//   VuWaterOceanWave_BuildSpectrum        0x59d220
//   VuWaterOceanWave_EvolveField          0x59d4d0
//   VuWaterOceanWave_BuildBezierPatches   0x59d6c0
//   VuWaterInfiniteOceanWave_SampleHeight 0x59e9e0
//   Vu_GaussianDeviate                    0x50fd90
//
// Grid size is 1 << Complexity. The spectrum is Phillips with A = 0.0081 and
// L = wind^2 / gravity; h0 is a gaussian pair scaled by sqrt(S * 0.5), drawn
// from ran1. Only half the field is generated - the original's inner loop runs
// to n/2 and leans on hermitian symmetry for a real result. The field evolves
// as h0 * e^(i*omega*t) with the phase folded into [-pi, pi]. Sampling scales
// the query point by n / Bin Siz, rotates it by Wave Direction - pi, and reads
// the surface with a bicubic central-difference patch.
class c_ocean
{
public:
    // One live VuPointWaveEntity emission. Declared here rather than beside
    // wave_t because surge() takes it as a parameter, and a nested type used
    // in a member function's SIGNATURE must be declared earlier in the class.
    struct ripple_t
    {
        float x = 0.0f;
        float y = 0.0f;
        float falloff = 5.0f;       // VuPointWaveEntity_ctor @0x5d43b0
        float magnitude = 1.0f;
        float range_start = 0.0f;
        float range_end = 20.0f;
        float speed = 10.0f;
        float frequency = 5.0f;
        float foaminess = 1.0f;
        float age = 0.0f;
        // The four members VuWaterPointWave_ctor @0x5a3340 DERIVES rather than
        // copies, all reciprocals of 1.0, stored at wave +124/+128/+132/+136.
        // They are computed once at emission exactly as retail computes them,
        // not recomputed at each sample: 1.0/x is what retail stores, and a
        // division written the other way round is a different number.
        //
        // Carrying them also removes the need for the guards that used to
        // stand in surge(). A Speed of zero makes `rate` infinite here, and
        // the kernel's own front-arrival test then rejects every radius - the
        // same way retail's does, with no clamp of ours in the way.
        float rate = 0.0f;            // 1.0 / Speed
        float amplitude_scale = 0.0f; // 1.0 / (Range End - Range Start)
        float slope = 0.0f;           // 1.0 / Falloff Time
        float inverse_magnitude = 0.0f; // 1.0 / Magnitude, unread by the kernel
    };

    // `direction_rad` is RADIANS, as the entity stores it. Wave Direction is a
    // VuAngleProperty, so retail converts the authored degrees once on the way
    // into the entity and every reader downstream sees radians. This used to
    // take degrees and convert here, which put the conversion in the wrong
    // place - harmless while one call site fed it, and a double conversion the
    // moment the property came through the schema binder.
    void add( float complexity, float bin, float direction_rad, float wind, float power, float suppression, float height, float gravity, const float* center, const float* extent, float decay_x, float decay_y, bool detail = false )
    {
        wave_t wave{};
        // VuWaterOceanWave_Update: n = 1 << Complexity, with no clamp in the
        // original. Kept a ceiling here only so a bad asset cannot ask for a
        // field that will not fit; 1 << 5 = 32 is what the levels author.
        int size = 1 << static_cast< int >( complexity );
        if ( size < 8 )
            size = 8;
        if ( size > 128 )
            size = 128;
        wave.n = size;
        wave.bin = bin > 1.0f ? bin : 100.0f;
        wave.wind = wind;
        wave.power = power;
        wave.suppression = suppression;
        wave.height_scale = height;
        // VuInfiniteOceanWaveEntity_ctor @0x5d8380 stores 0x411CE560 = 9.806,
        // not the textbook 9.80665 guessed here before the ctor was read. The
        // "> 0.1" fallback is ours: retail cannot reach a zero gravity because
        // the constructor always seeds one, but an authored zero would divide
        // by zero here.
        wave.gravity = gravity > 0.1f ? gravity : 9.806f;
        // Update stores Wave Direction - pi and caches its sine and cosine.
        wave.angle = direction_rad - 3.14159265f;
        wave.sine = std::sin( wave.angle );
        wave.cosine = std::cos( wave.angle );
        wave.wind_x = -wave.sine;
        wave.wind_y = -wave.cosine;
        wave.infinite = center == nullptr;
        wave.detail = detail;
        if ( center != nullptr )
        {
            wave.origin[ 0 ] = center[ 0 ];
            wave.origin[ 1 ] = center[ 1 ];
            wave.extent[ 0 ] = extent != nullptr ? extent[ 0 ] : 1.0f;
            wave.extent[ 1 ] = extent != nullptr ? extent[ 1 ] : 1.0f;
            wave.decay[ 0 ] = decay_x;
            wave.decay[ 1 ] = decay_y;
        }
        build( wave );
        m_waves.push_back( std::move( wave ) );
        LOG_INFO( "ocean n=%d bin=%.0f height=%.4f gravity=%.5f wind=%.2f", size, bin, height, wave.gravity, wind );
    }

    // VuPointWaveEntity's "Emit" input plug, transliterated from
    // VuPointWaveEntity_tick @0x5d4c50 and VuWaterPointWave_ctor @0x5a3340.
    // Retail allocates a runtime wave per emission and releases it on expiry;
    // a vector entry with an age is the same lifetime without the allocator.
    void emit( float x, float y, float falloff, float magnitude, float range_start,
        float range_end, float speed, float frequency, float foaminess )
    {
        ripple_t ripple{};
        ripple.x = x;
        ripple.y = y;
        ripple.falloff = falloff;
        ripple.magnitude = magnitude;
        ripple.range_start = range_start;
        ripple.range_end = range_end;
        ripple.speed = speed;
        ripple.frequency = frequency;
        ripple.foaminess = foaminess;
        // VuWaterPointWave_ctor's four derived members, in its order.
        ripple.rate = 1.0f / speed;
        ripple.amplitude_scale = 1.0f / ( range_end - range_start );
        ripple.slope = 1.0f / falloff;
        ripple.inverse_magnitude = 1.0f / magnitude;
        m_ripples.push_back( ripple );
    }

    void clear( )
    {
        m_waves.clear( );
        m_ripples.clear( );
        m_logged = false;
        m_time = 0.0f;
    }

    void advance( float dt )
    {
        if ( dt < 0.0f )
            dt = 0.0f;
        m_time += dt;
        // CORRECTED against VuWaterPointWave's tick @0x5a34b0, which computes
        // its lifetime as
        //
        //     limit = ( this[ +124 ] * this[ +104 ] ) + this[ +92 ]
        //           = ( 1.0 / Speed ) * Range End + Falloff Time
        //
        // What stood here used (Range End - Range Start) / Speed, subtracting
        // Range Start on the reasoning that the front starts there and only
        // the span has to be crossed. RETAIL DOES NOT SUBTRACT IT - the
        // lifetime is Range End over Speed, full stop - so that term was an
        // invention, and a ripple whose Range Start is non-zero was dying
        // early by exactly Range Start / Speed.
        for ( std::size_t index = 0; index < m_ripples.size( ); )
        {
            ripple_t& ripple = m_ripples[ index ];
            ripple.age += dt;
            const float limit = ( ripple.rate * ripple.range_end ) + ripple.falloff;
            if ( ripple.age > limit )
                m_ripples.erase( m_ripples.begin( ) + static_cast< std::ptrdiff_t >( index ) );
            else
                ++index;
        }
        for ( wave_t& wave : m_waves )
            evolve( wave );
        if ( !m_logged && !m_waves.empty( ) )
        {
            for ( const wave_t& wave : m_waves )
            {
                float peak = 0.0f;
                for ( float value : wave.displacement )
                    peak = std::fabs( value ) > peak ? std::fabs( value ) : peak;
                LOG_INFO( "ocean peak %.3f detail %d", peak, wave.detail ? 1 : 0 );
            }
            m_logged = true;
        }
    }

    void normals( std::vector< unsigned char >& rgba, float& tile ) const
    {
        const wave_t* wave = nullptr;
        for ( const wave_t& item : m_waves )
        {
            if ( item.detail )
            {
                wave = &item;
                break;
            }
        }
        if ( wave == nullptr )
        {
            for ( const wave_t& item : m_waves )
            {
                if ( item.infinite )
                {
                    wave = &item;
                    break;
                }
            }
        }
        if ( wave == nullptr || wave->displacement.empty( ) )
            return;
        tile = wave->bin;
        const int n = wave->n;
        rgba.resize( static_cast< std::size_t >( n * n * 4 ) );
        const float step = wave->bin / static_cast< float >( n );
        for ( int iy = 0; iy < n; ++iy )
        {
            const int y0 = ( iy + n - 1 ) % n;
            const int y1 = ( iy + 1 ) % n;
            for ( int ix = 0; ix < n; ++ix )
            {
                const int x0 = ( ix + n - 1 ) % n;
                const int x1 = ( ix + 1 ) % n;
                const float dx = ( wave->displacement[ static_cast< std::size_t >( iy * n + x1 ) ] - wave->displacement[ static_cast< std::size_t >( iy * n + x0 ) ] ) / ( 2.0f * step );
                const float dy = ( wave->displacement[ static_cast< std::size_t >( y1 * n + ix ) ] - wave->displacement[ static_cast< std::size_t >( y0 * n + ix ) ] ) / ( 2.0f * step );
                float nx = -dx;
                float ny = -dy;
                float nz = 1.0f;
                const float length = std::sqrt( nx * nx + ny * ny + nz * nz );
                nx /= length;
                ny /= length;
                nz /= length;
                const std::size_t pixel = static_cast< std::size_t >( iy * n + ix ) * 4;
                rgba[ pixel ] = static_cast< unsigned char >( ( nx * 0.5f + 0.5f ) * 255.0f );
                rgba[ pixel + 1 ] = static_cast< unsigned char >( ( ny * 0.5f + 0.5f ) * 255.0f );
                rgba[ pixel + 2 ] = static_cast< unsigned char >( ( nz * 0.5f + 0.5f ) * 255.0f );
                rgba[ pixel + 3 ] = 255;
            }
        }
    }

    float height( float x, float y ) const
    {
        float sum = 0.0f;
        for ( const wave_t& wave : m_waves )
        {
            if ( !wave.detail )
                sum += sample( wave, x, y );
        }
        for ( const ripple_t& ripple : m_ripples )
            sum += surge( ripple, x, y );
        return sum;
    }

    // The foam channel VuWaterPointWave_accumulate writes alongside height.
    // Nothing consumes it yet - the water shader's foam comes from its own
    // texture - but it is computed here so the equation stays whole rather
    // than being half-transliterated.
    float froth( float x, float y ) const
    {
        float sum = 0.0f;
        for ( const ripple_t& ripple : m_ripples )
        {
            const float span = ripple.range_end - ripple.range_start;
            if ( span <= 0.0f || ripple.magnitude == 0.0f )
                continue;
            const float dx = x - ripple.x;
            const float dy = y - ripple.y;
            const float distance = std::sqrt( dx * dx + dy * dy );
            if ( distance >= ripple.range_end )
                continue;
            const float reach = distance - ripple.range_start;
            const float depth = reach > 0.0f ? reach : 0.0f;
            const float amplitude = ( span - depth ) * ripple.magnitude / span;
            if ( amplitude <= 0.0f )
                continue;
            // amplitude / Magnitude cancels the amplitude scale back out, so
            // foam does NOT grow with a louder ripple - see
            // VuWaterPointWave_accumulate @0x5a3510, this[34] * v82.
            const float taper = 1.0f - ( ripple.age - ripple.falloff ) * ripple.speed / span;
            sum += ( amplitude / ripple.magnitude ) * ripple.foaminess
                 * ( taper < 1.0f ? taper : 1.0f );
        }
        return sum;
    }

private:
    // ran1: Park-Miller minimal standard behind a 32 entry Bays-Durham
    // shuffle. BuildSpectrum writes it as 16807*s - 2147483647*(s/127773),
    // which is Schrage's trick in one line because 16807*127773 + 2836 is
    // exactly 2147483647. Seeded at -1, warmed for 40 iterations: the first
    // eight draws are discarded and the last 32 fill the shuffle table. The
    // seed is fixed, so every level gets the same field for a given spectrum.
    class random_t
    {
    public:
        random_t( )
        {
            m_seed = -1;
            for ( int index = 39; index >= 0; --index )
            {
                m_seed = step( m_seed );
                if ( index < 32 )
                    m_table[ index ] = m_seed;
            }
            m_last = m_table[ 0 ];
        }

        float unit( )
        {
            m_seed = step( m_seed );
            const int slot = static_cast< int >( m_last / divisor );
            m_last = m_table[ slot & 31 ];
            m_table[ slot & 31 ] = m_seed;
            const float value = static_cast< float >( m_last ) * ( 1.0f / 2147483647.0f );
            return value > 0.99999994f ? 0.99999994f : value;
        }

        // Vu_GaussianDeviate: the polar form, which draws a pair and hands
        // back one, so the cached half must survive to the next call or the
        // sequence walks out of step with the original.
        float gauss( )
        {
            if ( m_have )
            {
                m_have = false;
                return m_spare;
            }
            float u = 0.0f;
            float v = 0.0f;
            float radius = 0.0f;
            do
            {
                u = unit( ) * 2.0f - 1.0f;
                v = unit( ) * 2.0f - 1.0f;
                radius = u * u + v * v;
            }
            while ( radius >= 1.0f || radius == 0.0f );
            const float factor = std::sqrt( -2.0f * std::log( radius ) / radius );
            m_spare = u * factor;
            m_have = true;
            return v * factor;
        }

    private:
        static std::int32_t step( std::int32_t seed )
        {
            const std::int32_t k = seed / 127773;
            seed = 16807 * ( seed - k * 127773 ) - 2836 * k;
            if ( seed < 0 )
                seed += 2147483647;
            return seed;
        }

        static const std::int32_t divisor = 1 + 2147483646 / 32;

        std::int32_t m_seed = -1;
        std::int32_t m_last = 0;
        std::int32_t m_table[ 32 ]{};
        float m_spare = 0.0f;
        bool m_have = false;
    };

    struct wave_t
    {
        int n = 0;
        float bin = 100.0f;
        float wind = 5.0f;
        float wind_x = 0.0f;
        float wind_y = -1.0f;
        float angle = 0.0f;
        float sine = 0.0f;
        float cosine = 1.0f;
        float power = 3.0f;
        float suppression = 0.5f;
        float height_scale = 0.1f;
        float gravity = 9.806f;
        bool infinite = true;
        bool detail = false;
        float origin[ 2 ]{};
        float extent[ 2 ]{ 1.0f, 1.0f };
        // VuRectangularOceanWaveEntity_ctor @0x5d8ad0 defaults both to 0.9.
        float decay[ 2 ]{ 0.9f, 0.9f };
        std::vector< std::complex< float > > h0;
        std::vector< float > omega;
        std::vector< float > displacement;
        std::vector< std::complex< float > > field;
        std::vector< std::complex< float > > line;
    };

    // BuildSpectrum. k is centred, (i - n/2) * 2pi / Bin Siz, not the FFT
    // wrap-around layout, and the inner loop only covers n/2 because the
    // conjugate half is implied.
    static void build( wave_t& wave )
    {
        const int n = wave.n;
        wave.h0.assign( static_cast< std::size_t >( n * n ), {} );
        wave.omega.assign( static_cast< std::size_t >( n * n ), 0.0f );
        wave.displacement.assign( static_cast< std::size_t >( n * n ), 0.0f );
        random_t random{};
        const float scale = 6.2831855f / wave.bin;
        const float phillips_l = ( wave.wind * wave.wind ) / wave.gravity;
        const int half = n / 2;
        for ( int iy = 0; iy < n; ++iy )
        {
            const float ky = static_cast< float >( iy - half ) * scale;
            for ( int ix = 0; ix < half; ++ix )
            {
                const float kx = static_cast< float >( ix - half ) * scale;
                const float k2 = kx * kx + ky * ky;
                float amplitude = 0.0f;
                if ( k2 > 0.0f && phillips_l > 0.0f )
                {
                    const float klen = std::sqrt( k2 );
                    const float align = std::fabs( ( kx * wave.wind_x + ky * wave.wind_y ) / klen );
                    float spectrum = 0.0081f * std::exp( -1.0f / ( k2 * phillips_l * phillips_l ) ) / ( k2 * k2 );
                    spectrum *= std::pow( align, wave.power );
                    spectrum *= std::exp( -k2 * wave.suppression * wave.suppression );
                    amplitude = std::sqrt( std::fabs( spectrum ) * 0.5f );
                    wave.omega[ static_cast< std::size_t >( iy * n + ix ) ] = std::sqrt( wave.gravity * klen );
                }
                const float re = random.gauss( ) * amplitude;
                const float im = random.gauss( ) * amplitude;
                const std::size_t index = static_cast< std::size_t >( iy * n + ix );
                wave.h0[ index ] = { re, im };
                // mirror into the conjugate half so the row transform below
                // produces a real surface
                const int mx = ( n - ix ) & ( n - 1 );
                const int my = ( n - iy ) & ( n - 1 );
                const std::size_t opposite = static_cast< std::size_t >( my * n + mx );
                if ( opposite != index )
                {
                    wave.h0[ opposite ] = { re, -im };
                    wave.omega[ opposite ] = wave.omega[ index ];
                }
            }
        }
    }

    static void cis( float radians, float& sine, float& cosine )
    {
        static float table[ 1024 ]{};
        static bool ready = false;
        if ( !ready )
        {
            for ( int index = 0; index < 1024; ++index )
                table[ index ] = std::sin( index * 6.2831853f / 1024.0f );
            ready = true;
        }
        float turns = radians * 0.15915494f;
        turns -= static_cast< float >( static_cast< int >( turns ) );
        if ( turns < 0.0f )
            turns += 1.0f;
        const int index = static_cast< int >( turns * 1024.0f ) & 1023;
        sine = table[ index ];
        cosine = table[ ( index + 256 ) & 1023 ];
    }

    static void fft( std::complex< float >* values, int n )
    {
        for ( int i = 1, j = 0; i < n; ++i )
        {
            int bit = n >> 1;
            for ( ; j & bit; bit >>= 1 )
                j ^= bit;
            j ^= bit;
            if ( i < j )
                std::swap( values[ i ], values[ j ] );
        }
        for ( int length = 2; length <= n; length <<= 1 )
        {
            const float angle = 6.2831853f / static_cast< float >( length );
            float step_s = 0.0f;
            float step_c = 0.0f;
            cis( angle, step_s, step_c );
            for ( int start = 0; start < n; start += length )
            {
                float turn_c = 1.0f;
                float turn_s = 0.0f;
                const int half = length / 2;
                for ( int offset = 0; offset < half; ++offset )
                {
                    std::complex< float >& left = values[ start + offset ];
                    std::complex< float >& right = values[ start + offset + half ];
                    const float right_re = right.real( ) * turn_c - right.imag( ) * turn_s;
                    const float right_im = right.real( ) * turn_s + right.imag( ) * turn_c;
                    const float left_re = left.real( );
                    const float left_im = left.imag( );
                    left = { left_re + right_re, left_im + right_im };
                    right = { left_re - right_re, left_im - right_im };
                    const float next_c = turn_c * step_c - turn_s * step_s;
                    turn_s = turn_c * step_s + turn_s * step_c;
                    turn_c = next_c;
                }
            }
        }
    }

    // EvolveField rotates h0 by the phase directly - there is no conjugate
    // pair sum - and folds omega*t into [-pi, pi] before the sine and cosine.
    void evolve( wave_t& wave ) const
    {
        const int n = wave.n;
        if ( wave.field.size( ) != wave.h0.size( ) )
            wave.field.resize( wave.h0.size( ) );
        std::complex< float >* field = wave.field.data( );
        const std::complex< float >* h0 = wave.h0.data( );
        const float* omega = wave.omega.data( );
        for ( int index = 0; index < n * n; ++index )
        {
            float sine = 0.0f;
            float cosine = 0.0f;
            cis( omega[ index ] * m_time, sine, cosine );
            const float re = h0[ index ].real( );
            const float im = h0[ index ].imag( );
            field[ index ] = { re * cosine - im * sine, re * sine + im * cosine };
        }
        for ( int iy = 0; iy < n; ++iy )
            fft( field + iy * n, n );
        if ( static_cast< int >( wave.line.size( ) ) != n )
            wave.line.resize( static_cast< std::size_t >( n ) );
        std::complex< float >* line = wave.line.data( );
        // Height Multiplier is parameter +104, which BuildSpectrum never
        // reads, so it lands on the field output. The 1/n^2 is the inverse
        // transform normalisation this unnormalised FFT still owes; the
        // original folds it into its own transform.
        const float gain = wave.height_scale / static_cast< float >( n * n );
        for ( int ix = 0; ix < n; ++ix )
        {
            for ( int iy = 0; iy < n; ++iy )
                line[ iy ] = field[ iy * n + ix ];
            fft( line, n );
            for ( int iy = 0; iy < n; ++iy )
                wave.displacement[ static_cast< std::size_t >( iy * n + ix ) ] = line[ iy ].real( ) * gain;
        }
    }

    // One transient ripple. This used to be a hand-written transliteration of
    // VuWaterPointWave_accumulate; it now DEFERS to point_wave_vertex in
    // wave.hxx, which is checked against retail's kernel on eleven oracle
    // vectors at a worst deviation of 0.000e+00.
    //
    // TWO THINGS THE HAND-WRITTEN VERSION HAD WRONG, both now gone:
    //
    //   IT CONTRIBUTED AHEAD OF THE WAVEFRONT. Retail has TWO rejections -
    //   `clock <= scaled` when the front has not yet reached this radius, and
    //   `edge <= clock` once the tail has passed. Only the second was here, so
    //   a radius the ring had not arrived at still contributed, with a
    //   negative phase and a decay above 1.
    //
    //   IT GUARDED AGAINST ZERO Speed, Falloff and span. Retail has no such
    //   tests, because the constructor's reciprocals make them unnecessary: a
    //   Speed of zero gives an infinite rate, and the front-arrival test then
    //   rejects every radius on its own. The guards were invented and they
    //   masked that mechanism.
    //
    // The kernel writes a height and a displacement; only the height is
    // summed here, because that is all this caller consumes - the
    // displacement is a NAMED GAP, not a silent one.
    static float surge( const ripple_t& ripple, float x, float y )
    {
        point_wave_t wave{};
        wave.centre_x = ripple.x;
        wave.centre_y = ripple.y;
        wave.tail_width = ripple.falloff;
        wave.amplitude = ripple.magnitude;
        wave.inner_radius = ripple.range_start;
        wave.outer_radius = ripple.range_end;
        wave.phase_scale = ripple.frequency;
        wave.clock = ripple.age;
        wave.rate = ripple.rate;
        wave.amplitude_scale = ripple.amplitude_scale;
        wave.slope = ripple.slope;

        float displacement[ 3 ]{};
        float height = 0.0f;
        point_wave_vertex( wave, x, y, displacement, height );
        return height;
    }

    // The region taper, checked against VuWaterRectangularOceanWave_accumulate
    // @0x59f840. Retail computes u = (2 / width) * dx, rejects the sample when
    // |u| or |v| reaches 1, and ramps with (|u| - 1) / (decay - 1). Dividing by
    // the half extent is the same normalisation, and (|u| - 1) / (decay - 1) is
    // (1 - |u|) / (1 - decay) with both signs flipped - so the form below is
    // identical, not merely similar.
    static float window( const wave_t& wave, float x, float y )
    {
        if ( wave.infinite )
            return 1.0f;
        const float half_x = wave.extent[ 0 ] * 0.5f;
        const float half_y = wave.extent[ 1 ] * 0.5f;
        // Retail divides by the extent with no minimum-size test; an earlier
        // guard here rejected anything under a metre, which would silently
        // delete a small authored region. Only the division by zero is kept.
        if ( half_x <= 0.0f || half_y <= 0.0f )
            return 0.0f;
        const float ax = std::fabs( ( x - wave.origin[ 0 ] ) / half_x );
        const float ay = std::fabs( ( y - wave.origin[ 1 ] ) / half_y );
        if ( ax >= 1.0f || ay >= 1.0f )
            return 0.0f;
        const float fall_x = ax <= wave.decay[ 0 ] ? 1.0f : ( 1.0f - ax ) / ( 1.0f - wave.decay[ 0 ] );
        const float fall_y = ay <= wave.decay[ 1 ] ? 1.0f : ( 1.0f - ay ) / ( 1.0f - wave.decay[ 1 ] );
        return fall_x * fall_y;
    }

    // BuildBezierPatches lays down 16 control points per cell where each
    // inner point is the corner plus a sixth of a central difference. That is
    // the hermite-to-bezier conversion of a central-difference tangent, so the
    // surface is a bicubic catmull-rom patch. Evaluating the basis directly
    // gives the same surface without keeping 64 bytes per cell resident.
    static float spline( float a, float b, float c, float d, float t )
    {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return b
            + 0.5f * t * ( c - a )
            + 0.5f * t2 * ( 2.0f * a - 5.0f * b + 4.0f * c - d )
            + 0.5f * t3 * ( 3.0f * b - 3.0f * c + d - a );
    }

    // SampleHeight scales the query point by n / Bin Siz and rotates it by the
    // stored Wave Direction - pi before reading the surface.
    static float sample( const wave_t& wave, float x, float y )
    {
        if ( wave.displacement.empty( ) )
            return 0.0f;
        const float weight = window( wave, x, y );
        if ( weight <= 0.0f )
            return 0.0f;
        const int n = wave.n;
        const float scale = static_cast< float >( n ) / wave.bin;
        const float sx = x * scale;
        const float sy = y * scale;
        const float rx = wave.cosine * sx + wave.sine * sy;
        const float ry = wave.cosine * sy - wave.sine * sx;
        const float fx = std::floor( rx );
        const float fy = std::floor( ry );
        const int x0 = static_cast< int >( fx );
        const int y0 = static_cast< int >( fy );
        const float tx = rx - fx;
        const float ty = ry - fy;
        const int mask = n - 1;
        float rows[ 4 ]{};
        for ( int row = 0; row < 4; ++row )
        {
            const int iy = ( y0 + row - 1 ) & mask;
            const float* line = &wave.displacement[ static_cast< std::size_t >( iy * n ) ];
            rows[ row ] = spline(
                line[ ( x0 - 1 ) & mask ],
                line[ x0 & mask ],
                line[ ( x0 + 1 ) & mask ],
                line[ ( x0 + 2 ) & mask ],
                tx );
        }
        return spline( rows[ 0 ], rows[ 1 ], rows[ 2 ], rows[ 3 ], ty ) * weight;
    }

    std::vector< wave_t > m_waves;
    std::vector< ripple_t > m_ripples;
    float m_time = 0.0f;
    bool m_logged = false;
};
