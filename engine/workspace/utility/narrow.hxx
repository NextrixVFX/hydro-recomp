#pragma once

#include <workspace/utility/solver.hxx>

// The narrowphase checks against retail: the manifold, the simplex, the support functions, GJK, EPA and ConvexConvex.
// (Split out of verify.hxx; verify.hxx's verify_all runs them.)

// The 2.79 manifold against retail on built manifolds. For addContactPoint
// the new point's words retail never writes (its stack point is not
// cleared: +148..+191 and the row words other than the accumulated
// impulse) are UNKNOWN and masked.
struct verify_manifold_log_t
{
    unsigned events[ 16 ][ 8 ];
    int count;
    const solver_manifold_t* manifold;
    const solver_rigid_t* objects;
};

inline void verify_manifold_event( verify_manifold_log_t& l, const unsigned ( &e )[ 8 ] )
{
    if ( l.count < 16 )
        std::memcpy( l.events[ l.count ], e, sizeof( e ) );
    ++l.count;
}

inline bool verify_manifold( )
{
    int failed = 0;
    int per[ 3 ] = {};
    for ( std::size_t index = 0; index < g_golden_manifold_count; ++index )
    {
        const golden_manifold_t& g = g_golden_manifold[ index ];
        static solver_rigid_t objects[ 2 ];
        for ( int k = 0; k < 2; ++k )
        {
            objects[ k ] = solver_rigid_t{};
            objects[ k ].collision_flags = g.m_flags[ k ];
            std::memcpy( &objects[ k ].friction, &g.m_friction[ k ], 4 );
            std::memcpy( &objects[ k ].restitution, &g.m_restitution[ k ], 4 );
        }
        static solver_manifold_t m;
        m = solver_manifold_t{};
        for ( int k = 0; k < 4; ++k )
            std::memcpy( &m.points[ k ], g.m_in[ k ], 288 );
        m.body0 = g.m_swapped ? &objects[ 1 ] : &objects[ 0 ];
        m.body1 = g.m_swapped ? &objects[ 0 ] : &objects[ 1 ];
        m.num_contacts = g.m_count;
        std::memcpy( &m.contact_breaking_threshold, &g.m_threshold, 4 );

        verify_manifold_log_t log{};
        log.manifold = &m;
        log.objects = objects;
        contact_callbacks_t cb;
        cb.user = &log;
        auto oid = []( const verify_manifold_log_t& l, const solver_rigid_t* o ) { return static_cast< unsigned >( o - l.objects ); };
        auto sid = []( const verify_manifold_log_t& l, const solver_contact_point_t& p ) { return static_cast< unsigned >( &p - l.manifold->points ); };
        if ( g.m_callbacks[ 0 ] )
            cb.added = []( void* u, solver_contact_point_t& cp, solver_rigid_t* o0, int p0, int i0, solver_rigid_t* o1, int p1, int i1 )
            {
                verify_manifold_log_t& l = *static_cast< verify_manifold_log_t* >( u );
                const unsigned e[ 8 ] = { 1u, static_cast< unsigned >( &cp - l.manifold->points ), static_cast< unsigned >( o0 - l.objects ),
                    static_cast< unsigned >( p0 ), static_cast< unsigned >( i0 ), static_cast< unsigned >( o1 - l.objects ),
                    static_cast< unsigned >( p1 ), static_cast< unsigned >( i1 ) };
                verify_manifold_event( l, e );
            };
        if ( g.m_callbacks[ 1 ] )
            cb.processed = []( void* u, solver_contact_point_t& cp, solver_rigid_t* b0, solver_rigid_t* b1 )
            {
                verify_manifold_log_t& l = *static_cast< verify_manifold_log_t* >( u );
                const unsigned e[ 8 ] = { 2u, static_cast< unsigned >( &cp - l.manifold->points ), static_cast< unsigned >( b0 - l.objects ),
                    static_cast< unsigned >( b1 - l.objects ), 0, 0, 0, 0 };
                verify_manifold_event( l, e );
            };
        if ( g.m_callbacks[ 2 ] )
            cb.destroyed = []( void* u, unsigned data )
            {
                verify_manifold_log_t& l = *static_cast< verify_manifold_log_t* >( u );
                const unsigned e[ 8 ] = { 3u, data, 0, 0, 0, 0, 0, 0 };
                verify_manifold_event( l, e );
            };
        ( void ) oid;
        ( void ) sid;

        float t0[ 16 ], t1[ 16 ], normal[ 4 ], point[ 4 ], depth;
        std::memcpy( t0, g.m_t0, 64 );
        std::memcpy( t1, g.m_t1, 64 );
        std::memcpy( normal, g.m_normal, 16 );
        std::memcpy( point, g.m_point, 16 );
        std::memcpy( &depth, &g.m_depth, 4 );
        if ( g.m_kind == 0 )
        {
            manifold_result_t r;
            r.manifold = &m;
            std::memcpy( r.basis0, t0, 48 );
            std::memcpy( r.origin0, t0 + 12, 16 );
            std::memcpy( r.basis1, t1, 48 );
            std::memcpy( r.origin1, t1 + 12, 16 );
            r.object0 = &objects[ 0 ];
            r.object1 = &objects[ 1 ];
            r.part_id0 = g.m_ids[ 0 ];
            r.part_id1 = g.m_ids[ 1 ];
            r.index0 = g.m_ids[ 2 ];
            r.index1 = g.m_ids[ 3 ];
            manifold_result_add_contact_point( r, normal, point, depth, cb );
        }
        else if ( g.m_kind == 1 )
            manifold_refresh( m, t0, t0 + 12, t1, t1 + 12, cb );
        else
            manifold_remove_point( m, g.m_arg, cb );
        ++per[ g.m_kind ];

        const char* what = nullptr;
        if ( m.num_contacts != g.m_out_count )
            what = "the count";
        for ( int k = 0; k < 4 && !what; ++k )
        {
            unsigned ours[ 72 ];
            std::memcpy( ours, &m.points[ k ], 288 );
            for ( int w = 0; w < 72 && !what; ++w )
            {
                const bool unknown = g.m_kind == 0 && ( ( w >= 37 && w < 48 ) || ( w >= 48 && w < 72 && ( w - 48 ) % 8 != 7 ) );
                if ( !unknown && ours[ w ] != g.m_out[ k ][ w ] )
                    what = "a point";
            }
        }
        if ( !what && log.count != g.m_event_count )
            what = "the callback count";
        for ( int e = 0; e < log.count && !what; ++e )
            if ( std::memcmp( log.events[ e ], g.m_events[ e ], 32 ) != 0 )
                what = "a callback";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "manifold vector %d (kind %d, count %d): %s differs", static_cast< int >( index ), g.m_kind, g.m_count, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the 2.79 manifold matches retail on %d vectors (addContactPoint %d, refreshContactPoints %d, removeContactPoint %d), exact",
            static_cast< int >( g_golden_manifold_count ), per[ 0 ], per[ 1 ], per[ 2 ] );
    return failed == 0;
}

// The 2.79 btVoronoiSimplexSolver against retail on built 400-byte solvers:
// every word out and the returned bool. Coverage is counted from OUR
// outputs: the used-vertex patterns reached by updates of 3 and 4 vertices,
// and the tetrahedron's degenerate / origin-inside outcomes.
inline bool verify_simplex( )
{
    int failed = 0;
    int per[ 3 ] = {};
    unsigned tri = 0, tet = 0;
    int degenerate = 0, inside = 0;
    for ( std::size_t index = 0; index < g_golden_simplex_count; ++index )
    {
        const golden_simplex_t& g = g_golden_simplex[ index ];
        simplex_solver_t s;
        std::memcpy( &s, g.m_in, 400 );
        const bool fresh = s.needs_update != 0;
        float w[ 4 ];
        std::memcpy( w, g.m_w, 16 );
        int result = 0;
        if ( g.m_kind == 0 )
            result = simplex_update( s ) ? 1 : 0;
        else if ( g.m_kind == 1 )
            result = simplex_in_simplex( s, w ) ? 1 : 0;
        else
            simplex_reset( s );
        ++per[ g.m_kind ];
        if ( g.m_kind == 0 && fresh && ( g.m_n == 3 || g.m_n == 4 ) )
        {
            const unsigned used = s.cached_bc.used & 15u;
            ( g.m_n == 3 ? tri : tet ) |= 1u << used;
            if ( g.m_n == 4 && s.cached_bc.degenerate )
                ++degenerate;
            if ( g.m_n == 4 && !s.cached_bc.degenerate && used == 15 && result )
                ++inside;
        }
        unsigned ours[ 100 ];
        std::memcpy( ours, &s, 400 );
        const char* what = nullptr;
        if ( result != g.m_result )
            what = "the result";
        for ( int k = 0; k < 100 && !what; ++k )
            if ( ours[ k ] != g.m_out[ k ] )
                what = "a solver word";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "simplex vector %d (kind %d, n %d): %s differs", static_cast< int >( index ), g.m_kind, g.m_n, what );
        }
    }
    auto bits = []( unsigned m ) { int n = 0; for ( ; m; m &= m - 1 ) ++n; return n; };
    if ( failed == 0 )
        LOG_SUCCESS( "the 2.79 simplex solver matches retail on %d vectors (update %d, inSimplex %d, reset %d), exact; "
                     "triangle regions %d of 7, tetrahedron used-patterns %d, degenerate %d, origin inside %d",
            static_cast< int >( g_golden_simplex_count ), per[ 0 ], per[ 1 ], per[ 2 ], bits( tri & 0xFEu ), bits( tet ),
            degenerate, inside );
    return failed == 0;
}

// The support switch @0x553980 against retail on fake shapes: all four
// lanes and whether the shape's virtual was reached. A hull / point cloud
// with no point above -1e18 is retail's points[-1] read - an UNKNOWN,
// masked and counted (no content shape is type 4 or 5).
inline bool verify_support( )
{
    int failed = 0;
    int per_type[ 32 ] = {};
    int unknown = 0, hooked = 0;
    struct hook_state_t
    {
        const float* dir;
        bool called;
    };
    for ( std::size_t index = 0; index < g_golden_support_count; ++index )
    {
        const golden_support_t& g = g_golden_support[ index ];
        convex_shape_t s{};
        s.type = g.m_type;
        s.up_axis = g.m_up;
        std::memcpy( s.local_scaling, g.m_scaling, 16 );
        std::memcpy( s.implicit, g.m_implicit, 16 );
        std::memcpy( &s.margin, &g.m_margin, 4 );
        std::memcpy( s.vertices, g.m_verts, 48 );
        float points[ 8 ][ 4 ];
        std::memcpy( points, g.m_points, sizeof( points ) );
        s.points = points;
        s.num_points = g.m_count;
        float dir[ 4 ];
        std::memcpy( dir, g.m_dir, 16 );
        hook_state_t hs{ dir, false };
        s.user = &hs;
        s.virtual_support = []( void* user, const convex_shape_t&, const float* d, float* out )
        {
            hook_state_t& h = *static_cast< hook_state_t* >( user );
            h.called = d == h.dir;
            const uint32_t marker[ 4 ] = { 0xABCD0001u, 0xABCD0002u, 0xABCD0003u, 0xABCD0004u };
            std::memcpy( out, marker, 16 );
            return true;
        };
        uint32_t out[ 4 ] = { 0x77777777u, 0x77777777u, 0x77777777u, 0x77777777u };
        float fout[ 4 ];
        std::memcpy( fout, out, 16 );
        const bool known = shape_support( s, dir, fout );
        std::memcpy( out, fout, 16 );
        if ( g.m_type >= 0 && g.m_type < 32 )
            ++per_type[ g.m_type ];
        if ( hs.called )
            ++hooked;
        if ( !known && ( g.m_type == 4 || g.m_type == 5 ) )
        {
            ++unknown;
            continue;
        }
        const char* what = nullptr;
        if ( ( hs.called ? 1 : 0 ) != g.m_hooked )
            what = "the virtual call";
        else if ( std::memcmp( out, g.m_out, 16 ) != 0 )
            what = "the support vertex";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "support vector %d (type %d, up %d): %s differs", static_cast< int >( index ), g.m_type, g.m_up, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "localGetSupportVertexWithoutMarginNonVirtual matches retail on %d vectors, exact (box %d, triangle %d, "
                     "sphere %d, capsule %d, cylinder %d, hull %d, cloud %d, to the virtual %d; %d hull reads of points[-1] masked)",
            static_cast< int >( g_golden_support_count ), per_type[ 0 ], per_type[ 1 ], per_type[ 8 ], per_type[ 10 ],
            per_type[ 13 ], per_type[ 4 ], per_type[ 5 ], hooked, unknown );
    return failed == 0;
}

// Type 3 end to end (0x553980 -> 0x526570 -> VuStridingMesh walk ->
// LocalSupportVertexCallback) against retail with the real vtables.
inline bool verify_mesh_support( )
{
    int failed = 0, empty = 0, ragged = 0;
    for ( std::size_t index = 0; index < g_golden_mesh_support_count; ++index )
    {
        const golden_mesh_support_t& g = g_golden_mesh_support[ index ];
        float verts[ 32 ][ 4 ];
        std::memcpy( verts, g.m_verts, sizeof( verts ) );
        striding_mesh_t mesh{};
        std::memcpy( mesh.scaling, g.m_scaling, 16 );
        mesh.vertices = verts;
        mesh.num_vertices = g.m_nv;
        mesh.indices = g.m_ids;
        mesh.index_count = g.m_count;
        convex_shape_t s{};
        s.type = 3;
        s.mesh = &mesh;
        float dir[ 4 ];
        std::memcpy( dir, g.m_dir, 16 );
        float out[ 4 ];
        const uint32_t fill[ 4 ] = { 0x77777777u, 0x77777777u, 0x77777777u, 0x77777777u };
        std::memcpy( out, fill, 16 );
        const bool ok = shape_support( s, dir, out );
        if ( g.m_count == 0 )
            ++empty;
        if ( g.m_count % 3 )
            ++ragged;
        if ( !ok || std::memcmp( out, g.m_out, 16 ) != 0 )
        {
            ++failed;
            LOG_ERROR( "mesh support vector %d (%d vertices, %d indices): the support vertex differs", static_cast< int >( index ),
                g.m_nv, g.m_count );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btConvexTriangleMeshShape support through VuStridingMesh matches retail on %d meshes, exact "
                     "(%d with no subpart, %d with a ragged index count)",
            static_cast< int >( g_golden_mesh_support_count ), empty, ragged );
    return failed == 0;
}

// getClosestPointsNonVirtual @0x60E0B0 against retail on built detectors:
// the detector words it owns (+16 axis, +68 distance, +72 method, +76
// iterations, +80 degenerate code), the whole simplex, what the
// penetration solver was handed, and the contact. Coverage by method and
// degenerate code, counted from our outputs.
inline bool verify_gjk( )
{
    int failed = 0;
    unsigned methods = 0, codes = 0;
    int contacts = 0, deep = 0;
    struct log_t
    {
        const golden_gjk_t* g;
        bool called;
        bool args_ok;
        uint32_t v_in[ 4 ], pa_in[ 4 ], pb_in[ 4 ], ta[ 16 ], tb[ 16 ];
        int contacts;
        uint32_t event[ 9 ];
        const simplex_solver_t* simplex;
        const convex_shape_t* a;
        const convex_shape_t* b;
    };
    for ( std::size_t index = 0; index < g_golden_gjk_count; ++index )
    {
        const golden_gjk_t& g = g_golden_gjk[ index ];
        float verts[ 2 ][ 12 ][ 4 ];
        striding_mesh_t meshes[ 2 ]{};
        convex_shape_t shapes[ 2 ]{};
        for ( int k = 0; k < 2; ++k )
        {
            const golden_gjk_shape_t& gs = g.m_shapes[ k ];
            convex_shape_t& s = shapes[ k ];
            s.type = gs.m_kind;
            std::memcpy( s.implicit, gs.m_half, 16 );
            if ( gs.m_kind == 3 )
            {
                std::memcpy( verts[ k ], gs.m_verts, sizeof( verts[ k ] ) );
                std::memcpy( meshes[ k ].scaling, gs.m_scaling, 16 );
                meshes[ k ].vertices = verts[ k ];
                meshes[ k ].num_vertices = gs.m_nv;
                meshes[ k ].indices = gs.m_ids;
                meshes[ k ].index_count = gs.m_count;
                s.mesh = &meshes[ k ];
            }
        }
        simplex_solver_t simplex;
        std::memcpy( &simplex, g.m_sim_in, 400 );
        gjk_detector_t d{};
        std::memcpy( d.cached_separating_axis, &g.m_det_in[ 4 ], 16 );
        d.pen_depth_solver = g.m_has_pd != 0;
        d.simplex = &simplex;
        d.a = &shapes[ 0 ];
        d.b = &shapes[ 1 ];
        d.shape_type_a = shapes[ 0 ].type;
        d.shape_type_b = shapes[ 1 ].type;
        std::memcpy( &d.margin_a, &g.m_margins[ 0 ], 4 );
        std::memcpy( &d.margin_b, &g.m_margins[ 1 ], 4 );
        d.ignore_margin = static_cast< uint8_t >( g.m_ignore );
        std::memcpy( &d.cached_separating_distance, &g.m_det_in[ 17 ], 4 );
        d.last_used_method = static_cast< int >( g.m_det_in[ 18 ] );
        d.cur_iter = static_cast< int >( g.m_det_in[ 19 ] );
        d.degenerate_simplex = static_cast< int >( g.m_det_in[ 20 ] );
        d.catch_degeneracies = g.m_catch;
        gjk_input_t input{};
        std::memcpy( input.transform_a, g.m_ta, 64 );
        std::memcpy( input.transform_b, g.m_tb, 64 );
        std::memcpy( &input.max_distance2, &g.m_maxd2, 4 );
        input.stack_alloc = reinterpret_cast< void* >( static_cast< uintptr_t >( 0x5747 ) );
        log_t log{};
        log.g = &g;
        log.simplex = &simplex;
        log.a = &shapes[ 0 ];
        log.b = &shapes[ 1 ];
        gjk_hooks_t hooks;
        hooks.user = &log;
        hooks.pen_depth = []( void* u, simplex_solver_t& s, const convex_shape_t& a, const convex_shape_t& b, const float* ta,
                              const float* tb, float* v, float* pa, float* pb, void* dbg, void* stk )
        {
            log_t& l = *static_cast< log_t* >( u );
            l.called = true;
            l.args_ok = &s == l.simplex && &a == l.a && &b == l.b && dbg == reinterpret_cast< void* >( static_cast< uintptr_t >( 0xD00D ) ) &&
                stk == reinterpret_cast< void* >( static_cast< uintptr_t >( 0x5747 ) );
            std::memcpy( l.v_in, v, 16 );
            std::memcpy( l.pa_in, pa, 16 );
            std::memcpy( l.pb_in, pb, 16 );
            std::memcpy( l.ta, ta, 64 );
            std::memcpy( l.tb, tb, 64 );
            std::memcpy( v, l.g->m_pd_v_out, 16 );
            std::memcpy( pa, l.g->m_pd_pa_out, 16 );
            std::memcpy( pb, l.g->m_pd_pb_out, 16 );
            return l.g->m_pd_ret != 0 ? 1 : 0;
        };
        hooks.add_contact = []( void* u, const float* n, const float* p, float depth )
        {
            log_t& l = *static_cast< log_t* >( u );
            ++l.contacts;
            std::memcpy( &l.event[ 0 ], n, 16 );
            std::memcpy( &l.event[ 4 ], p, 16 );
            std::memcpy( &l.event[ 8 ], &depth, 4 );
        };
        const bool ok = gjk_closest_points( d, input, hooks, reinterpret_cast< void* >( static_cast< uintptr_t >( 0xD00D ) ) );
        methods |= 1u << ( d.last_used_method + 1 );
        codes |= 1u << d.degenerate_simplex;
        contacts += log.contacts;
        deep += log.called ? 1 : 0;
        uint32_t axis[ 4 ], dist;
        std::memcpy( axis, d.cached_separating_axis, 16 );
        std::memcpy( &dist, &d.cached_separating_distance, 4 );
        const char* what = nullptr;
        if ( !ok )
            what = "a support (returned false)";
        else if ( std::memcmp( axis, &g.m_det_out[ 0 ], 16 ) != 0 )
            what = "the separating axis";
        else if ( dist != g.m_det_out[ 13 ] )
            what = "the separating distance";
        else if ( static_cast< unsigned >( d.last_used_method ) != g.m_det_out[ 14 ] )
            what = "the method";
        else if ( static_cast< unsigned >( d.cur_iter ) != g.m_det_out[ 15 ] )
            what = "the iteration count";
        else if ( static_cast< unsigned >( d.degenerate_simplex ) != g.m_det_out[ 16 ] )
            what = "the degenerate code";
        else if ( std::memcmp( &simplex, g.m_sim_out, 400 ) != 0 )
            what = "the simplex";
        else if ( ( log.called ? 1 : 0 ) != g.m_pd_called )
            what = "the pd call";
        else if ( log.called && ( !log.args_ok || std::memcmp( log.v_in, g.m_pd_v_in, 16 ) || std::memcmp( log.pa_in, g.m_pd_pa_in, 16 ) ||
                                    std::memcmp( log.pb_in, g.m_pd_pb_in, 16 ) || std::memcmp( log.ta, g.m_pd_ta, 64 ) ||
                                    std::memcmp( log.tb, g.m_pd_tb, 64 ) ) )
            what = "what the pd solver was handed";
        else if ( log.contacts != g.m_contact )
            what = "the contact count";
        else if ( log.contacts && std::memcmp( log.event, g.m_event, 36 ) != 0 )
            what = "the contact";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "gjk vector %d (shapes %d-%d): %s differs", static_cast< int >( index ), g.m_shapes[ 0 ].m_kind,
                g.m_shapes[ 1 ].m_kind, what );
        }
    }
    if ( failed == 0 )
    {
        char m[ 64 ] = {}, c[ 64 ] = {};
        int mi = 0, ci = 0;
        for ( int k = 0; k < 11; ++k )
            if ( methods & ( 1u << ( k + 1 ) ) )
                mi += std::snprintf( m + mi, sizeof( m ) - mi, "%d ", k );
        for ( int k = 0; k < 14; ++k )
            if ( codes & ( 1u << k ) )
                ci += std::snprintf( c + ci, sizeof( c ) - ci, "%d ", k );
        LOG_SUCCESS( "getClosestPointsNonVirtual matches retail on %d pairs, exact (%d contacts, %d pd calls; methods %s; "
                     "degenerate codes %s)",
            static_cast< int >( g_golden_gjk_count ), contacts, deep, m, c );
    }
    return failed == 0;
}

// gjkepa2's Initialize @0x611ED0 and GJK::getsupport @0x60FF40 against
// retail: the results words Initialize writes (status, both witnesses), the
// MinkowskiDiff's two transforms (every lane), which support it picked, and
// the support vertex (d and w). Pre-filled words outside those must stay.
inline bool verify_epa_support( )
{
    int failed = 0, margins = 0;
    for ( std::size_t index = 0; index < g_golden_epa_support_count; ++index )
    {
        const golden_epa_support_t& g = g_golden_epa_support[ index ];
        float verts[ 2 ][ 12 ][ 4 ];
        striding_mesh_t meshes[ 2 ]{};
        convex_shape_t shapes[ 2 ]{};
        for ( int k = 0; k < 2; ++k )
        {
            const golden_epa_shape_t& gs = g.m_shapes[ k ];
            convex_shape_t& s = shapes[ k ];
            s.type = gs.m_kind;
            std::memcpy( s.implicit, gs.m_half, 16 );
            std::memcpy( &s.margin, &gs.m_margin, 4 );
            std::memcpy( s.local_scaling, gs.m_scaling, 16 );
            s.up_axis = gs.m_axis;
            if ( gs.m_kind == 3 )
            {
                std::memcpy( verts[ k ], gs.m_verts, sizeof( verts[ k ] ) );
                std::memcpy( meshes[ k ].scaling, gs.m_scaling, 16 );
                meshes[ k ].vertices = verts[ k ];
                meshes[ k ].num_vertices = gs.m_nv;
                meshes[ k ].indices = gs.m_ids;
                meshes[ k ].index_count = gs.m_count;
                s.mesh = &meshes[ k ];
            }
        }
        float ta[ 16 ], tb[ 16 ], d[ 4 ];
        std::memcpy( ta, g.m_ta, 64 );
        std::memcpy( tb, g.m_tb, 64 );
        std::memcpy( d, g.m_d, 16 );
        epa_results_t results;
        std::memset( &results, 0x77, sizeof( results ) );
        epa_shape_t shape{};
        epa_initialize( &shapes[ 0 ], ta, &shapes[ 1 ], tb, results, shape, g.m_flag != 0 );
        epa_sv_t sv{};
        const bool ok = epa_get_support( shape, d, sv );
        margins += shape.with_margin ? 1 : 0;
        uint32_t res[ 17 ];
        std::memcpy( res, &results, 68 );
        const char* what = nullptr;
        if ( !ok )
            what = "a support (returned false)";
        else if ( std::memcmp( res, g.m_res_out, 68 ) != 0 )
            what = "the results";
        else if ( std::memcmp( shape.toshape1, &g.m_sh_out[ 4 ], 48 ) != 0 || std::memcmp( shape.toshape0, &g.m_sh_out[ 16 ], 64 ) != 0 )
            what = "the transforms";
        else if ( g.m_sh_out[ 32 ] != ( shape.with_margin ? 0x5540B0u : 0x553980u ) )
            what = "the support picked";
        else if ( std::memcmp( &sv, g.m_sv_out, 32 ) != 0 )
            what = "the support vertex";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "epa support vector %d (shapes %d-%d, margin %d): %s differs", static_cast< int >( index ),
                g.m_shapes[ 0 ].m_kind, g.m_shapes[ 1 ].m_kind, g.m_flag, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "gjkepa2 Initialize + getsupport match retail on %d pairs, exact (%d with margins, through "
                     "localGetSupportVertexNonVirtual 0x5540B0)",
            static_cast< int >( g_golden_epa_support_count ), margins );
    return failed == 0;
}

// gjkepa2's three projectorigin overloads against retail: the returned
// squared distance (bits), the weights and the mask - pre-filled words must
// stay where retail leaves them. Masks reached are counted per kind.
inline bool verify_epa_project( )
{
    int failed = 0;
    unsigned masks[ 3 ] = {};
    int degenerate[ 3 ] = {};
    for ( std::size_t index = 0; index < g_golden_epa_project_count; ++index )
    {
        const golden_epa_project_t& g = g_golden_epa_project[ index ];
        float p[ 4 ][ 4 ];
        std::memcpy( p, g.m_pts, sizeof( p ) );
        uint32_t wbits[ 4 ] = { 0x77777777u, 0x77777777u, 0x77777777u, 0x77777777u };
        float w[ 4 ];
        std::memcpy( w, wbits, 16 );
        unsigned m = 0x77777777u;
        float r;
        if ( g.m_kind == 0 )
            r = epa_project_segment( p[ 0 ], p[ 1 ], w, m );
        else if ( g.m_kind == 1 )
            r = epa_project_triangle( p[ 0 ], p[ 1 ], p[ 2 ], w, m );
        else
            r = epa_project_tetra( p[ 0 ], p[ 1 ], p[ 2 ], p[ 3 ], w, m );
        uint32_t rb;
        std::memcpy( &rb, &r, 4 );
        std::memcpy( wbits, w, 16 );
        if ( m != 0x77777777u )
            masks[ g.m_kind ] |= 1u << ( m & 15 );
        else
            ++degenerate[ g.m_kind ];
        if ( rb != g.m_ret || std::memcmp( wbits, g.m_w, 16 ) != 0 || m != g.m_m )
        {
            ++failed;
            LOG_ERROR( "epa projectorigin vector %d (kind %d): %s differs", static_cast< int >( index ), g.m_kind,
                rb != g.m_ret ? "the distance" : m != g.m_m ? "the mask" : "the weights" );
        }
    }
    auto bits = []( unsigned x ) { int n = 0; for ( ; x; x &= x - 1 ) ++n; return n; };
    if ( failed == 0 )
        LOG_SUCCESS( "gjkepa2 projectorigin matches retail on %d simplices, exact (masks reached: segment %d, triangle %d, "
                     "tetrahedron %d; degenerate -1 returns %d / %d / %d)",
            static_cast< int >( g_golden_epa_project_count ), bits( masks[ 0 ] ), bits( masks[ 1 ] ), bits( masks[ 2 ] ),
            degenerate[ 0 ], degenerate[ 1 ], degenerate[ 2 ] );
    return failed == 0;
}

// Retail's Initialize + GJK::Evaluate (+ EncloseOrigin when asked) replayed
// on one golden pair; `img` gets our epa_gjk_t laid back out as retail's
// 400-byte block (store indices -> pointers, never-written words 0 as in the
// zero-filled oracle block). Returns false where a support returned false.
template < typename G, typename F >
inline bool epa_gjk_replay( const G& g, bool enclose, epa_gjk_t& gjk, uint32_t* img, int& enclosed, int& entry_rank, F&& after )
{
    float verts[ 2 ][ 12 ][ 4 ];
    striding_mesh_t meshes[ 2 ]{};
    convex_shape_t shapes[ 2 ]{};
    for ( int k = 0; k < 2; ++k )
    {
        const golden_epa_shape_t& gs = g.m_shapes[ k ];
        convex_shape_t& s = shapes[ k ];
        s.type = gs.m_kind;
        std::memcpy( s.implicit, gs.m_half, 16 );
        std::memcpy( &s.margin, &gs.m_margin, 4 );
        std::memcpy( s.local_scaling, gs.m_scaling, 16 );
        s.up_axis = gs.m_axis;
        if ( gs.m_kind == 3 )
        {
            std::memcpy( verts[ k ], gs.m_verts, sizeof( verts[ k ] ) );
            std::memcpy( meshes[ k ].scaling, gs.m_scaling, 16 );
            meshes[ k ].vertices = verts[ k ];
            meshes[ k ].num_vertices = gs.m_nv;
            meshes[ k ].indices = gs.m_ids;
            meshes[ k ].index_count = gs.m_count;
            s.mesh = &meshes[ k ];
        }
    }
    float ta[ 16 ], tb[ 16 ], guess[ 4 ];
    std::memcpy( ta, g.m_ta, 64 );
    std::memcpy( tb, g.m_tb, 64 );
    std::memcpy( guess, g.m_guess, 16 );
    epa_results_t results{};
    epa_shape_t shape{};
    epa_initialize( &shapes[ 0 ], ta, &shapes[ 1 ], tb, results, shape, true );
    bool ok = epa_gjk_evaluate( gjk, shape, guess );
    entry_rank = gjk.simplex >= 0 ? static_cast< int >( gjk.simplices[ gjk.simplex ].rank ) : -1;
    enclosed = 0;
    if ( ok && enclose )
    {
        enclosed = epa_enclose_origin( gjk );
        ok = enclosed >= 0;
    }
    if ( ok )
        ok = after( gjk );
    std::memset( img, 0, 400 );
    const uint32_t base = g_golden_epa_gjk_base;
    auto ptr = [ & ]( int k ) { return k < 0 ? 0u : base + 240u + 32u * static_cast< uint32_t >( k ); };
    img[ 0 ] = g_golden_epa_gjk_shapes[ 0 ];
    img[ 1 ] = g_golden_epa_gjk_shapes[ 1 ];
    std::memcpy( &img[ 4 ], gjk.shape.toshape1, 48 );
    std::memcpy( &img[ 16 ], gjk.shape.toshape0, 64 );
    img[ 32 ] = gjk.shape.with_margin ? 0x5540B0u : 0x553980u;
    std::memcpy( &img[ 36 ], gjk.ray, 16 );
    std::memcpy( &img[ 40 ], &gjk.distance, 4 );
    for ( int s = 0; s < 2; ++s )
    {
        for ( int k = 0; k < 4; ++k )
            img[ 41 + 9 * s + k ] = ptr( gjk.simplices[ s ].c[ k ] );
        std::memcpy( &img[ 45 + 9 * s ], gjk.simplices[ s ].p, 16 );
        img[ 49 + 9 * s ] = gjk.simplices[ s ].rank;
    }
    std::memcpy( &img[ 60 ], gjk.store, 128 );
    for ( int k = 0; k < 4; ++k )
        img[ 92 + k ] = ptr( gjk.free[ k ] );
    img[ 96 ] = gjk.nfree;
    img[ 97 ] = gjk.current;
    img[ 98 ] = gjk.simplex < 0 ? 0u : base + 164u + 36u * static_cast< uint32_t >( gjk.simplex );
    img[ 99 ] = static_cast< uint32_t >( gjk.status );
    return ok;
}

// The EPA leaves against retail: Initialize, then the recorded sequence of
// newface / findbest / remove, our epa_t laid back out as retail's words
// (header, faces 0..9, nextsv + the lists; pointers mapped to retail's
// addresses) plus each call's returned face. Counted: faces made /
// rejected, p clamped to 0, final statuses.
inline bool verify_epa_hull( )
{
    int failed = 0;
    int statuses[ 10 ] = {};
    int made = 0, rejected = 0, clamped = 0, bests = 0, removes = 0;
    static epa_t e;
    const uint32_t base = g_golden_epa_hull_base, svbase = g_golden_epa_hull_svs;
    for ( std::size_t index = 0; index < g_golden_epa_hull_count; ++index )
    {
        const golden_epa_hull_t& g = g_golden_epa_hull[ index ];
        std::memset( &e, 0, sizeof( e ) );
        epa_sv_t svs[ 6 ]{};
        for ( int j = 0; j < 6; ++j )
            std::memcpy( svs[ j ].w, &g.m_ws[ 4 * j ], 16 );
        epa_init( e );
        auto fi = [ & ]( const epa_face_t* f ) { return f ? static_cast< int >( f - e.fc_store ) : -1; };
        int rets[ 8 ];
        for ( int k = 0; k < 8; ++k )
        {
            if ( index % 8 == 7 && k == 3 )
                e.stock.root = nullptr;
            const int* op = &g.m_ops[ 5 * k ];
            if ( op[ 0 ] == 1 )
            {
                epa_face_t* f = epa_newface( e, &svs[ op[ 1 ] ], &svs[ op[ 2 ] ], &svs[ op[ 3 ] ], op[ 4 ] != 0 );
                rets[ k ] = fi( f );
                if ( f )
                {
                    ++made;
                    clamped += f->p == 0.0f ? 1 : 0;
                }
                else
                    ++rejected;
            }
            else if ( op[ 0 ] == 2 )
            {
                rets[ k ] = fi( epa_findbest( e ) );
                ++bests;
            }
            else
            {
                epa_list_remove( e.hull, &e.fc_store[ op[ 1 ] ] );
                rets[ k ] = 0;
                ++removes;
            }
        }
        auto sp = [ & ]( const epa_sv_t* p ) { return p ? svbase + 32u * static_cast< uint32_t >( p - svs ) : 0u; };
        auto fp = [ & ]( const epa_face_t* f ) { return f ? base + 0x850u + 64u * static_cast< uint32_t >( f - e.fc_store ) : 0u; };
        uint32_t img[ 185 ] = {};
        img[ 0 ] = static_cast< uint32_t >( e.status );
        for ( int k = 0; k < 4; ++k )
            img[ 1 + k ] = sp( e.c[ k ] );
        std::memcpy( &img[ 5 ], e.p, 16 );
        img[ 9 ] = e.rank;
        std::memcpy( &img[ 12 ], e.normal, 16 );
        std::memcpy( &img[ 16 ], &e.depth, 4 );
        for ( int k = 0; k < 10; ++k )
        {
            const epa_face_t& f = e.fc_store[ k ];
            uint32_t* w = &img[ 20 + 16 * k ];
            std::memcpy( w, f.n, 16 );
            std::memcpy( &w[ 4 ], &f.d, 4 );
            std::memcpy( &w[ 5 ], &f.p, 4 );
            for ( int j = 0; j < 3; ++j )
            {
                w[ 6 + j ] = sp( f.c[ j ] );
                w[ 9 + j ] = fp( f.f[ j ] );
            }
            w[ 12 ] = fp( f.l[ 0 ] );
            w[ 13 ] = fp( f.l[ 1 ] );
            w[ 14 ] = f.e[ 0 ] | f.e[ 1 ] << 8 | f.e[ 2 ] << 16 | static_cast< uint32_t >( f.pass ) << 24;
        }
        img[ 180 ] = e.nextsv;
        img[ 181 ] = fp( e.hull.root );
        img[ 182 ] = e.hull.count;
        img[ 183 ] = fp( e.stock.root );
        img[ 184 ] = e.stock.count;
        if ( e.status >= 0 && e.status < 10 )
            ++statuses[ e.status ];
        int bad = -1;
        for ( int k = 0; k < 185 && bad < 0; ++k )
            if ( img[ k ] != g.m_out[ k ] )
                bad = k;
        const bool same_rets = std::memcmp( rets, g.m_rets, sizeof( rets ) ) == 0;
        if ( bad >= 0 || !same_rets )
        {
            ++failed;
            LOG_ERROR( "epa hull vector %d (kind %d): %s (word %d)", static_cast< int >( index ), static_cast< int >( index % 8 ),
                bad >= 0 ? "the object differs" : "a returned face differs", bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "gjkepa2 EPA Initialize / newface / findbest / remove match retail on %d sequences, exact (faces made %d, "
                     "rejected %d, p clamped %d; findbest %d, remove %d; final status 2: %d, 3: %d, 5: %d, 9: %d)",
            static_cast< int >( g_golden_epa_hull_count ), made, rejected, clamped, bests, removes, statuses[ 2 ], statuses[ 3 ],
            statuses[ 5 ], statuses[ 9 ] );
    return failed == 0;
}

// EPA::Evaluate @0x6110A0 chained as Penetration chains it (Initialize ->
// GJK::Evaluate, Inside only -> EPA::Initialize -> EPA::Evaluate with the
// same guess): the GJK block after, the status, and the WHOLE EPA object as
// retail's 2585 words (pointers into the GJK store / sv_store / fc_store
// mapped to retail's addresses). Counted: statuses, vertices used.
inline bool verify_epa_evaluate( )
{
    int failed = 0;
    int statuses[ 10 ] = {};
    unsigned max_sv = 0, total_sv = 0;
    static epa_t e;
    const uint32_t ebase = g_golden_epa_eval_base, gbase = g_golden_epa_gjk_base;
    for ( std::size_t index = 0; index < g_golden_epa_eval_count; ++index )
    {
        const golden_epa_eval_t& g = g_golden_epa_eval[ index ];
        std::memset( &e, 0, sizeof( e ) );
        epa_gjk_t gjk;
        uint32_t gimg[ 100 ];
        int enclosed = 0, rank = -1, status = -2;
        float guess[ 4 ];
        std::memcpy( guess, g.m_guess, 16 );
        const bool ok = epa_gjk_replay( g, false, gjk, gimg, enclosed, rank, [ & ]( epa_gjk_t& k ) {
            if ( k.status != 1 )
                return false;
            epa_init( e );
            status = epa_evaluate( e, k, guess );
            return status >= 0;
        } );
        auto sp = [ & ]( const epa_sv_t* p ) -> uint32_t {
            if ( !p )
                return 0u;
            if ( p >= gjk.store && p < gjk.store + 4 )
                return gbase + 240u + 32u * static_cast< uint32_t >( p - gjk.store );
            return ebase + 80u + 32u * static_cast< uint32_t >( p - e.sv_store );
        };
        auto fp = [ & ]( const epa_face_t* f ) { return f ? ebase + 0x850u + 64u * static_cast< uint32_t >( f - e.fc_store ) : 0u; };
        static uint32_t img[ 2585 ];
        std::memset( img, 0, sizeof( img ) );
        img[ 0 ] = static_cast< uint32_t >( e.status );
        for ( int k = 0; k < 4; ++k )
            img[ 1 + k ] = sp( e.c[ k ] );
        std::memcpy( &img[ 5 ], e.p, 16 );
        img[ 9 ] = e.rank;
        std::memcpy( &img[ 12 ], e.normal, 16 );
        std::memcpy( &img[ 16 ], &e.depth, 4 );
        std::memcpy( &img[ 20 ], e.sv_store, sizeof( e.sv_store ) );
        for ( unsigned k = 0; k < g_epa_max_faces; ++k )
        {
            const epa_face_t& f = e.fc_store[ k ];
            uint32_t* w = &img[ 532 + 16 * k ];
            std::memcpy( w, f.n, 16 );
            std::memcpy( &w[ 4 ], &f.d, 4 );
            std::memcpy( &w[ 5 ], &f.p, 4 );
            for ( int j = 0; j < 3; ++j )
            {
                w[ 6 + j ] = sp( f.c[ j ] );
                w[ 9 + j ] = fp( f.f[ j ] );
            }
            w[ 12 ] = fp( f.l[ 0 ] );
            w[ 13 ] = fp( f.l[ 1 ] );
            w[ 14 ] = f.e[ 0 ] | f.e[ 1 ] << 8 | f.e[ 2 ] << 16 | static_cast< uint32_t >( f.pass ) << 24;
        }
        img[ 2580 ] = e.nextsv;
        img[ 2581 ] = fp( e.hull.root );
        img[ 2582 ] = e.hull.count;
        img[ 2583 ] = fp( e.stock.root );
        img[ 2584 ] = e.stock.count;
        // Retail's image: Initialize's, then this vector's changed words.
        static uint32_t want[ 2585 ];
        std::memcpy( want, g_golden_epa_eval_init, sizeof( want ) );
        for ( unsigned k = 0; k < g.m_diff_count; ++k )
            want[ g_golden_epa_eval_diff[ 2 * ( g.m_diff_at + k ) ] ] = g_golden_epa_eval_diff[ 2 * ( g.m_diff_at + k ) + 1 ];
        int bad = -1, gbad = -1;
        for ( int k = 0; k < 2585 && bad < 0; ++k )
            if ( img[ k ] != want[ k ] )
                bad = k;
        for ( int k = 0; k < 100 && gbad < 0; ++k )
            if ( gimg[ k ] != g.m_out[ k ] )
                gbad = k;
        if ( status >= 0 && status < 10 )
            ++statuses[ status ];
        total_sv += e.nextsv;
        max_sv = e.nextsv > max_sv ? e.nextsv : max_sv;
        if ( !ok || bad >= 0 || gbad >= 0 || status != g.m_epa_status )
        {
            ++failed;
            LOG_ERROR( "epa evaluate vector %d (shapes %d-%d): %s (word %d, gjk word %d, status %d / %d)", static_cast< int >( index ),
                g.m_shapes[ 0 ].m_kind, g.m_shapes[ 1 ].m_kind, !ok ? "the chain stopped" : bad >= 0 ? "the EPA differs"
                : gbad >= 0 ? "the GJK block differs" : "the status differs", bad, gbad, status, g.m_epa_status );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "gjkepa2 EPA::Evaluate matches retail on %d Inside pairs, the whole EPA object + GJK block exact "
                     "(AccuraryReached %d, InvalidHull %d, FallBack %d, OutOfVertices %d, Valid %d; vertices %u total, max %u)",
            static_cast< int >( g_golden_epa_eval_count ), statuses[ 7 ], statuses[ 4 ], statuses[ 8 ], statuses[ 6 ], statuses[ 0 ],
            total_sv, max_sv );
    return failed == 0;
}

// A golden shape pair built in place (the meshes point into it - never copy
// it after build).
struct epa_pair_t
{
    float verts[ 2 ][ 12 ][ 4 ];
    striding_mesh_t meshes[ 2 ]{};
    convex_shape_t shapes[ 2 ]{};

    template < typename G >
    void build( const G& g )
    {
        for ( int k = 0; k < 2; ++k )
        {
            const golden_epa_shape_t& gs = g.m_shapes[ k ];
            convex_shape_t& s = shapes[ k ];
            s.type = gs.m_kind;
            std::memcpy( s.implicit, gs.m_half, 16 );
            std::memcpy( &s.margin, &gs.m_margin, 4 );
            std::memcpy( s.local_scaling, gs.m_scaling, 16 );
            s.up_axis = gs.m_axis;
            if ( gs.m_kind == 3 )
            {
                std::memcpy( verts[ k ], gs.m_verts, sizeof( verts[ k ] ) );
                std::memcpy( meshes[ k ].scaling, gs.m_scaling, 16 );
                meshes[ k ].vertices = verts[ k ];
                meshes[ k ].num_vertices = gs.m_nv;
                meshes[ k ].indices = gs.m_ids;
                meshes[ k ].index_count = gs.m_count;
                s.mesh = &meshes[ k ];
            }
        }
    }
};

// btGjkEpaSolver2::Penetration @0x6129E0 against retail called directly on
// every pair: the bool and the 17 result words (pre-filled 0x77777777 in
// both, so the unwritten normal / distance of a false return compare too).
inline bool verify_epa_penetration( )
{
    int failed = 0, penetrating = 0, separated = 0;
    for ( std::size_t index = 0; index < g_golden_epa_pen_count; ++index )
    {
        const golden_epa_pen_t& g = g_golden_epa_pen[ index ];
        epa_pair_t pair;
        pair.build( g );
        float ta[ 16 ], tb[ 16 ], guess[ 4 ];
        std::memcpy( ta, g.m_ta, 64 );
        std::memcpy( tb, g.m_tb, 64 );
        std::memcpy( guess, g.m_guess, 16 );
        epa_results_t results;
        std::memset( &results, 0x77, sizeof( results ) );
        const int ret = epa_penetration( &pair.shapes[ 0 ], ta, &pair.shapes[ 1 ], tb, guess, results );
        uint32_t res[ 17 ];
        std::memcpy( res, &results, 68 );
        ( ret == 1 ? penetrating : separated ) += ret >= 0 ? 1 : 0;
        int bad = -1;
        for ( int k = 0; k < 17 && bad < 0; ++k )
            if ( res[ k ] != g.m_res[ k ] )
                bad = k;
        if ( ret != g.m_ret || bad >= 0 )
        {
            ++failed;
            LOG_ERROR( "epa penetration vector %d (shapes %d-%d): %s (word %d, ret %d / %d)", static_cast< int >( index ),
                g.m_shapes[ 0 ].m_kind, g.m_shapes[ 1 ].m_kind, ret != g.m_ret ? "the bool differs" : "the results differ", bad, ret,
                g.m_ret );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btGjkEpaSolver2::Penetration matches retail on %d pairs, results exact (penetrating %d, GJK separated %d)",
            static_cast< int >( g_golden_epa_pen_count ), penetrating, separated );
    return failed == 0;
}

// btGjkEpaSolver2::Distance @0x612410 against retail called directly on
// every pair: the bool and the 17 result words (pre-filled 0x77777777).
inline bool verify_epa_distance( )
{
    int failed = 0, separated = 0, inside = 0;
    for ( std::size_t index = 0; index < g_golden_epa_dist_count; ++index )
    {
        const golden_epa_pen_t& g = g_golden_epa_dist[ index ];
        epa_pair_t pair;
        pair.build( g );
        float ta[ 16 ], tb[ 16 ], guess[ 4 ];
        std::memcpy( ta, g.m_ta, 64 );
        std::memcpy( tb, g.m_tb, 64 );
        std::memcpy( guess, g.m_guess, 16 );
        epa_results_t results;
        std::memset( &results, 0x77, sizeof( results ) );
        const int ret = epa_distance( &pair.shapes[ 0 ], ta, &pair.shapes[ 1 ], tb, guess, results );
        uint32_t res[ 17 ];
        std::memcpy( res, &results, 68 );
        separated += ret == 1 ? 1 : 0;
        inside += ret == 0 && results.status == 1 ? 1 : 0;
        int bad = -1;
        for ( int k = 0; k < 17 && bad < 0; ++k )
            if ( res[ k ] != g.m_res[ k ] )
                bad = k;
        if ( ret != g.m_ret || bad >= 0 )
        {
            ++failed;
            LOG_ERROR( "epa distance vector %d (shapes %d-%d): %s (word %d, ret %d / %d)", static_cast< int >( index ),
                g.m_shapes[ 0 ].m_kind, g.m_shapes[ 1 ].m_kind, ret != g.m_ret ? "the bool differs" : "the results differ", bad, ret,
                g.m_ret );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btGjkEpaSolver2::Distance matches retail on %d pairs, results exact (separated %d, inside %d)",
            static_cast< int >( g_golden_epa_dist_count ), separated, inside );
    return failed == 0;
}

// btGjkEpaPenetrationDepthSolver::calcPenDepth @0x5BAF60 against retail: v,
// pa, pb (pre-filled 0x77777777 in both) and the bool. Counted: true,
// false-with-Distance-outputs, false-untouched.
inline bool verify_pen_depth( )
{
    int failed = 0, pen = 0, dist = 0, none = 0;
    for ( std::size_t index = 0; index < g_golden_pen_depth_count; ++index )
    {
        const golden_pen_depth_t& g = g_golden_pen_depth[ index ];
        epa_pair_t pair;
        pair.build( g );
        float ta[ 16 ], tb[ 16 ];
        std::memcpy( ta, g.m_ta, 64 );
        std::memcpy( tb, g.m_tb, 64 );
        uint32_t out[ 12 ];
        std::memset( out, 0x77, sizeof( out ) );
        float v[ 4 ], pa[ 4 ], pb[ 4 ];
        std::memcpy( v, &out[ 0 ], 16 );
        std::memcpy( pa, &out[ 4 ], 16 );
        std::memcpy( pb, &out[ 8 ], 16 );
        const int ret = epa_calc_pen_depth( pair.shapes[ 0 ], pair.shapes[ 1 ], ta, tb, v, pa, pb );
        std::memcpy( &out[ 0 ], v, 16 );
        std::memcpy( &out[ 4 ], pa, 16 );
        std::memcpy( &out[ 8 ], pb, 16 );
        if ( ret == 1 )
            ++pen;
        else if ( ret == 0 )
            ( out[ 0 ] != 0x77777777u ? dist : none ) += 1;
        if ( ret != g.m_ret || std::memcmp( out, g.m_out, sizeof( out ) ) != 0 )
        {
            ++failed;
            LOG_ERROR( "pen depth vector %d (shapes %d-%d): %s (ret %d / %d)", static_cast< int >( index ), g.m_shapes[ 0 ].m_kind,
                g.m_shapes[ 1 ].m_kind, ret != g.m_ret ? "the bool differs" : "the outputs differ", ret, g.m_ret );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btGjkEpaPenetrationDepthSolver::calcPenDepth matches retail on %d pairs, v / pa / pb exact (true %d, "
                     "false with Distance's outputs %d, false untouched %d)",
            static_cast< int >( g_golden_pen_depth_count ), pen, dist, none );
    return failed == 0;
}

// getClosestPointsNonVirtual @0x60E0B0 END TO END with the EPA pd solver
// (epa_pen_depth_hook) against retail running its real
// btGjkEpaPenetrationDepthSolver: detector words +16..+87, the simplex and
// the contact. Counted by method.
inline bool verify_gjk_epa( )
{
    int failed = 0;
    int methods[ 11 ] = {};
    for ( std::size_t index = 0; index < g_golden_gjk_epa_count; ++index )
    {
        const golden_gjk_epa_t& g = g_golden_gjk_epa[ index ];
        epa_pair_t pair;
        pair.build( g );
        simplex_solver_t simplex;
        std::memcpy( &simplex, g.m_sim_in, 400 );
        gjk_detector_t d{};
        std::memcpy( d.cached_separating_axis, &g.m_det_in[ 4 ], 16 );
        d.pen_depth_solver = true;
        d.simplex = &simplex;
        d.a = &pair.shapes[ 0 ];
        d.b = &pair.shapes[ 1 ];
        d.shape_type_a = pair.shapes[ 0 ].type;
        d.shape_type_b = pair.shapes[ 1 ].type;
        std::memcpy( &d.margin_a, &g.m_det_in[ 14 ], 4 );
        std::memcpy( &d.margin_b, &g.m_det_in[ 15 ], 4 );
        d.ignore_margin = static_cast< uint8_t >( g.m_det_in[ 16 ] & 0xFF );
        std::memcpy( &d.cached_separating_distance, &g.m_det_in[ 17 ], 4 );
        d.last_used_method = static_cast< int >( g.m_det_in[ 18 ] );
        d.cur_iter = static_cast< int >( g.m_det_in[ 19 ] );
        d.degenerate_simplex = static_cast< int >( g.m_det_in[ 20 ] );
        d.catch_degeneracies = g.m_det_in[ 21 ];
        gjk_input_t input{};
        std::memcpy( input.transform_a, g.m_ta, 64 );
        std::memcpy( input.transform_b, g.m_tb, 64 );
        std::memcpy( &input.max_distance2, &g.m_maxd2, 4 );
        struct log_t
        {
            int contacts;
            uint32_t event[ 9 ];
        } log{};
        gjk_hooks_t hooks;
        hooks.user = &log;
        hooks.pen_depth = epa_pen_depth_hook;
        hooks.add_contact = []( void* u, const float* n, const float* p, float depth )
        {
            log_t& l = *static_cast< log_t* >( u );
            ++l.contacts;
            std::memcpy( &l.event[ 0 ], n, 16 );
            std::memcpy( &l.event[ 4 ], p, 16 );
            std::memcpy( &l.event[ 8 ], &depth, 4 );
        };
        const bool ok = gjk_closest_points( d, input, hooks, nullptr );
        if ( d.last_used_method >= -1 && d.last_used_method <= 9 )
            ++methods[ d.last_used_method + 1 ];
        uint32_t axis[ 4 ], dist;
        std::memcpy( axis, d.cached_separating_axis, 16 );
        std::memcpy( &dist, &d.cached_separating_distance, 4 );
        const char* what = nullptr;
        if ( !ok )
            what = "a support or the pd solver reached unrecreated code";
        else if ( std::memcmp( axis, &g.m_det_out[ 0 ], 16 ) != 0 )
            what = "the separating axis";
        else if ( dist != g.m_det_out[ 13 ] )
            what = "the separating distance";
        else if ( static_cast< unsigned >( d.last_used_method ) != g.m_det_out[ 14 ] )
            what = "the method";
        else if ( static_cast< unsigned >( d.cur_iter ) != g.m_det_out[ 15 ] )
            what = "the iteration count";
        else if ( static_cast< unsigned >( d.degenerate_simplex ) != g.m_det_out[ 16 ] )
            what = "the degenerate code";
        else if ( std::memcmp( &simplex, g.m_sim_out, 400 ) != 0 )
            what = "the simplex";
        else if ( log.contacts != g.m_contact )
            what = "the contact count";
        else if ( log.contacts && std::memcmp( log.event, g.m_event, 36 ) != 0 )
            what = "the contact";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "gjk epa vector %d (shapes %d-%d): %s differs", static_cast< int >( index ), g.m_shapes[ 0 ].m_kind,
                g.m_shapes[ 1 ].m_kind, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "getClosestPointsNonVirtual + the EPA pd solver match retail end to end on %d pairs, exact (method 1: %d, "
                     "3 EPA: %d, 5: %d, 8: %d, 9: %d)",
            static_cast< int >( g_golden_gjk_epa_count ), methods[ 2 ], methods[ 4 ], methods[ 6 ], methods[ 9 ], methods[ 10 ] );
    return failed == 0;
}

// btConvexConvexAlgorithm::processCollision @0x5FC900, the GJK + EPA path,
// against retail on an empty owned manifold: the four points (the words a
// new point leaves UNKNOWN masked, as verify_manifold's addContactPoint) and
// the count. Counted: pairs that kept a contact.
inline bool verify_convex_convex( )
{
    int failed = 0, kept = 0;
    for ( std::size_t index = 0; index < g_golden_convex_convex_count; ++index )
    {
        const golden_convex_convex_t& g = g_golden_convex_convex[ index ];
        epa_pair_t pair;
        pair.build( g );
        static solver_rigid_t objects[ 2 ];
        for ( int k = 0; k < 2; ++k )
        {
            objects[ k ] = solver_rigid_t{};
            std::memcpy( &objects[ k ].friction, &g.m_friction[ k ], 4 );
            std::memcpy( &objects[ k ].restitution, &g.m_restitution[ k ], 4 );
        }
        static solver_manifold_t m;
        m = solver_manifold_t{};
        m.body0 = g.m_swapped ? &objects[ 1 ] : &objects[ 0 ];
        m.body1 = g.m_swapped ? &objects[ 0 ] : &objects[ 1 ];
        std::memcpy( &m.contact_breaking_threshold, &g.m_threshold, 4 );
        simplex_solver_t simplex;
        std::memcpy( &simplex, g.m_sim_in, 400 );
        float ta[ 16 ], tb[ 16 ];
        std::memcpy( ta, g.m_ta, 64 );
        std::memcpy( tb, g.m_tb, 64 );
        manifold_result_t r{};
        std::memcpy( r.basis0, ta, 48 );
        std::memcpy( r.origin0, ta + 12, 16 );
        std::memcpy( r.basis1, tb, 48 );
        std::memcpy( r.origin1, tb + 12, 16 );
        r.object0 = &objects[ 0 ];
        r.object1 = &objects[ 1 ];
        convex_convex_t alg{};
        alg.simplex = &simplex;
        alg.pd_solver = true;
        alg.own_manifold = true;
        alg.manifold = &m;
        alg.perturbation_iterations = 0;
        alg.minimum_points_threshold = 3;
        contact_callbacks_t cb;
        const int ret = convex_convex_process( alg, pair.shapes[ 0 ], ta, pair.shapes[ 1 ], tb, r, cb, nullptr, nullptr );
        kept += m.num_contacts > 0 ? 1 : 0;
        const char* what = nullptr;
        if ( ret != 1 )
            what = "a path not recreated was reached";
        else if ( r.manifold != &m )
            what = "result+4";
        else if ( m.num_contacts != g.m_count )
            what = "the count";
        int bad_word = -1;
        unsigned bad_ours = 0, bad_retail = 0;
        for ( int k = 0; k < 4 && !what; ++k )
        {
            unsigned ours[ 72 ];
            std::memcpy( ours, &m.points[ k ], 288 );
            for ( int w = 0; w < 72 && !what; ++w )
            {
                const bool unknown = ( w >= 37 && w < 48 ) || ( w >= 48 && w < 72 && ( w - 48 ) % 8 != 7 );
                // Word 29 (+116): the lateralFrictionInitialized BYTE is written; its three
                // padding bytes are retail's uncleared stack (seen non-zero under this
                // deeper call chain) - UNKNOWN, masked.
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
            LOG_ERROR( "convex convex vector %d (shapes %d-%d): %s (word %d: ours 0x%08X, retail 0x%08X)", static_cast< int >( index ),
                g.m_shapes[ 0 ].m_kind, g.m_shapes[ 1 ].m_kind, what, bad_word, bad_ours, bad_retail );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btConvexConvexAlgorithm::processCollision (GJK + EPA path) matches retail on %d pairs, the manifold exact "
                     "(%d kept a contact)",
            static_cast< int >( g_golden_convex_convex_count ), kept );
    return failed == 0;
}

// GJK::EncloseOrigin @0x60F8D0 after Initialize + Evaluate against retail:
// the whole 400-byte block and the bool. Counted: entry ranks, true / false.
inline bool verify_epa_enclose( )
{
    int failed = 0;
    int outcomes[ 2 ] = {};
    unsigned ranks = 0;
    for ( std::size_t index = 0; index < g_golden_epa_enclose_count; ++index )
    {
        const golden_epa_enclose_t& g = g_golden_epa_enclose[ index ];
        epa_gjk_t gjk;
        uint32_t img[ 100 ];
        int enclosed = 0, rank = -1;
        const bool ok = epa_gjk_replay( g, true, gjk, img, enclosed, rank, []( epa_gjk_t& ) { return true; } );
        if ( rank >= 0 )
            ranks |= 1u << rank;
        if ( ok )
            ++outcomes[ enclosed != 0 ];
        int bad = -1;
        for ( int k = 0; k < 100 && bad < 0; ++k )
            if ( img[ k ] != g.m_out[ k ] )
                bad = k;
        if ( !ok || bad >= 0 || gjk.status != g.m_status || enclosed != g.m_enclosed )
        {
            ++failed;
            LOG_ERROR( "epa enclose vector %d (shapes %d-%d): %s (word %d)", static_cast< int >( index ), g.m_shapes[ 0 ].m_kind,
                g.m_shapes[ 1 ].m_kind, !ok ? "a support returned false" : bad >= 0 ? "the block differs"
                : gjk.status != g.m_status ? "the status differs" : "the bool differs", bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "gjkepa2 GJK::EncloseOrigin matches retail on %d pairs, block + bool exact (true %d, false %d; "
                     "entry ranks mask 0x%X)",
            static_cast< int >( g_golden_epa_enclose_count ), outcomes[ 1 ], outcomes[ 0 ], ranks );
    return failed == 0;
}

// GJK::Evaluate @0x60EF60 against retail: our epa_gjk_t laid back out as
// retail's 400-byte block (store indices -> pointers, never-written words 0
// as in the zero-filled oracle block) and the status. Counted: final status
// and the rank of the simplex left for EPA.
inline bool verify_epa_gjk( )
{
    int failed = 0;
    int statuses[ 3 ] = {};
    unsigned ranks = 0;
    for ( std::size_t index = 0; index < g_golden_epa_gjk_count; ++index )
    {
        const golden_epa_gjk_t& g = g_golden_epa_gjk[ index ];
        float verts[ 2 ][ 12 ][ 4 ];
        striding_mesh_t meshes[ 2 ]{};
        convex_shape_t shapes[ 2 ]{};
        for ( int k = 0; k < 2; ++k )
        {
            const golden_epa_shape_t& gs = g.m_shapes[ k ];
            convex_shape_t& s = shapes[ k ];
            s.type = gs.m_kind;
            std::memcpy( s.implicit, gs.m_half, 16 );
            std::memcpy( &s.margin, &gs.m_margin, 4 );
            std::memcpy( s.local_scaling, gs.m_scaling, 16 );
            s.up_axis = gs.m_axis;
            if ( gs.m_kind == 3 )
            {
                std::memcpy( verts[ k ], gs.m_verts, sizeof( verts[ k ] ) );
                std::memcpy( meshes[ k ].scaling, gs.m_scaling, 16 );
                meshes[ k ].vertices = verts[ k ];
                meshes[ k ].num_vertices = gs.m_nv;
                meshes[ k ].indices = gs.m_ids;
                meshes[ k ].index_count = gs.m_count;
                s.mesh = &meshes[ k ];
            }
        }
        float ta[ 16 ], tb[ 16 ], guess[ 4 ];
        std::memcpy( ta, g.m_ta, 64 );
        std::memcpy( tb, g.m_tb, 64 );
        std::memcpy( guess, g.m_guess, 16 );
        epa_results_t results{};
        epa_shape_t shape{};
        epa_initialize( &shapes[ 0 ], ta, &shapes[ 1 ], tb, results, shape, true );
        epa_gjk_t gjk;
        const bool ok = epa_gjk_evaluate( gjk, shape, guess );
        // Retail's image of the block.
        uint32_t img[ 100 ] = {};
        const uint32_t base = g_golden_epa_gjk_base;
        auto ptr = [ & ]( int k ) { return k < 0 ? 0u : base + 240u + 32u * static_cast< uint32_t >( k ); };
        img[ 0 ] = g_golden_epa_gjk_shapes[ 0 ];
        img[ 1 ] = g_golden_epa_gjk_shapes[ 1 ];
        std::memcpy( &img[ 4 ], gjk.shape.toshape1, 48 );
        std::memcpy( &img[ 16 ], gjk.shape.toshape0, 64 );
        img[ 32 ] = gjk.shape.with_margin ? 0x5540B0u : 0x553980u;
        std::memcpy( &img[ 36 ], gjk.ray, 16 );
        std::memcpy( &img[ 40 ], &gjk.distance, 4 );
        for ( int s = 0; s < 2; ++s )
        {
            for ( int k = 0; k < 4; ++k )
                img[ 41 + 9 * s + k ] = ptr( gjk.simplices[ s ].c[ k ] );
            std::memcpy( &img[ 45 + 9 * s ], gjk.simplices[ s ].p, 16 );
            img[ 49 + 9 * s ] = gjk.simplices[ s ].rank;
        }
        std::memcpy( &img[ 60 ], gjk.store, 128 );
        for ( int k = 0; k < 4; ++k )
            img[ 92 + k ] = ptr( gjk.free[ k ] );
        img[ 96 ] = gjk.nfree;
        img[ 97 ] = gjk.current;
        img[ 98 ] = gjk.simplex < 0 ? 0u : base + 164u + 36u * static_cast< uint32_t >( gjk.simplex );
        img[ 99 ] = static_cast< uint32_t >( gjk.status );
        if ( gjk.status >= 0 && gjk.status < 3 )
            ++statuses[ gjk.status ];
        if ( gjk.simplex >= 0 )
            ranks |= 1u << gjk.simplices[ gjk.simplex ].rank;
        int bad = -1;
        for ( int k = 0; k < 100 && bad < 0; ++k )
            if ( img[ k ] != g.m_out[ k ] )
                bad = k;
        if ( !ok || bad >= 0 || gjk.status != g.m_status )
        {
            ++failed;
            LOG_ERROR( "epa gjk vector %d (shapes %d-%d): %s (word %d)", static_cast< int >( index ), g.m_shapes[ 0 ].m_kind,
                g.m_shapes[ 1 ].m_kind, !ok ? "a support returned false" : bad >= 0 ? "the block differs" : "the status differs", bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "gjkepa2 GJK::Evaluate matches retail on %d pairs, the whole 400-byte block exact (valid %d, inside %d, "
                     "failed %d; final ranks mask 0x%X)",
            static_cast< int >( g_golden_epa_gjk_count ), statuses[ 0 ], statuses[ 1 ], statuses[ 2 ], ranks );
    return failed == 0;
}

// world_solver_info against the btContactSolverInfo ctor @0x6060D0's own
// stores (splitImpulse excepted: VuDynamicsImpl's init sets it to 1).
inline bool verify_solver_info( )
{
    const solver_info_t info = world_solver_info( );
    auto bits = []( float f ) { unsigned u; std::memcpy( &u, &f, 4 ); return u; };
    const unsigned* g = g_golden_solver_info;
    const bool same = g[ 3 ] == 0x77777777u                         // timeStep: not written by the ctor
        && g[ 5 ] == static_cast< unsigned >( info.num_iterations )
        && g[ 8 ] == bits( info.erp )
        && g[ 11 ] == 0u && info.split_impulse == 1                    // ctor 0, Vu init 1
        && g[ 12 ] == bits( info.split_impulse_penetration_threshold )
        && g[ 13 ] == bits( info.linear_slop )
        && g[ 14 ] == bits( info.warmstarting_factor )
        && g[ 15 ] == static_cast< unsigned >( info.solver_mode )
        && g[ 16 ] == static_cast< unsigned >( info.resting_contact_restitution_threshold )
        && g[ 17 ] == static_cast< unsigned >( info.minimum_solver_batch_size );
    if ( same )
        LOG_SUCCESS( "world_solver_info matches the btContactSolverInfo ctor @0x6060D0 field for field (bits), splitImpulse from 0x4F8050" );
    else
        LOG_ERROR( "world_solver_info differs from the btContactSolverInfo ctor @0x6060D0" );
    return same;
}
