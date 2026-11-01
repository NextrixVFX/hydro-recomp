#pragma once

// The race session's VuEventManager (g 0x759028; engine entity/broadcast.hxx,
// verified - case event_manager) and the handlers registered into it.
//
// Retail: VuEntity_registerTickMethod 0x5156E0 (the VuEventMap's add) puts a
// VuMethod1< T, void, const VuParams& > into the owner's map AND into the
// manager's subscriber tree under the FNV-1a hash of the event name; the
// VuEventMap dtor 0x515620 takes each one out again (0x5057E0). A broadcast
// (0x505650 by name / 0x505690 by hash) runs every handler of the hash in the
// tree's order - ascending VuMethodInterface1* - inside the call. The engine
// init 0x566230 creates the manager (0x505440 at 0x5663DD) and calls its init
// (slot 5 0x5055C0: the Decision tick 0x505950) at once, before any game
// system exists, so its Decision tick is the phase's first.
//
// SUBSTITUTION, FLAGGED: the tree's key is the handler object's HEAP address
// (operator new 0xC in each ctor); ours is a handle given out in registration
// order (race_bus_subscribe). Two handlers of one hash therefore run in their
// registration order here and in heap order in retail - the same only when
// the heap hands out ascending addresses. A code address alone cannot be the
// key: two instances of one class (every human driver's 0x40F680) share it
// and the set would merge them. Where the order is observable is listed at
// each multi-subscriber hash (world/session.hxx race_session_events): in a
// race only the HUDs' own subscribers (the UI's binding) - UNKNOWN.
//
// Platform hooks: the manager's mutex (g 0x759098 vt+0x40 .. +0x4C: a no-op -
// one thread here) and the tick manager's real delta (tick_real_delta).

#include <workspace/core/entity/broadcast.hxx>
#include <workspace/core/ui/flow.hxx>
#include <workspace/core/world/tick.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct race_bus_handler_t
{
    std::uint32_t method = 0;    // the retail handler's address (what it recreates)
    std::string name;            // the event name it was registered under
    std::string owner;           // ours: who registered it (logs)
    std::function< void( const ui_params_t& ) > fn;
};

struct race_bus_t
{
    event_manager_t manager;
    std::map< std::uint32_t, race_bus_handler_t > handlers;              // handle -> handler
    std::vector< std::pair< std::uint32_t, std::uint32_t > > registrations;   // (hash, handle), registration order
    std::uint32_t next = 0;
    int fired = 0;               // dev counts (not retail)
    int unheard = 0;
    const tick_manager_t* ticks = nullptr;   // the session's tick manager (the real delta)
};

// VuParams (0x84 bytes: 0x80 of records, +0x80 the size) <-> the manager's 33 words.
inline void race_bus_words( const ui_params_t& p, std::uint32_t* words )
{
    std::memcpy( words, p.m_bytes, 128 );
    std::memcpy( words + 32, &p.m_size, 4 );
}

inline ui_params_t race_bus_params( const std::uint32_t* words )
{
    ui_params_t p;
    std::memcpy( p.m_bytes, words, 128 );
    std::memcpy( &p.m_size, words + 32, 4 );
    return p;
}

// 0x5156E0's manager half (event_manager_subscribe = 0x505A90 + 0x5063B0). Answers the handle.
inline std::uint32_t race_bus_subscribe( race_bus_t& b, const char* name, std::uint32_t method, const std::string& owner,
                                         std::function< void( const ui_params_t& ) > fn )
{
    const std::uint32_t handle = ++b.next;   // SUBSTITUTION: the VuMethod's heap address (see the top)
    const std::uint32_t hash = event_hash( name );
    b.handlers[ handle ] = race_bus_handler_t{ method, name, owner, std::move( fn ) };
    b.registrations.push_back( { hash, handle } );
    event_manager_subscribe( b.manager, hash, handle );
    return handle;
}

inline event_manager_hooks_t race_bus_hooks( race_bus_t& b )
{
    event_manager_hooks_t h;
    h.fire = [ &b ]( std::uint32_t handle, const std::uint32_t* words ) {
        const auto it = b.handlers.find( handle );
        if ( it == b.handlers.end( ) || !it->second.fn )
            return;
        ++b.fired;
        it->second.fn( race_bus_params( words ) );   // the handler's vt+8( params )
    };
    h.real_delta = [ &b ] { return b.ticks ? b.ticks->raw : 0.0f; };   // g 0x759008 vt+0x3C (logic.hxx tick_real_delta)
    h.lock = []( bool ) { };                                           // the mutex: platform, one thread here
    return h;
}

// 0x505690: every handler of the hash, inside the call. Answers how many ran.
inline int race_bus_broadcast( race_bus_t& b, std::uint32_t hash, const ui_params_t& p )
{
    const int before = b.fired;
    std::uint32_t words[ 33 ];
    race_bus_words( p, words );
    event_manager_broadcast( b.manager, hash, words, race_bus_hooks( b ) );
    if ( b.fired == before )
        ++b.unheard;
    return b.fired - before;
}

// The manager's Decision tick 0x505950 (its queue: nothing recreated in a race queues - 0x505720's callers are
// VuControlMethodManager's 0x4AEB30 / 0x4AEBE0, 0x4AEC40 and the game's 0x4DAB50 .. 0x4DCC10, none recreated).
inline void race_bus_tick( void* user, float dt )
{
    race_bus_t& b = *static_cast< race_bus_t* >( user );
    event_manager_tick( b.manager, dt, race_bus_hooks( b ) );
}

// The VuEventMap dtors 0x515620 at the owners' deletion: each registration's 0x5057E0 (hash, handler).
inline void race_bus_release( race_bus_t& b )
{
    const event_manager_hooks_t h = race_bus_hooks( b );
    for ( const auto& r : b.registrations )
        event_manager_remove( b.manager, r.first, r.second, h );
    b.registrations.clear( );
    b.handlers.clear( );
}
