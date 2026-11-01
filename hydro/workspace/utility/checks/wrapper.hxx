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
// VuCollisionManager's impact tick 0x4ADA10 (world/collide.hxx collision_impact_tick on pfxmgr.hxx's manager and a pfx_heap_t
// holding the case's fake instances) against retail (case collision_impact): per tick the hook log (audio, creates, the
// instance slots) and the count; at the end every process's +0x1C and the manager's entries (flags, serial).
struct cim_log_t
{
    std::vector< unsigned > log;   // length-prefixed entries
    unsigned entries = 0;
    std::deque< unsigned > event, param, create;
    const collision_tables_t* tables = nullptr;
    void add( std::initializer_list< unsigned > e )
    {
        log.push_back( static_cast< unsigned >( e.size( ) ) );
        log.insert( log.end( ), e );
        ++entries;
    }
};

inline unsigned cim_cell( const vu_array_t< collision_name_t >& names, const char* s )
{
    for ( std::size_t c = 0; c < names.data.size( ); ++c )
        if ( names.data[ c ].data( ) == s )
            return static_cast< unsigned >( c );
    return 0xFFFFu;
}

inline bool verify_collision_impact( )
{
    constexpr std::uint32_t arg = 0x30000000u, block0 = arg + 0x1100u;
    const unsigned* s = g_golden_cim_words;
    collision_tables_t t;
    t.types = static_cast< std::int32_t >( *s++ );
    const unsigned nc = *s++;
    for ( unsigned c = 0; c < nc; ++c, s += 2 )
    {
        const std::string a = s[ 0 ] ? "sfx_" + std::to_string( c ) : "", b = s[ 1 ] ? "Generic/Impact/p" + std::to_string( c ) : "";
        t.impact_sfx.data.emplace_back( );
        t.impact_pfx.data.emplace_back( );
        std::memcpy( t.impact_sfx.data.back( ).data( ), a.c_str( ), a.size( ) + 1 );
        std::memcpy( t.impact_pfx.data.back( ).data( ), b.c_str( ), b.size( ) + 1 );
    }
    pfx_heap_t heap;
    heap.add( arg + 0x1000u, 0xE000u );
    const unsigned ni = *s++;
    for ( unsigned i = 0; i < ni; ++i, s += 2 )
        heap[ arg + s[ 0 ] ] = s[ 1 ];
    const unsigned np = *s++;
    const unsigned* procs = s;
    s += np;
    cim_log_t L;
    L.tables = &t;
    pfx_manager_ops_t ops;
    ops.user = &L;
    ops.create = []( void* u, const char* path ) {
        cim_log_t& l = *static_cast< cim_log_t* >( u );
        const std::uint32_t a = l.create.front( );
        l.create.pop_front( );
        l.add( { 6u, cim_cell( l.tables->impact_pfx, path ), a ? ( a - block0 ) / 0x280u : 0xFFFFFFFFu } );
        return a;
    };
    pfx_manager_t pm;
    pfx_manager_init( pm, ops );
    collision_impact_hooks_t k;
    k.user = &L;
    k.get_event = []( void* u, const char* name, std::uint32_t& ev ) {
        cim_log_t& l = *static_cast< cim_log_t* >( u );
        const unsigned ok = l.event.front( );
        l.event.pop_front( );
        ev = 0xE000u + l.entries;
        l.add( { 1u, cim_cell( l.tables->impact_sfx, name ), 1u, ok } );
        return ok != 0u;
    };
    k.set_position = []( void* u, std::uint32_t ev, const float* p ) {
        unsigned w[ 3 ];
        std::memcpy( w, p, 12 );
        static_cast< cim_log_t* >( u )->add( { 2u, ev, w[ 0 ], w[ 1 ], w[ 2 ], 0u, 0u } );
    };
    k.get_parameter = []( void* u, std::uint32_t ev, const char* name, std::uint32_t& p ) {
        cim_log_t& l = *static_cast< cim_log_t* >( u );
        const unsigned ok = l.param.front( );
        l.param.pop_front( );
        p = 0xF000u + l.entries;
        l.add( { 3u, ev, std::strcmp( name, "intensity" ) == 0 ? 1u : 0u, ok } );
        return ok != 0u;
    };
    k.set_value = []( void* u, std::uint32_t p, float v ) {
        unsigned w;
        std::memcpy( &w, &v, 4 );
        static_cast< cim_log_t* >( u )->add( { 4u, p, w } );
    };
    k.start = []( void* u, std::uint32_t ev ) { static_cast< cim_log_t* >( u )->add( { 5u, ev } ); };
    k.pfx_position = []( void* u, std::uint32_t i, const float* p ) {
        unsigned w[ 3 ];
        std::memcpy( w, p, 12 );
        static_cast< cim_log_t* >( u )->add( { 7u, ( i - block0 ) / 0x280u, w[ 0 ], w[ 1 ], w[ 2 ] } );
    };
    k.pfx_duration = []( void* u, std::uint32_t i ) { static_cast< cim_log_t* >( u )->add( { 8u, ( i - block0 ) / 0x280u } ); };
    k.pfx_start = []( void* u, std::uint32_t i ) { static_cast< cim_log_t* >( u )->add( { 9u, ( i - block0 ) / 0x280u } ); };
    k.pfx_walk = []( void* u, std::uint32_t i ) { static_cast< cim_log_t* >( u )->add( { 10u, ( i - block0 ) / 0x280u } ); };
    collision_manager_t m;
    m.impacts.data.resize( 8 );
    m.impacts.capacity = 8;
    const unsigned nt = *s++;
    int bad = 0;
    for ( unsigned tk = 0; tk < nt; ++tk )
    {
        const unsigned nr = *s++;
        for ( unsigned i = 0; i < nr; ++i, s += 7 )
            std::memcpy( &m.impacts.data[ i ], s, 28 );
        m.impacts.count = static_cast< std::int32_t >( nr );
        L.event.assign( s, s + nr );
        L.param.assign( s + nr, s + 2 * nr );
        L.create.clear( );
        for ( unsigned i = 0; i < nr; ++i )
            L.create.push_back( s[ 2 * nr + i ] == 0xFFFFFFFFu ? 0u : block0 + 0x280u * s[ 2 * nr + i ] );
        s += 3 * nr;
        L.log.clear( );
        L.entries = 0;
        collision_impact_tick( m, t, pm, ops, heap, k );
        const unsigned nl = *s++;
        bad += L.log != std::vector< unsigned >( s, s + nl );
        s += nl;
        bad += static_cast< unsigned >( m.impacts.count ) != *s++;
    }
    const unsigned nf = *s++;
    for ( unsigned i = 0; i < nf && i < np; ++i )
        bad += heap[ arg + procs[ i ] + 0x1Cu ] != s[ i ];
    s += nf;
    const unsigned ne = *s++;
    for ( unsigned i = 0; i < ne; ++i, s += 2 )
    {
        const pfx_manager_entry_t& x = pm.entries[ static_cast< std::size_t >( pm.table[ i ] ) ];
        bad += ( x.flags != s[ 0 ] ) + ( x.serial != s[ 1 ] );
    }
    if ( bad == 0 )
        LOG_SUCCESS( "collision_impact_tick matches VuCollisionManager's impact tick 0x4ADA10 on a retail VuPfxManager: %u ticks, %u fountain "
                     "processes, %u entries", nt, np, ne );
    else
        LOG_ERROR( "collision_impact FAILED %d", bad );
    return bad == 0;
}
// VuCollisionManager's tick 0x4AD970 / scrape tick 0x4ADC00 (removal 0x4AE4F0) / reset 0x4AD2B0 (world/collide.hxx
// collision_manager_tick / collision_scrape_tick / collision_manager_reset on pfxmgr.hxx's manager and a pfx_heap_t holding
// the case's fake instances) against retail (case collision_scrape): per op the hook log (groups, audio, creates, the
// instance slots, the dev page), the records (bodies, parts, +0x10, event, handle), count, capacity, the groups; at the end
// every process's +0x20 and the manager's entries.
inline bool verify_collision_scrape( )
{
    constexpr std::uint32_t arg = 0x30000000u, block0 = arg + 0x6400u;
    const unsigned* s = g_golden_csc_words;
    collision_tables_t t;
    t.types = static_cast< std::int32_t >( *s++ );
    const unsigned nc = *s++;
    for ( unsigned c = 0; c < nc; ++c, s += 2 )
    {
        const std::string a = s[ 0 ] ? "scrape_" + std::to_string( c ) : "", b = s[ 1 ] ? "Generic/Scrape/p" + std::to_string( c ) : "";
        t.scrape_sfx.data.emplace_back( );
        t.scrape_pfx.data.emplace_back( );
        std::memcpy( t.scrape_sfx.data.back( ).data( ), a.c_str( ), a.size( ) + 1 );
        std::memcpy( t.scrape_pfx.data.back( ).data( ), b.c_str( ), b.size( ) + 1 );
    }
    collision_mesh_asset_t meshes[ 2 ];
    for ( collision_mesh_asset_t& m : meshes )
    {
        m.triangles.bytes.resize( 64 );
        std::memcpy( m.triangles.bytes.data( ), s, 64 );
        m.triangles.size = m.triangles.capacity = 16;
        s += 16;
        m.materials.bytes.assign( 32 * 44, 0x77u );
        for ( int i = 0; i < 32; ++i )
            m.materials.bytes[ static_cast< std::size_t >( 44 * i + 40 ) ] = static_cast< std::uint8_t >( *s++ );
        m.materials.size = m.materials.capacity = 32;
    }
    pfx_heap_t heap;
    heap.add( arg + 0x6200u, 0x6000u );
    const unsigned ni = *s++;
    for ( unsigned i = 0; i < ni; ++i, s += 2 )
        heap[ arg + s[ 0 ] ] = s[ 1 ];
    const unsigned np = *s++;
    const unsigned* procs = s;
    s += np;
    struct log_t : cim_log_t
    {
        unsigned page = 0;
        int bad_group = 0;
    } L;
    L.tables = &t;
    pfx_manager_ops_t ops;
    ops.user = &L;
    ops.create = []( void* u, const char* path ) {
        log_t& l = *static_cast< log_t* >( u );
        const std::uint32_t a = l.create.front( );
        l.create.pop_front( );
        l.add( { 6u, cim_cell( l.tables->scrape_pfx, path ), a ? ( a - block0 ) / 0x280u : 0xFFFFFFFFu } );
        return a;
    };
    pfx_manager_t pm;
    pfx_manager_init( pm, ops );
    collision_scrape_hooks_t k;
    k.user = &L;
    k.get_event = []( void* u, std::uint32_t group, const char* name, std::uint32_t& ev ) {
        log_t& l = *static_cast< log_t* >( u );
        const unsigned ok = l.event.front( );
        l.event.pop_front( );
        l.bad_group += group != arg + 0x200u;
        ev = ok ? 0xE000u + l.entries : 0u;
        l.add( { 1u, cim_cell( l.tables->scrape_sfx, name ), 1u, ev } );
    };
    k.set_position = []( void* u, std::uint32_t ev, const float* p ) {
        unsigned w[ 3 ];
        std::memcpy( w, p, 12 );
        static_cast< log_t* >( u )->add( { 2u, ev, w[ 0 ], w[ 1 ], w[ 2 ], 0u, 0u } );
    };
    k.get_parameter = []( void* u, std::uint32_t ev, const char* name, std::uint32_t& p ) {
        log_t& l = *static_cast< log_t* >( u );
        const unsigned ok = l.param.front( );
        l.param.pop_front( );
        if ( ok )
            p = 0xF000u + l.entries;
        l.add( { 3u, ev, std::strcmp( name, "speed" ) == 0 ? 1u : 0u, ok } );
        return ok != 0u;
    };
    k.set_value = []( void* u, std::uint32_t p, float v ) {
        unsigned w;
        std::memcpy( &w, &v, 4 );
        static_cast< log_t* >( u )->add( { 4u, p, w } );
    };
    k.start = []( void* u, std::uint32_t ev ) { static_cast< log_t* >( u )->add( { 5u, ev } ); };
    k.stop = []( void* u, std::uint32_t ev, bool immediate ) { static_cast< log_t* >( u )->add( { 11u, ev, immediate ? 1u : 0u } ); };
    k.pfx_position = []( void* u, std::uint32_t i, const float* p ) {
        unsigned w[ 3 ];
        std::memcpy( w, p, 12 );
        static_cast< log_t* >( u )->add( { 7u, ( i - block0 ) / 0x280u, w[ 0 ], w[ 1 ], w[ 2 ] } );
    };
    k.pfx_start = []( void* u, std::uint32_t i ) { static_cast< log_t* >( u )->add( { 9u, ( i - block0 ) / 0x280u } ); };
    k.pfx_walk = []( void* u, std::uint32_t i ) { static_cast< log_t* >( u )->add( { 10u, ( i - block0 ) / 0x280u } ); };
    k.pfx_stop = []( void* u, std::uint32_t i, bool immediate ) {
        static_cast< log_t* >( u )->add( { 12u, ( i - block0 ) / 0x280u, immediate ? 1u : 0u } );
    };
    collision_tick_hooks_t th;
    th.user = &L;
    th.get_group = []( void* u, const char* name, std::uint32_t& g ) {
        const bool scrapes = std::strcmp( name, "Global/Scrapes/Scrapes_Player" ) == 0;
        const unsigned which = std::strcmp( name, "Global/Collisions/Collisions_Player" ) == 0 ? 0u : scrapes ? 1u : 9u;
        static_cast< log_t* >( u )->add( { 13u, which, 1u, 1u } );
        g = scrapes ? arg + 0x200u : arg + 0x280u;
    };
    th.dev_page = []( void* u, std::int32_t n ) {
        log_t& l = *static_cast< log_t* >( u );
        if ( l.page != 1u )
            return;
        l.add( { 15u } );
        l.add( { 16u, 1u, 1u, static_cast< unsigned >( n ) } );
    };
    const collision_impact_hooks_t ih;
    collision_manager_t m;
    m.scrapes.data.resize( 2 );
    m.scrapes.capacity = 2;
    std::vector< std::uint8_t > bodies[ 4 ], owners[ 4 ];
    for ( int i = 0; i < 4; ++i )
    {
        bodies[ i ].assign( 0x300, 0 );
        owners[ i ].assign( 0x480, 0 );
    }
    const unsigned nt = *s++;
    int bad = 0, unread = 0;
    unsigned records = 0, points = 0;
    for ( unsigned tk = 0; tk < nt; ++tk )
    {
        const unsigned kind = *s++;
        L.page = *s++;
        const collision_mesh_asset_t* mesh_of[ 4 ];
        for ( int i = 0; i < 4; ++i, s += 14 )
        {
            std::uint8_t* b = bodies[ i ].data( );
            std::memcpy( b + 0xF4, &s[ 0 ], 4 );
            const std::uint16_t g = static_cast< std::uint16_t >( s[ 1 ] );
            std::memcpy( b + 0x2CA, &g, 2 );
            owners[ i ][ 0x467 ] = static_cast< std::uint8_t >( s[ 2 ] );
            b[ 0x2C8 ] = static_cast< std::uint8_t >( s[ 3 ] );
            mesh_of[ i ] = s[ 4 ] ? &meshes[ s[ 4 ] - 1u ] : nullptr;
            std::memcpy( b + 0x40, s + 5, 12 );
            std::memcpy( b + 0x140, s + 8, 12 );
            std::memcpy( b + 0x150, s + 11, 12 );
        }
        const unsigned nm = *s++;
        std::vector< collision_manifold_t > mf( nm );
        std::vector< std::vector< std::uint8_t > > pts( nm );
        for ( unsigned j = 0; j < nm; ++j )
        {
            const unsigned ia = s[ 0 ], ib = s[ 1 ], n = s[ 2 ];
            s += 3;
            pts[ j ].assign( 0x120u * ( n ? n : 1u ), 0 );
            for ( unsigned c = 0; c < n; ++c, s += 8 )
            {
                std::uint8_t* p = pts[ j ].data( ) + 0x120u * c;
                std::memcpy( p + 0x20, s, 12 );
                std::memcpy( p + 0x30, s + 3, 12 );
                std::memcpy( p + 0x64, s + 6, 8 );
            }
            points += n;
            mf[ j ] = { bodies[ ia ].data( ), bodies[ ib ].data( ), owners[ ia ].data( ), owners[ ib ].data( ), mesh_of[ ia ], mesh_of[ ib ],
                        static_cast< std::int32_t >( n ), pts[ j ].data( ) };
        }
        unsigned n = *s++;
        L.event.assign( s, s + n );
        s += n;
        n = *s++;
        L.param.assign( s, s + n );
        s += n;
        n = *s++;
        L.create.clear( );
        for ( unsigned i = 0; i < n; ++i )
            L.create.push_back( s[ i ] == 0xFFFFFFFFu ? 0u : block0 + 0x280u * s[ i ] );
        s += n;
        L.log.clear( );
        L.entries = 0;
        if ( kind == 0u )
            unread += collision_manager_tick( m, t, mf.data( ), static_cast< std::int32_t >( nm ), pm, ops, heap, 0u, ih, k, th );
        else
            collision_manager_reset( m, pm, heap, 0u, k );
        bad += !L.event.empty( ) + !L.param.empty( ) + !L.create.empty( );
        const unsigned nl = *s++;
        bad += L.log != std::vector< unsigned >( s, s + nl );
        s += nl;
        const unsigned count = *s++, cap = *s++;
        bad += ( count != static_cast< unsigned >( m.scrapes.count ) ) + ( cap != static_cast< unsigned >( m.scrapes.capacity ) );
        for ( unsigned i = 0; i < count; ++i, s += 7 )
        {
            if ( i >= static_cast< unsigned >( m.scrapes.count ) )
            {
                ++bad;
                continue;
            }
            const collision_scrape_t& r = m.scrapes.data[ i ];
            unsigned ia = 9u, ib = 9u;
            for ( unsigned q = 0; q < 4; ++q )
            {
                ia = r.body_a == bodies[ q ].data( ) ? q : ia;
                ib = r.body_b == bodies[ q ].data( ) ? q : ib;
            }
            bad += ( ia != s[ 0 ] ) + ( ib != s[ 1 ] ) + ( static_cast< unsigned >( r.part_a ) != s[ 2 ] ) + ( static_cast< unsigned >( r.part_b ) != s[ 3 ] ) +
                   ( r.alive != s[ 4 ] ) + ( r.event != s[ 5 ] ) + ( r.handle != s[ 6 ] );
        }
        records += count;
        bad += ( s[ 0 ] != ( m.group_impact == arg + 0x280u ? 1u : 0u ) ) + ( s[ 1 ] != ( m.group_scrape == arg + 0x200u ? 1u : 0u ) );
        s += 2;
    }
    const unsigned nf = *s++;
    for ( unsigned i = 0; i < nf && i < np; ++i )
        bad += heap[ arg + procs[ i ] + 0x20u ] != s[ i ];
    s += nf;
    const unsigned ne = *s++;
    for ( unsigned i = 0; i < ne; ++i, s += 2 )
    {
        const pfx_manager_entry_t& x = pm.entries[ static_cast< std::size_t >( pm.table[ i ] ) ];
        bad += ( x.flags != s[ 0 ] ) + ( x.serial != s[ 1 ] );
    }
    bad += unread + L.bad_group;
    if ( bad == 0 )
        LOG_SUCCESS( "collision_manager_tick / collision_scrape_tick / collision_manager_reset match VuCollisionManager's 0x4AD970 / 0x4ADC00 / "
                     "0x4AD2B0 on a retail VuPfxManager: %u ops, %u contact points, %u records seen, %u fountain processes", nt, points, records, np );
    else
        LOG_ERROR( "collision_scrape FAILED %d", bad );
    return bad == 0;
}
// VuCollisionManager's ctor 0x4ACDF0 / init 0x4AD110 (world/collide.hxx collision_manager_construct / _init, the loads 0x4AD690)
// against retail (case collision_init) on the pack's SurfaceTableDB, with boat/surface.hxx's table built from SurfaceDB.
inline bool verify_collision_init( )
{
    c_archive archive;
    const std::string pack = world_check_pack( );
    std::vector< std::uint8_t > sdb, stdb;
    if ( pack.empty( ) || !archive.open( pack.c_str( ) ) || !archive.read( "VuDBAsset/SurfaceDB", sdb ) || !archive.read( "VuDBAsset/SurfaceTableDB", stdb ) )
    {
        LOG_ERROR( "collision_init: the pack's SurfaceDB / SurfaceTableDB are not readable" );
        return false;
    }
    const value_t surface_db = c_reader( sdb ).parse( );
    const value_t table_db = c_reader( stdb ).parse( );
    surface_table_t surfaces;
    surface_table_build( surfaces, &surface_db );
    const unsigned* s = g_golden_cin_words;
    collision_manager_t m;
    collision_tables_t t;
    collision_manager_construct( m, t );
    int bad = 0;
    const vu_array_t< collision_name_t >* tables[ 4 ] = { &t.impact_sfx, &t.scrape_sfx, &t.impact_pfx, &t.scrape_pfx };
    for ( int k = 0; k < 4; ++k )   // ctor words +0x18 + 0xC k: data present, count, capacity
        bad += ( s[ 6 + 3 * k ] != 1u ) + ( s[ 7 + 3 * k ] != static_cast< unsigned >( tables[ k ]->count ) ) +
               ( s[ 8 + 3 * k ] != static_cast< unsigned >( tables[ k ]->capacity ) );
    bad += ( s[ 19 ] != static_cast< unsigned >( m.impacts.count ) ) + ( s[ 20 ] != static_cast< unsigned >( m.impacts.capacity ) );
    bad += ( s[ 22 ] != static_cast< unsigned >( m.scrapes.count ) ) + ( s[ 23 ] != static_cast< unsigned >( m.scrapes.capacity ) );
    s += 24;
    struct log_t
    {
        const value_t* doc;
        unsigned listener = 0, asset = 0, tick = 0;
    } log{ &table_db };
    collision_init_hooks_t h;
    h.user = &log;
    h.add_listener = []( void* u ) { static_cast< log_t* >( u )->listener = 1; };
    h.asset = []( void* u, const char* name ) -> const value_t* {
        log_t& l = *static_cast< log_t* >( u );
        l.asset = std::strcmp( name, "SurfaceTableDB" ) == 0 ? 1u : 0u;
        return l.doc;
    };
    h.register_tick = []( void* u, const char* phase ) { static_cast< log_t* >( u )->tick = std::strcmp( phase, "Decision" ) == 0 ? 1u : 0u; };
    const bool ok = collision_manager_init( t, surfaces, h );
    bad += ( ok ? 1u : 0u ) != s[ 0 ];
    bad += s[ 1 ] != 1u;   // +0x10 = the asset the hook answered
    bad += static_cast< unsigned >( t.types ) != s[ 2 ];
    bad += ( log.listener != s[ 3 ] ) + ( log.asset != s[ 4 ] ) + ( log.tick != s[ 5 ] );
    s += 6;
    int cells = 0;
    for ( int k = 0; k < 4; ++k )
    {
        const unsigned count = *s++, capacity = *s++;
        bad += ( count != static_cast< unsigned >( tables[ k ]->count ) ) + ( capacity != static_cast< unsigned >( tables[ k ]->capacity ) );
        for ( unsigned c = 0; c < count && c < tables[ k ]->data.size( ); ++c, ++cells )
            bad += std::memcmp( tables[ k ]->data[ c ].data( ), s + 16 * c, 64 ) != 0;
        s += 16 * count;
    }
    if ( bad == 0 )
        LOG_SUCCESS( "collision_manager_construct / _init match VuCollisionManager's 0x4ACDF0 / 0x4AD110 (0x4AD690, 0x4AE420) on SurfaceTableDB: "
                     "%d types, %d cells of 64 bytes", t.types, cells );
    else
        LOG_ERROR( "collision_init FAILED %d", bad );
    return bad == 0;
}