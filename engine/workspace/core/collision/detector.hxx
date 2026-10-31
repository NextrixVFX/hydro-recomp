#pragma once

#include <cmath>
#include <cstring>
#include <xmmintrin.h>

#include <workspace/core/collision/convex.hxx>

// btSphereTriangleCollisionAlgorithm (Bullet 2.79, vt 0x757698) and its
// SphereTriangleDetector (vt 0x7576AC), recreated from retail's disassembly
// (memory/audit.md "Batch 8n"): processCollision 0x5FB9E0, getClosestPoints
// 0x620F00, collide 0x621580, pointInTriangle 0x621BC0. The triangle is the
// btTriangleShape (vt 0x751C7C): getNumEdges 0x406A60 = 3, getEdge 0x5B7D20
// = (vertex i, vertex (i + 1) % 3), getVertex 0x5B7CF0 = the 16 bytes at +0x50
// + 16 i. Every compare below is retail's `comiss` with its own jump; the ones
// that decide on NaN are SSE intrinsics (the MSVC NaN lesson, Batch 8i).

inline bool detector_gt( float a, float b )
{
    return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0;   // `comiss a, b; ja`
}

// pointInTriangle @0x621BC0 (stdcall vertices, normal, p; retn 0Ch): per edge
// e = v[i+1] - v[i] the edge normal e x n, r = (en.y pv.y + en.x pv.x) +
// en.z pv.z with pv = p - v[i]; true when all three are > 0 (`comiss; jbe`)
// or all three are <= 0 (`comiss 0, r; jb` -> false) - NaN fails both.
inline bool detector_point_in_triangle( const float ( *v )[ 4 ], const float* n, const float* p )
{
    float r[ 3 ];
    for ( int i = 0; i < 3; ++i )
    {
        const float* a = v[ i ];
        const float* b = v[ ( i + 1 ) % 3 ];
        const float ex = b[ 0 ] - a[ 0 ], ey = b[ 1 ] - a[ 1 ], ez = b[ 2 ] - a[ 2 ];
        const float nx = n[ 2 ] * ey - n[ 1 ] * ez;
        const float ny = n[ 0 ] * ez - n[ 2 ] * ex;
        const float nz = n[ 1 ] * ex - n[ 0 ] * ey;
        const float px = p[ 0 ] - a[ 0 ], py = p[ 1 ] - a[ 1 ], pz = p[ 2 ] - a[ 2 ];
        r[ i ] = ( ny * py + nx * px ) + nz * pz;
    }
    if ( detector_gt( r[ 0 ], 0.0f ) && detector_gt( r[ 1 ], 0.0f ) && detector_gt( r[ 2 ], 0.0f ) )
        return true;
    const __m128 z = _mm_setzero_ps( );
    return _mm_comige_ss( z, _mm_set_ss( r[ 0 ] ) ) && _mm_comige_ss( z, _mm_set_ss( r[ 1 ] ) ) && _mm_comige_ss( z, _mm_set_ss( r[ 2 ] ) );
}

// collide @0x621580 (thiscall detector; center, point*, normal*, depth*,
// timeOfImpact* - NEVER READ, the caller leaves its slot unwritten -,
// contactBreakingThreshold; retn 18h). radius = sphere +0x20 * +0x10; rwt =
// radius + threshold. The triangle normal (e1 x e2) / sqrtps, flipped by *
// -1.0 (0x6F78A0) where `comiss 0, dist; ja`; `comiss rwt, dist; jbe` -> no
// contact. The face test (pointInTriangle) -> center - n * dist; else every
// edge's SegmentSqrDistance (inlined): t = v.diff; t > 0 -> dotVV > t ? t /=
// dotVV, diff -= v t : t = 1, diff -= v; nearest = v t + from; a strictly
// closer-than-rwt^2 edge replaces the contact (the LAST such edge wins). Then
// contactToCentre, `comiss rwt^2, d2; jbe` -> no contact; d2 > 1.1920929e-7 ->
// normal = contactToCentre normalised ((x x + y y) + z z), depth = -(radius -
// sqrt(d2)); else the triangle normal, depth = -radius.
inline bool detector_collide( const convex_shape_t& sphere, const convex_shape_t& tri, const float* center, float* point, float* normal,
    float& depth, float threshold )
{
    const float radius = sphere.implicit[ 0 ] * sphere.local_scaling[ 0 ];
    const float rwt = radius + threshold;
    const float* v0 = tri.vertices[ 0 ];
    const float* v1 = tri.vertices[ 1 ];
    const float* v2 = tri.vertices[ 2 ];
    const float e1x = v1[ 0 ] - v0[ 0 ], e1y = v1[ 1 ] - v0[ 1 ], e1z = v1[ 2 ] - v0[ 2 ];
    const float e2x = v2[ 0 ] - v0[ 0 ], e2y = v2[ 1 ] - v0[ 1 ], e2z = v2[ 2 ] - v0[ 2 ];
    float nx = e1y * e2z - e1z * e2y;
    float ny = e1z * e2x - e1x * e2z;
    float nz = e1x * e2y - e1y * e2x;
    const float len2 = ( ny * ny + nx * nx ) + nz * nz;
    const float inv = 1.0f / _mm_cvtss_f32( _mm_sqrt_ps( _mm_set_ss( len2 ) ) );
    nx = nx * inv;
    ny = ny * inv;
    nz = nz * inv;
    float dist = ( ( center[ 1 ] - v0[ 1 ] ) * ny + ( center[ 0 ] - v0[ 0 ] ) * nx ) + ( center[ 2 ] - v0[ 2 ] ) * nz;
    if ( detector_gt( 0.0f, dist ) )
    {
        const float m = support_bits( 0xBF800000u );
        nx = nx * m;
        ny = ny * m;
        nz = nz * m;
        dist = dist * m;
    }
    if ( !detector_gt( rwt, dist ) )
        return false;
    const float tri_normal[ 4 ] = { nx, ny, nz, 0.0f };
    float contact[ 4 ];
    if ( detector_point_in_triangle( tri.vertices, tri_normal, center ) )
    {
        contact[ 0 ] = center[ 0 ] - nx * dist;
        contact[ 1 ] = center[ 1 ] - ny * dist;
        contact[ 2 ] = center[ 2 ] - nz * dist;
        contact[ 3 ] = 0.0f;
    }
    else
    {
        const float rwt2 = rwt * rwt;
        bool has = false;
        std::memcpy( contact, tri_normal, 16 );   // var_50: the normal copy, replaced by a hit
        for ( int i = 0; i < 3; ++i )
        {
            const float* pa = tri.vertices[ i ];
            const float* pb = tri.vertices[ ( i + 1 ) % 3 ];
            const float vx = pb[ 0 ] - pa[ 0 ], vy = pb[ 1 ] - pa[ 1 ], vz = pb[ 2 ] - pa[ 2 ];
            float dx = center[ 0 ] - pa[ 0 ], dy = center[ 1 ] - pa[ 1 ], dz = center[ 2 ] - pa[ 2 ];
            float t = ( vy * dy + vx * dx ) + vz * dz;
            if ( detector_gt( t, 0.0f ) )
            {
                const float vv = ( vy * vy + vx * vx ) + vz * vz;
                if ( detector_gt( vv, t ) )
                {
                    t = t / vv;
                    dy = dy - vy * t;
                    dz = dz - vz * t;
                    dx = dx - vx * t;
                }
                else
                {
                    t = 1.0f;
                    dx = dx - vx;
                    dy = dy - vy;
                    dz = dz - vz;
                }
            }
            else
                t = 0.0f;
            const float near_x = vx * t + pa[ 0 ];
            const float near_y = vy * t + pa[ 1 ];
            const float near_z = vz * t + pa[ 2 ];
            const float d2 = ( dy * dy + dx * dx ) + dz * dz;
            if ( detector_gt( rwt2, d2 ) )
            {
                has = true;
                contact[ 0 ] = near_x;
                contact[ 1 ] = near_y;
                contact[ 2 ] = near_z;
                contact[ 3 ] = 0.0f;
            }
        }
        if ( !has )
            return false;
    }
    const float cx = center[ 0 ] - contact[ 0 ];
    const float cy = center[ 1 ] - contact[ 1 ];
    const float cz = center[ 2 ] - contact[ 2 ];
    const float d2 = ( cy * cy + cx * cx ) + cz * cz;
    if ( !detector_gt( rwt * rwt, d2 ) )
        return false;
    float r;
    if ( detector_gt( d2, support_bits( 0x34000000u ) ) )
    {
        const float l2 = ( cx * cx + cy * cy ) + cz * cz;
        const float il = 1.0f / _mm_cvtss_f32( _mm_sqrt_ps( _mm_set_ss( l2 ) ) );
        normal[ 0 ] = cx * il;
        normal[ 1 ] = cy * il;
        normal[ 2 ] = cz * il;
        normal[ 3 ] = 0.0f;
        r = radius - _mm_cvtss_f32( _mm_sqrt_ps( _mm_set_ss( d2 ) ) );
    }
    else
    {
        std::memcpy( normal, tri_normal, 16 );
        r = radius;
    }
    std::memcpy( point, contact, 16 );
    depth = support_negate( r );
    return true;
}

// getClosestPoints @0x620F00 (thiscall detector; input, result, debugDraw,
// swapResults; retn 10h): the sphere's origin in the triangle's frame (B =
// the triangle transform): o[k] = (d.x B[k] + d.y B[4+k]) + d.z B[8+k], d =
// A.origin - B.origin (the rotated basis it also builds is never read);
// collide; then swapped: nB = B * n, the point on A = (B * p + B.origin) + nB
// * depth, addContactPoint(-nB, that, depth); else addContactPoint(B * n,
// B * p + B.origin, depth). B * v rows: (v.x B[r] + v.y B[r+1]) + v.z B[r+2].
// depth starts 0 (var_E4).
inline void detector_get_closest_points( const convex_shape_t& sphere, const convex_shape_t& tri, float threshold, const float* ta,
    const float* tb, bool swap, manifold_result_t& result, const contact_callbacks_t& cb )
{
    const float dx = ta[ 12 ] - tb[ 12 ], dy = ta[ 13 ] - tb[ 13 ], dz = ta[ 14 ] - tb[ 14 ];
    float center[ 4 ];
    for ( int k = 0; k < 3; ++k )
        center[ k ] = ( dx * tb[ k ] + dy * tb[ 4 + k ] ) + dz * tb[ 8 + k ];
    center[ 3 ] = 0.0f;
    float point[ 4 ], normal[ 4 ], depth = 0.0f;
    if ( !detector_collide( sphere, tri, center, point, normal, depth, threshold ) )
        return;
    float nb[ 4 ], pw[ 4 ];
    for ( int r = 0; r < 3; ++r )
    {
        nb[ r ] = ( normal[ 0 ] * tb[ 4 * r ] + normal[ 1 ] * tb[ 4 * r + 1 ] ) + normal[ 2 ] * tb[ 4 * r + 2 ];
        pw[ r ] = ( ( point[ 0 ] * tb[ 4 * r ] + point[ 1 ] * tb[ 4 * r + 1 ] ) + point[ 2 ] * tb[ 4 * r + 2 ] ) + tb[ 12 + r ];
    }
    nb[ 3 ] = pw[ 3 ] = 0.0f;
    if ( swap )
    {
        float na[ 4 ] = { support_negate( nb[ 0 ] ), support_negate( nb[ 1 ] ), support_negate( nb[ 2 ] ), 0.0f };
        float pa[ 4 ];
        for ( int r = 0; r < 3; ++r )
            pa[ r ] = pw[ r ] + nb[ r ] * depth;
        pa[ 3 ] = 0.0f;
        manifold_result_add_contact_point( result, na, pa, depth, cb );
    }
    else
        manifold_result_add_contact_point( result, nb, pw, depth, cb );
}
