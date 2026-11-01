#pragma once

#include <workspace/core/ui/bind.hxx>
#include <workspace/core/ui/controls.hxx>
#include <workspace/core/ui/slider.hxx>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The UI area's widget draw checks (batch 15; included by checks/leaves.hxx before
// verify_ui_leaves_all, which runs verify_ui_widgets_all). Everything ui.hxx /
// menu.hxx / leaves.hxx define above is usable.

// verify_ui_slider_draw: retail's VuSliderEntity draws 0x4700E0 (alpha) and
// 0x470010 (the layout bool) on every slider of the loaded Settings* / Controls*
// screens (oracle case slider_draw). The fields from OUR loaded entity
// (c_ui_binding::ui_bind_slider; each compared with retail's bytes first), the
// font params 0..40 from OUR load over FontDB (41..59 retail's - UNKNOWN), slot
// 12's value from OUR getter over the run's settings manager; the textures, the
// font answer and VuStringDB are the run's (inputs). Commands and records byte for
// byte, one sort per buffer through all runs, and the string ids in call order.
inline bool verify_ui_slider_draw( )
{
    ui_gfx_sort_t sort;
    sort.m_buffers[ 0 ].m_capacity = 0x300000;
    sort.m_buffers[ 1 ].m_used = 0x35;
    sort.m_buffers[ 1 ].m_capacity = 0x100000;
    for ( auto& b : sort.m_buffers )
        b.m_memory.resize( static_cast< std::size_t >( b.m_capacity ) );
    int failed = 0, runs = 0, commands = 0, strings = 0, fields = 0;
    for ( std::size_t k = 0; k < g_golden_sld_entities_count; ++k )
    {
        const golden_sld_entity_t& ge = g_golden_sld_entities[ k ];
        const entity_t* e = ui_lv_entity( ge.m_project, ge.m_index );
        const std::vector< std::uint8_t > raw = ui_unhex( ge.m_fields );
        bool same = e && e->m_ctor && e->m_ctor->m_vtable == ge.m_vtable && raw.size( ) == 0x150;
        ui_slider_t s;
        std::uint8_t params[ 60 ] = { };
        if ( same )
        {
            s = c_ui_binding::ui_bind_slider( *e, ge.m_vtable );
            const auto against = [ & ]( const void* ours, int offset, std::size_t bytes )
            {
                if ( std::memcmp( ours, raw.data( ) + ( offset - 0x60 ), bytes ) != 0 )
                    same = false;
                ++fields;
            };
            const std::uint8_t visible = s.m_visible ? 1 : 0;
            against( &visible, 0x64, 1 );
            against( s.m_scale, 0x68, 8 );
            against( s.m_frame, 0xA0, 16 );
            against( s.m_text, 0xB0, 16 );
            against( &s.m_h_align, 0x124, 4 );
            against( &s.m_v_align, 0x128, 4 );
            against( &s.m_no_clip, 0x12C, 1 );
            against( &s.m_single_line, 0x12D, 1 );
            against( &s.m_wordbreak, 0x12E, 1 );
            against( s.m_dst, 0x160, 16 );
            against( s.m_dst_delta, 0x170, 8 );
            against( s.m_src, 0x178, 16 );
            against( s.m_src_delta, 0x188, 8 );
            against( s.m_src_on_delta, 0x190, 8 );
            against( &s.m_count, 0x198, 4 );
            same = same && s.m_string_id == ge.m_string_id;
            s.m_depth = ui_frame_depth( *e );
            same = same && ui_lv_bits( s.m_depth ) == ge.m_depth;
            const std::vector< std::uint8_t > want = ui_unhex( ge.m_params );
            const ui_slot_t* name = ui_property_value( *e, "Font" );
            same = same && name && want.size( ) == 60;
            if ( same )
            {
                std::memcpy( params, want.data( ), 60 );   // 41..59: retail's (UNKNOWN)
                std::memset( params, 0, 41 );
                std::string asset;
                ui_font_params( ui_font_db( 0 ), name->m_text, params, asset );
                same = std::memcmp( params, want.data( ), 41 ) == 0;
                ++fields;
            }
        }
        if ( !same )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui slider draw: %s entity %d: OUR loaded fields differ from retail's", ge.m_project, ge.m_index );
            continue;
        }
        ui_texture_t textures[ 2 ];
        for ( int t = 0; t < 2; ++t )
            textures[ t ] = { g_golden_sld_textures[ t ].m_handle, g_golden_sld_textures[ t ].m_width, g_golden_sld_textures[ t ].m_height };
        for ( int r = 0; r < ge.m_runs; ++r )
        {
            const golden_sld_run_t& run = g_golden_sld_runs[ ge.m_first + r ];
            const golden_txt_env_t& te = g_golden_txt_envs[ run.m_env ];
            ui_font_draw_env_t env;
            const std::vector< std::uint8_t > matrix = ui_unhex( run.m_matrix );
            std::memcpy( env.m_matrix, matrix.data( ), 64 );
            std::memcpy( &env.m_top, &te.m_top, 4 );
            for ( int m = 0; m < 2; ++m )
            {
                env.m_material[ m ] = 0x5D001000u + 0x800u * static_cast< std::uint32_t >( m );   // TxtWorld's font materials
                env.m_material_key[ m ] = g_golden_txt_material_keys[ m ];
            }
            env.m_texture_material = g_golden_sld_material[ 0 ];
            env.m_texture_material_key = g_golden_sld_material[ 1 ];
            env.m_rect_material = g_golden_sld_rect_material[ 0 ];
            env.m_rect_material_key = g_golden_sld_rect_material[ 1 ];
            ui_settings_t settings;
            std::memcpy( &settings.m_steering, &run.m_settings[ 0 ], 4 );
            std::memcpy( &settings.m_effect, &run.m_settings[ 1 ], 4 );
            std::memcpy( &settings.m_music, &run.m_settings[ 2 ], 4 );
            std::memcpy( &settings.m_display, &run.m_settings[ 3 ], 4 );
            ui_slider_hooks_t hooks;
            hooks.m_settings = run.m_has_settings ? &settings : nullptr;
            const c_ui_slider slider( 0u, s, hooks );
            ui_slider_draw_inputs_t in;
            const ui_texture_t* const* choice = nullptr;
            const ui_texture_t* by[ 4 ] = { &textures[ 0 ], &textures[ 1 ], nullptr, nullptr };   // 2: an asset without a texture, 3: none
            choice = by;
            in.m_frame = choice[ run.m_choice[ 0 ] ];
            in.m_selected = choice[ run.m_choice[ 1 ] ];
            in.m_bar = choice[ run.m_choice[ 2 ] ];
            ui_text_font_t font;
            font.m_asset = run.m_font != 0;
            font.m_asset_font = g_golden_sld_fonts[ 0 ];
            font.m_default = te.m_default != 0;
            font.m_default_font = g_golden_sld_fonts[ 1 ];
            in.m_font = font.m_asset ? font.m_asset_font : font.m_default ? font.m_default_font : 0u;   // 0x626B80
            std::memcpy( in.m_params, params, 60 );
            std::vector< std::string > asked;
            in.m_string = [ & ]( const std::string& id ) { asked.push_back( id ); return run.m_empty ? std::string( ) : id + "~"; };
            in.m_db.m_present = true;   // the case always serves VuStringDB
            in.m_db.m_slot12 = te.m_sdb_answer != 0;
            in.m_value = slider.value( );
            sort.m_index = te.m_index;
            sort.m_key_lo = te.m_key_lo;
            sort.m_key_hi = te.m_key_hi;
            std::vector< ui_gfx_command_t >& list = sort.m_commands[ te.m_index ];
            const std::size_t before = list.size( );
            float alpha;
            std::memcpy( &alpha, &run.m_arg, 4 );
            bool ok = run.m_fn ? ui_slider_layout_draw( sort, env, s, run.m_visible != 0, run.m_latched != 0, in, run.m_arg != 0 )
                               : ui_slider_draw( sort, env, s, run.m_visible != 0, run.m_latched != 0, in, alpha );
            ok = ok && list.size( ) - before == static_cast< std::size_t >( run.m_commands ) && static_cast< int >( asked.size( ) ) == run.m_strings;
            for ( int q = 0; ok && q < run.m_strings; ++q )
            {
                ok = asked[ static_cast< std::size_t >( q ) ] == g_golden_sld_strings[ run.m_first_string + q ];
                ++strings;
            }
            for ( int c = 0; ok && c < run.m_commands; ++c )
            {
                const golden_txt_command_t& want = g_golden_sld_commands[ run.m_first + c ];
                const ui_gfx_command_t& h = list[ before + static_cast< std::size_t >( c ) ];
                const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
                const auto& memory = sort.m_buffers[ te.m_index ].m_memory;
                ok = std::memcmp( h.m_words, want.m_words, sizeof( h.m_words ) ) == 0 && h.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                     std::memcmp( memory.data( ) + h.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
                ++commands;
            }
            ++runs;
            if ( !ok && ++failed <= 8 )
                LOG_ERROR( "ui slider draw: %s entity %d run %d: %d commands (retail %d), %d string ids (retail %d) - or a word / record differs",
                    ge.m_project, ge.m_index, r, static_cast< int >( list.size( ) - before ), run.m_commands, static_cast< int >( asked.size( ) ),
                    run.m_strings );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_slider_draw / ui_slider_layout_draw match retail's VuSliderEntity draws 0x4700E0 / 0x470010 (the four subclasses' slot 12 "
                     "getters) on %d sliders x the runs (%d runs; %d of OUR loaded fields): %d commands byte for byte, %d string ids",
            static_cast< int >( g_golden_sld_entities_count ), runs, fields, commands, strings );
    else
        LOG_ERROR( "the UI slider draw FAILED %d checks against retail's", failed );
    return failed == 0;
}

// verify_ui_hud_controls: retail's HUD control draws - pause 0x4257A0 (slot 15
// 0x426330), recover 0x425F70 (0x425F50), the on-screen axis / button 0x42A0C0 -
// on every such entity of the loaded race HUD projects (oracle case
// hud_controls). The fields from OUR loaded properties (compared with retail's
// bytes first); the textures, the touch state, the boing scale and the two
// globals are the run's (inputs). Commands and records byte for byte, one sort
// per buffer through all runs.
inline bool verify_ui_hud_controls( )
{
    ui_gfx_sort_t sort;
    sort.m_buffers[ 0 ].m_capacity = 0x300000;
    sort.m_buffers[ 1 ].m_used = 0x35;
    sort.m_buffers[ 1 ].m_capacity = 0x100000;
    for ( auto& b : sort.m_buffers )
        b.m_memory.resize( static_cast< std::size_t >( b.m_capacity ) );
    int failed = 0, runs = 0, commands = 0, fields = 0;
    for ( std::size_t k = 0; k < g_golden_hc_entities_count; ++k )
    {
        const golden_hc_entity_t& ge = g_golden_hc_entities[ k ];
        const entity_t* e = ui_lv_entity( ge.m_project, ge.m_index );
        const std::vector< std::uint8_t > raw = ui_unhex( ge.m_fields );
        bool same = e && e->m_ctor && e->m_ctor->m_vtable == ge.m_vtable && raw.size( ) == 0x9C;
        ui_hud_control_t c;
        if ( same )
        {
            const bool onscreen = ge.m_kind == 2;
            const int scale = onscreen ? 0x60 : 0x64, image = onscreen ? 0x68 : 0x6C, anchor = onscreen ? 0xB8 : 0xBC;
            const auto take = [ & ]( const char* name, void* out, std::size_t bytes, int offset )
            {
                same = ui_frame_take( *e, name, out, bytes ) && std::memcmp( out, raw.data( ) + ( offset - 0x5C ), bytes ) == 0 && same;
                ++fields;
            };
            take( "Screen Scale", c.m_scale, 8, scale );
            take( "Image X", &c.m_image[ 0 ], 4, image );
            take( "Image Y", &c.m_image[ 1 ], 4, image + 4 );
            take( "Image Width", &c.m_image[ 2 ], 4, image + 8 );
            take( "Image Height", &c.m_image[ 3 ], 4, image + 12 );
            take( "Horizontal Anchor", &c.m_anchor.m_horizontal, 4, anchor );
            take( "Vertical Anchor", &c.m_anchor.m_vertical, 4, anchor + 4 );
            take( "Horizontal Ratio", &c.m_anchor.m_h_ratio, 4, anchor + 8 );
            take( "Vertical Ratio", &c.m_anchor.m_v_ratio, 4, anchor + 12 );
            if ( onscreen )
            {
                const char* const names[ 4 ] = { "Enable for Tilt", "Enable for Touch", "Enable for GamePad", "Enable for Keyboard" };
                for ( int f = 0; f < 4; ++f )
                    take( names[ f ], &c.m_enable[ f ], 1, 0xC8 + f );
            }
            c.m_depth = ui_frame_depth( *e );
            same = same && ui_lv_bits( c.m_depth ) == ge.m_depth;
        }
        if ( !same )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui hud controls: %s entity %d: OUR loaded fields differ from retail's", ge.m_project, ge.m_index );
            continue;
        }
        ui_texture_t textures[ 2 ];
        for ( int t = 0; t < 2; ++t )
            textures[ t ] = { g_golden_hc_textures[ t ].m_handle, g_golden_hc_textures[ t ].m_width, g_golden_hc_textures[ t ].m_height };
        const ui_texture_t* by[ 4 ] = { &textures[ 0 ], &textures[ 1 ], nullptr, nullptr };   // 2: an asset without a texture, 3: none
        for ( int r = 0; r < ge.m_runs; ++r )
        {
            const golden_hc_run_t& run = g_golden_hc_runs[ ge.m_first + r ];
            const golden_txt_env_t& te = g_golden_txt_envs[ run.m_env ];
            ui_font_draw_env_t env;
            const std::vector< std::uint8_t > matrix = ui_unhex( run.m_matrix );
            std::memcpy( env.m_matrix, matrix.data( ), 64 );
            env.m_texture_material = g_golden_hc_material[ 0 ];
            env.m_texture_material_key = g_golden_hc_material[ 1 ];
            ui_crop_t crop;
            std::memcpy( &crop.m_matrix[ 0 ], &te.m_crop[ 0 ], 4 );
            std::memcpy( &crop.m_matrix[ 5 ], &te.m_crop[ 1 ], 4 );
            std::memcpy( &crop.m_matrix[ 12 ], &te.m_crop[ 2 ], 4 );
            std::memcpy( &crop.m_matrix[ 13 ], &te.m_crop[ 3 ], 4 );
            ui_hud_control_inputs_t in;
            in.m_texture = by[ run.m_choice[ 0 ] ];
            in.m_pressed_texture = by[ run.m_choice[ 1 ] ];
            in.m_state = run.m_state;
            std::memcpy( &in.m_boing, &run.m_boing, 4 );
            in.m_race_present = run.m_race >= 0;
            in.m_race_28 = static_cast< std::uint32_t >( run.m_race );
            in.m_method_present = run.m_method >= 0;
            in.m_method = run.m_method;
            float alpha;
            std::memcpy( &alpha, &run.m_alpha, 4 );
            sort.m_index = te.m_index;
            sort.m_key_lo = te.m_key_lo;
            sort.m_key_hi = te.m_key_hi;
            std::vector< ui_gfx_command_t >& list = sort.m_commands[ te.m_index ];
            const std::size_t before = list.size( );
            if ( ge.m_kind == 2 )
                ui_hud_onscreen_draw( sort, env, crop, c, in, alpha );
            else
                ui_hud_button_draw( sort, env, crop, c, in, ge.m_kind == 1, alpha );
            bool ok = list.size( ) - before == static_cast< std::size_t >( run.m_commands );
            for ( int q = 0; ok && q < run.m_commands; ++q )
            {
                const golden_txt_command_t& want = g_golden_hc_commands[ run.m_first + q ];
                const ui_gfx_command_t& h = list[ before + static_cast< std::size_t >( q ) ];
                const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
                const auto& memory = sort.m_buffers[ te.m_index ].m_memory;
                ok = std::memcmp( h.m_words, want.m_words, sizeof( h.m_words ) ) == 0 && h.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                     std::memcmp( memory.data( ) + h.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
                ++commands;
            }
            ++runs;
            if ( !ok && ++failed <= 8 )
                LOG_ERROR( "ui hud controls: %s entity %d run %d: %d commands (retail %d) - or a word / record differs", ge.m_project, ge.m_index, r,
                    static_cast< int >( list.size( ) - before ), run.m_commands );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_hud_button_draw / ui_hud_onscreen_draw match retail's HUD control draws (pause 0x4257A0 / 0x426330, recover 0x425F70 / "
                     "0x425F50, on-screen 0x42A0C0) on %d entities x the runs (%d runs; %d of OUR loaded fields): %d commands byte for byte",
            static_cast< int >( g_golden_hc_entities_count ), runs, fields, commands );
    else
        LOG_ERROR( "the UI HUD control draws FAILED %d checks against retail's", failed );
    return failed == 0;
}

// verify_ui_hud_toast: retail's VuHUDHiddenPackageToastEntity OnPackageFound
// 0x424600 / OnUITick 0x4248E0 through the scripted sequence on every toast of the
// loaded HUD projects (oracle case hud_toast). The toast starts from OUR loaded
// Total / Fade Time (compared with retail's state at load); VuStringDB slot 6 is
// the step's answer (a HOOK). After each step: Visible, alpha, the three times and
// the text, plus the keys slot 6 was asked.
inline bool verify_ui_hud_toast( )
{
    int failed = 0, steps = 0;
    const auto state_text = []( const ui_hud_toast_t& t )
    {
        const float f[ 4 ] = { t.m_alpha, t.m_total, t.m_fade, t.m_left };
        char text[ 64 ];
        std::snprintf( text, sizeof( text ), "%02x", t.m_visible ? 1 : 0 );
        std::string out = text;
        for ( float x : f )
        {
            std::snprintf( text, sizeof( text ), ".0x%08X", ui_lv_bits( x ) );
            out += text;
        }
        return out + "." + t.m_text;
    };
    for ( std::size_t k = 0; k < g_golden_ts_toasts_count; ++k )
    {
        const golden_ts_toast_t& gt = g_golden_ts_toasts[ k ];
        const entity_t* e = ui_lv_entity( gt.m_project, gt.m_index );
        ui_hud_toast_t t;
        bool same = e && e->m_type == "VuHUDHiddenPackageToastEntity";
        if ( same )
        {
            same = ui_frame_take( *e, "Total Time", &t.m_total, 4 ) && ui_frame_take( *e, "Fade Time", &t.m_fade, 4 );
            std::uint8_t visible = 1;
            if ( ui_frame_take( *e, "Visible", &visible, 1 ) )
                t.m_visible = visible != 0;
            same = same && state_text( t ) == gt.m_initial;
        }
        if ( !same )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui hud toast: %s entity %d: OUR loaded state differs from retail's", gt.m_project, gt.m_index );
            continue;
        }
        for ( int s = 0; s < gt.m_steps; ++s )
        {
            const golden_ts_step_t& st = g_golden_ts_steps[ gt.m_first + s ];
            const std::vector< std::uint8_t > raw = ui_unhex( st.m_params );
            ui_params_t p;
            std::memcpy( p.m_bytes, raw.data( ), raw.size( ) < 128 ? raw.size( ) : 128 );
            p.m_size = static_cast< int >( raw.size( ) );
            std::string keys;
            if ( st.m_op == 0 )
                ui_hud_toast_tick( t, p );
            else if ( st.m_op == 1 )
                ui_hud_toast_found( t, p, [ & ]( const char* key )
                {
                    keys += keys.empty( ) ? key : std::string( "," ) + key;
                    return std::string( st.m_template );
                } );
            else
            {
                std::memcpy( &t.m_total, raw.data( ), 4 );
                std::memcpy( &t.m_fade, raw.data( ) + 4, 4 );
            }
            ++steps;
            if ( ( state_text( t ) != st.m_state || keys != st.m_keys ) && ++failed <= 8 )
                LOG_ERROR( "ui hud toast: %s entity %d step %d: %s (retail %s)", gt.m_project, gt.m_index, s, state_text( t ).c_str( ), st.m_state );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_hud_toast_found / ui_hud_toast_tick match retail's VuHUDHiddenPackageToastEntity OnPackageFound 0x424600 / OnUITick "
                     "0x4248E0 on %d toasts from OUR loaded Total / Fade Time: %d steps (visible, alpha, times, text, the string-DB keys)",
            static_cast< int >( g_golden_ts_toasts_count ), steps );
    else
        LOG_ERROR( "the UI HUD toast FAILED %d checks against retail's", failed );
    return failed == 0;
}

inline bool verify_ui_widgets_all( )
{
    bool ok = true;
    ok = verify_ui_slider_draw( ) && ok;
    ok = verify_ui_hud_controls( ) && ok;
    ok = verify_ui_hud_toast( ) && ok;
    return ok;
}
