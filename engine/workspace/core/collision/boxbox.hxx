#pragma once

#include <cmath>
#include <cstring>
#include <xmmintrin.h>

#include <workspace/core/collision/convex.hxx>

// btBoxBoxCollisionAlgorithm (Bullet 2.79, vt 0x757628) and btBoxBoxDetector,
// recreated from retail's disassembly (memory/audit.md "Batch 8o" / "Batch
// 8p"; scratchpad dis_bb.txt): processCollision 0x5FB780, getClosestPoints
// 0x620C70, dBoxBox2 0x61E7A0 (9412 bytes - the ODE routine with
// dLineClosestApproach inlined), intersectRectQuad2 0x61E390, cullPoints2
// 0x61E550. The vendored source is the structure only: retail computes the
// cross-axis tests with ONE reciprocal (inv = 1 / l; s2 * inv, n * inv - the
// zero component included) where the source divides, and dLineClosestApproach
// keeps only beta. Every compare is retail's `comiss` with its jump (ordered
// > / >=: NaN takes the "not" side).

// R as getClosestPoints copies it: the basis rows at [0..2] / [4..6] / [8..10]
// (retail leaves [3] / [7] / [11] unwritten - never read).
inline float boxbox_dot44( const float* a, const float* b )   // dDOT44: (a0 b0 + a4 b4) + a8 b8
{
    return ( a[ 0 ] * b[ 0 ] + a[ 4 ] * b[ 4 ] ) + a[ 8 ] * b[ 8 ];
}

inline float boxbox_dot41( const float* a, const float* b )   // dDOT41: (a0 b0 + a4 b1) + a8 b2
{
    return ( a[ 0 ] * b[ 0 ] + a[ 4 ] * b[ 1 ] ) + a[ 8 ] * b[ 2 ];
}

inline float boxbox_dot( const float* a, const float* b )     // dDOT: (a0 b0 + a1 b1) + a2 b2
{
    return ( a[ 0 ] * b[ 0 ] + a[ 1 ] * b[ 1 ] ) + a[ 2 ] * b[ 2 ];
}

inline float boxbox_abs( float x )
{
    return _mm_cvtss_f32( _mm_and_ps( _mm_set_ss( x ), _mm_castsi128_ps( _mm_set1_epi32( 0x7FFFFFFF ) ) ) );
}

inline bool boxbox_gt( float a, float b )   // `comiss a, b; ja` / `jbe` skips
{
    return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0;
}

// intersectRectQuad2 @0x61E390 (fastcall ecx = h, edx = p; stack ret): chop
// the quad by x = -h0, x = h0, y = -h1, y = h1; inside = `comiss h, sign p;
// jbe`; the crossing (setnbe XOR setnbe) point = ((nq' - pq') / (nq - pq)) *
// (h sign - pq) + pq', and h sign on the chopped axis; nr & 8 stops; the
// buffers alternate (r = old r != ret ? ret : buffer); a result not in ret is
// copied (memcpy_s, 8 nr bytes).
inline int boxbox_intersect_rect_quad( const float* h, float* p, float* ret )
{
    int nq = 4, nr = 0;
    float buffer[ 16 ];
    float* q = p;
    float* r = ret;
    for ( int dir = 0; dir <= 1; ++dir )
    {
        for ( int sign = -1; sign <= 1; sign += 2 )
        {
            const float s = static_cast< float >( sign );
            float* pq = q;
            float* pr = r;
            nr = 0;
            for ( int i = nq; i > 0; --i )
            {
                if ( boxbox_gt( h[ dir ], s * pq[ dir ] ) )
                {
                    pr[ 0 ] = pq[ 0 ];
                    pr[ 1 ] = pq[ 1 ];
                    pr += 2;
                    ++nr;
                    if ( nr & 8 )
                    {
                        q = r;
                        goto done;
                    }
                }
                float* nextq = ( i > 1 ) ? pq + 2 : q;
                if ( boxbox_gt( h[ dir ], nextq[ dir ] * s ) != boxbox_gt( h[ dir ], pq[ dir ] * s ) )
                {
                    const float hs = h[ dir ] * s;
                    pr[ 1 - dir ] = ( ( nextq[ 1 - dir ] - pq[ 1 - dir ] ) / ( nextq[ dir ] - pq[ dir ] ) ) * ( hs - pq[ dir ] ) + pq[ 1 - dir ];
                    pr[ dir ] = hs;
                    pr += 2;
                    ++nr;
                    if ( nr & 8 )
                    {
                        q = r;
                        goto done;
                    }
                }
                pq += 2;
            }
            q = r;
            r = ( q == ret ) ? buffer : ret;
            nq = nr;
        }
    }
done:
    if ( q != ret )
        std::memcpy( ret, q, static_cast< std::size_t >( nr ) * 8 );
    return nr;
}

// cullPoints2 @0x61E550 (fastcall ecx = n, edx = p; m, i0, iret): the
// centroid (n 1: the point; n 2: (p2 + p0) * 0.5; else the area-weighted sum,
// q = x_i y_{i+1} - y_i x_{i+1}, cx = (x_i + x_{i+1}) q + cx; |q_last + a| >
// 1.1920929e-7 -> 1 / ((q + a) * 3), else 1e18); the angles by x87 FPATAN of
// (y - cy, x - cx) on floats, stored as float - std::atan2 in double, rounded
// to float, stands in for the x87 instruction: AGREEMENT UNPROVEN (an
// extended-precision result rounded once vs a double rounded once - equal
// except in the rarest double-rounding cases; say so wherever it is used);
// then per j: a = float(j) * (6.2831855 / float(m)) + A[i0], `comiss a,
// 3.1415927; jbe`; the nearest available by |A - a| (2pi - d past pi), strict
// `comiss maxdiff, d; jbe` from 1e9; iret starts as i0 each round.
inline void boxbox_cull_points( int n, const float* p, int m, int i0, int* iret )
{
    float cx, cy;
    if ( n == 1 )
    {
        cx = p[ 0 ];
        cy = p[ 1 ];
    }
    else if ( n == 2 )
    {
        cx = ( p[ 2 ] + p[ 0 ] ) * 0.5f;
        cy = ( p[ 3 ] + p[ 1 ] ) * 0.5f;
    }
    else
    {
        float a = 0.0f, sx = 0.0f, sy = 0.0f;
        for ( int i = 0; i < n - 1; ++i )
        {
            const float q = p[ 2 * i ] * p[ 2 * i + 3 ] - p[ 2 * i + 1 ] * p[ 2 * i + 2 ];
            sx = ( p[ 2 * i ] + p[ 2 * i + 2 ] ) * q + sx;
            sy = ( p[ 2 * i + 1 ] + p[ 2 * i + 3 ] ) * q + sy;
            a = a + q;
        }
        const float q = p[ 2 * n - 2 ] * p[ 1 ] - p[ 2 * n - 1 ] * p[ 0 ];
        const float aq = q + a;
        const float f = boxbox_gt( boxbox_abs( aq ), support_bits( 0x34000000u ) ) ? 1.0f / ( aq * 3.0f ) : support_bits( 0x5D5E0B6Bu );
        cx = ( ( p[ 2 * n - 2 ] + p[ 0 ] ) * q + sx ) * f;
        cy = ( ( p[ 2 * n - 1 ] + p[ 1 ] ) * q + sy ) * f;
    }
    float angle[ 8 ];
    int avail[ 8 ];
    for ( int i = 0; i < n; ++i )
    {
        const float y = p[ 2 * i + 1 ] - cy;
        const float x = p[ 2 * i ] - cx;
        angle[ i ] = static_cast< float >( std::atan2( static_cast< double >( y ), static_cast< double >( x ) ) );   // FPATAN: UNPROVEN
        avail[ i ] = 1;
    }
    const float two_pi = support_bits( 0x40C90FDBu );
    const float pi = support_bits( 0x40490FDBu );
    iret[ 0 ] = i0;
    avail[ i0 ] = 0;
    const float step = two_pi / static_cast< float >( m );
    for ( int j = 1; j < m; ++j )
    {
        float a = static_cast< float >( j ) * step + angle[ i0 ];
        if ( boxbox_gt( a, pi ) )
            a = a - two_pi;
        float maxdiff = 1e9f;
        iret[ j ] = i0;
        for ( int i = 0; i < n; ++i )
        {
            if ( !avail[ i ] )
                continue;
            float diff = boxbox_abs( angle[ i ] - a );
            if ( boxbox_gt( diff, pi ) )
                diff = two_pi - diff;
            if ( boxbox_gt( maxdiff, diff ) )
            {
                maxdiff = diff;
                iret[ j ] = i;
            }
        }
        avail[ iret[ j ] ] = 0;
    }
}

// dBoxBox2 @0x61E7A0 with maxc 4 folded in. Returns the contact count (the
// points go to addContactPoint (vt+0xC): -normal (w 0), the point - its w is
// NOT written by retail on these paths (stack, UNKNOWN; 0 here) - and the
// depth argument).
inline int boxbox_collide( const float* p1, const float* R1, const float* side1, const float* p2, const float* R2, const float* side2,
    manifold_result_t& output, const contact_callbacks_t& cb )
{
    const float p[ 3 ] = { p2[ 0 ] - p1[ 0 ], p2[ 1 ] - p1[ 1 ], p2[ 2 ] - p1[ 2 ] };
    const float pp[ 3 ] = { boxbox_dot41( R1 + 0, p ), boxbox_dot41( R1 + 1, p ), boxbox_dot41( R1 + 2, p ) };
    const float A[ 3 ] = { side1[ 0 ] * 0.5f, side1[ 1 ] * 0.5f, side1[ 2 ] * 0.5f };
    const float B[ 3 ] = { side2[ 0 ] * 0.5f, side2[ 1 ] * 0.5f, side2[ 2 ] * 0.5f };
    const float R11 = boxbox_dot44( R1 + 0, R2 + 0 ), R12 = boxbox_dot44( R1 + 0, R2 + 1 ), R13 = boxbox_dot44( R1 + 0, R2 + 2 );
    const float R21 = boxbox_dot44( R1 + 1, R2 + 0 ), R22 = boxbox_dot44( R1 + 1, R2 + 1 ), R23 = boxbox_dot44( R1 + 1, R2 + 2 );
    const float R31 = boxbox_dot44( R1 + 2, R2 + 0 ), R32 = boxbox_dot44( R1 + 2, R2 + 1 ), R33 = boxbox_dot44( R1 + 2, R2 + 2 );
    float Q11 = boxbox_abs( R11 ), Q12 = boxbox_abs( R12 ), Q13 = boxbox_abs( R13 );
    float Q21 = boxbox_abs( R21 ), Q22 = boxbox_abs( R22 ), Q23 = boxbox_abs( R23 );
    float Q31 = boxbox_abs( R31 ), Q32 = boxbox_abs( R32 ), Q33 = boxbox_abs( R33 );

    float s = support_bits( 0xFF7FFFFFu );   // -FLT_MAX
    const float* normalR = nullptr;
    float normalC[ 3 ] = { 0.0f, 0.0f, 0.0f };
    bool invert = false;
    int code = 0;

    // The face axes: s2 = |expr1| - expr2; `s2 > 0` -> 0; `s2 > s` records.
    auto face = [ & ]( float e1, float e2, const float* norm, int cc ) -> bool {
        const float s2 = boxbox_abs( e1 ) - e2;
        if ( boxbox_gt( s2, 0.0f ) )
            return false;
        if ( boxbox_gt( s2, s ) )
        {
            s = s2;
            normalR = norm;
            invert = boxbox_gt( 0.0f, e1 );
            code = cc;
        }
        return true;
    };
    if ( !face( pp[ 0 ], ( ( A[ 0 ] + B[ 0 ] * Q11 ) + B[ 1 ] * Q12 ) + B[ 2 ] * Q13, R1 + 0, 1 ) )
        return 0;
    if ( !face( pp[ 1 ], ( ( A[ 1 ] + B[ 0 ] * Q21 ) + B[ 1 ] * Q22 ) + B[ 2 ] * Q23, R1 + 1, 2 ) )
        return 0;
    if ( !face( pp[ 2 ], ( ( A[ 2 ] + B[ 0 ] * Q31 ) + B[ 1 ] * Q32 ) + B[ 2 ] * Q33, R1 + 2, 3 ) )
        return 0;
    if ( !face( boxbox_dot41( R2 + 0, p ), ( ( A[ 0 ] * Q11 + A[ 1 ] * Q21 ) + A[ 2 ] * Q31 ) + B[ 0 ], R2 + 0, 4 ) )
        return 0;
    if ( !face( boxbox_dot41( R2 + 1, p ), ( ( A[ 0 ] * Q12 + A[ 1 ] * Q22 ) + A[ 2 ] * Q32 ) + B[ 1 ], R2 + 1, 5 ) )
        return 0;
    if ( !face( boxbox_dot41( R2 + 2, p ), ( ( A[ 0 ] * Q13 + A[ 1 ] * Q23 ) + A[ 2 ] * Q33 ) + B[ 2 ], R2 + 2, 6 ) )
        return 0;

    const float fudge2 = support_bits( 0x3727C5ACu );   // 1e-5
    Q11 = Q11 + fudge2;
    Q12 = Q12 + fudge2;
    Q13 = Q13 + fudge2;
    Q21 = Q21 + fudge2;
    Q22 = Q22 + fudge2;
    Q23 = Q23 + fudge2;
    Q31 = Q31 + fudge2;
    Q32 = Q32 + fudge2;
    Q33 = Q33 + fudge2;

    // The cross axes: `s2 > 1.1920929e-7` -> 0; l = sqrtps(nA^2 + nB^2) of the
    // two non-zero components (retail's own order); `l > eps` -> inv = 1 / l,
    // `(inv s2) 1.05 > s` records s = inv s2, normalC = n * inv (the zero
    // component as inv * 0).
    const float eps = support_bits( 0x34000000u );
    const float fudge = support_bits( 0x3F866666u );   // 1.05
    auto cross = [ & ]( float e1, float e2, float n0, float n1, float n2, float la, float lb, int cc ) -> bool {
        const float s2 = boxbox_abs( e1 ) - e2;
        if ( boxbox_gt( s2, eps ) )
            return false;
        const float l = _mm_cvtss_f32( _mm_sqrt_ps( _mm_set_ss( la * la + lb * lb ) ) );
        if ( boxbox_gt( l, eps ) )
        {
            const float inv = 1.0f / l;
            if ( boxbox_gt( ( inv * s2 ) * fudge, s ) )
            {
                s = inv * s2;
                normalR = nullptr;
                normalC[ 0 ] = n0 * inv;
                normalC[ 1 ] = n1 * inv;
                normalC[ 2 ] = n2 * inv;
                invert = boxbox_gt( 0.0f, e1 );
                code = cc;
            }
        }
        return true;
    };
    if ( !cross( pp[ 2 ] * R21 - pp[ 1 ] * R31, ( ( A[ 1 ] * Q31 + A[ 2 ] * Q21 ) + B[ 1 ] * Q13 ) + B[ 2 ] * Q12, 0.0f, support_negate( R31 ), R21,
             support_negate( R31 ), R21, 7 ) )
        return 0;
    if ( !cross( pp[ 2 ] * R22 - pp[ 1 ] * R32, ( ( A[ 1 ] * Q32 + A[ 2 ] * Q22 ) + B[ 0 ] * Q13 ) + B[ 2 ] * Q11, 0.0f, support_negate( R32 ), R22,
             R22, support_negate( R32 ), 8 ) )
        return 0;
    if ( !cross( pp[ 2 ] * R23 - pp[ 1 ] * R33, ( ( A[ 1 ] * Q33 + A[ 2 ] * Q23 ) + B[ 0 ] * Q12 ) + B[ 1 ] * Q11, 0.0f, support_negate( R33 ), R23,
             support_negate( R33 ), R23, 9 ) )
        return 0;
    if ( !cross( pp[ 0 ] * R31 - pp[ 2 ] * R11, ( ( A[ 0 ] * Q31 + A[ 2 ] * Q11 ) + B[ 1 ] * Q23 ) + B[ 2 ] * Q22, R31, 0.0f, support_negate( R11 ),
             R31, support_negate( R11 ), 10 ) )
        return 0;
    if ( !cross( pp[ 0 ] * R32 - pp[ 2 ] * R12, ( ( A[ 0 ] * Q32 + A[ 2 ] * Q12 ) + B[ 0 ] * Q23 ) + B[ 2 ] * Q21, R32, 0.0f, support_negate( R12 ),
             R32, support_negate( R12 ), 11 ) )
        return 0;
    if ( !cross( pp[ 0 ] * R33 - pp[ 2 ] * R13, ( ( A[ 0 ] * Q33 + A[ 2 ] * Q13 ) + B[ 0 ] * Q22 ) + B[ 1 ] * Q21, R33, 0.0f, support_negate( R13 ),
             support_negate( R13 ), R33, 12 ) )
        return 0;
    if ( !cross( pp[ 1 ] * R11 - pp[ 0 ] * R21, ( ( A[ 0 ] * Q21 + A[ 1 ] * Q11 ) + B[ 1 ] * Q33 ) + B[ 2 ] * Q32, support_negate( R21 ), R11, 0.0f,
             support_negate( R21 ), R11, 13 ) )
        return 0;
    if ( !cross( pp[ 1 ] * R12 - pp[ 0 ] * R22, ( ( A[ 0 ] * Q22 + A[ 1 ] * Q12 ) + B[ 0 ] * Q33 ) + B[ 2 ] * Q31, support_negate( R22 ), R12, 0.0f,
             support_negate( R22 ), R12, 14 ) )
        return 0;
    if ( !cross( pp[ 1 ] * R13 - pp[ 0 ] * R23, ( ( A[ 0 ] * Q23 + A[ 1 ] * Q13 ) + B[ 0 ] * Q32 ) + B[ 1 ] * Q31, support_negate( R23 ), R13, 0.0f,
             support_negate( R23 ), R13, 15 ) )
        return 0;
    if ( !code )
        return 0;

    float normal[ 3 ];
    if ( normalR )
    {
        normal[ 0 ] = normalR[ 0 ];
        normal[ 1 ] = normalR[ 4 ];
        normal[ 2 ] = normalR[ 8 ];
    }
    else
    {
        normal[ 0 ] = ( R1[ 0 ] * normalC[ 0 ] + R1[ 1 ] * normalC[ 1 ] ) + R1[ 2 ] * normalC[ 2 ];
        normal[ 1 ] = ( R1[ 4 ] * normalC[ 0 ] + R1[ 5 ] * normalC[ 1 ] ) + R1[ 6 ] * normalC[ 2 ];
        normal[ 2 ] = ( R1[ 8 ] * normalC[ 0 ] + R1[ 9 ] * normalC[ 1 ] ) + R1[ 10 ] * normalC[ 2 ];
    }
    if ( invert )
    {
        normal[ 0 ] = support_negate( normal[ 0 ] );
        normal[ 1 ] = support_negate( normal[ 1 ] );
        normal[ 2 ] = support_negate( normal[ 2 ] );
    }
    const float depth = support_negate( s );
    const float minus_normal[ 4 ] = { support_negate( normal[ 0 ] ), support_negate( normal[ 1 ] ), support_negate( normal[ 2 ] ), 0.0f };

    if ( code > 6 )
    {
        // An edge of box 1 against an edge of box 2.
        float pa[ 3 ] = { p1[ 0 ], p1[ 1 ], p1[ 2 ] };
        for ( int j = 0; j < 3; ++j )
        {
            const float sign = boxbox_gt( ( normal[ 0 ] * R1[ j ] + normal[ 1 ] * R1[ 4 + j ] ) + normal[ 2 ] * R1[ 8 + j ], 0.0f ) ? 1.0f : -1.0f;
            const float k = A[ j ] * sign;
            for ( int i = 0; i < 3; ++i )
                pa[ i ] = R1[ 4 * i + j ] * k + pa[ i ];
        }
        float pb[ 4 ] = { p2[ 0 ], p2[ 1 ], p2[ 2 ], 0.0f };
        for ( int j = 0; j < 3; ++j )
        {
            const float sign = boxbox_gt( ( normal[ 0 ] * R2[ j ] + normal[ 1 ] * R2[ 4 + j ] ) + normal[ 2 ] * R2[ 8 + j ], 0.0f ) ? -1.0f : 1.0f;
            const float k = B[ j ] * sign;
            for ( int i = 0; i < 3; ++i )
                pb[ i ] = R2[ 4 * i + j ] * k + pb[ i ];
        }
        const int ka = ( code - 7 ) / 3, kb = ( code - 7 ) % 3;
        const float ua[ 3 ] = { R1[ ka ], R1[ ka + 4 ], R1[ ka + 8 ] };
        const float ub[ 3 ] = { R2[ kb ], R2[ kb + 4 ], R2[ kb + 8 ] };
        // dLineClosestApproach inlined, beta only: d = 1 - uaub^2; `comiss
        // 1e-4, d; jnb` -> 0; else beta = (q1 uaub + q2) * (1 / d).
        const float d0 = pb[ 0 ] - pa[ 0 ], d1 = pb[ 1 ] - pa[ 1 ], d2 = pb[ 2 ] - pa[ 2 ];
        const float uaub = ( ub[ 1 ] * ua[ 1 ] + ub[ 0 ] * ua[ 0 ] ) + ub[ 2 ] * ua[ 2 ];
        const float q1 = ( d1 * ua[ 1 ] + d0 * ua[ 0 ] ) + d2 * ua[ 2 ];
        const float q2 = support_negate( ( d1 * ub[ 1 ] + d0 * ub[ 0 ] ) + d2 * ub[ 2 ] );
        const float d = 1.0f - uaub * uaub;
        float beta = 0.0f;
        if ( !_mm_comige_ss( _mm_set_ss( support_bits( 0x38D1B717u ) ), _mm_set_ss( d ) ) )
            beta = ( q1 * uaub + q2 ) * ( 1.0f / d );
        for ( int i = 0; i < 3; ++i )
            pb[ i ] = beta * ub[ i ] + pb[ i ];
        manifold_result_add_contact_point( output, minus_normal, pb, support_negate( depth ), cb );
        return 1;
    }

    // A face of one box against the other.
    const float *Ra, *Rb, *pa, *pb, *Sa, *Sb;
    if ( code <= 3 )
    {
        Ra = R1, Rb = R2, pa = p1, pb = p2, Sa = A, Sb = B;
    }
    else
    {
        Ra = R2, Rb = R1, pa = p2, pb = p1, Sa = B, Sb = A;
    }
    float normal2[ 3 ];
    for ( int i = 0; i < 3; ++i )
        normal2[ i ] = code <= 3 ? normal[ i ] : support_negate( normal[ i ] );
    const float nr[ 3 ] = { boxbox_dot41( Rb + 0, normal2 ), boxbox_dot41( Rb + 1, normal2 ), boxbox_dot41( Rb + 2, normal2 ) };
    const float anr[ 3 ] = { boxbox_abs( nr[ 0 ] ), boxbox_abs( nr[ 1 ] ), boxbox_abs( nr[ 2 ] ) };
    int lanr, a1, a2;
    if ( boxbox_gt( anr[ 1 ], anr[ 0 ] ) )
    {
        if ( boxbox_gt( anr[ 1 ], anr[ 2 ] ) )
            a1 = 0, lanr = 1, a2 = 2;
        else
            a1 = 0, a2 = 1, lanr = 2;
    }
    else
    {
        if ( boxbox_gt( anr[ 0 ], anr[ 2 ] ) )
            lanr = 0, a1 = 1, a2 = 2;
        else
            a1 = 0, a2 = 1, lanr = 2;
    }
    float center[ 3 ];
    for ( int i = 0; i < 3; ++i )
    {
        const float k = Rb[ 4 * i + lanr ] * Sb[ lanr ];
        center[ i ] = boxbox_gt( 0.0f, nr[ lanr ] ) ? k + ( pb[ i ] - pa[ i ] ) : ( pb[ i ] - pa[ i ] ) - k;
    }
    const int codeN = code <= 3 ? code - 1 : code - 4;
    int code1, code2;
    if ( codeN == 0 )
        code1 = 1, code2 = 2;
    else if ( codeN == 1 )
        code1 = 0, code2 = 2;
    else
        code1 = 0, code2 = 1;
    const float c1 = ( center[ 0 ] * Ra[ code1 ] + center[ 1 ] * Ra[ 4 + code1 ] ) + center[ 2 ] * Ra[ 8 + code1 ];
    const float c2 = ( center[ 0 ] * Ra[ code2 ] + center[ 1 ] * Ra[ 4 + code2 ] ) + center[ 2 ] * Ra[ 8 + code2 ];
    float m11 = boxbox_dot44( Ra + code1, Rb + a1 );
    float m12 = boxbox_dot44( Ra + code1, Rb + a2 );
    float m21 = boxbox_dot44( Ra + code2, Rb + a1 );
    float m22 = boxbox_dot44( Ra + code2, Rb + a2 );
    float quad[ 8 ];
    {
        const float k1 = m11 * Sb[ a1 ], k2 = m21 * Sb[ a1 ], k3 = m12 * Sb[ a2 ], k4 = m22 * Sb[ a2 ];
        quad[ 0 ] = ( c1 - k1 ) - k3;
        quad[ 1 ] = ( c2 - k2 ) - k4;
        quad[ 2 ] = ( c1 - k1 ) + k3;
        quad[ 3 ] = ( c2 - k2 ) + k4;
        quad[ 4 ] = ( c1 + k1 ) + k3;
        quad[ 5 ] = ( c2 + k2 ) + k4;
        quad[ 6 ] = ( c1 + k1 ) - k3;
        quad[ 7 ] = ( c2 + k2 ) - k4;
    }
    const float rect[ 2 ] = { Sa[ code1 ], Sa[ code2 ] };
    float ret[ 16 ];
    const int n = boxbox_intersect_rect_quad( rect, quad, ret );
    if ( n < 1 )
        return 0;
    const float det1 = 1.0f / ( m22 * m11 - m21 * m12 );
    m11 = det1 * m11;
    m12 = det1 * m12;
    m21 = det1 * m21;
    m22 = det1 * m22;
    float point[ 3 * 8 ];
    float dep[ 8 ];
    int cnum = 0;
    for ( int j = 0; j < n; ++j )
    {
        const float dx = ret[ 2 * j ] - c1, dy = ret[ 2 * j + 1 ] - c2;
        const float k1 = m22 * dx - m12 * dy;
        const float k2 = m11 * dy - m21 * dx;
        for ( int i = 0; i < 3; ++i )
            point[ 3 * cnum + i ] = ( Rb[ 4 * i + a1 ] * k1 + center[ i ] ) + Rb[ 4 * i + a2 ] * k2;
        dep[ cnum ] = Sa[ codeN ] - boxbox_dot( normal2, point + 3 * cnum );
        if ( _mm_comige_ss( _mm_set_ss( dep[ cnum ] ), _mm_setzero_ps( ) ) )   // `comiss dep, 0; jb` skips
        {
            ret[ 2 * cnum ] = ret[ 2 * j ];
            ret[ 2 * cnum + 1 ] = ret[ 2 * j + 1 ];
            ++cnum;
        }
    }
    if ( cnum < 1 )
        return 0;
    auto emit = [ & ]( int k ) {
        float w[ 4 ];
        for ( int i = 0; i < 3; ++i )
        {
            w[ i ] = point[ 3 * k + i ] + pa[ i ];
            if ( code >= 4 )
                w[ i ] = w[ i ] - normal[ i ] * dep[ k ];
        }
        w[ 3 ] = 0.0f;
        manifold_result_add_contact_point( output, minus_normal, w, support_negate( dep[ k ] ), cb );
    };
    if ( cnum <= 4 )
    {
        for ( int j = 0; j < cnum; ++j )
            emit( j );
        return cnum;
    }
    int i1 = 0;
    float maxdepth = dep[ 0 ];
    for ( int i = 1; i < cnum; ++i )
        if ( boxbox_gt( dep[ i ], maxdepth ) )
        {
            maxdepth = dep[ i ];
            i1 = i;
        }
    int iret[ 8 ];
    boxbox_cull_points( cnum, ret, 4, i1, iret );
    for ( int j = 0; j < 4; ++j )
        emit( iret[ j ] );
    return 4;
}

// processCollision @0x5FB780 (thiscall; body0, body1, info, result; retn
// 10h): no manifold -> nothing; result +4 = the manifold; getClosestPoints
// 0x620C70: R1 / R2 = the bases, side = (implicit + getMargin) * 2.0 per
// lane, dBoxBox2(A.origin, R1, side1, B.origin, R2, side2); then, when owned,
// the inlined refresh. No swap anywhere.
struct box_box_t
{
    bool own_manifold = false;
    solver_manifold_t* manifold = nullptr;
};

inline int box_box_process( box_box_t& a, const convex_shape_t& box0, const float* t0, const convex_shape_t& box1, const float* t1,
    manifold_result_t& result, const contact_callbacks_t& cb )
{
    if ( !a.manifold )
        return 1;
    result.manifold = a.manifold;
    float m0, m1;
    if ( !shape_get_margin( box1, m1 ) || !shape_get_margin( box0, m0 ) )
        return -1;
    const float side1[ 3 ] = { ( box0.implicit[ 0 ] + m0 ) * 2.0f, ( box0.implicit[ 1 ] + m0 ) * 2.0f, ( box0.implicit[ 2 ] + m0 ) * 2.0f };
    const float side2[ 3 ] = { ( box1.implicit[ 0 ] + m1 ) * 2.0f, ( box1.implicit[ 1 ] + m1 ) * 2.0f, ( box1.implicit[ 2 ] + m1 ) * 2.0f };
    boxbox_collide( t0 + 12, t0, side1, t1 + 12, t1, side2, result, cb );
    if ( a.own_manifold )
    {
        solver_manifold_t& m = *result.manifold;
        if ( m.num_contacts != 0 )
        {
            if ( m.body0 == result.object0 )
                manifold_refresh( m, result.basis0, result.origin0, result.basis1, result.origin1, cb );
            else
                manifold_refresh( m, result.basis1, result.origin1, result.basis0, result.origin0, cb );
        }
    }
    return 1;
}
