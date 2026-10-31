#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// VuWater's 2D bounding-box tree, recreated from retail's disassembly: the
// tree each VuWaterSurface keeps of the waves over it (surface +0xFC) and the
// one VuWater keeps of its surfaces (water +0x3C) are both this structure.
//
//   tree +0x00  root               +0x04  free-list head
//   tree +0x08  the chunk size for growth (64; the ctor @0x598c70 allocates
//               a first chunk of 128 through 0x598e50)
//   tree +0x0C  the std::list of chunks     +0x14  leaf count
//   tree +0x18  nodes in use                +0x1C  nodes free
//
//   node (32 bytes): +0 min x, +4 min y, +8 max x, +12 max y; +16 parent;
//   +20 the object on a leaf / the first child / the next free node; +24 the
//   second child, 0 on a leaf; +28 a tag (the wave tree's leaves carry their
//   surface there, which is what the query hands the kernels as ctx+92)
//
// Nodes are held by index (retail is 32-bit; this build is not), -1 for
// retail's null; global indices run through the chunks in allocation order.
// A leaf's object is an opaque int here - the caller's.
//
// This is the structure query.hxx's water_node_t walks (rect, first, second,
// tag); building a water_t from a water_tree_t is a field-for-field copy with
// second -1 on leaves.
struct water_tree_node_t
{
    float rect[ 4 ]{};
    int parent = -1;
    int first = -1;
    int second = -1;
    std::uint32_t tag = 0;
};

struct water_tree_t
{
    std::vector< water_tree_node_t > nodes;
    int root = -1;
    int free = -1;
    int chunk = 64;
    int leaves = 0;
    int used = 0;
    int free_count = 0;
};

// minss / maxss with retail's operand order (VuMath_min @0x403260 and its max
// sibling @0x403280: the second operand wins a tie or a NaN).
inline float water_min( float a, float b ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); }
inline float water_max( float a, float b ) { return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); }

// 0x598e50: a chunk of n zeroed nodes, each linked to the next, the last to
// the old free head; the free head becomes the chunk's first node.
inline void water_tree_grow( water_tree_t& t, int n )
{
    const int start = static_cast< int >( t.nodes.size( ) );
    t.nodes.resize( start + n );
    for ( int k = 0; k < n; ++k )
        t.nodes[ start + k ].first = start + k + 1;
    t.nodes[ start + n - 1 ].first = t.free;
    t.free_count += n;
    t.free = start;
}

// 0x598c70.
inline void water_tree_init( water_tree_t& t )
{
    t = water_tree_t{};
    water_tree_grow( t, 128 );
}

inline int water_tree_pop( water_tree_t& t )
{
    if ( t.free < 0 )
        water_tree_grow( t, t.chunk );
    const int n = t.free;
    t.free = t.nodes[ n ].first;
    return n;
}

// 0x598f00 (parent, leaf, sibling): a new internal node over both boxes.
inline int water_tree_parent( water_tree_t& t, int parent, int leaf, int sibling )
{
    const int n = water_tree_pop( t );
    water_tree_node_t& p = t.nodes[ n ];
    p.parent = parent;
    p.first = -1;
    p.second = -1;
    p.tag = 0;
    ++t.used;
    --t.free_count;
    const float* a = t.nodes[ leaf ].rect;
    const float* b = t.nodes[ sibling ].rect;
    p.rect[ 0 ] = water_min( a[ 0 ], b[ 0 ] );
    p.rect[ 1 ] = water_min( a[ 1 ], b[ 1 ] );
    p.rect[ 2 ] = water_max( a[ 2 ], b[ 2 ] );
    p.rect[ 3 ] = water_max( a[ 3 ], b[ 3 ] );
    return n;
}

inline void water_tree_refit( water_tree_t& t, int n )
{
    water_tree_node_t& p = t.nodes[ n ];
    const float* a = t.nodes[ p.first ].rect;
    const float* b = t.nodes[ p.second ].rect;
    p.rect[ 0 ] = water_min( a[ 0 ], b[ 0 ] );
    p.rect[ 1 ] = water_min( a[ 1 ], b[ 1 ] );
    p.rect[ 2 ] = water_max( a[ 2 ], b[ 2 ] );
    p.rect[ 3 ] = water_max( a[ 3 ], b[ 3 ] );
}

// comiss a, b; jb -> false: a >= b, ordered.
inline bool water_tree_ge( float a, float b ) { return _mm_comige_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; }

// 0x599010 (start, leaf): descend from `start` while internal, into the child
// whose summed centre is nearer the leaf's (|dx| + |dy| on min + max sums; the
// SECOND child on a tie or a NaN - setbe), pair the leaf with that node under
// a new parent, then refit upward until a parent already contains its child.
inline void water_tree_insert( water_tree_t& t, int start, int leaf )
{
    if ( t.root < 0 )
    {
        t.root = leaf;
        t.nodes[ leaf ].parent = -1;
        return;
    }
    int at = start;
    if ( t.nodes[ at ].second >= 0 )
    {
        const float* l = t.nodes[ leaf ].rect;
        const float sx = l[ 2 ] + l[ 0 ];
        const float sy = l[ 3 ] + l[ 1 ];
        do
        {
            const float* a = t.nodes[ t.nodes[ at ].first ].rect;
            const float* b = t.nodes[ t.nodes[ at ].second ].rect;
            const float d1 = std::fabs( sx - ( a[ 0 ] + a[ 2 ] ) ) + std::fabs( sy - ( a[ 3 ] + a[ 1 ] ) );
            const float d2 = std::fabs( sx - ( b[ 0 ] + b[ 2 ] ) ) + std::fabs( sy - ( b[ 3 ] + b[ 1 ] ) );
            const bool second = !_mm_comigt_ss( _mm_set_ss( d2 ), _mm_set_ss( d1 ) );
            at = second ? t.nodes[ at ].second : t.nodes[ at ].first;
        } while ( t.nodes[ at ].second >= 0 );
    }
    const int old = t.nodes[ at ].parent;
    const int p = water_tree_parent( t, old, leaf, at );
    if ( old >= 0 )
    {
        if ( t.nodes[ old ].second == at )
            t.nodes[ old ].second = p;
        else
            t.nodes[ old ].first = p;
    }
    t.nodes[ p ].first = at;
    t.nodes[ at ].parent = p;
    t.nodes[ p ].second = leaf;
    t.nodes[ leaf ].parent = p;
    if ( old < 0 )
    {
        t.root = p;
        return;
    }
    int child = p;
    for ( int up = old; up >= 0; up = t.nodes[ up ].parent )
    {
        const float* r = t.nodes[ child ].rect;
        const float* q = t.nodes[ up ].rect;
        if ( water_tree_ge( r[ 0 ], q[ 0 ] ) && water_tree_ge( r[ 1 ], q[ 1 ] ) && water_tree_ge( q[ 2 ], r[ 2 ] ) &&
             water_tree_ge( q[ 3 ], r[ 3 ] ) )
        {
            break;
        }
        water_tree_refit( t, up );
        child = up;
    }
}

// 0x598de0 (object, rect*): pop a node, make it a leaf, insert from the root.
inline int water_tree_add( water_tree_t& t, int object, const float* rect )
{
    const int n = water_tree_pop( t );
    water_tree_node_t& l = t.nodes[ n ];
    l.parent = -1;
    l.first = object;
    l.second = -1;
    l.tag = 0;
    ++t.used;
    --t.free_count;
    std::memcpy( l.rect, rect, 16 );
    water_tree_insert( t, t.root, n );
    ++t.leaves;
    return n;
}

// 0x599220 (leaf): the leaf's sibling takes its parent's place and the parent
// goes on the free list; then refit upward until a box comes out unchanged.
// The LEAF IS NOT FREED - its caller (0x529b90) does that. Returns the node
// the refit stopped at, the root when it ran out, or -1 (retail's 0) when the
// leaf was the root.
inline int water_tree_remove( water_tree_t& t, int leaf )
{
    if ( leaf == t.root )
    {
        t.root = -1;
        return -1;
    }
    const int parent = t.nodes[ leaf ].parent;
    const int grand = t.nodes[ parent ].parent;
    const int sibling = t.nodes[ parent ].second == leaf ? t.nodes[ parent ].first : t.nodes[ parent ].second;
    auto release = [ & ]( int n ) {
        t.nodes[ n ].first = t.free;
        --t.used;
        ++t.free_count;
        t.free = n;
    };
    if ( grand < 0 )
    {
        t.root = sibling;
        t.nodes[ sibling ].parent = -1;
        release( parent );
        return t.root;
    }
    if ( t.nodes[ grand ].second == parent )
        t.nodes[ grand ].second = sibling;
    else
        t.nodes[ grand ].first = sibling;
    t.nodes[ sibling ].parent = grand;
    release( parent );
    for ( int up = grand; up >= 0; up = t.nodes[ up ].parent )
    {
        float before[ 4 ];
        std::memcpy( before, t.nodes[ up ].rect, 16 );
        water_tree_refit( t, up );
        const float* r = t.nodes[ up ].rect;
        if ( before[ 0 ] == r[ 0 ] && before[ 1 ] == r[ 1 ] && before[ 2 ] == r[ 2 ] && before[ 3 ] == r[ 3 ] )
            return up;
    }
    return t.root;
}

// 0x529b90's leaf release, after 0x599220: the leaf back on the free list,
// one leaf and one node fewer.
inline void water_tree_release_leaf( water_tree_t& t, int leaf )
{
    t.nodes[ leaf ].first = t.free;
    --t.used;
    ++t.free_count;
    --t.leaves;
    t.free = leaf;
}

// ---------------------------------------------------------------------------
// VuWater HOLDING ITS WAVES: the add @0x529a90 and the removal @0x529b90, with
// the two surface-tree walks @0x52b4d0 (a box) and @0x52b3e0 (a point) and the
// per-surface test @0x529c60. Both open with a dynamics-world lock (through
// g_vudynamicsimpl 0x75900c) and a wait on the water renderer's thread (its
// +0x2B5 byte): platform layer, hooks, not modelled.
//
// What each object contributes, by retail offset:
//   surface  +0x90 .. +0xCF  world-to-surface rows (row-vector 4x4)
//            +0xD8 / +0xE8   the z band     +0xF0 / +0xF4  the half extents
//            +0xFC           its wave tree
//   wave     +0x0C .. +0x2B  its box (min x, y, z, -, max x, y, z, -)
//            +0x2C / +0x30   centre         +0x34 radius
//            +0x40           its entry list (pool nodes {leaf, next})
//            +0x44 bit 0     a POINT wave: tested by its box's centre alone
//   water    +0x08 / +0x10   the pool's free count / head; +0x3C the surface tree
struct water_held_surface_t
{
    float rows[ 16 ]{};
    float z_low = 0.0f;
    float z_high = 0.0f;
    float half[ 2 ]{};
    water_tree_t waves;
};

struct water_held_wave_t
{
    float box[ 8 ]{};
    float centre[ 2 ]{};
    float radius = 0.0f;
    bool point = false;
    int entries = -1;   // +0x40: the first pool node, -1 none
};

struct water_pool_node_t
{
    int leaf = -1;      // retail's leaf pointer: this leaf ...
    int surface = -1;   // ... of this surface's wave tree (the leaf's +28 tag)
    int next = -1;
};

struct water_hold_t
{
    water_tree_t surface_tree;   // leaves' objects: surface indices
    std::vector< water_held_surface_t > surfaces;
    std::vector< water_held_wave_t > waves;
    std::vector< water_pool_node_t > pool;
    int pool_free = -1;
    int pool_count = 0;
};

// 0x529c60 (water, wave, surface).
inline void water_hold_test( water_hold_t& w, int wave, int surface )
{
    water_held_wave_t& v = w.waves[ wave ];
    water_held_surface_t& s = w.surfaces[ surface ];
    if ( v.point )
    {
        const float z = ( v.box[ 6 ] + v.box[ 2 ] ) * 0.5f;
        if ( water_tree_ge( s.z_low, z ) || water_tree_ge( z, s.z_high ) )
            return;
    }
    else
    {
        const float* m = s.rows;
        const float lx = ( ( ( m[ 4 ] * v.centre[ 1 ] ) + ( m[ 0 ] * v.centre[ 0 ] ) ) + ( m[ 8 ] * 0.0f ) ) + m[ 12 ];
        const float ly = ( ( ( m[ 5 ] * v.centre[ 1 ] ) + ( m[ 1 ] * v.centre[ 0 ] ) ) + ( m[ 9 ] * 0.0f ) ) + m[ 13 ];
        const float ex = water_max( std::fabs( lx ) - s.half[ 0 ], 0.0f );
        const float ey = water_max( std::fabs( ly ) - s.half[ 1 ], 0.0f );
        if ( water_tree_ge( ( ey * ey ) + ( ex * ex ), v.radius * v.radius ) )
            return;
        if ( water_tree_ge( s.z_low, v.box[ 6 ] ) || water_tree_ge( v.box[ 2 ], s.z_high ) )
            return;
    }
    const float rect[ 4 ] = { v.box[ 0 ], v.box[ 1 ], v.box[ 4 ], v.box[ 5 ] };
    const int leaf = water_tree_add( s.waves, wave, rect );
    s.waves.nodes[ leaf ].tag = static_cast< std::uint32_t >( surface );
    const int e = w.pool_free;
    w.pool_free = w.pool[ e ].next;
    --w.pool_count;
    w.pool[ e ].leaf = leaf;
    w.pool[ e ].surface = surface;
    w.pool[ e ].next = v.entries;
    v.entries = e;
}

// The walks @0x52b4d0 / @0x52b3e0: an explicit stack, a node's first child
// pushed before its second (so the second is visited first); every surface
// leaf whose box overlaps the query goes to 0x529c60. Retail's stack is 257
// words with no bound check; a tree deep enough to overflow it is not modelled.
inline void water_hold_add( water_hold_t& w, int wave )
{
    const water_held_wave_t& v = w.waves[ wave ];
    const water_tree_t& t = w.surface_tree;
    if ( t.root < 0 )
        return;
    float q[ 4 ];
    if ( v.point )
    {
        q[ 0 ] = ( v.box[ 4 ] + v.box[ 0 ] ) * 0.5f;
        q[ 1 ] = ( v.box[ 5 ] + v.box[ 1 ] ) * 0.5f;
    }
    else
    {
        q[ 0 ] = v.box[ 0 ];
        q[ 1 ] = v.box[ 1 ];
        q[ 2 ] = v.box[ 4 ];
        q[ 3 ] = v.box[ 5 ];
    }
    std::vector< int > stack{ t.root };
    while ( !stack.empty( ) )
    {
        const int n = stack.back( );
        stack.pop_back( );
        const float* r = t.nodes[ n ].rect;
        const bool hit = v.point
            ? ( water_tree_ge( q[ 0 ], r[ 0 ] ) && water_tree_ge( q[ 1 ], r[ 1 ] ) && water_tree_ge( r[ 2 ], q[ 0 ] ) &&
                  water_tree_ge( r[ 3 ], q[ 1 ] ) )
            : ( water_tree_ge( q[ 2 ], r[ 0 ] ) && water_tree_ge( r[ 2 ], q[ 0 ] ) && water_tree_ge( q[ 3 ], r[ 1 ] ) &&
                  water_tree_ge( r[ 3 ], q[ 1 ] ) );
        if ( !hit )
            continue;
        if ( t.nodes[ n ].second >= 0 )
        {
            stack.push_back( t.nodes[ n ].first );
            stack.push_back( t.nodes[ n ].second );
        }
        else
        {
            water_hold_test( w, wave, t.nodes[ n ].first );
        }
    }
}

// 0x529b90 (water, wave): until its list is empty, remove the head entry's
// leaf from its tagged surface's tree, free the leaf, unlink the FIRST entry
// holding that leaf and pool it. (Retail would loop forever if no entry held
// it; the head always does.)
inline void water_hold_remove( water_hold_t& w, int wave )
{
    water_held_wave_t& v = w.waves[ wave ];
    while ( v.entries >= 0 )
    {
        const int leaf = w.pool[ v.entries ].leaf;
        const int surface = w.pool[ v.entries ].surface;
        water_tree_t& tree = w.surfaces[ surface ].waves;
        water_tree_remove( tree, leaf );
        water_tree_release_leaf( tree, leaf );
        int prev = -1;
        int e = v.entries;
        while ( e >= 0 && !( w.pool[ e ].leaf == leaf && w.pool[ e ].surface == surface ) )
        {
            prev = e;
            e = w.pool[ e ].next;
        }
        if ( prev >= 0 )
            w.pool[ prev ].next = w.pool[ e ].next;
        else
            v.entries = w.pool[ e ].next;
        w.pool[ e ].next = w.pool_free;
        ++w.pool_count;
        w.pool_free = e;
    }
}