#pragma once

#include <cmath>
#include <cstring>

#include <workspace/core/collision/box.hxx>
#include <workspace/core/collision/concave.hxx>

// btSphereSphereCollisionAlgorithm (Bullet 2.79, vt 0x75784C), recreated from
// retail's disassembly (memory/audit.md "Batch 8m"): the CreateFunc (vt
// 0x74F5F8, slot 1 0x56D0D0), processCollision 0x5FEA10, the dtor 0x5FEC00.

// +4 the dispatcher, +8 ownManifold, +0xC the manifold. The CreateFunc
// (thiscall info, body0, body1; retn 0Ch) allocates 16 bytes through the
// dispatcher (vt+0x34), then ALWAYS getNewManifold(body0, body1) (vt+8) and
// ownManifold 1 - no clearManifold. The dtor releases (vt+0xC) only an owned,
// non-null manifold.
struct sphere_sphere_t
{
    bool own_manifold = false;
    solver_manifold_t* manifold = nullptr;
};

// The sphere's radius as processCollision reads it: m_implicitShapeDimensions
// .x (+0x20) * m_localScaling.x (+0x10).
inline float sphere_radius( const convex_shape_t& s )
{
    return s.implicit[ 0 ] * s.local_scaling[ 0 ];
}

// btManifoldResult::refreshContactPoints as 0x5FEA10 inlines it: no points ->
// nothing; the manifold's body0 (+1168) against the result's (+144) picks
// refreshContactPoints 0x5B6C10 (A, B) or (B, A).
inline void sphere_refresh( manifold_result_t& r, const contact_callbacks_t& cb )
{
    solver_manifold_t& m = *r.manifold;
    if ( m.num_contacts == 0 )
        return;
    if ( m.body0 == r.object0 )
        manifold_refresh( m, r.basis0, r.origin0, r.basis1, r.origin1, cb );
    else
        manifold_refresh( m, r.basis1, r.origin1, r.basis0, r.origin0, cb );
}

// processCollision @0x5FEA10 (thiscall; body0, body1, info, result; retn
// 10h): no manifold -> nothing; result +4 = the manifold; d = body0's origin
// (+0x40) - body1's, len = sqrtss((dy dy + dx dx) + dz dz); r0 / r1 the two
// radii, `comiss len, r1 + r0; jbe` - apart -> the refresh, done. Else dist =
// len - (r1 + r0); the normal (1, 0, 0, 0) (0x6F7930) unless `comiss len,
// 1.1920929e-7; jbe` fails -> d * (1 / len), w 0; the point = body1's origin
// + normal * r1, w 0; addContactPoint (vt+0xC: normal, point, dist); then the
// refresh. 2.79 has no CLEAR_MANIFOLD here.
inline int sphere_sphere_process( sphere_sphere_t& a, const concave_body_t& b0, const concave_body_t& b1, manifold_result_t& r,
    const contact_callbacks_t& cb )
{
    if ( !a.manifold )
        return 1;
    if ( !b0.convex || !b1.convex || b0.convex->type != 8 || b1.convex->type != 8 )
        return -1;
    r.manifold = a.manifold;
    const float* p0 = b0.transform + 12;
    const float* p1 = b1.transform + 12;
    const float dx = p0[ 0 ] - p1[ 0 ];
    const float dy = p0[ 1 ] - p1[ 1 ];
    const float dz = p0[ 2 ] - p1[ 2 ];
    const float len = std::sqrt( ( dy * dy + dx * dx ) + dz * dz );
    const float r0 = sphere_radius( *b0.convex );
    const float r1 = sphere_radius( *b1.convex );
    const float sum = r1 + r0;
    if ( len > sum )
    {
        sphere_refresh( r, cb );
        return 1;
    }
    const float dist = len - sum;
    float normal[ 4 ] = { 1.0f, 0.0f, 0.0f, 0.0f };
    if ( len > support_bits( 0x34000000u ) )
    {
        const float inv = 1.0f / len;
        normal[ 0 ] = inv * dx;
        normal[ 1 ] = inv * dy;
        normal[ 2 ] = inv * dz;
        normal[ 3 ] = 0.0f;
    }
    float point[ 4 ];
    point[ 0 ] = p1[ 0 ] + normal[ 0 ] * r1;
    point[ 1 ] = p1[ 1 ] + normal[ 1 ] * r1;
    point[ 2 ] = p1[ 2 ] + normal[ 2 ] * r1;
    point[ 3 ] = 0.0f;
    manifold_result_add_contact_point( r, normal, point, dist, cb );
    sphere_refresh( r, cb );
    return 1;
}

// btSphereShape's own virtuals (Bullet 2.79, vtable 0x73ADD4), recreated
// from retail's disassembly over the shape's bytes (box.hxx's layout; the
// radius is implicit dimension x, +0x20). Its getMargin is slot 10
// 0x43D650: radius x scaling.x (+0x20 x +0x10), returned through fld - the
// product is never a signaling NaN, so the x87 round trip is exact.
//   7  calculateLocalInertia 0x5535A0
//   14 localGetSupportingVertex 0x553360
//   15 localGetSupportingVertexWithoutMargin 0x5532F0
//   17 the batched form 0x553320

inline float sphere_margin( const std::uint8_t* s )
{
    return box_lane( s, 0x20 ) * box_lane( s, 0x10 );
}

// 0x5535A0 calculateLocalInertia(mass, inertia&): (m (m (mass x 0.4))) on
// all three lanes (0x6F7650 = 0x3ECCCCCD), m the margin, w = 0.
inline void sphere_local_inertia( const std::uint8_t* s, float mass, float* inertia )
{
    std::uint32_t bits = 0x3ECCCCCDu;
    float two_fifths;
    std::memcpy( &two_fifths, &bits, 4 );
    const float first = sphere_margin( s ) * ( mass * two_fifths );
    const float elem = sphere_margin( s ) * first;
    inertia[ 3 ] = 0.0f;
    inertia[ 0 ] = elem;
    inertia[ 1 ] = elem;
    inertia[ 2 ] = elem;
}

// 0x5532F0 localGetSupportingVertexWithoutMargin: (0, 0, 0, 0).
inline void sphere_support_without_margin( float* out )
{
    out[ 0 ] = out[ 1 ] = out[ 2 ] = out[ 3 ] = 0.0f;
}

// 0x553320 the batched form: zeros, four lanes per vector; n <= 0 nothing.
inline void sphere_support_batched( float* outs, int n )
{
    for ( int k = 0; k < n; ++k )
        sphere_support_without_margin( outs + 4 * k );
}

// 0x553360 localGetSupportingVertex(dir): slot 15's zeros, then - with no
// margin test, unlike 0x5AE950 - + the margin along dir / |dir| ((x^2 + y^2)
// + z^2 below 1.42e-14 (0x6F7578): dir = (-1, -1, -1) first); w stays 0.
inline void sphere_support( const std::uint8_t* s, const float* dir, float* out )
{
    sphere_support_without_margin( out );
    float x = dir[ 0 ], y = dir[ 1 ], z = dir[ 2 ];
    const std::uint32_t eps_bits = 0x28800000u;
    float eps;
    std::memcpy( &eps, &eps_bits, 4 );
    if ( eps > ( x * x + y * y ) + z * z )
        x = y = z = -1.0f;
    const float inverse = 1.0f / std::sqrt( ( x * x + y * y ) + z * z );
    x = x * inverse;
    y = y * inverse;
    z = z * inverse;
    const float m = sphere_margin( s );
    out[ 0 ] = out[ 0 ] + x * m;
    out[ 1 ] = out[ 1 ] + y * m;
    out[ 2 ] = out[ 2 ] + z * m;
}