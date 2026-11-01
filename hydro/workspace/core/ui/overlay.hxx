#pragma once

#include <workspace/core/ui/bind.hxx>
#include <workspace/core/ui/frame.hxx>
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
    // VuHUDEntity slot 13 0x423A70 (vt+0x34; the state draws pass 1.0) over the
    // tree as ui_screen_frame does for a screen (frame.hxx): the key bits, the
    // crop matrix / VuUIImpl +0x8C pushed, each DIRECT child's first UI draw
    // component, containers 0x62F7E0 walked into; the leaves with `in`'s
    // inputs plus the HUD's runtime state: Visible / alpha as Show / Hide /
    // SetAlpha left them, the text getters (slot 12): VuHUDElementEntity
    // 0x423F40 over env( ).m_world (its wrong-way Type writes +0x108), VuUITextEntity
    // 0x446DE0 = VuStringDB slot 7 (in.m_string) on the runtime String ID, the
    // toast 0x4242D0 = its +0x118. The speedometer 0x42BA00, the minimap 0x427BD0
    // and the pause / recover / on-screen controls (0x4257A0 / 0x425F70 /
    // 0x42A0C0) are recreated (gauge.hxx / map.hxx / controls.hxx) but read the
    // boat's physics, the display and the input devices: `other` emits them
    // (the caller's - it holds those); without it they count unrecreated.
    ui_frame_t draw( ui_gfx_sort_t& sort, const ui_impl_t& impl, ui_frame_inputs_t in, float alpha = 1.0f,
                     const std::function< void( const ui_frame_draw_t& ) >& other = nullptr )
    {
        in.m_shown = [ this ]( const entity_t& e, std::uint8_t& visible, float& a )
        {
            const ui_bound_t* b = bound( e );
            if ( !b || !b->m_shown )
                return false;
            visible = b->m_shown->m_visible ? 1 : 0;
            a = b->m_shown->m_alpha;
            return true;
        };
        const std::function< std::string( const entity_t& ) > base_text = in.m_text;
        const std::function< std::string( const std::string& ) > string7 = in.m_string;
        in.m_text = [ this, base_text, string7 ]( const entity_t& e ) -> std::string
        {
            ui_bound_t* b = bound( e );
            if ( b && b->m_element )
            {
                b->m_element->m_alpha = b->m_shown->m_alpha;
                const std::string text = b->m_element->text( m_env.m_world );
                b->m_shown->m_alpha = b->m_element->m_alpha;
                return text;
            }
            if ( b && b->m_toast )
                return b->m_toast->m_text;
            if ( b && b->m_vtable == k_ui_text_vtable )
                return string7 ? string7( b->m_shown->m_string_id ) : std::string( );
            return base_text ? base_text( e ) : std::string( );
        };
        ui_frame_t frame;
        ui_gfx_stacks_t stacks;
        int depth = 0;
        std::function< void( const entity_t&, int, float ) > walk = [ & ]( const entity_t& e, int component, float a )
        {
            const ui_draw_target_t* target = ui_draw_target_of( e );
            if ( target && target->m_kind == ui_frame_kind_t::container )
            {
                ++frame.m_containers;
                ++depth;
                ui_container_draw( e, a, walk );
                --depth;
                return;
            }
            ui_frame_draw_t d;
            d.m_entity = &e;
            d.m_component = component;
            d.m_alpha = a;
            if ( !stacks.m_matrices.empty( ) )
                d.m_matrix = stacks.m_matrices.back( );
            if ( !stacks.m_floats.empty( ) )
                d.m_top = stacks.m_floats.back( );
            d.m_key_lo = sort.m_key_lo;
            d.m_key_hi = sort.m_key_hi;
            d.m_depth = depth;
            d.m_kind = target ? target->m_kind : ui_frame_kind_t::other;
            d.m_target = target ? target->m_target : 0u;
            const std::size_t before = sort.m_commands[ sort.m_index ].size( );
            if ( ui_frame_ready( d.m_kind, &in ) )
            {
                if ( !ui_frame_emit( sort, impl, d, in ) )
                    ++frame.m_unknown;
                d.m_emitted = true;
            }
            else if ( other && d.m_kind == ui_frame_kind_t::other )
            {
                other( d );
                d.m_emitted = true;
            }
            else
                ++frame.m_unrecreated;
            d.m_first = static_cast< int >( before );
            d.m_commands = static_cast< int >( sort.m_commands[ sort.m_index ].size( ) - before );
            frame.m_draws.push_back( d );
        };
        ui_hud_draw( sort, stacks, impl, m_root, alpha, walk );
        return frame;
    }

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

    // A pause / recover button's touch callback (c_ui_binding::hud_touch: 0
    // down, 1 up, 2 move) by the entity's long name - the touch device's
    // dispatch is the caller's (not read). False when no such button.
    bool touch( const std::string& long_name, int kind, const float point[ 2 ] )
    {
        for ( const auto& b : m_binding.m_bound )
            if ( ui_long_name( *b->m_entity ) == long_name )
                return m_binding.hud_touch( *b->m_entity, kind, point );
        return false;
    }

private:
    ui_bound_t* bound( const entity_t& e )
    {
        const int k = m_binding.bound_index( e );
        return k >= 0 ? m_binding.m_bound[ static_cast< std::size_t >( k ) ].get( ) : nullptr;
    }

    ui_bind_hooks_t with_env( ui_bind_hooks_t hooks )
    {
        hooks.m_hud = &m_env;
        return hooks;
    }

    entity_t& m_root;
    ui_hud_env_t m_env;   // declared before m_binding: the binding keeps its address
    c_ui_binding m_binding;
};
