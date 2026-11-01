#pragma once

#include <workspace/core/collision/raytest.hxx>
#include <workspace/core/boat/rays.hxx>
#include <workspace/core/boat/effects.hxx>
#include <workspace/core/collision/leaves.hxx>
#include <workspace/core/collision/teardown.hxx>
#include <workspace/core/collision/arena.hxx>

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
// verify_ray_bvh (boat batch 16): the same, on worlds holding the level's
// btBvhTriangleMeshShape (kind 21: Env_Hydroelectric_z09b_col, the BVH and
// mesh g_golden_bvh_assets[ 1 ]) - the type-21 branch, performRaycast and the
// triangle callback, with retail's addSingleResult answers fed back; the
// adapter is compared in mode 3 (the RayTestWrapper's answer).
inline bool verify_ray_table( const unsigned* g, const char* name, bool with_bvh );
inline bool verify_ray_test( )
{
    return verify_ray_table( g_golden_ray_test_words, "ray_test", false );
}

inline bool verify_ray_bvh( )
{
    return verify_ray_table( g_golden_ray_bvh_words, "ray_bvh", true );
}

inline bool verify_ray_table( const unsigned* g, const char* name, bool with_bvh )
{
    const unsigned nv = *g++;
    int failed = 0, calls = 0, hits = 0, adapter_rays = 0, adapter_hits = 0, adapter_failed = 0, mesh_hits = 0;
    const golden_bvh_asset_t& a = g_golden_bvh_assets[ 1 ];
    quantized_bvh_t level_bvh{};
    std::memcpy( level_bvh.aabb_min, a.m_header + 0, 16 );
    std::memcpy( level_bvh.aabb_max, a.m_header + 4, 16 );
    std::memcpy( level_bvh.quantization, a.m_header + 8, 16 );
    level_bvh.cur_node_index = static_cast< int >( a.m_header[ 13 ] );
    level_bvh.use_quantization = static_cast< uint8_t >( a.m_quantized );
    level_bvh.nodes = reinterpret_cast< const bvh_node_t* >( a.m_nodes );
    level_bvh.traversal_mode = a.m_traversal;
    striding_mesh_t level{};
    level.scaling[ 0 ] = level.scaling[ 1 ] = level.scaling[ 2 ] = 1.0f;
    level.vertices = reinterpret_cast< const float( * )[ 4 ] >( a.m_verts );
    level.num_vertices = a.m_nverts;
    level.indices = a.m_indices;
    level.index_count = a.m_nindices;
    const bvh_mesh_shape_t level_shape{ 21, 0.0f, &level, &level_bvh };
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
            if ( kind == 21 )
            {
                objects[ k ].body.transform = transforms[ k ].data( );
                objects[ k ].body.concave = &level_shape;
                continue;
            }
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
        if ( start_closest == 0x3F800000u && ( with_bvh ? mode == 3 && cb.flags == 0 : mode == 1 ) )
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
        {
            hits += e[ 0 ] == 2 ? 1 : 0;
            mesh_hits += e[ 0 ] == 2 && objects[ e[ 1 ] ].body.concave ? 1 : 0;
        }
        const bool same = status == 1 && rp.log == glog && mine_closest == closest_after;
        if ( !same && ++failed <= 8 )
        {
            std::size_t at = 0;
            while ( at < rp.log.size( ) && at < glog.size( ) && rp.log[ at ] == glog[ at ] )
                ++at;
            LOG_ERROR( "%s %u: st %d calls %d/%d at %d closest %08X/%08X", name, v, status, static_cast< int >( rp.log.size( ) ),
                       static_cast< int >( glog.size( ) ), static_cast< int >( at ), mine_closest, closest_after );
            if ( at < rp.log.size( ) && at < glog.size( ) && rp.log[ at ].size( ) == 13 && glog[ at ].size( ) == 13 )
                LOG_ERROR( "n %08X %08X %08X f %08X / %08X %08X %08X f %08X", rp.log[ at ][ 5 ], rp.log[ at ][ 6 ], rp.log[ at ][ 7 ],
                           rp.log[ at ][ 9 ], glog[ at ][ 5 ], glog[ at ][ 6 ], glog[ at ][ 7 ], glog[ at ][ 9 ] );
        }
    }
    bool adapter_ok = adapter_failed == 0;
    if ( adapter_ok )
        LOG_SUCCESS( "boat_ray_hits (VuDynamicsRayTest 0x50CFB0's ray on the race world, boat/rays.hxx) gives retail's hits%s: %d rays, %d hits",
                     with_bvh ? " on worlds with the level's BVH mesh" : "", adapter_rays, adapter_hits );
    else
        LOG_ERROR( "%s ray_hits FAILED %d of %d rays", name, adapter_failed, adapter_rays );
    if ( failed == 0 && with_bvh )
    {
        LOG_SUCCESS( "rayTestSingle 0x54C9B0's BVH-mesh path (the local ray 0x54CCC0, BridgeTriangleRaycastCallback 0x54D410, performRaycast "
                     "0x5AECB0, the quantized ray walk 0x587E60, processNode 0x5AED70, processTriangle 0x5B8500, reportHit 0x54D4D0) on the "
                     "level's Env_Hydroelectric_z09b_col matches retail: %u rays, %d callback calls, %d hits (%d on the mesh)", nv, calls, hits,
                     mesh_hits );
        return adapter_ok;
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "btCollisionWorld::rayTest 0x54F590 (btDbvtBroadphase 0x56E040 / 0x56EE90, btSingleRayCallback 0x54F330 / 0x54F4E0, "
                     "rayTestSingle 0x54C9B0's convex and compound paths, btSubsimplexConvexCast 0x5B6FC0, btDbvt::rayTest 0x551F10, "
                     "RayTester 0x54D700, LocalInfoAdder2 0x54D6A0, the hull's slot 14 0x526820) matches retail: %u rays, %d callback calls, %d hits",
                     nv, calls, hits );
        return adapter_ok;
    }
    LOG_ERROR( "%s FAILED %d of %u rays", name, failed, nv );
    return false;
}

// verify_compound_pair (boat batch 16): boat vs boat - compound_process_collision
// with BOTH bodies compounds (the nested swapped Compound algorithm the matrix's
// (3, 31) gives, engine collision/compound.hxx) over the case's four frames: per
// frame the outer child's kind and the nested one's, gNumManifold, the dispatcher's
// manifolds (points masked as verify_compound), result+4 and the ids.
inline bool verify_compound_pair( )
{
    const golden_bvh_asset_t& ha = g_golden_bvh_assets[ 0 ];
    striding_mesh_t hull_mesh{};
    hull_mesh.scaling[ 0 ] = hull_mesh.scaling[ 1 ] = hull_mesh.scaling[ 2 ] = 1.0f;
    hull_mesh.vertices = reinterpret_cast< const float( * )[ 4 ] >( ha.m_verts );
    hull_mesh.num_vertices = ha.m_nverts;
    hull_mesh.indices = ha.m_indices;
    hull_mesh.index_count = ha.m_nindices;
    const unsigned* g = g_golden_compound_pair_words;
    const unsigned nv = *g++;
    int failed = 0, nested = 0, contacts = 0, removals = 0;
    for ( unsigned v = 0; v < nv; ++v )
    {
        auto f = [ & ]( ) { float x; std::memcpy( &x, g++, 4 ); return x; };
        const bool use_box = *g++ != 0;
        convex_shape_t hull{};
        hull.type = 3;
        hull.local_scaling[ 0 ] = hull.local_scaling[ 1 ] = hull.local_scaling[ 2 ] = 1.0f;
        hull.margin = f( );
        float com[ 3 ], com2[ 3 ], pth[ 2 ];
        for ( float& x : com )
            x = f( );
        for ( float& x : com2 )
            x = f( );
        for ( float& x : pth )
            x = f( );
        static solver_rigid_t objects[ 2 ];
        for ( int k = 0; k < 2; ++k )
            objects[ k ] = solver_rigid_t{};
        objects[ 0 ].friction = f( );
        objects[ 1 ].friction = f( );
        objects[ 0 ].restitution = f( );
        objects[ 1 ].restitution = f( );
        convex_shape_t box{};
        box.type = 0;
        box.local_scaling[ 0 ] = box.local_scaling[ 1 ] = box.local_scaling[ 2 ] = 1.0f;
        for ( float& x : box.implicit )
            x = f( );
        box.margin = f( );
        for ( float& x : hull.local_aabb_min )
            x = f( );
        for ( float& x : hull.local_aabb_max )
            x = f( );
        hull.mesh = &hull_mesh;
        float tt[ 16 ];
        std::memcpy( tt, g, 64 );
        g += 16;
        simplex_solver_t simplex;
        std::memcpy( &simplex, g, 400 );
        g += 100;
        g_num_manifolds = static_cast< int >( *g++ );
        compound_shape_t a, b;
        const char* what = nullptr;
        int frame_no = -1;
        for ( int k = 0; k < 2; ++k )
        {
            float child_t[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 };
            for ( int j = 0; j < 3; ++j )
                child_t[ 12 + j ] = support_negate( k ? com2[ j ] : com[ j ] );
            if ( compound_add_child( k ? b : a, child_t, k && use_box ? box : hull ) != 1 )
                what = "addChildShape reached unrecreated code";
        }
        collision_dispatcher_t d;
        d.simplex = &simplex;
        compound_algorithm_t alg;
        compound_create( alg, a, false, nullptr );
        for ( int fr = 0; fr < 4; ++fr )
        {
            float t[ 16 ];
            std::memcpy( t, g, 64 );
            g += 16;
            const int kind = static_cast< int >( *g++ ), inner = static_cast< int >( *g++ ), numman = static_cast< int >( *g++ );
            const int rman_want = static_cast< int >( *g++ );
            int ids_want[ 4 ];
            std::memcpy( ids_want, g, 16 );
            g += 4;
            const int nmans = static_cast< int >( *g++ );
            const unsigned* mans = g;
            g += nmans * ( 6 + 4 * 72 );
            if ( what )
                continue;
            frame_no = fr;
            const concave_body_t cb{ t, nullptr, nullptr, &objects[ 0 ], &a, pth[ 0 ] };
            const concave_body_t ob{ tt, nullptr, nullptr, &objects[ 1 ], &b, pth[ 1 ] };
            manifold_result_t r{};
            std::memcpy( r.basis0, t, 48 );
            std::memcpy( r.origin0, t + 12, 16 );
            std::memcpy( r.basis1, tt, 48 );
            std::memcpy( r.origin1, tt + 12, 16 );
            r.object0 = &objects[ 0 ];
            r.object1 = &objects[ 1 ];
            const bool had = alg.children[ 0 ] != nullptr;
            if ( compound_process_collision( d, alg, cb, ob, r, nullptr, nullptr ) != 1 )
            {
                what = "a path not recreated was reached";
                continue;
            }
            const child_algorithm_t* c = alg.children[ 0 ].get( );
            const int my_kind = c ? c->kind : 0;
            int my_inner = -1;
            if ( c && c->kind == 3 )
                my_inner = c->compound->children[ 0 ] ? c->compound->children[ 0 ]->kind : 0;
            nested += my_kind == 3 ? 1 : 0;
            removals += had && !c ? 1 : 0;
            int rman = -1;
            for ( std::size_t m = 0; m < d.manifolds.size( ); ++m )
                if ( d.manifolds[ m ] == r.manifold )
                    rman = static_cast< int >( m );
            if ( r.manifold && rman < 0 )
                rman = -2;
            const int ids[ 4 ] = { r.part_id0, r.part_id1, r.index0, r.index1 };
            if ( my_kind != kind || my_inner != inner )
                what = "the child kinds";
            else if ( g_num_manifolds != numman )
                what = "gNumManifold";
            else if ( static_cast< int >( d.manifolds.size( ) ) != nmans )
                what = "the dispatcher's manifold count";
            else if ( rman != rman_want )
                what = "result+4";
            else if ( std::memcmp( ids, ids_want, 16 ) != 0 )
                what = "the result's ids";
            for ( int m = 0; m < nmans && !what; ++m )
            {
                const unsigned* gm = mans + m * ( 6 + 4 * 72 );
                const solver_manifold_t& ours = *d.manifolds[ m ];
                const int b0 = ours.body0 == &objects[ 0 ] ? 0 : ours.body0 == &objects[ 1 ] ? 1 : -1;
                const int b1 = ours.body1 == &objects[ 0 ] ? 0 : ours.body1 == &objects[ 1 ] ? 1 : -1;
                unsigned th[ 2 ];
                std::memcpy( th, &ours.contact_breaking_threshold, 4 );
                std::memcpy( th + 1, &ours.contact_processing_threshold, 4 );
                contacts += ours.num_contacts > 0 ? 1 : 0;
                if ( static_cast< unsigned >( ours.num_contacts ) != gm[ 0 ] )
                    what = "a manifold's count";
                else if ( b0 != static_cast< int >( gm[ 1 ] ) || b1 != static_cast< int >( gm[ 2 ] ) )
                    what = "a manifold's bodies";
                else if ( th[ 0 ] != gm[ 3 ] || th[ 1 ] != gm[ 4 ] )
                    what = "a manifold's thresholds";
                else if ( static_cast< unsigned >( ours.dispatcher_index ) != gm[ 5 ] )
                    what = "a manifold's +1196";
                for ( int k = 0; k < 4 && !what; ++k )
                {
                    unsigned pw[ 72 ];
                    std::memcpy( pw, &ours.points[ k ], 288 );
                    for ( int x = 0; x < 72 && !what; ++x )
                    {
                        const bool unknown = ( x >= 37 && x < 48 ) || ( x >= 48 && x < 72 && ( x - 48 ) % 8 != 7 );
                        const unsigned mask = x == 29 ? 0xFFu : 0xFFFFFFFFu;
                        if ( !unknown && ( pw[ x ] & mask ) != ( gm[ 6 + 72 * k + x ] & mask ) )
                            what = "a manifold point";
                    }
                }
            }
        }
        if ( what && ++failed <= 6 )
            LOG_ERROR( "compound_pair vector %u frame %d: %s", v, frame_no, what );
        for ( auto& c : alg.children )
            c.reset( );
    }
    g_num_manifolds = 0;
    if ( failed == 0 )
    {
        LOG_SUCCESS( "btCompoundCollisionAlgorithm compound vs compound (the nested SwappedCreateFunc 0x56C4F0 Compound, its "
                     "getAllContactManifolds 0x5F7A00 / removeChildAlgorithms 0x5F7BD0) matches retail over %u vectors x 4 frames on the "
                     "real boat hull: %d frames nested, %d contact manifolds, %d removals", nv, nested, contacts, removals );
        return true;
    }
    LOG_ERROR( "compound_pair FAILED %d of %u", failed, nv );
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
// verify_boat_contact: boat_contact_handler (boat events.hxx: 0x41E620 with
// 0x4AD340 / 0x534DA0 / applyImpulse) on the case's images - the body after,
// boat +0x468 / +0x389, and 0x41E2E0's arguments (read back from a driver
// whose +0xC8 / +0xCC start at -FLT_MAX, so maxss gives the argument).
inline bool verify_boat_contact( )
{
    const unsigned* g = g_golden_boat_contact_words;
    const unsigned nv = *g++;
    int failed = 0, raised = 0, impulses = 0, hits = 0;
    for ( unsigned v = 0; v < nv; ++v )
    {
        std::vector< std::uint8_t > body( 0x2D4 ), other( 0x2D4 ), boat( 0x500 ), driver( 0x100 );
        std::memcpy( body.data( ), g, 0x2D4 );
        g += 0x2D4 / 4;
        std::memcpy( other.data( ), g, 0x2D4 );
        g += 0x2D4 / 4;
        float point[ 4 ], normal[ 4 ];
        std::memcpy( point, g, 16 );
        std::memcpy( normal, g + 4, 16 );
        g += 8;
        std::memcpy( boat.data( ) + 0x134, g++, 4 );
        std::memcpy( boat.data( ) + 0x140, g++, 4 );
        const bool answer = *g++ != 0;
        boat[ 0x468 ] = static_cast< std::uint8_t >( *g++ );
        boat[ 0x389 ] = static_cast< std::uint8_t >( *g++ );
        const std::vector< std::uint8_t > before = body;
        const unsigned* after = g;
        g += 0x2D4 / 4;
        const unsigned b468 = *g++, b389 = *g++, nlog = *g++;
        std::vector< unsigned > glog( g, g + 3 * nlog );
        g += 3 * nlog;
        const std::uint32_t lowest = 0xFF7FFFFFu;
        std::memcpy( driver.data( ) + 0xC8, &lowest, 4 );
        std::memcpy( driver.data( ) + 0xCC, &lowest, 4 );
        boat_contact_handler( boat.data( ), body.data( ), other.data( ), point, normal, answer, true, driver.data( ) );
        std::vector< unsigned > log;
        unsigned c8, cc;
        std::memcpy( &c8, driver.data( ) + 0xC8, 4 );
        std::memcpy( &cc, driver.data( ) + 0xCC, 4 );
        if ( c8 != lowest || cc != lowest )
            log = { 1u, c8, cc };
        raised += nlog ? 1 : 0;
        impulses += std::memcmp( before.data( ), after, 0x2D4 ) != 0 ? 1 : 0;
        hits += b468 == 1 ? 1 : 0;
        const bool same = std::memcmp( body.data( ), after, 0x2D4 ) == 0 && boat[ 0x468 ] == b468 && boat[ 0x389 ] == b389 && log == glog;
        if ( !same && ++failed <= 6 )
            LOG_ERROR( "boat_contact %u: body %d, +0x468 %u (%u), +0x389 %u (%u), %d raise (%d)", v,
                       std::memcmp( body.data( ), after, 0x2D4 ) == 0 ? 1 : 0, boat[ 0x468 ], b468, boat[ 0x389 ], b389,
                       static_cast< int >( log.size( ) / 3 ), static_cast< int >( nlog ) );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "VuBoatEntity's contact handler 0x41E620 (0x4AD340, 0x534DA0, applyImpulse 0x4221C0, 0x41E2E0) matches retail: %u contacts, "
                     "%d rumbles, %d impulses, %d +0x468 sets", nv, raised, impulses, hits );
        return true;
    }
    LOG_ERROR( "boat_contact FAILED %d of %u contacts", failed, nv );
    return false;
}
// verify_piece_fluid: piece_fluid_step (boat damage.hxx: VuPiece's fluid step
// 0x412E30) on the case's piece, solver, water and body: the body after (its
// stale w lanes as verify_boat_records masks them), the solver's +0x50 and
// +0x54 .. +0xFF (w lanes of the vectors masked), the fake kernel's calls.
inline bool verify_piece_fluid( )
{
    int failed = 0, capped = 0, wet = 0, calls = 0;
    for ( std::size_t index = 0; index < g_golden_piece_fluid_count; ++index )
    {
        const unsigned* s = g_golden_piece_fluid_words + g_golden_piece_fluid_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        auto f = [ & ]( ) { float x; std::memcpy( &x, s++, 4 ); return x; };
        const float dt = f( );
        std::vector< std::uint8_t > piece( 0x220 ), body( 672 );
        for ( int k = 0; k < 3; ++k )
            state_put_f( piece.data( ), 0x1B0 + 4 * k, f( ) );
        std::uint8_t* sv = piece.data( ) + 0x78;
        for ( int k = 0; k < 3; ++k )
            state_put_f( sv, 4 + 4 * k, f( ) );      // +4 the offset
        for ( int k = 0; k < 6; ++k )
            state_put_f( sv, 0x14 + 4 * k, f( ) );   // +0x14 drag, buoyancy, velocity scale, the axis drags
        state_put_f( sv, 0x100, f( ) );              // the floor
        std::memset( sv + 84, 0x77, 256 - 84 );
        const bool is_wet = u( ) != 0;
        const float height = f( );
        water_t water;
        water.skip_waves = false;
        water.surface_root = is_wet ? 0 : -1;
        water.nodes.resize( u( ) );
        for ( water_node_t& node : water.nodes )
        {
            for ( float& x : node.rect )
                x = f( );
            node.first = static_cast< int >( u( ) );
            node.second = static_cast< int >( u( ) );
            node.tag = u( );
        }
        const int wave_root = static_cast< int >( u( ) );
        fluid_fake_t fake;
        water.waves.resize( u( ) );
        for ( water_wave_t& w : water.waves )
        {
            w.centre[ 0 ] = f( );
            w.centre[ 1 ] = f( );
            w.radius = f( );
            fake.k.push_back( f( ) );
        }
        fluids_mesh_t mesh;
        mesh.verts.resize( 4 * u( ) );
        for ( float& x : mesh.verts )
            x = f( );
        mesh.edges.resize( 2 * u( ) );
        for ( std::uint32_t& x : mesh.edges )
            x = u( );
        mesh.triangles.resize( 15 * u( ) );
        for ( std::uint32_t& x : mesh.triangles )
            x = u( );
        mesh.total_area = f( );
        for ( int k = 0; k < 3; ++k )
        {
            float lo = mesh.verts[ k ], hi = mesh.verts[ k ];
            for ( std::size_t v = 0; v < mesh.verts.size( ); v += 4 )
            {
                lo = mesh.verts[ v + k ] < lo ? mesh.verts[ v + k ] : lo;
                hi = mesh.verts[ v + k ] > hi ? mesh.verts[ v + k ] : hi;
            }
            mesh.aabb_min[ k ] = lo;
            mesh.aabb_max[ k ] = hi;
        }
        mesh.aabb_min[ 3 ] = mesh.aabb_max[ 3 ] = 1.0f;
        std::memcpy( body.data( ), s, 672 );
        s += 168;
        const unsigned* after = s;
        s += 168;
        const unsigned b50 = u( );
        const unsigned* sw = s;
        s += 43;
        const unsigned ncalls = u( );
        water_surface_t surface{ };
        surface.height = height;
        surface.below = surface.above = 50.0f;
        surface.inverse[ 0 ] = surface.inverse[ 5 ] = surface.inverse[ 10 ] = surface.inverse[ 15 ] = 1.0f;
        for ( int k = 0; k < 3; ++k )
            surface.inverse[ 12 + k ] = -state_f( body.data( ), 0x40 + 4 * k );
        surface.floor = -100.0f;
        surface.half_x = surface.half_y = 400.0f;
        surface.wave_root = wave_root;
        water.surfaces.push_back( surface );
        fluid_solver_t solver{ };
        solver.mesh = &mesh;
        solver.vertices.resize( mesh.verts.size( ) / 4 );
        solver.edges.resize( mesh.edges.size( ) / 2 );
        std::memset( solver.vertices.data( ), 0x77, solver.vertices.size( ) * sizeof( water_vertex_t ) );
        std::memset( solver.edges.data( ), 0x77, solver.edges.size( ) * sizeof( fluid_edge_t ) );
        std::memset( &solver.volume, 0x77, 4 );
        std::memset( &solver.area, 0x77, 4 );
        std::memset( &solver.min_depth, 0x77, 4 );
        std::memset( solver.acc, 0x77, sizeof( solver.acc ) );
        float gravity[ 3 ] = { 0.0f, 0.0f, -19.612f };
        const std::vector< std::uint8_t > before = body;
        const char* what = nullptr;
        int bad = -1;
        if ( !piece_fluid_step( solver, piece.data( ), body.data( ), water, fluid_fake_kernel, &fake, fluid_fake_gravity, gravity, dt ) )
            what = "a path not recreated was reached";
        for ( int k = 0; k < 672 && !what; k += 4 )
        {
            const bool w_lane = ( k >= 432 && k < 464 && ( k - 432 ) % 16 == 12 ) || k == 348;
            if ( !w_lane && std::memcmp( body.data( ) + k, after + k / 4, 4 ) != 0 )
                what = "a rigid body word", bad = k;
        }
        if ( !what && sv[ 0x50 ] != b50 )
            what = "solver +0x50";
        for ( int k = 0; k < 43 && !what; ++k )
        {
            const int offset = 84 + 4 * k;
            if ( !( offset >= 96 && ( offset - 96 ) % 16 == 12 ) && std::memcmp( sv + offset, sw + k, 4 ) != 0 )
                what = "a solver word", bad = offset;
        }
        if ( !what && fake.log.size( ) != 3 * ncalls )
            what = "the number of kernel calls";
        for ( unsigned k = 0; k < 3 * ncalls && !what; ++k )
            if ( s[ k ] != fake.log[ k ] )
                what = "a kernel call";
        calls += static_cast< int >( ncalls );
        wet += is_wet ? 1 : 0;
        // capped: the body's total force moved by less than the solver's force (a scale below 1 was applied)
        const float fx = state_f( body.data( ), 0x1B0 ) - state_f( before.data( ), 0x1B0 );
        capped += !what && solver.acc[ 0 ] != 0.0f && std::fabs( fx ) < std::fabs( solver.acc[ 0 ] * state_f( before.data( ), 0x170 ) ) * 0.999f ? 1 : 0;
        if ( what && ++failed <= 6 )
            LOG_ERROR( "piece_fluid vector %d: %s (+%d)", static_cast< int >( index ), what, bad );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "VuPiece's fluid step 0x412E30 (VuFluidSolver_prepare / solve, the capped force and torque) matches retail: %d steps, %d wet, "
                     "%d capped, %d kernel calls", static_cast< int >( g_golden_piece_fluid_count ), wet, capped, calls );
        return true;
    }
    LOG_ERROR( "piece_fluid FAILED %d of %d steps", failed, static_cast< int >( g_golden_piece_fluid_count ) );
    return false;
}
// verify_piece_dtor: piece_destroy (boat damage.hxx: 0x412B00) on the case's
// images - the hook log in order and the image after.
inline bool verify_piece_dtor( )
{
    const unsigned* g = g_golden_piece_dtor_words;
    const unsigned nv = *g++;
    int failed = 0, calls = 0;
    for ( unsigned v = 0; v < nv; ++v )
    {
        const unsigned self = *g++;
        std::vector< std::uint8_t > img( 0x230 );
        std::memcpy( img.data( ), g, 0x230 );
        g += 0x230 / 4;
        const unsigned* after = g;
        g += 0x230 / 4;
        const unsigned n = *g++;
        std::vector< unsigned > want( g, g + 2 * n );
        g += 2 * n;
        std::vector< unsigned > got;
        piece_destroy( img.data( ), self, []( void* u, int code, std::uint32_t word ) {
            auto& l = *static_cast< std::vector< unsigned >* >( u );
            l.push_back( static_cast< unsigned >( code ) );
            l.push_back( word );
        }, &got );
        calls += static_cast< int >( n );
        if ( ( got != want || std::memcmp( img.data( ), after, 0x230 ) != 0 ) && ++failed <= 4 )
            LOG_ERROR( "piece_dtor %u: %d calls (retail %d), image %s", v, static_cast< int >( got.size( ) / 2 ), static_cast< int >( n ),
                       std::memcmp( img.data( ), after, 0x230 ) == 0 ? "same" : "differs" );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "VuPiece's dtor body 0x412B00 (piece_destroy, its callees hooks) matches retail: %u images, %d hook calls", nv, calls );
        return true;
    }
    LOG_ERROR( "piece_dtor FAILED %d of %u", failed, nv );
    return false;
}
// verify_boat_pfx (boat batch 17): the pfx controller tick 0x419700
// (boat/effects.hxx boat_pfx_tick) on the oracle's fakes (cases/boat.py
// boat_pfx): the call log (every VuPfxSystemInstance slot with its argument
// words, and 0x41E2E0's xmm1 / xmm2) in order, the on bytes, the aabb's x y z
// lanes - and its w lanes where no record merged (a merge leaves 0x41A790's
// stale stack there: UNKNOWN).
struct boat_pfx_check_t
{
    std::vector< unsigned > log;
    std::vector< const unsigned* > aabbs;   // by system handle - 0x100
};

inline void boat_pfx_check_put( void* u, unsigned slot, std::uint32_t s, const void* words, unsigned n )
{
    auto& l = static_cast< boat_pfx_check_t* >( u )->log;
    l.push_back( slot );
    l.push_back( s - 0x100u );
    l.push_back( n );
    const unsigned* w = static_cast< const unsigned* >( words );
    if ( n )
        l.insert( l.end( ), w, w + n );
}

inline bool verify_boat_pfx( )
{
    int failed = 0, calls = 0, merges = 0, starts = 0, stops = 0, raises = 0;
    for ( std::size_t v = 0; v < g_golden_boat_pfx_count; ++v )
    {
        const unsigned* g = g_golden_boat_pfx_words + g_golden_boat_pfx_offsets[ v ];
        auto f = [ & ]( ) { float x; std::memcpy( &x, g++, 4 ); return x; };
        const float dt = f( );
        boat_pfx_boat_t b;
        b.throttle = f( );
        b.d570 = f( );
        boat_pfx_t c;
        for ( auto& x : c.on )
            x = static_cast< std::uint8_t >( *g++ );
        const bool extra = *g++ != 0;
        const unsigned* vel = g;
        g += 4;
        b.velocity = vel;
        b.b32c = static_cast< std::uint8_t >( *g++ );
        b.f400 = *g++;
        b.f404 = *g++;
        b.f2b4 = f( );
        std::uint8_t colour[ 8 ];
        std::memcpy( colour, g, 8 );
        g += 2;
        b.colour = colour;
        float off[ 3 ], world[ 16 ];
        for ( float& x : off )
            x = f( );
        for ( float& x : world )
            x = f( );
        b.offset = off;
        b.world = world;
        b.bone_count = static_cast< int >( *g++ );
        std::vector< float > bones( 16 * static_cast< std::size_t >( b.bone_count ) );
        for ( float& x : bones )
            x = f( );
        b.bones = bones.empty( ) ? nullptr : bones.data( );
        boat_pfx_check_t k;
        for ( int li = 0; li < 7; ++li )
        {
            const unsigned n = *g++;
            for ( unsigned ri = 0; ri < n; ++ri )
            {
                boat_pfx_record_t r;
                r.system = 0x100u + static_cast< unsigned >( li ) * 16u + ri;
                r.bone = static_cast< std::int32_t >( *g++ );
                for ( float& x : r.local )
                    x = f( );
                if ( k.aabbs.size( ) <= r.system - 0x100u )
                    k.aabbs.resize( r.system - 0x100u + 1, nullptr );
                k.aabbs[ r.system - 0x100u ] = g;
                g += 8;
                c.list[ li ].push_back( r );
            }
        }
        if ( extra )
            c.extra = 0x170u;
        const unsigned nlog = *g++;
        const unsigned* want = g;
        for ( unsigned e = 0; e < nlog; ++e )
        {
            const unsigned slot = g[ 0 ];
            starts += slot == 11;
            stops += slot == 12;
            raises += slot == 100;
            g += 3 + g[ 2 ];
        }
        const std::vector< unsigned > want_log( want, g );
        const unsigned* after = g;
        boat_pfx_ops_t o;
        o.user = &k;
        // the oracle logs 4 words at slot 0's pointer (the slot itself reads 3)
        o.set_position = []( void* u, std::uint32_t s, const float* p ) { boat_pfx_check_put( u, 0, s, p, 4 ); };
        o.set_matrix = []( void* u, std::uint32_t s, const float* m ) { boat_pfx_check_put( u, 2, s, m, 16 ); };
        o.set_velocity = []( void* u, std::uint32_t s, const std::uint32_t* w ) { boat_pfx_check_put( u, 3, s, w, 4 ); };
        o.set_color = []( void* u, std::uint32_t s, const float* c4 ) { boat_pfx_check_put( u, 5, s, c4, 4 ); };
        o.aabb = []( void* u, std::uint32_t s ) -> const std::uint32_t* {
            boat_pfx_check_put( u, 7, s, nullptr, 0 );
            return static_cast< boat_pfx_check_t* >( u )->aabbs[ s - 0x100u ];
        };
        o.start = []( void* u, std::uint32_t s ) { boat_pfx_check_put( u, 11, s, nullptr, 0 ); };
        o.stop = []( void* u, std::uint32_t s, bool immediate ) {
            const unsigned w = immediate ? 1u : 0u;
            boat_pfx_check_put( u, 12, s, &w, 1 );
        };
        o.tick = []( void* u, std::uint32_t s, float t ) { boat_pfx_check_put( u, 13, s, &t, 1 ); };
        o.raise = []( void* u, float a, float bb ) {
            const float w[ 2 ] = { a, bb };
            boat_pfx_check_put( u, 100, 0x100u, w, 2 );
        };
        const bool ran = boat_pfx_tick( c, b, dt, o );
        bool same = ran && k.log == want_log;
        for ( int i = 0; i < 5; ++i )
            same = same && c.on[ i ] == after[ i ];
        for ( int i = 0; i < 8; ++i )
            if ( ( i != 3 && i != 7 ) || !c.aabb_w_stale )
                same = same && c.aabb[ i ] == after[ 5 + i ];
        merges += c.aabb_w_stale ? 1 : 0;
        calls += static_cast< int >( nlog );
        if ( !same && ++failed <= 4 )
            LOG_ERROR( "boat_pfx %u: ran %d, %d log words (retail %d), on %d%d%d%d%d (retail %u%u%u%u%u), aabb x 0x%08X (retail 0x%08X)",
                       static_cast< unsigned >( v ), ran ? 1 : 0, static_cast< int >( k.log.size( ) ), static_cast< int >( want_log.size( ) ),
                       c.on[ 0 ], c.on[ 1 ], c.on[ 2 ], c.on[ 3 ], c.on[ 4 ], after[ 0 ], after[ 1 ], after[ 2 ], after[ 3 ], after[ 4 ],
                       c.aabb[ 0 ], after[ 5 ] );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the boat's pfx controller tick 0x419700 (boat_pfx_tick: 0x41ABE0 / 0x41AC50 / 0x41AAD0 / 0x41AB70 / 0x41A790) "
                     "matches retail: %d ticks, %d calls (%d starts, %d stops, %d 0x41E2E0), %d with a merged aabb",
                     static_cast< int >( g_golden_boat_pfx_count ), calls, starts, stops, raises, merges );
        return true;
    }
    LOG_ERROR( "boat_pfx FAILED %d of %d", failed, static_cast< int >( g_golden_boat_pfx_count ) );
    return false;
}
// verify_boat_debris (boat batch 17): the debris tick 0x411A50 + the parts
// 0x51F1C0 (boat/effects.hxx boat_debris_tick) against cases/boat.py
// boat_debris: every part image after, the pfx call log, the aabb's x y z
// lanes (its w lanes are a stack word retail never writes).
inline bool verify_boat_debris( )
{
    int failed = 0, calls = 0, parts = 0, pieces = 0;
    for ( std::size_t v = 0; v < g_golden_boat_debris_count; ++v )
    {
        const unsigned* g = g_golden_boat_debris_words + g_golden_boat_debris_offsets[ v ];
        auto f = [ & ]( ) { float x; std::memcpy( &x, g++, 4 ); return x; };
        const float dt = f( );
        boat_debris_t d;
        d.gravity = f( );
        d.timer = f( );
        d.out = static_cast< std::uint8_t >( *g++ );
        const unsigned np = *g++;
        std::vector< std::array< float, 8 > > boxes( np + 8 );
        for ( unsigned k = 0; k < np; ++k )
        {
            boat_debris_part_t p;
            std::memcpy( p.image, g, 0xD8 );
            g += 0xD8 / 4;
            std::memcpy( boxes[ k ].data( ), g, 32 );
            g += 8;
            d.parts.push_back( p );
        }
        for ( unsigned k = 0; k < np; ++k )
            d.parts[ k ].model_aabb = boxes[ k ].data( );
        const unsigned nq = *g++;
        std::vector< std::array< float, 16 > > mats( nq );
        std::vector< std::array< float, 8 > > qbox( nq ), sbox( nq );
        std::vector< std::array< float, 3 > > offs( nq );
        for ( unsigned k = 0; k < nq; ++k )
        {
            boat_debris_piece_t q;
            std::memcpy( mats[ k ].data( ), g, 64 );
            g += 16;
            q.matrix = mats[ k ].data( );
            if ( *g++ )
            {
                std::memcpy( qbox[ k ].data( ), g, 32 );
                g += 8;
                q.model_aabb = qbox[ k ].data( );
            }
            std::memcpy( offs[ k ].data( ), g, 12 );
            g += 3;
            q.offset = offs[ k ].data( );
            if ( *g++ )
            {
                std::memcpy( sbox[ k ].data( ), g, 32 );
                g += 8;
                q.pfx = 0x100u + k;
            }
            d.pieces.push_back( q );
        }
        const unsigned nlog = *g++;
        const unsigned* want = g;
        for ( unsigned e = 0; e < nlog; ++e )
            g += 3 + g[ 2 ];
        const std::vector< unsigned > want_log( want, g );
        struct ctx_t
        {
            boat_pfx_check_t k;
            std::vector< std::array< float, 8 > >* boxes;
        } c{ { }, &sbox };
        boat_pfx_ops_t o;
        o.user = &c;
        o.set_matrix = []( void* u, std::uint32_t s, const float* m ) { boat_pfx_check_put( &static_cast< ctx_t* >( u )->k, 2, s, m, 16 ); };
        o.tick = []( void* u, std::uint32_t s, float t ) { boat_pfx_check_put( &static_cast< ctx_t* >( u )->k, 13, s, &t, 1 ); };
        o.aabb = []( void* u, std::uint32_t s ) -> const std::uint32_t* {
            ctx_t& x = *static_cast< ctx_t* >( u );
            boat_pfx_check_put( &x.k, 7, s, nullptr, 0 );
            return reinterpret_cast< const std::uint32_t* >( ( *x.boxes )[ s - 0x100u ].data( ) );
        };
        boat_debris_tick( d, dt, o );
        bool same = c.k.log == want_log;
        for ( unsigned k = 0; k < np; ++k )
        {
            same = same && std::memcmp( d.parts[ k ].image, g, 0xD8 ) == 0;
            g += 0xD8 / 4;
        }
        for ( int i = 0; i < 8; ++i )
            if ( i != 3 && i != 7 )
                same = same && std::memcmp( &d.aabb[ i ], g + i, 4 ) == 0;
        calls += static_cast< int >( nlog );
        parts += d.timer > 0.0f ? static_cast< int >( np ) : 0;
        pieces += d.out ? static_cast< int >( nq ) : 0;
        if ( !same && ++failed <= 4 )
        {
            unsigned ax;
            std::memcpy( &ax, &d.aabb[ 0 ], 4 );
            LOG_ERROR( "boat_debris %u: log %s, aabb x 0x%08X (retail 0x%08X)", static_cast< unsigned >( v ), c.k.log == want_log ? "same" : "differs",
                       ax, g[ 0 ] );
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the boat's debris tick 0x411A50 + parts 0x51F1C0 (boat_debris_tick / boat_debris_parts_tick) match retail: %d ticks, "
                     "%d parts stepped, %d pieces placed, %d pfx calls", static_cast< int >( g_golden_boat_debris_count ), parts, pieces, calls );
        return true;
    }
    LOG_ERROR( "boat_debris FAILED %d of %d", failed, static_cast< int >( g_golden_boat_debris_count ) );
    return false;
}
// verify_piece_remove (boat batch 17): the debris reset 0x411950 - boat/state.hxx
// boat_debris_reset with its piece_remove hook = boat/effects.hxx
// boat_piece_remove - against cases/boat.py piece_remove: the calls in order,
// +0x94 / +0xC8, each piece's +0x20C and +0x228.
inline std::uint32_t piece_check_word( const std::uint8_t* image, int at )
{
    std::uint32_t w;
    std::memcpy( &w, image + at, 4 );
    return w;
}

inline bool verify_piece_remove( )
{
    int failed = 0, calls = 0, removed = 0;
    for ( std::size_t v = 0; v < g_golden_piece_remove_count; ++v )
    {
        const unsigned* g = g_golden_piece_remove_words + g_golden_piece_remove_offsets[ v ];
        std::uint8_t debris[ 0xD0 ]{};
        debris[ 0x94 ] = static_cast< std::uint8_t >( *g++ );
        image_put32( debris, 0xC8, *g++ );
        const unsigned n = *g++;
        image_put32( debris, 0xA0, n );
        std::vector< std::array< std::uint8_t, 0x230 > > ps( n );
        for ( unsigned k = 0; k < n; ++k )
        {
            ps[ k ].fill( 0 );
            ps[ k ][ 0x20C ] = static_cast< std::uint8_t >( *g++ );
            image_put32( ps[ k ].data( ), 0x208, *g++ );
            image_put32( ps[ k ].data( ), 0x228, *g++ );
        }
        const unsigned nlog = *g++;
        const std::vector< unsigned > want( g, g + 2 * nlog );
        g += 2 * nlog;
        struct ctx_t
        {
            std::vector< std::array< std::uint8_t, 0x230 > >* ps;
            std::vector< unsigned > log;
            boat_piece_hooks_t h;
        } c{ &ps, { }, { } };
        c.h.user = &c;
        c.h.remove_body = []( void* u, std::uint32_t b ) { static_cast< ctx_t* >( u )->log.insert( static_cast< ctx_t* >( u )->log.end( ), { 1u, b } ); };
        c.h.remove_step = []( void* u, std::uint8_t* s ) {
            ctx_t& x = *static_cast< ctx_t* >( u );
            unsigned k = 0xEEEEu;
            for ( std::size_t i = 0; i < x.ps->size( ); ++i )
                if ( s == ( *x.ps )[ i ].data( ) + 4 )
                    k = static_cast< unsigned >( i );
            x.log.insert( x.log.end( ), { 2u, k } );
        };
        c.h.pfx_release = []( void* u, std::uint32_t s ) { static_cast< ctx_t* >( u )->log.insert( static_cast< ctx_t* >( u )->log.end( ), { 3u, s } ); };
        boat_decision_hooks_t hooks;
        hooks.user = &c;
        hooks.piece_remove = []( void* u, int k ) {
            ctx_t& x = *static_cast< ctx_t* >( u );
            boat_piece_remove( ( *x.ps )[ static_cast< std::size_t >( k ) ].data( ), x.h );
        };
        boat_debris_reset( debris, hooks );
        bool same = c.log == want && debris[ 0x94 ] == g[ 0 ] && piece_check_word( debris, 0xC8 ) == g[ 1 ];
        g += 2;
        for ( unsigned k = 0; k < n; ++k )
        {
            same = same && ps[ k ][ 0x20C ] == g[ 0 ] && piece_check_word( ps[ k ].data( ), 0x228 ) == g[ 1 ];
            g += 2;
        }
        calls += static_cast< int >( nlog );
        removed += ( n > 0 && piece_check_word( debris, 0xA0 ) == n && c.log.size( ) > 0 ) ? 1 : 0;
        if ( !same && ++failed <= 4 )
            LOG_ERROR( "piece_remove %u: %d log words (retail %d)", static_cast< unsigned >( v ), static_cast< int >( c.log.size( ) ),
                       static_cast< int >( want.size( ) ) );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the debris reset 0x411950 (boat_debris_reset + boat_piece_remove) matches retail: %d resets, %d hook calls",
                     static_cast< int >( g_golden_piece_remove_count ), calls );
        return true;
    }
    LOG_ERROR( "piece_remove FAILED %d of %d", failed, static_cast< int >( g_golden_piece_remove_count ) );
    return false;
}
// verify_hull_splash (boat batch 17): the hull tick's splash path (boat/effects.hxx
// boat_hull_splash) against cases/boat.py hull_splash (0x416630 run whole, the
// create planned): the calls in order (create, setMatrix 16 words, slot 3,
// start) and every entry's serial after.
inline bool verify_hull_splash( )
{
    int failed = 0, started = 0, created = 0;
    for ( std::size_t v = 0; v < g_golden_hull_splash_count; ++v )
    {
        const unsigned* g = g_golden_hull_splash_words + g_golden_hull_splash_offsets[ v ];
        float s178;
        std::memcpy( &s178, g++, 4 );
        const unsigned* vel = g;
        g += 4;
        float pos[ 4 ];
        std::memcpy( pos, g, 16 );
        g += 4;
        const unsigned namelen = *g++;
        pfx_manager_t m;
        m.capacity = *g++;
        m.mask = *g++;
        m.bits = *g++;
        m.serial_cap = *g++;
        const unsigned ne = *g++;
        m.entries.resize( ne );
        for ( unsigned k = 0; k < ne; ++k )
        {
            m.entries[ k ].flags = *g++;
            m.entries[ k ].index = *g++;
            m.entries[ k ].serial = *g++;
            m.entries[ k ].instance = 0x100u + k;
        }
        m.table.resize( m.capacity );
        for ( unsigned k = 0; k < m.capacity; ++k )
            m.table[ k ] = static_cast< int >( *g++ );
        const unsigned pick = *g++;
        const unsigned nlog = *g++;
        const unsigned* want = g;
        for ( unsigned e = 0; e < nlog; ++e )
            g += 1 + g[ 0 ];
        const std::vector< unsigned > want_log( want, g );
        struct ctx_t
        {
            std::vector< unsigned > log;
            unsigned namelen, pick;
        } c{ { }, namelen, pick };
        auto put = []( void* u, std::initializer_list< unsigned > head, const unsigned* w, unsigned n ) {
            auto& l = static_cast< ctx_t* >( u )->log;
            l.push_back( static_cast< unsigned >( head.size( ) ) + n );
            l.insert( l.end( ), head );
            if ( n )
                l.insert( l.end( ), w, w + n );
        };
        static void ( *s_put )( void*, std::initializer_list< unsigned >, const unsigned*, unsigned );
        s_put = put;
        boat_pfx_ops_t o;
        o.user = &c;
        o.set_matrix = []( void* u, std::uint32_t s, const float* mm ) { s_put( u, { 2u, s - 0x100u }, reinterpret_cast< const unsigned* >( mm ), 16 ); };
        o.set_velocity = []( void* u, std::uint32_t s, const std::uint32_t* w ) { s_put( u, { 3u, s - 0x100u }, w, 4 ); };
        o.start = []( void* u, std::uint32_t s ) { s_put( u, { 11u, s - 0x100u }, nullptr, 0 ); };
        // boat_hull_tick's splash branch: hull +0x178 not ordered-equal to 0.
        if ( s178 != 0.0f || s178 != s178 )
        {
            ++created;
            const std::string name( namelen, 'n' );
            boat_hull_splash( m, name.c_str( ), []( void* u, const char* nm ) {
                ctx_t& x = *static_cast< ctx_t* >( u );
                x.log.insert( x.log.end( ), { 2u, 1u, static_cast< unsigned >( std::strlen( nm ) ) } );
                return x.pick == 0xFFFFFFFFu ? -1 : static_cast< int >( x.pick );
            }, &c, vel, pos, o );
        }
        bool same = c.log == want_log;
        for ( unsigned k = 0; k < ne; ++k )
            same = same && m.entries[ k ].serial == g[ k ];
        for ( std::size_t i = 0; i + 1 < c.log.size( ); i += 1 + c.log[ i ] )
            started += c.log[ i + 1 ] == 11u;
        if ( !same && ++failed <= 4 )
            LOG_ERROR( "hull_splash %u: %d log words (retail %d)", static_cast< unsigned >( v ), static_cast< int >( c.log.size( ) ),
                       static_cast< int >( want_log.size( ) ) );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the hull tick's splash 0x4167B8 .. 0x416921 (boat_hull_splash: createEffect, handle, lookup, lookAtBasis, slots 2 / 3 / 11) "
                     "matches retail: %d ticks, %d splashes, %d effects started", static_cast< int >( g_golden_hull_splash_count ), created, started );
        return true;
    }
    LOG_ERROR( "hull_splash FAILED %d of %d", failed, static_cast< int >( g_golden_hull_splash_count ) );
    return false;
}
// verify_debris_launch (boat batch 17): the launch 0x410FF0 + the parts 0x51E790
// (boat/effects.hxx boat_debris_launch) against cases/boat.py debris_launch:
// the hook calls in order, +0x94 / +0x98 / +0xC8, every part image (but the
// four stack words +0x60 / +0x70 / +0x80 / +0x90), every piece image and body,
// the VuRandom state after.
inline bool verify_debris_launch( )
{
    constexpr int kBody = 0x2D4;
    const std::size_t body_bytes = sizeof( solver_rigid_t ) < kBody ? sizeof( solver_rigid_t ) : kBody;
    int failed = 0, calls = 0, parts_n = 0, pieces_n = 0;
    for ( std::size_t v = 0; v < g_golden_debris_launch_count; ++v )
    {
        const unsigned* g = g_golden_debris_launch_words + g_golden_debris_launch_offsets[ v ];
        fleet_random_t rng;
        rng.seed = static_cast< std::int32_t >( *g++ );
        rng.last = static_cast< std::int32_t >( *g++ );
        for ( auto& x : rng.table )
            x = static_cast< std::int32_t >( *g++ );
        boat_debris_sub_t sub;
        std::memcpy( sub.range, g, 72 );
        g += 18;
        const unsigned np = *g++;
        std::vector< boat_debris_part_t > parts( np );
        std::vector< std::array< float, 16 > > mm( np );
        for ( unsigned k = 0; k < np; ++k )
        {
            std::memcpy( parts[ k ].image, g, 0xD8 );
            g += 0xD8 / 4;
            std::memcpy( mm[ k ].data( ), g, 64 );
            g += 16;
            parts[ k ].model_matrix = mm[ k ].data( );
        }
        float world[ 16 ];
        std::memcpy( world, g, 64 );
        g += 16;
        solver_rigid_t body{ };
        std::memcpy( &body, g, body_bytes );
        g += kBody / 4;
        std::uint8_t debris[ 0xD0 ]{};
        debris[ 0x94 ] = static_cast< std::uint8_t >( *g++ );
        image_put32( debris, 0xC8, *g++ );
        const unsigned nq = *g++;
        image_put32( debris, 0xA0, nq );
        std::vector< std::array< std::uint8_t, 0x230 > > imgs( nq );
        std::vector< solver_rigid_t > bodies( nq );
        std::vector< boat_launch_piece_t > pieces( nq );
        std::vector< unsigned > plan( nq );
        for ( unsigned k = 0; k < nq; ++k )
        {
            std::memcpy( imgs[ k ].data( ), g, 0x230 );
            g += 0x230 / 4;
            bodies[ k ] = solver_rigid_t{ };
            std::memcpy( &bodies[ k ], g, body_bytes );
            g += kBody / 4;
            const unsigned len = *g++;
            char name[ 33 ]{};
            std::memcpy( name, g, 32 );
            g += 8;
            pieces[ k ].image = imgs[ k ].data( );
            pieces[ k ].body = &bodies[ k ];
            pieces[ k ].pfx_name.assign( name, len );
            plan[ k ] = *g++;
        }
        const unsigned nlog = *g++;
        const std::vector< unsigned > want( g, g + 2 * nlog );
        g += 2 * nlog;
        struct ctx_t
        {
            std::vector< std::array< std::uint8_t, 0x230 > >* imgs;
            std::vector< unsigned >* plan;
            std::size_t next = 0;
            std::vector< unsigned > log;
        } c{ &imgs, &plan };
        auto index_of = []( ctx_t& x, const std::uint8_t* s ) {
            for ( std::size_t i = 0; i < x.imgs->size( ); ++i )
                if ( s == ( *x.imgs )[ i ].data( ) + 4 )
                    return static_cast< unsigned >( i );
            return 0xEEEEu;
        };
        static unsigned ( *s_index )( ctx_t&, const std::uint8_t* );
        s_index = index_of;
        boat_launch_hooks_t h;
        h.remove.user = &c;
        auto add = []( void* u, unsigned a, unsigned b ) { static_cast< ctx_t* >( u )->log.insert( static_cast< ctx_t* >( u )->log.end( ), { a, b } ); };
        static void ( *s_add )( void*, unsigned, unsigned );
        s_add = add;
        h.remove.remove_body = []( void* u, std::uint32_t b ) { s_add( u, 1, b ); };
        h.remove.remove_step = []( void* u, std::uint8_t* s ) { s_add( u, 2, s_index( *static_cast< ctx_t* >( u ), s ) ); };
        h.remove.pfx_release = []( void* u, std::uint32_t s ) { s_add( u, 3, s ); };
        h.add_body = []( void* u, std::uint32_t b ) { s_add( u, 4, b ); };
        h.add_step = []( void* u, std::uint8_t* s ) { s_add( u, 5, s_index( *static_cast< ctx_t* >( u ), s ) ); };
        h.pfx_create = []( void* u, const std::string& n ) -> std::uint32_t {
            ctx_t& x = *static_cast< ctx_t* >( u );
            s_add( u, 6, static_cast< unsigned >( n.size( ) ) );
            return ( *x.plan )[ x.next++ ];
        };
        h.pfx_start = []( void* u, std::uint32_t s ) { s_add( u, 7, s ); };
        boat_debris_launch( debris, sub, parts, pieces, world, body, rng, h );
        bool same = c.log == want && debris[ 0x94 ] == g[ 0 ] && piece_check_word( debris, 0x98 ) == g[ 1 ] && piece_check_word( debris, 0xC8 ) == g[ 2 ];
        g += 3;
        for ( unsigned k = 0; k < np; ++k )
        {
            for ( int w = 0; w < 0xD8 / 4; ++w )
                if ( w != 0x60 / 4 && w != 0x70 / 4 && w != 0x80 / 4 && w != 0x90 / 4 )
                    same = same && piece_check_word( parts[ k ].image, 4 * w ) == g[ w ];
            g += 0xD8 / 4;
        }
        for ( unsigned k = 0; k < nq; ++k )
        {
            same = same && std::memcmp( imgs[ k ].data( ), g, 0x230 ) == 0;
            g += 0x230 / 4;
            same = same && std::memcmp( &bodies[ k ], g, body_bytes ) == 0;
            g += kBody / 4;
        }
        same = same && static_cast< unsigned >( rng.seed ) == g[ 0 ] && static_cast< unsigned >( rng.last ) == g[ 1 ] &&
               std::memcmp( rng.table, g + 2, 128 ) == 0;
        calls += static_cast< int >( nlog );
        parts_n += static_cast< int >( np );
        pieces_n += static_cast< int >( nq );
        if ( !same && ++failed <= 4 )
            LOG_ERROR( "debris_launch %u: log %s, rng %s", static_cast< unsigned >( v ), c.log == want ? "same" : "differs",
                       static_cast< unsigned >( rng.seed ) == g[ 0 ] ? "same" : "differs" );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the debris launch 0x410FF0 + parts 0x51E790 (boat_debris_launch / boat_debris_parts_launch) match retail: %d launches, "
                     "%d parts, %d pieces, %d hook calls", static_cast< int >( g_golden_debris_launch_count ), parts_n, pieces_n, calls );
        return true;
    }
    LOG_ERROR( "debris_launch FAILED %d of %d", failed, static_cast< int >( g_golden_debris_launch_count ) );
    return false;
}
// verify_bt_dtors (boat batch 17): engine collision/leaves.hxx k_bt_scalar_dtors
// against cases/boat.py bt_dtors - every row with flags 0 / 1 / 2 / 3 / ~1: eax,
// the object's words, the operator delete calls.
inline bool verify_bt_dtors( )
{
    const unsigned* g = g_golden_bt_dtors_words;
    const unsigned n = *g++;
    int failed = 0, deletes = 0;
    for ( unsigned v = 0; v < n; ++v )
    {
        const unsigned address = *g++, flags = *g++, self = *g++;
        std::uint8_t object[ 64 ];
        std::memcpy( object, g, 64 );
        g += 16;
        const unsigned eax = *g++;
        const unsigned* after = g;
        g += 16;
        const unsigned nd = *g++;
        const std::vector< unsigned > want( g, g + nd );
        g += nd;
        const bt_scalar_dtor_t* d = nullptr;
        for ( const bt_scalar_dtor_t& x : k_bt_scalar_dtors )
            if ( x.address == address )
                d = &x;
        std::vector< unsigned > got;
        const unsigned r = d ? bt_scalar_dtor_run( *d, object, self, flags, []( void* u, std::uint32_t o ) {
            static_cast< std::vector< unsigned >* >( u )->push_back( o );
        }, &got ) : 0xEEEEEEEEu;
        deletes += static_cast< int >( nd );
        if ( ( !d || r != eax || got != want || std::memcmp( object, after, 64 ) != 0 ) && ++failed <= 4 )
            LOG_ERROR( "bt_dtors 0x%06X flags 0x%X: eax 0x%08X (retail 0x%08X), %d deletes (retail %d)", address, flags, r, eax,
                       static_cast< int >( got.size( ) ), static_cast< int >( nd ) );
    }
    if ( failed == 0 && n == 5 * ( sizeof( k_bt_scalar_dtors ) / sizeof( k_bt_scalar_dtors[ 0 ] ) ) )
    {
        LOG_SUCCESS( "the %d empty-dtor scalar deleting destructors (k_bt_scalar_dtors) match retail: %u calls, %d deletes",
                     static_cast< int >( sizeof( k_bt_scalar_dtors ) / sizeof( k_bt_scalar_dtors[ 0 ] ) ), n, deletes );
        return true;
    }
    LOG_ERROR( "bt_dtors FAILED %d of %u", failed, n );
    return false;
}
// verify_needs_collision (boat batch 18): engine collision/dispatch.hxx
// dispatcher_needs_collision (now calling dynamics/wrapper.hxx
// rigid_check_collide_with for the +0x108 override) against cases/boat.py
// needs_collision - 0x56D710 run whole on the real VuRigidBody vtable.
inline bool verify_needs_collision( )
{
    const unsigned* g = g_golden_needs_collision_words;
    const unsigned n = *g++;
    int failed = 0, overrides = 0, refused = 0;
    for ( unsigned v = 0; v < n; ++v )
    {
        collision_object_t o[ 3 ];
        o[ 0 ].activation_state = static_cast< int >( *g++ );
        o[ 1 ].activation_state = static_cast< int >( *g++ );
        o[ 0 ].check_collide = *g++ != 0;
        o[ 1 ].internal_type = static_cast< int >( *g++ );
        o[ 0 ].constraint_ref_count = static_cast< std::int32_t >( *g++ );
        solver_rigid_t bodies[ 3 ];
        for ( int k = 0; k < 3; ++k )
            o[ k ].body.object = &bodies[ k ];
        std::vector< rigid_constraint_ref_t > refs( *g++ );
        for ( rigid_constraint_ref_t& r : refs )
        {
            r.a = &bodies[ *g++ ];
            r.b = &bodies[ *g++ ];
        }
        o[ 0 ].constraint_refs = refs.data( );
        const int want = static_cast< int >( *g++ );
        const int got = dispatcher_needs_collision( o[ 0 ], o[ 1 ] );
        overrides += o[ 0 ].check_collide ? 1 : 0;
        refused += want == 0 ? 1 : 0;
        if ( got != want && ++failed <= 4 )
            LOG_ERROR( "needs_collision %u: %d (retail %d)", v, got, want );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "needsCollision 0x56D710 + VuRigidBody slot 0 0x50E7B0 (dispatcher_needs_collision) match retail: %u vectors, %d through "
                     "the override, %d refused", n, overrides, refused );
        return true;
    }
    LOG_ERROR( "needs_collision FAILED %d of %u", failed, n );
    return false;
}
// verify_debris_follow (boat batch 18): boat/effects.hxx boat_debris_follow
// behind state.hxx boat_decision_tick's gate (+0x46B, then a signed piece
// count > 0 - reproduced here, not called) against cases/boat.py
// debris_follow: retail's segment 0x41F103 .. 0x41F1A3 run on its own.
inline bool verify_debris_follow( )
{
    const unsigned* g = g_golden_debris_follow_words;
    const unsigned n = *g++;
    int failed = 0, follows = 0;
    struct log_t { std::vector< unsigned > words; };
    for ( unsigned v = 0; v < n; ++v )
    {
        const bool destroyed = *g++ != 0;
        const std::int32_t count = static_cast< std::int32_t >( *g++ );
        float matrix[ 16 ];
        std::memcpy( matrix, g, 64 );
        g += 16;
        vu_rigid_body_t body, piece;
        std::memcpy( &body, g, 0x2D4 );
        g += 0x2D4 / 4;
        std::memcpy( &piece, g, 0x2D4 );
        g += 0x2D4 / 4;
        const unsigned exit = *g++, nlog = *g++;
        const std::vector< unsigned > want( g, g + 18 * nlog );
        g += 18 * nlog;
        log_t log;
        unsigned got_exit = 2;
        if ( destroyed )
        {
            got_exit = 1;
            if ( count > 0 )
            {
                ++follows;
                boat_debris_follow( body.body, piece.body, matrix, []( void* u, const float* m ) {
                    std::vector< unsigned >& w = static_cast< log_t* >( u )->words;
                    w.push_back( 0 );
                    w.push_back( 1 );
                    const unsigned* x = reinterpret_cast< const unsigned* >( m );
                    w.insert( w.end( ), x, x + 16 );
                }, &log );
            }
        }
        const bool same = got_exit == exit && log.words == want && std::memcmp( &body, g, 0x2D4 ) == 0 &&
                          std::memcmp( &piece, g + 0x2D4 / 4, 0x2D4 ) == 0;
        g += 2 * ( 0x2D4 / 4 );
        if ( !same && ++failed <= 4 )
            LOG_ERROR( "debris_follow %u: exit %u (retail %u), %u calls (retail %u)", v, got_exit, exit,
                       static_cast< unsigned >( log.words.size( ) / 18 ), nlog );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the destroyed branch 0x41F103 .. 0x41F1A3 (boat_debris_follow behind boat_decision_tick's gate) matches retail: %u vectors, "
                     "%d follows (setWorldTransform's matrix, both bodies)", n, follows );
        return true;
    }
    LOG_ERROR( "debris_follow FAILED %d of %u", failed, n );
    return false;
}
// verify_destroyed_event (boat batch 18): boat/effects.hxx boat_destroyed_event
// against cases/boat.py destroyed_event - 0x41E240 run whole, FMOD logged. The
// hooks stand for the platform's FMOD; what retail hands them beyond the name
// (g_vuaudio +8 the system, mode 1, null velocity / orientation) is checked
// as constants of the golden calls. The get hook answers as the case did
// (false for a name holding "bad", the handle the case wrote either way).
inline bool verify_destroyed_event( )
{
    const unsigned* g = g_golden_destroyed_event_words;
    const unsigned n = *g++;
    int failed = 0, started = 0;
    for ( unsigned v = 0; v < n; ++v )
    {
        const unsigned length = *g++;
        const std::string name( reinterpret_cast< const char* >( g ), length );
        g += ( length + 3 ) / 4;
        float position[ 3 ];
        std::memcpy( position, g, 12 );
        g += 3;
        const unsigned ncalls = *g++;
        std::vector< unsigned > want;
        bool constants = true;
        for ( unsigned c = 0; c < ncalls; ++c )
        {
            const unsigned kind = *g++;
            want.push_back( kind );
            if ( kind == 1 )
            {
                constants = constants && g[ 0 ] == 0x5150u && g[ 1 ] == 1u;
                want.push_back( g[ 2 ] );
                g += 3;
            }
            else if ( kind == 2 )
            {
                want.insert( want.end( ), g, g + 4 );
                constants = constants && g[ 4 ] == 0u && g[ 5 ] == 0u;
                g += 6;
            }
            else
                want.push_back( *g++ );
        }
        struct ctx_t
        {
            std::vector< unsigned > got;
            unsigned handle;
        } c{ { }, 0xE001u + v };
        boat_event_hooks_t h;
        h.user = &c;
        h.get = []( void* u, const std::string& s, std::uint32_t& ev ) {
            ctx_t& x = *static_cast< ctx_t* >( u );
            x.got.push_back( 1 );
            x.got.push_back( x.handle );
            ev = x.handle;
            return s.find( "bad" ) == std::string::npos;
        };
        h.attributes = []( void* u, std::uint32_t ev, const float* p ) {
            ctx_t& x = *static_cast< ctx_t* >( u );
            x.got.push_back( 2 );
            x.got.push_back( ev );
            const unsigned* w = reinterpret_cast< const unsigned* >( p );
            x.got.insert( x.got.end( ), w, w + 3 );
        };
        h.start = []( void* u, std::uint32_t ev ) {
            ctx_t& x = *static_cast< ctx_t* >( u );
            x.got.push_back( 3 );
            x.got.push_back( ev );
        };
        boat_destroyed_event( name, position, h );
        started += ncalls == 3 ? 1 : 0;
        if ( ( c.got != want || !constants ) && ++failed <= 4 )
            LOG_ERROR( "destroyed_event %u (%s): %u words (retail %u)%s", v, name.c_str( ), static_cast< unsigned >( c.got.size( ) ),
                       static_cast< unsigned >( want.size( ) ), constants ? "" : ", a constant differs" );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "0x41E240 (boat_destroyed_event) matches retail: %u events, %d started", n, started );
        return true;
    }
    LOG_ERROR( "destroyed_event FAILED %d of %u", failed, n );
    return false;
}
// verify_bt_teardown (boat batch 18): engine collision/teardown.hxx against
// cases/boat.py bt_teardown - the twelve scalar deleting dtors 0x56D290 /
// 0x5B53E0 / 0x54C0C0 / 0x56DD30 / 0x56FB80 / 0x624580 / 0x6000F0 / 0x56CCD0 /
// 0x586320 / 0x43D5E0 / 0x5471A0 / 0x6060A0 run whole on fake objects in one arena: the
// arena after, gNumAlignedFree, the frees, the deletes, the virtual calls.
inline bool verify_bt_teardown( )
{
    struct arena_t
    {
        std::uint32_t base = 0;
        std::vector< std::uint8_t > bytes;
        bool outside = false;
        std::vector< unsigned > frees, deletes;
        const unsigned* vcalls = nullptr;
        unsigned nv = 0, at = 0;
        bool vbad = false;
        std::uint8_t* p( std::uint32_t a, unsigned n )
        {
            if ( a < base || a + n > base + bytes.size( ) )
            {
                outside = true;
                return nullptr;
            }
            return bytes.data( ) + ( a - base );
        }
    };
    const unsigned* g = g_golden_bt_teardown_words;
    const unsigned n = *g++;
    int failed = 0, frees = 0, calls = 0;
    for ( unsigned v = 0; v < n; ++v )
    {
        const unsigned kind = *g++, address = *g++, flags = *g++, self = *g++, base = *g++, eax = *g++, c0 = *g++, c1 = *g++;
        arena_t x;
        x.base = base;
        const unsigned nf = *g++;
        const std::vector< unsigned > want_frees( g, g + nf );
        g += nf;
        const unsigned nd = *g++;
        const std::vector< unsigned > want_deletes( g, g + nd );
        g += nd;
        x.nv = *g++;
        x.vcalls = g;
        for ( unsigned k = 0; k < x.nv; ++k )
            g += 4 + g[ 2 ];
        const unsigned nch = *g++;
        const unsigned* changed = g;
        g += 2 * nch;
        const unsigned nw = *g++;
        x.bytes.assign( reinterpret_cast< const std::uint8_t* >( g ), reinterpret_cast< const std::uint8_t* >( g + nw ) );
        g += nw;
        std::vector< std::uint8_t > want( x.bytes );
        for ( unsigned k = 0; k < nch; ++k )
            std::memcpy( want.data( ) + 4 * changed[ 2 * k ], &changed[ 2 * k + 1 ], 4 );
        std::uint32_t counter = c0;
        bt_mem_t m;
        m.user = &x;
        m.num_aligned_free = &counter;
        m.rd = []( void* u, std::uint32_t a ) -> std::uint32_t {
            std::uint32_t w = 0;
            if ( std::uint8_t* q = static_cast< arena_t* >( u )->p( a, 4 ) )
                std::memcpy( &w, q, 4 );
            return w;
        };
        m.rd8 = []( void* u, std::uint32_t a ) -> std::uint8_t {
            std::uint8_t* q = static_cast< arena_t* >( u )->p( a, 1 );
            return q ? *q : 0;
        };
        m.wr = []( void* u, std::uint32_t a, std::uint32_t w ) {
            if ( std::uint8_t* q = static_cast< arena_t* >( u )->p( a, 4 ) )
                std::memcpy( q, &w, 4 );
        };
        m.wr8 = []( void* u, std::uint32_t a, std::uint8_t b ) {
            if ( std::uint8_t* q = static_cast< arena_t* >( u )->p( a, 1 ) )
                *q = b;
        };
        m.aligned_free = []( void* u, std::uint32_t p ) { static_cast< arena_t* >( u )->frees.push_back( p ); };
        m.release = []( void* u, std::uint32_t p ) { static_cast< arena_t* >( u )->deletes.push_back( p ); };
        m.vcall = []( void* u, std::uint32_t s, std::uint32_t off, const std::uint32_t* args, int count ) -> std::uint32_t {
            arena_t& a = *static_cast< arena_t* >( u );
            if ( a.at >= a.nv )
            {
                a.vbad = true;
                return 0;
            }
            const unsigned* c = a.vcalls;
            for ( unsigned k = 0; k < a.at; ++k )
                c += 4 + c[ 2 ];
            ++a.at;
            if ( c[ 0 ] != s || c[ 1 ] != off || c[ 2 ] != static_cast< unsigned >( count ) )
                a.vbad = true;
            for ( int k = 0; k < count && !a.vbad; ++k )
                a.vbad = c[ 3 + k ] != args[ k ];
            return c[ 3 + c[ 2 ] ];
        };
        void ( *const inners[ 8 ] )( const bt_mem_t&, std::uint32_t ) = { bt_dispatcher_destroy, bt_convex_polyhedron_destroy, bt_collision_world_destroy,
            bt_dbvt_broadphase_destroy, bt_sequential_solver_destroy, bt_island_manager_destroy, bt_dynamics_world_destroy,
            bt_collision_configuration_destroy };
        std::uint32_t r;
        if ( kind == 9 )
            r = bt_quantized_bvh_delete( m, self, flags );
        else if ( kind == 10 || kind == 11 )
            r = bt_aligned_base_delete( m, self, flags, kind == 10 ? 0x74CF98u : 0x74DEBCu );
        else if ( kind == 12 )
            r = bt_dynamics_world_base_delete( m, self, flags );
        else
            r = bt_deleting( m, self, flags, inners[ ( kind - 1 ) & 7 ] );
        frees += static_cast< int >( nf );
        calls += static_cast< int >( x.nv );
        const bool same = !x.outside && !x.vbad && x.at == x.nv && r == eax && counter == c1 && x.frees == want_frees &&
                          x.deletes == want_deletes && x.bytes == want;
        if ( !same && ++failed <= 4 )
            LOG_ERROR( "bt_teardown %u (0x%06X flags %u): %d frees (retail %u), arena %s, calls %s", v, address, flags,
                       static_cast< int >( x.frees.size( ) ), nf, x.bytes == want ? "same" : "differs", x.vbad || x.at != x.nv ? "differ" : "same" );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the twelve Bullet teardown dtors (0x56D290 .. 0x6060A0, collision/teardown.hxx) match retail: %u calls, "
                     "%d frees, %d virtual calls", n, frees, calls );
        return true;
    }
    LOG_ERROR( "bt_teardown FAILED %d of %u", failed, n );
    return false;
}
// verify_bt_misc (boat batch 18): the small rows of engine
// collision/teardown.hxx (0x56C270, 0x552540, 0x5B80E0, 0x5AF690, 0x43D5B0,
// 0x54FE50) against cases/boat.py bt_misc: eax, the arena after, the virtual
// calls in order (each answered with retail's logged eax).
inline bool verify_bt_misc( )
{
    struct arena_t
    {
        std::uint32_t base = 0;
        std::vector< std::uint8_t > bytes;
        bool bad = false;
        const unsigned* vcalls = nullptr;
        unsigned nv = 0, at = 0;
        std::uint8_t* p( std::uint32_t a, unsigned n )
        {
            if ( a < base || a + n > base + bytes.size( ) )
            {
                bad = true;
                return nullptr;
            }
            return bytes.data( ) + ( a - base );
        }
    };
    const unsigned* g = g_golden_bt_misc_words;
    const unsigned n = *g++;
    int failed = 0, calls = 0;
    for ( unsigned v = 0; v < n; ++v )
    {
        const unsigned kind = *g++, address = *g++, self = *g++, base = *g++, eax = *g++, na = *g++;
        const unsigned* args = g;
        g += na;
        arena_t x;
        x.base = base;
        x.nv = *g++;
        x.vcalls = g;
        for ( unsigned k = 0; k < x.nv; ++k )
            g += 4 + g[ 2 ];
        const unsigned nch = *g++;
        const unsigned* changed = g;
        g += 2 * nch;
        const unsigned nw = *g++;
        x.bytes.assign( reinterpret_cast< const std::uint8_t* >( g ), reinterpret_cast< const std::uint8_t* >( g + nw ) );
        g += nw;
        std::vector< std::uint8_t > want( x.bytes );
        for ( unsigned k = 0; k < nch; ++k )
            std::memcpy( want.data( ) + 4 * changed[ 2 * k ], &changed[ 2 * k + 1 ], 4 );
        bt_mem_t m;
        m.user = &x;
        m.rd = []( void* u, std::uint32_t a ) -> std::uint32_t {
            std::uint32_t w = 0;
            if ( std::uint8_t* q = static_cast< arena_t* >( u )->p( a, 4 ) )
                std::memcpy( &w, q, 4 );
            return w;
        };
        m.wr = []( void* u, std::uint32_t a, std::uint32_t w ) {
            if ( std::uint8_t* q = static_cast< arena_t* >( u )->p( a, 4 ) )
                std::memcpy( q, &w, 4 );
        };
        m.vcall = []( void* u, std::uint32_t s, std::uint32_t off, const std::uint32_t* a, int count ) -> std::uint32_t {
            arena_t& y = *static_cast< arena_t* >( u );
            if ( y.at >= y.nv )
            {
                y.bad = true;
                return 0;
            }
            const unsigned* c = y.vcalls;
            for ( unsigned k = 0; k < y.at; ++k )
                c += 4 + c[ 2 ];
            ++y.at;
            y.bad = y.bad || c[ 0 ] != s || c[ 1 ] != off || c[ 2 ] != static_cast< unsigned >( count );
            for ( int k = 0; k < count && !y.bad; ++k )
                y.bad = c[ 3 + k ] != a[ k ];
            return c[ 3 + c[ 2 ] ];
        };
        std::uint32_t r = 0;
        switch ( kind )
        {
            case 1: r = bt_empty_algorithm_create( m, args[ 0 ] ); break;
            case 2: r = bt_convex_result_needs_collision( m, self, args[ 0 ] ) ? 1u : 0u; break;
            case 3: bt_zero_inertia( m, args[ 1 ] ); r = eax; break;   // void: eax is not an answer
            case 4: r = bt_support_forward( m, self, args[ 0 ], args[ 1 ] ); break;
            case 5: r = bt_forward_43D5B0( m, self, args, static_cast< int >( na ) ); break;
            default: r = bt_forward_54FE50( m, self, args, static_cast< int >( na ) ); break;
        }
        calls += static_cast< int >( x.nv );
        if ( ( x.bad || x.at != x.nv || r != eax || x.bytes != want ) && ++failed <= 4 )
            LOG_ERROR( "bt_misc %u (0x%06X): eax 0x%08X (retail 0x%08X), arena %s", v, address, r, eax, x.bytes == want ? "same" : "differs" );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the small Bullet rows 0x56C270 / 0x552540 / 0x5B80E0 / 0x5AF690 / 0x43D5B0 / 0x54FE50 (collision/teardown.hxx) match "
                     "retail: %u calls, %d virtual calls", n, calls );
        return true;
    }
    LOG_ERROR( "bt_misc FAILED %d of %u", failed, n );
    return false;
}
// verify_bt_rows (boat batch 19): engine collision/arena.hxx against
// cases/boat.py bt_rows - thirteen Bullet rows (0x552F30, 0x5B8E10, 0x5F6700,
// 0x526B10 + 0x526B40, 0x5AEC10, 0x54C940, 0x56E000, 0x56EA70, 0x56EB20,
// 0x605FA0, 0x6027A0, 0x602830, 0x527160) run whole on fake objects in one
// arena: eax (ST0 bits / al), the arena after, gNumAlignedFree, the frees,
// the deletes, the virtual calls in order (arguments compared, retail's eax
// and out words answered). bt_rows_verify runs one case's words through
// `call( kind, m, io, self, args )` (it answers the row's eax).
template < typename F > inline bool bt_rows_verify( const unsigned* g, const char* tag, const char* what, F&& call )
{
    struct arena_t
    {
        std::uint32_t base = 0;
        std::vector< std::uint8_t > bytes;
        bool bad = false;
        std::vector< unsigned > frees, deletes;
        const unsigned* vcalls = nullptr;
        unsigned nv = 0, at = 0;
        // (boat batch 21) a perturbed model can chase a chain through garbage forever (a hash chain whose reads
        // answer 0): past 50M accesses the vector is abandoned (thrown, caught around the row: a failure).
        unsigned long long ops = 0;
        std::uint8_t* p( std::uint32_t a, unsigned n )
        {
            if ( ++ops > 50000000ull )
                throw ops;
            if ( a < base || a + n > base + bytes.size( ) )
            {
                bad = true;
                return nullptr;
            }
            return bytes.data( ) + ( a - base );
        }
        // The next logged call: self, offset, count, args, eax, nout, outs.
        const unsigned* next( std::uint32_t s, std::uint32_t off, const std::uint32_t* args, int count )
        {
            if ( at >= nv )
            {
                bad = true;
                return nullptr;
            }
            const unsigned* c = vcalls;
            for ( unsigned k = 0; k < at; ++k )
                c += 5 + c[ 2 ] + c[ 4 + c[ 2 ] ];
            ++at;
            bad = bad || c[ 0 ] != s || c[ 1 ] != off || c[ 2 ] != static_cast< unsigned >( count );
            for ( int k = 0; k < count && !bad; ++k )
                bad = c[ 3 + k ] != args[ k ];
            return bad ? nullptr : c;
        }
    };
    const unsigned n = *g++;
    int failed = 0, calls = 0, frees = 0;
    for ( unsigned v = 0; v < n; ++v )
    {
        const unsigned kind = *g++, address = *g++, self = *g++, base = *g++, eax = *g++, c0 = *g++, c1 = *g++, na = *g++;
        const unsigned* args = g;
        g += na;
        arena_t x;
        x.base = base;
        const unsigned nf = *g++;
        const std::vector< unsigned > want_frees( g, g + nf );
        g += nf;
        const unsigned nd = *g++;
        const std::vector< unsigned > want_deletes( g, g + nd );
        g += nd;
        x.nv = *g++;
        x.vcalls = g;
        for ( unsigned k = 0; k < x.nv; ++k )
            g += 5 + g[ 2 ] + g[ 4 + g[ 2 ] ];
        const unsigned nch = *g++;
        const unsigned* changed = g;
        g += 2 * nch;
        const unsigned nw = *g++;
        x.bytes.assign( reinterpret_cast< const std::uint8_t* >( g ), reinterpret_cast< const std::uint8_t* >( g + nw ) );
        g += nw;
        std::vector< std::uint8_t > want( x.bytes );
        for ( unsigned k = 0; k < nch; ++k )
            std::memcpy( want.data( ) + 4 * changed[ 2 * k ], &changed[ 2 * k + 1 ], 4 );
        std::uint32_t counter = c0, allocs = 0;
        bt_mem_t m;
        m.user = &x;
        m.num_aligned_free = &counter;
        // (boat batch 21) btAlignedAlloc's function [0x7358A4]: a logged call on self 0x7358A4, offset 0 (size,
        // alignment); gNumAlignedAlloc counts from 0 (a row's check answers its count as eax).
        m.num_aligned_alloc = &allocs;
        m.aligned_alloc = []( void* u, std::uint32_t size, std::uint32_t alignment ) -> std::uint32_t {
            arena_t& y = *static_cast< arena_t* >( u );
            const std::uint32_t a[ 2 ] = { size, alignment };
            const unsigned* c = y.next( 0x7358A4u, 0, a, 2 );
            return c ? c[ 3 + c[ 2 ] ] : 0u;
        };
        m.rd = []( void* u, std::uint32_t a ) -> std::uint32_t {
            std::uint32_t w = 0;
            if ( std::uint8_t* q = static_cast< arena_t* >( u )->p( a, 4 ) )
                std::memcpy( &w, q, 4 );
            return w;
        };
        m.rd8 = []( void* u, std::uint32_t a ) -> std::uint8_t {
            std::uint8_t* q = static_cast< arena_t* >( u )->p( a, 1 );
            return q ? *q : 0;
        };
        m.wr = []( void* u, std::uint32_t a, std::uint32_t w ) {
            if ( std::uint8_t* q = static_cast< arena_t* >( u )->p( a, 4 ) )
                std::memcpy( q, &w, 4 );
        };
        m.wr8 = []( void* u, std::uint32_t a, std::uint8_t b ) {
            if ( std::uint8_t* q = static_cast< arena_t* >( u )->p( a, 1 ) )
                *q = b;
        };
        m.aligned_free = []( void* u, std::uint32_t q ) { static_cast< arena_t* >( u )->frees.push_back( q ); };
        m.release = []( void* u, std::uint32_t q ) { static_cast< arena_t* >( u )->deletes.push_back( q ); };
        m.vcall = []( void* u, std::uint32_t s, std::uint32_t off, const std::uint32_t* a, int count ) -> std::uint32_t {
            arena_t& y = *static_cast< arena_t* >( u );
            const unsigned* c = y.next( s, off, a, count );
            if ( !c )
                return 0;
            y.bad = y.bad || c[ 4 + c[ 2 ] ] != 0;
            return c[ 3 + c[ 2 ] ];
        };
        bt_io_t io;
        io.m = &m;
        io.vcall_out = []( void* u, std::uint32_t s, std::uint32_t off, const std::uint32_t* a, int count, std::uint32_t* out, int words ) -> std::uint32_t {
            arena_t& y = *static_cast< arena_t* >( u );
            const unsigned* c = y.next( s, off, a, count );
            if ( !c )
                return 0;
            if ( c[ 4 + c[ 2 ] ] != static_cast< unsigned >( words ) )
            {
                y.bad = true;
                return 0;
            }
            std::memcpy( out, c + 5 + c[ 2 ], 4u * static_cast< unsigned >( words ) );
            return c[ 3 + c[ 2 ] ];
        };
        std::uint32_t r = 0;
        try
        {
            r = call( kind, m, io, self, args );
        }
        catch ( unsigned long long )
        {
            x.bad = true;
        }
        calls += static_cast< int >( x.nv );
        frees += static_cast< int >( nf );
        const bool same = !x.bad && x.at == x.nv && r == eax && counter == c1 && x.frees == want_frees && x.deletes == want_deletes && x.bytes == want;
        if ( !same && ++failed <= 4 )
        {
            std::size_t first = 0;
            while ( first < want.size( ) && x.bytes[ first ] == want[ first ] )
                ++first;
            LOG_ERROR( "%s %u (0x%06X): eax 0x%08X (retail 0x%08X), arena %s (first at +0x%X), calls %s, frees %d (retail %u)", tag, v, address, r,
                       eax, x.bytes == want ? "same" : "differs", static_cast< unsigned >( first ), x.bad || x.at != x.nv ? "differ" : "same",
                       static_cast< int >( x.frees.size( ) ), nf );
            if ( first + 16 <= want.size( ) )
            {
                std::uint32_t h[ 4 ], w[ 4 ];
                std::memcpy( h, x.bytes.data( ) + ( first & ~3u ), 16 );
                std::memcpy( w, want.data( ) + ( first & ~3u ), 16 );
                LOG_ERROR( "   ours %08X %08X %08X %08X retail %08X %08X %08X %08X", h[ 0 ], h[ 1 ], h[ 2 ], h[ 3 ], w[ 0 ], w[ 1 ], w[ 2 ], w[ 3 ] );
            }
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "%s match retail: %u calls, %d virtual calls, %d frees", what, n, calls, frees );
        return true;
    }
    LOG_ERROR( "%s FAILED %d of %u", tag, failed, n );
    return false;
}

inline bool verify_bt_rows( )
{
    return bt_rows_verify( g_golden_bt_rows_words, "bt_rows",
        "thirteen Bullet rows 0x552F30 .. 0x527160 (collision/arena.hxx: scaling, point collector, three dtors, removeCollisionObject, the "
        "broadphase's getAabb / getBroadphaseAabb / resetPool, the convex result callbacks, recalculateLocalAabb)",
        []( unsigned kind, const bt_mem_t& m, const bt_io_t& io, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            switch ( kind )
            {
                case 1: bt_convex_internal_set_scaling( m, self, args[ 0 ] ); return 0;
                case 2: bt_point_collector_add( m, self, args[ 0 ], args[ 1 ], args[ 2 ] ); return 0;
                case 3: return bt_convex_triangle_callback_delete( m, self, args[ 0 ] );
                case 4: return bt_compound_shape_delete( m, self, args[ 0 ] );
                case 5: return bt_bvh_mesh_shape_delete( m, self, args[ 0 ] );
                case 6: bt_collision_world_remove_object( m, self, args[ 0 ] ); return 0;
                case 7: bt_dbvt_broadphase_get_aabb( m, args[ 0 ], args[ 1 ], args[ 2 ] ); return 0;
                case 8: bt_dbvt_broadphase_world_aabb( m, self, args[ 0 ], args[ 1 ] ); return 0;
                case 9: bt_dbvt_broadphase_reset_pool( m, self ); return 0;
                case 10: return bt_closest_convex_add_single_result( m, self, args[ 0 ], args[ 1 ] );
                case 11: return bt_closest_not_me_add_single_result( m, self, args[ 0 ], args[ 1 ] );
                case 12: return bt_closest_not_me_needs_collision( m, self, args[ 0 ] ) ? 1u : 0u;
                default: bt_compound_recalculate_local_aabb( io, self ); return 0;
            }
        } );
}

// verify_bt_rows2 (boat batch 19): cases/boat.py bt_rows2 - the polyhedral
// calculateLocalInertia 0x54B4C0 (vt+0x28 is retail's getMargin leaf 0x43D5C0
// in the case: fld [this + 0x30] - answered here from the arena) and the
// btTriangleShape rows 0x5B7D60 / 0x5B7E20 / 0x5B7FA0 / 0x5B7FC0 / 0x5B83D0.
inline bool verify_bt_rows2( )
{
    return bt_rows_verify( g_golden_bt_rows2_words, "bt_rows2",
        "six Bullet rows 0x54B4C0 / 0x5B7D60 / 0x5B7E20 / 0x5B7FA0 / 0x5B7FC0 / 0x5B83D0 (collision/arena.hxx: the polyhedral "
        "calculateLocalInertia, btTriangleShape's support / batched support / getPlane / getPlaneEquation / penetration direction)",
        []( unsigned kind, const bt_mem_t& m, const bt_io_t& io, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            switch ( kind )
            {
                case 1:
                    bt_polyhedral_local_inertia( io, self, args[ 0 ], args[ 1 ],
                        []( const bt_mem_t& q, std::uint32_t s ) -> std::uint32_t { return q.rd( q.user, s + 0x30 ); } );   // 0x43D5C0
                    return 0;
                case 2: bt_triangle_support( m, self, args[ 0 ], args[ 1 ] ); return 0;
                case 3: bt_triangle_support_batch( m, self, args[ 0 ], args[ 1 ], args[ 2 ] ); return 0;
                case 4: return bt_triangle_get_plane( m, self, args );
                case 5: bt_triangle_plane_equation( m, self, args[ 1 ], args[ 2 ] ); return 0;
                default: bt_triangle_penetration_direction( m, self, args[ 0 ], args[ 1 ] ); return 0;
            }
        } );
}

// verify_bt_rows3 (boat batch 19): cases/boat.py bt_rows3 - 0x5263F0,
// 0x57B840, 0x552570, 0x624520, 0x56FAD0, 0x413340, 0x551E90.
inline bool verify_bt_rows3( )
{
    return bt_rows_verify( g_golden_bt_rows3_words, "bt_rows3",
        "seven Bullet rows 0x5263F0 / 0x57B840 / 0x552570 / 0x624520 / 0x56FAD0 / 0x413340 / 0x551E90 (collision/arena.hxx: the polyhedral "
        "dtor, CProfileNode::Reset, LocalConvexResult, the island manager / sequential solver ctors, btMatrix3x3, btHashMap::findIndex)",
        []( unsigned kind, const bt_mem_t& m, const bt_io_t&, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            switch ( kind )
            {
                case 1: return bt_polyhedral_shape_delete( m, self, args[ 0 ] );
                case 2: bt_profile_node_reset( m, self ); return 0;
                case 3: return bt_local_convex_result_construct( m, self, args );
                case 4: return bt_island_manager_construct( m, self );
                case 5: return bt_sequential_solver_construct( m, self );
                case 6: return bt_matrix3x3_construct( m, self, args );
                default: return bt_hash_map_find_index( m, self, args[ 0 ] );
            }
        } );
}

// verify_bt_rows4 (boat batch 19): cases/boat.py bt_rows4 - btBoxShape::
// getHalfExtentsWithMargin 0x548B20 (vt+0x28 = retail's leaf 0x43D5C0 in the
// case: fld [this + 0x30]) and btCollisionDispatcher's ctor 0x56D1B0.
inline bool verify_bt_rows4( )
{
    return bt_rows_verify( g_golden_bt_rows4_words, "bt_rows4",
        "two Bullet rows 0x548B20 / 0x56D1B0 (collision/arena.hxx: getHalfExtentsWithMargin, the collision dispatcher's ctor with its 36 x 36 "
        "double-dispatch table)",
        []( unsigned kind, const bt_mem_t& m, const bt_io_t&, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            if ( kind == 1 )
                return bt_box_half_extents_with_margin( m, self, args[ 0 ],
                    []( const bt_mem_t& q, std::uint32_t s ) -> std::uint32_t { return q.rd( q.user, s + 0x30 ); } );   // 0x43D5C0
            return bt_collision_dispatcher_construct( m, self, args[ 0 ] );
        } );
}

// verify_bt_rows5 (boat batch 19): cases/boat.py bt_rows5 - the
// serialization rows 0x554620 / 0x554670 / 0x43D680 / 0x5AEAC0 / 0x5AF5F0 /
// 0x5AF640 / 0x605C90.
inline bool verify_bt_rows5( )
{
    return bt_rows_verify( g_golden_bt_rows5_words, "bt_rows5",
        "seven Bullet serialization rows 0x554620 / 0x554670 / 0x43D680 / 0x5AEAC0 / 0x5AF5F0 / 0x5AF640 / 0x605C90 (collision/arena.hxx)",
        []( unsigned kind, const bt_mem_t& m, const bt_io_t&, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            switch ( kind )
            {
                case 1: return bt_shape_serialize( m, self, args[ 0 ], args[ 1 ] );
                case 2: bt_shape_serialize_single( m, self, args[ 0 ] ); return 0;
                case 3: return bt_convex_internal_serialize( m, self, args[ 0 ], args[ 1 ], false );
                case 4: return bt_convex_internal_serialize( m, self, args[ 0 ], args[ 1 ], true );
                case 5: bt_bvh_serialize_single_bvh( m, self, args[ 0 ] ); return 0;
                case 6: bt_bvh_serialize_single_info_map( m, self, args[ 0 ] ); return 0;
                default: bt_world_serialize_rigid_bodies( m, self, args[ 0 ] ); return 0;
            }
        } );
}

// A bt_mem_t over the arena with a stack frame in front of it: addresses in
// [0x0F000000, +0x5C) are a local buffer (a retail stack frame the model
// keeps in memory), the rest - and every hook - go to the inner one.
struct bt_framed_t
{
    const bt_mem_t* inner = nullptr;
    std::uint8_t bytes[ 0x5C ] = { };
    std::uint8_t* p( std::uint32_t a, unsigned n )
    {
        return a >= 0x0F000000u && a + n <= 0x0F000000u + sizeof( bytes ) ? bytes + ( a - 0x0F000000u ) : nullptr;
    }
};

inline bt_mem_t bt_framed_mem( bt_framed_t& fr, const bt_mem_t& m )
{
    fr.inner = &m;
    bt_mem_t f = m;
    f.user = &fr;
    f.rd = []( void* u, std::uint32_t a ) -> std::uint32_t {
        bt_framed_t& y = *static_cast< bt_framed_t* >( u );
        if ( std::uint8_t* q = y.p( a, 4 ) )
        {
            std::uint32_t w;
            std::memcpy( &w, q, 4 );
            return w;
        }
        return y.inner->rd( y.inner->user, a );
    };
    f.rd8 = []( void* u, std::uint32_t a ) -> std::uint8_t {
        bt_framed_t& y = *static_cast< bt_framed_t* >( u );
        if ( std::uint8_t* q = y.p( a, 1 ) )
            return *q;
        return y.inner->rd8( y.inner->user, a );
    };
    f.wr = []( void* u, std::uint32_t a, std::uint32_t w ) {
        bt_framed_t& y = *static_cast< bt_framed_t* >( u );
        if ( std::uint8_t* q = y.p( a, 4 ) )
            std::memcpy( q, &w, 4 );
        else
            y.inner->wr( y.inner->user, a, w );
    };
    f.wr8 = []( void* u, std::uint32_t a, std::uint8_t b ) {
        bt_framed_t& y = *static_cast< bt_framed_t* >( u );
        if ( std::uint8_t* q = y.p( a, 1 ) )
            *q = b;
        else
            y.inner->wr8( y.inner->user, a, b );
    };
    f.aligned_free = []( void* u, std::uint32_t q ) {
        bt_framed_t& y = *static_cast< bt_framed_t* >( u );
        y.inner->aligned_free( y.inner->user, q );
    };
    f.aligned_alloc = []( void* u, std::uint32_t size, std::uint32_t alignment ) -> std::uint32_t {
        bt_framed_t& y = *static_cast< bt_framed_t* >( u );
        return y.inner->aligned_alloc( y.inner->user, size, alignment );
    };
    f.vcall = []( void* u, std::uint32_t s, std::uint32_t off, const std::uint32_t* a, int count ) -> std::uint32_t {
        bt_framed_t& y = *static_cast< bt_framed_t* >( u );
        return y.inner->vcall( y.inner->user, s, off, a, count );
    };
    return f;
}
// verify_bt_rows6 (boat batch 21): cases/boat.py bt_rows6 - btCollisionWorld::
// serialize 0x551A00, btDiscreteDynamicsWorld::serialize 0x605D60,
// serializeCollisionObjects 0x5516E0 and the btHashMap rows insert 0x551A30 /
// growTables 0x551C70 / dtor 0x5518B0. eax = the row's gNumAlignedAlloc count.
// 0x5516E0's stack frame (the local map, key, value) is a 0x5C-byte buffer at
// 0x0F000000 filled with the case's stack fill (args[ 1 ]): the key's second
// word is read from it, as retail reads its stack.
inline bool verify_bt_rows6( )
{
    return bt_rows_verify( g_golden_bt_rows6_words, "bt_rows6",
        "six Bullet rows 0x551A00 / 0x605D60 / 0x5516E0 / 0x551A30 / 0x551C70 / 0x5518B0 (collision/arena.hxx: the world serialize, "
        "serializeCollisionObjects, btHashMap insert / growTables / dtor)",
        []( unsigned kind, const bt_mem_t& m, const bt_io_t&, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            bt_framed_t fr;
            const std::uint32_t before = *m.num_aligned_alloc;
            if ( kind <= 3 )
                for ( unsigned k = 0; k < sizeof( fr.bytes ); k += 4 )
                    std::memcpy( fr.bytes + k, &args[ 1 ], 4 );
            const bt_mem_t f = bt_framed_mem( fr, m );
            switch ( kind )
            {
                case 1: bt_collision_world_serialize( f, self, args[ 0 ], 0x0F000000u, false ); break;
                case 2: bt_collision_world_serialize( f, self, args[ 0 ], 0x0F000000u, true ); break;
                case 3: bt_collision_world_serialize_objects( f, self, args[ 0 ], 0x0F000000u ); break;
                case 4: bt_hash_map_insert( f, self, args[ 0 ], args[ 1 ] ); break;
                case 5: bt_hash_map_grow_tables( f, self ); break;
                default: bt_hash_map_destroy( f, self ); break;
            }
            return *m.num_aligned_alloc - before;
        } );
}
// verify_bt_rows7 (boat batch 21): cases/boat.py bt_rows7 - btDbvtBroadphase::
// aabbTest 0x56E0D0 (its stack policy + volume in the 0x0F000000 frame) and
// btDbvt::collideTV 0x56F220 (this = the root, args the volume and the
// policy); eax = the row's gNumAlignedAlloc count.
inline bool verify_bt_rows7( )
{
    return bt_rows_verify( g_golden_bt_rows7_words, "bt_rows7",
        "two Bullet rows 0x56E0D0 / 0x56F220 (collision/arena.hxx: btDbvtBroadphase::aabbTest, btDbvt::collideTV with its growing stack)",
        []( unsigned kind, const bt_mem_t& m, const bt_io_t&, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            bt_framed_t fr;
            const std::uint32_t before = *m.num_aligned_alloc;
            const bt_mem_t f = bt_framed_mem( fr, m );
            if ( kind == 1 )
                bt_dbvt_broadphase_aabb_test( f, self, args[ 0 ], args[ 1 ], args[ 2 ], 0x0F000000u );
            else
                bt_dbvt_collide_tv( f, self, args[ 0 ], args[ 1 ] );
            return *m.num_aligned_alloc - before;
        } );
}// verify_bt_rows8 (boat batch 21): cases/boat.py bt_rows8 - the polyhedral
// localGetSupportingVertexWithoutMargin 0x54B230 / batchedUnitVector... 0x54B3C0
// (getVertex through io), btTriangleShape::isInside 0x5B8110 (getEdge through
// io) and btConvexPlaneCollisionAlgorithm::CreateFunc 0x56C5B0.
inline bool verify_bt_rows8( )
{
    return bt_rows_verify( g_golden_bt_rows8_words, "bt_rows8",
        "four Bullet rows 0x54B230 / 0x54B3C0 / 0x5B8110 / 0x56C5B0 (collision/arena.hxx: the polyhedral supporting vertex and its batched "
        "form, btTriangleShape::isInside, the convex-plane CreateFunc)",
        []( unsigned kind, const bt_mem_t& m, const bt_io_t& io, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            switch ( kind )
            {
                case 1: return bt_polyhedral_local_support( io, self, args[ 0 ], args[ 1 ] );
                case 2: bt_polyhedral_batched_support( io, self, args[ 0 ], args[ 1 ], static_cast< std::int32_t >( args[ 2 ] ) ); return 0;
                case 3: return bt_triangle_is_inside( io, self, args[ 0 ], args[ 1 ] ) ? 1u : 0u;
                default: return bt_convex_plane_create( m, self, args[ 0 ], args[ 1 ], args[ 2 ] );
            }
        } );
}// verify_bt_rows9 (boat batch 21): cases/boat.py bt_rows9 - btConvexShape::
// getAabbSlow 0x552F90 (vt+0x28 = retail's leaf 0x43D5C0 in the case: fld
// [this + 0x30]) and btConvexShape::project 0x553610; the support calls'
// directions compared as their arguments.
inline bool verify_bt_rows9( )
{
    return bt_rows_verify( g_golden_bt_rows9_words, "bt_rows9",
        "two Bullet rows 0x552F90 / 0x553610 (collision/arena.hxx: btConvexShape::getAabbSlow and project)",
        []( unsigned kind, const bt_mem_t&, const bt_io_t& io, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            if ( kind == 1 )
                bt_convex_aabb_slow( io, self, args[ 0 ], args[ 1 ], args[ 2 ],
                    []( const bt_mem_t& q, std::uint32_t s ) -> std::uint32_t { return q.rd( q.user, s + 0x30 ); } );   // 0x43D5C0
            else
                bt_convex_project( io, self, args[ 0 ], args[ 1 ], args[ 2 ], args[ 3 ] );
            return 0;
        } );
}

// verify_bt_rows10 (boat batch 22): cases/boat.py bt_rows10 - btCompoundShape::
// setLocalScaling 0x5276D0 (updateChildTransform 0x526F40 and btDbvt::update
// 0x594960 inside), updateChildTransform 0x526F40 (its transform's words read
// from the arena), btDbvt::update 0x594960 (this = the tree, the volume's words
// from the arena) and btCompoundShape::serialize 0x5278B0. eax = the row's
// gNumAlignedAlloc count (0x5278B0: its answer).
inline bool verify_bt_rows10( )
{
    return bt_rows_verify( g_golden_bt_rows10_words, "bt_rows10",
        "four Bullet rows 0x5276D0 / 0x526F40 / 0x594960 / 0x5278B0 (collision/arena.hxx: the compound's setLocalScaling, "
        "updateChildTransform, btDbvt::update with removeleaf / insertleaf, the compound's serialize)",
        []( unsigned kind, const bt_mem_t& m, const bt_io_t& io, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            const std::uint32_t before = *m.num_aligned_alloc;
            std::uint32_t w[ 16 ];
            switch ( kind )
            {
                case 1: bt_compound_set_local_scaling( io, self, args[ 0 ] ); break;
                case 2:
                    for ( std::uint32_t k = 0; k < 16; ++k )
                        w[ k ] = m.rd( m.user, args[ 1 ] + 4 * k );
                    bt_compound_update_child_transform( io, self, args[ 0 ], w );
                    break;
                case 3:
                    for ( std::uint32_t k = 0; k < 8; ++k )
                        w[ k ] = m.rd( m.user, args[ 1 ] + 4 * k );
                    bt_dbvt_update( m, self, args[ 0 ], w );
                    break;
                default: return bt_compound_serialize( m, self, args[ 0 ], args[ 1 ] );
            }
            return *m.num_aligned_alloc - before;
        } );
}

// verify_bt_rows11 (boat batch 22): cases/boat.py bt_rows11 - btQuantizedBvh's
// setQuantizationValues 0x586350, assignInternalNodeFromLeafNode 0x588870,
// setInternalNodeAabbMin 0x58A570 / Max 0x58A4B0, mergeInternalNodeAabb
// 0x58A290 (quantized and plain).
inline bool verify_bt_rows11( )
{
    return bt_rows_verify( g_golden_bt_rows11_words, "bt_rows11",
        "five Bullet rows 0x586350 / 0x588870 / 0x58A570 / 0x58A4B0 / 0x58A290 (collision/arena.hxx: btQuantizedBvh's quantization "
        "values and internal-node helpers)",
        []( unsigned kind, const bt_mem_t& m, const bt_io_t&, std::uint32_t self, const unsigned* args ) -> std::uint32_t {
            switch ( kind )
            {
                case 1: bt_qbvh_set_quantization( m, self, args[ 0 ], args[ 1 ] ); break;
                case 2: bt_qbvh_assign_from_leaf( m, self, args[ 0 ], args[ 1 ] ); break;
                case 3: bt_qbvh_set_node_bound( m, self, args[ 0 ], args[ 1 ], false ); break;
                case 4: bt_qbvh_set_node_bound( m, self, args[ 0 ], args[ 1 ], true ); break;
                default: bt_qbvh_merge_node( m, self, args[ 0 ], args[ 1 ], args[ 2 ] ); break;
            }
            return 0;
        } );
}// verify_mesh_scaling (boat batch 20): cases/boat.py mesh_scaling -
// btTriangleMeshShape::setLocalScaling 0x60C1C0 (concave.hxx mesh_shape_set
// _local_scaling: under the BVH vtable the BVH-walk recalc, under the plain
// vtable the all-triangles walk) and btBvhTriangleMeshShape::setLocalScaling
// 0x5AF300 (bvh_mesh_set_local_scaling, the no-rebuild vectors): the shape
// built as the case builds it (ctor, setOptimizedBvh on the level), the
// cached aabb before, then after the row; the margin and the mesh scaling.
inline bool verify_mesh_scaling( )
{
    const golden_bvh_asset_t& la = g_golden_bvh_assets[ 1 ];
    const golden_bvh_asset_t& ha = g_golden_bvh_assets[ 0 ];
    quantized_bvh_t bvh{};
    std::memcpy( bvh.aabb_min, la.m_header + 0, 16 );
    std::memcpy( bvh.aabb_max, la.m_header + 4, 16 );
    std::memcpy( bvh.quantization, la.m_header + 8, 16 );
    bvh.cur_node_index = static_cast< int >( la.m_header[ 13 ] );
    bvh.use_quantization = static_cast< uint8_t >( la.m_quantized );
    bvh.nodes = reinterpret_cast< const bvh_node_t* >( la.m_nodes );
    bvh.traversal_mode = la.m_traversal;
    int failed = 0, changed = 0, nan_lanes = 0;
    for ( std::size_t index = 0; index < g_golden_mesh_scaling_count; ++index )
    {
        const golden_mesh_scaling_t& g = g_golden_mesh_scaling[ index ];
        const golden_bvh_asset_t& a = g.m_level ? la : ha;
        striding_mesh_t mesh{};
        std::memcpy( mesh.scaling, g.m_s0, 16 );
        mesh.vertices = reinterpret_cast< const float( * )[ 4 ] >( a.m_verts );
        mesh.num_vertices = a.m_nverts;
        mesh.indices = a.m_indices;
        mesh.index_count = a.m_nindices;
        bvh_mesh_shape_t s{ 21, 0.0f, nullptr, nullptr };
        const char* what = nullptr;
        int bad_word = -1;
        unsigned w[ 8 ] = {}, r[ 8 ] = {};
        float s0[ 4 ], s1[ 4 ];
        std::memcpy( s0, g.m_s0, 16 );
        std::memcpy( s1, g.m_s1, 16 );
        for ( int k = 0; k < 3; ++k )
            nan_lanes += std::isnan( s1[ k ] ) ? 1 : 0;
        if ( bvh_mesh_shape_init( s, &mesh ) != 1 || ( g.m_level && bvh_mesh_set_optimized_bvh( s, mesh, &bvh, s0 ) != 1 ) )
            what = "the setup reached unrecreated code";
        std::memcpy( w, s.local_aabb_min, 16 );
        std::memcpy( w + 4, s.local_aabb_max, 16 );
        for ( int k = 0; k < 8 && !what; ++k )
            if ( w[ k ] != g.m_before[ k ] )
            {
                what = "the aabb before the row";
                bad_word = k;
                std::memcpy( r, g.m_before, 32 );
            }
        if ( !what )
        {
            std::memcpy( &s.margin, &g.m_margin, 4 );
            const int rc = g.m_fn == 0x5AF300u ? bvh_mesh_set_local_scaling( s, mesh, s1 ) : mesh_shape_set_local_scaling( s, mesh, s1, g.m_plain == 0 );
            if ( rc != 1 )
                what = "the row reached unrecreated code";
            std::memcpy( w, s.local_aabb_min, 16 );
            std::memcpy( w + 4, s.local_aabb_max, 16 );
            changed += std::memcmp( g.m_aabb, g.m_before, 32 ) != 0 ? 1 : 0;
            for ( int k = 0; k < 8 && !what; ++k )
                if ( w[ k ] != g.m_aabb[ k ] )
                {
                    what = "the aabb after the row";
                    bad_word = k;
                    std::memcpy( r, g.m_aabb, 32 );
                }
            unsigned m;
            std::memcpy( &m, &s.margin, 4 );
            if ( !what && m != g.m_margin_after )
                what = "the margin";
            if ( !what && std::memcmp( mesh.scaling, g.m_mesh_scaling, 16 ) != 0 )
                what = "the mesh scaling";
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "mesh scaling vector %d (0x%X): %s (word %d: ours 0x%08X, retail 0x%08X)", static_cast< int >( index ), g.m_fn, what,
                bad_word, bad_word >= 0 ? w[ bad_word ] : 0u, bad_word >= 0 ? r[ bad_word ] : 0u );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btTriangleMeshShape::setLocalScaling 0x60C1C0 + btBvhTriangleMeshShape::setLocalScaling 0x5AF300 match retail over %d "
                     "meshes, exact (%d aabbs changed, %d NaN scaling lanes)",
            static_cast< int >( g_golden_mesh_scaling_count ), changed, nan_lanes );
    return failed == 0;
}
// verify_compound_remove (boat batch 20): cases/boat.py compound_remove -
// btCompoundShape::removeChildShape 0x527110 (compound.hxx compound_remove
// _child_shape -> removeChildShapeByIndex 0x527030 + the typed
// recalculateLocalAabb 0x527160) on the typed compound built like retail's
// (ctor + 0 or 1 addChildShape of a box): before the row (count, local aabb,
// revision) and after, exact.
inline bool verify_compound_remove( )
{
    int failed = 0, removed = 0;
    for ( std::size_t index = 0; index < g_golden_compound_remove_count; ++index )
    {
        const golden_compound_remove_t& g = g_golden_compound_remove[ index ];
        convex_shape_t box{}, other{};
        for ( convex_shape_t* s : { &box, &other } )
        {
            s->type = 0;
            s->local_scaling[ 0 ] = s->local_scaling[ 1 ] = s->local_scaling[ 2 ] = 1.0f;
            std::memcpy( &s->margin, &g.m_margin, 4 );
        }
        std::memcpy( box.implicit, g.m_half, 16 );
        other.implicit[ 0 ] = 1.0f;
        other.implicit[ 1 ] = 2.0f;
        other.implicit[ 2 ] = 3.0f;
        float t[ 16 ];
        std::memcpy( t, g.m_transform, 64 );
        compound_shape_t c;
        const char* what = nullptr;
        if ( g.m_n != 0 && compound_add_child( c, t, box ) != 1 )
            what = "addChildShape reached unrecreated code";
        unsigned w[ 10 ];
        const auto snap = [ & ]( ) {
            w[ 0 ] = static_cast< unsigned >( c.children.size( ) );
            std::memcpy( w + 1, c.local_aabb_min, 16 );
            std::memcpy( w + 5, c.local_aabb_max, 16 );
            w[ 9 ] = static_cast< unsigned >( c.revision );
        };
        snap( );
        if ( !what && std::memcmp( w, g.m_before, 40 ) != 0 )
            what = "the compound before the row";
        if ( !what )
        {
            const convex_shape_t* target = g.m_target == 0 ? &box : g.m_target == 1 ? &other : nullptr;
            if ( compound_remove_child_shape( c, target ) != 1 )
                what = "the row reached unrecreated code";
            snap( );
            removed += g.m_count < g.m_before[ 0 ] ? 1 : 0;
            if ( !what && ( w[ 0 ] != g.m_count || w[ 9 ] != g.m_revision ) )
                what = "the count / revision";
            if ( !what && std::memcmp( w + 1, g.m_aabb, 32 ) != 0 )
                what = "the local aabb";
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "compound remove vector %d: %s", static_cast< int >( index ), what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btCompoundShape::removeChildShape 0x527110 (+ 0x527030, the typed recalculateLocalAabb 0x527160) matches retail over %d "
                     "compounds, exact (%d children removed)",
            static_cast< int >( g_golden_compound_remove_count ), removed );
    return failed == 0;
}
// verify_hull_scaling (boat batch 20): cases/boat.py hull_scaling -
// btPolyhedralConvexAabbCachingShape::setLocalScaling 0x54B5E0 (support.hxx
// shape_set_aabb_caching_scaling) on the hull built by the
// btConvexTriangleMeshShape ctor under a mesh scaling, the margin set: the
// shape's scaling and cached aabb before and after, the mesh scaling, exact.
inline bool verify_hull_scaling( )
{
    const golden_bvh_asset_t& ha = g_golden_bvh_assets[ 0 ];
    int failed = 0, nan_lanes = 0;
    for ( std::size_t index = 0; index < g_golden_hull_scaling_count; ++index )
    {
        const golden_hull_scaling_t& g = g_golden_hull_scaling[ index ];
        striding_mesh_t mesh{};
        std::memcpy( mesh.scaling, g.m_s0, 16 );
        mesh.vertices = reinterpret_cast< const float( * )[ 4 ] >( ha.m_verts );
        mesh.num_vertices = ha.m_nverts;
        mesh.indices = ha.m_indices;
        mesh.index_count = ha.m_nindices;
        convex_shape_t s;
        shape_init_triangle_mesh_hull( s, &mesh );
        std::memcpy( &s.margin, &g.m_margin, 4 );
        unsigned w[ 12 ];
        const auto snap = [ & ]( ) {
            std::memcpy( w, s.local_scaling, 16 );
            std::memcpy( w + 4, s.local_aabb_min, 16 );
            std::memcpy( w + 8, s.local_aabb_max, 16 );
        };
        snap( );
        const char* what = nullptr;
        if ( std::memcmp( w, g.m_before, 48 ) != 0 )
            what = "the hull before the row";
        if ( !what )
        {
            float s1[ 4 ];
            std::memcpy( s1, g.m_s1, 16 );
            for ( int k = 0; k < 3; ++k )
                nan_lanes += std::isnan( s1[ k ] ) ? 1 : 0;
            if ( shape_set_aabb_caching_scaling( s, s1 ) != 1 )
                what = "the row reached unrecreated code";
            snap( );
            if ( !what && std::memcmp( w, g.m_scaling, 16 ) != 0 )
                what = "the shape's scaling";
            if ( !what && std::memcmp( w + 4, g.m_aabb, 32 ) != 0 )
                what = "the cached aabb";
            if ( !what && std::memcmp( mesh.scaling, g.m_mesh_scaling, 16 ) != 0 )
                what = "the mesh scaling";
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "hull scaling vector %d: %s", static_cast< int >( index ), what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btPolyhedralConvexAabbCachingShape::setLocalScaling 0x54B5E0 (+ recalcLocalAabb 0x54B700) matches retail over %d hulls, "
                     "exact (%d NaN scaling lanes)",
            static_cast< int >( g_golden_hull_scaling_count ), nan_lanes );
    return failed == 0;
}