#pragma once

#include <workspace/core/ui/bind.hxx>
#include <workspace/core/ui/overlay.hxx>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
    int sliders = 0;
    int failed = 0, steps = 0, traces = 0, buttons = 0, tickers = 0, rows = 0, entities = 0, unknown_inputs = 0, unknown_events = 0;
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
        // the case's ticker environment: VuStringDB answers "<id>|text", the measure FRM_MEASURE x the text's
        // length, the tick manager's delta FRM_DELTA, the font TxtWorld's
        float measure, delta;
        std::memcpy( &measure, &g_golden_frm_measure, 4 );
        std::memcpy( &delta, &g_golden_frm_delta, 4 );
        hooks.m_info.m_string = []( const std::string& id ) { return id + "|text"; };
        hooks.m_info.m_measure = [ measure ]( std::uint32_t, const std::string& text, float )
        {
            return static_cast< float >( static_cast< double >( measure ) * static_cast< double >( text.size( ) ) );
        };
        hooks.m_delta = [ delta ] { return delta; };
        const std::uint32_t font = gp.m_font;
        hooks.m_font = [ font ]( const entity_t& ) { return font; };
        // the sliders' environment: the settings manager's fields, the inverse crop matrix and the step's touch points (hex words)
        const auto words = []( const char* text, std::vector< float >& out )
        {
            out.clear( );
            for ( const char* at = text; at && *at; )
            {
                char* end = nullptr;
                const std::uint32_t bits = static_cast< std::uint32_t >( std::strtoul( at, &end, 16 ) );
                float f;
                std::memcpy( &f, &bits, 4 );
                out.push_back( f );
                if ( !end || end == at )
                    break;
                at = *end ? end + 1 : end;
            }
        };
        std::vector< float > settings0, inverse, points;
        words( gp.m_settings, settings0 );
        words( gp.m_inverse, inverse );
        ui_settings_t settings;
        if ( settings0.size( ) == 5 )
        {
            settings.m_steering = settings0[ 0 ];
            settings.m_effect = settings0[ 1 ];
            settings.m_music = settings0[ 2 ];
            settings.m_display = settings0[ 3 ];
            settings.m_display_out = settings0[ 4 ];
        }
        hooks.m_settings = &settings;
        hooks.m_inverse = inverse.size( ) == 16 ? inverse.data( ) : nullptr;
        hooks.m_touch_count = [ &points ] { return static_cast< int >( points.size( ) / 2 ); };
        hooks.m_touch_point = [ &points ]( int i, float out[ 2 ] )
        {
            if ( i >= 0 && static_cast< std::size_t >( 2 * i + 1 ) < points.size( ) )
                out[ 0 ] = points[ static_cast< std::size_t >( 2 * i ) ], out[ 1 ] = points[ static_cast< std::size_t >( 2 * i + 1 ) ];
        };
        const auto settings_text = [ &settings ]
        {
            const float f[ 5 ] = { settings.m_steering, settings.m_effect, settings.m_music, settings.m_display, settings.m_display_out };
            std::string text;
            for ( int k = 0; k < 5; ++k )
            {
                std::uint32_t bits;
                std::memcpy( &bits, &f[ k ], 4 );
                char word[ 16 ];
                std::snprintf( word, sizeof( word ), k ? ".0x%08X" : "0x%08X", bits );
                text += word;
            }
            return text;
        };
        c_ui_binding bind( *root, hooks );
        const auto same_sliders = [ &bind, &sliders ]( int first, int count )
        {
            std::vector< std::pair< int, std::string > > have;
            for ( const auto& b : bind.m_bound )
                if ( b->m_slider )
                {
                    char text[ 16 ];
                    std::snprintf( text, sizeof( text ), "%02x.%02x%02x", b->m_slider->m_visible, b->m_slider->m_focused, b->m_slider->m_latched );
                    have.push_back( { b->m_index, text } );
                }
            bool ok = have.size( ) == static_cast< std::size_t >( count );
            for ( int k = 0; ok && k < count; ++k )
                ok = have[ static_cast< std::size_t >( k ) ].first == g_golden_frm_sliders[ first + k ].m_index &&
                     have[ static_cast< std::size_t >( k ) ].second == g_golden_frm_sliders[ first + k ].m_state;
            sliders += count;
            return ok;
        };
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
        const auto same_tickers = [ &bind, &tickers ]( int first, int count )
        {
            std::vector< std::pair< int, std::string > > have;
            for ( const auto& b : bind.m_bound )
                if ( b->m_ticker )
                {
                    const ui_info_ticker_t& t = *b->m_ticker;
                    const float f[ 7 ] = { t.m_delay, t.m_speed, t.m_scroll.m_extent, t.m_scroll.m_gap, t.m_time, t.m_scroll.m_x, t.m_scroll.m_y };
                    std::string text;
                    for ( float x : f )
                    {
                        char word[ 16 ];
                        std::uint32_t bits;
                        std::memcpy( &bits, &x, 4 );
                        std::snprintf( word, sizeof( word ), "0x%08X.", bits );
                        text += word;
                    }
                    have.push_back( { b->m_index, text + t.m_id } );
                }
            bool ok = have.size( ) == static_cast< std::size_t >( count );
            for ( int k = 0; ok && k < count; ++k )
                ok = have[ static_cast< std::size_t >( k ) ].first == g_golden_frm_tickers[ first + k ].m_index &&
                     have[ static_cast< std::size_t >( k ) ].second == g_golden_frm_tickers[ first + k ].m_state;
            tickers += count;
            return ok;
        };
        if ( !same_buttons( gp.m_first_button, gp.m_buttons ) )
            bad( "the buttons' loaded state", gp.m_asset, -1 );
        if ( !same_tickers( gp.m_first_ticker, gp.m_tickers ) )
            bad( "the tickers' loaded state", gp.m_asset, -1 );
        if ( !same_sliders( gp.m_first_slider, gp.m_sliders ) )
            bad( "the sliders' loaded state", gp.m_asset, -1 );
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
            words( st.m_touches, points );   // "x,y;x,y": the separators are skipped alike
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
                char font[ 16 ];
                std::snprintf( font, sizeof( font ), "0x%08X", t.m_font );
                const bool params = t.m_kind == ui_trace_kind_t::measure ? std::strcmp( font, g.m_params ) == 0
                                    : t.m_kind == ui_trace_kind_t::sound || t.m_kind == ui_trace_kind_t::event || t.m_kind == ui_trace_kind_t::string ||
                                              t.m_kind == ui_trace_kind_t::touches || t.m_kind == ui_trace_kind_t::touch ||
                                              t.m_kind == ui_trace_kind_t::category
                                        ? true
                                        : ui_hex( t.m_params ) == g.m_params;
                ok = static_cast< int >( t.m_kind ) == g.m_kind && t.m_index == g.m_index && t.m_name == g.m_name && t.m_word == g.m_word && params &&
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
            if ( !same_tickers( st.m_first_ticker, st.m_tickers ) )
                bad( "the ticker states", gp.m_asset, s );
            if ( !same_sliders( st.m_first_slider, st.m_sliders ) )
                bad( "the slider states", gp.m_asset, s );
            if ( settings_text( ) != st.m_settings )
                bad( "the settings manager", gp.m_asset, s );
            std::string after;
            vujb_dump( game_root, after );
            if ( after != st.m_root )
                bad( "the game-data root", gp.m_asset, s );
            ++steps;
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "c_ui_binding matches retail's frames over %d real Screens projects (VuUIScreenEntity 0x629A70 / 0x629B90 / 0x629B00, "
                     "containers 0x62F7A0 / 0x62F7C0, buttons, VuUIEventEntity, VuIfEntity 0x5DDE60, the Info ticker, SetInfoString 0x471E30, "
                     "the actions; graph 0x5023A0 / 0x404BE0, broadcast 0x505690): %d entities and %d event-map rows, %d steps, %d trace entries "
                     "in order (%d unrecreated inputs, %d unrecreated handlers - hooks), %d button states, %d ticker states, %d slider states "
                     "(VuSliderEntity 0x46F8F0 / 0x46F940 / 0x46FCC0, the settings manager after each step), the game-data root",
            static_cast< int >( g_golden_frm_projects_count ), entities, rows, steps, traces, unknown_inputs, unknown_events, buttons, tickers,
            sliders );
    else
        LOG_ERROR( "the UI per-frame binding FAILED %d checks against retail's", failed );
    return failed == 0;
}

// The race HUD driver (core/ui overlay.hxx c_ui_race_hud over c_ui_binding's HUD
// kinds) on OUR loaded Race Pre / Game / PostGameHUD against retail's own run
// (oracle case hud_frames: gameInitialize 0x500B10 + OnProjectInitialized
// 0x501260, VuHUDEntity tick 0x423A20, the world's multi delay outputs fired,
// race events broadcast, gameRelease 0x500B80; the graph live): the tree and
// the event maps, then per step the whole trace in order and every text base /
// image / speedometer state.
inline std::string ui_hud_state_text( const ui_bound_t& b )
{
    char text[ 96 ];
    std::uint32_t bits[ 4 ];
    if ( b.m_speedometer )
    {
        std::memcpy( &bits[ 1 ], &b.m_speedometer->m_v_offset, 4 );
        std::memcpy( &bits[ 2 ], &b.m_speedometer->m_countdown, 4 );
        std::snprintf( text, sizeof( text ), "0x%08X.0x%08X.0x%08X", b.m_speedometer->m_metric, bits[ 1 ], bits[ 2 ] );
        return text;
    }
    std::memcpy( &bits[ 0 ], &b.m_shown->m_alpha, 4 );
    std::snprintf( text, sizeof( text ), "%02x.0x%08X", b.m_shown->m_visible ? 1 : 0, bits[ 0 ] );
    std::string out = text;
    if ( b.m_vtable == k_ui_text_vtable )
        out += "." + b.m_shown->m_string_id;
    else if ( b.m_toast )
    {
        std::memcpy( &bits[ 1 ], &b.m_toast->m_total, 4 );
        std::memcpy( &bits[ 2 ], &b.m_toast->m_fade, 4 );
        std::memcpy( &bits[ 3 ], &b.m_toast->m_left, 4 );
        std::snprintf( text, sizeof( text ), ".0x%08X.0x%08X.0x%08X.", bits[ 1 ], bits[ 2 ], bits[ 3 ] );
        out += text + b.m_toast->m_text;
    }
    return out;
}

inline bool verify_ui_hud_frames( )
{
    const ui_loaded_t& base = ui_loaded( );
    std::vector< std::string > names( g_golden_entity_templates, g_golden_entity_templates + g_golden_entity_templates_count );
    static const entity_registry_t registry = build_entity_registry( names );
    const std::map< std::string, value_t >* templates = &base.templates;
    int failed = 0, steps = 0, traces = 0, states = 0, rows = 0, entities = 0, unknown_inputs = 0, unknown_events = 0, hooks_seen = 0;
    auto bad = [ &failed ]( const char* what, const char* project, int step )
    {
        if ( ++failed <= 8 )
            LOG_ERROR( "ui hud frames: %s differs from retail (%s, step %d)", what, project, step );
    };
    const auto bits_of = []( const char* hex, std::uint8_t* out, int max ) -> int
    {
        int n = 0;
        for ( const char* at = hex; at[ 0 ] && at[ 1 ] && n < max; at += 2 )
        {
            const char pair[ 3 ] = { at[ 0 ], at[ 1 ], 0 };
            out[ n++ ] = static_cast< std::uint8_t >( std::strtoul( pair, nullptr, 16 ) );
        }
        return n;
    };
    for ( std::size_t w = 0; w < g_golden_hf_projects_count; ++w )
    {
        const golden_hf_project_t& gp = g_golden_hf_projects[ w ];
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
        const std::unique_ptr< entity_t > root = ui_hud_load( member( document, "VuProject" ), project->m_path, registry,
                                                              [ templates ]( const std::string& name ) -> const value_t*
                                                              {
                                                                  const auto found = templates->find( "VuTemplateAsset/" + name );
                                                                  return found == templates->end( ) ? nullptr : &found->second;
                                                              } );
        if ( !root )
        {
            bad( "the load", gp.m_asset, -1 );
            continue;
        }
        ui_bind_hooks_t hooks;
        hooks.m_string_db = []( const char* ) { return std::string( g_golden_hf_template ); };   // the case's VuStringDB slot 6
        hooks.m_external = []( entity_t&, const char*, const ui_params_t& ) { return ui_ret_t( ); };   // the case answers { 0, 0 }
        c_ui_race_hud hud( *root, hooks );
        c_ui_binding& bind = hud.binding( );
        bool same = bind.m_bound.size( ) == static_cast< std::size_t >( gp.m_entities );
        for ( int k = 0; same && k < gp.m_entities; ++k )
        {
            const golden_hf_entity_t& ge = g_golden_hf_entities[ gp.m_first_entity + k ];
            const ui_bound_t& b = *bind.m_bound[ static_cast< std::size_t >( k ) ];
            const int parent = b.m_entity->m_parent ? bind.bound_index( *b.m_entity->m_parent ) : -1;
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
                same = methods[ static_cast< std::size_t >( r ) ].first == g_golden_hf_rows[ ge.m_first_row + r ].m_hash &&
                       methods[ static_cast< std::size_t >( r ) ].second == g_golden_hf_rows[ ge.m_first_row + r ].m_method;
            rows += ge.m_rows;
            ++entities;
        }
        if ( !same )
        {
            bad( "the bound tree / event maps", gp.m_asset, -1 );
            continue;
        }
        const auto same_states = [ &bind, &states ]( int first, int count )
        {
            std::vector< std::pair< int, std::string > > have;
            for ( const auto& b : bind.m_bound )
                if ( b->m_shown || b->m_speedometer )
                    have.push_back( { b->m_index, ui_hud_state_text( *b ) } );
            bool ok = have.size( ) == static_cast< std::size_t >( count );
            for ( int k = 0; ok && k < count; ++k )
                ok = have[ static_cast< std::size_t >( k ) ].first == g_golden_hf_states[ first + k ].m_index &&
                     have[ static_cast< std::size_t >( k ) ].second == g_golden_hf_states[ first + k ].m_state;
            states += count;
            return ok;
        };
        if ( !same_states( gp.m_first_state, gp.m_states ) )
            bad( "the loaded states", gp.m_asset, -1 );
        entity_t* delay = nullptr;
        for ( const auto& b : bind.m_bound )
            if ( b->m_vtable == k_ui_multi_delay_vtable )
                delay = b->m_entity;
        std::vector< ui_hud_boat_t > boats;
        for ( int s = 0; s < gp.m_steps; ++s )
        {
            const golden_hf_step_t& st = g_golden_hf_steps[ gp.m_first_step + s ];
            const golden_hf_world_t& gw = g_golden_hf_worlds[ st.m_world ];
            ui_hud_env_t& env = hud.env( );
            env = ui_hud_env_t( );
            boats.assign( static_cast< std::size_t >( gw.m_boats ), ui_hud_boat_t( ) );
            for ( int k = 0; k < gw.m_boats; ++k )
            {
                const golden_hf_boat_t& gb = g_golden_hf_boats[ gw.m_first_boat + k ];
                ui_hud_boat_t& b = boats[ static_cast< std::size_t >( k ) ];
                b.m_lap = gb.m_lap;
                std::memcpy( &b.m_time_1200, &gb.m_lap_time, 4 );
                std::memcpy( &b.m_time_1204, &gb.m_race_time, 4 );
                std::memcpy( &b.m_time_1208, &gb.m_best, 4 );
                b.m_place = gb.m_place;
                b.m_laps = gb.m_laps;
                std::memcpy( &b.m_wrong_way, &gb.m_wrong_way, 4 );
                b.m_byte_812 = static_cast< std::uint8_t >( gb.m_byte_812 );
                env.m_world.m_boats.push_back( &b );
            }
            env.m_world.m_manager = gw.m_manager != 0;
            env.m_world.m_settings = gw.m_units != nullptr;
            env.m_world.m_units = gw.m_units ? gw.m_units : "";
            env.m_world.m_local = gw.m_local;
            env.m_world.m_boat_count = gw.m_count;
            env.m_control_method = gw.m_method;
            env.m_track_built = gw.m_built != 0;
            env.m_track_circuit = gw.m_circuit != 0;
            bind.m_trace.clear( );
            if ( std::strcmp( st.m_op, "init" ) == 0 )
                hud.initialize( );
            else if ( std::strcmp( st.m_op, "release" ) == 0 )
                hud.release( );
            else if ( std::strcmp( st.m_op, "tick" ) == 0 )
            {
                const std::uint32_t dt_bits = static_cast< std::uint32_t >( std::strtoul( st.m_arg, nullptr, 16 ) );
                float dt;
                std::memcpy( &dt, &dt_bits, 4 );
                hud.tick( dt );
            }
            else if ( std::strcmp( st.m_op, "fire" ) == 0 )
            {
                if ( delay )
                    bind.fire_plug( *delay, st.m_arg, ui_params_t( ) );
            }
            else
            {
                ui_params_t p;
                p.m_size = bits_of( st.m_params, p.m_bytes, 128 );
                hud.post( st.m_arg, p );
            }
            bool ok = bind.m_trace.size( ) == static_cast< std::size_t >( st.m_logs );
            for ( int k = 0; ok && k < st.m_logs; ++k )
            {
                const ui_trace_t& t = bind.m_trace[ static_cast< std::size_t >( k ) ];
                const golden_hf_log_t& g = g_golden_hf_logs[ st.m_first_log + k ];
                const bool params = t.m_kind == ui_trace_kind_t::event || t.m_kind == ui_trace_kind_t::string || t.m_kind == ui_trace_kind_t::hook
                                        ? true
                                        : ui_hex( t.m_params ) == g.m_params;
                ok = static_cast< int >( t.m_kind ) == g.m_kind && t.m_index == g.m_index && t.m_name == g.m_name && t.m_word == g.m_word && params &&
                     ( t.m_known ? 1 : 0 ) == g.m_known;
                if ( !t.m_known && t.m_kind == ui_trace_kind_t::input )
                    ++unknown_inputs;
                if ( t.m_kind == ui_trace_kind_t::event )
                    ++unknown_events;
                if ( t.m_kind == ui_trace_kind_t::hook )
                    ++hooks_seen;
            }
            if ( !ok )
                bad( "the trace", gp.m_asset, s );
            traces += st.m_logs;
            if ( !same_states( st.m_first_state, st.m_states ) )
                bad( "the states", gp.m_asset, s );
            if ( root->m_flags != st.m_flags )
                bad( "the root's +0x14", gp.m_asset, s );
            ++steps;
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "c_ui_race_hud matches retail's race HUD frames over %d projects (gameInitialize 0x500B10 + OnProjectInitialized 0x501260, "
                     "VuHUDEntity tick 0x423A20, gameRelease 0x500B80; Show / Hide / SetAlpha / SetStringID, VuTriggerForwarderEntity 0x5E0FC0, "
                     "VuIfEntity, VuIsCircuitTrack 0x42E1F0, VuControlMethodEntity 0x42EAE0 / 0x42EC80, VuStartLightsOutAction 0x405470, the toast, "
                     "the speedometer): %d entities and %d event-map rows, %d steps, %d trace entries in order (%d hooks, %d world-owned / audio "
                     "inputs, %d unrecreated handlers), %d states",
            static_cast< int >( g_golden_hf_projects_count ), entities, rows, steps, traces, hooks_seen, unknown_inputs, unknown_events, states );
    else
        LOG_ERROR( "the race HUD driver FAILED %d checks against retail's", failed );
    return failed == 0;
}

inline bool verify_ui_update_all( )
{
    bool ok = true;
    ok = verify_ui_frames( ) && ok;
    ok = verify_ui_hud_frames( ) && ok;
    return ok;
}
