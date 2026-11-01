#pragma once

#include <workspace/core/ui/leave.hxx>

// c_world's --front-end-defaults - member definitions (one class, split by concern).
//
// THE TEST PROFILE (the user's decision, batch 16 / 22): an everything-unlocked
// profile - core/ui leave.hxx ui_profile_all_unlocked, a DECLARED platform
// answer set for the profile / unlock / license / progress hooks (saves and
// profiles are re-made for the platform, not 1:1) - run through the UI agent's
// recreated single-player selector chain (ui_front_end_root: VuSetGameModeUIAction
// 0x472740, SetSinglePlayer 0x44EF10, the GameConfig<Mode> screen's
// VuGameConfigEntity slot 4 0x448120 + onGameInitialize 0x45D050, the boat
// screen's VuBoatConfigEntity slot 4 + 0x44CD10 + onGameRelease 0x44D090) and
// VuFrontEndGameMode's exit data step 0x4BF9B0 (ui_front_end_game_data_root:
// the builder by GameConfig.Mode). Verified against retail under the same
// answers by the UI agent's verify_ui_front_end_defaults (oracle case
// front_end_defaults).
//
// What is ours here, said where it happens:
// - the screens are LOADED, not navigated: Screens/GameConfig<Mode> is the
//   project SinglePlayer's VuPushScreenUIAction "Screen" names for that mode
//   (authored: Screens/GameConfigRace / Slalom / TimeTrial / Championship; Main's
//   for Ghost), and the boat screen is the one the GameConfig screen's own
//   VuPushScreenUIAction "Screen" names (BoatConfig / BoatConfigSlalom / ...).
//   Retail reaches them through the screen stack (flow.hxx "OnPushScreen" from
//   the graph: IfAvailable . True -> PushScreenBoatConfig); that graph is not
//   run, so the two pushes are read off the loaded action's property.
// - the screens' output plugs (OnSelectionChanged / OnOptionChanged ...) and
//   broadcasts go nowhere: the screens' graphs are not driven (logged, counted).
// - the player's picks: --race / --course enter as the profile's remembered
//   picks, root.LastGameConfigSelection.Track / .Course (where retail keeps them:
//   select.hxx slot 24 0x45E210 reads them; the recreated walk keeps a pick
//   only if it is a choice and unlocked), --boat / --skin as overrides into
//   root.BoatConfig.PlayerBoats[0].Properties after the boat screen's release
//   (the user's instruction: the values the boat screen would leave).
// - the string DB / sign-in hooks of the exit's builders are unset (the AI
//   names: VuStringDB not recreated; the player's name: platform sign-in).

// The loaded front-end screen of an asset through the UI's world (the UI
// property loads ui_front_end_screen reads), or null (logged).
inline std::unique_ptr< entity_t > c_world::front_end_screen( const std::string& name )
{
    const std::string asset = "VuProjectAsset/" + name;
    std::vector< std::uint8_t > bytes;
    if ( !m_archive.read( asset.c_str( ), bytes ) )
    {
        LOG_WARNING( "front-end defaults: %s not in the pack", asset.c_str( ) );
        return nullptr;
    }
    const value_t document = c_reader( bytes ).parse( );
    const value_t* project = find( document, "VuProject" );
    if ( !project )
        return nullptr;
    const entity_registry_t registry = build_entity_registry( m_archive.names( "VuTemplateAsset/" ) );
    const entity_world_t world = ui_world( registry, [ this ]( const std::string& n ) { return template_document( n ); } );
    return entity_load_project( *project, race_project_file( m_archive, asset.c_str( ) ), world );
}

// The first entity of `type` in pre-order, or null.
inline const entity_t* c_world::front_end_find( const entity_t& e, const char* type )
{
    if ( e.m_type == type && e.m_ctor )
        return &e;
    for ( const auto& child : e.m_children )
        if ( const entity_t* found = front_end_find( *child, type ) )
            return found;
    return nullptr;
}

// The GameData the test profile's front end leaves for `mode` (false, logged,
// on any path not recreated). `dbs` = BoatDB / TrackDB loaded (race_dbs).
inline bool c_world::front_end_defaults( const std::string& mode, const std::string& track, const std::string& course,
                                         const race_session_dbs_t& dbs, value_t& game_data )
{
    if ( mode.empty( ) )
    {
        LOG_WARNING( "front-end defaults: --mode is required (the SinglePlayer screen's choice); no race" );
        return false;
    }
    const auto db = [ this ]( const char* asset, value_t& out ) -> const value_t*
    {
        std::vector< std::uint8_t > bytes;
        if ( !m_archive.read( asset, bytes ) )
            return nullptr;
        out = c_reader( bytes ).parse( );
        return &out;
    };
    value_t option_db, champ_db;
    ui_front_end_dbs_t fdbs;
    fdbs.m_option_db = db( "VuDBAsset/OptionDB", option_db );
    fdbs.m_champ_db = db( "VuDBAsset/ChampDB", champ_db );
    fdbs.m_boat_db = dbs.boat_db;
    fdbs.m_track_db = dbs.track_db;
    fdbs.m_skin_db = m_skin_db.tag == 6 ? &m_skin_db : nullptr;
    if ( !fdbs.m_option_db || !fdbs.m_champ_db || !fdbs.m_boat_db || !fdbs.m_track_db || !fdbs.m_skin_db )
    {
        LOG_ERROR( "front-end defaults: OptionDB / ChampDB / BoatDB / TrackDB / SkinDB not readable from the pack; no race" );
        return false;
    }
    // The game screen and the boat screen its push action names (see the head of this file).
    const std::string game_name = "Screens/GameConfig" + mode;
    const std::unique_ptr< entity_t > game_tree = front_end_screen( game_name );
    const entity_t* game = game_tree ? front_end_find( *game_tree, "VuGameConfigEntity" ) : nullptr;
    if ( !game )
    {
        LOG_WARNING( "front-end defaults: %s has no loaded VuGameConfigEntity (UNKNOWN); no race", game_name.c_str( ) );
        return false;
    }
    std::string boat_name;
    const auto pushes = [ & ]( const auto& self, const entity_t& e ) -> void
    {
        if ( e.m_type == "VuPushScreenUIAction" && e.m_ctor && boat_name.empty( ) )
            if ( const ui_slot_t* v = ui_property_value( e, "Screen" ) )
                if ( v->m_text.compare( 0, 18, "Screens/BoatConfig" ) == 0 )
                    boat_name = v->m_text;
        for ( const auto& child : e.m_children )
            self( self, *child );
    };
    pushes( pushes, *game_tree );
    const std::unique_ptr< entity_t > boat_tree = boat_name.empty( ) ? nullptr : front_end_screen( boat_name );
    const entity_t* boat = boat_tree ? front_end_find( *boat_tree, "VuBoatConfigEntity" ) : nullptr;
    if ( !boat )
    {
        LOG_WARNING( "front-end defaults: %s pushes no loadable boat screen with a VuBoatConfigEntity ('%s'); no race", game_name.c_str( ),
                     boat_name.c_str( ) );
        return false;
    }
    value_t root;
    if ( !track.empty( ) )
        ui_json_set_string( ui_json_child( ui_json_child( root, "LastGameConfigSelection" ), "Track" ), track.c_str( ) );
    if ( !course.empty( ) )
        ui_json_set_string( ui_json_child( ui_json_child( root, "LastGameConfigSelection" ), "Course" ), course.c_str( ) );
    int fires = 0, broadcasts = 0;
    ui_config_hooks_t h;
    h.touch = [ ]( ) { };
    h.draw_add = [ ]( ) { };   // the 3D draw manager (rendering layer)
    h.fire = [ &fires ]( const std::string&, const ui_params_t& ) { ++fires; return true; };
    h.broadcast = [ &broadcasts ]( std::uint32_t, const ui_params_t& ) { ++broadcasts; };
    h.sound = [ ]( const char* ) { };    // audio: platform hook
    h.trophy = [ ]( const char* ) { };   // the trophy model (asset factory / renderer)
    const ui_profile_t p = ui_profile_all_unlocked( );
    if ( !ui_front_end_root( root, mode, ui_front_end_screen( *game, game->m_flags ), ui_front_end_screen( *boat, boat->m_flags ), fdbs, "", p, h ) )
    {
        LOG_WARNING( "front-end defaults: the selector chain stopped on a path not recreated (%s / %s); no race", game_name.c_str( ), boat_name.c_str( ) );
        return false;
    }
    value_t& properties = ui_json_child( ui_json_at( ui_json_child( ui_json_child( root, "BoatConfig" ), "PlayerBoats" ), 0 ), "Properties" );
    const char* boat_pick = c_engine::option( "boat" );
    const char* skin_pick = c_engine::option( "skin" );
    if ( boat_pick && boat_pick[ 0 ] )
        ui_json_set_string( ui_json_child( properties, "Boat" ), boat_pick );
    if ( skin_pick && skin_pick[ 0 ] )
        ui_json_set_string( ui_json_child( properties, "Skin" ), skin_pick );
    const value_t& config = ui_json_find( root, "GameConfig" );
    LOG_INFO( "front-end defaults (test profile, everything unlocked): %s (+0x14 %u) / %s (+0x14 %u) -> GameConfig Mode '%s' Track '%s' Course '%s' "
              "Championship '%s', PlayerBoats[0] Boat '%s' Skin '%s'%s; %d screen output fires / %d broadcasts not run (the screens' graphs)",
              game_name.c_str( ), game->m_flags, boat_name.c_str( ), boat->m_flags, ui_json_text( member( config, "Mode" ) ).c_str( ),
              ui_json_text( member( config, "Track" ) ).c_str( ), ui_json_text( member( config, "Course" ) ).c_str( ),
              ui_json_text( member( config, "Championship" ) ).c_str( ), ui_json_text( member( properties, "Boat" ) ).c_str( ),
              ui_json_text( member( properties, "Skin" ) ).c_str( ), boat_pick || skin_pick ? " (--boat / --skin overrides)" : "", fires, broadcasts );
    ui_game_env_t env;
    env.boat_db = dbs.boat_db;
    env.track_db = dbs.track_db;
    env.random = &m_vurandom;
    ui_game_config_t built;
    game_data = ui_front_end_game_data_root( root, env, &built );
    if ( !built.built )
    {
        LOG_WARNING( "front-end defaults: GameData UNKNOWN - mode '%s' reaches builder 0x%X, %s; no race", mode.c_str( ), built.builder,
                     built.builder ? "not recreated" : "none (not a front-end event type)" );
        return false;
    }
    return true;
}
