#pragma once

#include <workspace/core/boat/track.hxx>
#include <workspace/core/entity/node.hxx>
#include <workspace/core/entity/property.hxx>
#include <workspace/core/ui/fsm.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
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

// ---- 0x483AD0: the sixth finished AI boat ----------------------------------
//
// Over the boat manager's AI list (g 0x758FB8 +0x30 data / +0x34 count): the
// finished (+0x32F) and not yet hidden (+0x494) boats are counted and the one
// with the greatest +0x330 (comiss > the running best, which starts at 0.0;
// the index starts at 0) is remembered; with more than five such boats that
// one is hidden - its draw component (+0x6C) taken from g_vu3ddrawmanager
// 0x7590B8 (vt+0x2C) when its +0x1E is set (then +0x1E = 0), the boat's
// vt+0x48 (slot 18 0x421940, the disable), +0x494 = 1. `hide` is those calls
// on that AI boat index.
inline void race_rules_hide_finished( const std::vector< std::uint8_t* >& ais, const std::function< void( int ai ) >& hide )
{
    int count = 0, best = 0;
    float most = 0.0f;
    for ( std::size_t k = 0; k < ais.size( ); ++k )
    {
        const std::uint8_t* b = ais[ k ];
        if ( !b[ 0x32F ] || b[ 0x494 ] )
            continue;
        ++count;
        const float v = rules_f( b, 0x330 );
        if ( _mm_comigt_ss( _mm_set_ss( v ), _mm_set_ss( most ) ) )
        {
            best = static_cast< int >( k );
            most = v;
        }
    }
    if ( count <= 5 )
        return;
    if ( hide )
        hide( best );
    ais[ static_cast< std::size_t >( best ) ][ 0x494 ] = 1;
}

// ---- VuRaceGame's own part: the ctor 0x489030 and slot 7 0x489A40 ----------
//
// The ctor, past its base 0x480BA0: +0x6F8 .. +0x734 and +0x744 zeroed (the
// +0x6F8 VuFSM's words and the checkpoint-buoy mask); two event methods
// through 0x5156E0 (VuMethod1<VuRaceGame, void, VuParams const&>, vtable
// 0x73F8EC): "OnStartLightsOut" -> 0x48A780, "OnCheckPointBuoyHit" ->
// 0x48A7B0; the FSM's states (addState 0x555850; VuMethod0 vtable 0x73F8DC,
// VuMethod1<float> 0x73F8CC): PreGame {enter 0x48A7F0, exit 0x48A860, tick
// 0x48A8B0, draw 0x486C00}, Game {0x48A910, 0x4872A0, 0x48A970, 0x487370},
// PostGame {0x48AB70, 0x48ADE0, 0x48AE80, 0x48AF30}, Exit {none}; transitions
// (addTransition 0x555920): PreGame -> Game "StartLightsOut", Game -> PostGame
// "HumanFinished", Game -> PostGame "EveryoneFinished", "" (every state) ->
// Exit "Exit"; then the three HUD projects loaded by 0x4B4360 into +0x738
// (HUDs/RacePreGameHUD), +0x73C (HUDs/RaceGameHUD), +0x740
// (HUDs/RacePostGameHUD). The machine is the UI agent's verified VuFSM
// (ui/fsm.hxx c_fsm); the time in the state is its +0x24 (game +0x71C).
//
// What the states reach that is not the rules' (hooks, each named):
//   hud(project, verb): a HUD project's gameInitialize (when its root's +0x14
//     & 4 is clear: g_vu_entity_repository vt+0x18, 0x500B10, 0x501260
//     "OnProjectInitialized"), its release (when set: 0x500B80, repository
//     vt+0x1C), its tick (the root, when it is a VuHUDEntity - RTTI 0x7362AC -
//     vt+0x30(dt)) and its draw (vt+0x34(1.0)) - the HUD's update / drawing is
//     the UI's; the project load / init / release run through the world.
//   platform(name): PostGame's enter 0x48AB70 after its HUD - the broadcast
//     "OnSetAllowPause" {false}, the progress manager's vt+0x34 (race results to
//     the save), the leaderboard 0x4B0650 ("Race", -(int)(time x 1000)) when the
//     human did not quit (+0x334 clear), the voice-over vt+0x1C ("FinishFirst" /
//     "FinishSecond" / "FinishThird" / "FinishLast" / "FinishLessThanThird") and
//     vt+0x20 "DidNotUseBoost"; its exit 0x48ADE0's "DingDong" achievement -
//     saves / online / achievements / audio: platform services (BRIEF).
enum class race_hud_verb_t { initialize, release, tick };

struct race_rules_hooks_t
{
    std::function< void( int project, race_hud_verb_t verb, float dt ) > hud;
    std::function< void( const char* what ) > platform;
};

struct race_rules_t
{
    c_fsm fsm;                       // +0x6F8
    std::uint32_t buoys = 0;         // +0x744
    race_rules_hooks_t hooks;
    // The tick's inputs, refreshed by the caller before slot 7: the game's
    // boats, the boat manager's human list.
    std::vector< race_rules_boat_t >* boats = nullptr;
    std::vector< std::uint8_t* > humans;
    race_rules_game_t* game = nullptr;
    bool sorted = true;              // false once race_rules_places refused (more than 32 boats)
};

// 0x48A7F0 / 0x48A860: every game boat's +0x308 += 1 / -= 1 (the drivers'
// input held off), then the PreGame HUD project's gameInitialize / release.
inline void race_rules_hold( race_rules_t& r, int by )
{
    if ( r.boats )
        for ( race_rules_boat_t& b : *r.boats )
            rules_put_i32( b.boat, 0x308, rules_i32( b.boat, 0x308 ) + by );
}

inline void race_rules_hud( race_rules_t& r, int project, race_hud_verb_t verb, float dt = 0.0f )
{
    if ( r.hooks.hud )
        r.hooks.hud( project, verb, dt );
}

// 0x48A970 (Game) / 0x48AE80 (PostGame) begin with 0x4831D0(dt) and 0x48AB20.
inline void race_rules_clock_and_place( race_rules_t& r, float dt )
{
    if ( !r.boats || !r.game )
        return;
    race_rules_clocks( *r.boats, dt );
    if ( !race_rules_places( *r.game, *r.boats ) )
        r.sorted = false;
}

inline void race_rules_construct( race_rules_t& r )
{
    r.fsm = c_fsm{ };
    r.buoys = 0;
    fsm_state_t& pre = r.fsm.add_state( "PreGame" );
    pre.m_enter = [ &r ] { race_rules_hold( r, 1 ); race_rules_hud( r, 0, race_hud_verb_t::initialize ); };
    pre.m_exit = [ &r ] { race_rules_hold( r, -1 ); race_rules_hud( r, 0, race_hud_verb_t::release ); };
    pre.m_tick = [ &r ]( float dt ) { race_rules_hud( r, 0, race_hud_verb_t::tick, dt ); };
    pre.m_address[ 0 ] = 0x48A7F0u, pre.m_address[ 1 ] = 0x48A860u, pre.m_address[ 2 ] = 0x48A8B0u, pre.m_address[ 3 ] = 0x486C00u;
    fsm_state_t& game = r.fsm.add_state( "Game" );
    game.m_enter = [ &r ] { race_rules_hud( r, 1, race_hud_verb_t::initialize ); };
    game.m_exit = [ &r ] { race_rules_hud( r, 1, race_hud_verb_t::release ); };
    // 0x48A970: the clocks and places; HumanFinished = the first human's +0x32F
    // (0 with no human), EveryoneFinished = the AND of every game boat's +0x32F
    // (1 with none) - stored into the conditions' +28; then the HUD's tick.
    game.m_tick = [ &r ]( float dt ) {
        race_rules_clock_and_place( r, dt );
        const std::uint8_t human = r.humans.empty( ) ? 0 : r.humans[ 0 ][ 0x32F ];
        std::uint8_t everyone = 1;
        if ( r.boats )
            for ( const race_rules_boat_t& b : *r.boats )
                everyone &= b.boat[ 0x32F ];
        r.fsm.set_condition( "HumanFinished", human != 0 );
        r.fsm.set_condition( "EveryoneFinished", everyone != 0 );
        race_rules_hud( r, 1, race_hud_verb_t::tick, dt );
    };
    game.m_address[ 0 ] = 0x48A910u, game.m_address[ 1 ] = 0x4872A0u, game.m_address[ 2 ] = 0x48A970u, game.m_address[ 3 ] = 0x487370u;
    fsm_state_t& post = r.fsm.add_state( "PostGame" );
    post.m_enter = [ &r ] {
        race_rules_hud( r, 2, race_hud_verb_t::initialize );
        if ( r.hooks.platform )
            r.hooks.platform( "PostGame enter 0x48AB70: OnSetAllowPause, the progress manager, the leaderboard, the voice-over" );
    };
    post.m_exit = [ &r ] {
        race_rules_hud( r, 2, race_hud_verb_t::release );
        if ( r.hooks.platform )
            r.hooks.platform( "PostGame exit 0x48ADE0: the DingDong achievement" );
    };
    // 0x48AE80: the clocks and places; "Exit" = the time in the state >= 4.0.
    post.m_tick = [ &r ]( float dt ) {
        race_rules_clock_and_place( r, dt );
        r.fsm.set_condition( "Exit", r.fsm.m_time >= 4.0f );
        race_rules_hud( r, 2, race_hud_verb_t::tick, dt );
    };
    post.m_address[ 0 ] = 0x48AB70u, post.m_address[ 1 ] = 0x48ADE0u, post.m_address[ 2 ] = 0x48AE80u, post.m_address[ 3 ] = 0x48AF30u;
    r.fsm.add_state( "Exit" );
    r.fsm.add_transition( "PreGame", "Game", "StartLightsOut" );
    r.fsm.add_transition( "Game", "PostGame", "HumanFinished" );
    r.fsm.add_transition( "Game", "PostGame", "EveryoneFinished" );
    r.fsm.add_transition( "", "Exit", "Exit" );
}

// VuRaceGame slot 5 0x4896A0's "the state machine enters its first state":
// the current state's (+0x6F8 -> PreGame) enter.
inline void race_rules_start( race_rules_t& r )
{
    if ( fsm_state_t* s = r.fsm.current( ) )
        if ( s->m_enter )
            s->m_enter( );
}

// 0x48A780 ("OnStartLightsOut"): the condition StartLightsOut's +28 = 1.
inline void race_rules_on_start_lights_out( race_rules_t& r ) { r.fsm.set_condition( "StartLightsOut", true ); }

// 0x48A7B0 ("OnCheckPointBuoyHit"): 0x4237B0 reads the params' first entry (a
// 4-byte tag, then when the tag is 0 the 4-byte value; else 0): +0x744 |= 1 <<
// that value (shl: the count taken mod 32).
inline void race_rules_on_buoy_hit( race_rules_t& r, const std::uint8_t* params, std::int32_t size )
{
    std::int32_t value = 0;
    if ( size > 0 )
    {
        std::int32_t tag = -1;
        std::memcpy( &tag, params, 4 );
        if ( tag == 0 && size >= 8 )
            std::memcpy( &value, params + 4, 4 );
    }
    r.buoys |= 1u << ( static_cast< std::uint32_t >( value ) & 31u );
}

// Slot 7 0x489A40(dt): evaluate 0x555AD0, the current state's tick, +0x71C +=
// dt; false once the current state's name is "Exit".
inline bool race_rules_tick( race_rules_t& r, float dt )
{
    r.fsm.evaluate( );
    fsm_state_t* s = r.fsm.current( );
    if ( s && s->m_tick )
        s->m_tick( dt );
    r.fsm.m_time = r.fsm.m_time + dt;
    s = r.fsm.current( );
    return !s || s->m_name != "Exit";
}
// ---- VuMultiDelayEntity (create 0x5DFD00, ctor 0x5DFD80) ---------------------
//
// The countdown's clock (the HUD template HUD/Countdown). Eight VuBasicProperty
// <float,1> "Delay 1" .. "Delay 8" -> +0x60 .. +0x7C (default 0); the input
// plug "In" (0x5E2AD0 -> 0x5E0660); outputs "1" .. "8" (+0x80 .. +0x9C); its
// gameInitialize 0x5E0610 registers 0x5E06B0 in "Decision".
struct multi_delay_state_t
{
    float value[ 8 ] = { };
    float defaults[ 8 ] = { };
};

inline constexpr const char* k_multi_delay_class = "VuMultiDelayEntity";

inline std::shared_ptr< void > multi_delay_state_for( const std::string& cls )
{
    return cls == k_multi_delay_class ? std::make_shared< multi_delay_state_t >( ) : nullptr;
}

inline void multi_delay_load_properties( entity_t& e, const value_t& properties )
{
    if ( e.m_type != k_multi_delay_class || !e.m_state )
        return;
    multi_delay_state_t& s = *static_cast< multi_delay_state_t* >( e.m_state.get( ) );
    static const char* names[ 8 ] = { "Delay 1", "Delay 2", "Delay 3", "Delay 4", "Delay 5", "Delay 6", "Delay 7", "Delay 8" };
    for ( int k = 0; k < 8; ++k )
        property_float( properties, names[ k ], s.value[ k ], s.defaults[ k ], false );
}

inline void multi_delay_mark_properties( entity_t& e )
{
    if ( e.m_type != k_multi_delay_class || !e.m_state )
        return;
    multi_delay_state_t& s = *static_cast< multi_delay_state_t* >( e.m_state.get( ) );
    std::memcpy( s.defaults, s.value, sizeof( s.defaults ) );
}

// The runtime words: +0xA0 active, +0xA4 the time, +0xA8 the In's params.
struct multi_delay_t
{
    float delay[ 8 ] = { };
    std::uint8_t active = 0;
    float time = 0.0f;
    std::uint8_t params[ 0x84 ] = { };
};

// "In" 0x5E0660 (thiscall; VuRetVal* out, VuParams const&; retn 8): out = 0;
// when not active: +0xA0 = 1, +0xA4 = 0xB4000000 (-2^-23, so a Delay of 0
// fires on the first tick) and +0xA8 = a LOCAL VuParams (rep movsd 0x21 dwords
// from [ebp-0x84], whose size word [ebp-4] is set 0 - the incoming params are
// NOT copied; the oracle shows +0xA8 never equals them): an empty VuParams,
// its 0x80 data bytes stack garbage nothing reads (size 0) - zeros here.
// Active: ignored.
inline void multi_delay_in( multi_delay_t& m )
{
    if ( m.active )
        return;
    m.active = 1;
    const std::uint32_t start = 0xB4000000u;
    std::memcpy( &m.time, &start, 4 );
    std::memset( m.params, 0, sizeof( m.params ) );
}

// The Decision tick 0x5E06B0(dt): only while active. t = dt + time; active =
// 0; for each output k: when Delay k > the OLD time (comiss, ordered) - t >=
// Delay k (comiss / jb: ordered) fires output k with the stored params, else
// active = 1 again; then time = t.
inline void multi_delay_tick( multi_delay_t& m, float dt, const std::function< void( int output, const std::uint8_t* params ) >& fire )
{
    if ( !m.active )
        return;
    const float t = dt + m.time;
    m.active = 0;
    for ( int k = 0; k < 8; ++k )
    {
        const float d = m.delay[ k ];
        if ( !_mm_comigt_ss( _mm_set_ss( d ), _mm_set_ss( m.time ) ) )
            continue;
        if ( _mm_comige_ss( _mm_set_ss( t ), _mm_set_ss( d ) ) )
        {
            if ( fire )
                fire( k, m.params );
        }
        else
            m.active = 1;
    }
    m.time = t;
}