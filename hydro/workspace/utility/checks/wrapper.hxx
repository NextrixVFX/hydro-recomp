#pragma once

// VuDynamicsImpl's frame and small slots (engine dynamics/wrapper.hxx) against retail (case dynamics_impl: a fake
// object with retail's vtable 0x74C044 - its own slot 25 0x4F8900 real -, every vtable / global call a logged stub,
// retail's std::list code on the harness heap). The ops are replayed in order through the C++ functions with hooks that
// log the same events; compared per op: the events (code and words, in order), the +0xC .. +0xF bytes, +0x10, +0x14, the
// three lists and their sizes, +0x20 .. +0x3C, g_vudynamicsimpl, slot 22's float / slot 23's pointer.
// Then the world agent's race_dynamics_kick / race_dynamics_sync (world/dynamics.hxx: DynamicsKick 0x4F8A90 + the thread
// 0x4F8950, DynamicsSync 0x4F8A20 - the step run at the Kick) against the same kick / thread / sync triples: the step
// taken or not and its three arguments, the motion-state sync taken or not.

#include <workspace/core/dynamics/wrapper.hxx>
#include <workspace/core/world/collide.hxx>
#include <workspace/core/world/dynamics.hxx>

#include <deque>
#include <vector>

struct dyw_log_t
{
    std::vector< std::vector< unsigned > > events;
    std::deque< unsigned > waits;
    float clock = 0.0f;
    unsigned record[ 2 ] = { 0x3FC00000u, 0x12345678u };   // the case's contact record
};

inline unsigned dyw_bits( float f )
{
    unsigned u;
    std::memcpy( &u, &f, 4 );
    return u;
}

inline float dyw_float( unsigned u )
{
    float f;
    std::memcpy( &f, &u, 4 );
    return f;
}

inline void* dyw_token( unsigned id )
{
    return reinterpret_cast< void* >( static_cast< std::uintptr_t >( id ) );
}

inline vu_dynamics_wrapper_hooks_t dyw_hooks( dyw_log_t& l )
{
    vu_dynamics_wrapper_hooks_t h{};
    h.user = &l;
    h.clock = []( void* u ) { return static_cast< double >( static_cast< dyw_log_t* >( u )->clock ); };
    h.priority = []( void* u, std::int32_t p ) { static_cast< dyw_log_t* >( u )->events.push_back( { 3u, static_cast< unsigned >( p ) } ); };
    h.close = []( void* u, std::uint32_t x ) { static_cast< dyw_log_t* >( u )->events.push_back( { 5u, x } ); };
    h.end_thread = []( void* u ) { static_cast< dyw_log_t* >( u )->events.push_back( { 4u } ); };
    h.signal = []( void* u, std::uint32_t x ) { static_cast< dyw_log_t* >( u )->events.push_back( { 2u, x } ); };
    h.wait = []( void* u, std::uint32_t x, std::int32_t t ) {
        dyw_log_t& l = *static_cast< dyw_log_t* >( u );
        l.events.push_back( { 1u, x, static_cast< unsigned >( t ) } );
        unsigned a = 0;
        if ( !l.waits.empty( ) )
        {
            a = l.waits.front( );
            l.waits.pop_front( );
        }
        return ( a & 0xFFu ) != 0u;
    };
    h.step = []( void* u, float dt, std::int32_t m, float f ) {
        static_cast< dyw_log_t* >( u )->events.push_back( { 10u, dyw_bits( dt ), static_cast< unsigned >( m ), dyw_bits( f ) } );
    };
    h.add_constraint = []( void* u, std::uint32_t c, std::uint32_t d ) { static_cast< dyw_log_t* >( u )->events.push_back( { 11u, c, d } ); };
    h.remove_constraint = []( void* u, std::uint32_t c ) { static_cast< dyw_log_t* >( u )->events.push_back( { 12u, c } ); };
    h.sync_motion = []( void* u ) { static_cast< dyw_log_t* >( u )->events.push_back( { 13u } ); };
    h.debug_mode = []( void* u, std::uint32_t m ) { static_cast< dyw_log_t* >( u )->events.push_back( { 20u, m } ); };
    h.contact_slot0 = []( void* u ) { static_cast< dyw_log_t* >( u )->events.push_back( { 21u } ); };
    h.contact_slot1 = []( void* u ) { static_cast< dyw_log_t* >( u )->events.push_back( { 22u } ); };
    h.contact_record = []( void* u, std::uint32_t k ) -> const std::uint32_t* {
        dyw_log_t& l = *static_cast< dyw_log_t* >( u );
        l.events.push_back( { 23u, k } );
        return l.record;
    };
    h.query = []( void* u, std::uint32_t a ) { static_cast< dyw_log_t* >( u )->events.push_back( { 24u, a } ); };
    h.callback0 = []( void* u, void* c, float dt, std::uint32_t f ) {
        static_cast< dyw_log_t* >( u )->events.push_back( { 30u, static_cast< unsigned >( reinterpret_cast< std::uintptr_t >( c ) ), dyw_bits( dt ), f } );
    };
    h.callback1 = []( void* u, void* c, float dt ) {
        static_cast< dyw_log_t* >( u )->events.push_back( { 31u, static_cast< unsigned >( reinterpret_cast< std::uintptr_t >( c ) ), dyw_bits( dt ) } );
    };
    h.unregister = []( void* u ) { static_cast< dyw_log_t* >( u )->events.push_back( { 25u, 1u } ); };
    h.dev_unregister = []( void* u ) { static_cast< dyw_log_t* >( u )->events.push_back( { 26u, 1u } ); };
    h.destroy = []( void* u, int offset ) {
        // the case's object ids: the world 1, the drawer 2, +0x20 .. +0x30 4 .. 8
        const unsigned id = offset == 0x34 ? 1u : offset == 0x3C ? 2u : static_cast< unsigned >( 4 + ( offset - 0x20 ) / 4 );
        static_cast< dyw_log_t* >( u )->events.push_back( { 14u, id, 1u } );
    };
    h.contact_destroy = []( void* u ) { static_cast< dyw_log_t* >( u )->events.push_back( { 27u } ); };
    return h;
}

inline bool verify_dynamics_impl( )
{
    dyw_log_t log;
    vu_dynamics_wrapper_t w{};
    w.hooks = dyw_hooks( log );
    vu_dynamics_wrapper_bind( w );
    // the case's inputs: the thread / event handles, the drawer's three entries
    w.thread = 0x3333u;
    w.go = 0x1111u;
    w.base.thread = 0x2222u;
    w.debug = { { 4u, true }, { 8u, false }, { 0x10u, true } };
    vu_dynamics_wrapper_t* global = &w;
    // the world agent's race_dynamics_t, driven by the same kick / thread / sync triples
    race_dynamics_t race{};
    race_dynamics_init( race, nullptr );
    int race_bad = 0, race_steps = 0, race_syncs = 0;
    float race_dt = 0.0f;
    bool race_kicked = false;
    const unsigned* s = g_golden_dyw_words;
    const unsigned n = *s++;
    int failed = 0, compared = 0;
    for ( unsigned c = 0; c < n; ++c )
    {
        const unsigned fn = *s++;
        const unsigned na = *s++;
        const unsigned* a = s;
        s += na;
        w.async = static_cast< std::uint8_t >( s[ 0 ] );
        w.base.pending = static_cast< std::uint8_t >( s[ 1 ] );
        w.quit = static_cast< std::uint8_t >( s[ 2 ] );
        s += 4;
        const unsigned nw = *s++;
        log.waits.assign( s, s + nw );
        s += nw;
        log.clock = dyw_float( *s++ );
        log.events.clear( );
        global = &w;   // the case pokes g_vudynamicsimpl = the object before every call
        unsigned eax = 0, st0 = 0;
        switch ( fn )
        {
        case 0x4F7B70u: vu_dynamics_add_listener( w, dyw_token( a[ 0 ] ) ); break;
        case 0x4F7BE0u: vu_dynamics_remove_listener( w, dyw_token( a[ 0 ] ) ); break;
        case 0x4F7C00u: vu_dynamics_add_step_callback( w.base, dyw_token( a[ 0 ] ) ); break;   // manager.hxx slot 11
        case 0x4F8BC0u: vu_dynamics_substep_begin( w, dyw_float( a[ 0 ] ), a[ 1 ] ); break;
        case 0x4F8C00u: vu_dynamics_substep( w, dyw_float( a[ 0 ] ) ); break;
        case 0x4F8690u: vu_dynamics_add_contact_listener( w, dyw_token( a[ 0 ] ) ); break;
        case 0x4F86E0u: vu_dynamics_remove_contact_listener( w, dyw_token( a[ 0 ] ) ); break;
        case 0x4F8840u: vu_dynamics_add_constraint( w, a[ 0 ], a[ 1 ] ); break;
        case 0x4F8860u: vu_dynamics_remove_constraint( w, a[ 0 ] ); break;
        case 0x4F8880u: vu_dynamics_contact_slot0( w ); break;
        case 0x4F8890u: vu_dynamics_contact_slot1( w ); break;
        case 0x4F88B0u: st0 = dyw_bits( vu_dynamics_contact_value( w, a[ 0 ] ) ); break;
        case 0x4F88D0u: eax = vu_dynamics_contact_tail( w, a[ 0 ] ) == log.record + 1 ? 1u : 0u; break;
        case 0x4F8920u: vu_dynamics_query( w, a[ 0 ] ); break;
        case 0x4F8A90u:
            vu_dynamics_kick( w, dyw_float( a[ 0 ] ) );
            race_steps = race.steps;
            race_dynamics_kick( race, dyw_float( a[ 0 ] ) );
            race_dt = dyw_float( a[ 0 ] );
            race_kicked = true;
            break;
        case 0x4F8950u: vu_dynamics_thread( w ); break;
        case 0x4F8A20u:
            vu_dynamics_frame_sync( w, dyw_float( a[ 0 ] ) );
            race_syncs = race.syncs;
            race_dynamics_sync( race, dyw_float( a[ 0 ] ) );
            break;
        case 0x4F7B50u: vu_dynamics_clear_global( global ); break;
        case 0x4F8520u: vu_dynamics_release( w ); break;
        default: ++failed; LOG_ERROR( "dynamics_impl: op 0x%X not dispatched", fn ); break;
        }
        int bad = 0;
        const unsigned ne = *s++;
        bad += ne != log.events.size( );
        bool stepped = false, synced = false;
        const unsigned* step_args = nullptr;
        for ( unsigned i = 0; i < ne; ++i )
        {
            const unsigned len = *s++;
            const std::vector< unsigned > e( s, s + len );
            if ( e[ 0 ] == 10u )
            {
                stepped = true;
                step_args = s + 1;
            }
            synced = synced || e[ 0 ] == 13u;
            ++compared;
            bad += i >= log.events.size( ) || log.events[ i ] != e;
            s += len;
        }
        const unsigned bytes[ 4 ] = { w.async, w.base.pending, w.quit, 0u };
        for ( int k = 0; k < 4; ++k )
            bad += *s++ != bytes[ k ];
        bad += *s++ != dyw_bits( w.dt );
        bad += *s++ != dyw_bits( w.interval );
        auto list = [ & ]( const std::vector< void* >& v ) {
            const unsigned m = *s++;
            int b = m != v.size( );
            for ( unsigned i = 0; i < m; ++i, ++s )
                b += i >= v.size( ) || reinterpret_cast< std::uintptr_t >( v[ i ] ) != *s;
            b += *s++ != v.size( );
            return b;
        };
        bad += list( w.base.listeners );
        bad += list( w.base.step_callbacks );
        bad += list( w.contact_listeners );
        for ( int k = 0; k < 8; ++k )
            bad += *s++ != ( w.objects[ k ] ? 1u : 0u );
        bad += *s++ != ( global ? 1u : 0u );
        bad += *s++ != eax;
        bad += *s++ != st0;
        compared += 22;
        // the world agent's frame: the kick's step (taken at the Kick) decided as the thread decides it
        if ( fn == 0x4F8950u && race_kicked && w.quit == 0 )
        {
            const bool took = race.steps != race_steps;
            race_bad += took != stepped;
            if ( stepped && step_args )
                race_bad += step_args[ 0 ] != dyw_bits( race.dt ) || step_args[ 1 ] != static_cast< unsigned >( race.max_sub_steps ) ||
                            step_args[ 2 ] != dyw_bits( race.fixed_time_step ) || dyw_bits( race_dt ) != step_args[ 0 ];
            race_kicked = false;
        }
        if ( fn == 0x4F8A20u )
            race_bad += ( race.syncs != race_syncs ) != synced;
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "dynamics_impl: op %u (0x%X): %d differences", c, fn, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuDynamicsImpl's frame (0x4F8A90 / 0x4F8950 / 0x4F8A20 / 0x4F8BC0 / 0x4F8C00) and slots 3 4 9 10 11 13 14 17 18 19 "
                     "20 22 23 26 match retail: %u ops, %d values", n, compared );
    else
        LOG_ERROR( "dynamics_impl FAILED %d of %u", failed, n );
    if ( race_bad == 0 )
        LOG_SUCCESS( "world/dynamics.hxx race_dynamics_kick / race_dynamics_sync take the step (dt, 6, 1/60) and the motion-state sync "
                     "exactly when retail's Kick + thread / Sync do (dt 1/60, 0, -1, NaN, 1e-30, 0.05, -0)" );
    else
        LOG_ERROR( "race_dynamics frame FAILED %d", race_bad );
    return failed == 0 && race_bad == 0;
}

// btRigidBody::checkCollideWithOverride 0x50E7B0 and VuCollisionManager's slots 4 / 3 (engine dynamics/wrapper.hxx
// rigid_check_collide_with, vu_collision_manager_clear_global / _release) against retail (case rigid_collide).
inline bool verify_rigid_collide( )
{
    const unsigned* s = g_golden_rgc_words;
    const unsigned n = *s++;
    int bad = 0;
    const int bodies[ 3 ] = { 0, 0, 0 };
    for ( unsigned c = 0; c < n; ++c )
    {
        const unsigned type = *s++, count = *s++, nr = *s++;
        std::vector< rigid_constraint_ref_t > refs;
        for ( unsigned i = 0; i < nr; ++i, s += 2 )
            refs.push_back( { &bodies[ s[ 0 ] ], &bodies[ s[ 1 ] ] } );
        const bool got = rigid_check_collide_with( type, &bodies[ 1 ], refs.data( ), static_cast< std::int32_t >( count ) );
        bad += ( got ? 1u : 0u ) != *s++;
    }
    int manager = 0;
    int* global = &manager;
    vu_collision_manager_clear_global( global );
    bad += ( global ? 1u : 0u ) != *s++;
    struct log_t
    {
        std::vector< unsigned > words;
        const void* manager;
    } log{ {}, &manager };
    vu_collision_manager_hooks_t h{};
    h.user = &log;
    h.remove_contact_listener = []( void* u, const void* l ) {
        log_t& x = *static_cast< log_t* >( u );
        x.words.insert( x.words.end( ), { 1u, l == static_cast< const unsigned char* >( x.manager ) + 4 ? 1u : 0u } );
    };
    h.asset_release = []( void* u, std::uint32_t a ) { static_cast< log_t* >( u )->words.insert( static_cast< log_t* >( u )->words.end( ), { 2u, a } ); };
    h.unregister = []( void* u, const void* m ) {
        log_t& x = *static_cast< log_t* >( u );
        x.words.insert( x.words.end( ), { 3u, m == x.manager ? 2u : 0u } );
    };
    vu_collision_manager_release( &manager, 0xABCDu, h );   // the case's +0x10
    const unsigned ne = *s++;
    bad += log.words != std::vector< unsigned >( s, s + 2 * ne );
    if ( bad == 0 )
        LOG_SUCCESS( "rigid_check_collide_with matches btRigidBody::checkCollideWithOverride 0x50E7B0 on %u vectors; VuCollisionManager's "
                     "0x4ACD20 / 0x4AD270 match", n );
    else
        LOG_ERROR( "rigid_collide FAILED %d", bad );
    return bad == 0;
}
// VuSimpleStridingMesh's lock 0x527CD0 / slot 6 0x527D40 (engine dynamics/wrapper.hxx) against retail (case simple_mesh).
inline bool verify_simple_mesh( )
{
    const unsigned* s = g_golden_smh_words;
    const unsigned n = *s++;
    int bad = 0;
    const int vdata = 0, idata = 0;
    for ( unsigned c = 0; c < n; ++c, s += 11 )
    {
        const simple_mesh_lock_t l = simple_striding_mesh_lock( &vdata, static_cast< std::int32_t >( s[ 0 ] ), &idata, static_cast< std::int32_t >( s[ 1 ] ) );
        const unsigned got[ 8 ] = { l.vertices == &vdata ? 1u : 0u, static_cast< unsigned >( l.vertex_count ), static_cast< unsigned >( l.vertex_type ),
                                    static_cast< unsigned >( l.vertex_stride ), l.indices == &idata ? 1u : 0u, static_cast< unsigned >( l.index_stride ),
                                    static_cast< unsigned >( l.faces ), static_cast< unsigned >( l.index_type ) };
        bad += std::memcmp( got, s + 2, sizeof( got ) ) != 0;
        bad += ( simple_striding_mesh_parts( static_cast< std::int32_t >( s[ 1 ] ) ) ? 1u : 0u ) != s[ 10 ];
    }
    if ( bad == 0 )
        LOG_SUCCESS( "simple_striding_mesh_lock / _parts match VuSimpleStridingMesh 0x527CD0 / 0x527D40 on %u meshes", n );
    else
        LOG_ERROR( "simple_mesh FAILED %d", bad );
    return bad == 0;
}
// VuCollisionManager's contact-added listener 0x4AD540 (world/collide.hxx collision_contact_added, with boat/events.hxx's
// 0x4AD340 and vu_array_reserve 0x4AE490) against retail (case collision_contact): one manager across the contacts, the
// bodies' images rebuilt per contact from the golden inputs; per contact the count, capacity and last record, then every
// record.
inline bool verify_collision_contact( )
{
    const unsigned* s = g_golden_ccn_words;
    std::vector< std::vector< std::uint8_t > > bodies( 4, std::vector< std::uint8_t >( 0x400, 0 ) ), owners( 4, std::vector< std::uint8_t >( 0x500, 0 ) );
    collision_manager_t m;
    const unsigned ns = *s++;
    m.scrapes.data.resize( ns );
    m.scrapes.count = m.scrapes.capacity = static_cast< std::int32_t >( ns );
    for ( unsigned i = 0; i < ns; ++i, s += 4 )
        m.scrapes.data[ i ] = { bodies[ s[ 0 ] ].data( ), bodies[ s[ 1 ] ].data( ), static_cast< std::int32_t >( s[ 2 ] ), static_cast< std::int32_t >( s[ 3 ] ), {} };
    m.impacts.data.resize( 2 );
    m.impacts.capacity = 2;
    const unsigned n = *s++;
    int bad = 0;
    for ( unsigned c = 0; c < n; ++c )
    {
        const unsigned ia = *s++, ib = *s++;
        for ( unsigned side = 0; side < 2; ++side, s += 12 )
        {
            std::uint8_t* b = bodies[ side ? ib : ia ].data( );
            std::uint8_t* o = owners[ side ? ib : ia ].data( );
            const std::uint16_t group = static_cast< std::uint16_t >( s[ 1 ] );
            std::memcpy( b + 0xD8, &s[ 0 ], 4 );
            std::memcpy( b + 0x2CA, &group, 2 );
            o[ 0x467 ] = static_cast< std::uint8_t >( s[ 2 ] );
            std::memcpy( b + 0x40, s + 3, 12 );
            std::memcpy( b + 0x140, s + 6, 12 );
            std::memcpy( b + 0x150, s + 9, 12 );
        }
        float point[ 4 ], normal[ 3 ];
        std::memcpy( point, s, 16 );
        std::memcpy( normal, s + 4, 12 );
        s += 7;
        const unsigned pa = *s++, pb = *s++;
        collision_contact_added( m, bodies[ ia ].data( ), bodies[ ib ].data( ), owners[ ia ].data( ), owners[ ib ].data( ), point, normal,
                                 static_cast< std::uint8_t >( pa ), static_cast< std::uint8_t >( pb ) );
        bad += static_cast< unsigned >( m.impacts.count ) != *s++;
        bad += static_cast< unsigned >( m.impacts.capacity ) != *s++;
        unsigned last[ 7 ] = {};
        if ( m.impacts.count > 0 )
            std::memcpy( last, &m.impacts.data[ static_cast< std::size_t >( m.impacts.count - 1 ) ], 28 );
        bad += std::memcmp( last, s, 28 ) != 0;
        s += 7;
    }
    const unsigned nf = *s++;
    bad += nf != 7u * static_cast< unsigned >( m.impacts.count );
    if ( nf == 7u * static_cast< unsigned >( m.impacts.count ) )
        bad += std::memcmp( m.impacts.data.data( ), s, 4u * nf ) != 0;
    if ( bad == 0 )
        LOG_SUCCESS( "collision_contact_added matches VuCollisionManager's listener 0x4AD540 (0x4AD340, 0x4AE490) on %u contacts: %d impact "
                     "records, capacity %d", n, m.impacts.count, m.impacts.capacity );
    else
        LOG_ERROR( "collision_contact FAILED %d", bad );
    return bad == 0;
}