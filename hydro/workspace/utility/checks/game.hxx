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