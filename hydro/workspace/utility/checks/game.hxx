#pragma once

#include <workspace/core/world/rules.hxx>
#include <workspace/core/world/tick.hxx>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The world area's game-mode checks (split from checks/world.hxx, included
// from it; verify_world_all runs them): the game objects' state machines and
// handlers against retail's own code run whole in the oracle.

// race_rules_construct / race_rules_start / race_rules_tick and the handlers
// race_rules_on_start_lights_out / race_rules_on_buoy_hit (core/world
// rules.hxx) against VuRaceGame's ctor 0x489030 run whole, then driven by
// retail's slot 7 0x489A40, 0x48A780 and 0x48A7B0 (oracle case race_game_fsm).
// The table: each state's name, flag, FNV-1a hash, the four delegates' methods
// (enter / exit / tick / draw: m_address; the VuMethod0 / VuMethod1<float>
// vtables 0x73F8DC / 0x73F8CC asserted) and transitions; the conditions; the
// event methods (k_race_rules_events) and the HUD loads (k_race_hud_projects);
// one failure per mismatching item. Then per frame: slot 7's result, the
// current / previous state, the time +0x71C, the conditions' value and pulse,
// +0x744, every boat's +0x308 / +0x4B0 / +0x4B4 / +0x4CC and the calls (the HUD
// verbs as retail's live bit and root type gate them, the broadcast, the
// platform calls) - one failure per differing frame.
inline bool verify_race_game_fsm( )
{
    int failed = 0, frames = 0, calls = 0;
    const auto fail = [ & ]( const std::string& what ) {
        if ( ++failed <= 10 )
            LOG_ERROR( "race game fsm: %s", what.c_str( ) );
    };
    char text[ 256 ];
    race_rules_t probe;
    race_rules_construct( probe );
    if ( probe.fsm.m_states.size( ) != g_golden_rgf_states_count )
        fail( "state count differs" );
    for ( std::size_t k = 0; k < g_golden_rgf_states_count && k < probe.fsm.m_states.size( ); ++k )
    {
        const golden_rgf_state_t& g = g_golden_rgf_states[ k ];
        const fsm_state_t& s = *probe.fsm.m_states[ k ];
        bool same = s.m_name == g.m_name && static_cast< int >( s.m_flag ) == g.m_flag && s.m_hash == g.m_hash &&
                    s.m_transitions.size( ) == static_cast< std::size_t >( g.m_trs );
        for ( int d = 0; d < 4; ++d )
        {
            same = same && s.m_address[ d ] == g.m_method[ d ];
            const unsigned vt = g.m_method[ d ] == 0 ? 0u : d == 2 ? 0x73F8CCu : 0x73F8DCu;
            same = same && g.m_vtable[ d ] == vt;
        }
        for ( int t = 0; same && t < g.m_trs; ++t )
            same = s.m_transitions[ static_cast< std::size_t >( t ) ].first == g_golden_rgf_transitions[ g.m_tr + t ][ 0 ] &&
                   s.m_transitions[ static_cast< std::size_t >( t ) ].second == g_golden_rgf_transitions[ g.m_tr + t ][ 1 ];
        if ( !same )
            fail( std::string( "state " ) + g.m_name + " differs from the ctor's" );
    }
    if ( probe.fsm.m_conditions.size( ) != g_golden_rgf_conditions_count )
        fail( "condition count differs" );
    for ( std::size_t k = 0; k < g_golden_rgf_conditions_count && k < probe.fsm.m_conditions.size( ); ++k )
    {
        const fsm_condition_t& c = probe.fsm.m_conditions[ k ];
        const golden_rgf_condition_t& g = g_golden_rgf_conditions[ k ];
        if ( c.m_name != g.m_name || c.m_hash != g.m_hash || c.m_value != g.m_value || c.m_pulse != g.m_pulse )
            fail( std::string( "condition " ) + g.m_name + " differs" );
    }
    if ( g_golden_rgf_events_count != 2 )
        fail( "VuRaceGame's event count differs" );
    for ( std::size_t k = 0; k < g_golden_rgf_events_count && k < 2; ++k )
        if ( std::strcmp( k_race_rules_events[ k ].name, g_golden_rgf_events[ k ].m_name ) != 0 ||
             k_race_rules_events[ k ].method != g_golden_rgf_events[ k ].m_method )
            fail( std::string( "event " ) + g_golden_rgf_events[ k ].m_name + " differs" );
    if ( g_golden_rgf_loaded_count != 3 )
        fail( "the HUD load count differs" );
    for ( std::size_t k = 0; k < g_golden_rgf_loaded_count && k < 3; ++k )
        if ( std::strcmp( k_race_hud_projects[ k ], g_golden_rgf_loaded[ k ] ) != 0 )
            fail( std::string( "HUD load " ) + g_golden_rgf_loaded[ k ] + " differs" );
    // VuGame's own machines (+0x0C, +0x67C: vugame_fsms_construct) - the same table compare per machine
    const c_fsm* machines[ 2 ] = { &probe.event, &probe.split };
    for ( int m = 0; m < 2; ++m )
    {
        const golden_rgf_machine_t& gm = g_golden_rgf_machines[ m ];
        const c_fsm& f = *machines[ m ];
        if ( f.m_states.size( ) != static_cast< std::size_t >( gm.m_states ) || f.m_conditions.size( ) != static_cast< std::size_t >( gm.m_conditions ) ||
             f.m_current != gm.m_current || gm.m_time != 0u || f.m_time != 0.0f )
        {
            fail( "VuGame machine " + std::to_string( m ) + ": the counts / current / time differ" );
            continue;
        }
        for ( int k = 0; k < gm.m_states; ++k )
        {
            const golden_rgf_state_t& g = g_golden_rgf_machine_states[ gm.m_state + k ];
            const fsm_state_t& s = *f.m_states[ static_cast< std::size_t >( k ) ];
            bool same = s.m_name == g.m_name && static_cast< int >( s.m_flag ) == g.m_flag && s.m_hash == g.m_hash &&
                        s.m_transitions.size( ) == static_cast< std::size_t >( g.m_trs );
            for ( int d = 0; d < 4; ++d )
                same = same && s.m_address[ d ] == g.m_method[ d ] && g.m_vtable[ d ] == ( g.m_method[ d ] == 0 ? 0u : d == 2 ? 0x73F63Cu : 0x73F64Cu );
            for ( int t = 0; same && t < g.m_trs; ++t )
                same = s.m_transitions[ static_cast< std::size_t >( t ) ].first == g_golden_rgf_machine_transitions[ g.m_tr + t ][ 0 ] &&
                       s.m_transitions[ static_cast< std::size_t >( t ) ].second == g_golden_rgf_machine_transitions[ g.m_tr + t ][ 1 ];
            if ( !same )
                fail( "VuGame machine " + std::to_string( m ) + " state " + g.m_name + " differs from the ctor's" );
        }
        for ( int k = 0; k < gm.m_conditions; ++k )
        {
            const fsm_condition_t& c = f.m_conditions[ static_cast< std::size_t >( k ) ];
            const golden_rgf_condition_t& g = g_golden_rgf_machine_conditions[ gm.m_condition + k ];
            if ( c.m_name != g.m_name || c.m_hash != g.m_hash || c.m_value != g.m_value || c.m_pulse != g.m_pulse )
                fail( "VuGame machine " + std::to_string( m ) + " condition " + g.m_name + " differs" );
        }
    }
    // the ctor's words: current = the first state, previous / next / time 0, +0x720 .. +0x734 and +0x744 zero
    if ( probe.fsm.m_current != g_golden_rgf_words[ 0 ] || g_golden_rgf_words[ 1 ] != 0 || g_golden_rgf_words[ 2 ] != 0 ||
         g_golden_rgf_words[ 3 ] != 0 || g_golden_rgf_words[ 4 ] != 1 || probe.buoys != 0 || probe.fsm.m_time != 0.0f )
        fail( "the ctor's words differ" );

    for ( std::size_t v = 0; v < g_golden_rgf_vectors_count; ++v )
    {
        const golden_rgf_vector_t& gv = g_golden_rgf_vectors[ v ];
        race_rules_t r;
        race_rules_construct( r );
        std::vector< std::vector< std::uint8_t > > images( static_cast< std::size_t >( gv.m_boats ), std::vector< std::uint8_t >( 0x600, 0 ) );
        std::vector< race_rules_boat_t > boats( images.size( ) );
        race_rules_game_t game;
        for ( std::size_t k = 0; k < images.size( ); ++k )
        {
            const golden_rgf_boat_t& b = g_golden_rgf_boats[ gv.m_boat + static_cast< int >( k ) ];
            std::uint8_t* m = images[ k ].data( );
            rules_put_i32( m, 0x308, b.m_hold );
            std::memcpy( m + 0x328, &b.m_boost, 4 );
            m[ 0x32D ] = static_cast< std::uint8_t >( b.m_used );
            m[ 0x334 ] = static_cast< std::uint8_t >( b.m_quit );
            rules_put_i32( m, 0x4AC, b.m_lap );
            std::memcpy( m + 0x4B0, &b.m_lapclock, 4 );
            std::memcpy( m + 0x4B4, &b.m_clock, 4 );
            std::memcpy( m + 0x4B8, &b.m_best, 4 );
            std::memcpy( m + 0x4BC, &b.m_left, 4 );
            rules_put_i32( m, 0x4CC, static_cast< std::int32_t >( k ) );
            boats[ k ].boat = m;
            game.order.push_back( static_cast< int >( k ) );
        }
        r.boats = &boats;
        r.game = &game;
        if ( gv.m_human )
            r.humans.push_back( images[ 0 ].data( ) );
        r.track = gv.m_track;
        int live[ 3 ] = { gv.m_live[ 0 ], gv.m_live[ 1 ], gv.m_live[ 2 ] };
        std::vector< std::string > got;
        r.hooks.hud = [ & ]( int p, race_hud_verb_t verb, float dt ) {
            if ( !gv.m_present[ p ] )
                return;
            if ( verb == race_hud_verb_t::initialize && !live[ p ] )
            {
                live[ p ] = 1;
                got.push_back( "init " + std::to_string( p ) );
            }
            else if ( verb == race_hud_verb_t::release && live[ p ] )
            {
                live[ p ] = 0;
                got.push_back( "release " + std::to_string( p ) );
            }
            else if ( verb == race_hud_verb_t::tick && gv.m_hud[ p ] )
            {
                unsigned w;
                std::memcpy( &w, &dt, 4 );
                std::snprintf( text, sizeof( text ), "tick %d %08X", p, w );
                got.push_back( text );
            }
        };
        r.hooks.broadcast = [ & ]( const char* name, bool value ) { got.push_back( std::string( "broadcast " ) + name + ( value ? " 1" : " 0" ) ); };
        r.hooks.platform = [ & ]( const std::string& call ) { got.push_back( call ); };
        const auto same_calls = [ & ]( int first, int count ) {
            if ( got.size( ) != static_cast< std::size_t >( count ) )
                return false;
            for ( int c = 0; c < count; ++c )
                if ( got[ static_cast< std::size_t >( c ) ] != g_golden_rgf_calls[ first + c ] )
                    return false;
            return true;
        };
        // slot 5's tail: current = the first state, its enter
        r.fsm.m_current = 0;
        race_rules_start( r );
        calls += gv.m_calls;
        if ( !same_calls( gv.m_call, gv.m_calls ) )
            fail( "vector " + std::to_string( v ) + ": the start's calls differ" );
        for ( int f = 0; f < gv.m_frames; ++f )
        {
            const golden_rgf_frame_t& g = g_golden_rgf_frames[ gv.m_frame + f ];
            got.clear( );
            if ( g.m_lights )
                race_rules_on_start_lights_out( r );
            for ( int b = 0; b < g.m_buoys; ++b )
            {
                const golden_rgf_buoy_t& gb = g_golden_rgf_buoys[ g.m_buoy + b ];
                std::uint8_t params[ 0x80 ] = { };
                std::memcpy( params, &gb.m_tag, 4 );
                std::memcpy( params + 4, &gb.m_value, 4 );
                race_rules_on_buoy_hit( r, params, gb.m_size );
            }
            for ( std::size_t k = 0; k < images.size( ); ++k )
                if ( g.m_finish & ( 1u << k ) )
                    images[ k ][ 0x32F ] = 1;
            float dt;
            std::memcpy( &dt, &g.m_dt, 4 );
            const bool ret = race_rules_tick( r, dt );
            unsigned time;
            std::memcpy( &time, &r.fsm.m_time, 4 );
            bool same = static_cast< int >( ret ) == g.m_ret && r.fsm.m_current == g.m_current && r.fsm.m_previous == g.m_previous &&
                        time == g.m_time && r.buoys == g.m_mask && r.fsm.m_conditions.size( ) == 4;
            for ( std::size_t c = 0; same && c < 4; ++c )
                same = r.fsm.m_conditions[ c ].m_value == g.m_conditions[ 2 * c ] && r.fsm.m_conditions[ c ].m_pulse == g.m_conditions[ 2 * c + 1 ];
            for ( std::size_t k = 0; same && k < images.size( ); ++k )
            {
                const golden_rgf_row_t& row = g_golden_rgf_rows[ g.m_row + static_cast< int >( k ) ];
                const std::uint8_t* m = images[ k ].data( );
                unsigned lapclock, clock;
                std::memcpy( &lapclock, m + 0x4B0, 4 );
                std::memcpy( &clock, m + 0x4B4, 4 );
                same = rules_i32( m, 0x308 ) == row.m_hold && lapclock == row.m_lapclock && clock == row.m_clock && rules_i32( m, 0x4CC ) == row.m_place;
            }
            const bool calls_same = same_calls( g.m_call, g.m_calls );
            ++frames;
            calls += g.m_calls;
            if ( !same || !calls_same )
            {
                std::snprintf( text, sizeof( text ), "vector %d frame %d: ret %d state %d prev %d time 0x%08X mask 0x%X calls %d%s, retail %d %d %d 0x%08X 0x%X %d%s",
                               static_cast< int >( v ), f, static_cast< int >( ret ), r.fsm.m_current, r.fsm.m_previous, time, r.buoys,
                               static_cast< int >( got.size( ) ), got.empty( ) ? "" : ( " first '" + got[ 0 ] + "'" ).c_str( ), g.m_ret, g.m_current,
                               g.m_previous, g.m_time, g.m_mask, g.m_calls, g.m_calls ? ( std::string( " first '" ) + g_golden_rgf_calls[ g.m_call ] + "'" ).c_str( ) : "" );
                fail( text );
            }
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "race_rules (VuRaceGame) match retail's ctor 0x489030 run whole + slot 7 0x489A40 / 0x48A780 / 0x48A7B0: the table (%d states, %d conditions, "
                     "2 events, 3 HUD loads), %d vectors, %d frames, %d calls",
                     static_cast< int >( g_golden_rgf_states_count ), static_cast< int >( g_golden_rgf_conditions_count ),
                     static_cast< int >( g_golden_rgf_vectors_count ), frames, calls );
    else
        LOG_ERROR( "race game fsm FAILED %d checks", failed );
    return failed == 0;
}

// timetrial_rules_construct + race_rules_start / race_rules_tick / race_rules_on_start_lights_out (core/world rules.hxx)
// against VuTimeTrialGame's ctor 0x48ED30 run whole, then retail's slot 5 0x48F310, slot 7 0x48F350 and 0x490540 frame
// by frame (oracle case timetrial_game_fsm; the human's TrackDB times a drawn document read by retail's reader, here by
// c_reader). Per frame: slot 7's result, current / previous / time, every condition's value and pulse, every boat's
// +0x308 / +0x4B0 / +0x4B4 / +0x4CC and the calls (HUD verbs, the broadcast, the platform text) - one failure per
// differing frame; the start's calls once per vector.
inline bool verify_timetrial_game_fsm( )
{
    int failed = 0, frames = 0, calls = 0, unrecreated = 0;
    const auto fail = [ & ]( const std::string& what ) {
        if ( ++failed <= 10 )
            LOG_ERROR( "timetrial game fsm: %s", what.c_str( ) );
    };
    const unsigned* w = g_golden_ttf_words;
    std::size_t s = 0;
    const auto next = [ & ] { return *w++; };
    const auto text = [ & ] { return std::string( g_golden_ttf_text[ s++ ] ); };
    char line[ 256 ];
    const unsigned nv = next( );
    for ( unsigned v = 0; v < nv; ++v )
    {
        const int nb = static_cast< int >( next( ) );
        const bool human = next( ) != 0;
        int present[ 3 ], live[ 3 ], hud[ 3 ];
        for ( int& x : present ) x = static_cast< int >( next( ) );
        for ( int& x : live ) x = static_cast< int >( next( ) );
        for ( int& x : hud ) x = static_cast< int >( next( ) );
        race_rules_t r;
        timetrial_rules_construct( r );
        std::vector< std::vector< std::uint8_t > > images( static_cast< std::size_t >( nb ), std::vector< std::uint8_t >( 0x600, 0 ) );
        std::vector< race_rules_boat_t > boats( images.size( ) );
        race_rules_game_t game;
        for ( std::size_t k = 0; k < images.size( ); ++k )
        {
            std::uint8_t* m = images[ k ].data( );
            const std::int32_t lap = static_cast< std::int32_t >( next( ) );
            const unsigned lapclock = next( ), clock = next( ), best = next( ), left = next( ), quit = next( ), boost = next( ), used = next( );
            const std::int32_t hold = static_cast< std::int32_t >( next( ) ), place = static_cast< std::int32_t >( next( ) );
            rules_put_i32( m, 0x308, hold );
            std::memcpy( m + 0x328, &boost, 4 );
            m[ 0x32D ] = static_cast< std::uint8_t >( used );
            m[ 0x334 ] = static_cast< std::uint8_t >( quit );
            rules_put_i32( m, 0x4AC, lap );
            std::memcpy( m + 0x4B0, &lapclock, 4 );
            std::memcpy( m + 0x4B4, &clock, 4 );
            std::memcpy( m + 0x4B8, &best, 4 );
            std::memcpy( m + 0x4BC, &left, 4 );
            rules_put_i32( m, 0x4CC, place );
            boats[ k ].boat = m;
            game.order.push_back( static_cast< int >( k ) );
        }
        r.boats = &boats;
        r.game = &game;
        if ( human )
            r.humans.push_back( images[ 0 ].data( ) );
        r.track = text( );
        r.human_boat = text( );
        const std::string hex = text( );
        std::vector< std::uint8_t > bytes;
        for ( std::size_t k = 0; k + 1 < hex.size( ); k += 2 )
            bytes.push_back( static_cast< std::uint8_t >( std::stoul( hex.substr( k, 2 ), nullptr, 16 ) ) );
        const value_t db = c_reader( bytes ).parse( );
        r.track_db = &db;
        r.unrecreated = [ & ]( std::uint32_t, const char* ) { ++unrecreated; };
        std::vector< std::string > got;
        r.hooks.hud = [ & ]( int p, race_hud_verb_t verb, float dt ) {
            if ( !present[ p ] )
                return;
            if ( verb == race_hud_verb_t::initialize && !live[ p ] )
            {
                live[ p ] = 1;
                got.push_back( "init " + std::to_string( p ) );
            }
            else if ( verb == race_hud_verb_t::release && live[ p ] )
            {
                live[ p ] = 0;
                got.push_back( "release " + std::to_string( p ) );
            }
            else if ( verb == race_hud_verb_t::tick && hud[ p ] )
            {
                unsigned b;
                std::memcpy( &b, &dt, 4 );
                std::snprintf( line, sizeof( line ), "tick %d %08X", p, b );
                got.push_back( line );
            }
        };
        r.hooks.broadcast = [ & ]( const char* name, bool value ) { got.push_back( std::string( "broadcast " ) + name + ( value ? " 1" : " 0" ) ); };
        r.hooks.platform = [ & ]( const std::string& call ) { got.push_back( call ); };
        const auto same_calls = [ & ]( unsigned count ) {
            bool same = got.size( ) == count;
            for ( unsigned c = 0; c < count; ++c )
                same = text( ) == ( c < got.size( ) ? got[ c ] : std::string( ) ) && same;
            return same;
        };
        r.fsm.m_current = 0;
        race_rules_start( r );
        const unsigned start = next( );
        calls += static_cast< int >( start );
        if ( !same_calls( start ) )
            fail( "vector " + std::to_string( v ) + ": the start's calls differ" );
        const unsigned nf = next( );
        for ( unsigned f = 0; f < nf; ++f )
        {
            got.clear( );
            const unsigned lights = next( ), finish = next( ), dtw = next( ), want_ret = next( );
            const int want_cur = static_cast< int >( next( ) ), want_prev = static_cast< int >( next( ) );
            const unsigned want_time = next( ), nc = next( );
            if ( lights )
                race_rules_on_start_lights_out( r );
            for ( std::size_t k = 0; k < images.size( ); ++k )
                if ( finish & ( 1u << k ) )
                    images[ k ][ 0x32F ] = 1;
            float dt;
            std::memcpy( &dt, &dtw, 4 );
            const bool ret = race_rules_tick( r, dt );
            unsigned time;
            std::memcpy( &time, &r.fsm.m_time, 4 );
            bool same = static_cast< unsigned >( ret ) == want_ret && r.fsm.m_current == want_cur && r.fsm.m_previous == want_prev && time == want_time &&
                        r.fsm.m_conditions.size( ) == nc;
            for ( unsigned c = 0; c < nc; ++c )
            {
                const unsigned value = next( ), pulse = next( );
                same = same && c < r.fsm.m_conditions.size( ) && r.fsm.m_conditions[ c ].m_value == value && r.fsm.m_conditions[ c ].m_pulse == pulse;
            }
            for ( std::size_t k = 0; k < images.size( ); ++k )
            {
                const std::uint8_t* m = images[ k ].data( );
                unsigned row[ 4 ];
                std::memcpy( &row[ 0 ], m + 0x308, 4 );
                std::memcpy( &row[ 1 ], m + 0x4B0, 4 );
                std::memcpy( &row[ 2 ], m + 0x4B4, 4 );
                std::memcpy( &row[ 3 ], m + 0x4CC, 4 );
                for ( unsigned x : row )
                    same = next( ) == x && same;
            }
            const unsigned nc2 = next( );
            const std::size_t first = s;
            const bool calls_same = same_calls( nc2 );
            ++frames;
            calls += static_cast< int >( nc2 );
            if ( !same || !calls_same )
            {
                std::snprintf( line, sizeof( line ), "vector %u frame %u: ret %d state %d prev %d time 0x%08X calls %d%s, retail %u %d %d 0x%08X %u%s", v, f,
                               static_cast< int >( ret ), r.fsm.m_current, r.fsm.m_previous, time, static_cast< int >( got.size( ) ),
                               got.empty( ) ? "" : ( " first '" + got[ 0 ] + "'" ).c_str( ), want_ret, want_cur, want_prev, want_time, nc2,
                               nc2 ? ( std::string( " first '" ) + g_golden_ttf_text[ first ] + "'" ).c_str( ) : "" );
                fail( line );
            }
        }
    }
    if ( unrecreated )
        fail( std::to_string( unrecreated ) + " paths not recreated reached" );
    if ( failed == 0 )
        LOG_SUCCESS( "timetrial_rules (VuTimeTrialGame) match retail's ctor 0x48ED30 + slot 5 0x48F310 / slot 7 0x48F350 / 0x490540 and the 12 "
                     "handlers: %u vectors, %d frames, %d calls", nv, frames, calls );
    else
        LOG_ERROR( "timetrial game fsm FAILED %d checks", failed );
    return failed == 0;
}

// checkpoint_buoy_contact (core/world rules.hxx) against VuCheckpointBuoyEntity's
// contact callback 0x4350B0 run real (oracle case buoy_hit; the base 0x437F40
// and the broadcast 0x505690 intercepted): per contact whether the base ran
// first, whether a broadcast was made, on the event manager g 0x759028, its
// hash (0x505650's FNV-1a of the name), the params' two words and the size -
// one failure per differing contact.
inline bool verify_buoy_hit( )
{
    int failed = 0, broadcasts = 0;
    const std::uint32_t hash = boat_fnv_range( reinterpret_cast< const std::uint8_t* >( k_buoy_hit_event ),
                                               static_cast< std::uint32_t >( std::strlen( k_buoy_hit_event ) ), 0x811C9DC5u );
    for ( std::size_t k = 0; k < g_golden_buoy_hit_count; ++k )
    {
        const golden_buoy_hit_t& g = g_golden_buoy_hit[ k ];
        buoy_contact_t c;
        c.group = static_cast< std::uint16_t >( g.m_group );
        c.owner = g.m_owner != 0;
        c.owner_is_boat = g.m_rtti == 1 || g.m_rtti == 2;
        c.driver_vtable = g.m_vtable;
        c.index = g.m_index;
        int base = 0;
        std::uint8_t params[ 0x84 ] = { };
        std::int32_t size = 0;
        const int r = checkpoint_buoy_contact( c, [ & ]( ) { ++base; }, params, size );
        std::uint32_t p0 = 0, p1 = 0;
        std::memcpy( &p0, params, 4 );
        std::memcpy( &p1, params + 4, 4 );
        const bool same = r >= 0 && base == g.m_base && r == g.m_broadcast &&
                          ( r == 0 || ( g.m_manager == 1 && hash == g.m_hash && p0 == g.m_p0 && p1 == g.m_p1 && size == g.m_size ) );
        broadcasts += r == 1 ? 1 : 0;
        if ( !same && ++failed <= 10 )
            LOG_ERROR( "buoy hit: contact %d (group 0x%X owner %d rtti %d vtable 0x%X index %d): ours %d base %d hash 0x%X %08X %08X size %d, retail %d %d 0x%X %08X %08X %d",
                       static_cast< int >( k ), g.m_group, g.m_owner, g.m_rtti, g.m_vtable, g.m_index, r, base, hash, p0, p1, size,
                       g.m_broadcast, g.m_base, g.m_hash, g.m_p0, g.m_p1, g.m_size );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "checkpoint_buoy_contact matches VuCheckpointBuoyEntity's 0x4350B0: %d contacts, %d OnCheckPointBuoyHit broadcasts (hash, params, size)",
                     static_cast< int >( g_golden_buoy_hit_count ), broadcasts );
    else
        LOG_ERROR( "buoy hit FAILED %d contacts", failed );
    return failed == 0;
}
// tick_run / tick_register (core/world tick.hxx) when a handler registers
// another DURING its call, against retail's tick 0x4F72F0 with slot 8
// 0x4F7550 run from inside the handler (oracle case tick_same_pass): the
// handler loop reads the node's next AFTER the call and re-reads the list head
// each step (0x4F7415 / 0x4F741D), the phase loop likewise (0x4F7431 /
// 0x4F7433) - so a handler pushed on the back of the running phase, or of a
// later phase, runs in the SAME pass; one put in an earlier phase waits for
// the next tick. Per scenario three ticks' call order - one failure per tick.
struct tick_same_pass_probe_t
{
    tick_manager_t* tm = nullptr;
    std::vector< int >* calls = nullptr;
    const int* reg = nullptr;       // caller, new handler, phase
    bool* once = nullptr;
    int index = 0;
    std::vector< tick_same_pass_probe_t >* all = nullptr;
};

inline const char* tick_phase_name( const tick_manager_t& tm, int index )
{
    int k = 0;
    for ( const tick_phase_t& p : tm.phases )
        if ( k++ == index )
            return p.name.c_str( );
    return "";
}

inline void tick_same_pass_call( void* user, float )
{
    tick_same_pass_probe_t& p = *static_cast< tick_same_pass_probe_t* >( user );
    p.calls->push_back( p.index );
    if ( p.index == p.reg[ 0 ] && !*p.once )
    {
        *p.once = true;
        tick_same_pass_probe_t& n = ( *p.all )[ static_cast< std::size_t >( p.reg[ 1 ] ) ];
        tick_register( *p.tm, { &n, tick_same_pass_call, &n }, tick_phase_name( *p.tm, p.reg[ 2 ] ) );
    }
}

inline bool verify_tick_same_pass( )
{
    int failed = 0;
    for ( std::size_t v = 0; v < g_golden_tick_same_pass_count; ++v )
    {
        const golden_tick_same_pass_t& g = g_golden_tick_same_pass[ v ];
        tick_manager_t tm;
        tick_manager_init( tm );
        std::vector< int > calls;
        bool once = false;
        std::vector< tick_same_pass_probe_t > probes( 4 );
        for ( int h = 0; h < 4; ++h )
            probes[ static_cast< std::size_t >( h ) ] = { &tm, &calls, g.m_register, &once, h, &probes };
        for ( int k = 0; k < 2; ++k )
            if ( g.m_initial[ 2 * k ] >= 0 )
            {
                tick_same_pass_probe_t& p = probes[ static_cast< std::size_t >( g.m_initial[ 2 * k ] ) ];
                tick_register( tm, { &p, tick_same_pass_call, &p }, tick_phase_name( tm, g.m_initial[ 2 * k + 1 ] ) );
            }
        for ( int t = 0; t < 3; ++t )
        {
            calls.clear( );
            tick_run( tm, 1000.0 + t / 60.0 );
            bool same = calls.size( ) == static_cast< std::size_t >( g.m_count[ t ] );
            for ( std::size_t c = 0; same && c < calls.size( ); ++c )
                same = calls[ c ] == g_golden_tick_same_pass_calls[ g.m_first[ t ] + static_cast< int >( c ) ];
            if ( !same && ++failed <= 10 )
                LOG_ERROR( "tick same pass: scenario %d tick %d: %d calls, retail %d", static_cast< int >( v ), t, static_cast< int >( calls.size( ) ), g.m_count[ t ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "tick_run matches retail's 0x4F72F0 with a registration made inside a handler (%d scenarios x 3 ticks: same / later phase in the same pass, "
                     "an earlier phase on the next tick)", static_cast< int >( g_golden_tick_same_pass_count ) );
    else
        LOG_ERROR( "tick same pass FAILED %d ticks", failed );
    return failed == 0;
}
// game_mode_defs / game_mode_construct (core/world rules.hxx) against the
// Slalom / TimeTrial / Ghost ctors 0x48BEC0 / 0x48ED30 / 0x484A20 run whole
// (oracle case game_ctors): per game the vtable, RTTI name, size, the FSM's
// offset, the three HUD loads and their words; the FSM's states (name, flag,
// FNV hash, the four methods and their VuMethod vtables), transitions,
// conditions, current state and time; the events (VuGame's two, then the
// game's: name, VuMethod1 vtable, method); VuGame's +0x0C / +0x67C machines'
// state counts and current state - one failure per differing item.
inline bool verify_game_ctors( )
{
    int failed = 0, states = 0, events = 0;
    const auto fail = [ & ]( const std::string& what ) {
        if ( ++failed <= 10 )
            LOG_ERROR( "game ctors: %s", what.c_str( ) );
    };
    const std::vector< game_mode_def_t >& defs = game_mode_defs( );
    if ( defs.size( ) != g_golden_gc_games_count )
        fail( "game count differs" );
    for ( std::size_t k = 0; k < g_golden_gc_games_count && k < defs.size( ); ++k )
    {
        const golden_gc_game_t& g = g_golden_gc_games[ k ];
        const game_mode_def_t& d = defs[ k ];
        const std::string who = g.m_ctor;
        if ( who != d.ctor || std::string( g.m_rtti ) != d.rtti || g.m_vtable != d.vtable || g.m_size != d.size || g.m_fsm_at != d.fsm_at )
            fail( who + ": the class / vtable / size / FSM offset differ" );
        for ( int h = 0; h < 3; ++h )
            if ( std::string( g.m_loaded[ h ] ) != d.huds[ h ] || g.m_load_at[ h ] != d.hud_at[ h ] )
                fail( who + ": HUD load " + std::to_string( h ) + " differs" );
        c_fsm f;
        game_mode_construct( f, d, nullptr );
        if ( f.m_states.size( ) != static_cast< std::size_t >( g.m_states ) )
            fail( who + ": state count differs" );
        for ( int s = 0; s < g.m_states && static_cast< std::size_t >( s ) < f.m_states.size( ); ++s )
        {
            const golden_rgf_state_t& gs = g_golden_gc_states[ g.m_state + s ];
            const fsm_state_t& st = *f.m_states[ static_cast< std::size_t >( s ) ];
            bool same = st.m_name == gs.m_name && static_cast< int >( st.m_flag ) == gs.m_flag && st.m_hash == gs.m_hash &&
                        st.m_transitions.size( ) == static_cast< std::size_t >( gs.m_trs );
            for ( int m = 0; m < 4; ++m )
            {
                const std::uint32_t vt = st.m_address[ m ] == 0 ? 0u : m == 2 ? d.method1_vtable : d.method0_vtable;
                same = same && st.m_address[ m ] == gs.m_method[ m ] && gs.m_vtable[ m ] == vt;
            }
            for ( int t = 0; same && t < gs.m_trs; ++t )
                same = st.m_transitions[ static_cast< std::size_t >( t ) ].first == g_golden_gc_transitions[ gs.m_tr + t ][ 0 ] &&
                       st.m_transitions[ static_cast< std::size_t >( t ) ].second == g_golden_gc_transitions[ gs.m_tr + t ][ 1 ];
            if ( !same )
                fail( who + ": state " + gs.m_name + " differs" );
            ++states;
        }
        if ( f.m_conditions.size( ) != static_cast< std::size_t >( g.m_conditions ) )
            fail( who + ": condition count differs" );
        for ( int c = 0; c < g.m_conditions && static_cast< std::size_t >( c ) < f.m_conditions.size( ); ++c )
        {
            const golden_rgf_condition_t& gc = g_golden_gc_conditions[ g.m_condition + c ];
            const auto& fc = f.m_conditions[ static_cast< std::size_t >( c ) ];
            if ( fc.m_name != gc.m_name || fc.m_hash != gc.m_hash || fc.m_value != gc.m_value || fc.m_pulse != gc.m_pulse )
                fail( who + ": condition " + gc.m_name + " differs" );
        }
        unsigned time;
        std::memcpy( &time, &f.m_time, 4 );
        if ( f.m_current != g.m_current || time != g.m_time )
            fail( who + ": the current state / time after the ctor differ" );
        std::vector< std::pair< race_rules_event_t, std::uint32_t > > ev;
        for ( const race_rules_event_t& e : k_vugame_events )
            ev.push_back( { e, k_vugame_event_vtable } );
        for ( const race_rules_event_t& e : d.events )
            ev.push_back( { e, d.event_vtable } );
        if ( ev.size( ) != static_cast< std::size_t >( g.m_events ) )
            fail( who + ": event count differs" );
        for ( int e = 0; e < g.m_events && static_cast< std::size_t >( e ) < ev.size( ); ++e )
        {
            const golden_gc_event_t& ge = g_golden_gc_events[ g.m_event + e ];
            if ( std::strcmp( ev[ static_cast< std::size_t >( e ) ].first.name, ge.m_name ) != 0 || ev[ static_cast< std::size_t >( e ) ].first.method != ge.m_method ||
                 ev[ static_cast< std::size_t >( e ) ].second != ge.m_vtable )
                fail( who + ": event " + ge.m_name + " differs" );
            ++events;
        }
        race_rules_t probe;
        vugame_fsms_construct( probe );
        vugame_fsm_begin( probe.event );
        vugame_fsm_begin( probe.split );
        if ( probe.event.m_states.size( ) != static_cast< std::size_t >( g.m_states_0c ) || probe.split.m_states.size( ) != static_cast< std::size_t >( g.m_states_67c ) ||
             g.m_current_0c != 0 || g.m_current_67c != 0 )
            fail( who + ": VuGame's +0x0C / +0x67C machines differ" );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "game_mode_defs match the Slalom / TimeTrial / Ghost ctors 0x48BEC0 / 0x48ED30 / 0x484A20 run whole: %d games, %d states, %d events, "
                     "9 HUD loads", static_cast< int >( g_golden_gc_games_count ), states, events );
    else
        LOG_ERROR( "game ctors FAILED %d checks", failed );
    return failed == 0;
}
// vugame_results_* / vugame_awards_* (core/world rules.hxx) against VuGame's
// Results / Awards exit, tick and draw 0x484250 .. 0x484580 run real (oracle
// case vugame_handlers): per call the hooks reached in order (as text), the
// four +0x0C conditions' value / pulse after, and the screens' presence after
// - one failure per differing call.
inline bool verify_vugame_handlers( )
{
    int failed = 0;
    static const char* const k_names[ 4 ] = { "EventFinished", "OnUserNext", "NoResults", "AwardsDone" };
    for ( std::size_t k = 0; k < g_golden_vh_count; ++k )
    {
        const golden_vh_t& g = g_golden_vh[ k ];
        race_rules_t r;
        vugame_fsms_construct( r );
        bool shape = r.event.m_conditions.size( ) == 4;
        for ( std::size_t c = 0; shape && c < 4; ++c )
        {
            shape = r.event.m_conditions[ c ].m_name == k_names[ c ];
            r.event.m_conditions[ c ].m_value = static_cast< std::uint8_t >( g.m_before[ 2 * c ] );
            r.event.m_conditions[ c ].m_pulse = static_cast< std::uint8_t >( g.m_before[ 2 * c + 1 ] );
        }
        r.results_screen = g.m_screen != 0;
        r.award_screen = g.m_award != 0;
        std::vector< std::string > got;
        char text[ 64 ];
        r.screen_hooks.screen = [ & ]( int screen, int verb, float dt ) {
            const char* who = screen == 0 ? "screen" : "award";
            unsigned bits;
            std::memcpy( &bits, &dt, 4 );
            if ( verb == 0 )
                std::snprintf( text, sizeof( text ), screen == 0 ? "%s 0x8" : "%s 0x47F4C0", who );
            else if ( verb == 1 )
                std::snprintf( text, sizeof( text ), "%s dtor", who );
            else if ( verb == 2 )
                std::snprintf( text, sizeof( text ), "%s 0xC %08X", who, bits );
            else
                std::snprintf( text, sizeof( text ), "%s 0x10", who );
            got.push_back( text );
        };
        r.screen_hooks.json_type = [ & ]( const char* name ) { got.push_back( std::string( "json " ) + name ); return g.m_json; };
        r.screen_hooks.paused = [ & ] { return g.m_paused != 0; };
        r.screen_hooks.game_results_tick = [ & ]( float dt ) {
            unsigned bits;
            std::memcpy( &bits, &dt, 4 );
            std::snprintf( text, sizeof( text ), "game 0x30 %08X", bits );
            got.push_back( text );
        };
        r.screen_hooks.award_done = [ & ] { got.push_back( "award 0x47F720" ); return g.m_done != 0; };
        float dt;
        std::memcpy( &dt, &g.m_dt, 4 );
        switch ( g.m_fn )
        {
        case 0: vugame_results_exit( r ); break;
        case 1: vugame_results_tick( r, dt ); break;
        case 2: vugame_results_draw( r ); break;
        case 3: vugame_awards_exit( r ); break;
        case 4: vugame_awards_tick( r, dt ); break;
        default: vugame_awards_draw( r ); break;
        }
        bool same = shape && got.size( ) == static_cast< std::size_t >( g.m_calls ) && r.results_screen == ( g.m_screen_after != 0 ) &&
                    r.award_screen == ( g.m_award_after != 0 );
        for ( std::size_t c = 0; same && c < got.size( ); ++c )
            same = got[ c ] == g_golden_vh_calls[ g.m_call + static_cast< int >( c ) ];
        for ( std::size_t c = 0; same && c < 4; ++c )
            same = r.event.m_conditions[ c ].m_value == g.m_after[ 2 * c ] && r.event.m_conditions[ c ].m_pulse == g.m_after[ 2 * c + 1 ];
        if ( !same && ++failed <= 10 )
            LOG_ERROR( "vugame handlers: call %d (fn %d screen %d award %d paused %d json %d done %d): %d calls%s, retail %d%s", static_cast< int >( k ),
                       g.m_fn, g.m_screen, g.m_award, g.m_paused, g.m_json, g.m_done, static_cast< int >( got.size( ) ),
                       got.empty( ) ? "" : ( " first '" + got[ 0 ] + "'" ).c_str( ), g.m_calls,
                       g.m_calls ? ( std::string( " first '" ) + g_golden_vh_calls[ g.m_call ] + "'" ).c_str( ) : "" );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuGame's Results / Awards exit, tick and draw match retail's 0x484250 .. 0x484580: %d calls", static_cast< int >( g_golden_vh_count ) );
    else
        LOG_ERROR( "vugame handlers FAILED %d calls", failed );
    return failed == 0;
}
// vugame_split_expire / vugame_personal_best_enter (core/world rules.hxx)
// against the +0x67C machine's Intro / Split / Result ticks 0x483CB0 /
// 0x483D00 / 0x483E00 and PersonalBestConfig's enter 0x483C40 run real (oracle
// case split_handlers): per call the hooks reached, the five named conditions'
// value / pulse after, +0x6A4 and +0x6AC after - one failure per differing call.
inline bool verify_split_handlers( )
{
    int failed = 0;
    for ( std::size_t k = 0; k < g_golden_split_handlers_count; ++k )
    {
        const golden_split_handler_t& g = g_golden_split_handlers[ k ];
        race_rules_t r;
        vugame_fsms_construct( r );
        bool shape = true;
        for ( int c = 0; c < 5; ++c )
        {
            const int i = r.split.condition_index( g_golden_split_handler_names[ c ] );
            shape = shape && i >= 0;
            if ( i >= 0 )
            {
                r.split.m_conditions[ static_cast< std::size_t >( i ) ].m_value = static_cast< std::uint8_t >( g.m_before[ 2 * c ] );
                r.split.m_conditions[ static_cast< std::size_t >( i ) ].m_pulse = static_cast< std::uint8_t >( g.m_before[ 2 * c + 1 ] );
            }
        }
        std::memcpy( &r.split.m_time, &g.m_time, 4 );
        std::memcpy( &r.split_duration, &g.m_duration, 4 );
        r.split_kind = g.m_kind_before;
        std::vector< std::string > got;
        r.screen_hooks.personal_best = [ & ] { got.push_back( "game 0x38" ); return g.m_pb != 0; };
        r.screen_hooks.string_db = [ & ]( const char* key ) { got.push_back( std::string( "string " ) + key ); return std::string( g.m_text ); };
        if ( g.m_fn == 3 )
            vugame_personal_best_enter( r );
        else
            vugame_split_expire( r );
        bool same = shape && got.size( ) == static_cast< std::size_t >( g.m_calls ) && r.split_kind == g.m_kind && r.opponent == g.m_name;
        for ( std::size_t c = 0; same && c < got.size( ); ++c )
            same = got[ c ] == g_golden_split_handler_calls[ g.m_call + static_cast< int >( c ) ];
        for ( int c = 0; same && c < 5; ++c )
        {
            const auto& fc = r.split.m_conditions[ static_cast< std::size_t >( r.split.condition_index( g_golden_split_handler_names[ c ] ) ) ];
            same = fc.m_value == g.m_after[ 2 * c ] && fc.m_pulse == g.m_after[ 2 * c + 1 ];
        }
        if ( !same && ++failed <= 10 )
            LOG_ERROR( "split handlers: call %d (fn %d time 0x%08X duration 0x%08X pb %d): %d calls, kind %d '%s'; retail %d, %d '%s'", static_cast< int >( k ), g.m_fn,
                       g.m_time, g.m_duration, g.m_pb, static_cast< int >( got.size( ) ), r.split_kind, r.opponent.c_str( ), g.m_calls, g.m_kind, g.m_name );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuGame's +0x67C Intro / Split / Result ticks and PersonalBestConfig enter match retail's 0x483CB0 / 0x483D00 / 0x483E00 / 0x483C40: %d calls",
                     static_cast< int >( g_golden_split_handlers_count ) );
    else
        LOG_ERROR( "split handlers FAILED %d calls", failed );
    return failed == 0;
}