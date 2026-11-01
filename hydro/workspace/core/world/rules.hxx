#pragma once

#include <workspace/core/boat/track.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <xmmintrin.h>

// THE RACE RULES - VuGame's per-frame race bookkeeping and VuRaceGame's own
// part, recreated from the disassembly / Hex-Rays (world batch 19):
//
//   VuGame's tick 0x482F10 (__usercall: ecx the game, xmm1 dt), reached every
//   frame from VuActionGameMode's "Run" state tick 0x4BAB50, which the game
//   mode manager's Decision tick 0x4CBFD0 runs through the mode's slot 4
//   0x4B99E0 (its FSM): 0x483230 (each boat's distances along the track),
//   0x483540 (the checkpoints: laps, lap times, the finish), 0x483AD0 (the
//   sixth finished AI boat hidden), the game's slot 7 (VuRaceGame 0x489A40: its
//   +0x6F8 state machine), then VuGame's two own state machines (+0x0C, +0x67C:
//   hooks here), then each boat's +0x4DC = its transform's world (the previous
//   position the next frame's checkpoint test starts from).
//
// The boat image fields (the 0x520 VuBoatEntity) these read and write:
//   +0x308 the input-off count (the drivers ignore input while non-zero),
//   +0x32F finished, +0x4AC the lap (0 before the start line), +0x4B0 the lap
//   time, +0x4B4 the race time, +0x4B8 the best lap, +0x4BC the distance left
//   in the lap, +0x4C0 the distance done in the lap, +0x4C4 the distance left in
//   the race, +0x4C8 the distance done in the race, +0x4CC the place (0 first),
//   +0x4D8 the next checkpoint (a VuAiWaypointEntity; here a waypoint index, -1
//   none), +0x4DC the world last frame (row 3 at +0x50C).

inline float rules_f( const std::uint8_t* b, int at )
{
    float f;
    std::memcpy( &f, b + at, 4 );
    return f;
}
inline void rules_put_f( std::uint8_t* b, int at, float v ) { std::memcpy( b + at, &v, 4 ); }
inline std::int32_t rules_i32( const std::uint8_t* b, int at )
{
    std::int32_t v;
    std::memcpy( &v, b + at, 4 );
    return v;
}
inline void rules_put_i32( std::uint8_t* b, int at, std::int32_t v ) { std::memcpy( b + at, &v, 4 ); }
inline float rules_minss( float a, float b ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); }

// One of the game's boats (+0x38 list, +0x3C count) as the rules see it.
// `sector` is the driver's vt+0x5C: VuHumanDriver 0x40ED30 = its +0x84 (the
// human's current sector), VuAiDriver 0x40BD20 = +0x90 > 0 ? [+0x8C][0] : null
// (its plan's first sector) - the sector's image or null. `human` the driver's
// slot 12 predicate.
struct race_rules_boat_t
{
    std::uint8_t* boat = nullptr;
    const std::uint8_t* transform = nullptr;   // boat +0x50's component: world at +0x50, row 3 at +0x80
    std::uint8_t* driver = nullptr;            // boat +0x220's image (+0x78 written)
    const std::uint8_t* sector = nullptr;
    int checkpoint = -1;                       // +0x4D8
    bool human = false;
};

// The VuGame fields the rules read: +0x44 a circuit, +0x48 the track length,
// +0x4C the lap count; VuRaceGame's +0x720 the place order (boat indices).
struct race_rules_game_t
{
    std::uint8_t circuit = 0;
    float length = 0.0f;
    std::int32_t laps = 0;
    std::vector< int > order;
};

// ---- 0x483230: the distances --------------------------------------------------
//
// For each boat whose driver has a sector q (row 3 p of its world): a =
// ((q+0x44 p.y + q+0x40 p.x) + q+0x48 p.z) + q+0x4C (the sector's entry
// plane), b = ((q+0x74 p.y + p.x q+0x70) + q+0x78 p.z) + q+0x7C (its exit
// plane), f = minss(1.0, a / (b + a)); +0x4BC = q+0xBC - q+0xB0 f; +0x4C0 =
// length - that. A circuit: +0x4C8 = (float)lap x length - +0x4BC, +0x4C4 =
// (float)laps x length - +0x4C8; else +0x4C8 = +0x4C0, +0x4C4 = +0x4BC.
inline void race_rules_distances( const race_rules_game_t& g, std::vector< race_rules_boat_t >& boats )
{
    for ( race_rules_boat_t& r : boats )
    {
        const std::uint8_t* q = r.sector;
        if ( !q )
            continue;
        const float x = rules_f( r.transform, 0x80 ), y = rules_f( r.transform, 0x84 ), z = rules_f( r.transform, 0x88 );
        const float a = ( ( rules_f( q, 0x44 ) * y + rules_f( q, 0x40 ) * x ) + rules_f( q, 0x48 ) * z ) + rules_f( q, 0x4C );
        const float b = ( ( rules_f( q, 0x74 ) * y + x * rules_f( q, 0x70 ) ) + rules_f( q, 0x78 ) * z ) + rules_f( q, 0x7C );
        const float f = rules_minss( 1.0f, a / ( b + a ) );
        const float left = rules_f( q, 0xBC ) - rules_f( q, 0xB0 ) * f;
        rules_put_f( r.boat, 0x4BC, left );
        const float done = g.length - left;
        rules_put_f( r.boat, 0x4C0, done );
        if ( g.circuit )
        {
            const float total = static_cast< float >( rules_i32( r.boat, 0x4AC ) ) * g.length - rules_f( r.boat, 0x4BC );
            rules_put_f( r.boat, 0x4C8, total );
            rules_put_f( r.boat, 0x4C4, static_cast< float >( g.laps ) * g.length - total );
        }
        else
        {
            rules_put_f( r.boat, 0x4C8, done );
            rules_put_f( r.boat, 0x4C4, left );
        }
    }
}

// ---- 0x483540: the checkpoints ------------------------------------------------
//
// A checkpoint as the test reads its VuAiWaypointEntity: the transform's world
// row 1 (+0x60, the line's normal) and row 3 (+0x80, its centre), +0x8A Finish,
// +0x90 CheckPoint Width, +0x94 CheckPoint Height.
struct race_checkpoint_view_t
{
    float normal[ 3 ];
    float origin[ 3 ];
    std::uint8_t finish;
    float width, height;
};

struct race_checkpoint_hooks_t
{
    // The checkpoint's view (false: unknown - the boat is skipped, counted).
    std::function< bool( int waypoint, race_checkpoint_view_t& ) > view;
    // The track manager g 0x758FF4: slot 19 vt+0x4C 0x4D0900 (the checkpoint's
    // id) and slot 15 vt+0x3C 0x4D0820 (the next checkpoint, -1 none).
    std::function< std::int32_t( int waypoint ) > index;
    std::function< int( int waypoint ) > next;
    // The game's vt+0x3C (VuRaceGame slot 15 0x48B180, the split / result
    // display) with the boat and the checkpoint crossed.
    std::function< void( int boat, int waypoint ) > crossed;
    // 0x483B60's g_vuaimanagerimpl vt+0x30 (slot 12 0x4036C0) on the boat.
    std::function< void( int boat ) > finished;
    int unknown = 0;
};

// 0x483B60 (cdecl boat): +0x308 += 1, +0x32F = 1, the AI manager's slot 12.
inline void race_rules_finish( race_rules_boat_t& r, int index, const race_checkpoint_hooks_t& h )
{
    rules_put_i32( r.boat, 0x308, rules_i32( r.boat, 0x308 ) + 1 );
    r.boat[ 0x32F ] = 1;
    if ( h.finished )
        h.finished( index );
}

// For each unfinished boat with a next checkpoint c: the step from P (+0x50C,
// last frame) to C (the world's row 3) against c's line. a = ((n0 (P0 - o0) +
// n1 (P1 - o1)) + n2 (P2 - o2)), b the same at C; comiss 0, b a / jb: not
// crossed. The crossing point: |a| = A, inv = 1 / (|b| + A), px = inv ((C0 -
// P0) A) + P0, py likewise; inside when width x 0.5 > VuMath_sqrt(ry^2 + rx^2)
// (ry = py - o1, rx = px - o0) and C2 - o2 > -10.0 and height > C2 - o2. Then
// driver +0x78 = slot 19(c), +0x4D8 = slot 15(c), the game's vt+0x3C(boat, c);
// and when c is a Finish or the boat is still on lap 0: with lap >= 1, +0x4B8 =
// minss(+0x4B8, +0x4B0) and +0x4B0 = 0; lap == the lap count -> 0x483B60; lap
// += 1; +0x4BC = the track length.
inline void race_rules_checkpoints( const race_rules_game_t& g, std::vector< race_rules_boat_t >& boats, race_checkpoint_hooks_t& h )
{
    for ( std::size_t k = 0; k < boats.size( ); ++k )
    {
        race_rules_boat_t& r = boats[ k ];
        if ( r.boat[ 0x32F ] || r.checkpoint < 0 )
            continue;
        race_checkpoint_view_t c{};
        if ( !h.view || !h.view( r.checkpoint, c ) )
        {
            ++h.unknown;
            continue;
        }
        const float p0 = rules_f( r.boat, 0x50C ), p1 = rules_f( r.boat, 0x510 ), p2 = rules_f( r.boat, 0x514 );
        const float c0 = rules_f( r.transform, 0x80 ), c1 = rules_f( r.transform, 0x84 ), c2 = rules_f( r.transform, 0x88 );
        const float a = ( c.normal[ 0 ] * ( p0 - c.origin[ 0 ] ) + c.normal[ 1 ] * ( p1 - c.origin[ 1 ] ) ) + c.normal[ 2 ] * ( p2 - c.origin[ 2 ] );
        const float b = ( c.normal[ 0 ] * ( c0 - c.origin[ 0 ] ) + c.normal[ 1 ] * ( c1 - c.origin[ 1 ] ) ) + c.normal[ 2 ] * ( c2 - c.origin[ 2 ] );
        if ( !( 0.0f >= b * a ) )
            continue;
        const float aa = std::fabs( a );
        const float dx = ( c0 - p0 ) * aa, dy = ( c1 - p1 ) * aa;
        const float inv = 1.0f / ( std::fabs( b ) + aa );
        const float ry = ( inv * dy + p1 ) - c.origin[ 1 ];
        const float rx = ( inv * dx + p0 ) - c.origin[ 0 ];
        const float d = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ry * ry + rx * rx ) ) );
        if ( !( c.width * 0.5f > d ) )
            continue;
        const float dz = c2 - c.origin[ 2 ];
        if ( !( dz > -10.0f ) || !( c.height > dz ) )
            continue;
        const int crossed = r.checkpoint;
        if ( r.driver && h.index )
            rules_put_i32( r.driver, 0x78, h.index( crossed ) );
        r.checkpoint = h.next ? h.next( crossed ) : -1;
        if ( h.crossed )
            h.crossed( static_cast< int >( k ), crossed );
        const std::int32_t lap = rules_i32( r.boat, 0x4AC );
        if ( !c.finish && lap != 0 )
            continue;
        if ( lap >= 1 )
        {
            rules_put_f( r.boat, 0x4B8, rules_minss( rules_f( r.boat, 0x4B8 ), rules_f( r.boat, 0x4B0 ) ) );
            rules_put_i32( r.boat, 0x4B0, 0 );
        }
        if ( lap == g.laps )
            race_rules_finish( r, static_cast< int >( k ), h );
        rules_put_i32( r.boat, 0x4AC, rules_i32( r.boat, 0x4AC ) + 1 );
        rules_put_f( r.boat, 0x4BC, g.length );
    }
}

// Slot 15 0x4D0820 (thiscall track; waypoint): its index i in the checkpoint
// list (+0x28 data, +0x20 count), none -> null; j = i + 1, taken modulo the
// count on a circuit (slot 9); j past the list -> null; else checkpoints[j].
inline int race_rules_next_checkpoint( const track_manager_t& tm, int waypoint )
{
    const int n = static_cast< int >( tm.checkpoints.size( ) );
    for ( int i = 0; i < n; ++i )
    {
        if ( tm.checkpoints[ static_cast< std::size_t >( i ) ] != waypoint )
            continue;
        int j = i + 1;
        if ( track_is_circuit( tm ) )
            j %= n;
        return j < n ? tm.checkpoints[ static_cast< std::size_t >( j ) ] : -1;
    }
    return -1;
}

// ---- 0x4831D0 / 0x48AB20: the clocks and the places --------------------------
//
// 0x4831D0 (__usercall ecx the game, xmm1 dt): each unfinished boat's +0x4B4
// += dt, and +0x4B0 += dt once its lap is >= 1.
inline void race_rules_clocks( std::vector< race_rules_boat_t >& boats, float dt )
{
    for ( race_rules_boat_t& r : boats )
    {
        if ( r.boat[ 0x32F ] )
            continue;
        rules_put_f( r.boat, 0x4B4, dt + rules_f( r.boat, 0x4B4 ) );
        if ( rules_i32( r.boat, 0x4AC ) >= 1 )
            rules_put_f( r.boat, 0x4B0, rules_f( r.boat, 0x4B0 ) + dt );
    }
}

// The order predicate MSVC's std::sort inlines (0x48BB30 / 0x48B460 /
// 0x48B9C0): a before b when a finished and (b did not, or b's race time
// +0x4B4 is greater: comiss, so a NaN orders nothing); an unfinished a never
// before a finished b; else the higher lap (+0x4AC) first, then the greater
// lap distance left +0x4BC of b (a has less left).
inline bool race_rules_before( const std::uint8_t* a, const std::uint8_t* b )
{
    if ( a[ 0x32F ] )
        return !b[ 0x32F ] || _mm_comigt_ss( _mm_set_ss( rules_f( b, 0x4B4 ) ), _mm_set_ss( rules_f( a, 0x4B4 ) ) );
    if ( b[ 0x32F ] )
        return false;
    const std::int32_t la = rules_i32( a, 0x4AC ), lb = rules_i32( b, 0x4AC );
    if ( la != lb )
        return la > lb;
    return _mm_comigt_ss( _mm_set_ss( rules_f( b, 0x4BC ) ), _mm_set_ss( rules_f( a, 0x4BC ) ) );
}

// 0x48AB20 (thiscall game): std::sort 0x48B360 over the +0x720 order list with
// that predicate, then each boat's +0x4CC = its place. Retail's sort is MSVC's
// introsort; at 32 elements or fewer it is the insertion sort 0x48BB30 alone,
// recreated here 1:1 (an element less than the first is moved to the front,
// else walked back while it is less than its left neighbour). More than 32
// boats takes the partition / heap paths (0x48B460 / 0x48B9C0 / 0x48BAE0) -
// not recreated: false, nothing sorted (UNKNOWN).
inline bool race_rules_places( race_rules_game_t& g, std::vector< race_rules_boat_t >& boats )
{
    std::vector< int >& o = g.order;
    if ( o.size( ) > 32 )
        return false;
    const auto less = [ & ]( int x, int y ) { return race_rules_before( boats[ static_cast< std::size_t >( x ) ].boat, boats[ static_cast< std::size_t >( y ) ].boat ); };
    for ( std::size_t k = 1; k < o.size( ); ++k )
    {
        const int v = o[ k ];
        if ( less( v, o[ 0 ] ) )
        {
            std::memmove( &o[ 1 ], &o[ 0 ], k * sizeof( int ) );
            o[ 0 ] = v;
            continue;
        }
        std::size_t i = k;
        while ( less( v, o[ i - 1 ] ) )
        {
            o[ i ] = o[ i - 1 ];
            --i;
        }
        o[ i ] = v;
    }
    for ( std::size_t p = 0; p < boats.size( ) && p < o.size( ); ++p )
        rules_put_i32( boats[ static_cast< std::size_t >( o[ p ] ) ].boat, 0x4CC, static_cast< std::int32_t >( p ) );
    return true;
}
