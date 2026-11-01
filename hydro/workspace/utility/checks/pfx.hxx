#pragma once

#include <workspace/core/world/pfxclass.hxx>
#include <workspace/core/world/pfxsim.hxx>
#include <workspace/core/world/pfxload.hxx>
#include <workspace/core/world/pfxmgr.hxx>

#include <cstring>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

// The water area's particle checks (included by checks/water.hxx before verify_water_all, which runs
// verify_pfx_all). Golden tables come from hydro-tools oracle/cases/water.py through emit/water.py.

// The fountain instances' slot 2 (world/pfxclass.hxx) against retail (case fountain_init): 0x55E100 quad,
// 0x55E9B0 directional quad, 0x55E640 geom, on random particles, params, system velocities and slot 5
// matrices, the global VuRand seeded per vector. Compared: the particle's 32 words after (but the one word
// each of 0x55E9B0 / 0x55E640 takes from an unwritten stack word: +0x24 / +0x50, UNKNOWN), the VuRand's 34
// words after, and the number of matrix fetches.
inline bool verify_fountain_init( )
{
    int failed = 0, words = 0;
    const unsigned* s = g_golden_fin_words;
    for ( std::size_t index = 0; index < g_golden_fin_count; ++index, s += 169 )
    {
        const unsigned kind = s[ 0 ];
        pfx_rand_t rand;
        rand.seed = static_cast< std::int32_t >( s[ 1 ] );
        rand.previous = static_cast< std::int32_t >( s[ 2 ] );
        for ( int k = 0; k < 32; ++k )
            rand.table[ k ] = static_cast< std::int32_t >( s[ 3 + k ] );
        std::uint8_t params[ 0x100 ]{};
        std::memcpy( params + 0x98, s + 35, 64 );
        float svel[ 3 ], matrix[ 16 ];
        std::memcpy( svel, s + 51, 12 );
        std::memcpy( matrix, s + 54, 64 );
        const unsigned* before = s + 70;
        const unsigned* after = s + 102;
        const unsigned* rng_after = s + 134;
        const unsigned fetches = s[ 168 ];
        pfx_quad_particle_t p;
        std::memcpy( p.w, before, sizeof( p.w ) );
        unsigned got_fetches = 0;
        if ( kind == 0 )
            pfx_quad_fountain_init( rand, params, p );
        else if ( kind == 1 )
            pfx_directional_fountain_init( rand, params, svel, matrix, p ), got_fetches = 1;
        else
            pfx_geom_fountain_init( rand, params, matrix, p ), got_fetches = 1;
        const int unknown = kind == 1 ? 0x24 / 4 : kind == 2 ? 0x50 / 4 : -1;
        int bad = 0;
        for ( int k = 0; k < 30; ++k )
        {
            if ( k == unknown )
                continue;
            ++words;
            if ( p.w[ k ] != after[ k ] )
            {
                if ( bad < 3 )
                    LOG_ERROR( "fountain_init vector %d kind %u word +0x%X: 0x%08X, retail 0x%08X", static_cast< int >( index ), kind, 4 * k,
                        p.w[ k ], after[ k ] );
                ++bad;
            }
        }
        // a particle is 30 words in this model; retail's words +0x78 / +0x7C stay as they were
        bad += ( after[ 30 ] != before[ 30 ] || after[ 31 ] != before[ 31 ] ) ? 1 : 0;
        const std::uint32_t r[ 34 ] = { static_cast< std::uint32_t >( rand.seed ), static_cast< std::uint32_t >( rand.previous ) };
        for ( int k = 0; k < 34; ++k )
        {
            const std::uint32_t v = k < 2 ? r[ k ] : static_cast< std::uint32_t >( rand.table[ k - 2 ] );
            ++words;
            bad += v != rng_after[ k ] ? 1 : 0;
        }
        bad += got_fetches != fetches ? 1 : 0;
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "fountain_init vector %d (kind %u): %d differences", static_cast< int >( index ), kind, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "pfx_quad / directional / geom_fountain_init match the fountain instances' slot 2 0x55E100 / 0x55E9B0 / 0x55E640 "
                     "on %d vectors, %d words (particle + VuRand) exact",
            static_cast< int >( g_golden_fin_count ), words );
    else
        LOG_ERROR( "fountain_init FAILED %d of %d", failed, static_cast< int >( g_golden_fin_count ) );
    return failed == 0;
}

// The pattern instances' draw slots (world/pfxclass.hxx) against retail (case pattern_draw): geom 0x5903A0 / 0x5906A0,
// quad 0x5920B0, trail 0x58F9C0 with the model draws 0x520130 / 0x520670 and the renderers 0x58BD60 / 0x58D030 recorded.
// Rebuilt here from the recreation: every call in order - the model matrix's 16 words, the colour retail stored at
// params +0x74 and the +0x78 byte, the list element, the pass argument, the renderer and its arguments.
inline bool verify_pattern_draw( )
{
    int failed = 0, calls = 0;
    const unsigned* s = g_golden_pdr_words;
    float identity[ 16 ];
    std::memcpy( identity, s, 64 );
    s += 16;
    for ( std::size_t index = 0; index < g_golden_pdr_count; ++index )
    {
        const unsigned kind = s[ 0 ], n = s[ 1 ], space = s[ 2 ];
        float matrix[ 16 ], tint[ 4 ], scale;
        std::memcpy( matrix, s + 3, 64 );
        std::memcpy( &scale, s + 19, 4 );
        std::memcpy( tint, s + 20, 16 );
        const unsigned flag = s[ 24 ], asset = s[ 25 ], nel = s[ 26 ], arg = s[ 27 ];
        s += 28;
        std::vector< pfx_quad_particle_t > parts( n );
        for ( unsigned i = 0; i < n; ++i, s += 25 )
            std::memcpy( parts[ i ].w, s, 100 );
        std::vector< std::vector< unsigned > > want;
        const unsigned ncalls = *s++;
        for ( unsigned c = 0; c < ncalls; ++c )
        {
            want.emplace_back( s + 1, s + 1 + s[ 0 ] );
            s += 1 + s[ 0 ];
        }
        std::vector< std::vector< unsigned > > got;
        const float* d = space == 1u ? matrix : identity;
        auto words = [ & ]( std::vector< unsigned >& v, const float* m ) {
            for ( int k = 0; k < 16; ++k )
            {
                unsigned u;
                std::memcpy( &u, m + k, 4 );
                v.push_back( u );
            }
        };
        if ( kind == 0u || kind == 1u )
        {
            for ( const pfx_quad_particle_t& q : parts )
            {
                float m[ 16 ];
                pfx_geom_draw_matrix( q, d, scale, m );
                if ( kind == 0u )
                {
                    std::vector< unsigned > v{ 0u, 0x5Cu, arg };
                    words( v, m );
                    v.push_back( pfx_geom_draw_colour( q, tint ) );
                    v.push_back( flag );
                    got.push_back( v );
                }
                else
                {
                    for ( unsigned e = 0; asset != 0u && e < nel; ++e )
                    {
                        std::vector< unsigned > v{ 1u, 0x5Cu, 0x1001u + e, arg };   // the case's element k has the value 0x1001 + k
                        words( v, m );
                        got.push_back( v );
                    }
                }
            }
        }
        else if ( pfx_pattern_submits( n ) )
        {
            got.push_back( { kind, kind == 2u ? 0xCu : 0x10u, arg, 0u } );
        }
        calls += static_cast< int >( want.size( ) );
        if ( got != want )
        {
            ++failed;
            int call = -1, word = -1;
            for ( std::size_t c = 0; call < 0 && c < got.size( ) && c < want.size( ); ++c )
                for ( std::size_t k = 0; k < got[ c ].size( ) && k < want[ c ].size( ); ++k )
                    if ( got[ c ][ k ] != want[ c ][ k ] )
                    {
                        call = static_cast< int >( c ), word = static_cast< int >( k );
                        break;
                    }
            LOG_ERROR( "pattern_draw vector %d (kind %u): %d calls, retail %d; first difference call %d word %d", static_cast< int >( index ),
                kind, static_cast< int >( got.size( ) ), static_cast< int >( want.size( ) ), call, word );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "pfx_geom_draw_matrix / pfx_geom_draw_colour / pfx_pattern_submits match the draw slots 0x5903A0 / 0x5906A0 / "
                     "0x5920B0 / 0x58F9C0 on %d vectors, %d draw calls word for word",
            static_cast< int >( g_golden_pdr_count ), calls );
    else
        LOG_ERROR( "pattern_draw FAILED %d of %d", failed, static_cast< int >( g_golden_pdr_count ) );
    return failed == 0;
}

// The VuPfx pools and the system lifecycle (world/pfxsim.hxx) against retail's memory (case pfx_lifecycle): the
// resources' ctor + init, then a plan of createSystemInstance / start / allocateParticle / tick / stop / release /
// VuPfxImpl tick on hand-built node trees (real node vtables); the pattern ticks are the case's stubs, replayed. After
// the init and after every operation: the 25 resource words and every block word (but the system AABB pads +0x8C /
// +0x9C and the pattern AABB pads +0x38 / +0x48, stack words), every return, every pattern tick's pattern and dt.
struct pfx_check_sim_t
{
    std::map< std::uint32_t, std::vector< std::uint32_t > > kids;
    const unsigned* ticks = nullptr;
    int nticks = 0, bad = 0;
};

inline bool verify_pfx_lifecycle( )
{
    const unsigned* s = g_golden_plc_words;
    const std::uint32_t res = s[ 0 ];
    const std::uint32_t blocks[ 4 ] = { s[ 1 ], s[ 2 ], s[ 3 ], s[ 4 ] };
    std::uint32_t pairs[ 8 ];
    std::memcpy( pairs, s + 5, 32 );
    s += 13 + 16;
    for ( int k = 1; k < 8; k += 2 )
        pairs[ k ] = ( pairs[ k ] + 15u ) & 0xFFFFFFF0u;   // the block sizes as 0x58B9A0 rounds them (the regions)
    pfx_heap_t h;
    h.add( res, 0x64 );
    for ( int k = 0; k < 4; ++k )
        h.add( blocks[ k ], pairs[ 2 * k ] * pairs[ 2 * k + 1 ] );
    pfx_check_sim_t sim;
    const unsigned nnodes = *s++;
    std::vector< std::uint32_t > addr;
    const unsigned* ns = s;
    for ( unsigned i = 0; i < nnodes; ++i )
    {
        addr.push_back( ns[ 0 ] );
        ns += 5 + ns[ 4 ];
    }
    h.add( addr[ 0 ], 0x80u * nnodes );
    for ( unsigned i = 0; i < nnodes; ++i )
    {
        h[ s[ 0 ] ] = s[ 1 ];
        h[ s[ 0 ] + 0x30 ] = s[ 2 ];
        h[ s[ 0 ] + 0x34 ] = s[ 3 ];
        std::vector< std::uint32_t >& v = sim.kids[ s[ 0 ] ];
        for ( unsigned k = 0; k < s[ 4 ]; ++k )
            v.push_back( addr[ s[ 5 + k ] ] );
        s += 5 + s[ 4 ];
    }
    pfx_sim_hooks_t hooks;
    hooks.user = &sim;
    hooks.children = []( void* u, std::uint32_t node ) -> const std::vector< std::uint32_t >* {
        auto& m = static_cast< pfx_check_sim_t* >( u )->kids;
        auto it = m.find( node );
        return it == m.end( ) ? nullptr : &it->second;
    };
    hooks.pattern_tick = []( void* u, pfx_heap_t& hp, std::uint32_t pattern, float dt ) {
        pfx_check_sim_t& c = *static_cast< pfx_check_sim_t* >( u );
        if ( c.nticks <= 0 )
        {
            ++c.bad;
            return;
        }
        std::uint32_t d;
        std::memcpy( &d, &dt, 4 );
        c.bad += ( c.ticks[ 0 ] != pattern || c.ticks[ 1 ] != d ) ? 1 : 0;
        hp[ pattern + 0x28 ] = c.ticks[ 2 ];
        for ( int k = 0; k < 3; ++k )
        {
            hp[ pattern + 0x2C + 4 * k ] = c.ticks[ 3 + k ];
            hp[ pattern + 0x3C + 4 * k ] = c.ticks[ 6 + k ];
        }
        c.ticks += 9;
        --c.nticks;
    };
    const std::uint32_t sizes[ 4 ] = { pairs[ 0 ] * pairs[ 1 ], pairs[ 2 ] * pairs[ 3 ], pairs[ 4 ] * pairs[ 5 ], pairs[ 6 ] * pairs[ 7 ] };
    int failed = 0, compared = 0;
    auto compare = [ & ]( const unsigned* want, int step ) {
        int bad = 0;
        for ( int k = 0; k < 25; ++k, ++compared )
            if ( h[ res + 4 * k ] != want[ k ] )
            {
                if ( bad < 2 )
                    LOG_ERROR( "pfx_lifecycle step %d: resources +0x%X = 0x%08X, retail 0x%08X", step, 4 * k, h[ res + 4 * k ], want[ k ] );
                ++bad;
            }
        want += 25;
        for ( int b = 0; b < 4; ++b )
            for ( std::uint32_t o = 0; o < sizes[ b ]; o += 4, ++want )
            {
                if ( b == 0 && ( o % pairs[ 1 ] == 0x8Cu || o % pairs[ 1 ] == 0x9Cu ) )
                    continue;
                if ( b == 1 && ( o % pairs[ 3 ] == 0x38u || o % pairs[ 3 ] == 0x48u ) )   // 0x58B240 stores stack words there
                    continue;
                ++compared;
                if ( h[ blocks[ b ] + o ] != *want )
                {
                    if ( bad < 2 )
                        LOG_ERROR( "pfx_lifecycle step %d: block %d +0x%X = 0x%08X, retail 0x%08X", step, b, o, h[ blocks[ b ] + o ], *want );
                    ++bad;
                }
            }
        return bad;
    };
    const std::size_t region = 25 + ( sizes[ 0 ] + sizes[ 1 ] + sizes[ 2 ] + sizes[ 3 ] ) / 4;
    pfx_resources_ctor( h, res );
    std::uint32_t authored[ 8 ];
    std::memcpy( authored, g_golden_plc_words + 5, 32 );
    pfx_resources_init( h, res, authored, blocks );
    failed += compare( s, -1 ) ? 1 : 0;
    s += region;
    const unsigned nops = *s++;
    for ( unsigned i = 0; i < nops; ++i )
    {
        const unsigned code = *s++;
        int bad = 0;
        switch ( code )
        {
        case 0:
            bad += pfx_system_instance_create( h, res, addr[ s[ 0 ] ], hooks ) != s[ 1 ] ? 1 : 0;
            s += 2;
            break;
        case 1: pfx_system_start( h, s[ 0 ] ); s += 1; break;
        case 2:
            for ( unsigned k = 0; k < s[ 1 ]; ++k )
                bad += pfx_particle_allocate( h, res, s[ 0 ] ) != s[ 2 + k ] ? 1 : 0;
            s += 2 + s[ 1 ];
            break;
        case 3:
        {
            float dt;
            std::memcpy( &dt, s + 1, 4 );
            sim.ticks = s + 3;
            sim.nticks = static_cast< int >( s[ 2 ] );
            sim.bad = 0;
            pfx_system_tick( h, s[ 0 ], dt, hooks );
            bad += sim.bad + sim.nticks;
            s += 3 + 9 * s[ 2 ];
            break;
        }
        case 4: pfx_system_stop( h, res, s[ 0 ], s[ 1 ] != 0u ); s += 2; break;
        case 5: pfx_system_release( h, res, s[ 0 ] ); s += 1; break;
        default: pfx_impl_tick( h, res ); break;
        }
        bad += compare( s, static_cast< int >( i ) );
        s += region;
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "pfx_lifecycle step %d (operation %u): %d differences", static_cast< int >( i ), code, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuPfxResources 0x58B5B0 / 0x58B670 and the system lifecycle (0x51DA70 / 0x58B880 / 0x58A940 / 0x58B2D0 / 0x58AC90 / "
                     "0x58B4E0 / 0x58AD70 / 0x58ACC0 / 0x51DAE0 / 0x58AA70 / 0x58B3F0 / 0x51DD80) match retail's memory over %d operations, "
                     "%d words",
            static_cast< int >( nops ), compared );
    else
        LOG_ERROR( "pfx_lifecycle FAILED %d of %d steps", failed, static_cast< int >( nops ) + 1 );
    return failed == 0;
}

// VuPfx systems END TO END (world/pfxsim.hxx: pfx_system_tick with pfx_heap_pattern_tick, every process's slot 1 by
// its vtable, the fountains' emit + slot 2 on the heap) against retail running its own per-frame code (case pfx_run):
// after the init and every operation, the 25 resource words, every block word (but the stack-word pads: system +0x8C /
// +0x9C, pattern +0x38 / +0x48, and a directional fountain particle's +0x24 / a geom fountain particle's +0x50 - here
// every particle's +0x24 and +0x50 are skipped, as the case cannot tell which fountain made a particle) and the
// VuRand's 34 words; no unrecreated path may be reached.
//
// With `boats` (case pfx_boats): the boat processes on the heap - the frame's boats / fluids from the table, the binds
// (operation 8: pfx_bind_thrust / pfx_bind_hull / pfx_bind_spring) and every operation's recorded hook calls replayed in
// order (prepare: the fluids and the matrix compared; the one-point query: the point compared, the answer handed back;
// the snap: the point compared, the height handed back) - a call out of order or left over is a difference. Every
// particle's +0x14 is skipped too: a hull particle from a picked sample takes retail's stale stack word there (spray.hxx).
struct pfx_check_run_t
{
    std::map< std::uint32_t, std::vector< std::uint32_t > > kids;
    pfx_frame_t frame;
    std::uint32_t r = 0;
    // the boats
    std::vector< pfx_frame_boat_t > boats;
    std::vector< pfx_frame_fluids_t > fluids;
    std::vector< std::unique_ptr< fluids_mesh_t > > meshes;
    std::vector< std::unique_ptr< fluid_solver_t > > solvers;
    std::vector< std::unique_ptr< solver_rigid_t > > bodies;
    std::vector< std::unique_ptr< boat_view_t > > views;
    std::vector< std::unique_ptr< pfx_hull_boat_t > > hulls;
    std::vector< std::vector< std::uint8_t > > records;
    const unsigned* calls = nullptr;
    unsigned left = 0;
    int bad = 0;
    int made[ 4 ]{};
    const unsigned* next( unsigned type )
    {
        if ( left == 0u || calls[ 1 ] != type )
        {
            ++bad;
            return nullptr;
        }
        const unsigned* c = calls + 2;
        ++made[ type & 3u ];
        calls += 1 + calls[ 0 ];
        --left;
        return c;
    }
};

// The boat table (g_golden_pbt_boats) into the frame's entries.
inline void pfx_check_boats( pfx_check_run_t& run, const unsigned* b, std::size_t count )
{
    for ( std::size_t k = 0; k < count; ++k )
    {
        pfx_frame_boat_t e;
        e.address = b[ 0 ];
        const std::uint32_t fluids = b[ 1 ];
        run.bodies.push_back( std::make_unique< solver_rigid_t >( ) );
        solver_rigid_t& body = *run.bodies.back( );
        const unsigned* m = b + 13;   // the matrix
        const unsigned* bodies = b + 13 + 16 + 4 + 2 + 16;
        for ( int i = 0; i < 4; ++i )
            body.linear_velocity[ i ] = water_bits( bodies[ 4 + i ] );
        run.views.push_back( std::make_unique< boat_view_t >( boat_view_t{ water_bits( b[ 2 ] ), b[ 3 ], static_cast< std::uint8_t >( b[ 4 ] ),
            static_cast< std::uint8_t >( b[ 5 ] ), { }, 0, 0, 0, body } ) );
        boat_view_t& v = *run.views.back( );
        for ( int i = 0; i < 3; ++i )
            v.o50_60[ i ] = water_bits( m[ 4 + i ] );
        v.o22C_04 = water_bits( b[ 6 ] );
        v.o22C_0C = water_bits( b[ 7 ] );
        v.o22C0_470 = water_bits( b[ 8 ] );
        run.records.emplace_back( 32, std::uint8_t( 0 ) );
        for ( int i = 0; i < 4; ++i )
            run.records.back( )[ 8 * i ] = static_cast< std::uint8_t >( b[ 9 + i ] );
        e.view = &v;
        e.records = run.records.back( ).data( );
        for ( int i = 0; i < 3; ++i )
        {
            e.position[ i ] = water_bits( m[ 12 + i ] );
            e.velocity[ i ] = water_bits( bodies[ 4 + i ] );
        }
        run.hulls.push_back( std::make_unique< pfx_hull_boat_t >( ) );
        pfx_hull_boat_t& h = *run.hulls.back( );
        std::memcpy( h.transform, m, 64 );
        std::memcpy( h.offset, m + 16, 16 );
        h.part_flag = static_cast< std::uint8_t >( m[ 20 ] );
        h.part_count = static_cast< std::int32_t >( m[ 21 ] );
        std::memcpy( h.part_matrix, m + 22, 64 );
        pfx_hull_body_t* hb[ 2 ] = { &h.body, &h.part_body };
        for ( int j = 0; j < 2; ++j )
        {
            std::memcpy( hb[ j ]->position, bodies + 12 * j, 12 );
            std::memcpy( hb[ j ]->linear, bodies + 12 * j + 4, 12 );
            std::memcpy( hb[ j ]->angular, bodies + 12 * j + 8, 12 );
        }
        e.hull = &h;
        run.boats.push_back( e );
        const unsigned* t = bodies + 24;
        const unsigned ntri = t[ 0 ], nedge = t[ 1 ];
        run.meshes.push_back( std::make_unique< fluids_mesh_t >( ) );
        run.meshes.back( )->triangles.assign( t + 2, t + 2 + 15 * ntri );
        run.solvers.push_back( std::make_unique< fluid_solver_t >( ) );
        fluid_solver_t& sv = *run.solvers.back( );
        sv.mesh = run.meshes.back( ).get( );
        sv.edges.resize( nedge );
        std::memcpy( sv.edges.data( ), t + 2 + 15 * ntri, 40u * nedge );
        run.fluids.push_back( { fluids, &sv } );
        b = t + 2 + 15 * ntri + 10 * nedge;
    }
    run.frame.boats = run.boats.data( );
    run.frame.boat_count = static_cast< int >( run.boats.size( ) );
    run.frame.fluids = run.fluids.data( );
    run.frame.fluids_count = static_cast< int >( run.fluids.size( ) );
    run.frame.user = &run;
    run.frame.prepare = []( void* u, fluid_solver_t& solver, const float* matrix ) {
        pfx_check_run_t& c = *static_cast< pfx_check_run_t* >( u );
        const unsigned* r = c.next( 1u );
        if ( r == nullptr )
            return;
        if ( r[ 0 ] >= c.solvers.size( ) || c.solvers[ r[ 0 ] ].get( ) != &solver || std::memcmp( r + 1, matrix, 64 ) != 0 )
            ++c.bad;
    };
    run.frame.point = []( void* u, const float* point, float* out ) {
        pfx_check_run_t& c = *static_cast< pfx_check_run_t* >( u );
        const unsigned* r = c.next( 2u );
        if ( r == nullptr )
            return false;
        if ( std::memcmp( r, point, 12 ) != 0 )
            ++c.bad;
        std::memcpy( out, r + 3, 32 );
        return true;
    };
    run.frame.water = true;
    run.frame.snap = []( void* u, const float* point, water_vertex_t& out ) {
        pfx_check_run_t& c = *static_cast< pfx_check_run_t* >( u );
        const unsigned* r = c.next( 3u );
        if ( r == nullptr )
            return false;
        if ( std::memcmp( r, point, 12 ) != 0 )   // the w word is the particle's +0x14 (skipped above); the height never reads it
            ++c.bad;
        std::memcpy( &out.height, r + 4, 4 );
        return true;
    };
}

inline bool pfx_check_run( const unsigned* s, const char* tag, const unsigned* boats = nullptr, std::size_t boat_count = 0 )
{
    const std::uint32_t res = s[ 0 ];
    const std::uint32_t blocks[ 4 ] = { s[ 1 ], s[ 2 ], s[ 3 ], s[ 4 ] };
    std::uint32_t authored[ 8 ], pairs[ 8 ];
    std::memcpy( authored, s + 5, 32 );
    std::memcpy( pairs, s + 5, 32 );
    for ( int k = 1; k < 8; k += 2 )
        pairs[ k ] = ( pairs[ k ] + 15u ) & 0xFFFFFFF0u;
    pfx_check_run_t run;
    run.r = res;
    if ( boats != nullptr )
        pfx_check_boats( run, boats, boat_count );
    const unsigned* box = s + 13;
    for ( int k = 0; k < 3; ++k )
    {
        std::memcpy( &run.frame.geom_box[ k ], box + k, 4 );
        std::memcpy( &run.frame.geom_box[ 3 + k ], box + 4 + k, 4 );
    }
    float camera[ 3 ];
    std::memcpy( camera, s + 21, 12 );
    pfx_rand_t rand;
    rand.seed = static_cast< std::int32_t >( s[ 24 ] );
    rand.previous = static_cast< std::int32_t >( s[ 25 ] );
    for ( int k = 0; k < 32; ++k )
        rand.table[ k ] = static_cast< std::int32_t >( s[ 26 + k ] );
    run.frame.cameras = camera;
    run.frame.camera_count = 1;
    run.frame.rand = &rand;
    s += 58;
    pfx_heap_t h;
    h.add( res, 0x64 );
    for ( int k = 0; k < 4; ++k )
        h.add( blocks[ k ], pairs[ 2 * k ] * pairs[ 2 * k + 1 ] );
    const unsigned nnodes = *s++;
    std::vector< std::uint32_t > addr;
    for ( const unsigned* ns = s; addr.size( ) < nnodes; )
    {
        addr.push_back( ns[ 0 ] );
        const unsigned nw = ns[ 2 ];
        ns += 4 + 2 * nw + ns[ 3 + 2 * nw ];
    }
    h.add( addr[ 0 ], 0x100u * nnodes );
    for ( unsigned i = 0; i < nnodes; ++i )
    {
        const std::uint32_t a = s[ 0 ];
        h[ a ] = s[ 1 ];
        const unsigned nw = s[ 2 ];
        for ( unsigned k = 0; k < nw; ++k )
            h[ a + s[ 3 + 2 * k ] ] = s[ 4 + 2 * k ];
        const unsigned nk = s[ 3 + 2 * nw ];
        std::vector< std::uint32_t >& v = run.kids[ a ];
        for ( unsigned k = 0; k < nk; ++k )
            v.push_back( addr[ s[ 4 + 2 * nw + k ] ] );
        s += 4 + 2 * nw + nk;
    }
    pfx_sim_hooks_t hooks;
    hooks.user = &run;
    hooks.children = []( void* u, std::uint32_t node ) -> const std::vector< std::uint32_t >* {
        auto& m = static_cast< pfx_check_run_t* >( u )->kids;
        auto it = m.find( node );
        return it == m.end( ) ? nullptr : &it->second;
    };
    hooks.pattern_tick = []( void* u, pfx_heap_t& hp, std::uint32_t pattern, float dt ) {
        pfx_check_run_t& c = *static_cast< pfx_check_run_t* >( u );
        pfx_heap_pattern_tick( hp, c.r, pattern, dt, c.frame );
    };
    const std::uint32_t sizes[ 4 ] = { pairs[ 0 ] * pairs[ 1 ], pairs[ 2 ] * pairs[ 3 ], pairs[ 4 ] * pairs[ 5 ], pairs[ 6 ] * pairs[ 7 ] };
    int compared = 0;
    auto compare = [ & ]( const unsigned* want, int step ) {
        int bad = 0;
        auto one = [ & ]( std::uint32_t got, std::uint32_t w, const char* what, std::uint32_t o ) {
            ++compared;
            if ( got == w )
                return;
            static std::map< const char*, int > logged;
            if ( logged[ what ]++ < 3 )
                LOG_ERROR( "%s step %d: %s +0x%X = 0x%08X, retail 0x%08X", tag, step, what, o, got, w );
            ++bad;
        };
        for ( std::uint32_t k = 0; k < 25; ++k )
            one( h[ res + 4 * k ], *want++, "resources", 4 * k );
        static const char* const names[ 4 ] = { "systems", "patterns", "processes", "particles" };
        for ( int b = 0; b < 4; ++b )
            for ( std::uint32_t o = 0; o < sizes[ b ]; o += 4, ++want )
            {
                const std::uint32_t in = o % pairs[ 2 * b + 1 ];
                if ( ( b == 0 && ( in == 0x8Cu || in == 0x9Cu ) ) || ( b == 1 && ( in == 0x38u || in == 0x48u ) ) ||
                     ( b == 3 && ( in == 0x24u || in == 0x50u || ( boats != nullptr && in == 0x14u ) ) ) )
                    continue;
                one( h[ blocks[ b ] + o ], *want, names[ b ], o );
            }
        one( static_cast< std::uint32_t >( rand.seed ), *want++, "VuRand", 0 );
        one( static_cast< std::uint32_t >( rand.previous ), *want++, "VuRand", 4 );
        for ( int k = 0; k < 32; ++k )
            one( static_cast< std::uint32_t >( rand.table[ k ] ), *want++, "VuRand", 8 + 4 * k );
        return bad;
    };
    const std::size_t region = 25 + ( sizes[ 0 ] + sizes[ 1 ] + sizes[ 2 ] + sizes[ 3 ] ) / 4 + 34;
    pfx_resources_ctor( h, res );
    pfx_resources_init( h, res, authored, blocks );
    int failed = compare( s, -1 ) ? 1 : 0;
    s += region;
    const unsigned nops = *s++;
    int particles = 0;
    for ( unsigned i = 0; i < nops; ++i )
    {
        const unsigned code = *s++;
        int bad = 0;
        if ( boats != nullptr )
        {
            static const unsigned operands[ 9 ] = { 2, 1, 0, 2, 2, 1, 0, 7, 3 };
            const unsigned* cl = s + operands[ code < 9u ? code : 6u ];
            run.left = cl[ 0 ];
            run.calls = cl + 1;
        }
        switch ( code )
        {
        case 0: bad += pfx_system_instance_create( h, res, addr[ s[ 0 ] ], hooks ) != s[ 1 ] ? 1 : 0; s += 2; break;
        case 1: pfx_system_start( h, s[ 0 ] ); s += 1; break;
        case 3:
        {
            float dt;
            std::memcpy( &dt, s + 1, 4 );
            pfx_system_tick( h, s[ 0 ], dt, hooks );
            s += 2;
            break;
        }
        case 4: pfx_system_stop( h, res, s[ 0 ], s[ 1 ] != 0u ); s += 2; break;
        case 5: pfx_system_release( h, res, s[ 0 ] ); s += 1; break;
        case 7:
            for ( int k = 0; k < 3; ++k )
            {
                h[ s[ 0 ] + 0x4C + 4 * k ] = s[ 1 + k ];
                h[ s[ 0 ] + 0x5C + 4 * k ] = s[ 4 + k ];
            }
            s += 7;
            break;
        case 8:
        {
            const int bi = static_cast< int >( s[ 2 ] );
            const std::uint32_t boat = bi >= 0 ? run.boats[ static_cast< std::size_t >( bi ) ].address : 0u;
            const std::uint32_t fluids = bi >= 0 ? run.fluids[ static_cast< std::size_t >( bi ) ].address : 0u;
            if ( s[ 1 ] == 0u )
                pfx_bind_thrust( h, s[ 0 ], boat );
            else if ( s[ 1 ] == 1u )
                pfx_bind_hull( h, s[ 0 ], boat, fluids );
            else
                pfx_bind_spring( h, s[ 0 ], boat );
            s += 3;
            break;
        }
        default: pfx_impl_tick( h, res ); break;
        }
        if ( boats != nullptr )
        {
            bad += run.bad + ( run.left != 0u ? 1 : 0 );   // the operation's calls all consumed, none differing
            if ( run.bad != 0 || run.left != 0u )
                LOG_ERROR( "%s step %d: %d hook calls differ, %u not made (next type %u)", tag, static_cast< int >( i ), run.bad, run.left,
                    run.left != 0u ? run.calls[ 1 ] : 0u );
            run.bad = 0;
            const unsigned ncalls = *s++;
            for ( unsigned c = 0; c < ncalls; ++c )
                s += 1 + s[ 0 ];
        }
        bad += compare( s, static_cast< int >( i ) );
        const int live = static_cast< int >( pairs[ 6 ] - h[ res + 0x3C ] );
        particles = live > particles ? live : particles;
        s += region;
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "%s step %d (operation %u): %d differences", tag, static_cast< int >( i ), code, bad );
        }
    }
    if ( run.frame.unrecreated != 0 )
    {
        ++failed;
        LOG_ERROR( "%s reached %d unrecreated paths", tag, run.frame.unrecreated );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "%s: VuPfx systems end to end (create, start, the real pattern / fountain / tick-process frames, stop, release) "
                     "match retail's memory over %d operations (up to %d live particles), %d words",
            tag, static_cast< int >( nops ), particles, compared );
    else
        LOG_ERROR( "%s FAILED %d of %d steps", tag, failed, static_cast< int >( nops ) + 1 );
    if ( failed == 0 && boats != nullptr )
        LOG_INFO( "%s: the boat hooks replayed in order - %d VuFluidSolver_prepare, %d one-point queries, %d water snaps", tag, run.made[ 1 ],
            run.made[ 2 ], run.made[ 3 ] );
    return failed == 0;
}

inline bool verify_pfx_run( ) { return pfx_check_run( g_golden_prn_words, "pfx_run" ); }

// The trail pattern's tick 0x58F0C0 (pfxsim.hxx pfx_heap_trail_tick) in the same end-to-end run (case pfx_trail: pfx_run's
// harness with trail patterns and their generic processes among the quad / geom ones).
inline bool verify_pfx_trail( ) { return pfx_check_run( g_golden_ptr_words, "pfx_trail" ); }

// The boat processes on the heap (pfxsim.hxx pfx_process_tick: thrust 0x4CD5B0, hull 0x4CE760, boat spring 0x4CD980, the
// fountains' water snap; the binds 0x41A69B / 0x41A083 / 0x43BFD6) in the same end-to-end run (case pfx_boats).
inline bool verify_pfx_boats( ) { return pfx_check_run( g_golden_pbt_words, "pfx_boats", g_golden_pbt_boats, g_golden_pbt_boat_count ); }

// The water classes' getType slots (world/surface.hxx g_water_types) against retail (case water_types): per slot the
// descriptor it returns and the chain of names its descriptor walks to.
inline bool verify_water_types( )
{
    int bad = 0;
    const std::size_t n = sizeof( g_water_types ) / sizeof( g_water_types[ 0 ] );
    for ( std::size_t i = 0; i < g_golden_wty_count; ++i )
    {
        const golden_water_type_t& g = g_golden_wty[ i ];
        const water_type_t* t = nullptr;
        for ( std::size_t k = 0; k < n; ++k )
            if ( g_water_types[ k ].address == g.address )
                t = &g_water_types[ k ];
        if ( t == nullptr || t->descriptor != g.descriptor || std::strcmp( t->chain, g.chain ) != 0 )
        {
            ++bad;
            LOG_ERROR( "water_types 0x%X: descriptor / chain differ (retail 0x%X %s)", g.address, g.descriptor, g.chain );
        }
    }
    bad += n != g_golden_wty_count ? 1 : 0;
    if ( bad == 0 )
        LOG_SUCCESS( "g_water_types match the water classes' %d getType slots (descriptor and chain each)", static_cast< int >( n ) );
    else
        LOG_ERROR( "water_types FAILED %d", bad );
    return bad == 0;
}

// The effect-asset load (world/pfxload.hxx) against retail's own (case pfx_load): the race's VuPfxAsset documents
// (Hydroelectric, Generic) read from Base.apf and loaded - every node depth first in map order (path, node vtable,
// refcount +4, +8, child count +0x10, the words +0x30 .. size with the std::string members and the VuAssetProperty
// object pointers - quad +0x98 / +0x9C, trail +0x8C, retail heap addresses - zeroed on both sides; the strings
// compared as text), the texture / model hook calls in order, and 0x51DBD0 on every node path plus misses.
inline void pfx_load_rows( const pfx_runtime_t& rt, const pfx_library_t& lib, std::uint32_t node, const std::string& path,
    std::vector< std::pair< std::string, std::uint32_t > >& out )
{
    out.emplace_back( path, node );
    auto k = lib.kids.find( node );
    if ( k != lib.kids.end( ) )
        for ( const auto& c : k->second )
            pfx_load_rows( rt, lib, c.second, path + "/" + c.first, out );
}

inline bool verify_pfx_load( )
{
    c_archive archive;
    const std::string pack = world_check_pack( );
    if ( pack.empty( ) || !archive.open( pack.c_str( ) ) )
    {
        LOG_ERROR( "pfx_load: Base.apf did not open" );
        return false;
    }
    static pfx_runtime_t rt;
    pfx_runtime_init( rt );
    pfx_library_t lib;
    struct hook_t
    {
        int project, kind;
        std::uint32_t node, offset;
        std::string text;
    };
    struct log_t
    {
        int project = 0;
        std::vector< hook_t > calls;
    } log;
    pfx_load_hooks_t hooks;
    hooks.user = &log;
    hooks.texture = []( void* u, std::uint32_t node, std::uint32_t offset, const std::string& name ) {
        log_t& l = *static_cast< log_t* >( u );
        l.calls.push_back( { l.project, 0, node, offset, name } );
    };
    hooks.model = []( void* u, std::uint32_t node, const std::string& name ) {
        log_t& l = *static_cast< log_t* >( u );
        l.calls.push_back( { l.project, 1, node, 0u, name } );
    };
    int failed = 0, words = 0;
    for ( std::size_t p = 0; p < g_golden_pld_project_count; ++p )
    {
        std::vector< std::uint8_t > blob;
        log.project = static_cast< int >( p );
        if ( !archive.read( g_golden_pld_projects[ p ].asset, blob ) ||
             pfx_load_asset( rt, lib, g_golden_pld_projects[ p ].name, blob, hooks ) == 0u )
        {
            LOG_ERROR( "pfx_load: %s did not load", g_golden_pld_projects[ p ].asset );
            return false;
        }
    }
    std::vector< std::pair< std::string, std::uint32_t > > rows;
    for ( const auto& pr : lib.projects )
        pfx_load_rows( rt, lib, pr.second, pr.first, rows );
    std::map< std::uint32_t, std::string > path_of;
    for ( const auto& r : rows )
        path_of[ r.second ] = r.first;
    if ( rows.size( ) != g_golden_pld_node_count )
    {
        LOG_ERROR( "pfx_load: %d nodes, retail %d", static_cast< int >( rows.size( ) ), static_cast< int >( g_golden_pld_node_count ) );
        return false;
    }
    std::map< int, std::map< std::uint32_t, std::string > > texts;
    for ( std::size_t i = 0; i < g_golden_pld_text_count; ++i )
        texts[ g_golden_pld_texts[ i ].row ][ g_golden_pld_texts[ i ].offset ] = g_golden_pld_texts[ i ].text;
    for ( std::size_t i = 0; i < rows.size( ); ++i )
    {
        const golden_pfx_node_t& g = g_golden_pld_nodes[ i ];
        const std::uint32_t a = rows[ i ].second;
        pfx_heap_t& h = rt.heap;
        int bad = rows[ i ].first != g.path || h[ a ] != g.vtable || h[ a + 4 ] != g.w4 || h[ a + 8 ] != g.w8 || h[ a + 0x10 ] != g.count;
        const pfx_node_type_t* t = pfx_node_type_of( h[ a ] );
        std::vector< std::uint32_t > masked, unknown;
        if ( t != nullptr )
            for ( int k = 0; k < t->prop_count; ++k )
            {
                if ( t->props[ k ].kind == pfx_prop_name || t->props[ k ].kind == pfx_prop_texture )
                    for ( std::uint32_t w = 0; w < 6; ++w )
                        masked.push_back( t->props[ k ].offset + 4u * w );
                if ( t->props[ k ].kind == pfx_prop_vec3 || t->props[ k ].kind == pfx_prop_rot3 )
                    unknown.push_back( t->props[ k ].offset + 12u );   // UNKNOWN: the stored w lane (pfxload.hxx)
            }
        if ( h[ a ] == 0x7505ACu )
            masked.insert( masked.end( ), { 0x98u, 0x9Cu } );
        if ( h[ a ] == 0x750294u )
            masked.push_back( 0x8Cu );
        for ( unsigned w = 0; w < g.nwords; ++w )
        {
            const std::uint32_t off = 0x30u + 4u * w;
            if ( std::find( unknown.begin( ), unknown.end( ), off ) != unknown.end( ) )
                continue;
            const std::uint32_t ours = std::find( masked.begin( ), masked.end( ), off ) != masked.end( ) ? 0u : h[ a + off ];
            bad += ours != g_golden_pld_words[ g.first + w ];
            ++words;
        }
        const auto& want = texts[ static_cast< int >( i ) ];
        for ( const auto& tx : want )
        {
            auto s = lib.strings.find( { a, tx.first } );
            bad += s == lib.strings.end( ) || s->second != tx.second;
        }
        if ( bad )
        {
            if ( failed < 6 )
                LOG_ERROR( "pfx_load: node %s differs from retail's load (%d)", g.path, bad );
            ++failed;
        }
    }
    int hook_bad = log.calls.size( ) != g_golden_pld_hook_count;
    for ( std::size_t i = 0; i < g_golden_pld_hook_count && i < log.calls.size( ); ++i )
    {
        const golden_pfx_hook_t& g = g_golden_pld_hooks[ i ];
        const hook_t& c = log.calls[ i ];
        hook_bad += c.project != g.project || c.kind != g.kind || path_of[ c.node ] != g.path || c.offset != g.offset || c.text != g.text;
    }
    int lookup_bad = 0;
    for ( std::size_t i = 0; i < g_golden_pld_lookup_count; ++i )
    {
        const std::uint32_t n = pfx_find( lib, g_golden_pld_lookups[ i ].query );
        lookup_bad += ( n == 0u ? std::string( ) : path_of[ n ] ) != g_golden_pld_lookups[ i ].found;
    }
    if ( failed == 0 && hook_bad == 0 && lookup_bad == 0 )
        LOG_SUCCESS( "pfx_load builds retail's node trees from %d VuPfxAsset documents: %d nodes, %d words, %d strings, %d hook calls, "
                     "%d path lookups exact",
            static_cast< int >( g_golden_pld_project_count ), static_cast< int >( rows.size( ) ), words,
            static_cast< int >( g_golden_pld_text_count ), static_cast< int >( g_golden_pld_hook_count ),
            static_cast< int >( g_golden_pld_lookup_count ) );
    else
        LOG_ERROR( "pfx_load FAILED: %d nodes, %d hook calls, %d lookups differ", failed, hook_bad, lookup_bad );
    return failed == 0 && hook_bad == 0 && lookup_bad == 0;
}

// VuPfxManager (world/pfxmgr.hxx) against retail (case pfx_manager): init, then 70 planned operations - createEffect
// (the instance answer planned), the "Final" tick, a release by entry, releaseAll, new instance answers - on scripted
// instances; after each the manager's words, every entry's words and the hook log (draw add / remove / update, the
// instance tick / release, the create request) in order. The draw component's +0x30 / +0x40 before its first AABB
// copy are UNKNOWN (not compared).
inline bool verify_pfx_manager( )
{
    struct script_t
    {
        std::vector< std::vector< std::uint32_t > > insts;   // state, count, AABB 8
        std::uint32_t answer = 0;
        std::vector< std::uint32_t > log;
    } s;
    pfx_manager_ops_t ops;
    ops.user = &s;
    static auto name_index = []( const char* p ) -> std::uint32_t {
        for ( std::uint32_t i = 0; i < sizeof( g_golden_pmg_names ) / sizeof( g_golden_pmg_names[ 0 ] ); ++i )
            if ( std::strcmp( g_golden_pmg_names[ i ], p ) == 0 )
                return i;
        return 0xFFFFu;
    };
    ops.create = []( void* u, const char* p ) {
        script_t& x = *static_cast< script_t* >( u );
        x.log.insert( x.log.end( ), { 4u, name_index( p ) } );
        return x.answer;
    };
    ops.release = []( void* u, std::uint32_t i ) { static_cast< script_t* >( u )->log.insert( static_cast< script_t* >( u )->log.end( ), { 5u, i - 1u } ); };
    ops.tick = []( void* u, std::uint32_t i, float dt ) {
        std::uint32_t b;
        std::memcpy( &b, &dt, 4 );
        static_cast< script_t* >( u )->log.insert( static_cast< script_t* >( u )->log.end( ), { 6u, i - 1u, b } );
    };
    ops.state = []( void* u, std::uint32_t i ) { return static_cast< script_t* >( u )->insts[ i - 1 ][ 0 ]; };
    ops.count = []( void* u, std::uint32_t i ) { return static_cast< script_t* >( u )->insts[ i - 1 ][ 1 ]; };
    ops.aabb = []( void* u, std::uint32_t i ) -> const std::uint32_t* { return static_cast< script_t* >( u )->insts[ i - 1 ].data( ) + 2; };
    ops.draw_add = []( void* u, int e ) { static_cast< script_t* >( u )->log.insert( static_cast< script_t* >( u )->log.end( ), { 1u, static_cast< std::uint32_t >( e ) } ); };
    ops.draw_remove = []( void* u, int e ) { static_cast< script_t* >( u )->log.insert( static_cast< script_t* >( u )->log.end( ), { 2u, static_cast< std::uint32_t >( e ) } ); };
    ops.draw_update = []( void* u, int e ) { static_cast< script_t* >( u )->log.insert( static_cast< script_t* >( u )->log.end( ), { 3u, static_cast< std::uint32_t >( e ) } ); };
    ops.register_tick = []( void* u ) { static_cast< script_t* >( u )->log.push_back( 0u ); };
    const unsigned* w = g_golden_pmg_words;
    std::size_t at = 0;
    pfx_manager_t m;
    int failed = 0, compared = 0;
    const auto ix = []( int e ) { return e < 0 ? 0xFFFFFFFEu : static_cast< std::uint32_t >( e ); };
    auto check_state = [&]( const char* what, int step ) {
        const std::uint32_t head[ 12 ] = { 0u, m.capacity, 0u, m.mask, m.bits, m.serial_cap, ix( m.free_first ), ix( m.free_last ),
            m.free_count, ix( m.active_first ), ix( m.active_last ), m.active_count };
        int bad = 0;
        for ( int k = 0; k < 12; ++k, ++compared )
            bad += head[ k ] != w[ at + k ];
        at += 12;
        for ( const pfx_manager_entry_t& x : m.entries )
        {
            const std::uint32_t ours[ 16 ] = { x.flags, ix( x.next ), ix( x.prev ), x.instance ? x.instance - 1u : 0xFFFFFFFFu, x.index, x.serial,
                static_cast< std::uint32_t >( x.reflect ) | ( static_cast< std::uint32_t >( x.b1d ) << 8 ) |
                    ( static_cast< std::uint32_t >( x.registered ) << 16 ) | ( static_cast< std::uint32_t >( x.b1f ) << 24 ),
                x.w20, x.aabb[ 0 ], x.aabb[ 1 ], x.aabb[ 2 ], x.aabb[ 3 ], x.aabb[ 4 ], x.aabb[ 5 ], x.aabb[ 6 ], x.aabb[ 7 ] };
            for ( int k = 0; k < 16; ++k )
            {
                if ( !x.aabb_known && ( k == 11 || k == 15 ) )
                    continue;   // UNKNOWN component +0x30 / +0x40
                bad += ours[ k ] != w[ at + k ];
                ++compared;
            }
            at += 16;
        }
        const std::uint32_t n = w[ at++ ];
        std::vector< std::uint32_t > want( w + at, w + at );
        for ( std::uint32_t k = 0; k < n; ++k )
        {
            const std::uint32_t code = w[ at++ ];
            want.push_back( code );
            const int args = code == 0 ? 0 : code == 6 ? 2 : 1;
            for ( int j = 0; j < args; ++j )
                want.push_back( w[ at++ ] );
        }
        bad += want != s.log;
        ++compared;
        if ( bad )
        {
            if ( failed < 6 )
                LOG_ERROR( "pfx_manager: %s (step %d) differs from retail (%d)", what, step, bad );
            ++failed;
        }
    };
    pfx_manager_init( m, ops );
    check_state( "init", -1 );
    const std::uint32_t nops = w[ at++ ];
    for ( std::uint32_t i = 0; i < nops; ++i )
    {
        s.log.clear( );
        const std::uint32_t code = w[ at++ ];
        if ( code == 0 )
        {
            const std::uint32_t name = w[ at++ ], answer = w[ at++ ], ret = w[ at++ ];
            s.answer = 0u;
            if ( answer != 0xFFFFFFFFu )
            {
                s.insts.emplace_back( w + at, w + at + 10 );
                at += 10;
                s.answer = answer + 1u;
            }
            const int e = pfx_manager_create( m, g_golden_pmg_names[ name ], ops );
            failed += ix( e ) != ret ? 1 : 0;
        }
        else if ( code == 1 )
        {
            float dt;
            std::memcpy( &dt, &w[ at++ ], 4 );
            pfx_manager_tick( m, dt, ops );
        }
        else if ( code == 2 )
            pfx_manager_release( m, static_cast< int >( w[ at++ ] ), ops );
        else if ( code == 3 )
            pfx_manager_release_all( m, ops );
        else if ( code == 4 )
            for ( auto& inst : s.insts )
            {
                inst.assign( w + at, w + at + 10 );
                at += 10;
            }
        check_state( "operation", static_cast< int >( i ) );
    }
    if ( at != g_golden_pmg_size )
    {
        LOG_ERROR( "pfx_manager: golden layout read %d of %d words", static_cast< int >( at ), static_cast< int >( g_golden_pmg_size ) );
        return false;
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuPfxManager matches retail's 0x52B7B0 / 0x52BCD0 / 0x52BBF0 / 0x52BC30 / 0x52BD50 / 0x52BB60 over %d operations: "
                     "%d words and hook logs exact",
            static_cast< int >( nops ), compared );
    else
        LOG_ERROR( "pfx_manager FAILED %d of %d steps", failed, static_cast< int >( nops ) + 1 );
    return failed == 0;
}

// water_point_height (world/query.hxx) against VuWater's base-mode one-point query 0x529140 - the fountain's water snap -
// on Hydroelectric's built water (case water_snap): every out word (+0x1C, retail's stale stack, skipped; +0x24 must stay
// as the caller had it), the slot 3 calls in order (wave, tag, ctx+0x60, ctx+0x64), kernels water_view_kernel.
inline bool verify_water_snap( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "water_snap: the document did not load" );
        return false;
    }
    water_built_t built{};
    water_build_world( *tree, built, []( entity_t& e ) { return entity_transform( e ); } );
    water_view_t view{};
    water_view_build( built, view );
    struct logged_t
    {
        const water_view_t* view;
        std::vector< std::uint32_t > log;
    } logged{ &view, {} };
    const water_kernel_fn kernel = []( void* user, int wave, water_query_t& ctx ) {
        logged_t& l = *static_cast< logged_t* >( user );
        l.log.insert( l.log.end( ), { static_cast< std::uint32_t >( wave ), ctx.tag, ctx.flag96, ctx.flag100 } );
        return water_view_kernel( const_cast< water_view_t* >( l.view ), wave, ctx );
    };
    int failed = 0, found = 0, calls = 0, skipped = 0, moved = 0;
    for ( std::size_t index = 0; index < g_golden_wsn_count; ++index )
    {
        const unsigned* s = g_golden_wsn_words + g_golden_wsn_offsets[ index ];
        view.water.skip_waves = s[ 0 ] != 0u;
        skipped += s[ 0 ] ? 1 : 0;
        float point[ 4 ];
        std::memcpy( point, s + 1, 16 );
        std::uint32_t out[ 11 ];
        std::memset( out, 0x77, sizeof( out ) );
        logged.log.clear( );
        const bool reached = water_point_height( view.water, point, kernel, &logged, *reinterpret_cast< water_vertex_t* >( out ) );
        const char* what = reached ? nullptr : "a path not recreated was reached";
        for ( int k = 0; k < 11; ++k )
            if ( !what && k != 7 && out[ k ] != s[ 5 + k ] )
                what = "an out word";
        found += s[ 5 + 8 ] != 0xFEFFFFFFu ? 1 : 0;
        moved += ( s[ 5 + 4 ] | s[ 5 + 5 ] | s[ 5 + 6 ] ) != 0u ? 1 : 0;
        const unsigned n = s[ 16 ];
        calls += static_cast< int >( n );
        if ( !what && logged.log.size( ) != 4 * n )
            what = "the number of slot 3 calls";
        for ( unsigned k = 0; k < 4 * n && !what; ++k )
            if ( logged.log[ k ] != s[ 17 + k ] )
                what = "a slot 3 call";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "water_snap: query %d: %s", static_cast< int >( index ), what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "water_point_height matches VuWater's base-mode one-point query 0x529140 (0x52AEC0 / 0x413500(0) / 0x52B0E0) on %d "
                     "queries: %d on a surface, %d displaced by waves, %d slot 3 calls, %d with waves skipped - every out word but the stale "
                     "+0x1C",
            static_cast< int >( g_golden_wsn_count ), found, moved, calls, skipped );
    else
        LOG_ERROR( "water_snap FAILED %d of %d", failed, static_cast< int >( g_golden_wsn_count ) );
    return failed == 0;
}

// pfx_reload_project / pfx_remove_project and the node dtors (world/pfxload.hxx) against VuPfxImpl's 0x51D870 / 0x51D9C0
// and retail's node slot 0s (case pfx_reload): the race's two documents loaded, three nodes held (refcount 2), then reload
// Generic with Hydroelectric's document, remove Hydroelectric, reload / remove a missing name. Compared: each return, the
// events in order (0x563C00's entries, the geom model resets, the texture asset releases: by the node's path before the
// operation and the offset), every remaining node (path, vtable, +4, child count), the held nodes' refcounts.
inline bool verify_pfx_reload( )
{
    c_archive archive;
    const std::string pack = world_check_pack( );
    if ( pack.empty( ) || !archive.open( pack.c_str( ) ) )
    {
        LOG_ERROR( "pfx_reload: Base.apf did not open" );
        return false;
    }
    static pfx_runtime_t rt;
    pfx_runtime_init( rt );
    pfx_library_t lib;
    std::map< std::string, value_t > docs;
    for ( const char* asset : { "VuPfxAsset/Hydroelectric", "VuPfxAsset/Generic" } )
    {
        std::vector< std::uint8_t > blob;
        const std::string name = std::string( asset ).substr( 11 );
        if ( !archive.read( asset, blob ) || !pfx_asset_document( blob, docs[ asset ] ) || pfx_add_project( rt, lib, name, docs[ asset ] ) == 0u )
        {
            LOG_ERROR( "pfx_reload: %s did not load", asset );
            return false;
        }
    }
    auto paths = [ & ]( ) {
        std::vector< std::pair< std::string, std::uint32_t > > rows;
        for ( const auto& pr : lib.projects )
            pfx_load_rows( rt, lib, pr.second, pr.first, rows );
        return rows;
    };
    std::map< std::string, std::uint32_t > held;
    for ( const auto& r : paths( ) )
        for ( std::size_t i = 0; i < g_golden_prl_ref_count; ++i )
            if ( r.first == g_golden_prl_refs[ i ].path )
            {
                rt.heap[ r.second + 4 ] = 2u;
                held[ r.first ] = r.second;
            }
    struct log_t
    {
        std::map< std::uint32_t, std::string > path_of;
        std::vector< std::tuple< int, std::string, std::uint32_t > > events;
    } log;
    pfx_release_hooks_t hooks;
    hooks.user = &log;
    hooks.destroyed = []( void* u, std::uint32_t node ) {
        log_t& l = *static_cast< log_t* >( u );
        l.events.emplace_back( 0, l.path_of[ node ], 0u );
    };
    hooks.model = []( void* u, std::uint32_t node ) {
        log_t& l = *static_cast< log_t* >( u );
        l.events.emplace_back( 1, l.path_of[ node ], 0u );
    };
    hooks.texture = []( void* u, std::uint32_t node, std::uint32_t offset ) {
        log_t& l = *static_cast< log_t* >( u );
        l.events.emplace_back( 2, l.path_of[ node ], offset );
    };
    int failed = 0, events = 0;
    for ( std::size_t i = 0; i < g_golden_prl_op_count; ++i )
    {
        const golden_pfx_reload_op_t& op = g_golden_prl_ops[ i ];
        log.path_of.clear( );
        for ( const auto& r : paths( ) )
            log.path_of[ r.second ] = r.first;
        log.events.clear( );
        unsigned ret = 0;
        if ( op.remove )
            pfx_remove_project( rt, lib, op.name, hooks );
        else
            ret = pfx_reload_project( rt, lib, op.name, docs[ op.doc ], pfx_load_hooks_t( ), hooks ) ? 1u : 0u;
        int bad = ret != op.ret || log.events.size( ) != op.count;
        for ( unsigned e = 0; e < op.count && e < log.events.size( ); ++e )
        {
            const golden_pfx_reload_event_t& g = g_golden_prl_events[ op.first + e ];
            bad += std::get< 0 >( log.events[ e ] ) != g.kind || std::get< 1 >( log.events[ e ] ) != g.path || std::get< 2 >( log.events[ e ] ) != g.offset;
        }
        events += static_cast< int >( op.count );
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "pfx_reload: %s %s: %d differences (%d events, retail %u)", op.remove ? "remove" : "reload", op.name, bad,
                static_cast< int >( log.events.size( ) ), op.count );
        }
    }
    const auto rows = paths( );
    int row_bad = rows.size( ) != g_golden_prl_row_count;
    for ( std::size_t i = 0; i < rows.size( ) && i < g_golden_prl_row_count; ++i )
    {
        const golden_pfx_reload_row_t& g = g_golden_prl_rows[ i ];
        const std::uint32_t a = rows[ i ].second;
        row_bad += rows[ i ].first != g.path || rt.heap[ a ] != g.vtable || rt.heap[ a + 4 ] != g.w4 || rt.heap[ a + 0x10 ] != g.count;
    }
    for ( std::size_t i = 0; i < g_golden_prl_ref_count; ++i )
        row_bad += held.count( g_golden_prl_refs[ i ].path ) == 0 || rt.heap[ held[ g_golden_prl_refs[ i ].path ] + 4 ] != g_golden_prl_refs[ i ].refs;
    if ( row_bad )
    {
        ++failed;
        LOG_ERROR( "pfx_reload: %d differences in the nodes after (%d nodes, retail %d)", row_bad, static_cast< int >( rows.size( ) ),
            static_cast< int >( g_golden_prl_row_count ) );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "pfx_reload_project / pfx_remove_project and the node dtors match VuPfxImpl 0x51D870 / 0x51D9C0 and 0x563C00 et al.: "
                     "%d operations, %d destruction events in order, %d nodes after, %d held refcounts",
            static_cast< int >( g_golden_prl_op_count ), events, static_cast< int >( rows.size( ) ), static_cast< int >( g_golden_prl_ref_count ) );
    else
        LOG_ERROR( "pfx_reload FAILED %d", failed );
    return failed == 0;
}

inline bool verify_pfx_all( )
{
    bool ok = true;
    ok = verify_pfx_reload( ) && ok;
    ok = verify_water_snap( ) && ok;
    ok = verify_pfx_boats( ) && ok;
    ok = verify_pfx_manager( ) && ok;
    ok = verify_pfx_load( ) && ok;
    ok = verify_water_types( ) && ok;
    ok = verify_pfx_run( ) && ok;
    ok = verify_pfx_trail( ) && ok;
    ok = verify_pfx_lifecycle( ) && ok;
    ok = verify_pattern_draw( ) && ok;
    ok = verify_fountain_init( ) && ok;
    return ok;
}
