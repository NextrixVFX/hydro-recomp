#pragma once

#include <workspace/core/dynamics/step.hxx>
#include <workspace/core/dynamics/writeback.hxx>
#include <workspace/core/world/transform.hxx>

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

// ---- VuBoatEntity's btMotionState::setWorldTransform 0x41E460 --------------
//
// The boat body's motion state (boat +0x5C, vtable 0x7383C4 slot 2; thiscall;
// const btTransform&; retn 4) - VuBoatEntity code, recreated here because the
// race's synchronizeMotionStates reaches it (flagged for the boat agent): M =
// VuMatrix_multiply(this = translation(-(boat +0x290)) 0x410AB0, the body
// transform as a VuMatrix 0x410B40) - the centre of mass taken back out; the
// transform component's (boat +0x50) world = M and its world euler, then
// 0x534830 and 0x534C00(0): transform_set_world (world/transform.hxx) on the
// component's image (+0x10 local, +0x50 world, +0x90 / +0xA0 the eulers'
// lanes 0 - 2; their fourth lanes are stack words - left). The boat has no
// parent and no children in a race (0x481CE0's boats; the nuisance boats'
// owners are not their parents - 0x42FDC0): `has_parent` true is the
// parent path on an image, NOT modelled - false returned, nothing written.
inline bool boat_motion_set( const std::uint8_t* boat, std::uint8_t* transform, bool has_parent, const float* basis, const float* origin )
{
    if ( has_parent )
        return false;
    float bt[ 16 ], m[ 16 ], tr[ 16 ], out[ 16 ];
    std::memcpy( bt, basis, 48 );
    std::memcpy( bt + 12, origin, 16 );
    hull_vu_from_bt( m, bt );
    float c[ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        std::memcpy( &c[ k ], boat + 0x290 + 4 * k, 4 );
        c[ k ] = -c[ k ];   // xorps with the sign mask 0x6F7CD0
    }
    hull_translation( tr, c );
    hull_multiply( tr, out, m );
    transform_component_t t{};
    transform_set_world( t, out, nullptr );
    std::memcpy( transform + 0x10, t.local, 64 );
    std::memcpy( transform + 0x50, t.world, 64 );
    std::memcpy( transform + 0x90, t.rotation, 12 );
    std::memcpy( transform + 0xA0, t.world_rotation, 12 );
    return true;
}