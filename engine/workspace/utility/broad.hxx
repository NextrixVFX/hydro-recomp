#pragma once

#include <workspace/utility/meshes.hxx>

// The broadphase and collision-stage checks against retail: the stage end to end, updateAabbs, the pair cache, the btDbvt and the broadphase.
// (Split out of verify.hxx; verify.hxx's verify_all runs them.)

// performDiscreteCollisionDetection end to end against retail: a world with
// the btDbvtBroadphase (the ghost callback installed), objects added through
// addCollisionObject (0 the compound boat 1 / -1, 1 the static level 2 / -3,
// 2 / 3 boxes 1 / -1), six frames - box 3 teleported away at frame 3; per
// frame gNumManifold, the dispatcher's manifolds in array order (masked as
// verify_compound) and the pair cache in ORDER with each pair's algorithm.
// Counted: pair removals, pair-frames skipped by needsCollision, contact
// manifolds.
inline bool verify_collision_stage( )
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
    int failed = 0, removals = 0, skipped = 0, with_contacts = 0;
    for ( std::size_t index = 0; index < g_golden_collision_stage_count; ++index )
    {
        const golden_collision_stage_t& g = g_golden_collision_stage[ index ];
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
            bodies[ k ].body = concave_body_t{ transforms[ k ], k >= 2 ? &box : nullptr, k == 1 ? &tri : nullptr, &objects[ k ],
                k == 0 ? &shape : nullptr, p };
            bodies[ k ].activation_state = g.m_states[ k ];
            bodies[ k ].collision_flags = k == 1 ? 1 : 0;
            if ( collision_world_add_object( *w, bodies[ k ], groups[ k ][ 0 ], groups[ k ][ 1 ] ) != 1 )
                what = "addCollisionObject reached unrecreated code";
        }
        g_num_manifolds = g.m_numman0;
        int last_pairs = -1;
        for ( int f = 0; f < 6 && !what; ++f )
        {
            const golden_collision_stage_frame_t& gf = g.m_frames[ f ];
            frame_no = f;
            std::memcpy( transforms[ 0 ], g.m_boat[ f ], 64 );
            std::memcpy( transforms[ 2 ], g.m_box[ f ], 64 );
            std::memcpy( transforms[ 3 ], g.m_box2[ f ], 64 );
            if ( collision_world_perform_discrete( *w ) != 1 )
            {
                what = "a path not recreated was reached";
                break;
            }
            const pair_cache_t& c = w->broadphase->cache;
            removals += ( last_pairs > c.pairs.size ) ? last_pairs - c.pairs.size : 0;
            last_pairs = c.pairs.size;
            if ( c.pairs.size != gf.m_npairs )
            {
                what = "the pair count";
                bad_ours = static_cast< unsigned >( c.pairs.size );
                bad_retail = static_cast< unsigned >( gf.m_npairs );
                break;
            }
            for ( int k = 0; k < gf.m_npairs && !what; ++k )
            {
                const broadphase_pair_t& p = c.pairs.data[ k ];
                const pair_algorithm_t* a = static_cast< const pair_algorithm_t* >( p.algorithm );
                const int kind = a ? a->kind : 0;
                const int child = ( kind == 3 && !a->compound.children.empty( ) && a->compound.children[ 0 ] ) ? a->compound.children[ 0 ]->kind : 0;
                const int sw = kind == 3 ? a->compound.swapped : kind == 2 ? a->leaf.concave.swapped : 0;
                const int i0 = static_cast< int >( static_cast< collision_object_t* >( p.proxy0->client ) - bodies );
                const int i1 = static_cast< int >( static_cast< collision_object_t* >( p.proxy1->client ) - bodies );
                skipped += kind == 0 ? 1 : 0;
                const int ours[ 5 ] = { i0, i1, kind, child, sw };
                for ( int x = 0; x < 5 && !what; ++x )
                    if ( ours[ x ] != gf.m_pairs[ k ][ x ] )
                    {
                        what = "a pair";
                        bad_word = 5 * k + x;
                        bad_ours = static_cast< unsigned >( ours[ x ] );
                        bad_retail = static_cast< unsigned >( gf.m_pairs[ k ][ x ] );
                    }
            }
            if ( what )
                break;
            const collision_dispatcher_t& d = w->dispatcher;
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
            LOG_ERROR( "collision stage vector %d frame %d: %s (word %d: ours 0x%08X, retail 0x%08X)", static_cast< int >( index ), frame_no,
                what, bad_word, bad_ours, bad_retail );
        }
    }
    g_num_manifolds = 0;
    if ( failed == 0 )
        LOG_SUCCESS( "performDiscreteCollisionDetection matches retail end to end over %d worlds x 6 frames, exact (%d pair "
                     "removals, %d pair-frames skipped, %d contact manifolds)",
            static_cast< int >( g_golden_collision_stage_count ), removals, skipped, with_contacts );
    return failed == 0;
}

// updateAabbs + updateSingleAabb against retail: six objects (box, cylinder,
// sphere, the boat hull, the level, the compound boat - its getAabb
// 0x527280) per vector; every setAabb (proxy, min, max - all four lanes),
// the states after, the report byte and the drawer's string count.
struct verify_aabb_calls_t
{
    std::vector< std::array< unsigned, 9 > > calls;
    int reports = 0;
};

inline bool verify_update_aabb( )
{
    const golden_bvh_asset_t& la = g_golden_bvh_assets[ 1 ];
    const golden_bvh_asset_t& ha = g_golden_bvh_assets[ 0 ];
    striding_mesh_t hull_mesh{};
    hull_mesh.scaling[ 0 ] = hull_mesh.scaling[ 1 ] = hull_mesh.scaling[ 2 ] = 1.0f;
    hull_mesh.vertices = reinterpret_cast< const float( * )[ 4 ] >( ha.m_verts );
    hull_mesh.num_vertices = ha.m_nverts;
    hull_mesh.indices = ha.m_indices;
    hull_mesh.index_count = ha.m_nindices;
    int failed = 0, sets = 0, overflows = 0, skipped = 0, merged = 0;
    for ( std::size_t index = 0; index < g_golden_update_aabb_count; ++index )
    {
        const golden_update_aabb_t& g = g_golden_update_aabb[ index ];
        const char* what = nullptr;
        int bad_word = -1;
        unsigned bad_ours = 0, bad_retail = 0;
        convex_shape_t prims[ 3 ]{};
        const int kinds[ 3 ] = { 0, 13, 8 };
        for ( int k = 0; k < 3; ++k )
        {
            prims[ k ].type = kinds[ k ];
            prims[ k ].local_scaling[ 0 ] = prims[ k ].local_scaling[ 1 ] = prims[ k ].local_scaling[ 2 ] = 1.0f;
            std::memcpy( prims[ k ].implicit, g.m_half[ k ], 16 );
            std::memcpy( &prims[ k ].margin, &g.m_margin[ k ], 4 );
            prims[ k ].up_axis = g.m_axis[ k ];
        }
        convex_shape_t hull{};
        hull.type = 3;
        hull.local_scaling[ 0 ] = hull.local_scaling[ 1 ] = hull.local_scaling[ 2 ] = 1.0f;
        std::memcpy( &hull.margin, &g.m_hmargin, 4 );
        std::memcpy( hull.local_aabb_min, g.m_hlo, 16 );
        std::memcpy( hull.local_aabb_max, g.m_hhi, 16 );
        hull.mesh = &hull_mesh;
        bvh_mesh_shape_t tri{ 21, 0.0f, nullptr, nullptr };
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
        float transforms[ 6 ][ 16 ], interps[ 6 ][ 16 ];
        collision_object_t objects[ 6 ];
        std::vector< collision_object_t* > list;
        for ( int k = 0; k < 6; ++k )
        {
            const golden_update_aabb_object_t& go = g.m_objects[ k ];
            std::memcpy( transforms[ k ], go.m_t, 64 );
            std::memcpy( interps[ k ], go.m_it, 64 );
            objects[ k ].body = concave_body_t{ transforms[ k ], k < 3 ? &prims[ k ] : k == 3 ? &hull : nullptr, k == 4 ? &tri : nullptr,
                nullptr, k == 5 ? &shape : nullptr, 0.0f };
            objects[ k ].interpolation_transform = interps[ k ];
            objects[ k ].collision_flags = go.m_flags;
            objects[ k ].activation_state = go.m_state;
            objects[ k ].internal_type = go.m_type;
            objects[ k ].proxy = go.m_proxy;
            list.push_back( &objects[ k ] );
            skipped += ( !g.m_force && ( go.m_state == 2 || go.m_state == 5 ) ) ? 1 : 0;
            merged += ( g.m_use_cont && go.m_type == 2 ) ? 1 : 0;
        }
        verify_aabb_calls_t rec;
        aabb_world_t w;
        w.use_continuous = g.m_use_cont != 0;
        w.force_update_all = g.m_force != 0;
        w.user = &rec;
        w.set_aabb = []( void* user, unsigned proxy, const float* mn, const float* mx ) -> int {
            std::array< unsigned, 9 > c{};
            c[ 0 ] = proxy;
            std::memcpy( &c[ 1 ], mn, 16 );
            std::memcpy( &c[ 5 ], mx, 16 );
            static_cast< verify_aabb_calls_t* >( user )->calls.push_back( c );
            return 1;
        };
        if ( g.m_drawer )
            w.report = []( void* user, const char* ) { ++static_cast< verify_aabb_calls_t* >( user )->reports; };
        g_aabb_report_pending = g.m_report_in != 0;
        if ( !what && update_aabbs( w, list ) != 1 )
            what = "a path not recreated was reached";
        if ( !what )
        {
            if ( static_cast< int >( rec.calls.size( ) ) != g.m_ncalls )
                what = "the setAabb count";
            for ( int c = 0; c < g.m_ncalls && !what; ++c )
            {
                if ( g.m_calls[ c ][ 9 ] != 1u )
                    what = "retail's dispatcher argument";
                for ( int x = 0; x < 9 && !what; ++x )
                    if ( rec.calls[ c ][ x ] != g.m_calls[ c ][ x ] )
                    {
                        what = "a setAabb call";
                        bad_word = 10 * c + x;
                        bad_ours = rec.calls[ c ][ x ];
                        bad_retail = g.m_calls[ c ][ x ];
                    }
            }
            for ( int k = 0; k < 6 && !what; ++k )
                if ( objects[ k ].activation_state != g.m_states[ k ] )
                    what = "an activation state";
            if ( !what && ( g_aabb_report_pending ? 1 : 0 ) != g.m_report_out )
                what = "the report byte";
            if ( !what && rec.reports != g.m_reports )
                what = "the drawer's strings";
        }
        sets += static_cast< int >( rec.calls.size( ) );
        for ( int k = 0; k < 6; ++k )
            overflows += objects[ k ].activation_state != g.m_objects[ k ].m_state ? 1 : 0;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "update_aabb vector %d: %s (word %d: ours 0x%08X, retail 0x%08X)", static_cast< int >( index ), what, bad_word,
                bad_ours, bad_retail );
        }
    }
    g_aabb_report_pending = true;
    if ( failed == 0 )
        LOG_SUCCESS( "updateAabbs + updateSingleAabb + the compound getAabb match retail over %d vectors x 6 objects, exact (%d "
                     "setAabb, %d overflow state writes, %d skipped, %d continuous merges)",
            static_cast< int >( g_golden_update_aabb_count ), sets, overflows, skipped, merged );
    return failed == 0;
}

// btHashedOverlappingPairCache against retail: the ctor then 160 seeded
// operations per vector on 40 proxies; per step the return and an FNV-1a 64
// digest of EVERY state word (pair array size / capacity / pairs as proxy
// indices + algorithm token + internal, the hash and next tables' size /
// capacity / words, the filter and ghost words) plus the algorithm clean
// records; the final state word for word.
struct verify_pair_cache_rec_t
{
    std::vector< unsigned > records;
    const broadphase_proxy_t* base = nullptr;
    bool ghost_mismatch = false;
    int tokens[ 32 ] = {};
};

inline void verify_pair_cache_words( const pair_cache_t& c, const broadphase_proxy_t* proxies, const verify_pair_cache_rec_t& rec,
    const std::vector< unsigned >& clean, std::vector< unsigned >& w )
{
    w.clear( );
    w.push_back( static_cast< unsigned >( c.pairs.size ) );
    w.push_back( static_cast< unsigned >( c.pairs.capacity ) );
    for ( int k = 0; k < c.pairs.size; ++k )
    {
        const broadphase_pair_t& p = c.pairs.data[ k ];
        w.push_back( static_cast< unsigned >( p.proxy0 - proxies ) );
        w.push_back( static_cast< unsigned >( p.proxy1 - proxies ) );
        w.push_back( p.algorithm ? static_cast< unsigned >( static_cast< const int* >( p.algorithm ) - rec.tokens ) : 0xFFFFFFFFu );
        w.push_back( p.internal );
    }
    w.push_back( static_cast< unsigned >( c.hash_table.size ) );
    w.push_back( static_cast< unsigned >( c.hash_table.capacity ) );
    for ( int k = 0; k < c.hash_table.size; ++k )
        w.push_back( static_cast< unsigned >( c.hash_table.data[ k ] ) );
    w.push_back( static_cast< unsigned >( c.next.size ) );
    w.push_back( static_cast< unsigned >( c.next.capacity ) );
    for ( int k = 0; k < c.next.size; ++k )
        w.push_back( static_cast< unsigned >( c.next.data[ k ] ) );
    w.push_back( 0u );
    w.push_back( c.has_ghost ? 1u : 0u );
    w.push_back( static_cast< unsigned >( clean.size( ) / 3 ) );
    w.insert( w.end( ), clean.begin( ), clean.end( ) );
}

inline unsigned long long verify_fnv64( const std::vector< unsigned >& w )
{
    unsigned long long h = 0xCBF29CE484222325ull;
    for ( unsigned x : w )
        for ( int k = 0; k < 4; ++k )
        {
            h ^= ( x >> ( 8 * k ) ) & 0xFFu;
            h *= 0x100000001B3ull;
        }
    return h;
}

inline bool verify_pair_cache( )
{
    int failed = 0, steps = 0, removed = 0, grown = 0, cleaned = 0;
    for ( std::size_t index = 0; index < g_golden_pair_cache_count; ++index )
    {
        const golden_pair_cache_t& g = g_golden_pair_cache[ index ];
        broadphase_proxy_t proxies[ 40 ];
        for ( int k = 0; k < 40; ++k )
        {
            proxies[ k ].uid = g.m_uid[ k ];
            proxies[ k ].group = static_cast< short >( g.m_group[ k ] );
            proxies[ k ].mask = static_cast< short >( g.m_mask[ k ] );
            proxies[ k ].client_type = g.m_ctype[ k ];
            proxies[ k ].client = &proxies[ k ];
        }
        verify_pair_cache_rec_t rec;
        rec.base = proxies;
        pair_cache_t c;
        pair_cache_init( c );
        c.hooks.user = &rec;
        c.hooks.clean_algorithm = []( void* user, void* algorithm ) {
            verify_pair_cache_rec_t* r = static_cast< verify_pair_cache_rec_t* >( user );
            const unsigned token = static_cast< unsigned >( static_cast< int* >( algorithm ) - r->tokens );
            r->records.insert( r->records.end( ), { 1u, token, 0u, 2u, token, 0u } );
        };
        c.has_ghost = g.m_ghost != 0;
        c.hooks.ghost_add = []( void* user, void* ghost, broadphase_proxy_t* other, broadphase_proxy_t* self ) {
            verify_pair_cache_rec_t* r = static_cast< verify_pair_cache_rec_t* >( user );
            const broadphase_proxy_t* base = r->base;
            if ( ghost != self->client )
                r->ghost_mismatch = true;
            r->records.insert( r->records.end( ), { 3u, static_cast< unsigned >( self - base ), static_cast< unsigned >( other - base ) } );
        };
        c.hooks.ghost_remove = []( void* user, void* ghost, broadphase_proxy_t* other, broadphase_proxy_t* self ) {
            verify_pair_cache_rec_t* r = static_cast< verify_pair_cache_rec_t* >( user );
            const broadphase_proxy_t* base = r->base;
            if ( ghost != self->client )
                r->ghost_mismatch = true;
            r->records.insert( r->records.end( ), { 4u, static_cast< unsigned >( self - base ), static_cast< unsigned >( other - base ) } );
        };
        const char* what = nullptr;
        int bad_step = -1;
        std::vector< unsigned > w;
        for ( int s = 0; s < 161 && !what; ++s )
        {
            const golden_pair_cache_step_t& gs = g.m_steps[ s ];
            rec.records.clear( );
            const int size_before = c.pairs.size, cap_before = c.pairs.capacity;
            int ret = 0, r = 1;
            broadphase_proxy_t* a = &proxies[ gs.m_a ];
            broadphase_proxy_t* b = &proxies[ gs.m_b ];
            switch ( gs.m_op )
            {
                case 0:
                    break;
                case 1:
                    ret = pair_cache_add( c, a, b );
                    if ( ret == -2 )
                        ret = -1;
                    else if ( ret < 0 )
                        r = -1;
                    break;
                case 2:
                {
                    unsigned internal = 0;
                    r = pair_cache_remove( c, a, b, internal );
                    ret = r == 1 ? static_cast< int >( internal ) : 0;
                    break;
                }
                case 3:
                    ret = pair_cache_find( c, a, b );
                    break;
                case 4:
                    r = pair_cache_remove_containing( c, a );
                    break;
                case 5:
                    r = pair_cache_clean_proxy( c, a );
                    break;
                case 6:
                    if ( gs.m_v < 0 )
                        c.pairs.data[ gs.m_a ].algorithm = &rec.tokens[ -1 - gs.m_v ];
                    else
                        c.pairs.data[ gs.m_a ].internal = static_cast< unsigned >( gs.m_v );
                    break;
            }
            if ( r < 0 )
            {
                what = "a hook not recreated was reached";
                bad_step = s;
                break;
            }
            ++steps;
            removed += c.pairs.size < size_before ? 1 : 0;
            grown += c.pairs.capacity > cap_before ? 1 : 0;
            for ( std::size_t k = 0; k < rec.records.size( ); k += 3 )
                cleaned += rec.records[ k ] == 1u ? 1 : 0;
            if ( rec.ghost_mismatch )
                what = "a ghost call's this";
            verify_pair_cache_words( c, proxies, rec, rec.records, w );
            if ( ret != gs.m_ret )
                what = "an operation's return";
            else if ( verify_fnv64( w ) != gs.m_digest )
                what = "the state digest";
            if ( what )
                bad_step = s;
        }
        if ( !what )
        {
            verify_pair_cache_words( c, proxies, rec, {}, w );
            if ( static_cast< int >( w.size( ) ) != g.m_final_count || std::memcmp( w.data( ), g.m_final, 4 * w.size( ) ) != 0 )
                what = "the final state";
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "pair cache vector %d step %d: %s", static_cast< int >( index ), bad_step, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btHashedOverlappingPairCache matches retail over %d vectors x 160 operations, every table word (%d steps, %d "
                     "removals, %d growths, %d algorithm cleans)",
            static_cast< int >( g_golden_pair_cache_count ), steps, removed, grown, cleaned );
    return failed == 0;
}

// btDbvt against retail: per vector 200 seeded insert / update /
// updateVelocity / removeleaf steps (m_lkhd -1 / 0 / 1 / 3); per step the
// result (and updateVelocity's edited volume) and an FNV-1a 64 digest of the
// tree (root, m_free, m_leaves, m_lkhd, every reachable node pre-order:
// id, parent, leaf flag, data or both childs, the 8 volume words); the final
// tree word for word.
inline void verify_dbvt_words( const dbvt_t& t, std::vector< unsigned >& w )
{
    auto id = []( const dbvt_node_t* n ) { return n ? static_cast< unsigned >( n->id ) : 0xFFFFFFFFu; };
    w.clear( );
    w.push_back( id( t.root ) );
    w.push_back( id( t.free ) );
    w.push_back( static_cast< unsigned >( t.leaves ) );
    w.push_back( static_cast< unsigned >( t.lkhd ) );
    std::vector< const dbvt_node_t* > stack;
    if ( t.root )
        stack.push_back( t.root );
    while ( !stack.empty( ) )
    {
        const dbvt_node_t* n = stack.back( );
        stack.pop_back( );
        const bool leaf = dbvt_is_leaf( n );
        w.push_back( id( n ) );
        w.push_back( id( n->parent ) );
        w.push_back( leaf ? 1u : 0u );
        if ( leaf )
            w.push_back( static_cast< unsigned >( reinterpret_cast< std::uintptr_t >( n->data ) ) );
        else
        {
            w.push_back( id( n->childs[ 0 ] ) );
            w.push_back( id( n->childs[ 1 ] ) );
        }
        unsigned v[ 8 ];
        std::memcpy( v, n->mn, 16 );
        std::memcpy( v + 4, n->mx, 16 );
        w.insert( w.end( ), v, v + 8 );
        if ( !leaf )
        {
            stack.push_back( n->childs[ 1 ] );
            stack.push_back( n->childs[ 0 ] );
        }
    }
}

inline bool verify_dbvt( )
{
    int failed = 0, steps = 0, moved = 0, contained = 0, removed = 0;
    for ( std::size_t index = 0; index < g_golden_dbvt_count; ++index )
    {
        const golden_dbvt_t& g = g_golden_dbvt[ index ];
        dbvt_t t;
        t.lkhd = g.m_lkhd;
        const char* what = nullptr;
        int bad_step = -1;
        std::vector< unsigned > w;
        for ( int s = 0; s < 200 && !what; ++s )
        {
            const golden_dbvt_step_t& gs = g.m_steps[ s ];
            float vol[ 8 ];
            std::memcpy( vol, gs.m_vol, 32 );
            int ret = 0;
            dbvt_node_t* leaf = gs.m_a >= 0 ? t.pool[ gs.m_a ].get( ) : nullptr;
            switch ( gs.m_op )
            {
                case 1:
                {
                    dbvt_node_t* n = dbvt_insert( t, vol, vol + 4, reinterpret_cast< void* >( static_cast< std::uintptr_t >( gs.m_extra[ 0 ] ) ) );
                    ret = n->id;
                    break;
                }
                case 2:
                    dbvt_update( t, leaf, vol, vol + 4 );
                    break;
                case 3:
                {
                    float velocity[ 4 ];
                    std::memcpy( velocity, gs.m_extra, 16 );
                    ret = dbvt_update_velocity( t, leaf, vol, vol + 4, velocity ) ? 1 : 0;
                    ( ret ? moved : contained )++;
                    if ( std::memcmp( vol, gs.m_extra + 5, 32 ) != 0 )
                        what = "updateVelocity's edited volume";
                    break;
                }
                case 4:
                {
                    const dbvt_node_t* r = dbvt_remove_leaf( t, leaf );
                    ret = r ? r->id : -1;
                    ++removed;
                    break;
                }
            }
            ++steps;
            verify_dbvt_words( t, w );
            if ( !what && ret != gs.m_ret )
                what = "an operation's result";
            else if ( !what && verify_fnv64( w ) != gs.m_digest )
                what = "the tree digest";
            if ( what )
                bad_step = s;
        }
        if ( !what )
        {
            verify_dbvt_words( t, w );
            if ( static_cast< int >( w.size( ) ) != g.m_final_count || std::memcmp( w.data( ), g.m_final, 4 * w.size( ) ) != 0 )
                what = "the final tree";
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "dbvt vector %d step %d (op %d): %s", static_cast< int >( index ), bad_step,
                bad_step >= 0 ? g.m_steps[ bad_step ].m_op : -1, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btDbvt insert / update / updateVelocity / removeleaf match retail over %d vectors x 200 steps, every node "
                     "word (%d steps, %d moved, %d contained, %d removeleaf)",
            static_cast< int >( g_golden_dbvt_count ), steps, moved, contained, removed );
    return failed == 0;
}

// btDbvtBroadphase against retail: built by its ctor with its own pair cache
// and the real btGhostPairCallback; 120 seeded create / setAabb / destroy /
// calculateOverlappingPairs steps per vector; per step an FNV-1a 64 digest
// of the whole state (the scalars, the stage lists, every live proxy, both
// trees pre-order, the pair cache in ORDER with its tables) and the ghost
// records; the final state word for word.
struct verify_broadphase_rec_t
{
    std::vector< unsigned > records;
};

inline void verify_broadphase_tree( const dbvt_t& t, std::vector< unsigned >& w )
{
    auto id = []( const dbvt_node_t* n ) { return n ? static_cast< unsigned >( n->id ) : 0xFFFFFFFFu; };
    w.push_back( id( t.root ) );
    w.push_back( id( t.free ) );
    w.push_back( static_cast< unsigned >( t.lkhd ) );
    w.push_back( static_cast< unsigned >( t.leaves ) );
    w.push_back( t.opath );
    std::vector< const dbvt_node_t* > stack;
    if ( t.root )
        stack.push_back( t.root );
    while ( !stack.empty( ) )
    {
        const dbvt_node_t* n = stack.back( );
        stack.pop_back( );
        const bool leaf = dbvt_is_leaf( n );
        w.push_back( id( n ) );
        w.push_back( id( n->parent ) );
        w.push_back( leaf ? 1u : 0u );
        if ( leaf )
            w.push_back( static_cast< unsigned >( static_cast< const dbvt_proxy_t* >( static_cast< broadphase_proxy_t* >( n->data ) )->index ) );
        else
        {
            w.push_back( id( n->childs[ 0 ] ) );
            w.push_back( id( n->childs[ 1 ] ) );
            stack.push_back( n->childs[ 1 ] );
            stack.push_back( n->childs[ 0 ] );
        }
        unsigned v[ 8 ];
        std::memcpy( v, n->mn, 16 );
        std::memcpy( v + 4, n->mx, 16 );
        w.insert( w.end( ), v, v + 8 );
    }
}

inline void verify_broadphase_words( const dbvt_broadphase_t& bp, const std::vector< dbvt_proxy_t* >& alive, std::vector< unsigned >& w )
{
    auto pidx = []( const broadphase_proxy_t* p ) { return p ? static_cast< unsigned >( static_cast< const dbvt_proxy_t* >( p )->index ) : 0xFFFFFFFFu; };
    auto bits = []( float f ) { unsigned u; std::memcpy( &u, &f, 4 ); return u; };
    w.clear( );
    for ( int k = 0; k < 3; ++k )
        w.push_back( pidx( bp.stage_roots[ k ] ) );
    const unsigned scalars[ 13 ] = { bits( bp.prediction ), static_cast< unsigned >( bp.stage_current ), static_cast< unsigned >( bp.fupdates ),
        static_cast< unsigned >( bp.dupdates ), static_cast< unsigned >( bp.cupdates ), static_cast< unsigned >( bp.newpairs ),
        static_cast< unsigned >( bp.fixedleft ), bp.updates_call, bp.updates_done, bits( bp.updates_ratio ), static_cast< unsigned >( bp.pid ),
        static_cast< unsigned >( bp.cid ), static_cast< unsigned >( bp.gid ) };
    w.insert( w.end( ), scalars, scalars + 13 );
    w.push_back( 1u | ( bp.deferedcollide ? 0x100u : 0u ) | ( bp.needcleanup ? 0x10000u : 0u ) );
    for ( const dbvt_proxy_t* p : alive )
    {
        w.push_back( static_cast< unsigned >( p->index ) );
        w.push_back( static_cast< unsigned >( p->uid ) );
        w.push_back( static_cast< unsigned >( p->stage ) );
        w.push_back( pidx( p->links[ 0 ] ) );
        w.push_back( pidx( p->links[ 1 ] ) );
        w.push_back( p->leaf ? static_cast< unsigned >( p->leaf->id ) : 0xFFFFFFFFu );
        unsigned a[ 8 ];
        std::memcpy( a, p->aabb_min, 16 );
        std::memcpy( a + 4, p->aabb_max, 16 );
        w.insert( w.end( ), a, a + 8 );
    }
    verify_broadphase_tree( bp.sets[ 0 ], w );
    verify_broadphase_tree( bp.sets[ 1 ], w );
    const pair_cache_t& c = bp.cache;
    w.push_back( static_cast< unsigned >( c.pairs.size ) );
    w.push_back( static_cast< unsigned >( c.pairs.capacity ) );
    for ( int k = 0; k < c.pairs.size; ++k )
    {
        const broadphase_pair_t& p = c.pairs.data[ k ];
        w.push_back( pidx( p.proxy0 ) );
        w.push_back( pidx( p.proxy1 ) );
        w.push_back( p.algorithm ? 1u : 0u );
        w.push_back( p.internal );
    }
    w.push_back( static_cast< unsigned >( c.hash_table.size ) );
    w.push_back( static_cast< unsigned >( c.hash_table.capacity ) );
    for ( int k = 0; k < c.hash_table.size; ++k )
        w.push_back( static_cast< unsigned >( c.hash_table.data[ k ] ) );
    w.push_back( static_cast< unsigned >( c.next.size ) );
    w.push_back( static_cast< unsigned >( c.next.capacity ) );
    for ( int k = 0; k < c.next.size; ++k )
        w.push_back( static_cast< unsigned >( c.next.data[ k ] ) );
}

inline bool verify_broadphase( )
{
    int failed = 0, steps = 0, pairs_max = 0, fixed_moves = 0, cleanups = 0;
    for ( std::size_t index = 0; index < g_golden_broadphase_count; ++index )
    {
        const golden_broadphase_t& g = g_golden_broadphase[ index ];
        auto bp = std::make_unique< dbvt_broadphase_t >( );
        broadphase_init( *bp );
        std::memcpy( &bp->prediction, &g.m_prediction, 4 );
        bp->dupdates = g.m_dupdates;
        bp->cupdates = g.m_cupdates;
        verify_broadphase_rec_t rec;
        bp->cache.has_ghost = true;
        bp->cache.hooks.user = &rec;
        bp->cache.hooks.ghost_add = []( void* user, void*, broadphase_proxy_t* other, broadphase_proxy_t* self ) {
            static_cast< verify_broadphase_rec_t* >( user )->records.insert( static_cast< verify_broadphase_rec_t* >( user )->records.end( ),
                { 3u, static_cast< unsigned >( static_cast< dbvt_proxy_t* >( self )->index ), static_cast< unsigned >( static_cast< dbvt_proxy_t* >( other )->index ) } );
        };
        bp->cache.hooks.ghost_remove = []( void* user, void*, broadphase_proxy_t* other, broadphase_proxy_t* self ) {
            static_cast< verify_broadphase_rec_t* >( user )->records.insert( static_cast< verify_broadphase_rec_t* >( user )->records.end( ),
                { 4u, static_cast< unsigned >( static_cast< dbvt_proxy_t* >( self )->index ), static_cast< unsigned >( static_cast< dbvt_proxy_t* >( other )->index ) } );
        };
        std::vector< dbvt_proxy_t* > alive;
        const char* what = nullptr;
        int bad_step = -1;
        std::vector< unsigned > w;
        for ( int s = 0; s < 120 && !what; ++s )
        {
            const golden_broadphase_step_t& gs = g.m_steps[ s ];
            rec.records.clear( );
            float mn[ 4 ], mx[ 4 ];
            std::memcpy( mn, gs.m_args + 2, 16 );
            std::memcpy( mx, gs.m_args + 6, 16 );
            int r = 1;
            const int fixed_before = bp->sets[ 1 ].leaves;
            switch ( gs.m_op )
            {
                case 1:
                {
                    const int k = static_cast< int >( gs.m_args[ 0 ] );
                    dbvt_proxy_t* p = broadphase_create_proxy( *bp, mn, mx, nullptr, g.m_ctype[ k ], static_cast< short >( g.m_group[ k ] ),
                        static_cast< short >( g.m_mask[ k ] ), r );
                    p->client = p;
                    if ( static_cast< unsigned >( p->index ) != gs.m_args[ 1 ] )
                        what = "the proxy order";
                    alive.push_back( p );
                    break;
                }
                case 2:
                {
                    dbvt_proxy_t* p = bp->proxies[ gs.m_args[ 1 ] ].get( );
                    r = broadphase_destroy_proxy( *bp, p );
                    alive.erase( std::find( alive.begin( ), alive.end( ), p ) );
                    break;
                }
                case 3:
                    r = broadphase_set_aabb( *bp, bp->proxies[ gs.m_args[ 1 ] ].get( ), mn, mx );
                    break;
                case 7:
                    r = broadphase_calculate_overlapping_pairs( *bp );
                    ++cleanups;
                    break;
            }
            if ( !what && r < 0 )
                what = "a path not recreated was reached";
            ++steps;
            fixed_moves += bp->sets[ 1 ].leaves > fixed_before ? 1 : 0;
            pairs_max = bp->cache.pairs.size > pairs_max ? bp->cache.pairs.size : pairs_max;
            verify_broadphase_words( *bp, alive, w );
            const std::vector< unsigned > state = w;
            w.push_back( static_cast< unsigned >( rec.records.size( ) / 3 ) );
            w.insert( w.end( ), rec.records.begin( ), rec.records.end( ) );
            if ( !what && verify_fnv64( w ) != gs.m_digest )
                what = "the state digest";
            if ( what )
                bad_step = s;
            if ( s == 119 && !what &&
                 ( static_cast< int >( state.size( ) ) != g.m_final_count || std::memcmp( state.data( ), g.m_final, 4 * state.size( ) ) != 0 ) )
                what = "the final state";
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "broadphase vector %d step %d (op %d): %s", static_cast< int >( index ), bad_step,
                bad_step >= 0 ? g.m_steps[ bad_step ].m_op : -1, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btDbvtBroadphase create / setAabb / destroy / calculateOverlappingPairs match retail over %d vectors x 120 steps, "
                     "every word (%d steps, up to %d pairs, %d steps moved proxies to the fixed set, %d collides)",
            static_cast< int >( g_golden_broadphase_count ), steps, pairs_max, fixed_moves, cleanups );
    return failed == 0;
}
