#pragma once

#include <workspace/core/dynamics/step.hxx>
#include <workspace/core/dynamics/writeback.hxx>

#include <cstdint>
#include <cstring>

// The race's dynamics world, the frame side: what VuDynamicsImpl (vtable
// 0x74C044, g_vudynamicsimpl 0x75900C) and its VuDynamicsWorldImpl (0x74F9BC)
// do around the step, recreated from retail's disassembly (world batch 15).
// The step itself is the engine's verified world_step_simulation 0x574C30
// (dynamics/step.hxx) over Bullet's recreated stages.

// ---- btDiscreteDynamicsWorld::synchronizeMotionStates 0x600B00 -------------
//
// A Bullet function the race reaches through DynamicsSync 0x4F8A20 (world slot
// 18) - absent from the engine and blocking (no body's motion reaches its
// entity without it), so recreated HERE and verified (case sync_motion_states).
// Its CProfileSample is timing only - not recreated.
//
// The body's motion state (+0x1F4, a Vu object: the boat's is VuBoatEntity's
// +0x5C, setWorldTransform 0x41E460) is the caller's: `has` answers whether a
// body has one (+0x1F4 != 0), `set` is its vt+8 setWorldTransform.
using motion_has_fn = bool ( * )( void* user, const solver_rigid_t& body );
using motion_set_fn = void ( * )( void* user, solver_rigid_t& body, const float* basis, const float* origin );

// synchronizeSingleMotionState 0x600600 (thiscall world; body; retn 4): a
// body with a motion state and flags +0xD8 & 3 == 0 (neither static nor
// kinematic) hands the motion state integrateTransform(its interpolation
// transform +0x50 / +0x80, the interpolation velocities +0x90 / +0xA0,
// localTime (world +0xF0) x hitFraction (+0xFC)) - the inlined integrate is
// writeback.hxx's integrate_transform, operation for operation (read
// 0x600639 .. 0x600AD7: the origin, the clamp 0x6F7684, the Taylor branch
// below 0.001, sinf((a x 0.5) x t) / a, cosf((a x t) x 0.5), getRotation
// 0x50F8E0, the product, the normalise, setRotation; w lanes 0).
inline void world_synchronize_single_motion_state( float local_time, solver_rigid_t& b, motion_has_fn has, motion_set_fn set, void* user )
{
    if ( !has || !has( user, b ) )
        return;
    if ( ( b.collision_flags & 3 ) != 0 )
        return;
    float basis[ 12 ], origin[ 4 ];
    integrate_transform( b.interpolation_basis, b.interpolation_origin, b.interpolation_linear, b.interpolation_angular,
                         b.hit_fraction * local_time, basis, origin );
    if ( set )
        set( user, b, basis, origin );
}

// synchronizeMotionStates 0x600B00 (thiscall world; ret): with the world's
// +0xF6 (m_synchronizeAllMotionStates - 0 from the ctor 0x5FFF40, no writer
// found) every collision object (+8 / +0x10) whose internal type +0xF4 & 2;
// otherwise every non-static rigid body (+0xCC / +0xD4) whose activation state
// +0xE4 is neither 2 nor 5.
inline void world_synchronize_motion_states( world_t& w, bool synchronize_all, motion_has_fn has, motion_set_fn set, void* user )
{
    if ( synchronize_all )
    {
        for ( solver_rigid_t* o : w.objects )
            if ( ( o->internal_type & 2 ) != 0 )
                world_synchronize_single_motion_state( w.local_time, *o, has, set, user );
        return;
    }
    for ( solver_rigid_t* b : w.bodies )
        if ( b->activation_state != 2 && b->activation_state != 5 )
            world_synchronize_single_motion_state( w.local_time, *b, has, set, user );
}
