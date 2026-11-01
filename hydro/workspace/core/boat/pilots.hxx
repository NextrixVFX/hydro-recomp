#pragma once

#include <workspace/core/boat/fleet.hxx>
#include <workspace/core/boat/human.hxx>

#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// The other drivers' ticks (each the driver's vtable slot 18, vt+0x48, that
// the Decision tick 0x41EFF0 calls): VuGhostDriver 0x40EBA0, VuAiUiDriver
// 0x40E5C0 and VuAiNuisanceDriver 0x40DA90. VuBoat_loadFromDB 0x41CFF0 picks
// the driver by the boat's Driver string: the human (0xD0, ctor 0x40ED50),
// "AI" (0xD4, 0x40B690), "UI" (0xD4: 0x40B690 then the VuAiUiDriver vtable
// 0x737CDC), "Nuisance" (0x104, 0x40D880 - a VuAiDriver plus a weak reference
// to the boat it bothers), "Ghost" (0x84, 0x40EB50).

// The path points of a plan (the driver's +0x8C / +0x90): its sectors'
// images, in plan order.
inline std::vector< const std::uint8_t* > pilot_points( const track_manager_t& tm, const fleet_plan_t& plan )
{
    std::vector< const std::uint8_t* > points;
    for ( int s : plan.sectors )
        points.push_back( s >= 0 ? tm.sectors[ s ].image : nullptr );
    return points;
}

// One plane-pair fraction along a sector at a point, as 0x40E800 / 0x40E280 /
// 0x40E060 / 0x40E5C0 compute it: d0 = ((n0.y p.y + p.x n0.x) + n0.z p.z) +
// d0, d3 the same of plane 3 (+0x70); d0 / (d3 + d0).
inline float pilot_fraction( const std::uint8_t* q, const float* p )
{
    const float d0 = ( ( state_f( q, 0x44 ) * p[ 1 ] + p[ 0 ] * state_f( q, 0x40 ) ) + state_f( q, 0x48 ) * p[ 2 ] ) + state_f( q, 0x4C );
    const float d3 = ( ( state_f( q, 0x74 ) * p[ 1 ] + p[ 0 ] * state_f( q, 0x70 ) ) + state_f( q, 0x78 ) * p[ 2 ] ) + state_f( q, 0x7C );
    return d0 / ( d3 + d0 );
}

// The least of a sector's four planes at a point (minss from FLT_MAX).
inline float pilot_inside( const std::uint8_t* q, const float* p )
{
    float m = 3.4028235e38f;
    for ( int k = 0; k < 4; ++k )
    {
        const int at = 0x40 + 16 * k;
        m = track_minss( m, ( ( state_f( q, at + 4 ) * p[ 1 ] + p[ 0 ] * state_f( q, at ) ) + state_f( q, at + 8 ) * p[ 2 ] ) + state_f( q, at + 12 ) );
    }
    return m;
}

// ---- VuGhostDriver (vtable 0x737DDC; ctor 0x40EB50: +0x78 -1, +0x80 0) ----

// Slot 18 0x40EBA0 (thiscall, the driver; dt unread; retn 4): only the sector
// at +0x80, refreshed from the boat's position (component +0x80) as the
// human's (slot 21 with checkpoint -1).
struct ghost_pilot_t
{
    int sector = -1;   // +0x80 as a sector index
};

inline void ghost_driver_tick( const track_manager_t& tm, ghost_pilot_t& p, const std::uint8_t* transform )
{
    human_sector_refresh( tm, reinterpret_cast< const float* >( transform + 0x80 ), p.sector, -1 );
}

// ---- VuAiUiDriver (vtable 0x737CDC) ----

// 0x40E800 (thiscall, the driver; component*; retn 4): the UI driver keeps its
// own plan. With a first point whose fraction at the position (component
// +0x80) is ordered above 1 the plan advances (slot 2 0x4D0DD0); then, with no
// first point or the position outside it (least plane below 0, or a NaN), the
// plan starts (slot 1 0x4D0D50) from slot 21's sector for the position and
// +0x78 - unless there is none.
inline void ui_plan_update( const track_manager_t& tm, fleet_plan_t& plan, const std::uint8_t* transform, std::int32_t checkpoint,
    fleet_random_t& rng )
{
    const float* pos = reinterpret_cast< const float* >( transform + 0x80 );
    bool start = true;
    if ( !plan.sectors.empty( ) && plan.sectors[ 0 ] >= 0 )
    {
        if ( pilot_fraction( tm.sectors[ plan.sectors[ 0 ] ].image, pos ) > 1.0f )
            fleet_plan_advance( plan, tm, rng );
        if ( !plan.sectors.empty( ) && plan.sectors[ 0 ] >= 0 )
            start = !human_ge( pilot_inside( tm.sectors[ plan.sectors[ 0 ] ].image, pos ), 0.0f );
    }
    if ( start )
    {
        const int s = track_nearest( tm, pos, checkpoint, false );
        if ( s >= 0 )
            fleet_plan_start( plan, tm, s, rng );
    }
}

struct ui_pilot_t
{
    std::uint8_t* driver = nullptr;   // 0xD4 (+0x80 the mode: 1 from the ctor)
    fleet_plan_t plan;                // +0x84
};

// Slot 18 0x40E5C0 (thiscall, the driver; dt; retn 4): VuAiDriver's tick
// 0x40B940 first (boat/ai.hxx's ai_tick, on the plan as it stands), then the
// plan update 0x40E800, then its own steer: +0x9C = ((engine +0xC x +4) x
// 0.44704) x 0.25 (0x6F762C), and with a first point: c = maxss(0, minss(1,
// its fraction at the position)), +0x9C = (1 - c) a + c b (a / b its speed
// hints +0xB4 / +0xB8 when above 0, else the default), the path follow
// 0x40BEC0 into +0xA8 and the avoidance 0x40C750, +0xB8 = minss(+0x9C,
// (owner +0x470 x (engine +0xC x +4)) x 0.44704), 0x40D110 (steer / throttle
// from 0); 0x40D5A0 with them (0 without a point) and no boost. The springs
// +0xC0 / +0xC8 step twice per tick (0x40B940's 0x40D5A0 and this one).
inline void ui_driver_tick( const track_manager_t& tm, ui_pilot_t& p, ai_tick_t t, fleet_random_t& rng, float dt, const ai_tick_hooks_t& hooks )
{
    std::uint8_t* drv = p.driver;
    t.driver = drv;
    std::vector< const std::uint8_t* > points = pilot_points( tm, p.plan );
    t.points = points.empty( ) ? nullptr : points.data( );
    t.count = static_cast< std::int32_t >( points.size( ) );
    ai_tick( t, dt, hooks );
    ui_plan_update( tm, p.plan, t.transform, human_i32( drv, 0x78 ), rng );
    points = pilot_points( tm, p.plan );
    const float* pos = reinterpret_cast< const float* >( t.transform + 0x80 );
    const float* fwd = reinterpret_cast< const float* >( t.transform + 0x60 );
    const float mph = ai_bits( 0x3EE4E26Du );
    const float base = ( ( state_f( t.engine, 0xC ) * state_f( t.engine, 4 ) ) * mph ) * 0.25f;
    state_put_f( drv, 0x9C, base );
    float steer = 0.0f, throttle = 0.0f;
    if ( !points.empty( ) && points[ 0 ] )
    {
        const std::uint8_t* q = points[ 0 ];
        const float f = pilot_fraction( q, pos );
        const float a = state_f( q, 0xB4 ) > 0.0f ? state_f( q, 0xB4 ) : base;
        const float b = state_f( q, 0xB8 ) > 0.0f ? state_f( q, 0xB8 ) : base;
        const float c = ai_maxss( 0.0f, ai_minss( 1.0f, f ) );
        state_put_f( drv, 0x9C, ( 1.0f - c ) * a + c * b );
        float* target = reinterpret_cast< float* >( drv + 0xA8 );
        float direction[ 4 ];
        ai_path_follow( points.data( ), static_cast< std::int32_t >( points.size( ) ), pos, target, direction );
        ai_avoid( state_f( drv, 0x98 ), t.body, pos, target, hooks.avoid );
        const float top = ( state_f( t.owner, 0x470 ) * ( state_f( t.engine, 0xC ) * state_f( t.engine, 4 ) ) ) * mph;
        const float wanted = ai_minss( state_f( drv, 0x9C ), top );
        state_put_f( drv, 0xB8, wanted );
        ai_steer( t.body, t.solver, pos, fwd, target, state_f( drv, 0xA0 ), wanted, dt, &steer, &throttle );
    }
    ai_controls( drv, t.boat, t.animation, dt, steer, throttle, 0, hooks.controls );
}
