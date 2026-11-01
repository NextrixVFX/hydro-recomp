#pragma once

#include <workspace/core/dynamics/manager.hxx>

#include <cstdint>
#include <cstring>
#include <vector>

// VuDynamicsImpl (vtable 0x74C044, g_vudynamicsimpl 0x75900C) - its frame and
// its small slots, recreated from retail's disassembly and verified as call
// sequences (hydro checks/wrapper.hxx, case dynamics_impl). manager.hxx holds
// the registration slots 11 / 12 / 15 / 16 and the own wait slot 25 0x4F8900
// (vu_dynamics_sync); this file the rest of what the race reaches.
//
// The object: +8 the step thread, +0xC async, +0xD a step in flight, +0xE quit,
// +0x10 the step's dt, +0x14 the frame interval (Kick) / the clock (Sync),
// +0x18 the "go" event, +0x1C the "done" event, +0x20 .. +0x30 five Bullet
// objects init 0x4F8050 makes (the collision configuration, dispatcher,
// broadphase, solver, ghost-pair callback), +0x34 the VuDynamicsWorldImpl,
// +0x38 the contact manager (0x574EC0; its +4 / +8 a std::list of contact
// listeners), +0x3C the debug drawer (its +8 a std::list of {mode +8, enabled
// byte +0xC}), +0x40 / +0x44 the body listeners, +0x48 / +0x4C the step
// callbacks, +0x50 max substeps, +0x54 the fixed step.
//
// Every call through a vtable or a global is a hook (vu_dynamics_wrapper_hooks_t):
// the thread system g 0x759098 (platform), the clock g_vumetrosys 0x759000 vt+0x28
// (platform), the world's Bullet slots (the engine's collision/world.hxx and
// dynamics/step.hxx are the recreations: the caller binds them), the contact
// manager's slots, the tick manager / g 0x759024 unregistration, the
// destructors. The dev profiler g 0x7590A0 (tested for null at each use) is
// not modelled: retail's ships null.
struct vu_dynamics_wrapper_t;

struct vu_dynamics_wrapper_hooks_t
{
    void* user = nullptr;
    double ( *clock )( void* user ) = nullptr;                                     // g 0x759000 vt+0x28 (x87 double)
    void ( *priority )( void* user, std::int32_t level ) = nullptr;                // g 0x759098 vt+0x1C
    void ( *close )( void* user, std::uint32_t handle ) = nullptr;                 // vt+0x24
    void ( *end_thread )( void* user ) = nullptr;                                  // vt+0x28
    void ( *signal )( void* user, std::uint32_t handle ) = nullptr;                // vt+0x38
    bool ( *wait )( void* user, std::uint32_t handle, std::int32_t timeout ) = nullptr;   // vt+0x3C
    void ( *step )( void* user, float dt, std::int32_t max_sub_steps, float fixed ) = nullptr;   // world slot 11 stepSimulation
    void ( *add_constraint )( void* user, std::uint32_t constraint, std::uint32_t disable ) = nullptr;   // world slot 12 (0x601890)
    void ( *remove_constraint )( void* user, std::uint32_t constraint ) = nullptr;   // world slot 13 (0x6019A0)
    void ( *sync_motion )( void* user ) = nullptr;                                  // world slot 18 (0x600B00)
    void ( *debug_mode )( void* user, std::uint32_t mode ) = nullptr;               // the drawer's vt+0x28
    void ( *contact_slot0 )( void* user ) = nullptr;                                // the contact manager's slot 0
    void ( *contact_slot1 )( void* user ) = nullptr;                                // slot 1
    const std::uint32_t* ( *contact_record )( void* user, std::uint32_t key ) = nullptr;   // slot 3 (returns a record)
    void ( *query )( void* user, std::uint32_t arg ) = nullptr;                     // 0x57AA40 (thiscall on the argument)
    void ( *callback0 )( void* user, void* callback, float dt, std::uint32_t flag ) = nullptr;   // a step callback's vt+0
    void ( *callback1 )( void* user, void* callback, float dt ) = nullptr;           // vt+4
    void ( *unregister )( void* user ) = nullptr;                                   // g 0x759008 vt+0x28 (this)
    void ( *dev_unregister )( void* user ) = nullptr;                               // g 0x759024 vt+0x20 (this)
    void ( *destroy )( void* user, int offset ) = nullptr;                          // the object at `offset`: slot 0 (1)
    void ( *contact_destroy )( void* user ) = nullptr;                              // 0x575150, then operator delete
};

struct vu_dynamics_wrapper_t
{
    vu_dynamics_t base{};                        // +0xD, +0x1C, +0x40, +0x48 (manager.hxx)
    std::uint32_t thread = 0;                    // +8
    std::uint8_t async = 1;                      // +0xC
    std::uint8_t quit = 0;                       // +0xE
    float dt = 0.0f;                             // +0x10
    float interval = 0.0f;                       // +0x14
    std::uint32_t go = 0;                        // +0x18
    std::int32_t max_sub_steps = 6;              // +0x50
    float fixed_time_step = 0.0166666675f;       // +0x54 (0x3C888889)
    bool objects[ 8 ] = { true, true, true, true, true, true, true, true };   // +0x20 .. +0x3C set
    std::vector< void* > contact_listeners;      // the contact manager's +4 list
    struct debug_entry_t
    {
        std::uint32_t mode;
        bool enabled;
    };
    std::vector< debug_entry_t > debug;          // the drawer's +8 list
    vu_dynamics_wrapper_hooks_t hooks{};
};

// manager.hxx's 0x4F8900 bound to this object: retail calls the wait and then
// clears +0xD whatever it answered (vu_dynamics_sync ignores the answer).
inline void vu_dynamics_wrapper_bind( vu_dynamics_wrapper_t& w )
{
    w.base.hooks.user = &w;
    w.base.hooks.sync = []( void*, vu_dynamics_t& d ) { return vu_dynamics_sync( d ); };
    w.base.hooks.wait = []( void* u, std::uint32_t handle, std::int32_t timeout ) {
        vu_dynamics_wrapper_t& x = *static_cast< vu_dynamics_wrapper_t* >( u );
        if ( x.hooks.wait )
            x.hooks.wait( x.hooks.user, handle, timeout );
        return true;
    };
}

inline void vu_dynamics_own_sync( vu_dynamics_wrapper_t& w )
{
    vu_dynamics_wrapper_bind( w );
    vu_dynamics_sync( w.base );
}

// DynamicsKick 0x4F8A90 (thiscall; dt; retn 4): +0x14 = (float) clock - +0x14
// (cvtpd2ps, then subss), +0xD = 1, +0x10 = dt, signal "go" (+0x18); not async
// (+0xC == 0): own slot 25.
inline void vu_dynamics_kick( vu_dynamics_wrapper_t& w, float dt )
{
    const float now = static_cast< float >( w.hooks.clock ? w.hooks.clock( w.hooks.user ) : 0.0 );
    w.base.pending = 1;
    w.interval = now - w.interval;
    w.dt = dt;
    if ( w.hooks.signal )
        w.hooks.signal( w.hooks.user, w.go );
    if ( w.async == 0 )
        vu_dynamics_own_sync( w );
}

// The step thread 0x4F8950 (thiscall): priority 5; while the wait on "go"
// (+0x18, -1) answers true and +0xE is clear: when +0x10 > 0 (comiss / jbe: a
// NaN skips) world slot 11 stepSimulation( +0x10, +0x50, +0x54 ); signal
// "done" (+0x1C). Then the thread's end (vt+0x28, a tail jump).
inline void vu_dynamics_thread( vu_dynamics_wrapper_t& w )
{
    if ( w.hooks.priority )
        w.hooks.priority( w.hooks.user, 5 );
    while ( w.hooks.wait && w.hooks.wait( w.hooks.user, w.go, -1 ) )
    {
        if ( w.quit != 0 )
            break;
        if ( w.dt > 0.0f && w.hooks.step )
            w.hooks.step( w.hooks.user, w.dt, w.max_sub_steps, w.fixed_time_step );
        if ( w.hooks.signal )
            w.hooks.signal( w.hooks.user, w.base.thread );
    }
    if ( w.hooks.end_thread )
        w.hooks.end_thread( w.hooks.user );
}

// DynamicsSync 0x4F8A20 (thiscall; dt; retn 4): own slot 25; +0x14 = the clock
// (fstp dword); when dt > 0.0 (0x6F7570; comiss / jbe): world slot 18
// synchronizeMotionStates, then the drawer's vt+0x28 with the OR of its
// enabled entries' modes (list order).
inline void vu_dynamics_frame_sync( vu_dynamics_wrapper_t& w, float dt )
{
    vu_dynamics_own_sync( w );
    w.interval = static_cast< float >( w.hooks.clock ? w.hooks.clock( w.hooks.user ) : 0.0 );
    if ( !( dt > 0.0f ) )
        return;
    if ( w.hooks.sync_motion )
        w.hooks.sync_motion( w.hooks.user );
    std::uint32_t mode = 0;
    for ( const vu_dynamics_wrapper_t::debug_entry_t& e : w.debug )
        if ( e.enabled )
            mode |= e.mode;
    if ( w.hooks.debug_mode )
        w.hooks.debug_mode( w.hooks.user, mode );
}

// The substep interface (this + 4; world +0x110): slot 0 0x4F8BC0 (dt, flag;
// retn 8) / slot 1 0x4F8C00 (dt; retn 4) - every step callback (+0x48, list
// order) its vt+0 / vt+4.
inline void vu_dynamics_substep_begin( vu_dynamics_wrapper_t& w, float dt, std::uint32_t flag )
{
    for ( void* c : w.base.step_callbacks )
        if ( w.hooks.callback0 )
            w.hooks.callback0( w.hooks.user, c, dt, flag );
}

inline void vu_dynamics_substep( vu_dynamics_wrapper_t& w, float dt )
{
    for ( void* c : w.base.step_callbacks )
        if ( w.hooks.callback1 )
            w.hooks.callback1( w.hooks.user, c, dt );
}

// Slot 9 0x4F7B70 (thiscall; listener; retn 4): push_back on +0x40 - no wait
// (unlike slot 11); at size 0x15555554 retail throws (not recreated).
inline void vu_dynamics_add_listener( vu_dynamics_wrapper_t& w, void* listener )
{
    w.base.listeners.push_back( listener );
}

// Slot 10 0x4F7BE0: std::list::remove 0x4A44A0 on +0x40 - every node holding it.
inline void vu_dynamics_remove_listener( vu_dynamics_wrapper_t& w, void* listener )
{
    for ( std::size_t i = 0; i < w.base.listeners.size( ); )
        if ( w.base.listeners[ i ] == listener )
            w.base.listeners.erase( w.base.listeners.begin( ) + static_cast< std::ptrdiff_t >( i ) );
        else
            ++i;
}

// Slots 13 0x4F8690 / 14 0x4F86E0: the same two on the contact manager's +4 list.
inline void vu_dynamics_add_contact_listener( vu_dynamics_wrapper_t& w, void* listener )
{
    w.contact_listeners.push_back( listener );
}

inline void vu_dynamics_remove_contact_listener( vu_dynamics_wrapper_t& w, void* listener )
{
    for ( std::size_t i = 0; i < w.contact_listeners.size( ); )
        if ( w.contact_listeners[ i ] == listener )
            w.contact_listeners.erase( w.contact_listeners.begin( ) + static_cast< std::ptrdiff_t >( i ) );
        else
            ++i;
}

// Slots 17 0x4F8840 / 18 0x4F8860: own slot 25, then the world's slot 12
// addConstraint( constraint, disable ) / slot 13 removeConstraint( constraint ).
inline void vu_dynamics_add_constraint( vu_dynamics_wrapper_t& w, std::uint32_t constraint, std::uint32_t disable )
{
    vu_dynamics_own_sync( w );
    if ( w.hooks.add_constraint )
        w.hooks.add_constraint( w.hooks.user, constraint, disable );
}

inline void vu_dynamics_remove_constraint( vu_dynamics_wrapper_t& w, std::uint32_t constraint )
{
    vu_dynamics_own_sync( w );
    if ( w.hooks.remove_constraint )
        w.hooks.remove_constraint( w.hooks.user, constraint );
}

// Slots 19 0x4F8880 / 20 0x4F8890: the contact manager's slot 0 / 1 (tail jumps).
inline void vu_dynamics_contact_slot0( vu_dynamics_wrapper_t& w )
{
    if ( w.hooks.contact_slot0 )
        w.hooks.contact_slot0( w.hooks.user );
}

inline void vu_dynamics_contact_slot1( vu_dynamics_wrapper_t& w )
{
    if ( w.hooks.contact_slot1 )
        w.hooks.contact_slot1( w.hooks.user );
}

// Slots 22 0x4F88B0 / 23 0x4F88D0 (key; retn 4): the contact manager's slot 3
// (key) - a record; 22 answers its +0 as a float (fld), 23 its address + 4.
// Null when the hook is missing.
inline const std::uint32_t* vu_dynamics_contact_record( vu_dynamics_wrapper_t& w, std::uint32_t key )
{
    return w.hooks.contact_record ? w.hooks.contact_record( w.hooks.user, key ) : nullptr;
}

inline float vu_dynamics_contact_value( vu_dynamics_wrapper_t& w, std::uint32_t key )
{
    const std::uint32_t* r = vu_dynamics_contact_record( w, key );
    float v = 0.0f;
    if ( r )
        std::memcpy( &v, r, 4 );
    return v;
}

inline const std::uint32_t* vu_dynamics_contact_tail( vu_dynamics_wrapper_t& w, std::uint32_t key )
{
    const std::uint32_t* r = vu_dynamics_contact_record( w, key );
    return r ? r + 1 : nullptr;
}

// Slot 26 0x4F8920 (arg; retn 4): own slot 25, then 0x57AA40 thiscall on the
// argument (it reads g_vudynamicsimpl's slot 27 - not recreated: the hook).
inline void vu_dynamics_query( vu_dynamics_wrapper_t& w, std::uint32_t arg )
{
    vu_dynamics_own_sync( w );
    if ( w.hooks.query )
        w.hooks.query( w.hooks.user, arg );
}

// Slot 4 0x4F7B50: g_vudynamicsimpl 0x75900C = 0.
inline void vu_dynamics_clear_global( vu_dynamics_wrapper_t*& global )
{
    global = nullptr;
}

// Slot 3 0x4F8520 (release): own slot 25; +0xE = 1 and "go" signalled (the
// thread's quit); the thread handle (+8) closed; the tick manager's and g
// 0x759024's unregistration (this); then each object destroyed and nulled:
// the drawer +0x3C (slot 0 with 1), the contact manager +0x38 (0x575150 +
// operator delete), the world +0x34, +0x30, +0x2C, +0x28, +0x24, +0x20 (slot 0
// with 1, when set).
inline void vu_dynamics_release( vu_dynamics_wrapper_t& w )
{
    vu_dynamics_own_sync( w );
    w.quit = 1;
    if ( w.hooks.signal )
        w.hooks.signal( w.hooks.user, w.go );
    if ( w.hooks.close )
        w.hooks.close( w.hooks.user, w.thread );
    if ( w.hooks.unregister )
        w.hooks.unregister( w.hooks.user );
    if ( w.hooks.dev_unregister )
        w.hooks.dev_unregister( w.hooks.user );
    for ( int offset = 0x3C; offset >= 0x20; offset -= 4 )
    {
        bool& set = w.objects[ ( offset - 0x20 ) / 4 ];
        if ( !set )
            continue;
        if ( offset == 0x38 )
        {
            if ( w.hooks.contact_destroy )
                w.hooks.contact_destroy( w.hooks.user );
        }
        else if ( w.hooks.destroy )
            w.hooks.destroy( w.hooks.user, offset );
        set = false;
    }
}

// ---------------------------------------------------------------------------
// VuDynamicsImpl's create 0x4F7CB0 on retail's memory (a 32-bit image the
// hooks write), verified (hydro checks/wrapper.hxx, case dynamics_create):
// operator new( 0x5C ) - a null answer stores g_vudynamicsimpl 0x75900C = 0 and
// returns 0 -, then 0x4F7D30 (thiscall; one unread stack word; retn 4: +8 and
// +0x18 .. +0x40, +0x48 = 0) and 0x4F7D90 (the ctor proper: vtables +0 0x74C044
// and the substep interface +4 0x74C0BC (0x738070 first), +8 0, word +0xC 0,
// byte +0xE 0, +0x10 0, +0x20 .. +0x2C 0, +0x34 0, the body listeners +0x40 /
// +0x44 and the step callbacks +0x48 / +0x4C = a std::list sentinel each
// (0x4031B0( 0, 0 ): operator new( 0xC ), +0 / +4 self-linked, +8 unwritten -
// a null answer is _Xbad_alloc 0x64C0AC, a throw: the create stops here), byte
// +0x58 0; with the dev system g 0x759020 set: its vt+0x18 "DynamicsInfo" { 50,
// 10, 40, 80 }, "DynamicsProfile" { 5, 5, 90, 90 } (VuRect floats), then the
// tick manager g 0x759008 vt+0x20( a 0x10-byte VuMethod delegate { 0x74BFAC,
// this, 0x4F8AF0 (the dev stats page), 0 } - 0 when its new failed -, "Final" );
// then the thread system g 0x759098 vt+0x30 twice: "go" +0x18, "done" +0x1C);
// g_vudynamicsimpl = the object, returned. NEVER written: +0x14, +0x50, +0x54,
// +0x59 .. +0x5B (init slot 5 0x4F8050 sets +0xC / +0x50 / +0x54 later).
// The dev system, the tick manager and the thread system are hooks (dev /
// platform); the dev page 0x4F8AF0 itself is DEV, not recreated.
struct vu_dynamics_create_hooks_t
{
    void* user = nullptr;
    std::uint32_t ( *allocate )( void* user, std::uint32_t size ) = nullptr;   // operator new 0x64C154 (0 = failed)
    void ( *write )( void* user, std::uint32_t address, const void* bytes, std::uint32_t size ) = nullptr;
    void ( *bad_alloc )( void* user ) = nullptr;                                 // 0x64C0AC _Xbad_alloc (throws)
    bool dev = false;                                                            // g 0x759020 non-null
    void ( *dev_page )( void* user, const char* name, const float rect[ 4 ] ) = nullptr;   // g 0x759020 vt+0x18
    void ( *register_tick )( void* user, std::uint32_t delegate, const char* phase ) = nullptr;   // g 0x759008 vt+0x20
    std::uint32_t ( *create_event )( void* user ) = nullptr;                     // g 0x759098 vt+0x30
};

inline std::uint32_t vu_dynamics_impl_create( std::uint32_t& global, const vu_dynamics_create_hooks_t& h )
{
    const auto w32 = [ & ]( std::uint32_t at, std::uint32_t v ) { h.write( h.user, at, &v, 4 ); };
    const auto w16 = [ & ]( std::uint32_t at, std::uint16_t v ) { h.write( h.user, at, &v, 2 ); };
    const auto w8 = [ & ]( std::uint32_t at, std::uint8_t v ) { h.write( h.user, at, &v, 1 ); };
    const std::uint32_t self = h.allocate( h.user, 0x5C );
    if ( self == 0u )
    {
        global = 0u;
        return 0u;
    }
    // 0x4F7D30
    w32( self + 8, 0u );
    for ( std::uint32_t o = 0x18; o <= 0x40; o += 4 )
        w32( self + o, 0u );
    w32( self + 0x48, 0u );
    // 0x4F7D90
    w32( self + 4, 0x738070u );
    w32( self + 0, 0x74C044u );
    w32( self + 4, 0x74C0BCu );
    w32( self + 8, 0u );
    w16( self + 0xC, 0u );
    w8( self + 0xE, 0u );
    w32( self + 0x10, 0u );
    for ( std::uint32_t o = 0x20; o <= 0x2C; o += 4 )
        w32( self + o, 0u );
    w32( self + 0x34, 0u );
    for ( std::uint32_t o : { 0x40u, 0x48u } )
    {
        w32( self + o, 0u );
        w32( self + o + 4, 0u );
        const std::uint32_t node = h.allocate( h.user, 0xC );
        if ( node == 0u )
        {
            if ( h.bad_alloc )
                h.bad_alloc( h.user );
            return 0u;
        }
        w32( node + 0, node );
        w32( node + 4, node );
        w32( self + o, node );
    }
    w8( self + 0x58, 0u );
    if ( h.dev )
    {
        static const float info[ 4 ] = { 50.0f, 10.0f, 40.0f, 80.0f };
        static const float profile[ 4 ] = { 5.0f, 5.0f, 90.0f, 90.0f };
        if ( h.dev_page )
        {
            h.dev_page( h.user, "DynamicsInfo", info );
            h.dev_page( h.user, "DynamicsProfile", profile );
        }
        const std::uint32_t delegate = h.allocate( h.user, 0x10 );
        if ( delegate != 0u )
        {
            w32( delegate + 0, 0x74BFACu );
            w32( delegate + 4, self );
            w32( delegate + 8, 0x4F8AF0u );
            w32( delegate + 0xC, 0u );
        }
        if ( h.register_tick )
            h.register_tick( h.user, delegate, "Final" );
    }
    w32( self + 0x18, h.create_event ? h.create_event( h.user ) : 0u );
    w32( self + 0x1C, h.create_event ? h.create_event( h.user ) : 0u );
    global = self;
    return self;
}

// The same construction on the struct model: what 0x4F7D30 / 0x4F7D90 leave in
// the fields vu_dynamics_wrapper_t carries (interval, max_sub_steps and
// fixed_time_step untouched - never written there).
inline void vu_dynamics_wrapper_construct( vu_dynamics_wrapper_t& w, std::uint32_t go, std::uint32_t done )
{
    w.thread = 0u;
    w.async = 0u;
    w.base.pending = 0u;
    w.quit = 0u;
    w.dt = 0.0f;
    w.go = go;
    w.base.thread = done;
    for ( bool& o : w.objects )
        o = false;
    w.base.listeners.clear( );
    w.base.step_callbacks.clear( );
}

// ---------------------------------------------------------------------------
// btRigidBody::checkCollideWithOverride 0x50E7B0 (VuRigidBody's slot 0;
// thiscall body; other; retn 4 -> al) - the dispatcher's needsCollision hook
// (collision/dispatch.hxx answers -1 for it): true unless the other is a rigid
// body (its +0xF4 internal type & 2) that one of this body's constraint
// references (+0x1FC a signed count, +0x204 the array) joins - a constraint's
// +0x18 (rbA) or +0x1C (rbB) is the other. Verified: case rigid_collide.
struct rigid_constraint_ref_t
{
    const void* a;   // the constraint's +0x18
    const void* b;   // +0x1C
};

inline bool rigid_check_collide_with( std::uint32_t other_internal_type, const void* other, const rigid_constraint_ref_t* refs, std::int32_t count )
{
    if ( ( other_internal_type & 2u ) == 0u )
        return true;
    for ( std::int32_t i = 0; i < count; ++i )
        if ( refs[ i ].a == other || refs[ i ].b == other )
            return false;
    return true;
}

// ---------------------------------------------------------------------------
// VuCollisionManager (g 0x758FBC; vtables 0x745434 and the contact-listener
// interface 0x74542C at +4). Slot 4 0x4ACD20: the global = 0. Slot 3 0x4AD270
// (release): g_vudynamicsimpl's slot 14 (vu_dynamics_remove_contact_listener)
// with this + 4 (null when this is null), the asset manager's (g 0x759004)
// vt+0x24 release of +0x10, the tick manager's (g 0x759008) vt+0x28 with this.
// Verified: case rigid_collide.
struct vu_collision_manager_hooks_t
{
    void* user = nullptr;
    void ( *remove_contact_listener )( void* user, const void* listener ) = nullptr;
    void ( *asset_release )( void* user, std::uint32_t asset ) = nullptr;
    void ( *unregister )( void* user, const void* manager ) = nullptr;
};

inline void vu_collision_manager_release( const void* manager, std::uint32_t asset, const vu_collision_manager_hooks_t& h )
{
    const void* listener = manager ? static_cast< const unsigned char* >( manager ) + 4 : nullptr;
    if ( h.remove_contact_listener )
        h.remove_contact_listener( h.user, listener );
    if ( h.asset_release )
        h.asset_release( h.user, asset );
    if ( h.unregister )
        h.unregister( h.user, manager );
}

template < typename T >
inline void vu_collision_manager_clear_global( T*& global )
{
    global = nullptr;
}
// ---------------------------------------------------------------------------
// VuSimpleStridingMesh (vtable slot 3 0x527CD0, slot 6 0x527D40; slot 0
// 0x410D50 sets the base vtable 0x738128 and deletes on bit 0; slot 2 0x410D40
// is `retn 0x24`). Its +0x20 / +0x24 point at two {data, count} arrays - the
// vertices and the 16-bit indices. The lock 0x527CD0
// (getLockedReadOnlyVertexIndexBase; the subpart ignored) is VuStridingMesh's
// 0x527C50 (collision/bvh.hxx) on these arrays (census vdiff: only the source
// words differ): PHY_FLOAT (0) vertices, stride 16; PHY_SHORT (3) indices,
// stride 6; faces = the index count / 3 (signed, toward zero). Slot 6
// (getNumSubParts here) = the index count != 0. Verified: case simple_mesh.
struct simple_mesh_lock_t
{
    const void* vertices;
    std::int32_t vertex_count;
    std::int32_t vertex_type;
    std::int32_t vertex_stride;
    const void* indices;
    std::int32_t index_stride;
    std::int32_t faces;
    std::int32_t index_type;
};

inline simple_mesh_lock_t simple_striding_mesh_lock( const void* vertices, std::int32_t vertex_count, const void* indices, std::int32_t index_count )
{
    return { vertices, vertex_count, 0, 16, indices, 6, index_count / 3, 3 };
}

inline bool simple_striding_mesh_parts( std::int32_t index_count )
{
    return index_count != 0;
}