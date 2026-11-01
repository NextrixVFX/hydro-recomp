#pragma once

#include <workspace/core/collision/raytest.hxx>
#include <workspace/core/boat/rays.hxx>

#include <array>
#include <cstring>
#include <memory>
#include <vector>

// The ray-test checks (boat batch 15): btCollisionWorld::rayTest and what it
// reaches, against the oracle (cases/race.py). Included from checks/race.hxx;
// verify_race_all runs them.

struct ray_check_shape_t
{
    convex_shape_t shape{};
    striding_mesh_t mesh{};
    std::vector< float > verts;
    std::vector< std::uint16_t > ids;
};

inline void ray_check_read_shape( const unsigned*& g, ray_check_shape_t& s )
{
    auto f = [ & ]( ) { float x; std::memcpy( &x, g++, 4 ); return x; };
    s.shape.type = static_cast< int >( *g++ );
    for ( float& x : s.shape.local_scaling )
        x = f( );
    for ( float& x : s.shape.implicit )
        x = f( );
    s.shape.margin = f( );
    s.shape.up_axis = static_cast< int >( *g++ );
    const unsigned nv = *g++;
    s.verts.resize( 4 * nv );
    for ( float& x : s.verts )
        x = f( );
    const unsigned ni = *g++;
    s.ids.resize( ni );
    for ( auto& x : s.ids )
        x = static_cast< std::uint16_t >( *g++ );
}

// verify_ray_test: per vector the world rebuilt from the golden words - the
// objects (their shapes, transforms, proxies), the two broadphase trees node
// for node - and collision_world_ray_test run with a callback that REPLAYS
// retail's answers (needsCollision's per object, addSingleResult's return
// and closestHitFraction) and compares every call (which, in order, and the
// LocalRayResult's object, shape info, normal and fraction words, the
// normalInWorld flag) and the closest at the end.
inline bool verify_ray_test( )
{
    const unsigned* g = g_golden_ray_test_words;
    const unsigned nv = *g++;
    int failed = 0, calls = 0, hits = 0, adapter_rays = 0, adapter_hits = 0, adapter_failed = 0;
    for ( unsigned v = 0; v < nv; ++v )
    {
        const unsigned n = *g++;
        std::vector< ray_check_shape_t > shapes( n );
        std::vector< compound_shape_t > compounds( n );
        std::vector< std::array< float, 16 > > transforms( n );
        std::vector< collision_object_t > objects( n );
        std::vector< std::unique_ptr< dbvt_proxy_t > > proxies( n );
        for ( unsigned k = 0; k < n; ++k )
        {
            const int kind = static_cast< int >( *g++ );
            std::memcpy( transforms[ k ].data( ), g, 64 );
            g += 16;
            proxies[ k ] = std::make_unique< dbvt_proxy_t >( );
            proxies[ k ]->client = &objects[ k ];
            proxies[ k ]->group = static_cast< short >( *g++ );
            proxies[ k ]->mask = static_cast< short >( *g++ );
            ray_check_read_shape( g, shapes[ k ] );
            ray_check_shape_t& s = shapes[ k ];
            s.mesh.scaling[ 0 ] = s.shape.local_scaling[ 0 ];
            s.mesh.scaling[ 1 ] = s.shape.local_scaling[ 1 ];
            s.mesh.scaling[ 2 ] = s.shape.local_scaling[ 2 ];
            s.mesh.scaling[ 3 ] = s.shape.local_scaling[ 3 ];
            s.mesh.vertices = reinterpret_cast< const float( * )[ 4 ] >( s.verts.data( ) );
            s.mesh.num_vertices = static_cast< int >( s.verts.size( ) / 4 );
            s.mesh.indices = s.ids.data( );
            s.mesh.index_count = static_cast< int >( s.ids.size( ) );
            if ( s.shape.type == 3 )
                s.shape.mesh = &s.mesh;
            objects[ k ].body.transform = transforms[ k ].data( );
            if ( kind == 31 )
            {
                compound_shape_t& c = compounds[ k ];
                compound_child_t child{};
                std::memcpy( child.transform, g, 64 );
                g += 16;
                child.shape = &s.shape;
                child.type = s.shape.type;
                c.children.push_back( child );
                c.has_root = true;
                std::memcpy( c.root_min, g, 16 );
                std::memcpy( c.root_max, g + 4, 16 );
                g += 8;
                c.root_data = 0;
                objects[ k ].body.compound = &c;
            }
            else
                objects[ k ].body.convex = &s.shape;
        }
        collision_world_t w;
        w.broadphase = std::make_unique< dbvt_broadphase_t >( );
        std::vector< std::vector< dbvt_node_t > > nodes( 2 );
        for ( int set = 0; set < 2; ++set )
        {
            const unsigned nr = *g++;
            nodes[ set ].resize( nr );
            for ( unsigned r = 0; r < nr; ++r )
            {
                const int at = static_cast< int >( g[ 0 ] ), c0 = static_cast< int >( g[ 2 ] ), c1 = static_cast< int >( g[ 3 ] );
                const int leaf = static_cast< int >( g[ 4 ] );
                dbvt_node_t& nd = nodes[ set ][ static_cast< std::size_t >( at ) ];
                std::memcpy( nd.mn, g + 5, 16 );
                std::memcpy( nd.mx, g + 9, 16 );
                if ( leaf >= 0 )
                {
                    nd.childs[ 0 ] = nullptr;
                    nd.childs[ 1 ] = nullptr;
                    nd.data = static_cast< broadphase_proxy_t* >( proxies[ static_cast< std::size_t >( leaf ) ].get( ) );
                }
                else
                {
                    nd.childs[ 0 ] = &nodes[ set ][ static_cast< std::size_t >( c0 ) ];
                    nd.childs[ 1 ] = &nodes[ set ][ static_cast< std::size_t >( c1 ) ];
                }
                g += 13;
            }
            w.broadphase->sets[ set ].root = nr ? &nodes[ set ][ 0 ] : nullptr;
        }
        float from[ 4 ], to[ 4 ];
        std::memcpy( from, g, 16 );
        std::memcpy( to, g + 4, 16 );
        g += 8;
        ray_callback_t cb;
        std::memcpy( &cb.closest_hit_fraction, g++, 4 );
        const unsigned mode = *g++;   // the recorder's (its answers are in the log)
        cb.group = static_cast< short >( *g++ );
        cb.mask = static_cast< short >( *g++ );
        cb.flags = *g++;
        const unsigned* accept = g;
        g += n;
        const unsigned nlog = *g++;
        std::vector< std::vector< unsigned > > glog( nlog );
        for ( auto& e : glog )
        {
            const unsigned len = g[ 0 ] == 1 ? 3 : 13;
            e.assign( g, g + len );
            g += len;
        }
        const unsigned closest_after = *g++;
        struct replay_t
        {
            const std::vector< std::vector< unsigned > >* glog;
            std::vector< std::vector< unsigned > > log;
            const std::vector< collision_object_t >* objects;
            const std::vector< std::unique_ptr< dbvt_proxy_t > >* proxies;
            const unsigned* accept;
        } rp{ &glog, {}, &objects, &proxies, accept };
        cb.user = &rp;
        cb.needs_collision = []( void* u, ray_callback_t&, const broadphase_proxy_t& p ) {
            replay_t& r = *static_cast< replay_t* >( u );
            unsigned k = 0;
            while ( k < r.proxies->size( ) && static_cast< const broadphase_proxy_t* >( ( *r.proxies )[ k ].get( ) ) != &p )
                ++k;
            const unsigned ans = k < r.proxies->size( ) ? r.accept[ k ] : 0u;
            r.log.push_back( { 1u, k, ans } );
            return ans != 0;
        };
        cb.add_single_result = []( void* u, ray_callback_t& self, ray_local_result_t& res, bool niw ) {
            replay_t& r = *static_cast< replay_t* >( u );
            const unsigned k = static_cast< unsigned >( res.object - r.objects->data( ) );
            std::vector< unsigned > e = { 2u, k, res.info ? 1u : 0u, res.info ? static_cast< unsigned >( res.info->part ) : 0u,
                                          res.info ? static_cast< unsigned >( res.info->triangle ) : 0u };
            for ( int j = 0; j < 4; ++j )
            {
                unsigned x;
                std::memcpy( &x, &res.normal[ j ], 4 );
                e.push_back( x );
            }
            unsigned fr;
            std::memcpy( &fr, &res.fraction, 4 );
            e.push_back( fr );
            e.push_back( niw ? 1u : 0u );
            // retail's answer for this call (by position), else the fraction
            const std::size_t at = r.log.size( );
            unsigned ans = fr, closest = fr;
            if ( at < r.glog->size( ) && ( *r.glog )[ at ].size( ) == 13 )
            {
                ans = ( *r.glog )[ at ][ 11 ];
                closest = ( *r.glog )[ at ][ 12 ];
            }
            e.push_back( ans );
            e.push_back( closest );
            r.log.push_back( e );
            std::memcpy( &self.closest_hit_fraction, &closest, 4 );
            float a;
            std::memcpy( &a, &ans, 4 );
            return a;
        };
        // boat_ray_hits (boat/rays.hxx) on the same world where the recorder
        // behaved as the RayTestWrapper does (mode 1: closest kept, 1.0
        // returned; closest 1.0 at the start): its hits = retail's
        // addSingleResult calls on the objects needsCollision accepted.
        unsigned start_closest;
        std::memcpy( &start_closest, &cb.closest_hit_fraction, 4 );
        if ( mode == 1 && start_closest == 0x3F800000u )
        {
            boat_ray_world_t rw;
            rw.world = &w;
            rw.user = &objects;
            rw.image = []( void*, const collision_object_t& o ) { return reinterpret_cast< const std::uint8_t* >( &o ); };
            const ai_ray_hit_t* got = nullptr;
            const int nh = boat_ray_hits( &rw, from, to, &got );
            std::vector< std::vector< unsigned > > mine, want;
            for ( int k = 0; k < nh; ++k )
            {
                const unsigned obj = static_cast< unsigned >( reinterpret_cast< const collision_object_t* >( got[ k ].body ) - objects.data( ) );
                if ( !accept[ obj ] )
                    continue;
                std::vector< unsigned > e = { obj, got[ k ].world ? 1u : 0u };
                for ( int j = 0; j < 4; ++j )
                {
                    unsigned x;
                    std::memcpy( &x, &got[ k ].normal[ j ], 4 );
                    e.push_back( x );
                }
                unsigned fr;
                std::memcpy( &fr, &got[ k ].fraction, 4 );
                e.push_back( fr );
                mine.push_back( e );
            }
            for ( const auto& e : glog )
                if ( e[ 0 ] == 2 )
                    want.push_back( { e[ 1 ], e[ 10 ], e[ 5 ], e[ 6 ], e[ 7 ], e[ 8 ], e[ 9 ] } );
            ++adapter_rays;
            adapter_hits += static_cast< int >( want.size( ) );
            if ( rw.unknown || mine != want )
            {
                if ( ++adapter_failed <= 4 )
                    LOG_ERROR( "ray_hits %u: %d hits (retail %d)", v, static_cast< int >( mine.size( ) ), static_cast< int >( want.size( ) ) );
            }
        }
        const int status = collision_world_ray_test( w, from, to, cb );
        unsigned mine_closest;
        std::memcpy( &mine_closest, &cb.closest_hit_fraction, 4 );
        calls += static_cast< int >( glog.size( ) );
        for ( const auto& e : glog )
            hits += e[ 0 ] == 2 ? 1 : 0;
        const bool same = status == 1 && rp.log == glog && mine_closest == closest_after;
        if ( !same && ++failed <= 8 )
        {
            std::size_t at = 0;
            while ( at < rp.log.size( ) && at < glog.size( ) && rp.log[ at ] == glog[ at ] )
                ++at;
            LOG_ERROR( "ray_test %u: st %d calls %d/%d at %d closest %08X/%08X", v, status, static_cast< int >( rp.log.size( ) ),
                       static_cast< int >( glog.size( ) ), static_cast< int >( at ), mine_closest, closest_after );
            if ( at < rp.log.size( ) && at < glog.size( ) && rp.log[ at ].size( ) == 13 && glog[ at ].size( ) == 13 )
                LOG_ERROR( "n %08X %08X %08X f %08X / %08X %08X %08X f %08X", rp.log[ at ][ 5 ], rp.log[ at ][ 6 ], rp.log[ at ][ 7 ],
                           rp.log[ at ][ 9 ], glog[ at ][ 5 ], glog[ at ][ 6 ], glog[ at ][ 7 ], glog[ at ][ 9 ] );
        }
    }
    bool adapter_ok = adapter_failed == 0;
    if ( adapter_ok )
        LOG_SUCCESS( "boat_ray_hits (VuDynamicsRayTest 0x50CFB0's ray on the race world, boat/rays.hxx) gives retail's hits: %d rays, %d hits", adapter_rays,
                     adapter_hits );
    else
        LOG_ERROR( "ray_hits FAILED %d of %d rays", adapter_failed, adapter_rays );
    if ( failed == 0 )
    {
        LOG_SUCCESS( "btCollisionWorld::rayTest 0x54F590 (btDbvtBroadphase 0x56E040 / 0x56EE90, btSingleRayCallback 0x54F330 / 0x54F4E0, "
                     "rayTestSingle 0x54C9B0's convex and compound paths, btSubsimplexConvexCast 0x5B6FC0, btDbvt::rayTest 0x551F10, "
                     "RayTester 0x54D700, LocalInfoAdder2 0x54D6A0, the hull's slot 14 0x526820) matches retail: %u rays, %d callback calls, %d hits",
                     nv, calls, hits );
        return adapter_ok;
    }
    LOG_ERROR( "ray_test FAILED %d of %u rays", failed, nv );
    return false;
}

// verify_simplex_ctor: simplex_construct on the allocator's random block =
// the block retail's btDefaultCollisionConfiguration ctor 0x56C880 left.
inline bool verify_simplex_ctor( )
{
    const unsigned* g = g_golden_simplex_ctor_words;
    const unsigned nv = *g++;
    int failed = 0;
    for ( unsigned v = 0; v < nv; ++v )
    {
        simplex_solver_t s;
        std::memcpy( &s, g, 400 );
        g += 100;
        simplex_construct( s );
        if ( std::memcmp( &s, g, 400 ) != 0 && ++failed <= 4 )
            LOG_ERROR( "simplex_ctor vector %u differs", v );
        g += 100;
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "btDefaultCollisionConfiguration's ctor 0x56C880 on its btVoronoiSimplexSolver (simplex_construct) matches retail: %u blocks", nv );
        return true;
    }
    LOG_ERROR( "simplex_ctor FAILED %d of %u blocks", failed, nv );
    return false;
}