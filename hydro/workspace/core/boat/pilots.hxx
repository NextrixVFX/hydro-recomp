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

// ---- VuAiNuisanceDriver (vtable 0x737C4C; 0x104) ----
// A VuAiDriver that harasses another boat: +0xE4 a VuWeakRef<VuBoatEntity>
// (vtable 0x737C44; +0xE8 / +0xEC its list links, +0xF0 the target boat),
// +0xF4 the target's sector, +0xF8 / +0xFC the audio line's event holder
// (0x50FBF0's object: +0xFC the FMOD event), +0x100 the last line played. Its
// tick does not call 0x40B940: it steers itself to its own target +0xD4.

// What it reaches that is not recreated: the boat's DB 0x41D500 (the boat's
// BoatDB entry) and its "Nuisance" / "AudioLines" members (the asset system:
// `line_count` = the list's size, -1 when not a list; `line` = element k's
// text, "" when not a string), the FMOD event holder 0x50FBF0 (the old event
// stopped, getEvent, the finish callback 0x50FBB0 - `line_event` returns the
// handle written to +0xFC, 0 on failure), FMOD start / set3DAttributes /
// setCallback + release, and the weak reference's release 0x40E4D0 (its list
// links in the target - `release_target`; the image's +0xE8 / +0xEC / +0xF0
// are zeroed here).
struct nuisance_hooks_t
{
    ai_tick_hooks_t ai;
    void* user = nullptr;
    int ( *line_count )( void* user ) = nullptr;
    const char* ( *line )( void* user, int k ) = nullptr;
    std::uint32_t ( *line_event )( void* user, const char* name ) = nullptr;
    void ( *start )( void* user, std::uint32_t event ) = nullptr;
    void ( *attributes )( void* user, std::uint32_t event, const float* pos, const float* vel ) = nullptr;
    void ( *stop )( void* user, std::uint32_t event ) = nullptr;
    void ( *release_target )( void* user ) = nullptr;
};

struct nuisance_pilot_t
{
    std::uint8_t* driver = nullptr;     // 0x104
    fleet_plan_t plan;                  // +0x84
    bool target = false;                // +0xF0 set (the boat is the caller's: its images in nuisance_target_t)
    int target_sector = -1;             // +0xF4
};

struct nuisance_target_t
{
    const std::uint8_t* transform = nullptr;   // the target's +0x50
    const std::uint8_t* body = nullptr;        // the target's +0x21C
};

// 0x40E060 (thiscall, the driver; the target's component; retn 4): with no
// sector, slot 21 (the target's position, -1). Else, once the target's
// fraction along it is ordered above 1, the first of its next sectors that
// holds the position (least plane >= 0, ordered) - or the last one tried
// (none without next sectors). No sector after: the weak reference's
// release (vt+0 0x40E4D0).
inline void nuisance_target_sector( const track_manager_t& tm, nuisance_pilot_t& p, const std::uint8_t* target_transform,
    const nuisance_hooks_t& hooks )
{
    const float* tp = reinterpret_cast< const float* >( target_transform + 0x80 );
    if ( p.target_sector < 0 )
        p.target_sector = track_nearest( tm, tp, -1, false );
    else if ( pilot_fraction( tm.sectors[ p.target_sector ].image, tp ) > 1.0f )
    {
        int last = -1;
        int found = -1;
        for ( int n : tm.sectors[ p.target_sector ].next )
        {
            last = n;
            if ( human_ge( pilot_inside( tm.sectors[ n ].image, tp ), 0.0f ) )
            {
                found = n;
                break;
            }
        }
        p.target_sector = found >= 0 ? found : last;
    }
    if ( p.target_sector < 0 )
    {
        for ( int at : { 0xE8, 0xEC, 0xF0 } )
            image_put32( p.driver, at, 0 );
        p.target = false;
        if ( hooks.release_target )
            hooks.release_target( hooks.user );
    }
}

// 0x40E280 (thiscall, the driver; the component; retn 4): the plan starts
// (slot 1) from slot 21's sector for the position and +0x78 when it has no
// first point or the position is outside it (least plane below 0, or a NaN);
// then, with a first point whose fraction is ordered above 1, it advances.
inline void nuisance_plan_update( const track_manager_t& tm, fleet_plan_t& plan, const std::uint8_t* transform, std::int32_t checkpoint,
    fleet_random_t& rng )
{
    const float* pos = reinterpret_cast< const float* >( transform + 0x80 );
    const bool first = !plan.sectors.empty( ) && plan.sectors[ 0 ] >= 0;
    if ( !first || !human_ge( pilot_inside( tm.sectors[ plan.sectors[ 0 ] ].image, pos ), 0.0f ) )
    {
        const int s = track_nearest( tm, pos, checkpoint, false );
        if ( s >= 0 )
            fleet_plan_start( plan, tm, s, rng );
    }
    if ( !plan.sectors.empty( ) && plan.sectors[ 0 ] >= 0 && pilot_fraction( tm.sectors[ plan.sectors[ 0 ] ].image, pos ) > 1.0f )
        fleet_plan_advance( plan, tm, rng );
}

// Slot 18 0x40DA90 (thiscall, the driver; dt; retn 4). +0xA0 = the speed
// signed by the forward row (ordered >= 0: +1) as 0x40B940's. With a target:
// its speed v, 0x40E060, 0x40E280, +0x9C = 0 and, with a first point and the
// target's sector: c = the unit direction to the target dotted with point 0's
// direction; +0x9C = the boat's own top ((engine +0xC x +4) x 0.44704) when c
// > 0, v when c >= -0.8660254 (or a NaN), else ((1 - (|c| - 0.8660254) x
// 7.4641008) x (maxss(v, top) - v / 2)) + v / 2; the path follow into +0xD4,
// the avoidance, +0xB8 = minss(+0x9C, (owner +0x470 x (engine +0xC x +4)) x
// 0.44704), 0x40D110. 0x40D5A0 (no boost). Then the audio line: with no event
// (+0xFC), a target and the boat's +0x46B clear, the DB's lines: with more
// than one, VuRandom (the global) draws until the pick differs from +0x100 -
// pick = clamp(fistp(fistp(r x n - 0.5)), 0, n - 1) (x87, round to nearest) -
// else line 0; a non-empty line's event is made and started; +0x100 = the
// pick. An event's 3D attributes follow the boat (component +0x80, the body's
// velocity); with +0x46B it is stopped and +0xFC = 0.
inline void nuisance_driver_tick( const track_manager_t& tm, nuisance_pilot_t& p, ai_tick_t t, const nuisance_target_t& target,
    fleet_random_t& rng, float dt, const nuisance_hooks_t& hooks )
{
    std::uint8_t* drv = p.driver;
    std::uint8_t* boat = t.boat;
    const float vx = state_f( t.body, 0x140 ), vy = state_f( t.body, 0x144 ), vz = state_f( t.body, 0x148 );
    const float* fwd = reinterpret_cast< const float* >( t.transform + 0x60 );
    const float d = ( fwd[ 1 ] * vy + fwd[ 0 ] * vx ) + fwd[ 2 ] * vz;
    const float sign = human_ge( d, 0.0f ) ? 1.0f : -1.0f;
    state_put_f( drv, 0xA0, hull_sqrt( ( vx * vx + vy * vy ) + vz * vz ) * sign );
    const float* pos = reinterpret_cast< const float* >( t.transform + 0x80 );
    const float mph = ai_bits( 0x3EE4E26Du );
    const float own = ( state_f( t.engine, 0xC ) * state_f( t.engine, 4 ) ) * mph;
    float steer = 0.0f, throttle = 0.0f;
    if ( p.target )
    {
        const float tx = state_f( target.body, 0x140 ), ty = state_f( target.body, 0x144 ), tz = state_f( target.body, 0x148 );
        const float tv = hull_sqrt( ( tx * tx + ty * ty ) + tz * tz );
        nuisance_target_sector( tm, p, target.transform, hooks );
        nuisance_plan_update( tm, p.plan, t.transform, human_i32( drv, 0x78 ), rng );
        state_put_f( drv, 0x9C, 0.0f );
        const std::vector< const std::uint8_t* > points = pilot_points( tm, p.plan );
        if ( !points.empty( ) && points[ 0 ] && p.target_sector >= 0 && target.transform )
        {
            const std::uint8_t* q = points[ 0 ];
            const float dx = state_f( target.transform, 0x80 ) - pos[ 0 ];
            const float dy = state_f( target.transform, 0x84 ) - pos[ 1 ];
            const float dz = state_f( target.transform, 0x88 ) - pos[ 2 ];
            const float inv = 1.0f / hull_sqrt( ( dy * dy + dx * dx ) + dz * dz );
            const float c = ( state_f( q, 0xA4 ) * ( dy * inv ) + ( dx * inv ) * state_f( q, 0xA0 ) ) + state_f( q, 0xA8 ) * ( dz * inv );
            if ( human_gt( c, 0.0f ) )
                state_put_f( drv, 0x9C, own );
            else if ( human_gt( ai_bits( 0xBF5DB3D7u ), c ) )
            {
                const float half = tv * 0.5f;
                const float m = ai_maxss( tv, own );
                state_put_f( drv, 0x9C, ( 1.0f - ( std::fabs( c ) - ai_bits( 0x3F5DB3D7u ) ) * ai_bits( 0x40EED9EAu ) ) * ( m - half ) + half );
            }
            else
                state_put_f( drv, 0x9C, tv );
            float* aim = reinterpret_cast< float* >( drv + 0xD4 );
            float direction[ 4 ];
            ai_path_follow( points.data( ), static_cast< std::int32_t >( points.size( ) ), pos, aim, direction );
            ai_avoid( state_f( drv, 0x98 ), t.body, pos, aim, hooks.ai.avoid );
            const float top = ( state_f( t.owner, 0x470 ) * ( state_f( t.engine, 0xC ) * state_f( t.engine, 4 ) ) ) * mph;
            const float wanted = ai_minss( state_f( drv, 0x9C ), top );
            state_put_f( drv, 0xB8, wanted );
            ai_steer( t.body, t.solver, pos, fwd, aim, state_f( drv, 0xA0 ), wanted, dt, &steer, &throttle );
        }
    }
    ai_controls( drv, boat, t.animation, dt, steer, throttle, 0, hooks.ai.controls );
    if ( human_i32( drv, 0xFC ) == 0 && p.target && !boat[ 0x46B ] )
    {
        const int n = hooks.line_count ? hooks.line_count( hooks.user ) : -1;
        if ( n > 0 )
        {
            int pick = 0;
            if ( n > 1 )
            {
                do
                {
                    const float x = rng.draw( ) * static_cast< float >( n );
                    const int i = static_cast< int >( std::nearbyint( static_cast< double >( x ) - 0.5 ) );   // fld; fsub; fistp
                    pick = static_cast< int >( std::nearbyint( static_cast< double >( static_cast< float >( i ) ) ) );
                    if ( n - 1 < pick )
                        pick = n - 1;
                    if ( pick < 0 )
                        pick = 0;
                } while ( pick == human_i32( drv, 0x100 ) );
            }
            const char* name = hooks.line ? hooks.line( hooks.user, pick ) : "";
            if ( name && *name && hooks.line_event )
            {
                const std::uint32_t e = hooks.line_event( hooks.user, name );
                image_put32( drv, 0xFC, e );
                if ( e != 0 && hooks.start )
                    hooks.start( hooks.user, e );
            }
            image_put32( drv, 0x100, static_cast< std::uint32_t >( pick ) );
        }
    }
    const std::uint32_t e = static_cast< std::uint32_t >( human_i32( drv, 0xFC ) );
    if ( e != 0 )
    {
        const float vel[ 4 ] = { vx, vy, vz, state_f( t.body, 0x14C ) };
        if ( hooks.attributes )
            hooks.attributes( hooks.user, e, pos, vel );
        if ( boat[ 0x46B ] )
        {
            if ( hooks.stop )
                hooks.stop( hooks.user, e );
            image_put32( drv, 0xFC, 0 );
        }
    }
}

// ---- The drivers' small virtuals (the animation tick's driver vt+0x64 / +0x68 are slots 25 / 26) ----

// Slot 26 0x40EB20 (every driver class): the boat's +0x300 (the steer
// control), returned on x87 (a float load: exact).
inline float driver_steer_value( const std::uint8_t* boat ) { return state_f( boat, 0x300 ); }

// Slots 24 and 25 0x40EB00 (VuDriverEntity, the human, the ghost): 0x41D570
// on the boat (tick.hxx's boat_41D570 - the boat's +0x2FC / +0x308 / +0x32C /
// +0x32F, the component's forward row, the engine's +4 / +0xC and its owner's
// +0x470, the body's velocity), through x87.
inline float driver_entity_throttle( const std::uint8_t* boat, const std::uint8_t* transform, const std::uint8_t* engine,
    const std::uint8_t* body )
{
    solver_rigid_t b = boat_decision_body( body );
    std::uint32_t i308;
    std::memcpy( &i308, boat + 0x308, 4 );
    boat_view_t view{ state_f( boat, 0x2FC ), i308, boat[ 0x32C ], boat[ 0x32F ], { }, state_f( engine, 4 ), state_f( engine, 0xC ),
                      state_f( boat, 0x470 ), b };
    for ( int k = 0; k < 3; ++k )
        view.o50_60[ k ] = state_f( transform, 0x60 + 4 * k );
    return boat_41D570( view );
}

// VuAiDriver's (and the UI / nuisance drivers') slot 24 0x40BD40, and slot 25
// 0x40BDD0 (jmp vt+0x60: slot 24): 1 while the boat's +0x32C, else
// maxss(+0x9C / ((owner +0x470 x (engine +0xC x +4)) x 0.44704), boat +0x2FC).
inline float ai_driver_throttle( const std::uint8_t* driver, const std::uint8_t* boat, const std::uint8_t* engine, const std::uint8_t* owner )
{
    if ( boat[ 0x32C ] )
        return 1.0f;
    const float top = ( state_f( owner, 0x470 ) * ( state_f( engine, 0xC ) * state_f( engine, 4 ) ) ) * ai_bits( 0x3EE4E26Du );
    return ai_maxss( state_f( driver, 0x9C ) / top, state_f( boat, 0x2FC ) );
}

// Slot 23, the driver's current sector: VuAiDriver's 0x40BD20 the plan's first
// point (none without one), the human's 0x40ED30 its +0x84, the ghost's
// 0x40EB40 its +0x80 (the pilots' `sector` fields).
inline int ai_driver_sector( const fleet_plan_t& plan ) { return plan.sectors.empty( ) ? -1 : plan.sectors[ 0 ]; }
inline int human_driver_sector( const human_pilot_t& p ) { return p.sector; }
inline int ghost_driver_sector( const ghost_pilot_t& p ) { return p.sector; }
