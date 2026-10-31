#pragma once

#include <workspace/core/dynamics/iterate.hxx>
#include <workspace/core/dynamics/writeback.hxx>

#include <cmath>
#include <cstddef>

// solveGroupCacheFriendlyFinish @0x5744F0 (solver vtable slot 6), recreated
// from the disassembly: thiscall solver, nine stack arguments (bodies,
// numBodies, manifolds, numManifolds, constraints, numConstraints, info,
// debugDrawer, stackAlloc).

// The btManifoldPoint members the solver reads and writes (the finish, the
// friction and contact setup). Only these offsets are touched; the rest of
// the point is UNKNOWN here.
struct solver_contact_point_t
{
    float local_point_a[ 4 ];               // +0  (contact.hxx)
    float local_point_b[ 4 ];               // +16
    float position_world_on_b[ 4 ];         // +32
    float position_world_on_a[ 4 ];         // +48
    float normal_world_on_b[ 4 ];           // +64
    float distance;                         // +80
    float combined_friction;                // +84
    float combined_restitution;             // +88
    int part_id_0;                          // +92
    int part_id_1;                          // +96
    int index_0;                            // +100
    int index_1;                            // +104
    // +108 m_userPersistentData: a POINTER in retail, owned by the Vu contact
    // callbacks; carried here as the 32-bit value only (contact.hxx).
    unsigned user_persistent_data;
    float applied_impulse;                  // +112
    unsigned char lateral_friction_initialized;  // +116 (byte)
    unsigned char m_unknown1[ 3 ];
    float lateral_impulse_1;                // +120 (2.82: m_appliedImpulseLateral1)
    float lateral_impulse_2;                // +124 (2.82: m_appliedImpulseLateral2)
    float contact_motion_1;                 // +128
    float contact_motion_2;                 // +132
    float contact_cfm_1;                    // +136
    float contact_cfm_2;                    // +140
    int life_time;                          // +144
    unsigned char m_unknown3[ 160 - 148 ];
    float lateral_friction_dir_1[ 4 ];      // +160
    float lateral_friction_dir_2[ 4 ];      // +176
    // +192 2.79's btConstraintRow m_constraintRow[3], 32 bytes each; only the
    // accumulated impulse (row float 7, +220 / +252 / +284) is read - by the
    // manifold's replace (contact.hxx). The solver never touches them.
    float constraint_row[ 3 ][ 8 ];
};
static_assert( offsetof( solver_contact_point_t, position_world_on_b ) == 32, "btManifoldPoint +32" );
static_assert( offsetof( solver_contact_point_t, position_world_on_a ) == 48, "btManifoldPoint +48" );
static_assert( offsetof( solver_contact_point_t, normal_world_on_b ) == 64, "btManifoldPoint +64" );
static_assert( offsetof( solver_contact_point_t, distance ) == 80, "btManifoldPoint +80" );
static_assert( offsetof( solver_contact_point_t, combined_friction ) == 84, "btManifoldPoint +84" );
static_assert( offsetof( solver_contact_point_t, combined_restitution ) == 88, "btManifoldPoint +88" );
static_assert( offsetof( solver_contact_point_t, applied_impulse ) == 112, "btManifoldPoint +112" );
static_assert( offsetof( solver_contact_point_t, lateral_impulse_1 ) == 120, "btManifoldPoint +120" );
static_assert( offsetof( solver_contact_point_t, lateral_impulse_2 ) == 124, "btManifoldPoint +124" );
static_assert( offsetof( solver_contact_point_t, life_time ) == 144, "btManifoldPoint +144" );
static_assert( offsetof( solver_contact_point_t, lateral_friction_initialized ) == 116, "btManifoldPoint +116" );
static_assert( offsetof( solver_contact_point_t, contact_motion_1 ) == 128, "btManifoldPoint +128" );
static_assert( offsetof( solver_contact_point_t, contact_cfm_2 ) == 140, "btManifoldPoint +140" );
static_assert( offsetof( solver_contact_point_t, lateral_friction_dir_1 ) == 160, "btManifoldPoint +160" );
static_assert( offsetof( solver_contact_point_t, lateral_friction_dir_2 ) == 176, "btManifoldPoint +176" );
static_assert( offsetof( solver_contact_point_t, part_id_0 ) == 92, "btManifoldPoint +92" );
static_assert( offsetof( solver_contact_point_t, user_persistent_data ) == 108, "btManifoldPoint +108" );
static_assert( offsetof( solver_contact_point_t, constraint_row ) == 192, "btManifoldPoint +192" );
static_assert( sizeof( solver_contact_point_t ) == 288, "btManifoldPoint stride 288 (convertContact @0x5727D0)" );

// The btPersistentManifold members convertContact reads: four points from
// +16 at a 288-byte stride, the two bodies, the point count and the contact
// processing threshold. The rest (+1180 breaking threshold and the header)
// is not read here.
struct solver_manifold_t
{
    unsigned char m_unknown0[ 16 ];
    solver_contact_point_t points[ 4 ];     // +16
    struct solver_rigid_t* body0;           // +1168
    struct solver_rigid_t* body1;           // +1172
    int num_contacts;                       // +1176
    float contact_breaking_threshold;       // +1180
    float contact_processing_threshold;     // +1184
    unsigned char m_unknown1188[ 8 ];
    int dispatcher_index;                   // +1196, the dispatcher's manifold array slot (getNewManifold 0x56D380)
};
static_assert( offsetof( solver_manifold_t, points ) == 16, "btPersistentManifold +16" );
static_assert( sizeof( void* ) != 4 || offsetof( solver_manifold_t, num_contacts ) == 1176, "btPersistentManifold +1176" );
static_assert( sizeof( void* ) != 4 || offsetof( solver_manifold_t, contact_processing_threshold ) == 1184, "btPersistentManifold +1184" );

// The btTypedConstraint members the finish reads and writes; the rest is
// UNKNOWN here.
struct solver_typed_constraint_t
{
    unsigned char m_unknown0[ 16 ];
    float breaking_impulse_threshold;       // +16
    unsigned char enabled;                  // +20
    unsigned char m_unknown1[ 11 ];
    float applied_impulse;                  // +32
};
static_assert( offsetof( solver_typed_constraint_t, breaking_impulse_threshold ) == 16, "btTypedConstraint +16" );
static_assert( offsetof( solver_typed_constraint_t, enabled ) == 20, "btTypedConstraint +20" );
static_assert( offsetof( solver_typed_constraint_t, applied_impulse ) == 32, "btTypedConstraint +32" );

inline void solver_finish( solver_t& s, solver_rigid_t* const* bodies, int body_count, const solver_info_t& info )
{
    // Each contact row's impulse back into its manifold point (row +136);
    // with solverMode bit 8, the two friction rows that follow its
    // frictionIndex too.
    for ( int i = 0; i < s.contact_count; ++i )
    {
        const solver_constraint_t& c = s.contacts[ i ];
        auto& point = *static_cast< solver_contact_point_t* >( c.original_contact_point );
        point.applied_impulse = c.applied_impulse[ 0 ];
        if ( ( info.solver_mode & 8 ) != 0 )
        {
            point.lateral_impulse_1 = s.frictions[ c.friction_index ].applied_impulse[ 0 ];
            // Retail reads frictionPool[frictionIndex + 1] unconditionally.
            // Without solverMode 0x10 a contact has ONE friction row, so for
            // the last contact that slot is past the pool - unused capacity
            // or memory beyond the allocation - and retail stores whatever is
            // there. That value is UNKNOWN and is not reproduced: the point
            // is left alone. Nothing reads +124 without 0x10 (the warm start
            // and the cached directions only use it under 0x10, and with
            // 0x10 every contact has both rows).
            if ( c.friction_index + 1 < s.friction_count )
                point.lateral_impulse_2 = s.frictions[ c.friction_index + 1 ].applied_impulse[ 0 ];
        }
    }
    // Each non-contact row's impulse back into its typed constraint, which
    // is disabled once |impulse| reaches its breaking threshold (andps /
    // comiss / jb - an unordered compare leaves it enabled).
    for ( int i = 0; i < s.non_contact_count; ++i )
    {
        const solver_constraint_t& c = s.non_contacts[ i ];
        auto& constraint = *static_cast< solver_typed_constraint_t* >( c.original_contact_point );
        constraint.applied_impulse = c.applied_impulse[ 0 ];
        if ( std::fabs( c.applied_impulse[ 0 ] ) >= constraint.breaking_impulse_threshold )
            constraint.enabled = 0;
    }
    // The velocity writeback, for rigid bodies only (internalType & 2).
    for ( int i = 0; i < body_count; ++i )
    {
        solver_rigid_t& b = *bodies[ i ];
        if ( ( b.internal_type & 2 ) == 0 )
            continue;
        if ( info.split_impulse != 0 )
            writeback_velocity_split( b, info.time_step );
        else
            writeback_velocity( b );
    }
    // The three pools resized to zero. The resize's copy loops only run for
    // a negative size, so the observable effect is the size alone.
    s.contact_count = 0;
    s.non_contact_count = 0;
    s.friction_count = 0;
}