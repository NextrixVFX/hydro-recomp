#pragma once

#include <workspace/core/ui/combo.hxx>
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
    int failed = 0, commands = 0, fields = 0, rows = 0, strings = 0;
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
        if ( run.m_box )
        {
            const float d = depth * 0.005f + 0.5f;   // 0x475B10: 0x6F75C4, 0x6F765C
            known = !ce.m_combos.empty( ) && ui_combo_draw( sort, env, b, ce.m_combos[ 0 ], position, scale, d, in );
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
        LOG_SUCCESS( "ui_config_draw / ui_combo_draw match retail's VuBaseConfigEntity slot 16 0x448F40 -> 0x448F50 and 0x475B10 -> the combo box draw "
                     "0x4578D0 (0x457E00, 0x457EB0) on %d runs (%d rows; %d of OUR loaded fields): %d commands and records byte for byte, %d string ids",
            static_cast< int >( g_golden_cbd_runs_count ), rows, fields, commands, strings );
    else
        LOG_ERROR( "the UI combo box draw FAILED %d checks against retail's", failed );
    return failed == 0;
}

inline bool verify_ui_leaves_all( )
{
    bool ok = true;
    ok = verify_ui_combo_draw( ) && ok;
    ok = verify_ui_info_tick( ) && ok;
    return ok;
}
