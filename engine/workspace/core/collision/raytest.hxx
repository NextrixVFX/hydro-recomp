#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <workspace/core/collision/world.hxx>
#include <workspace/core/collision/compound.hxx>
#include <workspace/core/collision/simplex.hxx>
#include <workspace/core/collision/box.hxx>
#include <workspace/core/collision/cylinder.hxx>
#include <workspace/core/collision/sphere.hxx>

// btCollisionWorld::rayTest (Bullet 2.79) as the race reaches it - the camera's
// collision 0x40B090 and the AI's avoidance 0x40C750 through VuDynamicsRayTest
// 0x50CFB0: g_vudynamicsimpl (0x75900C) vt+0x6C (0x4F8940: the world, +0x34)
// -> the world's vt+0x18 rayTest 0x54F590 - recreated from retail's
// disassembly (boat batch 15):
//   0x54F590 rayTest: the btSingleRayCallback (ctor 0x54F330, vtable 0x74E764)
//     on the stack, the broadphase's (world +0x4C) vt+0x14 rayTest with a zero
//     aabbMin / aabbMax;
//   0x56E040 btDbvtBroadphase::rayTest: m_sets[0] (+4), then m_sets[1] (+44),
//     each with a root, through 0x56EE90 btDbvt::rayTestInternal;
//   0x54F4E0 btSingleRayCallback::process (slot 1): nothing once the result's
//     closestHitFraction (+4) is 0; its needsCollision (vt+4) on the proxy
//     (object +0xC8), then rayTestSingle(the ray's transforms, the object,
//     its shape +0xCC, its world transform +0x10, the result);
//   0x54C9B0 rayTestSingle: a convex (type < 20) through btSubsimplexConvex
//     Cast 0x5B6FC0 with a radius-0 btSphereShape (vt 0x73ADD4) and a stack
//     btVoronoiSimplexSolver (threshold 0x38D1B717); a compound (31) through
//     the child tree's btDbvt::rayTest 0x551F10 -> RayTester::Process
//     0x54D700 (the child's world transform, the object's shape swapped for
//     the child's, LocalInfoAdder2 vt 0x74E84C: needsCollision 0x54D690 /
//     addSingleResult 0x54D6A0); the BVH mesh (21) through performRaycast
//     0x5AECB0 (boat batch 16, below) - the result's addSingleResult answer
//     IS read there (the Bridge callback's hitFraction).
// The result callback is the caller's (btCollisionWorld::RayResultCallback:
// +4 closestHitFraction, +8 collisionObject, +0xC group, +0xE mask, +0x10
// flags; vt+4 needsCollision, vt+8 addSingleResult). Every path not recreated
// returns -1 (UNKNOWN) and stops the test where retail would have gone on.

struct ray_shape_info_t   // btCollisionWorld::LocalShapeInfo
{
    int part;
    int triangle;
};

struct ray_local_result_t   // btCollisionWorld::LocalRayResult: +0 object, +4 shape info, +0x10 normal, +0x20 fraction
{
    collision_object_t* object = nullptr;
    const ray_shape_info_t* info = nullptr;
    float normal[ 4 ] = {};
    float fraction = 0.0f;
};

struct ray_callback_t
{
    float closest_hit_fraction = 1.0f;   // +4
    const collision_object_t* object = nullptr;   // +8 (nothing in rayTest writes it)
    short group = 1;                      // +0xC
    short mask = -1;                      // +0xE
    std::uint32_t flags = 0;              // +0x10
    void* user = nullptr;
    // vt+4. Null: the base RayResultCallback::needsCollision 0x50D0C0 -
    // (proxy group & mask) && (group & proxy mask), as shorts.
    bool ( *needs_collision )( void* user, ray_callback_t& self, const broadphase_proxy_t& proxy ) = nullptr;
    // vt+8 (thiscall; LocalRayResult&, bool normalInWorldSpace; the float on
    // x87). Required.
    float ( *add_single_result )( void* user, ray_callback_t& self, ray_local_result_t& r, bool normal_in_world ) = nullptr;
};

inline bool ray_needs_collision( ray_callback_t& cb, const broadphase_proxy_t& proxy )
{
    if ( cb.needs_collision )
        return cb.needs_collision( cb.user, cb, proxy );
    return ( proxy.group & cb.mask ) != 0 && ( cb.group & proxy.mask ) != 0;
}

// ---- The shapes' localGetSupportingVertex (vt+0x38, slot 14) --------------
//
// On the shape's bytes as the engine's support code reads them (+0x10
// scaling, +0x20 implicit dimensions, +0x30 margin, +0x40 the up axis).
inline void ray_shape_image( const convex_shape_t& s, std::uint8_t* img )
{
    std::memset( img, 0, 0x50 );
    std::memcpy( img + 0x10, s.local_scaling, 16 );
    std::memcpy( img + 0x20, s.implicit, 16 );
    std::memcpy( img + 0x30, &s.margin, 4 );
    std::memcpy( img + 0x40, &s.up_axis, 4 );
}

// btConvexTriangleMeshShape (type 3) slot 14 0x526820: slot 15 0x526570
// (support_triangle_mesh) into out, then - when getMargin (vt+0x28: +0x30 on
// x87) is not exactly 0 (`ucomiss; lahf; test ah, 44h; jnp`: NaN applies) -
// vec = dir, (-1, -1, -1) (0x6F78A0) when 1.42e-14 (0x6F7578) > (y^2 + x^2) +
// z^2; r = 1 / sqrtss((y^2 + x^2) + z^2); out += vec r margin (the margin read
// again), lanes 0 - 2 (w is slot 15's).
inline void ray_mesh_hull_support( const convex_shape_t& s, const std::uint8_t* img, const float* dir, float* out )
{
    support_triangle_mesh( *s.mesh, dir, out );
    const float margin = box_margin( img );
    if ( margin == 0.0f )
        return;
    float x = dir[ 0 ], y = dir[ 1 ], z = dir[ 2 ];
    if ( support_bits( 0x28800000u ) > ( y * y + x * x ) + z * z )
        x = y = z = -1.0f;
    const float r = 1.0f / std::sqrt( ( y * y + x * x ) + z * z );
    const float vy = y * r, vz = z * r, vx = r * x;
    const float m = box_margin( img );
    out[ 1 ] = vy * m + out[ 1 ];
    out[ 2 ] = vz * m + out[ 2 ];
    out[ 0 ] = out[ 0 ] + vx * m;
}

// The four the content gives a body (box 0x548840, sphere 0x553360, cylinder
// 0x5AE950, the hull above); any other type: false (not recreated).
inline bool ray_support( const convex_shape_t& s, const float* dir, float* out )
{
    alignas( 16 ) std::uint8_t img[ 0x50 ];
    ray_shape_image( s, img );
    switch ( s.type )
    {
    case 0:
        box_support( img, dir, out );
        return true;
    case 8:
        sphere_support( img, dir, out );
        return true;
    case 13:
        cylinder_support( img, cylinder_axes( s.up_axis ), dir, out );
        return true;
    case 3:
        if ( !s.mesh )
            return false;
        ray_mesh_hull_support( s, img, dir, out );
        return true;
    default:
        return false;
    }
}

// ---- btSubsimplexConvexCast::calcTimeOfImpact 0x5B6FC0 -------------------
//
// (thiscall caster: +4 the simplex solver, +8 convexA, +0xC convexB; fromA,
// toA, fromB, toB, result; retn 14h) - result +0x90 normal, +0xA0 hit point,
// +0xB0 fraction, +0xB8 allowedPenetration. Transliterated operation for
// operation (the sums' association as retail's; the compares as its `comiss`
// branches).
struct ray_cast_result_t
{
    float normal[ 4 ] = {};
    float hit_point[ 4 ] = {};
    float fraction = 0.0f;
    float allowed_penetration = 0.0f;
};

// v * basis (Bullet's operator*(v, M): the basis' columns).
inline void ray_times_basis( const float* v, const float* b, float* out )
{
    for ( int c = 0; c < 3; ++c )
        out[ c ] = ( b[ 4 + c ] * v[ 1 ] + b[ c ] * v[ 0 ] ) + b[ 8 + c ] * v[ 2 ];
    out[ 3 ] = 0.0f;
}

// t(p) = basis p + origin.
inline void ray_transform_point( const float* t, const float* p, float* out )
{
    for ( int r = 0; r < 3; ++r )
        out[ r ] = ( ( t[ 4 * r ] * p[ 0 ] + t[ 4 * r + 1 ] * p[ 1 ] ) + p[ 2 ] * t[ 4 * r + 2 ] ) + t[ 12 + r ];
    out[ 3 ] = 0.0f;
}

// -1 a support not recreated, 0 false, 1 true.
inline int ray_subsimplex_cast( simplex_solver_t& solver, const convex_shape_t& a, const convex_shape_t& b, const float* from_a,
                                const float* to_a, const float* from_b, const float* to_b, ray_cast_result_t& result )
{
    simplex_reset( solver );
    float lambda = 0.0f;
    float ia[ 16 ], ib[ 16 ];   // interpolatedTransA / B
    std::memcpy( ia, from_a, 64 );
    std::memcpy( ib, from_b, 64 );
    const float lay = to_a[ 13 ] - from_a[ 13 ], laz = to_a[ 14 ] - from_a[ 14 ], lax = to_a[ 12 ] - from_a[ 12 ];
    const float lby = to_b[ 13 ] - from_b[ 13 ], lbz = to_b[ 14 ] - from_b[ 14 ], lbx = to_b[ 12 ] - from_b[ 12 ];
    const float r[ 3 ] = { lax - lbx, lay - lby, laz - lbz };
    float d[ 4 ], s[ 4 ], sa[ 4 ], sb[ 4 ];
    {
        const float nr[ 3 ] = { support_negate( r[ 0 ] ), support_negate( r[ 1 ] ), support_negate( r[ 2 ] ) };
        ray_times_basis( nr, from_a, d );
        if ( !ray_support( a, d, s ) )
            return -1;
        ray_transform_point( from_a, s, sa );
        ray_times_basis( r, from_b, d );
        if ( !ray_support( b, d, s ) )
            return -1;
        ray_transform_point( from_b, s, sb );
    }
    float v[ 4 ] = { sa[ 0 ] - sb[ 0 ], sa[ 1 ] - sb[ 1 ], sa[ 2 ] - sb[ 2 ], 0.0f };
    float n[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int max_iter = 32;
    if ( ( v[ 1 ] * v[ 1 ] + v[ 0 ] * v[ 0 ] ) + v[ 2 ] * v[ 2 ] > support_bits( 0x38D1B717u ) )
    {
        for ( ;; )
        {
            if ( max_iter-- == 0 )
                break;
            const float nv[ 3 ] = { support_negate( v[ 0 ] ), support_negate( v[ 1 ] ), support_negate( v[ 2 ] ) };
            ray_times_basis( nv, ia, d );
            if ( !ray_support( a, d, s ) )
                return -1;
            ray_transform_point( ia, s, sa );
            ray_times_basis( v, ib, d );
            if ( !ray_support( b, d, s ) )
                return -1;
            ray_transform_point( ib, s, sb );
            float w[ 4 ] = { sa[ 0 ] - sb[ 0 ], sa[ 1 ] - sb[ 1 ], sa[ 2 ] - sb[ 2 ], 0.0f };
            const float vdotw = ( w[ 1 ] * v[ 1 ] + w[ 0 ] * v[ 0 ] ) + w[ 2 ] * v[ 2 ];
            if ( lambda > 1.0f )
                return 0;
            if ( vdotw > 0.0f )
            {
                const float vdotr = ( r[ 1 ] * v[ 1 ] + r[ 0 ] * v[ 0 ] ) + r[ 2 ] * v[ 2 ];
                if ( vdotr >= support_bits( 0xA8800000u ) )   // -1.42e-14 (0x6F786C), `jae`
                    return 0;
                lambda = lambda - vdotw / vdotr;
                const float k = 1.0f - lambda;
                ia[ 12 ] = k * from_a[ 12 ] + lambda * to_a[ 12 ];
                ia[ 13 ] = to_a[ 13 ] * lambda + k * from_a[ 13 ];
                ia[ 14 ] = k * from_a[ 14 ] + to_a[ 14 ] * lambda;
                ib[ 12 ] = k * from_b[ 12 ] + to_b[ 12 ] * lambda;
                ib[ 13 ] = k * from_b[ 13 ] + to_b[ 13 ] * lambda;
                ib[ 14 ] = to_b[ 14 ] * lambda + k * from_b[ 14 ];
                std::memcpy( n, v, 16 );
            }
            if ( !simplex_in_simplex( solver, w ) )
            {
                // addVertex, inlined: lastW, needsUpdate, W / P / Q [count], ++count.
                std::memcpy( solver.last_w, w, 16 );
                solver.needs_update = 1;
                std::memcpy( solver.w[ solver.num_vertices ], w, 16 );
                std::memcpy( solver.p[ solver.num_vertices ], sa, 16 );
                std::memcpy( solver.q[ solver.num_vertices ], sb, 16 );
                ++solver.num_vertices;
            }
            const bool ok = simplex_update( solver );
            std::memcpy( v, solver.cached_v, 16 );
            if ( !ok )
                break;
            if ( !( ( v[ 1 ] * v[ 1 ] + v[ 0 ] * v[ 0 ] ) + v[ 2 ] * v[ 2 ] > support_bits( 0x38D1B717u ) ) )
                break;
        }
    }
    result.fraction = lambda;
    const float n2 = ( n[ 1 ] * n[ 1 ] + n[ 0 ] * n[ 0 ] ) + n[ 2 ] * n[ 2 ];
    if ( !( n2 >= support_bits( 0x28800000u ) ) )   // `jb` - a NaN takes the zero normal
    {
        result.normal[ 0 ] = result.normal[ 1 ] = result.normal[ 2 ] = result.normal[ 3 ] = 0.0f;
    }
    else
    {
        const float inv = 1.0f / std::sqrt( n2 );
        result.normal[ 0 ] = inv * n[ 0 ];
        result.normal[ 1 ] = inv * n[ 1 ];
        result.normal[ 2 ] = inv * n[ 2 ];
        result.normal[ 3 ] = 0.0f;
    }
    if ( ( result.normal[ 1 ] * r[ 1 ] + r[ 0 ] * result.normal[ 0 ] ) + result.normal[ 2 ] * r[ 2 ] >= support_negate( result.allowed_penetration ) )
        return 0;
    simplex_update( solver );   // compute_points
    std::memcpy( result.hit_point, solver.cached_p2, 16 );
    return 1;
}

// ---- rayTestSingle 0x54C9B0 -------------------------------------------------

// The shape at the object's +0xCC as rayTestSingle is handed it.
struct ray_shape_t
{
    const convex_shape_t* convex = nullptr;
    const bvh_mesh_shape_t* mesh = nullptr;
    const compound_shape_t* compound = nullptr;
};

inline int ray_shape_type( const ray_shape_t& s )
{
    return s.convex ? s.convex->type : s.mesh ? s.mesh->type : s.compound ? 31 : -1;
}

// The child's world transform 0x54D700 computes: basis = C.basis K.basis
// ((C[r][0] K[0][c] + C[r][1] K[1][c]) + C[r][2] K[2][c]), origin = C.basis
// K.origin + C.origin, w lanes 0.
inline void ray_child_world( const float* c, const float* k, float* out )
{
    for ( int r = 0; r < 3; ++r )
    {
        for ( int col = 0; col < 3; ++col )
            out[ 4 * r + col ] = ( c[ 4 * r ] * k[ col ] + c[ 4 * r + 1 ] * k[ 4 + col ] ) + c[ 4 * r + 2 ] * k[ 8 + col ];
        out[ 4 * r + 3 ] = 0.0f;
        out[ 12 + r ] = ( ( c[ 4 * r ] * k[ 12 ] + c[ 4 * r + 1 ] * k[ 13 ] ) + k[ 14 ] * c[ 4 * r + 2 ] ) + c[ 12 + r ];
    }
    out[ 15 ] = 0.0f;
}

// colObjWorldTransform.inverseTimes(t).getOrigin() as 0x54D267 inlines it:
// d = t.o - C.o; x = (C[1][0] dy + C[0][0] dx) + C[2][0] dz, y = (dy C[1][1] +
// C[0][1] dx) + dz C[2][1], z = (dy C[1][2] + dx C[0][2]) + dz C[2][2], w 0.
inline void ray_local_origin( const float* c, const float* t, float* out )
{
    const float dy = t[ 13 ] - c[ 13 ], dx = t[ 12 ] - c[ 12 ], dz = t[ 14 ] - c[ 14 ];
    out[ 0 ] = ( c[ 4 ] * dy + c[ 0 ] * dx ) + c[ 8 ] * dz;
    out[ 1 ] = ( dy * c[ 5 ] + c[ 1 ] * dx ) + dz * c[ 9 ];
    out[ 2 ] = ( dy * c[ 6 ] + dx * c[ 2 ] ) + dz * c[ 10 ];
    out[ 3 ] = 0.0f;
}

// The ray-box test btDbvt::rayTestInternal 0x56EE90 and btDbvt::rayTest
// 0x551F10 inline (btRayAabb2): per axis tmin = (bounds[sign] - from) inv,
// tmax = (bounds[1 - sign] - from) inv; reject when tmin > tymax or tymin >
// tmax (`comiss; ja`); tmin = max, tmax = min (`comiss; jbe`); hit when
// lambda_max > tmin and tmax > 0 (both `comiss; jbe`).
inline bool ray_aabb( const float* from, const float* inv, const unsigned* signs, const float* lo, const float* hi, float lambda_max )
{
    const float* bounds[ 2 ] = { lo, hi };
    float tmin = ( bounds[ signs[ 0 ] ][ 0 ] - from[ 0 ] ) * inv[ 0 ];
    float tmax = ( bounds[ 1 - signs[ 0 ] ][ 0 ] - from[ 0 ] ) * inv[ 0 ];
    for ( int k = 1; k < 3; ++k )
    {
        const float t0 = ( bounds[ signs[ k ] ][ k ] - from[ k ] ) * inv[ k ];
        const float t1 = ( bounds[ 1 - signs[ k ] ][ k ] - from[ k ] ) * inv[ k ];
        if ( tmin > t1 || t0 > tmax )
            return false;
        if ( t0 > tmin )
            tmin = t0;
        if ( tmax > t1 )
            tmax = t1;
    }
    return lambda_max > tmin && tmax > 0.0f;
}

// The ray's direction terms as the btSingleRayCallback ctor 0x54F330 and
// btDbvt::rayTest 0x551F10 build them: d = to - from, n = d (1 / sqrtss(len2))
// (len2's association per site), inv = 1e18 (0x6F7858) for an exactly-zero
// lane (`ucomiss; lahf; test ah, 44h; jp` - NaN divides) else 1 / n, sign = 0
// > inv (`comiss; seta`).
struct ray_dir_t
{
    float inv[ 3 ];
    unsigned signs[ 3 ];
    float lambda_max;
};

inline float ray_inverse( float n )
{
    return n == 0.0f ? support_bits( 0x5D5E0B6Bu ) : 1.0f / n;
}

// ---- The BVH mesh's ray path (boat batch 16) ------------------------------
//
// rayTestSingle's type-21 branch (0x54CCC0 .. 0x54CF15): the ray in the
// mesh's frame, a BridgeTriangleRaycastCallback (ctor 0x54D410, vtable
// 0x74E85C: slot 1 processTriangle 0x5B8500, slot 2 reportHit 0x54D4D0) and
// btBvhTriangleMeshShape::performRaycast 0x5AECB0 -> the quantized ray walk
// 0x587E60 -> MyNodeOverlapCallback::processNode 0x5AED70 (bvh.hxx
// mesh_process_node: the same lock, corner order and scaling products).
// The callback (at retail's offsets): +0x10 from, +0x20 to (the local ray),
// +0x30 flags (the result's +0x10), +0x34 hitFraction (1.0 in the ctor,
// then the result's closestHitFraction +4), +0x40 the result, +0x44 the
// object, +0x48 the shape, +0x50 the object's world transform (64 b).
struct ray_triangle_t
{
    float from[ 4 ];
    float to[ 4 ];
    std::uint32_t flags;
    float hit_fraction;
    ray_callback_t* result;
    collision_object_t* object;
    const float* world;
};

// btTriangleRaycastCallback::reportHit as the Bridge overrides it 0x54D4D0
// (thiscall; hitNormalLocal*, hitFraction, partId, triangleIndex; retn 10h):
// LocalShapeInfo {part, triangle}; the normal by the world basis rows
// (+0x50 / +0x60 / +0x70): r = (b[r][1] n.y + n.x b[r][0]) + b[r][2] n.z, w 0;
// LocalRayResult {object +0x44, the info, the normal, the fraction}; the
// result's addSingleResult(r, true) - its x87 answer returned.
inline float ray_triangle_report( ray_triangle_t& c, const float* n, float fraction, int part, int index )
{
    const ray_shape_info_t info{ part, index };
    const float* b = c.world;
    ray_local_result_t r;
    r.object = c.object;
    r.info = &info;
    for ( int row = 0; row < 3; ++row )
        r.normal[ row ] = ( b[ 4 * row + 1 ] * n[ 1 ] + n[ 0 ] * b[ 4 * row ] ) + b[ 4 * row + 2 ] * n[ 2 ];
    r.normal[ 3 ] = 0.0f;
    r.fraction = fraction;
    return c.result->add_single_result( c.result->user, *c.result, r, true );
}

// btTriangleRaycastCallback::processTriangle 0x5B8500 (thiscall; triangle*,
// partId, triangleIndex; retn 0Ch), from its disassembly in operand order:
// v10 = v1 - v0, v20 = v2 - v0, n = (v20.z v10.y - v20.y v10.z, v20.x v10.z -
// v20.z v10.x, v20.y v10.x - v20.x v10.y); dist = (n.y v0.y + v0.x n.x) + n.z
// v0.z; dist_a / dist_b the same form on from / to, minus dist. Leaves
// unless dist_b dist_a < 0 (`comiss; jnb` - a NaN product goes on); flag 1
// leaves when dist_a > 0 (`ja`); d = dist_a / (dist_a - dist_b), on only when
// hitFraction > d (`jbe`); tolerance = ((n.x^2 + n.y^2) + n.z^2) x -1e-4
// (0x6F7878); point = from (1 - d) + to d; the three edge tests (cp . n =
// (cp.y n.y + cp.x n.x) + cp.z n.z, each `comiss; jb` against the tolerance:
// a NaN leaves); n x 1 / sqrt(len2); flag 2 or 0 >= dist_a (`jnb`) reports
// -n (xorps 0x6F7CD0), else n; hitFraction = the report's answer (fstp).
inline void ray_triangle_process( void* user, float ( *t )[ 4 ], int part, int index )
{
    ray_triangle_t& c = *static_cast< ray_triangle_t* >( user );
    const float* v0 = t[ 0 ];
    const float* v1 = t[ 1 ];
    const float* v2 = t[ 2 ];
    const float e20y = v2[ 1 ] - v0[ 1 ], e10y = v1[ 1 ] - v0[ 1 ], e10z = v1[ 2 ] - v0[ 2 ];
    const float e20x = v2[ 0 ] - v0[ 0 ], e20z = v2[ 2 ] - v0[ 2 ], e10x = v1[ 0 ] - v0[ 0 ];
    const float nx = e20z * e10y - e20y * e10z;
    const float ny = e20x * e10z - e20z * e10x;
    const float nz = e20y * e10x - e20x * e10y;
    const float dist = ( ny * v0[ 1 ] + v0[ 0 ] * nx ) + nz * v0[ 2 ];
    const float dist_a = ( ( ny * c.from[ 1 ] + c.from[ 0 ] * nx ) + c.from[ 2 ] * nz ) - dist;
    const float dist_b = ( ( c.to[ 1 ] * ny + c.to[ 0 ] * nx ) + c.to[ 2 ] * nz ) - dist;
    if ( dist_b * dist_a >= 0.0f )
        return;
    if ( ( c.flags & 1u ) != 0 && dist_a > 0.0f )
        return;
    const float d = dist_a / ( dist_a - dist_b );
    if ( !( c.hit_fraction > d ) )
        return;
    const float len2 = ( nx * nx + ny * ny ) + nz * nz;
    const float tolerance = len2 * support_bits( 0xB8D1B717u );
    const float k = 1.0f - d;
    const float px = c.from[ 0 ] * k + c.to[ 0 ] * d;
    const float py = c.from[ 1 ] * k + c.to[ 1 ] * d;
    const float pz = c.from[ 2 ] * k + c.to[ 2 ] * d;
    const float p0y = v0[ 1 ] - py, p1x = v1[ 0 ] - px, p0x = v0[ 0 ] - px, p0z = v0[ 2 ] - pz, p1y = v1[ 1 ] - py, p1z = v1[ 2 ] - pz;
    {
        const float cx = p1z * p0y - p1y * p0z;
        const float cy = p1x * p0z - p1z * p0x;
        const float cz = p1y * p0x - p1x * p0y;
        if ( !( ( cy * ny + cx * nx ) + cz * nz >= tolerance ) )
            return;
    }
    const float p2y = v2[ 1 ] - py, p2z = v2[ 2 ] - pz, p2x = v2[ 0 ] - px;
    {
        const float cx = p2z * p1y - p2y * p1z;
        const float cy = p2x * p1z - p2z * p1x;
        const float cz = p2y * p1x - p2x * p1y;
        if ( !( ( cy * ny + cx * nx ) + cz * nz >= tolerance ) )
            return;
    }
    {
        const float cx = p2y * p0z - p2z * p0y;
        const float cy = p2z * p0x - p2x * p0z;
        const float cz = p2x * p0y - p2y * p0x;
        if ( !( ( cy * ny + cx * nx ) + cz * nz >= tolerance ) )
            return;
    }
    const float r = 1.0f / std::sqrt( len2 );
    float n[ 4 ] = { nx * r, ny * r, nz * r, 0.0f };
    if ( ( c.flags & 2u ) != 0 || 0.0f >= dist_a )
    {
        n[ 0 ] = support_negate( n[ 0 ] );
        n[ 1 ] = support_negate( n[ 1 ] );
        n[ 2 ] = support_negate( n[ 2 ] );
    }
    c.hit_fraction = ray_triangle_report( c, n, d, part, index );
}

// The quantized ray walk 0x587E60 (thiscall bvh; nodeCallback, raySource*,
// rayTarget*, aabbMin*, aabbMax*, startNodeIndex (unread: the walk starts at
// node 0), endNodeIndex; retn 1Ch), from its disassembly: d = to - from, len2
// = (dy^2 + dx^2) + dz^2, r = 1 / sqrtss, n = d r, lambda_max = (dy n.y + dx
// n.x) + dz n.z; inverses / signs as ray_inverse (`ucomiss; lahf; test ah,
// 44h`); the ray's box = min / max of from and to (`comiss; jbe` picks, all
// four lanes) + aabbMin / aabbMax, quantized as reportAabbOverlappingNodex
// (clamp, cvttss2si, & 0xFFFE / + 1.0 | 1); per node (one iteration each)
// the six 16-bit overlap compares, and only when they overlap the node's
// box unquantized (min x 1 / quantization + bvh min, the reciprocals by
// divss), minus aabbMax / aabbMin, through btRayAabb2 (ray_aabb: its
// `ja` rejects, `jbe` updates, lambda_max > tmin, tmax > 0.0 0x6F7570). A
// leaf that hits is processNode(index >> 21, index & 0x1FFFFF); a hit or a
// leaf steps on, any other node skips by its escape index; max iterations
// as the AABB walk.
inline void bvh_ray_walk( const quantized_bvh_t& bvh, const bvh_node_callback_t& callback, const float* from, const float* to,
                          const float* aabb_min, const float* aabb_max, int end )
{
    const float dx = to[ 0 ] - from[ 0 ], dy = to[ 1 ] - from[ 1 ], dz = to[ 2 ] - from[ 2 ];
    const float r = 1.0f / std::sqrt( ( dy * dy + dx * dx ) + dz * dz );
    const float nx = dx * r, ny = dy * r, nz = dz * r;
    ray_dir_t rd;
    rd.lambda_max = ( dy * ny + dx * nx ) + dz * nz;
    rd.inv[ 0 ] = ray_inverse( nx );
    rd.inv[ 1 ] = ray_inverse( ny );
    rd.inv[ 2 ] = ray_inverse( nz );
    for ( int k = 0; k < 3; ++k )
        rd.signs[ k ] = 0.0f > rd.inv[ k ] ? 1u : 0u;
    float lo[ 4 ], hi[ 4 ];
    for ( int k = 0; k < 4; ++k )
    {
        lo[ k ] = from[ k ] > to[ k ] ? to[ k ] : from[ k ];
        hi[ k ] = to[ k ] > from[ k ] ? to[ k ] : from[ k ];
    }
    for ( int k = 0; k < 3; ++k )
    {
        lo[ k ] = lo[ k ] + aabb_min[ k ];
        hi[ k ] = hi[ k ] + aabb_max[ k ];
    }
    uint16_t qmin[ 3 ], qmax[ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        const float v = bvh_clamp( lo[ k ], bvh.aabb_min[ k ], bvh.aabb_max[ k ] );
        qmin[ k ] = static_cast< uint16_t >( _mm_cvtt_ss2si( _mm_set_ss( ( v - bvh.aabb_min[ k ] ) * bvh.quantization[ k ] ) ) & 0xFFFE );
    }
    for ( int k = 0; k < 3; ++k )
    {
        const float v = bvh_clamp( hi[ k ], bvh.aabb_min[ k ], bvh.aabb_max[ k ] );
        qmax[ k ] = static_cast< uint16_t >( _mm_cvtt_ss2si( _mm_set_ss( ( v - bvh.aabb_min[ k ] ) * bvh.quantization[ k ] + 1.0f ) ) | 1 );
    }
    int index = 0, iterations = 0;
    const bvh_node_t* node = bvh.nodes;
    while ( index < end )
    {
        ++iterations;
        const int overlap = ( node->max[ 0 ] >= qmin[ 0 ] ) & ( qmax[ 0 ] >= node->min[ 0 ] ) & ( qmax[ 1 ] >= node->min[ 1 ] ) &
                            ( qmax[ 2 ] >= node->min[ 2 ] ) & ( node->max[ 1 ] >= qmin[ 1 ] ) & ( node->max[ 2 ] >= qmin[ 2 ] );
        const int32_t value = node->escape_or_triangle;
        const bool leaf = value >= 0;
        bool hit = false;
        if ( overlap )
        {
            const float ix = 1.0f / bvh.quantization[ 0 ], iy = 1.0f / bvh.quantization[ 1 ], iz = 1.0f / bvh.quantization[ 2 ];
            float b0[ 4 ], b1[ 4 ];
            b0[ 0 ] = static_cast< float >( node->min[ 0 ] ) * ix + bvh.aabb_min[ 0 ];
            b0[ 1 ] = bvh.aabb_min[ 1 ] + static_cast< float >( node->min[ 1 ] ) * iy;
            b0[ 2 ] = bvh.aabb_min[ 2 ] + static_cast< float >( node->min[ 2 ] ) * iz;
            b1[ 0 ] = static_cast< float >( node->max[ 0 ] ) * ix + bvh.aabb_min[ 0 ];
            b1[ 1 ] = bvh.aabb_min[ 1 ] + static_cast< float >( node->max[ 1 ] ) * iy;
            b1[ 2 ] = bvh.aabb_min[ 2 ] + static_cast< float >( node->max[ 2 ] ) * iz;
            for ( int k = 0; k < 3; ++k )
            {
                b0[ k ] = b0[ k ] - aabb_max[ k ];
                b1[ k ] = b1[ k ] - aabb_min[ k ];
            }
            b0[ 3 ] = b1[ 3 ] = 0.0f;
            hit = ray_aabb( from, rd.inv, rd.signs, b0, b1, rd.lambda_max );
        }
        if ( leaf && hit )
            callback.process_node( callback.user, value >> 21, value & 0x1FFFFF );
        if ( hit || leaf )
        {
            ++node;
            ++index;
        }
        else
        {
            const int escape = static_cast< int >( 0u - static_cast< uint32_t >( value ) );
            node += escape;
            index += escape;
        }
    }
    if ( g_bvh_max_iterations < iterations )
        g_bvh_max_iterations = iterations;
}

// btBvhTriangleMeshShape::performRaycast 0x5AECB0 (thiscall shape; callback,
// raySource*, rayTarget*): MyNodeOverlapCallback (vt 0x751760: +4 the mesh
// interface +0x30, +8 the callback), a zero aabbMin / aabbMax, then by the
// BVH's useQuantization (+0x48) the quantized walk 0x587E60 (0, +0x44) or
// walkStacklessTreeAgainstRay 0x587930 - NOT recreated (-1): the census of
// the 236 collision-mesh assets found every tree quantized (bvh.hxx).
inline int bvh_mesh_perform_raycast( const bvh_mesh_shape_t& s, const bvh_triangle_callback_t& callback, const float* from, const float* to )
{
    if ( !s.bvh || !s.mesh )
        return -1;
    if ( !s.bvh->use_quantization )
        return -1;
    mesh_node_callback_t node{ s.mesh, callback, {} };
    const bvh_node_callback_t walk{ &node, mesh_process_node };
    const float zero[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bvh_ray_walk( *s.bvh, walk, from, to, zero, zero, s.bvh->cur_node_index );
    return 1;
}

// The ray in the mesh's frame as 0x54CCC0 computes it (T the object's world
// transform, i = basis^T (-origin)): i[c] = ((T[c] n0) + (T[4+c] n1)) +
// T[8+c] n2; local[c] = (((T[4+c] p.y + T[c] p.x) + T[8+c] p.z) + i[c]) for
// z, and ((T[c] p.x + T[4+c] p.y) + T[8+c] p.z) + i[c] for x / y; w 0.
inline void ray_bvh_local( const float* t, const float* p, float* out )
{
    const float n0 = support_negate( t[ 12 ] ), n1 = support_negate( t[ 13 ] ), n2 = support_negate( t[ 14 ] );
    float i[ 3 ];
    for ( int c = 0; c < 3; ++c )
        i[ c ] = ( t[ c ] * n0 + t[ 4 + c ] * n1 ) + t[ 8 + c ] * n2;
    out[ 0 ] = ( ( t[ 0 ] * p[ 0 ] + t[ 4 ] * p[ 1 ] ) + t[ 8 ] * p[ 2 ] ) + i[ 0 ];
    out[ 1 ] = ( ( t[ 1 ] * p[ 0 ] + t[ 5 ] * p[ 1 ] ) + t[ 9 ] * p[ 2 ] ) + i[ 1 ];
    out[ 2 ] = ( ( t[ 6 ] * p[ 1 ] + t[ 2 ] * p[ 0 ] ) + t[ 10 ] * p[ 2 ] ) + i[ 2 ];
    out[ 3 ] = 0.0f;
}

struct ray_test_t;
inline int ray_test_single( ray_test_t& t, const float* from, const float* to, collision_object_t& o, const ray_shape_t& shape,
                            const float* world, ray_callback_t& cb );

struct ray_test_t
{
    simplex_solver_t solver{};   // rayTestSingle's stack btVoronoiSimplexSolver (threshold 0x38D1B717 - set per test)
    int status = 1;
};

// LocalInfoAdder2 (vt 0x74E84C): +4 closest (the user's at construction),
// +0xC group 1, +0xE mask -1, +0x10 flags 0, +0x14 the user, +0x18 the child.
struct ray_info_adder_t
{
    ray_callback_t* user;
    int child;
};

inline float ray_info_adder_add( void* u, ray_callback_t& self, ray_local_result_t& r, bool normal_in_world )
{
    ray_info_adder_t& a = *static_cast< ray_info_adder_t* >( u );
    const ray_shape_info_t info{ -1, a.child };
    if ( !r.info )
        r.info = &info;
    const float result = a.user->add_single_result( a.user->user, *a.user, r, normal_in_world );
    self.closest_hit_fraction = a.user->closest_hit_fraction;
    return result;
}

inline bool ray_info_adder_needs( void* u, ray_callback_t&, const broadphase_proxy_t& proxy )
{
    return ray_needs_collision( *static_cast< ray_info_adder_t* >( u )->user, proxy );
}

// RayTester::Process 0x54D700 (thiscall tester; child index; retn 4).
inline int ray_compound_child( ray_test_t& t, const float* from, const float* to, collision_object_t& o, const compound_shape_t& c,
                               const float* world, ray_callback_t& cb, int i )
{
    const compound_child_t& child = c.children[ static_cast< std::size_t >( i ) ];
    float cw[ 16 ];
    ray_child_world( world, child.transform, cw );
    const concave_body_t saved = o.body;   // the object's +0xCC swapped for the child's shape
    o.body.convex = child.shape;
    o.body.concave = nullptr;
    o.body.compound = nullptr;
    ray_info_adder_t adder{ &cb, i };
    ray_callback_t my{};
    my.closest_hit_fraction = cb.closest_hit_fraction;
    my.user = &adder;
    my.needs_collision = ray_info_adder_needs;
    my.add_single_result = ray_info_adder_add;
    ray_shape_t s;
    s.convex = child.shape;
    const int r = ray_test_single( t, from, to, o, s, cw, my );
    o.body = saved;
    return r;
}

// rayTestSingle 0x54C9B0 (fastcall rayFromTrans, rayToTrans; object, shape,
// colObjWorldTransform, resultCallback). The ray transforms here always have
// the identity basis (the btSingleRayCallback's) - `from` / `to` are their
// 16 words.
inline int ray_test_single( ray_test_t& t, const float* from, const float* to, collision_object_t& o, const ray_shape_t& shape,
                            const float* world, ray_callback_t& cb )
{
    const int type = ray_shape_type( shape );
    if ( type < 0 )
        return -1;
    if ( type < 20 )
    {
        // The radius-0 sphere (vt 0x73ADD4, type 8, scaling (1, 1, 1, 0)
        // 0x6F7B00, +0x20 = 0, margin +0x30 = 0).
        convex_shape_t sphere{};
        sphere.type = 8;
        sphere.local_scaling[ 0 ] = sphere.local_scaling[ 1 ] = sphere.local_scaling[ 2 ] = 1.0f;
        ray_cast_result_t res;
        res.fraction = cb.closest_hit_fraction;
        simplex_construct( t.solver );   // the stack solver's ctor: the same two stores (0x54CA80 .. 0x54CAD0)
        const int ok = ray_subsimplex_cast( t.solver, sphere, *shape.convex, from, to, world, world, res );
        if ( ok < 0 )
            return -1;
        if ( !ok )
            return 1;
        const float* n = res.normal;
        if ( !( ( n[ 1 ] * n[ 1 ] + n[ 0 ] * n[ 0 ] ) + n[ 2 ] * n[ 2 ] > support_bits( 0x38D1B717u ) ) )
            return 1;
        if ( !( cb.closest_hit_fraction > res.fraction ) )
            return 1;
        // castResult.m_normal = rayFromTrans.getBasis() * m_normal (the
        // identity basis' words), then normalised.
        const float b[ 12 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
        const float x = ( b[ 1 ] * n[ 1 ] + n[ 0 ] * b[ 0 ] ) + b[ 2 ] * n[ 2 ];
        const float y = ( b[ 5 ] * n[ 1 ] + n[ 0 ] * b[ 4 ] ) + b[ 6 ] * n[ 2 ];
        const float z = ( b[ 9 ] * n[ 1 ] + n[ 0 ] * b[ 8 ] ) + b[ 10 ] * n[ 2 ];
        const float inv = 1.0f / std::sqrt( ( y * y + x * x ) + z * z );
        ray_local_result_t r;
        r.object = &o;
        r.normal[ 0 ] = x * inv;
        r.normal[ 1 ] = y * inv;
        r.normal[ 2 ] = z * inv;
        r.normal[ 3 ] = 0.0f;
        r.fraction = res.fraction;
        cb.add_single_result( cb.user, cb, r, true );
        return 1;
    }
    if ( type == 31 )
    {
        // The compound's btDbvt (+0x40) - always built by the ctor 0x5269C0;
        // the model holds one leaf (compound.hxx). Its root may be absent.
        const compound_shape_t& c = *shape.compound;
        if ( !c.has_root )
            return 1;
        float lf[ 4 ], lt[ 4 ];
        ray_local_origin( world, from, lf );
        ray_local_origin( world, to, lt );
        // btDbvt::rayTest 0x551F10 (fastcall root, rayFrom; rayTo, policy):
        // d = to - from, len2 = (dx^2 + dy^2) + dz^2, n = (dx r, r dy, r dz),
        // lambda_max = (dx n.x + n.y dy) + n.z dz; the stack from the root.
        const float dx = lt[ 0 ] - lf[ 0 ], dy = lt[ 1 ] - lf[ 1 ], dz = lt[ 2 ] - lf[ 2 ];
        const float rr = 1.0f / std::sqrt( ( dx * dx + dy * dy ) + dz * dz );
        const float nx = dx * rr, ny = rr * dy, nz = rr * dz;
        ray_dir_t rd;
        rd.inv[ 0 ] = ray_inverse( nx );
        rd.inv[ 1 ] = ray_inverse( ny );
        rd.inv[ 2 ] = ray_inverse( nz );
        for ( int k = 0; k < 3; ++k )
            rd.signs[ k ] = 0.0f > rd.inv[ k ] ? 1u : 0u;
        rd.lambda_max = ( dx * nx + ny * dy ) + nz * dz;
        if ( !ray_aabb( lf, rd.inv, rd.signs, c.root_min, c.root_max, rd.lambda_max ) )
            return 1;
        return ray_compound_child( t, from, to, o, c, world, cb, c.root_data );
    }
    if ( type == 21 && shape.mesh )
    {
        // The BVH mesh (0x54CCC0): the local ray, the Bridge callback (ctor
        // 0x54D410: flags = the result's +0x10, hitFraction 1.0 then the
        // result's +4), performRaycast.
        ray_triangle_t c;
        ray_bvh_local( world, from + 12, c.from );
        ray_bvh_local( world, to + 12, c.to );
        c.flags = cb.flags;
        c.hit_fraction = cb.closest_hit_fraction;
        c.result = &cb;
        c.object = &o;
        c.world = world;
        const bvh_triangle_callback_t tri{ &c, ray_triangle_process };
        return bvh_mesh_perform_raycast( *shape.mesh, tri, c.from, c.to );
    }
    if ( type >= 21 && type <= 29 )
        return -1;   // the generic concave path (0x54D5D0 + the shape's vt+0x38 processAllTriangles): no such shape is modelled
    return 1;        // any other type: nothing
}

// ---- btCollisionWorld::rayTest 0x54F590 -----------------------------------

// btDbvt::rayTestInternal 0x56EE90 over one broadphase set: bounds = the
// node's mins - aabbMax / maxs - aabbMin (both zero here), the stack popped
// from the top, an internal node pushes childs[0] then childs[1], a leaf is
// the callback's process (its data = the proxy).
template < typename F >
inline int ray_dbvt_internal( const dbvt_node_t* root, const float* from, const ray_dir_t& rd, F&& process )
{
    std::vector< const dbvt_node_t* > stack{ root };
    while ( !stack.empty( ) )
    {
        const dbvt_node_t* n = stack.back( );
        stack.pop_back( );
        float lo[ 4 ], hi[ 4 ];
        for ( int k = 0; k < 4; ++k )
        {
            lo[ k ] = n->mn[ k ] - 0.0f;
            hi[ k ] = n->mx[ k ] - 0.0f;
        }
        if ( !ray_aabb( from, rd.inv, rd.signs, lo, hi, rd.lambda_max ) )
            continue;
        if ( !dbvt_is_leaf( n ) )
        {
            stack.push_back( n->childs[ 0 ] );
            stack.push_back( n->childs[ 1 ] );
        }
        else if ( process( *static_cast< const dbvt_proxy_t* >( static_cast< broadphase_proxy_t* >( n->data ) ) ) < 0 )
            return -1;
    }
    return 1;
}

// The ray from `from` to `to` (16 words each, w lanes carried) through the
// world's broadphase: 1 done, -1 a path not recreated (UNKNOWN - the hits up
// to there were reported, the rest of the traversal did not run).
inline int collision_world_ray_test( collision_world_t& w, const float* from, const float* to, ray_callback_t& cb )
{
    // btSingleRayCallback 0x54F330: dir = to - from, len2 = (dy^2 + dx^2) +
    // dz^2 (the ctor's `addss` order: dx^2 + dy^2 -> the same bits), n = d
    // (1 / sqrt), inv / signs as ray_inverse, lambda_max = ((to.y - from.y)
    // n.y + (to.x - from.x) n.x) + (to.z - from.z) n.z.
    const float dx = to[ 0 ] - from[ 0 ], dy = to[ 1 ] - from[ 1 ], dz = to[ 2 ] - from[ 2 ];
    const float rr = 1.0f / std::sqrt( ( dy * dy + dx * dx ) + dz * dz );
    const float nx = dx * rr, ny = dy * rr, nz = dz * rr;
    ray_dir_t rd;
    rd.inv[ 0 ] = ray_inverse( nx );
    rd.inv[ 1 ] = ray_inverse( ny );
    rd.inv[ 2 ] = ray_inverse( nz );
    for ( int k = 0; k < 3; ++k )
        rd.signs[ k ] = 0.0f > rd.inv[ k ] ? 1u : 0u;
    rd.lambda_max = ( ( to[ 1 ] - from[ 1 ] ) * ny + ( to[ 0 ] - from[ 0 ] ) * nx ) + ( to[ 2 ] - from[ 2 ] ) * nz;
    // The ray transforms: identity bases (0x413280), origins = from / to.
    float ft[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }, tt[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
    std::memcpy( ft + 12, from, 16 );
    std::memcpy( tt + 12, to, 16 );
    ray_test_t t;
    auto process = [ & ]( const dbvt_proxy_t& p ) -> int {
        if ( cb.closest_hit_fraction == 0.0f )
            return 1;
        if ( !ray_needs_collision( cb, p ) )
            return 1;
        collision_object_t& o = *static_cast< collision_object_t* >( p.client );
        ray_shape_t s;
        s.convex = o.body.convex;
        s.mesh = o.body.concave;
        s.compound = o.body.compound;
        return ray_test_single( t, ft, tt, o, s, o.body.transform, cb );
    };
    if ( !w.broadphase )
        return -1;
    for ( int set = 0; set < 2; ++set )
        if ( w.broadphase->sets[ set ].root && ray_dbvt_internal( w.broadphase->sets[ set ].root, from, rd, process ) < 0 )
            return -1;
    return 1;
}
