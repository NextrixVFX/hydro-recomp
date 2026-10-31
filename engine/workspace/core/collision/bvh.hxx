#pragma once

#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

#include <workspace/core/collision/support.hxx>

// The level's BVH walk (2.79), recreated from retail's disassembly
// (memory/audit.md "Batch 8f"). The tree is NOT built at runtime: it is the
// collision-mesh asset's VuOptimizedBvh (asset +0xA8), installed on the
// btBvhTriangleMeshShape by setOptimizedBvh 0x5AF400. The census of all 236
// collision-mesh assets (scratchpad bvhcensus.py) found every tree quantized
// with traversal mode 0 and only the quantized contiguous nodes filled, so
// the stackless quantized walk is the only one the content reaches; the
// others return -1 (unknown) here, never a stand-in.

// btQuantizedBvhNode, 16 bytes: quantized min / max, then the escape index
// (negative, internal node) or part << 21 | triangle (leaf).
struct bvh_node_t
{
    uint16_t min[ 3 ];
    uint16_t max[ 3 ];
    int32_t escape_or_triangle;
};

// The fields of a VuOptimizedBvh (btQuantizedBvh) the walk reads, at
// retail's offsets: +0x10 aabb min, +0x20 aabb max, +0x30 quantization,
// +0x44 curNodeIndex, byte +0x48 useQuantization, +0x94 the quantized
// contiguous nodes' data, +0x9C traversal mode.
struct quantized_bvh_t
{
    float aabb_min[ 4 ];
    float aabb_max[ 4 ];
    float quantization[ 4 ];
    int cur_node_index;
    uint8_t use_quantization;
    const bvh_node_t* nodes;
    int traversal_mode;
};

// 0x7590B4 (.bss, starts 0): Bullet's maxIterations. The walk stores
// max(global, iterations) - a signed `cmovl` - after every walk.
inline int g_bvh_max_iterations = 0;

// btNodeOverlapCallback: vt+4 processNode(part, triangle).
struct bvh_node_callback_t
{
    void* user;
    void ( *process_node )( void* user, int part, int triangle );
};

// btTriangleCallback: vt+4 processTriangle(triangle, part, triangle index).
struct bvh_triangle_callback_t
{
    void* user;
    void ( *process_triangle )( void* user, float ( *triangle )[ 4 ], int part, int index );
};

// walkStacklessQuantizedTree @0x5885E0 (thiscall bvh; callback, qmin*,
// qmax*, start, end; retn 14h). Node overlap is six unsigned 16-bit compares
// (`cmp; sbb; inc`, all evaluated, no short circuit). A leaf that overlaps
// is reported; an overlapping node or any leaf steps to the next node; an
// internal node that does not overlap skips by its escape index. Every loop
// pass counts one iteration.
inline void bvh_walk_stackless_quantized( const quantized_bvh_t& bvh, const bvh_node_callback_t& callback,
    const uint16_t* qmin, const uint16_t* qmax, int start, int end )
{
    int index = start;
    const bvh_node_t* node = bvh.nodes + start;
    int iterations = 0;
    while ( index < end )
    {
        ++iterations;
        const int overlap = ( node->max[ 2 ] >= qmin[ 2 ] ) & ( node->max[ 1 ] >= qmin[ 1 ] ) & ( qmax[ 2 ] >= node->min[ 2 ] ) &
                            ( qmax[ 1 ] >= node->min[ 1 ] ) & ( node->max[ 0 ] >= qmin[ 0 ] ) & ( qmax[ 0 ] >= node->min[ 0 ] );
        const int32_t value = node->escape_or_triangle;
        const bool leaf = value >= 0;
        if ( leaf && overlap )
            callback.process_node( callback.user, value >> 21, value & 0x1FFFFF );
        if ( overlap || leaf )
        {
            ++node;
            ++index;
        }
        else
        {
            const int escape = static_cast< int >( 0u - static_cast< uint32_t >( value ) );
            node += escape;
            index += escape;
        }
    }
    if ( g_bvh_max_iterations < iterations )
        g_bvh_max_iterations = iterations;
}

// One lane of the quantize: clamp into the BVH box (`comiss; jbe` - a NaN
// lane passes both clamps untouched), then (v - min) * quantization
// (+ 1.0 for the max, 0x6F76B8) through cvttss2si (truncation; NaN and
// overflow give 0x80000000), the low 16 bits & 0xFFFE (min) or | 1 (max).
inline float bvh_clamp( float v, float lo, float hi )
{
    if ( lo > v )
        v = lo;
    if ( v > hi )
        v = hi;
    return v;
}

// reportAabbOverlappingNodex @0x587510 (thiscall bvh; callback, aabbMin*,
// aabbMax*; retn 0Ch). Returns 0, or -1 where retail goes to a walk not
// recreated: the non-quantized tree 0x587720, traversal 1 (0x5886E0) and 2
// (0x587800). Any other traversal value walks nothing.
inline int bvh_report_aabb_overlapping_nodes( const quantized_bvh_t& bvh, const bvh_node_callback_t& callback,
    const float* aabb_min, const float* aabb_max )
{
    if ( !bvh.use_quantization )
        return -1;   // walkStacklessTree 0x587720: unreached by content
    uint16_t qmin[ 3 ], qmax[ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        const float v = bvh_clamp( aabb_min[ k ], bvh.aabb_min[ k ], bvh.aabb_max[ k ] );
        const float q = ( v - bvh.aabb_min[ k ] ) * bvh.quantization[ k ];
        qmin[ k ] = static_cast< uint16_t >( _mm_cvtt_ss2si( _mm_set_ss( q ) ) & 0xFFFE );
    }
    for ( int k = 0; k < 3; ++k )
    {
        const float v = bvh_clamp( aabb_max[ k ], bvh.aabb_min[ k ], bvh.aabb_max[ k ] );
        const float q = ( v - bvh.aabb_min[ k ] ) * bvh.quantization[ k ] + 1.0f;
        qmax[ k ] = static_cast< uint16_t >( _mm_cvtt_ss2si( _mm_set_ss( q ) ) | 1 );
    }
    switch ( bvh.traversal_mode )
    {
    case 0:
        bvh_walk_stackless_quantized( bvh, callback, qmin, qmax, 0, bvh.cur_node_index );
        return 0;
    case 1:
        return -1;   // walkStacklessQuantizedTreeCacheFriendly 0x5886E0: unreached by content
    case 2:
        return -1;   // walkRecursiveQuantizedTreeAgainstQueryAabb 0x587800: unreached by content
    default:
        return 0;
    }
}

// btBvhTriangleMeshShape::processAllTriangles' MyNodeOverlapCallback (vt
// 0x751748; +4 the mesh interface, +8 the triangle callback, +0x10 the
// triangle buffer). processNode @0x5AF180 locks the mesh (vt+12), reads the
// triangle's three indices LAST TO FIRST into the buffer, calls the
// callback's processTriangle (vt+4), and unlocks (vt+0x14). For the
// VuStridingMesh the lock is getLockedReadOnlyVertexIndexBase @0x527C50:
// constant PHY_FLOAT vertices (stride 16) and PHY_SHORT indices (stride 6),
// the subpart IGNORED; the unlock 0x4021A0 is `ret 4`. processNode's other
// branches (PHY_DOUBLE vertices, PHY_INTEGER / byte indices) are therefore
// unreachable through this mesh. Each corner is scaling * v, w = 0.
struct mesh_node_callback_t
{
    const striding_mesh_t* mesh;
    bvh_triangle_callback_t callback;
    float triangle[ 3 ][ 4 ];
};

inline void mesh_process_node( void* user, int part, int index )
{
    mesh_node_callback_t& self = *static_cast< mesh_node_callback_t* >( user );
    const striding_mesh_t& mesh = *self.mesh;
    const uint16_t* tri = mesh.indices + 3 * index;
    for ( int j = 2; j >= 0; --j )
    {
        const float* v = mesh.vertices[ tri[ j ] ];
        self.triangle[ j ][ 0 ] = mesh.scaling[ 0 ] * v[ 0 ];
        self.triangle[ j ][ 1 ] = v[ 1 ] * mesh.scaling[ 1 ];
        self.triangle[ j ][ 2 ] = v[ 2 ] * mesh.scaling[ 2 ];
        self.triangle[ j ][ 3 ] = 0.0f;
    }
    self.callback.process_triangle( self.callback.user, self.triangle, part, index );
}

// btBvhTriangleMeshShape::processAllTriangles @0x5AF0F0 (thiscall shape;
// callback, aabbMin*, aabbMax*): the node callback on the stack with the mesh
// interface (shape +0x30), then the shape's BVH (+0x40) walked.
inline int bvh_mesh_process_all_triangles( const quantized_bvh_t& bvh, const striding_mesh_t& mesh,
    const bvh_triangle_callback_t& callback, const float* aabb_min, const float* aabb_max )
{
    mesh_node_callback_t node{ &mesh, callback, {} };
    const bvh_node_callback_t walk{ &node, mesh_process_node };
    return bvh_report_aabb_overlapping_nodes( bvh, walk, aabb_min, aabb_max );
}
