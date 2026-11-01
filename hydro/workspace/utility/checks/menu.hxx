#pragma once

#include <workspace/core/ui/frame.hxx>
#include <workspace/core/ui/navigate.hxx>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The UI area's menu checks (included by checks/ui.hxx before verify_ui_all,
// which runs verify_ui_menu_all): the config selectors' navigation and the
// screens' frame. Everything ui.hxx defines above its verify_ui_all is usable.

// The 19 nav bytes +0x2CD..+0x2DF as config_nav prints them.
inline std::string ui_cn_nav_hex( const ui_config_nav_t& n )
{
    std::uint8_t raw[ 19 ];
    raw[ 0 ] = n.m_down;
    raw[ 1 ] = n.m_held;
    raw[ 2 ] = n.m_up;
    std::memcpy( raw + 3, n.m_touch, 8 );
    std::memcpy( raw + 11, &n.m_press, 4 );
    std::memcpy( raw + 15, &n.m_press_index, 4 );
    std::string out;
    char buf[ 4 ];
    for ( std::uint8_t b : raw )
    {
        std::snprintf( buf, sizeof( buf ), "%02x", b );
        out += buf;
    }
    return out;
}

inline float ui_cn_float( std::uint32_t bits )
{
    float f;
    std::memcpy( &f, &bits, 4 );
    return f;
}

// ui_config_input / ui_config_touch_* / ui_config_tick / ui_config_focus /
// ui_config_change (hydro core/ui/navigate.hxx: slot 14 0x448A50, the touch
// callbacks 0x4481C0 / 0x4482C0 / 0x4482D0, slot 13 0x448470, 0x4496E0,
// 0x449830) against retail on each GameConfig* screen's VuGameConfigEntity
// (oracle case config_nav): the nav properties read from OUR loaded entity and
// compared with retail's floats; slot 4 + 0x45D050 (ui_game_config_build /
// _initialize) then 47 scripted pad / touch / tick steps per run, after each:
// the whole root, every combo record, the focus, the 19 nav bytes, the trophy
// byte and every hook call in order with its arguments.
inline bool verify_ui_config_nav( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    int failed = 0, runs = 0, steps = 0, calls = 0;
    auto bad = [ &failed ]( const char* what, int k )
    {
        if ( ++failed <= 8 )
            LOG_ERROR( "ui config nav: %s differs from retail (step %d)", what, k );
    };
    const value_t track_db = ui_cx_document( 2 ), option_db = ui_cx_document( 4 );
    static const float inverse[ 16 ] = { 1.25f, 0.03125f, 0, 0, -0.0625f, 0.875f, 0, 0, 0, 0, 1, 0, -0.1f, 0.05f, 0, 1 };
    for ( std::size_t k = 0; k < g_golden_cn_entities_count; ++k )
    {
        const golden_cn_entity_t& want = g_golden_cn_entities[ k ];
        const entity_t* have = nullptr;
        for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
            if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, want.m_project ) == 0 && want.m_index >= 0 &&
                 static_cast< std::size_t >( want.m_index ) < loaded.trees[ p ].size( ) )
                have = loaded.trees[ p ][ static_cast< std::size_t >( want.m_index ) ].second;
        if ( !have || have->m_type != "VuGameConfigEntity" || have->m_name != want.m_name )
        {
            bad( "the authored entity", static_cast< int >( k ) );
            continue;
        }
        // The nav fields from OUR loaded entity, then compared with retail's.
        ui_config_nav_t nav0;
        std::uint32_t bits[ 14 ] = { };
        auto take = [ & ]( const char* name, int words, int at )
        {
            if ( const ui_slot_t* v = ui_property_value( *have, name ) )
                for ( int w = 0; w < words; ++w )
                    bits[ at + w ] = v->m_bits[ w ];
            else
                bad( "a nav property (missing)", static_cast< int >( k ) );
        };
        take( "Screen Scale", 2, 0 );
        take( "Position X", 1, 2 );
        take( "Position Y", 1, 3 );
        take( "Spacing", 1, 4 );
        std::memcpy( &bits[ 5 ], &nav0.m_scroll_time, 4 );
        const char* const rects[ ] = { "TouchLeft X", "TouchLeft Y", "TouchLeft Width", "TouchLeft Height",
                                       "TouchRight X", "TouchRight Y", "TouchRight Width", "TouchRight Height" };
        for ( int r = 0; r < 8; ++r )
            take( rects[ r ], 1, 6 + r );
        for ( int w = 0; w < 14; ++w )
            if ( bits[ w ] != want.m_floats[ w ] )
                bad( "a nav float", static_cast< int >( k ) );
        nav0.m_scale[ 0 ] = ui_cn_float( bits[ 0 ] );
        nav0.m_scale[ 1 ] = ui_cn_float( bits[ 1 ] );
        nav0.m_position[ 0 ] = ui_cn_float( bits[ 2 ] );
        nav0.m_position[ 1 ] = ui_cn_float( bits[ 3 ] );
        nav0.m_spacing = ui_cn_float( bits[ 4 ] );
        for ( int r = 0; r < 4; ++r )
        {
            nav0.m_left[ r ] = ui_cn_float( bits[ 6 + r ] );
            nav0.m_right[ r ] = ui_cn_float( bits[ 10 + r ] );
        }
        if ( ui_cn_nav_hex( nav0 ) != want.m_initial )
            bad( "the ctor's nav bytes", static_cast< int >( k ) );
        const ui_front_end_screen_t screen = ui_front_end_screen( *have, want.m_flags );
        if ( screen.m_option_type != want.m_option_type )
            bad( "the OptionType", static_cast< int >( k ) );
        for ( int r = 0; r < want.m_runs; ++r )
        {
            const golden_cn_run_t& run = g_golden_cn_runs[ want.m_first + r ];
            const std::vector< std::uint8_t > rdoc = ui_unhex( run.m_root );
            value_t root = c_reader( rdoc ).parse( );
            ui_config_env_t env;
            env.m_root = &root;
            const golden_cn_step_t* step = nullptr;
            int at = 0;
            bool hooks_same = true;
            auto next = [ & ]( int kind ) -> const golden_bci_log_t*
            {
                if ( !step || at >= step->m_logs || g_golden_cn_logs[ step->m_first_log + at ].m_kind != kind )
                {
                    hooks_same = false;
                    return nullptr;
                }
                ++calls;
                return &g_golden_cn_logs[ step->m_first_log + at++ ];
            };
            ui_profile_t p;
            p.unlock = [ & ]( int slot, const std::string& a, const std::string& b, const std::string& c )
            {
                const golden_bci_log_t* g = next( 0 );
                hooks_same = hooks_same && g && g->m_slot == slot && a == g->m_a && b == g->m_b && c == g->m_c;
                return g && g->m_answer != 0;
            };
            p.license = [ & ]( ) { const golden_bci_log_t* g = next( 1 ); return g && g->m_answer != 0; };
            p.progress = [ & ]( const std::string& value )
            {
                const golden_bci_log_t* g = next( 2 );
                hooks_same = hooks_same && g && value == g->m_a;
                return g && g->m_answer != 0;
            };
            p.store_beta = [ & ]( ) { const golden_bci_log_t* g = next( 3 ); return g && g->m_answer != 0; };
            p.package = [ & ]( const std::string& asset )
            {
                const golden_bci_log_t* g = next( 8 );
                hooks_same = hooks_same && g && asset == g->m_a;
                return g && g->m_answer != 0;
            };
            p.next_choice = [ & ]( const value_t&, std::string& ) { next( 9 ); return false; };
            p.champ_rank = [ & ]( const std::string& champ )
            {
                const golden_bci_log_t* g = next( 10 );
                hooks_same = hooks_same && g && champ == g->m_a;
                return g ? g->m_answer : 0;
            };
            p.race_rank = [ & ]( const std::string& mode, const std::string& track, const std::string& course )
            {
                const golden_bci_log_t* g = next( 11 );
                hooks_same = hooks_same && g && mode == g->m_a && track == g->m_b && course == g->m_c;
                return g ? g->m_answer : 0;
            };
            ui_config_hooks_t h;
            h.touch = [ & ]( ) { next( 4 ); };
            h.draw_add = [ & ]( ) { next( 13 ); };
            h.fire = [ & ]( const std::string& plug, const ui_params_t& params )
            {
                const golden_bci_log_t* g = next( 5 );
                hooks_same = hooks_same && g && plug == g->m_a && ui_bci_hex( params ) == g->m_b;
                return g != nullptr;
            };
            h.broadcast = [ & ]( std::uint32_t hash, const ui_params_t& params )
            {
                const golden_bci_log_t* g = next( 6 );
                char text[ 16 ];
                std::snprintf( text, sizeof( text ), "0x%08X", hash );
                hooks_same = hooks_same && g && std::strcmp( g->m_a, text ) == 0 && ui_bci_hex( params ) == g->m_b;
            };
            h.sound = [ & ]( const char* event )
            {
                const golden_bci_log_t* g = next( 7 );
                hooks_same = hooks_same && g && std::strcmp( g->m_a, event ) == 0;
            };
            h.trophy = [ & ]( const char* model )
            {
                const golden_bci_log_t* g = next( 12 );
                hooks_same = hooks_same && g && std::strcmp( g->m_a, model ) == 0;
            };
            ui_config_entity_t e;
            e.m_single_player = screen.m_single_player;
            e.m_force_online = screen.m_force_online;
            ui_game_config_t2 g;
            ui_config_nav_t n = nav0;
            value_t doc;
            ui_config_class_t cls;
            ui_game_config_class_full( cls, e, g, root, p, h );
            ++runs;
            for ( int s = 0; s < run.m_steps; ++s )
            {
                step = &g_golden_cn_steps[ run.m_first + s ];
                const int row = run.m_first + s;
                at = 0;
                hooks_same = true;
                bool done = true;
                switch ( step->m_op )
                {
                case 5:
                    done = ui_game_config_build( e, env, doc, &option_db, screen.m_option_type, &track_db, screen.m_flags, p, h ) &&
                           ui_game_config_initialize( e, g, env, doc, p, h );
                    break;
                case 0:
                {
                    ui_params_t params;
                    if ( step->m_has_text )
                        params.add_string( step->m_text );
                    done = ui_config_input( e, env, doc, params, cls, p, h );
                    break;
                }
                case 1:
                    done = ui_config_tick( e, n, env, doc, ui_cn_float( step->m_a ), cls, p, h );
                    break;
                case 2:
                {
                    const float point[ 2 ] = { ui_cn_float( step->m_a ), ui_cn_float( step->m_b ) };
                    ui_config_touch_down( n, inverse, point );
                    break;
                }
                case 3:
                {
                    next( 15 );
                    const float point[ 2 ] = { ui_cn_float( step->m_a ), ui_cn_float( step->m_b ) };
                    ui_config_touch_move( n, inverse, point );
                    break;
                }
                default:
                    ui_config_touch_up( n );
                    break;
                }
                ++steps;
                std::string after;
                vujb_dump( root, after );
                if ( !done )
                    bad( "the step (a path not recreated)", row );
                if ( after != step->m_after )
                    bad( "the root after", row );
                if ( ui_cx_combo_text( e.m_combos ) != step->m_combos )
                    bad( "the combo records", row );
                if ( e.m_focus != step->m_focus )
                    bad( "the focus", row );
                if ( ui_cn_nav_hex( n ) != step->m_nav )
                    bad( "the nav bytes", row );
                if ( ( g.m_trophy ? 1 : 0 ) != step->m_trophy )
                    bad( "the trophy byte", row );
                if ( !hooks_same || at != step->m_logs )
                    bad( "the hook calls", row );
            }
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_config_input / touch / tick match retail's VuBaseConfigEntity navigation (slot 14 0x448A50, touch 0x4481C0 / "
                     "0x4482C0 / 0x4482D0, slot 13 0x448470, focus 0x4496E0, change 0x449830) on %d GameConfig entities from our loaded "
                     "properties: %d runs, %d steps (root, combos, focus, nav bytes, trophy), %d hook calls",
            static_cast< int >( g_golden_cn_entities_count ), runs, steps, calls );
    else
        LOG_ERROR( "the config navigation FAILED %d checks against retail's", failed );
    return failed == 0;
}

// ui_game_config_next / ui_game_config_done (hydro core/ui/select.hxx: slot 26
// 0x45E300 over slot 17 0x45D720 and slot 18 0x45D0C0) against retail's slot
// 26 called directly on every option of each GameConfig* screen's entity
// (oracle case config_next: two roots, six answer seeds): the result, the out
// text and every progress / unlock call in order with its arguments. Force
// Online from OUR loaded entity, compared with retail's authored byte.
inline bool verify_ui_config_next( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    int failed = 0, runs = 0, calls = 0;
    auto bad = [ &failed ]( const char* what, int k )
    {
        if ( ++failed <= 8 )
            LOG_ERROR( "ui config next: %s differs from retail (call %d)", what, k );
    };
    for ( std::size_t k = 0; k < g_golden_nx_entities_count; ++k )
    {
        const golden_nx_entity_t& want = g_golden_nx_entities[ k ];
        const entity_t* have = nullptr;
        for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
            if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, want.m_project ) == 0 && want.m_index >= 0 &&
                 static_cast< std::size_t >( want.m_index ) < loaded.trees[ p ].size( ) )
                have = loaded.trees[ p ][ static_cast< std::size_t >( want.m_index ) ].second;
        if ( !have || have->m_type != "VuGameConfigEntity" || have->m_name != want.m_name )
        {
            bad( "the authored entity", static_cast< int >( k ) );
            continue;
        }
        const ui_front_end_screen_t screen = ui_front_end_screen( *have );
        const std::vector< std::uint8_t > authored = ui_unhex( want.m_authored );
        if ( authored.size( ) < 2 || screen.m_single_player != ( authored[ 0 ] != 0 ) || screen.m_force_online != ( authored[ 1 ] != 0 ) )
            bad( "the authored bytes", static_cast< int >( k ) );
        ui_config_entity_t e;
        e.m_single_player = screen.m_single_player;
        e.m_force_online = screen.m_force_online;
        for ( int c = 0; c < want.m_calls; ++c )
        {
            const golden_nx_call_t& call = g_golden_nx_calls[ want.m_first + c ];
            const int row = want.m_first + c;
            const value_t root = c_reader( ui_unhex( call.m_root ) ).parse( );
            const value_t option = c_reader( ui_unhex( call.m_option ) ).parse( );
            int at = 0;
            bool hooks_same = true;
            auto next = [ & ]( int kind ) -> const golden_bci_log_t*
            {
                if ( at >= call.m_logs || g_golden_nx_logs[ call.m_first_log + at ].m_kind != kind )
                {
                    hooks_same = false;
                    return nullptr;
                }
                ++calls;
                return &g_golden_nx_logs[ call.m_first_log + at++ ];
            };
            ui_profile_t p;
            p.has_progress = std::strcmp( call.m_seed, "noprog" ) != 0;
            p.unlock = [ & ]( int slot, const std::string& a, const std::string& b, const std::string& cc )
            {
                const golden_bci_log_t* g = next( 0 );
                hooks_same = hooks_same && g && g->m_slot == slot && a == g->m_a && b == g->m_b && cc == g->m_c;
                return g && g->m_answer != 0;
            };
            p.race_done = [ & ]( const std::string& mode, const std::string& track, const std::string& course )
            {
                const golden_bci_log_t* g = next( 11 );
                hooks_same = hooks_same && g && mode == g->m_a && track == g->m_b && course == g->m_c;
                return g && g->m_answer != 0;
            };
            p.champ_done = [ & ]( const std::string& champ )
            {
                const golden_bci_log_t* g = next( 10 );
                hooks_same = hooks_same && g && champ == g->m_a;
                return g && g->m_answer != 0;
            };
            p.ghost_done = [ & ]( const std::string& track )
            {
                const golden_bci_log_t* g = next( 16 );
                hooks_same = hooks_same && g && track == g->m_a;
                return g && g->m_answer != 0;
            };
            std::string out;
            const bool result = ui_game_config_next( e, root, option, p, out );
            ++runs;
            if ( result != ( call.m_result != 0 ) )
                bad( "the result", row );
            if ( ui_cx_hex( out ) != call.m_out )
                bad( "the out text", row );
            if ( !hooks_same || at != call.m_logs )
                bad( "the hook calls", row );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_game_config_next matches retail's VuGameConfigEntity slot 26 0x45E300 (slot 17 0x45D720, slot 18 0x45D0C0) on %d "
                     "GameConfig entities: %d calls, the result, the out text, %d progress / unlock calls",
            static_cast< int >( g_golden_nx_entities_count ), runs, calls );
    else
        LOG_ERROR( "the game selector's next choice FAILED %d checks against retail's", failed );
    return failed == 0;
}

// ui_screen_draw (core/ui compose.hxx) against retail's VuUIScreenEntity draw
// slot 13 0x629D60 (oracle case screen_draw) on every Screens/* root under three
// environments (alpha, 0x81A184, the sort's +0x74, the screen's +0x5C): every
// draw reached in order (the screen's own component = child -1, then the
// direct children: component, alpha bits, both stacks' tops and sizes, the sort
// key), then the sizes and the key after. OUR trees decide which entities have
// a UI draw component; the ctor's +0x5C (4) is compared with retail's.
inline bool verify_ui_screen_draw( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    int failed = 0, calls = 0;
    for ( std::size_t r = 0; r < g_golden_scr_runs_count; ++r )
    {
        const golden_scr_run_t& run = g_golden_scr_runs[ r ];
        const golden_scr_env_t& se = g_golden_scr_envs[ run.m_env ];
        const golden_txt_env_t& e = g_golden_txt_envs[ se.m_txt ];
        const entity_t* root = nullptr;
        for ( std::size_t p = 0; p < g_golden_ui_projects_count && !root; ++p )
            if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, run.m_project ) == 0 )
                root = loaded.roots[ p ].get( );
        if ( !root || root->m_type != "VuUIScreenEntity" || root->m_children.size( ) != static_cast< std::size_t >( run.m_children ) ||
             run.m_layer0 != 4u )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui screen draw: %s has no loaded screen root, a different child count or +0x5C", run.m_project );
            continue;
        }
        ui_gfx_sort_t sort;
        sort.m_index = e.m_index;
        sort.m_key_lo = e.m_key_lo;
        sort.m_key_hi = se.m_key_hi;
        ui_gfx_stacks_t stacks;
        stacks.m_matrices.resize( static_cast< std::size_t >( run.m_matrices_before ) );
        stacks.m_floats.resize( static_cast< std::size_t >( run.m_floats_before ) );
        ui_impl_t impl;
        const std::vector< std::uint8_t > crop = ui_unhex( run.m_crop );
        std::memcpy( impl.m_crop.m_matrix, crop.data( ), 64 );
        std::memcpy( &impl.m_float, &se.m_float, 4 );
        float alpha;
        std::memcpy( &alpha, &se.m_alpha, 4 );
        bool same = true;
        int at = 0;
        ui_screen_draw( sort, stacks, impl, *root, se.m_layer, alpha, [ & ]( const entity_t& child, int component, float a )
        {
            if ( at >= run.m_calls )
            {
                same = false;
                return;
            }
            const golden_hud_call_t& want = g_golden_scr_calls[ run.m_first + at++ ];
            int index = -2;
            if ( &child == root )
                index = -1;
            for ( std::size_t k = 0; k < root->m_children.size( ); ++k )
                if ( root->m_children[ k ].get( ) == &child )
                    index = static_cast< int >( k );
            std::uint32_t bits, top;
            std::memcpy( &bits, &a, 4 );
            std::memcpy( &top, &stacks.m_floats.back( ), 4 );
            const std::vector< std::uint8_t > matrix = ui_unhex( want.m_matrix );
            same = same && index == want.m_child && component == want.m_component && bits == want.m_alpha &&
                   stacks.m_matrices.size( ) == static_cast< std::size_t >( want.m_matrices ) &&
                   std::memcmp( stacks.m_matrices.back( ).data( ), matrix.data( ), 64 ) == 0 &&
                   stacks.m_floats.size( ) == static_cast< std::size_t >( want.m_floats ) && top == want.m_float &&
                   sort.m_key_lo == want.m_key[ 0 ] && sort.m_key_hi == want.m_key[ 1 ];
            ++calls;
        } );
        same = same && at == run.m_calls && stacks.m_matrices.size( ) == static_cast< std::size_t >( run.m_matrices_after ) &&
               stacks.m_floats.size( ) == static_cast< std::size_t >( run.m_floats_after ) && sort.m_key_lo == run.m_key[ 0 ] && sort.m_key_hi == run.m_key[ 1 ];
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui screen draw: %s env %d reached %d draws, retail %d - or a call / stack / key differs", run.m_project, run.m_env, at, run.m_calls );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_screen_draw matches retail's VuUIScreenEntity draw slot 13 0x629D60 (0x50B4B0, 0x50B530, 0x628070) on %d runs over the "
                     "loaded Screens/* roots: %d draws - entity, component, alpha, both stacks' tops and sizes, the sort key - and the state after",
            static_cast< int >( g_golden_scr_runs_count ), calls );
    else
        LOG_ERROR( "the screen draw FAILED %d checks against retail's", failed );
    return failed == 0;
}

// ui_screen_frame (core/ui frame.hxx, THE EXPORT) on every Screens/* root
// under screen_draw's first environment: its depth-0 leaves must be exactly
// retail's screen-level draws minus the containers (which the export walks
// into), in order, with the same component, alpha bits and sort key. The
// containers' own walks are verify_ui_container_draw's.
inline bool verify_ui_screen_frame( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    int failed = 0, screens = 0, leaves = 0, texts = 0, unrecreated = 0, containers = 0;
    for ( std::size_t r = 0; r < g_golden_scr_runs_count; ++r )
    {
        const golden_scr_run_t& run = g_golden_scr_runs[ r ];
        if ( run.m_env != 0 )
            continue;
        const golden_scr_env_t& se = g_golden_scr_envs[ 0 ];
        const golden_txt_env_t& e = g_golden_txt_envs[ se.m_txt ];
        const entity_t* root = nullptr;
        for ( std::size_t p = 0; p < g_golden_ui_projects_count && !root; ++p )
            if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, run.m_project ) == 0 )
                root = loaded.roots[ p ].get( );
        if ( !root )
        {
            ++failed;
            continue;
        }
        ui_gfx_sort_t sort;
        sort.m_index = e.m_index;
        sort.m_key_lo = e.m_key_lo;
        sort.m_key_hi = se.m_key_hi;
        ui_impl_t impl;
        const std::vector< std::uint8_t > crop = ui_unhex( run.m_crop );
        std::memcpy( impl.m_crop.m_matrix, crop.data( ), 64 );
        std::memcpy( &impl.m_float, &se.m_float, 4 );
        float alpha;
        std::memcpy( &alpha, &se.m_alpha, 4 );
        const ui_frame_t frame = ui_screen_frame( *root, impl, sort, alpha, se.m_layer );
        std::vector< const ui_frame_draw_t* > top;
        for ( const ui_frame_draw_t& d : frame.m_draws )
        {
            if ( d.m_depth == 0 )
                top.push_back( &d );
            texts += d.m_kind == ui_frame_kind_t::text ? 1 : 0;        }
        std::size_t at = 0;
        bool same = true;
        for ( int c = 0; c < run.m_calls; ++c )
        {
            const golden_hud_call_t& want = g_golden_scr_calls[ run.m_first + c ];
            if ( want.m_child >= 0 && root->m_children[ static_cast< std::size_t >( want.m_child ) ]->m_type == "VuUIContainerEntity" )
                continue;
            if ( at >= top.size( ) )
            {
                same = false;
                break;
            }
            const ui_frame_draw_t& d = *top[ at++ ];
            const entity_t* who = want.m_child < 0 ? root : root->m_children[ static_cast< std::size_t >( want.m_child ) ].get( );
            std::uint32_t bits, ftop;
            std::memcpy( &bits, &d.m_alpha, 4 );
            std::memcpy( &ftop, &d.m_top, 4 );
            const std::vector< std::uint8_t > matrix = ui_unhex( want.m_matrix );
            same = same && d.m_entity == who && d.m_component == want.m_component && bits == want.m_alpha && ftop == want.m_float &&
                   std::memcmp( d.m_matrix.data( ), matrix.data( ), 64 ) == 0 && d.m_key_lo == want.m_key[ 0 ] && d.m_key_hi == want.m_key[ 1 ];
        }
        same = same && at == top.size( );
        ++screens;
        leaves += static_cast< int >( frame.m_draws.size( ) );
        unrecreated += frame.m_unrecreated;
        containers += frame.m_containers;
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui screen frame: %s's screen-level leaves differ from retail's draws", run.m_project );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_screen_frame (the export) reproduces retail's screen-level draws on %d Screens/* roots: %d leaf draws through %d "
                     "container walks - %d text (ui_text_draw), %d whose draw is NOT recreated (images, buttons, other types)",
            screens, leaves, containers, texts, unrecreated );
    else
        LOG_ERROR( "the screen frame export FAILED %d checks against retail's", failed );
    return failed == 0;
}

// ui_image_component_draw / ui_image_draw (core/ui image.hxx) against retail's
// VuUIImageEntity slot 15 0x4401C0 -> slot 13 0x629300 (oracle case image_draw)
// on every entity whose slot 13 is 0x629300 in the loaded projects, then a
// sweep: each new command's words and record byte for byte, in ONE sort per
// buffer through all vectors. For the real entities the fields are OUR loaded
// properties (compared with retail's bytes first); +0xB4 is retail's.
inline bool verify_ui_image_draw( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    ui_gfx_sort_t sort;   // TxtWorld's buffers: 0x300000 / 0x100000 bytes, buffer 1 with 0x35 used
    sort.m_buffers[ 0 ].m_capacity = 0x300000;
    sort.m_buffers[ 1 ].m_used = 0x35;
    sort.m_buffers[ 1 ].m_capacity = 0x100000;
    for ( auto& b : sort.m_buffers )
        b.m_memory.resize( static_cast< std::size_t >( b.m_capacity ) );
    int failed = 0, commands = 0, fields = 0, real = 0;
    for ( std::size_t k = 0; k < g_golden_img_vectors_count; ++k )
    {
        const golden_img_vector_t& v = g_golden_img_vectors[ k ];
        const golden_txt_env_t& e = g_golden_txt_envs[ v.m_env ];
        ui_crop_t crop;
        std::memcpy( &crop.m_matrix[ 0 ], &e.m_crop[ 0 ], 4 );
        std::memcpy( &crop.m_matrix[ 5 ], &e.m_crop[ 1 ], 4 );
        std::memcpy( &crop.m_matrix[ 12 ], &e.m_crop[ 2 ], 4 );
        std::memcpy( &crop.m_matrix[ 13 ], &e.m_crop[ 3 ], 4 );
        ui_font_draw_env_t env;
        const std::vector< std::uint8_t > matrix = ui_unhex( v.m_matrix );
        std::memcpy( env.m_matrix, matrix.data( ), 64 );
        env.m_texture_material = g_golden_img_materials[ 0 ][ 0 ];
        env.m_texture_material_key = g_golden_img_materials[ 0 ][ 1 ];
        env.m_rect_material = g_golden_img_materials[ 1 ][ 0 ];
        env.m_rect_material_key = g_golden_img_materials[ 1 ][ 1 ];
        const std::vector< std::uint8_t > b = ui_unhex( v.m_fields );
        const auto at = [ &b ]( int offset ) { return b.data( ) + ( offset - 0x6C ); };
        ui_image_t im;
        im.m_visible = *at( 0x6C );
        std::memcpy( &im.m_colour, at( 0x6D ), 4 );
        std::memcpy( im.m_screen_scale, at( 0x74 ), 8 );
        std::memcpy( im.m_dst, at( 0x7C ), 16 );
        std::memcpy( im.m_src, at( 0x8C ), 16 );
        std::memcpy( &im.m_rotation, at( 0x9C ), 4 );
        im.m_flip_x = *at( 0xA0 );
        im.m_flip_y = *at( 0xA1 );
        im.m_absolute = *at( 0xA2 );
        std::memcpy( &im.m_anchor.m_horizontal, at( 0xA4 ), 4 );
        std::memcpy( &im.m_anchor.m_vertical, at( 0xA8 ), 4 );
        std::memcpy( &im.m_anchor.m_h_ratio, at( 0xAC ), 4 );
        std::memcpy( &im.m_anchor.m_v_ratio, at( 0xB0 ), 4 );
        std::memcpy( &im.m_alpha, at( 0xB4 ), 4 );
        std::memcpy( &im.m_depth, &v.m_depth, 4 );
        if ( v.m_sweep == 0 )
        {
            const entity_t* have = nullptr;
            for ( std::size_t p = 0; p < g_golden_ui_projects_count && !have; ++p )
                if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, v.m_project ) == 0 && v.m_index < static_cast< int >( loaded.trees[ p ].size( ) ) )
                    have = loaded.trees[ p ][ static_cast< std::size_t >( v.m_index ) ].second;
            const ui_state_t* st = have && ui_is_state( *have ) ? static_cast< const ui_state_t* >( have->m_state.get( ) ) : nullptr;
            const ui_state_t* draw = nullptr;
            for ( const entity_component_t& c : have ? have->m_components : std::vector< entity_component_t >( ) )
                if ( c.m_vtable == k_ui_draw_component_vtable && c.m_state )
                    draw = static_cast< const ui_state_t* >( c.m_state.get( ) );
            bool same = st && draw && have->m_ctor && have->m_ctor->m_vtable == v.m_vtable;
            const auto take = [ & ]( const char* name, void* target, int bytes, const ui_state_t* from )
            {
                const ui_slot_t* slot = from ? ui_slot_named( *from, name ) : nullptr;
                if ( !slot )
                {
                    same = false;
                    return;
                }
                if ( std::memcmp( target, slot->m_bits, static_cast< std::size_t >( bytes ) ) != 0 )
                    same = false;
                std::memcpy( target, slot->m_bits, static_cast< std::size_t >( bytes ) );
                ++fields;
            };
            if ( same )
            {
                take( "Visible", &im.m_visible, 1, st );
                take( "Color", &im.m_colour, 4, st );
                take( "Screen Scale", im.m_screen_scale, 8, st );
                take( "DstX", &im.m_dst[ 0 ], 4, st );
                take( "DstY", &im.m_dst[ 1 ], 4, st );
                take( "DstWidth", &im.m_dst[ 2 ], 4, st );
                take( "DstHeight", &im.m_dst[ 3 ], 4, st );
                take( "SrcX", &im.m_src[ 0 ], 4, st );
                take( "SrcY", &im.m_src[ 1 ], 4, st );
                take( "SrcWidth", &im.m_src[ 2 ], 4, st );
                take( "SrcHeight", &im.m_src[ 3 ], 4, st );
                take( "Rotation", &im.m_rotation, 4, st );
                take( "FlipX", &im.m_flip_x, 1, st );
                take( "FlipY", &im.m_flip_y, 1, st );
                take( "Absolute Coords", &im.m_absolute, 1, st );
                take( "Horizontal Anchor", &im.m_anchor.m_horizontal, 4, st );
                take( "Horizontal Ratio", &im.m_anchor.m_h_ratio, 4, st );
                take( "Vertical Anchor", &im.m_anchor.m_vertical, 4, st );
                take( "Vertical Ratio", &im.m_anchor.m_v_ratio, 4, st );
                take( "Depth", &im.m_depth, 4, draw );
            }
            if ( !same && ++failed <= 8 )
                LOG_ERROR( "ui image draw: %s entity %d: our loaded fields differ from retail's", v.m_project, v.m_index );
            ++real;
        }
        const golden_img_texture_t& gt = g_golden_img_textures[ v.m_texture ];
        const ui_texture_t texture = { gt.m_handle, gt.m_width, gt.m_height };
        float alpha, aspect;
        std::memcpy( &alpha, &v.m_alpha, 4 );
        std::memcpy( &aspect, &v.m_aspect, 4 );
        sort.m_index = e.m_index;
        sort.m_key_lo = e.m_key_lo;
        sort.m_key_hi = e.m_key_hi;
        std::vector< ui_gfx_command_t >& list = sort.m_commands[ e.m_index ];
        const std::size_t before = list.size( );
        ui_image_component_draw( sort, env, crop, aspect, im, gt.m_handle ? &texture : nullptr, alpha );
        bool same = list.size( ) - before == static_cast< std::size_t >( v.m_commands ) && v.m_stack[ 0 ] == v.m_stack[ 2 ] && v.m_stack[ 1 ] == v.m_stack[ 3 ];
        for ( int c = 0; same && c < v.m_commands; ++c )
        {
            const golden_txt_command_t& want = g_golden_img_commands[ v.m_first + c ];
            const ui_gfx_command_t& have = list[ before + static_cast< std::size_t >( c ) ];
            const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
            const auto& memory = sort.m_buffers[ e.m_index ].m_memory;
            same = std::memcmp( have.m_words, want.m_words, sizeof( have.m_words ) ) == 0 && have.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                   std::memcmp( memory.data( ) + have.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
            ++commands;
        }
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui image draw: vector %d (sweep %d, %s %d) gave %d commands, retail %d - or a word / record byte differs", static_cast< int >( k ),
                v.m_sweep, v.m_project, v.m_index, static_cast< int >( list.size( ) - before ), v.m_commands );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_image_draw matches retail's VuUIImageEntity draw (slot 15 0x4401C0 -> slot 13 0x629300: anchor 0x6282D0, 0x410AB0 / "
                     "0x407DB0 / 0x40B5F0 / 0x4078C0, 0x50B4B0, texture draw 0x507360 / 0x507830) on %d vectors (%d on real images, %d of their "
                     "loaded fields): %d commands and records byte for byte",
            static_cast< int >( g_golden_img_vectors_count ), real, fields, commands );
    else
        LOG_ERROR( "the UI image draw FAILED %d checks against retail's", failed );
    return failed == 0;
}

inline bool verify_ui_menu_all( )
{
    bool ok = true;
    ok = verify_ui_image_draw( ) && ok;
    ok = verify_ui_screen_frame( ) && ok;
    ok = verify_ui_screen_draw( ) && ok;
    ok = verify_ui_config_next( ) && ok;
    ok = verify_ui_config_nav( ) && ok;
    return ok;
}
