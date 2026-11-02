#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// THE RENDER'S AABB TREE (render layer, platform independent), recreated 1:1 from the disassembly (water batch 30). The class name is
// not in the binary's RTTI; "dbrt" here is only this file's name. Vu3dDrawManager (+4) and VuLightManager (+0xFC) each own one.
// Same shape as btDbvt, but a separate piece of code: no margins, no prediction, no leaf volumes grown (engine collision/arena.hxx
// holds Bullet's 0x5942E0 / 0x594480, which this is not).
//   The tree (12 bytes): +0 the root, +4 one cached free node, +8 the leaf count (kept by the callers).
//   The node (0x2C, ctor 0x4109E0): +0 min, +0x10 max, +0x20 parent, +0x24 / +0x28 the children; a leaf has +0x28 = 0 and +0x24 its data.

struct dbrt_node_t
{
    float m_min[ 3 ] = { }, m_max[ 3 ] = { };
    dbrt_node_t* m_parent = nullptr;
    dbrt_node_t* m_child[ 2 ] = { };    // +0x24 / +0x28; m_child[ 1 ] null = a leaf
    void* m_data = nullptr;             // +0x24 of a leaf
    bool leaf( ) const { return m_child[ 1 ] == nullptr; }
};

struct dbrt_tree_t
{
    ~dbrt_tree_t( ) { clear( ); }

    // 0x5C27F0: every node freed, the cached one too.
    void clear( )
    {
        if ( m_root )
            free_all( m_root );
        m_root = nullptr;
        delete m_free;
        m_free = nullptr;
    }

    // 0x5C29E0.
    void free_all( dbrt_node_t* n )
    {
        if ( !n->leaf( ) )
        {
            free_all( n->m_child[ 0 ] );
            free_all( n->m_child[ 1 ] );
        }
        release( n );
    }

    dbrt_node_t* take( )
    {
        dbrt_node_t* n = m_free;
        if ( n )
        {
            m_free = nullptr;
            *n = dbrt_node_t( );
            return n;
        }
        return new dbrt_node_t( );
    }

    // The cached node's slot: what it held is freed.
    void release( dbrt_node_t* n )
    {
        delete m_free;
        m_free = n;
    }

    // 0x5C2860: a leaf over 32 bytes of aabb.
    dbrt_node_t* leaf( const float* aabb, void* data )
    {
        dbrt_node_t* n = take( );
        n->m_data = data;
        std::memcpy( n->m_min, aabb, 12 );
        std::memcpy( n->m_max, aabb + 4, 12 );
        return n;
    }

    static void merge( dbrt_node_t& out, const dbrt_node_t& a, const dbrt_node_t& b )
    {
        // minss / maxss with the first operand a: b comes back on a NaN.
        for ( int i = 0; i < 3; ++i )
        {
            out.m_min[ i ] = a.m_min[ i ] < b.m_min[ i ] ? a.m_min[ i ] : b.m_min[ i ];
            out.m_max[ i ] = a.m_max[ i ] > b.m_max[ i ] ? a.m_max[ i ] : b.m_max[ i ];
        }
    }

    // |y| + |x| + |z| of the centre sums, in the additions' order.
    static float proximity( const dbrt_node_t& n, const float centre[ 3 ] )
    {
        const float dx = centre[ 0 ] - ( n.m_min[ 0 ] + n.m_max[ 0 ] );
        const float dy = centre[ 1 ] - ( n.m_min[ 1 ] + n.m_max[ 1 ] );
        const float dz = centre[ 2 ] - ( n.m_min[ 2 ] + n.m_max[ 2 ] );
        return std::fabs( dy ) + std::fabs( dx ) + std::fabs( dz );
    }

    // 0x5C2A30 (start, leaf): down to the nearer child (child 1 on a tie or an unordered compare), a new parent (0x5C28E0) over the
    // sibling and the leaf, then refit upward until a parent already contains its child.
    void insert( dbrt_node_t* start, dbrt_node_t* leaf_node )
    {
        if ( !m_root )
        {
            m_root = leaf_node;
            leaf_node->m_parent = nullptr;
            return;
        }
        const float centre[ 3 ] = { leaf_node->m_min[ 0 ] + leaf_node->m_max[ 0 ], leaf_node->m_max[ 1 ] + leaf_node->m_min[ 1 ],
                                    leaf_node->m_max[ 2 ] + leaf_node->m_min[ 2 ] };
        dbrt_node_t* sibling = start;
        while ( !sibling->leaf( ) )
        {
            const float d0 = proximity( *sibling->m_child[ 0 ], centre ), d1 = proximity( *sibling->m_child[ 1 ], centre );
            sibling = sibling->m_child[ d1 > d0 ? 0 : 1 ];
        }
        dbrt_node_t* prev = sibling->m_parent;
        dbrt_node_t* node = take( );
        node->m_parent = prev;
        merge( *node, *leaf_node, *sibling );
        if ( !prev )
        {
            node->m_child[ 0 ] = sibling;
            sibling->m_parent = node;
            node->m_child[ 1 ] = leaf_node;
            leaf_node->m_parent = node;
            m_root = node;
            return;
        }
        prev->m_child[ prev->m_child[ 1 ] == sibling ? 1 : 0 ] = node;
        node->m_child[ 0 ] = sibling;
        sibling->m_parent = node;
        node->m_child[ 1 ] = leaf_node;
        leaf_node->m_parent = node;
        do
        {
            bool inside = true;
            for ( int i = 0; i < 3 && inside; ++i )
                inside = node->m_min[ i ] >= prev->m_min[ i ];
            for ( int i = 0; i < 3 && inside; ++i )
                inside = prev->m_max[ i ] >= node->m_max[ i ];
            if ( inside )
                return;
            merge( *prev, *prev->m_child[ 0 ], *prev->m_child[ 1 ] );
            node = prev;
            prev = prev->m_parent;
        } while ( prev );
    }

    // 0x5C2D40: the leaf's parent is cached, the sibling takes its place, the ancestors refit until one's box does not change.
    // Returns that ancestor, else the root. The leaf itself is the caller's to free.
    dbrt_node_t* remove( dbrt_node_t* leaf_node )
    {
        if ( leaf_node == m_root )
        {
            m_root = nullptr;
            return nullptr;
        }
        dbrt_node_t* parent = leaf_node->m_parent;
        dbrt_node_t* prev = parent->m_parent;
        dbrt_node_t* sibling = parent->m_child[ parent->m_child[ 1 ] == leaf_node ? 0 : 1 ];
        if ( !prev )
        {
            m_root = sibling;
            sibling->m_parent = nullptr;
            release( parent );
            return m_root;
        }
        prev->m_child[ prev->m_child[ 1 ] == parent ? 1 : 0 ] = sibling;
        sibling->m_parent = prev;
        release( parent );
        for ( dbrt_node_t* n = prev; n; n = n->m_parent )
        {
            const dbrt_node_t old = *n;
            merge( *n, *n->m_child[ 0 ], *n->m_child[ 1 ] );
            bool same = true;
            for ( int i = 0; i < 3; ++i )
                same = same && old.m_min[ i ] == n->m_min[ i ] && old.m_max[ i ] == n->m_max[ i ];
            if ( same )
                return n;
        }
        return m_root;
    }

    // Vu3dDrawManager 0x5A78B0: a new leaf over `aabb`, inserted from the root, counted.
    dbrt_node_t* add( const float* aabb, void* data )
    {
        dbrt_node_t* n = leaf( aabb, data );
        insert( m_root, n );
        ++m_count;
        return n;
    }

    // Vu3dDrawManager 0x5A78F0: removed, and the leaf itself takes the cached slot.
    void erase( dbrt_node_t* n )
    {
        remove( n );
        release( n );
        --m_count;
    }

    // Vu3dDrawManager 0x5A7930: removed, the new aabb, inserted from the root.
    void move( dbrt_node_t* n, const float* aabb )
    {
        remove( n );
        std::memcpy( n->m_min, aabb, 12 );
        std::memcpy( n->m_max, aabb + 4, 12 );
        insert( m_root, n );
    }

    dbrt_node_t* m_root = nullptr;
    dbrt_node_t* m_free = nullptr;
    int m_count = 0;
};

// 0x562510 (ecx min, edx max, plane, the sign index: bit 0 x, 1 y, 2 z >= 0): -1 the box is wholly behind, 1 wholly in front, 0 across.
inline int dbrt_plane_box( const float* min, const float* max, const float* plane, int sign )
{
    const float px = sign & 1 ? max[ 0 ] : min[ 0 ], py = sign & 2 ? max[ 1 ] : min[ 1 ], pz = sign & 4 ? max[ 2 ] : min[ 2 ];
    const float nx = sign & 1 ? min[ 0 ] : max[ 0 ], ny = sign & 2 ? min[ 1 ] : max[ 1 ], nz = sign & 4 ? min[ 2 ] : max[ 2 ];
    if ( 0.0f > plane[ 0 ] * px + plane[ 1 ] * py + plane[ 2 ] * pz + plane[ 3 ] )
        return -1;
    return plane[ 0 ] * nx + plane[ 1 ] * ny + plane[ 2 ] * nz + plane[ 3 ] >= 0.0f ? 1 : 0;
}

inline int dbrt_plane_sign( const float* plane )
{
    return ( plane[ 2 ] >= 0.0f ? 4 : 0 ) + ( plane[ 0 ] >= 0.0f ? 1 : 0 ) + ( plane[ 1 ] >= 0.0f ? 2 : 0 );
}

// 0x5A8010 / 0x5A8040: a subtree wholly inside, its leaves in order.
template < typename F >
inline void dbrt_visit_all( const dbrt_node_t* n, F& leaf )
{
    while ( !n->leaf( ) )
    {
        dbrt_visit_all( n->m_child[ 0 ], leaf );
        n = n->m_child[ 1 ];
    }
    leaf( n->m_data );
}

// 0x5A7B30 / 0x5A7DB0 (the two differ only in the leaf): the walk under `count` planes of 4 floats. A stack of (node, mask of the planes
// the node is wholly in front of). Child 1 is pushed last, so it is visited first. Retail's stack is 256 entries with no check.
template < typename F >
inline void dbrt_walk( const dbrt_node_t* root, const float* planes, int count, F&& leaf )
{
    if ( !root )
        return;
    const std::uint32_t all = ( 1u << count ) - 1u;
    std::vector< int > signs( static_cast< std::size_t >( count ) );
    for ( int i = 0; i < count; ++i )
        signs[ i ] = dbrt_plane_sign( planes + 4 * i );
    struct entry_t
    {
        const dbrt_node_t* m_node;
        std::uint32_t m_mask;
    };
    std::vector< entry_t > stack;
    stack.push_back( { root, 0u } );
    while ( !stack.empty( ) )
    {
        const entry_t e = stack.back( );
        stack.pop_back( );
        std::uint32_t mask = e.m_mask;
        bool outside = false;
        for ( int i = 0; i < count && !outside; ++i )
        {
            const std::uint32_t bit = 1u << i;
            if ( mask & bit )
                continue;
            const int side = dbrt_plane_box( e.m_node->m_min, e.m_node->m_max, planes + 4 * i, signs[ i ] );
            if ( side == -1 )
                outside = true;
            else if ( side == 1 )
                mask |= bit;
        }
        if ( outside )
            continue;
        if ( e.m_node->leaf( ) )
            leaf( e.m_node->m_data );
        else if ( mask != all )
        {
            stack.push_back( { e.m_node->m_child[ 0 ], mask } );
            stack.push_back( { e.m_node->m_child[ 1 ], mask } );
        }
        else
        {
            dbrt_visit_all( e.m_node->m_child[ 0 ], leaf );
            dbrt_visit_all( e.m_node->m_child[ 1 ], leaf );
        }
    }
}
