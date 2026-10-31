#pragma once

#include <workspace/core/world/wave.hxx>

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
    float below = 0.0f;   // +0x28, read by the point lookup @0x52aec0
    float above = 0.0f;   // +0x2C
    water_tree_t waves;
};

struct water_held_wave_t
{
    float box[ 8 ]{};
    float centre[ 2 ]{};
    float radius = 0.0f;
    bool point = false;
    int entries = -1;   // +0x40: the first pool node, -1 none
    // its type chain (vt slot 1) includes VuWaterWakeWave's descriptor 0x7368b8 - the only waves the wake
    // lookup @0x5296c0 evaluates - and then its two records
    bool wake = false;
    wake_wave_t wake_data{};
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
    int list = -1;   // +0x30: the wave list's head pool node (its leaf = the wave), -1 none
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
// ---------------------------------------------------------------------------
// THE WAKE LOOKUP @0x5296c0 (thiscall water; const VuVector4* point,
// VuBestWakeFinder* finder; retn 8) - what the boat's Decision tick 0x41f740
// asks: the best-aligned live wake under a point.
//
//   if water +0x18 (skip waves) is set: nothing.
//   0x52aec0 walks the surface tree at (x, y) (point-in-box, second child
//   first) and keeps the LAST surface whose space holds the point:
//   half_x >= |lx|, half_y >= |ly|, above (+0x2C) >= lz >= -below (+0x28).
//   0x52b270 walks that surface's wave tree at (x, y); each leaf whose type
//   chain includes VuWaterWakeWave (0x7368b8) and whose circle (+0x2C / +0x30,
//   radius +0x34) holds the point goes to the finder, VuBestWakeFinder
//   @0x421e50; any other leaf is passed over (the flat-wake type 0x7368c0 is
//   looked for and nothing done with it).
//
// The finder, by retail offset: +0x04 the point (x, y) the wake is sampled
// at, +0x0C / +0x10 a direction, +0x14 the answer (only ever raised).
struct water_wake_finder_t
{
    float point[ 2 ]{};
    float direction[ 2 ]{};
    float best = 0.0f;
};

// 0x52bf10 (thiscall wake; const float* xy, record* out; al): the wake's two
// records blended at the point as wake_vertex blends them, into `out`, with
// the same three rejections (between the cross-sections, inside the blended
// radius, the ring has travelled out to it). out->w (+12) is a stack word
// retail never writes: left untouched here.
inline bool wake_segment( const wake_wave_t& wave, float x, float y, wake_record_t& out )
{
    const wake_record_t& a = wave.a;
    const wake_record_t& b = wave.b;
    const float da = ( a.direction_y * ( y - a.y ) ) + ( a.direction_x * ( x - a.x ) );
    const float db = ( b.direction_y * ( y - b.y ) ) + ( b.direction_x * ( x - b.x ) );
    if ( water_tree_ge( db * da, 0.0f ) )
        return false;
    const float t = db / ( db - da );
    const float s = 1.0f - t;
    auto blend = [ & ]( float av, float bv ) { return ( av * t ) + ( bv * s ); };
    out.x = blend( a.x, b.x );
    out.y = blend( a.y, b.y );
    out.z = blend( a.z, b.z );
    out.radius = ( b.radius * s ) + ( a.radius * t );
    const float d2 = ( ( y - out.y ) * ( y - out.y ) ) + ( ( x - out.x ) * ( x - out.x ) );
    if ( !_mm_comigt_ss( _mm_set_ss( out.radius * out.radius ), _mm_set_ss( d2 ) ) )
        return false;
    out.age = ( b.age * s ) + ( a.age * t );
    out.speed = ( b.speed * s ) + ( a.speed * t );
    const float travelled = _mm_cvtss_f32( _mm_max_ss( _mm_setzero_ps( ),
        _mm_set_ss( _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( d2 ) ) ) - ( wave.inner_ratio * out.radius ) ) ) );
    if ( !_mm_comigt_ss( _mm_set_ss( out.age - ( travelled / out.speed ) ), _mm_setzero_ps( ) ) )
        return false;
    out.life = ( b.life * s ) + ( a.life * t );
    out.amplitude = ( b.amplitude * s ) + ( a.amplitude * t );
    out.direction_y = blend( a.direction_y, b.direction_y );
    out.direction_x = blend( a.direction_x, b.direction_x );
    out.frequency = ( b.frequency * s ) + ( a.frequency * t );
    out.fade_time = ( b.fade_time * s ) + ( a.fade_time * t );
    return true;
}

// VuBestWakeFinder @0x421e50 (thiscall finder, wake): score = alignment *
// freshness, alignment = clamp( direction . the record's direction, 0, 1 ),
// freshness = clamp( (2 - age) * 0.5263158, 0, 1 ) (both minss with 1 then
// maxss with 0, retail's order); the answer is raised to it (maxss).
inline void best_wake_process( water_wake_finder_t& f, const wake_wave_t& wave )
{
    wake_record_t r{};
    if ( !wake_segment( wave, f.point[ 0 ], f.point[ 1 ], r ) )
        return;
    auto clamp01 = []( float v ) {
        const float m = _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( 1.0f ), _mm_set_ss( v ) ) );
        return _mm_cvtss_f32( _mm_max_ss( _mm_setzero_ps( ), _mm_set_ss( m ) ) );
    };
    const float align = clamp01( ( f.direction[ 1 ] * r.direction_y ) + ( f.direction[ 0 ] * r.direction_x ) );
    const float fresh = clamp01( ( 2.0f - r.age ) * 0.5263158f );
    f.best = _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( f.best ), _mm_set_ss( fresh * align ) ) );
}

// @0x5296c0. `skip_waves` is water +0x18.
inline void water_best_wake( const water_hold_t& w, bool skip_waves, const float* point, water_wake_finder_t& finder )
{
    if ( skip_waves || w.surface_tree.root < 0 )
        return;
    const float px = point[ 0 ];
    const float py = point[ 1 ];
    auto inside = [ & ]( const float* r ) {
        return water_tree_ge( px, r[ 0 ] ) && water_tree_ge( py, r[ 1 ] ) && water_tree_ge( r[ 2 ], px ) && water_tree_ge( r[ 3 ], py );
    };
    int found = -1;
    std::vector< int > stack{ w.surface_tree.root };
    while ( !stack.empty( ) )
    {
        const int n = stack.back( );
        stack.pop_back( );
        const water_tree_node_t& node = w.surface_tree.nodes[ n ];
        if ( !inside( node.rect ) )
            continue;
        if ( node.second >= 0 )
        {
            stack.push_back( node.first );
            stack.push_back( node.second );
            continue;
        }
        const water_held_surface_t& s = w.surfaces[ node.first ];
        const float* m = s.rows;
        const float lx = ( ( ( m[ 4 ] * point[ 1 ] ) + ( point[ 0 ] * m[ 0 ] ) ) + ( m[ 8 ] * point[ 2 ] ) ) + m[ 12 ];
        const float ly = ( ( ( m[ 1 ] * point[ 0 ] ) + ( m[ 5 ] * point[ 1 ] ) ) + ( m[ 9 ] * point[ 2 ] ) ) + m[ 13 ];
        const float lz = ( ( ( m[ 2 ] * point[ 0 ] ) + ( m[ 6 ] * point[ 1 ] ) ) + ( m[ 10 ] * point[ 2 ] ) ) + m[ 14 ];
        if ( water_tree_ge( s.half[ 0 ], std::fabs( lx ) ) && water_tree_ge( s.half[ 1 ], std::fabs( ly ) ) &&
             water_tree_ge( s.above, lz ) && water_tree_ge( lz, -s.below ) )
            found = node.first;
    }
    if ( found < 0 )
        return;
    const water_tree_t& t = w.surfaces[ found ].waves;
    if ( t.root < 0 )
        return;
    stack.assign( 1, t.root );
    while ( !stack.empty( ) )
    {
        const int n = stack.back( );
        stack.pop_back( );
        const water_tree_node_t& node = t.nodes[ n ];
        if ( !inside( node.rect ) )
            continue;
        if ( node.second >= 0 )
        {
            stack.push_back( node.first );
            stack.push_back( node.second );
            continue;
        }
        const water_held_wave_t& v = w.waves[ node.first ];
        if ( !v.wake )
            continue;
        // the circle test is against the QUERY point; the finder samples the wake at its own +0x04
        const float dy = v.centre[ 1 ] - py;
        const float dx = v.centre[ 0 ] - px;
        if ( _mm_comigt_ss( _mm_set_ss( v.radius * v.radius ), _mm_set_ss( ( dy * dy ) + ( dx * dx ) ) ) )
            best_wake_process( finder, v.wake_data );
    }
}