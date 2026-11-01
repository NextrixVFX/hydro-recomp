#pragma once

#include <workspace/core/assets/archive.hxx>
#include <workspace/core/assets/reader.hxx>
#include <workspace/core/assets/tree.hxx>
#include <workspace/core/boat/driver.hxx>
#include <workspace/core/boat/hull.hxx>
#include <workspace/core/boat/track.hxx>
#include <workspace/core/entity/node.hxx>
#include <workspace/core/entity/property.hxx>
#include <workspace/core/game/types.hxx>
#include <workspace/core/world/tick.hxx>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <xmmintrin.h>

// The race game's start, as far as it is recreated. Retail's chain (read from
// the disassembly / Hex-Rays, world agent batch 7):
//
//   VuActionGameMode's load 0x4BA060: GameData = gameutil +80 ["GameData"]
//     (the dev config's "ActionGameData" when absent - compiled out), the race
//     project loaded, VuRaceGame ctor 0x489030 (VuGame ctor 0x480BA0), then
//   VuGame's boat load 0x481CE0: for each GameData["Boats"][k] a boat made by
//     0x484940 (the entity factory, by name), named "Boat%02d", its vt+0x2C
//     load 0x5009D0 on that entry (Properties: Boat / Skin / Driver Type ...,
//     Driver: Name), 0x500A60, pushed onto the game's +0x38 list; then the
//     project's gameInitialize (every VuAiWaypointEntity's slot 6: the track
//     manager's sectors) and "OnProjectInitialized"; then
//   VuGame's begin 0x4829A0: the track manager's slot 7 (the build) / 8 / 9 /
//     13, the game's slot 13 (VuRaceGame 0x48AF90: a HUMAN-driven boat -
//     driver slot 12 - takes the next VuGridPositionEntity's world (the boat
//     manager's +0x50 list, filled by the grid entities' gameInitialize
//     0x43A740 and sorted by 0x4AAA40); every other boat goes AiSpacing x k
//     metres down the track - race_track_start below - with +0x4AC = 1,
//     +0x4B0 = that distance / (top speed x 0.44704), driver +0x7C the
//     sector), each boat's driver +0x78 = track slot 19 (slot 14) and its
//     gameInitialize, then +0x4D4 = the lap count, +0x4D8, +0x4DC = its world.
//   VuRaceGame slot 5 0x4896A0 then hands every boat to the AI manager
//     (vt+0x2C addDriver, top speed lerp over the boat list from
//     Race.TopSpeed) and runs its slots 6 / 7 (fleet_start / fleet_run).
//
// THE GAP (UNKNOWN - nothing is stood in): GameData["Boats"] is written by the
// front end - VuFrontEndGameMode's GameConfig step 0x4BF9B0 -> 0x4C0A50 (race;
// 0x4C18D0 slalom, 0x4C1E30 time trial, 0x4C2C50 ghost), from GameConfig.Track
// and BoatConfig.PlayerBoats[0] (the player's chosen boat / skin, written by
// the boat-select screens' VuSetBoatConfigPlayerEntity / VuBoatConfigEntity)
// plus an AI roster picked by class (0x4560F0, 0x455700, 0x455820, 0x4C3380,
// 0x455BC0) and the player entry 0x4C3120 - none recreated. With no GameData
// there are no boats, so the AI fleet (fleet_add_driver / fleet_start /
// fleet_run), the per-frame race and the hull draw are not composed.

// The callbacks the race project's tree is loaded with: the verified entity
// loads (engine entity/node.hxx, hydro game/types.hxx) plus the AI waypoint's
// property set (boat track.hxx, ctor 0x431770) - the track manager reads it.
inline entity_world_t race_entity_world( const entity_registry_t& registry, std::function< const value_t*( const std::string& ) > templates )
{
    entity_world_t world;
    world.m_registry = &registry;
    world.m_ctor_for = entity_ctor_for;
    world.m_template = std::move( templates );
    world.m_components_for = entity_components_scripted;
    world.m_component_load = entity_load_component;
    world.m_component_post = entity_post_load_component;
    world.m_component_mark = entity_mark_component;
    world.m_state_for = []( const std::string& cls ) { auto s = waypoint_state_for( cls ); return s ? s : entity_state_for( cls ); };
    world.m_properties_load = []( entity_t& e, const value_t& p ) { waypoint_load_properties( e, p ); entity_load_properties( e, p ); };
    world.m_properties_mark = []( entity_t& e ) { waypoint_mark_properties( e ); entity_mark_properties( e ); };
    return world;
}

// ---- VuGame's boat load 0x481CE0 ------------------------------------------
//
// The VuBoatEntity as the entity tree sees it (create 0x41B690 = operator
// new(0x520) + 0x41B710 + ctor 0x41B850; VuEntity ctor 0x500710(0): +0x14 0,
// vtable 0x7383D4, no ctor-made children). Its three string properties (the
// +0x44 list in ctor order, every one VuAssetProperty-shaped: load 0x406C20,
// value read 0x406CD0) - "Driver Type" -> +0x74 (default "AI"), "Boat" ->
// +0x8C ("Default"), "Skin" -> +0xA4 ("", notify byte 1: its watcher, the skin
// pass 0x421490, fires when a string value differs from the reset target).
// The boat's components (transform / layout / draw / instigator) are the boat
// image's (boat construct.hxx), not modelled on this node.
constexpr entity_ctor_t k_race_boat_ctor{ "VuBoatEntity", 0x007383D4u, 0u };

struct race_boat_state_t
{
    std::string driver_type = "AI", boat = "Default", skin;   // +0x74 / +0x8C / +0xA4
    std::uint32_t hash = 0;                                   // +0x30, 0x500A60
};

inline const entity_ctor_t* race_ctor_for( const std::string& cls )
{
    return cls == "VuBoatEntity" ? &k_race_boat_ctor : entity_ctor_for( cls );
}

struct race_load_hooks_t
{
    // The boat's vt+8 (0x41CFF0, the postLoad: driver, sections, transform) on
    // the boat's GameData entry - its own unit.
    std::function< void( entity_t& boat, const value_t& data ) > post_load;
    // The Skin watcher 0x421490 (the skin pass - the rendering side).
    std::function< void( entity_t& boat ) > skin;
    // VuUIFont's reload 0x626CF0 (ui draw.hxx's ui_font_params) for the font
    // object at this game offset, after its name changed.
    std::function< void( int offset, const std::string& name ) > font_reload;
};

// The game fields 0x481CE0 writes. The VuRaceGame ctor 0x489030 is not
// recreated: the caller presets laps / fonts (its values are an input).
struct race_game_t
{
    std::vector< std::unique_ptr< entity_t > > boats;   // +0x38 data, +0x3C count
    std::int32_t capacity = 0;                           // +0x40
    std::int32_t laps = 0;                               // +0x4C
    std::string difficulty;                              // +0x50
    std::string track;                                   // +0x68
    std::uint8_t championship = 0;                       // +0x80
    std::string fonts[ 14 ];                             // each font object's +8
};

// The 14 HUD font objects (game offset, name) in 0x481CE0's call order.
inline const std::pair< int, const char* >* race_game_fonts( )
{
    static const std::pair< int, const char* > fonts[ 14 ] = {
        { 0x8C, "HUD_PlaceLabelFont" }, { 0xF0, "HUD_PlaceDataFont" }, { 0x154, "HUD_TimeLabelFont" },
        { 0x1B8, "HUD_TimeDataFont" }, { 0x21C, "HUD_BestLabelFont" }, { 0x280, "HUD_BestDataFont" },
        { 0x2E4, "HUD_LapLabelFont" }, { 0x348, "HUD_LapDataFont" }, { 0x3CC, "HUD_ChallengeNameFont" },
        { 0x430, "HUD_ChallengeTimeFont" }, { 0x4F8, "HUD_ChallengeTimeBehindFont" },
        { 0x494, "HUD_ChallengeTimeAheadFont" }, { 0x55C, "HUD_ChallengeResultTextFont" },
        { 0x5C0, "HUD_ChallengeResultTimeFont" } };
    return fonts;
}

// The boat's three property loads (0x406C20 each, list order): the target is
// reset to the default, a string value (tag 4) that differs is stored, and the
// Skin's notify fires its watcher then.
inline void race_boat_load_properties( entity_t& e, const value_t& properties, const race_load_hooks_t& hooks )
{
    race_boat_state_t& s = *static_cast< race_boat_state_t* >( e.m_state.get( ) );
    property_string( properties, "Driver Type", s.driver_type, "AI" );
    property_string( properties, "Boat", s.boat, "Default" );
    s.skin.clear( );
    const value_t& skin = member( properties, "Skin" );
    if ( skin.tag == 4 && skin.text != s.skin )
    {
        s.skin = skin.text;
        if ( hooks.skin )
            hooks.skin( e );
    }
}

// 0x500A60(seed) (thiscall; retn 4): +0x30 = FNV-1a of the name from `seed`;
// the hash then runs on over "/" (0x6CB0C4) and seeds every child's; each
// component's slot 5 (a hook) and the entity's vt+0x10 (VuBoatEntity: 0x401560,
// a nop) follow. Only a boat's hash has a place here.
inline std::uint32_t race_entity_hash( entity_t& e, std::uint32_t seed )
{
    std::uint32_t h = seed;
    for ( unsigned char c : e.m_name )
        h = ( h ^ c ) * 0x1000193u;
    if ( e.m_type == "VuBoatEntity" && e.m_state )
        static_cast< race_boat_state_t* >( e.m_state.get( ) )->hash = h;
    std::uint32_t path = ( h ^ static_cast< unsigned char >( '/' ) ) * 0x1000193u;
    for ( auto& child : e.m_children )
        race_entity_hash( *child, path );
    return h;
}

// 0x564AE0's file part: after the LAST '/', else the whole name.
inline std::string race_file_part( const std::string& name )
{
    const std::size_t slash = name.rfind( '/' );
    return slash == std::string::npos ? name : name.substr( slash + 1 );
}

// 0x481CE0 itself (this = the game, `game_data` = GameData). `util_data` is
// g_vugameutil's +0x50 container (0x4D3260 reads ["GameData"]["Level"] there -
// when that is not an object, g_vudevconfig's "ActionGameData", a hook: null
// here), `trackdb` TrackDB's document (g_vugameutil +0x10's +0x58). Per entry
// of GameData["Boats"] (0x4FBC40: an array's count, else 0): the registry's
// create for "VuBoatEntity" (0x484940 - [0x73628C]), the name "Boat%02d"
// (k + 1), vt+0x2C = VuEntity::load 0x5009D0 on the entry (out of range / not
// an array -> the null container), 0x500A60(0x811C9DC5), push_back (capacity
// max(n, c + c / 2) when full). Then "Difficulty" (tag 4) -> +0x50,
// "IsChampionship" (tag 3) -> +0x80, the track = the Level's file part ->
// +0x68, TrackDB["VuDBAsset"][track]["LapCount"] (1 int, 2 float truncated, 7
// the int64's low dword; anything else keeps +0x4C), each font object renamed
// and reloaded when its name differs (0x626B00), then the game's vt+0x10 on
// GameData (VuRaceGame slot 4 = 0x4021A0, ret 4: nothing). False when the
// registry cannot create the boat type (retail would push null - UNKNOWN).
inline bool race_load_boats( race_game_t& game, const value_t& game_data, const value_t& util_data, const value_t& trackdb,
                             const entity_registry_t& registry, const race_load_hooks_t& hooks )
{
    entity_world_t world = race_entity_world( registry, nullptr );
    world.m_ctor_for = race_ctor_for;
    const auto state_for = world.m_state_for;
    world.m_state_for = [ state_for ]( const std::string& cls ) -> std::shared_ptr< void > {
        return cls == "VuBoatEntity" ? std::make_shared< race_boat_state_t >( ) : state_for( cls );
    };
    const auto properties_load = world.m_properties_load;
    world.m_properties_load = [ properties_load, &hooks ]( entity_t& e, const value_t& p ) {
        if ( e.m_type == "VuBoatEntity" )
            race_boat_load_properties( e, p, hooks );
        else
            properties_load( e, p );
    };
    static const value_t null{};
    const value_t& boats = member( game_data, "Boats" );
    const int count = boats.tag == 5 ? static_cast< int >( boats.list.size( ) ) : 0;
    for ( int k = 0; k < count; ++k )
    {
        std::unique_ptr< entity_t > boat = entity_create( "VuBoatEntity", world );
        if ( !boat )
            return false;
        char name[ 16 ];
        std::snprintf( name, sizeof( name ), "Boat%02d", k + 1 );
        boat->m_name = name;
        const value_t& entry = k < static_cast< int >( boats.list.size( ) ) ? boats.list[ static_cast< std::size_t >( k ) ] : null;
        entity_load( *boat, &entry, world );
        if ( hooks.post_load )
            hooks.post_load( *boat, entry );
        race_entity_hash( *boat, 0x811C9DC5u );
        const std::int32_t n = static_cast< std::int32_t >( game.boats.size( ) ) + 1;
        if ( n > game.capacity )
        {
            std::int32_t grown = game.capacity + game.capacity / 2;
            if ( n > grown )
                grown = n;
            if ( grown > game.capacity )
                game.capacity = grown;
        }
        game.boats.push_back( std::move( boat ) );
    }
    const value_t& difficulty = member( game_data, "Difficulty" );
    if ( difficulty.tag == 4 )
        game.difficulty = difficulty.text;
    const value_t& championship = member( game_data, "IsChampionship" );
    if ( championship.tag == 3 )
        game.championship = static_cast< std::uint8_t >( championship.integer );
    const value_t& gd = member( util_data, "GameData" );
    const value_t& level = member( gd.tag == 6 ? gd : null, "Level" );
    game.track = race_file_part( level.tag == 4 ? level.text : std::string( ) );
    const value_t& laps = member( member( member( trackdb, "VuDBAsset" ), game.track.c_str( ) ), "LapCount" );
    if ( laps.tag == 1 )
        game.laps = laps.integer;
    else if ( laps.tag == 2 )
        game.laps = _mm_cvtt_ss2si( _mm_set_ss( laps.real ) );
    else if ( laps.tag == 7 )
        game.laps = static_cast< std::int32_t >( static_cast< std::uint64_t >( laps.int64 ) & 0xFFFFFFFFu );
    const auto* fonts = race_game_fonts( );
    for ( int k = 0; k < 14; ++k )
        if ( game.fonts[ k ] != fonts[ k ].second )
        {
            game.fonts[ k ] = fonts[ k ].second;
            if ( hooks.font_reload )
                hooks.font_reload( fonts[ k ].first, game.fonts[ k ] );
        }
    return true;
}

// ---- VuBoatEntity gameInitialize 0x41D350 / gameRelease 0x41D470 ---------
//
// Slot 6 (0x500B10's vt+0x18): two VuMethod1<VuBoatEntity, void, float>
// delegates (operator new(0x10): vtable 0x738348, the boat, the member, 0)
// registered through g_vutickmanagerimpl 0x759008's slot 8 - 0x4077ED (a
// thunk: vt+0x34, slot 13 = the Decision tick 0x41EFF0) in "Decision", then
// 0x404AB2 (vt+0x38, slot 14 = 0x41F330, the Build tick) in "Build". Then, in
// order, the draw component (+0x6C) handed to g_vu3ddrawmanager 0x7590B8's
// slot 10 when its +0x1E is 0 (+0x1E = 1), the boat's slot 17 (0x421980),
// the driver's (+0x220) slot 6, 0x4191F0 on the hull (+0x228), the engine
// (+0x22C) event start - 0x50FBF0 on its +0xC4 with the name +0x4C, true ->
// FMOD::Event::start(+0xC8) - 0x410610 on +0x230, 0x419FF0 on +0x234, the
// boat manager g 0x758FB8's 0x4AAF60(boat), 0x531F50 on the instigator +0x70.
// Slot 7 (gameRelease): the instigator manager g 0x759064's 0x5362A0 when the
// instigator's +0x28 is set, then +0x28 = 0; 0x4AB040(boat) on the boat
// manager; slot 10 unregister(boat) (every phase); the draw manager's slot 11
// when +0x1E (then 0); the boat's slot 18 (0x421940); the driver's slot 7;
// 0x419260 hull, 0x415330 engine, 0x4108C0 +0x230, a tail jmp 0x411950 on
// +0x238. Every callee but the tick manager's is a HOOK here (`call` gets
// its name and object, as the case names them); `engine_ready` is 0x50FBF0
// (the audio layer, a hook).
struct race_boat_init_hooks_t
{
    std::function< void( const std::string& row ) > call;
    std::function< bool( ) > engine_ready;
};

inline void race_boat_game_initialize( const void* boat, tick_manager_t& tm, const tick_handler_t& decision, const tick_handler_t& build,
                                       std::uint8_t& draw_added, const race_boat_init_hooks_t& hooks )
{
    const auto call = [ & ]( const std::string& row ) { if ( hooks.call ) hooks.call( row ); };
    // The delegates' words as retail stores them (the owner stands for the boat).
    if ( tick_register( tm, decision, "Decision" ) )
        call( "tick register tick 0x738348 boat 0x4077ED 0 Decision" );
    if ( tick_register( tm, build, "Build" ) )
        call( "tick register tick 0x738348 boat 0x404AB2 0 Build" );
    ( void )boat;
    if ( !draw_added )
    {
        call( "draw add drawm draw" );
        draw_added = 1;
    }
    call( "slot17 boat" );
    call( "driver slot6 driver" );
    call( "0x4191F0 hull" );
    call( "0x50FBF0 engine+0xC4 Engine/Ev" );
    if ( hooks.engine_ready && hooks.engine_ready( ) )
        call( "FMOD start engine+0xC4 0xE7E7" );
    call( "0x410610 anim" );
    call( "0x419FF0 part234" );
    call( "0x4AAF60 boatm boat" );
    call( "0x531F50 inst" );
}

inline void race_boat_game_release( const void* boat, tick_manager_t& tm, std::uint8_t& draw_added, std::uint8_t& instigator_registered,
                                    const race_boat_init_hooks_t& hooks )
{
    const auto call = [ & ]( const std::string& row ) { if ( hooks.call ) hooks.call( row ); };
    if ( instigator_registered )
        call( "0x5362A0 instm inst" );
    instigator_registered = 0;
    call( "0x4AB040 boatm boat" );
    tick_unregister_all( tm, boat );
    call( "tick unregister tick boat" );
    if ( draw_added )
    {
        call( "draw remove drawm draw" );
        draw_added = 0;
    }
    call( "slot18 boat" );
    call( "driver slot7 driver" );
    call( "0x419260 hull" );
    call( "0x415330 engine" );
    call( "0x4108C0 anim" );
    call( "0x411950 damage" );
}

// ---- VuBoatEntity postLoad 0x41CFF0 -----------------------------------------
//
// The boat's vt+8 (VuEntity::load's trailing call; thiscall boat, the boat's
// data container; retn 4). The Driver Type text (+0x74) is compared EXACTLY
// (0x404990 / the 5-byte "Human" compare: same length, same bytes) with, in
// order: "Human" -> operator new(0xD0), 0x41D2E0, +0x84 = 0, the Human ctor
// 0x40ED50(boat); "AI" -> new(0xD4), 0x41D2E0, +0x8C = +0xBC = 0, VuAiDriver's
// ctor 0x40B690(boat); "UI" -> the same then vtable 0x737CDC (VuAiUiDriver);
// "Nuisance" -> new(0x104), 0x41D280, the ctor 0x40D880(boat); "Ghost" ->
// new(0x84), 0x41D2E0, +0x80 = 0, the ctor 0x40EB50(boat). The block goes to
// +0x220 (operator new's null too). ANY OTHER TEXT LEAVES +0x220 AS IT IS
// (the oracle shows the previous driver kept and loaded). Then, with +0x220
// set, the driver's vt+8 load on data["Driver"] (0x4FBDF0), then 0x420A70
// (boat_load_sections, boat load.hxx) and 0x421670 (boat_transform_changed).
// UNKNOWN (not recreated): the Human / Nuisance / Ghost ctors - such a driver
// is an UNKNOWN object here (its load is a hook), and operator new's failure.
enum class race_driver_class_t { ai, ui, human, nuisance, ghost };

struct race_boat_driver_t
{
    race_driver_class_t cls = race_driver_class_t::ai;
    std::uint32_t size = 0;                 // operator new's request
    bool known = false;                     // false: the ctor is not recreated (UNKNOWN object)
    std::vector< image_object_t > graph;    // AI / UI: ai_driver_construct's objects (boat driver.hxx)
    boat_strings_t strings;                 // +0x60, the Name
};

struct race_post_load_hooks_t
{
    // A driver ctor not recreated (0x40ED50 / 0x40D880 / 0x40EB50).
    std::function< void( std::uint32_t ctor ) > unknown_ctor;
    // vt+8 on a driver whose class is not recreated, with data["Driver"].
    std::function< void( race_boat_driver_t& driver, const value_t& container ) > unknown_load;
    // 0x420A70 and 0x421670, the composed units (the running game passes
    // boat_load_sections / boat_transform_changed on the boat's objects).
    std::function< void( ) > load_sections;
    std::function< void( ) > transform_changed;
};

// `boat` the boat's 0x520 image (the driver's load reads its +0x270 / +0x280
// aabb, still the ctor's zeros on a fresh boat). True when every step ran
// recreated code; false when the driver at +0x220 is UNKNOWN (an unrecreated
// ctor, or a kept driver of an unknown class) - every step still runs, in
// retail's order.
inline bool race_boat_post_load( std::uint8_t* boat, const std::string& driver_type, const value_t& data,
                                 std::unique_ptr< race_boat_driver_t >& driver, const race_post_load_hooks_t& hooks )
{
    auto make = [ & ]( race_driver_class_t cls, std::uint32_t size, std::uint32_t ctor )
    {
        driver = std::make_unique< race_boat_driver_t >( );
        driver->cls = cls;
        driver->size = size;
        if ( ctor )
        {
            if ( hooks.unknown_ctor )
                hooks.unknown_ctor( ctor );
            return;
        }
        driver->known = true;
        driver->graph = ai_driver_construct( );
        if ( cls == race_driver_class_t::ui )
            image_put32( driver->graph[ 0 ].image.data( ), 0, 0x00737CDCu );   // VuAiUiDriver
    };
    if ( driver_type == "Human" )
        make( race_driver_class_t::human, 0xD0, 0x0040ED50u );
    else if ( driver_type == "AI" )
        make( race_driver_class_t::ai, 0xD4, 0 );
    else if ( driver_type == "UI" )
        make( race_driver_class_t::ui, 0xD4, 0 );
    else if ( driver_type == "Nuisance" )
        make( race_driver_class_t::nuisance, 0x104, 0x0040D880u );
    else if ( driver_type == "Ghost" )
        make( race_driver_class_t::ghost, 0x84, 0x0040EB50u );
    bool known = true;
    if ( driver )
    {
        const value_t& container = member( data, "Driver" );
        if ( driver->known )
            ai_driver_load( driver->graph[ 0 ].image.data( ), driver->strings, container, boat );   // 0x40B8A0 (AI / UI)
        else
        {
            known = false;
            if ( hooks.unknown_load )
                hooks.unknown_load( *driver, container );
        }
    }
    if ( hooks.load_sections )
        hooks.load_sections( );
    if ( hooks.transform_changed )
        hooks.transform_changed( );
    return known && driver != nullptr;
}

// The project asset's AssetData "File" path (0x55FBE0), or empty.
inline std::string race_project_file( c_archive& archive, const char* asset )
{
    std::vector< std::uint8_t > bytes;
    const std::string prefix = "VuProjectAsset/";
    if ( std::strncmp( asset, prefix.c_str( ), prefix.size( ) ) != 0 || !archive.read( "Assets/AssetData", bytes ) )
        return {};
    const value_t database = c_reader( bytes ).parse( );
    const value_t& file = member( member( member( database, "VuProjectAsset" ), asset + prefix.size( ) ), "File" );
    return file.tag == 4 ? file.text : std::string( );
}

// VuProject::load 0x556390 on the race project: the registry as start-up
// leaves it and the tree through race_entity_world. Null when not loadable.
inline std::unique_ptr< entity_t > race_load_tree( c_archive& archive, const char* asset, const value_t& project,
                                                   std::function< const value_t*( const std::string& ) > templates )
{
    const std::string path = race_project_file( archive, asset );
    if ( path.empty( ) )
        return nullptr;
    const entity_registry_t registry = build_entity_registry( archive.names( "VuTemplateAsset/" ) );
    entity_world_t world = race_entity_world( registry, std::move( templates ) );
    return entity_load_project( project, path, world );
}

// 0x4D1E10 (__usercall: ecx the 64-byte matrix out, xmm0 the distance; the
// sector out on the stack, may be null; ret), VuRaceGame 0x48AF90's start
// position for a boat not driven by a human: from the track's start sector
// (slot 11) along next[0] to the end sector (slot 12) inclusive, sectors whose
// waypoint Allows Recovery (+0x8B) use up their length (+0xB0) until one is
// longer than what is left (ordered compare, `ja`); there f = left / length,
// the point lerps +0x80 -> +0x90 (a x (1 - f) + b x f per lane) and the matrix
// is the identity with row 1 = the direction's xy over VuMath_sqrt(y y + x x)
// (z = that inverse x 0), row 0 = (fy - fz 0, fz 0 - fx, fx 0 - fy 0), row 2
// (0, 0, 1), row 3 the point. False (nothing written) when no sector is long
// enough or the track has no start.
inline bool race_track_start( const track_manager_t& tm, float distance, float* out, int* sector )
{
    const int end = track_end_sector( tm );
    int s = track_start_sector( tm );
    float left = distance;
    while ( s >= 0 )
    {
        const track_sector_t& t = tm.sectors[ static_cast< std::size_t >( s ) ];
        if ( tm.waypoints[ static_cast< std::size_t >( t.waypoint ) ].p.allow_recovery )
        {
            const float length = state_f( t.image, 0xB0 );
            if ( _mm_comigt_ss( _mm_set_ss( length ), _mm_set_ss( left ) ) )
            {
                const float f = left / length;
                const float g = 1.0f - f;
                const float x = state_f( t.image, 0x80 ) * g + state_f( t.image, 0x90 ) * f;
                const float y = state_f( t.image, 0x84 ) * g + state_f( t.image, 0x94 ) * f;
                const float z = state_f( t.image, 0x88 ) * g + state_f( t.image, 0x98 ) * f;
                const float dx = state_f( t.image, 0xA0 ), dy = state_f( t.image, 0xA4 );
                const float inv = 1.0f / hull_sqrt( dy * dy + dx * dx );
                const float zero = 0.0f;
                const float fx = inv * dx, fy = inv * dy, fz = inv * zero;
                const float fz0 = fz * zero;
                for ( int k = 0; k < 16; ++k )
                    out[ k ] = k % 5 == 0 ? 1.0f : 0.0f;   // VuMatrix_identity
                out[ 0 ] = fy - fz0;
                out[ 1 ] = fz0 - fx;
                out[ 2 ] = fx * zero - fy * zero;
                out[ 4 ] = fx;
                out[ 5 ] = fy;
                out[ 6 ] = fz;
                out[ 12 ] = x;
                out[ 13 ] = y;
                out[ 8 ] = 0.0f;
                out[ 9 ] = 0.0f;
                out[ 10 ] = 1.0f;
                out[ 14 ] = z;
                if ( sector )
                    *sector = s;
                return true;
            }
            left = left - length;
        }
        if ( s == end )
            break;
        // Retail reads the next list's first word whatever its count; an empty
        // list before the end sector (not on a built track) is UNKNOWN - stop.
        s = t.next.empty( ) ? -1 : t.next[ 0 ];
    }
    return false;
}
