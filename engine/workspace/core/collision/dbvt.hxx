#pragma once

#include <memory>
#include <vector>
#include <xmmintrin.h>

#include <workspace/core/collision/support.hxx>

// btDbvt (Bullet 2.79) as the btDbvtBroadphase uses it, recreated from
// retail's disassembly (memory/audit.md "Batch 8i"): insert 0x5948D0,
// insertleaf 0x5942E0, removeleaf 0x594480, update 0x594960, update with
// velocity 0x5949E0, clear 0x5946B0, recursedeletenode 0x594290. Every
// volume is FOUR lanes (minps / maxps / the Proximity sum read lane 3 too).

// btDbvtNode (48 bytes): +0 mins, +16 maxs, +32 parent, +36 childs[0] / data
// (a union), +40 childs[1] (0 = a leaf). `id` is the allocation order -
// retail's address, for the check.
struct dbvt_node_t
{
    float mn[ 4 ];
    float mx[ 4 ];
    dbvt_node_t* parent;
    union
    {
        dbvt_node_t* childs[ 2 ];
        void* data;
    };
    int id;
};

// btDbvt: +0 root, +4 m_free (ONE cached node), +8 m_lkhd, +12 m_leaves, +16
// m_opath. Nodes freed with m_free occupied go back to the allocator; ours
// keeps them owned (`pool`) but never reuses them.
struct dbvt_t
{
    dbvt_node_t* root = nullptr;
    dbvt_node_t* free = nullptr;
    int lkhd = -1;
    int leaves = 0;
    unsigned opath = 0;
    std::vector< std::unique_ptr< dbvt_node_t > > pool;
    int allocations = 0;
    // A counter shared by several trees (the broadphase's two sets) so node
    // ids keep the ALLOCATION ORDER across them - optimizeIncremental's sort
    // compares node ADDRESSES (an UNKNOWN in the game: there they are heap
    // addresses; ours / the oracle's bump allocator give allocation order).
    int* counter = nullptr;
};

inline bool dbvt_is_leaf( const dbvt_node_t* n )
{
    return n->childs[ 1 ] == nullptr;
}

inline void dbvt_merge( const float* amn, const float* amx, const float* bmn, const float* bmx, float* mn, float* mx )
{
    _mm_storeu_ps( mx, _mm_max_ps( _mm_loadu_ps( amx ), _mm_loadu_ps( bmx ) ) );
    _mm_storeu_ps( mn, _mm_min_ps( _mm_loadu_ps( amn ), _mm_loadu_ps( bmn ) ) );
}

// createnode: the m_free node when there is one (taken), else a fresh 48-byte
// allocation.
inline dbvt_node_t* dbvt_take_node( dbvt_t& t )
{
    if ( t.free )
    {
        dbvt_node_t* n = t.free;
        t.free = nullptr;
        return n;
    }
    t.pool.push_back( std::make_unique< dbvt_node_t >( ) );
    dbvt_node_t* n = t.pool.back( ).get( );
    n->id = t.counter ? ( *t.counter )++ : t.allocations++;
    return n;
}

// deletenode: the cached node is FREED and this one cached.
inline void dbvt_delete_node( dbvt_t& t, dbvt_node_t* n )
{
    t.free = n;
}

// The Contain test (`comiss; jb` x6 - NaN fails): a's box holds b's, lanes
// 0..2.
inline bool dbvt_contain( const float* amn, const float* amx, const float* bmn, const float* bmx )
{
    return bmn[ 0 ] >= amn[ 0 ] && bmn[ 1 ] >= amn[ 1 ] && bmn[ 2 ] >= amn[ 2 ] && amx[ 0 ] >= bmx[ 0 ] && amx[ 1 ] >= bmx[ 1 ] &&
           amx[ 2 ] >= bmx[ 2 ];
}

// Proximity as insertleaf inlines it: |(c.max + c.min) - (l.max + l.min)|
// over FOUR lanes, then (x + z) + (y + w) (movhlps, shufps 1).
inline float dbvt_proximity( const dbvt_node_t* c, __m128 lsum )
{
    const __m128 mask = _mm_castsi128_ps( _mm_set1_epi32( 0x7FFFFFFF ) );
    const __m128 d = _mm_and_ps( _mm_sub_ps( _mm_add_ps( _mm_loadu_ps( c->mx ), _mm_loadu_ps( c->mn ) ), lsum ), mask );
    const __m128 h = _mm_add_ps( _mm_movehl_ps( d, d ), d );
    return _mm_cvtss_f32( _mm_add_ss( h, _mm_shuffle_ps( h, h, 1 ) ) );
}

// insertleaf @0x5942E0 (ecx tree, edx start, stack leaf; caller pops): an
// empty tree takes the leaf as root (parent 0); else descend from `start`
// while internal to childs[cmpless(d1, d0) & 1] (NaN -> child 0); a node
// (createnode: parent = the sibling's parent, childs 0, volume = merge(leaf,
// sibling) - minps(leaf, sibling) / maxps(leaf, sibling)) takes the
// sibling's place; walking up, each ancestor that does not Contain the node
// below is re-merged from its childs (minps(c0, c1)) until one does.
inline void dbvt_insert_leaf( dbvt_t& t, dbvt_node_t* start, dbvt_node_t* leaf )
{
    if ( !t.root )
    {
        t.root = leaf;
        leaf->parent = nullptr;
        return;
    }
    dbvt_node_t* sibling = start;
    if ( !dbvt_is_leaf( sibling ) )
    {
        const __m128 lsum = _mm_add_ps( _mm_loadu_ps( leaf->mx ), _mm_loadu_ps( leaf->mn ) );
        do
        {
            const float d0 = dbvt_proximity( sibling->childs[ 0 ], lsum );
            const float d1 = dbvt_proximity( sibling->childs[ 1 ], lsum );
            // retail's own `cmpless d1, d0; movd; and 1` - a C++ `d1 <= d0`
            // ternary is NOT NaN-safe under MSVC /O2 (a perturbation to
            // `d0 < d1 ? 0 : 1` compiled to the same code; ac2a132's issue).
            const int pick = _mm_cvtsi128_si32( _mm_castps_si128( _mm_cmple_ss( _mm_set_ss( d1 ), _mm_set_ss( d0 ) ) ) ) & 1;
            sibling = sibling->childs[ pick ];
        } while ( !dbvt_is_leaf( sibling ) );
    }
    dbvt_node_t* prev = sibling->parent;
    dbvt_node_t* node = dbvt_take_node( t );
    node->parent = prev;
    node->childs[ 0 ] = nullptr;
    node->childs[ 1 ] = nullptr;
    dbvt_merge( leaf->mn, leaf->mx, sibling->mn, sibling->mx, node->mn, node->mx );
    if ( prev )
    {
        prev->childs[ prev->childs[ 1 ] == sibling ? 1 : 0 ] = node;
        node->childs[ 0 ] = sibling;
        sibling->parent = node;
        node->childs[ 1 ] = leaf;
        leaf->parent = node;
        do
        {
            if ( dbvt_contain( prev->mn, prev->mx, node->mn, node->mx ) )
                break;
            dbvt_merge( prev->childs[ 0 ]->mn, prev->childs[ 0 ]->mx, prev->childs[ 1 ]->mn, prev->childs[ 1 ]->mx, prev->mn, prev->mx );
            node = prev;
            prev = prev->parent;
        } while ( prev );
    }
    else
    {
        node->childs[ 0 ] = sibling;
        sibling->parent = node;
        node->childs[ 1 ] = leaf;
        leaf->parent = node;
        t.root = node;
    }
}

// removeleaf @0x594480 (fastcall tree, leaf): the root leaf empties the tree
// (returns 0); else the parent is replaced by the sibling and deleted; the
// ancestors are re-merged (minps(c0, c1)) until one whose six min / max
// lanes 0..2 come out ordered-EQUAL to before (`ucomiss; lahf; test ah,
// 44h; jp` - NaN = changed); returns that ancestor, or the root.
inline dbvt_node_t* dbvt_remove_leaf( dbvt_t& t, dbvt_node_t* leaf )
{
    if ( leaf == t.root )
    {
        t.root = nullptr;
        return nullptr;
    }
    dbvt_node_t* parent = leaf->parent;
    dbvt_node_t* prev = parent->parent;
    dbvt_node_t* sibling = parent->childs[ parent->childs[ 1 ] == leaf ? 0 : 1 ];
    if ( !prev )
    {
        t.root = sibling;
        sibling->parent = nullptr;
        dbvt_delete_node( t, parent );
        return t.root;
    }
    prev->childs[ prev->childs[ 1 ] == parent ? 1 : 0 ] = sibling;
    sibling->parent = prev;
    dbvt_delete_node( t, parent );
    while ( prev )
    {
        float pmn[ 4 ], pmx[ 4 ];
        std::memcpy( pmn, prev->mn, 16 );
        std::memcpy( pmx, prev->mx, 16 );
        dbvt_merge( prev->childs[ 0 ]->mn, prev->childs[ 0 ]->mx, prev->childs[ 1 ]->mn, prev->childs[ 1 ]->mx, prev->mn, prev->mx );
        if ( pmn[ 0 ] == prev->mn[ 0 ] && pmn[ 1 ] == prev->mn[ 1 ] && pmn[ 2 ] == prev->mn[ 2 ] && pmx[ 0 ] == prev->mx[ 0 ] &&
             pmx[ 1 ] == prev->mx[ 1 ] && pmx[ 2 ] == prev->mx[ 2 ] )
            return prev;
        prev = prev->parent;
    }
    return t.root;
}

// insert @0x5948D0 (thiscall volume, data; retn 8): a leaf from createnode
// (parent 0, data, childs[1] 0, the 32-byte volume), insertleaf from the
// root, ++m_leaves.
inline dbvt_node_t* dbvt_insert( dbvt_t& t, const float* mn, const float* mx, void* data )
{
    dbvt_node_t* leaf = dbvt_take_node( t );
    leaf->parent = nullptr;
    leaf->childs[ 1 ] = nullptr;
    leaf->data = data;
    std::memcpy( leaf->mn, mn, 16 );
    std::memcpy( leaf->mx, mx, 16 );
    dbvt_insert_leaf( t, t.root, leaf );
    ++t.leaves;
    return leaf;
}

// update @0x594960 (thiscall leaf, volume; retn 8): removeleaf; with a
// result, m_lkhd < 0 (`js`) starts from the WHOLE root, else from the result
// walked up at most m_lkhd parents; the volume copied; insertleaf.
inline void dbvt_update( dbvt_t& t, dbvt_node_t* leaf, const float* mn, const float* mx )
{
    dbvt_node_t* start = dbvt_remove_leaf( t, leaf );
    if ( start )
    {
        if ( t.lkhd < 0 )
            start = t.root;
        else
            for ( int i = 0; i < t.lkhd && start->parent; ++i )
                start = start->parent;
    }
    std::memcpy( leaf->mn, mn, 16 );
    std::memcpy( leaf->mx, mx, 16 );
    dbvt_insert_leaf( t, start, leaf );
}

// update with velocity @0x5949E0 (thiscall leaf, volume, velocity, margin;
// retn 10h): false when the leaf's volume Contains the new one; else the
// volume (the CALLER's buffer, edited in place) expanded by the .rdata
// constant 0.05 (0x3D4CCCCD) - the margin argument is NOT read - lanes 0..2
// min -= / max +=, then signed by the velocity (`comiss v, 0; jbe`: v > 0 ->
// max += v, else min += v), update, true.
inline bool dbvt_update_velocity( dbvt_t& t, dbvt_node_t* leaf, float* mn, float* mx, const float* velocity )
{
    if ( dbvt_contain( leaf->mn, leaf->mx, mn, mx ) )
        return false;
    const float m = support_bits( 0x3D4CCCCDu );
    for ( int k = 0; k < 3; ++k )
        mn[ k ] = mn[ k ] - m;
    for ( int k = 0; k < 3; ++k )
        mx[ k ] = mx[ k ] + m;
    for ( int k = 0; k < 3; ++k )
    {
        if ( velocity[ k ] > 0.0f )
            mx[ k ] = mx[ k ] + velocity[ k ];
        else
            mn[ k ] = mn[ k ] + velocity[ k ];
    }
    dbvt_update( t, leaf, mn, mx );
    return true;
}

// sort (inlined in optimizeIncremental): a node whose parent's ADDRESS is
// greater (`cmp; jbe` unsigned - ids here) swaps places with it - the
// parent takes the node's childs, the node takes the parent and the sibling
// (at the node's old index), the grandparent (or the root) points at the
// node, the two volumes are swapped - and the parent is returned; else the
// node.
inline dbvt_node_t* dbvt_sort( dbvt_t& t, dbvt_node_t* n )
{
    dbvt_node_t* p = n->parent;
    if ( !p || p->id <= n->id )
        return n;
    const int i = p->childs[ 1 ] == n ? 1 : 0;
    const int j = 1 - i;
    dbvt_node_t* s = p->childs[ j ];
    dbvt_node_t* q = p->parent;
    if ( q )
        q->childs[ q->childs[ 1 ] == p ? 1 : 0 ] = n;
    else
        t.root = n;
    s->parent = n;
    p->parent = n;
    n->parent = q;
    p->childs[ 0 ] = n->childs[ 0 ];
    p->childs[ 1 ] = n->childs[ 1 ];
    n->childs[ 0 ]->parent = p;
    n->childs[ 1 ]->parent = p;
    n->childs[ i ] = p;
    n->childs[ j ] = s;
    float v[ 8 ];
    std::memcpy( v, p->mn, 32 );
    std::memcpy( p->mn, n->mn, 32 );
    std::memcpy( n->mn, v, 32 );
    return p;
}

// optimizeIncremental @0x594770 (thiscall tree, passes; retn 4): passes < 0
// -> m_leaves; per pass from the root while internal: node = sort(node)->
// childs[(m_opath >> bit) & 1], bit = (bit + 1) & 31; then the leaf is
// re-inserted (removeleaf; a result restarts from the ROOT, none from 0) and
// ++m_opath.
inline void dbvt_optimize_incremental( dbvt_t& t, int passes )
{
    if ( passes < 0 )
        passes = t.leaves;
    if ( !t.root || passes <= 0 )
        return;
    do
    {
        dbvt_node_t* node = t.root;
        unsigned bit = 0;
        while ( !dbvt_is_leaf( node ) )
        {
            node = dbvt_sort( t, node )->childs[ ( t.opath >> bit ) & 1 ];
            bit = ( bit + 1 ) & 31;
        }
        dbvt_node_t* start = dbvt_remove_leaf( t, node );
        if ( start )
            start = t.root;
        dbvt_insert_leaf( t, start, node );
        ++t.opath;
    } while ( --passes );
}