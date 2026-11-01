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
