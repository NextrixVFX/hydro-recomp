#pragma once

#include <workspace/core/dynamics/manager.hxx>
#include <workspace/core/collision/compound.hxx>

#include <cstdint>

// VuBoatEntity's methods, recreated from retail's disassembly (vtable
// 0x7383D4, RTTI .?AVVuBoatEntity@@; memory/audit.md "Batch 9e").

// Slot 17 @0x421980 (thiscall boat; ret): once (boat +0x2A0 clear), the
// body (+0x21C) into the dynamics and the boat's +0x60 interface - the one
// tickThunk 0x41E5E0 is called on - onto its step callbacks, then +0x2A0 =
// 1. Retail calls g_vudynamicsimpl (0x75900C) through ITS vtable, slots 15
// and 11: VuDynamicsImpl's (0x74C044) are called here - that the object
// there IS a VuDynamicsImpl is the only class with that vtable, UNPROVEN
// live.
inline bool boat_enable( vu_dynamics_t& d, uint8_t& enabled, vu_rigid_body_t& body, void* tick_interface )
{
    if ( enabled )
        return true;
    if ( !vu_dynamics_add_rigid_body( d, body ) || !vu_dynamics_add_step_callback( d, tick_interface ) )
        return false;
    enabled = 1;
    return true;
}

// What slot 16 @0x421870 reaches: the boat's +0x290 .. +0x298 (and +0x29C -
// see below) and +0x134, its +0x2B0 asset handle, the body, and the body's
// collision shape (+0xCC; `shape` its identity for the hooks, `type` its +4,
// `compound` our model when the type is 31). Every vtable call is a hook:
// a shape's slot 0(1) (the scalar deleting dtor), the body's slot 2(0), and g
// 0x759004's vt+0x24(handle).
struct boat_teardown_hooks_t
{
    void* user = nullptr;
    bool ( *delete_shape )( void* user, const void* shape ) = nullptr;
    bool ( *body_slot2 )( void* user, vu_rigid_body_t& b ) = nullptr;
    bool ( *release )( void* user, uint32_t handle ) = nullptr;
};

struct boat_teardown_t
{
    float* centre;          // boat +0x290: 4 words
    float& f134;
    uint32_t& handle2B0;
    vu_rigid_body_t& body;
    const void* shape;      // body +0xCC; null for none
    int type;               // the shape's +4
    compound_shape_t* compound;
    boat_teardown_hooks_t hooks;
};

// Slot 16 @0x421870 (thiscall boat; ret): +0x290 .. +0x298 = 0 and +0x134 = 0;
// retail fills +0x29C from a stack word it never writes (`movq` of [esp+0x10]
// with only [esp+0x10] stored) - an UNKNOWN, left as it is here. With a body
// shape: a compound (type 31) loses its children first to last-index-0 order
// (each: child 0's shape, if any, through its slot 0(1), then
// removeChildShapeByIndex 0x527030(0)); the shape's slot 0(1); the body's slot
// 2(0). Then g 0x759004's vt+0x24(+0x2B0) and +0x2B0 = 0.
inline bool boat_teardown( boat_teardown_t& t )
{
    for ( int k = 0; k < 3; ++k )
        t.centre[ k ] = 0.0f;
    t.f134 = 0.0f;
    if ( t.shape )
    {
        if ( t.type == 31 )
        {
            if ( !t.compound )
                return false;
            while ( !t.compound->children.empty( ) )
            {
                const void* child = t.compound->children[ 0 ].shape;
                if ( child && ( !t.hooks.delete_shape || !t.hooks.delete_shape( t.hooks.user, child ) ) )
                    return false;
                if ( compound_remove_child( *t.compound, 0 ) < 0 )
                    return false;
            }
        }
        if ( !t.hooks.delete_shape || !t.hooks.delete_shape( t.hooks.user, t.shape ) )
            return false;
        if ( !t.hooks.body_slot2 || !t.hooks.body_slot2( t.hooks.user, t.body ) )
            return false;
    }
    if ( !t.hooks.release || !t.hooks.release( t.hooks.user, t.handle2B0 ) )
        return false;
    t.handle2B0 = 0;
    return true;
}

// Slot 18 @0x421940 (thiscall boat; ret): slot 17 undone - when +0x2A0 is
// set, the dynamics' slots 16 (the body) and 12 (the +0x60 interface), then
// +0x2A0 = 0. The same UNPROVEN class assumption as boat_enable.
inline bool boat_disable( vu_dynamics_t& d, uint8_t& enabled, vu_rigid_body_t& body, void* tick_interface )
{
    if ( !enabled )
        return true;
    if ( !vu_dynamics_remove_rigid_body( d, body ) || !vu_dynamics_remove_step_callback( d, tick_interface ) )
        return false;
    enabled = 0;
    return true;
}
