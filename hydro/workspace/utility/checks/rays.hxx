#pragma once

#include <workspace/core/collision/raytest.hxx>
#include <workspace/core/boat/rays.hxx>
#include <workspace/core/boat/effects.hxx>

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