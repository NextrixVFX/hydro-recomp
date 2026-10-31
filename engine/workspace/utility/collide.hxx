#pragma once

#include <workspace/core/collision/world.hxx>
#include <workspace/utility/golden.hxx>
#include <workspace/utility/logger.hxx>

#include <cstring>
#include <memory>

// The collision stage inside the dynamics world, against retail
// (case_world_collide, golden `wcl`): bodies added through addRigidBody
// 0x6013E0 into a world_t with the collision world attached, then six frames
// of the world's collide stage (world_stage_collide_attached).

// One frame of a collision world against golden_collision_stage_frame_t: the
// pair cache in order {object0, object1, kind, child kind, swapped}, then
// gNumManifold and the dispatcher's manifolds (masked as verify_compound).
inline const char* collide_compare_frame( const collision_world_t& w, const collision_object_t* bodies, const solver_rigid_t* objects,
    const golden_collision_stage_frame_t& gf, int& bad_word, unsigned& bad_ours, unsigned& bad_retail, int& skipped, int& with_contacts )
{
    const pair_cache_t& c = w.broadphase->cache;
    if ( c.pairs.size != gf.m_npairs )
    {
        bad_ours = static_cast< unsigned >( c.pairs.size );
        bad_retail = static_cast< unsigned >( gf.m_npairs );
        return "the pair count";
    }
    for ( int k = 0; k < gf.m_npairs; ++k )
    {
        const broadphase_pair_t& p = c.pairs.data[ k ];
        const pair_algorithm_t* a = static_cast< const pair_algorithm_t* >( p.algorithm );
        const int kind = a ? a->kind : 0;
        const int child = ( kind == 3 && !a->compound.children.empty( ) && a->compound.children[ 0 ] ) ? a->compound.children[ 0 ]->kind : 0;
        const int sw = kind == 3 ? a->compound.swapped : kind == 2 ? a->leaf.concave.swapped : 0;
        const int i0 = static_cast< int >( static_cast< const collision_object_t* >( p.proxy0->client ) - bodies );
        const int i1 = static_cast< int >( static_cast< const collision_object_t* >( p.proxy1->client ) - bodies );
        skipped += kind == 0 ? 1 : 0;
        const int ours[ 5 ] = { i0, i1, kind, child, sw };
        for ( int x = 0; x < 5; ++x )
            if ( ours[ x ] != gf.m_pairs[ k ][ x ] )
            {
                bad_word = 5 * k + x;
                bad_ours = static_cast< unsigned >( ours[ x ] );
                bad_retail = static_cast< unsigned >( gf.m_pairs[ k ][ x ] );
                return "a pair";
            }
    }
    const collision_dispatcher_t& d = w.dispatcher;
    if ( g_num_manifolds != gf.m_numman )
        return "gNumManifold";
    if ( static_cast< int >( d.manifolds.size( ) ) != gf.m_nmans )
        return "the dispatcher's manifold count";
    for ( int m = 0; m < gf.m_nmans && m < 4; ++m )
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
            return "a manifold's count";
        if ( b0 != gm.m_bodies[ 0 ] || b1 != gm.m_bodies[ 1 ] )
            return "a manifold's bodies";
        if ( std::memcmp( th, gm.m_thresholds, 8 ) != 0 )
        {
            bad_ours = th[ 0 ];
            bad_retail = gm.m_thresholds[ 0 ];
            return "a manifold's thresholds";
        }
        if ( ours.dispatcher_index != gm.m_index )
            return "a manifold's +1196";
        for ( int k = 0; k < 4; ++k )
        {
            unsigned pw[ 72 ];
            std::memcpy( pw, &ours.points[ k ], 288 );
            for ( int x = 0; x < 72; ++x )
            {
                const bool unknown = ( x >= 37 && x < 48 ) || ( x >= 48 && x < 72 && ( x - 48 ) % 8 != 7 );
                const unsigned mask = x == 29 ? 0xFFu : 0xFFFFFFFFu;
                if ( !unknown && ( pw[ x ] & mask ) != ( gm.m_points[ k ][ x ] & mask ) )
                {
                    bad_word = 1000 * m + 72 * k + x;
                    bad_ours = pw[ x ];
                    bad_retail = gm.m_points[ k ][ x ];
                    return "a manifold point";
                }
            }
        }
    }
    return nullptr;
}

// SphereSphere against retail (case_sphere_stage, golden `sphere_stage`): the
// boat compound, the level, two spheres beyond the level's aabb (touching /
// apart / coincident), added through addCollisionObject; six frames, sphere
// 3 sent away at frame 3 in the even vectors (the pair removal -> the dtor).
inline bool verify_far_stage( const golden_collision_stage_t* table, std::size_t count, bool spheres, const char* label )
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
    int failed = 0, skipped = 0, with_contacts = 0, sphere_pairs = 0, removals = 0;
    for ( std::size_t index = 0; index < count; ++index )
    {
        const golden_collision_stage_t& g = table[ index ];
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
        convex_shape_t sphere[ 2 ] = {};
        for ( int k = 0; k < 2; ++k )
        {
            sphere[ k ].local_scaling[ 0 ] = sphere[ k ].local_scaling[ 1 ] = sphere[ k ].local_scaling[ 2 ] = 1.0f;
            if ( spheres )
            {
                sphere[ k ].type = 8;
                std::memcpy( &sphere[ k ].implicit[ 0 ], &g.m_half[ k ], 4 );
                std::memcpy( &sphere[ k ].margin, &g.m_half[ k ], 4 );
            }
            else
            {
                sphere[ k ].type = 0;   // box 2 the stage's box shape, box 3 its own
                std::memcpy( sphere[ k ].implicit, k == 0 ? g.m_half : g.m_half2, 16 );
                std::memcpy( &sphere[ k ].margin, k == 0 ? &g.m_bmargin : &g.m_bmargin2, 4 );
            }
        }
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
        static solver_rigid_t objects[ 4 ];
        float transforms[ 4 ][ 16 ] = {};
        transforms[ 1 ][ 0 ] = transforms[ 1 ][ 5 ] = transforms[ 1 ][ 10 ] = 1.0f;
        std::memcpy( transforms[ 0 ], g.m_boat[ 0 ], 64 );
        std::memcpy( transforms[ 2 ], g.m_box[ 0 ], 64 );
        std::memcpy( transforms[ 3 ], g.m_box2[ 0 ], 64 );
        collision_object_t bodies[ 4 ];
        auto w = std::make_unique< collision_world_t >( );
        collision_world_init( *w );
        w->dispatcher.simplex = &simplex;
        static const short groups[ 4 ][ 2 ] = { { 1, -1 }, { 2, -3 }, { 1, -1 }, { 1, -1 } };
        for ( int k = 0; k < 4 && !what; ++k )
        {
            objects[ k ] = solver_rigid_t{};
            std::memcpy( &objects[ k ].friction, &g.m_friction[ k ], 4 );
            std::memcpy( &objects[ k ].restitution, &g.m_restitution[ k ], 4 );
            float p;
            std::memcpy( &p, &g.m_pth[ k ], 4 );
            bodies[ k ].body = concave_body_t{ transforms[ k ], k >= 2 ? &sphere[ k - 2 ] : nullptr, k == 1 ? &tri : nullptr,
                &objects[ k ], k == 0 ? &shape : nullptr, p };
            bodies[ k ].activation_state = g.m_states[ k ];
            bodies[ k ].collision_flags = k == 1 ? 1 : 0;
            if ( collision_world_add_object( *w, bodies[ k ], groups[ k ][ 0 ], groups[ k ][ 1 ] ) != 1 )
                what = "addCollisionObject reached unrecreated code";
        }
        g_num_manifolds = g.m_numman0;
        int last = -1;
        for ( int f = 0; f < 6 && !what; ++f )
        {
            frame_no = f;
            std::memcpy( transforms[ 0 ], g.m_boat[ f ], 64 );
            std::memcpy( transforms[ 2 ], g.m_box[ f ], 64 );
            std::memcpy( transforms[ 3 ], g.m_box2[ f ], 64 );
            if ( collision_world_perform_discrete( *w ) != 1 )
            {
                what = "a path not recreated was reached";
                break;
            }
            const int n = w->broadphase->cache.pairs.size;
            removals += ( last > n ) ? last - n : 0;
            last = n;
            for ( int k = 0; k < n; ++k )
            {
                const pair_algorithm_t* a = static_cast< const pair_algorithm_t* >( w->broadphase->cache.pairs.data[ k ].algorithm );
                sphere_pairs += ( a && a->kind == ( spheres ? 4 : 5 ) ) ? 1 : 0;
            }
            what = collide_compare_frame( *w, bodies, objects, g.m_frames[ f ], bad_word, bad_ours, bad_retail, skipped, with_contacts );
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "%s vector %d frame %d: %s (word %d: ours 0x%08X, retail 0x%08X)", label, static_cast< int >( index ), frame_no,
                what, bad_word, bad_ours, bad_retail );
        }
    }
    g_num_manifolds = 0;
    if ( failed == 0 )
        LOG_SUCCESS( "%s in the collision stage matches retail over %d worlds x 6 frames, exact (%d pair-frames, %d removals, "
                     "%d contact manifolds)",
            label, static_cast< int >( count ), sphere_pairs, removals, with_contacts );
    return failed == 0;
}

inline bool verify_sphere_stage( )
{
    return verify_far_stage( g_golden_sphere_stage, g_golden_sphere_stage_count, true, "SphereSphere" );
}

// BoxBox against retail (case_box_stage): the two boxes (one shape) beyond the
// level, face-face / edge-edge / random / deep; box 3 sent away in the odd
// vectors.
inline bool verify_box_stage( )
{
    return verify_far_stage( g_golden_box_stage, g_golden_box_stage_count, false, "BoxBox" );
}

inline bool verify_world_collide( )
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
    int failed = 0, skipped = 0, with_contacts = 0, statics = 0, gravity_set = 0, removed_bodies = 0;
    for ( std::size_t index = 0; index < g_golden_world_collide_count; ++index )
    {
        const golden_world_collide_t& g = g_golden_world_collide[ index ];
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
        static solver_rigid_t objects[ 4 ];
        collision_object_t bodies[ 4 ];
        auto cw = std::make_unique< collision_world_t >( );
        collision_world_init( *cw );
        cw->dispatcher.simplex = &simplex;
        auto w = std::make_unique< world_t >( );
        std::memcpy( w->gravity, g.m_grav, 16 );
        world_attach_collision( *w, *cw );
        static const short groups[ 4 ][ 2 ] = { { 1, -1 }, { 2, -3 }, { 1, -1 }, { 1, -1 } };
        static const float identity[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 };
        const unsigned* const initial[ 4 ] = { g.m_boat[ 0 ], nullptr, g.m_box[ 0 ], g.m_box2[ 0 ] };
        for ( int k = 0; k < 4 && !what; ++k )
        {
            solver_rigid_t& b = objects[ k ];
            b = solver_rigid_t{};
            std::memcpy( &b.friction, &g.m_friction[ k ], 4 );
            std::memcpy( &b.restitution, &g.m_restitution[ k ], 4 );
            std::memcpy( &b.inverse_mass, &g.m_inv_mass[ k ], 4 );
            b.rigidbody_flags = g.m_rb_flags[ k ];
            b.collision_flags = g.m_flags[ k ];
            b.activation_state = g.m_states[ k ];
            b.internal_type = 2;
            std::memset( b.gravity, 0x77, 32 );
            std::memcpy( b.basis, initial[ k ] ? static_cast< const void* >( initial[ k ] ) : identity, 64 );
            std::memcpy( b.interpolation_basis, b.basis, 64 );
            float p;
            std::memcpy( &p, &g.m_pth[ k ], 4 );
            bodies[ k ] = collision_object_t{};
            bodies[ k ].body = concave_body_t{ nullptr, k >= 2 ? &box : nullptr, k == 1 ? &tri : nullptr, &b, k == 0 ? &shape : nullptr, p };
            if ( world_add_rigid_body( *w, *cw, bodies[ k ], false, groups[ k ][ 0 ], groups[ k ][ 1 ] ) != 1 )
                what = "addRigidBody reached unrecreated code";
        }
        // the adds: gravity, states, the non-static list, the object array
        for ( int k = 0; k < 4 && !what; ++k )
        {
            unsigned gw[ 8 ];
            std::memcpy( gw, objects[ k ].gravity, 32 );
            gravity_set += gw[ 0 ] != 0x77777777u ? 1 : 0;
            statics += ( objects[ k ].collision_flags & 1 ) ? 1 : 0;
            for ( int x = 0; x < 8 && !what; ++x )
                if ( gw[ x ] != g.m_gravity_out[ k ][ x ] )
                {
                    what = "a body's gravity";
                    bad_word = 8 * k + x;
                    bad_ours = gw[ x ];
                    bad_retail = g.m_gravity_out[ k ][ x ];
                }
            if ( !what && objects[ k ].activation_state != g.m_states_out[ k ] )
                what = "a body's state after the add";
        }
        if ( !what && static_cast< int >( w->bodies.size( ) ) != g.m_nnonstatic )
            what = "the non-static count";
        for ( int k = 0; k < g.m_nnonstatic && !what; ++k )
            if ( w->bodies[ k ] != &objects[ g.m_nonstatic[ k ] ] )
                what = "the non-static order";
        if ( !what && static_cast< int >( w->objects.size( ) ) != g.m_nobjects )
            what = "the object count";
        for ( int k = 0; k < g.m_nobjects && !what; ++k )
            if ( w->objects[ k ] != &objects[ g.m_objects[ k ] ] )
                what = "the object order";
        g_num_manifolds = g.m_numman0;
        const unsigned( *tracks[ 4 ] )[ 16 ] = { g.m_boat, nullptr, g.m_box, g.m_box2 };
        for ( int f = 0; f < 6 && !what; ++f )
        {
            frame_no = f;
            if ( f == 3 && g.m_removed >= 0 )
            {
                ++removed_bodies;
                if ( world_remove_collision_object( *w, *cw, bodies[ g.m_removed ] ) != 1 )
                {
                    what = "removeCollisionObject reached unrecreated code";
                    break;
                }
            }
            for ( int k = 0; k < 4; ++k )
                if ( tracks[ k ] )
                {
                    std::memcpy( objects[ k ].interpolation_basis, tracks[ k ][ f ? f - 1 : 0 ], 64 );
                    std::memcpy( objects[ k ].basis, tracks[ k ][ f ], 64 );
                }
            w->stages.collide( *w );
            if ( w->refused )
            {
                what = "a path not recreated was reached";
                break;
            }
            const golden_world_collide_frame_t& gf = g.m_frames[ f ];
            what = collide_compare_frame( *cw, bodies, objects, gf.m_stage, bad_word, bad_ours, bad_retail, skipped, with_contacts );
            for ( int k = 0; k < 4 && !what; ++k )
                if ( objects[ k ].activation_state != gf.m_states[ k ] )
                    what = "a body's state after the frame";
            // the world's arrays (the removal's swap-with-last order included)
            if ( !what && static_cast< int >( w->bodies.size( ) ) != gf.m_nnonstatic )
                what = "the non-static count after the frame";
            for ( int k = 0; k < gf.m_nnonstatic && !what; ++k )
                if ( w->bodies[ k ] != &objects[ gf.m_nonstatic[ k ] ] )
                    what = "the non-static order after the frame";
            if ( !what && ( static_cast< int >( w->objects.size( ) ) != gf.m_nobjects ||
                              static_cast< int >( cw->objects.size( ) ) != gf.m_nobjects ) )
                what = "the object count after the frame";
            for ( int k = 0; k < gf.m_nobjects && !what; ++k )
                if ( w->objects[ k ] != &objects[ gf.m_objects[ k ] ] || cw->objects[ k ] != &bodies[ gf.m_objects[ k ] ] )
                    what = "the object order after the frame";
            // the stage's hand-off: the manifolds and the pairs in cache order
            if ( !what && w->manifolds != cw->dispatcher.manifolds )
                what = "the world's manifolds";
            if ( !what && static_cast< int >( w->pairs.size( ) ) != gf.m_stage.m_npairs )
                what = "the world's pairs";
            for ( int k = 0; k < gf.m_stage.m_npairs && !what; ++k )
                if ( w->pairs[ k ].object0 != &objects[ gf.m_stage.m_pairs[ k ][ 0 ] ] ||
                     w->pairs[ k ].object1 != &objects[ gf.m_stage.m_pairs[ k ][ 1 ] ] )
                    what = "a world pair";
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "world collide vector %d frame %d: %s (word %d: ours 0x%08X, retail 0x%08X)", static_cast< int >( index ), frame_no,
                what, bad_word, bad_ours, bad_retail );
        }
        w.reset( );
    }
    g_num_manifolds = 0;
    if ( failed == 0 )
        LOG_SUCCESS( "addRigidBody + removeCollisionObject + the world's collision stage match retail over %d worlds x 6 frames, "
                     "exact (%d bodies with gravity set, %d static, %d removed, %d pair-frames skipped, %d contact manifolds)",
            static_cast< int >( g_golden_world_collide_count ), gravity_set, statics, removed_bodies, skipped, with_contacts );
    return failed == 0;
}
