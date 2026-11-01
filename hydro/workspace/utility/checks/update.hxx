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
    int sliders = 0, extras = 0;
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
        if ( gp.m_root0[ 0 ] )
            game_root = c_reader( ui_unhex( gp.m_root0 ) ).parse( );
        ui_bind_hooks_t hooks;
        // batch 19, the GameConfig* screens: the case's frm_answer (FNV-1a of "tag|texts": bit 0 the bool, (h >> 8) % 5
        // the placing), the real OptionDB / TrackDB (config_expand's blobs), VuStringDB slot 6 "<id>|short"
        const value_t option_db = ui_cx_document( 4 ), track_db = ui_cx_document( 2 );
        hooks.m_platform = []( const std::string& name )
        {
            const std::uint32_t h = fsm_hash( name.c_str( ) );
            return std::pair< bool, int >( ( h & 1u ) != 0, static_cast< int >( ( h >> 8 ) % 5u ) );
        };
        hooks.m_option_db = &option_db;
        hooks.m_track_db = &track_db;
        const value_t boat_db = ui_cx_document( 0 ), skin_db = ui_cx_document( 1 ), champ_db = ui_cx_document( 3 );
        hooks.m_boat_db = &boat_db;
        hooks.m_skin_db = &skin_db;
        hooks.m_champ_db = &champ_db;
        if ( gp.m_config_flags >= 0 )
            hooks.m_config_flags = static_cast< std::uint32_t >( gp.m_config_flags );
        hooks.m_string_db = []( const char* id ) { return std::string( id ) + "|short"; };
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
        // batch 19: the config selectors ("c:focus.nav bytes.trophy|combos") and the lock texts ("t:" +0x10C)
        const auto same_extras = [ &bind, &extras ]( int first, int count )
        {
            std::vector< std::pair< int, std::string > > have;
            for ( const auto& b : bind.m_bound )
                if ( b->m_config )
                {
                    ui_bound_config_t& c = *b->m_config;
                    char head[ 32 ];
                    std::snprintf( head, sizeof( head ), c.m_boat ? "b:%d." : "c:%d.", c.base( ).m_focus );
                    have.push_back( { b->m_index, head + ui_cn_nav_hex( c.m_nav ) + ( c.m_boat ? "|" : c.m_game.m_trophy ? ".1|" : ".0|" ) +
                                                      ui_cx_combo_text( c.base( ).m_combos ) } );
                }
                else if ( b->m_stat )
                    have.push_back( { b->m_index, "v:" + std::to_string( b->m_stat->m_value ) } );
                else if ( b->m_lock_text )
                    have.push_back( { b->m_index, "t:" + *b->m_lock_text } );
                else if ( b->m_combo )
                {
                    const c_ui_combo_box& x = *b->m_combo;
                    std::uint32_t shown;
                    std::memcpy( &shown, &x.m_shown, 4 );
                    char text[ 64 ];
                    std::snprintf( text, sizeof( text ), "s:%02x%02x%02x.%08x.%08x.%02x%02x%02x%02x.%d", x.m_latched, x.m_left_pressed, x.m_right_pressed,
                                   static_cast< std::uint32_t >( x.m_current ), shown, x.m_focused, x.m_down, x.m_held, x.m_up, x.m_press );
                    have.push_back( { b->m_index, text } );
                }
            bool ok = have.size( ) == static_cast< std::size_t >( count );
            for ( int k = 0; ok && k < count; ++k )
                ok = have[ static_cast< std::size_t >( k ) ].first == g_golden_frm_extras[ first + k ].m_index &&
                     have[ static_cast< std::size_t >( k ) ].second == g_golden_frm_extras[ first + k ].m_state;
            extras += count;
            return ok;
        };
        if ( !same_extras( gp.m_first_extra, gp.m_extras ) )
            bad( "the selectors' / lock texts' loaded state", gp.m_asset, -1 );
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
            if ( std::strcmp( st.m_op, "setup" ) == 0 )
                bind.config_setup( );
            else if ( std::strcmp( st.m_op, "enter" ) == 0 )
                bind.enter( );
            else if ( std::strcmp( st.m_op, "exit" ) == 0 )
                bind.exit( );
            else
                bind.tick( dt, st.m_allow != 0 );
            bool ok = bind.m_trace.size( ) == static_cast< std::size_t >( st.m_logs );
            if ( !ok && failed < 8 )
                LOG_ERROR( "ui frames: %s step %d: %d trace entries, retail %d", gp.m_asset, s, static_cast< int >( bind.m_trace.size( ) ), st.m_logs );
            for ( int k = 0; k < st.m_logs && k < static_cast< int >( bind.m_trace.size( ) ); ++k )
            {
                const ui_trace_t& t = bind.m_trace[ static_cast< std::size_t >( k ) ];
                const golden_frm_log_t& g = g_golden_frm_logs[ st.m_first_log + k ];
                if ( static_cast< int >( t.m_kind ) != g.m_kind || t.m_name != g.m_name || t.m_word != g.m_word )
                {
                    if ( failed < 8 )
                        LOG_ERROR( "ui frames: %s step %d entry %d: kind %d '%s' 0x%08X, retail kind %d '%s' 0x%08X", gp.m_asset, s, k,
                                   static_cast< int >( t.m_kind ), t.m_name.c_str( ), t.m_word, g.m_kind, g.m_name, g.m_word );
                    break;
                }
            }
            for ( int k = 0; ok && k < st.m_logs; ++k )
            {
                const ui_trace_t& t = bind.m_trace[ static_cast< std::size_t >( k ) ];
                const golden_frm_log_t& g = g_golden_frm_logs[ st.m_first_log + k ];
                char font[ 16 ];
                std::snprintf( font, sizeof( font ), "0x%08X", t.m_font );
                const bool params = t.m_kind == ui_trace_kind_t::measure ? std::strcmp( font, g.m_params ) == 0
                                    : t.m_kind == ui_trace_kind_t::sound || t.m_kind == ui_trace_kind_t::event || t.m_kind == ui_trace_kind_t::string ||
                                              t.m_kind == ui_trace_kind_t::touches || t.m_kind == ui_trace_kind_t::touch ||
                                              t.m_kind == ui_trace_kind_t::category || t.m_kind == ui_trace_kind_t::hook ||
                                              t.m_kind == ui_trace_kind_t::profile
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
            if ( !same_extras( st.m_first_extra, st.m_extras ) )
                bad( "the selector / lock text states", gp.m_asset, s );
            if ( settings_text( ) != st.m_settings )
                bad( "the settings manager", gp.m_asset, s );
            std::string after;
            vujb_dump( game_root, after );
            if ( after != st.m_root )
                bad( "the game-data root", gp.m_asset, s );
            ++steps;
        }
        if ( bind.m_config_faults )
            bad( "a selector path (not recreated)", gp.m_asset, -1 );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "c_ui_binding matches retail's frames over %d real Screens projects (VuUIScreenEntity 0x629A70 / 0x629B90 / 0x629B00, "
                     "containers 0x62F7A0 / 0x62F7C0, buttons, VuUIEventEntity, VuIfEntity 0x5DDE60, the Info ticker, SetInfoString 0x471E30, "
                     "the actions; graph 0x5023A0 / 0x404BE0, broadcast 0x505690): %d entities and %d event-map rows, %d steps, %d trace entries "
                     "in order (%d unrecreated inputs, %d unrecreated handlers - hooks), %d button states, %d ticker states, %d slider states "
                     "(VuSliderEntity 0x46F8F0 / 0x46F940 / 0x46FCC0, the settings manager after each step), %d selector / lock text states "
                     "(VuGameConfigEntity slots 4 / 6 / 13 / 14 + its input plugs, VuShortUnlockUITextEntity 0x445570), the game-data root",
            static_cast< int >( g_golden_frm_projects_count ), entities, rows, steps, traces, unknown_inputs, unknown_events, buttons, tickers,
            sliders, extras );
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
    if ( b.m_hud_button )
    {
        const ui_hud_button_t& h = *b.m_hud_button;
        std::memcpy( &bits[ 0 ], &h.m_point[ 0 ], 4 );
        std::memcpy( &bits[ 1 ], &h.m_point[ 1 ], 4 );
        std::memcpy( &bits[ 2 ], &h.m_boing, 4 );
        std::memcpy( &bits[ 3 ], &h.m_time, 4 );
        std::snprintf( text, sizeof( text ), "%02x%02x%02x.0x%08X.0x%08X.%d", h.m_down, h.m_held, h.m_up, bits[ 0 ], bits[ 1 ], h.m_state );
        std::string out = text;
        if ( h.m_recover )
        {
            std::snprintf( text, sizeof( text ), ".0x%08X.0x%08X", bits[ 2 ], bits[ 3 ] );
            out += text;
        }
        return out;
    }
    if ( b.m_onscreen )
    {
        std::snprintf( text, sizeof( text ), "%02x", b.m_onscreen->m_pressed );
        return text;
    }
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
    int draws = 0, commands = 0, stubs = 0, emitted = 0;
    const ui_texture_t texture_a = { g_golden_hf_textures[ 0 ].m_handle, g_golden_hf_textures[ 0 ].m_width, g_golden_hf_textures[ 0 ].m_height };
    const golden_img_texture_t& gtb = g_golden_hf_textures[ sizeof( g_golden_hf_textures ) / sizeof( g_golden_hf_textures[ 0 ] ) > 1 ? 1 : 0 ];
    const ui_texture_t texture_b = { gtb.m_handle, gtb.m_width, gtb.m_height };
    const golden_txt_env_t& txt = g_golden_txt_envs[ 0 ];
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
        ui_impl_t impl;
        const std::vector< std::uint8_t > crop = ui_unhex( gp.m_crop );
        if ( crop.size( ) == 64 )
            std::memcpy( impl.m_crop.m_matrix, crop.data( ), 64 );
        // batch 17: the controls' environment (the case's touch device / input manager / CN_INVERSE)
        ui_touch_device_t device;
        std::vector< std::array< float, 2 > > points;
        int pause_answer = 0;
        float inverse[ 16 ];
        std::memcpy( inverse, g_golden_hf_inverse, sizeof( inverse ) );
        ui_bind_hooks_t hooks;
        hooks.m_string_db = []( const char* ) { return std::string( g_golden_hf_template ); };   // the case's VuStringDB slot 6
        hooks.m_external = []( entity_t&, const char*, const ui_params_t& ) { return ui_ret_t( ); };   // the case answers { 0, 0 }
        hooks.m_touch_device = &device;
        hooks.m_crop = &impl.m_crop;
        hooks.m_inverse = inverse;
        hooks.m_touch_count = [ &points ] { return static_cast< int >( points.size( ) ); };
        hooks.m_touch_point = [ &points ]( int k, float out[ 2 ] )
        {
            if ( k >= 0 && static_cast< std::size_t >( k ) < points.size( ) )
            {
                out[ 0 ] = points[ static_cast< std::size_t >( k ) ][ 0 ];
                out[ 1 ] = points[ static_cast< std::size_t >( k ) ][ 1 ];
            }
        };
        hooks.m_input = [ &pause_answer ]( int, const char* name ) { return std::strcmp( name, "Pause" ) == 0 && pause_answer != 0; };
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
                if ( b->m_shown || b->m_speedometer || b->m_hud_button || b->m_onscreen )
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
        // The draw steps' environment (the case's TxtWorld environment 0): ONE sort per project (one harness each).
        ui_gfx_sort_t sort;
        sort.m_buffers[ 0 ].m_capacity = 0x300000;
        sort.m_buffers[ 1 ].m_used = 0x35;
        sort.m_buffers[ 1 ].m_capacity = 0x100000;
        for ( auto& sb : sort.m_buffers )
            sb.m_memory.resize( static_cast< std::size_t >( sb.m_capacity ) );
        ui_frame_inputs_t in;
        for ( int m = 0; m < 2; ++m )
        {
            in.m_env.m_material[ m ] = 0x5D001000u + 0x800u * static_cast< std::uint32_t >( m );   // TxtWorld's font materials
            in.m_env.m_material_key[ m ] = g_golden_txt_material_keys[ m ];
        }
        in.m_env.m_texture_material = g_golden_img_materials[ 0 ][ 0 ];
        in.m_env.m_texture_material_key = g_golden_img_materials[ 0 ][ 1 ];
        in.m_env.m_rect_material = g_golden_img_materials[ 1 ][ 0 ];
        in.m_env.m_rect_material_key = g_golden_img_materials[ 1 ][ 1 ];
        in.m_db.m_present = true;
        in.m_db.m_slot12 = txt.m_sdb_answer != 0;
        in.m_fontdb = &ui_font_db( 0 );
        const std::uint32_t font = gp.m_font;
        in.m_font = [ font ]( const entity_t& ) { ui_text_font_t f; f.m_asset = true; f.m_asset_font = font; return f; };
        in.m_texture = [ &texture_a ]( const entity_t& who, int ) -> const ui_texture_t* { return who.m_name.size( ) % 2 ? &texture_a : nullptr; };
        in.m_params_tail = [ &bind, &gp ]( const entity_t& who, std::uint8_t params[ 60 ] )
        {
            const int at = bind.bound_index( who );
            for ( int k = 0; k < gp.m_tails; ++k )
                if ( g_golden_hf_tails[ gp.m_first_tail + k ].m_index == at )
                {
                    const std::vector< std::uint8_t > tail = ui_unhex( g_golden_hf_tails[ gp.m_first_tail + k ].m_state );
                    std::memcpy( params + 41, tail.data( ), 19 );
                }
        };
        in.m_string = []( const std::string& id ) { return id + "~"; };   // the case's VuStringDB slot 7
        // batch 17: the controls' leaves live - the case's stand-ins (Texture -> texture 0, Pressed -> texture 1 for odd-length names)
        in.m_control_texture = [ &texture_a, &texture_b ]( const entity_t& who, int which ) -> const ui_texture_t*
        {
            return which == 0 ? &texture_a : who.m_name.size( ) % 2 ? &texture_b : nullptr;
        };
        const auto parse_floats = []( const char* text, std::vector< float >& out )
        {
            out.clear( );
            for ( const char* at = text; *at; )
            {
                char* end = nullptr;
                const std::uint32_t v = static_cast< std::uint32_t >( std::strtoul( at, &end, 16 ) );
                if ( end == at )
                    break;
                float f;
                std::memcpy( &f, &v, 4 );
                out.push_back( f );
                at = *end ? end + 1 : end;
            }
        };
        std::vector< float > floats;
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
            env.m_manager_28 = static_cast< std::uint32_t >( gw.m_manager_28 );
            env.m_method_present = true;
            env.m_driver.m_present = gw.m_manager != 0 && gw.m_local >= 0 && gw.m_local < gw.m_boats && gw.m_driver[ 0 ] >= 0;
            env.m_driver.m_slot12 = gw.m_driver[ 0 ] > 0;
            env.m_driver.m_8c = static_cast< std::uint8_t >( gw.m_driver[ 1 ] );
            env.m_driver.m_388 = static_cast< std::uint8_t >( gw.m_driver[ 2 ] );
            // batch 18: the gauges' inputs (the case's HF_GAUGES for this world)
            ui_hud_gauges_t& gauges = hud.gauges( );
            gauges = ui_hud_gauges_t( );
            if ( static_cast< std::size_t >( st.m_world ) < g_golden_hf_gauges_count )
            {
                const golden_hf_gauge_t& gg = g_golden_hf_gauges[ st.m_world ];
                gauges.m_live = true;
                gauges.m_display[ 0 ] = gg.m_display[ 0 ];
                gauges.m_display[ 1 ] = gg.m_display[ 1 ];
                const bool local = gw.m_manager != 0 && gw.m_local >= 0 && gw.m_local < gw.m_boats;
                ui_speedometer_boat_t& sb = gauges.m_boat;
                sb.m_present = local && gg.m_present != 0;
                std::memcpy( sb.m_velocity, gg.m_velocity, 8 );
                sb.m_boost_count = gg.m_count;
                std::memcpy( &sb.m_digit_scale, &gg.m_scale, 4 );
                std::memcpy( &sb.m_boost, &gg.m_boost, 4 );
                sb.m_mode = gg.m_mode;
                sb.m_byte_32d = static_cast< std::uint8_t >( gg.m_byte_32d );
                sb.m_byte_32c = local ? static_cast< std::uint8_t >( g_golden_hf_boats[ gw.m_first_boat + gw.m_local ].m_byte_812 ) : 0;
                gauges.m_map.m_manager = gw.m_manager != 0;
                for ( int k = 0; k < gg.m_fleet; ++k )
                {
                    const golden_hf_fleet_t& gf = g_golden_hf_fleet[ gg.m_first_fleet + k ];
                    ui_map_boat_t mb;
                    std::memcpy( mb.m_position, &gf.m_floats[ 0 ], 8 );
                    std::memcpy( mb.m_direction, &gf.m_floats[ 2 ], 8 );
                    std::memcpy( mb.m_heading, &gf.m_floats[ 4 ], 8 );
                    mb.m_answer = gf.m_answer != 0;
                    mb.m_kind = gf.m_kind;
                    gauges.m_map.m_boats.push_back( mb );
                }
                gauges.m_map.m_local_present = local && gw.m_local < gg.m_fleet;
                if ( gauges.m_map.m_local_present )
                    gauges.m_map.m_local = gauges.m_map.m_boats[ static_cast< std::size_t >( gw.m_local ) ];
                const char* asset = gp.m_asset;
                const auto self = [ &bind, asset ]( const entity_t& who )
                {
                    const int at = bind.bound_index( who );
                    for ( std::size_t k = 0; k < g_golden_hf_gauge_entities_count; ++k )
                        if ( g_golden_hf_gauge_entities[ k ].m_index == at && std::strcmp( g_golden_hf_gauge_entities[ k ].m_asset, asset ) == 0 )
                            return g_golden_hf_gauge_entities[ k ].m_address;
                    return 0u;
                };
                gauges.m_speedometer = [ self ]( const entity_t& who, ui_speedometer_t& s )
                {
                    const auto texture = []( std::uint32_t slot )
                    {
                        ui_texture_t t;
                        for ( const golden_hf_texture_t& g : g_golden_hf_spd_textures )
                            if ( g.m_slot == slot )
                                t = { g.m_handle, g.m_width, g.m_height };
                        return t;
                    };
                    s.m_material = g_golden_hf_gauge_materials[ 0 ][ 0 ];
                    s.m_material_key = g_golden_hf_gauge_materials[ 0 ][ 1 ];
                    s.m_back_imperial = texture( 0xA0 );
                    s.m_back_metric = texture( 0xA4 );
                    s.m_needle = texture( 0xA8 );
                    for ( int k = 0; k < 8; ++k )
                        s.m_boost[ k ] = texture( 0xC0u + 4u * static_cast< std::uint32_t >( k ) );
                    s.m_self = self( who );
                };
                gauges.m_minimap = [ self ]( const entity_t& who, ui_minimap_t& m )
                {
                    m.m_material = g_golden_hf_gauge_materials[ 1 ][ 0 ];
                    m.m_material_key = g_golden_hf_gauge_materials[ 1 ][ 1 ];
                    m.m_icon = g_golden_hf_gauge_materials[ 2 ][ 0 ];
                    m.m_self = self( who );
                };
            }
            parse_floats( st.m_points, floats );
            points.clear( );
            for ( std::size_t k = 0; k + 1 < floats.size( ); k += 2 )
                points.push_back( { floats[ k ], floats[ k + 1 ] } );
            bind.m_trace.clear( );
            const auto by_name = [ &bind ]( const char* name ) -> entity_t*
            {
                for ( const auto& b : bind.m_bound )
                    if ( b->m_entity->m_name == name )
                        return b->m_entity;
                return nullptr;
            };
            const bool b17_op = std::strcmp( st.m_op, "touches" ) == 0 || std::strcmp( st.m_op, "down" ) == 0 || std::strcmp( st.m_op, "up" ) == 0 ||
                                std::strcmp( st.m_op, "move" ) == 0 || std::strcmp( st.m_op, "pause" ) == 0 || std::strcmp( st.m_op, "prio" ) == 0;
            if ( b17_op )
            {
                if ( std::strcmp( st.m_op, "pause" ) == 0 )
                    pause_answer = std::atoi( st.m_arg );
                else if ( std::strcmp( st.m_op, "prio" ) == 0 )
                    device.m_priority = static_cast< std::uint32_t >( std::atoi( st.m_arg ) );
                else if ( std::strcmp( st.m_op, "touches" ) != 0 )
                {
                    parse_floats( st.m_point, floats );
                    const float pt[ 2 ] = { floats.size( ) > 0 ? floats[ 0 ] : 0.0f, floats.size( ) > 1 ? floats[ 1 ] : 0.0f };
                    if ( entity_t* who = by_name( st.m_arg ) )
                        bind.hud_touch( *who, std::strcmp( st.m_op, "down" ) == 0 ? 0 : std::strcmp( st.m_op, "up" ) == 0 ? 1 : 2, pt );
                }
            }
            else if ( std::strcmp( st.m_op, "init" ) == 0 )
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
            else if ( std::strcmp( st.m_op, "draw" ) == 0 )
            {
                sort.m_index = txt.m_index;
                sort.m_key_lo = txt.m_key_lo;
                sort.m_key_hi = txt.m_key_hi;
                std::vector< ui_gfx_command_t >& list = sort.m_commands[ txt.m_index ];
                const std::size_t before = list.size( );
                const ui_frame_t frame = hud.draw( sort, impl, in, 1.0f );
                bool same = frame.m_unknown == 0 && list.size( ) - before == static_cast< std::size_t >( st.m_commands ) &&
                            sort.m_key_lo == st.m_key[ 0 ] && sort.m_key_hi == st.m_key[ 1 ];
                if ( !same && failed < 8 )
                    LOG_ERROR( "ui hud frames: draw step %d: %d commands (retail %d), %d unknown", s, static_cast< int >( list.size( ) - before ), st.m_commands,
                               frame.m_unknown );
                for ( int c = 0; same && c < st.m_commands; ++c )
                {
                    const golden_txt_command_t& want = g_golden_hf_commands[ st.m_first_command + c ];
                    const ui_gfx_command_t& have = list[ before + static_cast< std::size_t >( c ) ];
                    const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
                    const auto& memory = sort.m_buffers[ txt.m_index ].m_memory;
                    same = std::memcmp( have.m_words, want.m_words, sizeof( have.m_words ) ) == 0 && have.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                           std::memcmp( memory.data( ) + have.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
                    if ( !same && failed < 8 )
                    {
                        int byte = -1;
                        for ( std::size_t q = 0; byte < 0 && q < record.size( ) && have.m_words[ 3 ] + q < memory.size( ); ++q )
                            if ( memory[ have.m_words[ 3 ] + q ] != record[ q ] )
                                byte = static_cast< int >( q );
                        LOG_ERROR( "ui hud frames: draw step %d command %d (callback 0x%08X / retail 0x%08X): record byte %d differs (-1 = a word)", s, c,
                                   have.m_words[ 2 ], want.m_words[ 2 ], byte );
                    }
                    ++commands;
                }
                int u = 0;
                for ( const ui_frame_draw_t& d : frame.m_draws )
                {
                    emitted += d.m_emitted ? 1 : 0;
                    if ( d.m_emitted )
                        continue;
                    std::uint32_t abits;
                    std::memcpy( &abits, &d.m_alpha, 4 );
                    same = same && u < st.m_stubs && g_golden_hf_stubs[ st.m_first_stub + u ].m_index == bind.bound_index( *d.m_entity ) &&
                           g_golden_hf_stubs[ st.m_first_stub + u ].m_alpha == abits;
                    ++u;
                }
                same = same && u == st.m_stubs;
                stubs += u;
                ++draws;
                if ( !same )
                    bad( "the draw (commands / records / unrecreated leaves / key)", gp.m_asset, s );
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
                     "the speedometer, the controls' ticks: on-screen 0x429E10, pause / recover 0x4255B0 + slots 13 / 14 0x4261B0 / 0x4262A0 / "
                     "0x425DC0 / 0x425F10, touch callbacks 0x425400 / 0x425490 / 0x4254B0, VuTouch list 0x53FB90 / 0x53FA60): %d entities and "
                     "%d event-map rows, %d steps, %d trace entries in order (%d hooks, %d world-owned / audio "
                     "inputs, %d unrecreated handlers), %d states; %d draws (0x423A70, text / image leaves live over the runtime state): %d leaves "
                     "emitted (batch 18: the speedometer 0x42BA00 and minimap 0x427BD0 live too), %d commands and records byte for byte, %d "
                     "unrecreated leaves (stubbed) in order",
            static_cast< int >( g_golden_hf_projects_count ), entities, rows, steps, traces, hooks_seen, unknown_inputs, unknown_events, states, draws,
            emitted, commands, stubs );
    else
        LOG_ERROR( "the race HUD driver FAILED %d checks against retail's", failed );
    return failed == 0;
}

// THE TEST PROFILE (core/ui leave.hxx ui_profile_all_unlocked) through
// ui_front_end_root and ui_front_end_game_data_root against retail's selector
// chain + VuFrontEndGameMode exit 0x4BF9B0 under the same answers (oracle case
// front_end_defaults: front_end_root's harness, every unlock / license /
// progress / package answer true, ranks 0, store beta false, 0x45E300 live;
// then the exit with VuRandom = gd_rng(12345)): per mode and run the root after
// the boat screen, the root after the exit (GameData built), VuRandom after,
// the number of VuStringDB calls.
inline bool verify_ui_front_end_defaults( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    int failed = 0, runs = 0;
    const value_t boat_db = ui_fr_document( 0 ), skin_db = ui_fr_document( 1 ), track_db = ui_fr_document( 2 ), champ_db = ui_fr_document( 3 ),
                  option_db = ui_fr_document( 4 );
    ui_front_end_dbs_t dbs;
    dbs.m_option_db = &option_db;
    dbs.m_boat_db = &boat_db;
    dbs.m_skin_db = &skin_db;
    dbs.m_track_db = &track_db;
    dbs.m_champ_db = &champ_db;
    const ui_profile_t p = ui_profile_all_unlocked( );
    for ( std::size_t r = 0; r < g_golden_fd_runs_count; ++r )
    {
        const golden_fd_run_t& run = g_golden_fd_runs[ r ];
        const golden_fr_pair_t* pair = nullptr;
        for ( std::size_t k = 0; k < g_golden_fr_pairs_count; ++k )
            if ( std::strcmp( g_golden_fr_pairs[ k ].m_mode, run.m_mode ) == 0 )
                pair = &g_golden_fr_pairs[ k ];
        const auto entity_at = [ & ]( const char* project, int index ) -> const entity_t*
        {
            for ( std::size_t q = 0; q < g_golden_ui_projects_count; ++q )
                if ( std::strcmp( g_golden_ui_projects[ q ].m_asset, project ) == 0 && index >= 0 &&
                     static_cast< std::size_t >( index ) < loaded.trees[ q ].size( ) )
                    return loaded.trees[ q ][ static_cast< std::size_t >( index ) ].second;
            return nullptr;
        };
        const entity_t* ge = pair ? entity_at( pair->m_game_project, pair->m_game_index ) : nullptr;
        const entity_t* be = pair ? entity_at( pair->m_boat_project, pair->m_boat_index ) : nullptr;
        if ( !ge || !be )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui front end defaults: the %s screens not loaded", run.m_mode );
            continue;
        }
        value_t root = c_reader( ui_unhex( run.m_root ) ).parse( );
        ui_config_hooks_t h;
        h.touch = [ ]( ) { };
        h.draw_add = [ ]( ) { };
        h.fire = [ ]( const std::string&, const ui_params_t& ) { return true; };
        h.broadcast = [ ]( std::uint32_t, const ui_params_t& ) { };
        h.sound = [ ]( const char* ) { };
        h.trophy = [ ]( const char* ) { };
        const bool done = ui_front_end_root( root, run.m_mode, ui_front_end_screen( *ge, pair->m_game_flags ), ui_front_end_screen( *be, pair->m_boat_flags ),
                                             dbs, run.m_op, p, h );
        std::string after;
        vujb_dump( root, after );
        const std::vector< std::uint8_t > rng0 = ui_unhex( run.m_rng ), rng1 = ui_unhex( run.m_rng_after );
        fleet_random_t rng;
        std::memcpy( &rng.seed, rng0.data( ), 4 );
        std::memcpy( &rng.last, rng0.data( ) + 4, 4 );
        std::memcpy( rng.table, rng0.data( ) + 8, 128 );
        ui_game_env_t env;
        env.boat_db = &boat_db;
        env.track_db = &track_db;
        env.random = &rng;
        env.player_name = "Player One";
        int strings = 0;
        env.string_db = [ &strings ]( const char* id ) { ++strings; return std::string( "S" ) + id; };
        static const value_t null_round{ };
        env.championship = &null_round;
        ui_front_end_game_data_root( root, env );
        std::string game_after;
        vujb_dump( root, game_after );
        const bool same = done && after == run.m_after && game_after == run.m_game_after && strings == run.m_strings && rng1.size( ) == 136 &&
                          std::memcmp( &rng.seed, rng1.data( ), 4 ) == 0 && std::memcmp( &rng.last, rng1.data( ) + 4, 4 ) == 0 &&
                          std::memcmp( rng.table, rng1.data( ) + 8, 128 ) == 0;
        ++runs;
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui front end defaults: %s run %d (op \"%s\") differs from retail (root after %s, GameData root %s, strings %d / %d)", run.m_mode,
                       static_cast< int >( r ), run.m_op, after == run.m_after ? "same" : "DIFFERS", game_after == run.m_game_after ? "same" : "DIFFERS",
                       strings, run.m_strings );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the everything-unlocked test profile (ui_profile_all_unlocked) through ui_front_end_root + ui_front_end_game_data_root matches "
                     "retail's selector chain and VuFrontEndGameMode exit 0x4BF9B0 under the same answers on %d runs (5 modes x op \"\" / \"push\"): "
                     "the root after the boat screen, the GameData root, VuRandom, the string calls", runs );
    else
        LOG_ERROR( "the front-end test profile FAILED %d runs against retail's", failed );
    return failed == 0;
}

// ui_race_checkpoint (core/ui split.hxx) against retail's VuRaceGame slot 15
// 0x48B180 on a fake game (oracle case race_split): +0x6D8 / +0x6DC, every
// condition's value / pulse bytes (and the record before the vector - the
// missing-condition write), the split vector after.
inline bool verify_ui_race_split( )
{
    int failed = 0, runs = 0, pulses = 0, splits = 0, missing = 0;
    for ( std::size_t r = 0; r < g_golden_spl_vectors_count; ++r )
    {
        const golden_spl_vector_t& v = g_golden_spl_vectors[ r ];
        const golden_spl_conditions_t& gc = g_golden_spl_conditions[ v.m_conditions ];
        const golden_spl_json_t& gj = g_golden_spl_json[ v.m_json ];
        c_fsm fsm;
        for ( int k = 0; k < gc.m_count; ++k )
        {
            fsm_condition_t c;
            c.m_name = gc.m_names[ k ];
            c.m_hash = fsm_hash( gc.m_names[ k ] );
            fsm.m_conditions.push_back( c );
        }
        ui_race_split_t g;
        g.m_laps = v.m_laps;
        std::memcpy( &g.m_best_time, &v.m_best, 4 );
        g.m_best_splits.tag = static_cast< std::uint32_t >( gj.m_tag );
        for ( int k = 0; gj.m_tag == 5 && k < gj.m_count; ++k )
        {
            const golden_spl_element_t& ge = g_golden_spl_elements[ gj.m_first + k ];
            value_t e;
            e.tag = static_cast< std::uint32_t >( ge.m_tag );
            e.integer = static_cast< int >( ge.m_bits );
            std::memcpy( &e.real, &ge.m_bits, 4 );
            e.int64 = ge.m_int64;
            g.m_best_splits.list.push_back( e );
        }
        std::memcpy( &g.m_split_delta, &g_golden_spl_sentinel, 4 );
        std::memcpy( &g.m_result_delta, &g_golden_spl_sentinel, 4 );
        for ( int k = 0; k < v.m_prior; ++k )
        {
            float t;
            std::memcpy( &t, &g_golden_spl_words[ v.m_first_word + k ], 4 );
            g.m_splits.push_back( t );
        }
        ui_race_split_boat_t boat;
        boat.m_human = v.m_human != 0;
        boat.m_lap = v.m_lap;
        std::memcpy( &boat.m_race_time, &v.m_race, 4 );
        const bool ok = ui_race_checkpoint( g, fsm, boat, v.m_finish != 0 );
        // the bytes as retail's records hold them: +0x1C value, +0x1D pulse; the record before the vector first
        std::string have = ok ? "0000" : "0001";
        for ( const fsm_condition_t& c : fsm.m_conditions )
        {
            char text[ 8 ];
            std::snprintf( text, sizeof( text ), "%02x%02x", c.m_value, c.m_pulse );
            have += text;
            pulses += c.m_pulse;
        }
        missing += ok ? 0 : 1;
        std::uint32_t deltas[ 2 ];
        std::memcpy( &deltas[ 0 ], &g.m_split_delta, 4 );
        std::memcpy( &deltas[ 1 ], &g.m_result_delta, 4 );
        bool same = have == v.m_pulses && deltas[ 0 ] == v.m_deltas[ 0 ] && deltas[ 1 ] == v.m_deltas[ 1 ] &&
                    g.m_splits.size( ) == static_cast< std::size_t >( v.m_after );
        for ( int k = 0; same && k < v.m_after; ++k )
            same = std::memcmp( &g.m_splits[ static_cast< std::size_t >( k ) ], &g_golden_spl_words[ v.m_first_word + v.m_prior + k ], 4 ) == 0;
        splits += v.m_after;
        ++runs;
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui race split: vector %d differs from retail (conditions %s / %s, deltas 0x%08X 0x%08X / 0x%08X 0x%08X, splits %d / %d)",
                       static_cast< int >( r ), have.c_str( ), v.m_pulses, deltas[ 0 ], deltas[ 1 ], v.m_deltas[ 0 ], v.m_deltas[ 1 ],
                       static_cast< int >( g.m_splits.size( ) ), v.m_after );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_race_checkpoint matches retail's VuRaceGame slot 15 0x48B180 (0x555C60 condition lookup, getValue 0x4FC590, push_back "
                     "0x440990) on %d vectors: +0x6D8 / +0x6DC, %d pulses set, %d missing-condition writes, %d split times after",
            runs, pulses, missing, splits );
    else
        LOG_ERROR( "the race split FAILED %d vectors against retail's", failed );
    return failed == 0;
}

inline bool verify_ui_update_all( )
{

    bool ok = true;
    ok = verify_ui_frames( ) && ok;
    ok = verify_ui_hud_frames( ) && ok;
    ok = verify_ui_front_end_defaults( ) && ok;
    ok = verify_ui_race_split( ) && ok;
    return ok;
}
