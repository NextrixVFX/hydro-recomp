#pragma once

#include <workspace/core/boat/hull.hxx>
#include <workspace/core/boat/state.hxx>
#include <workspace/core/world/query.hxx>

#include <cstdint>
#include <cstring>
#include <string>
#include <xmmintrin.h>

// VuBoatEntity's Build tick (vtable slot 14 0x41F330, thiscall boat; dt;
// retn 4), READ in full (IDA + disassembly, world batch 14). In order:
//   1. 0x52F690 on the gfx-settings blender at the static 0x759430 with the
//      transform's position (+0x80, 16 b): the blended settings into boat
//      +0x408 (0x530450) - the far plane the race camera reads is in there;
//   2. VuWater's surface under the position (0x52ACB0 on the surface tree,
//      the record's limit 0xFEFFFFFF); found: +0x2B4 = that surface's map
//      sample 0x535E10 (world/query.hxx water_map_sample) and the colour
//      +0x2B8..+0x2C4 = (1 - v) x (byte +0x454 + k) / 255 + (byte +0x458 +
//      k) / 255 x v per lane;
//   3. the driver's vt+0x4C(dt);
//   4. the animation (+0x230) +4's 0x5169E0(dt) / 0x516CF0 when set, the
//      model instance +0x2A4's 0x532720(anim +4), +0x2A8's 0x532990(+0x2A4)
//      when its +0x14, then both instances' +0x24 / +0x28 flipped (== 0);
//   5. the pfx 0x419700(dt) on +0x234 and the debris 0x411A50(dt) on +0x238;
//   6. the aabb: M = (instance +0x2A4 +0x50) . world . translation(+0x49C)
//      (0x4078C0 twice, 0x410AB0), the instance's +0x2C aabb through M
//      (0x41B2A0), merged (0x517530) with the pfx +0x98 and debris +0xA8
//      aabbs when theirs are not empty (min x != FLT_MAX), into the draw
//      component (+0x6C) +0x24..+0x43; registered (+0x1E) -> the draw
//      manager g 0x7590B8 vt+0x30(draw);
//   7. 0x4200E0(dt) - boat_build_audio below, RECREATED + VERIFIED.
// Step 2 is recreated below (boat_build_water, verified through 0x52ACB0);
// steps 1 and 3 - 6 are NOT yet (the gfx settings blender, the driver's
// vt+0x4C, the model instances / animation system, 0x419700, 0x411A50).

// 0x4200E0's outside calls: 0x50FBF0 on the event slot +0x488 (the engine's
// event create: true leaves the FMOD event at +0x48C) and the FMOD imports
// start 0x64C354, setCallback 0x64C334, release 0x64C340 - audio (platform).
struct boat_build_audio_hooks_t
{
    void* user = nullptr;
    bool ( *slot )( void* user, std::uint8_t* boat, const std::string& name ) = nullptr;
    void ( *start )( void* user, std::uint32_t handle ) = nullptr;
    void ( *callback )( void* user, std::uint32_t handle ) = nullptr;
    void ( *release )( void* user, std::uint32_t handle ) = nullptr;
};

// 0x4200E0 (__usercall: ecx the boat, xmm1 dt). s = |body +0x140| (VuMath_sqrt
// of (x x + y y) + z z), m = ((engine +0xC x +4) x +0x28) x owner +0x470:
// r = (s x 2.2369363 - m x 1.05) / (m x 1.1 - m x 1.05), k = 1 when solver
// +0x54 - 1.1920929e-07 is ordered >= 0 else 0.25; +0x3F8 moves at 0.5 / s up
// (k r - +0x3F8 >= 0) else 1 / s down, clamped [0, 1]; +0x3FC = |camera
// +0x20| / ((owner +0x470 x (+0xC x +4)) x 0.44704) + +0x3F8 x 3; h =
// clamp(solver +0x5C x 0.1), 0 unless |v|^2 - 1998.4475 >= 0; +0x400 moves at
// 2 / s toward max(h, +0x3F8) (up when >= its value); q = clamp((s / that
// speed - 0.5) x 2); +0x404 moves at 2 / s up when +0x404 - q x +0x30C is
// below 0 (or NaN), else down. Clamps are minss(1, x) then maxss(0, .).
// With the +0x204 string non-empty (+0x214): +0x404 > 0.1 -> the event
// started once (+0x484 = 1); else a started one stopped (setCallback, release,
// +0x48C = 0) and +0x484 = 0.
inline void boat_build_audio( std::uint8_t* boat, const std::uint8_t* body, const std::uint8_t* engine, const std::uint8_t* solver,
                              const std::uint8_t* camera, const std::string& name, float dt, const boat_build_audio_hooks_t& hooks )
{
    auto ge = []( float a, float b ) { return _mm_comige_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; };   // ordered a >= b
    auto gt = []( float a, float b ) { return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; };
    auto minss = []( float a, float b ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); };
    auto maxss = []( float a, float b ) { return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); };
    auto clamp = [ & ]( float x ) { return maxss( 0.0f, minss( 1.0f, x ) ); };
    const float vx = state_f( body, 0x140 ), vy = state_f( body, 0x144 ), vz = state_f( body, 0x148 );
    const float s = hull_sqrt( ( vx * vx + vy * vy ) + vz * vz );
    const float owner = state_f( boat, 0x470 );
    const float m = ( ( state_f( engine, 0xC ) * state_f( engine, 4 ) ) * state_f( engine, 0x28 ) ) * owner;
    const float r = ( s * water_bits( 0x400F29F7u ) - m * water_bits( 0x3F866666u ) ) / ( m * water_bits( 0x3F8CCCCDu ) - m * water_bits( 0x3F866666u ) );
    const float k = ge( state_f( solver, 0x54 ) - water_bits( 0x34000000u ), 0.0f ) ? 1.0f : 0.25f;
    const float sign = ge( k * r - state_f( boat, 0x3F8 ), 0.0f ) ? 0.5f : -1.0f;
    const float f3f8 = clamp( sign * dt + state_f( boat, 0x3F8 ) );
    state_put_f( boat, 0x3F8, f3f8 );
    const float cx = state_f( camera, 0x20 ), cy = state_f( camera, 0x24 ), cz = state_f( camera, 0x28 );
    const float c = hull_sqrt( ( cx * cx + cy * cy ) + cz * cz );
    const float speed = ( owner * ( state_f( engine, 0xC ) * state_f( engine, 4 ) ) ) * water_bits( 0x3EE4E26Du );
    state_put_f( boat, 0x3FC, c / speed + f3f8 * 3.0f );
    float h = clamp( state_f( solver, 0x5C ) * water_bits( 0x3DCCCCCDu ) );
    if ( !ge( ( vx * vx + vy * vy ) + vz * vz - water_bits( 0x44F9CE52u ), 0.0f ) )
        h = 0.0f;
    const float rate = ge( maxss( h, f3f8 ) - state_f( boat, 0x400 ), 0.0f ) ? 2.0f : -2.0f;
    state_put_f( boat, 0x400, clamp( rate * dt + state_f( boat, 0x400 ) ) );
    const float q = clamp( ( s / speed - 0.5f ) * 2.0f );
    const float rate2 = ge( state_f( boat, 0x404 ) - q * state_f( boat, 0x30C ), 0.0f ) ? -2.0f : 2.0f;
    const float f404 = clamp( rate2 * dt + state_f( boat, 0x404 ) );
    state_put_f( boat, 0x404, f404 );
    if ( name.empty( ) )
        return;
    std::uint32_t handle;
    std::memcpy( &handle, boat + 0x48C, 4 );
    if ( gt( f404, water_bits( 0x3DCCCCCDu ) ) )
    {
        if ( boat[ 0x484 ] )
            return;
        if ( hooks.slot && hooks.slot( hooks.user, boat, name ) && hooks.start )
        {
            std::memcpy( &handle, boat + 0x48C, 4 );
            hooks.start( hooks.user, handle );
        }
        boat[ 0x484 ] = 1;
        return;
    }
    if ( !boat[ 0x484 ] )
        return;
    if ( handle )
    {
        if ( hooks.callback )
            hooks.callback( hooks.user, handle );
        if ( hooks.release )
            hooks.release( hooks.user, handle );
        image_put32( boat, 0x48C, 0 );
    }
    boat[ 0x484 ] = 0;
}

// Step 2 (0x41F38D .. 0x41F54E): the position (transform +0x80, 16 bytes -
// copied before step 1's call) as the record {point, +0x10 0xFEFFFFFF, +0x14
// 0} and its xy, 0x52ACB0 on VuWater's surface tree ([[0x759058] +0x3C]:
// world/query.hxx water_surface_under). Found: +0x2B4 = v = that surface's
// map sample 0x535E10 at the position (water_map_sample), and per lane k
// +0x2B8 + 4k = (1 - v) x (byte(+0x454 + k) x c) + (byte(+0x458 + k) x c) x
// v, c = 0x3B808081 (1 / 255; movzx -> cvtdq2ps -> mulss); not found:
// nothing written. False (nothing written): no VuWater (retail would read
// through a null global - not recreated), a surface-tree stack overflow or a
// map cell outside the map (both not recreated).
inline bool boat_build_water( std::uint8_t* boat, const float* position, const water_t* w )
{
    if ( !w )
        return false;
    const int found = water_surface_under( *w, position, water_bits( 0xFEFFFFFFu ) );
    if ( found == -2 )
        return false;
    if ( found < 0 )
        return true;
    float v;
    if ( !water_map_sample( w->surfaces[ static_cast< std::size_t >( found ) ], position, v ) )
        return false;
    state_put_f( boat, 0x2B4, v );
    const float c = water_bits( 0x3B808081u );
    const float u = 1.0f - v;
    for ( int k = 0; k < 4; ++k )
    {
        const float a = static_cast< float >( static_cast< std::int32_t >( boat[ 0x458 + k ] ) ) * c;
        const float b = static_cast< float >( static_cast< std::int32_t >( boat[ 0x454 + k ] ) ) * c;
        state_put_f( boat, 0x2B8 + 4 * k, u * b + a * v );
    }
    return true;
}