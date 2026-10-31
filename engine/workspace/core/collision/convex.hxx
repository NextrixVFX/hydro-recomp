#pragma once

#include <workspace/core/collision/polytope.hxx>
#include <workspace/core/dynamics/contact.hxx>

// btConvexConvexAlgorithm (Bullet 2.79), recreated from retail's disassembly
// of processCollision @0x5FC900 (memory/audit.md "Batch 8b"). Only the path
// the content takes is recreated - GJK + the EPA pd solver into the manifold
// result, then the owned manifold's refresh. Every other path returns -1
// (unknown) on reaching it: a missing manifold (the dispatcher's
// getNewManifold, vt+8), capsule-capsule (both types 10), the polyhedral
// SAT / clipping (both types < 7 with shape A's polyhedron set) and the
// multipoint perturbation (+28 nonzero and fewer than +32 points).

// The members processCollision reads, at retail's offsets.
struct convex_convex_t
{
    simplex_solver_t* simplex;          // +8
    bool pd_solver;                     // +12 (a btGjkEpaPenetrationDepthSolver: only non-null is read)
    bool own_manifold;                  // +16
    solver_manifold_t* manifold;        // +20
    int perturbation_iterations;        // +28 (CreateFunc default 0)
    int minimum_points_threshold;       // +32 (CreateFunc default 3)
};

// processCollision @0x5FC900 (thiscall; body0, body1, dispatchInfo,
// resultOut; retn 10h). result+4 = the manifold. The detector is built on
// the stack by 0x60DFD0 (A, B, simplex, pd; margins by getMargin); the
// input's transforms are the bodies' +16 world transforms, maxDistance2 =
// ((marginA + marginB) + manifold+1180)^2, stackAlloc = dispatchInfo+40,
// the debug drawer dispatchInfo+20; getClosestPointsNonVirtual @0x60E0B0
// into the result (addContactPoint, vt+12). Tail - btManifoldResult::
// refreshContactPoints inlined: when the manifold is owned and holds points,
// refresh 0x5B6C10 with (result+16, result+80) if the manifold's body0
// (+1168) is the result's object0 (+144), else swapped. Returns 1, or -1
// where a path not recreated is reached.
inline int convex_convex_process( convex_convex_t& alg, const convex_shape_t& a, const float* transform_a, const convex_shape_t& b,
    const float* transform_b, manifold_result_t& result, const contact_callbacks_t& cb, void* debug_draw, void* stack_alloc )
{
    if ( !alg.manifold )
        return -1;
    result.manifold = alg.manifold;
    if ( a.type == 10 && b.type == 10 )
        return -1;
    float margin_a, margin_b;
    if ( !shape_get_margin( a, margin_a ) || !shape_get_margin( b, margin_b ) )
        return -1;
    gjk_detector_t d{};
    gjk_construct( d, &a, &b, alg.simplex, alg.pd_solver, margin_a, margin_b );
    gjk_input_t input{};
    const float reach = ( margin_a + margin_b ) + alg.manifold->contact_breaking_threshold;
    input.max_distance2 = reach * reach;
    std::memcpy( input.transform_a, transform_a, 64 );
    std::memcpy( input.transform_b, transform_b, 64 );
    input.stack_alloc = stack_alloc;
    if ( a.type < 7 && b.type < 7 && a.polyhedron )
        return -1;
    struct sink_t
    {
        manifold_result_t* result;
        const contact_callbacks_t* cb;
    } sink{ &result, &cb };
    gjk_hooks_t hooks;
    hooks.user = &sink;
    hooks.pen_depth = epa_pen_depth_hook;
    hooks.add_contact = []( void* u, const float* normal, const float* point, float depth )
    {
        sink_t& s = *static_cast< sink_t* >( u );
        manifold_result_add_contact_point( *s.result, normal, point, depth, *s.cb );
    };
    if ( !gjk_closest_points( d, input, hooks, debug_draw ) )
        return -1;
    if ( alg.perturbation_iterations != 0 && result.manifold->num_contacts < alg.minimum_points_threshold )
        return -1;
    if ( alg.own_manifold )
    {
        solver_manifold_t& m = *result.manifold;
        if ( m.num_contacts != 0 )
        {
            if ( m.body0 == result.object0 )
                manifold_refresh( m, result.basis0, result.origin0, result.basis1, result.origin1, cb );
            else
                manifold_refresh( m, result.basis1, result.origin1, result.basis0, result.origin0, cb );
        }
    }
    return 1;
}
