#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <vector>

// VuEventManager (vtable 0x74C484, g 0x759028; 0x34 bytes) - the engine's named
// event broadcast and its delayed queue. Recreated from retail's disassembly:
//
//   0x505440  create: new 0x34, +4 the subscriber map (0x505F30: an MSVC
//             unordered_map< hash, std::set< VuMethodInterface1* > >), +0x24 /
//             +0x28 / +0x2C the queue vector 0, g 0x759028 = it.
//   slot 5 0x5055C0 init: a VuMethod (vtable 0x74C474, this, 0x505950) handed
//           to the tick manager (g 0x759008 vt+0x20) for phase "Decision"
//           (0x6CB3E0); +0x30 = the threading layer's new mutex (g 0x759098
//           vt+0x40); answers true.
//   slot 3 0x505610 release: the tick manager's vt+0x28( this ) (its methods
//           unregistered), then g 0x759098 vt+0x44( +0x30 ). (Its walk of the
//           map's list to the end has no body.)
//   slot 4 0x505430: g 0x759028 = 0.
//   slot 0 0x505520 / 0x505550 dtor: the queue's storage, the map's bucket
//           vector (+0xC) and its list (0x506170: every node's set and the node,
//           then the head) freed; vtable 0x736EE8.
//   0x505690 broadcast( hash, const VuParams& ): the hash's set (0x4F2BA0
//           find), each handler in the set's order (ascending pointer: the
//           tree's compare) - its vt+8( params ), the in-order successor taken
//           AFTER the call.
//   0x505650 broadcast( const char* name, params ): FNV-1a 32 of the name, then
//           0x505690.
//   0x505720 queue( float, bool real_time, const char* name, const VuParams& ):
//           under the mutex (g 0x759098 vt+0x48 / +0x4C) one record appended
//           (0x505BB0 resize n + 1): +0 handler 0, +4 delay 0 - FACT: the first
//           argument is never read, so a queued broadcast always fires on the
//           next tick -, +8 the real-time byte, +0xC the name's FNV-1a, +0x10 the
//           params (0x84 bytes).
//   0x5057E0 remove( hash, handler ): the handler erased from the hash's set
//           (0x4B8360 find, 0x505C70 erase) when both exist; then under the
//           mutex every queued record whose +0 is the handler swapped with the
//           LAST record and the vector shrunk by one (the swapped-in record is
//           examined again; the last one just dropped).
//   0x505950 tick( dt ) ("Decision"): under the mutex, the tick manager's real
//           delta (vt+0x3C) read once; per record in order: delay -= (byte +8 ?
//           real : dt); when !( 0 < delay ) (comiss: NaN keeps it): its +0 ?
//           +0's vt+8( &params ) : broadcast( +0xC, params ), then the record
//           erased in place (the later ones moved down) and the same index
//           examined again; the count is re-read every step.
// SUBSTITUTION: the unordered_map (MSVC hash / list / buckets) is a std::map -
// lookups agree; no iteration over it is observable (release's walk is empty).
// The set is std::set< std::uint32_t > of the handlers' addresses (same order).
// Platform hooks: the mutex (g 0x759098), the tick manager (g 0x759008).
// Verified against retail: hydro checks/logic.hxx verify_event_manager (case
// event_manager: retail's own object made by 0x505440).

struct event_record_t
{
    std::uint32_t handler = 0;          // +0
    float delay = 0.0f;                 // +4
    std::uint8_t real_time = 0;         // +8
    std::uint32_t hash = 0;             // +0xC
    std::uint32_t params[ 33 ] = { };   // +0x10 .. +0x93 (VuParams)
};

struct event_manager_t
{
    std::map< std::uint32_t, std::set< std::uint32_t > > subscribers;   // +4
    std::vector< event_record_t > queue;                                // +0x24
};

struct event_manager_hooks_t
{
    // A handler's vt+8( params ): the handler's address and the params' 33 words. It may call back into the manager.
    std::function< void( std::uint32_t handler, const std::uint32_t* params ) > fire;
    std::function< float( ) > real_delta;                // g 0x759008 vt+0x3C (fld)
    std::function< void( bool locked ) > lock;           // g 0x759098 vt+0x48 / +0x4C on +0x30
};

inline std::uint32_t event_hash( const char* name )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( const unsigned char* p = reinterpret_cast< const unsigned char* >( name ); *p != 0; ++p )
        h = ( h ^ *p ) * 0x1000193u;
    return h;
}

// What registration does to the manager (0x5156E0's second half: 0x505A90 find-or-insert, 0x5063B0 the set insert).
inline void event_manager_subscribe( event_manager_t& m, std::uint32_t hash, std::uint32_t handler )
{
    m.subscribers[ hash ].insert( handler );
}

// 0x505690.
inline void event_manager_broadcast( event_manager_t& m, std::uint32_t hash, const std::uint32_t* params, const event_manager_hooks_t& h )
{
    const auto it = m.subscribers.find( hash );
    if ( it == m.subscribers.end( ) )
        return;
    const std::set< std::uint32_t >& set = it->second;
    for ( auto at = set.begin( ); at != set.end( ); )
    {
        const std::uint32_t handler = *at;
        h.fire( handler, params );
        at = set.upper_bound( handler );   // the successor after the call (the node itself stays: no erase from inside)
    }
}

// 0x505650.
inline void event_manager_broadcast( event_manager_t& m, const char* name, const std::uint32_t* params, const event_manager_hooks_t& h )
{
    event_manager_broadcast( m, event_hash( name ), params, h );
}

// 0x505720 (the float is not read).
inline void event_manager_queue( event_manager_t& m, float, bool real_time, const char* name, const std::uint32_t* params,
                                 const event_manager_hooks_t& h )
{
    const std::uint32_t hash = event_hash( name );
    h.lock( true );
    event_record_t r;
    r.real_time = real_time ? 1 : 0;
    r.hash = hash;
    std::memcpy( r.params, params, sizeof( r.params ) );
    m.queue.push_back( r );
    h.lock( false );
}

// 0x5057E0.
inline void event_manager_remove( event_manager_t& m, std::uint32_t hash, std::uint32_t handler, const event_manager_hooks_t& h )
{
    const auto it = m.subscribers.find( hash );
    if ( it != m.subscribers.end( ) )
        it->second.erase( handler );
    h.lock( true );
    for ( std::size_t i = 0; i < m.queue.size( ); ++i )
        if ( m.queue[ i ].handler == handler )
        {
            if ( i + 1 < m.queue.size( ) )
            {
                std::swap( m.queue[ i ], m.queue.back( ) );
                m.queue.pop_back( );
                --i;
            }
            else
                m.queue.pop_back( );
        }
    h.lock( false );
}

// Slot 5 0x5055C0 (init) and slot 3 0x505610 (release), the platform calls as callbacks: register( method vtable
// 0x74C474, the manager, the tick 0x505950, "Decision" ) = g 0x759008 vt+0x20; create = g 0x759098 vt+0x40 (its answer
// stored at +0x30); unregister( manager ) = g 0x759008 vt+0x28; destroy( mutex ) = g 0x759098 vt+0x44.
inline bool event_manager_init( std::uint32_t self, std::uint32_t& mutex,
                                const std::function< void( std::uint32_t, std::uint32_t, std::uint32_t, const char* ) >& register_tick,
                                const std::function< std::uint32_t( ) >& create )
{
    register_tick( 0x74C474u, self, 0x505950u, "Decision" );
    mutex = create( );
    return true;
}

inline void event_manager_release( std::uint32_t self, std::uint32_t mutex, const std::function< void( std::uint32_t ) >& unregister,
                                   const std::function< void( std::uint32_t ) >& destroy )
{
    unregister( self );
    destroy( mutex );
}

// 0x505950.
inline void event_manager_tick( event_manager_t& m, float dt, const event_manager_hooks_t& h )
{
    h.lock( true );
    const float real = h.real_delta( );
    for ( std::size_t i = 0; i < m.queue.size( ); ++i )
    {
        event_record_t& r = m.queue[ i ];
        r.delay = r.delay - ( r.real_time != 0 ? real : dt );
        if ( 0.0f < r.delay || r.delay != r.delay )
            continue;
        std::uint32_t params[ 33 ];
        std::memcpy( params, r.params, sizeof( params ) );   // retail passes &record +0x10; a copy keeps it stable here
        const std::uint32_t handler = r.handler, hash = r.hash;
        if ( handler != 0 )
            h.fire( handler, params );
        else
            event_manager_broadcast( m, hash, params, h );
        m.queue.erase( m.queue.begin( ) + static_cast< std::ptrdiff_t >( i ) );
        --i;
    }
    h.lock( false );
}
