#pragma once

#include <workspace/core/ui/combo.hxx>
#include <workspace/core/ui/difficulty.hxx>
#include <workspace/core/ui/frame.hxx>
#include <workspace/core/ui/leaves.hxx>
#include <workspace/core/ui/ticker.hxx>

#include <cstring>
#include <string>

// The UI area's leaf checks (batch 12; included by checks/menu.hxx before
// verify_ui_menu_all, which runs verify_ui_leaves_all): the Screens roots' leaf
// draws and their runtime state. Everything ui.hxx / menu.hxx define above is usable.

inline std::uint32_t ui_lv_bits( float f )
{
    std::uint32_t b;
    std::memcpy( &b, &f, 4 );
    return b;
}

inline float ui_lv_float( std::uint32_t b )
{
    float f;
    std::memcpy( &f, &b, 4 );
    return f;
}

// A sort laid out as TxtWorld's (buffer 0: 0x300000 bytes; buffer 1: 0x100000, 0x35 used).
inline void ui_lv_sort( ui_gfx_sort_t& sort )
{
    sort.m_buffers[ 0 ].m_capacity = 0x300000;
    sort.m_buffers[ 1 ].m_used = 0x35;
    sort.m_buffers[ 1 ].m_capacity = 0x100000;
    for ( auto& b : sort.m_buffers )
        b.m_memory.resize( static_cast< std::size_t >( b.m_capacity ) );
}

// A command's record size by its callback (texture 104, rect 84, font 121 + the text).
inline std::size_t ui_lv_record_size( const ui_gfx_sort_t& sort, int buffer, const ui_gfx_command_t& c )
{
    if ( c.m_words[ 2 ] == 0x507430u )
        return 104;
    if ( c.m_words[ 2 ] == 0x5078E0u || c.m_words[ 2 ] == 0x507AB0u )
        return 84;
    const auto& m = sort.m_buffers[ buffer ].m_memory;
    std::size_t n = 0;
    while ( c.m_words[ 3 ] + 120 + n < m.size( ) && m[ c.m_words[ 3 ] + 120 + n ] )
        ++n;
    return 121 + n;
}

inline const entity_t* ui_lv_entity( const char* project, int index )
{
    const ui_loaded_t& loaded = ui_loaded( );
    for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
        if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, project ) == 0 && index < static_cast< int >( loaded.trees[ p ].size( ) ) )
            return loaded.trees[ p ][ static_cast< std::size_t >( index ) ].second;
    return nullptr;
}

// verify_ui_info_tick: retail's OnSetInfoStringID 0x446480 / OnUITick 0x446580 /
// OnClearInfoStringID 0x446450 / slot 12 0x446860 through one scripted sequence on
// every real Info entity (+ a raw delay / speed sweep on the first). The ticker
// starts from OUR loaded entity (InfoScrollDelay / Speed compared with retail's
// bits; Screen Scale and Width are OUR loaded ones); after each step the state
// (delay, speed, extent, gap, time, x, y bits), the id and the text, and the hook
// calls in order (the string DB ids; the measure's text, font and xmm3 bits).
inline bool verify_ui_info_tick( )
{
    int failed = 0, steps = 0, calls = 0;
    for ( std::size_t r = 0; r < g_golden_itk_runs_count; ++r )
    {
        const golden_itk_run_t& run = g_golden_itk_runs[ r ];
        const entity_t* e = ui_lv_entity( run.m_project, run.m_index );
        if ( !e || e->m_type != "VuInfoUITextEntity" )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui info tick: %s entity %d is not OUR loaded VuInfoUITextEntity", run.m_project, run.m_index );
            continue;
        }
        ui_info_ticker_t t = ui_info_ticker_of( *e );
        if ( run.m_raw )
        {
            t.m_delay = ui_lv_float( run.m_raw_bits[ 0 ] );
            t.m_speed = ui_lv_float( run.m_raw_bits[ 1 ] );
        }
        float scale[ 2 ] = { 0.0f, 0.0f };
        float width = 0.0f;
        bool ok = ui_frame_take( *e, "Screen Scale", scale, 8 ) && ui_frame_take( *e, "Width", &width, 4 );
        const auto state = [ & ]( std::uint32_t out[ 7 ] )
        {
            const float f[ 7 ] = { t.m_delay, t.m_speed, t.m_scroll.m_extent, t.m_scroll.m_gap, t.m_time, t.m_scroll.m_x, t.m_scroll.m_y };
            for ( int k = 0; k < 7; ++k )
                out[ k ] = ui_lv_bits( f[ k ] );
        };
        std::uint32_t have[ 7 ];
        state( have );
        if ( !ok || std::memcmp( have, run.m_initial, sizeof( have ) ) != 0 )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui info tick: run %d (%s %d): OUR loaded delay / speed / fields differ from retail's", static_cast< int >( r ), run.m_project,
                    run.m_index );
            continue;
        }
        float answer = 0.0f;
        std::vector< golden_itk_log_t > log;
        std::vector< std::string > texts;
        ui_info_hooks_t hooks;
        hooks.m_string = [ & ]( const std::string& id )
        {
            texts.push_back( id );
            log.push_back( { 0, nullptr, 0u, 0u, 0 } );
            return id + "|text";   // the case's VuStringDB answer
        };
        hooks.m_measure = [ & ]( std::uint32_t font, const std::string& text, float aspect )
        {
            texts.push_back( text );
            log.push_back( { 1, nullptr, font, ui_lv_bits( aspect ), 0xA0 } );   // the params pointer is +0xA0 by construction
            return answer;
        };
        for ( int s = 0; s < run.m_steps; ++s )
        {
            const golden_itk_step_t& st = g_golden_itk_steps[ run.m_first + s ];
            log.clear( );
            texts.clear( );
            int returned = -1;
            if ( st.m_op == 0 )
                ui_info_tick( t, width, ui_lv_float( st.m_arg ) );
            else if ( st.m_op == 1 )
            {
                answer = ui_lv_float( st.m_arg );
                ui_info_set( t, st.m_id, scale, width, st.m_font_none ? 0u : run.m_font, hooks );
            }
            else if ( st.m_op == 2 )
                ui_info_clear( t );
            else
            {
                ui_info_text( t, hooks );
                returned = 0x140;
            }
            state( have );
            bool same = std::memcmp( have, st.m_after, sizeof( have ) ) == 0 && t.m_id == st.m_after_id && t.m_text == st.m_after_text &&
                        returned == st.m_returned && static_cast< int >( log.size( ) ) == st.m_logs;
            for ( int c = 0; same && c < st.m_logs; ++c )
            {
                const golden_itk_log_t& want = g_golden_itk_logs[ st.m_first_log + c ];
                const golden_itk_log_t& got = log[ static_cast< std::size_t >( c ) ];
                same = got.m_measure == want.m_measure && texts[ static_cast< std::size_t >( c ) ] == want.m_text && got.m_font == want.m_font &&
                       got.m_aspect == want.m_aspect && got.m_params == want.m_params;
                ++calls;
            }
            ++steps;
            if ( !same && ++failed <= 8 )
                LOG_ERROR( "ui info tick: run %d (%s %d) step %d (op %d): state / id / text / hook calls differ from retail's", static_cast< int >( r ),
                    run.m_project, run.m_index, s, st.m_op );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_info_set / ui_info_tick / ui_info_clear / ui_info_text match retail's VuInfoUITextEntity OnSetInfoStringID 0x446480, "
                     "OnUITick 0x446580, OnClearInfoStringID 0x446450 and slot 12 0x446860 on %d runs (OUR loaded entities): %d steps, %d hook calls",
            static_cast< int >( g_golden_itk_runs_count ), steps, calls );
    else
        LOG_ERROR( "the UI info ticker FAILED %d checks against retail's", failed );
    return failed == 0;
}

// verify_ui_combo_draw: retail's combo box draw 0x4578D0 through VuBaseConfigEntity
// slot 16 0x448F40 -> 0x448F50 (every GameConfig / BoatConfig selector after
// retail's slot 4) and 0x475B10 (every settings combo entity) x the variants.
// The style block, the scale / position / spacing / Depth and the two fonts'
// params 0..40 from OUR loaded entity (each compared with retail's bytes first;
// params 41..59 are retail's indeterminate stack - UNKNOWN - taken from its
// bytes); the option vector / records (built by slot 4, verified elsewhere) are
// retail's. Commands and records byte for byte, one sort per buffer, and the
// VuStringDB ids in call order.
inline bool verify_ui_combo_draw( )
{
    ui_gfx_sort_t sort;
    sort.m_buffers[ 0 ].m_capacity = 0x300000;
    sort.m_buffers[ 1 ].m_used = 0x35;
    sort.m_buffers[ 1 ].m_capacity = 0x100000;
    for ( auto& b : sort.m_buffers )
        b.m_memory.resize( static_cast< std::size_t >( b.m_capacity ) );
    constexpr std::uint32_t k_material[ 2 ] = { 0x5D001000u, 0x5D001800u };   // TxtWorld's font materials
    int failed = 0, commands = 0, fields = 0, rows = 0, strings = 0, layouts = 0;
    for ( std::size_t k = 0; k < g_golden_cbd_runs_count; ++k )
    {
        const golden_cbd_run_t& run = g_golden_cbd_runs[ k ];
        const golden_txt_env_t& ge = g_golden_txt_envs[ run.m_env ];
        const entity_t* e = ui_lv_entity( run.m_project, run.m_index );
        const std::vector< std::uint8_t > block = ui_unhex( run.m_block );
        bool same = e && e->m_ctor && e->m_ctor->m_vtable == run.m_vtable && block.size( ) == 0x1E0;
        ui_combo_block_t b;
        float scale[ 2 ] = { }, position[ 2 ] = { }, spacing = 0.0f;
        float depth = 0.0f;
        if ( same )
        {
            b = ui_combo_block_of( *e );
            const auto against = [ & ]( const void* ours, int offset, std::size_t bytes )
            {
                if ( std::memcmp( ours, block.data( ) + offset, bytes ) != 0 )
                    same = false;
                ++fields;
            };
            against( b.m_label_rect, 0x08, 16 );
            against( &b.m_label_selected, 0x7C, 4 );
            against( b.m_value_offset, 0x98, 8 );
            against( b.m_value_rect, 0xA0, 16 );
            against( &b.m_value_selected, 0x114, 12 );
            against( b.m_left, 0x1B4, 8 );
            against( b.m_right, 0x1BC, 8 );
            for ( int f = 0; f < 2; ++f )
            {
                std::uint8_t* params = f ? b.m_value_params : b.m_label_params;
                const int at = f ? 0xD0 : 0x38;
                std::memcpy( params, block.data( ) + at, 60 );
                const ui_slot_t* name = ui_property_value( *e, f ? "ValueFont" : "HeadingFont" );
                std::string asset;
                if ( !name )
                    same = false;
                else
                    ui_font_params( ui_font_db( 0 ), name->m_text, params, asset );
                against( params, at, 41 );
            }
            same = ui_frame_take( *e, "Screen Scale", scale, 8 ) && ui_frame_take( *e, "Position X", &position[ 0 ], 4 ) &&
                   ui_frame_take( *e, "Position Y", &position[ 1 ], 4 ) && same;
            if ( !run.m_box )
                same = ui_frame_take( *e, "Spacing", &spacing, 4 ) && same;
            depth = ui_frame_depth( *e );
            if ( ui_lv_bits( depth ) != run.m_depth )
                same = false;
        }
        if ( !same )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui combo draw: run %d (%s %d): OUR loaded entity / block fields differ from retail's", static_cast< int >( k ), run.m_project,
                    run.m_index );
            continue;
        }
        // The records and the option vector (retail's).
        ui_config_entity_t ce;
        for ( int r = 0; r < run.m_records; ++r )
        {
            const golden_cbd_record_t& gr = g_golden_cbd_records[ run.m_first_record + r ];
            const std::vector< std::uint8_t > head = ui_unhex( gr.m_head );
            ui_config_combo_t c;
            c.m_focused = head[ 0 ];
            c.m_b41 = head[ 1 ];
            c.m_b42 = head[ 2 ];
            std::memcpy( &c.m_index, head.data( ) + 4, 4 );
            std::memcpy( &c.m_shown, head.data( ) + 8, 4 );
            c.m_string_id = gr.m_id;
            for ( int j = 0; j < gr.m_choices; ++j )
            {
                c.m_choice_ids.push_back( g_golden_cbd_choices[ gr.m_first + j ].m_id );
                c.m_locked.push_back( static_cast< std::uint8_t >( g_golden_cbd_choices[ gr.m_first + j ].m_locked ) );
            }
            ce.m_combos.push_back( c );
        }
        std::vector< value_t > options( static_cast< std::size_t >( run.m_ids ) );
        for ( int i = 0; i < run.m_ids; ++i )
        {
            value_t id;
            id.kind = value_t::kind_t::integer;
            id.tag = 1;
            id.integer = g_golden_cbd_ids[ run.m_first_id + i ];
            options[ static_cast< std::size_t >( i ) ].kind = value_t::kind_t::object;
            options[ static_cast< std::size_t >( i ) ].tag = 6;
            options[ static_cast< std::size_t >( i ) ].object.push_back( { "ComboBoxID", id } );
        }
        for ( const value_t& o : options )
            ce.m_options.push_back( &o );
        // The environment.
        ui_font_draw_env_t env;
        const std::vector< std::uint8_t > matrix = ui_unhex( run.m_matrix );
        std::memcpy( env.m_matrix, matrix.data( ), 64 );
        std::memcpy( &env.m_top, &ge.m_top, 4 );
        for ( int m = 0; m < 2; ++m )
        {
            env.m_material[ m ] = k_material[ m ];
            env.m_material_key[ m ] = g_golden_txt_material_keys[ m ];
        }
        env.m_texture_material = g_golden_cbd_material[ 0 ];
        env.m_texture_material_key = g_golden_cbd_material[ 1 ];
        ui_texture_t textures[ 7 ];
        for ( int t = 0; t < 7; ++t )
            if ( run.m_textures[ t ] >= 0 )
            {
                const golden_img_texture_t& g = g_golden_cbd_textures[ run.m_textures[ t ] ];
                textures[ t ] = { g.m_handle, g.m_width, g.m_height };
            }
        std::vector< std::string > asked;
        ui_combo_inputs_t in;
        in.m_label_font = run.m_label_font ? g_golden_cbd_font : 0u;
        in.m_value_font = run.m_value_font ? g_golden_cbd_font : 0u;
        in.m_string = [ & ]( const std::string& id )
        {
            asked.push_back( id );
            return id + "~";   // the case's VuStringDB answer
        };
        in.m_texture = [ & ]( int which ) -> const ui_texture_t* { return run.m_textures[ which ] >= 0 ? &textures[ which ] : nullptr; };
        sort.m_index = ge.m_index;
        sort.m_key_lo = ge.m_key_lo;
        sort.m_key_hi = ge.m_key_hi;
        std::vector< ui_gfx_command_t >& list = sort.m_commands[ ge.m_index ];
        const std::size_t before = list.size( );
        bool known;
        env.m_rect_material = g_golden_cbd_rect_material[ 0 ];
        env.m_rect_material_key = g_golden_cbd_rect_material[ 1 ];
        if ( run.m_box )
        {
            const float d = depth * 0.005f + 0.5f;   // 0x475B10: 0x6F75C4, 0x6F765C
            known = !ce.m_combos.empty( ) && ui_combo_draw( sort, env, b, ce.m_combos[ 0 ], position, scale, d, in );
        }
        else if ( run.m_layout >= 0 )
        {
            // slot 15 0x448B90: TouchLeft / TouchRight from OUR loaded properties, compared with retail's +0x278..
            float touch[ 8 ] = { };
            const char* const names[ 8 ] = { "TouchLeft X", "TouchLeft Y", "TouchLeft Width", "TouchLeft Height",
                                             "TouchRight X", "TouchRight Y", "TouchRight Width", "TouchRight Height" };
            bool loaded = true;
            for ( int t = 0; t < 8; ++t )
                loaded = ui_frame_take( *e, names[ t ], &touch[ t ], 4 ) && loaded;
            const std::vector< std::uint8_t > want = ui_unhex( run.m_touch );
            loaded = loaded && want.size( ) == 32 && std::memcmp( touch, want.data( ), 32 ) == 0;
            fields += 8;
            known = loaded && ui_config_layout_draw( sort, env, ce, b, scale, position, spacing, depth, touch, touch + 4, in, run.m_layout != 0 );
            ++layouts;
        }
        else
            known = ui_config_draw( sort, env, ce, b, scale, position, spacing, depth, in );
        rows += run.m_box ? 1 : run.m_ids;
        same = known && list.size( ) - before == static_cast< std::size_t >( run.m_commands ) && static_cast< int >( asked.size( ) ) == run.m_strings;
        for ( int s = 0; same && s < run.m_strings; ++s )
        {
            same = asked[ static_cast< std::size_t >( s ) ] == g_golden_cbd_strings[ run.m_first_string + s ];
            ++strings;
        }
        for ( int c = 0; same && c < run.m_commands; ++c )
        {
            const golden_txt_command_t& want = g_golden_cbd_commands[ run.m_first + c ];
            const ui_gfx_command_t& have = list[ before + static_cast< std::size_t >( c ) ];
            const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
            const auto& memory = sort.m_buffers[ ge.m_index ].m_memory;
            same = std::memcmp( have.m_words, want.m_words, sizeof( have.m_words ) ) == 0 && have.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                   std::memcmp( memory.data( ) + have.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
            ++commands;
        }
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui combo draw: run %d (%s %d variant %d) gave %d commands (retail %d), %d string ids (retail %d) - or a word / record byte differs",
                static_cast< int >( k ), run.m_project, run.m_index, run.m_variant, static_cast< int >( list.size( ) - before ), run.m_commands,
                static_cast< int >( asked.size( ) ), run.m_strings );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_config_draw / ui_combo_draw / ui_config_layout_draw match retail's VuBaseConfigEntity slot 16 0x448F40 -> 0x448F50, slot "
                     "15 0x448B90 (%d layout runs) and 0x475B10 -> the combo box draw 0x4578D0 (0x457E00, 0x457EB0) on %d runs (%d rows; %d of OUR "
                     "loaded fields): %d commands and records byte for byte, %d string ids",
            layouts, static_cast< int >( g_golden_cbd_runs_count ), rows, fields, commands, strings );
    else
        LOG_ERROR( "the UI combo box draw FAILED %d checks against retail's", failed );
    return failed == 0;
}

// verify_ui_stat_bar: retail's VuBoatStatEntity "Set" 0x44FCA0 + draw 0x44FB60 and
// VuLoadingProgressBarEntity slot 6 0x463220 / "OnLoadProgress" 0x463240 / "Show"
// 0x462C40 / "Hide" 0x462C60 + draw 0x4632E0 through one script each on every
// such entity of the Screens projects. The state starts from the ctor's (+0xB0 =
// 5; +0x84 = 0, +0x88 = 1) and the fields from OUR loaded entity (compared with
// retail's bytes); after each step the state bits and the commands byte for byte.
inline bool verify_ui_stat_bar( )
{
    ui_gfx_sort_t sort;
    sort.m_buffers[ 0 ].m_capacity = 0x300000;
    sort.m_buffers[ 1 ].m_used = 0x35;
    sort.m_buffers[ 1 ].m_capacity = 0x100000;
    for ( auto& b : sort.m_buffers )
        b.m_memory.resize( static_cast< std::size_t >( b.m_capacity ) );
    int failed = 0, steps = 0, commands = 0, fields = 0;
    for ( std::size_t k = 0; k < g_golden_sb_runs_count; ++k )
    {
        const golden_sb_run_t& run = g_golden_sb_runs[ k ];
        const entity_t* e = ui_lv_entity( run.m_project, run.m_index );
        const std::vector< std::uint8_t > raw = ui_unhex( run.m_fields );
        const auto at = [ &raw ]( int offset ) { return raw.data( ) + ( offset - 0x60 ); };
        bool same = e && e->m_type == ( run.m_bar ? "VuLoadingProgressBarEntity" : "VuBoatStatEntity" ) && raw.size( ) == 0x58;
        const auto take = [ & ]( const char* name, void* out, int offset, std::size_t bytes )
        {
            if ( !same || !ui_frame_take( *e, name, out, bytes ) || std::memcmp( out, at( offset ), bytes ) != 0 )
                same = false;
            ++fields;
        };
        ui_boat_stat_fields_t sf;
        ui_progress_bar_fields_t bf;
        if ( run.m_bar )
        {
            take( "Screen Scale", bf.m_screen_scale, 0x64, 8 );
            take( "Rect", bf.m_rect, 0x6C, 16 );
            take( "Color", &bf.m_colour, 0x7C, 4 );
            take( "Initially Visible", &bf.m_initially_visible, 0x80, 1 );
            bf.m_depth = same ? ui_frame_depth( *e ) : 0.0f;
            same = same && ui_lv_bits( bf.m_depth ) == run.m_depth;
        }
        else
        {
            take( "Screen Scale", sf.m_screen_scale, 0x64, 8 );
            take( "Position X", &sf.m_position[ 0 ], 0x6C, 4 );
            take( "Position Y", &sf.m_position[ 1 ], 0x70, 4 );
            take( "Spacing", &sf.m_spacing, 0x74, 4 );
            sf.m_depth = same ? ui_frame_depth( *e ) : 0.0f;
            same = same && ui_lv_bits( sf.m_depth ) == run.m_depth;
        }
        ui_boat_stat_t stat;
        ui_progress_bar_t bar;
        const auto state = [ & ]( std::uint32_t out[ 2 ] )
        {
            if ( run.m_bar )
            {
                out[ 0 ] = ui_lv_bits( bar.m_progress );
                out[ 1 ] = bar.m_visible;
            }
            else
            {
                out[ 0 ] = static_cast< std::uint32_t >( stat.m_value );
                out[ 1 ] = 0;
            }
        };
        std::uint32_t have[ 2 ];
        state( have );
        if ( !same || have[ 0 ] != run.m_initial[ 0 ] || have[ 1 ] != run.m_initial[ 1 ] )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui stat / bar: run %d (%s %d): OUR loaded fields or the ctor state differ from retail's", static_cast< int >( k ), run.m_project,
                    run.m_index );
            continue;
        }
        for ( int s = 0; s < run.m_steps; ++s )
        {
            const golden_sb_step_t& st = g_golden_sb_steps[ run.m_first + s ];
            ui_params_t params;
            if ( st.m_type == 0 || st.m_type == 2 )
            {
                const std::uint32_t type = static_cast< std::uint32_t >( st.m_type );
                std::memcpy( params.m_bytes, &type, 4 );
                std::memcpy( params.m_bytes + 4, &st.m_value, 4 );
                params.m_size = 8;
            }
            else if ( st.m_type == 4 )
                params.add_string( "x" );   // the case's only string record
            std::size_t before = 0;
            const golden_txt_env_t& ge = g_golden_txt_envs[ st.m_op == 0 ? st.m_env : 0 ];
            std::vector< ui_gfx_command_t >& list = sort.m_commands[ ge.m_index ];
            if ( st.m_op == 0 )
            {
                ui_font_draw_env_t env;
                const std::vector< std::uint8_t > matrix = ui_unhex( st.m_matrix );
                std::memcpy( env.m_matrix, matrix.data( ), 64 );
                env.m_texture_material = g_golden_sb_materials[ 0 ][ 0 ];
                env.m_texture_material_key = g_golden_sb_materials[ 0 ][ 1 ];
                env.m_rect_material = g_golden_sb_materials[ 1 ][ 0 ];
                env.m_rect_material_key = g_golden_sb_materials[ 1 ][ 1 ];
                sort.m_index = ge.m_index;
                sort.m_key_lo = ge.m_key_lo;
                sort.m_key_hi = ge.m_key_hi;
                before = list.size( );
                if ( run.m_bar )
                    ui_progress_bar_draw( sort, env, bf, bar, ui_lv_float( st.m_arg ) );
                else
                {
                    const ui_texture_t off = { g_golden_sb_textures[ 0 ].m_handle, g_golden_sb_textures[ 0 ].m_width, g_golden_sb_textures[ 0 ].m_height };
                    const ui_texture_t on = { g_golden_sb_textures[ 1 ].m_handle, g_golden_sb_textures[ 1 ].m_width, g_golden_sb_textures[ 1 ].m_height };
                    ui_boat_stat_draw( sort, env, sf, stat, ( st.m_arg & 1 ) ? &off : nullptr, ( st.m_arg & 2 ) ? &on : nullptr );
                }
            }
            else if ( st.m_op == 1 )
                ui_boat_stat_set( stat, params );
            else if ( st.m_op == 2 )
                ui_progress_bar_on_load_progress( bar, params );
            else if ( st.m_op == 3 )
                ui_progress_bar_initialize( bar, bf );
            else if ( st.m_op == 4 )
                ui_progress_bar_show( bar );
            else
                ui_progress_bar_hide( bar );
            state( have );
            same = have[ 0 ] == st.m_state[ 0 ] && have[ 1 ] == st.m_state[ 1 ];
            if ( st.m_op == 0 )
            {
                same = same && list.size( ) - before == static_cast< std::size_t >( st.m_commands );
                for ( int c = 0; same && c < st.m_commands; ++c )
                {
                    const golden_txt_command_t& want = g_golden_sb_commands[ st.m_first + c ];
                    const ui_gfx_command_t& h = list[ before + static_cast< std::size_t >( c ) ];
                    const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
                    const auto& memory = sort.m_buffers[ ge.m_index ].m_memory;
                    same = std::memcmp( h.m_words, want.m_words, sizeof( h.m_words ) ) == 0 && h.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                           std::memcmp( memory.data( ) + h.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
                    ++commands;
                }
            }
            ++steps;
            if ( !same && ++failed <= 8 )
                LOG_ERROR( "ui stat / bar: run %d (%s %d) step %d (op %d): the state or the commands differ from retail's", static_cast< int >( k ),
                    run.m_project, run.m_index, s, st.m_op );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_boat_stat_* / ui_progress_bar_* match retail's VuBoatStatEntity Set 0x44FCA0 + draw 0x44FB60 and VuLoadingProgressBarEntity "
                     "0x463220 / 0x463240 / 0x462C40 / 0x462C60 + draw 0x4632E0 on %d entities (%d of OUR loaded fields): %d steps, %d commands byte for byte",
            static_cast< int >( g_golden_sb_runs_count ), fields, steps, commands );
    else
        LOG_ERROR( "the UI boat stat / progress bar FAILED %d checks against retail's", failed );
    return failed == 0;
}

// verify_ui_screen_frame_leaves: ROUTING, not a retail comparison - ui_screen_frame
// on every Screens/* VuUIScreenEntity root, without and with the new inputs
// (Info: the ticker's ctor state; config: an empty option vector as loaded;
// settings combo: a record { id = the entity name, no entries }; boat stat: the
// ctor's 5; bar: the ctor's state), and every newly emitted leaf's commands equal
// to its standalone recreated draw (each verified against retail by its own
// check) run on a fresh sort with the leaf's matrix / float / key - words except
// the memory offset, and the records. Also counts the unrecreated leaves.
inline bool verify_ui_screen_frame_leaves( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    const ui_texture_t texture_a = { 0x5D00A000u, 256, 128 };
    const ui_texture_t texture_b = { 0x5D00A020u, 64, 32 };
    int failed = 0, roots = 0, before_total = 0, after_total = 0, routed = 0, commands = 0;
    for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
    {
        if ( std::strncmp( g_golden_ui_projects[ p ].m_asset, "Screens/", 8 ) != 0 || loaded.trees[ p ].empty( ) )
            continue;
        const entity_t& root = *loaded.trees[ p ][ 0 ].second;
        if ( !root.m_ctor || root.m_ctor->m_vtable != 0x006F7140u )
            continue;
        ++roots;
        ui_frame_inputs_t in;
        in.m_fontdb = &ui_font_db( 0 );
        in.m_text = []( const entity_t& who ) { return who.m_name; };
        in.m_font = []( const entity_t& ) { ui_text_font_t f; f.m_asset = true; f.m_asset_font = 0x0F0E0D0Cu; return f; };
        in.m_texture = [ & ]( const entity_t&, int which ) -> const ui_texture_t* { return which == 0 ? &texture_a : which == 1 ? &texture_b : nullptr; };
        ui_impl_t impl;
        ui_gfx_sort_t s0;
        ui_lv_sort( s0 );
        const ui_frame_t a = ui_screen_frame( root, impl, s0, 1.0f, 4, &in );
        std::vector< ui_config_entity_t > configs( loaded.trees[ p ].size( ) );
        std::vector< ui_config_combo_t > combos( loaded.trees[ p ].size( ) );
        const auto slot = [ & ]( const entity_t& who )
        {
            for ( std::size_t k = 0; k < loaded.trees[ p ].size( ); ++k )
                if ( loaded.trees[ p ][ k ].second == &who )
                    return k;
            return std::size_t( 0 );
        };
        for ( std::size_t k = 0; k < combos.size( ); ++k )
            combos[ k ].m_string_id = loaded.trees[ p ][ k ].second->m_name;
        in.m_info = []( const entity_t& ) { return ui_info_scroll_t( ); };
        in.m_config = [ & ]( const entity_t& who ) -> const ui_config_entity_t* { return &configs[ slot( who ) ]; };
        in.m_combo = [ & ]( const entity_t& who ) -> const ui_config_combo_t* { return &combos[ slot( who ) ]; };
        in.m_combo_inputs = [ & ]( const entity_t& )
        {
            ui_combo_inputs_t ci;
            ci.m_label_font = ci.m_value_font = 0x0F0E0D0Cu;
            ci.m_string = []( const std::string& id ) { return id + "~"; };
            ci.m_texture = [ & ]( int which ) -> const ui_texture_t* { return which % 2 ? &texture_b : &texture_a; };
            return ci;
        };
        in.m_stat = []( const entity_t& ) { return ui_boat_stat_t( ); };
        in.m_bar = []( const entity_t& ) { return ui_progress_bar_t( ); };
        ui_gfx_sort_t s1;
        ui_lv_sort( s1 );
        const ui_frame_t b = ui_screen_frame( root, impl, s1, 1.0f, 4, &in );
        before_total += a.m_unrecreated;
        after_total += b.m_unrecreated;
        bool same = a.m_draws.size( ) == b.m_draws.size( ) && b.m_unknown == 0;
        for ( std::size_t k = 0; same && k < b.m_draws.size( ); ++k )
        {
            const ui_frame_draw_t& d = b.m_draws[ k ];
            if ( d.m_emitted == a.m_draws[ k ].m_emitted )
                continue;
            // A newly emitted leaf: its standalone draw on a fresh sort.
            ui_gfx_sort_t s2;
            ui_lv_sort( s2 );
            s2.m_index = s1.m_index;
            s2.m_key_lo = d.m_key_lo;
            s2.m_key_hi = d.m_key_hi;
            ui_frame_draw_t alone = d;
            alone.m_emitted = false;
            const bool known = ui_frame_emit( s2, impl, alone, in );
            const auto& want = s1.m_commands[ s1.m_index ];
            const auto& got = s2.m_commands[ s2.m_index ];
            same = known && static_cast< int >( got.size( ) ) == d.m_commands;
            for ( int c = 0; same && c < d.m_commands; ++c )
            {
                const ui_gfx_command_t& w = want[ static_cast< std::size_t >( d.m_first + c ) ];
                const ui_gfx_command_t& h = got[ static_cast< std::size_t >( c ) ];
                const std::size_t size = ui_lv_record_size( s1, s1.m_index, w );
                for ( int j = 0; j < 8; ++j )
                    same = same && ( j == 3 || w.m_words[ j ] == h.m_words[ j ] );
                same = same && ( std::memcmp( s1.m_buffers[ s1.m_index ].m_memory.data( ) + w.m_words[ 3 ],
                                                           s2.m_buffers[ s2.m_index ].m_memory.data( ) + h.m_words[ 3 ], size ) == 0 );
                ++commands;
            }
            ++routed;
        }
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui screen frame leaves: %s: a newly routed leaf's commands differ from its standalone draw", g_golden_ui_projects[ p ].m_asset );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_screen_frame routes the Info / config selector / settings combo / boat stat / progress bar leaves to their recreated draws on %d "
                     "Screens roots: %d leaves newly emitted (%d commands equal to the standalone draws); unrecreated leaves %d -> %d",
            roots, routed, commands, before_total, after_total );
    else
        LOG_ERROR( "the screen frame's leaf routing FAILED %d checks", failed );
    return failed == 0;
}

// verify_ui_difficulty: retail's class picker 0x459D70 and VuDifficultyImageEntity
// 0x45AF40 / VuDifficultyTextEntity 0x45AA30 (oracle case difficulty_draw) on
// every such entity of the Screens projects x the scenarios (root / round / dev
// documents, Loading Screen written raw): the picker's answer and its progress
// slot-27 calls, then the draw's commands byte for byte (one sort per buffer)
// and the VuStringDB ids. The fields from OUR loaded entity (compared with
// retail's bytes; the text's font params 0..40 from OUR "Font C / B / A " over
// FontDB - 41..59 retail's, UNKNOWN); the real BoatDB / ChampDB documents.
inline bool verify_ui_difficulty( )
{
    ui_gfx_sort_t sort;
    ui_lv_sort( sort );
    const value_t boat_db = ui_fr_document( 0 ), champ_db = ui_fr_document( 3 );
    const char* const fonts[ 3 ] = { "Font C ", "Font B ", "Font A " };
    const char* const ids[ 3 ] = { "C String ID", "B String ID", "A String ID" };
    int failed = 0, runs = 0, commands = 0, fields = 0, strings = 0, classes[ 4 ] = { };
    for ( std::size_t n = 0; n < g_golden_dd_entities_count; ++n )
    {
        const golden_dd_entity_t& ge = g_golden_dd_entities[ n ];
        const entity_t* e = ui_lv_entity( ge.m_project, ge.m_index );
        const std::vector< std::uint8_t > raw = ui_unhex( ge.m_fields );
        bool same = e && e->m_type == ( ge.m_text ? "VuDifficultyTextEntity" : "VuDifficultyImageEntity" ) && raw.size( ) == ( ge.m_text ? 0x1Cu : 0x14u );
        const auto take = [ & ]( const char* name, void* out, const std::uint8_t* want, std::size_t bytes )
        {
            if ( !same || !ui_frame_take( *e, name, out, bytes ) || std::memcmp( out, want, bytes ) != 0 )
                same = false;
            ++fields;
        };
        ui_difficulty_image_t im;
        ui_difficulty_text_t tx;
        if ( same && !ge.m_text )
        {
            take( "Screen Scale", im.m_screen_scale, raw.data( ), 8 );
            take( "Loading Screen", &im.m_loading_screen, raw.data( ) + 8, 1 );
            take( "Position X", &im.m_position[ 0 ], raw.data( ) + 0xC, 4 );
            take( "Position Y", &im.m_position[ 1 ], raw.data( ) + 0x10, 4 );
            im.m_depth = ui_frame_depth( *e );
            same = same && ui_lv_bits( im.m_depth ) == ge.m_depth;
        }
        else if ( same )
        {
            const std::vector< std::uint8_t > flags = ui_unhex( ge.m_flags );
            take( "Screen Scale", tx.m_screen_scale, raw.data( ), 8 );
            take( "Loading Screen", &tx.m_loading_screen, raw.data( ) + 8, 1 );
            take( "X", &tx.m_rect[ 0 ], raw.data( ) + 0xC, 4 );
            take( "Y", &tx.m_rect[ 1 ], raw.data( ) + 0x10, 4 );
            take( "Width", &tx.m_rect[ 2 ], raw.data( ) + 0x14, 4 );
            take( "Height", &tx.m_rect[ 3 ], raw.data( ) + 0x18, 4 );
            same = same && flags.size( ) == 12;
            if ( same )
            {
                take( "Horizontal Alignment", &tx.m_h_align, flags.data( ), 4 );
                take( "Vertical Alignment", &tx.m_v_align, flags.data( ) + 4, 4 );
                take( "No Clip", &tx.m_no_clip, flags.data( ) + 8, 1 );
                take( "Single Line", &tx.m_single_line, flags.data( ) + 9, 1 );
                take( "Wordbreak", &tx.m_wordbreak, flags.data( ) + 10, 1 );
            }
            for ( int k = 0; same && k < 3; ++k )
            {
                const ui_slot_t* id = ui_property_value( *e, ids[ k ] );
                same = id && id->m_text == ge.m_ids[ k ];
                if ( same )
                    tx.m_string_id[ k ] = id->m_text;
                const std::vector< std::uint8_t > want = ui_unhex( ge.m_params[ k ] );
                const ui_slot_t* name = ui_property_value( *e, fonts[ k ] );
                same = same && name && want.size( ) == 60;
                if ( !same )
                    break;
                std::memcpy( tx.m_params[ k ], want.data( ), 60 );   // 41..59: retail's (UNKNOWN)
                std::memset( tx.m_params[ k ], 0, 41 );
                std::string asset;
                ui_font_params( ui_font_db( 0 ), name->m_text, tx.m_params[ k ], asset );
                same = std::memcmp( tx.m_params[ k ], want.data( ), 41 ) == 0;
                fields += 2;
            }
            tx.m_depth = ui_frame_depth( *e );
            same = same && ui_lv_bits( tx.m_depth ) == ge.m_depth;
        }
        if ( !same )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui difficulty: %s entity %d: OUR loaded fields differ from retail's", ge.m_project, ge.m_index );
            continue;
        }
        for ( int r = 0; r < ge.m_runs; ++r )
        {
            const golden_dd_run_t& run = g_golden_dd_runs[ ge.m_first + r ];
            const golden_txt_env_t& te = g_golden_txt_envs[ run.m_env ];
            const value_t root = c_reader( ui_unhex( run.m_root ) ).parse( );
            const std::vector< std::uint8_t > round_raw = ui_unhex( run.m_round ), dev_raw = ui_unhex( run.m_dev );
            const value_t round = round_raw.empty( ) ? value_t( ) : c_reader( round_raw ).parse( );
            const value_t dev = dev_raw.empty( ) ? value_t( ) : c_reader( dev_raw ).parse( );
            int rounds = 0;
            ui_difficulty_env_t de;
            de.m_root = &root;
            de.m_dev = dev_raw.empty( ) ? nullptr : &dev;
            de.m_boat_db = &boat_db;
            de.m_champ_db = &champ_db;
            de.m_round = [ & ]( ) -> const value_t& { ++rounds; return round; };
            const int k = ui_difficulty_class( run.m_loading != 0, de );
            ++classes[ k + 1 ];
            bool ok = k == run.m_class && rounds == run.m_rounds;
            ui_font_draw_env_t env;
            const std::vector< std::uint8_t > matrix = ui_unhex( run.m_matrix );
            std::memcpy( env.m_matrix, matrix.data( ), 64 );
            std::memcpy( &env.m_top, &te.m_top, 4 );
            for ( int m = 0; m < 2; ++m )
            {
                env.m_material[ m ] = 0x5D001000u + 0x800u * static_cast< std::uint32_t >( m );   // TxtWorld's font materials
                env.m_material_key[ m ] = g_golden_txt_material_keys[ m ];
            }
            env.m_texture_material = g_golden_dd_material[ 0 ];
            env.m_texture_material_key = g_golden_dd_material[ 1 ];
            sort.m_index = te.m_index;
            sort.m_key_lo = te.m_key_lo;
            sort.m_key_hi = te.m_key_hi;
            std::vector< ui_gfx_command_t >& list = sort.m_commands[ te.m_index ];
            const std::size_t before = list.size( );
            std::vector< std::string > asked;
            // the draw asks the picker again (its slot-27 calls are not counted twice: the case counts the first)
            if ( !ge.m_text )
            {
                ui_texture_t textures[ 2 ];
                for ( int t = 0; t < 2; ++t )
                    textures[ t ] = { g_golden_dd_textures[ t ].m_handle, g_golden_dd_textures[ t ].m_width, g_golden_dd_textures[ t ].m_height };
                const int c = k >= 0 ? run.m_choice[ k ] : 3;
                ui_difficulty_image_draw( sort, env, im, k, c < 2 ? &textures[ c ] : nullptr );
            }
            else
            {
                ui_string_db_t db;
                db.m_present = true;   // the case always serves VuStringDB
                db.m_slot12 = te.m_sdb_answer != 0;
                ui_text_font_t font;
                if ( k >= 0 )
                {
                    font.m_asset = run.m_fonts[ k ] != 0;
                    font.m_asset_font = g_golden_dd_fonts[ 0 ];
                }
                font.m_default = te.m_default != 0;
                font.m_default_font = g_golden_dd_fonts[ 1 ];
                ok = ui_difficulty_text_draw( sort, env, db, tx, k, font, [ & ]( const std::string& id ) { asked.push_back( id ); return id + "~"; } ) && ok;
            }
            ok = ok && list.size( ) - before == static_cast< std::size_t >( run.m_commands ) && static_cast< int >( asked.size( ) ) == run.m_strings;
            for ( int s = 0; ok && s < run.m_strings; ++s )
            {
                ok = asked[ static_cast< std::size_t >( s ) ] == g_golden_dd_strings[ run.m_first_string + s ];
                ++strings;
            }
            for ( int c = 0; ok && c < run.m_commands; ++c )
            {
                const golden_txt_command_t& want = g_golden_dd_commands[ run.m_first + c ];
                const ui_gfx_command_t& h = list[ before + static_cast< std::size_t >( c ) ];
                const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
                const auto& memory = sort.m_buffers[ te.m_index ].m_memory;
                ok = std::memcmp( h.m_words, want.m_words, sizeof( h.m_words ) ) == 0 && h.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                     std::memcmp( memory.data( ) + h.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
                ++commands;
            }
            ++runs;
            if ( !ok && ++failed <= 8 )
                LOG_ERROR( "ui difficulty: %s entity %d scenario %d: class %d (retail %d), %d slot-27 calls (retail %d), %d commands (retail %d) - or "
                           "an id / word / record differs", ge.m_project, ge.m_index, run.m_scenario, k, run.m_class, rounds, run.m_rounds,
                           static_cast< int >( list.size( ) - before ), run.m_commands );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_difficulty_class / ui_difficulty_image_draw / ui_difficulty_text_draw match retail's picker 0x459D70 (0x4D31D0, 0x4D32F0, "
                     "0x4D37B0) and the draws 0x45AF40 / 0x45AA30 on %d entities x the scenarios (%d runs: %d none, %d C, %d B, %d A; %d of OUR "
                     "loaded fields): %d commands byte for byte, %d string ids",
            static_cast< int >( g_golden_dd_entities_count ), runs, classes[ 0 ], classes[ 1 ], classes[ 2 ], classes[ 3 ], fields, commands, strings );
    else
        LOG_ERROR( "the UI difficulty picker / draws FAILED %d checks against retail's", failed );
    return failed == 0;
}

#include <workspace/utility/checks/update.hxx>
#include <workspace/utility/checks/widgets.hxx>
#include <workspace/utility/checks/results.hxx>

inline bool verify_ui_leaves_all( )
{
    bool ok = true;
    ok = verify_ui_update_all( ) && ok;
    ok = verify_ui_widgets_all( ) && ok;
    ok = verify_ui_results_all( ) && ok;
    ok = verify_ui_difficulty( ) && ok;
    ok = verify_ui_screen_frame_leaves( ) && ok;
    ok = verify_ui_stat_bar( ) && ok;
    ok = verify_ui_combo_draw( ) && ok;
    ok = verify_ui_info_tick( ) && ok;
    return ok;
}
