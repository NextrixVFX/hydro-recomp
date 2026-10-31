#pragma once

#include <utility>
#include <vector>

// btHashedOverlappingPairCache (Bullet 2.79, vt 0x751B6C), recreated from
// retail's disassembly (memory/audit.md "Batch 8i"). Slots: 0 dtor 0x5B0C60,
// 1 addOverlappingPair 0x5B1950, 2 removeOverlappingPair 0x5B1470, 3
// removeOverlappingPairsContainingProxy 0x5B0E80, 4 / 5
// getOverlappingPairArrayPtr 0x4B65A0, 6 getOverlappingPairArray 0x4A12D0,
// 7 cleanOverlappingPair 0x5B0DA0, 8 getNumOverlappingPairs 0x5402E0, 9
// cleanProxyFromPairs 0x5B0DD0, 10 setOverlapFilterCallback 0x5B1940, 11
// processAllOverlappingPairs 0x5B1670, 12 findPair 0x5B0F10, 13
// hasDeferredRemoval 0x4052D0 (0), 14 setInternalGhostPairCallback 0x5B1930,
// 15 sortOverlappingPairs 0x5B16D0 (not recreated: no caller on this path).
// The stats gAddedPairs 0x7590CC / gRemovePairs 0x7590C8 / gFindPairs
// 0x7590C0 / gOverlappingPairs 0x7590C4 and the alloc counters are bumped
// by retail and not modelled.

// btBroadphaseProxy as the cache reads it: +0 the client object, +4 the
// collision filter group, +6 the mask (shorts), +12 the unique id;
// client_type is the CLIENT's +0xF4 internal type (4 = a ghost object), which
// the ghost pair callback reads.
struct broadphase_proxy_t
{
    void* client = nullptr;
    short group = 0;
    short mask = 0;
    int uid = 0;
    int client_type = 0;
};

// btBroadphasePair (16 bytes): proxy0, proxy1, the algorithm, internal info.
struct broadphase_pair_t
{
    broadphase_proxy_t* proxy0 = nullptr;
    broadphase_proxy_t* proxy1 = nullptr;
    void* algorithm = nullptr;
    unsigned internal = 0;
};

// btAlignedObjectArray's size / capacity, as the cache grows them: storage
// holds `capacity` elements; resize past the capacity reserves EXACTLY the
// new size and fills the new elements with T{}.
template < typename T > struct aligned_array_t
{
    int size = 0;
    int capacity = 0;
    std::vector< T > data;

    void reserve( int n )
    {
        if ( capacity < n )
        {
            data.resize( static_cast< std::size_t >( n ) );
            capacity = n;
        }
    }
    void resize( int n )
    {
        if ( n > size )
        {
            reserve( n );
            for ( int i = size; i < n; ++i )
                data[ i ] = T{};
        }
        size = n;
    }
};

struct pair_cache_t;

// The cache's callbacks into code that is not recreated: the overlap filter
// (+0x18, set -> its vt+4 decides), the ghost objects' own
// addOverlappingObjectInternal (vt+0x18: other, self) and
// removeOverlappingObjectInternal (vt+0x1C: other, dispatcher, self) that the
// ghost pair callback reaches, and the algorithm teardown cleanOverlappingPair
// performs (the algorithm's dtor vt+0(0) + the dispatcher's
// freeCollisionAlgorithm vt+56). A NULL hook where retail would call reports
// -1 (unknown).
struct pair_cache_hooks_t
{
    void* user = nullptr;
    void ( *clean_algorithm )( void* user, void* algorithm ) = nullptr;
    void ( *ghost_add )( void* user, void* ghost, broadphase_proxy_t* other, broadphase_proxy_t* self ) = nullptr;
    void ( *ghost_remove )( void* user, void* ghost, broadphase_proxy_t* other, broadphase_proxy_t* self ) = nullptr;
};

struct pair_cache_t
{
    aligned_array_t< broadphase_pair_t > pairs;   // +4 (+8 size, +0xC capacity, +0x10 data)
    bool has_filter = false;                      // +0x18
    aligned_array_t< int > hash_table;            // +0x20 (+0x24 size, +0x28 capacity, +0x2C data)
    aligned_array_t< int > next;                  // +0x34 (+0x38, +0x3C, +0x40)
    bool has_ghost = false;                       // +0x48: Vu 0x4F8050 installs a btGhostPairCallback
    pair_cache_hooks_t hooks;
};

// btGhostPairCallback (vt 0x74C02C; Vu installs it on the world's cache):
// addOverlappingPair 0x4F8F40 (a, b): a client whose +0xF4 is 4 (a ghost
// object) gets addOverlappingObjectInternal (vt+0x18) - a's ghost first with
// (b, a), then b's with (a, b); removeOverlappingPair 0x4F8F90 the same
// through vt+0x1C (other, dispatcher, self); both return 0;
// removeOverlappingPairsContainingProxy 0x410B30 is `ret 8`. -1 when a ghost
// is met with no hook.
inline int pair_cache_ghost_add( pair_cache_t& c, broadphase_proxy_t* a, broadphase_proxy_t* b )
{
    const bool g0 = a->client_type == 4, g1 = b->client_type == 4;
    if ( ( g0 || g1 ) && !c.hooks.ghost_add )
        return -1;
    if ( g0 )
        c.hooks.ghost_add( c.hooks.user, a->client, b, a );
    if ( g1 )
        c.hooks.ghost_add( c.hooks.user, b->client, a, b );
    return 1;
}

inline int pair_cache_ghost_remove( pair_cache_t& c, broadphase_proxy_t* a, broadphase_proxy_t* b )
{
    const bool g0 = a->client_type == 4, g1 = b->client_type == 4;
    if ( ( g0 || g1 ) && !c.hooks.ghost_remove )
        return -1;
    if ( g0 )
        c.hooks.ghost_remove( c.hooks.user, a->client, b, a );
    if ( g1 )
        c.hooks.ghost_remove( c.hooks.user, b->client, a, b );
    return 1;
}

// getHash as every site inlines it: key = uid0 | uid1 << 16 in a SIGNED int,
// key += ~(key << 15), key ^= key >> 10 (sar), key += key << 3 (lea *9),
// key ^= key >> 6, key += ~(key << 11), key ^= key >> 16; & (capacity - 1).
inline int pair_hash( int uid0, int uid1, int capacity )
{
    int key = static_cast< int >( static_cast< unsigned >( uid0 ) | ( static_cast< unsigned >( uid1 ) << 16 ) );
    key = static_cast< int >( static_cast< unsigned >( key ) + ~( static_cast< unsigned >( key ) << 15 ) );
    key ^= key >> 10;
    key = static_cast< int >( static_cast< unsigned >( key ) * 9u );
    key ^= key >> 6;
    key = static_cast< int >( static_cast< unsigned >( key ) + ~( static_cast< unsigned >( key ) << 11 ) );
    key ^= key >> 16;
    return static_cast< int >( static_cast< unsigned >( capacity - 1 ) & static_cast< unsigned >( key ) );
}

// growTables @0x5B0FE0: with the hash table smaller than the pair capacity,
// both tables are resized to it, set to -1 throughout, and the first OLD
// table-size pairs re-linked in index order (head insert).
inline void pair_cache_grow_tables( pair_cache_t& c )
{
    const int n = c.pairs.capacity;
    const int old = c.hash_table.size;
    if ( old >= n )
        return;
    c.hash_table.resize( n );
    c.next.resize( n );
    for ( int i = 0; i < n; ++i )
        c.hash_table.data[ i ] = -1;
    for ( int i = 0; i < n; ++i )
        c.next.data[ i ] = -1;
    for ( int i = 0; i < old; ++i )
    {
        const broadphase_pair_t& p = c.pairs.data[ i ];
        const int h = pair_hash( p.proxy0->uid, p.proxy1->uid, c.pairs.capacity );
        c.next.data[ i ] = c.hash_table.data[ h ];
        c.hash_table.data[ h ] = i;
    }
}

// The ctor @0x5B0B10: all arrays empty (owns 1), filter / ghost NULL, the pair
// array reserved to 2, then growTables.
inline void pair_cache_init( pair_cache_t& c )
{
    c = pair_cache_t{};
    c.pairs.reserve( 2 );
    pair_cache_grow_tables( c );
}

// The chain walk (internalFindPair inlined): index of the pair with these
// ordered uids under hash h, or -1.
inline int pair_cache_chain_find( const pair_cache_t& c, int h, int uid0, int uid1 )
{
    int i = c.hash_table.data[ h ];
    while ( i != -1 )
    {
        const broadphase_pair_t& p = c.pairs.data[ i ];
        if ( p.proxy0->uid == uid0 && p.proxy1->uid == uid1 )
            return i;
        i = c.next.data[ i ];
    }
    return -1;
}

// internalAddPair @0x5B1220: the proxies by uid (`cmp; jle` -> swap when a
// > b); an existing pair is returned; else the pair array grows when full
// (size ? 2 size : 1), size++, the ghost callback, growTables when the
// capacity grew (the hash recomputed), the pair (uid0 >= uid1 -> (b, a), else
// (a, b); algorithm 0, internal 0) head-linked at its index. Returns the
// pair's index, or -1 where a hook is unknown.
inline int pair_cache_internal_add( pair_cache_t& c, broadphase_proxy_t* a, broadphase_proxy_t* b )
{
    if ( a->uid > b->uid )
        std::swap( a, b );
    const int uid0 = a->uid, uid1 = b->uid;
    const int old_capacity = c.pairs.capacity;
    int h = pair_hash( uid0, uid1, old_capacity );
    const int found = pair_cache_chain_find( c, h, uid0, uid1 );
    if ( found >= 0 )
        return found;
    const int count = c.pairs.size;
    if ( count == c.pairs.capacity )
        c.pairs.reserve( count ? 2 * count : 1 );
    ++c.pairs.size;
    if ( c.has_ghost && pair_cache_ghost_add( c, a, b ) < 0 )
        return -1;
    if ( old_capacity < c.pairs.capacity )
    {
        pair_cache_grow_tables( c );
        h = pair_hash( uid0, uid1, c.pairs.capacity );
    }
    broadphase_pair_t& p = c.pairs.data[ count ];
    if ( a->uid >= b->uid )
    {
        p.proxy0 = b;
        p.proxy1 = a;
    }
    else
    {
        p.proxy0 = a;
        p.proxy1 = b;
    }
    p.algorithm = nullptr;
    p.internal = 0;
    c.next.data[ count ] = c.hash_table.data[ h ];
    c.hash_table.data[ h ] = count;
    return count;
}

// addOverlappingPair @0x5B1950 (slot 1): needsBroadphaseCollision - the
// filter callback when set (a hook), else (a.group & b.mask) && (a.mask &
// b.group) - then internalAddPair(a, b). Returns the pair's index, -2 when
// filtered out (retail returns 0), -1 unknown.
inline int pair_cache_add( pair_cache_t& c, broadphase_proxy_t* a, broadphase_proxy_t* b )
{
    if ( c.has_filter )
        return -1;   // the overlap filter callback (+0x18 vt+4): not recreated
    if ( ( a->group & b->mask ) == 0 || ( a->mask & b->group ) == 0 )
        return -2;
    return pair_cache_internal_add( c, a, b );
}

// findPair @0x5B0F10 (slot 12): the ordered uids' hash; an index past the
// hash table's size returns 0; else the chain. Returns the index or -1.
inline int pair_cache_find( const pair_cache_t& c, broadphase_proxy_t* a, broadphase_proxy_t* b )
{
    if ( a->uid > b->uid )
        std::swap( a, b );
    const int h = pair_hash( a->uid, b->uid, c.pairs.capacity );
    if ( h >= c.hash_table.size )
        return -1;
    return pair_cache_chain_find( c, h, a->uid, b->uid );
}

// cleanOverlappingPair @0x5B0DA0 (slot 7; pair, dispatcher): an algorithm
// is destroyed (vt+0(0)) and freed (dispatcher vt+56), then zeroed.
inline int pair_cache_clean_pair( pair_cache_t& c, broadphase_pair_t& p )
{
    if ( p.algorithm )
    {
        if ( !c.hooks.clean_algorithm )
            return -1;
        c.hooks.clean_algorithm( c.hooks.user, p.algorithm );
        p.algorithm = nullptr;
    }
    return 1;
}

// Unlinks index i from the chain under h (the predecessor walk; head ->
// hash_table[h] = next[i]).
inline void pair_cache_unlink( pair_cache_t& c, int h, int i )
{
    int index = c.hash_table.data[ h ];
    if ( index == i )
    {
        c.hash_table.data[ h ] = c.next.data[ i ];
        return;
    }
    int previous = -1;
    while ( index != i )
    {
        previous = index;
        index = c.next.data[ index ];
    }
    c.next.data[ previous ] = c.next.data[ i ];
}

// removeOverlappingPair @0x5B1470 (slot 2; a, b, dispatcher): the ordered
// uids' pair (none -> 0); cleanOverlappingPair (vt+28); its internal info
// kept for the return; unlinked; the ghost callback (vt+8); the LAST pair,
// when it is not this one, unlinked from its own chain, copied into the
// hole and head-linked there; size--. `internal` receives the pair's +12.
// Returns 1 removed, 0 not found, -1 unknown.
inline int pair_cache_remove( pair_cache_t& c, broadphase_proxy_t* a, broadphase_proxy_t* b, unsigned& internal )
{
    if ( a->uid > b->uid )
        std::swap( a, b );
    const int h = pair_hash( a->uid, b->uid, c.pairs.capacity );
    const int i = pair_cache_chain_find( c, h, a->uid, b->uid );
    if ( i < 0 )
        return 0;
    if ( pair_cache_clean_pair( c, c.pairs.data[ i ] ) < 0 )
        return -1;
    internal = c.pairs.data[ i ].internal;
    pair_cache_unlink( c, h, i );
    const int last = c.pairs.size - 1;
    if ( c.has_ghost && pair_cache_ghost_remove( c, a, b ) < 0 )
        return -1;
    if ( last != i )
    {
        const broadphase_pair_t& l = c.pairs.data[ last ];
        const int lh = pair_hash( l.proxy0->uid, l.proxy1->uid, c.pairs.capacity );
        pair_cache_unlink( c, lh, last );
        c.pairs.data[ i ] = c.pairs.data[ last ];
        c.next.data[ i ] = c.hash_table.data[ lh ];
        c.hash_table.data[ lh ] = i;
    }
    --c.pairs.size;
    return 1;
}

// processAllOverlappingPairs @0x5B1670 (slot 11; callback, dispatcher): a
// pair the callback's processOverlap (vt+4) accepts is removed (vt+8 with
// its proxies) and the index NOT advanced. The callback is a functor
// returning 1 remove, 0 keep, -1 unknown.
template < typename F > inline int pair_cache_process_all( pair_cache_t& c, F&& process )
{
    int i = 0;
    while ( i < c.pairs.size )
    {
        broadphase_pair_t& p = c.pairs.data[ i ];
        const int r = process( p );
        if ( r < 0 )
            return -1;
        if ( r )
        {
            unsigned internal = 0;
            if ( pair_cache_remove( c, p.proxy0, p.proxy1, internal ) < 0 )
                return -1;
        }
        else
            ++i;
    }
    return 1;
}

// removeOverlappingPairsContainingProxy @0x5B0E80 (slot 3): RemovePairCallback
// (vt 0x751B2C, processOverlap 0x5B0EF0: proxy0 == p || proxy1 == p).
inline int pair_cache_remove_containing( pair_cache_t& c, broadphase_proxy_t* proxy )
{
    return pair_cache_process_all( c, [ proxy ]( broadphase_pair_t& p ) { return ( p.proxy0 == proxy || p.proxy1 == proxy ) ? 1 : 0; } );
}

// cleanProxyFromPairs @0x5B0DD0 (slot 9): CleanPairCallback (vt 0x751B38,
// processOverlap 0x5B0E50: a pair with the proxy is cleaned through the
// cache's vt+28; always false).
inline int pair_cache_clean_proxy( pair_cache_t& c, broadphase_proxy_t* proxy )
{
    return pair_cache_process_all( c, [ &c, proxy ]( broadphase_pair_t& p ) {
        if ( p.proxy0 == proxy || p.proxy1 == proxy )
            if ( pair_cache_clean_pair( c, p ) < 0 )
                return -1;
        return 0;
    } );
}
