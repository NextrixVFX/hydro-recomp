#pragma once

#include <cstdint>
#include <cstring>

// btBoxShape's own virtuals (Bullet 2.79, vtable 0x74DEF4), recreated from
// retail's disassembly over the shape's bytes: +0 the vtable, +4 the shape
// type (0), +0x10 local scaling, +0x20 implicit shape dimensions (the half
// extents WITHOUT the margin), +0x30 the collision margin. The slots:
// 5 setLocalScaling 0x548A00, 7 calculateLocalInertia 0x5480C0, 9 setMargin
// 0x548900, 14 localGetSupportingVertex 0x548840, 15 ...WithoutMargin
// 0x5487D0, 17 batchedUnitVectorGetSupportingVertexWithoutMargin 0x548740,
// 20 getPreferredPenetrationDirection 0x548650, 24 getEdge 0x548580, 25
// getVertex 0x548460, 27 getPlane 0x548370, 28 isInside 0x5481A0, 29
// getPlaneEquation 0x548250 (slots 19 / 22 / 23 / 26 return 6 / 8 / 12 / 6).
// (getAabb, slot 1 0x547EE0, is in concave.hxx; the support switch's inline
// box case 0x547D80 in support.hxx.)

inline float box_lane( const std::uint8_t* s, int at )
{
    float v;
    std::memcpy( &v, s + at, 4 );
    return v;
}

inline void box_put( std::uint8_t* s, int at, float v )
{
    std::memcpy( s + at, &v, 4 );
}

// getMargin through the vtable: slot 10 0x43D5C0 = `fld dword [ecx+0x30]`,
// and every caller `fstp dword`s it - exact for every float but a signaling
// NaN, which the x87 load quiets (the invalid exception is masked).
inline float box_margin( const std::uint8_t* s )
{
    std::uint32_t bits;
    std::memcpy( &bits, s + 0x30, 4 );
    if ( ( bits & 0x7F800000u ) == 0x7F800000u && ( bits & 0x007FFFFFu ) != 0 )
        bits |= 0x00400000u;
    float v;
    std::memcpy( &v, &bits, 4 );
    return v;
}

inline float box_negate( float x )   // xorps with the 0x6F7CD0 sign mask
{
    std::uint32_t u;
    std::memcpy( &u, &x, 4 );
    u ^= 0x80000000u;
    std::memcpy( &x, &u, 4 );
    return x;
}

// 0x5480C0 calculateLocalInertia(mass, inertia&): the full extents
// l = (half + margin) x 2 (0x6F76F8) per lane, k = mass x 1/12 (0x6F760C);
// I = ((y^2 + z^2) k, (x^2 + z^2) k, (x^2 + y^2) k, 0).
inline void box_local_inertia( const std::uint8_t* s, float mass, float* inertia )
{
    const float m0 = box_margin( s ), m1 = box_margin( s ), m2 = box_margin( s );
    const float two = 2.0f;
    std::uint32_t twelfth_bits = 0x3DAAAAABu;   // 0x6F760C
    float twelfth;
    std::memcpy( &twelfth, &twelfth_bits, 4 );
    float ly = ( box_lane( s, 0x24 ) + m1 ) * two;
    float lz = ( box_lane( s, 0x28 ) + m0 ) * two;
    const float k = mass * twelfth;
    float lx = ( box_lane( s, 0x20 ) + m2 ) * two;
    ly = ly * ly;
    lz = lz * lz;
    lx = lx * lx;
    inertia[ 3 ] = 0.0f;
    inertia[ 0 ] = ( ly + lz ) * k;
    inertia[ 1 ] = ( lx + lz ) * k;
    inertia[ 2 ] = ( lx + ly ) * k;
}

// 0x548A00 setLocalScaling(scaling): the old half extents with the margin
// (half + margin) / old scaling, the new scaling |s| (andps 0x6F7CC0, w = 0),
// then half = new scaling x that - margin, w = 0.
inline void box_set_local_scaling( std::uint8_t* s, const float* scaling )
{
    const float m0 = box_margin( s ), m1 = box_margin( s ), m2 = box_margin( s );
    float x = box_lane( s, 0x20 );
    float y = box_lane( s, 0x24 ) + m1;
    float z = box_lane( s, 0x28 ) + m0;
    y = y / box_lane( s, 0x14 );
    z = z / box_lane( s, 0x18 );
    x = x + m2;
    auto magnitude = []( float v )
    {
        std::uint32_t u;
        std::memcpy( &u, &v, 4 );
        u &= 0x7FFFFFFFu;
        std::memcpy( &v, &u, 4 );
        return v;
    };
    x = x / box_lane( s, 0x10 );
    box_put( s, 0x10, magnitude( scaling[ 0 ] ) );
    box_put( s, 0x14, magnitude( scaling[ 1 ] ) );
    box_put( s, 0x18, magnitude( scaling[ 2 ] ) );
    box_put( s, 0x1C, 0.0f );
    const float hz = z * box_lane( s, 0x18 ) - m0;
    const float hx = box_lane( s, 0x10 ) * x - m2;
    const float hy = box_lane( s, 0x14 ) * y - m1;
    box_put( s, 0x20, hx );
    box_put( s, 0x24, hy );
    box_put( s, 0x28, hz );
    box_put( s, 0x2C, 0.0f );
}

// 0x548900 setMargin(margin): the half extents with the OLD margin, the
// margin stored at +0x30, then that minus the NEW margin (read back through
// getMargin), w = 0.
inline void box_set_margin( std::uint8_t* s, float margin )
{
    const float o0 = box_margin( s ), o1 = box_margin( s ), o2 = box_margin( s );
    const float x = box_lane( s, 0x20 ) + o2;
    const float y = box_lane( s, 0x24 ) + o1;
    const float z = box_lane( s, 0x28 ) + o0;
    box_put( s, 0x30, margin );
    const float n0 = box_margin( s ), n1 = box_margin( s ), n2 = box_margin( s );
    box_put( s, 0x20, x - n2 );
    box_put( s, 0x24, y - n1 );
    box_put( s, 0x28, z - n0 );
    box_put( s, 0x2C, 0.0f );
}

// One lane of the support: the extent when the direction's lane is >= 0
// (comiss / jae - a NaN direction lane takes the negated one), else its
// sign flipped.
inline float box_support_lane( float extent, float direction )
{
    return direction >= 0.0f ? extent : box_negate( extent );
}

// 0x548840 localGetSupportingVertex(dir): half + margin per lane, signed by
// the direction; w = 0.
inline void box_support( const std::uint8_t* s, const float* dir, float* out )
{
    const float m0 = box_margin( s ), m1 = box_margin( s ), m2 = box_margin( s );
    const float x = box_lane( s, 0x20 ) + m2;
    const float y = box_lane( s, 0x24 ) + m1;
    const float z = box_lane( s, 0x28 ) + m0;
    out[ 0 ] = box_support_lane( x, dir[ 0 ] );
    out[ 1 ] = box_support_lane( y, dir[ 1 ] );
    out[ 2 ] = box_support_lane( z, dir[ 2 ] );
    out[ 3 ] = 0.0f;
}

// 0x5487D0 localGetSupportingVertexWithoutMargin(dir): the half extents
// signed by the direction; w = 0.
inline void box_support_without_margin( const std::uint8_t* s, const float* dir, float* out )
{
    out[ 0 ] = box_support_lane( box_lane( s, 0x20 ), dir[ 0 ] );
    out[ 1 ] = box_support_lane( box_lane( s, 0x24 ), dir[ 1 ] );
    out[ 2 ] = box_support_lane( box_lane( s, 0x28 ), dir[ 2 ] );
    out[ 3 ] = 0.0f;
}

// 0x548740 batchedUnitVectorGetSupportingVertexWithoutMargin(dirs, outs, n):
// 0x5487D0 per vector (16-byte strides, w = 0); n <= 0 does nothing.
inline void box_support_batched( const std::uint8_t* s, const float* dirs, float* outs, int n )
{
    for ( int k = 0; k < n; ++k )
        box_support_without_margin( s, dirs + 4 * k, outs + 4 * k );
}

// 0x548650 getPreferredPenetrationDirection(index, out&): +-x, +-y, +-z
// for 0..5 (a jump table), w = 0; any other index writes nothing.
inline void box_preferred_direction( int index, float* out )
{
    if ( static_cast< unsigned >( index ) > 5 )
        return;
    for ( int k = 0; k < 4; ++k )
        out[ k ] = 0.0f;
    out[ index / 2 ] = ( index & 1 ) ? -1.0f : 1.0f;
}

// 0x548460 getVertex(i, out&): lane k = (1 - bit k) e - (bit k) e of the
// half extent with the margin e (bits 0 / 1 / 2 of i: x / y / z, each
// converted by cvtdq2ps); w = 0.
inline void box_vertex( const std::uint8_t* s, int i, float* out )
{
    const float m0 = box_margin( s ), m1 = box_margin( s ), m2 = box_margin( s );
    const float e[ 3 ] = { box_lane( s, 0x20 ) + m2, box_lane( s, 0x24 ) + m1, box_lane( s, 0x28 ) + m0 };
    for ( int k = 0; k < 3; ++k )
    {
        const int bit = ( i >> k ) & 1;
        out[ k ] = static_cast< float >( 1 - bit ) * e[ k ] - static_cast< float >( bit ) * e[ k ];
    }
    out[ 3 ] = 0.0f;
}

// 0x548580 getEdge(i, pa&, pb&): the vertex pair of edge i (0x548620's
// table: 0-1 0-2 1-3 2-3 0-4 1-5 2-6 3-7 4-5 4-6 5-7 6-7; any other i 0-0),
// each through getVertex (vtable slot 25).
inline void box_edge( const std::uint8_t* s, int i, float* pa, float* pb )
{
    static const int pairs[ 12 ][ 2 ] = { { 0, 1 }, { 0, 2 }, { 1, 3 }, { 2, 3 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
                                         { 4, 5 }, { 4, 6 }, { 5, 7 }, { 6, 7 } };
    const bool known = static_cast< unsigned >( i ) <= 11;
    box_vertex( s, known ? pairs[ i ][ 0 ] : 0, pa );
    box_vertex( s, known ? pairs[ i ][ 1 ] : 0, pb );
}

// 0x548250 getPlaneEquation(plane&, i): the face normal +-x / +-y / +-z
// (0x548350's table) and w = -(half extent WITHOUT the margin) on that
// axis (xorps sign); any other i writes nothing.
inline void box_plane_equation( const std::uint8_t* s, float* plane, int i )
{
    if ( static_cast< unsigned >( i ) > 5 )
        return;
    const int axis = i / 2;
    for ( int k = 0; k < 3; ++k )
        plane[ k ] = 0.0f;
    plane[ axis ] = ( i & 1 ) ? -1.0f : 1.0f;
    plane[ 3 ] = box_negate( box_lane( s, 0x20 + 4 * axis ) );
}

// 0x548370 getPlane(normal&, support&, i): getPlaneEquation (slot 29) into
// a stack plane, the normal = its xyz with w = 0, the support =
// localGetSupportingVertex (slot 14) of -normal. (For i > 5 the stack plane
// is never written: retail then reads stack residue - not modelled; callers
// here pass 0..5.)
inline void box_plane( const std::uint8_t* s, float* normal, float* support, int i )
{
    float plane[ 4 ] = { 0, 0, 0, 0 };
    box_plane_equation( s, plane, i );
    normal[ 0 ] = plane[ 0 ];
    normal[ 1 ] = plane[ 1 ];
    normal[ 2 ] = plane[ 2 ];
    normal[ 3 ] = 0.0f;
    const float away[ 4 ] = { box_negate( normal[ 0 ] ), box_negate( normal[ 1 ] ), box_negate( normal[ 2 ] ), 0.0f };
    box_support( s, away, support );
}

// 0x5481A0 isInside(point, tolerance): per lane h + tol >= p and p >= -h -
// tol (comiss / jb: a NaN fails), h the half extent WITHOUT the margin.
inline bool box_is_inside( const std::uint8_t* s, const float* p, float tolerance )
{
    for ( int k = 0; k < 3; ++k )
    {
        const float h = box_lane( s, 0x20 + 4 * k );
        if ( !( tolerance + h >= p[ k ] ) )
            return false;
        if ( !( p[ k ] >= box_negate( h ) - tolerance ) )
            return false;
    }
    return true;
}