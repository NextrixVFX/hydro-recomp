#pragma once

#include <workspace/core/dynamics/row.hxx>

// Bullet 2.79's solver iteration drivers, recreated from retail:
// solveSingleIteration @0x573EE0, splitImpulseIterations @0x5742B0 (vtable
// slot 5) and solveGroupCacheFriendlyIterations @0x5743C0 (slot 8).
//
// The solver object's pools are btAlignedObjectArrays (5 dwords each:
// allocator, size, capacity, data, owns). Retail's dword offsets are kept in
// the comments; the arrays themselves are modelled as native pointer + count.

// The btContactSolverInfo members these functions read.
struct solver_info_t
{
    float time_step;                    // +12
    int num_iterations;                 // +20
    float erp;                          // +32
    int split_impulse;                  // +44
    float split_impulse_penetration_threshold;  // +48
    float linear_slop;                  // +52
    float warmstarting_factor;          // +56
    int solver_mode;                    // +60  bit 1 RANDOMIZE_ORDER, 4 warm start, 8 / 0x10 friction rows, 0x100 SIMD
    int resting_contact_restitution_threshold;  // +64
    int minimum_solver_batch_size;      // +68  read by processIsland @0x601E90
};

// btContactSolverInfo's ctor @0x6060D0 (built at world +100 by the
// btDiscreteDynamicsWorld ctor @0x5FFF40), for the members modelled here,
// then VuDynamicsImpl's init @0x4F8050 setting world dword [36] = world +144
// = splitImpulse to 1. Members not modelled: tau 0.6, damping 1, friction
// 0.3, restitution 0, maxErrorReduction 20, sor 1, erp2 0.1, globalCfm 0.
// No later writer has been found - but a ctor default is not proof of the
// runtime value: confirm live.
inline solver_info_t world_solver_info( )
{
    solver_info_t info{};
    info.num_iterations = 10;
    info.erp = 0.2f;                                // 0x3E4CCCCD
    info.split_impulse = 1;                         // 0 in the ctor, 1 from 0x4F8050
    info.split_impulse_penetration_threshold = -0.02f;  // 0xBCA3D70A
    info.linear_slop = 0.0f;
    info.warmstarting_factor = 0.85f;               // 0x3F59999A
    info.solver_mode = 0x104;                       // SIMD | warm starting
    info.resting_contact_restitution_threshold = 2;
    info.minimum_solver_batch_size = 128;
    return info;
}

// One typed-constraint solve: retail calls vtable slot 5 (+20) of each
// btTypedConstraint with (rbA = c+24, rbB = c+28, info.timeStep) -
// solveConstraintObsolete. UNKNOWN until btTypedConstraint is read, so it is
// taken as a callback and never invented here.
using solver_constraint_fn = void ( * )( void* user, int index, float time_step );

struct solver_t
{
    solver_constraint_t* contacts = nullptr;        // dword [4], size [2]
    int contact_count = 0;
    int contact_capacity = 0;                       // dword [3]
    bool owns_contacts = false;                     // byte +20
    solver_constraint_t* non_contacts = nullptr;    // dword [9], size [7]
    int non_contact_count = 0;
    solver_constraint_t* frictions = nullptr;       // dword [14], size [12]
    int friction_count = 0;
    int friction_capacity = 0;                      // dword [13]
    bool owns_frictions = false;                    // byte +60
    int* order_contacts = nullptr;                  // dword [19]
    int order_contact_count = 0;                    // dword [17]
    int order_contact_capacity = 0;                 // dword [18]
    bool owns_order_contacts = false;               // byte +80
    int* order_frictions = nullptr;                 // dword [24]
    int order_friction_count = 0;                   // dword [22]
    int order_friction_capacity = 0;                // dword [23]
    bool owns_order_frictions = false;              // byte +100
    // dwords [26..30]: m_tmpConstraintSizesPool, 8-byte btConstraintInfo1
    // entries filled by each typed constraint's getInfo1 (vtable +12) - the
    // typed-constraint stage of the setup is not recreated (see setup.hxx).
    int constraint_sizes_count = 0;                 // dword [27]
    unsigned seed = 0;                              // dword [31] m_btSeed2
};

// btRand2 + btRandInt2 as inlined at 0x573F20: the seed is stepped and
// stored, then folded by n (unsigned compares) and taken modulo n (div).
inline unsigned solver_rand_int( solver_t& s, unsigned n )
{
    unsigned r = 1664525u * s.seed + 1013904223u;
    s.seed = r;
    if ( n <= 0x10000u )
    {
        r ^= r >> 16;
        if ( n <= 0x100u )
        {
            r ^= r >> 8;
            if ( n <= 0x10u )
            {
                r ^= r >> 4;
                if ( n <= 4u )
                {
                    r ^= r >> 2;
                    if ( n <= 2u )
                        r ^= r >> 1;
                }
            }
        }
    }
    return r % n;
}

// One pass of the in-place shuffle: order[i] swaps with order[rand % (i+1)].
inline void solver_shuffle( solver_t& s, int* order, int count )
{
    for ( int i = 0; i < count; ++i )
    {
        const int held = order[ i ];
        const unsigned j = solver_rand_int( s, static_cast< unsigned >( i + 1 ) );
        order[ i ] = order[ j ];
        order[ j ] = held;
    }
}

inline solver_rigid_t& solver_body_a( solver_constraint_t& c ) { return *static_cast< solver_rigid_t* >( c.solver_body_a ); }
inline solver_rigid_t& solver_body_b( solver_constraint_t& c ) { return *static_cast< solver_rigid_t* >( c.solver_body_b ); }

// solveSingleIteration @0x573EE0.
inline void solve_single_iteration( solver_t& s, int iteration, const solver_info_t& info,
    solver_constraint_fn solve_constraint, void* user, int constraint_count )
{
    if ( ( info.solver_mode & 1 ) != 0 && ( iteration & 7 ) == 0 )
    {
        solver_shuffle( s, s.order_contacts, s.contact_count );
        solver_shuffle( s, s.order_frictions, s.friction_count );
    }

    const bool simd = ( info.solver_mode & 0x100 ) != 0;
    auto generic = simd ? solver_row_generic_simd : solver_row_generic;
    auto lower = simd ? solver_row_lower_simd : solver_row_lower;

    // Non-contact rows in pool order - no order array.
    for ( int i = 0; i < s.non_contact_count; ++i )
    {
        solver_constraint_t& c = s.non_contacts[ i ];
        generic( solver_body_a( c ), solver_body_b( c ), c );
    }
    for ( int i = 0; i < constraint_count; ++i )
        solve_constraint( user, i, info.time_step );
    for ( int i = 0; i < s.contact_count; ++i )
    {
        solver_constraint_t& c = s.contacts[ s.order_contacts[ i ] ];
        lower( solver_body_a( c ), solver_body_b( c ), c );
    }
    for ( int i = 0; i < s.friction_count; ++i )
    {
        solver_constraint_t& c = s.frictions[ s.order_frictions[ i ] ];
        const float total = s.contacts[ c.friction_index ].applied_impulse[ 0 ];
        if ( total > 0.0f )
        {
            const float limit = c.friction * total;
            c.upper_limit = limit;
            c.lower_limit = -limit;
            generic( solver_body_a( c ), solver_body_b( c ), c );
        }
    }
}

// splitImpulseIterations @0x5742B0 (solver vtable slot 5): only with
// info.splitImpulse; numIterations passes over the contacts in order, rows
// with a zero rhsPenetration skipped.
inline void split_impulse_iterations( solver_t& s, const solver_info_t& info )
{
    if ( info.split_impulse == 0 )
        return;
    auto split = ( info.solver_mode & 0x100 ) != 0 ? solver_row_split_simd : solver_row_split;
    for ( int pass = 0; pass < info.num_iterations; ++pass )
    {
        for ( int i = 0; i < s.contact_count; ++i )
        {
            solver_constraint_t& c = s.contacts[ s.order_contacts[ i ] ];
            if ( c.rhs_penetration != 0.0f )
                split( solver_body_a( c ), solver_body_b( c ), c );
        }
    }
}

// solveGroupCacheFriendlyIterations @0x5743C0 (slot 8): the split passes
// first, then numIterations single iterations. Retail wraps it in a
// CProfileSample("solveGroupCacheFriendlyIterations"); that is timing
// instrumentation with no effect on the solve and is NOT recreated. The
// split call goes through the vtable (slot 5); the world creates a plain
// btSequentialImpulseConstraintSolver (vtable 0x74f8ec, slot 5 = 0x5742B0),
// so the call is bound here directly.
inline void solve_iterations( solver_t& s, const solver_info_t& info,
    solver_constraint_fn solve_constraint, void* user, int constraint_count )
{
    split_impulse_iterations( s, info );
    for ( int iteration = 0; iteration < info.num_iterations; ++iteration )
        solve_single_iteration( s, iteration, info, solve_constraint, user, constraint_count );
}
