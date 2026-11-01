#pragma once

#include <workspace/core/ui/results.hxx>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The UI area's race results checks (batch 20; included by checks/leaves.hxx before
// verify_ui_leaves_all, which runs verify_ui_results_all). Everything ui.hxx / menu.hxx
// / leaves.hxx / widgets.hxx define above is usable.

// One recorded runtime state (+0x330, +0x34C, the pad array's size / capacity / entries).
inline bool ui_rr_state_same( const ui_results_table_t& t, const golden_rr_state_t& s )
{
    if ( t.m_championship != s.m_champ || t.m_active_rows != s.m_active || t.m_size != s.m_size || t.m_capacity != s.m_capacity ||
         s.m_pads != ( s.m_size < 64 ? s.m_size : 64 ) )
        return false;
    for ( int k = 0; k < s.m_pads; ++k )
        if ( static_cast< std::uint32_t >( t.m_pads[ static_cast< std::size_t >( k ) ] ) != g_golden_rr_pads[ s.m_first_pad + k ] )
            return false;
    return true;
}

// verify_ui_race_results: retail's VuRaceResultsTableEntity slot 6 0x469B10, draw
// 0x46A500, layout draw 0x469BD0 and its VuRetVal plugs 0x469B70 / 0x469BB0 in one
// sequence per authored table (oracle case race_results), and VuUserNextUIAction slot 12
// 0x470E50. The fields from OUR loaded entity (ui_results_table_of; each compared with
// retail's bytes first), the six fonts' params 0..40 from OUR load over FontDB (41..59
// retail's - UNKNOWN), the state after the load = OUR ctor state; the game-data root =
// OUR reader (c_reader) on the run's VUJB; the font answers, the track manager and
// VuStringDB slot 6 are the run's (inputs). Per run: commands and records byte for byte
// (one sort per buffer through all runs), the string keys in order, the VuRetVal, the
// state after.
inline bool verify_ui_race_results( )
{
    ui_gfx_sort_t sort;
    ui_lv_sort( sort );
    int failed = 0, runs = 0, commands = 0, strings = 0, fields = 0;
    for ( std::size_t k = 0; k < g_golden_rr_entities_count; ++k )
    {
        const golden_rr_entity_t& ge = g_golden_rr_entities[ k ];
        const entity_t* e = ui_lv_entity( ge.m_project, ge.m_index );
        const std::vector< std::uint8_t > raw = ui_unhex( ge.m_fields );
        bool same = e && e->m_ctor && e->m_ctor->m_vtable == ge.m_vtable && raw.size( ) == 0x300;
        ui_results_table_t t;
        ui_results_inputs_t in;
        if ( same )
        {
            t = ui_results_table_of( *e );
            const auto against = [ & ]( const void* ours, int offset, std::size_t bytes )
            {
                if ( std::memcmp( ours, raw.data( ) + ( offset - 0x60 ), bytes ) != 0 )
                    same = false;
                ++fields;
            };
            against( t.m_scale, 0x64, 8 );
            against( t.m_pos, 0x6C, 8 );
            against( &t.m_highlight, 0x74, 4 );
            const int rects[ 6 ] = { 0x78, 0xEC, 0x160, 0x1D4, 0x248, 0x2BC };
            for ( int c = 0; c < 6; ++c )
                against( t.m_rect[ c ], rects[ c ], 16 );
            against( &t.m_width, 0x334, 4 );
            against( &t.m_spacing, 0x338, 4 );
            against( &t.m_header, 0x33C, 4 );
            against( &t.m_inactive, 0x340, 4 );
            against( &t.m_active, 0x344, 4 );
            against( &t.m_player, 0x348, 4 );
            same = same && ui_lv_bits( t.m_depth ) == ge.m_depth && ui_rr_state_same( t, ge.m_initial );
            for ( int c = 0; same && c < 6; ++c )
            {
                const golden_rr_font_t& gf = g_golden_rr_font_rows[ ge.m_first_font + c ];
                const ui_slot_t* name = ui_property_value( *e, k_ui_results_font_names[ c ] );
                const std::vector< std::uint8_t > want = ui_unhex( gf.m_params );
                same = name && name->m_text == gf.m_name && want.size( ) == 60;
                if ( !same )
                    break;
                std::uint8_t* params = in.m_fonts[ c ].m_params;
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
                LOG_ERROR( "ui race results: %s entity %d: OUR loaded fields / fonts / ctor state differ from retail's", ge.m_project, ge.m_index );
            continue;
        }
        std::vector< std::string > asked;
        bool flag_answer[ 2 ] = { false, false };
        in.m_string = [ & ]( const std::string& id ) { asked.push_back( id ); return id + "~"; };
        for ( int r = 0; r < ge.m_runs; ++r )
        {
            const golden_rr_run_t& run = g_golden_rr_runs[ ge.m_first + r ];
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
            env.m_texture_material = g_golden_rr_material[ 0 ];
            env.m_texture_material_key = g_golden_rr_material[ 1 ];
            env.m_rect_material = g_golden_rr_rect_material[ 0 ];
            env.m_rect_material_key = g_golden_rr_rect_material[ 1 ];
            for ( int c = 0; c < 6; ++c )   // 0x626B80: the asset's font, else VuUIImpl's default where the environment has one
                in.m_fonts[ c ].m_font = run.m_fonts[ c ] ? g_golden_rr_fonts[ 0 ] : te.m_default ? g_golden_rr_fonts[ 1 ] : 0u;
            const std::vector< std::uint8_t > blob = ui_unhex( run.m_blob );
            const value_t root = blob.empty( ) ? value_t( ) : c_reader( blob ).parse( );
            asked.clear( );
            sort.m_index = te.m_index;
            sort.m_key_lo = te.m_key_lo;
            sort.m_key_hi = te.m_key_hi;
            std::vector< ui_gfx_command_t >& list = sort.m_commands[ te.m_index ];
            const std::size_t before = list.size( );
            bool ok = true;
            if ( run.m_op == 0 )
                ui_results_initialize( t, root, nullptr );   // dev config g 0x75901C null: the shipping game
            else if ( run.m_op == 1 )
                ui_results_draw( sort, env, t, in, root );
            else if ( run.m_op == 2 )
                ui_results_layout_draw( sort, env, t, in );
            else
            {
                flag_answer[ 0 ] = run.m_track[ 0 ] != 0;
                flag_answer[ 1 ] = run.m_track[ 1 ] != 0;
                const bool v = run.m_op == 3 ? ui_results_is_circuit( flag_answer[ 0 ], flag_answer[ 1 ] ) : ui_results_points_shown( t );
                ok = run.m_ret[ 0 ] == 3u && ( run.m_ret[ 1 ] & 0xFFu ) == ( v ? 1u : 0u );
            }
            ok = ok && list.size( ) - before == static_cast< std::size_t >( run.m_commands ) && static_cast< int >( asked.size( ) ) == run.m_strings;
            for ( int q = 0; ok && q < run.m_strings; ++q )
            {
                ok = asked[ static_cast< std::size_t >( q ) ] == g_golden_rr_strings[ run.m_first_string + q ];
                ++strings;
            }
            for ( int c = 0; ok && c < run.m_commands; ++c )
            {
                const golden_txt_command_t& want = g_golden_rr_commands[ run.m_first + c ];
                const ui_gfx_command_t& h = list[ before + static_cast< std::size_t >( c ) ];
                const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
                const auto& memory = sort.m_buffers[ te.m_index ].m_memory;
                ok = std::memcmp( h.m_words, want.m_words, sizeof( h.m_words ) ) == 0 && h.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                     std::memcmp( memory.data( ) + h.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
                ++commands;
            }
            ok = ok && ui_rr_state_same( t, run.m_state );
            ++runs;
            if ( !ok && ++failed <= 8 )
                LOG_ERROR( "ui race results: %s entity %d run %d (op %d): %d commands (retail %d), %d string keys (retail %d) - or a word / record / "
                           "the state differs", ge.m_project, ge.m_index, r, run.m_op, static_cast< int >( list.size( ) - before ), run.m_commands,
                           static_cast< int >( asked.size( ) ), run.m_strings );
        }
    }
    int nexts = 0;
    for ( std::size_t k = 0; k < g_golden_rr_nexts_count; ++k )
    {
        const golden_rr_next_t& gn = g_golden_rr_nexts[ k ];
        std::vector< std::pair< std::uint32_t, int > > sent;
        const std::uint32_t ret = ui_user_next( [ &sent ]( std::uint32_t hash, const ui_params_t& p ) { sent.push_back( { hash, p.m_size } ); } );
        const bool ok = gn.m_slot12 == 0x470E50u && gn.m_broadcasts == 1 && sent.size( ) == 1 && sent[ 0 ].first == gn.m_hash &&
                        sent[ 0 ].second == gn.m_params_size && ret == gn.m_ret[ 0 ];
        ++nexts;
        if ( !ok && ++failed <= 8 )
            LOG_ERROR( "ui race results: %s VuUserNextUIAction %d: the broadcast / VuRetVal differs from retail's 0x470E50", gn.m_project, gn.m_index );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_results_initialize / ui_results_draw / ui_results_layout_draw / the plugs match retail's VuRaceResultsTableEntity 0x469B10 / "
                     "0x46A500 / 0x469BD0 / 0x469B70 / 0x469BB0 on %d tables (%d runs; %d of OUR loaded fields / fonts): %d commands byte for byte, "
                     "%d string keys, the state after each; VuUserNextUIAction 0x470E50 on %d actions",
            static_cast< int >( g_golden_rr_entities_count ), runs, fields, commands, strings, nexts );
    else
        LOG_ERROR( "the UI race results table FAILED %d checks against retail's", failed );
    return failed == 0;
}

inline bool verify_ui_results_all( )
{
    bool ok = true;
    ok = verify_ui_race_results( ) && ok;
    return ok;
}
