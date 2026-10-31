#pragma once

#include <workspace/core/collision/bvh.hxx>
#include <workspace/core/collision/convex.hxx>
#include <workspace/core/collision/detector.hxx>

// btConvexConcaveCollisionAlgorithm (Bullet 2.79), recreated from retail's
// disassembly (memory/audit.md "Batch 8f"): processCollision @0x5F6CF0, the
// callback's setTimeStepAndCounters @0x5F68F0 and processTriangle @0x5F6780,
// the convex shapes' getAabb, and the BVH walk (bvh.hxx). Per triangle the
// dispatcher's matrix (findAlgorithm 0x56D660, dispatcher +0xDC) gives
// ConvexConvex for every convex type < 20 against the triangle (type 1)
// EXCEPT the sphere (8: SphereTriangle, detector.hxx).

// A transform as the bodies hold it at +16: three basis rows of four, then
// the origin (16 floats).

// btBoxShape / btCylinderShape(X / Z)::getAabb @0x547EE0 (every vtable's
// slot 1): e = implicit + getMargin; ext[r] = (e1 |b[r][1]| + e0 |b[r][0]|)
// + e2 |b[r][2]| (fabs = andps 0x6F7CC0); min = origin - ext, max = origin
// + ext, w = 0.
inline uint32_t aabb_abs_bits( float x )
{
    uint32_t u;
    std::memcpy( &u, &x, 4 );
    return u & 0x7FFFFFFFu;
}

inline float aabb_abs( float x )
{
    return support_bits( aabb_abs_bits( x ) );
}

// A cached local aabb transformed (the hull's 0x54BC60 and the BVH mesh's
// 0x60BBB0 share the form): half extent (max - min) * 0.5 (0x6F765C) +
// margin, centre (min + max) * 0.5, the centre transformed, the extent by
// |basis|; w = 0.
inline void aabb_transform_local( const float* lo, const float* hi, float m, const float* t, float* aabb_min, float* aabb_max )
{
    const float half = 0.5f;
    const float ex = ( hi[ 0 ] - lo[ 0 ] ) * half + m;
    const float ey = ( hi[ 1 ] - lo[ 1 ] ) * half + m;
    const float ez = ( hi[ 2 ] - lo[ 2 ] ) * half + m;
    const float cx = ( lo[ 0 ] + hi[ 0 ] ) * half;
    const float cy = ( lo[ 1 ] + hi[ 1 ] ) * half;
    const float cz = ( lo[ 2 ] + hi[ 2 ] ) * half;
    const float wx = ( ( cx * t[ 0 ] + cy * t[ 1 ] ) + cz * t[ 2 ] ) + t[ 12 ];
    const float wy = ( ( cy * t[ 5 ] + cx * t[ 4 ] ) + cz * t[ 6 ] ) + t[ 13 ];
    const float wz = ( ( cy * t[ 9 ] + cx * t[ 8 ] ) + cz * t[ 10 ] ) + t[ 14 ];
    const float px = ( ex * aabb_abs( t[ 0 ] ) + ey * aabb_abs( t[ 1 ] ) ) + ez * aabb_abs( t[ 2 ] );
    const float py = ( ey * aabb_abs( t[ 5 ] ) + ex * aabb_abs( t[ 4 ] ) ) + ez * aabb_abs( t[ 6 ] );
    const float pz = ( ey * aabb_abs( t[ 9 ] ) + ex * aabb_abs( t[ 8 ] ) ) + ez * aabb_abs( t[ 10 ] );
    aabb_min[ 0 ] = wx - px;
    aabb_min[ 1 ] = wy - py;
    aabb_min[ 2 ] = wz - pz;
    aabb_min[ 3 ] = 0.0f;
    aabb_max[ 0 ] = px + wx;
    aabb_max[ 1 ] = py + wy;
    aabb_max[ 2 ] = pz + wz;
    aabb_max[ 3 ] = 0.0f;
}

// btConvexShape::getAabb by type, at retail's slot 1 bodies: 0 / 13 box
// path 0x547EE0, 8 sphere 0x5534B0, 3 the hull 0x54B6D0 -> 0x54BC60. Any
// other type: false (its getAabb is not recreated).
inline bool shape_get_aabb( const convex_shape_t& s, const float* t, float* aabb_min, float* aabb_max )
{
    float m;
    switch ( s.type )
    {
    case 0: case 13:
    {
        if ( !shape_get_margin( s, m ) )
            return false;
        const float e0 = s.implicit[ 0 ] + m;
        const float e1 = s.implicit[ 1 ] + m;
        const float e2 = s.implicit[ 2 ] + m;
        float ext[ 3 ];
        for ( int r = 0; r < 3; ++r )
            ext[ r ] = ( e1 * aabb_abs( t[ 4 * r + 1 ] ) + e0 * aabb_abs( t[ 4 * r ] ) ) + e2 * aabb_abs( t[ 4 * r + 2 ] );
        for ( int r = 0; r < 3; ++r )
        {
            aabb_min[ r ] = t[ 12 + r ] - ext[ r ];
            aabb_max[ r ] = t[ 12 + r ] + ext[ r ];
        }
        aabb_min[ 3 ] = 0.0f;
        aabb_max[ 3 ] = 0.0f;
        return true;
    }
    case 8:
        // getMargin called three times (one value): min = origin - m, max = origin + m.
        if ( !shape_get_margin( s, m ) )
            return false;
        for ( int r = 0; r < 3; ++r )
        {
            aabb_min[ r ] = t[ 12 + r ] - m;
            aabb_max[ r ] = t[ 12 + r ] + m;
        }
        aabb_min[ 3 ] = 0.0f;
        aabb_max[ 3 ] = 0.0f;
        return true;
    case 3:
        // 0x54BC60 (thiscall; trans, min, max, margin) on the cached local
        // aabb (+0x50 / +0x60).
        if ( !shape_get_margin( s, m ) )
            return false;
        aabb_transform_local( s.local_aabb_min, s.local_aabb_max, m, t, aabb_min, aabb_max );
        return true;
    default:
        return false;
    }
}

// btBvhTriangleMeshShape (type 21; vt 0x751794): +12 margin (getMargin
// 0x5AF6B0 = fld [ecx+0Ch]), +0x30 the mesh interface, +0x40 the BVH;
// processAllTriangles (vt+0x38) = 0x5AF0F0.
struct bvh_mesh_shape_t
{
    int type;
    float margin;
    const striding_mesh_t* mesh;
    const quantized_bvh_t* bvh;
    // +16 / +32: the cached local aabb getAabb 0x60BBB0 reads (written by
    // btTriangleMeshShape::recalcLocalAabb 0x60BED0 - mesh_shape_recalc_local
    // _aabb below).
    float local_aabb_min[ 4 ] = {};
    float local_aabb_max[ 4 ] = {};
};

// btBvhTriangleMeshShape::getAabb @0x60BBB0 (slot 1): the cached local aabb
// with getMargin (+12) through the shared transform form.
inline void bvh_mesh_get_aabb( const bvh_mesh_shape_t& s, const float* t, float* aabb_min, float* aabb_max )
{
    aabb_transform_local( s.local_aabb_min, s.local_aabb_max, s.margin, t, aabb_min, aabb_max );
}

// btTriangleMeshShape::localGetSupportingVertex @0x60C3D0's callback
// (SupportVertexCallback, vt 0x757D98; ctor 0x60BF90: support +0x10 = 0,
// the identity transform, maxDot +0x60 = -1e18 0xDD5E0B6B, the support
// direction +0x70 = vec * basis - for the identity exactly vec):
// processTriangle 0x60C0E0 per corner in order, dot = (t.y v.y + v.x t.x) +
// t.z v.z, strictly above the best (`comiss; jbe`) -> the corner's 16 bytes.
struct mesh_support_callback_t
{
    float support[ 4 ] = {};
    float max_dot;
    float vec[ 3 ];
};

inline void mesh_support_process_triangle( void* user, float ( *t )[ 4 ], int, int )
{
    mesh_support_callback_t& c = *static_cast< mesh_support_callback_t* >( user );
    for ( int k = 0; k < 3; ++k )
    {
        const float dot = ( t[ k ][ 1 ] * c.vec[ 1 ] + c.vec[ 0 ] * t[ k ][ 0 ] ) + t[ k ][ 2 ] * c.vec[ 2 ];
        if ( dot > c.max_dot )
        {
            c.max_dot = dot;
            std::memcpy( c.support, t[ k ], 16 );
        }
    }
}

// TestTriangleAgainstAabb2 as FilteredCallback::internalProcessTriangle
// 0x60C2B0 inlines it, axis x then z then y: btMin3 / btMax3 by `comiss;
// cmova` pointer picks (min3: t1 > t0 ? t0 : t1, then t2 > that ? that : t2;
// max3: t0 > t1 ? t0 : t1, then that > t2 ? that : t2 - an unordered compare
// keeps the later corner), rejected when min3 > aabbMax or aabbMin > max3
// (`comiss; ja`).
inline bool mesh_triangle_in_aabb( const float ( *t )[ 4 ], const float* mn, const float* mx )
{
    static const int axes[ 3 ] = { 0, 2, 1 };
    for ( int axis : axes )
    {
        const float* lo = t[ 1 ][ axis ] > t[ 0 ][ axis ] ? &t[ 0 ][ axis ] : &t[ 1 ][ axis ];
        lo = t[ 2 ][ axis ] > *lo ? lo : &t[ 2 ][ axis ];
        if ( *lo > mx[ axis ] )
            return false;
        const float* hi = t[ 0 ][ axis ] > t[ 1 ][ axis ] ? &t[ 0 ][ axis ] : &t[ 1 ][ axis ];
        hi = *hi > t[ 2 ][ axis ] ? hi : &t[ 2 ][ axis ];
        if ( mn[ axis ] > *hi )
            return false;
    }
    return true;
}

// The two processAllTriangles the support reaches: while the ctor runs the
// vtable is still btTriangleMeshShape's (0x757DB4: slot 14 0x60C200 - the
// FilteredCallback (vt 0x757DA4) through the mesh's InternalProcessAllTriangles
// 0x5180A0, every face in order, corners = vertex * scaling, w = 0); after it
// the BVH shape's 0x5AF0F0 (the BVH walk). The aabb is -1e18 .. 1e18
// (0x6F7C90 / 0x6F7B10).
inline int mesh_shape_support( const bvh_mesh_shape_t& s, bool through_bvh, const float* vec, float* out )
{
    mesh_support_callback_t c;
    c.max_dot = support_bits( 0xDD5E0B6Bu );
    std::memcpy( c.vec, vec, 12 );
    const float mn[ 4 ] = { support_bits( 0xDD5E0B6Bu ), support_bits( 0xDD5E0B6Bu ), support_bits( 0xDD5E0B6Bu ), 0.0f };
    const float mx[ 4 ] = { support_bits( 0x5D5E0B6Bu ), support_bits( 0x5D5E0B6Bu ), support_bits( 0x5D5E0B6Bu ), 0.0f };
    if ( through_bvh )
    {
        const bvh_triangle_callback_t cb{ &c, mesh_support_process_triangle };
        if ( !s.bvh || bvh_mesh_process_all_triangles( *s.bvh, *s.mesh, cb, mn, mx ) < 0 )
            return -1;
    }
    else
    {
        const striding_mesh_t& mesh = *s.mesh;
        const int parts = mesh.index_count != 0 ? 1 : 0;
        for ( int part = 0; part < parts; ++part )
        {
            const int faces = mesh.index_count / 3;
            for ( int f = 0; f < faces; ++f )
            {
                const uint16_t* tri = mesh.indices + 3 * f;
                float corner[ 3 ][ 4 ];
                for ( int k = 0; k < 3; ++k )
                {
                    const float* v = mesh.vertices[ tri[ k ] ];
                    corner[ k ][ 0 ] = v[ 0 ] * mesh.scaling[ 0 ];
                    corner[ k ][ 1 ] = v[ 1 ] * mesh.scaling[ 1 ];
                    corner[ k ][ 2 ] = v[ 2 ] * mesh.scaling[ 2 ];
                    corner[ k ][ 3 ] = 0.0f;
                }
                if ( mesh_triangle_in_aabb( corner, mn, mx ) )
                    mesh_support_process_triangle( &c, corner, part, f );
            }
        }
    }
    std::memcpy( out, c.support, 16 );
    return 1;
}

// btTriangleMeshShape::recalcLocalAabb @0x60BED0: per axis i a zeroed
// direction with lane i = 1 -> localGetSupportingVertex (vt+0x3C 0x60C3D0) ->
// max[i] = s[i] + margin (+0xC); lane i = -1 -> min[i] = s[i] - margin. The
// w lanes are not written.
inline int mesh_shape_recalc_local_aabb( bvh_mesh_shape_t& s, bool through_bvh )
{
    for ( int i = 0; i < 3; ++i )
    {
        float vec[ 4 ] = {}, sup[ 4 ];
        vec[ i ] = 1.0f;
        if ( mesh_shape_support( s, through_bvh, vec, sup ) < 0 )
            return -1;
        s.local_aabb_max[ i ] = sup[ i ] + s.margin;
        vec[ i ] = -1.0f;
        if ( mesh_shape_support( s, through_bvh, vec, sup ) < 0 )
            return -1;
        s.local_aabb_min[ i ] = sup[ i ] - s.margin;
    }
    return 1;
}

// The ctor 0x5AEB60 as Vu calls it (0x544577; thiscall mesh, 2 more args;
// retn 0Ch): +8 / +0xC (the margin) = 0, the mesh at +0x30, type 21, the
// mesh's hasPremadeAabb (vt+0x24 - the VuStridingMesh's 0x4052D0 returns
// 0) -> recalcLocalAabb on btTriangleMeshShape's vtable; then the BVH
// vtable, bvh +0x40 = 0, +0x44 = 0, useQuantized 1 / ownsBvh 0. No BVH is
// built: Vu's setOptimizedBvh supplies it.
inline int bvh_mesh_shape_init( bvh_mesh_shape_t& s, const striding_mesh_t* mesh )
{
    s = bvh_mesh_shape_t{ 21, 0.0f, mesh, nullptr };
    return mesh_shape_recalc_local_aabb( s, false );
}

// setOptimizedBvh @0x5AF400 (thiscall bvh, scaling*; retn 8): bvh +0x40,
// ownsBvh 0; d = ((dy^2 + dx^2) + dz^2) of getLocalScaling (the mesh's +16)
// - scaling; `comiss d, 1.1920929e-7 (0x6F7584); jbe` -> else the 16 bytes
// into the mesh's scaling and recalcLocalAabb, now through the BVH walk.
// Vu passes (1, 1, 1, 0) (0x6F7B00).
inline int bvh_mesh_set_optimized_bvh( bvh_mesh_shape_t& s, striding_mesh_t& mesh, const quantized_bvh_t* bvh, const float* scaling )
{
    s.bvh = bvh;
    const float dx = mesh.scaling[ 0 ] - scaling[ 0 ];
    const float dy = mesh.scaling[ 1 ] - scaling[ 1 ];
    const float dz = mesh.scaling[ 2 ] - scaling[ 2 ];
    const float d = ( dy * dy + dx * dx ) + dz * dz;
    if ( d > support_bits( 0x34000000u ) )
    {
        std::memcpy( mesh.scaling, scaling, 16 );
        return mesh_shape_recalc_local_aabb( s, true );
    }
    return 1;
}

struct compound_shape_t;

// One collision object as processCollision reads it: +16 the world
// transform, +204 the shape (a convex, the BVH mesh or a compound), +196
// the contact processing threshold, the object itself for the manifold /
// result identity.
struct concave_body_t
{
    const float* transform;
    const convex_shape_t* convex;
    const bvh_mesh_shape_t* concave;
    solver_rigid_t* object;
    const compound_shape_t* compound = nullptr;
    float processing_threshold = 0.0f;
};

// The shape's type (+4): a convex's own, the BVH mesh's 21, the compound's 31
// (ctor 0x5269C0).
inline int body_shape_type( const concave_body_t& b )
{
    return b.convex ? b.convex->type : b.concave ? b.concave->type : 31;
}

// The algorithm's members, at retail's offsets: +8 isSwapped; the
// btConvexTriangleCallback at +16 (+0x10 / +0x20 its aabb, +0x3C the margin,
// +0x44 = alg +0x54 the manifold). The per-triangle ConvexConvex takes
// ConvexConvex's CreateFunc 0x5FBCD0 values: simplex (cf+12), pd (cf+8),
// ownManifold 0, the SHARED manifold, cf+16 / cf+20 (config defaults 0 / 3).
struct convex_concave_t
{
    bool swapped;
    solver_manifold_t* manifold;
    simplex_solver_t* simplex;
    bool pd_solver;
    int perturbation_iterations;
    int minimum_points_threshold;
    float aabb_min[ 4 ];   // callback +0x10
    float aabb_max[ 4 ];   // callback +0x20
    float margin;          // callback +0x3C
};

// setTimeStepAndCounters @0x5F68F0 (ecx = callback; XMM1 = margin; stack
// dispatchInfo, result; retn 8): the convex transform in the triangle body's
// frame, inverse(tri) * convex, from the disassembly's own order:
//   i = basis^T * -origin(tri):  i[k] = (T[4+k] n1 + T[k] n0) + T[8+k] n2
//   origin[k] = ((C12 T[k] + C13 T[4+k]) + C14 T[8+k]) + i[k]
//   M[r][j] = (T[r] C[j] + T[4+r] C[4+j]) + T[8+r] C[8+j]   (operand order kept)
// then the convex shape's getAabb (vt+4) into the callback's aabb, max +=
// margin, min -= margin (lanes 0..2).
inline bool concave_set_time_step( convex_concave_t& alg, const convex_shape_t& convex, const float* c, const float* t, float margin )
{
    alg.margin = margin;
    const float n0 = support_negate( t[ 12 ] );
    const float n1 = support_negate( t[ 13 ] );
    const float n2 = support_negate( t[ 14 ] );
    float i[ 3 ];
    for ( int k = 0; k < 3; ++k )
        i[ k ] = ( t[ 4 + k ] * n1 + t[ k ] * n0 ) + t[ 8 + k ] * n2;
    float x[ 16 ];
    for ( int k = 0; k < 3; ++k )
        x[ 12 + k ] = ( ( c[ 12 ] * t[ k ] + c[ 13 ] * t[ 4 + k ] ) + c[ 14 ] * t[ 8 + k ] ) + i[ k ];
    x[ 15 ] = 0.0f;
    for ( int r = 0; r < 3; ++r )
    {
        x[ 4 * r + 0 ] = ( t[ r ] * c[ 0 ] + t[ 4 + r ] * c[ 4 ] ) + t[ 8 + r ] * c[ 8 ];
        x[ 4 * r + 1 ] = ( t[ r ] * c[ 1 ] + t[ 4 + r ] * c[ 5 ] ) + t[ 8 + r ] * c[ 9 ];
        x[ 4 * r + 2 ] = ( t[ r ] * c[ 2 ] + c[ 6 ] * t[ 4 + r ] ) + t[ 8 + r ] * c[ 10 ];
        x[ 4 * r + 3 ] = 0.0f;
    }
    if ( !shape_get_aabb( convex, x, alg.aabb_min, alg.aabb_max ) )
        return false;
    for ( int k = 0; k < 3; ++k )
        alg.aabb_max[ k ] = alg.aabb_max[ k ] + margin;
    for ( int k = 0; k < 3; ++k )
        alg.aabb_min[ k ] = alg.aabb_min[ k ] - margin;
    return true;
}

// processTriangle @0x5F6780 (thiscall callback; triangle*, part, index;
// retn 0Ch), per triangle the walk reports: proceed only if the convex
// type < 20 (signed); a btTriangleShape on the stack (ctor 0x5B7F10: type 1,
// scaling (1, 1, 1, 0), margin 0.04 then the callback's margin at +48, the
// vertices at +80, polyhedron 0) REPLACES the triangle body's shape (+204)
// for the call; findAlgorithm(convex body, tri body, the shared manifold);
// the result's shape identifiers A (+152 / +160) when the result's object0 is
// the triangle body, else B (+156 / +164); processCollision(convex body,
// tri body, dispatchInfo, result); the algorithm's dtor (ownManifold 0:
// nothing) and the dispatcher's freeCollisionAlgorithm 0x56D9C0 (pool
// bookkeeping only); the shape restored.
struct concave_walk_t
{
    convex_concave_t* alg;
    const concave_body_t* convex;
    const concave_body_t* tri;
    manifold_result_t* result;
    const contact_callbacks_t* cb;
    void* debug_draw;
    void* stack_alloc;
    int status;
};

inline void concave_process_triangle( void* user, float ( *triangle )[ 4 ], int part, int index )
{
    concave_walk_t& w = *static_cast< concave_walk_t* >( user );
    const convex_shape_t& convex = *w.convex->convex;
    if ( !( convex.type < 20 ) || w.status < 0 )
        return;
    convex_shape_t tri{};
    tri.type = 1;
    tri.local_scaling[ 0 ] = tri.local_scaling[ 1 ] = tri.local_scaling[ 2 ] = 1.0f;
    tri.margin = w.alg->margin;
    std::memcpy( tri.vertices, triangle, 48 );
    if ( w.result->object0 == w.tri->object )
    {
        w.result->part_id0 = part;
        w.result->index0 = index;
    }
    else
    {
        w.result->part_id1 = part;
        w.result->index1 = index;
    }
    if ( convex.type == 8 )
    {
        // The matrix's (8, 1): SphereTriangle (CreateFunc vt 0x74F5D4, slot 1
        // 0x56C770: the shared manifold, ownManifold 0, isSwapped = the
        // CreateFunc's +4 - 0 for (8, 1)); processCollision 0x5FB9E0 with the
        // sphere = body0: the detector {the sphere, the triangle, the
        // manifold's breaking threshold +1180} over the two bodies' world
        // transforms, maxDistance2 1e18 (unread); no refresh (not owned); the
        // dtor releases nothing.
        detector_get_closest_points( convex, tri, w.alg->manifold->contact_breaking_threshold, w.convex->transform, w.tri->transform, false,
            *w.result, *w.cb );
        return;
    }
    convex_convex_t a{};
    a.simplex = w.alg->simplex;
    a.pd_solver = w.alg->pd_solver;
    a.own_manifold = false;
    a.manifold = w.alg->manifold;
    a.perturbation_iterations = w.alg->perturbation_iterations;
    a.minimum_points_threshold = w.alg->minimum_points_threshold;
    if ( convex_convex_process( a, convex, w.convex->transform, tri, w.tri->transform, *w.result, *w.cb, w.debug_draw, w.stack_alloc ) < 0 )
        w.status = -1;
}

// processCollision @0x5F6CF0 (thiscall alg; body0, body1, dispatchInfo,
// resultOut; retn 10h): the convex / triangle bodies by isSwapped; proceed
// only if (triType - 21) <= 8 UNSIGNED and convexType < 20 (signed); margin
// = the concave shape's getMargin (vt+0x28); result+4 = the manifold;
// setTimeStepAndCounters; the manifold's bodies (+1172 tri, +1168 convex);
// the concave shape's processAllTriangles (vt+0x38) over the callback's
// aabb; then the refresh INLINED with NO ownManifold check: when the
// manifold holds points, refresh 0x5B6C10 with (result+16, result+80) if
// its body0 is the result's object0, else swapped. Returns 1, 0 when the
// type test rejects, -1 where code not recreated is reached.
inline int convex_concave_process( convex_concave_t& alg, const concave_body_t& body0, const concave_body_t& body1,
    manifold_result_t& result, const contact_callbacks_t& cb, void* debug_draw, void* stack_alloc )
{
    const concave_body_t& convex = alg.swapped ? body1 : body0;
    const concave_body_t& tri = alg.swapped ? body0 : body1;
    const int tri_type = body_shape_type( tri );
    const int convex_type = body_shape_type( convex );
    if ( static_cast< unsigned >( tri_type - 21 ) > 8u || !( convex_type < 20 ) )
        return 0;
    if ( tri.concave->type != 21 )
        return -1;   // another concave shape's getMargin / processAllTriangles
    const float margin = tri.concave->margin;
    result.manifold = alg.manifold;
    if ( !concave_set_time_step( alg, *convex.convex, convex.transform, tri.transform, margin ) )
        return -1;
    alg.manifold->body1 = tri.object;
    alg.manifold->body0 = convex.object;
    concave_walk_t walk{ &alg, &convex, &tri, &result, &cb, debug_draw, stack_alloc, 1 };
    const bvh_triangle_callback_t callback{ &walk, concave_process_triangle };
    if ( bvh_mesh_process_all_triangles( *tri.concave->bvh, *tri.concave->mesh, callback, alg.aabb_min, alg.aabb_max ) < 0 )
        return -1;
    if ( walk.status < 0 )
        return -1;
    solver_manifold_t& m = *result.manifold;
    if ( m.num_contacts != 0 )
    {
        if ( m.body0 == result.object0 )
            manifold_refresh( m, result.basis0, result.origin0, result.basis1, result.origin1, cb );
        else
            manifold_refresh( m, result.basis1, result.origin1, result.basis0, result.origin0, cb );
    }
    return 1;
}
