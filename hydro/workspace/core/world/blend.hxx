#pragma once

#include <workspace/core/boat/hull.hxx>
#include <workspace/core/world/transform.hxx>

#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// THE GFX-SETTINGS BLENDER (world batch 17): the static at 0x759430 the Build
// tick's step 1 (0x41F330 -> 0x52F690) and the Viewports tick's dev path
// read - +0 the 22-word settings record (the VuGfxSettingsEntity +0x70 block:
// +0 Camera Far Plane, +4 Clear Color, +8 / +0xC Fog Start / End, +0x10 Fog
// Color, +0x14 Contrast, +0x18 Tint, +0x1C .. +0x24 Gamma Min / Max / Curve,
// +0x28 .. +0x34 the four water colours, +0x38 Water Foam Texture Size, +0x3C /
// +0x40 the decal colours, +0x44 / +0x48 the depth foam value / intensity,
// +0x4C / +0x50 the pfx colours, +0x54 Foliage Tint), +0x58 / +0x5C / +0x60 the
// zone list (data, count, capacity). Recreated from Hex-Rays + the disassembly;
// verified by the oracle case gfx_blend (retail's 0x52F690 over zone lists
// with the real Rectangular / Elliptical vtables).

// Which record words are colours (4 bytes, each x 1/255 when accumulated).
inline bool gfx_word_is_colour( int k )
{
    return k == 1 || k == 4 || k == 5 || k == 6 || k == 10 || k == 11 || k == 12 || k == 13 || k == 15 || k == 16 || k == 19 || k == 20 ||
           k == 21;
}

inline float gfx_f( std::uint32_t w )
{
    float f;
    std::memcpy( &f, &w, 4 );
    return f;
}

inline std::uint32_t gfx_w( float f )
{
    std::uint32_t w;
    std::memcpy( &w, &f, 4 );
    return w;
}

// 0x41B400 (thiscall record): the default record.
inline void gfx_record_default( std::uint32_t* r )
{
    static const std::uint32_t k[ 22 ] = { 0x43FA0000u, 0xFF000000u, 0x7F7FFFFFu, 0x7F7FFFFFu, 0xFF000000u, 0xFF000000u, 0xFFFFFFFFu, 0x0u,
        0x3F800000u, 0x3F800000u, 0xC0423C2Du, 0xC05A503Cu, 0xFF808080u, 0xFFFFFFFFu, 0x41600000u, 0xFF808080u, 0xFFFFFFFFu, 0x3F000000u,
        0x3F000000u, 0xFFC0C0C0u, 0xFFFFFFFFu, 0xFFFFFFFFu };
    std::memcpy( r, k, sizeof( k ) );
}

// A zone (VuRectangularGfxSettingsEntity / VuEllipticalGfxSettingsEntity) as
// the blend reads it: its vt slot 12 (the weight), +0x50 its transform
// component, +0x60 Always Active, +0x64 / +0x68 / +0x6C Ramp Up Time /
// Duration / Ramp Down Time, +0x70 its record, +0xC8 the timer, +0xD0 / +0xD4
// the Inner Radius (X / Y) %.
enum class gfx_zone_kind_t
{
    rectangular,   // vtable 0x752E74, slot 12 0x5D2380
    elliptical,    // vtable 0x752F4C, slot 12 0x5D2850
};

struct gfx_zone_t
{
    gfx_zone_kind_t kind = gfx_zone_kind_t::rectangular;
    const transform_component_t* transform = nullptr;
    std::uint8_t always_active = 0;
    float ramp_up = 0.0f, duration = 0.0f, ramp_down = 0.0f;
    std::uint32_t record[ 22 ] = { };
    float timer = 0.0f;
    float inner[ 2 ] = { 0.5f, 0.5f };
};

struct gfx_blender_t
{
    std::uint32_t record[ 22 ] = { };
    std::vector< gfx_zone_t* > zones;
};

// 0x52F620 (the blender's init): the default record, an empty list (the
// 8-slot aligned array is allocator bookkeeping).
inline void gfx_blender_init( gfx_blender_t& b )
{
    gfx_record_default( b.record );
    b.zones.clear( );
}

// The point in the zone's frame: d = p - origin (+0x80); x = ((+0x54 dy +
// +0x50 dx) + +0x58 dz) / scale x (+0xB0), y the same over +0x60 row / +0xB4.
inline void gfx_zone_local( const gfx_zone_t& z, const float* p, float& x, float& y )
{
    const float* m = z.transform->world;
    const float dx = p[ 0 ] - m[ 12 ], dy = p[ 1 ] - m[ 13 ], dz = p[ 2 ] - m[ 14 ];
    x = ( ( m[ 1 ] * dy + m[ 0 ] * dx ) + m[ 2 ] * dz ) / z.transform->scale[ 0 ];
    y = ( ( m[ 5 ] * dy + m[ 4 ] * dx ) + m[ 6 ] * dz ) / z.transform->scale[ 1 ];
}

// 0x5D2380 (rectangular): ax = |x|, ay = |y| (VuVector_abs: andps); maxss(ax,
// ay) > 1 -> 0; w = 1, ax > inner x -> w = (ax - 1) / (inner x - 1); ay >
// inner y -> w = ((ay - 1) / (inner y - 1)) x w.
// 0x5D2850 (elliptical): d = y^2 + x^2; d >= 1 -> 0; s = VuMath_sqrt(d); inner
// > s -> 1, else (s - 1) / (inner - 1).
inline float gfx_zone_weight( const gfx_zone_t& z, const float* p )
{
    float x, y;
    gfx_zone_local( z, p, x, y );
    if ( z.kind == gfx_zone_kind_t::rectangular )
    {
        const float ax = _mm_cvtss_f32( _mm_andnot_ps( _mm_set_ss( -0.0f ), _mm_set_ss( x ) ) );
        const float ay = _mm_cvtss_f32( _mm_andnot_ps( _mm_set_ss( -0.0f ), _mm_set_ss( y ) ) );
        const float mx = _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( ax ), _mm_set_ss( ay ) ) );
        if ( _mm_comigt_ss( _mm_set_ss( mx ), _mm_set_ss( 1.0f ) ) )
            return 0.0f;
        float w = 1.0f;
        if ( _mm_comigt_ss( _mm_set_ss( ax ), _mm_set_ss( z.inner[ 0 ] ) ) )
            w = ( ax - 1.0f ) / ( z.inner[ 0 ] - 1.0f );
        if ( _mm_comigt_ss( _mm_set_ss( ay ), _mm_set_ss( z.inner[ 1 ] ) ) )
            w = ( ( ay - 1.0f ) / ( z.inner[ 1 ] - 1.0f ) ) * w;
        return w;
    }
    const float d = y * y + x * x;
    if ( !_mm_comilt_ss( _mm_set_ss( d ), _mm_set_ss( 1.0f ) ) )   // comiss d, 1; jb: on only when d < 1
        return 0.0f;
    const float s = hull_sqrt( d );
    if ( _mm_comigt_ss( _mm_set_ss( z.inner[ 0 ] ), _mm_set_ss( s ) ) )
        return 1.0f;
    return ( s - 1.0f ) / ( z.inner[ 0 ] - 1.0f );
}

// 0x52F7E0 (__usercall ecx accumulator, xmm2 weight; record* on the stack):
// acc[0] += w; each float word f: (f x w) + acc; each colour byte b: acc + ((b x
// 1/255 (0x3B808081)) x w) - the 61 lanes in record order.
inline void gfx_accumulate( float* acc, const std::uint32_t* r, float w )
{
    acc[ 0 ] = acc[ 0 ] + w;
    int at = 1;
    for ( int k = 0; k < 22; ++k )
    {
        if ( !gfx_word_is_colour( k ) )
        {
            acc[ at ] = gfx_f( r[ k ] ) * w + acc[ at ];
            ++at;
            continue;
        }
        for ( int c = 0; c < 4; ++c, ++at )
        {
            const float b = static_cast< float >( static_cast< std::int32_t >( ( r[ k ] >> ( 8 * c ) ) & 0xFFu ) );
            acc[ at ] = acc[ at ] + ( b * gfx_f( 0x3B808081u ) ) * w;
        }
    }
}

// 0x52FFE0 (thiscall accumulator): every lane x (1 / acc[0]) (the reciprocal
// recomputed per group from the unchanged acc[0] - the same value), acc[0] = 1.
inline void gfx_normalize( float* acc )
{
    const float v = 1.0f / acc[ 0 ];
    acc[ 1 ] = v * acc[ 1 ];
    for ( int k = 2; k < 62; ++k )
        acc[ k ] = acc[ k ] * v;
    acc[ 0 ] = 1.0f;
}

// 0x530450 (thiscall accumulator; record* out; retn 4): each float lane's
// word copied; each colour byte = the low byte of fistp(lane x 255.0f
// (0x6F7808)) - round to nearest, the integer indefinite 0x80000000 out of
// range (cvtss2si under the default MXCSR answers both alike).
inline void gfx_pack( const float* acc, std::uint32_t* out )
{
    int at = 1;
    for ( int k = 0; k < 22; ++k )
    {
        if ( !gfx_word_is_colour( k ) )
        {
            out[ k ] = gfx_w( acc[ at++ ] );
            continue;
        }
        std::uint32_t v = 0;
        for ( int c = 0; c < 4; ++c, ++at )
        {
            const std::int32_t i = _mm_cvtss_si32( _mm_set_ss( acc[ at ] * gfx_f( 0x437F0000u ) ) );
            v |= ( static_cast< std::uint32_t >( i ) & 0xFFu ) << ( 8 * c );
        }
        out[ k ] = v;
    }
}

// 0x52F690 (thiscall blender; position*, out*; retn 8): the 0xF8-byte
// accumulator zeroed; per zone (list order) w = its slot 12 (x87, a float);
// w > 0: f = 1 with Always Active, else with the timer t > 0: t <= ramp down
// -> t / ramp down, t <= duration + ramp down -> 1, else 1 - (((t - duration)
// - ramp down) / ramp up); t <= 0 -> 0; f x w > 0 -> 0x52F7E0(its record, f x
// w). Then acc[0] < 1 -> 0x52F7E0(the blender's record, 1 - acc[0]), else
// 0x52FFE0; 0x530450 into out.
inline void gfx_blend( const gfx_blender_t& b, const float* position, std::uint32_t* out )
{
    float acc[ 62 ] = { };
    auto gt = []( float a, float c ) { return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( c ) ) != 0; };
    for ( const gfx_zone_t* z : b.zones )
    {
        const float w = gfx_zone_weight( *z, position );
        if ( !gt( w, 0.0f ) )
            continue;
        float f;
        if ( z->always_active )
            f = 1.0f;
        else
        {
            const float t = z->timer;
            auto ge = []( float a, float c ) { return _mm_comige_ss( _mm_set_ss( a ), _mm_set_ss( c ) ) != 0; };   // comiss a, c; jb
            if ( ge( 0.0f, t ) )
                f = 0.0f;
            else if ( ge( z->ramp_down, t ) )
                f = t / z->ramp_down;
            else if ( ge( z->duration + z->ramp_down, t ) )
                f = 1.0f;
            else
                f = 1.0f - ( ( ( t - z->duration ) - z->ramp_down ) / z->ramp_up );
        }
        const float k = f * w;
        if ( gt( k, 0.0f ) )
            gfx_accumulate( acc, z->record, k );
    }
    if ( gt( 1.0f, acc[ 0 ] ) )
        gfx_accumulate( acc, b.record, 1.0f - acc[ 0 ] );
    else
        gfx_normalize( acc );
    gfx_pack( acc, out );
}

// The zones' "Build" tick 0x5A7210 (registered by gameInitialize when not
// Always Active): timer -= dt.
inline void gfx_zone_tick( gfx_zone_t& z, float dt )
{
    z.timer = z.timer - dt;
}

// VuGfxSettingsEntity's "Trigger" input (slot 13 0x5A7230): with the timer
// <= 0 (comiss 0, t; jb) the timer = (duration + ramp up) + ramp down.
inline void gfx_zone_trigger( gfx_zone_t& z )
{
    if ( _mm_comige_ss( _mm_set_ss( 0.0f ), _mm_set_ss( z.timer ) ) )
        z.timer = ( z.duration + z.ramp_up ) + z.ramp_down;
}

// VuGlobalGfxSettingsEntity's "Trigger" (slot 13 0x5D2980): its record (+0x70)
// copied into the blender's.
inline void gfx_global_trigger( gfx_blender_t& b, const std::uint32_t* record )
{
    std::memcpy( b.record, record, sizeof( b.record ) );
}
