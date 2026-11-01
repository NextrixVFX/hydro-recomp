#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>
#include <emmintrin.h>

#include <workspace/core/collision/concave.hxx>

// btOptimizedBvh::build (Bullet 2.79) as retail runs it at level load (boat
// batch 23), recreated from the disassembly of 0x5AF380, 0x582CB0, 0x583540,
// 0x586640, 0x587090, 0x586C30, 0x5887B0, 0x5868B0 and the node helpers
// 0x586350 / 0x588870 / 0x58A570 / 0x58A4B0 / 0x58A290 (their retail-memory
// twins are arena.hxx's bt_qbvh_*; these are the typed ones the world's
// bodies need).
//
// WHO REACHES IT: createRigidBody 0x544100 on a "triangle mesh" body installs
// the asset's VuOptimizedBvh with setOptimizedBvh 0x5AF400 under (1, 1, 1, 0),
// then, when the entity's transform scale is not (1, 1, 1), calls the shape's
// setLocalScaling (vt+0x14) = btBvhTriangleMeshShape::setLocalScaling 0x5AF300,
// whose rebuild branch is buildOptimizedBvh 0x5AF380: a NEW tree over the
// rescaled mesh. 59 props in the level templates take it (Area51, Tropical,
// Pack01Track01 / 03: tools tmp/b23/rbscale.py). The asset factory's second
// registered function for VuCollisionMeshAsset (0x518F50 -> 0x519CC0, which
// also reaches 0x582CB0) is the BAKE: only VuAssetFactoryImpl slot 17
// 0x4EE3B0 calls a type's +0x10, and nothing calls slot 17 but itself.
//
// SUBSTITUTIONS (flagged): std::vector for btAlignedObjectArray - the growth
// policy (capacity doubling, the copies) and the gNumAlignedAlloc 0x759054 /
// gNumAlignedFree 0x759050 counters are not modelled; btBvhSubtreeInfo's 12
// padding bytes are not kept (retail copies a default-constructed temporary
// with no initialiser: stack garbage).
//
// Only the quantized build is recreated: btBvhTriangleMeshShape's ctor sets
// m_useQuantizedAabbCompression (+0x48) = 1 and nothing in the image clears
// it, so 0x582CB0's NodeTriangleCallback branch (0x583210) answers -1. So does
// a mesh with no triangles (retail's buildTree writes node 0 of an empty array).

// btBvhSubtreeInfo (32 bytes): +0 quantized min, +6 max, +0xC root node
// index, +0x10 subtree size, +0x14 padding.
struct bvh_subtree_t
{
    uint16_t min[ 3 ];
    uint16_t max[ 3 ];
    int32_t root;
    int32_t size;
};

// A btOptimizedBvh built at runtime (alloc 0xC0, ctor 0x586220, vtable
// 0x74FEB4). `view` is what the walk reads (bvh.hxx); its `nodes` points into
// `nodes` (re-pointed by bvh_build_optimized).
struct optimized_bvh_t
{
    quantized_bvh_t view{ };
    int bullet_version = 0;               // +0x40
    std::vector< bvh_node_t > leaves;     // +0x74 m_quantizedLeafNodes (cleared after the build)
    std::vector< bvh_node_t > nodes;      // +0x88 m_quantizedContiguousNodes
    std::vector< bvh_subtree_t > subtrees;   // +0xA0 m_SubtreeHeaders
    int subtree_header_count = 0;         // +0xB4
};

// btQuantizedBvh ctor 0x586220: bulletVersion 279, useQuantization 0, the five
// arrays empty, traversal mode 0, aabb min (-FLT_MAX x3, 0) (0xFF7FFFFF),
// max (FLT_MAX x3, 0) (0x7F7FFFFF), the quantization unwritten (0 here).
inline void bvh_build_construct( optimized_bvh_t& b )
{
    b = optimized_bvh_t{ };
    b.bullet_version = 279;
    for ( int k = 0; k < 3; ++k )
    {
        b.view.aabb_min[ k ] = support_bits( 0xFF7FFFFFu );
        b.view.aabb_max[ k ] = support_bits( 0x7F7FFFFFu );
    }
}

// setQuantizationValues 0x586350 (margin 1.0 inlined): min - 1.0, max + 1.0,
// 65533.0 (0x6F7844) / (stored max - stored min), w 0, useQuantization 1.
inline void bvh_build_set_quantization( optimized_bvh_t& b, const float* mn, const float* mx )
{
    for ( int k = 0; k < 3; ++k )
        b.view.aabb_min[ k ] = mn[ k ] - 1.0f;
    b.view.aabb_min[ 3 ] = 0.0f;
    for ( int k = 0; k < 3; ++k )
        b.view.aabb_max[ k ] = mx[ k ] + 1.0f;
    b.view.aabb_max[ 3 ] = 0.0f;
    for ( int k = 0; k < 3; ++k )
        b.view.quantization[ k ] = support_bits( 0x477FFD00u ) / ( b.view.aabb_max[ k ] - b.view.aabb_min[ k ] );
    b.view.quantization[ 3 ] = 0.0f;
    b.view.use_quantization = 1;
}

// The unclamped quantize every build site inlines: quant * (p - bvhMin)
// through cvttss2si (truncation; NaN / overflow -> 0x80000000), min & 0xFFFE,
// max (+ 1.0 first) | 1, kept as u16.
inline uint16_t bvh_build_quantize( const optimized_bvh_t& b, float p, int k, bool is_max )
{
    const float q = b.view.quantization[ k ] * ( p - b.view.aabb_min[ k ] );
    const int v = is_max ? _mm_cvtt_ss2si( _mm_set_ss( q + 1.0f ) ) | 1 : _mm_cvtt_ss2si( _mm_set_ss( q ) ) & 0xFFFE;
    return static_cast< uint16_t >( v );
}

// QuantizedNodeTriangleCallback::internalProcessTriangleIndex 0x583540
// (thiscall cb; triangle*, part, index): min starts 1e18 (0x6F7858), max
// -1e18 (0x6F7908); corner 0 replaces a bound only when strictly inside it
// (`comiss; jbe`: a NaN lane keeps the start), corners 1 and 2 by setMin /
// setMax (bound > v -> v; v > bound -> v); per axis x, y, z: 0.002 (0x6F75B8)
// > max - min -> max += 0.001 (0x6F75B0), min -= 0.001; the six lanes
// quantized; escape = part << 21 | index; push_back.
inline void bvh_build_triangle( optimized_bvh_t& b, const float ( *t )[ 4 ], int part, int index )
{
    const float big = support_bits( 0x5D5E0B6Bu ), small = support_bits( 0xDD5E0B6Bu );
    float lo[ 3 ], hi[ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        lo[ k ] = big > t[ 0 ][ k ] ? t[ 0 ][ k ] : big;
        hi[ k ] = t[ 0 ][ k ] > small ? t[ 0 ][ k ] : small;
    }
    for ( int c = 1; c < 3; ++c )
    {
        for ( int k = 0; k < 3; ++k )
            if ( lo[ k ] > t[ c ][ k ] )
                lo[ k ] = t[ c ][ k ];
        for ( int k = 0; k < 3; ++k )
            if ( t[ c ][ k ] > hi[ k ] )
                hi[ k ] = t[ c ][ k ];
    }
    const float dim = support_bits( 0x3B03126Fu ), half = support_bits( 0x3A83126Fu );
    for ( int k = 0; k < 3; ++k )
    {
        if ( dim > hi[ k ] - lo[ k ] )
        {
            hi[ k ] = hi[ k ] + half;
            lo[ k ] = lo[ k ] - half;
        }
    }
    bvh_node_t n;
    for ( int k = 0; k < 3; ++k )
        n.min[ k ] = bvh_build_quantize( b, lo[ k ], k, false );
    for ( int k = 0; k < 3; ++k )
        n.max[ k ] = bvh_build_quantize( b, hi[ k ], k, true );
    n.escape_or_triangle = static_cast< int32_t >( static_cast< uint32_t >( part ) << 21 | static_cast< uint32_t >( index ) );
    b.leaves.push_back( n );
}

// VuStridingMesh::InternalProcessAllTriangles 0x5180A0 (vt 0x7380C4 slot 1)
// on what its getLockedReadOnlyVertexIndexBase 0x527C50 hands out (PHY_FLOAT
// vertices, PHY_SHORT indices, faces = index count / 3; getNumSubParts
// 0x527CC0 = index count != 0): per face its three corners in order, vertex *
// scaling (lane by lane, w 0), then the callback (part, face).
template < typename F > inline void bvh_build_mesh_triangles( const striding_mesh_t& mesh, F&& callback )
{
    const int parts = mesh.index_count != 0 ? 1 : 0;
    for ( int part = 0; part < parts; ++part )
    {
        const int faces = mesh.index_count / 3;
        for ( int f = 0; f < faces; ++f )
        {
            const uint16_t* tri = mesh.indices + 3 * f;
            float corner[ 3 ][ 4 ];
            for ( int k = 0; k < 3; ++k )
            {
                const float* v = mesh.vertices[ tri[ k ] ];
                corner[ k ][ 0 ] = v[ 0 ] * mesh.scaling[ 0 ];
                corner[ k ][ 1 ] = v[ 1 ] * mesh.scaling[ 1 ];
                corner[ k ][ 2 ] = v[ 2 ] * mesh.scaling[ 2 ];
                corner[ k ][ 3 ] = 0.0f;
            }
            callback( corner, part, f );
        }
    }
}

// Lane k of a quantized leaf's min (lane 0..2) or max (3..5) as a float: the
// u16 through cvtdq2ps.
inline float bvh_build_u( const bvh_node_t& n, int lane )
{
    return static_cast< float >( static_cast< int >( lane < 3 ? n.min[ lane ] : n.max[ lane - 3 ] ) );
}

// calcSplittingAxis 0x587090 (thiscall start, end; retn 8), quantized: the
// centres' mean (inv = 1.0 / q per lane, min + u * inv - see each lane), then
// the variance sum of (centre - mean)^2, scaled by 1.0 / (n - 1.0); the axis
// of the largest (y > x ? 1 + (z > y) : (z > x ? 2 : 0); unordered compares
// are false).
inline int bvh_build_calc_splitting_axis( const optimized_bvh_t& b, int start, int end )
{
    const float* mn = b.view.aabb_min;
    const float* q = b.view.quantization;
    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
    for ( int i = start; i < end; ++i )
    {
        const bvh_node_t& n = b.leaves[ i ];
        const float ix = 1.0f / q[ 0 ], iy = 1.0f / q[ 1 ], iz = 1.0f / q[ 2 ];
        const float lx = mn[ 0 ] + bvh_build_u( n, 0 ) * ix;
        const float ly = mn[ 1 ] + bvh_build_u( n, 1 ) * iy;
        const float lz = mn[ 2 ] + bvh_build_u( n, 2 ) * iz;
        const float hx = mn[ 0 ] + bvh_build_u( n, 3 ) * ix;
        const float hy = bvh_build_u( n, 4 ) * iy + mn[ 1 ];
        const float hz = bvh_build_u( n, 5 ) * iz + mn[ 2 ];
        sz = ( hz + lz ) * 0.5f + sz;
        sx = ( hx + lx ) * 0.5f + sx;
        sy = ( hy + ly ) * 0.5f + sy;
    }
    const float count = static_cast< float >( end - start );
    const float inv = 1.0f / count;
    const float meanx = inv * sx, meanz = sz * inv, meany = inv * sy;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    for ( int i = start; i < end; ++i )
    {
        const bvh_node_t& n = b.leaves[ i ];
        const float iy = 1.0f / q[ 1 ], iz = 1.0f / q[ 2 ], ix = 1.0f / q[ 0 ];
        const float ly = bvh_build_u( n, 1 ) * iy + mn[ 1 ];
        const float lz = bvh_build_u( n, 2 ) * iz + mn[ 2 ];
        const float lx = bvh_build_u( n, 0 ) * ix + mn[ 0 ];
        const float hx = bvh_build_u( n, 3 ) * ix + mn[ 0 ];
        const float hy = bvh_build_u( n, 4 ) * iy + mn[ 1 ];
        const float hz = bvh_build_u( n, 5 ) * iz + mn[ 2 ];
        const float dz = ( hz + lz ) * 0.5f - meanz;
        const float dx = ( hx + lx ) * 0.5f - meanx;
        const float dy = ( hy + ly ) * 0.5f - meany;
        vz = dz * dz + vz;
        vx = dx * dx + vx;
        vy = dy * dy + vy;
    }
    const float scale = 1.0f / ( count - 1.0f );
    vx = vx * scale;
    vy = vy * scale;
    vz = vz * scale;
    if ( vy > vx )
        return vz > vy ? 2 : 1;
    return vz > vx ? 2 : 0;
}

// sortAndCalcSplittingIndex 0x586C30 (thiscall start, end, axis; retn 0Ch),
// quantized: the centres' mean (the same form, its own operand order), then
// per leaf in order its centre (u / q + min this time) on the axis > the
// mean's (`comiss; jbe`) -> swapLeafNodes 0x5887B0(i, split) (the two 16-byte
// leaves exchanged), split++. Unbalanced (split <= start + n / 3 or split >=
// end - n / 3 - 1, n / 3 signed) -> start + (n >> 1).
inline int bvh_build_sort_and_split( optimized_bvh_t& b, int start, int end, int axis )
{
    const float* mn = b.view.aabb_min;
    const float* q = b.view.quantization;
    const int n = end - start;
    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
    for ( int i = start; i < end; ++i )
    {
        const bvh_node_t& l = b.leaves[ i ];
        const float iy = 1.0f / q[ 1 ], iz = 1.0f / q[ 2 ], ix = 1.0f / q[ 0 ];
        const float ly = bvh_build_u( l, 1 ) * iy + mn[ 1 ];
        const float lz = bvh_build_u( l, 2 ) * iz + mn[ 2 ];
        const float lx = bvh_build_u( l, 0 ) * ix + mn[ 0 ];
        const float hx = bvh_build_u( l, 3 ) * ix + mn[ 0 ];
        const float hy = bvh_build_u( l, 4 ) * iy + mn[ 1 ];
        const float hz = bvh_build_u( l, 5 ) * iz + mn[ 2 ];
        sz = ( hz + lz ) * 0.5f + sz;
        sx = ( hx + lx ) * 0.5f + sx;
        sy = sy + ( hy + ly ) * 0.5f;
    }
    const float inv = 1.0f / static_cast< float >( n );
    const float means[ 3 ] = { inv * sx, inv * sy, inv * sz };
    int split = start;
    for ( int i = start; i < end; ++i )
    {
        const bvh_node_t& l = b.leaves[ i ];
        const float lx = bvh_build_u( l, 0 ) / q[ 0 ] + mn[ 0 ];
        const float ly = mn[ 1 ] + bvh_build_u( l, 1 ) / q[ 1 ];
        const float lz = mn[ 2 ] + bvh_build_u( l, 2 ) / q[ 2 ];
        const float hx = bvh_build_u( l, 3 ) / q[ 0 ] + mn[ 0 ];
        const float hy = mn[ 1 ] + bvh_build_u( l, 4 ) / q[ 1 ];
        const float hz = mn[ 2 ] + bvh_build_u( l, 5 ) / q[ 2 ];
        const float centre[ 3 ] = { ( hx + lx ) * 0.5f, ( hy + ly ) * 0.5f, ( hz + lz ) * 0.5f };
        if ( centre[ axis ] > means[ axis ] )
        {
            const bvh_node_t t = b.leaves[ i ];
            b.leaves[ i ] = b.leaves[ split ];
            b.leaves[ split ] = t;
            ++split;
        }
    }
    const int third = n / 3;
    if ( split <= start + third || split >= end - third - 1 )
        return start + ( n >> 1 );
    return split;
}

// The leaf's subtree size as 0x5868B0 / 0x582CB0 read it: a leaf (escape >=
// 0) 1, else -escape.
inline int32_t bvh_build_subtree_size( const bvh_node_t& n )
{
    return n.escape_or_triangle < 0 ? -n.escape_or_triangle : 1;
}

// updateSubtreeHeaders 0x5868B0 (thiscall left, right; retn 8): each child
// whose subtree is <= 2048 bytes (16-byte nodes) gets a header (its quantized
// bounds, its index, its size), left first; then the header count.
inline void bvh_build_update_subtree_headers( optimized_bvh_t& b, int left, int right )
{
    const int children[ 2 ] = { left, right };
    for ( int c : children )
    {
        const bvh_node_t& n = b.nodes[ c ];
        const int32_t size = bvh_build_subtree_size( n );
        if ( 16 * size <= 2048 )
        {
            bvh_subtree_t h;
            std::memcpy( h.min, n.min, 6 );
            std::memcpy( h.max, n.max, 6 );
            h.root = c;
            h.size = size;
            b.subtrees.push_back( h );
        }
    }
    b.subtree_header_count = static_cast< int >( b.subtrees.size( ) );
}

// buildTree 0x586640 (thiscall start, end; retn 8), quantized: one leaf ->
// assignInternalNodeFromLeafNode 0x588870 (the 16 bytes) at curNodeIndex++.
// Else the axis (0x587090), the split (0x586C30); the internal node at
// curNodeIndex starts as the INVERTED box (setInternalNodeAabbMin 0x58A570 of
// the bvh max, setInternalNodeAabbMax 0x58A4B0 of the bvh min, both
// quantized); every leaf of the range unquantized (min + u / q, w 0) and
// merged (0x58A290: both points quantized, u16 min / max by unsigned
// compares); curNodeIndex++; the left range, the right range; the escape =
// curNodeIndex - the index on entry: > 2048 bytes -> updateSubtreeHeaders(the
// left child's index, the right's); the node's +0xC = -escape.
inline void bvh_build_tree( optimized_bvh_t& b, int start, int end )
{
    const int entry = b.view.cur_node_index;
    const int n = end - start;
    if ( n == 1 )
    {
        b.nodes[ entry ] = b.leaves[ start ];
        ++b.view.cur_node_index;
        return;
    }
    const int axis = bvh_build_calc_splitting_axis( b, start, end );
    const int split = bvh_build_sort_and_split( b, start, end, axis );
    const int internal = b.view.cur_node_index;
    bvh_node_t& node = b.nodes[ internal ];
    for ( int k = 0; k < 3; ++k )
        node.min[ k ] = bvh_build_quantize( b, b.view.aabb_max[ k ], k, false );
    for ( int k = 0; k < 3; ++k )
        node.max[ k ] = bvh_build_quantize( b, b.view.aabb_min[ k ], k, true );
    const float* mn = b.view.aabb_min;
    const float* q = b.view.quantization;
    for ( int i = start; i < end; ++i )
    {
        const bvh_node_t& l = b.leaves[ i ];
        float hi[ 3 ], lo[ 3 ];
        for ( int k = 0; k < 3; ++k )
            hi[ k ] = mn[ k ] + bvh_build_u( l, 3 + k ) / q[ k ];
        for ( int k = 0; k < 3; ++k )
            lo[ k ] = mn[ k ] + bvh_build_u( l, k ) / q[ k ];
        uint16_t qlo[ 3 ], qhi[ 3 ];
        for ( int k = 0; k < 3; ++k )
            qlo[ k ] = bvh_build_quantize( b, lo[ k ], k, false );
        for ( int k = 0; k < 3; ++k )
            qhi[ k ] = bvh_build_quantize( b, hi[ k ], k, true );
        bvh_node_t& to = b.nodes[ internal ];
        for ( int k = 0; k < 3; ++k )
        {
            if ( to.min[ k ] > qlo[ k ] )
                to.min[ k ] = qlo[ k ];
            if ( to.max[ k ] < qhi[ k ] )
                to.max[ k ] = qhi[ k ];
        }
    }
    const int left = ++b.view.cur_node_index;
    bvh_build_tree( b, start, split );
    const int right = b.view.cur_node_index;
    bvh_build_tree( b, split, end );
    const int escape = b.view.cur_node_index - entry;
    if ( 16 * escape > 2048 )
        bvh_build_update_subtree_headers( b, left, right );
    b.nodes[ internal ].escape_or_triangle = -escape;
}

// btOptimizedBvh::build 0x582CB0 (thiscall mesh, useQuantizedAabbCompression,
// bvhAabbMin*, bvhAabbMax*; retn 10h), the quantized branch: +0x48 = the
// flag; setQuantizationValues(min, max); the mesh's InternalProcessAllTriangles
// (vt+4) with the QuantizedNodeTriangleCallback (vt 0x74FE9C); the contiguous
// nodes resized to 2 x leaves (zero-filled); curNodeIndex = 0; buildTree(0,
// leaves); no subtree header yet -> one for node 0 (its bounds, root 0, its
// subtree size); the header count; m_quantizedLeafNodes and m_leafNodes
// cleared. 1 = built, -1 = a branch not recreated (see the file comment).
inline int bvh_build_optimized( optimized_bvh_t& b, const striding_mesh_t& mesh, bool quantized, const float* mn, const float* mx )
{
    b.view.use_quantization = quantized ? 1 : 0;
    if ( !quantized )
        return -1;   // NodeTriangleCallback 0x583210 / the 64-byte tree: unreached
    bvh_build_set_quantization( b, mn, mx );
    bvh_build_mesh_triangles( mesh, [ & ]( const float ( *t )[ 4 ], int part, int face ) { bvh_build_triangle( b, t, part, face ); } );
    const int leaves = static_cast< int >( b.leaves.size( ) );
    if ( leaves == 0 )
        return -1;   // retail's buildTree(0, 0) writes node 0 of an empty array
    b.nodes.assign( static_cast< std::size_t >( 2 * leaves ), bvh_node_t{ } );
    b.view.nodes = b.nodes.data( );
    b.view.cur_node_index = 0;
    bvh_build_tree( b, 0, leaves );
    if ( b.subtrees.empty( ) )
    {
        bvh_subtree_t h;
        std::memcpy( h.min, b.nodes[ 0 ].min, 6 );
        std::memcpy( h.max, b.nodes[ 0 ].max, 6 );
        h.root = 0;
        h.size = bvh_build_subtree_size( b.nodes[ 0 ] );
        b.subtrees.push_back( h );
    }
    b.subtree_header_count = static_cast< int >( b.subtrees.size( ) );
    b.leaves.clear( );
    b.leaves.shrink_to_fit( );
    return 1;
}

// buildOptimizedBvh 0x5AF380 (thiscall shape): ownsBvh (+0x49) -> the old
// tree's destructor (vt+0, flags 0) and btAlignedFree (the caller's storage
// here: `built` is replaced); a new btOptimizedBvh (0xC0 bytes, 16-aligned;
// ctor 0x586220, vtable 0x74FEB4) at +0x40, built over the shape's mesh
// (+0x30) with the shape's useQuantized (+0x48) and its cached local aabb
// (+0x10 / +0x20); ownsBvh = 1.
inline int bvh_mesh_build_optimized_bvh( bvh_mesh_shape_t& s, const striding_mesh_t& mesh, optimized_bvh_t& built, bool use_quantized = true )
{
    bvh_build_construct( built );
    const int r = bvh_build_optimized( built, mesh, use_quantized, s.local_aabb_min, s.local_aabb_max );
    s.bvh = &built.view;
    return r;
}

// btBvhTriangleMeshShape::setLocalScaling 0x5AF300 WITH its rebuild branch
// (the overload without `built` answers -1 there): d = (dy^2 + dx^2) + dz^2
// of the mesh's scaling - scaling; `comiss d, 1.1920929e-7; jbe` -> 1, nothing
// changed. Else the 16 bytes, the recalc through the CURRENT tree's walk, then
// buildOptimizedBvh into `built` (s.bvh points at it after).
inline int bvh_mesh_set_local_scaling( bvh_mesh_shape_t& s, striding_mesh_t& mesh, const float* scaling, optimized_bvh_t& built )
{
    const float dx = mesh.scaling[ 0 ] - scaling[ 0 ];
    const float dy = mesh.scaling[ 1 ] - scaling[ 1 ];
    const float dz = mesh.scaling[ 2 ] - scaling[ 2 ];
    const float d = ( dy * dy + dx * dx ) + dz * dz;
    if ( !( d > support_bits( 0x34000000u ) ) )
        return 1;
    if ( mesh_shape_set_local_scaling( s, mesh, scaling, true ) < 0 )
        return -1;
    return bvh_mesh_build_optimized_bvh( s, mesh, built );
}
