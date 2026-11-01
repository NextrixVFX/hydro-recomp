#pragma once

#include <workspace/core/boat/track.hxx>

#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// VuAiManagerImpl (vtable 0x735734, 17 slots; g_vuaimanagerimpl) and the
// VuRandomTrackPlan every VuAiDriver carries at +0x84 - the race logic that
// hands each AI driver its path points (driver +0x8C / +0x90 ARE the plan's
// data / count: boat/ai.hxx's ai_tick reads them) and its top speed (+0x9C).
// The manager: +0x18 / +0x19 RecordDebugEvents / RenderDebugEvents (their JSON
// writer 0x404810 and its save 0x4046F0 are hooks - debug output), +0x1C the
// state (1 init, 2 started, 3 running, 4 ended), +0x20 the race time, +0x24 /
// +0x2C one 0xDC entry per driver (ctor 0x403350), +0x34 / +0x38 the track's
// start / end sectors, +0x3C a byte (1 while running). Its tick (slot 14
// 0x403900) runs 0x403930 in state 3, then the dev-stat page 0x404630 (a
// hook: debug text).

inline float fleet_minss( float a, float b ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); }
inline float fleet_maxss( float a, float b ) { return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); }
inline float fleet_bits( std::uint32_t u ) { float f; std::memcpy( &f, &u, 4 ); return f; }

// VuRandom (0x50FD20 draw, thiscall on {seed, last, table[32]}): Park-Miller
// 16807 x s - 0x7FFFFFFF x (s / 127773) in 32-bit wraparound, plus 0x7FFFFFFF
// when negative; slot = last / 0x4000000 (signed, unmasked - the table index
// retail uses as is); last = table[slot]; table[slot] = seed; the draw is
// minss(cvtdq2ps(last) x 0x30000000 (2^-31), 0x3F7FFFFE) (VuMath_min).
// Retail's AI plans draw from THE global one (0x736810 - statically seed -1
// and a zeroed table; what seeds it in the game is NOT ESTABLISHED, see
// engine utility/random.hxx): the caller owns this state.
struct fleet_random_t
{
    std::int32_t seed = -1;
    std::int32_t last = 0;
    std::int32_t table[ 32 ] = { };

    float draw( )
    {
        const std::int32_t k = seed / 127773;
        std::int32_t s = static_cast< std::int32_t >( static_cast< std::uint32_t >( seed ) * 16807u - static_cast< std::uint32_t >( k ) * 0x7FFFFFFFu );
        if ( s < 0 )
            s = static_cast< std::int32_t >( static_cast< std::uint32_t >( s ) + 0x7FFFFFFFu );
        seed = s;
        const std::int32_t slot = last / 0x4000000;
        last = table[ slot ];
        table[ slot ] = seed;
        return fleet_minss( static_cast< float >( last ) * fleet_bits( 0x30000000u ), fleet_bits( 0x3F7FFFFEu ) );
    }
};

// VuTrackPlan / VuRandomTrackPlan (vtables 0x746CDC / 0x746CC8; ctor
// 0x4D0CC0): +4 the depth (8), +8 / +0xC / +0x10 the sectors (data, count,
// capacity) - sector indices here.
struct fleet_plan_t
{
    std::int32_t depth = 8;
    std::vector< int > sectors;
};

// VuRandomTrackPlan slot 3 0x4D0F10: no next sector - none (no draw); else one
// draw r, then the first next sector whose running sum of branch probabilities
// (+0xC8, from 0) exceeds r (comiss, strict), or next[0] when none does.
inline int fleet_plan_choose( const track_manager_t& tm, int sector, fleet_random_t& rng )
{
    const track_sector_t& s = tm.sectors[ sector ];
    if ( s.next.empty( ) )
        return -1;
    const float r = rng.draw( );
    float sum = 0.0f;
    for ( int n : s.next )
    {
        sum = sum + state_f( tm.sectors[ n ].image, 0xC8 );
        if ( sum > r )
            return n;
    }
    return s.next[ 0 ];
}

// 0x4D0E30: from the last sector, choose (slot 3) and append until the plan
// holds `depth` sectors or there is no next. An empty plan is left.
inline void fleet_plan_fill( fleet_plan_t& plan, const track_manager_t& tm, fleet_random_t& rng )
{
    if ( plan.sectors.empty( ) || plan.sectors.back( ) < 0 )
        return;
    int at = plan.sectors.back( );
    while ( static_cast< std::int32_t >( plan.sectors.size( ) ) < plan.depth )
    {
        at = fleet_plan_choose( tm, at, rng );
        if ( at < 0 )
            break;
        plan.sectors.push_back( at );
    }
}

// Slot 1 0x4D0D50: the plan = { sector }, then filled. (Its capacity growth to
// the depth is storage, not modelled.)
inline void fleet_plan_start( fleet_plan_t& plan, const track_manager_t& tm, int sector, fleet_random_t& rng )
{
    plan.sectors.assign( 1, sector );
    fleet_plan_fill( plan, tm, rng );
}

// Slot 2 0x4D0DD0: drop the first sector (memcpy down, count - 1), refill.
inline void fleet_plan_advance( fleet_plan_t& plan, const track_manager_t& tm, fleet_random_t& rng )
{
    if ( !plan.sectors.empty( ) )
        plan.sectors.erase( plan.sectors.begin( ) );
    fleet_plan_fill( plan, tm, rng );
}

// 0x4D0E10: the sector at `index`, or none.
inline int fleet_plan_at( const fleet_plan_t& plan, int index )
{
    return index < static_cast< int >( plan.sectors.size( ) ) ? plan.sectors[ index ] : -1;
}

// One VuAiDriver as the manager reads and writes it: the driver image
// (0xD4; +0x80 the mode, +0xA0 the signed speed are read; +0x78 the
// checkpoint id and +0x9C the top speed written), +0x7C the sector the race
// put it on, +0x84 its plan, +0xBC the boat it races (a boat index; -1 = 0).
struct fleet_pilot_t
{
    std::uint8_t* driver = nullptr;
    int boat = -1;
    int sector = -1;
    int other = -1;
    fleet_plan_t plan;
};

// A boat as the manager reads it: its transform component's world (+0x50 of
// the component) and its engine (+0x22C) floats +4 / +0xC.
struct fleet_boat_t
{
    const float* world = nullptr;
    float engine_4 = 0.0f;
    float engine_c = 0.0f;
};

// One entry (0xDC, ctor 0x403350 zeroes +0x80..+0xA8 and +0xD4 / +0xD8).
struct fleet_entry_t
{
    float prev[ 16 ] = { };      // +0x00 the boat's world last tick
    float cur[ 16 ] = { };       // +0x40 this tick
    bool matrices_known = false; // unwritten before the start (allocator memory)
    int pilot = -1;              // +0x80 the driver
    int boat = -1;               // +0x84
    int target = -1;             // +0x88 the boat it is set on (-1 = 0)
    int sector = -1;             // +0x8C the plan's first sector (-1 = 0)
    std::uint32_t w90 = 0;       // +0x90 (0; not read by any slot recreated)
    std::uint32_t w94 = 0;       // +0x94 (0; zeroed on a sector change)
    float cap = 0.0f;            // +0x98 the speed cap
    float arrival = 0.0f;        // +0x9C the arrival time (addDriver's float; > 0 enables the arrival speed)
    std::uint32_t wa0 = 0;       // +0xA0
    std::uint32_t wa4 = 0;       // +0xA4
    float wanted = 0.0f;         // +0xA8 the speed handed to the driver
    float ring[ 10 ] = { };      // +0xAC the driver's last ten speeds (slots not yet written: UNKNOWN)
    std::int32_t samples = 0;    // +0xD4
    float timer = 0.0f;          // +0xD8 the target's hold time
};

struct fleet_manager_t
{
    std::uint8_t record = 0;     // +0x18
    std::uint8_t render = 0;     // +0x19
    std::int32_t state = 1;      // +0x1C (1 after slot 5 0x4042E0's init)
    float time = 0.0f;           // +0x20
    std::vector< fleet_entry_t > entries;
    int start = -1;              // +0x34
    int end = -1;                // +0x38
    std::uint8_t running = 0;    // +0x3C
};

// Slot 11 0x403590 (boat, arrival float, cap float): a new entry for the
// boat's driver (+0x220); cap = engine +0xC x +4, and when the given cap is
// > 0 (comiss) minss(given, that) x 0.44704 (mph to m/s - only then);
// wanted = cap.
inline void fleet_add_driver( fleet_manager_t& m, const std::vector< fleet_boat_t >& boats, int pilot, int boat, float arrival, float cap )
{
    fleet_entry_t e;
    e.pilot = pilot;
    e.boat = boat;
    e.arrival = arrival;
    float top = boats[ boat ].engine_c * boats[ boat ].engine_4;
    e.cap = top;
    if ( cap > 0.0f )
        e.cap = fleet_minss( cap, top ) * 0.44703999f;
    e.wanted = e.cap;
    m.entries.push_back( e );
}

// Slot 12 0x4036C0 (boat): the first entry of that boat swapped with the last
// (three memcpy_s) and the count dropped.
inline void fleet_remove_driver( fleet_manager_t& m, int boat )
{
    for ( std::size_t k = 0; k < m.entries.size( ); ++k )
        if ( m.entries[ k ].boat == boat )
        {
            std::swap( m.entries[ k ], m.entries.back( ) );
            m.entries.pop_back( );
            return;
        }
}

// Slot 13 0x4037B0 (boat, &a, &b): the first entry of that boat's +0xA0 /
// +0xA4; false (nothing written) when there is none.
inline bool fleet_query( const fleet_manager_t& m, int boat, std::uint32_t& a, std::uint32_t& b )
{
    for ( const fleet_entry_t& e : m.entries )
        if ( e.boat == boat )
        {
            a = e.wa0;
            b = e.wa4;
            return true;
        }
    return false;
}

// Slot 6 0x4034B0: state 2, the track's start / end (track vt+0x2C / +0x30),
// then 0x404590: per entry both matrices = the boat's world, its sector = the
// driver's +0x7C, the driver's +0x78 = that sector's checkpoint id (+0xD0),
// and the plan started there (slot 1).
inline void fleet_start( fleet_manager_t& m, const track_manager_t& tm, std::vector< fleet_pilot_t >& pilots,
    const std::vector< fleet_boat_t >& boats, fleet_random_t& rng )
{
    m.state = 2;
    m.start = track_start_sector( tm );
    m.end = track_end_sector( tm );
    for ( fleet_entry_t& e : m.entries )
    {
        fleet_pilot_t& p = pilots[ e.pilot ];
        std::memcpy( e.cur, boats[ e.boat ].world, 64 );
        std::memcpy( e.prev, boats[ e.boat ].world, 64 );
        e.matrices_known = true;
        e.sector = p.sector;
        std::memcpy( p.driver + 0x78, tm.sectors[ p.sector ].image + 0xD0, 4 );
        fleet_plan_start( p.plan, tm, p.sector, rng );
    }
}

// Slot 7 0x4034E0: running (state 3, time 0, +0x3C 1). Slot 8 0x403500:
// state 4, +0x3C 0, then 0x4046F0 (with RecordDebugEvents the aidebug.json
// save - a hook).
inline void fleet_run( fleet_manager_t& m )
{
    m.state = 3;
    m.time = 0.0f;
    m.running = 1;
}

inline void fleet_end( fleet_manager_t& m )
{
    m.state = 4;
    m.running = 0;
}

// The sector fraction 0x403930 tests: d0 = plane 0's distance, d3 plane 3's,
// d0 / (d3 + d0) - 0 at the sector's start edge, 1 at its end edge.
inline float fleet_fraction( const std::uint8_t* s, float x, float y, float z )
{
    const float d0 = ( ( state_f( s, 0x40 ) * x + state_f( s, 0x44 ) * y ) + state_f( s, 0x48 ) * z ) + state_f( s, 0x4C );
    const float d3 = ( ( ( state_f( s, 0x70 ) * x + state_f( s, 0x74 ) * y ) + state_f( s, 0x78 ) * z ) + state_f( s, 0x7C ) ) + d0;
    return d0 / d3;
}

// The position 0x403930 takes from a matrix: ((r1 + r0) + r2) x 0 + r3 per
// lane - the origin through the matrix, written out.
inline void fleet_origin( const float* m, float* out )
{
    for ( int lane = 0; lane < 3; ++lane )
        out[ lane ] = ( ( m[ 4 + lane ] + m[ lane ] ) + m[ 8 + lane ] ) * 0.0f + m[ 12 + lane ];
}

// 0x403800 (thiscall; boat, a driver to skip): among the entries whose driver
// has mode (+0x80) >= 1 and is not the one skipped, the nearest whose current
// position (+0x70) is within 25 m of the boat's world position (squared
// ((dy^2 + dx^2) + dz^2) < 625 and below the best, both comiss) gets the boat:
// its driver's +0xBC, its +0x88, and its timer +0xD8 = 5.
inline void fleet_assign_nearest( fleet_manager_t& m, std::vector< fleet_pilot_t >& pilots, const std::vector< fleet_boat_t >& boats, int boat,
    int skip )
{
    float best = 3.4028235e38f;
    int pick = -1;
    const float* b = boats[ boat ].world + 12;
    for ( std::size_t k = 0; k < m.entries.size( ); ++k )
    {
        const fleet_entry_t& e = m.entries[ k ];
        std::int32_t mode;
        std::memcpy( &mode, pilots[ e.pilot ].driver + 0x80, 4 );
        if ( mode < 1 || e.pilot == skip )
            continue;
        const float dy = e.cur[ 13 ] - b[ 1 ], dx = e.cur[ 12 ] - b[ 0 ], dz = e.cur[ 14 ] - b[ 2 ];
        const float d = ( dy * dy + dx * dx ) + dz * dz;
        if ( 625.0f > d && best > d )
        {
            pick = static_cast< int >( k );
            best = d;
        }
    }
    if ( pick >= 0 )
    {
        fleet_entry_t& e = m.entries[ pick ];
        pilots[ e.pilot ].other = boat;
        e.target = boat;
        e.timer = 5.0f;
    }
}

// 0x403930 (usercall: ecx the manager, xmm1 dt), from its disassembly. The
// time += dt. Per entry: prev = cur, cur = the boat's world; with a sector:
// when the origin of prev is before the sector's end edge (1 > fraction) and
// the origin of cur at or past it (>= 1, both ordered), the plan advances
// (slot 2), the sector = the plan's first (or none), +0x94 = 0 (the
// SectorExit / SectorEnter debug records are a hook); the driver's speed goes
// into the ring (slot samples % 10), the average of the min(samples, 10)
// slots summed in slot order; wanted = cap; with a sector and an arrival
// time > 0 the remaining length (1 - fraction of cur's +0x70 row) x length,
// plus every next[0] sector's length up to (not including) the track's end,
// sets wanted = 89.408 once the time is past the arrival, else, when
// remaining / average + time > arrival, maxss(44.704, minss(89.408,
// remaining / (arrival - time))); then the speed hints (+0xB4 / +0xB8, the
// wanted when not > 0) blended by the clamped fraction of cur's origin cap a
// wanted above them (minss(cap, blend)). The driver's +0x9C = wanted (the
// RenderDebugEvents record a hook). Then per boat of VuBoatManager: every
// entry set on it counts its timer down by dt and drops it (driver +0xBC and
// +0x88 cleared) once 0 >= the timer; with no entry on it, or the last one
// dropped, 0x403800 hands it on (skipping the last such entry's driver).
inline void fleet_tick_running( fleet_manager_t& m, const track_manager_t& tm, std::vector< fleet_pilot_t >& pilots,
    const std::vector< fleet_boat_t >& boats, fleet_random_t& rng, float dt )
{
    m.time = dt + m.time;
    for ( fleet_entry_t& e : m.entries )
    {
        fleet_pilot_t& p = pilots[ e.pilot ];
        std::memcpy( e.prev, e.cur, 64 );
        std::memcpy( e.cur, boats[ e.boat ].world, 64 );
        if ( e.sector >= 0 )
        {
            float was[ 3 ], now[ 3 ];
            fleet_origin( e.prev, was );
            fleet_origin( e.cur, now );
            const std::uint8_t* s = tm.sectors[ e.sector ].image;
            const float before = fleet_fraction( s, was[ 0 ], was[ 1 ], was[ 2 ] );
            if ( _mm_comilt_ss( _mm_set_ss( before ), _mm_set_ss( 1.0f ) ) &&
                 _mm_comige_ss( _mm_set_ss( fleet_fraction( s, now[ 0 ], now[ 1 ], now[ 2 ] ) ), _mm_set_ss( 1.0f ) ) )
            {
                fleet_plan_advance( p.plan, tm, rng );
                e.sector = p.plan.sectors.empty( ) ? -1 : p.plan.sectors[ 0 ];
                e.w94 = 0;
            }
            std::memcpy( &e.ring[ e.samples % 10 ], p.driver + 0xA0, 4 );
            ++e.samples;
            const std::int32_t n = e.samples < 10 ? e.samples : 10;
            float sum = 0.0f;
            for ( std::int32_t k = 0; k < n; ++k )
                sum = sum + e.ring[ k ];
            const float average = sum / static_cast< float >( n );
            e.wanted = e.cap;
            if ( e.sector >= 0 )
            {
                const track_sector_t& here = tm.sectors[ e.sector ];
                const std::uint8_t* q = here.image;
                if ( e.arrival > 0.0f )
                {
                    const float x = e.cur[ 12 ], y = e.cur[ 13 ], z = e.cur[ 14 ];
                    float remaining = ( 1.0f - fleet_fraction( q, x, y, z ) ) * state_f( q, 0xB0 );
                    const track_sector_t* walk = &here;
                    while ( !walk->next.empty( ) )
                    {
                        const int n0 = walk->next[ 0 ];
                        if ( n0 == m.end )
                            break;
                        walk = &tm.sectors[ n0 ];
                        remaining = remaining + state_f( walk->image, 0xB0 );
                    }
                    if ( m.time > e.arrival )
                        e.wanted = fleet_bits( 0x42B2D0E5u );
                    else if ( remaining / average + m.time > e.arrival )
                        e.wanted = fleet_maxss( fleet_bits( 0x4232D0E5u ), fleet_minss( fleet_bits( 0x42B2D0E5u ), remaining / ( e.arrival - m.time ) ) );
                }
                float hint_a = state_f( q, 0xB4 );
                if ( !( hint_a > 0.0f ) )
                    hint_a = e.wanted;
                float hint_b = state_f( q, 0xB8 );
                if ( !( hint_b > 0.0f ) )
                    hint_b = e.wanted;
                const float t = fleet_maxss( 0.0f, fleet_minss( 1.0f, fleet_fraction( q, now[ 0 ], now[ 1 ], now[ 2 ] ) ) );
                const float blend = ( 1.0f - t ) * hint_a + t * hint_b;
                if ( e.wanted > blend )
                    e.wanted = fleet_minss( e.cap, blend );
            }
        }
        state_put_f( p.driver, 0x9C, e.wanted );
    }
    for ( int b = 0; b < static_cast< int >( boats.size( ) ); ++b )
    {
        bool found = false;
        bool dropped = false;
        int last = -1;
        for ( fleet_entry_t& e : m.entries )
        {
            if ( e.target != b )
                continue;
            const float t = e.timer - dt;
            found = true;
            last = e.pilot;
            e.timer = t;
            if ( _mm_comige_ss( _mm_set_ss( 0.0f ), _mm_set_ss( t ) ) )
            {
                e.target = -1;
                pilots[ e.pilot ].other = -1;
                dropped = true;
            }
            else
                dropped = false;
        }
        if ( !found || dropped )
            fleet_assign_nearest( m, pilots, boats, b, last );
    }
}

// Slot 14 0x403900 (thiscall; dt): 0x403930 in state 3, then 0x404630 (the
// "AI" dev-stat page - a hook).
inline void fleet_tick( fleet_manager_t& m, const track_manager_t& tm, std::vector< fleet_pilot_t >& pilots,
    const std::vector< fleet_boat_t >& boats, fleet_random_t& rng, float dt )
{
    if ( m.state == 3 )
        fleet_tick_running( m, tm, pilots, boats, rng, dt );
}
