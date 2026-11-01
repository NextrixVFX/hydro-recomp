#pragma once

#include <workspace/core/collision/concave.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// The two mesh assets the boat load reads, loaded from their pack payload the
// way retail's asset slot 5 (load) reads it: a VuBinaryDataReader over the
// decompressed blob ({data +0, size +4, position +8}; every read a memcpy_s at
// data + position, then position += count - no bounds check in retail: a read
// past the blob is UNKNOWN here, the load returns false).
//   VuCollisionMeshAsset (vtable 0x74CC3C, ctor 0x518F70): load 0x51AF20 with
//     the VuOptimizedBvh reader 0x583DD0 (its arrays: 0x585BC0 / 0x585DE0 /
//     0x585FE0), unload 0x51B140.
//   VuFluidsMeshAsset (vtable 0x74CDDC): load 0x523770, unload 0x5238E0.

struct mesh_stream_t
{
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t at = 0;

    bool read( void* out, std::size_t count )
    {
        if ( count > size || at > size - count )
            return false;
        if ( count )
            std::memcpy( out, data + at, count );
        at += count;
        return true;
    }
    bool skip_into( std::vector< std::uint8_t >& out, std::size_t count )
    {
        if ( count > size || at > size - count )
            return false;
        out.assign( data + at, data + at + count );
        at += count;
        return true;
    }
};

// A VuArray {data +0, size +4, capacity +8}: its resize (0x51C1F0 / 0x50B740 /
// 0x415690 / 0x51C260 / 0x524050 / 0x523FD0, one per element size) grows a
// capacity past n to max(n, capacity + capacity / 2) - on the ctor's empty
// arrays (capacity 0) that is n itself.
struct mesh_vu_array_t
{
    std::vector< std::uint8_t > bytes;
    std::int32_t size = 0;
    std::int32_t capacity = 0;
};

// A btAlignedObjectArray {+4 size, +8 capacity, +0xC data, +0x10 ownsMemory}.
struct mesh_bt_array_t
{
    std::vector< std::uint8_t > bytes;
    std::int32_t size = 0;
    std::int32_t capacity = 0;
    std::uint8_t owns = 1;
};

// The counted VuArray read 0x51AF20 / 0x523770 inline: n (4 bytes), the
// resize, then - when n != 0 - n * elem bytes. A negative n: UNKNOWN (false).
inline bool mesh_read_vu_array( mesh_stream_t& s, mesh_vu_array_t& a, std::size_t elem )
{
    std::int32_t n = 0;
    if ( !s.read( &n, 4 ) || n < 0 )
        return false;
    if ( n > a.capacity )
    {
        std::int32_t grown = a.capacity + a.capacity / 2;
        a.capacity = n > grown ? n : grown;
    }
    a.size = n;
    if ( n == 0 )
    {
        a.bytes.clear( );
        return true;
    }
    return s.skip_into( a.bytes, static_cast< std::size_t >( n ) * elem );
}

// 0x585BC0 (64-byte nodes) / 0x585DE0 (16-byte quantized nodes) / 0x585FE0
// (32-byte subtree headers), fastcall (stream ecx, array edx, the default
// element on the stack): c = the capacity read, s = the size read; capacity <
// c (signed) -> reserve(c) (a new block, the old elements copied); then
// resize(s) (s > size: reserve(s) when capacity < s, the new elements the
// default); size = s; then s * elem bytes copied over the data.
inline bool mesh_read_bt_array( mesh_stream_t& s, mesh_bt_array_t& a, std::size_t elem )
{
    std::int32_t c = 0, n = 0;
    if ( !s.read( &c, 4 ) || !s.read( &n, 4 ) || n < 0 )
        return false;
    if ( a.capacity < c )
    {
        a.capacity = c;
        a.owns = 1;
    }
    if ( n > a.size && a.capacity < n )
    {
        a.capacity = n;
        a.owns = 1;
    }
    a.size = n;
    if ( n == 0 )
    {
        a.bytes.clear( );
        return true;
    }
    return s.skip_into( a.bytes, static_cast< std::size_t >( n ) * elem );
}

// VuOptimizedBvh (btQuantizedBvh; 0xC0 bytes, ctor 0x586220) as 0x583DD0
// fills it: +0x10..+0x44 fourteen words (aabb min / max, quantization,
// +0x40 bulletVersion, +0x44 curNodeIndex), byte +0x48 useQuantization, the
// arrays +0x4C leaf nodes (64 b), +0x60 contiguous nodes (64 b), +0x74
// quantized leaf nodes (16 b), +0x88 quantized contiguous nodes (16 b), +0x9C
// traversal mode, +0xA0 subtree headers (32 b), +0xB4 the subtree header
// count.
struct mesh_bvh_t
{
    std::uint32_t header[ 14 ] = { };
    std::uint8_t use_quantization = 0;
    mesh_bt_array_t leaf, contiguous, quantized_leaf, quantized;
    std::int32_t traversal = 0;
    mesh_bt_array_t subtrees;
    std::int32_t subtree_count = 0;
};

inline bool mesh_read_bvh( mesh_stream_t& s, mesh_bvh_t& b )
{
    for ( std::uint32_t& w : b.header )
        if ( !s.read( &w, 4 ) )
            return false;
    return s.read( &b.use_quantization, 1 ) && mesh_read_bt_array( s, b.leaf, 64 ) && mesh_read_bt_array( s, b.contiguous, 64 )
        && mesh_read_bt_array( s, b.quantized_leaf, 16 ) && mesh_read_bt_array( s, b.quantized, 16 ) && s.read( &b.traversal, 4 )
        && mesh_read_bt_array( s, b.subtrees, 32 ) && s.read( &b.subtree_count, 4 );
}

// VuCollisionMeshAsset after 0x51AF20: +0x58 the vertices (16 b), +0x64 the
// u16 indices, +0x70 a u32 array (the triangles' material indices), +0x7C the
// materials (44 b: a NUL-ended surface name, byte +40 the surface id written
// by the load), +0x88..+0xA7 the aabb (min 4, max 4), +0xA8 the VuOptimizedBvh
// (btAlignedAlloc 0xC0), byte +0xAC.
struct collision_mesh_asset_t
{
    mesh_vu_array_t vertices, indices, triangles, materials;
    float aabb[ 8 ] = { };
    mesh_bvh_t bvh;
    std::uint8_t flag = 0;
};

// 0x51AF20 (thiscall asset; stream; retn 4). After the reads, every material
// (count signed > 0) gets byte +40 = g_vudynamicsimpl ? its vt+0x54 (slot 21
// 0x4F88A0: the surface-type table +0x38's slot 2) with the record : 0 - the
// `surface` hook (null: no dynamics, 0).
inline bool collision_mesh_load( mesh_stream_t& s, collision_mesh_asset_t& a,
    const std::function< std::uint8_t( const std::string& name ) >& surface )
{
    if ( !mesh_read_vu_array( s, a.vertices, 16 ) || !mesh_read_vu_array( s, a.indices, 2 ) ||
         !mesh_read_vu_array( s, a.triangles, 4 ) || !mesh_read_vu_array( s, a.materials, 44 ) )
        return false;
    a.bvh = mesh_bvh_t{ };
    if ( !mesh_read_bvh( s, a.bvh ) || !s.read( a.aabb, 32 ) || !s.read( &a.flag, 1 ) )
        return false;
    for ( std::int32_t k = 0; k < a.materials.size; ++k )
    {
        std::uint8_t* record = a.materials.bytes.data( ) + 44 * static_cast< std::size_t >( k );
        std::size_t length = 0;
        while ( length < 44 && record[ length ] )
            ++length;
        if ( length == 44 )
            return false;   // no NUL inside the record: retail reads past it (UNKNOWN)
        record[ 40 ] = surface ? surface( std::string( reinterpret_cast< const char* >( record ), length ) ) : 0;
    }
    return true;
}

// The striding mesh the boat's VuStridingMesh hands Bullet (getLockedReadOnly
// VertexIndexBase 0x527C50: asset +0x58 / +0x5C / +0x64 / +0x68). The mesh
// points into the asset: keep the asset alive.
inline void collision_mesh_striding( const collision_mesh_asset_t& a, striding_mesh_t& out )
{
    out.vertices = reinterpret_cast< const float ( * )[ 4 ] >( a.vertices.bytes.data( ) );
    out.num_vertices = a.vertices.size;
    out.indices = reinterpret_cast< const std::uint16_t* >( a.indices.bytes.data( ) );
    out.index_count = a.indices.size;
}

// VuFluidsMeshAsset after 0x523770: +0x78 the vertices (16 b), +0x84 the
// edges (8 b), +0x90 60-byte records, +0x58..+0x77 the aabb (min 4, max 4),
// +0x9C and +0xA0 two words. 0x5243A0 reads the counts +0x7C / +0x88, 0x526090
// the aabb. The ctor 0x522780 gives each of the three arrays an 8-entry block
// (capacity 8) - so a 10-vertex mesh grows to max(10, 8 + 4) = 12.
struct fluids_mesh_asset_t
{
    mesh_vu_array_t vertices{ { }, 0, 8 }, edges{ { }, 0, 8 }, records{ { }, 0, 8 };
    float aabb[ 8 ] = { };
    std::uint32_t w9c = 0, wa0 = 0;
};

inline bool fluids_mesh_load( mesh_stream_t& s, fluids_mesh_asset_t& a )
{
    return mesh_read_vu_array( s, a.vertices, 16 ) && mesh_read_vu_array( s, a.edges, 8 ) && mesh_read_vu_array( s, a.records, 60 )
        && s.read( a.aabb, 32 ) && s.read( &a.w9c, 4 ) && s.read( &a.wa0, 4 );
}
