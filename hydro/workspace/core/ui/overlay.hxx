#pragma once

#include <workspace/core/ui/bind.hxx>
#include <workspace/core/ui/wiring.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// THE EXPORT: the race HUD (VuRaceGame's three HUD projects, ctor 0x489030:
// +0x738 HUDs/RacePreGameHUD, +0x73C HUDs/RaceGameHUD, +0x740
// HUDs/RacePostGameHUD) driven the way VuRaceGame's states drive them.
//
//   ui_hud_load        the project's tree through the UI's scripted world (the
//                      loaded properties + the plugs / links - the HUD reads both).
//   c_ui_race_hud      one project: initialize = gameInitialize 0x500B10 +
//                      0x501260 "OnProjectInitialized" (a state's enter, when the
//                      root's +0x14 bit 2 is clear), tick = VuHUDEntity slot 12
//                      0x423A20 (vt+0x30: "OnUITick" { dt } to the direct
//                      children), release = gameRelease 0x500B80 (a state's exit),
//                      post = an event the race broadcasts (OnPackageFound,
//                      OnControlMethodChanged, ...), fire = an output plug of a node
//                      the WORLD owns (VuMultiDelayEntity's "1".."8" from its
//                      Decision tick).
//
// Everything the HUD's graph reaches is c_ui_binding's (core/ui bind.hxx, the
// m_hud kinds); the game it reads is ui_hud_env_t, refreshed by the caller
// before each call (ui_hud_env_fill: the rules' per-boat fields through
// ui_hud_boat_of). What it hands back out: the world-owned inputs (multi delay
// In, audio Start / Stop) through hooks.m_external and the broadcasts
// (OnStartLightsOut) through hooks.m_broadcast_out.

// VuProject::load 0x556390 of a HUD project document through the UI's world.
inline std::unique_ptr< entity_t > ui_hud_load( const value_t& project, const std::string& path, const entity_registry_t& registry,
                                                std::function< const value_t*( const std::string& ) > templates )
{
    const entity_world_t world = ui_scripted_world( registry, std::move( templates ) );
    return entity_load_project( project, path, world );
}

// The boat manager's view the HUD reads (hud.hxx ui_hud_world_t) from the
// rules' boats: each boat's 0x520 image (ui_hud_boat_of), the local (human)
// boat's index, the boat count (manager +0x1C), the settings' units. `boats`
// keeps the ui_hud_boat_t storage the world points into.
inline void ui_hud_env_fill( ui_hud_env_t& env, std::vector< ui_hud_boat_t >& boats, const std::vector< const std::uint8_t* >& images,
                             std::int32_t local, std::int32_t count, const char* units )
{
    boats.clear( );
    for ( const std::uint8_t* image : images )
        boats.push_back( image ? ui_hud_boat_of( image ) : ui_hud_boat_t( ) );
    env.m_world.m_boats.clear( );
    for ( std::size_t k = 0; k < images.size( ); ++k )
        env.m_world.m_boats.push_back( images[ k ] ? &boats[ k ] : nullptr );
    env.m_world.m_manager = true;
    env.m_world.m_settings = units != nullptr;
    env.m_world.m_units = units ? units : "";
    env.m_world.m_local = local;
    env.m_world.m_boat_count = count;
}

class c_ui_race_hud
{
public:
    // `hooks.m_hud` is replaced by this object's env.
    c_ui_race_hud( entity_t& root, ui_bind_hooks_t hooks ) : m_root( root ), m_binding( root, with_env( std::move( hooks ) ) ) { }

    ui_hud_env_t& env( ) { return m_env; }
    c_ui_binding& binding( ) { return m_binding; }

    void initialize( ) { m_binding.hud_initialize( ); }
    void release( ) { m_binding.hud_release( ); }
    void tick( float dt ) { m_binding.hud_tick( dt ); }
    void post( const char* event, const ui_params_t& p ) { m_binding.post( fsm_hash( event ), p ); }
    // An output plug of `long_name` (the entity's path under the root, "/"-joined, root first).
    ui_ret_t fire( const std::string& long_name, const char* plug, const ui_params_t& p )
    {
        for ( const auto& b : m_binding.m_bound )
            if ( ui_long_name( *b->m_entity ) == long_name )
                return m_binding.fire_plug( *b->m_entity, plug, p );
        ui_ret_t out;
        out.m_known = false;
        return out;
    }

private:
    ui_bind_hooks_t with_env( ui_bind_hooks_t hooks )
    {
        hooks.m_hud = &m_env;
        return hooks;
    }

    entity_t& m_root;
    ui_hud_env_t m_env;   // declared before m_binding: the binding keeps its address
    c_ui_binding m_binding;
};
