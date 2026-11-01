#pragma once

#include <workspace/core/world/wave.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// THE OCEAN FIELD: what the rectangular / circular ocean waves' constructor and
// tick build for their kernels (wave.hxx's ocean_patch_vertex / _vertex96) to
// read - transliterated from the disassembly, function by function:
//
//   VuWaterOceanWave_Update        0x59CCB0  the descriptor -> the whole field
//     0x59CE70  the arrays           0x59D010  the cell table
//     0x59D170  the dispersion       VuWaterOceanWave_BuildSpectrum 0x59D220
//       Vu_GaussianDeviate 0x50FD90 on a ran1 state ON BuildSpectrum's STACK
//     VuWaterOceanWave_EvolveField 0x59D4D0
//     0x608BB0  (IDB name VuWaterMap_BuildBezierPatches) = Numerical Recipes'
//               rlft3, inverse, nn1 = 1 folded in; it ends in fourn 0x608880
//     0x59D640  the heights          VuWaterOceanWave_BuildBezierPatches 0x59D6C0
//   the ocean tick 0x59CC00 runs EvolveField .. BuildBezierPatches again on the
//   new current buffer (wave.hxx's ocean_tick is its clock half).
//
// Wave words read (+0x4C .. +0x68, the descriptor's flat copy): Complexity,
// Bin Siz, Wave Direction, Gravity, Wind Speed, Directional Power,
// Suppression Wave Length, Height Multiplier. The arrays, as retail holds them:
//   +0x90 the dispersion (n * n/2 floats)   +0x94 h0 (n * n/2 complex)
//   +0x98 the rlft3 data, a 1-indexed f3tensor [1][1..n][1..n]
//   +0x9C speq, [1][1..2n]                  +0xA0 the cell table (12 ints / cell)
//   buffer k: +0xB8 + 16k the heights (n*n), +0xBC + 16k the patches (16 / cell)
// Here the data is flat row-major (D[ (i2-1)*n + (i3-1) ] = data[1][i2][i3] -
// retail's rows are contiguous, 0x608F40) and speq flat (S[ j-1 ] = speq[1][j]).
//
// SUBSTITUTIONS, named: the CRT's __libm_sse2_powf / expf / logf / sin are
// imports from the platform CRT (not in the image); they are computed here as
// the CRT's DOUBLE function narrowed to float (the oracle harness does the
// same - it is what measures them). The x87 fsincos of EvolveField is wave.hxx's
// wave_x87_sin / cos (host double under Unicorn; real hardware's fsincos is not
// measured and could differ in a rare last bit).

struct ocean_params_t
{
    std::uint32_t complexity = 0;   // +0x4C
    float bin = 0.0f;               // +0x50 Bin Siz
    float direction = 0.0f;         // +0x54
    float gravity = 0.0f;           // +0x58
    float wind = 0.0f;              // +0x5C
    float power = 0.0f;             // +0x60 Directional Power
    float suppression = 0.0f;       // +0x64
    float height = 0.0f;            // +0x68 Height Multiplier
};

inline ocean_params_t ocean_params_of( const std::uint32_t* word )   // word[ k ] = wave + 4k
{
    ocean_params_t p{};
    p.complexity = word[ 0x4C / 4 ];
    p.bin = wave_bits( word[ 0x50 / 4 ] );
    p.direction = wave_bits( word[ 0x54 / 4 ] );
    p.gravity = wave_bits( word[ 0x58 / 4 ] );
    p.wind = wave_bits( word[ 0x5C / 4 ] );
    p.power = wave_bits( word[ 0x60 / 4 ] );
    p.suppression = wave_bits( word[ 0x64 / 4 ] );
    p.height = wave_bits( word[ 0x68 / 4 ] );
    return p;
}

struct ocean_field_t
{
    int shift = 0;   // +0x6C
    int n = 0;       // +0x70 (0 here = never allocated: complexity 0)
    int mask = 0;    // +0x74
    std::vector< float > omega;
    std::vector< float > h0;
    std::vector< float > data;
    std::vector< float > speq;
    std::vector< std::int32_t > cells;
    std::vector< float > heights[ 2 ];
    std::vector< float > patches[ 2 ];
};

// --- the CRT, as the oracle measures it ------------------------------------
inline float ocean_crt_powf( float x, float y ) { return static_cast< float >( std::pow( static_cast< double >( x ), static_cast< double >( y ) ) ); }
inline float ocean_crt_expf( float x ) { return static_cast< float >( std::exp( static_cast< double >( x ) ) ); }
inline float ocean_crt_logf( float x ) { return static_cast< float >( std::log( static_cast< double >( x ) ) ); }
inline float ocean_sqrt( float x ) { return _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( x ) ) ); }   // VuMath_sqrt 0x40B5A0
inline float ocean_min( float a, float b ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); }   // VuMath_min 0x403260

// --- ran1 -------------------------------------------------------------------
// The state BuildSpectrum keeps at ebp-0x98 and hands to 0x50FD90 in ecx:
// +0 the seed, +4 iy, +8 the 32-entry shuffle table, +0x88 the spare flag
// (BuildSpectrum zeroes it), +0x8C the spare. Fresh on EVERY BuildSpectrum.
struct ocean_rng_t
{
    std::int32_t seed = 0;
    std::int32_t iy = 0;
    std::int32_t table[ 32 ]{};
    bool have = false;
    float spare = 0.0f;
};

// 16807 * s - 0x7FFFFFFF * ( s / 127773 ) in 32-bit wrapping arithmetic (the
// imul 0x834E0B5F / sar 16 idiom is C's truncating s / 127773), then
// + 0x7FFFFFFF when negative.
inline std::int32_t ocean_ran1_step( std::int32_t s )
{
    const std::int32_t q = s / 127773;
    const std::uint32_t r = static_cast< std::uint32_t >( s ) * 16807u - static_cast< std::uint32_t >( q ) * 0x7FFFFFFFu;
    std::int32_t v = static_cast< std::int32_t >( r );
    if ( v < 0 )
        v = v + 0x7FFFFFFF;
    return v;
}

// BuildSpectrum's prologue 0x59D234 .. 0x59D29D: s = -1 (NOT negated), 40
// steps, step c = 39 .. 0 stored at table[ c ] once c < 32; iy = table[ 0 ].
inline void ocean_rng_seed( ocean_rng_t& r )
{
    std::int32_t s = -1;
    for ( int c = 39; c >= 0; --c )
    {
        s = ocean_ran1_step( s );
        if ( c < 32 )
            r.table[ c ] = s;
    }
    r.seed = s;
    r.iy = r.table[ 0 ];
    r.have = false;
}

// One ran1 draw as 0x50FD90 inlines it twice: j = iy / 2^26, iy = table[ j ],
// table[ j ] = the new seed; min( iy * 2^-31, 0.99999988 ).
inline float ocean_ran1( ocean_rng_t& r )
{
    r.seed = ocean_ran1_step( r.seed );
    const int j = r.iy / 67108864;
    r.iy = r.table[ j ];
    r.table[ j ] = r.seed;
    return ocean_min( static_cast< float >( r.iy ) * 4.656613e-10f, 0.99999988f );
}

// Vu_GaussianDeviate 0x50FD90 (thiscall state): the polar form with a cached
// spare. v1 drawn first, v2 second; rsq = v2*v2 + v1*v1, redrawn while
// rsq >= 1 or rsq == 0; fac = sqrt( logf( rsq ) * -2 / rsq ); the spare is
// fac * v1, the answer fac * v2.
inline float ocean_gaussian( ocean_rng_t& r )
{
    if ( r.have )
    {
        r.have = false;
        return r.spare;
    }
    float v1;
    float v2;
    float rsq;
    do
    {
        v1 = ( ocean_ran1( r ) * 2.0f ) - 1.0f;
        v2 = ( ocean_ran1( r ) * 2.0f ) - 1.0f;
        rsq = ( v2 * v2 ) + ( v1 * v1 );
    } while ( rsq >= 1.0f || rsq == 0.0f );
    const float fac = ocean_sqrt( ( ocean_crt_logf( rsq ) * -2.0f ) / rsq );
    r.spare = fac * v1;
    r.have = true;
    return fac * v2;
}

// --- the arrays, and the two tables built once ------------------------------
// 0x59CE70: +0x90 (n/2)*n floats, +0x94 (n/2)*n*2, the map 0x608F40 (n rows
// of n), speq 2n + 1 (element 0 unused), the cells n*n*12, the two buffers;
// then 0x59CCB0 zeroes speq[ 1 .. 2n ]. operator new / malloc leave the rest
// unwritten: every element the field reads is written before it is read.
inline void ocean_field_alloc( ocean_field_t& f, std::uint32_t complexity )
{
    f.shift = static_cast< int >( complexity );
    f.n = 1 << f.shift;
    f.mask = f.n - 1;
    const std::size_t n = static_cast< std::size_t >( f.n );
    f.omega.assign( ( n / 2 ) * n, 0.0f );
    f.h0.assign( ( n / 2 ) * n * 2, 0.0f );
    f.data.assign( n * n, 0.0f );
    f.speq.assign( 2 * n, 0.0f );
    f.cells.assign( n * n * 12, 0 );
    for ( int k = 0; k < 2; ++k )
    {
        f.heights[ k ].assign( n * n, 0.0f );
        f.patches[ k ].assign( n * n * 16, 0.0f );
    }
}

// 0x59D010: for a = 0 .. n-1, b = 0 .. n-1 the entry at (b << shift) + a holds
// the twelve height indices its patch reads (a1 = a+1, am = a-1, a2 = a+2 and
// the same on b, each & mask).
inline void ocean_field_cells( ocean_field_t& f )
{
    const int s = f.shift;
    const int m = f.mask;
    for ( int a = 0; a < f.n; ++a )
    {
        const int a1 = ( a + 1 ) & m;
        const int am = ( a - 1 ) & m;
        const int a2 = ( a + 2 ) & m;
        for ( int b = 0; b < f.n; ++b )
        {
            const int b1 = ( b + 1 ) & m;
            const int bm = ( b - 1 ) & m;
            const int b2 = ( b + 2 ) & m;
            std::int32_t* e = &f.cells[ static_cast< std::size_t >( ( ( b << s ) + a ) * 12 ) ];
            e[ 0 ] = ( a << s ) + b;
            e[ 1 ] = ( a1 << s ) + b;
            e[ 2 ] = ( a1 << s ) + b1;
            e[ 3 ] = ( a << s ) + b1;
            e[ 4 ] = ( a << s ) + bm;
            e[ 5 ] = ( a1 << s ) + bm;
            e[ 6 ] = ( a2 << s ) + b;
            e[ 7 ] = ( a2 << s ) + b1;
            e[ 8 ] = ( a1 << s ) + b2;
            e[ 9 ] = ( a << s ) + b2;
            e[ 10 ] = ( am << s ) + b1;
            e[ 11 ] = ( am << s ) + b;
        }
    }
}

// 0x59D170: omega = sqrt( sqrt( kj*kj + ki*ki ) * gravity ), k = (index - n/2)
// * (2pi / Bin Siz), i over n, j over n/2.
inline void ocean_field_omega( ocean_field_t& f, const ocean_params_t& p )
{
    const int half = f.n / 2;
    std::size_t at = 0;
    for ( int i = 0; i < f.n; ++i )
        for ( int j = 0; j < half; ++j )
        {
            const float step = 6.2831855f / p.bin;
            const float ki = static_cast< float >( i - half ) * step;
            const float kj = static_cast< float >( j - half ) * step;
            f.omega[ at++ ] = ocean_sqrt( ocean_sqrt( ( kj * kj ) + ( ki * ki ) ) * p.gravity );
        }
}

// VuWaterOceanWave_BuildSpectrum 0x59D220: L = wind*wind / gravity; per (i, j)
// in the same order, P = 0 unless L > 0 and k2 = ky*ky + kx*kx > 0, then
//   P = ( |powf( (kx/k)*0 - (ky/k)*1, power )| * ( expf( -1 / ((k2*L)*L) )
//         * 0.0081 / (k2*k2) ) ) * expf( -((sup*k2)*sup) )
// and h0 = ( gauss * sqrt( P*0.5 ), gauss * sqrt( P*0.5 ) ), real first.
inline void ocean_build_spectrum( ocean_field_t& f, const ocean_params_t& p )
{
    ocean_rng_t r;
    ocean_rng_seed( r );
    const float L = ( p.wind * p.wind ) / p.gravity;
    const int half = f.n / 2;
    std::size_t at = 0;
    for ( int i = 0; i < f.n; ++i )
        for ( int j = 0; j < half; ++j )
        {
            const float step = 6.2831855f / p.bin;
            const float kx = static_cast< float >( i - half ) * step;
            const float ky = static_cast< float >( j - half ) * step;
            float P = 0.0f;
            if ( ocean_gt( L, 0.0f ) )
            {
                const float k2 = ( ky * ky ) + ( kx * kx );
                if ( ocean_gt( k2, 0.0f ) )
                {
                    const float inv = 1.0f / ocean_sqrt( k2 );
                    const float dot = ( ( inv * kx ) * 0.0f ) - ( ( inv * ky ) * 1.0f );
                    const float d = std::fabs( ocean_crt_powf( dot, p.power ) );
                    const float ph = ( ocean_crt_expf( -1.0f / ( ( k2 * L ) * L ) ) * 0.0081f ) / ( k2 * k2 );
                    const float a = d * ph;
                    P = a * ocean_crt_expf( -( ( p.suppression * k2 ) * p.suppression ) );
                }
            }
            const float amp = ocean_sqrt( P * 0.5f );
            f.h0[ at++ ] = ocean_gaussian( r ) * amp;
            f.h0[ at++ ] = ocean_gaussian( r ) * amp;
        }
}

// --- the per-tick rebuild ---------------------------------------------------
// VuWaterOceanWave_EvolveField 0x59D4D0: phase = (float)( (double)omega * time )
// + pi, folded by retail's truncation ( |x| - (int)(|x| / 2pi) * 2pi - pi,
// negated when x < 0 ), x87 fsincos; data[ i ][ 2j, 2j+1 ] = h0 * e^(i phase);
// speq[ 2i+1, 2i+2 ] = 0.
inline void ocean_evolve( ocean_field_t& f, double time )
{
    const int half = f.n / 2;
    std::size_t at = 0;
    for ( int i = 0; i < f.n; ++i )
    {
        float* row = &f.data[ static_cast< std::size_t >( i * f.n ) ];
        for ( int j = 0; j < half; ++j, ++at )
        {
            const float x = static_cast< float >( static_cast< double >( f.omega[ at ] ) * time ) + 3.1415927f;
            const float a = std::fabs( x );
            float w = ( a - static_cast< float >( static_cast< int >( a * 0.15915494f ) ) * 6.2831855f ) - 3.1415927f;
            if ( ocean_gt( 0.0f, x ) )   // unreached for omega >= 0, time >= 0 (perturbation: removing it moves nothing)
                w = -w;
            const float c = wave_x87_cos( w );
            const float s = wave_x87_sin( w );
            const float re = f.h0[ 2 * at ];
            const float im = f.h0[ 2 * at + 1 ];
            row[ 2 * j ] = ( re * c ) - ( s * im );
            row[ 2 * j + 1 ] = ( re * s ) + ( c * im );
        }
        f.speq[ static_cast< std::size_t >( 2 * i ) ] = 0.0f;
        f.speq[ static_cast< std::size_t >( 2 * i + 1 ) ] = 0.0f;
    }
}

// fourn 0x608880 (usercall data @ecx 1-indexed, nn @edx 1-indexed; ndim 3 and
// isign -1 folded in), as the disassembly runs it: the bit reversal, then the
// Danielson-Lanczos passes with the trigonometric recurrence in DOUBLE and the
// butterfly in FLOAT on (float) wr / wi.
inline void ocean_fourn( float* data /* data[ 1 .. ] */, const std::uint32_t* nn /* nn[ 1 .. 3 ] */ )
{
    const std::uint32_t ntot = nn[ 2 ] * nn[ 3 ] * nn[ 1 ];
    std::uint32_t nprev = 1;
    for ( int idim = 3; idim >= 1; --idim )
    {
        const std::uint32_t n = nn[ idim ];
        const std::uint32_t ip1 = nprev * 2;
        const std::uint32_t ip2 = ip1 * n;
        const std::uint32_t next_prev = n * nprev;
        const std::uint32_t nrem = ntot / next_prev;
        const std::uint32_t ip3 = nrem * ip2;
        std::uint32_t i2rev = 1;
        for ( std::uint32_t i2 = 1; i2 <= ip2; i2 += ip1 )
        {
            if ( i2 < i2rev )
            {
                for ( std::uint32_t i1 = i2; i1 <= ip1 - 2 + i2; i1 += 2 )
                    for ( std::uint32_t i3 = i1; i3 <= ip3; i3 += ip2 )
                    {
                        const std::uint32_t i3rev = i2rev + i3 - i2;
                        float t = data[ i3 ];
                        data[ i3 ] = data[ i3rev ];
                        data[ i3rev ] = t;
                        t = data[ i3 + 1 ];
                        data[ i3 + 1 ] = data[ i3rev + 1 ];
                        data[ i3rev + 1 ] = t;
                    }
            }
            std::uint32_t ibit = ip2 >> 1;
            if ( ibit >= ip1 )
            {
                while ( i2rev > ibit )
                {
                    i2rev -= ibit;
                    ibit >>= 1;
                    if ( ibit < ip1 )
                        break;
                }
            }
            i2rev += ibit;
        }
        std::uint32_t ifp1 = ip1;
        while ( ifp1 < ip2 )
        {
            const std::uint32_t ifp2 = ifp1 * 2;
            const double theta = -6.28318530717959 / static_cast< double >( ifp2 / ip1 );
            const double wtemp = std::sin( theta * 0.5 );
            const double wpr = ( wtemp * -2.0 ) * wtemp;
            const double wpi = std::sin( theta );
            double wr = 1.0;
            double wi = 0.0;
            for ( std::uint32_t i3 = 1; i3 <= ifp1; i3 += ip1 )
            {
                for ( std::uint32_t i1 = i3; i1 <= ip1 - 2 + i3; i1 += 2 )
                {
                    const float wif = static_cast< float >( wi );
                    const float wrf = static_cast< float >( wr );
                    for ( std::uint32_t k1 = i1; k1 <= ip3; k1 += ifp2 )
                    {
                        const std::uint32_t k2 = k1 + ifp1;
                        const float tempr = ( wrf * data[ k2 ] ) - ( wif * data[ k2 + 1 ] );
                        const float tempi = ( wrf * data[ k2 + 1 ] ) + ( wif * data[ k2 ] );
                        data[ k2 ] = data[ k1 ] - tempr;
                        data[ k2 + 1 ] = data[ k1 + 1 ] - tempi;
                        data[ k1 ] = tempr + data[ k1 ];
                        data[ k1 + 1 ] = data[ k1 + 1 ] + tempi;
                    }
                }
                const double t = ( wr * wpr ) - ( wi * wpi );
                const double u = wpi * wr;
                wr = wr + t;
                wi = wi + ( ( wi * wpr ) + u );
            }
            ifp1 = ifp2;
        }
        nprev = next_prev;
    }
}

// 0x608BB0 = rlft3( data, speq, 1, n, n, -1 ): theta = (2pi / n) * -1 (double),
// c1 = 0.5, c2 = 0.5; for i3 = 1 .. n/4 + 1, i2 = 1 .. n the NR unpacking with
// the i3 == 1 column against speq in float, the rest blended through wr / wi in
// DOUBLE (cvtps2pd ... cvtpd2ps per store); then fourn( data, {1, n, n/2} ).
inline void ocean_rlft3( ocean_field_t& f )
{
    const std::uint32_t nn2 = static_cast< std::uint32_t >( f.n );
    const std::uint32_t nn3 = static_cast< std::uint32_t >( f.n );
    // 1-indexed views: A( i2, k ) = data[1][i2][k], S( k ) = speq[1][k]
    auto A = [ & ]( std::uint32_t i2, std::uint32_t k ) -> float& { return f.data[ ( i2 - 1 ) * nn3 + ( k - 1 ) ]; };
    auto S = [ & ]( std::uint32_t k ) -> float& { return f.speq[ k - 1 ]; };
    const double theta = ( 6.28318530717959 / static_cast< double >( nn3 ) ) * -1.0;
    const double wtemp = std::sin( theta * 0.5 );
    const double wpr = ( wtemp * -2.0 ) * wtemp;
    const double wpi = std::sin( theta );
    double wr = 1.0;
    double wi = 0.0;
    std::uint32_t ii3 = 1;
    for ( std::uint32_t i3 = 1; i3 <= ( nn3 >> 2 ) + 1; ++i3, ii3 += 2 )
    {
        for ( std::uint32_t i2 = 1; i2 <= nn2; ++i2 )
        {
            if ( i3 == 1 )
            {
                const std::uint32_t j2 = i2 != 1 ? ( ( nn2 - i2 ) * 2 + 3 ) : 1;
                const float a1 = A( i2, 1 );
                const float s1 = S( j2 );
                const float s2 = S( j2 + 1 );
                const float h1r = ( s1 + a1 ) * 0.5f;
                const float h2i = ( a1 - s1 ) * 0.5f;
                const float a2 = A( i2, 2 );
                const float h1i = ( a2 - s2 ) * 0.5f;
                const float h2r = ( s2 + a2 ) * -0.5f;
                A( i2, 1 ) = h2r + h1r;
                A( i2, 2 ) = h2i + h1i;
                S( j2 ) = h1r - h2r;
                S( j2 + 1 ) = h2i - h1i;
            }
            else
            {
                const std::uint32_t j2 = i2 != 1 ? ( nn2 - i2 + 2 ) : 1;
                const std::uint32_t j3 = nn3 + 3 - 2 * i3;
                const float b1 = A( j2, j3 + 1 );
                const float b0 = A( j2, j3 );
                const float a1 = A( i2, ii3 + 1 );
                const float a0 = A( i2, ii3 );
                const float h1i = ( a1 - b1 ) * 0.5f;
                const float h2r = ( b1 + a1 ) * -0.5f;
                const float h2i = ( a0 - b0 ) * 0.5f;
                const float h1r = ( b0 + a0 ) * 0.5f;
                const double t4 = static_cast< double >( h2r ) * wr;
                const double t3 = static_cast< double >( h2i ) * wi;
                const double t2 = static_cast< double >( h2i ) * wr;
                const double t1 = static_cast< double >( h2r ) * wi;
                A( i2, ii3 ) = static_cast< float >( ( static_cast< double >( h1r ) + t4 ) - t3 );
                A( i2, ii3 + 1 ) = static_cast< float >( ( static_cast< double >( h1i ) + t2 ) + t1 );
                A( j2, j3 ) = static_cast< float >( ( static_cast< double >( h1r ) - t4 ) + t3 );
                A( j2, j3 + 1 ) = static_cast< float >( ( t2 - static_cast< double >( h1i ) ) + t1 );
            }
        }
        const double t = ( wr * wpr ) - ( wi * wpi );
        const double u = wpi * wr;
        wr = wr + t;
        wi = wi + ( ( wi * wpr ) + u );
    }
    const std::uint32_t nn[ 4 ] = { 0, 1, nn2, nn3 >> 1 };
    ocean_fourn( f.data.data( ) - 1, nn );
}

// 0x59D640: heights[ cur ][ i*n + j ] = ( data[ i ][ j ] * sign ) * Height
// Multiplier, sign starting at 1 and negated after every element and again
// after every row - the (-1)^(i+j) the centred spectrum needs.
inline void ocean_heights( ocean_field_t& f, const ocean_params_t& p, int current )
{
    float sign = 1.0f;
    float* out = f.heights[ current ].data( );
    for ( int i = 0; i < f.n; ++i )
    {
        const float* row = &f.data[ static_cast< std::size_t >( i * f.n ) ];
        for ( int j = 0; j < f.n; ++j )
        {
            *out++ = ( row[ j ] * sign ) * p.height;
            sign = sign * -1.0f;
        }
        sign = sign * -1.0f;
    }
}

// VuWaterOceanWave_BuildBezierPatches 0x59D6C0: per cell entry in table order,
// sixteen control points from twelve heights h[ e[ k ] ] and central
// differences scaled by 1/6 - stored in the order retail stores them.
inline void ocean_patches( ocean_field_t& f, int current )
{
    const float c = 0.16666667f;
    const float* h = f.heights[ current ].data( );
    float* P = f.patches[ current ].data( );
    const std::size_t cells = static_cast< std::size_t >( f.n ) * static_cast< std::size_t >( f.n );
    for ( std::size_t k = 0; k < cells; ++k, P += 16 )
    {
        const std::int32_t* e = &f.cells[ k * 12 ];
        const float h0 = h[ e[ 0 ] ];
        const float h1 = h[ e[ 1 ] ];
        const float h2 = h[ e[ 2 ] ];
        const float h3 = h[ e[ 3 ] ];
        float d2 = ( h[ e[ 11 ] ] - h1 ) * c;
        float d1 = ( h[ e[ 4 ] ] - h3 ) * c;
        P[ 0 ] = h0;
        P[ 11 ] = h0 - d1;
        P[ 4 ] = h0 - d2;
        P[ 12 ] = ( h0 - d2 ) - d1;
        d2 = ( h0 - h[ e[ 6 ] ] ) * c;
        d1 = ( h[ e[ 5 ] ] - h2 ) * c;
        P[ 1 ] = h1;
        P[ 5 ] = h1 + d2;
        P[ 6 ] = h1 - d1;
        P[ 13 ] = ( h1 - d1 ) + d2;
        d2 = ( h3 - h[ e[ 7 ] ] ) * c;
        d1 = ( h1 - h[ e[ 8 ] ] ) * c;
        P[ 2 ] = h2;
        P[ 7 ] = h2 + d1;
        P[ 8 ] = h2 + d2;
        P[ 14 ] = ( h2 + d2 ) + d1;
        d2 = ( h[ e[ 10 ] ] - h2 ) * c;
        d1 = ( h0 - h[ e[ 9 ] ] ) * c;
        P[ 3 ] = h3;
        P[ 9 ] = h3 - d2;
        P[ 10 ] = d1 + h3;
        P[ 15 ] = ( d1 + h3 ) - d2;
    }
}

// The four calls the ocean tick 0x59CC00 makes after advancing its clock (and
// that 0x59CCB0 makes once): EvolveField at the wave's time (+0x88, already
// advanced), rlft3, the heights and the patches into buffer `current`.
inline void ocean_field_rebuild( ocean_field_t& f, const ocean_params_t& p, double time, int current )
{
    ocean_evolve( f, time );
    ocean_rlft3( f );
    ocean_heights( f, p, current );
    ocean_patches( f, current );
}

// VuWaterOceanWave_Update 0x59CCB0's field half, as the constructor 0x59CB10
// runs it (complexity was the ctor's 0, so a non-zero one allocates): the
// cells, the dispersion, the spectrum, the rebuild into buffer 0 at `time`,
// then buffer 1 = a copy of buffer 0 (memcpy_s of the heights and patches).
// The clock half (+0x88, +0xA4 / +0xA8 = 0 / 1, the buffer times, buffer 1's
// time - 1.0) is wave.hxx's ocean_construct. Complexity 0: nothing allocated,
// and retail then writes 0x59D010's table through a null pointer - not
// reproduced; the field stays n = 0 (UNKNOWN to the kernels).
inline void ocean_field_update( ocean_field_t& f, const ocean_params_t& p, double time )
{
    if ( p.complexity == 0 )
    {
        f = ocean_field_t{};
        return;
    }
    if ( f.n == 0 || static_cast< std::uint32_t >( f.shift ) != p.complexity )
        ocean_field_alloc( f, p.complexity );
    ocean_field_cells( f );
    ocean_field_omega( f, p );
    ocean_build_spectrum( f, p );
    ocean_field_rebuild( f, p, time, 0 );
    f.heights[ 1 ] = f.heights[ 0 ];
    f.patches[ 1 ] = f.patches[ 0 ];
}
