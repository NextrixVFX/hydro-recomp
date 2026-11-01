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
// - same membership and order.
//   slot 13 0x552960 ( VuJsonContainer& out ): entity_repository_tree below.
// Verified against retail: hydro checks/logic.hxx verify_entity_repository
// (case entity_repository), verify_repository_tree (case repository_tree).

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

// slot 13 0x552960 ( VuJsonContainer& out ): every registered entity - the
// buckets 0 .. 255, each from its head by +0x58 (the next read after the
// entity is written) - as a tree of its long name's segments. Per entity:
// path = its long name (0x500BF0: the ancestors' names and its own, root
// first, "/"-joined); from `out`, while path != its name (entity +0x18, a
// whole-string compare): the container is made an object (inline 0x4FBB10 +
// tag 6 + a fresh map) and its member [ the first segment ] (0x564940: before
// the first '/', or the whole path) taken (0x4FCAB0, a null one inserted),
// path = the rest (0x5649C0: after the first '/', or the whole path); then
// the same make-object and member [ path ] with the answer dropped - so the
// entity itself is a null member unless something already sits there. `out`
// is not cleared first; an empty repository leaves it as it was.
// names( link, name, long_name ) gives the entity's +0x18 and 0x500BF0. A
// path with no '/' that is not the name loops for ever in retail (never so
// for a real long name, which ends in "/" + the name): returned here, UNKNOWN.
#include <workspace/core/entity/json.hxx>

template < typename N >
inline void entity_repository_tree( const entity_repository_t& r, value_t& out, N names )
{
    for ( std::uint32_t b = 0; b < 256; ++b )
        for ( const entity_link_t* e = r.buckets[ b ]; e != nullptr; e = e->next )
        {
            std::string name, path;
            names( *e, name, path );
            value_t* j = &out;
            while ( path != name )
            {
                const std::size_t slash = path.find( '/' );
                if ( slash == std::string::npos )
                    return;   // retail: no end
                j = &json_member( *j, path.substr( 0, slash ) );
                path = path.substr( slash + 1 );
            }
            json_member( *j, path );
        }
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
    // batch 28 (the framework dtors below):
    std::function< void( std::uint32_t container ) > json_clear;                             // 0x4FBB10 on retail memory
    std::function< void( std::uint32_t component ) > instigator_remove;                      // g 0x759064: 0x5362A0
    std::function< void( std::uint32_t block ) > aligned_free;                               // _aligned_free 0x64C15C
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

// ---------------------------------------------------------------------------
// Batch 28: the component / plug / ref / project destructors on retail's memory (same conventions as above). Verified
// against retail: hydro checks/logic.hxx verify_framework_dtors (case framework_dtors). A retail loop that cannot end
// (a list whose first element the step does not remove) answers false here: UNKNOWN (retail spins for ever).

// VuRefObj's tail (every VuRefObj dtor ends so): vtable 0x7458B8; while the weak-reference list +8 is non-zero, its
// first's slot 0 (no argument - it unlinks itself); vtable 0x7458C0.
template < typename M >
inline void vu_refobj_tail( M& m, std::uint32_t o, const vu_dtor_hooks_t& h )
{
    m[ o ] = 0x7458B8;
    while ( const std::uint32_t ref = m[ o + 8 ] )
        h.call( ref, 0, -1 );
    m[ o ] = 0x7458C0;
}

// The first `value` in the pointer vector at v (begin v, end v + 4) erased (memmove 0x64C144 of the words after it,
// end -= 4); false when absent. The count is the signed (end - begin) >> 2.
template < typename M >
inline bool vu_vector_erase( M& m, std::uint32_t v, std::uint32_t value )
{
    const std::uint32_t begin = m[ v ], end = m[ v + 4 ];
    const std::int32_t n = static_cast< std::int32_t >( end - begin ) >> 2;
    for ( std::int32_t i = 0; i < n; ++i )
        if ( m[ begin + 4u * static_cast< std::uint32_t >( i ) ] == value )
        {
            const std::uint32_t at = begin + 4u * static_cast< std::uint32_t >( i );
            for ( std::uint32_t q = at; q + 4 < end; q += 4 )
                m[ q ] = m[ q + 4 ];
            m[ v + 4 ] = end - 4;
            return true;
        }
    return false;
}

// A pointer vector's storage freed when begin is non-zero, then begin / end / capacity = 0.
template < typename M >
inline void vu_vector_free( M& m, std::uint32_t v, const vu_dtor_hooks_t& h )
{
    if ( const std::uint32_t begin = m[ v ] )
    {
        h.free( begin );
        m[ v ] = 0, m[ v + 4 ] = 0, m[ v + 8 ] = 0;
    }
}

// VuComponent's dtor body (inline in every component dtor; vtable 0x738CE0 after): the properties (+4, next +0x10 -
// the head moved on BEFORE each call) slot 0 with 1.
template < typename M >
inline void vu_component_base_dtor( M& m, std::uint32_t c, const vu_dtor_hooks_t& h )
{
    m[ c ] = 0x738CE0;
    while ( const std::uint32_t p = m[ c + 4 ] )
    {
        m[ c + 4 ] = m[ p + 0x10 ];
        h.call( p, 0, 1 );
    }
}

// An owned object's slot 0 with 1 when the word at a is non-zero.
template < typename M >
inline void vu_delete_owned( M& m, std::uint32_t a, const vu_dtor_hooks_t& h )
{
    if ( const std::uint32_t o = m[ a ] )
        h.call( o, 0, 1 );
}

// 0x539610 Vu2dLayoutComponent (vtable 0x738A78): +0x10 owned. 0x50B990 Vu3dLayoutComponent (0x74C550): +0x10, +0x14.
// 0x5342B0 VuTransformComponent (0x74D324): +0xC0. 0x531DC0 VuInstigatorComponent (0x74D29C): when byte +0x28, the
// instigator manager's removal (g 0x759064 0x5362A0: hook), byte +0x28 = 0. Each then VuComponent's body.
template < typename M >
inline void vu_2d_layout_dtor( M& m, std::uint32_t c, const vu_dtor_hooks_t& h )
{
    m[ c ] = 0x738A78;
    vu_delete_owned( m, c + 0x10, h );
    vu_component_base_dtor( m, c, h );
}

template < typename M >
inline void vu_3d_layout_dtor( M& m, std::uint32_t c, const vu_dtor_hooks_t& h )
{
    m[ c ] = 0x74C550;
    vu_delete_owned( m, c + 0x10, h );
    vu_delete_owned( m, c + 0x14, h );
    vu_component_base_dtor( m, c, h );
}

template < typename M >
inline void vu_transform_component_dtor( M& m, std::uint32_t c, const vu_dtor_hooks_t& h )
{
    m[ c ] = 0x74D324;
    vu_delete_owned( m, c + 0xC0, h );
    vu_component_base_dtor( m, c, h );
}

template < typename M >
inline void vu_instigator_dtor( M& m, std::uint32_t c, const vu_dtor_hooks_t& h )
{
    m[ c ] = 0x74D29C;
    if ( vu_byte( m, c + 0x28 ) != 0 )
        h.instigator_remove( c );
    vu_set_byte( m, c + 0x28, 0 );
    vu_component_base_dtor( m, c, h );
}

// 0x503BA0 VuScriptComponent (vtable 0x74C3F0): the plugs (+0x10 vector) and the refs (+0x1C vector), by index with
// the size re-read: --refcount +4, at 0 slot 0 with 1; then while the +0x28 vector (the refs that point AT this
// component) is not empty: its first ref r - when r +0x18 (its target) is set: r erased from the target's +0x28
// vector (if there), r +0x18 = 0, and r +0x20's vt+8 (no argument) when set; then the three vectors freed (+0x28,
// +0x1C, +0x10) and VuComponent's body. A first ref whose target is 0 (or another component) never leaves the list:
// false (UNKNOWN), the vectors and the body not reached.
template < typename M >
inline bool vu_script_component_dtor( M& m, std::uint32_t c, const vu_dtor_hooks_t& h )
{
    m[ c ] = 0x74C3F0;
    for ( const std::uint32_t list : { c + 0x10, c + 0x1C } )
        for ( std::int32_t i = 0; i < ( static_cast< std::int32_t >( m[ list + 4 ] - m[ list ] ) >> 2 ); ++i )
        {
            const std::uint32_t o = m[ m[ list ] + 4u * static_cast< std::uint32_t >( i ) ];
            if ( --m[ o + 4 ] == 0 )
                h.call( o, 0, 1 );
        }
    while ( ( m[ c + 0x2C ] - m[ c + 0x28 ] ) & ~3u )
    {
        const std::uint32_t r = m[ m[ c + 0x28 ] ], size = m[ c + 0x2C ] - m[ c + 0x28 ];
        if ( const std::uint32_t t = m[ r + 0x18 ] )
        {
            vu_vector_erase( m, t + 0x28, r );
            m[ r + 0x18 ] = 0;
            if ( const std::uint32_t owner = m[ r + 0x20 ] )
                h.call( owner, 8, -1 );
        }
        if ( m[ c + 0x2C ] - m[ c + 0x28 ] == size && m[ m[ c + 0x28 ] ] == r )
            return false;
    }
    vu_vector_free( m, c + 0x28, h );
    vu_vector_free( m, c + 0x1C, h );
    vu_vector_free( m, c + 0x10, h );
    vu_component_base_dtor( m, c, h );
    return true;
}

// 0x5042A0 VuScriptComponent slot 8 (thiscall component): the baseline its template-diff save (slot 7 0x504450)
// compares against - per plug (+0x10 vector) +0x5C = its connection count ((+0x54 - +0x50) sar 2); per ref (+0x1C
// vector) byte +0x1C = ( +0x18 != 0 ); +0x34 = the count of refs pointing at it ((+0x2C - +0x28) sar 2); +0x38 /
// +0x3C = +0x4C / +0x50 (the node position), +0x44 = +0x48. Counts signed, re-read each step.
template < typename M >
inline void vu_script_component_snapshot( M& m, std::uint32_t c )
{
    for ( std::int32_t i = 0; i < ( static_cast< std::int32_t >( m[ c + 0x14 ] - m[ c + 0x10 ] ) >> 2 ); ++i )
    {
        const std::uint32_t p = m[ m[ c + 0x10 ] + 4u * static_cast< std::uint32_t >( i ) ];
        m[ p + 0x5C ] = static_cast< std::uint32_t >( static_cast< std::int32_t >( m[ p + 0x54 ] - m[ p + 0x50 ] ) >> 2 );
    }
    for ( std::int32_t i = 0; i < ( static_cast< std::int32_t >( m[ c + 0x20 ] - m[ c + 0x1C ] ) >> 2 ); ++i )
    {
        const std::uint32_t r = m[ m[ c + 0x1C ] + 4u * static_cast< std::uint32_t >( i ) ];
        vu_set_byte( m, r + 0x1C, m[ r + 0x18 ] != 0 ? 1 : 0 );
    }
    m[ c + 0x34 ] = static_cast< std::uint32_t >( static_cast< std::int32_t >( m[ c + 0x2C ] - m[ c + 0x28 ] ) >> 2 );
    m[ c + 0x38 ] = m[ c + 0x4C ];
    m[ c + 0x3C ] = m[ c + 0x50 ];
    m[ c + 0x44 ] = m[ c + 0x48 ];
}

// 0x502340 (thiscall a, b): the connection undone both ways - b erased from a's +0x50 vector, then a from b's, then b
// from a's again ... until one is not found.
template < typename M >
inline void vu_plug_disconnect( M& m, std::uint32_t a, std::uint32_t b )
{
    while ( vu_vector_erase( m, a + 0x50, b ) )
        std::swap( a, b );
}

// 0x502180 VuScriptPlug (vtable 0x74C268; VuScriptOutputPlug's too): while +0x50 (the connections) is not empty,
// 0x502340( first, this ); the vector freed; the name VuString +0xC emptied; VuRefObj's tail. A first connection that
// does not list this plug back never leaves: false (UNKNOWN).
template < typename M >
inline bool vu_script_plug_dtor( M& m, std::uint32_t p, const vu_dtor_hooks_t& h )
{
    m[ p ] = 0x74C268;
    while ( ( m[ p + 0x54 ] - m[ p + 0x50 ] ) & ~3u )
    {
        const std::uint32_t size = m[ p + 0x54 ] - m[ p + 0x50 ], first = m[ m[ p + 0x50 ] ];
        vu_plug_disconnect( m, first, p );
        if ( m[ p + 0x54 ] - m[ p + 0x50 ] == size && m[ m[ p + 0x50 ] ] == first )
            return false;
    }
    vu_vector_free( m, p + 0x50, h );
    vu_string_tidy( m, p + 0xC, h );
    vu_refobj_tail( m, p, h );
    return true;
}

// 0x5426B0 VuScriptRef (vtable 0x74D8EC): when +0x18 (the target component) is set: 0x504980 (the target's +0x28
// vector loses this ref), +0x18 = 0, +0x20's vt+8 (no argument) when set; then +0x20's slot 0 with 1 when set;
// VuRefObj's tail.
template < typename M >
inline void vu_script_ref_dtor( M& m, std::uint32_t r, const vu_dtor_hooks_t& h )
{
    m[ r ] = 0x74D8EC;
    if ( const std::uint32_t t = m[ r + 0x18 ] )
    {
        vu_vector_erase( m, t + 0x28, r );   // 0x504980
        m[ r + 0x18 ] = 0;
        if ( const std::uint32_t owner = m[ r + 0x20 ] )
            h.call( owner, 8, -1 );
    }
    vu_delete_owned( m, r + 0x20, h );
    vu_refobj_tail( m, r, h );
}

// 0x4B4290 VuProject (vtable 0x7459F0): +0xC (its root entity) --refcount +4, at 0 slot 0 with 1, +0xC = 0; the
// VuJsonContainer +0x10 cleared (0x4FBB10: hook); VuRefObj's tail.
template < typename M >
inline void vu_project_dtor( M& m, std::uint32_t p, const vu_dtor_hooks_t& h )
{
    m[ p ] = 0x7459F0;
    if ( const std::uint32_t o = m[ p + 0xC ] )
    {
        if ( --m[ o + 4 ] == 0 )
            h.call( o, 0, 1 );
        m[ p + 0xC ] = 0;
    }
    h.json_clear( p + 0x10 );
    vu_refobj_tail( m, p, h );
}

// Entity classes' dtor bodies (each ends in VuEntity's 0x500870):
//   0x5CE980 VuKeyframeMotionEntity (inline): _aligned_free of the VuArray buffers +0xD0, +0xBC, +0xB0, +0xA0, +0x90, +0x7C
//            (in that order; a null one is passed too);
//   0x5EB000 VuStaticPfxEntity (inline): the VuString +0x70 emptied;
//   0x5F4AE0 VuSubstituteAssetEntity: the VuStrings +0x90, +0x78, +0x60 emptied;
//   0x5DB430 VuWaterSurfaceEntity (VuWaterRampEntity's 0x5D6F50 runs it): vtables 0x753F1C / +0x5C 0x753F08; +0x12C (a
//            ref-counted object) --refcount +4, at 0 slot 0 with 1, +0x12C = 0; 0x59A300 on +0xA4 (its VuStrings +0x6C,
//            +0x4C, +0x30, +0 emptied); the VuString +0x8C emptied.
template < typename M >
inline void vu_keyframe_motion_dtor( M& m, std::uint32_t e, const vu_dtor_hooks_t& h )
{
    for ( const std::uint32_t off : { 0xD0u, 0xBCu, 0xB0u, 0xA0u, 0x90u, 0x7Cu } )
        h.aligned_free( m[ e + off ] );
    vu_entity_dtor( m, e, h );
}

template < typename M >
inline void vu_static_pfx_dtor( M& m, std::uint32_t e, const vu_dtor_hooks_t& h )
{
    vu_string_tidy( m, e + 0x70, h );
    vu_entity_dtor( m, e, h );
}

template < typename M >
inline void vu_substitute_asset_dtor( M& m, std::uint32_t e, const vu_dtor_hooks_t& h )
{
    for ( const std::uint32_t off : { 0x90u, 0x78u, 0x60u } )
        vu_string_tidy( m, e + off, h );
    vu_entity_dtor( m, e, h );
}

template < typename M >
inline void vu_water_surface_entity_dtor( M& m, std::uint32_t e, const vu_dtor_hooks_t& h )
{
    m[ e ] = 0x753F1C;
    m[ e + 0x5C ] = 0x753F08;
    if ( const std::uint32_t o = m[ e + 0x12C ] )
    {
        if ( --m[ o + 4 ] == 0 )
            h.call( o, 0, 1 );
        m[ e + 0x12C ] = 0;
    }
    for ( const std::uint32_t off : { 0x6Cu, 0x4Cu, 0x30u, 0x0u } )   // 0x59A300
        vu_string_tidy( m, e + 0xA4 + off, h );
    vu_string_tidy( m, e + 0x8C, h );
    vu_entity_dtor( m, e, h );
}

// The deleting dtors (slot 0 / slot 2, thiscall object, flags): the body, then operator delete when flags & 1.
//   0x423770 VuComponent (body inline)       0x4268F0 Vu2dLayoutComponent     0x50B960 Vu3dLayoutComponent
//   0x531D90 VuInstigatorComponent           0x534280 VuTransformComponent    0x503B70 VuScriptComponent
//   0x4052E0 VuScriptPlug / VuScriptOutputPlug  0x542680 VuScriptRef      0x4B4330 VuProject
//   0x5EFA30 VuOnProjectInitializedEntity (vtable 0x756828, then VuEntity's 0x500870)
//   0x4032A0 VuMethod0 / 1 / 2 (vtable 0x736EDC only)
//   0x5CE980 VuKeyframeMotionEntity  0x5EB000 VuStaticPfxEntity  0x5F4AB0 VuSubstituteAssetEntity  0x5D6F50 VuWaterRampEntity
// Answers false when the body did (UNKNOWN; nothing freed). (Slot 12 of the enum's order is the case's VuScriptComponent
// slot 8 vectors - not a dtor; the check runs vu_script_component_snapshot for it.)
enum class vu_framework_dtor_t
{
    component, layout_2d, layout_3d, instigator, transform, script, plug, ref, project, project_initialized, method,
    snapshot_, keyframe_motion, static_pfx, substitute_asset, water_ramp
};

template < typename M >
inline bool vu_framework_delete( M& m, vu_framework_dtor_t kind, std::uint32_t o, std::uint32_t flags, const vu_dtor_hooks_t& h )
{
    bool ok = true;
    switch ( kind )
    {
    case vu_framework_dtor_t::component: vu_component_base_dtor( m, o, h ); break;
    case vu_framework_dtor_t::layout_2d: vu_2d_layout_dtor( m, o, h ); break;
    case vu_framework_dtor_t::layout_3d: vu_3d_layout_dtor( m, o, h ); break;
    case vu_framework_dtor_t::instigator: vu_instigator_dtor( m, o, h ); break;
    case vu_framework_dtor_t::transform: vu_transform_component_dtor( m, o, h ); break;
    case vu_framework_dtor_t::script: ok = vu_script_component_dtor( m, o, h ); break;
    case vu_framework_dtor_t::plug: ok = vu_script_plug_dtor( m, o, h ); break;
    case vu_framework_dtor_t::ref: vu_script_ref_dtor( m, o, h ); break;
    case vu_framework_dtor_t::project: vu_project_dtor( m, o, h ); break;
    case vu_framework_dtor_t::project_initialized: m[ o ] = 0x756828; vu_entity_dtor( m, o, h ); break;
    case vu_framework_dtor_t::method: m[ o ] = 0x736EDC; break;
    case vu_framework_dtor_t::snapshot_: return false;
    case vu_framework_dtor_t::keyframe_motion: vu_keyframe_motion_dtor( m, o, h ); break;
    case vu_framework_dtor_t::static_pfx: vu_static_pfx_dtor( m, o, h ); break;
    case vu_framework_dtor_t::substitute_asset: vu_substitute_asset_dtor( m, o, h ); break;
    case vu_framework_dtor_t::water_ramp: vu_water_surface_entity_dtor( m, o, h ); break;
    }
    if ( ok && ( flags & 1u ) != 0 )
        h.free( o );
    return ok;
}