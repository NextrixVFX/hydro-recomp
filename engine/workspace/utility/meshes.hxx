#pragma once

#include <workspace/utility/narrow.hxx>

// The mesh and compound checks against retail: the BVH walk, ConvexConcave, the compound, the dispatch layer and the cached local aabbs.
// (Split out of verify.hxx; verify.hxx's verify_all runs them.)

// btBvhTriangleMeshShape::processAllTriangles @0x5AF0F0 against retail on
// real asset BVHs: the reported triangles in call order (all 12 lanes, part,
// index) and the maxIterations global 0x7590B4 from a seeded prior value.
// Coverage: queries with hits, with none, NaN lanes, and the total triangles.
inline bool verify_bvh_walk( )
{
    struct recorder_t
    {
        std::vector< unsigned > words;
    };
    int failed = 0, hit = 0, miss = 0, triangles = 0, raised = 0;
    for ( std::size_t index = 0; index < g_golden_bvh_walk_count; ++index )
    {
        const golden_bvh_walk_t& g = g_golden_bvh_walk[ index ];
        const golden_bvh_asset_t& a = g_golden_bvh_assets[ g.m_asset ];
        quantized_bvh_t bvh{};
        std::memcpy( bvh.aabb_min, a.m_header + 0, 16 );
        std::memcpy( bvh.aabb_max, a.m_header + 4, 16 );
        std::memcpy( bvh.quantization, a.m_header + 8, 16 );
        bvh.cur_node_index = static_cast< int >( a.m_header[ 13 ] );
        bvh.use_quantization = static_cast< uint8_t >( a.m_quantized );
        bvh.nodes = reinterpret_cast< const bvh_node_t* >( a.m_nodes );
        bvh.traversal_mode = a.m_traversal;
        striding_mesh_t mesh{};
        std::memcpy( mesh.scaling, g.m_scaling, 16 );
        mesh.vertices = reinterpret_cast< const float( * )[ 4 ] >( a.m_verts );
        mesh.num_vertices = a.m_nverts;
        mesh.indices = a.m_indices;
        mesh.index_count = a.m_nindices;
        recorder_t rec;
        const bvh_triangle_callback_t cb{ &rec, []( void* user, float ( *tri )[ 4 ], int part, int tri_index ) {
                                             auto& r = *static_cast< recorder_t* >( user );
                                             unsigned w[ 12 ];
                                             std::memcpy( w, tri, 48 );
                                             r.words.insert( r.words.end( ), w, w + 12 );
                                             r.words.push_back( static_cast< unsigned >( part ) );
                                             r.words.push_back( static_cast< unsigned >( tri_index ) );
                                         } };
        float mn[ 4 ], mx[ 4 ];
        std::memcpy( mn, g.m_min, 16 );
        std::memcpy( mx, g.m_max, 16 );
        g_bvh_max_iterations = g.m_prior;
        const int result = bvh_mesh_process_all_triangles( bvh, mesh, cb, mn, mx );
        const unsigned* want = g_golden_bvh_records + 14 * g.m_first;
        const std::size_t n = static_cast< std::size_t >( g.m_count ) * 14;
        int first = -1;
        if ( rec.words.size( ) == n )
            for ( std::size_t w = 0; w < n && first < 0; ++w )
                if ( rec.words[ w ] != want[ w ] )
                    first = static_cast< int >( w );
        if ( result != 0 || rec.words.size( ) != n || first >= 0 || g_bvh_max_iterations != g.m_after )
        {
            ++failed;
            LOG_ERROR( "bvh walk vector %d: %d triangles (retail %d), first differing word %d, maxIterations %d (retail %d)",
                static_cast< int >( index ), static_cast< int >( rec.words.size( ) / 14 ), g.m_count, first, g_bvh_max_iterations,
                g.m_after );
        }
        ( g.m_count ? hit : miss ) += 1;
        triangles += g.m_count;
        raised += g.m_after != g.m_prior ? 1 : 0;
    }
    g_bvh_max_iterations = 0;
    if ( failed == 0 )
        LOG_SUCCESS( "the BVH walk 0x5AF0F0 -> 0x587510 -> 0x5885E0 -> 0x5AF180 matches retail on %d queries over %d real "
                     "assets, exact (%d with hits, %d without, %d triangles, maxIterations raised %d times)",
            static_cast< int >( g_golden_bvh_walk_count ), 2, hit, miss, triangles, raised );
    return failed == 0;
}

// btConvexConcaveCollisionAlgorithm::processCollision @0x5F6CF0 against
// retail end to end on the real level mesh (the bvh_walk block's asset 1):
// one or two calls on the same manifold, then the four points (masked as
// verify_convex_convex), the count, the callback's aabb + margin and the
// result's part / index ids. Counted: kept contacts, two-call vectors.
inline bool verify_convex_concave_table( const golden_convex_concave_t* table, std::size_t count, const char* label )
{
    const golden_bvh_asset_t& a = g_golden_bvh_assets[ 1 ];
    quantized_bvh_t bvh{};
    std::memcpy( bvh.aabb_min, a.m_header + 0, 16 );
    std::memcpy( bvh.aabb_max, a.m_header + 4, 16 );
    std::memcpy( bvh.quantization, a.m_header + 8, 16 );
    bvh.cur_node_index = static_cast< int >( a.m_header[ 13 ] );
    bvh.use_quantization = static_cast< uint8_t >( a.m_quantized );
    bvh.nodes = reinterpret_cast< const bvh_node_t* >( a.m_nodes );
    bvh.traversal_mode = a.m_traversal;
    striding_mesh_t level{};
    level.scaling[ 0 ] = level.scaling[ 1 ] = level.scaling[ 2 ] = 1.0f;
    level.vertices = reinterpret_cast< const float( * )[ 4 ] >( a.m_verts );
    level.num_vertices = a.m_nverts;
    level.indices = a.m_indices;
    level.index_count = a.m_nindices;
    int failed = 0, kept = 0, twice = 0, spheres = 0;
    for ( std::size_t index = 0; index < count; ++index )
    {
        const golden_convex_concave_t& g = table[ index ];
        const golden_epa_shape_t& gs = g.m_shape;
        float hull_verts[ 12 ][ 4 ];
        striding_mesh_t hull{};
        convex_shape_t s{};
        s.type = gs.m_kind;
        std::memcpy( s.implicit, gs.m_half, 16 );
        std::memcpy( &s.margin, &gs.m_margin, 4 );
        std::memcpy( s.local_scaling, gs.m_scaling, 16 );
        s.up_axis = gs.m_axis;
        std::memcpy( s.local_aabb_min, g.m_lo, 16 );
        std::memcpy( s.local_aabb_max, g.m_hi, 16 );
        if ( gs.m_kind == 3 )
        {
            std::memcpy( hull_verts, gs.m_verts, sizeof( hull_verts ) );
            std::memcpy( hull.scaling, gs.m_scaling, 16 );
            hull.vertices = hull_verts;
            hull.num_vertices = gs.m_nv;
            hull.indices = gs.m_ids;
            hull.index_count = gs.m_count;
            s.mesh = &hull;
        }
        bvh_mesh_shape_t tri_shape{ 21, 0.0f, &level, &bvh };
        std::memcpy( &tri_shape.margin, &g.m_tmargin, 4 );
        static solver_rigid_t objects[ 2 ];
        for ( int k = 0; k < 2; ++k )
        {
            objects[ k ] = solver_rigid_t{};
            std::memcpy( &objects[ k ].friction, &g.m_friction[ k ], 4 );
            std::memcpy( &objects[ k ].restitution, &g.m_restitution[ k ], 4 );
        }
        static solver_manifold_t m;
        m = solver_manifold_t{};
        std::memcpy( &m.contact_breaking_threshold, &g.m_threshold, 4 );
        simplex_solver_t simplex;
        std::memcpy( &simplex, g.m_sim_in, 400 );
        convex_concave_t alg{};
        alg.swapped = g.m_swapped != 0;
        alg.manifold = &m;
        alg.simplex = &simplex;
        alg.pd_solver = true;
        alg.perturbation_iterations = 0;
        alg.minimum_points_threshold = 3;
        float tt[ 16 ];
        std::memcpy( tt, g.m_tt, 64 );
        const char* what = nullptr;
        manifold_result_t r{};
        for ( int c = 0; c < g.m_ncalls && !what; ++c )
        {
            float tc[ 16 ];
            std::memcpy( tc, g.m_calls[ c ], 64 );
            const concave_body_t convex{ tc, &s, nullptr, &objects[ 0 ] };
            const concave_body_t tri{ tt, nullptr, &tri_shape, &objects[ 1 ] };
            const concave_body_t& b0 = alg.swapped ? tri : convex;
            const concave_body_t& b1 = alg.swapped ? convex : tri;
            r = manifold_result_t{};
            std::memcpy( r.basis0, b0.transform, 48 );
            std::memcpy( r.origin0, b0.transform + 12, 16 );
            std::memcpy( r.basis1, b1.transform, 48 );
            std::memcpy( r.origin1, b1.transform + 12, 16 );
            r.object0 = b0.object;
            r.object1 = b1.object;
            contact_callbacks_t cb;
            if ( convex_concave_process( alg, b0, b1, r, cb, nullptr, nullptr ) != 1 )
                what = "a path not recreated was reached";
        }
        twice += g.m_ncalls == 2 ? 1 : 0;
        kept += m.num_contacts > 0 ? 1 : 0;
        spheres += gs.m_kind == 8 ? 1 : 0;
        unsigned cbw[ 9 ];
        std::memcpy( cbw, alg.aabb_min, 16 );
        std::memcpy( cbw + 4, alg.aabb_max, 16 );
        std::memcpy( cbw + 8, &alg.margin, 4 );
        const int ids[ 4 ] = { r.part_id0, r.part_id1, r.index0, r.index1 };
        int bad_word = -1;
        unsigned bad_ours = 0, bad_retail = 0;
        if ( what )
            ;
        else if ( r.manifold != &m || m.body0 != &objects[ 0 ] || m.body1 != &objects[ 1 ] )
            what = "result+4 or the manifold's bodies";
        else if ( std::memcmp( cbw, g.m_cb_out, 36 ) != 0 )
            what = "the callback's aabb / margin";
        else if ( std::memcmp( ids, g.m_ids_out, 16 ) != 0 )
            what = "the result's part / index ids";
        else if ( m.num_contacts != g.m_count )
            what = "the count";
        for ( int k = 0; k < 4 && !what; ++k )
        {
            unsigned ours[ 72 ];
            std::memcpy( ours, &m.points[ k ], 288 );
            for ( int w = 0; w < 72 && !what; ++w )
            {
                const bool unknown = ( w >= 37 && w < 48 ) || ( w >= 48 && w < 72 && ( w - 48 ) % 8 != 7 );
                const unsigned mask = w == 29 ? 0xFFu : 0xFFFFFFFFu;
                if ( !unknown && ( ours[ w ] & mask ) != ( g.m_points[ k ][ w ] & mask ) )
                {
                    what = "a point";
                    bad_word = 72 * k + w;
                    bad_ours = ours[ w ];
                    bad_retail = g.m_points[ k ][ w ];
                }
            }
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "%s vector %d (shape %d): %s (word %d: ours 0x%08X, retail 0x%08X)", label, static_cast< int >( index ),
                gs.m_kind, what, bad_word, bad_ours, bad_retail );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "%s matches retail end to end on the real level mesh, %d vectors exact (%d kept a contact, %d called "
                     "twice on one manifold, %d spheres)",
            label, static_cast< int >( count ), kept, twice, spheres );
    return failed == 0;
}

// The ConvexConcave samples (box / cylinder / hull on real triangles, far
// spheres for getAabb 0x5534B0) and the sphere ones (every convex a sphere on
// a real triangle -> SphereTriangle, detector.hxx).
inline bool verify_convex_concave( )
{
    return verify_convex_concave_table( g_golden_convex_concave, g_golden_convex_concave_count,
        "btConvexConcaveCollisionAlgorithm::processCollision" );
}

inline bool verify_sphere_concave( )
{
    return verify_convex_concave_table( g_golden_sphere_concave, g_golden_sphere_concave_count,
        "ConvexConcave + btSphereTriangleCollisionAlgorithm" );
}

// btCompoundCollisionAlgorithm against retail: the compound as retail's ctor
// + addChildShape built it (record, local aabb, the root leaf, revision),
// then four frames of processCollision through the dispatcher model; per
// frame the dispatcher's manifolds in array order (points masked as
// verify_convex_convex, count, bodies, thresholds, +1196), the child slot,
// gNumManifold, result+4 and the ids. Counted: frames with a child, with
// contacts, removals.
inline bool verify_compound( )
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
    striding_mesh_t level{};
    level.scaling[ 0 ] = level.scaling[ 1 ] = level.scaling[ 2 ] = 1.0f;
    level.vertices = reinterpret_cast< const float( * )[ 4 ] >( la.m_verts );
    level.num_vertices = la.m_nverts;
    level.indices = la.m_indices;
    level.index_count = la.m_nindices;
    striding_mesh_t hull_mesh{};
    hull_mesh.scaling[ 0 ] = hull_mesh.scaling[ 1 ] = hull_mesh.scaling[ 2 ] = 1.0f;
    hull_mesh.vertices = reinterpret_cast< const float( * )[ 4 ] >( ha.m_verts );
    hull_mesh.num_vertices = ha.m_nverts;
    hull_mesh.indices = ha.m_indices;
    hull_mesh.index_count = ha.m_nindices;
    int failed = 0, with_child = 0, with_contacts = 0, removals = 0;
    for ( std::size_t index = 0; index < g_golden_compound_count; ++index )
    {
        const golden_compound_t& g = g_golden_compound[ index ];
        const char* what = nullptr;
        int frame_no = -1, bad_word = -1;
        unsigned bad_ours = 0, bad_retail = 0;
        convex_shape_t hull{};
        hull.type = 3;
        hull.local_scaling[ 0 ] = hull.local_scaling[ 1 ] = hull.local_scaling[ 2 ] = 1.0f;
        std::memcpy( &hull.margin, &g.m_hmargin, 4 );
        std::memcpy( hull.local_aabb_min, g.m_hlo, 16 );
        std::memcpy( hull.local_aabb_max, g.m_hhi, 16 );
        hull.mesh = &hull_mesh;
        convex_shape_t box{};
        box.type = 0;
        box.local_scaling[ 0 ] = box.local_scaling[ 1 ] = box.local_scaling[ 2 ] = 1.0f;
        std::memcpy( box.implicit, g.m_half, 16 );
        std::memcpy( &box.margin, &g.m_bmargin, 4 );
        bvh_mesh_shape_t tri{ 21, 0.0f, &level, &bvh };
        std::memcpy( &tri.margin, &g.m_tmargin, 4 );
        std::memcpy( tri.local_aabb_min, la.m_header + 0, 12 );
        std::memcpy( tri.local_aabb_max, la.m_header + 4, 12 );
        float child_t[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 };
        for ( int k = 0; k < 3; ++k )
        {
            float c;
            std::memcpy( &c, &g.m_com[ k ], 4 );
            child_t[ 12 + k ] = support_negate( c );
        }
        compound_shape_t shape;
        if ( compound_add_child( shape, child_t, hull ) != 1 )
            what = "addChildShape reached unrecreated code";
        unsigned w[ 8 ];
        if ( !what )
        {
            unsigned rt[ 16 ];
            std::memcpy( rt, shape.children[ 0 ].transform, 64 );
            std::memcpy( w, shape.local_aabb_min, 12 );
            std::memcpy( w + 3, shape.local_aabb_max, 12 );
            unsigned margin;
            std::memcpy( &margin, &shape.children[ 0 ].margin, 4 );
            if ( std::memcmp( rt, g.m_record, 64 ) != 0 || g.m_record[ 17 ] != 3u || g.m_record[ 18 ] != margin )
                what = "the child record";
            else if ( std::memcmp( w, g.m_built + 8, 12 ) != 0 || std::memcmp( w + 3, g.m_built + 12, 12 ) != 0 )
                what = "the local aabb";
            else if ( static_cast< unsigned >( shape.revision ) != g.m_built[ 17 ] )
                what = "the revision";
            else if ( std::memcmp( shape.root_min, g.m_leaf, 16 ) != 0 || std::memcmp( shape.root_max, g.m_leaf + 4, 16 ) != 0 ||
                      static_cast< unsigned >( shape.root_data ) != g.m_leaf[ 9 ] || g.m_leaf[ 10 ] != 0u )
                what = "the root leaf";
        }
        simplex_solver_t simplex;
        std::memcpy( &simplex, g.m_sim_in, 400 );
        collision_dispatcher_t d;
        d.simplex = &simplex;
        static solver_rigid_t objects[ 2 ];
        for ( int k = 0; k < 2; ++k )
        {
            objects[ k ] = solver_rigid_t{};
            std::memcpy( &objects[ k ].friction, &g.m_friction[ k ], 4 );
            std::memcpy( &objects[ k ].restitution, &g.m_restitution[ k ], 4 );
        }
        compound_algorithm_t alg;
        compound_create( alg, shape, false, nullptr );
        if ( !what && static_cast< unsigned >( alg.revision ) != g.m_alg[ 10 ] )
            what = "the algorithm's revision";
        g_num_manifolds = g.m_numman0;
        float tt[ 16 ];
        std::memcpy( tt, g.m_tt, 64 );
        float p0, p1;
        std::memcpy( &p0, &g.m_pth[ 0 ], 4 );
        std::memcpy( &p1, &g.m_pth[ 1 ], 4 );
        for ( int f = 0; f < 4 && !what; ++f )
        {
            const golden_compound_frame_t& gf = g.m_frames[ f ];
            frame_no = f;
            float t[ 16 ];
            std::memcpy( t, gf.m_t, 64 );
            const concave_body_t cb{ t, nullptr, nullptr, &objects[ 0 ], &shape, p0 };
            const concave_body_t ob{ tt, g.m_level ? nullptr : &box, g.m_level ? &tri : nullptr, &objects[ 1 ], nullptr, p1 };
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
                break;
            }
            const int kind = alg.children[ 0 ] ? alg.children[ 0 ]->kind : 0;
            with_child += kind ? 1 : 0;
            removals += ( had && !kind ) ? 1 : 0;
            int rman = -1;
            for ( std::size_t m = 0; m < d.manifolds.size( ); ++m )
                if ( d.manifolds[ m ] == r.manifold )
                    rman = static_cast< int >( m );
            if ( r.manifold && rman < 0 )
                rman = -2;
            const int ids[ 4 ] = { r.part_id0, r.part_id1, r.index0, r.index1 };
            if ( kind != gf.m_kind )
                what = "the child slot";
            else if ( g_num_manifolds != gf.m_numman )
                what = "gNumManifold";
            else if ( static_cast< int >( d.manifolds.size( ) ) != gf.m_nmans )
                what = "the dispatcher's manifold count";
            else if ( rman != gf.m_rman )
                what = "result+4";
            else if ( std::memcmp( ids, gf.m_ids, 16 ) != 0 )
                what = "the result's ids";
            for ( int m = 0; m < gf.m_nmans && m < 2 && !what; ++m )
            {
                const solver_manifold_t& ours = *d.manifolds[ m ];
                const golden_compound_manifold_t& gm = gf.m_mans[ m ];
                const int b0 = ours.body0 == &objects[ 0 ] ? 0 : ours.body0 == &objects[ 1 ] ? 1 : -1;
                const int b1 = ours.body1 == &objects[ 0 ] ? 0 : ours.body1 == &objects[ 1 ] ? 1 : -1;
                unsigned th[ 2 ];
                std::memcpy( th, &ours.contact_breaking_threshold, 4 );
                std::memcpy( th + 1, &ours.contact_processing_threshold, 4 );
                with_contacts += ours.num_contacts > 0 ? 1 : 0;
                if ( ours.num_contacts != gm.m_count )
                    what = "a manifold's count";
                else if ( b0 != gm.m_bodies[ 0 ] || b1 != gm.m_bodies[ 1 ] )
                    what = "a manifold's bodies";
                else if ( std::memcmp( th, gm.m_thresholds, 8 ) != 0 )
                {
                    what = "a manifold's thresholds";
                    bad_ours = th[ 0 ];
                    bad_retail = gm.m_thresholds[ 0 ];
                }
                else if ( ours.dispatcher_index != gm.m_index )
                    what = "a manifold's +1196";
                for ( int k = 0; k < 4 && !what; ++k )
                {
                    unsigned pw[ 72 ];
                    std::memcpy( pw, &ours.points[ k ], 288 );
                    for ( int x = 0; x < 72 && !what; ++x )
                    {
                        const bool unknown = ( x >= 37 && x < 48 ) || ( x >= 48 && x < 72 && ( x - 48 ) % 8 != 7 );
                        const unsigned mask = x == 29 ? 0xFFu : 0xFFFFFFFFu;
                        if ( !unknown && ( pw[ x ] & mask ) != ( gm.m_points[ k ][ x ] & mask ) )
                        {
                            what = "a manifold point";
                            bad_word = 72 * k + x;
                            bad_ours = pw[ x ];
                            bad_retail = gm.m_points[ k ][ x ];
                        }
                    }
                }
            }
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "compound vector %d (%s) frame %d: %s (word %d: ours 0x%08X, retail 0x%08X)", static_cast< int >( index ),
                g.m_level ? "level" : "box", frame_no, what, bad_word, bad_ours, bad_retail );
        }
        for ( auto& a : alg.children )
            a.reset( );
    }
    g_num_manifolds = 0;
    if ( failed == 0 )
        LOG_SUCCESS( "btCompoundCollisionAlgorithm matches retail over %d vectors x 4 frames on the real boat hull, exact (%d "
                     "frames with a child, %d contact manifolds, %d removals)",
            static_cast< int >( g_golden_compound_count ), with_child, with_contacts, removals );
    return failed == 0;
}

// dispatchAllCollisionPairs against retail: the boat compound, the level, two
// boxes, four pairs in the golden order, four frames through the nearCallback
// model; per frame the dispatcher's manifolds in array order (masked as
// verify_compound), gNumManifold, and each pair's algorithm kind, child kind
// and isSwapped. Counted: pairs skipped by needsCollision, swapped algorithms,
// contact manifolds.
inline bool verify_dispatch( )
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
    striding_mesh_t level{};
    level.scaling[ 0 ] = level.scaling[ 1 ] = level.scaling[ 2 ] = 1.0f;
    level.vertices = reinterpret_cast< const float( * )[ 4 ] >( la.m_verts );
    level.num_vertices = la.m_nverts;
    level.indices = la.m_indices;
    level.index_count = la.m_nindices;
    striding_mesh_t hull_mesh{};
    hull_mesh.scaling[ 0 ] = hull_mesh.scaling[ 1 ] = hull_mesh.scaling[ 2 ] = 1.0f;
    hull_mesh.vertices = reinterpret_cast< const float( * )[ 4 ] >( ha.m_verts );
    hull_mesh.num_vertices = ha.m_nverts;
    hull_mesh.indices = ha.m_indices;
    hull_mesh.index_count = ha.m_nindices;
    int failed = 0, skipped = 0, swapped_algs = 0, with_contacts = 0;
    for ( std::size_t index = 0; index < g_golden_dispatch_count; ++index )
    {
        const golden_dispatch_t& g = g_golden_dispatch[ index ];
        const char* what = nullptr;
        int frame_no = -1, bad_word = -1;
        unsigned bad_ours = 0, bad_retail = 0;
        convex_shape_t hull{};
        hull.type = 3;
        hull.local_scaling[ 0 ] = hull.local_scaling[ 1 ] = hull.local_scaling[ 2 ] = 1.0f;
        std::memcpy( &hull.margin, &g.m_hmargin, 4 );
        std::memcpy( hull.local_aabb_min, g.m_hlo, 16 );
        std::memcpy( hull.local_aabb_max, g.m_hhi, 16 );
        hull.mesh = &hull_mesh;
        convex_shape_t box{};
        box.type = 0;
        box.local_scaling[ 0 ] = box.local_scaling[ 1 ] = box.local_scaling[ 2 ] = 1.0f;
        std::memcpy( box.implicit, g.m_half, 16 );
        std::memcpy( &box.margin, &g.m_bmargin, 4 );
        bvh_mesh_shape_t tri{ 21, 0.0f, &level, &bvh };
        std::memcpy( &tri.margin, &g.m_tmargin, 4 );
        std::memcpy( tri.local_aabb_min, la.m_header + 0, 12 );
        std::memcpy( tri.local_aabb_max, la.m_header + 4, 12 );
        float child_t[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 };
        for ( int k = 0; k < 3; ++k )
        {
            float c;
            std::memcpy( &c, &g.m_com[ k ], 4 );
            child_t[ 12 + k ] = support_negate( c );
        }
        compound_shape_t shape;
        if ( compound_add_child( shape, child_t, hull ) != 1 )
            what = "addChildShape reached unrecreated code";
        simplex_solver_t simplex;
        std::memcpy( &simplex, g.m_sim_in, 400 );
        collision_dispatcher_t d;
        d.simplex = &simplex;
        static solver_rigid_t objects[ 4 ];
        float transforms[ 4 ][ 16 ] = {};
        transforms[ 1 ][ 0 ] = transforms[ 1 ][ 5 ] = transforms[ 1 ][ 10 ] = 1.0f;
        collision_object_t bodies[ 4 ];
        for ( int k = 0; k < 4; ++k )
        {
            objects[ k ] = solver_rigid_t{};
            std::memcpy( &objects[ k ].friction, &g.m_friction[ k ], 4 );
            std::memcpy( &objects[ k ].restitution, &g.m_restitution[ k ], 4 );
            float p;
            std::memcpy( &p, &g.m_pth[ k ], 4 );
            bodies[ k ].body = concave_body_t{ transforms[ k ], k >= 2 ? &box : nullptr, k == 1 ? &tri : nullptr, &objects[ k ],
                k == 0 ? &shape : nullptr, p };
            bodies[ k ].activation_state = g.m_states[ k ];
        }
        std::vector< overlapping_pair_t > pairs( 4 );
        pair_algorithm_store_t store;
        for ( int k = 0; k < 4; ++k )
        {
            pairs[ k ].object0 = &bodies[ g.m_pair_objects[ k ][ 0 ] ];
            pairs[ k ].object1 = &bodies[ g.m_pair_objects[ k ][ 1 ] ];
        }
        g_num_manifolds = g.m_numman0;
        for ( int f = 0; f < 4 && !what; ++f )
        {
            const golden_dispatch_frame_t& gf = g.m_frames[ f ];
            frame_no = f;
            std::memcpy( transforms[ 0 ], g.m_boat[ f ], 64 );
            std::memcpy( transforms[ 2 ], g.m_box[ f ], 64 );
            std::memcpy( transforms[ 3 ], g.m_box2[ f ], 64 );
            dispatch_info_t info{};
            if ( dispatch_all_collision_pairs( d, store, pairs, info ) != 1 )
            {
                what = "a path not recreated was reached";
                break;
            }
            for ( int k = 0; k < 4 && !what; ++k )
            {
                const pair_algorithm_t* a = pairs[ k ].algorithm;
                const int kind = a ? a->kind : 0;
                const int child = ( kind == 3 && !a->compound.children.empty( ) && a->compound.children[ 0 ] ) ? a->compound.children[ 0 ]->kind : 0;
                const int sw = kind == 3 ? a->compound.swapped : kind == 2 ? a->leaf.concave.swapped : 0;
                skipped += kind == 0 ? 1 : 0;
                swapped_algs += sw;
                if ( kind != gf.m_pairs[ k ][ 0 ] || child != gf.m_pairs[ k ][ 1 ] || sw != gf.m_pairs[ k ][ 2 ] )
                {
                    what = "a pair's algorithm";
                    bad_word = k;
                }
            }
            if ( what )
                break;
            if ( g_num_manifolds != gf.m_numman )
                what = "gNumManifold";
            else if ( static_cast< int >( d.manifolds.size( ) ) != gf.m_nmans )
                what = "the dispatcher's manifold count";
            for ( int m = 0; m < gf.m_nmans && m < 4 && !what; ++m )
            {
                const solver_manifold_t& ours = *d.manifolds[ m ];
                const golden_compound_manifold_t& gm = gf.m_mans[ m ];
                int b0 = -1, b1 = -1;
                for ( int k = 0; k < 4; ++k )
                {
                    if ( ours.body0 == &objects[ k ] )
                        b0 = k;
                    if ( ours.body1 == &objects[ k ] )
                        b1 = k;
                }
                unsigned th[ 2 ];
                std::memcpy( th, &ours.contact_breaking_threshold, 4 );
                std::memcpy( th + 1, &ours.contact_processing_threshold, 4 );
                with_contacts += ours.num_contacts > 0 ? 1 : 0;
                if ( ours.num_contacts != gm.m_count )
                    what = "a manifold's count";
                else if ( b0 != gm.m_bodies[ 0 ] || b1 != gm.m_bodies[ 1 ] )
                    what = "a manifold's bodies";
                else if ( std::memcmp( th, gm.m_thresholds, 8 ) != 0 )
                {
                    what = "a manifold's thresholds";
                    bad_ours = th[ 0 ];
                    bad_retail = gm.m_thresholds[ 0 ];
                }
                else if ( ours.dispatcher_index != gm.m_index )
                    what = "a manifold's +1196";
                for ( int k = 0; k < 4 && !what; ++k )
                {
                    unsigned pw[ 72 ];
                    std::memcpy( pw, &ours.points[ k ], 288 );
                    for ( int x = 0; x < 72 && !what; ++x )
                    {
                        const bool unknown = ( x >= 37 && x < 48 ) || ( x >= 48 && x < 72 && ( x - 48 ) % 8 != 7 );
                        const unsigned mask = x == 29 ? 0xFFu : 0xFFFFFFFFu;
                        if ( !unknown && ( pw[ x ] & mask ) != ( gm.m_points[ k ][ x ] & mask ) )
                        {
                            what = "a manifold point";
                            bad_word = 1000 * m + 72 * k + x;
                            bad_ours = pw[ x ];
                            bad_retail = gm.m_points[ k ][ x ];
                        }
                    }
                }
            }
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "dispatch vector %d frame %d: %s (word %d: ours 0x%08X, retail 0x%08X)", static_cast< int >( index ), frame_no,
                what, bad_word, bad_ours, bad_retail );
        }
        pairs.clear( );
    }
    g_num_manifolds = 0;
    if ( failed == 0 )
        LOG_SUCCESS( "dispatchAllCollisionPairs + the nearCallback match retail over %d vectors x 4 frames x 4 pairs, exact (%d "
                     "pair-frames skipped, %d swapped, %d contact manifolds)",
            static_cast< int >( g_golden_dispatch_count ), skipped, swapped_algs, with_contacts );
    return failed == 0;
}

// recalcLocalAabb against retail: the boat hull built as the ctor 0x526380
// builds it (margin 0.04, the recalc), then setMargin + setLocalScaling
// (identity, positive, negative, zero, wide and NaN lanes); every cached
// aabb dword after the ctor and after the rescale, and the mesh scaling.
inline bool verify_hull_recalc( )
{
    const golden_bvh_asset_t& ha = g_golden_bvh_assets[ 0 ];
    int failed = 0, nan_vectors = 0;
    for ( std::size_t index = 0; index < g_golden_hull_recalc_count; ++index )
    {
        const golden_hull_recalc_t& g = g_golden_hull_recalc[ index ];
        striding_mesh_t mesh{};
        mesh.scaling[ 0 ] = mesh.scaling[ 1 ] = mesh.scaling[ 2 ] = 1.0f;
        mesh.vertices = reinterpret_cast< const float( * )[ 4 ] >( ha.m_verts );
        mesh.num_vertices = ha.m_nverts;
        mesh.indices = ha.m_indices;
        mesh.index_count = ha.m_nindices;
        convex_shape_t s;
        shape_init_triangle_mesh_hull( s, &mesh );
        const char* what = nullptr;
        int bad_word = -1;
        unsigned w[ 8 ];
        std::memcpy( w, s.local_aabb_min, 16 );
        std::memcpy( w + 4, s.local_aabb_max, 16 );
        unsigned m;
        std::memcpy( &m, &s.margin, 4 );
        if ( m != g.m_ctor_margin || g.m_valid != 1 )
            what = "the ctor's margin / valid byte";
        for ( int k = 0; k < 8 && !what; ++k )
            if ( w[ k ] != g.m_ctor_aabb[ k ] )
            {
                what = "the ctor's aabb";
                bad_word = k;
            }
        if ( !what )
        {
            std::memcpy( &s.margin, &g.m_margin, 4 );
            float sc[ 4 ];
            std::memcpy( sc, g.m_scaling, 16 );
            for ( int k = 0; k < 3; ++k )
                nan_vectors += std::isnan( sc[ k ] ) ? 1 : 0;
            shape_set_triangle_mesh_scaling( s, mesh, sc );
            std::memcpy( w, s.local_aabb_min, 16 );
            std::memcpy( w + 4, s.local_aabb_max, 16 );
            for ( int k = 0; k < 8 && !what; ++k )
                if ( w[ k ] != g.m_aabb[ k ] )
                {
                    what = "the rescaled aabb";
                    bad_word = k;
                }
            if ( !what && std::memcmp( mesh.scaling, g.m_mesh_scaling, 16 ) != 0 )
                what = "the mesh scaling";
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "hull recalc vector %d: %s (word %d: ours 0x%08X, retail 0x%08X)", static_cast< int >( index ), what, bad_word,
                bad_word >= 0 ? w[ bad_word ] : 0u, bad_word >= 0 ? g.m_aabb[ bad_word ] : 0u );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "recalcLocalAabb + the btConvexTriangleMeshShape ctor + setLocalScaling match retail over %d hulls, exact "
                     "(%d with a NaN scaling lane)",
            static_cast< int >( g_golden_hull_recalc_count ), nan_vectors );
    return failed == 0;
}

// btTriangleMeshShape::recalcLocalAabb against retail: the BVH shape ctor on
// the level or the hull mesh under a pre-set scaling (identity, random, huge
// past the 1e18 filter, negative, NaN lanes), then on the level the margin
// and setOptimizedBvh (unchanged / near / rescaled -> the BVH-walk recalc).
inline bool verify_mesh_recalc( )
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
    int failed = 0, rescaled = 0, empty = 0;
    for ( std::size_t index = 0; index < g_golden_mesh_recalc_count; ++index )
    {
        const golden_mesh_recalc_t& g = g_golden_mesh_recalc[ index ];
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
        if ( bvh_mesh_shape_init( s, &mesh ) != 1 )
            what = "the ctor reached unrecreated code";
        std::memcpy( w, s.local_aabb_min, 16 );
        std::memcpy( w + 4, s.local_aabb_max, 16 );
        empty += ( !what && w[ 0 ] == 0 && w[ 4 ] == 0 ) ? 1 : 0;
        unsigned m;
        std::memcpy( &m, &s.margin, 4 );
        if ( !what && m != g.m_ctor_margin )
            what = "the ctor's margin";
        for ( int k = 0; k < 8 && !what; ++k )
            if ( w[ k ] != g.m_ctor_aabb[ k ] )
            {
                what = "the ctor's aabb";
                bad_word = k;
                std::memcpy( r, g.m_ctor_aabb, 32 );
            }
        if ( !what && g.m_level )
        {
            std::memcpy( &s.margin, &g.m_margin, 4 );
            float s1[ 4 ];
            std::memcpy( s1, g.m_s1, 16 );
            if ( bvh_mesh_set_optimized_bvh( s, mesh, &bvh, s1 ) != 1 )
                what = "setOptimizedBvh reached unrecreated code";
            std::memcpy( w, s.local_aabb_min, 16 );
            std::memcpy( w + 4, s.local_aabb_max, 16 );
            rescaled += std::memcmp( g.m_aabb, g.m_ctor_aabb, 32 ) != 0 ? 1 : 0;
            for ( int k = 0; k < 8 && !what; ++k )
                if ( w[ k ] != g.m_aabb[ k ] )
                {
                    what = "the aabb after setOptimizedBvh";
                    bad_word = k;
                    std::memcpy( r, g.m_aabb, 32 );
                }
            if ( !what && std::memcmp( mesh.scaling, g.m_mesh_scaling, 16 ) != 0 )
                what = "the mesh scaling";
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "mesh recalc vector %d: %s (word %d: ours 0x%08X, retail 0x%08X)", static_cast< int >( index ), what, bad_word,
                bad_word >= 0 ? w[ bad_word ] : 0u, bad_word >= 0 ? r[ bad_word ] : 0u );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btTriangleMeshShape::recalcLocalAabb + the BVH shape ctor + setOptimizedBvh match retail over %d meshes, exact "
                     "(%d rescaled through the BVH walk, %d filtered empty)",
            static_cast< int >( g_golden_mesh_recalc_count ), rescaled, empty );
    return failed == 0;
}
