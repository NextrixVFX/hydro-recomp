#pragma once

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

inline bool verify_ui_leaves_all( )
{
    bool ok = true;
    ok = verify_ui_info_tick( ) && ok;
    return ok;
}
