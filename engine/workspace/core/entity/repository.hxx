#pragma once

#include <cstdint>
#include <set>
#include <string>

// VuEntityRepositoryImpl (vtable 0x74E928, g 0x759080; 0x410 bytes) - the
// engine's entity index the race's lookups go through (g 0x759080 vt+0x18 /
// +0x1C / +0x28 / +0x2C ...). Recreated from retail's disassembly:
//
//   0x552630  create: new 0x410, vtable, +4 the project set (0x4C5040: its
//             head node), +8 its size 0, +0xC the 256 bucket heads zeroed
//             (memset 0x400), +0x40C the entity count 0; g 0x759080 = it.
//   0x552620  release: g 0x759080 = 0.  0x5526F0 slot 0 (deleting dtor): the
//             set's nodes erased (0x4C4B90) and its head freed.
//   slot 6  0x552740 addProject( project ): the set insert (0x4F22B0 /
//           0x4C5070) of the project's name (project +0xC -> +0x18, a VuString).
//   slot 7  0x552790 removeProject( project ): find, then erase (0x4C4CF0).
//   slot 12 0x5528D0 hasProject( const char* ): find != end.
//   slot 8  0x5527D0 addEntity( entity ): FIRST slot 10 on the entity's hash
//           (+0x30; the answer is unused), then the entity pushed on the FRONT
//           of bucket[ hash & 0xFF ]: the old head's +0x54 (prev) = entity,
//           entity +0x58 (next) = the old head, head = entity; ++count. The
//           entity's own +0x54 is NOT written (whatever it held stays).
//   slot 9  0x552810 removeEntity( entity ): the bucket head moves to the
//           entity's next when the entity is the head; prev's next = next;
//           next's prev = prev; the entity's +0x54 / +0x58 = 0; --count. No
//           membership test (an entity in no bucket still decrements).
//   slot 10 0x5528A0 findEntity( hash ): bucket[ hash & 0xFF ], walked by +0x58
//           for +0x30 == hash; 0 when none.
//   slot 11 0x552860 findEntity( const char* name ): FNV-1a 32 (0x811C9DC5,
//           0x1000193) of the text, then slot 10.
// SUBSTITUTION: the project set (an MSVC std::set<VuString> tree) is std::set
// - same membership and order. Slot 13 0x552960 (838 B) is not read here.
// Verified against retail: hydro checks/logic.hxx verify_entity_repository
// (case entity_repository).

// The words an entity lends the repository: +0x30 its hash, +0x54 prev, +0x58
// next. `entity` is the caller's object (what slot 10 answers).
struct entity_link_t
{
    std::uint32_t hash = 0;            // entity +0x30
    entity_link_t* prev = nullptr;     // entity +0x54
    entity_link_t* next = nullptr;     // entity +0x58
    void* entity = nullptr;
};

struct entity_repository_t
{
    std::set< std::string > projects;          // +4 / +8
    entity_link_t* buckets[ 256 ] = { };       // +0xC
    std::uint32_t count = 0;                   // +0x40C
};

// slot 10 0x5528A0.
inline entity_link_t* entity_repository_find( const entity_repository_t& r, std::uint32_t hash )
{
    for ( entity_link_t* e = r.buckets[ hash & 0xFF ]; e != nullptr; e = e->next )
        if ( e->hash == hash )
            return e;
    return nullptr;
}

// slot 11 0x552860: FNV-1a 32 of the name, then slot 10.
inline std::uint32_t entity_repository_hash( const char* name )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( const unsigned char* p = reinterpret_cast< const unsigned char* >( name ); *p != 0; ++p )
        h = ( h ^ *p ) * 0x1000193u;
    return h;
}

inline entity_link_t* entity_repository_find( const entity_repository_t& r, const char* name )
{
    return entity_repository_find( r, entity_repository_hash( name ) );
}

// slot 8 0x5527D0.
inline void entity_repository_add( entity_repository_t& r, entity_link_t& e )
{
    entity_repository_find( r, e.hash );   // retail's vt+0x28 call; its answer is dropped
    entity_link_t*& head = r.buckets[ e.hash & 0xFF ];
    if ( head != nullptr )
        head->prev = &e;
    e.next = head;
    head = &e;
    ++r.count;
}

// slot 9 0x552810.
inline void entity_repository_remove( entity_repository_t& r, entity_link_t& e )
{
    entity_link_t*& head = r.buckets[ e.hash & 0xFF ];
    if ( head == &e )
        head = e.next;
    if ( e.prev != nullptr )
        e.prev->next = e.next;
    if ( e.next != nullptr )
        e.next->prev = e.prev;
    e.prev = nullptr;
    e.next = nullptr;
    --r.count;
}

// slots 6 / 7 / 12 (0x552740 / 0x552790 / 0x5528D0). removeProject on a name
// not in the set erases end() in retail (0x4C4CF0 on the head: undefined) -
// answered false here, nothing erased: UNKNOWN for that path.
inline void entity_repository_add_project( entity_repository_t& r, const std::string& name )
{
    r.projects.insert( name );
}

inline bool entity_repository_remove_project( entity_repository_t& r, const std::string& name )
{
    return r.projects.erase( name ) != 0;
}

inline bool entity_repository_has_project( const entity_repository_t& r, const std::string& name )
{
    return r.projects.count( name ) != 0;
}

// ---------------------------------------------------------------------------
// The entity framework's two base destructors, on retail's own memory layout
// (M: any word store with std::uint32_t& operator[]( address ) - pfx_heap_t
// fits). Every call out (operator delete, a virtual, the managers) is a hook;
// the memory writes are retail's. Verified against retail: hydro
// checks/logic.hxx verify_entity_dtors (case entity_dtors).
#include <functional>

struct vu_dtor_hooks_t
{
    std::function< void( std::uint32_t block ) > free;                                       // operator delete 0x64C150
    // A virtual call: object, vtable byte offset, argument (-1 = none). It may write memory (a weak reference's
    // slot 0 unlinks itself from its target's +8 list - retail relies on that to end the loop).
    std::function< void( std::uint32_t object, std::uint32_t slot, std::int64_t arg ) > call;
    std::function< void( std::uint32_t asset ) > asset_release;                              // g 0x759004 vt+0x24
    std::function< void( std::uint32_t key, std::uint32_t value ) > event_remove;            // g 0x759028: 0x5057E0
};

template < typename M >
inline void vu_set_byte( M& m, std::uint32_t a, std::uint8_t v )
{
    std::uint32_t& w = m[ a & ~3u ];
    const unsigned shift = 8u * ( a & 3u );
    w = ( w & ~( 0xFFu << shift ) ) | ( static_cast< std::uint32_t >( v ) << shift );
}

template < typename M >
inline std::uint8_t vu_byte( M& m, std::uint32_t a )
{
    return static_cast< std::uint8_t >( m[ a & ~3u ] >> ( 8u * ( a & 3u ) ) );
}

// A VuString at s emptied (its heap buffer freed when the capacity s + 0x14 is
// 16 or more; capacity 15, size 0, the first byte 0).
template < typename M >
inline void vu_string_tidy( M& m, std::uint32_t s, const vu_dtor_hooks_t& h )
{
    if ( m[ s + 0x14 ] >= 0x10 )
        h.free( m[ s ] );
    m[ s + 0x14 ] = 0xF;
    m[ s + 0x10 ] = 0;
    vu_set_byte( m, s, 0 );
}

// 0x405260 - the property base's dtor (VuProperty: vtable 0x7376E4 after):
// the default-value string +0x2C and the name +0x14 emptied (in that order),
// then +0xC's slot 0 with 1 (its owned watcher, deleted) when non-zero.
template < typename M >
inline void vu_property_dtor( M& m, std::uint32_t p, const vu_dtor_hooks_t& h )
{
    vu_string_tidy( m, p + 0x2C, h );
    vu_string_tidy( m, p + 0x14, h );
    m[ p ] = 0x7376E4;
    if ( const std::uint32_t w = m[ p + 0xC ] )
        h.call( w, 0, 1 );
}

// 0x500E00 (thiscall parent, child): the child found in the parent's +0x38
// vector -> parent vt+0x28( child ), child +0x34 = 0, the vector closed over it
// (memmove 0x64C144), true; not found: false.
template < typename M >
inline bool vu_entity_remove_child( M& m, std::uint32_t parent, std::uint32_t child, const vu_dtor_hooks_t& h )
{
    std::uint32_t at = m[ parent + 0x38 ];
    const std::uint32_t end = m[ parent + 0x3C ];
    while ( at != end && m[ at ] != child )
        at += 4;
    if ( at == end )
        return false;
    h.call( parent, 0x28, child );
    m[ child + 0x34 ] = 0;
    const std::uint32_t stop = m[ parent + 0x3C ];
    for ( std::uint32_t q = at; q + 4 < stop; q += 4 )
        m[ q ] = m[ q + 4 ];
    m[ parent + 0x3C ] = stop - 4;
    return true;
}

// 0x500870 - VuEntity's dtor (thiscall entity; vtable 0x74C234 first):
//   1. the parent (+0x34): 0x500E00( parent, this );
//   2. the template asset (+0x4C): the asset manager's release (hook);
//   3. 0x500DB0: each child (+0x38 vector, by index, the size re-read): its +0x34 = 0, --refcount +4, at 0 its slot 0
//      with 1; then the vector's end = begin;
//   4. the components (+0x48, next +0xC - the head moved on BEFORE each call): vt+8 with 1;
//   5. the properties (+0x44, next +0x10, same order rule): slot 0 with 1;
//   6. the children vector's storage freed, +0x38 / +0x3C / +0x40 = 0;
//   7. the name VuString +0x18 emptied;
//   8. 0x515620 on the event map +0xC (MSVC tree: head +0xC, size +0x10; node left +0 parent +4 right +8, isnil +0xD,
//      key +0x10, value +0x14): in order, the event manager's removal (key, value) then the value's slot 0 with 1;
//      then every node freed (0x43CD80: right subtree first, then the node, then its left), head re-linked, size 0;
//      0x43CA10 on the emptied tree (0x43CD30: nothing left), then the head freed;
//   9. vtable 0x7458B8; while +8 (the weak-reference list) is non-zero: its first's slot 0 (no argument) - which must
//      unlink it; vtable 0x7458C0.
template < typename M >
inline void vu_tree_erase( M& m, std::uint32_t node, const vu_dtor_hooks_t& h )
{
    while ( vu_byte( m, node + 0xD ) == 0 )
    {
        vu_tree_erase( m, m[ node + 8 ], h );
        const std::uint32_t left = m[ node ];
        h.free( node );
        node = left;
    }
}

template < typename M >
inline void vu_entity_dtor( M& m, std::uint32_t e, const vu_dtor_hooks_t& h )
{
    m[ e ] = 0x74C234;
    if ( const std::uint32_t parent = m[ e + 0x34 ] )
        vu_entity_remove_child( m, parent, e, h );
    if ( const std::uint32_t asset = m[ e + 0x4C ] )
        h.asset_release( asset );
    for ( std::uint32_t i = 0; static_cast< std::int32_t >( i ) < static_cast< std::int32_t >( ( m[ e + 0x3C ] - m[ e + 0x38 ] ) ) >> 2; ++i )
    {
        const std::uint32_t child = m[ m[ e + 0x38 ] + 4 * i ];
        m[ child + 0x34 ] = 0;
        if ( --m[ m[ m[ e + 0x38 ] + 4 * i ] + 4 ] == 0 )
            h.call( m[ m[ e + 0x38 ] + 4 * i ], 0, 1 );
    }
    m[ e + 0x3C ] = m[ e + 0x38 ];
    while ( const std::uint32_t c = m[ e + 0x48 ] )
    {
        m[ e + 0x48 ] = m[ c + 0xC ];
        h.call( c, 8, 1 );
    }
    while ( const std::uint32_t p = m[ e + 0x44 ] )
    {
        m[ e + 0x44 ] = m[ p + 0x10 ];
        h.call( p, 0, 1 );
    }
    if ( const std::uint32_t kids = m[ e + 0x38 ] )
    {
        h.free( kids );
        m[ e + 0x38 ] = 0, m[ e + 0x3C ] = 0, m[ e + 0x40 ] = 0;
    }
    vu_string_tidy( m, e + 0x18, h );
    // 0x515620
    const std::uint32_t head = m[ e + 0xC ];
    for ( std::uint32_t n = m[ head ]; n != m[ e + 0xC ]; )
    {
        h.event_remove( m[ n + 0x10 ], m[ n + 0x14 ] );
        if ( const std::uint32_t value = m[ n + 0x14 ] )
            h.call( value, 0, 1 );
        if ( vu_byte( m, n + 0xD ) != 0 )
            continue;
        std::uint32_t r = m[ n + 8 ];
        if ( vu_byte( m, r + 0xD ) == 0 )
        {
            n = r;
            while ( vu_byte( m, m[ n ] + 0xD ) == 0 )
                n = m[ n ];
        }
        else
        {
            std::uint32_t q = m[ n + 4 ];
            while ( vu_byte( m, q + 0xD ) == 0 && n == m[ q + 8 ] )
                n = q, q = m[ q + 4 ];
            n = q;
        }
    }
    vu_tree_erase( m, m[ head + 4 ], h );
    m[ head + 4 ] = head, m[ head ] = head, m[ head + 8 ] = head;
    m[ e + 0x10 ] = 0;
    // 0x43CA10( begin = head's left, end = head ) -> 0x43CD30 on the empty tree: the root is the head (nil) - the same
    // re-link, size 0 again.
    vu_tree_erase( m, m[ head + 4 ], h );
    m[ head + 4 ] = head, m[ head ] = head, m[ head + 8 ] = head;
    m[ e + 0x10 ] = 0;
    h.free( m[ e + 0xC ] );
    m[ e ] = 0x7458B8;
    while ( const std::uint32_t ref = m[ e + 8 ] )
        h.call( ref, 0, -1 );
    m[ e ] = 0x7458C0;
}