#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Bullet 2.79's btVoronoiSimplexSolver, recreated from retail's disassembly
// (memory/audit.md "Batch 8b"; the 2.82 source only names the functions).
// The solver is the 400-byte object btDefaultCollisionConfiguration's ctor
// allocates (0x56C880): that ctor writes only the equal-vertex threshold
// (0x38D1B717, 1e-4) and clears the low four used-vertex bits - every other
// word is whatever the allocator returned until simplex_reset runs.
//
// Every compare below states retail's jump; each C++ form is chosen to give
// the same answer on NaN (`a >= b` is false on NaN, as `comiss; jb` is taken).

// btSubSimplexClosestResult: 48 bytes. The used-vertex bitfield is the low
// four bits of a 16-bit word; retail preserves the upper twelve.
struct simplex_result_t
{
    float closest[ 4 ];     // +0
    uint16_t used;          // +16
    uint16_t pad0;          // +18  UNKNOWN, never written
    float bary[ 4 ];        // +20
    uint8_t degenerate;     // +36
    uint8_t pad1[ 11 ];     // +37  UNKNOWN, never written
};
static_assert( sizeof( simplex_result_t ) == 48, "btSubSimplexClosestResult is 48 bytes" );

struct simplex_solver_t
{
    int num_vertices;               // +0
    int pad0[ 3 ];                  // +4   UNKNOWN, never written
    float w[ 5 ][ 4 ];              // +16  m_simplexVectorW
    float p[ 5 ][ 4 ];              // +96  m_simplexPointsP
    float q[ 5 ][ 4 ];              // +176 m_simplexPointsQ
    float cached_p1[ 4 ];           // +256
    float cached_p2[ 4 ];           // +272
    float cached_v[ 4 ];            // +288
    float last_w[ 4 ];              // +304
    float equal_vertex_threshold;   // +320
    uint8_t cached_valid_closest;   // +324
    uint8_t pad1[ 11 ];             // +325 UNKNOWN, never written
    simplex_result_t cached_bc;     // +336
    uint8_t needs_update;           // +384
    uint8_t pad2[ 15 ];             // +385 UNKNOWN, never written
};
static_assert( sizeof( simplex_solver_t ) == 400, "the solver is a 400-byte allocation" );
static_assert( offsetof( simplex_solver_t, w ) == 16 && offsetof( simplex_solver_t, p ) == 96 &&
                   offsetof( simplex_solver_t, q ) == 176 && offsetof( simplex_solver_t, cached_p1 ) == 256 &&
                   offsetof( simplex_solver_t, last_w ) == 304 && offsetof( simplex_solver_t, equal_vertex_threshold ) == 320 &&
                   offsetof( simplex_solver_t, cached_valid_closest ) == 324 && offsetof( simplex_solver_t, cached_bc ) == 336 &&
                   offsetof( simplex_solver_t, needs_update ) == 384,
    "retail's offsets" );

// btDefaultCollisionConfiguration's ctor 0x56C880 on the 400-byte solver it
// allocates first (0x56C8C5 .. 0x56C8EC; stored at config +0x20): the
// equal-vertex threshold (+0x140) = 0x38D1B717 (1e-4) and the cached
// result's used-vertex word (+0x160) &= 0xFFF0 - nothing else (oracle case
// simplex_ctor: the block's other 98 words are the allocator's).
inline void simplex_construct( simplex_solver_t& s )
{
    const uint32_t threshold = 0x38D1B717u;
    std::memcpy( &s.equal_vertex_threshold, &threshold, 4 );
    s.cached_bc.used &= 0xFFF0u;
}

// btSubSimplexClosestResult::reset @0x60C4C0.
inline void simplex_result_reset( simplex_result_t& r )
{
    r.degenerate = 0;
    r.bary[ 0 ] = r.bary[ 1 ] = r.bary[ 2 ] = r.bary[ 3 ] = 0.0f;
    r.used &= 0xFFF0u;
}

// removeVertex @0x60C4F0: the last vertex's W, P and Q (16 bytes each)
// overwrite slot `index`.
inline void simplex_remove_vertex( simplex_solver_t& s, int index )
{
    --s.num_vertices;
    std::memcpy( s.w[ index ], s.w[ s.num_vertices ], 16 );
    std::memcpy( s.p[ index ], s.p[ s.num_vertices ], 16 );
    std::memcpy( s.q[ index ], s.q[ s.num_vertices ], 16 );
}

// reduceVertices @0x60C570: highest slot first, the count re-read each time.
inline void simplex_reduce_vertices( simplex_solver_t& s, uint16_t used )
{
    if ( s.num_vertices >= 4 && !( used & 8 ) )
        simplex_remove_vertex( s, 3 );
    if ( s.num_vertices >= 3 && !( used & 4 ) )
        simplex_remove_vertex( s, 2 );
    if ( s.num_vertices >= 2 && !( used & 2 ) )
        simplex_remove_vertex( s, 1 );
    if ( s.num_vertices >= 1 && !( used & 1 ) )
        simplex_remove_vertex( s, 0 );
}

// reset @0x60C5D0. lastW = (1e18, 1e18, 1e18, 0) from .rdata 0x6F7B10
// (0x5D5E0B6B); the threshold is not touched.
inline void simplex_reset( simplex_solver_t& s )
{
    const uint32_t large[ 4 ] = { 0x5D5E0B6Bu, 0x5D5E0B6Bu, 0x5D5E0B6Bu, 0u };
    std::memcpy( s.last_w, large, 16 );
    s.cached_valid_closest = 0;
    s.num_vertices = 0;
    s.needs_update = 1;
    simplex_result_reset( s.cached_bc );
}

// inSimplex @0x60CFE0: any vertex within the threshold (`comiss thr, d2;
// cmovnb` - every vertex visited, NaN never counts), or w equal to lastW in
// all four lanes (ordered equality: `ucomiss; lahf; test ah, 44h; jp`).
inline bool simplex_in_simplex( const simplex_solver_t& s, const float* w )
{
    bool found = false;
    for ( int i = 0; i < s.num_vertices; ++i )
    {
        const float dx = w[ 0 ] - s.w[ i ][ 0 ];
        const float dy = w[ 1 ] - s.w[ i ][ 1 ];
        const float dz = w[ 2 ] - s.w[ i ][ 2 ];
        const float d2 = ( dy * dy + dx * dx ) + dz * dz;
        if ( s.equal_vertex_threshold >= d2 )
            found = true;
    }
    if ( w[ 3 ] == s.last_w[ 3 ] && w[ 2 ] == s.last_w[ 2 ] && w[ 1 ] == s.last_w[ 1 ] && w[ 0 ] == s.last_w[ 0 ] )
        return true;
    return found;
}

// closestPtPointTriangle @0x60D0B0 (p, a, b, c, result; always true). A
// vertex region copies all four lanes of that vertex; an edge or the face
// writes x, y, z and a zero w.
inline bool simplex_closest_triangle( const float* p, const float* a, const float* b, const float* c, simplex_result_t& r )
{
    r.used &= 0xFFF0u;
    const float abx = b[ 0 ] - a[ 0 ], aby = b[ 1 ] - a[ 1 ], abz = b[ 2 ] - a[ 2 ];
    const float acx = c[ 0 ] - a[ 0 ], acy = c[ 1 ] - a[ 1 ], acz = c[ 2 ] - a[ 2 ];
    const float apx = p[ 0 ] - a[ 0 ], apy = p[ 1 ] - a[ 1 ], apz = p[ 2 ] - a[ 2 ];
    const float d1 = ( apx * abx + apy * aby ) + apz * abz;
    const float d2 = ( apy * acy + apx * acx ) + apz * acz;
    auto vertex = [ & ]( const float* v, uint16_t bit, int k )
    {
        std::memcpy( r.closest, v, 16 );
        r.used |= bit;
        r.bary[ 0 ] = r.bary[ 1 ] = r.bary[ 2 ] = r.bary[ 3 ] = 0.0f;
        r.bary[ k ] = 1.0f;
        return true;
    };
    // `comiss 0, d1; jb` / `comiss 0, d2; jb`
    if ( 0.0f >= d1 && 0.0f >= d2 )
        return vertex( a, 1, 0 );

    const float bpx = p[ 0 ] - b[ 0 ], bpy = p[ 1 ] - b[ 1 ], bpz = p[ 2 ] - b[ 2 ];
    const float d3 = ( bpx * abx + bpy * aby ) + bpz * abz;
    const float d4 = ( bpy * acy + bpx * acx ) + bpz * acz;
    // `comiss d3, 0; jb` / `comiss d3, d4; jb`
    if ( d3 >= 0.0f && d3 >= d4 )
        return vertex( b, 2, 1 );

    const float vc = d4 * d1 - d3 * d2;
    // `comiss 0, vc; jb` / `comiss d1, 0; jb` / `comiss 0, d3; jb`
    if ( 0.0f >= vc && d1 >= 0.0f && 0.0f >= d3 )
    {
        const float v = d1 / ( d1 - d3 );
        r.closest[ 0 ] = abx * v + a[ 0 ];
        r.closest[ 1 ] = aby * v + a[ 1 ];
        r.closest[ 2 ] = abz * v + a[ 2 ];
        r.closest[ 3 ] = 0.0f;
        r.used |= 3;
        r.bary[ 1 ] = v;
        r.bary[ 2 ] = r.bary[ 3 ] = 0.0f;
        r.bary[ 0 ] = 1.0f - v;
        return true;
    }

    const float cpx = p[ 0 ] - c[ 0 ], cpy = p[ 1 ] - c[ 1 ], cpz = p[ 2 ] - c[ 2 ];
    const float d5 = ( cpx * abx + cpy * aby ) + cpz * abz;
    const float d6 = ( cpy * acy + cpx * acx ) + cpz * acz;
    // `comiss d6, 0; jb` / `comiss d6, d5; jb`
    if ( d6 >= 0.0f && d6 >= d5 )
        return vertex( c, 4, 2 );

    const float vb = d5 * d2 - d6 * d1;
    // `comiss 0, vb; jb` / `comiss d2, 0; jb` / `comiss 0, d6; jb`
    if ( 0.0f >= vb && d2 >= 0.0f && 0.0f >= d6 )
    {
        const float w = d2 / ( d2 - d6 );
        r.closest[ 0 ] = acx * w + a[ 0 ];
        r.closest[ 1 ] = acy * w + a[ 1 ];
        r.closest[ 2 ] = acz * w + a[ 2 ];
        r.closest[ 3 ] = 0.0f;
        r.used |= 5;
        r.bary[ 1 ] = 0.0f;
        r.bary[ 2 ] = w;
        r.bary[ 3 ] = 0.0f;
        r.bary[ 0 ] = 1.0f - w;
        return true;
    }

    const float va = d6 * d3 - d5 * d4;
    const float e43 = d4 - d3;
    const float e56 = d5 - d6;
    // `comiss 0, va; jb` / `comiss d4 - d3, 0; jb` / `comiss d5 - d6, 0; jb`
    if ( 0.0f >= va && e43 >= 0.0f && e56 >= 0.0f )
    {
        const float w = e43 / ( e56 + e43 );
        r.closest[ 0 ] = ( c[ 0 ] - b[ 0 ] ) * w + b[ 0 ];
        r.closest[ 1 ] = ( c[ 1 ] - b[ 1 ] ) * w + b[ 1 ];
        r.closest[ 2 ] = ( c[ 2 ] - b[ 2 ] ) * w + b[ 2 ];
        r.closest[ 3 ] = 0.0f;
        r.used |= 6;
        r.bary[ 0 ] = 0.0f;
        r.bary[ 1 ] = 1.0f - w;
        r.bary[ 2 ] = w;
        r.bary[ 3 ] = 0.0f;
        return true;
    }

    // The face: denom = 1 / ((va + vb) + vc).
    const float denom = 1.0f / ( ( va + vb ) + vc );
    const float v = denom * vb;
    const float w = denom * vc;
    r.closest[ 0 ] = ( abx * v + a[ 0 ] ) + acx * w;
    r.closest[ 1 ] = ( aby * v + a[ 1 ] ) + acy * w;
    r.closest[ 2 ] = ( abz * v + a[ 2 ] ) + acz * w;
    r.closest[ 3 ] = 0.0f;
    r.used |= 7;
    r.bary[ 1 ] = v;
    r.bary[ 0 ] = ( 1.0f - v ) - w;
    r.bary[ 2 ] = w;
    r.bary[ 3 ] = 0.0f;
    return true;
}

// pointOutsideOfPlane, inlined four times in 0x60D6D0 with this arithmetic
// each time: n = (b - a) x (c - a), signd = (d - a).n, signp = (p - a).n.
// -1 when 1e-8 (.rdata 0x6F7580, 0x322BCC77) > signd^2 (`jbe` - NaN is
// not degenerate), else `0 > signp * signd` (setnbe - NaN is inside).
inline int simplex_outside_plane( const float* p, const float* a, const float* b, const float* c, const float* d )
{
    const float ux = b[ 0 ] - a[ 0 ], uy = b[ 1 ] - a[ 1 ], uz = b[ 2 ] - a[ 2 ];
    const float vx = c[ 0 ] - a[ 0 ], vy = c[ 1 ] - a[ 1 ], vz = c[ 2 ] - a[ 2 ];
    const float nx = uy * vz - uz * vy;
    const float ny = uz * vx - ux * vz;
    const float nz = ux * vy - uy * vx;
    const float signd = ( ( d[ 0 ] - a[ 0 ] ) * nx + ( d[ 1 ] - a[ 1 ] ) * ny ) + ( d[ 2 ] - a[ 2 ] ) * nz;
    uint32_t eps_bits = 0x322BCC77u;
    float eps;
    std::memcpy( &eps, &eps_bits, 4 );
    if ( eps > signd * signd )
        return -1;
    const float signp = ( ( p[ 0 ] - a[ 0 ] ) * nx + ( p[ 1 ] - a[ 1 ] ) * ny ) + ( p[ 2 ] - a[ 2 ] ) * nz;
    return 0.0f > signp * signd ? 1 : 0;
}

// closestPtPointTetrahedron @0x60D6D0 (p, a, b, c, d, result). The result
// starts as p with all four used bits set; a degenerate plane sets
// `degenerate` and returns false; no plane outside returns false. Each
// outside face runs closestPtPointTriangle into a stack temporary and wins
// on `comiss best, d2; jbe` (strict, NaN never wins) from FLT_MAX
// (0x6F7864); the winner's used bits are remapped onto a..d and its
// barycentrics placed. Only the temporary's low bits are read, so its
// uncleared upper bits never matter.
inline bool simplex_closest_tetrahedron( const float* p, const float* a, const float* b, const float* c, const float* d,
    simplex_result_t& r )
{
    std::memcpy( r.closest, p, 16 );
    r.used |= 0x0F;
    const int abc = simplex_outside_plane( p, a, b, c, d );
    const int acd = simplex_outside_plane( p, a, c, d, b );
    const int adb = simplex_outside_plane( p, a, d, b, c );
    const int bdc = simplex_outside_plane( p, b, d, c, a );
    if ( abc < 0 || acd < 0 || adb < 0 || bdc < 0 )
    {
        r.degenerate = 1;
        return false;
    }
    if ( !abc && !acd && !adb && !bdc )
        return false;

    uint32_t max_bits = 0x7F7FFFFFu;
    float best;
    std::memcpy( &best, &max_bits, 4 );
    simplex_result_t t{};
    auto face = [ & ]( const float* x, const float* y, const float* z, int which )
    {
        simplex_closest_triangle( p, x, y, z, t );
        const float qx = t.closest[ 0 ] - p[ 0 ];
        const float qy = t.closest[ 1 ] - p[ 1 ];
        const float qz = t.closest[ 2 ] - p[ 2 ];
        const float d2 = ( qy * qy + qx * qx ) + qz * qz;
        if ( !( best > d2 ) )
            return;
        best = d2;
        std::memcpy( r.closest, t.closest, 16 );
        const unsigned u = t.used;
        unsigned bits = 0;
        switch ( which )
        {
        case 0: // a b c
            bits = u & 7;
            r.bary[ 0 ] = t.bary[ 0 ], r.bary[ 1 ] = t.bary[ 1 ], r.bary[ 2 ] = t.bary[ 2 ], r.bary[ 3 ] = 0.0f;
            break;
        case 1: // a c d
            bits = ( ( u & 6 ) << 1 ) | ( u & 1 );
            r.bary[ 0 ] = t.bary[ 0 ], r.bary[ 2 ] = t.bary[ 1 ], r.bary[ 1 ] = 0.0f, r.bary[ 3 ] = t.bary[ 2 ];
            break;
        case 2: // a d b
            bits = ( ( u >> 1 ) & 2 ) | ( ( u & 2 ) << 2 ) | ( u & 1 );
            r.bary[ 0 ] = t.bary[ 0 ], r.bary[ 1 ] = t.bary[ 2 ], r.bary[ 2 ] = 0.0f, r.bary[ 3 ] = t.bary[ 1 ];
            break;
        default: // b d c
            bits = ( ( ( ( u & 2 ) << 1 ) | ( u & 1 ) ) << 1 ) | ( u & 4 );
            r.bary[ 1 ] = t.bary[ 0 ], r.bary[ 2 ] = t.bary[ 2 ], r.bary[ 0 ] = 0.0f, r.bary[ 3 ] = t.bary[ 1 ];
            break;
        }
        r.used = static_cast< uint16_t >( ( r.used & 0xFFF0u ) | bits );
    };
    if ( abc )
        face( a, b, c, 0 );
    if ( acd )
        face( a, c, d, 1 );
    if ( adb )
        face( a, d, b, 2 );
    if ( bdc )
        face( b, d, c, 3 );
    return true;
}

// The cached points as cases 3 and 4 build them: ((x0 b0 + x1 b1) + x2 b2)
// (+ x3 b3), w = 0.
inline void simplex_weighted( const float ( *x )[ 4 ], const float* bary, int count, float* out )
{
    for ( int k = 0; k < 3; ++k )
    {
        float sum = x[ 0 ][ k ] * bary[ 0 ] + x[ 1 ][ k ] * bary[ 1 ];
        for ( int i = 2; i < count; ++i )
            sum = sum + x[ i ][ k ] * bary[ i ];
        out[ k ] = sum;
    }
    out[ 3 ] = 0.0f;
}

inline void simplex_cache_v( simplex_solver_t& s )
{
    s.cached_v[ 0 ] = s.cached_p1[ 0 ] - s.cached_p2[ 0 ];
    s.cached_v[ 1 ] = s.cached_p1[ 1 ] - s.cached_p2[ 1 ];
    s.cached_v[ 2 ] = s.cached_p1[ 2 ] - s.cached_p2[ 2 ];
    s.cached_v[ 3 ] = 0.0f;
}

// isValid as retail inlines it: `comiss bary, 0; jb invalid` on all four.
inline bool simplex_bary_valid( const simplex_result_t& r )
{
    return r.bary[ 0 ] >= 0.0f && r.bary[ 1 ] >= 0.0f && r.bary[ 2 ] >= 0.0f && r.bary[ 3 ] >= 0.0f;
}

// updateClosestVectorAndPoints @0x60C640. Without needsUpdate it returns the
// cached validity untouched.
inline bool simplex_update( simplex_solver_t& s )
{
    if ( !s.needs_update )
        return s.cached_valid_closest != 0;
    simplex_result_reset( s.cached_bc );
    s.needs_update = 0;
    simplex_result_t& bc = s.cached_bc;
    switch ( s.num_vertices )
    {
    case 1:
        std::memcpy( s.cached_p1, s.p[ 0 ], 16 );
        std::memcpy( s.cached_p2, s.q[ 0 ], 16 );
        simplex_cache_v( s );
        simplex_result_reset( bc );
        bc.bary[ 0 ] = 1.0f;
        bc.bary[ 1 ] = bc.bary[ 2 ] = bc.bary[ 3 ] = 0.0f;
        s.cached_valid_closest = 1;
        return true;
    case 2:
    {
        // diff = -W0 (xorps with the 0x6F7CD0 sign mask), v = W1 - W0.
        const float dx = -s.w[ 0 ][ 0 ], dy = -s.w[ 0 ][ 1 ], dz = -s.w[ 0 ][ 2 ];
        const float vx = s.w[ 1 ][ 0 ] - s.w[ 0 ][ 0 ];
        const float vy = s.w[ 1 ][ 1 ] - s.w[ 0 ][ 1 ];
        const float vz = s.w[ 1 ][ 2 ] - s.w[ 0 ][ 2 ];
        float t = ( vy * dy + vx * dx ) + vz * dz;
        // `comiss t, 0; jbe` then `comiss vv, t; jbe`
        if ( t > 0.0f )
        {
            const float vv = ( vy * vy + vx * vx ) + vz * vz;
            if ( vv > t )
            {
                bc.used |= 3;
                t = t / vv;
            }
            else
            {
                bc.used |= 2;
                t = 1.0f;
            }
        }
        else
        {
            bc.used |= 1;
            t = 0.0f;
        }
        bc.bary[ 1 ] = t;
        bc.bary[ 2 ] = bc.bary[ 3 ] = 0.0f;
        bc.bary[ 0 ] = 1.0f - t;
        for ( int k = 0; k < 3; ++k )
            s.cached_p1[ k ] = s.p[ 0 ][ k ] + ( s.p[ 1 ][ k ] - s.p[ 0 ][ k ] ) * t;
        s.cached_p1[ 3 ] = 0.0f;
        for ( int k = 0; k < 3; ++k )
            s.cached_p2[ k ] = s.q[ 0 ][ k ] + ( s.q[ 1 ][ k ] - s.q[ 0 ][ k ] ) * t;
        s.cached_p2[ 3 ] = 0.0f;
        simplex_cache_v( s );
        simplex_reduce_vertices( s, bc.used );
        s.cached_valid_closest = simplex_bary_valid( bc ) ? 1 : 0;
        return s.cached_valid_closest != 0;
    }
    case 3:
    {
        const float origin[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
        simplex_closest_triangle( origin, s.w[ 0 ], s.w[ 1 ], s.w[ 2 ], bc );
        simplex_weighted( s.p, bc.bary, 3, s.cached_p1 );
        simplex_weighted( s.q, bc.bary, 3, s.cached_p2 );
        simplex_cache_v( s );
        simplex_reduce_vertices( s, bc.used );
        s.cached_valid_closest = simplex_bary_valid( bc ) ? 1 : 0;
        return s.cached_valid_closest != 0;
    }
    case 4:
    {
        const float origin[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
        if ( simplex_closest_tetrahedron( origin, s.w[ 0 ], s.w[ 1 ], s.w[ 2 ], s.w[ 3 ], bc ) )
        {
            simplex_weighted( s.p, bc.bary, 4, s.cached_p1 );
            simplex_weighted( s.q, bc.bary, 4, s.cached_p2 );
            simplex_cache_v( s );
            simplex_reduce_vertices( s, bc.used );
            s.cached_valid_closest = simplex_bary_valid( bc ) ? 1 : 0;
            return s.cached_valid_closest != 0;
        }
        if ( bc.degenerate )
        {
            s.cached_valid_closest = 0;
            return false;
        }
        s.cached_valid_closest = 1;
        s.cached_v[ 0 ] = s.cached_v[ 1 ] = s.cached_v[ 2 ] = s.cached_v[ 3 ] = 0.0f;
        return true;
    }
    default:
        s.cached_valid_closest = 0;
        return false;
    }
}
