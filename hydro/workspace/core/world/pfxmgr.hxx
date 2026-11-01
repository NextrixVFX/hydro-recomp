#pragma once

#include <workspace/core/world/pfxload.hxx>

#include <cstdint>
#include <cstring>
#include <vector>

// VuPfxManager (g_vupfxmanager; the factory 0x52B700 -> ctor 0x52B750, 0x30
// bytes): the game's pool of fire-and-forget effects. Verified: case
// pfx_manager.
//   +0x04 the capacity (64), +0x08 the handle table (entry per index), +0x0C
//   the index mask, +0x10 the index bits, +0x14 the serial cap; +0x18 / +0x1C /
//   +0x20 the free entries (first, last, count), +0x24 / +0x28 / +0x2C the
//   active ones; an entry's links +0x5C next, +0x60 previous.
// An entry is a VuPfxEntity (0x552D50, 0x74 bytes): VuEntity 0x500710 with
// flags +0x14 = 8 (no repository), its one component a Vu3dDrawComponent
// 0x531A40 at +0x64 (+0x1C the "Reflect" byte, +0x1D 1, +0x1E registered with
// the 3D draw manager, +0x20 0, +0x24 .. +0x43 its AABB), +0x68 its system
// instance, +0x6C its index, +0x70 its serial.
// HOOKS (the caller's, through pfx_manager_ops_t): VuPfxImpl's
// createSystemInstance (slot 10, by path) and release (slot 11), the system
// instance's slots 13 tick / 8 count / 7 AABB / 6 state, the 3D draw manager
// g 0x7590B8 (+0x28 add, +0x2C remove, +0x30 update: the renderer), the tick
// manager's "Final" registration (g 0x759008 vt+0x20) - call pfx_manager_tick
// in the "Final" phase.
// UNKNOWN: the draw component's +0x30 / +0x40 before its first AABB copy
// (0x531A40 stores two qwords whose high dwords are uninitialised locals).
struct pfx_manager_entry_t
{
    std::uint32_t flags = 8;
    int next = -1, prev = -1;
    std::uint32_t instance = 0;
    std::uint32_t index = 0, serial = 1;
    std::uint8_t reflect = 0, b1d = 1, registered = 0, b1f = 0;
    std::uint32_t w20 = 0;
    std::uint32_t aabb[ 8 ]{};
    bool aabb_known = false;   // false: +0x30 / +0x40 UNKNOWN
};

struct pfx_manager_t
{
    std::uint32_t capacity = 64, mask = 0, bits = 0, serial_cap = 0;
    int free_first = -1, free_last = -1;
    std::uint32_t free_count = 0;
    int active_first = -1, active_last = -1;
    std::uint32_t active_count = 0;
    std::vector< int > table;
    std::vector< pfx_manager_entry_t > entries;
};

struct pfx_manager_ops_t
{
    void* user = nullptr;
    std::uint32_t ( *create )( void* user, const char* path ) = nullptr;   // g_vupfximpl slot 10 (0: none)
    void ( *release )( void* user, std::uint32_t instance ) = nullptr;     // slot 11
    void ( *tick )( void* user, std::uint32_t instance, float dt ) = nullptr;   // instance slot 13
    std::uint32_t ( *state )( void* user, std::uint32_t instance ) = nullptr;   // slot 6
    std::uint32_t ( *count )( void* user, std::uint32_t instance ) = nullptr;   // slot 8
    const std::uint32_t* ( *aabb )( void* user, std::uint32_t instance ) = nullptr;   // slot 7: 8 words
    void ( *draw_add )( void* user, int entry ) = nullptr;
    void ( *draw_remove )( void* user, int entry ) = nullptr;
    void ( *draw_update )( void* user, int entry ) = nullptr;
    void ( *register_tick )( void* user ) = nullptr;
};

// The two list shapes' operations on entry indices (links next / prev).
inline void pfx_manager_unlink( pfx_manager_t& m, int& first, int& last, std::uint32_t& count, int e )
{
    pfx_manager_entry_t& x = m.entries[ e ];
    const int n = x.next, p = x.prev;
    if ( e == first )
        first = n;
    if ( e == last )
        last = p;
    if ( n >= 0 )
        m.entries[ n ].prev = p;
    if ( p >= 0 )
        m.entries[ p ].next = n;
    x.next = -1;
    x.prev = -1;
    --count;
}

inline void pfx_manager_append( pfx_manager_t& m, int& first, int& last, std::uint32_t& count, int e )
{
    pfx_manager_entry_t& x = m.entries[ e ];
    x.next = -1;
    if ( last >= 0 )
    {
        x.prev = last;
        m.entries[ last ].next = e;
    }
    else
    {
        x.prev = -1;
        first = e;
    }
    ++count;
    last = e;
}

// 0x52B700 / 0x52B750 + init 0x52B7B0: 0x52B890( 64 ) on the fresh manager -
// 64 entries appended to the free list, the table, then the mask (the
// capacity - 1 with every lower bit set), its bit count, the serial cap
// 0xFFFFFFFF >> bits; table[i] = the i-th free entry, its index i, serial 1.
// (0x52B890's shrink branch - a capacity below the entry count: entries
// released through their refcount - is unreached from init: not recreated.)
// Then the "Final" tick registration (hook).
inline void pfx_manager_init( pfx_manager_t& m, const pfx_manager_ops_t& ops, std::uint32_t capacity = 64 )
{
    m = pfx_manager_t( );
    m.capacity = capacity;
    while ( m.free_count < m.capacity )
    {
        m.entries.emplace_back( );
        pfx_manager_append( m, m.free_first, m.free_last, m.free_count, static_cast< int >( m.entries.size( ) ) - 1 );
    }
    std::uint32_t v = m.capacity - 1;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    m.mask = v;
    std::uint32_t c = v - ( ( v >> 1 ) & 0x55555555u );
    c = ( c & 0x33333333u ) + ( ( c >> 2 ) & 0x33333333u );
    m.bits = ( 0x01010101u * ( ( c + ( c >> 4 ) ) & 0x0F0F0F0Fu ) ) >> 24;
    m.serial_cap = 0xFFFFFFFFu >> m.bits;
    m.table.assign( m.free_count, -1 );
    int e = m.free_first;
    for ( std::uint32_t i = 0; i < m.free_count; ++i )
    {
        m.table[ i ] = e;
        m.entries[ e ].index = i;
        m.entries[ e ].serial = 1;
        e = m.entries[ e ].next;
    }
    if ( ops.register_tick != nullptr )
        ops.register_tick( ops.user );
}

// VuEntity::gameInitialize 0x500B10 / gameRelease 0x500B80 on an entry (flags
// 8: no repository; the draw component's slots 9 / 10 are nops 0x401560; no
// children): VuPfxEntity slot 6 0x552EC0 "Reflect" = 0; slot 7 0x552ED0 the
// draw manager's remove when registered.
inline void pfx_manager_game_initialize( pfx_manager_entry_t& x )
{
    if ( x.flags & 4u )
        return;
    x.reflect = 0;
    x.flags |= 4u;
}

inline void pfx_manager_game_release( pfx_manager_t& m, int e, const pfx_manager_ops_t& ops )
{
    pfx_manager_entry_t& x = m.entries[ e ];
    if ( !( x.flags & 4u ) )
        return;
    if ( x.registered )
    {
        if ( ops.draw_remove != nullptr )
            ops.draw_remove( ops.user, e );
        x.registered = 0;
    }
    x.flags &= ~4u;
}

// createEffect 0x52BCD0: -1 with no free entry or no instance; else the free
// list's LAST entry, +0x68 = the instance, gameInitialize, the active append
// 0x52BDE0. Returns the entry.
inline int pfx_manager_create( pfx_manager_t& m, const char* path, const pfx_manager_ops_t& ops )
{
    if ( m.free_count == 0 )
        return -1;
    const std::uint32_t inst = ops.create != nullptr ? ops.create( ops.user, path ) : 0u;
    if ( inst == 0u )
        return -1;
    const int e = m.free_last;
    pfx_manager_unlink( m, m.free_first, m.free_last, m.free_count, e );
    m.entries[ e ].instance = inst;
    pfx_manager_game_initialize( m.entries[ e ] );
    pfx_manager_append( m, m.active_first, m.active_last, m.active_count, e );
    return e;
}

// 0x52BD50: off the active list, gameRelease, VuPfxImpl's release of +0x68
// (left in the entry), onto the free list's end.
inline void pfx_manager_release( pfx_manager_t& m, int e, const pfx_manager_ops_t& ops )
{
    pfx_manager_unlink( m, m.active_first, m.active_last, m.active_count, e );
    pfx_manager_game_release( m, e, ops );
    if ( ops.release != nullptr )
        ops.release( ops.user, m.entries[ e ].instance );
    pfx_manager_append( m, m.free_first, m.free_last, m.free_count, e );
}

// 0x52BB60 releaseAll: the active list's LAST entry each time, as 0x52BD50.
inline void pfx_manager_release_all( pfx_manager_t& m, const pfx_manager_ops_t& ops )
{
    while ( m.active_count != 0 )
        pfx_manager_release( m, m.active_last, ops );
}

// 0x52BC30 one entry: the instance's tick; with particles (slot 8) the draw
// manager's add once (registered), the AABB (slot 7, 8 words) copied to the
// component, the update; without, the remove when registered.
inline void pfx_manager_entry_tick( pfx_manager_t& m, int e, float dt, const pfx_manager_ops_t& ops )
{
    pfx_manager_entry_t& x = m.entries[ e ];
    ops.tick( ops.user, x.instance, dt );
    if ( ops.count( ops.user, x.instance ) != 0u )
    {
        if ( !x.registered )
        {
            if ( ops.draw_add != nullptr )
                ops.draw_add( ops.user, e );
            x.registered = 1;
        }
        std::memcpy( x.aabb, ops.aabb( ops.user, x.instance ), sizeof( x.aabb ) );
        x.aabb_known = true;
        if ( x.registered && ops.draw_update != nullptr )
            ops.draw_update( ops.user, e );
    }
    else if ( x.registered )
    {
        if ( ops.draw_remove != nullptr )
            ops.draw_remove( ops.user, e );
        x.registered = 0;
    }
}

// The "Final" tick 0x52BBF0: per active entry (the next taken first) 0x52BC30,
// then 0x52BD50 when its instance's state (slot 6) is 0.
inline void pfx_manager_tick( pfx_manager_t& m, float dt, const pfx_manager_ops_t& ops )
{
    for ( int e = m.active_first; e >= 0; )
    {
        const int next = m.entries[ e ].next;
        pfx_manager_entry_tick( m, e, dt, ops );
        if ( ops.state( ops.user, m.entries[ e ].instance ) == 0u )
            pfx_manager_release( m, e, ops );
        e = next;
    }
}

// The handles the game's callers keep (read from 0x43C5E0 / 0x43C6A0, the
// boat's): a new one bumps the entry's serial (past the cap: 1) and is
// index + (serial << bits); a lookup takes the entry at handle & mask (below
// the capacity) when it is initialised (flags & 4) and its serial is
// handle >> bits, else -1.
inline std::uint32_t pfx_manager_handle( pfx_manager_t& m, int e )
{
    if ( e < 0 )
        return 0u;
    pfx_manager_entry_t& x = m.entries[ e ];
    std::uint32_t s = x.serial + 1u;
    if ( s > m.serial_cap )
        s = 1u;
    x.serial = s;
    return x.index + ( s << m.bits );
}

inline int pfx_manager_lookup( const pfx_manager_t& m, std::uint32_t handle )
{
    const std::uint32_t i = handle & m.mask;
    if ( i >= m.capacity )
        return -1;
    const int e = m.table[ i ];
    const pfx_manager_entry_t& x = m.entries[ e ];
    return ( x.flags & 4u ) && x.serial == ( handle >> m.bits ) ? e : -1;
}

// The ops on the recreated runtime (pfxsim.hxx / pfxload.hxx): instances by
// path from `lib`, released through pfx_system_release, ticked by
// pfx_system_tick (the caller sets rt.frame's cameras first), the state /
// count / AABB words +0x7C / +0xA0 / +0x80. The draw manager and tick
// registration stay the caller's.
struct pfx_manager_binding_t
{
    pfx_runtime_t* rt = nullptr;
    const pfx_library_t* lib = nullptr;
};

inline pfx_manager_ops_t pfx_manager_runtime_ops( pfx_manager_binding_t& b )
{
    pfx_manager_ops_t o;
    o.user = &b;
    o.create = []( void* u, const char* path ) {
        pfx_manager_binding_t& x = *static_cast< pfx_manager_binding_t* >( u );
        return pfx_runtime_create_path( *x.rt, *x.lib, path );
    };
    o.release = []( void* u, std::uint32_t inst ) {
        pfx_runtime_t& rt = *static_cast< pfx_manager_binding_t* >( u )->rt;
        pfx_system_release( rt.heap, rt.resources, inst );
    };
    o.tick = []( void* u, std::uint32_t inst, float dt ) {
        pfx_runtime_t& rt = *static_cast< pfx_manager_binding_t* >( u )->rt;
        pfx_system_tick( rt.heap, inst, dt, rt.hooks );
    };
    o.state = []( void* u, std::uint32_t inst ) { return static_cast< pfx_manager_binding_t* >( u )->rt->heap[ inst + 0x7C ]; };
    o.count = []( void* u, std::uint32_t inst ) { return static_cast< pfx_manager_binding_t* >( u )->rt->heap[ inst + 0xA0 ]; };
    o.aabb = []( void* u, std::uint32_t inst ) -> const std::uint32_t* {
        return static_cast< pfx_manager_binding_t* >( u )->rt->heap.find( inst + 0x80 );
    };
    return o;
}
