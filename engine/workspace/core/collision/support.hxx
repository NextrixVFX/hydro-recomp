#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

// btConvexShape::localGetSupportVertexWithoutMarginNonVirtual @0x553980
// (2.79), recreated from retail's disassembly (memory/audit.md "Batch 8c").
// A switch on the shape type at +4. The ids are the ctors' own stores: box
// 0 (0x547D80), triangle 1 (0x5B7F10), convex triangle mesh 3 (0x526380 -
// every authored "convex hull" and the boat's hull), sphere 8 (0x43C700),
// cylinder 13 (0x5AE160; X / Y / Z differ only in the up axis at +64).
// Types 4 / 5 (convex hull / point cloud) and the capsule (10) are inline
// cases the content never builds; every other type - 3 included - goes to
// the shape's virtual localGetSupportingVertexWithoutMargin (vt+0x3C), a
// caller hook here until that code is recreated.
//
// The fields each case reads, at retail's offsets: +16 local scaling, +32
// implicit dimensions (box half extents, cylinder half extents, capsule
// radius / half height by axis), +48 margin, +64 up axis, +80 the triangle's
// three vertices, +128 / +132 / +140 the point cloud's points, the point
// count, the hull's points.
// VuStridingMesh (vt 0x7380C4; VuRigidBodyComponent's ctor builds it at
// component+0xA0, and the hull shape keeps it at +0x80). Its
// getLockedReadOnlyVertexIndexBase @0x527C50 hands out the asset's arrays
// with constant types: vertices asset+0x58 (PHY_FLOAT, stride 16), the
// vertex count asset+0x5C, indices asset+0x64 (PHY_SHORT, stride 6), faces =
// asset+0x68 / 3 (signed, truncating); getNumSubParts @0x527CC0 is
// asset+0x68 != 0. m_scaling is at +16.
struct striding_mesh_t
{
    float scaling[ 4 ];
    const float ( *vertices )[ 4 ];
    int num_vertices;
    const uint16_t* indices;
    int index_count;
};

struct convex_shape_t
{
    int type;
    float local_scaling[ 4 ];
    float implicit[ 4 ];
    float margin;
    int up_axis;
    float vertices[ 3 ][ 4 ];
    const float ( *points )[ 4 ];   // +140 for type 4, +128 for type 5
    int num_points;                 // +132
    const striding_mesh_t* mesh = nullptr;   // +128 for type 3
    // +64 for the polyhedral types (< 7: btPolyhedralConvexShape::m_polyhedron,
    // the same slot as the cylinder's up axis). ConvexConvex reads only whether
    // it is null; nothing in the content is known to set it (audit.md Batch 8b).
    const void* polyhedron = nullptr;
    // +0x50 / +0x60 for type 3 (btPolyhedralConvexAabbCachingShape's cached
    // local aabb, which getAabb 0x54B6D0 -> 0x54BC60 reads; written by
    // recalcLocalAabb 0x54B700 - shape_recalc_local_aabb below).
    float local_aabb_min[ 4 ] = {};
    float local_aabb_max[ 4 ] = {};
    void* user = nullptr;
    // vt+0x3C (this, out, dir): the shape's own virtual. Absent -> false.
    bool ( *virtual_support )( void* user, const convex_shape_t& shape, const float* dir, float* out ) = nullptr;
};

inline float support_bits( uint32_t u )
{
    float f;
    std::memcpy( &f, &u, 4 );
    return f;
}

// convexHullSupport @0x5538B0 (ecx = out, edx = dir; points, count,
// scaling on the stack, caller pops): v = dir * scaling, the first point of
// strictly greatest (p.y v.y + p.x v.x) + v.z p.z from -1e18 (0x6F7908),
// out = p * scaling, w = 0. With no point above -1e18 retail reads
// points[-1] - OUT OF BOUNDS, an UNKNOWN: return false there.
inline bool support_hull( const float ( *points )[ 4 ], int count, const float* scaling, const float* dir, float* out )
{
    const float vx = dir[ 0 ] * scaling[ 0 ];
    const float vy = dir[ 1 ] * scaling[ 1 ];
    const float vz = dir[ 2 ] * scaling[ 2 ];
    float best = support_bits( 0xDD5E0B6Bu );
    int index = -1;
    for ( int i = 0; i < count; ++i )
    {
        const float dot = ( points[ i ][ 1 ] * vy + points[ i ][ 0 ] * vx ) + vz * points[ i ][ 2 ];
        if ( dot > best )   // `comiss dot, best; jbe`
        {
            best = dot;
            index = i;
        }
    }
    if ( index < 0 )
        return false;   // UNKNOWN: retail reads points[-1]
    out[ 3 ] = 0.0f;
    out[ 0 ] = points[ index ][ 0 ] * scaling[ 0 ];
    out[ 1 ] = points[ index ][ 1 ] * scaling[ 1 ];
    out[ 2 ] = points[ index ][ 2 ] * scaling[ 2 ];
    return true;
}

inline float support_negate( float x )
{
    uint32_t u;
    std::memcpy( &u, &x, 4 );
    u ^= 0x80000000u;   // xorps with the 0x6F7CD0 sign mask
    std::memcpy( &x, &u, 4 );
    return x;
}

// btConvexTriangleMeshShape::localGetSupportingVertexWithoutMargin @0x526570
// with the walk it drives: out = 0; vec = dir * (1 / sqrt(len2)) unless 1e-4
// (0x6F75A4) > len2 = (y^2 + x^2) + z^2, then (1, 0, 0) (xmmword_6F7930);
// LocalSupportVertexCallback (support +16 = 0, maxDot +32 = -1e18
// 0xDD5E0B6B, vec +48) through btStridingMeshInterface::
// InternalProcessAllTriangles @0x5180A0 (the aabb it is given, -1e18 .. 1e18,
// is never read). The walk's only reachable branch for a VuStridingMesh is
// PHY_FLOAT vertices with PHY_SHORT indices: each corner is the vertex times
// the mesh scaling, w = 0. internalProcessTriangleIndex @0x526480 takes, per
// corner in order, a dot (v.y vec.y + vec.x v.x) + v.z vec.z strictly above
// the best (`comiss; jbe`) and copies that corner's four lanes.
inline void support_triangle_mesh_walk( const striding_mesh_t& mesh, const float* vec, float* out )
{
    float best = support_bits( 0xDD5E0B6Bu );
    float support[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
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
            for ( int k = 0; k < 3; ++k )
            {
                const float dot = ( corner[ k ][ 1 ] * vec[ 1 ] + vec[ 0 ] * corner[ k ][ 0 ] ) + corner[ k ][ 2 ] * vec[ 2 ];
                if ( dot > best )
                {
                    best = dot;
                    std::memcpy( support, corner[ k ], 16 );
                }
            }
        }
    }
    std::memcpy( out, support, 16 );
}

inline void support_triangle_mesh( const striding_mesh_t& mesh, const float* dir, float* out )
{
    float vec[ 3 ];
    const float len2 = ( dir[ 1 ] * dir[ 1 ] + dir[ 0 ] * dir[ 0 ] ) + dir[ 2 ] * dir[ 2 ];
    if ( support_bits( 0x38D1B717u ) > len2 )
    {
        vec[ 0 ] = 1.0f;
        vec[ 1 ] = 0.0f;
        vec[ 2 ] = 0.0f;
    }
    else
    {
        const float rlen = 1.0f / std::sqrt( len2 );
        vec[ 0 ] = dir[ 0 ] * rlen;
        vec[ 1 ] = dir[ 1 ] * rlen;
        vec[ 2 ] = dir[ 2 ] * rlen;
    }
    support_triangle_mesh_walk( mesh, vec, out );
}

// btPolyhedralConvexAabbCachingShape::recalcLocalAabb @0x54B700 for type 3:
// the valid byte +0x70 = 1 (no reader known - not modelled); the six static
// directions (+x, +y, +z, -x, -y, -z: .rdata 0x6F7930 / 0x6F7970 / 0x6F7AD0
// / 0x6F7940 / 0x6F79C0 / 0x6F7C60) through batchedUnitVectorGetSupporting
// VertexWithoutMargin (vt+0x44 = 0x5266F0: per direction the same walk as
// 0x526570 with the direction NOT normalised, the 16-byte support copied
// out); then max.x = margin + s0.x, min.x = s3.x - margin, max.y, min.y,
// max.z, min.z in that order - the w lanes are not written. The margin is
// +0x30 read directly (not the virtual).
inline void shape_recalc_local_aabb( convex_shape_t& s )
{
    static const float dirs[ 6 ][ 4 ] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { -1, 0, 0, 0 }, { 0, -1, 0, 0 }, { 0, 0, -1, 0 } };
    float sup[ 6 ][ 4 ] = {};
    for ( int k = 0; k < 6; ++k )
        support_triangle_mesh_walk( *s.mesh, dirs[ k ], sup[ k ] );
    s.local_aabb_max[ 0 ] = s.margin + sup[ 0 ][ 0 ];
    s.local_aabb_min[ 0 ] = sup[ 3 ][ 0 ] - s.margin;
    s.local_aabb_max[ 1 ] = s.margin + sup[ 1 ][ 1 ];
    s.local_aabb_min[ 1 ] = sup[ 4 ][ 1 ] - s.margin;
    s.local_aabb_max[ 2 ] = s.margin + sup[ 2 ][ 2 ];
    s.local_aabb_min[ 2 ] = sup[ 5 ][ 2 ] - s.margin;
}

// btConvexTriangleMeshShape(mesh, calcAabb) @0x526380 (thiscall; retn 8) -
// the boat loader 0x412270 inlines the same sequence: the base ctor 0x54B650
// (scaling 1, margin 0.04 0x3D23D70A, the cached aabb (1, 1, 1, 0) / (-1, -1,
// -1, 0), valid 0), type 3, the mesh at +0x80, then recalcLocalAabb
// UNCONDITIONALLY (calcAabb is never tested).
inline void shape_init_triangle_mesh_hull( convex_shape_t& s, const striding_mesh_t* mesh )
{
    s = convex_shape_t{};
    s.type = 3;
    s.local_scaling[ 0 ] = s.local_scaling[ 1 ] = s.local_scaling[ 2 ] = 1.0f;
    s.margin = support_bits( 0x3D23D70Au );
    s.mesh = mesh;
    shape_recalc_local_aabb( s );
}

// setLocalScaling @0x526970 (slot 5; thiscall scaling*; retn 4): the four
// lanes into the MESH's +16 (the shape's own +0x10 is untouched), then
// recalcLocalAabb. setMargin (slot 9 0x43D630) is a plain store - no recalc.
inline void shape_set_triangle_mesh_scaling( convex_shape_t& s, striding_mesh_t& mesh, const float* scaling )
{
    std::memcpy( mesh.scaling, scaling, 16 );
    shape_recalc_local_aabb( s );
}

inline bool shape_support( const convex_shape_t& s, const float* dir, float* out )
{
    switch ( s.type )
    {
    case 0:
    {
        // `comiss d, 0; jnb keep`: negative OR NaN flips the half extent.
        const float z = dir[ 2 ] >= 0.0f ? s.implicit[ 2 ] : support_negate( s.implicit[ 2 ] );
        const float y = dir[ 1 ] >= 0.0f ? s.implicit[ 1 ] : support_negate( s.implicit[ 1 ] );
        const float x = dir[ 0 ] >= 0.0f ? s.implicit[ 0 ] : support_negate( s.implicit[ 0 ] );
        out[ 0 ] = x;
        out[ 1 ] = y;
        out[ 2 ] = z;
        out[ 3 ] = 0.0f;
        return true;
    }
    case 1:
    {
        const float* v0 = s.vertices[ 0 ];
        const float* v1 = s.vertices[ 1 ];
        const float* v2 = s.vertices[ 2 ];
        const float d0 = ( v0[ 1 ] * dir[ 1 ] + v0[ 0 ] * dir[ 0 ] ) + v0[ 2 ] * dir[ 2 ];
        const float d1 = ( v1[ 1 ] * dir[ 1 ] + dir[ 0 ] * v1[ 0 ] ) + v1[ 2 ] * dir[ 2 ];
        const float d2 = ( v2[ 1 ] * dir[ 1 ] + v2[ 0 ] * dir[ 0 ] ) + v2[ 2 ] * dir[ 2 ];
        int i;
        // `comiss d1, d0; jbe` then setnbe / cmova - ordered greater.
        if ( d1 > d0 )
            i = ( d2 > d1 ) ? 2 : 1;
        else
            i = ( d2 > d0 ) ? 2 : 0;
        out[ 0 ] = s.vertices[ i ][ 0 ];
        out[ 1 ] = s.vertices[ i ][ 1 ];
        out[ 2 ] = s.vertices[ i ][ 2 ];
        out[ 3 ] = 0.0f;
        return true;
    }
    case 4:
        return support_hull( s.points, s.num_points, s.local_scaling, dir, out );
    case 5:
        return support_hull( s.points, s.num_points, s.local_scaling, dir, out );
    case 8:
        out[ 0 ] = out[ 1 ] = out[ 2 ] = out[ 3 ] = 0.0f;
        return true;
    case 10:
    {
        // The capsule: up = +64, half height implicit[up], radius
        // implicit[(up + 2) % 3]; vec = dir normalized unless 1e-4
        // (0x6F75A4) > |dir|^2 (then (1, 0, 0)); the better of the two cap
        // centres pushed out by radius * scaling and in by the margin,
        // strictly above -1e18, else zero.
        const int up = s.up_axis;
        const float half = s.implicit[ up ];
        const float radius = s.implicit[ ( up + 2 ) % 3 ];
        float vx = dir[ 0 ], vy = dir[ 1 ], vz = dir[ 2 ];
        const float len2 = ( vy * vy + vx * vx ) + vz * vz;
        if ( support_bits( 0x38D1B717u ) > len2 )
        {
            vx = 1.0f;
            vy = 0.0f;
            vz = 0.0f;
        }
        else
        {
            const float rlen = 1.0f / std::sqrt( len2 );
            vx = vx * rlen;
            vy = vy * rlen;
            vz = vz * rlen;
        }
        const float m = s.margin;
        const float sx = s.local_scaling[ 0 ] * vx;
        const float sy = s.local_scaling[ 1 ] * vy;
        const float sz = s.local_scaling[ 2 ] * vz;
        float best = support_bits( 0xDD5E0B6Bu );
        float sup[ 3 ] = { 0.0f, 0.0f, 0.0f };
        for ( int k = 0; k < 2; ++k )
        {
            float pos[ 3 ] = { 0.0f, 0.0f, 0.0f };
            pos[ up ] = k == 0 ? half : support_negate( half );
            const float x = ( sx * radius + pos[ 0 ] ) - vx * m;
            const float y = ( sy * radius + pos[ 1 ] ) - vy * m;
            const float z = ( sz * radius + pos[ 2 ] ) - vz * m;
            const float dot = ( y * vy + x * vx ) + z * vz;
            if ( dot > best )   // `comiss dot, best; jbe`
            {
                best = dot;
                sup[ 0 ] = x;
                sup[ 1 ] = y;
                sup[ 2 ] = z;
            }
        }
        out[ 0 ] = sup[ 0 ];
        out[ 1 ] = sup[ 1 ];
        out[ 2 ] = sup[ 2 ];
        out[ 3 ] = 0.0f;
        return true;
    }
    case 13:
    {
        // XX / YY (up) / ZZ by axis: 0 -> (1, 0, 2), 1 -> (0, 1, 2),
        // 2 -> (0, 2, 1). radius = implicit[XX], half height = implicit[up].
        int xx = 1, yy = 0, zz = 2;
        if ( s.up_axis == 1 )
            xx = 0, yy = 1, zz = 2;
        else if ( s.up_axis == 2 )
            xx = 0, yy = 2, zz = 1;
        else if ( s.up_axis != 0 )
            return false;   // UNKNOWN: implicit[up] read out of range; no ctor stores such an axis
        const float radius = s.implicit[ xx ];
        const float half = s.implicit[ s.up_axis ];
        float tmp[ 3 ];
        const float d = std::sqrt( dir[ xx ] * dir[ xx ] + dir[ zz ] * dir[ zz ] );
        // `comiss 0, v[YY]; jbe` - only an ordered negative flips.
        const float h = 0.0f > dir[ yy ] ? support_negate( half ) : half;
        // `ucomiss d, 0; lahf; test ah, 44h; jnp zero` - ordered equality.
        if ( d == 0.0f )
        {
            tmp[ xx ] = radius;
            tmp[ yy ] = h;
            tmp[ zz ] = 0.0f;
        }
        else
        {
            const float r = radius / d;
            tmp[ xx ] = dir[ xx ] * r;
            tmp[ yy ] = h;
            tmp[ zz ] = dir[ zz ] * r;
        }
        out[ 0 ] = tmp[ 0 ];
        out[ 1 ] = tmp[ 1 ];
        out[ 2 ] = tmp[ 2 ];
        out[ 3 ] = 0.0f;
        return true;
    }
    default:
        // Type 3 is stored only beside btConvexTriangleMeshShape's vtable
        // (0x526380, 0x412270, 0x420A70), whose slot 15 is 0x526570.
        if ( s.type == 3 && s.mesh )
        {
            support_triangle_mesh( *s.mesh, dir, out );
            return true;
        }
        if ( !s.virtual_support )
            return false;
        return s.virtual_support( s.user, s, dir, out );
    }
}

// The virtual getMargin (vt+0x28, returned on the x87 stack) of the four
// shape vtables in the content, read from their bytes: box 0x74DEF4,
// cylinder 0x751494 and hull 0x74CEBC share 0x43D5C0 (`fld [ecx+30h]`, the
// +48 margin); the sphere 0x73ADD4's 0x43D650 is +32 * +16. Any other type:
// false (not read).
inline bool shape_get_margin( const convex_shape_t& s, float& out )
{
    switch ( s.type )
    {
    case 0: case 1: case 3: case 13:   // 0x43D5C0 = +48 (the triangle's vt 0x751C7C slot 10 too)
        out = s.margin;
        return true;
    case 8:
        out = s.implicit[ 0 ] * s.local_scaling[ 0 ];
        return true;
    default:
        return false;
    }
}

// btConvexShape::localGetSupportVertexNonVirtual @0x5540B0 (thiscall; out*,
// dir*; retn 8): vec = dir, (-1, -1, -1, 0) (xmmword 0x6F7C80) when 1.42e-14
// (0x6F7578) > (y^2 + x^2) + z^2 (`comiss; jbe`); vec *= 1 / sqrt of the same
// sum of vec; the margin by type - 0, 1, 4, 5, 10, 13 read +48, the sphere 8
// +32 * +16, every other type its vt+40 getMargin (type 3's is 0x43D5C0 =
// +48; others return false); out = support(vec) + vec * margin, w = 0.
inline bool shape_support_margin( const convex_shape_t& s, const float* dir, float* out )
{
    float vec[ 4 ];
    std::memcpy( vec, dir, 16 );
    float x = vec[ 0 ], y = vec[ 1 ], z = vec[ 2 ];
    if ( support_bits( 0x28800000u ) > ( y * y + x * x ) + z * z )
    {
        const uint32_t minus[ 4 ] = { 0xBF800000u, 0xBF800000u, 0xBF800000u, 0u };
        std::memcpy( vec, minus, 16 );
        x = vec[ 0 ], y = vec[ 1 ], z = vec[ 2 ];
    }
    const float r = 1.0f / std::sqrt( ( y * y + x * x ) + z * z );
    vec[ 0 ] = x * r;
    vec[ 1 ] = y * r;
    vec[ 2 ] = z * r;
    float margin;
    switch ( s.type )
    {
    case 0: case 1: case 4: case 5: case 10: case 13:
        margin = s.margin;
        break;
    case 8:
        margin = s.implicit[ 0 ] * s.local_scaling[ 0 ];
        break;
    case 3:
        margin = s.margin;
        break;
    default:
        return false;   // the shape's own getMargin (vt+40), not recreated
    }
    const float mx = vec[ 0 ] * margin, my = vec[ 1 ] * margin, mz = vec[ 2 ] * margin;
    float sv[ 4 ];
    if ( !shape_support( s, vec, sv ) )
        return false;
    out[ 3 ] = 0.0f;
    out[ 2 ] = sv[ 2 ] + mz;
    out[ 1 ] = sv[ 1 ] + my;
    out[ 0 ] = sv[ 0 ] + mx;
    return true;
}