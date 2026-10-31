#pragma once

#include <workspace/core/collision/support.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>

// Bullet 2.79's btGjkEpaSolver2 (gjkepa2_impl), recreated from retail's
// disassembly leaf by leaf (memory/audit.md "Batch 8e"). Done so far: the
// MinkowskiDiff set-up (Initialize @0x611ED0) and its support
// (GJK::getsupport with MinkowskiDiff::Support inlined, @0x60FF40).

// btGjkEpaSolver2::sResults: status +0, witnesses +16 / +32, normal +48,
// distance +64.
struct epa_results_t
{
    int status;
    int pad0[ 3 ];
    float witness[ 2 ][ 4 ];
    float normal[ 4 ];
    float distance;
};

// gjkepa2_impl::MinkowskiDiff at retail's offsets: +0 / +4 the shapes,
// +16 m_toshape1 (three rows of four), +64 m_toshape0 (basis rows, origin
// +112), +128 the support member pointer (margin or not).
struct epa_shape_t
{
    const convex_shape_t* shapes[ 2 ];
    float toshape1[ 12 ];
    float toshape0[ 16 ];
    bool with_margin;
};

// Initialize @0x611ED0 (ecx shape0, edx wtrs0; shape1, wtrs1, results,
// shape, withmargins on the stack; the caller pops): results' status and
// both witnesses zeroed; m_toshape1 = B^T A; m_toshape0 = A^T B with origin
// A^T (oB - oA); the support picked by the flag (0x5540B0 / 0x553980).
// Every row's w lane and the origin's are written 0.
inline void epa_initialize( const convex_shape_t* shape0, const float* a, const convex_shape_t* shape1, const float* b,
    epa_results_t& results, epa_shape_t& shape, bool with_margin )
{
    std::memset( results.witness, 0, sizeof( results.witness ) );
    results.status = 0;
    shape.shapes[ 0 ] = shape0;
    shape.shapes[ 1 ] = shape1;
    // m_toshape1[i][j] = (B[0][i] A[0][j] + B[1][i] A[1][j]) + B[2][i] A[2][j]
    for ( int i = 0; i < 3; ++i )
    {
        for ( int j = 0; j < 3; ++j )
            shape.toshape1[ 4 * i + j ] = ( b[ i ] * a[ j ] + b[ 4 + i ] * a[ 4 + j ] ) + b[ 8 + i ] * a[ 8 + j ];
        shape.toshape1[ 4 * i + 3 ] = 0.0f;
    }
    // m_toshape0 basis[i][j] = (A[0][i] B[0][j] + A[1][i] B[1][j]) + A[2][i] B[2][j]
    for ( int i = 0; i < 3; ++i )
    {
        for ( int j = 0; j < 3; ++j )
            shape.toshape0[ 4 * i + j ] = ( b[ j ] * a[ i ] + b[ 4 + j ] * a[ 4 + i ] ) + b[ 8 + j ] * a[ 8 + i ];
        shape.toshape0[ 4 * i + 3 ] = 0.0f;
    }
    const float dx = b[ 12 ] - a[ 12 ], dy = b[ 13 ] - a[ 13 ], dz = b[ 14 ] - a[ 14 ];
    for ( int i = 0; i < 3; ++i )
        shape.toshape0[ 12 + i ] = ( dx * a[ i ] + dy * a[ 4 + i ] ) + dz * a[ 8 + i ];
    shape.toshape0[ 15 ] = 0.0f;
    shape.with_margin = with_margin;
}

inline bool epa_shape_support( const epa_shape_t& shape, int k, const float* dir, float* out )
{
    return shape.with_margin ? shape_support_margin( *shape.shapes[ k ], dir, out ) : shape_support( *shape.shapes[ k ], dir, out );
}

// gjkepa2_impl::sSV: d (the normalised direction) +0, w (the support) +16.
struct epa_sv_t
{
    float d[ 4 ];
    float w[ 4 ];
};

// GJK::getsupport @0x60FF40 (thiscall shape; d*, sv*; retn 8): sv.d = d /
// sqrt((x^2 + y^2) + z^2) (one reciprocal, w 0); shape 1's support along
// m_toshape1 * -sv.d, through m_toshape0; sv.w = shape 0's support along sv.d
// minus that (w 0).
inline bool epa_get_support( const epa_shape_t& shape, const float* d, epa_sv_t& sv )
{
    const float len = std::sqrt( ( d[ 0 ] * d[ 0 ] + d[ 1 ] * d[ 1 ] ) + d[ 2 ] * d[ 2 ] );
    const float r = 1.0f / len;
    sv.d[ 0 ] = d[ 0 ] * r;
    sv.d[ 1 ] = d[ 1 ] * r;
    sv.d[ 2 ] = d[ 2 ] * r;
    sv.d[ 3 ] = 0.0f;
    uint32_t bits;
    float n[ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        std::memcpy( &bits, &sv.d[ k ], 4 );
        bits ^= 0x80000000u;
        std::memcpy( &n[ k ], &bits, 4 );
    }
    const float* t1 = shape.toshape1;
    float dir1[ 4 ];
    dir1[ 0 ] = ( t1[ 1 ] * n[ 1 ] + t1[ 0 ] * n[ 0 ] ) + t1[ 2 ] * n[ 2 ];
    dir1[ 1 ] = ( t1[ 5 ] * n[ 1 ] + t1[ 4 ] * n[ 0 ] ) + t1[ 6 ] * n[ 2 ];
    dir1[ 2 ] = ( t1[ 9 ] * n[ 1 ] + t1[ 8 ] * n[ 0 ] ) + t1[ 10 ] * n[ 2 ];
    dir1[ 3 ] = 0.0f;
    float s1[ 4 ];
    if ( !epa_shape_support( shape, 1, dir1, s1 ) )
        return false;
    const float* t0 = shape.toshape0;
    float p1[ 3 ];
    for ( int r0 = 0; r0 < 3; ++r0 )
        p1[ r0 ] = ( ( t0[ 4 * r0 + 1 ] * s1[ 1 ] + s1[ 0 ] * t0[ 4 * r0 ] ) + t0[ 4 * r0 + 2 ] * s1[ 2 ] ) + t0[ 12 + r0 ];
    float s0[ 4 ];
    if ( !epa_shape_support( shape, 0, sv.d, s0 ) )
        return false;
    sv.w[ 0 ] = s0[ 0 ] - p1[ 0 ];
    sv.w[ 1 ] = s0[ 1 ] - p1[ 1 ];
    sv.w[ 2 ] = s0[ 2 ] - p1[ 2 ];
    sv.w[ 3 ] = 0.0f;
    return true;
}

// GJK::projectorigin, the three static overloads (ecx / edx carry the first
// two points, the rest on the stack, the caller pops, the squared distance
// returns in xmm0). imd3 = { 1, 2, 0 } (.rdata 0x6F6EB8).
inline constexpr unsigned g_epa_imd3[ 3 ] = { 1, 2, 0 };

// The segment @0x610220: d = b - a; -1 unless (d.y^2 + d.x^2) + d.z^2 > 0
// (w and m untouched); t = -((a.x d.x + a.y d.y) + a.z d.z) / l; `comiss t,
// 1; jb` - t >= 1 -> b (w 0 / 1, m 2); `comiss 0, t; jb` - 0 >= t -> a (w 1 /
// 0, m 1); else (NaN too) the interior point a + d t (w 1 - t / t, m 3).
inline float epa_project_segment( const float* a, const float* b, float* w, unsigned& m )
{
    const float dx = b[ 0 ] - a[ 0 ], dy = b[ 1 ] - a[ 1 ], dz = b[ 2 ] - a[ 2 ];
    const float l = ( dy * dy + dx * dx ) + dz * dz;
    if ( !( l > 0.0f ) )
        return -1.0f;
    float t = ( ( a[ 0 ] * dx + a[ 1 ] * dy ) + a[ 2 ] * dz ) / l;
    uint32_t bits;
    std::memcpy( &bits, &t, 4 );
    bits ^= 0x80000000u;
    std::memcpy( &t, &bits, 4 );
    if ( t >= 1.0f )
    {
        w[ 0 ] = 0.0f;
        w[ 1 ] = 1.0f;
        m = 2;
        return ( b[ 1 ] * b[ 1 ] + b[ 0 ] * b[ 0 ] ) + b[ 2 ] * b[ 2 ];
    }
    if ( 0.0f >= t )
    {
        w[ 0 ] = 1.0f;
        w[ 1 ] = 0.0f;
        m = 1;
        return ( a[ 1 ] * a[ 1 ] + a[ 0 ] * a[ 0 ] ) + a[ 2 ] * a[ 2 ];
    }
    w[ 0 ] = 1.0f - t;
    w[ 1 ] = t;
    m = 3;
    const float px = a[ 0 ] + dx * t, py = a[ 1 ] + dy * t, pz = a[ 2 ] + dz * t;
    return ( py * py + px * px ) + pz * pz;
}

inline void epa_cross( const float* u, const float* v, float* out )
{
    out[ 0 ] = u[ 1 ] * v[ 2 ] - u[ 2 ] * v[ 1 ];
    out[ 1 ] = u[ 2 ] * v[ 0 ] - u[ 0 ] * v[ 2 ];
    out[ 2 ] = u[ 0 ] * v[ 1 ] - u[ 1 ] * v[ 0 ];
}

// The triangle @0x6103C0: dl = { a - b, b - c, c - a }, n = dl0 x dl1, -1
// unless (n.y^2 + n.x^2) + n.z^2 > 0. Each edge i whose (v.y c.y + c.x v.x)
// + v.z c.z > 0 (v = vt[i], c = dl[i] x n) projects onto the segment
// (vt[i], vt[j]) - the sub-weights cleared ONCE before the loop - and wins
// on `0 > mindist || mindist > subd`. With no edge (mindist still < 0): the
// face, m 7, w from |dl1 x (b - p)| and |dl2 x (c - p)| over sqrt(l).
inline float epa_project_triangle( const float* a, const float* b, const float* c, float* w, unsigned& m )
{
    const float* vt[ 3 ] = { a, b, c };
    float dl[ 3 ][ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        dl[ 0 ][ k ] = a[ k ] - b[ k ];
        dl[ 1 ][ k ] = b[ k ] - c[ k ];
        dl[ 2 ][ k ] = c[ k ] - a[ k ];
    }
    float n[ 3 ];
    epa_cross( dl[ 0 ], dl[ 1 ], n );
    const float l = ( n[ 1 ] * n[ 1 ] + n[ 0 ] * n[ 0 ] ) + n[ 2 ] * n[ 2 ];
    if ( !( l > 0.0f ) )
        return -1.0f;
    float mindist = -1.0f;
    float subw[ 2 ] = { 0.0f, 0.0f };
    unsigned subm = 0;
    for ( unsigned i = 0; i < 3; ++i )
    {
        float e[ 3 ];
        epa_cross( dl[ i ], n, e );
        const float* v = vt[ i ];
        if ( ( v[ 1 ] * e[ 1 ] + e[ 0 ] * v[ 0 ] ) + v[ 2 ] * e[ 2 ] > 0.0f )
        {
            const unsigned j = g_epa_imd3[ i ];
            const float subd = epa_project_segment( vt[ i ], vt[ j ], subw, subm );
            if ( 0.0f > mindist || mindist > subd )
            {
                mindist = subd;
                m = ( ( subm & 1 ) ? 1u << i : 0u ) + ( ( subm & 2 ) ? 1u << j : 0u );
                w[ i ] = subw[ 0 ];
                w[ j ] = subw[ 1 ];
                w[ g_epa_imd3[ j ] ] = 0.0f;
            }
        }
    }
    if ( 0.0f > mindist )
    {
        const float d = ( a[ 0 ] * n[ 0 ] + n[ 1 ] * a[ 1 ] ) + n[ 2 ] * a[ 2 ];
        const float s = std::sqrt( l );
        m = 7;
        const float t = d / l;
        const float p[ 3 ] = { n[ 0 ] * t, n[ 1 ] * t, n[ 2 ] * t };
        mindist = ( p[ 1 ] * p[ 1 ] + p[ 0 ] * p[ 0 ] ) + p[ 2 ] * p[ 2 ];
        const float rs = 1.0f / s;
        const float q[ 3 ] = { b[ 0 ] - p[ 0 ], b[ 1 ] - p[ 1 ], b[ 2 ] - p[ 2 ] };
        float c1[ 3 ];
        epa_cross( dl[ 1 ], q, c1 );
        w[ 0 ] = std::sqrt( ( c1[ 1 ] * c1[ 1 ] + c1[ 0 ] * c1[ 0 ] ) + c1[ 2 ] * c1[ 2 ] ) * rs;
        const float r[ 3 ] = { c[ 0 ] - p[ 0 ], c[ 1 ] - p[ 1 ], c[ 2 ] - p[ 2 ] };
        float c2[ 3 ];
        epa_cross( dl[ 2 ], r, c2 );
        w[ 1 ] = std::sqrt( ( c2[ 1 ] * c2[ 1 ] + c2[ 0 ] * c2[ 0 ] ) + c2[ 2 ] * c2[ 2 ] ) * rs;
        w[ 2 ] = 1.0f - ( w[ 1 ] + w[ 0 ] );
    }
    return mindist;
}

// det(P, Q, R) as retail writes it every time: ((((Py Qz Rx + Qx Pz Ry) -
// Px Qz Ry) - Py Qx Rz) + Qy Px Rz) - Qy Pz Rx, each term (P . Q) . R.
inline float epa_det( const float* p, const float* q, const float* r )
{
    return ( ( ( ( ( p[ 1 ] * q[ 2 ] ) * r[ 0 ] + ( q[ 0 ] * p[ 2 ] ) * r[ 1 ] ) - ( p[ 0 ] * q[ 2 ] ) * r[ 1 ] ) -
                 ( p[ 1 ] * q[ 0 ] ) * r[ 2 ] ) +
               ( q[ 1 ] * p[ 0 ] ) * r[ 2 ] ) -
        ( q[ 1 ] * p[ 2 ] ) * r[ 0 ];
}

// The tetrahedron @0x610940 (ecx a, edx b; c, d, w, m): dl = { a - d,
// b - d, c - d }, vl = det(dl0, dl1, dl2); -1 unless 0 >= vl * a.((b - c) x
// (a - b)) (`comiss 0, x; jb`) and |vl| > 0 (andps 0x6F7CC0). Each face i
// with vl * d.(dl[i] x dl[j]) > 0 projects onto the triangle (vt[i], vt[j],
// d) - sub-weights cleared once - and wins on `0 > mindist || mindist >
// subd` (m gains 8 for the triangle's third point). With no face: the
// inside, mindist 0, m 15, w = det(c, b, d), det(a, c, d), det(b, a, d)
// times 1 / vl, w3 = 1 - ((w0 + w1) + w2).
inline float epa_project_tetra( const float* a, const float* b, const float* c, const float* d, float* w, unsigned& m )
{
    const float* vt[ 4 ] = { a, b, c, d };
    float dl[ 3 ][ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        dl[ 0 ][ k ] = a[ k ] - d[ k ];
        dl[ 1 ][ k ] = b[ k ] - d[ k ];
        dl[ 2 ][ k ] = c[ k ] - d[ k ];
    }
    const float vl = epa_det( dl[ 0 ], dl[ 1 ], dl[ 2 ] );
    const float u[ 3 ] = { b[ 0 ] - c[ 0 ], b[ 1 ] - c[ 1 ], b[ 2 ] - c[ 2 ] };
    const float v[ 3 ] = { a[ 0 ] - b[ 0 ], a[ 1 ] - b[ 1 ], a[ 2 ] - b[ 2 ] };
    float x[ 3 ];
    epa_cross( u, v, x );
    const float ng = ( ( x[ 1 ] * a[ 1 ] + x[ 0 ] * a[ 0 ] ) + x[ 2 ] * a[ 2 ] ) * vl;
    if ( !( 0.0f >= ng ) || !( std::fabs( vl ) > 0.0f ) )
        return -1.0f;
    float mindist = -1.0f;
    float subw[ 3 ] = { 0.0f, 0.0f, 0.0f };
    unsigned subm = 0;
    for ( unsigned i = 0; i < 3; ++i )
    {
        const unsigned j = g_epa_imd3[ i ];
        float e[ 3 ];
        epa_cross( dl[ i ], dl[ j ], e );
        const float s = ( ( e[ 1 ] * d[ 1 ] + e[ 0 ] * d[ 0 ] ) + e[ 2 ] * d[ 2 ] ) * vl;
        if ( s > 0.0f )
        {
            const float subd = epa_project_triangle( vt[ i ], vt[ j ], d, subw, subm );
            if ( 0.0f > mindist || mindist > subd )
            {
                mindist = subd;
                m = ( ( subm & 1 ) ? 1u << i : 0u ) + ( ( subm & 2 ) ? 1u << j : 0u ) + ( ( subm & 4 ) ? 8u : 0u );
                w[ i ] = subw[ 0 ];
                w[ j ] = subw[ 1 ];
                w[ g_epa_imd3[ j ] ] = 0.0f;
                w[ 3 ] = subw[ 2 ];
            }
        }
    }
    if ( 0.0f > mindist )
    {
        mindist = 0.0f;
        const float r = 1.0f / vl;
        m = 15;
        w[ 0 ] = epa_det( c, b, d ) * r;
        w[ 1 ] = epa_det( a, c, d ) * r;
        w[ 2 ] = epa_det( b, a, d ) * r;
        w[ 3 ] = 1.0f - ( ( w[ 0 ] + w[ 1 ] ) + w[ 2 ] );
    }
    return mindist;
}

// gjkepa2_impl::GJK at retail's offsets (dwords): +0 m_shape (the
// MinkowskiDiff, 132 b), +144 m_ray, +160 m_distance, +164 / +200
// m_simplices[2] (c[4] sSV pointers, p[4], rank), +240 m_store[4] (sSV, 32 b
// each), +368 m_free[4], +384 m_nfree, +388 m_current, +392 m_simplex,
// +396 m_status (0 Valid, 1 Inside, 2 Failed). Vertex pointers are kept as
// indices into m_store (-1 = never written); m_simplex as 0 / 1.
struct epa_simplex_t
{
    int c[ 4 ] = { -1, -1, -1, -1 };
    float p[ 4 ] = {};
    unsigned rank = 0;
};

struct epa_gjk_t
{
    epa_shape_t shape;
    float ray[ 4 ] = {};
    float distance = 0.0f;
    epa_simplex_t simplices[ 2 ];
    epa_sv_t store[ 4 ] = {};
    int free[ 4 ] = { -1, -1, -1, -1 };
    unsigned nfree = 0;
    unsigned current = 0;
    int simplex = -1;
    int status = 0;
};

inline float epa_negate( float x )
{
    uint32_t u;
    std::memcpy( &u, &x, 4 );
    u ^= 0x80000000u;
    std::memcpy( &x, &u, 4 );
    return x;
}

// appendvertice: p[rank] = 0, c[rank] = m_free[--m_nfree], then getsupport
// into it (rank counted before the support is taken).
inline bool epa_append_vertex( epa_gjk_t& g, epa_simplex_t& s, const float* v )
{
    s.p[ s.rank ] = 0.0f;
    s.c[ s.rank ] = g.free[ --g.nfree ];
    const int k = s.c[ s.rank++ ];
    return epa_get_support( g.shape, v, g.store[ k ] );
}

inline void epa_remove_vertex( epa_gjk_t& g, epa_simplex_t& s )
{
    g.free[ g.nfree++ ] = s.c[ --s.rank ];
}

// GJK::Evaluate @0x60EF60 (thiscall gjk; shape*, guess*). Constants from the
// binary: GJK_MIN_DISTANCE = GJK_DUPLICATED_EPS = GJK_ACCURARY = 1e-4
// (0x6F75A4), GJK_MAX_ITERATIONS = 128 (an immediate, unsigned `jnb`).
// Jumps: `comiss 1e-4, rl; ja` Inside; `comiss 1e-4, |w - lastw|^2; ja`
// duplicate; alpha takes omega only on an ordered omega > alpha (cmovbe);
// `comiss 0, (rl - alpha) - rl * 1e-4; jnb` and `comiss sqdist, 0; jb` remove
// the vertex. The rank-2 projection's xmm0 IS kept (Hex-Rays drops it).
// Returns false only where a support reaches code not yet recreated.
inline bool epa_gjk_evaluate( epa_gjk_t& g, const epa_shape_t& shape, const float* guess )
{
    for ( int k = 0; k < 4; ++k )
        g.free[ k ] = k;
    g.nfree = 4;
    g.current = 0;
    g.status = 0;
    g.shape = shape;
    g.distance = 0.0f;
    g.simplices[ 0 ].rank = 0;
    std::memcpy( g.ray, guess, 16 );
    const uint32_t eps_bits = 0x38D1B717u;
    float eps;
    std::memcpy( &eps, &eps_bits, 4 );
    const float sqrl = ( g.ray[ 0 ] * g.ray[ 0 ] + g.ray[ 1 ] * g.ray[ 1 ] ) + g.ray[ 2 ] * g.ray[ 2 ];
    float first[ 4 ] = { 1.0f, 0.0f, 0.0f, 0.0f };   // xmmword 0x6F7930
    if ( sqrl > 0.0f )
    {
        first[ 0 ] = epa_negate( g.ray[ 0 ] );
        first[ 1 ] = epa_negate( g.ray[ 1 ] );
        first[ 2 ] = epa_negate( g.ray[ 2 ] );
        first[ 3 ] = 0.0f;
    }
    if ( !epa_append_vertex( g, g.simplices[ 0 ], first ) )
        return false;
    g.simplices[ 0 ].p[ 0 ] = 1.0f;
    std::memcpy( g.ray, g.store[ g.simplices[ 0 ].c[ 0 ] ].w, 16 );
    float sqdist = sqrl;
    float lastw[ 4 ][ 4 ];
    for ( int k = 0; k < 4; ++k )
        std::memcpy( lastw[ k ], g.ray, 16 );
    unsigned clastw = 0;
    unsigned iterations = 0;
    float alpha = 0.0f;
    for ( ;; )
    {
        const unsigned next = 1 - g.current;
        epa_simplex_t& cs = g.simplices[ g.current ];
        epa_simplex_t& ns = g.simplices[ next ];
        const float rx = g.ray[ 0 ], ry = g.ray[ 1 ], rz = g.ray[ 2 ];
        const float rl = std::sqrt( ( rx * rx + ry * ry ) + rz * rz );
        if ( eps > rl )
        {
            g.status = 1;
            break;
        }
        const float v[ 4 ] = { epa_negate( rx ), epa_negate( ry ), epa_negate( rz ), 0.0f };
        if ( !epa_append_vertex( g, cs, v ) )
            return false;
        const float* w = g.store[ cs.c[ cs.rank - 1 ] ].w;
        bool found = false;
        for ( int k = 0; k < 4; ++k )
        {
            const float dy = w[ 1 ] - lastw[ k ][ 1 ], dx = w[ 0 ] - lastw[ k ][ 0 ], dz = w[ 2 ] - lastw[ k ][ 2 ];
            if ( eps > ( dy * dy + dx * dx ) + dz * dz )
            {
                found = true;
                break;
            }
        }
        if ( found )
        {
            epa_remove_vertex( g, g.simplices[ g.current ] );
            break;
        }
        clastw = ( clastw + 1 ) & 3;
        std::memcpy( lastw[ clastw ], w, 16 );
        const float omega = ( ( w[ 1 ] * g.ray[ 1 ] + g.ray[ 0 ] * w[ 0 ] ) + w[ 2 ] * g.ray[ 2 ] ) / rl;
        alpha = omega > alpha ? omega : alpha;
        if ( 0.0f >= ( rl - alpha ) - rl * eps )
        {
            epa_remove_vertex( g, g.simplices[ g.current ] );
            break;
        }
        float weights[ 4 ] = {};
        unsigned mask = 0;
        const unsigned rank = cs.rank;
        if ( rank == 2 )
            sqdist = epa_project_segment( g.store[ cs.c[ 0 ] ].w, g.store[ cs.c[ 1 ] ].w, weights, mask );
        else if ( rank == 3 )
            sqdist = epa_project_triangle( g.store[ cs.c[ 0 ] ].w, g.store[ cs.c[ 1 ] ].w, g.store[ cs.c[ 2 ] ].w, weights, mask );
        else if ( rank == 4 )
            sqdist = epa_project_tetra(
                g.store[ cs.c[ 0 ] ].w, g.store[ cs.c[ 1 ] ].w, g.store[ cs.c[ 2 ] ].w, g.store[ cs.c[ 3 ] ].w, weights, mask );
        if ( !( sqdist >= 0.0f ) )
        {
            epa_remove_vertex( g, g.simplices[ g.current ] );
            break;
        }
        ns.rank = 0;
        g.ray[ 0 ] = g.ray[ 1 ] = g.ray[ 2 ] = g.ray[ 3 ] = 0.0f;
        g.current = next;
        for ( unsigned i = 0; i < rank; ++i )
        {
            if ( mask & ( 1u << i ) )
            {
                const float wt = weights[ i ];
                ns.c[ ns.rank ] = cs.c[ i ];
                ns.p[ ns.rank++ ] = wt;
                const float* cw = g.store[ cs.c[ i ] ].w;
                g.ray[ 1 ] = cw[ 1 ] * wt + g.ray[ 1 ];
                g.ray[ 2 ] = cw[ 2 ] * wt + g.ray[ 2 ];
                g.ray[ 0 ] = g.ray[ 0 ] + wt * cw[ 0 ];
            }
            else
                g.free[ g.nfree++ ] = cs.c[ i ];
        }
        if ( mask == 15 )
            g.status = 1;
        ++iterations;
        g.status = iterations >= 128 ? 2 : g.status;
        if ( g.status != 0 )
            break;
    }
    g.simplex = static_cast< int >( g.current );
    if ( g.status == 0 )
        g.distance = std::sqrt( ( g.ray[ 0 ] * g.ray[ 0 ] + g.ray[ 1 ] * g.ray[ 1 ] ) + g.ray[ 2 ] * g.ray[ 2 ] );
    else if ( g.status == 1 )
        g.distance = 0.0f;
    return true;
}

// GJK::EncloseOrigin @0x60F8D0 (thiscall gjk) on m_simplex, recursing; the
// inlined appendvertice / removevertice are 0x6101D0 / 0x6101A0's. A true
// from the recursion returns at once, the vertices it appended left in. All
// three `> 0` tests are ordered (`comiss; jbe` / andps + `ja`). Negations are
// sign flips and the cross products are written term for term - the axes'
// zeros make signed zeros that the support's normalisation keeps.
// Returns -1 where a support reaches code not yet recreated.
inline int epa_enclose_origin( epa_gjk_t& g )
{
    epa_simplex_t& s = g.simplices[ g.simplex ];
    auto w = [ & ]( int k ) { return g.store[ s.c[ k ] ].w; };
    auto neg = [ & ]( const float* v, float* out )
    {
        out[ 0 ] = epa_negate( v[ 0 ] );
        out[ 1 ] = epa_negate( v[ 1 ] );
        out[ 2 ] = epa_negate( v[ 2 ] );
        out[ 3 ] = 0.0f;
    };
    switch ( s.rank )
    {
    case 1:
        for ( int i = 0; i < 3; ++i )
        {
            float axis[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
            axis[ i ] = 1.0f;
            if ( !epa_append_vertex( g, s, axis ) )
                return -1;
            int r = epa_enclose_origin( g );
            if ( r != 0 )
                return r;
            epa_remove_vertex( g, s );
            float back[ 4 ];
            neg( axis, back );
            if ( !epa_append_vertex( g, s, back ) )
                return -1;
            r = epa_enclose_origin( g );
            if ( r != 0 )
                return r;
            epa_remove_vertex( g, s );
        }
        return 0;
    case 2:
    {
        const float* w0 = w( 0 );
        const float* w1 = w( 1 );
        const float dx = w1[ 0 ] - w0[ 0 ], dy = w1[ 1 ] - w0[ 1 ], dz = w1[ 2 ] - w0[ 2 ];
        for ( int i = 0; i < 3; ++i )
        {
            float a[ 3 ] = { 0.0f, 0.0f, 0.0f };
            a[ i ] = 1.0f;
            float p[ 4 ];
            p[ 0 ] = a[ 2 ] * dy - a[ 1 ] * dz;
            p[ 1 ] = a[ 0 ] * dz - a[ 2 ] * dx;
            p[ 2 ] = a[ 1 ] * dx - a[ 0 ] * dy;
            p[ 3 ] = 0.0f;
            if ( ( p[ 1 ] * p[ 1 ] + p[ 0 ] * p[ 0 ] ) + p[ 2 ] * p[ 2 ] > 0.0f )
            {
                if ( !epa_append_vertex( g, s, p ) )
                    return -1;
                int r = epa_enclose_origin( g );
                if ( r != 0 )
                    return r;
                epa_remove_vertex( g, s );
                float back[ 4 ];
                neg( p, back );
                if ( !epa_append_vertex( g, s, back ) )
                    return -1;
                r = epa_enclose_origin( g );
                if ( r != 0 )
                    return r;
                epa_remove_vertex( g, s );
            }
        }
        return 0;
    }
    case 3:
    {
        const float* w0 = w( 0 );
        const float* w1 = w( 1 );
        const float* w2 = w( 2 );
        const float ax = w1[ 0 ] - w0[ 0 ], ay = w1[ 1 ] - w0[ 1 ], az = w1[ 2 ] - w0[ 2 ];
        const float bx = w2[ 0 ] - w0[ 0 ], by = w2[ 1 ] - w0[ 1 ], bz = w2[ 2 ] - w0[ 2 ];
        float n[ 4 ];
        n[ 0 ] = ay * bz - az * by;
        n[ 1 ] = az * bx - ax * bz;
        n[ 2 ] = ax * by - ay * bx;
        n[ 3 ] = 0.0f;
        if ( !( ( n[ 1 ] * n[ 1 ] + n[ 0 ] * n[ 0 ] ) + n[ 2 ] * n[ 2 ] > 0.0f ) )
            return 0;
        if ( !epa_append_vertex( g, s, n ) )
            return -1;
        int r = epa_enclose_origin( g );
        if ( r != 0 )
            return r;
        epa_remove_vertex( g, s );
        float back[ 4 ];
        neg( n, back );
        if ( !epa_append_vertex( g, s, back ) )
            return -1;
        r = epa_enclose_origin( g );
        if ( r != 0 )
            return r;
        epa_remove_vertex( g, s );
        return 0;
    }
    case 4:
    {
        const float* d = w( 3 );
        const float a[ 3 ] = { w( 0 )[ 0 ] - d[ 0 ], w( 0 )[ 1 ] - d[ 1 ], w( 0 )[ 2 ] - d[ 2 ] };
        const float b[ 3 ] = { w( 1 )[ 0 ] - d[ 0 ], w( 1 )[ 1 ] - d[ 1 ], w( 1 )[ 2 ] - d[ 2 ] };
        const float c[ 3 ] = { w( 2 )[ 0 ] - d[ 0 ], w( 2 )[ 1 ] - d[ 1 ], w( 2 )[ 2 ] - d[ 2 ] };
        return std::fabs( epa_det( a, b, c ) ) > 0.0f ? 1 : 0;
    }
    default:
        return 0;
    }
}