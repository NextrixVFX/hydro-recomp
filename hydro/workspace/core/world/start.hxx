#pragma once

#include <workspace/core/boat/manager.hxx>
#include <workspace/core/boat/respawn.hxx>
#include <workspace/core/boat/sections.hxx>
#include <workspace/core/boat/track.hxx>
#include <workspace/core/world/race.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <xmmintrin.h>

// The race's start: VuGame's begin 0x4829A0, VuRaceGame's slot 13 0x48AF90
// (the boats onto the grid / down the track) and slot 5 0x4896A0 (the AI
// fleet), with the boat manager (g 0x758FB8) they read - its add 0x4AAF60,
// remove 0x4AB040 and the grid list the grid positions' gameInitialize
// 0x43A740 fills. Recreated from the disassembly / Hex-Rays (world batch 10).

// ---- the boat manager -------------------------------------------------------
//
// Five boat lists (each data / count / capacity: +0x0C all, +0x18 racers,
// +0x24 humans, +0x30 AI, +0x3C ghosts; 0x415690 grows them to
// max(n, c + c / 2) - the capacities are allocator bookkeeping, not modelled),
// +0x58 the focus index, and the grid list (+0x48 count, +0x4C capacity, +0x50
// data; 0x404A40 doubles it) of grid position entities, kept sorted by name.
// A boat is its index in the game's list; a grid entry its name + its
// transform world (+0x50 of the grid entity's +0x50 component).
struct race_grid_entry_t
{
    std::string name;   // entity +0x18
    float world[ 16 ];
};

// The five lists and the viewport are the boat agent's boat_manager_t (boat
// manager.hxx: boat_manager_add 0x4AAF60 / boat_manager_remove 0x4AB040, whose
// entries are the boats' 0x520 images); the grid list is the world's.
struct race_boat_manager_t
{
    boat_manager_t lists;
    std::vector< race_grid_entry_t > grid;
};
// 0x4AAA40 (cdecl a, b): a's name (+0x18) before b's - the bytes compared
// unsigned over the shorter length, then the shorter first.
inline bool race_grid_before( const race_grid_entry_t& a, const race_grid_entry_t& b )
{
    const std::size_t n = a.name.size( ) < b.name.size( ) ? a.name.size( ) : b.name.size( );
    const int c = n ? std::memcmp( a.name.data( ), b.name.data( ), n ) : 0;
    if ( c )
        return c < 0;
    return a.name.size( ) < b.name.size( );
}

// 0x4AB940 (thiscall list; cmp, lo, hi): retail's own quicksort, 1:1 - the
// middle element ((lo + hi) / 2) as the pivot, Hoare partition with swaps,
// recursion on the left part, a loop on the right.
inline void race_grid_sort( std::vector< race_grid_entry_t >& v, int lo, int hi )
{
    for ( ;; )
    {
        int i = lo, j = hi;
        const race_grid_entry_t pivot = v[ static_cast< std::size_t >( ( lo + hi ) / 2 ) ];
        do
        {
            while ( race_grid_before( v[ static_cast< std::size_t >( i ) ], pivot ) )
                ++i;
            while ( race_grid_before( pivot, v[ static_cast< std::size_t >( j ) ] ) )
                --j;
            if ( i > j )
                break;
            std::swap( v[ static_cast< std::size_t >( i ) ], v[ static_cast< std::size_t >( j ) ] );
            ++i;
            --j;
        } while ( i <= j );
        if ( lo < j )
            race_grid_sort( v, lo, j );
        if ( i >= hi )
            break;
        lo = i;
    }
}

// 0x43A740 (thiscall grid entity): pushed onto the grid list, then with more
// than one entry 0x4AB940(0x4AAA40, 0, count - 1).
inline void race_grid_add( race_boat_manager_t& m, const race_grid_entry_t& e )
{
    m.grid.push_back( e );
    if ( m.grid.size( ) > 1 )
        race_grid_sort( m.grid, 0, static_cast< int >( m.grid.size( ) ) - 1 );
}

// ---- the boats as the start reads them -------------------------------------
//
// boat = the 0x520 image (+0x470 a float, +0x4AC / +0x4B0 / +0x4CC / +0x4D4 /
// +0x4DC written here); transform = its VuTransformComponent (boat +0x50; 0xC8:
// +0x10 local, +0x50 world, +0x90 local euler, +0xA0 world euler, +0xC0 the
// transform-changed delegate 0x421670 - hooks.transform_changed); engine =
// boat +0x22C (+4, +0xC floats); driver = boat +0x220's image (+0x78, +0x80
// written here) and its class (the predicate slots); sector = driver +0x7C
// (a sector index here); checkpoint = boat +0x4D8 (a waypoint index, -1 null).
struct race_start_boat_t
{
    std::uint8_t* boat = nullptr;
    std::uint8_t* transform = nullptr;
    std::uint8_t* engine = nullptr;
    std::uint8_t* driver = nullptr;
    boat_driver_t cls{};
    boat_driver_kind_t kind{};   // boat manager.hxx's view of the same vtable
    int sector = -1;
    int checkpoint = -1;
};

struct race_start_hooks_t
{
    std::function< void( int boat ) > transform_changed;   // the transform's +0xC0 delegate (0x421670)
    std::function< void( int boat ) > game_initialize;     // 0x500B10 on the boat (slot 6 -> boat_manager_add)
    std::function< void( const char* row ) > call;         // a recorded hook (state machines, results, analytics)
    // The AI manager g 0x758F8C: slot 11 addDriver(boat, 0, 0), slots 6 / 7
    // (fleet_add_driver / fleet_start / fleet_run, boat fleet.hxx).
    std::function< void( int boat ) > add_driver;
    std::function< void( ) > fleet_start;
    std::function< void( ) > fleet_run;
};

// 0x5344A0 (thiscall transform; matrix*, notify; retn 8) on a boat's transform:
// world (+0x50) = the matrix, world euler (+0xA0) = respawn_euler's (+0xAC a
// stack word: UNKNOWN, left), 0x534830 - the boat has no parent (+0x34 null):
// local = world, local euler = world euler (+0x9C copies +0xAC) - 0x534C00 (the
// boat has no children), then with notify the +0xC0 delegate.
inline void race_set_world( race_start_boat_t& b, int index, const float* m, bool notify, const race_start_hooks_t& hooks )
{
    std::uint8_t* t = b.transform;
    std::memcpy( t + 0x50, m, 64 );
    float e[ 3 ];
    respawn_euler( m, e );
    std::memcpy( t + 0xA0, e, 12 );
    std::memmove( t + 0x10, t + 0x50, 64 );
    std::memmove( t + 0x90, t + 0xA0, 16 );
    if ( notify && hooks.transform_changed )
        hooks.transform_changed( index );
}

// The number reads 0x48AF90 / 0x4896A0 share: tag 1 int (cvtdq2ps), 2 float,
// 7 int64 (fild / fstp); anything else keeps `fallback`.
inline float race_number( const value_t& v, float fallback )
{
    if ( v.tag == 1 )
        return static_cast< float >( v.integer );
    if ( v.tag == 2 )
        return v.real;
    if ( v.tag == 7 )
        return static_cast< float >( v.int64 );
    return fallback;
}

// ---- VuRaceGame slot 13 0x48AF90 ------------------------------------------
//
// AiSpacing = TrackDB["VuDBAsset"][the project root's name (game +0x34's +0xC
// +0x18)]["Race"]["AiSpacing"] (100.0 when absent), the distance d = 0. Per
// boat (game order): a human (driver slot 12) takes grid entry g (when g < the
// grid count) through 0x5344A0(notify 1), g counting every human; any other
// boat: d += AiSpacing, and when race_track_start finds the place: 0x5344A0(M,
// 1), +0x4AC = 1, +0x4B0 = d / ((boat +0x470 x (engine +0xC x engine +4)) x
// 0.44704), driver +0x7C = the sector, and +0x4D8 = the first waypoint with
// CheckPoint (+0x89) along next[0] from the sector's next - retail reads next[0]
// whatever the count; an empty list before a checkpoint is UNKNOWN (stops).
inline void race_place_boats( std::vector< race_start_boat_t >& boats, const race_boat_manager_t& m, const track_manager_t& tm,
                              const value_t& trackdb, const std::string& root_name, const race_start_hooks_t& hooks )
{
    const float spacing = race_number(
        member( member( member( member( trackdb, "VuDBAsset" ), root_name.c_str( ) ), "Race" ), "AiSpacing" ), 100.0f );
    float distance = 0.0f;
    std::size_t g = 0;
    for ( std::size_t k = 0; k < boats.size( ); ++k )
    {
        race_start_boat_t& b = boats[ k ];
        if ( b.cls.slot[ 0 ] )
        {
            if ( g < m.grid.size( ) )
                race_set_world( b, static_cast< int >( k ), m.grid[ g ].world, true, hooks );
            ++g;
            continue;
        }
        distance = spacing + distance;
        float mtx[ 16 ];
        int sector = -1;
        if ( !race_track_start( tm, distance, mtx, &sector ) )
            continue;
        race_set_world( b, static_cast< int >( k ), mtx, true, hooks );
        image_put32( b.boat, 0x4AC, 1 );
        const float top = state_f( b.engine, 0xC ) * state_f( b.engine, 4 );
        const float speed = ( state_f( b.boat, 0x470 ) * top ) * 0.44703999f;
        state_put_f( b.boat, 0x4B0, distance / speed );
        b.sector = sector;
        const track_sector_t* s = &tm.sectors[ static_cast< std::size_t >( sector ) ];
        while ( !s->next.empty( ) )
        {
            s = &tm.sectors[ static_cast< std::size_t >( s->next[ 0 ] ) ];
            if ( tm.waypoints[ static_cast< std::size_t >( s->waypoint ) ].p.checkpoint )
            {
                b.checkpoint = s->waypoint;
                break;
            }
        }
    }
}

// ---- VuRaceGame slot 5 0x4896A0 --------------------------------------------
//
// The game's +0x720 order list resized to the boat count (0x487960). With the
// track built (the AI manager's slot 10 0x403580 = track slot 8): TopSpeed =
// ConstantDB["VuDBAsset"]["Race"]["TopSpeed"] - an object: its [difficulty
// (+0x50)] entry, else the null - whose element 0 (hi) and 1 (lo) are numbers
// (100 when absent / not a list long enough). Per boat of the manager's AI list
// (count n, index i): engine +4 = (i ? (1 - i / (n - 1)) x lo + (i / (n - 1)) x
// hi : lo) x 0.01, but 1.0 for the last one when the manager has no human;
// driver +0x80 = 0 for difficulty "Easy", 2 for "Hard" (exact), else kept;
// addDriver(boat, 0, 0). Then slots 6 / 7 (start, run). Then (whatever the
// track) the game's +0x6F8 state machine enters its first state (a hook) and
// every boat's +0x4CC = its index = order[index].
inline void race_start_fleet( std::vector< race_start_boat_t >& boats, const race_boat_manager_t& m, const track_manager_t& tm,
                              const value_t& constantdb, const std::string& difficulty, std::vector< int >& order,
                              const race_start_hooks_t& hooks )
{
    order.resize( boats.size( ), 0 );
    if ( track_is_built( tm ) )
    {
        const value_t& table = member( member( member( constantdb, "VuDBAsset" ), "Race" ), "TopSpeed" );
        static const value_t null{};
        const value_t& entry = table.tag == 6 ? member( table, difficulty.c_str( ) ) : null;
        const bool list = entry.tag == 5;
        const float hi = race_number( list && entry.list.size( ) >= 1 ? entry.list[ 0 ] : null, 100.0f );
        const float lo = race_number( list && entry.list.size( ) >= 2 ? entry.list[ 1 ] : null, 100.0f );
        const int n = static_cast< int >( m.lists.ais.boats.size( ) );
        for ( int i = 0; i < n; ++i )
        {
            std::size_t index = 0;
            while ( index < boats.size( ) && boats[ index ].boat != m.lists.ais.boats[ static_cast< std::size_t >( i ) ] )
                ++index;
            if ( index == boats.size( ) )
                continue;   // an entry that is not one of this game's boats: not modelled
            race_start_boat_t& b = boats[ index ];
            float v = lo;
            if ( i > 0 )
            {
                const float t = static_cast< float >( i ) / static_cast< float >( n - 1 );
                v = ( 1.0f - t ) * lo + t * hi;
            }
            state_put_f( b.engine, 4, v * 0.0099999998f );
            if ( m.lists.humans.boats.empty( ) && i == n - 1 )
                image_put32( b.engine, 4, 0x3F800000u );
            if ( b.driver && difficulty == "Easy" )
                image_put32( b.driver, 0x80, 0 );
            else if ( b.driver && difficulty == "Hard" )
                image_put32( b.driver, 0x80, 2 );
            if ( hooks.add_driver )
                hooks.add_driver( static_cast< int >( index ) );
        }
        if ( hooks.fleet_start )
            hooks.fleet_start( );
        if ( hooks.fleet_run )
            hooks.fleet_run( );
    }
    if ( hooks.call )
        hooks.call( "state enter fsm+0x704" );
    for ( std::size_t i = 0; i < boats.size( ); ++i )
    {
        image_put32( boats[ i ].boat, 0x4CC, static_cast< std::uint32_t >( i ) );
        order[ i ] = static_cast< int >( i );
    }
}

// ---- VuGame's begin 0x4829A0 -----------------------------------------------
//
// The track manager's slot 7 (the build - track_build_from_tree's last step,
// run by the caller before this); when built (slot 8): game +0x44 = slot 9 (a
// circuit), +0x48 = slot 13 (the length). Slot 13 (race_place_boats). Per
// boat: when built, driver +0x78 = slot 19(slot 14) (the first checkpoint's
// index), then 0x500B10 (gameInitialize). Per boat: +0x4D4 = the laps (+0x4C),
// a null +0x4D8 = slot 14, +0x4DC = the transform's world. HOOKS: the game's
// two state machines (+0x0C from +0x18, +0x67C from +0x688: the first state's
// +0x20 enter), g_vugameutil +0x50's "GameResults" reset and the analytics
// event "LevelStart" {Difficulty} (g 0x758F9C vt+0x18 - a platform service).
// Then slot 5 (race_start_fleet).
struct race_start_game_t
{
    std::uint8_t circuit = 0;   // +0x44
    float length = 0.0f;        // +0x48
    std::int32_t laps = 0;      // +0x4C
    std::string difficulty;     // +0x50
    std::string root_name;      // +0x34's +0xC +0x18
    std::vector< int > order;   // +0x720
    bool timetrial = false;     // the game is a VuTimeTrialGame (vtable 0x73FB9C): its slots 13 / 5 below
};

// ---- VuTimeTrialGame slot 13 0x490CA0 (world batch 27; read, not oracle-measured) -----------------------------------
//
// With a game boat (+0x3C): boat [0] - whatever its driver - takes the grid's first entry (boat manager +0x48 count,
// +0x50 data [0] non-null) through 0x5344A0(its world, notify 1); then (grid or not) its +0x310 = ConstantDB (gameutil
// +4's +0x58) ["VuDBAsset"]["TimeTrial"]["InitialBoostEnergy"] - tag 1 cvtdq2ps, 2 the float, 7 fild / fstp, else 0.
// Slot 5 0x48F310: current = states [0] (+0x704's first), its +0x20 enter - nothing else (no +0x720 order, no +0x4CC,
// no AI manager start).
inline void race_place_timetrial( std::vector< race_start_boat_t >& boats, const race_boat_manager_t& m, const value_t& constantdb,
                                  const race_start_hooks_t& hooks )
{
    if ( boats.empty( ) )
        return;
    race_start_boat_t& b = boats[ 0 ];
    if ( !m.grid.empty( ) )
        race_set_world( b, 0, m.grid[ 0 ].world, true, hooks );
    state_put_f( b.boat, 0x310, race_number( member( member( member( constantdb, "VuDBAsset" ), "TimeTrial" ), "InitialBoostEnergy" ), 0.0f ) );
}

inline void race_begin( race_start_game_t& game, std::vector< race_start_boat_t >& boats, race_boat_manager_t& m, const track_manager_t& tm,
                        const value_t& trackdb, const value_t& constantdb, const race_start_hooks_t& hooks )
{
    const auto call = [ & ]( const char* row ) { if ( hooks.call ) hooks.call( row ); };
    call( "track build track" );
    const bool built = track_is_built( tm );
    if ( built )
    {
        game.circuit = track_is_circuit( tm ) ? 1 : 0;
        game.length = track_length( tm );
    }
    if ( game.timetrial )
        race_place_timetrial( boats, m, constantdb, hooks );
    else
        race_place_boats( boats, m, tm, trackdb, game.root_name, hooks );
    for ( std::size_t k = 0; k < boats.size( ); ++k )
    {
        if ( built && boats[ k ].driver )
            image_put32( boats[ k ].driver, 0x78,
                         static_cast< std::uint32_t >( track_checkpoint_index( tm, track_first_checkpoint( tm ) ) ) );
        if ( hooks.game_initialize )
            hooks.game_initialize( static_cast< int >( k ) );
    }
    for ( race_start_boat_t& b : boats )
    {
        image_put32( b.boat, 0x4D4, static_cast< std::uint32_t >( game.laps ) );
        if ( b.checkpoint < 0 )
            b.checkpoint = track_first_checkpoint( tm );
        std::memcpy( b.boat + 0x4DC, b.transform + 0x50, 64 );
    }
    call( "state enter fsm+0x18" );
    call( "state enter fsm+0x688" );
    call( "GameResults reset" );
    call( "analytics ana LevelStart" );   // its container {Difficulty: +0x50} is the service's
    if ( game.timetrial )
        call( "state enter fsm+0x704" );   // VuTimeTrialGame's slot 5 0x48F310
    else
        race_start_fleet( boats, m, tm, constantdb, game.difficulty, game.order, hooks );
}
