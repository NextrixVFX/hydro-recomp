#pragma once

#include <workspace/core/ui/action.hxx>
#include <workspace/core/ui/entity.hxx>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The front end's event graph: the loaded projects' VuScriptComponents (plugs
// declared by each type's ctor - game/plugs.hxx, links resolved by the
// component's load - engine entity/script.hxx) and the fire that walks them.
//
// The world: ui_world (core/ui entity.hxx) keeps the VuScriptComponent's one
// property node ("Enable") as a ui_state_t on the component's m_state, while
// script_of() reads that same m_state as a script_component_t. So the wiring
// loads its projects through a second world, ui_scripted_world: the script
// component's state is its plugs and refs (script_construct), its post-load is
// the recreated 0x503D70 / 0x504320 (entity_post_load_component), and its
// "Enable" node load / mark are NOT run there (a hook in that world - nothing
// the fire reads; the other worlds still load it).

// ui_components_for with the VuScriptComponent's state replaced by its plugs and refs.
inline std::vector< entity_component_t > ui_scripted_components_for( const std::string& cls )
{
    std::vector< entity_component_t > out = ui_components_for( cls );
    for ( entity_component_t& c : out )
        if ( c.m_vtable == k_script_component_vtable )
            c.m_state = script_construct( cls, g_script_facts );
    return out;
}

inline void ui_scripted_load_component( entity_t& owner, entity_component_t& component, const value_t& properties, bool override )
{
    if ( component.m_vtable == k_script_component_vtable )
        return;
    ui_load_component( owner, component, properties, override );
}

inline void ui_scripted_mark_component( entity_t& owner, entity_component_t& component )
{
    if ( component.m_vtable == k_script_component_vtable )
        return;
    ui_mark_component( owner, component );
}

inline entity_world_t ui_scripted_world( const entity_registry_t& registry, std::function< const value_t*( const std::string& ) > templates )
{
    entity_world_t world = ui_world( registry, std::move( templates ) );
    world.m_components_for = ui_scripted_components_for;
    world.m_component_load = ui_scripted_load_component;
    world.m_component_mark = ui_scripted_mark_component;
    world.m_component_post = entity_post_load_component;
    return world;
}

// A VuRetVal (8 bytes: the type word, then the value). m_known is false when
// the value came from an input whose handler is not recreated.
struct ui_ret_t
{
    std::uint32_t m_words[ 2 ] = { 0u, 0u };
    bool m_known = true;
};

class c_ui_wiring
{
public:
    // VuScriptOutputPlug_fire 0x5023A0 (output plug vt slot 2), on `owner`'s
    // plug `plug`: the result is zeroed; return type (+0x24) 0 -> every link in
    // +0x50 order (the count re-read after each call), their results dropped;
    // else only the first link (when there is one) and its result is returned.
    ui_ret_t fire( entity_t& owner, int plug, const ui_params_t& params )
    {
        ui_ret_t out;
        script_component_t* sc = script_of( owner );
        if ( !sc || plug < 0 || static_cast< std::size_t >( plug ) >= sc->m_plugs.size( ) )
        {
            out.m_known = false;
            return out;
        }
        const script_plug_t& p = sc->m_plugs[ static_cast< std::size_t >( plug ) ];
        if ( p.m_row->m_type == 0 )
        {
            for ( std::size_t k = 0; k < p.m_links.size( ); ++k )
                input( p.m_links[ k ], params );   // an unknown input's effects are counted in m_unknown; the result is 0 regardless
            return out;
        }
        if ( !p.m_links.empty( ) )
            out = input( p.m_links.front( ), params );
        return out;
    }

    // An input plug's slot 2 0x404BE0: its VuMethod1 (+0x60) slot 2 with the
    // params. Recreated: the four screen actions' "Trigger" (0x472800 -> the
    // thunk 0x4077E3 -> slot 12, core/ui action.hxx) with the "Screen" node the
    // action's load left. Any other input: UNKNOWN (counted, result unknown).
    ui_ret_t input( const script_link_t& link, const ui_params_t& params )
    {
        ui_ret_t out;
        ui_action_kind_t kind;
        script_component_t* sc = link.m_entity ? script_of( *link.m_entity ) : nullptr;
        const char* name = sc && link.m_plug >= 0 && static_cast< std::size_t >( link.m_plug ) < sc->m_plugs.size( )
                               ? sc->m_plugs[ static_cast< std::size_t >( link.m_plug ) ].m_row->m_name
                               : nullptr;
        if ( !name || !ui_action_kind_of( link.m_entity->m_type, kind ) || std::string( name ) != "Trigger" )
        {
            ++m_unknown;
            out.m_known = false;
            return out;
        }
        c_ui_action action( kind, ui_action_screen( *link.m_entity ) );
        action.m_broadcast = m_broadcast;
        out.m_words[ 0 ] = action.trigger( params );
        return out;
    }

    // The action's +92 "Screen" as its load stored it ("" for pop / reload: no such node).
    static std::string ui_action_screen( const entity_t& e )
    {
        if ( !ui_is_state( e ) )
            return std::string( );
        const ui_state_t* s = static_cast< const ui_state_t* >( e.m_state.get( ) );
        for ( int n = 0; n < s->m_count; ++n )
            if ( std::string( s->m_rows[ n ]->m_name ) == "Screen" )
                return s->m_value[ n ].m_text;
        return std::string( );
    }

    // HOOK: VuEventManager_broadcast 0x505690 (each subscriber runs inside the call).
    std::function< void( std::uint32_t, const ui_params_t& ) > m_broadcast;
    int m_unknown = 0;  // inputs reached whose handler is not recreated
};
