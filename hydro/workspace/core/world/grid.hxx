#pragma once

#include <workspace/core/world/kernel.hxx>
#include <workspace/core/world/query.hxx>
#include <workspace/core/world/surface.hxx>
#include <workspace/core/world/wave.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// THE WATER RENDERER'S CPU SIDE (VuWaterRenderer, ctor 0x594DD0), recreated
// from retail's disassembly / decompilation:
//
//   0x5979C0  the worker thread's loop: per viewport (camera = viewport +40),
//             every surface in VuWater's list (+0x20) whose box (+208) passes
//             the camera frustum 0x57D280 (identity matrix 0x7593B0) is built
//             by 0x597B50. +0x58 = 2 / VuWater+0x40, +0x5C = VuWater+0x40 * 4.
//   0x597B50  the surface's matrix (+0x10): T(-size/2) . Rz . T(position), the
//             patch quadtree 0x597110 / 0x597260, the edge stitch 0x597700, a
//             qsort by distance (0x594D90), then the vertices 0x597D70 and one
//             draw record (15 words) per surface.
//   0x595D40  the sixteen patch templates (one per stitched-edge mask, +0x70,
//             24 bytes each), triangles through 0x5970B0.
//   0x597D70  per patch: the template's vertices through the matrix into
//             32-byte records, its indices rebased, then VuWater's query
//             0x528FC0 in its +96 mode with the surface FORCED (ctx+0x4C),
//             which fills +8 height, +0xC / +0x10 gradient, +0x14 foam, +0x18
//             1.0, +0x1C 0 and runs the surface's waves' +96 kernels; then the
//             water map's three byte maps (surface +0x100).
//
// HOOKS / NOT RECREATED, said once here:
//   - the thread, the dev profile, the dev menu flags (+0x6C "Water/Normals",
//     +0x6D "Water/Flow": 0 from the ctor; their line draws 0x597850 /
//     0x5978E0 are debug-only and not recreated);
//   - +0x6E "Water/ClipMaps" is 1 from the ctor (water_grid_t::clip_maps):
//     0x597260 asks the clip test 0x5C7E80 only while it is set and the
//     surface has a VuWaterMapAsset (+0x100, surface.hxx water_build_maps).
//     The three byte-map terms of 0x597D70 depend on the map alone.
//   - qsort is the CRT DLL's: SUBSTITUTION std::stable_sort with the same
//     comparator (descending +0xC). Ties order differently from MSVC's qsort;
//     that changes only where a patch's vertices sit in the buffer.
//   - VuWater +0x40 is 1.0 from its ctor 0x528050; other writers not searched.

// ---------------------------------------------------------------------------
// THE CAMERA'S FRUSTUM BLOCK (VuCamera +388 .. +476), 0x50C550 after the
// projection setter 0x50BBE0 (+368 fov, +372 aspect, +376 near, +380 far, +384
// tanf of the fov): [0..3] the transform's row 3 (+352, the eye), [4..7] row 1
// (+320, forward), [8..11] row 2 (+336, up), [12..15] row 0 (+304, right),
// [16] near, [17] far, [19] near * +384, [18] that / aspect, [20] far / near,
// [21] [18] * -2 * far, [22] [19] * -2 * far. The planes at +480 (0x57CBA0)
// are not read by the water renderer.
struct water_frustum_t
{
    float f[ 23 ]{};
    float position[ 3 ]{};   // VuCamera +0: the distance test's eye
};

inline water_frustum_t water_frustum_build( const float* eye, const float* right, const float* forward, const float* up, float aspect,
    float near_z, float far_z, float tangent )
{
    water_frustum_t r{};
    for ( int k = 0; k < 3; ++k )
    {
        r.f[ k ] = eye[ k ];
        r.f[ 4 + k ] = forward[ k ];
        r.f[ 8 + k ] = up[ k ];
        r.f[ 12 + k ] = right[ k ];
        r.position[ k ] = eye[ k ];
    }
    const float v3 = tangent * near_z;
    r.f[ 16 ] = near_z;
    r.f[ 17 ] = far_z;
    r.f[ 19 ] = v3;
    r.f[ 18 ] = v3 / aspect;
    r.f[ 20 ] = r.f[ 17 ] / r.f[ 16 ];
    r.f[ 21 ] = ( r.f[ 18 ] * -2.0f ) * r.f[ 17 ];
    r.f[ 22 ] = ( r.f[ 19 ] * -2.0f ) * r.f[ 17 ];
    return r;
}

// 0x57D280 (thiscall frustum; aabb* - min at +0, max at +16; matrix*): the
// box through the matrix as an oriented box, separating-axis tested against
// the frustum - the forward axis, the right / up planes, the box's three
// axes, then the cross products. Transliterated from the decompilation in its
// operand order; true = may be visible.
inline bool water_frustum_test( const water_frustum_t& fr, const float* box, const float* m )
{
    const float* f = fr.f;
    const float cx = ( box[ 4 ] + box[ 0 ] ) * 0.5f;
    const float cy = ( box[ 5 ] + box[ 1 ] ) * 0.5f;
    const float cz = ( box[ 6 ] + box[ 2 ] ) * 0.5f;
    float wx = m[ 0 ] * cx;
    wx = ( ( wx + ( m[ 4 ] * cy ) ) + ( m[ 8 ] * cz ) ) + m[ 12 ];
    const float wy = ( ( ( m[ 5 ] * cy ) + ( m[ 1 ] * cx ) ) + ( m[ 9 ] * cz ) ) + m[ 13 ];
    const float wz = ( ( ( m[ 6 ] * cy ) + ( m[ 2 ] * cx ) ) + ( m[ 10 ] * cz ) ) + m[ 14 ];
    const float ex = ( box[ 4 ] - box[ 0 ] ) * 0.5f;
    const float ey = ( box[ 5 ] - box[ 1 ] ) * 0.5f;
    const float ez = ( box[ 6 ] - box[ 2 ] ) * 0.5f;
    const float l0 = std::sqrt( ( ( m[ 0 ] * m[ 0 ] ) + ( m[ 1 ] * m[ 1 ] ) ) + ( m[ 2 ] * m[ 2 ] ) );
    const float l1 = std::sqrt( ( ( m[ 4 ] * m[ 4 ] ) + ( m[ 5 ] * m[ 5 ] ) ) + ( m[ 6 ] * m[ 6 ] ) );
    const float l2 = std::sqrt( ( ( m[ 8 ] * m[ 8 ] ) + ( m[ 9 ] * m[ 9 ] ) ) + ( m[ 10 ] * m[ 10 ] ) );
    const float e[ 3 ] = { l0 * ex, l1 * ey, l2 * ez };   // v142
    const float dx = wx - f[ 0 ];
    const float dy = wy - f[ 1 ];
    const float dz = wz - f[ 2 ];
    // The box's unit axes (rows / their lengths).
    const float a0[ 3 ] = { ( 1.0f / l0 ) * m[ 0 ], ( 1.0f / l0 ) * m[ 1 ], ( 1.0f / l0 ) * m[ 2 ] };
    const float a1[ 3 ] = { ( 1.0f / l1 ) * m[ 4 ], ( 1.0f / l1 ) * m[ 5 ], ( 1.0f / l1 ) * m[ 6 ] };
    const float a2[ 3 ] = { ( 1.0f / l2 ) * m[ 8 ], ( 1.0f / l2 ) * m[ 9 ], ( 1.0f / l2 ) * m[ 10 ] };
    auto dot = []( const float* p, const float* a ) { return ( ( p[ 0 ] * a[ 0 ] ) + ( p[ 1 ] * a[ 1 ] ) ) + ( p[ 2 ] * a[ 2 ] ); };
    const float* fw = f + 4;
    const float* upv = f + 8;
    const float* rt = f + 12;
    const float near_z = f[ 16 ];
    const float d[ 3 ] = { dx, dy, dz };
    const float df = ( ( dx * fw[ 0 ] ) + ( dy * fw[ 1 ] ) ) + ( dz * fw[ 2 ] );   // v177
    const float fa[ 3 ] = { dot( fw, a0 ), dot( fw, a1 ), dot( fw, a2 ) };       // v164
    // 1. the forward axis against [near, far].
    float r = ( water_abs( fa[ 1 ] ) * e[ 1 ] ) + ( water_abs( fa[ 0 ] ) * e[ 0 ] );
    r = r + ( water_abs( fa[ 2 ] ) * e[ 2 ] );
    if ( !( f[ 16 ] <= ( df + r ) && ( df - r ) <= f[ 17 ] ) )
        return false;
    // 2. the two right planes.
    const float ra[ 3 ] = { dot( rt, a0 ), dot( rt, a1 ), dot( rt, a2 ) };   // v155
    const float rn[ 3 ] = { ra[ 0 ] * near_z, ra[ 1 ] * near_z, ra[ 2 ] * near_z };   // v149 (first use)
    const float s19[ 3 ] = { f[ 19 ] * fa[ 0 ], f[ 19 ] * fa[ 1 ], f[ 19 ] * fa[ 2 ] };
    const float p144[ 3 ] = { rn[ 0 ] - s19[ 0 ], rn[ 1 ] - s19[ 1 ], rn[ 2 ] - s19[ 2 ] };
    const float dr = ( ( rt[ 0 ] * dx ) + ( rt[ 1 ] * dy ) ) + ( rt[ 2 ] * dz );   // v180
    r = ( water_abs( p144[ 1 ] ) * e[ 1 ] ) + ( water_abs( p144[ 0 ] ) * e[ 0 ] );
    r = r + ( water_abs( p144[ 2 ] ) * e[ 2 ] );
    float pn = dr * near_z;
    float pf = f[ 19 ] * df;
    if ( !( f[ 22 ] <= ( ( pn - pf ) + r ) && ( pn - pf ) <= r ) )
        return false;
    const float p143[ 3 ] = { rn[ 0 ] + s19[ 0 ], rn[ 1 ] + s19[ 1 ], rn[ 2 ] + s19[ 2 ] };
    r = ( water_abs( p143[ 1 ] ) * e[ 1 ] ) + ( water_abs( p143[ 0 ] ) * e[ 0 ] );
    r = r + ( water_abs( p143[ 2 ] ) * e[ 2 ] );
    if ( !( f[ 22 ] <= ( r - ( pf + pn ) ) && -( pf + pn ) <= r ) )
        return false;
    // 3. the two up planes.
    const float ua[ 3 ] = { dot( upv, a0 ), dot( upv, a1 ), dot( upv, a2 ) };   // v167
    const float un[ 3 ] = { ua[ 0 ] * near_z, ua[ 1 ] * near_z, ua[ 2 ] * near_z };   // v145 (first use)
    const float s18[ 3 ] = { f[ 18 ] * fa[ 0 ], f[ 18 ] * fa[ 1 ], f[ 18 ] * fa[ 2 ] };
    const float p152[ 3 ] = { un[ 0 ] - s18[ 0 ], un[ 1 ] - s18[ 1 ], un[ 2 ] - s18[ 2 ] };
    const float du = ( ( upv[ 0 ] * dx ) + ( upv[ 1 ] * dy ) ) + ( upv[ 2 ] * dz );   // v183
    r = ( water_abs( p152[ 1 ] ) * e[ 1 ] ) + ( water_abs( p152[ 0 ] ) * e[ 0 ] );
    r = r + ( water_abs( p152[ 2 ] ) * e[ 2 ] );
    pf = f[ 18 ] * df;
    pn = du * near_z;
    if ( !( f[ 21 ] <= ( ( pn - pf ) + r ) && ( pn - pf ) <= r ) )
        return false;
    const float p148[ 3 ] = { un[ 0 ] + s18[ 0 ], un[ 1 ] + s18[ 1 ], un[ 2 ] + s18[ 2 ] };
    r = ( water_abs( p148[ 1 ] ) * e[ 1 ] ) + ( water_abs( p148[ 0 ] ) * e[ 0 ] );
    r = r + ( water_abs( p148[ 2 ] ) * e[ 2 ] );
    if ( !( f[ 21 ] <= ( r - ( pf + pn ) ) && -( pf + pn ) <= r ) )
        return false;
    // 4. the box's own three axes.
    for ( int k = 0; k < 3; ++k )
    {
        const float a = water_abs( ua[ k ] ) * f[ 18 ];
        const float b = a + ( water_abs( ra[ k ] ) * f[ 19 ] );
        const float c = near_z * fa[ k ];
        float lo = c - b;
        if ( lo < 0.0f )
            lo = lo * f[ 20 ];
        float hi = b + c;
        if ( hi > 0.0f )
            hi = f[ 20 ] * hi;
        const float p = ( ( dr * ra[ k ] ) + ( du * ua[ k ] ) ) + ( df * fa[ k ] );
        if ( lo > ( e[ k ] + p ) || ( p - e[ k ] ) > hi )
            return false;
    }
    // 5. forward x each box axis.
    for ( int k = 0; k < 3; ++k )
    {
        const float a = water_abs( fa[ k ] ) * f[ 18 ];
        float lo = -un[ k ] - a;
        if ( lo < 0.0f )
            lo = f[ 20 ] * lo;
        float hi = a - un[ k ];
        if ( hi > 0.0f )
            hi = f[ 20 ] * hi;
        const float p = ( du * fa[ k ] ) - ( df * ua[ k ] );
        float s = water_abs( ( fa[ 1 ] * ua[ k ] ) - ( ua[ 1 ] * fa[ k ] ) ) * e[ 1 ];
        s = s + ( water_abs( ( fa[ 0 ] * ua[ k ] ) - ( ua[ 0 ] * fa[ k ] ) ) * e[ 0 ] );
        s = s + ( water_abs( ( fa[ 2 ] * ua[ k ] ) - ( ua[ 2 ] * fa[ k ] ) ) * e[ 2 ] );
        if ( lo > ( p + s ) || ( p - s ) > hi )
            return false;
    }
    for ( int k = 0; k < 3; ++k )
    {
        const float a = water_abs( fa[ k ] ) * f[ 19 ];
        float lo = rn[ k ] - a;
        if ( lo < 0.0f )
            lo = f[ 20 ] * lo;
        float hi = rn[ k ] + a;
        if ( hi > 0.0f )
            hi = f[ 20 ] * hi;
        const float p = ( df * ra[ k ] ) - ( dr * fa[ k ] );
        float s = water_abs( ( fa[ 1 ] * ra[ k ] ) - ( ra[ 1 ] * fa[ k ] ) ) * e[ 1 ];
        s = s + ( water_abs( ( fa[ 0 ] * ra[ k ] ) - ( ra[ 0 ] * fa[ k ] ) ) * e[ 0 ] );
        s = s + ( water_abs( ( fa[ 2 ] * ra[ k ] ) - ( ra[ 2 ] * fa[ k ] ) ) * e[ 2 ] );
        if ( lo > ( p + s ) || ( p - s ) > hi )
            return false;
    }
    // 6. the four side edges' directions x each box axis (v149 / v145
    // rewritten: f18 * right +- f19 * up projections).
    float q149[ 3 ];
    float q145[ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        q149[ k ] = ( f[ 18 ] * ra[ k ] ) + ( ua[ k ] * f[ 19 ] );
        q145[ k ] = ( ua[ k ] * f[ 19 ] ) - ( f[ 18 ] * ra[ k ] );
    }
    // Each loop: (u, v, w) per j, the side's slab, then the box's radius.
    auto edge = [ & ]( const float* u, float su, const float* v, float sv, const float* w, bool far_sign, bool plus ) {
        for ( int j = 0; j < 3; ++j )
        {
            const float b = ( water_abs( u[ j ] ) * su ) + ( water_abs( v[ j ] ) * sv );
            const float c = far_sign ? -( w[ j ] * near_z ) : near_z * w[ j ];
            float lo = c - b;
            if ( lo < 0.0f )
                lo = f[ 20 ] * lo;
            float hi = c + b;
            if ( hi > 0.0f )
                hi = hi * f[ 20 ];
            const float p = plus ? ( ( dr * u[ j ] ) - ( du * v[ j ] ) ) + ( df * w[ j ] ) : ( ( u[ j ] * dr ) - ( du * v[ j ] ) ) - ( w[ j ] * df );
            float s = 0.0f;
            for ( int k = 0; k < 3; ++k )
            {
                const float t = plus ? ( ( ra[ k ] * u[ j ] ) - ( ua[ k ] * v[ j ] ) ) + ( fa[ k ] * w[ j ] )
                                     : ( ( ra[ k ] * u[ j ] ) - ( ua[ k ] * v[ j ] ) ) - ( fa[ k ] * w[ j ] );
                s = ( water_abs( t ) * e[ k ] ) + s;
            }
            if ( lo > ( p + s ) || ( p - s ) > hi )
                return false;
        }
        return true;
    };
    // v152 / v144 / v145: |v152| * f19 + |v144| * f18, -(v145 * near)
    if ( !edge( p152, f[ 19 ], p144, f[ 18 ], q145, true, false ) )
        return false;
    // v148 / v144 / v149: |v144| * f18 + |v148| * f19 (the same sum), -(near * v149)
    if ( !edge( p148, f[ 19 ], p144, f[ 18 ], q149, true, false ) )
        return false;
    // v152 / v143 / v149: near * v149, plus
    if ( !edge( p152, f[ 19 ], p143, f[ 18 ], q149, false, true ) )
        return false;
    // v148 / v143 / v145
    if ( !edge( p148, f[ 19 ], p143, f[ 18 ], q145, false, true ) )
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// THE SIXTEEN TEMPLATES (0x595D40): template t's edge bits - 1 the -x edge, 2
// the +x edge, 4 the -y edge, 8 the +y edge - are the edges whose neighbour
// is coarser; a set bit leaves that edge without its two midpoints. Vertices
// in [-1, 1]^2, 17 shared ones then two per fine edge in push order.
struct water_template_t
{
    std::vector< float > xy;   // pairs
    std::vector< std::uint16_t > index;
};

inline void water_template_triangle( water_template_t& t, int a, int b, int c )   // 0x5970B0
{
    t.index.push_back( static_cast< std::uint16_t >( a ) );
    t.index.push_back( static_cast< std::uint16_t >( b ) );
    t.index.push_back( static_cast< std::uint16_t >( c ) );
}

inline int water_template_vertex( water_template_t& t, float x, float y )
{
    t.xy.push_back( x );
    t.xy.push_back( y );
    return static_cast< std::uint16_t >( t.xy.size( ) / 2 ) - 1;   // (u16) size - 1
}

inline void water_templates( water_template_t* out )
{
    for ( int mask = 0; mask < 16; ++mask )
    {
        water_template_t& t = out[ mask ];
        t = water_template_t{};
        static const float shared[ 17 ][ 2 ] = { { 0, 0 }, { -0.5f, 0 }, { 0, -0.5f }, { 0.5f, 0 }, { 0, 0.5f }, { -0.5f, -0.5f }, { 0.5f, -0.5f },
            { 0.5f, 0.5f }, { -0.5f, 0.5f }, { -1, 0 }, { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
        for ( const auto& v : shared )
            water_template_vertex( t, v[ 0 ], v[ 1 ] );
        static const int inner[ 16 ][ 3 ] = { { 0, 1, 5 }, { 0, 5, 2 }, { 0, 2, 6 }, { 0, 6, 3 }, { 0, 3, 7 }, { 0, 7, 4 }, { 0, 4, 8 }, { 0, 8, 1 },
            { 1, 8, 9 }, { 1, 9, 5 }, { 2, 5, 10 }, { 2, 10, 6 }, { 3, 6, 11 }, { 3, 11, 7 }, { 4, 7, 12 }, { 4, 12, 8 } };
        for ( const auto& tri : inner )
            water_template_triangle( t, tri[ 0 ], tri[ 1 ], tri[ 2 ] );
        if ( mask & 1 )
        {
            water_template_triangle( t, 8, 16, 9 );
            water_template_triangle( t, 5, 9, 13 );
        }
        else
        {
            const int a = water_template_vertex( t, -1.0f, 0.5f );
            const int b = water_template_vertex( t, -1.0f, -0.5f );
            water_template_triangle( t, 8, 16, a );
            water_template_triangle( t, 8, a, 9 );
            water_template_triangle( t, 5, 9, b );
            water_template_triangle( t, 5, b, 13 );
        }
        if ( mask & 4 )
        {
            water_template_triangle( t, 5, 13, 10 );
            water_template_triangle( t, 6, 10, 14 );
        }
        else
        {
            const int a = water_template_vertex( t, -0.5f, -1.0f );
            const int b = water_template_vertex( t, 0.5f, -1.0f );
            water_template_triangle( t, 5, 13, a );
            water_template_triangle( t, 5, a, 10 );
            water_template_triangle( t, 6, 10, b );
            water_template_triangle( t, 6, b, 14 );
        }
        if ( mask & 2 )
        {
            water_template_triangle( t, 6, 14, 11 );
            water_template_triangle( t, 7, 11, 15 );
        }
        else
        {
            const int a = water_template_vertex( t, 1.0f, -0.5f );
            const int b = water_template_vertex( t, 1.0f, 0.5f );
            water_template_triangle( t, 6, 14, a );
            water_template_triangle( t, 6, a, 11 );
            water_template_triangle( t, 7, 11, b );
            water_template_triangle( t, 7, b, 15 );
        }
        if ( mask & 8 )
        {
            water_template_triangle( t, 7, 15, 12 );
            water_template_triangle( t, 8, 12, 16 );
        }
        else
        {
            const int a = water_template_vertex( t, 0.5f, 1.0f );
            const int b = water_template_vertex( t, -0.5f, 1.0f );
            water_template_triangle( t, 7, 15, a );
            water_template_triangle( t, 7, a, 12 );
            water_template_triangle( t, 8, 12, b );
            water_template_triangle( t, 8, b, 16 );
        }
    }
}

// ---------------------------------------------------------------------------
// THE PATCHES (renderer +0x60, 20 bytes): +0 / +4 the centre in surface-local
// space ([0, size]), +8 the half size, +0xC the distance to the eye, +0x10 the
// edge bits (the template).
struct water_patch_t
{
    float x, y, half, distance;
    std::uint32_t flags;
};

// The surface as the renderer reads it (VuWaterSurface words): +12 the
// position, +28 the rotation about z, +32 / +36 log2 of the sizes, +40 / +44
// depth below / above, +20 the height, +72 / +76 the sizes, +208 its box.
struct water_grid_surface_t
{
    const water_words_t* words = nullptr;
    int index = -1;   // in the water view
};

struct water_grid_t
{
    water_template_t templates[ 16 ];
    float matrix[ 16 ]{};      // +0x10
    float below = 0.0f;        // +0x50
    float above = 0.0f;        // +0x54
    float lod_min = 2.0f;      // +0x58 = 2 / VuWater+0x40
    float lod_ratio = 4.0f;    // +0x5C = VuWater+0x40 * 4
    std::vector< water_patch_t > patches;
    bool clip_maps = true;               // +0x6E "Water/ClipMaps", 1 from the ctor
    const water_map_t* map = nullptr;    // the surface being built's +0x100 (water_grid_build)
};

// 0x5C7E80 (thiscall VuWaterMapAsset; level, x, y; retn 0xC): true = the cell
// may hold water. Past the last level (+0x88), or past the level's byte array
// (row y of +0 cells, its size +0xC), it passes; otherwise its byte (+8) is
// the answer. Signed compares (jge), as retail. The indices 0x597110 / 0x597260
// hand it are never negative.
inline bool water_map_clip( const water_map_t& m, int level, int ix, int iy )
{
    if ( level >= static_cast< int >( m.entries.size( ) ) )
        return true;
    const water_map_t::entry_t& e = m.entries[ static_cast< std::size_t >( level ) ];
    const int row = static_cast< int >( e.a ) * iy;
    if ( row + ix >= static_cast< int >( e.bytes.size( ) ) )
        return true;
    return e.bytes[ static_cast< std::size_t >( row + ix ) ] != 0;
}

// 0x597260 (thiscall; level, x, y, centre*, half).
inline void water_grid_node( water_grid_t& g, const water_frustum_t& cam, int level, int ix, int iy, const float* centre, float half )
{
    const float box[ 8 ] = { centre[ 0 ] - half, centre[ 1 ] - half, -g.below, 0.0f, centre[ 0 ] + half, centre[ 1 ] + half, g.above, 0.0f };
    if ( !water_frustum_test( cam, box, g.matrix ) )
        return;
    // ClipMaps on and a map on the surface: its clip test decides.
    if ( g.clip_maps && g.map && !water_map_clip( *g.map, level, ix, iy ) )
        return;
    const float* m = g.matrix;
    const float mx = ( box[ 4 ] + box[ 0 ] ) * 0.5f;
    const float my = ( box[ 5 ] + box[ 1 ] ) * 0.5f;
    const float mz = ( box[ 2 ] + box[ 6 ] ) * 0.5f;
    const float wx = ( ( ( m[ 4 ] * my ) + ( mx * m[ 0 ] ) ) + ( m[ 8 ] * mz ) ) + m[ 12 ];
    const float wy = ( ( ( m[ 5 ] * my ) + ( m[ 1 ] * mx ) ) + ( m[ 9 ] * mz ) ) + m[ 13 ];
    const float wz = ( ( ( m[ 6 ] * my ) + ( m[ 2 ] * mx ) ) + ( m[ 10 ] * mz ) ) + m[ 14 ];
    const float hy = ( box[ 5 ] - box[ 1 ] ) * 0.5f;
    const float hx = ( box[ 4 ] - box[ 0 ] ) * 0.5f;
    const float* eye = cam.position;
    const float flat = std::sqrt( ( ( eye[ 1 ] - wy ) * ( eye[ 1 ] - wy ) ) + ( ( eye[ 0 ] - wx ) * ( eye[ 0 ] - wx ) ) );
    const float reach = std::sqrt( ( hy * hy ) + ( hx * hx ) );
    const float h = _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( flat - reach ), _mm_set_ss( 0.0f ) ) );
    const float v = _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( water_abs( eye[ 2 ] - wz ) - ( ( box[ 6 ] - box[ 2 ] ) * 0.5f ) ), _mm_set_ss( 0.0f ) ) );
    const float distance = std::sqrt( ( h * h ) + ( v * v ) );
    if ( _mm_comigt_ss( _mm_set_ss( distance / half ), _mm_set_ss( g.lod_ratio ) ) || _mm_comige_ss( _mm_set_ss( g.lod_min * 0.5f ), _mm_set_ss( half ) ) )
    {
        g.patches.push_back( { centre[ 0 ], centre[ 1 ], half, distance, 0u } );
        return;
    }
    const float q = half * 0.5f;
    float c[ 2 ];
    c[ 0 ] = centre[ 0 ] - q;
    c[ 1 ] = centre[ 1 ] - q;
    water_grid_node( g, cam, level + 1, 2 * ix, 2 * iy, c, q );
    c[ 0 ] = centre[ 0 ] + q;
    c[ 1 ] = centre[ 1 ] - q;
    water_grid_node( g, cam, level + 1, 2 * ix + 1, 2 * iy, c, q );
    c[ 0 ] = centre[ 0 ] + q;
    c[ 1 ] = centre[ 1 ] + q;
    water_grid_node( g, cam, level + 1, 2 * ix + 1, 2 * iy + 1, c, q );
    c[ 0 ] = centre[ 0 ] - q;
    c[ 1 ] = centre[ 1 ] + q;
    water_grid_node( g, cam, level + 1, 2 * ix, 2 * iy + 1, c, q );
}

// 0x597110: the roots - square cells of the shorter side along the longer.
inline void water_grid_roots( water_grid_t& g, const water_frustum_t& cam, const water_words_t& s )
{
    const int lx = static_cast< int >( s.word[ 32 / 4 ] );
    const int ly = static_cast< int >( s.word[ 36 / 4 ] );
    if ( ly >= lx )
    {
        const float half = static_cast< float >( 1 << lx ) * 0.5f;
        if ( ly <= lx )
        {
            const float c[ 2 ] = { half, half };
            water_grid_node( g, cam, 0, 0, 0, c, half );
            return;
        }
        const int n = 1 << ( ly - lx );
        for ( int i = 0, odd = 1; i < n; ++i, odd += 2 )
        {
            const float c[ 2 ] = { half, static_cast< float >( odd ) * half };
            water_grid_node( g, cam, 0, 0, i, c, half );
        }
        return;
    }
    const int n = 1 << ( lx - ly );
    const float half = static_cast< float >( 1 << ly ) * 0.5f;
    for ( int i = 0, odd = 1; i < n; ++i, odd += 2 )
    {
        const float c[ 2 ] = { static_cast< float >( odd ) * half, half };
        water_grid_node( g, cam, 0, i, 0, c, half );
    }
}

// 0x597700: an edge whose neighbour is larger (exactly abutting, overlapping
// along the edge) gets its bit.
inline void water_grid_stitch( std::vector< water_patch_t >& patches )
{
    for ( water_patch_t& p : patches )
    {
        const float h = p.half;
        const float right = p.x + h;
        const float left = p.x - h;
        const float bottom = p.y - h;
        const float top = p.y + h;
        for ( const water_patch_t& q : patches )
        {
            const float qh = q.half;
            if ( !( qh > h ) )
                continue;
            const float qright = q.x + qh;
            const float qleft = q.x - qh;
            const float qbottom = q.y - qh;
            const float qtop = q.y + qh;
            if ( left == qright && qtop > bottom && top > qbottom )
                p.flags |= 1u;
            if ( right == qleft && qtop > bottom && top > qbottom )
                p.flags |= 2u;
            if ( bottom == qtop && qright > left && right > qleft )
                p.flags |= 4u;
            if ( top == qbottom && qright > left && right > qleft )
                p.flags |= 8u;
        }
    }
}

// The 32-byte record 0x597D70 writes and the +96 query fills.
struct water_grid_record_t
{
    float x, y, height;
    float gradient[ 2 ];
    float foam;
    float shade;   // +0x18: 1.0 from the query, then the map's first byte map
    float extra;   // +0x1C: 0 from the query, plus the map's third
};
static_assert( sizeof( water_grid_record_t ) == 32, "retail's stride" );

// One surface's draw record (renderer +568 vector, 15 words): the surface,
// its position (4), rotation z, (float) sizes, below / above, the patch count,
// the vertex start / count, the index start / count.
struct water_grid_draw_t
{
    int surface = -1;
    float position[ 4 ]{};
    float rotation = 0.0f;
    float size[ 2 ]{};
    float below = 0.0f;
    float above = 0.0f;
    int patches = 0;
    int vertex_start = 0, vertex_count = 0;
    int index_start = 0, index_count = 0;
};

struct water_grid_frame_t
{
    std::vector< water_grid_record_t > vertices;   // renderer +520 (+24 * buffer)
    std::vector< std::uint16_t > indices;          // +532
    std::vector< water_grid_draw_t > draws;
    int failed = 0;          // patches whose query returned UNKNOWN + map cells off the map
};

// The query with ctx+0x4C set (0x528FC0's first branch): 0x529ED0 with the
// forced surface's height (+0x14, in XMM2), then - unless VuWater+0x18 - the
// wave walk 0x52A8D0 over that surface's tree (*(surface + 0xFC)) with the
// ctx's rect. query.hxx's water_fill / water_waves, composed.
inline bool water_query_forced( const water_t& w, water_query_t& ctx, int surface, water_kernel_fn kernel, void* user )
{
    const water_surface_t& s = w.surfaces[ static_cast< std::size_t >( surface ) ];
    water_fill( ctx, s.height );
    if ( w.skip_waves )
        return true;
    const float rect[ 4 ] = { ctx.aabb[ 0 ], ctx.aabb[ 1 ], ctx.aabb[ 4 ], ctx.aabb[ 5 ] };
    return water_waves( w, s.wave_root, ctx, rect, kernel, user );
}

// The water map's three byte maps on one patch's records (0x597D70 after the
// query, the map non-null). Which arrays are non-empty (+0x64, +0x70, +0x7C)
// picks one of six loops - 0x598590 (all three), 0x5986E0 (first + second),
// 0x598810 (first + third), the inline one (first only), 0x598940 (second +
// third), 0x598A70 (second), 0x598B70 (third); none, nothing - and every one
// computes the same cell and the same per-array term, so they are one loop
// here: u = (template x * half + centre x) * (1 / (float) surface +0x48),
// v likewise with y and +0x4C, cell = (int)((W - 1) * u) + W * (int)((H - 1)
// * v) (W +0x58, H +0x5C; cvttss2si), and per array byte * (1/255, 0x6F75BC):
// the first (+0x60) REPLACES +0x18, the second (+0x6C) is ADDED to +0x14,
// the third (+0x78) ADDED to +0x1C. Retail has no bounds check on the cell;
// the patches stay inside [0, size] so it never leaves the map - a cell that
// would is counted in `failed` and that vertex left alone (not retail's read).
inline void water_grid_map_terms( const water_map_t& m, const water_words_t& s, const water_patch_t& p, const water_template_t& t,
    water_grid_record_t* rec, int count, int& failed )
{
    const bool first = !m.map.empty( ), second = !m.second.empty( ), third = !m.third.empty( );
    if ( !first && !second && !third )
        return;
    const float scale = 0.0039215689f;   // 0x6F75BC (0x3B808081)
    const float inv_x = 1.0f / static_cast< float >( static_cast< std::int32_t >( s.word[ 0x48 / 4 ] ) );
    const float inv_y = 1.0f / static_cast< float >( static_cast< std::int32_t >( s.word[ 0x4C / 4 ] ) );
    const std::int32_t w = static_cast< std::int32_t >( m.width );
    const float wm = static_cast< float >( w - 1 );
    const float hm = static_cast< float >( static_cast< std::int32_t >( m.height ) - 1 );
    for ( int i = 0; i < count; ++i )
    {
        const float x = ( t.xy[ 2 * i ] * p.half ) + p.x;
        const float y = ( t.xy[ 2 * i + 1 ] * p.half ) + p.y;
        const std::int32_t cx = _mm_cvtt_ss2si( _mm_set_ss( wm * ( x * inv_x ) ) );
        const std::int32_t cy = _mm_cvtt_ss2si( _mm_set_ss( hm * ( y * inv_y ) ) );
        const std::int32_t cell = cx + ( cy * w );
        water_grid_record_t& r = rec[ i ];
        const auto in = [ cell ]( const std::vector< std::uint8_t >& a ) { return cell >= 0 && static_cast< std::size_t >( cell ) < a.size( ); };
        if ( ( first && !in( m.map ) ) || ( second && !in( m.second ) ) || ( third && !in( m.third ) ) )
        {
            ++failed;
            continue;
        }
        if ( first )
            r.shade = static_cast< float >( m.map[ static_cast< std::size_t >( cell ) ] ) * scale;
        if ( second )
            r.foam = ( static_cast< float >( m.second[ static_cast< std::size_t >( cell ) ] ) * scale ) + r.foam;
        if ( third )
            r.extra = ( static_cast< float >( m.third[ static_cast< std::size_t >( cell ) ] ) * scale ) + r.extra;
    }
}

// 0x597D70 (thiscall renderer; vertices*, indices*): every patch in order.
inline void water_grid_vertices( const water_grid_t& g, const water_words_t& s, int surface, water_view_t* view, water_grid_frame_t& out )
{
    const int first = static_cast< int >( out.vertices.size( ) );
    const float root2 = std::sqrt( 2.0f );   // VuMath_sqrt( 0x6F76F8 = 2.0 )
    const float* m = g.matrix;
    for ( const water_patch_t& p : g.patches )
    {
        const water_template_t& t = g.templates[ p.flags ];
        const int count = static_cast< int >( t.xy.size( ) / 2 );
        const int base = static_cast< int >( out.vertices.size( ) );
        out.vertices.resize( static_cast< std::size_t >( base + count ) );
        for ( int i = 0; i < count; ++i )
        {
            const float x = ( t.xy[ 2 * i ] * p.half ) + p.x;
            const float y = ( t.xy[ 2 * i + 1 ] * p.half ) + p.y;
            water_grid_record_t& r = out.vertices[ static_cast< std::size_t >( base + i ) ];
            r.x = ( ( ( m[ 4 ] * y ) + ( x * m[ 0 ] ) ) + ( m[ 8 ] * 0.0f ) ) + m[ 12 ];
            r.y = ( ( ( m[ 1 ] * x ) + ( y * m[ 5 ] ) ) + ( m[ 9 ] * 0.0f ) ) + m[ 13 ];
            r.height = ( ( ( x * m[ 2 ] ) + ( y * m[ 6 ] ) ) + ( m[ 10 ] * 0.0f ) ) + m[ 14 ];
        }
        const std::uint16_t rebase = static_cast< std::uint16_t >( base - first );
        for ( std::uint16_t i : t.index )
            out.indices.push_back( static_cast< std::uint16_t >( i + rebase ) );
        water_grid_record_t* rec = out.vertices.data( ) + base;
        // The query's box: vertices 13 .. 16 (the corners), min / max with the
        // corner first, seeded with +-FLT_MAX; z both the surface height.
        float lo[ 2 ] = { 3.4028235e38f, 3.4028235e38f };
        float hi[ 2 ] = { -3.4028235e38f, -3.4028235e38f };
        for ( int c = 13; c <= 16; ++c )
        {
            lo[ 0 ] = water_min( rec[ c ].x, lo[ 0 ] );
            lo[ 1 ] = water_min( rec[ c ].y, lo[ 1 ] );
            hi[ 0 ] = water_max( rec[ c ].x, hi[ 0 ] );
            hi[ 1 ] = water_max( rec[ c ].y, hi[ 1 ] );
        }
        water_query_t ctx{};
        ctx.count = count;
        ctx.aabb[ 0 ] = lo[ 0 ];
        ctx.aabb[ 1 ] = lo[ 1 ];
        ctx.aabb[ 2 ] = s.f( 20 );
        ctx.aabb[ 4 ] = hi[ 0 ];
        ctx.aabb[ 5 ] = hi[ 1 ];
        ctx.aabb[ 6 ] = s.f( 20 );
        ctx.centre[ 0 ] = rec[ 0 ].x;   // vertex 0: the patch centre (w: a stale stack word)
        ctx.centre[ 1 ] = rec[ 0 ].y;
        ctx.centre[ 2 ] = rec[ 0 ].height;
        ctx.radius = root2 * p.half;
        ctx.exclude_count = 0;
        ctx.forced = surface;
        ctx.flag96 = 1;
        ctx.flag100 = 0;
        ctx.records = reinterpret_cast< unsigned char* >( rec );
        ctx.stride = 0x20;
        // Every wave's slot 3 through kernel.hxx's dispatch (the ocean waves'
        // from the water agent's field, world/field.hxx).
        if ( !water_query_forced( view->water, ctx, surface, water_view_kernel, view ) )
            ++out.failed;
        // surface +0x100: its byte maps (none for a null map).
        if ( g.map )
            water_grid_map_terms( *g.map, s, p, t, rec, count, out.failed );
    }
}

// 0x597B50 for one surface (after 0x5979C0's frustum test of its box).
inline void water_grid_build( water_grid_t& g, const water_frustum_t& cam, const water_words_t& s, int surface, water_view_t* view,
    water_grid_frame_t& out, bool sort = true )
{
    g.below = s.f( 40 );
    g.above = s.f( 44 );
    // The surface's +0x100 as 0x597260 / 0x597D70 read it (through renderer +4).
    g.map = view && surface >= 0 && static_cast< std::size_t >( surface ) < view->water.surfaces.size( )
                ? view->water.surfaces[ static_cast< std::size_t >( surface ) ].map
                : nullptr;
    float t[ 16 ];
    float o[ 16 ];
    wave_matrix_identity( g.matrix );
    const float at[ 3 ] = { s.f( 12 ), s.f( 16 ), s.f( 20 ) };
    hull_translation( t, at );
    hull_multiply( g.matrix, o, t );
    std::memcpy( g.matrix, o, 64 );
    wave_matrix_rotation_z( t, s.f( 28 ) );
    hull_multiply( t, o, g.matrix );
    std::memcpy( g.matrix, o, 64 );
    const float corner[ 3 ] = { static_cast< float >( static_cast< std::int32_t >( s.word[ 72 / 4 ] ) ) * -0.5f,
        static_cast< float >( static_cast< std::int32_t >( s.word[ 76 / 4 ] ) ) * -0.5f, 0.0f };
    hull_translation( t, corner );
    hull_multiply( t, o, g.matrix );
    std::memcpy( g.matrix, o, 64 );
    g.patches.clear( );
    water_grid_roots( g, cam, s );
    if ( g.patches.empty( ) )
        return;
    water_grid_stitch( g.patches );
    // qsort( patches, n, 0x14, 0x594D90 ): descending +0xC (SUBSTITUTION, see the top).
    if ( sort )
        std::stable_sort( g.patches.begin( ), g.patches.end( ), []( const water_patch_t& a, const water_patch_t& b ) { return a.distance > b.distance; } );
    water_grid_draw_t d{};
    d.surface = surface;
    for ( int k = 0; k < 4; ++k )
        d.position[ k ] = s.f( 12 + 4 * k );
    d.rotation = s.f( 28 );
    d.size[ 0 ] = static_cast< float >( static_cast< std::int32_t >( s.word[ 72 / 4 ] ) );
    d.size[ 1 ] = static_cast< float >( static_cast< std::int32_t >( s.word[ 76 / 4 ] ) );
    d.below = s.f( 40 );
    d.above = s.f( 44 );
    d.patches = static_cast< int >( g.patches.size( ) );
    d.vertex_start = static_cast< int >( out.vertices.size( ) );
    d.index_start = static_cast< int >( out.indices.size( ) );
    water_grid_vertices( g, s, surface, view, out );
    d.vertex_count = static_cast< int >( out.vertices.size( ) ) - d.vertex_start;
    d.index_count = static_cast< int >( out.indices.size( ) ) - d.index_start;
    out.draws.push_back( d );
}

// 0x5979C0's body for one viewport: every built surface whose box (+208)
// passes the frustum (through the identity 0x7593B0).
inline void water_grid_frame( water_grid_t& g, const water_frustum_t& cam, const water_built_t& b, water_view_t* view, water_grid_frame_t& out )
{
    out.vertices.clear( );
    out.indices.clear( );
    out.draws.clear( );
    out.failed = 0;
    static const float identity[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    for ( std::size_t i = 0; i < b.surfaces.size( ); ++i )
    {
        const water_words_t& s = b.surfaces[ i ];
        const float box[ 8 ] = { s.f( 208 ), s.f( 212 ), s.f( 216 ), 0.0f, s.f( 224 ), s.f( 228 ), s.f( 232 ), 0.0f };
        if ( water_frustum_test( cam, box, identity ) )
            water_grid_build( g, cam, s, static_cast< int >( i ), view, out );
    }
}
