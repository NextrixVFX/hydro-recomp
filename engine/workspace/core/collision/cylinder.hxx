#pragma once

#include <workspace/core/collision/box.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>

// btCylinderShape / btCylinderShapeX / btCylinderShapeZ's own virtuals
// (Bullet 2.79; vtables 0x751494 / 0x7514F4 / 0x751434), recreated from
// retail's disassembly over the shape's bytes (box.hxx's layout - scaling
// +0x10, implicit dimensions +0x20, margin +0x30 - plus the up axis at
// +0x40). The axes (XX, YY, ZZ) are Y (0, 1, 2), X (1, 0, 2), Z (0, 2, 1):
// radius = implicit[XX], half height = implicit[YY].
//   7  calculateLocalInertia 0x5AE2D0 (shared by the three)
//   14 localGetSupportingVertex 0x5AE950 (btConvexInternalShape's, shared)
//   15 localGetSupportingVertexWithoutMargin Y 0x5AE560, X 0x5AE430, Z 0x5AE4C0
//   17 the batched form Y 0x5AE600, X 0x5AE7A0, Z 0x5AE6D0
// (slot 21 getRadius 0x5AE870 / 0x5AE8E0 returns on the x87 stack - not
// covered by the oracle case, not recreated.)

struct cylinder_axes_t
{
    int xx, yy, zz;
};

inline cylinder_axes_t cylinder_axes( int up )
{
    return up == 0 ? cylinder_axes_t{ 1, 0, 2 } : up == 2 ? cylinder_axes_t{ 0, 2, 1 } : cylinder_axes_t{ 0, 1, 2 };
}

// The support without the margin (0x5AE560 / 0x5AE430 / 0x5AE4C0, and per
// vector in the batched forms): s = sqrt(v[XX]^2 + v[ZZ]^2); for s != 0
// (ucomiss / lahf / jnp - a NaN s counts as non-zero) out[XX] = v[XX] x
// (radius / s), out[ZZ] = v[ZZ] x (radius / s), else out[XX] = radius,
// out[ZZ] = 0; out[YY] = the half height, negated when 0 > v[YY] (comiss:
// a NaN keeps it). Only the three lanes are written.
inline void cylinder_support_without_margin( const std::uint8_t* s, const cylinder_axes_t& a, const float* v, float* out )
{
    const float radius = box_lane( s, 0x20 + 4 * a.xx );
    float half = box_lane( s, 0x20 + 4 * a.yy );
    const float len = std::sqrt( v[ a.xx ] * v[ a.xx ] + v[ a.zz ] * v[ a.zz ] );
    if ( 0.0f > v[ a.yy ] )
        half = box_negate( half );
    if ( len == 0.0f )
    {
        out[ a.xx ] = radius;
        out[ a.zz ] = 0.0f;
    }
    else
    {
        const float d = radius / len;
        out[ a.xx ] = v[ a.xx ] * d;
        out[ a.zz ] = v[ a.zz ] * d;
    }
    out[ a.yy ] = half;
}

// The batched form: per 16-byte vector the same three lanes; the fourth is
// copied from a stack word never written (indeterminate - left alone here).
inline void cylinder_support_batched( const std::uint8_t* s, const cylinder_axes_t& a, const float* dirs, float* outs, int n )
{
    for ( int k = 0; k < n; ++k )
        cylinder_support_without_margin( s, a, dirs + 4 * k, outs + 4 * k );
}

// 0x5AE950 localGetSupportingVertex(dir): slot 15 into a temp (its w an
// unwritten stack word, copied out), then, unless the margin is exactly 0,
// + the margin along dir / |dir| ((x^2 + y^2) + z^2 below 0x28800000 =
// 1.42e-14: dir = (-1, -1, -1) first; 1 / sqrtss, three multiplies).
inline void cylinder_support( const std::uint8_t* s, const cylinder_axes_t& a, const float* dir, float* out )
{
    cylinder_support_without_margin( s, a, dir, out );
    const float margin = box_margin( s );
    if ( margin == 0.0f )
        return;
    float x = dir[ 0 ], y = dir[ 1 ], z = dir[ 2 ];
    const std::uint32_t eps_bits = 0x28800000u;   // 0x6F7578
    float eps;
    std::memcpy( &eps, &eps_bits, 4 );
    if ( eps > ( x * x + y * y ) + z * z )
        x = y = z = -1.0f;
    const float inverse = 1.0f / std::sqrt( ( x * x + y * y ) + z * z );
    x = x * inverse;
    y = y * inverse;
    z = z * inverse;
    const float m = box_margin( s );
    out[ 0 ] = out[ 0 ] + x * m;
    out[ 1 ] = out[ 1 ] + y * m;
    out[ 2 ] = out[ 2 ] + z * m;
}

// 0x5AE2D0 calculateLocalInertia(mass, inertia&): e = implicit + margin per
// lane; by the up axis (+0x40: 0 -> radius lane 1, height lane 0; 2 ->
// radius 0, height 2; anything else -> radius 0, height 1) r2 = e[r]^2,
// I_perp = ((4 h) h)(mass / 12) + r2 (mass / 4), I_axis = r2 (mass / 2);
// I_axis on the up lane (up 0 -> x, 2 -> z, else y), w = 0.
inline void cylinder_local_inertia( const std::uint8_t* s, float mass, float* inertia )
{
    const float m0 = box_margin( s ), m1 = box_margin( s ), m2 = box_margin( s );
    float e[ 3 ];
    e[ 0 ] = box_lane( s, 0x20 ) + m2;
    e[ 1 ] = box_lane( s, 0x24 ) + m1;
    e[ 2 ] = box_lane( s, 0x28 ) + m0;
    std::uint32_t twelfth_bits = 0x3DAAAAABu;   // 0x6F760C
    float twelfth;
    std::memcpy( &twelfth, &twelfth_bits, 4 );
    const float k12 = mass * twelfth;
    const float k4 = mass * 0.25f;   // 0x6F762C
    const float k2 = mass * 0.5f;    // 0x6F765C
    std::int32_t up;
    std::memcpy( &up, s + 0x40, 4 );
    const int r = up == 0 ? 1 : 0;
    const int h = up == 0 ? 0 : up == 2 ? 2 : 1;
    const float r2 = e[ r ] * e[ r ];
    const float perp = ( ( e[ h ] * 4.0f ) * e[ h ] ) * k12 + r2 * k4;
    const float axis = r2 * k2;
    inertia[ 3 ] = 0.0f;
    if ( up == 0 )
    {
        inertia[ 0 ] = axis;
        inertia[ 1 ] = perp;
        inertia[ 2 ] = perp;
    }
    else if ( up == 2 )
    {
        inertia[ 0 ] = perp;
        inertia[ 1 ] = perp;
        inertia[ 2 ] = axis;
    }
    else
    {
        inertia[ 0 ] = perp;
        inertia[ 1 ] = axis;
        inertia[ 2 ] = perp;
    }
}