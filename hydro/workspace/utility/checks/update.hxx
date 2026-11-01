#pragma once

#include <workspace/core/ui/bind.hxx>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The UI area's per-frame update checks (batch 14; included by checks/leaves.hxx
// before verify_ui_leaves_all, which runs verify_ui_update_all). Everything
// ui.hxx / menu.hxx / leaves.hxx define above is usable.

// c_ui_binding (core/ui bind.hxx) on OUR loaded Screens projects against
// retail's own frames (oracle case ui_frames: VuUIScreenEntity enter 0x629A70 /
// tick 0x629B90 / exit 0x629B00 with the whole graph live): the tree (pre-order
// parent, name, vtable), every entity's event map (hash, method) against
// retail's VuEventMap, then per scripted step the whole trace in order (fires,
// inputs with the recreated flag, broadcasts, sounds, unrecreated handlers -
// owner, plug, params bytes), every button's state and the game-data root.
inline bool verify_ui_frames( )
{
    const ui_loaded_t& base = ui_loaded( );
    std::vector< std::string > names( g_golden_entity_templates, g_golden_entity_templates + g_golden_entity_templates_count );
    static const entity_registry_t registry = build_entity_registry( names );
    const std::map< std::string, value_t >* templates = &base.templates;
    const entity_world_t world = ui_scripted_world( registry, [ templates ]( const std::string& name ) -> const value_t*
    {
        const auto found = templates->find( "VuTemplateAsset/" + name );
        return found == templates->end( ) ? nullptr : &found->second;
    } );
    int failed = 0, steps = 0, traces = 0, buttons = 0, rows = 0, entities = 0, unknown_inputs = 0, unknown_events = 0;
    auto bad = [ &failed ]( const char* what, const char* project, int step )
    {
        if ( ++failed <= 8 )
            LOG_ERROR( "ui frames: %s differs from retail (%s, step %d)", what, project, step );
    };
    float dt;
    std::memcpy( &dt, &g_golden_frm_dt, 4 );
    for ( std::size_t w = 0; w < g_golden_frm_projects_count; ++w )
    {
        const golden_frm_project_t& gp = g_golden_frm_projects[ w ];
        const golden_ui_project_t* project = nullptr;
        for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
            if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, gp.m_asset ) == 0 )
                project = &g_golden_ui_projects[ p ];
        if ( !project )
        {
            bad( "the project's presence in the loaded set", gp.m_asset, -1 );
            continue;
        }
        const unsigned char* raw = reinterpret_cast< const unsigned char* >( project->m_words );
        const value_t document = c_reader( std::vector< std::uint8_t >( raw, raw + project->m_bytes ) ).parse( );
        const std::unique_ptr< entity_t > root = entity_load_project( member( document, "VuProject" ), project->m_path, world );
        if ( !root )
        {
            bad( "the load", gp.m_asset, -1 );
            continue;
        }
        std::vector< std::string > down;
        value_t game_root;
        ui_bind_hooks_t hooks;
        hooks.m_input = [ &down ]( int, const char* name ) { return std::find( down.begin( ), down.end( ), name ) != down.end( ); };
        // the case's FRM_ANSWER (the license queries: platform services); anything else { 0, 0 }
        hooks.m_answer = []( const entity_t&, const char* plug )
        {
            ui_ret_t r;
            if ( std::strcmp( plug, "IsFullVersion" ) == 0 )
                r.m_words[ 0 ] = 3u, r.m_words[ 1 ] = 1u;
            else if ( std::strcmp( plug, "IsTrialVersion" ) == 0 )
                r.m_words[ 0 ] = 3u, r.m_words[ 1 ] = 0u;
            return r;
        };
        hooks.m_root = &game_root;
        c_ui_binding bind( *root, hooks );
        // the tree and the event maps
        bool same = bind.m_bound.size( ) == static_cast< std::size_t >( gp.m_entities );
        for ( int k = 0; same && k < gp.m_entities; ++k )
        {
            const golden_frm_entity_t& ge = g_golden_frm_entities[ gp.m_first_entity + k ];
            const ui_bound_t& b = *bind.m_bound[ static_cast< std::size_t >( k ) ];
            const int parent = b.m_entity->m_parent ? [ &bind, &b ]
            {
                for ( const auto& o : bind.m_bound )
                    if ( o->m_entity == b.m_entity->m_parent )
                        return o->m_index;
                return -2;
            }( ) : -1;
            if ( parent != ge.m_parent || b.m_entity->m_name != ge.m_name || b.m_vtable != ge.m_vtable )
            {
                same = false;
                break;
            }
            auto methods = b.m_methods;
            std::sort( methods.begin( ), methods.end( ) );
            if ( methods.size( ) != static_cast< std::size_t >( ge.m_rows ) )
                same = false;
            for ( int r = 0; same && r < ge.m_rows; ++r )
                same = methods[ static_cast< std::size_t >( r ) ].first == g_golden_frm_rows[ ge.m_first_row + r ].m_hash &&
                       methods[ static_cast< std::size_t >( r ) ].second == g_golden_frm_rows[ ge.m_first_row + r ].m_method;
            rows += ge.m_rows;
            ++entities;
        }
        if ( !same )
        {
            bad( "the bound tree / event maps", gp.m_asset, -1 );
            continue;
        }
        const auto states = [ &bind ]( std::vector< std::pair< int, std::string > >& out )
        {
            out.clear( );
            for ( const auto& b : bind.m_bound )
                if ( b->m_button )
                {
                    const c_ui_button& x = *b->m_button;
                    char text[ 64 ];
                    std::snprintf( text, sizeof( text ), "%02x%02x%02x.%02x%02x%02x%02x%02x.%u.", x.m_enabled, x.m_label, x.m_can_focus, x.m_focused,
                        x.m_want_focus, x.m_touched, x.m_touch_press, x.m_touch_release, x.m_press );
                    out.push_back( { b->m_index, std::string( text ) + x.m_string_id } );
                }
        };
        const auto same_buttons = [ &states, &buttons ]( int first, int count )
        {
            std::vector< std::pair< int, std::string > > have;
            states( have );
            bool ok = have.size( ) == static_cast< std::size_t >( count );
            for ( int k = 0; ok && k < count; ++k )
                ok = have[ static_cast< std::size_t >( k ) ].first == g_golden_frm_buttons[ first + k ].m_index &&
                     have[ static_cast< std::size_t >( k ) ].second == g_golden_frm_buttons[ first + k ].m_state;
            buttons += count;
            return ok;
        };
        if ( !same_buttons( gp.m_first_button, gp.m_buttons ) )
            bad( "the buttons' loaded state", gp.m_asset, -1 );
        for ( int s = 0; s < gp.m_steps; ++s )
        {
            const golden_frm_step_t& st = g_golden_frm_steps[ gp.m_first_step + s ];
            down.clear( );
            for ( const char* at = st.m_names; *at; )
            {
                const char* end = std::strchr( at, ',' );
                down.emplace_back( at, end ? static_cast< std::size_t >( end - at ) : std::strlen( at ) );
                at = end ? end + 1 : at + std::strlen( at );
            }
            bind.m_trace.clear( );
            if ( std::strcmp( st.m_op, "enter" ) == 0 )
                bind.enter( );
            else if ( std::strcmp( st.m_op, "exit" ) == 0 )
                bind.exit( );
            else
                bind.tick( dt, st.m_allow != 0 );
            bool ok = bind.m_trace.size( ) == static_cast< std::size_t >( st.m_logs );
            for ( int k = 0; ok && k < st.m_logs; ++k )
            {
                const ui_trace_t& t = bind.m_trace[ static_cast< std::size_t >( k ) ];
                const golden_frm_log_t& g = g_golden_frm_logs[ st.m_first_log + k ];
                ok = static_cast< int >( t.m_kind ) == g.m_kind && t.m_index == g.m_index && t.m_name == g.m_name && t.m_word == g.m_word &&
                     ( t.m_kind == ui_trace_kind_t::sound || t.m_kind == ui_trace_kind_t::event || ui_hex( t.m_params ) == g.m_params ) &&
                     ( t.m_known ? 1 : 0 ) == g.m_known;
                if ( !t.m_known && t.m_kind == ui_trace_kind_t::input )
                    ++unknown_inputs;
                if ( t.m_kind == ui_trace_kind_t::event )
                    ++unknown_events;
            }
            if ( !ok )
                bad( "the trace", gp.m_asset, s );
            traces += st.m_logs;
            if ( !same_buttons( st.m_first_button, st.m_buttons ) )
                bad( "the button states", gp.m_asset, s );
            std::string after;
            vujb_dump( game_root, after );
            if ( after != st.m_root )
                bad( "the game-data root", gp.m_asset, s );
            ++steps;
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "c_ui_binding matches retail's frames over %d real Screens projects (VuUIScreenEntity 0x629A70 / 0x629B90 / 0x629B00, "
                     "containers 0x62F7A0 / 0x62F7C0, buttons, VuUIEventEntity, VuIfEntity 0x5DDE60, the actions; graph 0x5023A0 / 0x404BE0, "
                     "broadcast 0x505690): %d entities and %d event-map rows, %d steps, %d trace entries in order (%d unrecreated inputs, %d "
                     "unrecreated handlers - hooks), %d button states, the game-data root",
            static_cast< int >( g_golden_frm_projects_count ), entities, rows, steps, traces, unknown_inputs, unknown_events, buttons );
    else
        LOG_ERROR( "the UI per-frame binding FAILED %d checks against retail's", failed );
    return failed == 0;
}

inline bool verify_ui_update_all( )
{
    bool ok = true;
    ok = verify_ui_frames( ) && ok;
    return ok;
}
