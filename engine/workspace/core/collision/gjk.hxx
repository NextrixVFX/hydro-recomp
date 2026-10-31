#pragma once

#include <workspace/core/collision/simplex.hxx>
#include <workspace/core/collision/support.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>

// btGjkPairDetector (2.79), recreated from retail's disassembly (memory/
// audit.md "Batch 8d"): the ctor @0x60DFD0 and getClosestPointsNonVirtual
// @0x60E0B0 (getClosestPoints, vt slot 1 @0x60E090, is a thunk to it).
//
// The penetration depth solver (+32, vt+4 calcPenDepth - 0x5BAF60 for the
// btGjkEpaPenetrationDepthSolver Vu configures) and the result's
// addContactPoint (vt+12) are caller hooks until recreated.

// ClosestPointInput as retail reads it: two btTransforms (three basis rows
// of four floats, then the origin), maximumDistanceSquared +128, the stack
// allocator +132 (only handed on to the penetration solver).
struct gjk_input_t
{
    float transform_a[ 16 ];
    float transform_b[ 16 ];
    float max_distance2;
    void* stack_alloc;
};

struct gjk_hooks_t
{
    void* user = nullptr;
    // calcPenDepth(simplex, a, b, transA, transB, v, pa, pb, debugDraw,
    // stackAlloc): the transforms are the detector's origin-shifted locals.
    // Returns retail's bool as 1 / 0, or -1 where the solver reached code
    // not yet recreated - the whole query then returns false (unknown),
    // never as if the solver had said 0.
    int ( *pen_depth )( void* user, simplex_solver_t& simplex, const convex_shape_t& a, const convex_shape_t& b,
        const float* transform_a, const float* transform_b, float* v, float* pa, float* pb, void* debug_draw,
        void* stack_alloc ) = nullptr;
    // Result::addContactPoint(normalOnBInWorld, pointInWorld, depth).
    void ( *add_contact )( void* user, const float* normal, const float* point, float depth ) = nullptr;
};

// The members getClosestPointsNonVirtual reads and writes, at retail's
// offsets in comments. `pen_depth_solver` stands for the +32 pointer: only
// its being non-null is read here.
struct gjk_detector_t
{
    float cached_separating_axis[ 4 ];      // +16
    bool pen_depth_solver;                  // +32
    simplex_solver_t* simplex;              // +36
    const convex_shape_t* a;                // +40
    const convex_shape_t* b;                // +44
    int shape_type_a, shape_type_b;         // +48 / +52
    float margin_a, margin_b;               // +56 / +60
    uint8_t ignore_margin;                  // +64
    float cached_separating_distance;       // +68
    int last_used_method;                   // +72
    int cur_iter;                           // +76
    int degenerate_simplex;                 // +80
    int catch_degeneracies;                 // +84
};

// gNumGjkChecks (0x759114) and gNumDeepPenetrationChecks (0x759110).
inline int g_gjk_checks = 0;
inline int g_gjk_deep_checks = 0;

// The ctor @0x60DFD0 (shapeA, shapeB, simplex, penetrationDepthSolver): axis
// (0, 1, 0, 0), the shapes' types (+4) and margins (getMargin vt+40, passed
// in), ignoreMargin 0, lastUsedMethod -1, catchDegeneracies 1. +68 / +76 /
// +80 are not written by the ctor.
inline void gjk_construct( gjk_detector_t& d, const convex_shape_t* a, const convex_shape_t* b, simplex_solver_t* simplex,
    bool pen_depth_solver, float margin_a, float margin_b )
{
    d.cached_separating_axis[ 0 ] = 0.0f;
    d.cached_separating_axis[ 1 ] = 1.0f;
    d.cached_separating_axis[ 2 ] = 0.0f;
    d.cached_separating_axis[ 3 ] = 0.0f;
    d.pen_depth_solver = pen_depth_solver;
    d.simplex = simplex;
    d.a = a;
    d.b = b;
    d.shape_type_a = a->type;
    d.shape_type_b = b->type;
    d.margin_a = margin_a;
    d.margin_b = margin_b;
    d.ignore_margin = 0;
    d.last_used_method = -1;
    d.catch_degeneracies = 1;
}

inline float gjk_bits( uint32_t u )
{
    float f;
    std::memcpy( &f, &u, 4 );
    return f;
}

inline float gjk_negate( float x )
{
    uint32_t u;
    std::memcpy( &u, &x, 4 );
    u ^= 0x80000000u;   // xorps with the 0x6F7CD0 sign mask
    std::memcpy( &x, &u, 4 );
    return x;
}

// getClosestPointsNonVirtual @0x60E0B0. Returns false only where a shape's
// support reaches code not yet recreated (shape_support false).
inline bool gjk_closest_points( gjk_detector_t& d, const gjk_input_t& input, const gjk_hooks_t& hooks, void* debug_draw )
{
    const float* in_a = input.transform_a;
    const float* in_b = input.transform_b;
    d.cached_separating_distance = 0.0f;
    float local_a[ 16 ], local_b[ 16 ];
    std::memcpy( local_a, in_a, 64 );
    std::memcpy( local_b, in_b, 64 );
    // positionOffset = (originA + originB) * 0.5 (0x6F765C), taken off both.
    float offset[ 3 ];
    for ( int k = 0; k < 3; ++k )
        offset[ k ] = ( local_b[ 12 + k ] + local_a[ 12 + k ] ) * 0.5f;
    for ( int k = 0; k < 3; ++k )
    {
        local_a[ 12 + k ] = local_a[ 12 + k ] - offset[ k ];
        local_b[ 12 + k ] = local_b[ 12 + k ] - offset[ k ];
    }
    // check2d: both shapes of type 17 or 18 (no such shape in the content).
    const bool check2d = ( d.a->type == 17 || d.a->type == 18 ) && ( d.b->type == 17 || d.b->type == 18 );
    float margin_a = d.margin_a;
    float margin_b = d.margin_b;
    ++g_gjk_checks;
    if ( d.ignore_margin )
        margin_a = margin_b = 0.0f;
    float squared_distance = gjk_bits( 0x5D5E0B6Bu );   // 1e18 (0x6F7858)
    d.cur_iter = 0;
    float* sep = d.cached_separating_axis;
    sep[ 0 ] = 0.0f;
    sep[ 1 ] = 1.0f;
    sep[ 2 ] = 0.0f;
    sep[ 3 ] = 0.0f;
    bool valid = false;
    d.degenerate_simplex = 0;
    d.last_used_method = -1;
    const float margin = margin_b + margin_a;
    simplex_solver_t& s = *d.simplex;
    simplex_reset( s );
    float distance = 0.0f;

    // Retail's stack buffers, kept as such: after the loop the penetration
    // solver receives `wbuf` (the last w) as pa and `pworld` as pb, and
    // `qworld` doubles as pointOnB.
    float wbuf[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float pworld[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float qworld[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float normal[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool check_simplex = false;

    for ( ;; )
    {
        const float vx = sep[ 0 ], vy = sep[ 1 ], vz = sep[ 2 ];
        const float nx = gjk_negate( vx ), ny = gjk_negate( vy ), nz = gjk_negate( vz );
        float sep_a[ 4 ], sep_b[ 4 ];
        sep_a[ 0 ] = ( nx * in_a[ 0 ] + ny * in_a[ 4 ] ) + in_a[ 8 ] * nz;
        sep_a[ 1 ] = ( nx * in_a[ 1 ] + ny * in_a[ 5 ] ) + in_a[ 9 ] * nz;
        sep_a[ 2 ] = ( nx * in_a[ 2 ] + ny * in_a[ 6 ] ) + in_a[ 10 ] * nz;
        sep_a[ 3 ] = 0.0f;
        sep_b[ 0 ] = ( in_b[ 0 ] * vx + in_b[ 4 ] * vy ) + vz * in_b[ 8 ];
        sep_b[ 1 ] = ( vx * in_b[ 1 ] + in_b[ 5 ] * vy ) + in_b[ 9 ] * vz;
        sep_b[ 2 ] = ( in_b[ 2 ] * vx + vy * in_b[ 6 ] ) + vz * in_b[ 10 ];
        sep_b[ 3 ] = 0.0f;
        float p_in_a[ 4 ], q_in_b[ 4 ];
        if ( !shape_support( *d.a, sep_a, p_in_a ) || !shape_support( *d.b, sep_b, q_in_b ) )
            return false;
        for ( int r = 0; r < 3; ++r )
        {
            pworld[ r ] = ( ( local_a[ 4 * r ] * p_in_a[ 0 ] + local_a[ 4 * r + 1 ] * p_in_a[ 1 ] ) + local_a[ 4 * r + 2 ] * p_in_a[ 2 ] ) +
                local_a[ 12 + r ];
            qworld[ r ] = ( ( local_b[ 4 * r ] * q_in_b[ 0 ] + local_b[ 4 * r + 1 ] * q_in_b[ 1 ] ) + local_b[ 4 * r + 2 ] * q_in_b[ 2 ] ) +
                local_b[ 12 + r ];
        }
        pworld[ 3 ] = 0.0f;
        qworld[ 3 ] = 0.0f;
        if ( check2d )
            pworld[ 2 ] = qworld[ 2 ] = 0.0f;
        wbuf[ 0 ] = pworld[ 0 ] - qworld[ 0 ];
        wbuf[ 1 ] = pworld[ 1 ] - qworld[ 1 ];
        wbuf[ 2 ] = pworld[ 2 ] - qworld[ 2 ];
        wbuf[ 3 ] = 0.0f;
        const float delta = ( vx * wbuf[ 0 ] + vy * wbuf[ 1 ] ) + vz * wbuf[ 2 ];
        // `comiss delta, 0; jbe` then `comiss delta^2, maxD2 * sqDist; ja`
        if ( delta > 0.0f && delta * delta > input.max_distance2 * squared_distance )
        {
            d.degenerate_simplex = 10;
            check_simplex = true;
            break;
        }
        if ( simplex_in_simplex( s, wbuf ) )
        {
            d.degenerate_simplex = 1;
            check_simplex = true;
            break;
        }
        const float f0 = squared_distance - delta;
        // `comiss sqDist * 1e-6, f0; jnb` (1e-6 = 0x6F7590)
        if ( squared_distance * gjk_bits( 0x358637BDu ) >= f0 )
        {
            // `comiss 0, f0; cmovnb`
            d.degenerate_simplex = ( 0.0f >= f0 ) ? 2 : 11;
            check_simplex = true;
            break;
        }
        // addVertex(w, pWorld, qWorld), inlined.
        std::memcpy( s.last_w, wbuf, 16 );
        s.needs_update = 1;
        std::memcpy( s.w[ s.num_vertices ], wbuf, 16 );
        std::memcpy( s.p[ s.num_vertices ], pworld, 16 );
        std::memcpy( s.q[ s.num_vertices ], qworld, 16 );
        ++s.num_vertices;
        // closest(newV), inlined: update, then newV = m_cachedV.
        const bool ok = simplex_update( s );
        float new_v[ 4 ];
        std::memcpy( new_v, s.cached_v, 16 );
        if ( !ok )
        {
            d.degenerate_simplex = 3;
            check_simplex = true;
            break;
        }
        const float len2 = ( new_v[ 0 ] * new_v[ 0 ] + new_v[ 1 ] * new_v[ 1 ] ) + new_v[ 2 ] * new_v[ 2 ];
        // `comiss 1e-6, len2; ja`
        if ( gjk_bits( 0x358637BDu ) > len2 )
        {
            std::memcpy( sep, new_v, 16 );
            d.degenerate_simplex = 6;
            check_simplex = true;
            break;
        }
        const float previous = squared_distance;
        const float eps = previous * gjk_bits( 0x34000000u );   // FLT_EPSILON (0x6F7584)
        const float drop = previous - len2;
        squared_distance = len2;
        // `comiss eps, drop; jnb`
        if ( eps >= drop )
        {
            d.degenerate_simplex = 12;
            check_simplex = true;
            break;
        }
        std::memcpy( sep, new_v, 16 );
        const int iter = d.cur_iter++;
        if ( iter > 1000 )
            break;
        if ( s.num_vertices == 4 )
        {
            d.degenerate_simplex = 13;
            break;
        }
    }

    if ( check_simplex )
    {
        // compute_points: only pointOnB (m_cachedP2) is read afterwards.
        simplex_update( s );
        std::memcpy( qworld, s.cached_p2, 16 );
        std::memcpy( normal, sep, 16 );
        const float len2 = ( normal[ 0 ] * normal[ 0 ] + normal[ 1 ] * normal[ 1 ] ) + normal[ 2 ] * normal[ 2 ];
        // `comisd 1e-4, (double)len2; jbe` - in DOUBLE (qword 0x6F7668).
        if ( 1e-4 > static_cast< double >( len2 ) )
            d.degenerate_simplex = 5;
        // `comiss len2, 1.42e-14; jbe` (0x6F7578)
        if ( len2 > gjk_bits( 0x28800000u ) )
        {
            const float rlen = 1.0f / std::sqrt( len2 );
            normal[ 0 ] = normal[ 0 ] * rlen;
            normal[ 1 ] = normal[ 1 ] * rlen;
            normal[ 2 ] = normal[ 2 ] * rlen;
            valid = true;
            const float k = margin_b / std::sqrt( squared_distance );
            qworld[ 0 ] = qworld[ 0 ] + k * sep[ 0 ];
            qworld[ 1 ] = qworld[ 1 ] + k * sep[ 1 ];
            qworld[ 2 ] = qworld[ 2 ] + k * sep[ 2 ];
            d.last_used_method = 1;
            distance = ( 1.0f / rlen ) - margin;
        }
        else
            d.last_used_method = 2;
    }

    // catchDegeneracyCase: `comisd 0.01, (double)(margin + distance); jbe`.
    const bool catch_case = d.catch_degeneracies && d.pen_depth_solver && d.degenerate_simplex &&
        0.01 > static_cast< double >( margin + distance );
    bool report = valid;
    if ( !( valid && !catch_case ) && d.pen_depth_solver )
    {
        ++g_gjk_deep_checks;
        sep[ 0 ] = sep[ 1 ] = sep[ 2 ] = sep[ 3 ] = 0.0f;
        float* pa = wbuf;
        float* pb = pworld;
        const int pd = hooks.pen_depth ? hooks.pen_depth( hooks.user, s, *d.a, *d.b, local_a, local_b, sep, pa, pb, debug_draw,
                                             input.stack_alloc )
                                       : 0;
        if ( pd < 0 )
            return false;
        const bool ok = pd != 0;
        if ( ok )
        {
            const float pax = pa[ 0 ], pay = pa[ 1 ], paz = pa[ 2 ];
            float tx = pb[ 0 ] - pax;
            float ty = pb[ 1 ] - pay;
            float tz = pb[ 2 ] - paz;
            pa[ 3 ] = 0.0f;
            float len2 = ( ty * ty + tx * tx ) + tz * tz;
            // `comiss 1.42e-14, len2; jb` - else the normal is v, written
            // into the pa buffer (all four lanes).
            if ( !( gjk_bits( 0x28800000u ) < len2 ) )
            {
                std::memcpy( pa, sep, 16 );
                tx = pa[ 0 ];
                ty = pa[ 1 ];
                tz = pa[ 2 ];
                len2 = ( sep[ 1 ] * sep[ 1 ] + sep[ 0 ] * sep[ 0 ] ) + sep[ 2 ] * sep[ 2 ];
            }
            if ( len2 > gjk_bits( 0x28800000u ) )
            {
                const float r = 1.0f / std::sqrt( len2 );
                const float dy = pay - pb[ 1 ];
                const float dx = pax - pb[ 0 ];
                const float dz = paz - pb[ 2 ];
                pa[ 0 ] = r * tx;
                pa[ 1 ] = r * ty;
                pa[ 2 ] = r * tz;
                const float distance2 = gjk_negate( std::sqrt( ( dy * dy + dx * dx ) + dz * dz ) );
                // `comiss distance, distance2; ja` accepts.
                if ( valid && !( distance > distance2 ) )
                    d.last_used_method = 8;
                else
                {
                    distance = distance2;
                    std::memcpy( qworld, pb, 16 );
                    std::memcpy( normal, pa, 16 );
                    d.last_used_method = 3;
                    report = true;
                }
            }
            else
                d.last_used_method = 9;
        }
        else
        {
            const float l2 = ( sep[ 0 ] * sep[ 0 ] + sep[ 1 ] * sep[ 1 ] ) + sep[ 2 ] * sep[ 2 ];
            // `comiss l2, 0; jbe`
            if ( l2 > 0.0f )
            {
                const float dy = pa[ 1 ] - pb[ 1 ];
                const float dx = pa[ 0 ] - pb[ 0 ];
                const float dz = pa[ 2 ] - pb[ 2 ];
                const float distance2 = std::sqrt( ( dy * dy + dx * dx ) + dz * dz ) - margin;
                if ( valid && !( distance > distance2 ) )
                    d.last_used_method = 5;
                else
                {
                    distance = distance2;
                    std::memcpy( qworld, pb, 16 );
                    qworld[ 1 ] = sep[ 1 ] * margin_b + qworld[ 1 ];
                    qworld[ 0 ] = sep[ 0 ] * margin_b + qworld[ 0 ];
                    qworld[ 2 ] = sep[ 2 ] * margin_b + qworld[ 2 ];
                    std::memcpy( normal, sep, 16 );
                    const float nl = ( normal[ 1 ] * normal[ 1 ] + normal[ 0 ] * normal[ 0 ] ) + normal[ 2 ] * normal[ 2 ];
                    d.last_used_method = 6;
                    const float r = 1.0f / std::sqrt( nl );
                    normal[ 0 ] = normal[ 0 ] * r;
                    normal[ 1 ] = normal[ 1 ] * r;
                    normal[ 2 ] = normal[ 2 ] * r;
                    report = true;
                }
            }
        }
    }
    if ( !report )
        return true;
    // `comiss 0, distance; ja` or `comiss maxD2, distance^2; jbe`.
    if ( 0.0f > distance || input.max_distance2 > distance * distance )
    {
        std::memcpy( sep, normal, 16 );
        d.cached_separating_distance = distance;
        float point[ 4 ];
        point[ 0 ] = offset[ 0 ] + qworld[ 0 ];
        point[ 1 ] = offset[ 1 ] + qworld[ 1 ];
        point[ 2 ] = offset[ 2 ] + qworld[ 2 ];
        point[ 3 ] = 0.0f;
        if ( hooks.add_contact )
            hooks.add_contact( hooks.user, normal, point, distance );
    }
    return true;
}
