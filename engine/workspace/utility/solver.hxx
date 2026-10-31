#pragma once

#include <workspace/core/collision/epa.hxx>
#include <workspace/core/collision/polytope.hxx>
#include <workspace/core/collision/convex.hxx>
#include <array>
#include <workspace/core/collision/compound.hxx>
#include <workspace/core/collision/dispatch.hxx>
#include <workspace/core/collision/pairs.hxx>
#include <workspace/core/collision/dbvt.hxx>
#include <workspace/core/collision/broadphase.hxx>
#include <workspace/core/collision/world.hxx>
#include <workspace/utility/collide.hxx>
#include <algorithm>
#include <workspace/core/collision/gjk.hxx>
#include <workspace/core/collision/simplex.hxx>
#include <workspace/core/collision/support.hxx>
#include <workspace/core/dynamics/contact.hxx>
#include <workspace/core/dynamics/finish.hxx>
#include <workspace/core/dynamics/island.hxx>
#include <workspace/core/dynamics/iterate.hxx>
#include <workspace/core/dynamics/row.hxx>
#include <workspace/core/dynamics/setup.hxx>
#include <workspace/core/dynamics/step.hxx>
#include <workspace/core/dynamics/world.hxx>
#include <workspace/core/dynamics/writeback.hxx>
#include <workspace/utility/golden.hxx>
#include <workspace/utility/logger.hxx>
#include <workspace/utility/math.hxx>

#include <cmath>
#include <cstring>

// The dynamics checks against retail: the solver rows and iterations, the setup, the islands, the world step and its passes.
// (Split out of verify.hxx; verify.hxx's verify_all runs them.)

// The scalar row resolvers against Bullet 2.79's @0x56FF70 (generic),
// @0x570470 (lower limit) and @0x5707B0 (split penetration). Multiplies, adds
// and compares only, so the bar is bit exactness. The three share a vector
// layout; for the split one the deltas are the push / turn velocities,
// appliedImpulse is appliedPushImpulse and rhs is rhsPenetration.
inline bool verify_solver_row_case( const char* name, const golden_row_t* table, std::size_t count,
    void ( *resolve )( solver_rigid_t&, solver_rigid_t&, solver_constraint_t& ), bool split )
{
    int checked = 0;
    int failed = 0;
    for ( std::size_t index = 0; index < count; ++index )
    {
        const float* in = table[ index ].m_in;
        solver_rigid_t a{};
        solver_rigid_t b{};
        solver_constraint_t c{};
        auto body = [ split ]( solver_rigid_t& r, const float* v )
        {
            r.inverse_mass = v[ 0 ];
            float* linear = split ? r.push_velocity : r.delta_linear;
            float* angular = split ? r.turn_velocity : r.delta_angular;
            for ( int i = 0; i < 3; ++i )
            {
                linear[ i ] = v[ 1 + i ];
                angular[ i ] = v[ 4 + i ];
                r.angular_factor[ i ] = v[ 7 + i ];
                r.inverse_mass_vector[ i ] = v[ 10 + i ];
            }
        };
        body( a, in );
        body( b, in + 13 );
        const float* r = in + 26;
        for ( int i = 0; i < 3; ++i )
        {
            c.relpos1_cross_normal[ i ] = r[ i ];
            c.contact_normal[ i ] = r[ 3 + i ];
            c.relpos2_cross_normal[ i ] = r[ 6 + i ];
            c.angular_component_a[ i ] = r[ 9 + i ];
            c.angular_component_b[ i ] = r[ 12 + i ];
        }
        float* applied = split ? c.applied_push_impulse : c.applied_impulse;
        for ( int lane = 0; lane < 4; ++lane )
            applied[ lane ] = r[ 15 ];
        c.jac_diag_ab_inv = r[ 16 ];
        ( split ? c.rhs_penetration : c.rhs ) = r[ 17 ];
        c.cfm = r[ 18 ];
        c.lower_limit = r[ 19 ];
        c.upper_limit = r[ 20 ];

        const int recoveries = g_split_impulse_recoveries;
        resolve( a, b, c );
        const int counted = g_split_impulse_recoveries - recoveries;

        const float* al = split ? a.push_velocity : a.delta_linear;
        const float* aa = split ? a.turn_velocity : a.delta_angular;
        const float* bl = split ? b.push_velocity : b.delta_linear;
        const float* ba = split ? b.turn_velocity : b.delta_angular;
        const float ours[ 16 ] = { al[ 0 ], al[ 1 ], al[ 2 ], aa[ 0 ], aa[ 1 ], aa[ 2 ],
            bl[ 0 ], bl[ 1 ], bl[ 2 ], ba[ 0 ], ba[ 1 ], ba[ 2 ],
            applied[ 0 ], applied[ 1 ], applied[ 2 ], applied[ 3 ] };
        ++checked;
        if ( counted != ( split ? 1 : 0 ) )
        {
            ++failed;
            LOG_ERROR( "%s vector %d: dword_7590A8 moved by %d", name, static_cast< int >( index ), counted );
            continue;
        }
        if ( std::memcmp( ours, table[ index ].m_out, sizeof( ours ) ) != 0 )
        {
            ++failed;
            for ( int slot = 0; slot < 16; ++slot )
            {
                if ( std::memcmp( &ours[ slot ], &table[ index ].m_out[ slot ], sizeof( float ) ) != 0 )
                {
                    LOG_ERROR( "%s vector %d slot %d: %.9g vs retail %.9g",
                        name, static_cast< int >( index ), slot, ours[ slot ], table[ index ].m_out[ slot ] );
                    break;
                }
            }
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "%s matches retail on %d vectors, bit exact", name, checked );
    else
        LOG_ERROR( "%s FAILED %d of %d vectors", name, failed, checked );
    return failed == 0;
}

// The SIMD row resolvers @0x570AE0 (split), @0x56FD80 (generic) and
// @0x5702C0 (lower limit): all four lanes in and out, one layout (golden.py).
inline bool verify_solver_row_simd_case( const char* name, const golden_simd_row_t* table, std::size_t count,
    void ( *resolve )( solver_rigid_t&, solver_rigid_t&, solver_constraint_t& ), bool split )
{
    int checked = 0;
    int failed = 0;
    for ( std::size_t index = 0; index < count; ++index )
    {
        const float* in = table[ index ].m_in;
        solver_rigid_t a{};
        solver_rigid_t b{};
        solver_constraint_t c{};
        float* a_linear = split ? a.push_velocity : a.delta_linear;
        float* a_angular = split ? a.turn_velocity : a.delta_angular;
        float* b_linear = split ? b.push_velocity : b.delta_linear;
        float* b_angular = split ? b.turn_velocity : b.delta_angular;
        float* applied = split ? c.applied_push_impulse : c.applied_impulse;
        for ( int lane = 0; lane < 4; ++lane )
        {
            a.inverse_mass_vector[ lane ] = in[ lane ];
            a_linear[ lane ] = in[ 4 + lane ];
            a_angular[ lane ] = in[ 8 + lane ];
            b.inverse_mass_vector[ lane ] = in[ 12 + lane ];
            b_linear[ lane ] = in[ 16 + lane ];
            b_angular[ lane ] = in[ 20 + lane ];
            c.relpos1_cross_normal[ lane ] = in[ 24 + lane ];
            c.contact_normal[ lane ] = in[ 28 + lane ];
            c.relpos2_cross_normal[ lane ] = in[ 32 + lane ];
            c.angular_component_a[ lane ] = in[ 36 + lane ];
            c.angular_component_b[ lane ] = in[ 40 + lane ];
            applied[ lane ] = in[ 44 ];
            if ( split )
                c.applied_impulse[ lane ] = in[ 49 ];
        }
        if ( !split )
            c.upper_limit = in[ 49 ];
        c.jac_diag_ab_inv = in[ 45 ];
        ( split ? c.rhs_penetration : c.rhs ) = in[ 46 ];
        c.cfm = in[ 47 ];
        c.lower_limit = in[ 48 ];

        const int recoveries = g_split_impulse_recoveries;
        resolve( a, b, c );
        ++checked;
        if ( g_split_impulse_recoveries - recoveries != ( split ? 1 : 0 ) )
        {
            ++failed;
            LOG_ERROR( "%s vector %d: dword_7590A8 moved wrongly", name, static_cast< int >( index ) );
            continue;
        }
        float ours[ 24 ];
        std::memcpy( ours, a_linear, 16 );
        std::memcpy( ours + 4, a_angular, 16 );
        std::memcpy( ours + 8, b_linear, 16 );
        std::memcpy( ours + 12, b_angular, 16 );
        std::memcpy( ours + 16, c.applied_push_impulse, 16 );
        std::memcpy( ours + 20, c.applied_impulse, 16 );
        for ( int slot = 0; slot < 24; ++slot )
        {
            if ( std::memcmp( &ours[ slot ], &table[ index ].m_out[ slot ], sizeof( float ) ) != 0 )
            {
                ++failed;
                LOG_ERROR( "%s vector %d slot %d: %.9g vs retail %.9g",
                    name, static_cast< int >( index ), slot, ours[ slot ], table[ index ].m_out[ slot ] );
                break;
            }
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "%s matches retail on %d vectors, all four lanes, bit exact", name, checked );
    else
        LOG_ERROR( "%s FAILED %d of %d vectors", name, failed, checked );
    return failed == 0;
}
inline bool verify_solver_row( )
{
    bool ok = true;
    ok = verify_solver_row_case( "solver_row_generic (@0x56FF70)", g_golden_row, g_golden_row_count, solver_row_generic, false ) && ok;
    ok = verify_solver_row_case( "solver_row_lower (@0x570470)", g_golden_row_lower, g_golden_row_lower_count, solver_row_lower, false ) && ok;
    ok = verify_solver_row_case( "solver_row_split (@0x5707B0)", g_golden_row_split, g_golden_row_split_count, solver_row_split, true ) && ok;
    ok = verify_solver_row_simd_case( "solver_row_split_simd (@0x570AE0)", g_golden_row_simd, g_golden_row_simd_count, solver_row_split_simd, true ) && ok;
    ok = verify_solver_row_simd_case( "solver_row_generic_simd (@0x56FD80)", g_golden_row_simd_generic, g_golden_row_simd_generic_count, solver_row_generic_simd, false ) && ok;
    ok = verify_solver_row_simd_case( "solver_row_lower_simd (@0x5702C0)", g_golden_row_simd_lower, g_golden_row_simd_lower_count, solver_row_lower_simd, false ) && ok;
    return ok;
}

// solve_single_iteration against solveSingleIteration @0x573EE0: a whole
// hand-built solver (3 bodies, 4 contact / 2 non-contact / 4 friction rows)
// in four modes at iterations 0, 3 and 8. Bodies, impulses, friction
// limits, both order arrays and the seed must all match to the bit.
inline bool verify_iteration( )
{
    int checked = 0;
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_iteration_count; ++index )
    {
        const golden_iteration_t& g = g_golden_iteration[ index ];
        solver_rigid_t bodies[ 3 ]{};
        for ( int b = 0; b < 3; ++b )
        {
            const float* v = g.m_bodies[ b ];
            bodies[ b ].inverse_mass = v[ 0 ];
            for ( int lane = 0; lane < 4; ++lane )
            {
                bodies[ b ].angular_factor[ lane ] = v[ 1 + lane ];
                bodies[ b ].inverse_mass_vector[ lane ] = v[ 5 + lane ];
                bodies[ b ].delta_linear[ lane ] = v[ 9 + lane ];
                bodies[ b ].delta_angular[ lane ] = v[ 13 + lane ];
            }
        }
        solver_constraint_t rows[ 10 ]{};
        for ( int k = 0; k < 10; ++k )
        {
            const float* r = g.m_rows[ k ];
            solver_constraint_t& c = rows[ k ];
            for ( int lane = 0; lane < 4; ++lane )
            {
                c.relpos1_cross_normal[ lane ] = r[ lane ];
                c.contact_normal[ lane ] = r[ 4 + lane ];
                c.relpos2_cross_normal[ lane ] = r[ 8 + lane ];
                c.angular_component_a[ lane ] = r[ 12 + lane ];
                c.angular_component_b[ lane ] = r[ 16 + lane ];
                c.applied_impulse[ lane ] = r[ 20 ];
            }
            c.jac_diag_ab_inv = r[ 21 ];
            c.rhs = r[ 22 ];
            c.cfm = r[ 23 ];
            c.lower_limit = r[ 24 ];
            c.upper_limit = r[ 25 ];
            c.friction = r[ 26 ];
            c.friction_index = g.m_meta[ k ][ 0 ];
            c.solver_body_a = &bodies[ g.m_meta[ k ][ 1 ] ];
            c.solver_body_b = &bodies[ g.m_meta[ k ][ 2 ] ];
        }
        int order_contacts[ 4 ];
        int order_frictions[ 4 ];
        std::memcpy( order_contacts, g.m_order_contacts, sizeof( order_contacts ) );
        std::memcpy( order_frictions, g.m_order_frictions, sizeof( order_frictions ) );

        solver_t s;
        s.contacts = rows;
        s.contact_count = 4;
        s.non_contacts = rows + 4;
        s.non_contact_count = 2;
        s.frictions = rows + 6;
        s.friction_count = 4;
        s.order_contacts = order_contacts;
        s.order_frictions = order_frictions;
        s.seed = g.m_seed;
        solver_info_t info{};
        info.time_step = 1.0f / 60.0f;
        info.solver_mode = static_cast< int >( g.m_mode );

        solve_single_iteration( s, g.m_iteration, info, nullptr, nullptr, 0 );

        float ours_bodies[ 24 ];
        for ( int b = 0; b < 3; ++b )
        {
            std::memcpy( ours_bodies + 8 * b, bodies[ b ].delta_linear, 16 );
            std::memcpy( ours_bodies + 8 * b + 4, bodies[ b ].delta_angular, 16 );
        }
        float ours_rows[ 60 ];
        for ( int k = 0; k < 10; ++k )
        {
            std::memcpy( ours_rows + 6 * k, rows[ k ].applied_impulse, 16 );
            ours_rows[ 6 * k + 4 ] = rows[ k ].lower_limit;
            ours_rows[ 6 * k + 5 ] = rows[ k ].upper_limit;
        }
        ++checked;
        const char* what = nullptr;
        int slot = -1;
        for ( int i = 0; i < 24 && !what; ++i )
            if ( std::memcmp( &ours_bodies[ i ], &g.m_out_bodies[ i ], 4 ) != 0 ) { what = "body"; slot = i; }
        for ( int i = 0; i < 60 && !what; ++i )
            if ( std::memcmp( &ours_rows[ i ], &g.m_out_rows[ i ], 4 ) != 0 ) { what = "row"; slot = i; }
        if ( !what && std::memcmp( order_contacts, g.m_out_order_contacts, sizeof( order_contacts ) ) != 0 ) what = "contact order";
        if ( !what && std::memcmp( order_frictions, g.m_out_order_frictions, sizeof( order_frictions ) ) != 0 ) what = "friction order";
        if ( !what && s.seed != g.m_out_seed ) what = "seed";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "solve_single_iteration vector %d (mode %#x, iteration %d): %s %d differs",
                static_cast< int >( index ), g.m_mode, g.m_iteration, what, slot );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "solve_single_iteration matches solveSingleIteration @0x573EE0 on %d whole-solver vectors, bit exact", checked );
    else
        LOG_ERROR( "solve_single_iteration FAILED %d of %d vectors against @0x573EE0", failed, checked );
    return failed == 0;
}
// split_impulse_iterations against splitImpulseIterations @0x5742B0: the
// splitImpulse gate, both modes, 1 and 3 passes, a zero-rhsPenetration
// contact that must be skipped, and the dword_7590A8 count.
inline bool verify_split_iterations( )
{
    int checked = 0;
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_split_iterations_count; ++index )
    {
        const golden_split_iterations_t& g = g_golden_split_iterations[ index ];
        solver_rigid_t bodies[ 3 ]{};
        for ( int b = 0; b < 3; ++b )
        {
            const float* v = g.m_bodies[ b ];
            bodies[ b ].inverse_mass = v[ 0 ];
            for ( int lane = 0; lane < 4; ++lane )
            {
                bodies[ b ].angular_factor[ lane ] = v[ 1 + lane ];
                bodies[ b ].inverse_mass_vector[ lane ] = v[ 5 + lane ];
                bodies[ b ].push_velocity[ lane ] = v[ 9 + lane ];
                bodies[ b ].turn_velocity[ lane ] = v[ 13 + lane ];
            }
        }
        solver_constraint_t rows[ 4 ]{};
        for ( int k = 0; k < 4; ++k )
        {
            const float* r = g.m_rows[ k ];
            solver_constraint_t& c = rows[ k ];
            for ( int lane = 0; lane < 4; ++lane )
            {
                c.relpos1_cross_normal[ lane ] = r[ lane ];
                c.contact_normal[ lane ] = r[ 4 + lane ];
                c.relpos2_cross_normal[ lane ] = r[ 8 + lane ];
                c.angular_component_a[ lane ] = r[ 12 + lane ];
                c.angular_component_b[ lane ] = r[ 16 + lane ];
                c.applied_push_impulse[ lane ] = r[ 20 ];
                c.applied_impulse[ lane ] = r[ 25 ];
            }
            c.jac_diag_ab_inv = r[ 21 ];
            c.rhs_penetration = r[ 22 ];
            c.cfm = r[ 23 ];
            c.lower_limit = r[ 24 ];
            c.solver_body_a = &bodies[ g.m_meta[ k ][ 0 ] ];
            c.solver_body_b = &bodies[ g.m_meta[ k ][ 1 ] ];
        }
        int order[ 4 ];
        std::memcpy( order, g.m_order, sizeof( order ) );
        solver_t s;
        s.contacts = rows;
        s.contact_count = 4;
        s.order_contacts = order;
        solver_info_t info{};
        info.num_iterations = g.m_iterations;
        info.split_impulse = g.m_split;
        info.solver_mode = static_cast< int >( g.m_mode );

        const int recoveries = g_split_impulse_recoveries;
        split_impulse_iterations( s, info );
        const int counted = g_split_impulse_recoveries - recoveries;

        float ours_bodies[ 24 ];
        for ( int b = 0; b < 3; ++b )
        {
            std::memcpy( ours_bodies + 8 * b, bodies[ b ].push_velocity, 16 );
            std::memcpy( ours_bodies + 8 * b + 4, bodies[ b ].turn_velocity, 16 );
        }
        float ours_rows[ 32 ];
        for ( int k = 0; k < 4; ++k )
        {
            std::memcpy( ours_rows + 8 * k, rows[ k ].applied_push_impulse, 16 );
            std::memcpy( ours_rows + 8 * k + 4, rows[ k ].applied_impulse, 16 );
        }
        ++checked;
        const bool same = counted == g.m_counted
            && std::memcmp( ours_bodies, g.m_out_bodies, sizeof( ours_bodies ) ) == 0
            && std::memcmp( ours_rows, g.m_out_rows, sizeof( ours_rows ) ) == 0;
        if ( !same )
        {
            ++failed;
            LOG_ERROR( "split_impulse_iterations vector %d (split %d, mode %#x, %d passes) differs; count %d vs %d",
                static_cast< int >( index ), g.m_split, g.m_mode, g.m_iterations, counted, g.m_counted );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "split_impulse_iterations matches splitImpulseIterations @0x5742B0 on %d whole-solver vectors, bit exact", checked );
    else
        LOG_ERROR( "split_impulse_iterations FAILED %d of %d vectors against @0x5742B0", failed, checked );
    return failed == 0;
}
// solve_iterations against solveGroupCacheFriendlyIterations @0x5743C0:
// the split passes then numIterations single iterations, in four modes, 1 /
// 3 / 9 iterations (9 reaches the second shuffle at iteration 8), seed from 0.
inline bool verify_solve_iterations( )
{
    int checked = 0;
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_solve_iterations_count; ++index )
    {
        const golden_solve_iterations_t& g = g_golden_solve_iterations[ index ];
        solver_rigid_t bodies[ 3 ]{};
        for ( int b = 0; b < 3; ++b )
        {
            const float* v = g.m_bodies[ b ];
            bodies[ b ].inverse_mass = v[ 0 ];
            for ( int lane = 0; lane < 4; ++lane )
            {
                bodies[ b ].angular_factor[ lane ] = v[ 1 + lane ];
                bodies[ b ].inverse_mass_vector[ lane ] = v[ 5 + lane ];
                bodies[ b ].delta_linear[ lane ] = v[ 9 + lane ];
                bodies[ b ].delta_angular[ lane ] = v[ 13 + lane ];
                bodies[ b ].push_velocity[ lane ] = v[ 17 + lane ];
                bodies[ b ].turn_velocity[ lane ] = v[ 21 + lane ];
            }
        }
        solver_constraint_t rows[ 10 ]{};
        for ( int k = 0; k < 10; ++k )
        {
            const float* r = g.m_rows[ k ];
            solver_constraint_t& c = rows[ k ];
            for ( int lane = 0; lane < 4; ++lane )
            {
                c.relpos1_cross_normal[ lane ] = r[ lane ];
                c.contact_normal[ lane ] = r[ 4 + lane ];
                c.relpos2_cross_normal[ lane ] = r[ 8 + lane ];
                c.angular_component_a[ lane ] = r[ 12 + lane ];
                c.angular_component_b[ lane ] = r[ 16 + lane ];
                c.applied_impulse[ lane ] = r[ 20 ];
                c.applied_push_impulse[ lane ] = r[ 27 ];
            }
            c.jac_diag_ab_inv = r[ 21 ];
            c.rhs = r[ 22 ];
            c.cfm = r[ 23 ];
            c.lower_limit = r[ 24 ];
            c.upper_limit = r[ 25 ];
            c.friction = r[ 26 ];
            c.rhs_penetration = r[ 28 ];
            c.friction_index = g.m_meta[ k ][ 0 ];
            c.solver_body_a = &bodies[ g.m_meta[ k ][ 1 ] ];
            c.solver_body_b = &bodies[ g.m_meta[ k ][ 2 ] ];
        }
        int order_contacts[ 4 ];
        int order_frictions[ 4 ];
        std::memcpy( order_contacts, g.m_order_contacts, sizeof( order_contacts ) );
        std::memcpy( order_frictions, g.m_order_frictions, sizeof( order_frictions ) );
        solver_t s;
        s.contacts = rows;
        s.contact_count = 4;
        s.non_contacts = rows + 4;
        s.non_contact_count = 2;
        s.frictions = rows + 6;
        s.friction_count = 4;
        s.order_contacts = order_contacts;
        s.order_frictions = order_frictions;
        solver_info_t info{};
        info.time_step = 1.0f / 60.0f;
        info.num_iterations = g.m_iterations;
        info.split_impulse = g.m_split;
        info.solver_mode = static_cast< int >( g.m_mode );

        const int recoveries = g_split_impulse_recoveries;
        solve_iterations( s, info, nullptr, nullptr, 0 );
        const int counted = g_split_impulse_recoveries - recoveries;

        float ours_bodies[ 48 ];
        for ( int b = 0; b < 3; ++b )
        {
            std::memcpy( ours_bodies + 16 * b, bodies[ b ].delta_linear, 16 );
            std::memcpy( ours_bodies + 16 * b + 4, bodies[ b ].delta_angular, 16 );
            std::memcpy( ours_bodies + 16 * b + 8, bodies[ b ].push_velocity, 16 );
            std::memcpy( ours_bodies + 16 * b + 12, bodies[ b ].turn_velocity, 16 );
        }
        float ours_rows[ 100 ];
        for ( int k = 0; k < 10; ++k )
        {
            std::memcpy( ours_rows + 10 * k, rows[ k ].applied_impulse, 16 );
            std::memcpy( ours_rows + 10 * k + 4, rows[ k ].applied_push_impulse, 16 );
            ours_rows[ 10 * k + 8 ] = rows[ k ].lower_limit;
            ours_rows[ 10 * k + 9 ] = rows[ k ].upper_limit;
        }
        ++checked;
        const bool same = counted == g.m_counted && s.seed == g.m_out_seed
            && std::memcmp( ours_bodies, g.m_out_bodies, sizeof( ours_bodies ) ) == 0
            && std::memcmp( ours_rows, g.m_out_rows, sizeof( ours_rows ) ) == 0
            && std::memcmp( order_contacts, g.m_out_order_contacts, sizeof( order_contacts ) ) == 0
            && std::memcmp( order_frictions, g.m_out_order_frictions, sizeof( order_frictions ) ) == 0;
        if ( !same )
        {
            ++failed;
            LOG_ERROR( "solve_iterations vector %d (mode %#x, %d iterations, split %d) differs",
                static_cast< int >( index ), g.m_mode, g.m_iterations, g.m_split );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "solve_iterations matches solveGroupCacheFriendlyIterations @0x5743C0 on %d whole-solver vectors, bit exact", checked );
    else
        LOG_ERROR( "solve_iterations FAILED %d of %d vectors against @0x5743C0", failed, checked );
    return failed == 0;
}
// basis_rotation against btMatrix3x3::getRotation @0x50F8E0: trace > 0 and
// each largest-diagonal branch. Square roots and divides only - bit exact.
inline bool verify_rotation( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_rotation_count; ++index )
    {
        float q[ 4 ];
        basis_rotation( g_golden_rotation[ index ].m_in, q );
        if ( std::memcmp( q, g_golden_rotation[ index ].m_out, sizeof( q ) ) != 0 )
        {
            ++failed;
            LOG_ERROR( "basis_rotation vector %d differs from @0x50F8E0", static_cast< int >( index ) );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "basis_rotation matches btMatrix3x3::getRotation @0x50F8E0 on %d vectors, bit exact",
            static_cast< int >( g_golden_rotation_count ) );
    return failed == 0;
}

// The two writebacks against @0x574AC0 and @0x50E810. Bit exact EXCEPT that
// sinf / cosf are the CRT's in retail and float(sin(double)) here and in the
// oracle - so this proves everything around the libm call, not the call.
inline bool verify_writeback( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_writeback_count; ++index )
    {
        const golden_writeback_t& g = g_golden_writeback[ index ];
        solver_rigid_t b{};
        b.inverse_mass = g.m_in[ 0 ];
        float* lanes[ 6 ] = { b.linear_velocity, b.angular_velocity, b.delta_linear,
            b.delta_angular, b.push_velocity, b.turn_velocity };
        for ( int k = 0; k < 6; ++k )
            std::memcpy( lanes[ k ], g.m_in + 1 + 4 * k, 16 );
        std::memcpy( b.basis, g.m_in + 25, 48 );
        std::memcpy( b.origin, g.m_in + 37, 16 );
        if ( g.m_split )
            writeback_velocity_split( b, g.m_dt );
        else
            writeback_velocity( b );
        float ours[ 24 ];
        std::memcpy( ours, b.linear_velocity, 16 );
        std::memcpy( ours + 4, b.angular_velocity, 16 );
        std::memcpy( ours + 8, b.basis, 48 );
        std::memcpy( ours + 20, b.origin, 16 );
        for ( int slot = 0; slot < 24; ++slot )
        {
            if ( std::memcmp( &ours[ slot ], &g.m_out[ slot ], 4 ) != 0 )
            {
                ++failed;
                LOG_ERROR( "writeback vector %d (split %d) slot %d: %.9g vs retail %.9g",
                    static_cast< int >( index ), g.m_split, slot, ours[ slot ], g.m_out[ slot ] );
                break;
            }
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "writeback matches internalWritebackVelocity @0x574AC0 / @0x50E810 on %d vectors, bit exact (libm as the oracle's)",
            static_cast< int >( g_golden_writeback_count ) );
    else
        LOG_ERROR( "writeback FAILED %d of %d vectors", failed, static_cast< int >( g_golden_writeback_count ) );
    return failed == 0;
}
// solver_finish against solveGroupCacheFriendlyFinish @0x5744F0: impulses
// back to manifold points (and the mode & 8 lateral copies), typed
// constraints and the breaking threshold, the rigid-only body writeback in
// both splitImpulse modes, and the pool sizes. libm as the writeback check.
inline bool verify_finish( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_finish_count; ++index )
    {
        const golden_finish_t& g = g_golden_finish[ index ];
        solver_rigid_t bodies[ 3 ]{};
        solver_rigid_t* list[ 3 ];
        for ( int b = 0; b < 3; ++b )
        {
            const float* v = g.m_bodies[ b ];
            bodies[ b ].inverse_mass = v[ 0 ];
            float* lanes[ 6 ] = { bodies[ b ].linear_velocity, bodies[ b ].angular_velocity, bodies[ b ].delta_linear,
                bodies[ b ].delta_angular, bodies[ b ].push_velocity, bodies[ b ].turn_velocity };
            for ( int k = 0; k < 6; ++k )
                std::memcpy( lanes[ k ], v + 1 + 4 * k, 16 );
            std::memcpy( bodies[ b ].basis, v + 25, 48 );
            std::memcpy( bodies[ b ].origin, v + 37, 16 );
            bodies[ b ].internal_type = g.m_types[ b ];
            list[ b ] = &bodies[ b ];
        }
        solver_contact_point_t points[ 4 ]{};
        solver_typed_constraint_t constraints[ 2 ]{};
        solver_constraint_t rows[ 10 ]{};
        for ( int k = 0; k < 10; ++k )
            for ( int lane = 0; lane < 4; ++lane )
                rows[ k ].applied_impulse[ lane ] = g.m_applied[ k ];
        for ( int k = 0; k < 4; ++k )
        {
            rows[ k ].friction_index = g.m_friction_index[ k ];
            rows[ k ].original_contact_point = &points[ k ];
        }
        for ( int k = 0; k < 2; ++k )
        {
            constraints[ k ].breaking_impulse_threshold = g.m_thresholds[ k ];
            constraints[ k ].enabled = 1;
            rows[ 4 + k ].original_contact_point = &constraints[ k ];
        }
        solver_t s;
        s.contacts = rows;
        s.contact_count = 4;
        s.non_contacts = rows + 4;
        s.non_contact_count = 2;
        s.frictions = rows + 6;
        s.friction_count = 4;
        solver_info_t info{};
        info.time_step = g.m_dt;
        info.split_impulse = g.m_split;
        info.solver_mode = static_cast< int >( g.m_mode );

        solver_finish( s, list, 3, info );

        float ours_points[ 12 ];
        for ( int k = 0; k < 4; ++k )
        {
            ours_points[ 3 * k ] = points[ k ].applied_impulse;
            ours_points[ 3 * k + 1 ] = points[ k ].lateral_impulse_1;
            ours_points[ 3 * k + 2 ] = points[ k ].lateral_impulse_2;
        }
        float ours_constraints[ 2 ] = { constraints[ 0 ].applied_impulse, constraints[ 1 ].applied_impulse };
        float ours_bodies[ 72 ];
        for ( int b = 0; b < 3; ++b )
        {
            std::memcpy( ours_bodies + 24 * b, bodies[ b ].linear_velocity, 16 );
            std::memcpy( ours_bodies + 24 * b + 4, bodies[ b ].angular_velocity, 16 );
            std::memcpy( ours_bodies + 24 * b + 8, bodies[ b ].basis, 48 );
            std::memcpy( ours_bodies + 24 * b + 20, bodies[ b ].origin, 16 );
        }
        const bool same = std::memcmp( ours_points, g.m_out_points, sizeof( ours_points ) ) == 0
            && std::memcmp( ours_constraints, g.m_out_constraints, sizeof( ours_constraints ) ) == 0
            && constraints[ 0 ].enabled == g.m_out_enabled[ 0 ] && constraints[ 1 ].enabled == g.m_out_enabled[ 1 ]
            && std::memcmp( ours_bodies, g.m_out_bodies, sizeof( ours_bodies ) ) == 0
            && s.contact_count == g.m_out_sizes[ 0 ] && s.non_contact_count == g.m_out_sizes[ 1 ]
            && s.friction_count == g.m_out_sizes[ 2 ];
        if ( !same )
        {
            ++failed;
            LOG_ERROR( "solver_finish vector %d (split %d, mode %u) differs", static_cast< int >( index ), g.m_split, g.m_mode );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "solver_finish matches solveGroupCacheFriendlyFinish @0x5744F0 on %d vectors, bit exact (libm as the oracle's)",
            static_cast< int >( g_golden_finish_count ) );
    return failed == 0;
}
// apply_anisotropic_friction against applyAnisotropicFriction @0x570CA0.
inline bool verify_anisotropic( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_aniso_count; ++index )
    {
        const golden_aniso_t& g = g_golden_aniso[ index ];
        solver_rigid_t b{};
        std::memcpy( b.basis, g.m_in, 48 );
        std::memcpy( b.anisotropic_friction, g.m_in + 12, 16 );
        b.has_anisotropic_friction = g.m_flag;
        float d[ 4 ];
        std::memcpy( d, g.m_in + 16, 16 );
        apply_anisotropic_friction( &b, d );
        if ( std::memcmp( d, g.m_out, sizeof( d ) ) != 0 )
        {
            ++failed;
            LOG_ERROR( "apply_anisotropic_friction vector %d differs from @0x570CA0", static_cast< int >( index ) );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "apply_anisotropic_friction matches applyAnisotropicFriction @0x570CA0 on %d vectors, bit exact",
            static_cast< int >( g_golden_aniso_count ) );
    return failed == 0;
}
// set_friction_constraint_impulse against setFrictionConstraintImpulse
// @0x572230: every solverMode combination of bits 4 / 8 / 0x10, null and
// static bodies.
inline bool verify_friction_impulse( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_friction_impulse_count; ++index )
    {
        const golden_friction_impulse_t& g = g_golden_friction_impulse[ index ];
        solver_rigid_t bodies[ 2 ]{};
        for ( int k = 0; k < 2; ++k )
        {
            const float* v = g.m_bodies[ k ];
            bodies[ k ].inverse_mass = v[ 0 ];
            for ( int i = 0; i < 3; ++i )
            {
                bodies[ k ].linear_factor[ i ] = v[ 1 + i ];
                bodies[ k ].angular_factor[ i ] = v[ 4 + i ];
                bodies[ k ].delta_linear[ i ] = v[ 7 + i ];
                bodies[ k ].delta_angular[ i ] = v[ 10 + i ];
            }
        }
        solver_constraint_t pool[ 4 ]{};
        for ( int k = 0; k < 2; ++k )
        {
            solver_constraint_t& f = pool[ g.m_friction_index + k ];
            const float* r = g.m_frictions[ k ];
            for ( int i = 0; i < 3; ++i )
            {
                f.contact_normal[ i ] = r[ i ];
                f.angular_component_a[ i ] = r[ 3 + i ];
                f.angular_component_b[ i ] = r[ 6 + i ];
            }
            for ( int lane = 0; lane < 4; ++lane )
                f.applied_impulse[ lane ] = r[ 9 ];
        }
        solver_constraint_t contact{};
        contact.friction_index = g.m_friction_index;
        solver_contact_point_t point{};
        point.lateral_impulse_1 = g.m_scalars[ 0 ];
        point.lateral_impulse_2 = g.m_scalars[ 1 ];
        solver_info_t info{};
        info.warmstarting_factor = g.m_scalars[ 2 ];
        info.solver_mode = static_cast< int >( g.m_mode );
        solver_t s;
        s.frictions = pool;
        set_friction_constraint_impulse( s, contact, g.m_a_null ? nullptr : &bodies[ 0 ],
            g.m_b_null ? nullptr : &bodies[ 1 ], point, info );
        float ours[ 20 ];
        std::memcpy( ours, pool[ g.m_friction_index ].applied_impulse, 16 );
        std::memcpy( ours + 4, pool[ g.m_friction_index + 1 ].applied_impulse, 16 );
        for ( int k = 0; k < 2; ++k )
        {
            std::memcpy( ours + 8 + 6 * k, bodies[ k ].delta_linear, 12 );
            std::memcpy( ours + 11 + 6 * k, bodies[ k ].delta_angular, 12 );
        }
        if ( std::memcmp( ours, g.m_out, sizeof( ours ) ) != 0 )
        {
            ++failed;
            LOG_ERROR( "set_friction_constraint_impulse vector %d (mode %#x) differs", static_cast< int >( index ), g.m_mode );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "set_friction_constraint_impulse matches setFrictionConstraintImpulse @0x572230 on %d vectors, bit exact",
            static_cast< int >( g_golden_friction_impulse_count ) );
    return failed == 0;
}
// setup_friction_constraint against setupFrictionConstraint @0x570E00. The
// row starts as 7.0 everywhere, so the slots retail leaves alone (+120,
// +124, +156) are compared too; the body slots by identity (the object or
// getFixedBody's static).
inline bool verify_friction_setup( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_friction_setup_count; ++index )
    {
        const golden_friction_setup_t& g = g_golden_friction_setup[ index ];
        solver_rigid_t objects[ 2 ]{};
        for ( int k = 0; k < 2; ++k )
        {
            const float* v = g.m_bodies[ k ];
            objects[ k ].internal_type = g.m_types[ k ];
            objects[ k ].inverse_mass = v[ 0 ];
            for ( int i = 0; i < 3; ++i )
            {
                objects[ k ].angular_factor[ i ] = v[ 1 + i ];
                for ( int j = 0; j < 3; ++j )
                    objects[ k ].inv_inertia_world[ 4 * i + j ] = v[ 4 + 3 * i + j ];
                objects[ k ].linear_velocity[ i ] = v[ 13 + i ];
                objects[ k ].angular_velocity[ i ] = v[ 16 + i ];
            }
        }
        solver_constraint_t c;
        float* words = reinterpret_cast< float* >( &c );
        for ( std::size_t i = 0; i < sizeof( c ) / sizeof( float ); ++i )
            words[ i ] = 7.0f;
        solver_contact_point_t point{};
        point.combined_friction = g.m_scalars[ 0 ];
        setup_friction_constraint( c, g.m_normal, point, g.m_r1, g.m_r2, &objects[ 0 ], &objects[ 1 ],
            g.m_scalars[ 1 ], g.m_scalars[ 2 ], g.m_scalars[ 3 ] );
        const float tail[ 5 ] = { c.rhs, c.cfm, c.lower_limit, c.upper_limit, c.rhs_penetration };
        const bool same = std::memcmp( &c, g.m_raw, sizeof( g.m_raw ) ) == 0
            && std::memcmp( tail, g.m_tail, sizeof( tail ) ) == 0
            && ( c.solver_body_a == &objects[ 0 ] ) == ( g.m_a_is_object != 0 )
            && ( c.solver_body_b == &objects[ 1 ] ) == ( g.m_b_is_object != 0 )
            && ( g.m_a_is_object || c.solver_body_a == &fixed_body( ) )
            && ( g.m_b_is_object || c.solver_body_b == &fixed_body( ) )
            && ( c.original_contact_point == nullptr ) == ( g.m_point_null != 0 );
        if ( !same )
        {
            ++failed;
            LOG_ERROR( "setup_friction_constraint vector %d (types %d %d) differs", static_cast< int >( index ),
                g.m_types[ 0 ], g.m_types[ 1 ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "setup_friction_constraint matches setupFrictionConstraint @0x570E00 on %d vectors, bit exact",
            static_cast< int >( g_golden_friction_setup_count ) );
    return failed == 0;
}
// add_friction_constraint against addFrictionConstraint @0x571430: growth
// from empty, from full, with room; every existing row survives the copy;
// the new row as setupFrictionConstraint makes it, with +124 set.
inline bool verify_add_friction( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_add_friction_count; ++index )
    {
        const golden_add_friction_t& g = g_golden_add_friction[ index ];
        solver_rigid_t objects[ 2 ]{};
        for ( int k = 0; k < 2; ++k )
        {
            const float* v = g.m_bodies[ k ];
            objects[ k ].internal_type = g.m_types[ k ];
            objects[ k ].inverse_mass = v[ 0 ];
            for ( int i = 0; i < 3; ++i )
            {
                objects[ k ].angular_factor[ i ] = v[ 1 + i ];
                for ( int j = 0; j < 3; ++j )
                    objects[ k ].inv_inertia_world[ 4 * i + j ] = v[ 4 + 3 * i + j ];
                objects[ k ].linear_velocity[ i ] = v[ 13 + i ];
                objects[ k ].angular_velocity[ i ] = v[ 16 + i ];
            }
        }
        solver_constraint_t storage[ 4 ];
        for ( int k = 0; k < 4; ++k )
        {
            float* words = reinterpret_cast< float* >( &storage[ k ] );
            for ( int j = 0; j < 48; ++j )
                words[ j ] = k < g.m_size ? static_cast< float >( 1000 * k + j + 1 ) : 7.0f;
        }
        solver_t s;
        s.frictions = g.m_capacity != 0 ? storage : nullptr;
        s.friction_count = g.m_size;
        s.friction_capacity = g.m_capacity;
        s.owns_frictions = false;           // the test's storage is never freed by the solver
        solver_contact_point_t point{};
        point.combined_friction = g.m_scalars[ 0 ];
        const int row = add_friction_constraint( s, g.m_normal, g.m_friction_index, point, g.m_r1, g.m_r2,
            &objects[ 0 ], &objects[ 1 ], g.m_scalars[ 1 ], g.m_scalars[ 2 ], g.m_scalars[ 3 ] );
        bool same = row == g.m_size && s.friction_count == g.m_out_size && s.friction_capacity == g.m_out_capacity
            && ( s.frictions != storage ) == ( g.m_moved != 0 );
        for ( int k = 0; k < g.m_size && same; ++k )
        {
            const float* words = reinterpret_cast< const float* >( &s.frictions[ k ] );
            for ( int j = 0; j < 32; ++j )
                same = same && words[ j ] == static_cast< float >( 1000 * k + j + 1 );
        }
        const solver_constraint_t& c = s.frictions[ row ];
        const float tail[ 4 ] = { c.rhs, c.cfm, c.lower_limit, c.upper_limit };
        same = same && c.friction_index == g.m_friction_index
            && std::memcmp( &c, g.m_raw, 30 * sizeof( float ) ) == 0
            && std::memcmp( tail, g.m_tail, sizeof( tail ) ) == 0;
        if ( s.owns_frictions )
            delete[] s.frictions;
        if ( !same )
        {
            ++failed;
            LOG_ERROR( "add_friction_constraint vector %d (size %d, capacity %d) differs", static_cast< int >( index ),
                g.m_size, g.m_capacity );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "add_friction_constraint matches addFrictionConstraint @0x571430 on %d vectors (growth, copy, new row), bit exact",
            static_cast< int >( g_golden_add_friction_count ) );
    return failed == 0;
}
// setup_contact_constraint against setupContactConstraint @0x571560: the
// row (7.0 pre-fill, so untouched slots count), the five outputs and the
// warm-started bodies.
inline bool verify_contact_setup( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_contact_setup_count; ++index )
    {
        const golden_contact_setup_t& g = g_golden_contact_setup[ index ];
        solver_rigid_t objects[ 2 ]{};
        for ( int k = 0; k < 2; ++k )
        {
            const float* v = g.m_bodies[ k ];
            solver_rigid_t& o = objects[ k ];
            o.internal_type = g.m_types[ k ];
            o.inverse_mass = v[ 0 ];
            for ( int i = 0; i < 3; ++i )
            {
                o.linear_factor[ i ] = v[ 1 + i ];
                o.angular_factor[ i ] = v[ 4 + i ];
                for ( int j = 0; j < 3; ++j )
                    o.inv_inertia_world[ 4 * i + j ] = v[ 7 + 3 * i + j ];
                o.linear_velocity[ i ] = v[ 16 + i ];
                o.angular_velocity[ i ] = v[ 19 + i ];
                o.delta_linear[ i ] = v[ 22 + i ];
                o.delta_angular[ i ] = v[ 25 + i ];
                o.origin[ i ] = v[ 28 + i ];
            }
        }
        solver_contact_point_t cp{};
        std::memcpy( cp.position_world_on_b, g.m_point, 12 );
        std::memcpy( cp.position_world_on_a, g.m_point + 3, 12 );
        std::memcpy( cp.normal_world_on_b, g.m_point + 6, 16 );
        cp.distance = g.m_point[ 10 ];
        cp.combined_friction = g.m_point[ 11 ];
        cp.combined_restitution = g.m_point[ 12 ];
        cp.applied_impulse = g.m_point[ 13 ];
        cp.life_time = g.m_life;
        solver_info_t info{};
        info.time_step = g.m_infof[ 0 ];
        info.erp = g.m_infof[ 1 ];
        info.split_impulse_penetration_threshold = g.m_infof[ 2 ];
        info.linear_slop = g.m_infof[ 3 ];
        info.warmstarting_factor = g.m_infof[ 4 ];
        info.split_impulse = g.m_infoi[ 0 ];
        info.solver_mode = g.m_infoi[ 1 ];
        info.resting_contact_restitution_threshold = g.m_infoi[ 2 ];

        solver_constraint_t c;
        float* words = reinterpret_cast< float* >( &c );
        for ( std::size_t i = 0; i < sizeof( c ) / sizeof( float ); ++i )
            words[ i ] = 7.0f;
        float vel[ 4 ], rel_pos1[ 4 ], rel_pos2[ 4 ];
        float rel_vel = 0.0f, relaxation = 0.0f;
        setup_contact_constraint( c, &objects[ 0 ], &objects[ 1 ], cp, info, vel, rel_vel, relaxation, rel_pos1, rel_pos2 );

        const float tail[ 5 ] = { c.rhs, c.cfm, c.lower_limit, c.upper_limit, c.rhs_penetration };
        float outs[ 14 ];
        std::memcpy( outs, vel, 16 );
        outs[ 4 ] = rel_vel;
        outs[ 5 ] = relaxation;
        std::memcpy( outs + 6, rel_pos1, 16 );
        std::memcpy( outs + 10, rel_pos2, 16 );
        float deltas[ 12 ];
        for ( int k = 0; k < 2; ++k )
        {
            std::memcpy( deltas + 6 * k, objects[ k ].delta_linear, 12 );
            std::memcpy( deltas + 6 * k + 3, objects[ k ].delta_angular, 12 );
        }
        const char* what = nullptr;
        if ( std::memcmp( &c, g.m_raw, sizeof( g.m_raw ) ) != 0 ) what = "row";
        else if ( std::memcmp( tail, g.m_tail, sizeof( tail ) ) != 0 ) what = "row tail";
        else if ( std::memcmp( outs, g.m_outs, sizeof( outs ) ) != 0 ) what = "outputs";
        else if ( std::memcmp( deltas, g.m_deltas, sizeof( deltas ) ) != 0 ) what = "deltas";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "setup_contact_constraint vector %d: %s differ", static_cast< int >( index ), what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "setup_contact_constraint matches setupContactConstraint @0x571560 on %d vectors, bit exact",
            static_cast< int >( g_golden_contact_setup_count ) );
    return failed == 0;
}
// convert_contact against convertContact @0x5727D0: a whole manifold of 1-3
// points into empty pools - every contact and friction row (the slots
// written; +120 and a friction row's +156 are left alone and grown memory
// is not cleared in retail), the directions and flags written back into the
// points, and both bodies' warm-started deltas.
inline bool verify_convert_contact( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_convert_count; ++index )
    {
        const golden_convert_t& g = g_golden_convert[ index ];
        solver_rigid_t objects[ 2 ]{};
        for ( int k = 0; k < 2; ++k )
        {
            const float* v = g.m_bodies[ k ];
            solver_rigid_t& o = objects[ k ];
            o.internal_type = g.m_types[ k ];
            o.has_anisotropic_friction = g.m_has_aniso[ k ];
            o.inverse_mass = v[ 0 ];
            for ( int i = 0; i < 3; ++i )
            {
                o.linear_factor[ i ] = v[ 1 + i ];
                o.angular_factor[ i ] = v[ 4 + i ];
                for ( int j = 0; j < 3; ++j )
                {
                    o.inv_inertia_world[ 4 * i + j ] = v[ 7 + 3 * i + j ];
                    o.basis[ 4 * i + j ] = v[ 7 + 3 * i + j ];
                }
                o.linear_velocity[ i ] = v[ 16 + i ];
                o.angular_velocity[ i ] = v[ 19 + i ];
                o.delta_linear[ i ] = v[ 22 + i ];
                o.delta_angular[ i ] = v[ 25 + i ];
                o.origin[ i ] = v[ 28 + i ];
                o.anisotropic_friction[ i ] = v[ 31 + i ];
            }
        }
        static solver_manifold_t manifold;
        manifold = solver_manifold_t{};
        manifold.body0 = &objects[ 0 ];
        manifold.body1 = &objects[ 1 ];
        manifold.num_contacts = g.m_num;
        manifold.contact_processing_threshold = g.m_threshold;
        for ( int p = 0; p < g.m_num; ++p )
        {
            const float* v = g.m_points[ p ];
            solver_contact_point_t& cp = manifold.points[ p ];
            std::memcpy( cp.position_world_on_b, v, 12 );
            std::memcpy( cp.position_world_on_a, v + 3, 12 );
            std::memcpy( cp.normal_world_on_b, v + 6, 16 );
            cp.distance = v[ 10 ];
            cp.combined_friction = v[ 11 ];
            cp.combined_restitution = v[ 12 ];
            cp.applied_impulse = v[ 13 ];
            cp.lateral_impulse_1 = v[ 14 ];
            cp.lateral_impulse_2 = v[ 15 ];
            cp.contact_motion_1 = v[ 16 ];
            cp.contact_motion_2 = v[ 17 ];
            cp.contact_cfm_1 = v[ 18 ];
            cp.contact_cfm_2 = v[ 19 ];
            std::memcpy( cp.lateral_friction_dir_1, v + 20, 16 );
            std::memcpy( cp.lateral_friction_dir_2, v + 24, 16 );
            cp.life_time = g.m_lives[ p ];
            cp.lateral_friction_initialized = static_cast< unsigned char >( g.m_inits[ p ] );
        }
        solver_info_t info{};
        info.time_step = g.m_infof[ 0 ];
        info.erp = g.m_infof[ 1 ];
        info.split_impulse_penetration_threshold = g.m_infof[ 2 ];
        info.linear_slop = g.m_infof[ 3 ];
        info.warmstarting_factor = g.m_infof[ 4 ];
        info.split_impulse = g.m_infoi[ 0 ];
        info.solver_mode = g.m_infoi[ 1 ];
        info.resting_contact_restitution_threshold = g.m_infoi[ 2 ];

        solver_t s;
        convert_contact( s, manifold, info );

        auto ident = [ & ]( const void* p )
        {
            return p == &objects[ 0 ] ? 0 : p == &objects[ 1 ] ? 1 : p == &fixed_body( ) ? 2 : 3;
        };
        auto same_row = [ & ]( const solver_constraint_t& c, const golden_convert_row_t& r, bool contact )
        {
            const float tail[ 5 ] = { c.rhs, c.cfm, c.lower_limit, c.upper_limit, c.rhs_penetration };
            const int point = contact ? static_cast< int >( static_cast< const solver_contact_point_t* >( c.original_contact_point ) - manifold.points )
                                      : ( c.original_contact_point != nullptr ? 1 : 0 );
            return std::memcmp( &c, r.m_raw, sizeof( r.m_raw ) ) == 0 && c.friction_index == r.m_friction_index
                && std::memcmp( tail, r.m_tail, ( contact ? 5 : 4 ) * sizeof( float ) ) == 0
                && ident( c.solver_body_a ) == r.m_a && ident( c.solver_body_b ) == r.m_b && point == r.m_point;
        };
        const char* what = nullptr;
        if ( s.contact_count != g.m_contact_count || s.friction_count != g.m_friction_count )
            what = "row counts";
        for ( int k = 0; k < s.contact_count && !what; ++k )
            if ( !same_row( s.contacts[ k ], g.m_contacts[ k ], true ) )
                what = "a contact row";
        for ( int k = 0; k < s.friction_count && !what; ++k )
            if ( !same_row( s.frictions[ k ], g.m_frictions[ k ], false ) )
                what = "a friction row";
        for ( int p = 0; p < g.m_num && !what; ++p )
        {
            const solver_contact_point_t& cp = manifold.points[ p ];
            float out[ 9 ];
            std::memcpy( out, cp.lateral_friction_dir_1, 16 );
            std::memcpy( out + 4, cp.lateral_friction_dir_2, 16 );
            out[ 8 ] = static_cast< float >( cp.lateral_friction_initialized );
            if ( std::memcmp( out, g.m_points_out[ p ], sizeof( out ) ) != 0 )
                what = "a point";
        }
        float deltas[ 12 ];
        for ( int k = 0; k < 2; ++k )
        {
            std::memcpy( deltas + 6 * k, objects[ k ].delta_linear, 12 );
            std::memcpy( deltas + 6 * k + 3, objects[ k ].delta_angular, 12 );
        }
        if ( !what && std::memcmp( deltas, g.m_deltas, sizeof( deltas ) ) != 0 )
            what = "deltas";
        if ( s.owns_contacts )
            delete[] s.contacts;
        if ( s.owns_frictions )
            delete[] s.frictions;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "convert_contact vector %d (mode %#x, %d points): %s differ", static_cast< int >( index ),
                g.m_infoi[ 1 ], g.m_num, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "convert_contact matches convertContact @0x5727D0 on %d manifolds, bit exact",
            static_cast< int >( g_golden_convert_count ) );
    return failed == 0;
}
// solver_setup against solveGroupCacheFriendlySetup @0x572E90 (no typed
// constraints): the early out, the body zeroing, the manifold loop, the
// order arrays and the two pool counts it resets.
inline bool verify_solver_setup( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_setup_count; ++index )
    {
        const golden_setup_t& g = g_golden_setup[ index ];
        solver_rigid_t objects[ 3 ]{};
        solver_rigid_t* list[ 3 ];
        for ( int k = 0; k < 3; ++k )
        {
            const float* v = g.m_bodies[ k ];
            solver_rigid_t& o = objects[ k ];
            o.internal_type = g.m_types[ k ];
            o.has_anisotropic_friction = g.m_has_aniso[ k ];
            o.inverse_mass = v[ 0 ];
            for ( int i = 0; i < 3; ++i )
            {
                o.linear_factor[ i ] = v[ 1 + i ];
                o.angular_factor[ i ] = v[ 4 + i ];
                for ( int j = 0; j < 3; ++j )
                {
                    o.inv_inertia_world[ 4 * i + j ] = v[ 7 + 3 * i + j ];
                    o.basis[ 4 * i + j ] = v[ 7 + 3 * i + j ];
                }
                o.linear_velocity[ i ] = v[ 16 + i ];
                o.angular_velocity[ i ] = v[ 19 + i ];
                o.delta_linear[ i ] = v[ 22 + i ];
                o.delta_angular[ i ] = v[ 25 + i ];
                o.origin[ i ] = v[ 28 + i ];
                o.anisotropic_friction[ i ] = v[ 31 + i ];
                o.push_velocity[ i ] = v[ 34 + i ];
                o.turn_velocity[ i ] = v[ 37 + i ];
            }
            list[ k ] = &o;
        }
        // The w lanes the oracle wrote are not in the vector; compare x..z
        // of every lane-4 output except where the setup zeroes all four.
        static solver_manifold_t manifolds[ 2 ];
        solver_manifold_t* mlist[ 2 ] = { &manifolds[ 0 ], &manifolds[ 1 ] };
        for ( int m = 0; m < 2; ++m )
        {
            manifolds[ m ] = solver_manifold_t{};
            if ( m >= g.m_manifold_count )
                continue;
            const golden_setup_manifold_t& gm = g.m_manifolds[ m ];
            manifolds[ m ].body0 = &objects[ gm.m_pair[ 0 ] ];
            manifolds[ m ].body1 = &objects[ gm.m_pair[ 1 ] ];
            manifolds[ m ].num_contacts = gm.m_num;
            manifolds[ m ].contact_processing_threshold = gm.m_threshold;
            for ( int p = 0; p < gm.m_num; ++p )
            {
                const float* v = gm.m_points[ p ];
                solver_contact_point_t& cp = manifolds[ m ].points[ p ];
                std::memcpy( cp.position_world_on_b, v, 12 );
                std::memcpy( cp.position_world_on_a, v + 3, 12 );
                std::memcpy( cp.normal_world_on_b, v + 6, 16 );
                cp.distance = v[ 10 ];
                cp.combined_friction = v[ 11 ];
                cp.combined_restitution = v[ 12 ];
                cp.applied_impulse = v[ 13 ];
                cp.lateral_impulse_1 = v[ 14 ];
                cp.lateral_impulse_2 = v[ 15 ];
                cp.contact_motion_1 = v[ 16 ];
                cp.contact_motion_2 = v[ 17 ];
                cp.contact_cfm_1 = v[ 18 ];
                cp.contact_cfm_2 = v[ 19 ];
                std::memcpy( cp.lateral_friction_dir_1, v + 20, 16 );
                std::memcpy( cp.lateral_friction_dir_2, v + 24, 16 );
                cp.life_time = gm.m_lives[ p ];
                cp.lateral_friction_initialized = static_cast< unsigned char >( gm.m_inits[ p ] );
            }
        }
        solver_info_t info{};
        info.time_step = g.m_infof[ 0 ];
        info.erp = g.m_infof[ 1 ];
        info.split_impulse_penetration_threshold = g.m_infof[ 2 ];
        info.linear_slop = g.m_infof[ 3 ];
        info.warmstarting_factor = g.m_infof[ 4 ];
        info.split_impulse = g.m_infoi[ 0 ];
        info.solver_mode = g.m_infoi[ 1 ];
        info.resting_contact_restitution_threshold = g.m_infoi[ 2 ];

        solver_t s;
        s.non_contact_count = 3;
        s.constraint_sizes_count = 2;
        const bool ran = solver_setup( s, list, 3, mlist, g.m_manifold_count, 0, info );

        auto ident = [ & ]( const void* p )
        {
            for ( int k = 0; k < 3; ++k )
                if ( p == &objects[ k ] )
                    return k;
            return p == &fixed_body( ) ? 3 : 4;
        };
        auto same_row = [ & ]( const solver_constraint_t& c, const golden_convert_row_t& r, bool contact )
        {
            const float tail[ 5 ] = { c.rhs, c.cfm, c.lower_limit, c.upper_limit, c.rhs_penetration };
            int point = c.original_contact_point != nullptr ? 1 : 0;
            if ( contact )
            {
                const auto* cp = static_cast< const solver_contact_point_t* >( c.original_contact_point );
                for ( int m = 0; m < 2; ++m )
                    if ( cp >= manifolds[ m ].points && cp < manifolds[ m ].points + 4 )
                        point = m * 4 + static_cast< int >( cp - manifolds[ m ].points );
            }
            return std::memcmp( &c, r.m_raw, sizeof( r.m_raw ) ) == 0 && c.friction_index == r.m_friction_index
                && std::memcmp( tail, r.m_tail, ( contact ? 5 : 4 ) * sizeof( float ) ) == 0
                && ident( c.solver_body_a ) == r.m_a && ident( c.solver_body_b ) == r.m_b && point == r.m_point;
        };
        const char* what = nullptr;
        if ( !ran )
            what = "the run";
        else if ( s.contact_count != g.m_contact_count || s.friction_count != g.m_friction_count
            || s.non_contact_count != g.m_sizes[ 0 ] || s.constraint_sizes_count != g.m_sizes[ 1 ] )
            what = "counts";
        for ( int k = 0; k < s.contact_count && !what; ++k )
            if ( !same_row( s.contacts[ k ], g.m_contacts[ k ], true ) || s.order_contacts[ k ] != g.m_order_contacts[ k ] )
                what = "a contact row or its order";
        for ( int k = 0; k < s.friction_count && !what; ++k )
            if ( !same_row( s.frictions[ k ], g.m_frictions[ k ], false ) || s.order_frictions[ k ] != g.m_order_frictions[ k ] )
                what = "a friction row or its order";
        for ( int k = 0; k < 3 && !what; ++k )
        {
            const float* lanes[ 4 ] = { objects[ k ].delta_linear, objects[ k ].delta_angular,
                objects[ k ].push_velocity, objects[ k ].turn_velocity };
            for ( int q = 0; q < 4 && !what; ++q )
                if ( std::memcmp( lanes[ q ], g.m_out_bodies + 16 * k + 4 * q, 12 ) != 0 )
                    what = "a body";
        }
        if ( s.owns_contacts ) delete[] s.contacts;
        if ( s.owns_frictions ) delete[] s.frictions;
        if ( s.owns_order_contacts ) delete[] s.order_contacts;
        if ( s.owns_order_frictions ) delete[] s.order_frictions;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "solver_setup vector %d (mode %#x, %d manifolds): %s differ", static_cast< int >( index ),
                g.m_infoi[ 1 ], g.m_manifold_count, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "solver_setup matches solveGroupCacheFriendlySetup @0x572E90 on %d vectors (no typed constraints), bit exact",
            static_cast< int >( g_golden_setup_count ) );
    return failed == 0;
}
// solver_solve_group against solveGroup @0x574870 end to end - retail ran
// its own vtable's setup, iterations and finish. Bodies' velocities,
// transforms, deltas, push / turn (all four lanes), every point's impulses,
// directions and flag, the seed and the pool counts. libm as the
// writeback's.
inline bool verify_solve_group( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_group_count; ++index )
    {
        const golden_group_t& g = g_golden_group[ index ];
        solver_rigid_t objects[ 3 ]{};
        solver_rigid_t* list[ 3 ];
        for ( int k = 0; k < 3; ++k )
        {
            const float* v = g.m_bodies[ k ];
            solver_rigid_t& o = objects[ k ];
            o.internal_type = g.m_types[ k ];
            o.has_anisotropic_friction = g.m_has_aniso[ k ];
            o.inverse_mass = v[ 0 ];
            for ( int i = 0; i < 3; ++i )
            {
                o.linear_factor[ i ] = v[ 1 + i ];
                o.angular_factor[ i ] = v[ 4 + i ];
                for ( int j = 0; j < 3; ++j )
                {
                    o.inv_inertia_world[ 4 * i + j ] = v[ 7 + 3 * i + j ];
                    o.basis[ 4 * i + j ] = v[ 7 + 3 * i + j ];
                }
                o.linear_velocity[ i ] = v[ 16 + i ];
                o.angular_velocity[ i ] = v[ 19 + i ];
                o.delta_linear[ i ] = v[ 22 + i ];
                o.delta_angular[ i ] = v[ 25 + i ];
                o.origin[ i ] = v[ 28 + i ];
                o.anisotropic_friction[ i ] = v[ 31 + i ];
                o.push_velocity[ i ] = v[ 34 + i ];
                o.turn_velocity[ i ] = v[ 37 + i ];
                o.inverse_mass_vector[ i ] = v[ 40 + i ];
            }
            list[ k ] = &o;
        }
        static solver_manifold_t manifolds[ 2 ];
        solver_manifold_t* mlist[ 2 ] = { &manifolds[ 0 ], &manifolds[ 1 ] };
        for ( int m = 0; m < 2; ++m )
        {
            manifolds[ m ] = solver_manifold_t{};
            if ( m >= g.m_manifold_count )
                continue;
            const golden_setup_manifold_t& gm = g.m_manifolds[ m ];
            manifolds[ m ].body0 = &objects[ gm.m_pair[ 0 ] ];
            manifolds[ m ].body1 = &objects[ gm.m_pair[ 1 ] ];
            manifolds[ m ].num_contacts = gm.m_num;
            manifolds[ m ].contact_processing_threshold = gm.m_threshold;
            for ( int p = 0; p < gm.m_num; ++p )
            {
                const float* v = gm.m_points[ p ];
                solver_contact_point_t& cp = manifolds[ m ].points[ p ];
                std::memcpy( cp.position_world_on_b, v, 12 );
                std::memcpy( cp.position_world_on_a, v + 3, 12 );
                std::memcpy( cp.normal_world_on_b, v + 6, 16 );
                cp.distance = v[ 10 ];
                cp.combined_friction = v[ 11 ];
                cp.combined_restitution = v[ 12 ];
                cp.applied_impulse = v[ 13 ];
                cp.lateral_impulse_1 = v[ 14 ];
                cp.lateral_impulse_2 = v[ 15 ];
                cp.contact_motion_1 = v[ 16 ];
                cp.contact_motion_2 = v[ 17 ];
                cp.contact_cfm_1 = v[ 18 ];
                cp.contact_cfm_2 = v[ 19 ];
                std::memcpy( cp.lateral_friction_dir_1, v + 20, 16 );
                std::memcpy( cp.lateral_friction_dir_2, v + 24, 16 );
                cp.life_time = gm.m_lives[ p ];
                cp.lateral_friction_initialized = static_cast< unsigned char >( gm.m_inits[ p ] );
            }
        }
        solver_info_t info{};
        info.time_step = g.m_infof[ 0 ];
        info.erp = g.m_infof[ 1 ];
        info.split_impulse_penetration_threshold = g.m_infof[ 2 ];
        info.linear_slop = g.m_infof[ 3 ];
        info.warmstarting_factor = g.m_infof[ 4 ];
        info.split_impulse = g.m_infoi[ 0 ];
        info.solver_mode = g.m_infoi[ 1 ];
        info.resting_contact_restitution_threshold = g.m_infoi[ 2 ];
        info.num_iterations = g.m_infoi[ 3 ];

        solver_t s;
        const bool ran = solver_solve_group( s, list, 3, mlist, g.m_manifold_count, 0, info );

        float ours[ 120 ];
        for ( int k = 0; k < 3; ++k )
        {
            float* o = ours + 40 * k;
            std::memcpy( o, objects[ k ].linear_velocity, 16 );
            std::memcpy( o + 4, objects[ k ].angular_velocity, 16 );
            std::memcpy( o + 8, objects[ k ].basis, 48 );
            std::memcpy( o + 20, objects[ k ].origin, 16 );
            std::memcpy( o + 24, objects[ k ].delta_linear, 16 );
            std::memcpy( o + 28, objects[ k ].delta_angular, 16 );
            std::memcpy( o + 32, objects[ k ].push_velocity, 16 );
            std::memcpy( o + 36, objects[ k ].turn_velocity, 16 );
        }
        const char* what = nullptr;
        if ( !ran )
            what = "the run";
        else if ( std::memcmp( ours, g.m_out_bodies, sizeof( ours ) ) != 0 )
            what = "the bodies";
        else if ( s.seed != g.m_seed || s.contact_count != g.m_counts[ 0 ] || s.non_contact_count != g.m_counts[ 1 ]
            || s.friction_count != g.m_counts[ 2 ] )
            what = "the seed or counts";
        int slot = 0;
        for ( int m = 0; m < g.m_manifold_count && !what; ++m )
        {
            for ( int p = 0; p < g.m_manifolds[ m ].m_num && !what; ++p, ++slot )
            {
                const solver_contact_point_t& cp = manifolds[ m ].points[ p ];
                float out[ 12 ] = { cp.applied_impulse, cp.lateral_impulse_1, cp.lateral_impulse_2 };
                std::memcpy( out + 3, cp.lateral_friction_dir_1, 16 );
                std::memcpy( out + 7, cp.lateral_friction_dir_2, 16 );
                out[ 11 ] = static_cast< float >( cp.lateral_friction_initialized );
                // Without mode 0x10 retail may fill lateral 2 from past the
                // friction pool (see solver_finish) - not comparable, never read.
                if ( ( info.solver_mode & 0x10 ) == 0 )
                    out[ 2 ] = g.m_out_points[ slot ][ 2 ];
                if ( std::memcmp( out, g.m_out_points[ slot ], sizeof( out ) ) != 0 )
                    what = "a point";
            }
        }
        if ( s.owns_contacts ) delete[] s.contacts;
        if ( s.owns_frictions ) delete[] s.frictions;
        if ( s.owns_order_contacts ) delete[] s.order_contacts;
        if ( s.owns_order_frictions ) delete[] s.order_frictions;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "solver_solve_group vector %d (mode %#x, %d manifolds, %d iterations): %s differ",
                static_cast< int >( index ), g.m_infoi[ 1 ], g.m_manifold_count, g.m_infoi[ 3 ], what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "solver_solve_group matches solveGroup @0x574870 end to end on %d vectors, bit exact (libm as the oracle's)",
            static_cast< int >( g_golden_group_count ) );
    return failed == 0;
}
// calculateSimulationIslands @0x602580 then buildAndProcessIslands @0x624D20,
// against retail run with a recording processIsland.
inline bool verify_islands( )
{
    int failed = 0;
    int calls_seen = 0;
    for ( std::size_t index = 0; index < g_golden_islands_count; ++index )
    {
        const golden_islands_t& g = g_golden_islands[ index ];
        static solver_rigid_t objects[ 8 ];
        solver_rigid_t* list[ 8 ];
        for ( int k = 0; k < g.m_count; ++k )
        {
            objects[ k ] = solver_rigid_t{};
            objects[ k ].collision_flags = g.m_flags[ k ];
            objects[ k ].island_tag = 77;
            objects[ k ].companion_id = 77;
            objects[ k ].activation_state = g.m_states[ k ];
            objects[ k ].deactivation_time = g.m_deact[ k ];
            objects[ k ].hit_fraction = 0.5f;
            list[ k ] = &objects[ k ];
        }
        island_pair_t pairs[ 16 ];
        for ( int p = 0; p < g.m_pair_count; ++p )
            pairs[ p ] = { g.m_pairs[ p ][ 2 ] ? nullptr : &objects[ g.m_pairs[ p ][ 0 ] ], &objects[ g.m_pairs[ p ][ 1 ] ] };
        island_constraint_t constraints[ 3 ];
        const island_constraint_t* clist[ 3 ];
        for ( int c = 0; c < g.m_constraint_count; ++c )
        {
            constraints[ c ] = { &objects[ g.m_constraints[ c ][ 0 ] ], &objects[ g.m_constraints[ c ][ 1 ] ] };
            clist[ c ] = &constraints[ c ];
        }
        static solver_manifold_t manifolds[ 6 ];
        solver_manifold_t* mlist[ 6 ];
        for ( int m = 0; m < g.m_manifold_count; ++m )
        {
            manifolds[ m ] = solver_manifold_t{};
            manifolds[ m ].body0 = &objects[ g.m_manifolds[ m ][ 0 ] ];
            manifolds[ m ].body1 = &objects[ g.m_manifolds[ m ][ 1 ] ];
            mlist[ m ] = &manifolds[ m ];
        }

        auto states_match = [ & ]( const int ( *rows )[ 3 ], const float ( *frows )[ 2 ] )
        {
            for ( int k = 0; k < g.m_count; ++k )
            {
                const solver_rigid_t& o = objects[ k ];
                const float f[ 2 ] = { o.deactivation_time, o.hit_fraction };
                if ( o.island_tag != rows[ k ][ 0 ] || o.companion_id != rows[ k ][ 1 ] || o.activation_state != rows[ k ][ 2 ]
                    || std::memcmp( f, frows[ k ], sizeof( f ) ) != 0 )
                    return false;
            }
            return true;
        };

        island_manager_t manager;
        manager.split_islands = g.m_split != 0;
        island_calculate( manager, list, g.m_count, pairs, g.m_pair_count, clist, g.m_constraint_count );
        const char* what = nullptr;
        if ( !states_match( g.m_calc, g.m_calc_f ) )
            what = "the objects after calculate";
        else if ( static_cast< int >( manager.elements.size() ) != g.m_element_count )
            what = "the element count";
        for ( int e = 0; e < g.m_element_count && !what; ++e )
            if ( manager.elements[ e ].id != g.m_elements[ e ][ 0 ] || manager.elements[ e ].sz != g.m_elements[ e ][ 1 ] )
                what = "the union-find elements";

        int call = 0;
        island_build_and_process( manager, list, g.m_count, mlist, g.m_manifold_count,
            [ & ]( solver_rigid_t* const* bodies, int body_count, solver_manifold_t* const* island_manifolds,
                int island_manifold_count, int island_id )
            {
                if ( what )
                    return;
                if ( call >= g.m_call_count )
                {
                    what = "an extra processIsland call";
                    return;
                }
                const golden_island_call_t& gc = g.m_calls[ call++ ];
                bool same = gc.m_body_count == body_count && gc.m_manifold_count == island_manifold_count && gc.m_island == island_id;
                for ( int b = 0; same && b < body_count; ++b )
                    same = bodies[ b ] == &objects[ gc.m_bodies[ b ] ];
                for ( int k = 0; same && k < island_manifold_count; ++k )
                    same = island_manifolds[ k ] == &manifolds[ gc.m_manifolds[ k ] ];
                if ( !same )
                    what = "a processIsland call";
            } );
        calls_seen += call;
        if ( !what && call != g.m_call_count )
            what = "the processIsland call count";
        if ( !what && !states_match( g.m_build, g.m_build_f ) )
            what = "the objects after build";
        if ( !what && static_cast< int >( manager.island_manifolds.size() ) != g.m_island_manifold_count )
            what = "the island manifold count";
        for ( int k = 0; k < g.m_island_manifold_count && !what; ++k )
            if ( manager.island_manifolds[ k ] != &manifolds[ g.m_island_manifolds[ k ] ] )
                what = "the island manifolds";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "islands vector %d (%d objects, %d pairs, %d manifolds, %d constraints, split %d): %s differ",
                static_cast< int >( index ), g.m_count, g.m_pair_count, g.m_manifold_count, g.m_constraint_count, g.m_split, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "islands match calculateSimulationIslands @0x602580 + buildAndProcessIslands @0x624D20 on %d vectors (%d island calls), exact",
            static_cast< int >( g_golden_islands_count ), calls_seen );
    return failed == 0;
}

// solveConstraints @0x601B60 end to end (after calculateSimulationIslands),
// against retail run with a fake solver recording every solveGroup call.
struct verify_solve_record_t
{
    const golden_solve_constraints_t* golden;
    solver_rigid_t* objects;
    solver_manifold_t* manifolds;
    island_constraint_t* constraints;
    int call;
    const char* what;
};

inline bool verify_solve_record( void* user, solver_rigid_t* const* bodies, int body_count, solver_manifold_t* const* manifolds,
    int manifold_count, const island_constraint_t* const* constraints, int constraint_count )
{
    verify_solve_record_t& r = *static_cast< verify_solve_record_t* >( user );
    if ( r.what )
        return true;
    if ( r.call >= r.golden->m_call_count )
    {
        r.what = "an extra solveGroup call";
        return true;
    }
    const golden_solve_call_t& gc = r.golden->m_calls[ r.call++ ];
    bool same = gc.m_body_count == body_count && gc.m_manifold_count == manifold_count && gc.m_constraint_count == constraint_count
        && gc.m_null[ 0 ] == ( bodies == nullptr ) && gc.m_null[ 1 ] == ( manifolds == nullptr )
        && gc.m_null[ 2 ] == ( constraints == nullptr );
    for ( int k = 0; same && k < body_count; ++k )
        same = bodies[ k ] == &r.objects[ gc.m_bodies[ k ] ];
    for ( int k = 0; same && k < manifold_count; ++k )
        same = manifolds[ k ] == &r.manifolds[ gc.m_manifolds[ k ] ];
    for ( int k = 0; same && k < constraint_count; ++k )
        same = constraints[ k ] == &r.constraints[ gc.m_constraints[ k ] ];
    if ( !same )
        r.what = "a solveGroup call";
    return true;
}

inline bool verify_solve_constraints( )
{
    int failed = 0;
    int calls_seen = 0;
    for ( std::size_t index = 0; index < g_golden_solve_constraints_count; ++index )
    {
        const golden_solve_constraints_t& g = g_golden_solve_constraints[ index ];
        static solver_rigid_t objects[ 8 ];
        solver_rigid_t* list[ 8 ];
        for ( int k = 0; k < g.m_count; ++k )
        {
            objects[ k ] = solver_rigid_t{};
            objects[ k ].collision_flags = g.m_flags[ k ];
            objects[ k ].island_tag = 77;
            objects[ k ].companion_id = 77;
            objects[ k ].activation_state = g.m_states[ k ];
            objects[ k ].deactivation_time = g.m_deact[ k ];
            objects[ k ].hit_fraction = 0.5f;
            list[ k ] = &objects[ k ];
        }
        island_pair_t pairs[ 16 ];
        for ( int p = 0; p < g.m_pair_count; ++p )
            pairs[ p ] = { &objects[ g.m_pairs[ p ][ 0 ] ], &objects[ g.m_pairs[ p ][ 1 ] ] };
        island_constraint_t constraints[ 4 ];
        const island_constraint_t* clist[ 4 ];
        for ( int c = 0; c < g.m_constraint_count; ++c )
        {
            constraints[ c ] = { &objects[ g.m_constraints[ c ][ 0 ] ], &objects[ g.m_constraints[ c ][ 1 ] ] };
            clist[ c ] = &constraints[ c ];
        }
        static solver_manifold_t manifolds[ 6 ];
        solver_manifold_t* mlist[ 6 ];
        for ( int m = 0; m < g.m_manifold_count; ++m )
        {
            manifolds[ m ] = solver_manifold_t{};
            manifolds[ m ].body0 = &objects[ g.m_manifolds[ m ][ 0 ] ];
            manifolds[ m ].body1 = &objects[ g.m_manifolds[ m ][ 1 ] ];
            mlist[ m ] = &manifolds[ m ];
        }

        island_manager_t manager;
        manager.split_islands = g.m_split != 0;
        island_calculate( manager, list, g.m_count, pairs, g.m_pair_count, clist, g.m_constraint_count );
        solver_info_t info{};
        info.minimum_solver_batch_size = g.m_batch;
        solver_t solver;
        verify_solve_record_t record{ &g, objects, manifolds, constraints, 0, nullptr };
        island_solve_constraints( solver, info, manager, list, g.m_count, mlist, g.m_manifold_count, clist,
            g.m_constraint_count, verify_solve_record, &record );
        calls_seen += record.call;
        const char* what = record.what;
        if ( !what && record.call != g.m_call_count )
            what = "the solveGroup call count";
        for ( int k = 0; k < g.m_count && !what; ++k )
        {
            const solver_rigid_t& o = objects[ k ];
            const float f[ 2 ] = { o.deactivation_time, o.hit_fraction };
            if ( o.island_tag != g.m_after[ k ][ 0 ] || o.companion_id != g.m_after[ k ][ 1 ]
                || o.activation_state != g.m_after[ k ][ 2 ] || std::memcmp( f, g.m_after_f[ k ], sizeof( f ) ) != 0 )
                what = "the objects after";
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "solve_constraints vector %d (%d objects, %d manifolds, %d constraints, split %d, batch %d): %s differ",
                static_cast< int >( index ), g.m_count, g.m_manifold_count, g.m_constraint_count, g.m_split, g.m_batch, what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "island_solve_constraints matches solveConstraints @0x601B60 on %d vectors (%d solveGroup calls), exact",
            static_cast< int >( g_golden_solve_constraints_count ), calls_seen );
    return failed == 0;
}

// btDiscreteDynamicsWorld::updateActivationState @0x601640, floats as bits.
inline bool verify_update_activation( )
{
    int failed = 0;
    int counts[ 6 ] = {};   // resulting states 0..5, to show what was reached
    for ( std::size_t index = 0; index < g_golden_activation_count; ++index )
    {
        const golden_activation_t& g = g_golden_activation[ index ];
        static solver_rigid_t objects[ 8 ];
        solver_rigid_t* list[ 8 ];
        for ( int k = 0; k < g.m_count; ++k )
        {
            const golden_activation_body_t& gb = g.m_bodies[ k ];
            solver_rigid_t& o = objects[ k ];
            o = solver_rigid_t{};
            o.activation_state = gb.m_state;
            o.collision_flags = gb.m_flags;
            std::memcpy( &o.deactivation_time, &gb.m_in[ 0 ], 4 );
            std::memcpy( o.linear_velocity, &gb.m_in[ 1 ], 12 );
            std::memcpy( o.angular_velocity, &gb.m_in[ 4 ], 12 );
            o.linear_velocity[ 3 ] = 7.0f;
            o.angular_velocity[ 3 ] = 7.0f;
            std::memcpy( &o.linear_sleeping_threshold, &gb.m_in[ 7 ], 4 );
            std::memcpy( &o.angular_sleeping_threshold, &gb.m_in[ 8 ], 4 );
            list[ k ] = gb.m_null ? nullptr : &o;
        }
        float dt;
        std::memcpy( &dt, &g.m_dt, 4 );
        world_update_activation_state( list, g.m_count, dt, g.m_disable != 0 );
        bool same = true;
        for ( int k = 0; k < g.m_count && same; ++k )
        {
            const golden_activation_body_t& gb = g.m_bodies[ k ];
            const solver_rigid_t& o = objects[ k ];
            unsigned after[ 9 ];
            std::memcpy( &after[ 0 ], &o.deactivation_time, 4 );
            std::memcpy( &after[ 1 ], o.linear_velocity, 16 );
            std::memcpy( &after[ 5 ], o.angular_velocity, 16 );
            same = o.activation_state == gb.m_after_state && std::memcmp( after, gb.m_after, sizeof( after ) ) == 0;
            if ( gb.m_after_state >= 0 && gb.m_after_state < 6 )
                ++counts[ gb.m_after_state ];
        }
        if ( !same )
        {
            ++failed;
            LOG_ERROR( "updateActivationState vector %d (%d bodies, disable %d) differs", static_cast< int >( index ), g.m_count, g.m_disable );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "world_update_activation_state matches updateActivationState @0x601640 on %d vectors, bit exact (states after: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d)",
            static_cast< int >( g_golden_activation_count ), counts[ 0 ], counts[ 1 ], counts[ 2 ], counts[ 3 ], counts[ 4 ], counts[ 5 ] );
    return failed == 0;
}

// btDiscreteDynamicsWorld::integrateTransforms @0x602870 (no CCD branch -
// the oracle proves retail never entered it on these inputs).
inline bool verify_integrate_transforms( )
{
    int failed = 0;
    int moved = 0;
    for ( std::size_t index = 0; index < g_golden_integrate_count; ++index )
    {
        const golden_integrate_t& g = g_golden_integrate[ index ];
        static solver_rigid_t objects[ 6 ];
        solver_rigid_t* list[ 6 ];
        int shapes[ 6 ];
        for ( int k = 0; k < g.m_count; ++k )
        {
            const golden_integrate_body_t& gb = g.m_bodies[ k ];
            solver_rigid_t& o = objects[ k ];
            o = solver_rigid_t{};
            for ( int r = 0; r < 3; ++r )
            {
                for ( int c = 0; c < 3; ++c )
                    o.basis[ 4 * r + c ] = gb.m_in[ 3 * r + c ];
                o.origin[ r ] = gb.m_in[ 9 + r ];
                o.linear_velocity[ r ] = gb.m_in[ 12 + r ];
                o.angular_velocity[ r ] = gb.m_in[ 15 + r ];
                o.inv_inertia_local[ r ] = gb.m_in[ 18 + r ];
            }
            for ( int i = 0; i < 12; ++i )
            {
                o.interpolation_basis[ i ] = 7.0f;
                o.inv_inertia_world[ i ] = 7.0f;
            }
            for ( int i = 0; i < 4; ++i )
                o.interpolation_origin[ i ] = o.interpolation_linear[ i ] = o.interpolation_angular[ i ] = 7.0f;
            o.hit_fraction = 7.0f;
            o.ccd_swept_sphere_radius = 0.5f;
            o.ccd_motion_threshold = gb.m_in[ 21 ];
            o.activation_state = gb.m_state;
            o.collision_flags = gb.m_flags;
            shapes[ k ] = gb.m_shape;
            list[ k ] = &o;
        }
        const bool ran = world_integrate_transforms( list, shapes, g.m_count, g.m_dt, g.m_continuous != 0 );
        bool same = ran;
        for ( int k = 0; k < g.m_count && same; ++k )
        {
            const solver_rigid_t& o = objects[ k ];
            float out[ 53 ];
            out[ 0 ] = o.hit_fraction;
            std::memcpy( out + 1, o.basis, 48 );
            std::memcpy( out + 13, o.origin, 16 );
            std::memcpy( out + 17, o.interpolation_basis, 48 );
            std::memcpy( out + 29, o.interpolation_origin, 16 );
            std::memcpy( out + 33, o.interpolation_linear, 16 );
            std::memcpy( out + 37, o.interpolation_angular, 16 );
            std::memcpy( out + 41, o.inv_inertia_world, 48 );
            same = std::memcmp( out, g.m_bodies[ k ].m_out, sizeof( out ) ) == 0;
            if ( o.interpolation_linear[ 0 ] != 7.0f )
                ++moved;
        }
        if ( !same )
        {
            ++failed;
            LOG_ERROR( "integrateTransforms vector %d (%d bodies, continuous %d) differs%s", static_cast< int >( index ),
                g.m_count, g.m_continuous, ran ? "" : " (refused the CCD branch)" );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "world_integrate_transforms matches integrateTransforms @0x602870 on %d vectors (%d bodies integrated), bit exact (libm as the oracle's)",
            static_cast< int >( g_golden_integrate_count ), moved );
    return failed == 0;
}

// The per-step body passes against retail, body +16..+499 compared as raw
// bytes (solver_rigid_t is retail's layout there - no pointers before +500).
inline bool verify_body_step( )
{
    static const char* const names[ 5 ] = { "integrateVelocities @0x50E490", "applyDamping @0x50DCE0",
        "predictUnconstraintMotion @0x6031B0", "applyGravity @0x600540", "clearForces @0x6004C0" };
    int failed = 0;
    int per[ 5 ] = {};
    for ( std::size_t index = 0; index < g_golden_body_step_count; ++index )
    {
        const golden_body_step_t& g = g_golden_body_step[ index ];
        static solver_rigid_t objects[ 3 ];
        solver_rigid_t* list[ 3 ];
        for ( int k = 0; k < g.m_count; ++k )
        {
            objects[ k ] = solver_rigid_t{};
            std::memcpy( reinterpret_cast< unsigned char* >( &objects[ k ] ) + 16, g.m_in[ k ], 484 );
            list[ k ] = &objects[ k ];
        }
        float dt;
        std::memcpy( &dt, &g.m_dt, 4 );
        switch ( g.m_fn )
        {
            case 0: body_integrate_velocities( objects[ 0 ], dt ); break;
            case 1: body_apply_damping( objects[ 0 ], dt ); break;
            case 2: world_predict_unconstraint_motion( list, g.m_count, dt ); break;
            case 3: world_apply_gravity( list, g.m_count ); break;
            default: world_clear_forces( list, g.m_count ); break;
        }
        bool same = true;
        for ( int k = 0; k < g.m_count && same; ++k )
            same = std::memcmp( reinterpret_cast< const unsigned char* >( &objects[ k ] ) + 16, g.m_out[ k ], 484 ) == 0;
        ++per[ g.m_fn ];
        if ( !same )
        {
            ++failed;
            LOG_ERROR( "body_step vector %d (%s, %d bodies) differs", static_cast< int >( index ), names[ g.m_fn ], g.m_count );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "body passes match retail bit exact on %d vectors (integrateVelocities %d, applyDamping %d, predictUnconstraintMotion %d, applyGravity %d, clearForces %d; libm as the oracle's)",
            static_cast< int >( g_golden_body_step_count ), per[ 0 ], per[ 1 ], per[ 2 ], per[ 3 ], per[ 4 ] );
    return failed == 0;
}

// saveKinematicState @0x6002E0 -> 0x50DB40 -> calculateVelocity 0x50F7D0.
inline bool verify_save_kinematic( )
{
    int failed = 0;
    int saved = 0;
    for ( std::size_t index = 0; index < g_golden_kinematic_count; ++index )
    {
        const golden_kinematic_t& g = g_golden_kinematic[ index ];
        static solver_rigid_t objects[ 4 ];
        solver_rigid_t* list[ 4 ];
        char has[ 4 ];
        for ( int k = 0; k < g.m_count; ++k )
        {
            objects[ k ] = solver_rigid_t{};
            std::memcpy( reinterpret_cast< unsigned char* >( &objects[ k ] ) + 16, g.m_in[ k ], 484 );
            list[ k ] = &objects[ k ];
            has[ k ] = g.m_motion[ k ] != 0;
        }
        struct chosen_t { const golden_kinematic_t* g; solver_rigid_t* objects; } user{ &g, objects };
        auto motion = []( void* u, solver_rigid_t& body )
        {
            const chosen_t& c = *static_cast< const chosen_t* >( u );
            const int k = static_cast< int >( &body - c.objects );
            std::memcpy( body.basis, c.g->m_chosen[ k ], 48 );
            std::memcpy( body.origin, c.g->m_chosen[ k ] + 12, 16 );
        };
        float dt;
        std::memcpy( &dt, &g.m_dt, 4 );
        world_save_kinematic_state( list, has, g.m_count, dt, motion, &user );
        bool same = true;
        for ( int k = 0; k < g.m_count && same; ++k )
        {
            same = std::memcmp( reinterpret_cast< const unsigned char* >( &objects[ k ] ) + 16, g.m_out[ k ], 484 ) == 0;
            if ( std::memcmp( g.m_in[ k ], g.m_out[ k ], 484 ) != 0 )
                ++saved;
        }
        if ( !same )
        {
            ++failed;
            LOG_ERROR( "saveKinematicState vector %d (%d objects) differs", static_cast< int >( index ), g.m_count );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "world_save_kinematic_state matches saveKinematicState @0x6002E0 on %d vectors (%d objects changed), bit exact (libm as the oracle's)",
            static_cast< int >( g_golden_kinematic_count ), saved );
    return failed == 0;
}

// world_step_simulation / world_internal_step against retail's call
// sequences (stepSimulation @0x574C30, internalSingleStepSimulation
// @0x600E80), every stage a recorder.
struct verify_step_log_t
{
    unsigned events[ 64 ][ 3 ];
    int count;
};

static verify_step_log_t* g_verify_step_log = nullptr;

inline unsigned verify_bits( float f ) { unsigned u; std::memcpy( &u, &f, 4 ); return u; }

inline void verify_step_event( unsigned code, unsigned a = 0, unsigned b = 0 )
{
    verify_step_log_t& l = *g_verify_step_log;
    if ( l.count < 64 )
    {
        l.events[ l.count ][ 0 ] = code;
        l.events[ l.count ][ 1 ] = a;
        l.events[ l.count ][ 2 ] = b;
    }
    ++l.count;
}

inline bool verify_step( )
{
    int failed = 0;
    int events_seen = 0;
    for ( std::size_t index = 0; index < g_golden_step_count; ++index )
    {
        const golden_step_t& g = g_golden_step[ index ];
        verify_step_log_t log{};
        g_verify_step_log = &log;
        world_t w;
        w.stages.save_kinematic = []( world_t&, float dt ) { verify_step_event( 1, verify_bits( dt ) ); };
        w.stages.gravity = []( world_t& ) { verify_step_event( 2 ); };
        w.stages.clear_forces = []( world_t& ) { verify_step_event( 6 ); };
        w.stages.predict = []( world_t&, float dt ) { verify_step_event( 7, verify_bits( dt ) ); };
        w.stages.collide = []( world_t& ) { verify_step_event( 8 ); };
        w.stages.islands = []( world_t& ) { verify_step_event( 9 ); };
        w.stages.solve = []( world_t& ) { verify_step_event( 10, 1 ); return true; };
        w.stages.integrate = []( world_t&, float dt ) { verify_step_event( 11, verify_bits( dt ) ); return true; };
        w.stages.actions = []( world_t&, float dt ) { verify_step_event( 12, verify_bits( dt ) ); };
        w.stages.activation = []( world_t&, float dt ) { verify_step_event( 13, verify_bits( dt ) ); };
        if ( !g.m_internal )
            w.stages.internal_step = []( world_t&, float dt ) { verify_step_event( 5, verify_bits( dt ) ); return true; };
        if ( g.m_pre )
            w.hooks.pre_tick = []( void*, world_t&, float dt ) { verify_step_event( 14, 1, verify_bits( dt ) ); };
        if ( g.m_tick )
            w.hooks.tick = []( void*, world_t&, float dt ) { verify_step_event( 15, 1, verify_bits( dt ) ); };
        w.hooks.has_substep_callback = g.m_cb != 0;
        w.hooks.substep_begin = []( void*, float dt, bool stepping ) { verify_step_event( 3, verify_bits( dt ), stepping ? 1u : 0u ); };
        w.hooks.substep = []( void*, float dt ) { verify_step_event( 4, verify_bits( dt ) ); };
        w.hooks.has_debug_drawer = g.m_drawer != 0;
        w.hooks.debug_mode = g.m_mode;
        w.disable_deactivation = true;      // retail's byte starts at the sentinel 0x55
        float dt, fixed, local0;
        std::memcpy( &dt, &g.m_dt, 4 );
        std::memcpy( &fixed, &g.m_fixed, 4 );
        std::memcpy( &local0, &g.m_local0, 4 );
        w.local_time = local0;

        const char* what = nullptr;
        if ( g.m_internal )
        {
            world_internal_step( w, dt );
            if ( verify_bits( w.dispatch_time_step ) != g.m_stores[ 0 ] || static_cast< unsigned >( w.dispatch_step_count ) != g.m_stores[ 1 ]
                || verify_bits( w.info.time_step ) != g.m_stores[ 6 ] )
                what = "the dispatch / solver info stores";
        }
        else
        {
            const int n = world_step_simulation( w, dt, g.m_max_sub, fixed );
            if ( n != g.m_result )
                what = "the substep count";
            else if ( verify_bits( w.local_time ) != g.m_local )
                what = "the local time";
            else if ( g.m_drawer ? ( g.m_flag != ( w.disable_deactivation ? 1 : 0 ) ) : ( g.m_flag != 0x55 || !w.disable_deactivation ) )
                what = "gDisableDeactivation";
        }
        if ( !what && log.count != g.m_event_count )
            what = "the event count";
        for ( int e = 0; e < log.count && !what; ++e )
            if ( std::memcmp( log.events[ e ], g.m_events[ e ], 12 ) != 0 )
                what = "an event";
        events_seen += log.count;
        g_verify_step_log = nullptr;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "step vector %d (%s): %s differs", static_cast< int >( index ), g.m_internal ? "internal" : "stepSimulation", what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "world_step_simulation / world_internal_step match retail's call sequences on %d vectors (%d events)",
            static_cast< int >( g_golden_step_count ), events_seen );
    return failed == 0;
}
