#pragma once

// VuLensWaterEmitterEntity (vtable 0x75557C, 0x6C bytes: create 0x5E64C0) - a
// point that puts water drops on the camera lens while the camera is near it.
// Properties (engine entity/schema.hxx): Radius +0x60 (25), Drops Per Second
// +0x64 (64). Its emitter interface sits at +0x5C (slot 0 0x5E6780); the
// entity's slots 6 / 7 (0x5E6720 / 0x5E6750, adjustor-thunk null tests: a null
// entity hands 0) add / remove that interface with the lens-water manager
// g 0x759048 (VuLensWaterManagerImpl vt+0x38 / vt+0x3C - render side: hooks).
// Verified against retail: hydro checks/logic.hxx verify_lens_water (case
// lens_water).

#include <cstdint>
#include <cstring>
#include <xmmintrin.h>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include <workspace/core/world/pfx.hxx>

struct lens_water_hooks_t
{
    void* user = nullptr;
    // g 0x759048 vt+0x38 (add) / vt+0x3C (remove) with the emitter interface (entity +0x5C).
    void ( *add )( void* user, const void* emitter ) = nullptr;
    void ( *remove )( void* user, const void* emitter ) = nullptr;
};

// Slot 6 0x5E6720 (game initialize).
inline void lens_water_emitter_start( const void* emitter, const lens_water_hooks_t& h )
{
    if ( h.add )
        h.add( h.user, emitter );
}

// Slot 7 0x5E6750 (game release).
inline void lens_water_emitter_stop( const void* emitter, const lens_water_hooks_t& h )
{
    if ( h.remove )
        h.remove( h.user, emitter );
}

// The interface's slot 0 0x5E6780 (point = the camera): the entity's world
// position (its transform component, entity +0x50 -> +0x80 .. +0x88) against
// the point; Drops Per Second (+0x64) when Radius^2 >= the squared distance
// (comiss / jb: unordered answers 0), else 0 (fldz). The sum is (dy^2 + dx^2)
// + dz^2 in that order.
inline float lens_water_emitter_rate( const float* position, float radius, float drops, const float* point )
{
    const float dx = position[ 0 ] - point[ 0 ];
    const float dy = position[ 1 ] - point[ 1 ];
    const float dz = position[ 2 ] - point[ 2 ];
    const float d = ( dy * dy + dx * dx ) + dz * dz;
    return radius * radius >= d ? drops : 0.0f;
}

// VuLensWaterManagerImpl's drop draw 0x515060 (thiscall manager, count, drops;
// retn 8), reached from the render command 0x5144D0 -> 0x514D70 that slot 13
// 0x5143F0 queues with a copy of the manager's drop array (+0x74, 36-byte
// drops, count +0x2474). THE CPU HALF: 6 vertices of 16 bytes per drop into the
// static 0x799898 (x, y, u, v), drawn by g 0x75907C vt+0xC4( 3, 0, 6 count,
// 4 count, manager +0x2478 (the index buffer), 0x799898, 0x10 ). The GPU half is
// the render layer (hooks, not recreated): the state calls vt+0xAC / +0x94 /
// +0x98 / +0xA0 before and after, the shader +0x30 -> +0x58 bound (vt+0x84),
// the two textures (vt+0xB4: manager +0x14 -> slot 3 into +0x34, +0x44 into
// +0x38), the shader constants (lens_water_draw_scales below) and vt+0x88.
//
// Drop words read: +0 / +4 the position (x, y), +8 / +0xC the previous one,
// +0x18 the size. The streak runs along d = position - previous (normalised by
// 1 / sqrt(dy^2 + dx^2), the length taken as 1 when length - FLT_EPSILON is not
// >= 0, unordered included); h = size x 0.5. Vertices 0 / 1 sit at the previous
// position offset by -h n_y / +h n_y across, 2 / 3 at the position likewise,
// 4 / 5 at the position pushed on by (h n_x, h n_y). The "+ n x 0" terms are
// retail's (x 0 kept: -0 / NaN propagate). u, v: (0, .5) (1, .5) (0, .5)
// (1, .5) (0, 1) (1, 1).
struct lens_water_vertex_t
{
    float x, y, u, v;
};

inline void lens_water_drop_fill( int count, const float* drops, lens_water_vertex_t* out )
{
    for ( int i = 0; i < count; ++i, drops += 9, out += 6 )
    {
        const float* r = drops;
        const float dy = r[ 1 ] - r[ 3 ];
        const float dx = r[ 0 ] - r[ 2 ];
        const float h = r[ 6 ] * 0.5f;                                       // 0x6F765C
        float len = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( dy * dy + dx * dx ) ) );   // VuMath_sqrt 0x40B5A0
        if ( !( len - 1.1920929e-07f >= 0.0f ) )                             // 0x6F7584, comiss / jae
            len = 1.0f;
        const float inv = 1.0f / len;
        const float ny = inv * dy;
        const float nx = inv * dx;
        const float nh = -h;                                                 // xorps 0x6F7CD0
        const float ny0 = ny * 0.0f;
        const float nx0 = nx * 0.0f;
        const float mnx = -nx;
        const float a = nh * ny;                                             // xmm4
        const float b = nh * mnx;                                            // xmm5
        const float c = mnx * h;                                             // xmm6
        const float e = ny * h;                                              // [ebp+0xC]
        const float f = nx * h;                                              // xmm7
        out[ 0 ] = { ( r[ 2 ] + a ) + nx0, ( b + r[ 3 ] ) + ny0, 0.0f, 0.5f };
        out[ 1 ] = { ( r[ 2 ] + e ) + nx0, ( c + r[ 3 ] ) + ny0, 1.0f, 0.5f };
        out[ 2 ] = { ( r[ 0 ] + a ) + nx0, ( r[ 1 ] + b ) + ny0, 0.0f, 0.5f };
        out[ 3 ] = { ( r[ 0 ] + e ) + nx0, ( r[ 1 ] + c ) + ny0, 1.0f, 0.5f };
        out[ 4 ] = { f + ( r[ 0 ] + a ), ( r[ 1 ] + b ) + e, 0.0f, 1.0f };
        out[ 5 ] = { f + ( r[ 0 ] + e ), ( r[ 1 ] + c ) + e, 1.0f, 1.0f };
    }
}

// 0x515060's two shader constants (CPU arithmetic of the GPU half): the shader's
// vt+0x10( manager +0x3C, 1 / manager +0x10 ) and, when manager +0x40 is not 0,
// vt+0x20( +0x40, { 1 / (float) +8, 1 / (float) +0xC } ) (cvtdq2ps of the two
// ints - the target's width / height).
inline void lens_water_draw_scales( float aspect, std::int32_t width, std::int32_t height, float* out3 )
{
    out3[ 0 ] = 1.0f / aspect;
    out3[ 1 ] = 1.0f / static_cast< float >( width );
    out3[ 2 ] = 1.0f / static_cast< float >( height );
}

// ---------------------------------------------------------------------------
// VuLensWaterManagerImpl (vtable 0x74C96C, g 0x759048): the drops' CPU side.
//   +4      bool  enabled (slot 6 0x5135D0 sets, slot 7 0x5135E0 answers)
//   +0x10   float the lens's half width (the drops live in |x| <= it, |y| <= 1)
//   +0x1C   bool  the targets want clearing (slot 9 sets it; the draw clears)
//   +0x64   float a scroll the tick advances (frac kept, + 255 x 1.0 x dt)
//   +0x68   float the outward push (slot 11 0x513620: 1.0 (0x7358A0) x arg)
//   +0x6C   float seconds since the last drop died, +0x70 bool active (slot 8
//                 0x5135F0 answers it)
//   +0x74   256 drops x 36 bytes (lens_water_drop_fill's layout + +0x10 / +0x14
//                 the velocity, +0x1C life, +0x20 the wander timer)
//   +0x2474 the drop count, +0x2484 / +0x2488 the emitters (a VuArray: slot 14
//                 0x513640 appends, slot 15 0x513670 removes), +0x2490 the drops
//                 owed (slot 10 0x513600 adds its float).
// The tuning statics (.data, never written in the race): size 0.025 .. 0.05
// (0x73589C / 0x73588C), life 1 .. 3 (0x73587C / 0x735884), wander timer 0.25
// .. 0.5 (0x735888 / 0x735894), wander 0.5 (0x735890), gravity -0.5
// (0x735880), 1.0 (0x735898 / 0x7358A0). Draws are the global VuRand 0x736810
// (world/pfx.hxx pfx_rand_t), each r = minss( prev x 2^-31, 0.99999988 ).
struct lens_water_manager_t
{
    bool enabled = true;          // +4
    float half_width = 0.0f;      // +0x10
    bool clear = false;           // +0x1C
    float scroll = 0.0f;          // +0x64
    float push = 0.0f;            // +0x68
    float idle = 0.0f;            // +0x6C
    bool active = false;          // +0x70
    float drops[ 256 * 9 ]{};     // +0x74
    std::int32_t count = 0;       // +0x2474
    float owed = 0.0f;            // +0x2490
    // +0x2484 / +0x2488 / +0x248C the emitters: VuArray< emitter interface * > (data, count, capacity). `emitters` holds
    // `emitter_capacity` words (the block); those past the count are whatever the block last held (0 here when it grew -
    // retail's _aligned_malloc block is uninitialised there: never compared).
    std::vector< std::uint32_t > emitters;
    std::int32_t emitter_count = 0;
    std::int32_t emitter_capacity = 0;
};

// VuArray<4-byte>::resize 0x415690 (thiscall n; retn 4) on the emitter list: n past the capacity grows the block to
// max( capacity + capacity / 2 (cdq / sar: toward zero), n ), copying the count's words; the count becomes n either way.
inline void lens_water_emitters_resize( lens_water_manager_t& m, std::int32_t n )
{
    const std::int32_t cap = m.emitter_capacity;
    if ( n > cap )
    {
        std::int32_t grown = cap + cap / 2;
        if ( n > grown )
            grown = n;
        if ( grown > cap )
        {
            std::vector< std::uint32_t > block( static_cast< std::size_t >( grown ), 0u );
            for ( std::int32_t i = 0; i < m.emitter_count; ++i )
                block[ static_cast< std::size_t >( i ) ] = m.emitters[ static_cast< std::size_t >( i ) ];
            m.emitters.swap( block );
            m.emitter_capacity = grown;
        }
    }
    m.emitter_count = n;
}

// Slot 14 0x513640 (thiscall emitter; retn 4): resize( count + 1 ), then the last word = the emitter.
inline void lens_water_add_emitter( lens_water_manager_t& m, std::uint32_t emitter )
{
    lens_water_emitters_resize( m, m.emitter_count + 1 );
    m.emitters[ static_cast< std::size_t >( m.emitter_count - 1 ) ] = emitter;
}

// Slot 15 0x513670 -> 0x5154A0 (VuArray remove-swap, thiscall &emitter; retn 4): the FIRST word equal to the emitter,
// when it is not the last, SWAPS with the last (the removed pointer stays in the block just past the new count); then
// 0x415690's body inline with count - 1. Not found / empty: nothing.
inline void lens_water_remove_emitter( lens_water_manager_t& m, std::uint32_t emitter )
{
    const std::int32_t n = m.emitter_count;
    if ( n <= 0 )
        return;
    std::int32_t i = 0;
    while ( m.emitters[ static_cast< std::size_t >( i ) ] != emitter )
        if ( ++i >= n )
            return;
    if ( i + 1 < n )
        std::swap( m.emitters[ static_cast< std::size_t >( i ) ], m.emitters[ static_cast< std::size_t >( n - 1 ) ] );
    lens_water_emitters_resize( m, n - 1 );
}

// Slot 9 0x5142C0: no drops, no push, the targets cleared.
inline void lens_water_clear( lens_water_manager_t& m )
{
    m.count = 0;
    m.push = 0.0f;
    m.clear = true;
}

// Slot 10 0x513600 / slot 11 0x513620.
inline void lens_water_add_drops( lens_water_manager_t& m, float n ) { m.owed = n + m.owed; }
inline void lens_water_set_push( lens_water_manager_t& m, float x ) { m.push = 1.0f * x; }

// The tick 0x514500 (VuMethod1; dt). Disabled: slot 9 (vt+0x24 - its own
// clear) and nothing else. Else every emitter's slot 0 rate on the first
// viewport's position (g 0x759060 +0x28; `rate( i )`, the x87 answer) x dt
// adds to the owed drops; n = (int) owed (cvttss2si), owed -= n, and min( n,
// 256 - count ) drops spawn: x = r ( w - (-w) ) + (-w), y = 2 r - 1, the
// previous position = the position, no velocity, size / life / wander timer =
// r (hi - lo) + lo - five draws in that order. The scroll keeps its fraction
// and gains 255 dt. Any drop alive: active, idle 0; else active drops once
// idle exceeds 1 / 1.0, idle += dt. Then each drop: life -= dt - dead at <= 0;
// gone when |x| > w or |y| > 1 (andps; NaN stays); timer -= dt and, when
// below 0, a new timer (one draw) and a sideways kick s = r (0.5 - -0.5) +
// -0.5 along (vy, -vx); then the previous position = the position, the
// position += v dt, and v += (p / |p|) push dt (|p| < FLT_EPSILON: / 1) and vy
// += -0.5 dt. A dead drop takes the last drop's 36 bytes and the count drops
// by one (that slot is visited next).
inline void lens_water_tick( lens_water_manager_t& m, float dt, int emitters, const std::function< float( int ) >& rate, pfx_rand_t& rand )
{
    if ( !m.enabled )
    {
        lens_water_clear( m );
        return;
    }
    for ( int i = 0; i < emitters; ++i )
        m.owed = rate( i ) * dt + m.owed;
    const std::int32_t n = _mm_cvttss_si32( _mm_set_ss( m.owed ) );
    if ( n != 0 )
    {
        m.owed = m.owed - static_cast< float >( n );
        std::int32_t spawn = 256 - m.count;
        if ( n < spawn )
            spawn = n;
        for ( ; spawn > 0; --spawn )
        {
            float* d = m.drops + 9 * m.count;
            ++m.count;
            const float w = m.half_width, nw = -m.half_width;
            d[ 0 ] = rand.next( ) * ( w - nw ) + nw;
            d[ 1 ] = rand.next( ) * 2.0f - 1.0f;
            d[ 2 ] = d[ 0 ];
            d[ 4 ] = 0.0f;
            d[ 5 ] = 0.0f;
            d[ 3 ] = d[ 1 ];
            d[ 6 ] = rand.next( ) * ( 0.05f - 0.025f ) + 0.025f;
            d[ 7 ] = rand.next( ) * ( 3.0f - 1.0f ) + 1.0f;
            d[ 8 ] = rand.next( ) * ( 0.5f - 0.25f ) + 0.25f;
        }
    }
    const float f = m.scroll - static_cast< float >( _mm_cvttss_si32( _mm_set_ss( m.scroll ) ) );
    m.scroll = 1.0f * 255.0f * dt + f;
    if ( m.count != 0 )
    {
        m.active = true;
        m.idle = 0.0f;
    }
    else
    {
        if ( m.idle > 1.0f / 1.0f )
            m.active = false;
        m.idle = m.idle + dt;
    }
    for ( std::int32_t i = 0; i < m.count; ++i )
    {
        float* d = m.drops + 9 * i;
        d[ 7 ] = d[ 7 ] - dt;
        bool dead = 0.0f >= d[ 7 ];
        if ( !dead )
            dead = std::fabs( d[ 0 ] ) > m.half_width || std::fabs( d[ 1 ] ) > 1.0f;
        if ( dead )
        {
            std::memcpy( d, m.drops + 9 * ( m.count - 1 ), 36 );
            --m.count;
            --i;
            continue;
        }
        d[ 8 ] = d[ 8 ] - dt;
        if ( 0.0f > d[ 8 ] )
        {
            const float vy = d[ 5 ], mvx = -d[ 4 ];
            d[ 8 ] = rand.next( ) * ( 0.5f - 0.25f ) + 0.25f;
            const float s = rand.next( ) * ( 0.5f - -0.5f ) + -0.5f;
            d[ 4 ] = vy * s + d[ 4 ];
            d[ 5 ] = mvx * s + d[ 5 ];
        }
        d[ 2 ] = d[ 0 ];
        d[ 3 ] = d[ 1 ];
        const float y = dt * d[ 5 ] + d[ 1 ];
        const float x = d[ 4 ] * dt + d[ 0 ];
        d[ 1 ] = y;
        d[ 0 ] = x;
        float len = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( y * y + x * x ) ) );   // VuMath_sqrt 0x40B5A0
        if ( !( len - 1.1920929e-07f >= 0.0f ) )
            len = 1.0f;
        const float inv = 1.0f / len;
        d[ 4 ] = d[ 4 ] + inv * x * m.push * dt;
        d[ 5 ] = inv * y * m.push * dt + d[ 5 ];
        d[ 5 ] = -0.5f * dt + d[ 5 ];
    }
}