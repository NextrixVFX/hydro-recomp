#pragma once

#include <cstddef>

// Bullet 2.79's constraint-row resolvers, recreated from retail.
//
// The 2.79 solver is not the 2.82 one: it solves into velocity deltas kept
// INSIDE btRigidBody (+576 / +592) rather than into a separate btSolverBody,
// so these functions take rigid bodies. See engine/impl/bullet/VENDOR.md and
// memory/audit.md "Batch 6" for the evidence.
//
// Only the members a resolver reads are modelled. The offsets are retail's
// and are pinned by static_assert, so a layout drift cannot pass silently.

// The btRigidBody members the resolvers touch, at their retail offsets.
struct solver_rigid_t
{
    unsigned char m_head[ 16 ];
    float basis[ 12 ];                      // +16  m_worldTransform basis, rows of 4
    float origin[ 4 ];                      // +64  m_worldTransform origin
    float interpolation_basis[ 12 ];        // +80  m_interpolationWorldTransform
    float interpolation_origin[ 4 ];        // +128
    float interpolation_linear[ 4 ];        // +144 m_interpolationLinearVelocity
    float interpolation_angular[ 4 ];       // +160 m_interpolationAngularVelocity
    float anisotropic_friction[ 4 ];        // +176 m_anisotropicFriction
    int has_anisotropic_friction;           // +192 m_hasAnisotropicFriction
    unsigned char m_gap_friction[ 216 - 196 ];
    int collision_flags;                    // +216 m_collisionFlags (1 static, 2 kinematic, 4 no contact response)
    int island_tag;                         // +220 m_islandTag1
    int companion_id;                       // +224 m_companionId
    int activation_state;                   // +228 m_activationState1 (1 active, 2 island sleeping, 3 wants deactivation, 4 disable deactivation, 5 disable simulation)
    float deactivation_time;                // +232 m_deactivationTime
    float friction;                         // +236 m_friction
    float restitution;                      // +240 m_restitution
    int internal_type;                      // +244 m_internalType (2 = CO_RIGID_BODY)
    unsigned char m_gap_type[ 252 - 248 ];
    float hit_fraction;                     // +252 m_hitFraction
    float ccd_swept_sphere_radius;          // +256 m_ccdSweptSphereRadius
    float ccd_motion_threshold;             // +260 m_ccdMotionThreshold
    unsigned char m_gap_hit[ 272 - 264 ];
    float inv_inertia_world[ 12 ];          // +272 m_invInertiaTensorWorld, rows of 4
    float linear_velocity[ 4 ];             // +320 m_linearVelocity
    float angular_velocity[ 4 ];            // +336 m_angularVelocity
    float inverse_mass;                     // +352
    unsigned char m_gap_mass[ 368 - 356 ];
    float linear_factor[ 4 ];               // +368 m_linearFactor
    float gravity[ 4 ];                     // +384 m_gravity (acceleration * mass)
    float gravity_acceleration[ 4 ];        // +400 m_gravity_acceleration
    float inv_inertia_local[ 4 ];           // +416 m_invInertiaLocal
    float total_force[ 4 ];                 // +432 m_totalForce
    float total_torque[ 4 ];                // +448 m_totalTorque
    float linear_damping;                   // +464 m_linearDamping
    float angular_damping;                  // +468 m_angularDamping
    unsigned char additional_damping;       // +472 m_additionalDamping (bool)
    unsigned char m_gap_additional[ 3 ];
    float additional_damping_factor;        // +476 m_additionalDampingFactor
    float additional_linear_threshold_sqr;  // +480 m_additionalLinearDampingThresholdSqr
    float additional_angular_threshold_sqr; // +484 m_additionalAngularDampingThresholdSqr
    float additional_angular_damping_factor;// +488 (not read by applyDamping)
    float linear_sleeping_threshold;        // +492 m_linearSleepingThreshold
    float angular_sleeping_threshold;       // +496 m_angularSleepingThreshold
    // +500 motion state, +504 constraint refs, +524 flags, +528 debug id
    // (setupRigidBody @0x50D1D0). +532..+575 is read by no Bullet function
    // on a rigid body (whole-image displacement scan, control +576 found
    // every resolver). 2.82's ALIGNED16 would put the delta at +544; retail
    // has +576 = 9 x 64 - consistent with a 64-byte alignment, NOT proven.
    unsigned char m_gap_sleep[ 524 - 500 ];
    int rigidbody_flags;                    // +524 m_rigidbodyFlags (1 = BT_DISABLE_WORLD_GRAVITY)
    unsigned char m_gap_flags[ 576 - 528 ];
    alignas( 16 ) float delta_linear[ 4 ];  // +576 m_deltaLinearVelocity
    float delta_angular[ 4 ];               // +592 m_deltaAngularVelocity
    float angular_factor[ 4 ];              // +608 m_angularFactor
    float inverse_mass_vector[ 4 ];         // +624 m_invMass (linearFactor * inverseMass)
    float push_velocity[ 4 ];               // +640 m_pushVelocity
    float turn_velocity[ 4 ];               // +656 m_turnVelocity
    // +672 / +676: m_contactSolverType / m_frictionSolverType - zeroed by
    // setupRigidBody @0x50D1D0 in 2.82's order, read by nothing. Not modelled.
};
static_assert( offsetof( solver_rigid_t, basis ) == 16, "btRigidBody +16" );
static_assert( offsetof( solver_rigid_t, origin ) == 64, "btRigidBody +64" );
static_assert( offsetof( solver_rigid_t, interpolation_basis ) == 80, "btCollisionObject +80" );
static_assert( offsetof( solver_rigid_t, interpolation_linear ) == 144, "btCollisionObject +144" );
static_assert( offsetof( solver_rigid_t, interpolation_angular ) == 160, "btCollisionObject +160" );
static_assert( offsetof( solver_rigid_t, anisotropic_friction ) == 176, "btCollisionObject +176" );
static_assert( offsetof( solver_rigid_t, gravity ) == 384, "btRigidBody +384" );
static_assert( offsetof( solver_rigid_t, inv_inertia_local ) == 416, "btRigidBody +416" );
static_assert( offsetof( solver_rigid_t, total_force ) == 432, "btRigidBody +432" );
static_assert( offsetof( solver_rigid_t, total_torque ) == 448, "btRigidBody +448" );
static_assert( offsetof( solver_rigid_t, linear_damping ) == 464, "btRigidBody +464" );
static_assert( offsetof( solver_rigid_t, additional_damping ) == 472, "btRigidBody +472" );
static_assert( offsetof( solver_rigid_t, additional_angular_threshold_sqr ) == 484, "btRigidBody +484" );
static_assert( offsetof( solver_rigid_t, has_anisotropic_friction ) == 192, "btCollisionObject +192" );
static_assert( offsetof( solver_rigid_t, collision_flags ) == 216, "btCollisionObject +216" );
static_assert( offsetof( solver_rigid_t, island_tag ) == 220, "btCollisionObject +220" );
static_assert( offsetof( solver_rigid_t, companion_id ) == 224, "btCollisionObject +224" );
static_assert( offsetof( solver_rigid_t, activation_state ) == 228, "btCollisionObject +228" );
static_assert( offsetof( solver_rigid_t, deactivation_time ) == 232, "btCollisionObject +232" );
static_assert( offsetof( solver_rigid_t, internal_type ) == 244, "btCollisionObject +244" );
static_assert( offsetof( solver_rigid_t, hit_fraction ) == 252, "btCollisionObject +252" );
static_assert( offsetof( solver_rigid_t, ccd_motion_threshold ) == 260, "btCollisionObject +260" );
static_assert( offsetof( solver_rigid_t, inv_inertia_world ) == 272, "btRigidBody +272" );
static_assert( offsetof( solver_rigid_t, linear_velocity ) == 320, "btRigidBody +320" );
static_assert( offsetof( solver_rigid_t, angular_velocity ) == 336, "btRigidBody +336" );
static_assert( offsetof( solver_rigid_t, inverse_mass ) == 352, "btRigidBody +352" );
static_assert( offsetof( solver_rigid_t, linear_factor ) == 368, "btRigidBody +368" );
static_assert( offsetof( solver_rigid_t, linear_sleeping_threshold ) == 492, "btRigidBody +492" );
static_assert( offsetof( solver_rigid_t, angular_sleeping_threshold ) == 496, "btRigidBody +496" );
static_assert( offsetof( solver_rigid_t, rigidbody_flags ) == 524, "btRigidBody +524" );
static_assert( offsetof( solver_rigid_t, delta_linear ) == 576, "btRigidBody +576" );
static_assert( offsetof( solver_rigid_t, delta_angular ) == 592, "btRigidBody +592" );
static_assert( offsetof( solver_rigid_t, angular_factor ) == 608, "btRigidBody +608" );
static_assert( offsetof( solver_rigid_t, inverse_mass_vector ) == 624, "btRigidBody +624" );
static_assert( offsetof( solver_rigid_t, push_velocity ) == 640, "btRigidBody +640" );
static_assert( offsetof( solver_rigid_t, turn_velocity ) == 656, "btRigidBody +656" );

// dword_7590A8, incremented once per resolveSplitPenetrationImpulse call -
// 2.82's gNumSplitImpulseRecoveries sits in the same place in the same
// function. Nothing in the resolvers reads it.
inline int g_split_impulse_recoveries = 0;

// btSolverConstraint as 2.79 lays it out: 160 bytes of members, 64-byte
// aligned to the 192-byte stride the solver walks.
struct alignas( 64 ) solver_constraint_t
{
    float relpos1_cross_normal[ 4 ];        // +0
    float contact_normal[ 4 ];              // +16
    float relpos2_cross_normal[ 4 ];        // +32
    float angular_component_a[ 4 ];         // +48
    float angular_component_b[ 4 ];         // +64
    float applied_push_impulse[ 4 ];        // +80  btSimdScalar
    float applied_impulse[ 4 ];             // +96  btSimdScalar
    float friction;                         // +112
    float jac_diag_ab_inv;                  // +116
    int rows_per_kernel;                    // +120
    int friction_index;                     // +124
    void* solver_body_a;                    // +128
    void* solver_body_b;                    // +132
    void* original_contact_point;           // +136
    float rhs;                              // +140
    float cfm;                              // +144
    float lower_limit;                      // +148
    float upper_limit;                      // +152
    float rhs_penetration;                  // +156
};
static_assert( sizeof( void* ) != 4 || sizeof( solver_constraint_t ) == 192, "btSolverConstraint stride 192" );
static_assert( offsetof( solver_constraint_t, applied_impulse ) == 96, "btSolverConstraint +96" );
static_assert( offsetof( solver_constraint_t, jac_diag_ab_inv ) == 116, "btSolverConstraint +116" );
static_assert( sizeof( void* ) != 4 || offsetof( solver_constraint_t, rhs ) == 140, "btSolverConstraint +140" );

// resolveSingleConstraintRowGeneric @0x56FF70. The order of every float
// operation follows the retail code - addition is not associative, and the
// oracle compares bits.
inline void solver_row_generic( solver_rigid_t& a, solver_rigid_t& b, solver_constraint_t& c )
{
    const float n0 = c.contact_normal[ 0 ];
    const float n1 = c.contact_normal[ 1 ];
    const float applied = c.applied_impulse[ 0 ];

    const float vel_a = ( ( c.relpos1_cross_normal[ 1 ] * a.delta_angular[ 1 ] + c.relpos1_cross_normal[ 0 ] * a.delta_angular[ 0 ] )
                            + c.relpos1_cross_normal[ 2 ] * a.delta_angular[ 2 ] )
        + ( ( n0 * a.delta_linear[ 0 ] + a.delta_linear[ 1 ] * n1 ) + a.delta_linear[ 2 ] * c.contact_normal[ 2 ] );
    const float vel_b = ( ( c.relpos2_cross_normal[ 1 ] * b.delta_angular[ 1 ] + c.relpos2_cross_normal[ 0 ] * b.delta_angular[ 0 ] )
                            + c.relpos2_cross_normal[ 2 ] * b.delta_angular[ 2 ] )
        - ( ( n1 * b.delta_linear[ 1 ] + b.delta_linear[ 0 ] * n0 ) + b.delta_linear[ 2 ] * c.contact_normal[ 2 ] );

    float delta = ( ( c.rhs - c.cfm * applied ) - vel_a * c.jac_diag_ab_inv ) - vel_b * c.jac_diag_ab_inv;

    const float sum = applied + delta;
    float stored = sum;
    if ( c.lower_limit <= sum )
    {
        if ( !( sum <= c.upper_limit ) )
        {
            delta = c.upper_limit - applied;
            stored = c.upper_limit;
        }
    }
    else
    {
        delta = c.lower_limit - applied;
        stored = c.lower_limit;
    }
    for ( int lane = 0; lane < 4; ++lane )
        c.applied_impulse[ lane ] = stored;

    const float az = a.inverse_mass_vector[ 2 ] * c.contact_normal[ 2 ];
    if ( a.inverse_mass != 0.0f )
    {
        const float x = ( n0 * a.inverse_mass_vector[ 0 ] ) * delta + a.delta_linear[ 0 ];
        a.delta_linear[ 1 ] = a.delta_linear[ 1 ] + ( a.inverse_mass_vector[ 1 ] * c.contact_normal[ 1 ] ) * delta;
        const float z = a.delta_linear[ 2 ] + az * delta;
        a.delta_linear[ 0 ] = x;
        a.delta_linear[ 2 ] = z;
        const float ay = c.angular_component_a[ 1 ] * ( a.angular_factor[ 1 ] * delta );
        const float aw = c.angular_component_a[ 2 ] * ( a.angular_factor[ 2 ] * delta ) + a.delta_angular[ 2 ];
        a.delta_angular[ 0 ] = a.delta_angular[ 0 ] + ( delta * a.angular_factor[ 0 ] ) * c.angular_component_a[ 0 ];
        a.delta_angular[ 1 ] = a.delta_angular[ 1 ] + ay;
        a.delta_angular[ 2 ] = aw;
    }

    const float bz = b.inverse_mass_vector[ 2 ] * -c.contact_normal[ 2 ];
    if ( b.inverse_mass != 0.0f )
    {
        const float y = ( b.inverse_mass_vector[ 1 ] * -c.contact_normal[ 1 ] ) * delta + b.delta_linear[ 1 ];
        b.delta_linear[ 0 ] = b.delta_linear[ 0 ] + ( -c.contact_normal[ 0 ] * b.inverse_mass_vector[ 0 ] ) * delta;
        const float z = b.delta_linear[ 2 ] + bz * delta;
        b.delta_linear[ 1 ] = y;
        b.delta_linear[ 2 ] = z;
        const float old_z = b.delta_angular[ 2 ];
        const float bz_ang = c.angular_component_b[ 2 ] * ( b.angular_factor[ 2 ] * delta );
        const float by_ang = c.angular_component_b[ 1 ] * ( b.angular_factor[ 1 ] * delta ) + b.delta_angular[ 1 ];
        b.delta_angular[ 0 ] = ( delta * b.angular_factor[ 0 ] ) * c.angular_component_b[ 0 ] + b.delta_angular[ 0 ];
        b.delta_angular[ 1 ] = by_ang;
        b.delta_angular[ 2 ] = old_z + bz_ang;
    }
}

// resolveSingleConstraintRowLowerLimit @0x570470: the generic row with the
// upper clamp removed. Its own operand order, not the generic one's.
inline void solver_row_lower( solver_rigid_t& a, solver_rigid_t& b, solver_constraint_t& c )
{
    const float n0 = c.contact_normal[ 0 ];
    const float n1 = c.contact_normal[ 1 ];
    const float n2 = c.contact_normal[ 2 ];

    const float vel_a = ( ( c.relpos1_cross_normal[ 1 ] * a.delta_angular[ 1 ] + a.delta_angular[ 0 ] * c.relpos1_cross_normal[ 0 ] )
                            + c.relpos1_cross_normal[ 2 ] * a.delta_angular[ 2 ] )
        + ( ( n1 * a.delta_linear[ 1 ] + n0 * a.delta_linear[ 0 ] ) + n2 * a.delta_linear[ 2 ] );
    const float vel_b = ( ( c.relpos2_cross_normal[ 1 ] * b.delta_angular[ 1 ] + c.relpos2_cross_normal[ 0 ] * b.delta_angular[ 0 ] )
                            + c.relpos2_cross_normal[ 2 ] * b.delta_angular[ 2 ] )
        - ( ( b.delta_linear[ 0 ] * n0 + b.delta_linear[ 1 ] * n1 ) + n2 * b.delta_linear[ 2 ] );

    float delta = ( ( c.rhs - c.cfm * c.applied_impulse[ 0 ] ) - vel_a * c.jac_diag_ab_inv ) - vel_b * c.jac_diag_ab_inv;

    const float sum = c.applied_impulse[ 0 ] + delta;
    float stored = sum;
    if ( !( c.lower_limit <= sum ) )
    {
        delta = c.lower_limit - c.applied_impulse[ 0 ];
        stored = c.lower_limit;
    }
    for ( int lane = 0; lane < 4; ++lane )
        c.applied_impulse[ lane ] = stored;

    if ( a.inverse_mass != 0.0f )
    {
        a.delta_linear[ 0 ] = ( a.inverse_mass_vector[ 0 ] * n0 ) * delta + a.delta_linear[ 0 ];
        a.delta_linear[ 1 ] = ( a.inverse_mass_vector[ 1 ] * n1 ) * delta + a.delta_linear[ 1 ];
        a.delta_linear[ 2 ] = ( a.inverse_mass_vector[ 2 ] * n2 ) * delta + a.delta_linear[ 2 ];
        a.delta_angular[ 0 ] = a.delta_angular[ 0 ] + c.angular_component_a[ 0 ] * ( a.angular_factor[ 0 ] * delta );
        a.delta_angular[ 1 ] = a.delta_angular[ 1 ] + c.angular_component_a[ 1 ] * ( a.angular_factor[ 1 ] * delta );
        a.delta_angular[ 2 ] = a.delta_angular[ 2 ] + c.angular_component_a[ 2 ] * ( a.angular_factor[ 2 ] * delta );
    }
    if ( b.inverse_mass != 0.0f )
    {
        b.delta_linear[ 0 ] = b.delta_linear[ 0 ] + ( b.inverse_mass_vector[ 0 ] * -n0 ) * delta;
        b.delta_linear[ 1 ] = b.delta_linear[ 1 ] + ( b.inverse_mass_vector[ 1 ] * -n1 ) * delta;
        b.delta_linear[ 2 ] = ( b.inverse_mass_vector[ 2 ] * -n2 ) * delta + b.delta_linear[ 2 ];
        b.delta_angular[ 0 ] = c.angular_component_b[ 0 ] * ( b.angular_factor[ 0 ] * delta ) + b.delta_angular[ 0 ];
        b.delta_angular[ 1 ] = b.delta_angular[ 1 ] + c.angular_component_b[ 1 ] * ( b.angular_factor[ 1 ] * delta );
        b.delta_angular[ 2 ] = c.angular_component_b[ 2 ] * ( b.angular_factor[ 2 ] * delta ) + b.delta_angular[ 2 ];
    }
}

// resolveSplitPenetrationImpulse @0x5707B0 (the scalar one; 0x570AE0 is the
// SIMD one). Lower clamp only, on the push / turn velocities. The caller
// (0x57438D) skips rows whose rhsPenetration is zero; that test is the
// caller's and is not here.
inline void solver_row_split( solver_rigid_t& a, solver_rigid_t& b, solver_constraint_t& c )
{
    const float n0 = c.contact_normal[ 0 ];
    const float n1 = c.contact_normal[ 1 ];
    const float n2 = c.contact_normal[ 2 ];

    const float rhs = c.rhs_penetration - c.cfm * c.applied_push_impulse[ 0 ];
    const float turn_a = ( c.relpos1_cross_normal[ 1 ] * a.turn_velocity[ 1 ] + a.turn_velocity[ 0 ] * c.relpos1_cross_normal[ 0 ] )
        + c.relpos1_cross_normal[ 2 ] * a.turn_velocity[ 2 ];
    const float push_a = ( n0 * a.push_velocity[ 0 ] + a.push_velocity[ 1 ] * n1 ) + a.push_velocity[ 2 ] * n2;
    const float push_b = n1 * b.push_velocity[ 1 ] + n0 * b.push_velocity[ 0 ];
    ++g_split_impulse_recoveries;
    const float partial = rhs - ( turn_a + push_a ) * c.jac_diag_ab_inv;
    float delta = partial
        - ( ( ( c.relpos2_cross_normal[ 1 ] * b.turn_velocity[ 1 ] + c.relpos2_cross_normal[ 0 ] * b.turn_velocity[ 0 ] )
                + c.relpos2_cross_normal[ 2 ] * b.turn_velocity[ 2 ] )
              - ( push_b + b.push_velocity[ 2 ] * n2 ) )
            * c.jac_diag_ab_inv;

    const float sum = c.applied_push_impulse[ 0 ] + delta;
    float stored = sum;
    if ( !( c.lower_limit <= sum ) )
    {
        delta = c.lower_limit - c.applied_push_impulse[ 0 ];
        stored = c.lower_limit;
    }
    for ( int lane = 0; lane < 4; ++lane )
        c.applied_push_impulse[ lane ] = stored;

    if ( a.inverse_mass != 0.0f )
    {
        a.push_velocity[ 0 ] = ( n0 * a.inverse_mass_vector[ 0 ] ) * delta + a.push_velocity[ 0 ];
        a.push_velocity[ 1 ] = a.push_velocity[ 1 ] + ( a.inverse_mass_vector[ 1 ] * n1 ) * delta;
        a.push_velocity[ 2 ] = a.push_velocity[ 2 ] + ( a.inverse_mass_vector[ 2 ] * n2 ) * delta;
        a.turn_velocity[ 0 ] = a.turn_velocity[ 0 ] + ( a.angular_factor[ 0 ] * delta ) * c.angular_component_a[ 0 ];
        a.turn_velocity[ 1 ] = c.angular_component_a[ 1 ] * ( a.angular_factor[ 1 ] * delta ) + a.turn_velocity[ 1 ];
        a.turn_velocity[ 2 ] = c.angular_component_a[ 2 ] * ( a.angular_factor[ 2 ] * delta ) + a.turn_velocity[ 2 ];
    }
    if ( b.inverse_mass != 0.0f )
    {
        b.push_velocity[ 0 ] = ( -n0 * b.inverse_mass_vector[ 0 ] ) * delta + b.push_velocity[ 0 ];
        b.push_velocity[ 1 ] = ( b.inverse_mass_vector[ 1 ] * -n1 ) * delta + b.push_velocity[ 1 ];
        b.push_velocity[ 2 ] = b.push_velocity[ 2 ] + ( b.inverse_mass_vector[ 2 ] * -n2 ) * delta;
        b.turn_velocity[ 0 ] = ( b.angular_factor[ 0 ] * delta ) * c.angular_component_b[ 0 ] + b.turn_velocity[ 0 ];
        b.turn_velocity[ 1 ] = c.angular_component_b[ 1 ] * ( b.angular_factor[ 1 ] * delta ) + b.turn_velocity[ 1 ];
        b.turn_velocity[ 2 ] = b.turn_velocity[ 2 ] + c.angular_component_b[ 2 ] * ( b.angular_factor[ 2 ] * delta );
    }
}

// resolveSplitPenetrationSIMD @0x570AE0, transliterated from the
// DISASSEMBLY (A in ECX, (b, row) on the stack, retn 8). Every product is
// four-lane; the dot products are horizontal sums taken (z + y) + x and
// splatted, so one scalar per sum serves every lane.
//
// Three things differ from the scalar resolver and are retail's, not ours:
//  - the clamped sum is stored to appliedImpulse (+96, `movq [edx+60h]`),
//    NOT to appliedPushImpulse (+80), which it reads;
//  - there is no inverse-mass test on either body;
//  - the angular factor is not applied.
inline void solver_row_split_simd( solver_rigid_t& a, solver_rigid_t& b, solver_constraint_t& c )
{
    auto hsum = [ ]( const float* x, const float* y )
    {
        return ( x[ 2 ] * y[ 2 ] + x[ 1 ] * y[ 1 ] ) + x[ 0 ] * y[ 0 ];
    };
    const float push_a = hsum( a.push_velocity, c.contact_normal );
    const float turn_a = hsum( c.relpos1_cross_normal, a.turn_velocity );
    const float lower = c.lower_limit;
    const float applied = c.applied_push_impulse[ 0 ];
    const float jac = c.jac_diag_ab_inv;
    const float push_b = hsum( c.contact_normal, b.push_velocity );
    const float turn_b = hsum( c.relpos2_cross_normal, b.turn_velocity );

    float delta = ( c.rhs_penetration - c.cfm * applied ) - ( push_a + turn_a ) * jac;
    ++g_split_impulse_recoveries;
    delta = delta - ( turn_b - push_b ) * jac;

    const float sum = delta + applied;
    const bool below = sum < lower;
    const float stored = below ? lower : sum;
    for ( int lane = 0; lane < 4; ++lane )
        c.applied_impulse[ lane ] = stored;
    const float impulse = below ? lower - applied : delta;

    for ( int lane = 0; lane < 4; ++lane )
    {
        a.push_velocity[ lane ] = ( a.inverse_mass_vector[ lane ] * c.contact_normal[ lane ] ) * impulse + a.push_velocity[ lane ];
        a.turn_velocity[ lane ] = c.angular_component_a[ lane ] * impulse + a.turn_velocity[ lane ];
        b.push_velocity[ lane ] = b.push_velocity[ lane ] - impulse * ( b.inverse_mass_vector[ lane ] * c.contact_normal[ lane ] );
        b.turn_velocity[ lane ] = c.angular_component_b[ lane ] * impulse + b.turn_velocity[ lane ];
    }
}

// The two SIMD row resolvers solveSingleIteration calls when solverMode has
// bit 0x100: resolveSingleConstraintRowGenericSIMD @0x56FD80 and
// resolveSingleConstraintRowLowerLimitSIMD @0x5702C0. Both DISASSEMBLED:
// stdcall (a, b, row), retn 0Ch, on the deltas (+576 / +592). Like the SIMD
// split resolver they test no inverse mass and apply no angular factor.
inline float solver_row_simd_delta( const solver_rigid_t& a, const solver_rigid_t& b, const solver_constraint_t& c )
{
    auto hsum = [ ]( const float* x, const float* y )
    {
        return ( x[ 2 ] * y[ 2 ] + x[ 1 ] * y[ 1 ] ) + x[ 0 ] * y[ 0 ];
    };
    const float linear_a = hsum( c.contact_normal, a.delta_linear );
    const float angular_a = hsum( c.relpos1_cross_normal, a.delta_angular );
    const float angular_b = hsum( c.relpos2_cross_normal, b.delta_angular );
    const float linear_b = hsum( c.contact_normal, b.delta_linear );
    const float jac = c.jac_diag_ab_inv;
    const float delta = ( c.rhs - c.cfm * c.applied_impulse[ 0 ] ) - ( linear_a + angular_a ) * jac;
    return delta - ( angular_b - linear_b ) * jac;
}

inline void solver_row_simd_apply( solver_rigid_t& a, solver_rigid_t& b, const solver_constraint_t& c, float impulse )
{
    for ( int lane = 0; lane < 4; ++lane )
    {
        a.delta_linear[ lane ] = ( a.inverse_mass_vector[ lane ] * c.contact_normal[ lane ] ) * impulse + a.delta_linear[ lane ];
        a.delta_angular[ lane ] = c.angular_component_a[ lane ] * impulse + a.delta_angular[ lane ];
        b.delta_linear[ lane ] = b.delta_linear[ lane ] - impulse * ( b.inverse_mass_vector[ lane ] * c.contact_normal[ lane ] );
        b.delta_angular[ lane ] = c.angular_component_b[ lane ] * impulse + b.delta_angular[ lane ];
    }
}

// Two masks off the same sum. The store is done twice, as retail does: first
// sum < lower ? lower : sum, then that is kept only while sum < upper.
inline void solver_row_generic_simd( solver_rigid_t& a, solver_rigid_t& b, solver_constraint_t& c )
{
    const float applied = c.applied_impulse[ 0 ];
    const float lower = c.lower_limit;
    const float upper = c.upper_limit;
    const float delta = solver_row_simd_delta( a, b, c );
    const float sum = delta + applied;
    const bool below = sum < lower;
    const bool under = sum < upper;
    const float first = below ? lower : sum;
    const float stored = under ? first : upper;
    for ( int lane = 0; lane < 4; ++lane )
        c.applied_impulse[ lane ] = stored;
    const float clamped = below ? lower - applied : delta;
    const float impulse = under ? clamped : upper - applied;
    solver_row_simd_apply( a, b, c, impulse );
}

inline void solver_row_lower_simd( solver_rigid_t& a, solver_rigid_t& b, solver_constraint_t& c )
{
    const float applied = c.applied_impulse[ 0 ];
    const float lower = c.lower_limit;
    const float delta = solver_row_simd_delta( a, b, c );
    const float sum = delta + applied;
    const bool below = sum < lower;
    const float stored = below ? lower : sum;
    for ( int lane = 0; lane < 4; ++lane )
        c.applied_impulse[ lane ] = stored;
    solver_row_simd_apply( a, b, c, below ? lower - applied : delta );
}
