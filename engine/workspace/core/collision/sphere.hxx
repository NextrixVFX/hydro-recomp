#pragma once

#include <cmath>
#include <cstring>

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
