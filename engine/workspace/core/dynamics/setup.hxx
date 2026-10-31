#pragma once

#include <workspace/core/dynamics/finish.hxx>
#include <workspace/core/dynamics/iterate.hxx>
#include <workspace/core/dynamics/row.hxx>

#include <cstring>

// Bullet 2.79's solver setup (solveGroupCacheFriendlySetup @0x572E90 and its
// tree), recreated from retail leaves first. See memory/audit.md "Batch 6".

// applyAnisotropicFriction @0x570CA0 (fastcall: body in ECX, direction in
// EDX). Into the body's frame, scaled per axis, back out; w lane zeroed. A
// null body or one without anisotropic friction leaves the direction alone.
inline void apply_anisotropic_friction( const solver_rigid_t* body, float* d )
{
    if ( body == nullptr || body->has_anisotropic_friction == 0 )
        return;
    const float* m = body->basis;
    const float* a = body->anisotropic_friction;
    const float lx = a[ 0 ] * ( ( m[ 0 ] * d[ 0 ] + m[ 4 ] * d[ 1 ] ) + m[ 8 ] * d[ 2 ] );
    const float ly = a[ 1 ] * ( ( m[ 5 ] * d[ 1 ] + m[ 1 ] * d[ 0 ] ) + m[ 9 ] * d[ 2 ] );
    const float lz = a[ 2 ] * ( ( m[ 6 ] * d[ 1 ] + m[ 2 ] * d[ 0 ] ) + m[ 10 ] * d[ 2 ] );
    const float x = ( m[ 1 ] * ly + m[ 0 ] * lx ) + m[ 2 ] * lz;
    const float y = ( m[ 5 ] * ly + m[ 4 ] * lx ) + m[ 6 ] * lz;
    const float z = ( m[ 9 ] * ly + m[ 8 ] * lx ) + m[ 10 ] * lz;
    d[ 0 ] = x;
    d[ 1 ] = y;
    d[ 2 ] = z;
    d[ 3 ] = 0.0f;
}

// setFrictionConstraintImpulse @0x572230 (thiscall solver; contact row,
// body A, body B, manifold point, info; retn 14h), from the disassembly.
// With solverMode bit 8 the row's first friction row (and with bit 0x10 the
// second) is warm-started from the point's lateral impulse when bit 4 is
// set, else zeroed; without bit 8 both are zeroed. The impulse is applied
// straight to the deltas of each non-null dynamic body.
//
// Retail's arithmetic, kept as it is: the FIRST friction row scales the
// linear part by linearFactor (+368), the SECOND does not; and body B's
// linear part uses the contact normal UNnegated with the negated impulse,
// while its angular part negates both angularComponentB and the impulse.
inline void set_friction_constraint_impulse( solver_t& s, const solver_constraint_t& contact,
    solver_rigid_t* a, solver_rigid_t* b, const solver_contact_point_t& point, const solver_info_t& info )
{
    auto zero = [ ]( solver_constraint_t& f )
    {
        for ( int lane = 0; lane < 4; ++lane )
            f.applied_impulse[ lane ] = 0.0f;
    };
    auto warm = [ & ]( solver_constraint_t& f, float lateral, bool linear_factor )
    {
        const float impulse = lateral * info.warmstarting_factor;
        for ( int lane = 0; lane < 4; ++lane )
            f.applied_impulse[ lane ] = impulse;
        if ( a != nullptr && a->inverse_mass != 0.0f )
        {
            const float inv = a->inverse_mass;
            for ( int i = 0; i < 3; ++i )
            {
                const float linear = linear_factor ? ( f.contact_normal[ i ] * inv ) * a->linear_factor[ i ] : f.contact_normal[ i ] * inv;
                a->delta_linear[ i ] = a->delta_linear[ i ] + linear * impulse;
            }
            for ( int i = 0; i < 3; ++i )
                a->delta_angular[ i ] = a->delta_angular[ i ] + f.angular_component_a[ i ] * ( a->angular_factor[ i ] * impulse );
        }
        if ( b != nullptr && b->inverse_mass != 0.0f )
        {
            const float inv = b->inverse_mass;
            const float negative = -impulse;
            for ( int i = 0; i < 3; ++i )
            {
                const float linear = linear_factor ? ( f.contact_normal[ i ] * inv ) * b->linear_factor[ i ] : f.contact_normal[ i ] * inv;
                b->delta_linear[ i ] = b->delta_linear[ i ] + linear * negative;
            }
            for ( int i = 0; i < 3; ++i )
                b->delta_angular[ i ] = b->delta_angular[ i ] + ( b->angular_factor[ i ] * negative ) * -f.angular_component_b[ i ];
        }
    };

    solver_constraint_t& first = s.frictions[ contact.friction_index ];
    if ( ( info.solver_mode & 8 ) == 0 )
    {
        zero( first );
        if ( ( info.solver_mode & 0x10 ) != 0 )
            zero( s.frictions[ contact.friction_index + 1 ] );
        return;
    }
    if ( ( info.solver_mode & 4 ) != 0 )
        warm( first, point.lateral_impulse_1, true );
    else
        zero( first );
    if ( ( info.solver_mode & 0x10 ) == 0 )
        return;
    solver_constraint_t& second = s.frictions[ contact.friction_index + 1 ];
    if ( ( info.solver_mode & 4 ) != 0 )
        warm( second, point.lateral_impulse_2, false );
    else
        zero( second );
}

// getFixedBody @0x5749D0: a function-static btRigidBody (guard dword_819A0C
// bit 1, built by the ctor 0x50D0F0, at dword_819A40) given zero mass by
// setMassProps(0, 0) on EVERY call. Modelled as a zero-initialised
// solver_rigid_t: of what the solver reads of it - inverse mass +352 and the
// inverse-mass vector +624 - both are 0 after setMassProps, as here. The
// rest of the ctor's state (linear / angular factors) is not modelled.
inline solver_rigid_t& fixed_body( )
{
    static solver_rigid_t s_fixed{};
    return s_fixed;
}

// setupFrictionConstraint @0x570E00 (twelve stack arguments, retn 30h: row,
// normal, two body slots it never reads, manifold point, rel_pos1, rel_pos2,
// colObj0, colObj1, relaxation, desiredVelocity, cfmSlip), from the
// decompilation with signs and stores checked in the disassembly. A body
// counts only if its internalType has bit 2; both collision objects are
// dereferenced without a null test, as in retail.
//
// Writes +0..+116, +128..+152. Leaves +120, +124 and +156 alone - the caller
// sets +124; nothing reads the other two on a friction row.
inline void setup_friction_constraint( solver_constraint_t& c, const float* normal, const solver_contact_point_t& point,
    const float* r1, const float* r2, solver_rigid_t* object0, solver_rigid_t* object1,
    float relaxation, float desired_velocity, float cfm_slip )
{
    solver_rigid_t* a = ( object0->internal_type & 2 ) != 0 ? object0 : nullptr;
    solver_rigid_t* b = ( object1->internal_type & 2 ) != 0 ? object1 : nullptr;
    std::memcpy( c.contact_normal, normal, 16 );
    c.solver_body_a = a != nullptr ? a : &fixed_body( );
    c.solver_body_b = b != nullptr ? b : &fixed_body( );
    for ( int lane = 0; lane < 4; ++lane )
    {
        c.applied_impulse[ lane ] = 0.0f;
        c.applied_push_impulse[ lane ] = 0.0f;
    }
    c.friction = point.combined_friction;
    c.original_contact_point = nullptr;

    const float n0 = c.contact_normal[ 0 ], n1 = c.contact_normal[ 1 ], n2 = c.contact_normal[ 2 ];
    const float x1 = n2 * r1[ 1 ] - n1 * r1[ 2 ];
    const float y1 = n0 * r1[ 2 ] - n2 * r1[ 0 ];
    const float z1 = n1 * r1[ 0 ] - n0 * r1[ 1 ];
    c.relpos1_cross_normal[ 0 ] = x1;
    c.relpos1_cross_normal[ 1 ] = y1;
    c.relpos1_cross_normal[ 2 ] = z1;
    c.relpos1_cross_normal[ 3 ] = 0.0f;
    if ( a != nullptr )
    {
        const float* m = a->inv_inertia_world;
        c.angular_component_a[ 0 ] = ( ( m[ 1 ] * y1 + x1 * m[ 0 ] ) + m[ 2 ] * z1 ) * a->angular_factor[ 0 ];
        c.angular_component_a[ 1 ] = a->angular_factor[ 1 ] * ( ( m[ 5 ] * y1 + x1 * m[ 4 ] ) + m[ 6 ] * z1 );
        c.angular_component_a[ 2 ] = a->angular_factor[ 2 ] * ( ( m[ 9 ] * y1 + x1 * m[ 8 ] ) + m[ 10 ] * z1 );
    }
    else
    {
        c.angular_component_a[ 0 ] = c.angular_component_a[ 1 ] = c.angular_component_a[ 2 ] = 0.0f;
    }
    c.angular_component_a[ 3 ] = 0.0f;

    const float m0 = -n0, m1 = -n1, m2 = -n2;
    const float x2 = m2 * r2[ 1 ] - m1 * r2[ 2 ];
    const float y2 = m0 * r2[ 2 ] - m2 * r2[ 0 ];
    const float z2 = m1 * r2[ 0 ] - m0 * r2[ 1 ];
    c.relpos2_cross_normal[ 0 ] = x2;
    c.relpos2_cross_normal[ 1 ] = y2;
    c.relpos2_cross_normal[ 2 ] = z2;
    c.relpos2_cross_normal[ 3 ] = 0.0f;
    if ( b != nullptr )
    {
        const float* m = b->inv_inertia_world;
        c.angular_component_b[ 0 ] = ( ( m[ 1 ] * y2 + x2 * m[ 0 ] ) + m[ 2 ] * z2 ) * b->angular_factor[ 0 ];
        c.angular_component_b[ 1 ] = b->angular_factor[ 1 ] * ( ( m[ 5 ] * y2 + x2 * m[ 4 ] ) + m[ 6 ] * z2 );
        c.angular_component_b[ 2 ] = b->angular_factor[ 2 ] * ( ( m[ 9 ] * y2 + x2 * m[ 8 ] ) + m[ 10 ] * z2 );
    }
    else
    {
        c.angular_component_b[ 0 ] = c.angular_component_b[ 1 ] = c.angular_component_b[ 2 ] = 0.0f;
    }
    c.angular_component_b[ 3 ] = 0.0f;

    // n . (angularComponent x rel_pos) + inverse mass, per dynamic body.
    float denom_a = 0.0f;
    if ( a != nullptr )
    {
        const float* ga = c.angular_component_a;
        denom_a = ( ( n1 * ( r1[ 0 ] * ga[ 2 ] - r1[ 2 ] * ga[ 0 ] ) + n0 * ( r1[ 2 ] * ga[ 1 ] - r1[ 1 ] * ga[ 2 ] ) )
                      + n2 * ( r1[ 1 ] * ga[ 0 ] - r1[ 0 ] * ga[ 1 ] ) )
            + a->inverse_mass;
    }
    float denom_b = 0.0f;
    if ( b != nullptr )
    {
        const float gx = -c.angular_component_b[ 0 ], gy = -c.angular_component_b[ 1 ], gz = -c.angular_component_b[ 2 ];
        denom_b = ( ( n1 * ( gz * r2[ 0 ] - gx * r2[ 2 ] ) + n0 * ( gy * r2[ 2 ] - gz * r2[ 1 ] ) )
                      + n2 * ( gx * r2[ 1 ] - gy * r2[ 0 ] ) )
            + b->inverse_mass;
    }
    const float jac = relaxation / ( denom_b + denom_a );
    c.jac_diag_ab_inv = jac;

    // The relative velocity along the row. A missing body contributes zero
    // vectors, multiplied through as retail does.
    const float zero[ 4 ] = { };
    const float* lin_a = a != nullptr ? a->linear_velocity : zero;
    const float* ang_a = a != nullptr ? a->angular_velocity : zero;
    const float* lin_b = b != nullptr ? b->linear_velocity : zero;
    const float* ang_b = b != nullptr ? b->angular_velocity : zero;
    const float vel_a = ( ( ang_a[ 1 ] * y1 + x1 * ang_a[ 0 ] ) + ang_a[ 2 ] * z1 )
        + ( ( lin_a[ 1 ] * n1 + n0 * lin_a[ 0 ] ) + lin_a[ 2 ] * n2 );
    const float ang_dot_b = ( y2 * ang_b[ 1 ] + ang_b[ 0 ] * x2 ) + z2 * ang_b[ 2 ];
    const float lin_dot_b = ( lin_b[ 1 ] * n1 + n0 * lin_b[ 0 ] ) + lin_b[ 2 ] * n2;
    c.lower_limit = 0.0f;
    c.upper_limit = 1.0e10f;                // 0x501502F9
    c.rhs = ( desired_velocity - ( ( ang_dot_b - lin_dot_b ) + vel_a ) ) * jac;
    c.cfm = cfm_slip;
}

// btAlignedObjectArray<btSolverConstraint>::expand as both setup paths
// inline it: when full, grow to twice the size (1 when empty) through
// btAlignedAlloc, copy every row, free the old block if owned. Returns the
// new row's index. A grown block is not cleared in retail; here it is zero.
inline int solver_pool_expand( solver_constraint_t*& data, int& count, int& capacity, bool& owns )
{
    if ( count == capacity )
    {
        const int grown = count != 0 ? 2 * count : 1;
        if ( capacity < grown )
        {
            solver_constraint_t* fresh = new solver_constraint_t[ grown ]{};
            for ( int i = 0; i < count; ++i )
                fresh[ i ] = data[ i ];
            if ( owns )
                delete[] data;
            data = fresh;
            owns = true;
            capacity = grown;
        }
    }
    return count++;
}

// addFrictionConstraint @0x571430 (thiscall solver; twelve stack arguments,
// retn 30h: normal, two body slots it never reads, frictionIndex, manifold
// point, rel_pos1, rel_pos2, colObj0, colObj1, relaxation,
// desiredVelocity, cfmSlip). Grows the friction pool as btAlignedObjectArray
// does - when full, to twice the size (1 when empty), copying every row and
// freeing the old block if the pool owns it - then appends a row whose +124
// is frictionIndex and hands it to setupFrictionConstraint. Retail returns
// the row; this returns its index, which survives a later growth.
//
// A grown block is NOT cleared in retail, so the new row's +120 and +156
// (which setupFrictionConstraint leaves alone, and nothing reads on a
// friction row) hold whatever the allocator returned; here they are zero.
inline int add_friction_constraint( solver_t& s, const float* normal, int friction_index,
    const solver_contact_point_t& point, const float* r1, const float* r2, solver_rigid_t* object0,
    solver_rigid_t* object1, float relaxation, float desired_velocity, float cfm_slip )
{
    const int index = solver_pool_expand( s.frictions, s.friction_count, s.friction_capacity, s.owns_frictions );
    solver_constraint_t& c = s.frictions[ index ];
    c.friction_index = friction_index;
    setup_friction_constraint( c, normal, point, r1, r2, object0, object1, relaxation, desired_velocity, cfm_slip );
    return index;
}

// setupContactConstraint @0x571560 (ten stack arguments, retn 28h: row,
// colObj0, colObj1, manifold point, info, and the outputs vel, rel_vel,
// relaxation, rel_pos1, rel_pos2), from the decompilation with every
// negation, compare and store checked in the disassembly.
//
// Writes the row's +0..+116 and +140..+156. Leaves +120 / +124 / +128 /
// +132 / +136 to the caller. The compares are retail's, NaN included: a
// restitution is zeroed only when 0 >= it (comiss / jb), the penetration
// goes to the positional term unless pen > 0 (comiss / jbe), and the split
// path is taken unless pen > threshold (comiss / ja).
inline void setup_contact_constraint( solver_constraint_t& c, solver_rigid_t* object0, solver_rigid_t* object1,
    const solver_contact_point_t& cp, const solver_info_t& info,
    float* vel, float& rel_vel, float& relaxation, float* rel_pos1, float* rel_pos2 )
{
    solver_rigid_t* a = ( object0->internal_type & 2 ) != 0 ? object0 : nullptr;
    solver_rigid_t* b = ( object1->internal_type & 2 ) != 0 ? object1 : nullptr;
    const float* r1 = rel_pos1;
    const float* r2 = rel_pos2;
    rel_pos1[ 0 ] = cp.position_world_on_a[ 0 ] - object0->origin[ 0 ];
    rel_pos1[ 1 ] = cp.position_world_on_a[ 1 ] - object0->origin[ 1 ];
    rel_pos1[ 2 ] = cp.position_world_on_a[ 2 ] - object0->origin[ 2 ];
    rel_pos1[ 3 ] = 0.0f;
    rel_pos2[ 0 ] = cp.position_world_on_b[ 0 ] - object1->origin[ 0 ];
    rel_pos2[ 1 ] = cp.position_world_on_b[ 1 ] - object1->origin[ 1 ];
    rel_pos2[ 2 ] = cp.position_world_on_b[ 2 ] - object1->origin[ 2 ];
    rel_pos2[ 3 ] = 0.0f;
    relaxation = 1.0f;

    const float* n = cp.normal_world_on_b;
    // rel_pos1 x normal, as the torque axis of A.
    const float tx = n[ 2 ] * r1[ 1 ] - n[ 1 ] * r1[ 2 ];
    const float ty = n[ 0 ] * r1[ 2 ] - n[ 2 ] * r1[ 0 ];
    const float tz = n[ 1 ] * r1[ 0 ] - n[ 0 ] * r1[ 1 ];
    if ( a != nullptr )
    {
        const float* m = a->inv_inertia_world;
        c.angular_component_a[ 0 ] = ( ( m[ 1 ] * ty + m[ 0 ] * tx ) + m[ 2 ] * tz ) * a->angular_factor[ 0 ];
        c.angular_component_a[ 1 ] = a->angular_factor[ 1 ] * ( ( m[ 5 ] * ty + m[ 4 ] * tx ) + m[ 6 ] * tz );
        c.angular_component_a[ 2 ] = a->angular_factor[ 2 ] * ( ( m[ 9 ] * ty + m[ 8 ] * tx ) + m[ 10 ] * tz );
    }
    else
    {
        c.angular_component_a[ 0 ] = c.angular_component_a[ 1 ] = c.angular_component_a[ 2 ] = 0.0f;
    }
    c.angular_component_a[ 3 ] = 0.0f;

    // rel_pos2 x normal, negated, as the torque axis of B.
    const float ux = r2[ 1 ] * n[ 2 ] - r2[ 2 ] * n[ 1 ];
    const float uy = r2[ 2 ] * n[ 0 ] - r2[ 0 ] * n[ 2 ];
    const float uz = r2[ 0 ] * n[ 1 ] - r2[ 1 ] * n[ 0 ];
    if ( b != nullptr )
    {
        const float* m = b->inv_inertia_world;
        const float mx = -ux, my = -uy, mz = -uz;
        c.angular_component_b[ 0 ] = b->angular_factor[ 0 ] * ( ( m[ 1 ] * my + m[ 0 ] * mx ) + m[ 2 ] * mz );
        c.angular_component_b[ 1 ] = b->angular_factor[ 1 ] * ( ( m[ 5 ] * my + m[ 4 ] * mx ) + m[ 6 ] * mz );
        c.angular_component_b[ 2 ] = b->angular_factor[ 2 ] * ( ( m[ 9 ] * my + m[ 8 ] * mx ) + m[ 10 ] * mz );
    }
    else
    {
        c.angular_component_b[ 0 ] = c.angular_component_b[ 1 ] = c.angular_component_b[ 2 ] = 0.0f;
    }
    c.angular_component_b[ 3 ] = 0.0f;

    float denom_a = 0.0f;
    if ( a != nullptr )
    {
        const float* g = c.angular_component_a;
        denom_a = ( ( n[ 1 ] * ( r1[ 0 ] * g[ 2 ] - r1[ 2 ] * g[ 0 ] ) + n[ 0 ] * ( r1[ 2 ] * g[ 1 ] - r1[ 1 ] * g[ 2 ] ) )
                      + n[ 2 ] * ( r1[ 1 ] * g[ 0 ] - r1[ 0 ] * g[ 1 ] ) )
            + a->inverse_mass;
    }
    float denom_b = 0.0f;
    if ( b != nullptr )
    {
        const float gx = -c.angular_component_b[ 0 ], gy = -c.angular_component_b[ 1 ], gz = -c.angular_component_b[ 2 ];
        denom_b = ( ( n[ 1 ] * ( r2[ 0 ] * gz - r2[ 2 ] * gx ) + n[ 0 ] * ( r2[ 2 ] * gy - r2[ 1 ] * gz ) )
                      + n[ 2 ] * ( r2[ 1 ] * gx - r2[ 0 ] * gy ) )
            + b->inverse_mass;
    }
    c.jac_diag_ab_inv = relaxation / ( denom_b + denom_a );

    std::memcpy( c.contact_normal, cp.normal_world_on_b, 16 );
    const float n0 = c.contact_normal[ 0 ], n1 = c.contact_normal[ 1 ], n2 = c.contact_normal[ 2 ];
    c.relpos1_cross_normal[ 0 ] = r1[ 1 ] * n2 - r1[ 2 ] * n1;
    c.relpos1_cross_normal[ 1 ] = r1[ 2 ] * n0 - r1[ 0 ] * n2;
    c.relpos1_cross_normal[ 2 ] = r1[ 0 ] * n1 - r1[ 1 ] * n0;
    c.relpos1_cross_normal[ 3 ] = 0.0f;
    const float m0 = -n0, m1 = -n1, m2 = -n2;
    c.relpos2_cross_normal[ 0 ] = r2[ 1 ] * m2 - r2[ 2 ] * m1;
    c.relpos2_cross_normal[ 1 ] = r2[ 2 ] * m0 - r2[ 0 ] * m2;
    c.relpos2_cross_normal[ 2 ] = r2[ 0 ] * m1 - r2[ 1 ] * m0;
    c.relpos2_cross_normal[ 3 ] = 0.0f;

    // Velocity of each contact point: linear + angular x rel_pos.
    float vel1[ 3 ] = { 0.0f, 0.0f, 0.0f };
    if ( a != nullptr )
    {
        const float* lv = a->linear_velocity;
        const float* av = a->angular_velocity;
        vel1[ 0 ] = ( r1[ 2 ] * av[ 1 ] - r1[ 1 ] * av[ 2 ] ) + lv[ 0 ];
        vel1[ 1 ] = lv[ 1 ] + ( r1[ 0 ] * av[ 2 ] - r1[ 2 ] * av[ 0 ] );
        vel1[ 2 ] = lv[ 2 ] + ( r1[ 1 ] * av[ 0 ] - r1[ 0 ] * av[ 1 ] );
    }
    float vel2[ 3 ] = { 0.0f, 0.0f, 0.0f };
    if ( b != nullptr )
    {
        const float* lv = b->linear_velocity;
        const float* av = b->angular_velocity;
        vel2[ 0 ] = lv[ 0 ] + ( r2[ 2 ] * av[ 1 ] - r2[ 1 ] * av[ 2 ] );
        vel2[ 1 ] = lv[ 1 ] + ( r2[ 0 ] * av[ 2 ] - r2[ 2 ] * av[ 0 ] );
        vel2[ 2 ] = lv[ 2 ] + ( r2[ 1 ] * av[ 0 ] - r2[ 0 ] * av[ 1 ] );
    }
    vel[ 0 ] = vel1[ 0 ] - vel2[ 0 ];
    vel[ 1 ] = vel1[ 1 ] - vel2[ 1 ];
    vel[ 2 ] = vel1[ 2 ] - vel2[ 2 ];
    vel[ 3 ] = 0.0f;
    rel_vel = ( vel[ 1 ] * n[ 1 ] + n[ 0 ] * vel[ 0 ] ) + vel[ 2 ] * n[ 2 ];

    const float penetration = cp.distance + info.linear_slop;
    c.friction = cp.combined_friction;

    float restitution = 0.0f;
    if ( !( cp.life_time > info.resting_contact_restitution_threshold ) )
    {
        restitution = -( cp.combined_restitution * rel_vel );
        if ( 0.0f >= restitution )
            restitution = 0.0f;
    }

    if ( ( info.solver_mode & 4 ) != 0 )
    {
        const float impulse = cp.applied_impulse * info.warmstarting_factor;
        for ( int lane = 0; lane < 4; ++lane )
            c.applied_impulse[ lane ] = impulse;
        if ( a != nullptr )
        {
            const float inv = a->inverse_mass;
            const float lx = ( n0 * inv ) * a->linear_factor[ 0 ];
            const float ly = a->linear_factor[ 1 ] * ( n1 * inv );
            const float lz = a->linear_factor[ 2 ] * ( n2 * inv );
            if ( inv != 0.0f )
            {
                a->delta_linear[ 0 ] = a->delta_linear[ 0 ] + impulse * lx;
                a->delta_linear[ 1 ] = a->delta_linear[ 1 ] + impulse * ly;
                a->delta_linear[ 2 ] = a->delta_linear[ 2 ] + impulse * lz;
                for ( int i = 0; i < 3; ++i )
                    a->delta_angular[ i ] = a->delta_angular[ i ] + c.angular_component_a[ i ] * ( a->angular_factor[ i ] * impulse );
            }
        }
        if ( b != nullptr )
        {
            const float inv = b->inverse_mass;
            const float lx = ( n0 * inv ) * b->linear_factor[ 0 ];
            const float ly = b->linear_factor[ 1 ] * ( n1 * inv );
            const float lz = b->linear_factor[ 2 ] * ( n2 * inv );
            const float negative = -impulse;
            if ( inv != 0.0f )
            {
                // As setFrictionConstraintImpulse: the normal unnegated, the
                // impulse negated - retail's form, kept.
                b->delta_linear[ 0 ] = negative * lx + b->delta_linear[ 0 ];
                b->delta_linear[ 1 ] = negative * ly + b->delta_linear[ 1 ];
                b->delta_linear[ 2 ] = negative * lz + b->delta_linear[ 2 ];
                for ( int i = 0; i < 3; ++i )
                    b->delta_angular[ i ] = b->delta_angular[ i ] + ( b->angular_factor[ i ] * negative ) * -c.angular_component_b[ i ];
            }
        }
    }
    else
    {
        for ( int lane = 0; lane < 4; ++lane )
            c.applied_impulse[ lane ] = 0.0f;
    }
    for ( int lane = 0; lane < 4; ++lane )
        c.applied_push_impulse[ lane ] = 0.0f;

    const float zero[ 4 ] = { };
    const float* lin_a = a != nullptr ? a->linear_velocity : zero;
    const float* ang_a = a != nullptr ? a->angular_velocity : zero;
    const float* lin_b = b != nullptr ? b->linear_velocity : zero;
    const float* ang_b = b != nullptr ? b->angular_velocity : zero;
    const float* p1 = c.relpos1_cross_normal;
    const float* p2 = c.relpos2_cross_normal;
    const float vel1_dot = ( ( p1[ 1 ] * ang_a[ 1 ] + p1[ 0 ] * ang_a[ 0 ] ) + p1[ 2 ] * ang_a[ 2 ] )
        + ( ( lin_a[ 1 ] * n1 + n0 * lin_a[ 0 ] ) + lin_a[ 2 ] * n2 );
    const float ang_dot_b = ( ang_b[ 1 ] * p2[ 1 ] + ang_b[ 0 ] * p2[ 0 ] ) + ang_b[ 2 ] * p2[ 2 ];
    const float lin_dot_b = ( lin_b[ 1 ] * n1 + lin_b[ 0 ] * n0 ) + lin_b[ 2 ] * n2;
    float velocity_error = restitution - ( ( ang_dot_b - lin_dot_b ) + vel1_dot );

    float positional_error = 0.0f;
    if ( penetration > 0.0f )
        velocity_error = velocity_error - penetration / info.time_step;
    else
        positional_error = -( ( info.erp * penetration ) / info.time_step );

    const float penetration_impulse = c.jac_diag_ab_inv * positional_error;
    const float velocity_impulse = c.jac_diag_ab_inv * velocity_error;
    if ( info.split_impulse != 0 && !( penetration > info.split_impulse_penetration_threshold ) )
    {
        c.rhs_penetration = penetration_impulse;
        c.rhs = velocity_impulse;
    }
    else
    {
        c.rhs = velocity_impulse + penetration_impulse;
        c.rhs_penetration = 0.0f;
    }
    c.cfm = 0.0f;
    c.lower_limit = 0.0f;
    c.upper_limit = 1.0e10f;                // 0x501502F9
}

// convertContact @0x5727D0 (thiscall solver; manifold, info; retn 8), from
// the disassembly. Nothing is done unless one of the two bodies is rigid
// with a non-zero inverse mass. Each point the manifold's processing
// threshold admits (threshold >= distance - a NaN distance is skipped)
// becomes a contact row, then its friction rows, then the warm start.
//
// Retail's order, kept: on the fresh path the SECOND friction direction's
// row (mode 0x10) is appended BEFORE the first's; on the cached path (mode
// 0x20 with the point's flag set) the first comes first. The fresh path
// writes both directions back into the point, after applyAnisotropicFriction.
inline void convert_contact( solver_t& s, solver_manifold_t& manifold, const solver_info_t& info )
{
    solver_rigid_t* object0 = manifold.body0;
    solver_rigid_t* object1 = manifold.body1;
    solver_rigid_t* a = ( object0->internal_type & 2 ) != 0 ? object0 : nullptr;
    solver_rigid_t* b = ( object1->internal_type & 2 ) != 0 ? object1 : nullptr;
    const bool dynamic_a = a != nullptr && a->inverse_mass != 0.0f;
    const bool dynamic_b = b != nullptr && b->inverse_mass != 0.0f;
    if ( !dynamic_a && !dynamic_b )
        return;

    for ( int j = 0; j < manifold.num_contacts; ++j )
    {
        solver_contact_point_t& cp = manifold.points[ j ];
        if ( !( manifold.contact_processing_threshold >= cp.distance ) )
            continue;

        const int contact_index = solver_pool_expand( s.contacts, s.contact_count, s.contact_capacity, s.owns_contacts );
        solver_constraint_t& c = s.contacts[ contact_index ];
        c.solver_body_a = a != nullptr ? a : &fixed_body( );
        c.solver_body_b = b != nullptr ? b : &fixed_body( );
        c.original_contact_point = &cp;
        float vel[ 4 ], rel_pos1[ 4 ], rel_pos2[ 4 ];
        float rel_vel = 0.0f, relaxation = 0.0f;
        setup_contact_constraint( c, object0, object1, cp, info, vel, rel_vel, relaxation, rel_pos1, rel_pos2 );
        // c may move no further: only the friction pool grows below.
        c.friction_index = s.friction_count;

        const float* n = cp.normal_world_on_b;
        float* dir1 = cp.lateral_friction_dir_1;
        float* dir2 = cp.lateral_friction_dir_2;
        if ( ( info.solver_mode & 0x20 ) != 0 && cp.lateral_friction_initialized != 0 )
        {
            add_friction_constraint( s, dir1, contact_index, cp, rel_pos1, rel_pos2, object0, object1, relaxation,
                cp.contact_motion_1, cp.contact_cfm_1 );
            if ( ( info.solver_mode & 0x10 ) != 0 )
                add_friction_constraint( s, dir2, contact_index, cp, rel_pos1, rel_pos2, object0, object1, relaxation,
                    cp.contact_motion_2, cp.contact_cfm_2 );
        }
        else
        {
            dir1[ 0 ] = vel[ 0 ] - n[ 0 ] * rel_vel;
            dir1[ 1 ] = vel[ 1 ] - n[ 1 ] * rel_vel;
            dir1[ 2 ] = vel[ 2 ] - rel_vel * n[ 2 ];
            dir1[ 3 ] = 0.0f;
            const float lateral = ( dir1[ 0 ] * dir1[ 0 ] + dir1[ 1 ] * dir1[ 1 ] ) + dir1[ 2 ] * dir1[ 2 ];
            if ( ( info.solver_mode & 0x40 ) == 0 && lateral > 1.1920929e-7f )     // 0x34000000
            {
                const float inv = 1.0f / std::sqrt( lateral );
                dir1[ 0 ] = dir1[ 0 ] * inv;
                dir1[ 1 ] = dir1[ 1 ] * inv;
                dir1[ 2 ] = dir1[ 2 ] * inv;
                if ( ( info.solver_mode & 0x10 ) != 0 )
                {
                    dir2[ 0 ] = n[ 2 ] * dir1[ 1 ] - dir1[ 2 ] * n[ 1 ];
                    dir2[ 1 ] = n[ 0 ] * dir1[ 2 ] - dir1[ 0 ] * n[ 2 ];
                    dir2[ 2 ] = dir1[ 0 ] * n[ 1 ] - n[ 0 ] * dir1[ 1 ];
                    dir2[ 3 ] = 0.0f;
                    const float inv2 = 1.0f / std::sqrt( ( dir2[ 0 ] * dir2[ 0 ] + dir2[ 1 ] * dir2[ 1 ] ) + dir2[ 2 ] * dir2[ 2 ] );
                    dir2[ 0 ] = dir2[ 0 ] * inv2;
                    dir2[ 1 ] = dir2[ 1 ] * inv2;
                    dir2[ 2 ] = dir2[ 2 ] * inv2;
                }
            }
            else
            {
                // btPlaneSpace1, inlined. The w lanes are not written here.
                if ( std::fabs( n[ 2 ] ) > 0.70710677f )      // 0x3F3504F3
                {
                    const float sq = n[ 2 ] * n[ 2 ] + n[ 1 ] * n[ 1 ];
                    const float k = 1.0f / std::sqrt( sq );
                    dir1[ 0 ] = 0.0f;
                    dir1[ 1 ] = -( k * n[ 2 ] );
                    dir1[ 2 ] = k * n[ 1 ];
                    dir2[ 0 ] = k * sq;
                    dir2[ 1 ] = -( dir1[ 2 ] * n[ 0 ] );
                    dir2[ 2 ] = dir1[ 1 ] * n[ 0 ];
                }
                else
                {
                    const float sq = n[ 0 ] * n[ 0 ] + n[ 1 ] * n[ 1 ];
                    const float k = 1.0f / std::sqrt( sq );
                    dir1[ 0 ] = -( n[ 1 ] * k );
                    dir1[ 1 ] = n[ 0 ] * k;
                    dir1[ 2 ] = 0.0f;
                    dir2[ 0 ] = -( dir1[ 1 ] * n[ 2 ] );
                    dir2[ 1 ] = dir1[ 0 ] * n[ 2 ];
                    dir2[ 2 ] = k * sq;
                }
            }
            if ( ( info.solver_mode & 0x10 ) != 0 )
            {
                apply_anisotropic_friction( object0, dir2 );
                apply_anisotropic_friction( object1, dir2 );
                add_friction_constraint( s, dir2, contact_index, cp, rel_pos1, rel_pos2, object0, object1, relaxation, 0.0f, 0.0f );
            }
            apply_anisotropic_friction( object0, dir1 );
            apply_anisotropic_friction( object1, dir1 );
            add_friction_constraint( s, dir1, contact_index, cp, rel_pos1, rel_pos2, object0, object1, relaxation, 0.0f, 0.0f );
            cp.lateral_friction_initialized = 1;
        }
        set_friction_constraint_impulse( s, s.contacts[ contact_index ], a, b, cp, info );
    }
}

// btAlignedObjectArray<int>::resize as the setup inlines it for the two
// order arrays: past the capacity, reallocate to EXACTLY the new size (no
// doubling) and copy; new entries zeroed.
inline void solver_order_resize( int*& data, int& count, int& capacity, bool& owns, int size )
{
    if ( size > count && capacity < size )
    {
        int* fresh = new int[ size ]{};
        for ( int i = 0; i < count; ++i )
            fresh[ i ] = data[ i ];
        if ( owns )
            delete[] data;
        data = fresh;
        owns = true;
        capacity = size;
    }
    for ( int i = count; i < size; ++i )
        data[ i ] = 0;
    count = size;
}

// solveGroupCacheFriendlySetup @0x572E90 (solver vtable slot 7; thiscall,
// nine stack arguments), from the decompilation and disassembly. Its
// CProfileSample("solveGroupCacheFriendlySetup") - started inline, stopped
// by 0x5525C0 CProfileManager::Stop_Profile - is instrumentation and is not
// recreated.
//
// THE TYPED-CONSTRAINT STAGE IS NOT RECREATED. Retail calls each
// constraint's buildJacobian (vtable +4), zeroes its +32, sizes the
// constraint pool [26..30] with getInfo1 (vtable +12, skipped for a disabled
// one: +20 == 0), resizes the non-contact pool to the row total and fills
// each row through getInfo2 (vtable +16) - per-class virtual code that has
// not been read. Whether the boat world holds any btTypedConstraint at all
// is open; until it is read this returns false for a non-zero count rather
// than guess.
inline bool solver_setup( solver_t& s, solver_rigid_t* const* bodies, int body_count,
    solver_manifold_t* const* manifolds, int manifold_count, int constraint_count, const solver_info_t& info )
{
    if ( constraint_count != 0 )
        return false;
    if ( constraint_count + manifold_count == 0 )
        return true;

    for ( int i = 0; i < body_count; ++i )
    {
        solver_rigid_t& b = *bodies[ i ];
        if ( ( b.internal_type & 2 ) == 0 )
            continue;
        for ( int lane = 0; lane < 4; ++lane )
        {
            b.delta_linear[ lane ] = 0.0f;
            b.delta_angular[ lane ] = 0.0f;
        }
        if ( info.split_impulse != 0 )
        {
            for ( int lane = 0; lane < 4; ++lane )
            {
                b.push_velocity[ lane ] = 0.0f;
                b.turn_velocity[ lane ] = 0.0f;
            }
        }
    }

    // With no typed constraints the sizes pool and the non-contact pool are
    // both resized to zero.
    s.constraint_sizes_count = 0;
    s.non_contact_count = 0;

    for ( int i = 0; i < manifold_count; ++i )
        convert_contact( s, *manifolds[ i ], info );

    solver_order_resize( s.order_contacts, s.order_contact_count, s.order_contact_capacity, s.owns_order_contacts, s.contact_count );
    solver_order_resize( s.order_frictions, s.order_friction_count, s.order_friction_capacity, s.owns_order_frictions, s.friction_count );
    for ( int i = 0; i < s.contact_count; ++i )
        s.order_contacts[ i ] = i;
    for ( int i = 0; i < s.friction_count; ++i )
        s.order_frictions[ i ] = i;
    return true;
}

// solveGroup @0x574870 (solver vtable slot 2; thiscall, ten stack arguments,
// retn 28h): setup (slot 7), iterations (slot 8), finish (slot 6), each with
// the first nine arguments, inside a CProfileSample("solveGroup") that is
// not recreated; returns 0. The iterations and the finish run EVEN WHEN THE
// SETUP EARLY-OUTS - with no manifolds, the finish still writes every rigid
// body's (unzeroed) deltas back into its velocities, as retail does. The
// slots are bound directly: the world's solver is a plain
// btSequentialImpulseConstraintSolver (vtable 0x74f8ec). Returns false only
// for typed constraints, whose stage is not recreated (see solver_setup).
inline bool solver_solve_group( solver_t& s, solver_rigid_t* const* bodies, int body_count,
    solver_manifold_t* const* manifolds, int manifold_count, int constraint_count, const solver_info_t& info )
{
    if ( !solver_setup( s, bodies, body_count, manifolds, manifold_count, constraint_count, info ) )
        return false;
    solve_iterations( s, info, nullptr, nullptr, 0 );
    solver_finish( s, bodies, body_count, info );
    return true;
}
