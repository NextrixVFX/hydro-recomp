#pragma once

#include <workspace/core/boat/fleet.hxx>
#include <workspace/core/world/keyframe.hxx>
#include <workspace/core/world/rules.hxx>

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include <xmmintrin.h>

// THE OTHER GAME MODES' OWN SLOTS - VuTimeTrialGame / VuSlalomGame / VuGhostGame
// past their state machines (rules.hxx holds the machines), recreated from the
// disassembly (world batch 28).
//
// SUBSTITUTION: the qsort import (MSVCR110.dll, 0x64C178) is keyframe.hxx's
// vc_qsort (the VC11 CRT algorithm from its published source); only distinct
// keys were measured (case timetrial_results' intercept).
// Hooks (not recreated): VuStringDB g 0x759078 vt+0x18 getString (the string
// tables), the progress manager g 0x758FD8 vt+0x6C (the profile's current
// championship round - a saves-side service).

// A container's number as 0x490490 / 0x48F560 read one: tag 1 cvtdq2ps, 2 the
// float, 7 fild / fstp (round to nearest), anything else 0.
inline float modes_number( const value_t& v )
{
    if ( v.tag == 1 )
        return static_cast< float >( static_cast< std::int32_t >( v.integer ) );
    if ( v.tag == 2 )
        return v.real;
    if ( v.tag == 7 )
        return static_cast< float >( v.int64 );
    return 0.0f;
}

// 0x4FBDF0 (a member by C string; a non-map or an absent name -> the static
// null container 0x7593A0).
inline const value_t& modes_member( const value_t& v, const char* key ) { return ui_json_find( v, key ); }

// An array's element i when it is one and i is in range (the cmp [c], 5 / js / jge
// guards), else the static null container.
inline const value_t& modes_element( const value_t& v, std::int32_t i )
{
    static const value_t null{ };
    if ( v.tag != 5 || i < 0 || static_cast< std::size_t >( i ) >= v.list.size( ) )
        return null;
    return v.list[ static_cast< std::size_t >( i ) ];
}

// ---- VuTimeTrialGame slot 12 0x48F560: its results writer (world batch 28) --
//
// thiscall(game, float dt; retn 4 - dt unread), reached as the game's vt+0x30
// from VuGame's Results tick 0x484280 every frame of that state (case
// timetrial_results: retail's run whole). Once: +0x738 set -> nothing; else
// +0x738 = 1, then by +0x80 (GameData "IsChampionship", 0x481CE0):
//
// NOT a championship (0x490042): gameutil (g 0x758FF8) +0x50 ["GameResults"]
// cleared; ["CircuitRace"] = tag 3 +0x44; ["Table"] (0x4FBCD0) 4 nulls. The
// racer = the boat manager's +0x18 list [0]; its row Table[min(+0x4CC, 3)]
// (cmovg; 0x4FBC60): DriverName (the driver +0x220's +0x60), BestLapTime +0x4B8,
// LapsCompleted +0x4D4, TotalTime +0x4B4, IsLocal (driver vt+0x38), IsHuman
// (vt+0x30), BoatClass +0x160, IsFinished +0x32F, Boat +0x178, TierAchieved =
// +0x4CC. The reference times: TrackDB (gameutil +0x10's +0x58) ["VuDBAsset"]
// [+0x68 track] ["TimeTrial"] [the racer's +0x160]. Then three tiers t = 0..2,
// the row counter r from 0 (r skips the racer's place: t == +0x4CC -> r + 1),
// Table[r++]: TierEntry = 1, TierName = VuStringDB getString(sprintf_s(64,
// "Results_Tier_00%d", t)), TierTime = the reference times [t] (0 when absent),
// Tier = t.
//
// A championship: a local std::vector of containers - the human (the boat
// manager's +0x24 list [0]) {DriverName, BestLapTime, TotalTime, IsLocal,
// IsHuman, BoatClass, IsFinished, Boat, LapsCompleted} - then per entry k of the
// progress manager's (g 0x758FD8 vt+0x6C) ["AIBoats"] (n = its count): its
// ["Properties"]["Boat"] names a BoatDB (gameutil +8's +0x58) ["VuDBAsset"]
// entry; (lo, hi, x) = (4, 15, 25), the human's class "B" (6, 25, 35), "C" (8,
// 35, 45); t = k / n; a = (1 - t) lo + t x; b = t hi + (1 - t) 0; base = the
// human class's reference times [0] (when the array is not empty); total =
// draw (a - b) + b + base (THE global VuRandom 0x736810); on a circuit lap =
// total / laps (+0x4C) - draw x 2.5, else 0. {DriverName the entry's, BestLapTime
// lap, TotalTime total, IsLocal 1, IsHuman 0, BoatClass the HUMAN's +0x160,
// IsFinished 1, Boat the BoatDB entry's StringID, LapsCompleted circuit ? 3 : 1}.
// qsort (0x490490: TotalTime ascending, comiss), then GameResults cleared,
// CircuitRace, Table[k] = the k-th (made a map / an array as they are not).
struct timetrial_results_boat_t
{
    const std::uint8_t* boat = nullptr;   // the 0x520 image (+0x32F, +0x4B4, +0x4B8, +0x4CC, +0x4D4)
    race_results_entry_t entry;           // its strings and its driver's slots 12 / 14
};

struct timetrial_results_in_t
{
    std::uint8_t championship = 0;   // +0x80
    std::uint8_t circuit = 0;        // +0x44
    std::int32_t laps = 0;           // +0x4C
    std::string track;               // +0x68
    timetrial_results_boat_t racer;  // the boat manager's +0x18 list [0]
    timetrial_results_boat_t human;  // its +0x24 list [0]
    const value_t* track_db = nullptr;
    const value_t* boat_db = nullptr;
    std::function< const value_t*( ) > current;              // HOOK: g 0x758FD8 vt+0x6C (null: the static null)
    std::function< std::string( const char* id ) > string_db; // HOOK: g 0x759078 vt+0x18
    fleet_random_t* random = nullptr;                         // THE global VuRandom 0x736810
    // VuSlalomGame slot 12 0x48CAF0 (world batch 29, case slalom_results): the same writer but its once flag +0x768, the
    // TrackDB path ["Slalom"][class]["Times"], a "GatesMissed" (+0x4D0, tag 1) after each human row's TotalTime (not the
    // AI rows'), the AI lap time's draw x 5.0 (0x6F7768) for 2.5; its comparator 0x48DA90 = 0x490490's (TotalTime).
    bool slalom = false;
};

// False when a path not recreated was reached (no StringDB / VuRandom bound, a
// negative place): what can be written still is, as retail's order has it.
inline bool timetrial_write_results( value_t& util_root, std::uint8_t& once, const timetrial_results_in_t& in )
{
    if ( once )
        return true;
    once = 1;
    static const value_t null{ };
    bool known = true;
    const auto text = [ ]( value_t& row, const char* key, const std::string& s ) { ui_json_set_string( ui_json_child( row, key ), s.c_str( ) ); };
    const auto flag = [ ]( value_t& row, const char* key, int b ) { race_results_set( ui_json_child( row, key ), 3, b ); };
    const value_t& tracks = modes_member( in.track_db ? *in.track_db : null, "VuDBAsset" );
    const value_t& mode = modes_member( ui_json_find( tracks, in.track ), in.slalom ? "Slalom" : "TimeTrial" );
    const auto refs_of = [ & ]( const std::string& cls ) -> const value_t& {
        return in.slalom ? modes_member( ui_json_find( mode, cls ), "Times" ) : ui_json_find( mode, cls );
    };
    const auto gates = [ & ]( value_t& row, const std::uint8_t* b ) {
        if ( in.slalom )
            race_results_set( ui_json_child( row, "GatesMissed" ), 1, rules_i32( b, 0x4D0 ) );
    };
    if ( !in.championship )
    {
        const std::uint8_t* b = in.racer.boat;
        const race_results_entry_t& e = in.racer.entry;
        value_t& results = ui_json_child( util_root, "GameResults" );
        ui_json_clear( results );
        race_results_set( ui_json_child( results, "CircuitRace" ), 3, in.circuit );
        {
            value_t& table = ui_json_child( results, "Table" );
            if ( table.tag != 5 )
            {
                ui_json_clear( table );
                table.tag = 5;
                table.kind = value_t::kind_t::list;
            }
            table.list.resize( 4 );
        }
        const std::int32_t place = rules_i32( b, 0x4CC );
        if ( place < 0 )
            return false;   // 0x4FBC60 on a negative index writes before the array: UNKNOWN
        {
            value_t& row = ui_json_at( ui_json_child( results, "Table" ), static_cast< std::size_t >( place > 3 ? 3 : place ) );
            text( row, "DriverName", e.driver_name );
            race_results_set_float( ui_json_child( row, "BestLapTime" ), rules_f( b, 0x4B8 ) );
            race_results_set( ui_json_child( row, "LapsCompleted" ), 1, rules_i32( b, 0x4D4 ) );
            race_results_set_float( ui_json_child( row, "TotalTime" ), rules_f( b, 0x4B4 ) );
            gates( row, b );
            flag( row, "IsLocal", e.local ? 1 : 0 );
            flag( row, "IsHuman", e.human ? 1 : 0 );
            text( row, "BoatClass", e.boat_class );
            flag( row, "IsFinished", b[ 0x32F ] );
            text( row, "Boat", e.boat_name );
            race_results_set( ui_json_child( row, "TierAchieved" ), 1, place );
        }
        const value_t& refs = refs_of( e.boat_class );
        std::int32_t r = 0;
        for ( std::int32_t t = 0; t < 3; ++t )
        {
            if ( t == place )
                ++r;
            value_t& row = ui_json_at( ui_json_child( results, "Table" ), static_cast< std::size_t >( r ) );
            ++r;
            char id[ 64 ];
            std::snprintf( id, sizeof( id ), "Results_Tier_00%d", t );
            flag( row, "TierEntry", 1 );
            if ( in.string_db )
                text( row, "TierName", in.string_db( id ) );
            else
                known = false;   // VuStringDB's getString - the string tables are not recreated
            race_results_set_float( ui_json_child( row, "TierTime" ), modes_number( modes_element( refs, t ) ) );
            race_results_set( ui_json_child( row, "Tier" ), 1, t );
        }
        return known;
    }
    const std::uint8_t* h = in.human.boat;
    const race_results_entry_t& he = in.human.entry;
    std::vector< value_t > rows;
    {
        value_t c;
        text( c, "DriverName", he.driver_name );
        race_results_set_float( ui_json_child( c, "BestLapTime" ), rules_f( h, 0x4B8 ) );
        race_results_set_float( ui_json_child( c, "TotalTime" ), rules_f( h, 0x4B4 ) );
        gates( c, h );
        flag( c, "IsLocal", he.local ? 1 : 0 );
        flag( c, "IsHuman", he.human ? 1 : 0 );
        text( c, "BoatClass", he.boat_class );
        flag( c, "IsFinished", h[ 0x32F ] );
        text( c, "Boat", he.boat_name );
        race_results_set( ui_json_child( c, "LapsCompleted" ), 1, rules_i32( h, 0x4D4 ) );
        rows.push_back( c );
    }
    const value_t* current = in.current ? in.current( ) : nullptr;
    const value_t& ai = modes_member( current ? *current : null, "AIBoats" );
    const value_t& boats = modes_member( in.boat_db ? *in.boat_db : null, "VuDBAsset" );
    const std::int32_t n = ai.tag == 5 ? static_cast< std::int32_t >( ai.list.size( ) ) : 0;   // 0x4FBC40
    for ( std::int32_t k = 0; k < n; ++k )
    {
        const value_t& e = modes_element( ai, k );
        const std::string& name = ui_json_text( modes_member( modes_member( e, "Properties" ), "Boat" ) );
        const value_t& entry = ui_json_find( boats, name );
        const value_t& refs = refs_of( he.boat_class );
        float lo = 4.0f, hi = 15.0f, x = 25.0f;
        if ( he.boat_class == "B" )
            lo = 6.0f, hi = 25.0f, x = 35.0f;
        else if ( he.boat_class == "C" )
            lo = 8.0f, hi = 35.0f, x = 45.0f;
        const float t = static_cast< float >( k ) / static_cast< float >( n );
        const float u = 1.0f - t;
        const float a = u * lo + t * x;
        const float bb = t * hi + u * 0.0f;
        const float base = modes_number( refs.tag == 5 && !refs.list.empty( ) ? refs.list[ 0 ] : null );
        if ( !in.random )
            return false;   // THE global VuRandom not bound: the draws are UNKNOWN
        const float total = ( in.random->draw( ) * ( a - bb ) + bb ) + base;
        float lap = 0.0f;
        if ( in.circuit )
        {
            const float r2 = in.random->draw( );
            lap = total / static_cast< float >( in.laps ) - r2 * ( in.slalom ? 5.0f : 2.5f );
        }
        value_t c;
        text( c, "DriverName", ui_json_text( modes_member( e, "DriverName" ) ) );
        race_results_set_float( ui_json_child( c, "BestLapTime" ), lap );
        race_results_set_float( ui_json_child( c, "TotalTime" ), total );
        flag( c, "IsLocal", 1 );
        flag( c, "IsHuman", 0 );
        text( c, "BoatClass", he.boat_class );
        flag( c, "IsFinished", 1 );
        text( c, "Boat", ui_json_text( modes_member( entry, "StringID" ) ) );
        race_results_set( ui_json_child( c, "LapsCompleted" ), 1, in.circuit ? 3 : 1 );
        rows.push_back( c );
    }
    // 0x490490: -1 when b's TotalTime > a's (comiss), else a's > b's.
    const auto total_of = [ & ]( int i ) { return modes_number( modes_member( rows[ static_cast< std::size_t >( i ) ], "TotalTime" ) ); };
    vc_qsort( static_cast< int >( rows.size( ) ),
              [ & ]( int i, int j ) {
                  const float p = total_of( i ), q = total_of( j );
                  if ( _mm_comigt_ss( _mm_set_ss( q ), _mm_set_ss( p ) ) )
                      return -1;
                  return _mm_comigt_ss( _mm_set_ss( p ), _mm_set_ss( q ) ) ? 1 : 0;
              },
              [ & ]( int i, int j ) { if ( i != j ) std::swap( rows[ static_cast< std::size_t >( i ) ], rows[ static_cast< std::size_t >( j ) ] ); } );
    value_t& results = ui_json_child( util_root, "GameResults" );
    ui_json_clear( results );
    race_results_set( ui_json_child( results, "CircuitRace" ), 3, in.circuit );
    for ( std::size_t k = 0; k < rows.size( ); ++k )
    {
        value_t& table = ui_json_child( results, "Table" );
        if ( table.tag != 5 )
        {
            ui_json_clear( table );
            table.tag = 5;
            table.kind = value_t::kind_t::list;
        }
        if ( k >= table.list.size( ) )
            table.list.resize( k + 1 );
        table.list[ k ] = rows[ k ];
    }
    return known;
}
