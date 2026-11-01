#pragma once

#include <workspace/core/dynamics/body.hxx>

#include <cstdint>
#include <cstring>
#include <vector>

// VuDynamicsImpl's registration methods, recreated from retail's
// disassembly (memory/audit.md "Batch 9e"; vtable 0x74C044, RTTI
// .?AVVuDynamicsImpl@@): slot 25 @0x4F8900 (the wait for an in-flight
// physics step), slot 15 @0x4F8720 (addRigidBody), slot 11 @0x4F7C00 (add a
// step callback). Every call retail makes through a vtable is a hook: the
// object's own slot 25, the world's (+0x34) slot 19 - VuDynamicsWorldImpl's
// is Bullet's addRigidBody 0x6013E0 (collision/world.hxx
// world_add_rigid_body), the thread system's (g 0x759098) vt+0x3C, and each
// listener's slot 0. A hook that is absent or fails returns false.
//
// Retail's std::lists are vectors here, in list order: the listeners at
// +0x40 (each node's +8 object) and the step callbacks at +0x48 (size +0x4C;
// push_back through _Buynode 0x4031F0 / 0x4031B0, 12-byte nodes).

struct vu_dynamics_t;

struct vu_dynamics_hooks_t
{
    void* user = nullptr;
    bool ( *sync )( void* user, vu_dynamics_t& d ) = nullptr;                              // own slot 25 (vt+0x64)
    bool ( *wait )( void* user, uint32_t handle, int32_t timeout ) = nullptr;               // g 0x759098 vt+0x3C
    bool ( *world_add )( void* user, vu_rigid_body_t& b, int32_t group, int32_t mask ) = nullptr;   // world slot 19
    bool ( *listener )( void* user, void* listener, vu_rigid_body_t& b ) = nullptr;           // a listener's slot 0
    // removal (slot 16): the dispatcher (world +0x18) slots 8 / 9, the world's slot 21, a listener's slot 1
    int ( *manifold_count )( void* user ) = nullptr;                                         // -1: not recreated
    bool ( *manifold_bodies )( void* user, int i, vu_rigid_body_t*& b0, vu_rigid_body_t*& b1 ) = nullptr;   // +0x490 / +0x494
    bool ( *world_remove )( void* user, vu_rigid_body_t& b ) = nullptr;
    bool ( *listener_remove )( void* user, void* listener, vu_rigid_body_t& b ) = nullptr;
};

struct vu_dynamics_t
{
    uint8_t pending;                       // +0xD: a step in flight
    uint32_t thread;                       // +0x1C: what the wait is handed
    std::vector< void* > listeners;        // +0x40
    std::vector< void* > step_callbacks;   // +0x48 / +0x4C
    vu_dynamics_hooks_t hooks;
};

// Slot 25 @0x4F8900: with +0xD set, the thread system's vt+0x3C(+0x1C, -1),
// then +0xD = 0 - WHATEVER the wait answered (eax is never read; case
// dynamics_sync). An absent wait hook is the one false: the wait was not made
// (+0xD left as it was).
inline bool vu_dynamics_sync( vu_dynamics_t& d )
{
    if ( d.pending )
    {
        if ( !d.hooks.wait )
            return false;
        ( void )d.hooks.wait( d.hooks.user, d.thread, -1 );
        d.pending = 0;
    }
    return true;
}

// Slot 15 @0x4F8720 (thiscall; body; retn 4): own slot 25; body +0xD8 |= 8;
// the world's slot 19 (body, the u16 at +0x2CA, the u16 at +0x2CC - movzx);
// each listener's slot 0 (body), in list order.
inline bool vu_dynamics_add_rigid_body( vu_dynamics_t& d, vu_rigid_body_t& b )
{
    if ( !d.hooks.sync || !d.hooks.sync( d.hooks.user, d ) )
        return false;
    uint16_t group, mask;
    std::memcpy( &mask, reinterpret_cast< const unsigned char* >( &b ) + 0x2CC, 2 );
    body_word( b, 0xD8 ) |= 8;
    std::memcpy( &group, reinterpret_cast< const unsigned char* >( &b ) + 0x2CA, 2 );
    if ( !d.hooks.world_add || !d.hooks.world_add( d.hooks.user, b, group, mask ) )
        return false;
    for ( void* l : d.listeners )
        if ( !d.hooks.listener || !d.hooks.listener( d.hooks.user, l, b ) )
            return false;
    return true;
}

// A body woken by the removal of one it touches: activation state +0xE4 = 1
// unless 4 / 5 (disable deactivation / simulation), deactivation time +0xE8 = 0.
inline void vu_dynamics_wake( vu_rigid_body_t& b )
{
    const uint32_t state = body_word( b, 0xE4 );
    if ( state != 4 && state != 5 )
        body_word( b, 0xE4 ) = 1;
    body_word( b, 0xE8 ) = 0;
}

// Slot 16 @0x4F8780 (thiscall; body; retn 4): own slot 25; for each of the
// dispatcher's manifolds (slots 8 / 9) the OTHER body of one that touches
// this body is woken - +0x490 first, then +0x494, both tested (a manifold of
// the body with itself wakes it twice); the world's slot 21 (removeRigidBody);
// each listener's slot 1.
inline bool vu_dynamics_remove_rigid_body( vu_dynamics_t& d, vu_rigid_body_t& b )
{
    if ( !d.hooks.sync || !d.hooks.sync( d.hooks.user, d ) )
        return false;
    const int n = d.hooks.manifold_count ? d.hooks.manifold_count( d.hooks.user ) : -1;
    if ( n < 0 )
        return false;
    for ( int i = 0; i < n; ++i )
    {
        vu_rigid_body_t* b0 = nullptr;
        vu_rigid_body_t* b1 = nullptr;
        if ( !d.hooks.manifold_bodies || !d.hooks.manifold_bodies( d.hooks.user, i, b0, b1 ) )
            return false;
        if ( b0 == &b )
            vu_dynamics_wake( *b1 );
        if ( b1 == &b )
            vu_dynamics_wake( *b0 );
    }
    if ( !d.hooks.world_remove || !d.hooks.world_remove( d.hooks.user, b ) )
        return false;
    for ( void* l : d.listeners )
        if ( !d.hooks.listener_remove || !d.hooks.listener_remove( d.hooks.user, l, b ) )
            return false;
    return true;
}

// Slot 12 @0x4F7C70 (thiscall; callback; retn 4): own slot 25, then
// std::list::remove 0x4A44A0 on +0x48 - EVERY node holding the value (copied
// first), in order, the size decremented per node.
inline bool vu_dynamics_remove_step_callback( vu_dynamics_t& d, void* callback )
{
    if ( !d.hooks.sync || !d.hooks.sync( d.hooks.user, d ) )
        return false;
    for ( std::size_t i = 0; i < d.step_callbacks.size( ); )
        if ( d.step_callbacks[ i ] == callback )
            d.step_callbacks.erase( d.step_callbacks.begin( ) + static_cast< std::ptrdiff_t >( i ) );
        else
            ++i;
    return true;
}

// Slot 11 @0x4F7C00 (thiscall; callback; retn 4): own slot 25, then
// push_back; at size 0x15555554 retail throws ("list<T> too long", the
// import at 0x64C0A4) - not recreated.
inline bool vu_dynamics_add_step_callback( vu_dynamics_t& d, void* callback )
{
    if ( !d.hooks.sync || !d.hooks.sync( d.hooks.user, d ) )
        return false;
    if ( d.step_callbacks.size( ) >= 0x15555554u )
        return false;
    d.step_callbacks.push_back( callback );
    return true;
}
