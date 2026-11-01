#pragma once

#include <workspace/core/world/field.hxx>

#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// THE WATER RENDERER'S WAVE TEXTURE (the IDB calls 0x6080D0 VuWaterMap_BuildSpectrum; no RTTI - a plain member
// object the water renderer's ctor 0x594DD0 builds with 0x607720): a 64 x 64 FFT ocean, rebuilt on the render
// thread and turned into a two-channel slope map with a seven-level mip chain - the texture the water shader reads.
// Transliterated from the disassembly:
//   0x607720  the ctor: the params and their copy, the arrays, format 3 (8 when 0x5ADDA0( "Metro", 3 ) answers
//             false - a render-device capability query: HOOK, the caller passes the format), the two textures
//             (0x57C9B0: render, HOOK), the "Water/Texture/..." dev-config floats (g 0x759014 vt+0x20 / vt+0x18:
//             DEV, they only ever change the live params), the "Build" tick 0x607D00 (g 0x759008 vt+0x20) and the
//             render-thread hook 0x607D20 (g 0x759024 vt+0x1C), then 0x608010.
//   0x608010  = 0x608030 (the dispersion: field.hxx ocean_field_omega with n = 64 on the COPY's Gravity / World
//             Size) + 0x6080D0 (BuildSpectrum: field.hxx ocean_build_spectrum with n = 64 on the copy's Gravity,
//             World Size (as Bin Siz), Wind Speed, Directional Power, Suppression Wave Length - census/vdiff.py:
//             the same body as 0x59D220 with n fixed; the oracle case confirms it bit for bit).
//   0x607D00  the "Build" tick: time (double, +0x70) += (double)( Time Factor (the LIVE +0x1C) * dt ).
//   0x607D20  the render-thread hook: when g 0x759058's +0x46 byte is set, a gfx-sort command whose callback
//             0x607EF0 runs 0x607F10 (and, with the dev "Show" byte +0x68, a debug quad 0x507360) - RENDER / DEV,
//             the caller decides when water_texture_build runs.
//   0x607F10  the build: current = ( current + 1 ) % 2; when the 8 live params differ (dword compare) from the copy,
//             the copy is refreshed and 0x608030 + 0x6080D0 rerun; then EvolveField 0x608370 (field.hxx
//             ocean_evolve, n = 64), rlft3 0x608BB0 (ocean_rlft3, 64 x 64), the heights 0x6084B0 (ocean_heights
//             with the copy's Height Factor), the slopes 0x6085C0, mip 0 uploaded (texture[ current ] vt+8( 0,
//             mip0, 0x2000 ): HOOK), then levels 1 .. 6 by 0x608730, each uploaded.
// Layout (offsets from the object): +0 format, +4 .. +0x20 the live params (Gravity 9.81, World Size 10, Wind Speed
// 10, Directional Power 0, Suppression Wave Length 0, Height Factor 1, Time Factor 1, +0x20 0.5 (no dev var, never
// read here)), +0x24 .. +0x40 their copy, +0x44 / +0x48 the textures, +0x4C current, +0x50 omega (64 x 32), +0x54 h0
// (64 x 32 complex), +0x58 the rlft3 tensor (rows of 64 floats, contiguous from its +4), +0x5C speq (129 floats,
// [1 .. 128] zeroed by the ctor), +0x60 the heights (64 x 64), +0x64 the mip table (7 byte arrays, (64 >> l)^2 x 2),
// +0x68 the dev "Show" byte, +0x70 the time.

struct water_texture_t
{
    std::uint32_t format = 3;    // +0 3 (signed slopes) or 8 (biased by 128)
    float live[ 8 ] = { 9.81f, 10.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f };    // +4
    float built[ 8 ] = { 9.81f, 10.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f };   // +0x24
    std::int32_t current = 0;    // +0x4C
    ocean_field_t field;         // +0x50 omega / +0x54 h0 / +0x58 data / +0x5C speq (n = 64)
    std::vector< float > heights;                    // +0x60
    std::vector< std::uint8_t > mips[ 7 ];           // +0x64
    bool show = false;           // +0x68
    double time = 0.0;           // +0x70
};

struct water_texture_hooks_t
{
    void* user = nullptr;
    // texture[ current ] vt+8( level, bytes, size ) - the GPU upload.
    void ( *upload )( void* user, int texture, int level, const std::uint8_t* bytes, std::uint32_t size ) = nullptr;
};

// The copy's params as field.hxx's spectrum reads them (+0x24 Gravity, +0x28 World Size as Bin Siz, +0x2C Wind Speed,
// +0x30 Directional Power, +0x34 Suppression Wave Length, +0x38 Height Factor).
inline ocean_params_t water_texture_params( const water_texture_t& t )
{
    ocean_params_t p{};
    p.complexity = 6;
    p.gravity = t.built[ 0 ];
    p.bin = t.built[ 1 ];
    p.wind = t.built[ 2 ];
    p.power = t.built[ 3 ];
    p.suppression = t.built[ 4 ];
    p.height = t.built[ 5 ];
    return p;
}

// 0x608010: the dispersion 0x608030 and the spectrum 0x6080D0 from the copy.
inline void water_texture_spectrum( water_texture_t& t )
{
    const ocean_params_t p = water_texture_params( t );
    ocean_field_omega( t.field, p );
    ocean_build_spectrum( t.field, p );
}

// 0x607720's CPU half: the arrays (64 x 64; speq zeroed), current 0, time 0, Show off, then 0x608010. `format` is
// the device query's answer (3, or 8 when 0x5ADDA0 refuses 3).
inline void water_texture_init( water_texture_t& t, std::uint32_t format )
{
    t.format = format;
    t.current = 0;
    t.show = false;
    t.time = 0.0;
    ocean_field_alloc( t.field, 6 );
    t.heights.assign( 64 * 64, 0.0f );
    for ( int l = 0; l < 7; ++l )
        t.mips[ l ].assign( static_cast< std::size_t >( ( 64 >> l ) * ( 64 >> l ) * 2 ), 0 );
    water_texture_spectrum( t );
}

// The "Build" tick 0x607D00 (VuMethod1, dt).
inline void water_texture_tick( water_texture_t& t, float dt )
{
    t.time = static_cast< double >( t.live[ 6 ] * dt ) + t.time;
}

// fistp of a float (round to nearest even; out of range / NaN -> 0x80000000), its low byte.
inline std::uint8_t water_texture_byte( float x )
{
    return static_cast< std::uint8_t >( _mm_cvtss_si32( _mm_set_ss( x ) ) & 0xFF );
}

// 0x6085C0: per texel (row r, column c) of the heights, wrapping at 64: byte 0 = ( h[ r ][ c+1 ] - h[ r ][ c ] ) x 127,
// byte 1 = ( h[ r+1 ][ c ] - h[ r ][ c ] ) x 127, each fistp'd - format 8 adds 128 before the fistp; any other format
// writes nothing.
inline void water_texture_slopes( water_texture_t& t )
{
    if ( t.format != 3u && t.format != 8u )
        return;
    const float bias = 128.0f;
    const float* h = t.heights.data( );
    std::uint8_t* out = t.mips[ 0 ].data( );
    for ( int r = 0; r < 64; ++r )
    {
        const int down = ( ( r + 1 ) & 63 ) * 64;
        for ( int c = 0; c < 64; ++c, out += 2 )
        {
            const float here = h[ r * 64 + c ];
            const float dy = h[ down + c ] - here;
            const float dx = h[ r * 64 + ( ( c + 1 ) & 63 ) ] - here;
            if ( t.format == 3u )
            {
                out[ 0 ] = water_texture_byte( dx * 127.0f );
                out[ 1 ] = water_texture_byte( dy * 127.0f );
            }
            else
            {
                out[ 0 ] = water_texture_byte( dx * 127.0f + bias );
                out[ 1 ] = water_texture_byte( dy * 127.0f + bias );
            }
        }
    }
}

// 0x608730( level ): the 2 x 2 box of mip level - 1 into level. Format 3: signed bytes (movsx), the sum / 4 truncated
// toward zero (cdq / and 3 / sar 2); format 8: unsigned (movzx), ( sum + 2 ) >> 2; any other format: nothing.
inline void water_texture_mip( water_texture_t& t, int level )
{
    const int src_w = 64 >> ( level - 1 );
    const int dst_w = 64 >> level;
    if ( t.format != 3u && t.format != 8u )
        return;
    const std::uint8_t* src = t.mips[ level - 1 ].data( );
    std::uint8_t* dst = t.mips[ level ].data( );
    for ( int y = 0; y < dst_w; ++y )
    {
        const std::uint8_t* s = src + static_cast< std::size_t >( 2 * y * src_w * 2 );
        const std::uint8_t* u = s + static_cast< std::size_t >( src_w * 2 );
        for ( int x = 0; x < dst_w; ++x, s += 4, u += 4, dst += 2 )
        {
            for ( int ch = 0; ch < 2; ++ch )
            {
                if ( t.format == 3u )
                {
                    const int sum = static_cast< std::int8_t >( s[ ch + 2 ] ) + static_cast< std::int8_t >( u[ ch + 2 ] ) +
                                    static_cast< std::int8_t >( s[ ch ] ) + static_cast< std::int8_t >( u[ ch ] );
                    dst[ ch ] = static_cast< std::uint8_t >( sum / 4 );
                }
                else
                {
                    const int sum = s[ ch + 2 ] + u[ ch + 2 ] + u[ ch ] + 2 + s[ ch ];
                    dst[ ch ] = static_cast< std::uint8_t >( sum >> 2 );
                }
            }
        }
    }
}

// 0x607F10 (the gfx-sort command's callback 0x607EF0 hands it the object).
inline void water_texture_build( water_texture_t& t, const water_texture_hooks_t& h )
{
    t.current = ( t.current + 1 ) % 2;
    if ( std::memcmp( t.live, t.built, sizeof( t.built ) ) != 0 )
    {
        std::memcpy( t.built, t.live, sizeof( t.built ) );
        water_texture_spectrum( t );
    }
    const ocean_params_t p = water_texture_params( t );
    ocean_evolve( t.field, t.time );
    ocean_rlft3( t.field );
    // 0x6084B0 = ocean_heights' body into +0x60 (one buffer).
    float sign = 1.0f;
    float* out = t.heights.data( );
    for ( int i = 0; i < 64; ++i )
    {
        const float* row = &t.field.data[ static_cast< std::size_t >( i * 64 ) ];
        for ( int j = 0; j < 64; ++j )
        {
            *out++ = ( row[ j ] * sign ) * p.height;
            sign = sign * -1.0f;
        }
        sign = sign * -1.0f;
    }
    water_texture_slopes( t );
    if ( h.upload )
        h.upload( h.user, t.current, 0, t.mips[ 0 ].data( ), 0x2000u );
    for ( int l = 1; l < 7; ++l )
    {
        water_texture_mip( t, l );
        if ( h.upload )
            h.upload( h.user, t.current, l, t.mips[ l ].data( ), static_cast< std::uint32_t >( ( 64 >> l ) * ( 64 >> l ) * 2 ) );
    }
}
