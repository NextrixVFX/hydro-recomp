#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// The water query, recreated from retail's disassembly (memory/audit.md
// "Batch 9b"; scratchpad dis_fp.txt): VuWater's per-vertex surface and wave
// pass @0x528FC0 (thiscall g_vuwater; ctx*; retn 4) over the surface-tree
// walk 0x52AA60, the one-surface containment test 0x527F70, the per-vertex
// surface tagger 0x529F70, the flat fill 0x529ED0 and the wave-tree walk
// 0x52A8D0, which calls each overlapping wave's slot 3 kernel (wave.hxx) - a
// Vu virtual, so a hook here.
//
// Objects are held by index, not by pointer (retail is 32-bit; this build is
// not): a tree node's +20 / +24 are node or object indices, -1 for retail's
// null; a vertex's surface tag is surface index + 1, 0 for retail's null.
// Both ctx+96 paths are recreated: 0 (VuFluidSolver_prepare's, 40-byte
// water_vertex_t records) and set (the water renderer 0x597D70's: raw records
// with a stride, ctx.records / ctx.stride - 0x529ED0 / 0x529F70's +96
// branches; the waves' +96 kernels are world/wave.hxx's *_vertex96).

// One record of the vertex array the query fills (retail's 40-byte stride, the
// VuFluidSolver's +56 array): +0 the world position; +12 a word retail never
// writes (the caller's stale stack - see fluid.hxx); +16 the displacement the
// waves accumulate; +28 0x529ED0's / 0x529F70's stale stack word; +32 the
// water height; +36 the submerged flag (the solver's, not the query's).
struct water_vertex_t
{
    float pos[ 3 ];
    uint32_t pos_w;
    float disp[ 3 ];
    uint32_t disp_w;
    float height;
    uint32_t submerged;
};
static_assert( sizeof( water_vertex_t ) == 40, "retail's stride" );

// A node of VuWater's 2D trees (the surface tree at *(*(water+0x3C)) and each
// surface's wave tree at *(*(surface+0xFC))): +0 min x, +4 min y, +8 max x,
// +12 max y; +20 the object (leaf) or the first child; +24 the second child,
// -1 on a leaf (retail: 0); +28 the tag the wave walk hands the kernel
// (ctx+92) - a surface, so a surface index + 1 here.
struct water_node_t
{
    float rect[ 4 ];
    int first;
    int second;
    uint32_t tag;
};

// VuWaterMapAsset (vtable 0x752498), as its load 0x5C8650 (slot 5, thiscall
// asset; VuBinaryDataReader& {+0 data, +8 offset}; retn 4; returns 1) reads
// it - every field a 4-byte memcpy_s or a byte array 0x5AC7F0 (a u32 count,
// then that many bytes) in this order: +0x58 width, +0x5C height, +0x60 /
// +0x64 / +0x68 the map (data, size, capacity), +0x6C and +0x78 two more
// byte arrays, then a u32 count, the pointer array +0x84 sized to it by
// 0x415690, and per entry a new 0x14-byte record (+8 / +0xC / +0x10 zeroed):
// +0 and +4 a u32 each, +8 a byte array. Only the map (+0x60, its size +0x64)
// is read by anything recreated (0x535E10). The arrays' capacity growth
// (0x5AC7F0: max( n, cap + cap / 2 ), 16-byte aligned) is allocation only.
struct water_map_t
{
    std::uint32_t width = 0;    // +0x58
    std::uint32_t height = 0;   // +0x5C
    std::vector< std::uint8_t > map;      // +0x60 / +0x64
    std::vector< std::uint8_t > second;   // +0x6C
    std::vector< std::uint8_t > third;    // +0x78
    struct entry_t
    {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        std::vector< std::uint8_t > bytes;
    };
    std::vector< entry_t > entries;   // +0x84 / +0x88
};

// 0x5C8650 on a blob (the asset's bytes as the pack holds them). Retail's
// reader has no bounds check: a blob too short for what it declares is read
// past its end there; here it returns false (not recreated).
inline bool water_map_load( const std::uint8_t* data, std::size_t size, water_map_t& out )
{
    out = water_map_t{};
    std::size_t at = 0;
    auto u32 = [ & ]( std::uint32_t& v ) {
        if ( size - at < 4 || at > size )
            return false;
        std::memcpy( &v, data + at, 4 );
        at += 4;
        return true;
    };
    auto bytes = [ & ]( std::vector< std::uint8_t >& v ) {
        std::uint32_t n = 0;
        if ( !u32( n ) || size - at < n )
            return false;
        v.assign( data + at, data + at + n );
        at += n;
        return true;
    };
    std::uint32_t count = 0;
    if ( !u32( out.width ) || !u32( out.height ) || !bytes( out.map ) || !bytes( out.second ) || !bytes( out.third ) || !u32( count ) )
        return false;
    for ( std::uint32_t k = 0; k < count; ++k )
    {
        water_map_t::entry_t e{};
        if ( !u32( e.a ) || !u32( e.b ) || !bytes( e.bytes ) )
            return false;
        out.entries.push_back( std::move( e ) );
    }
    return true;
}

// The VuWaterSurface fields the query reads: +20 the height, +40 / +44 the
// depth below / above, +144 .. +200 the world-to-surface transform (rows of
// a row-vector 4x4: +144 row 0, +160 row 1, +176 row 2, +192 the
// translation), +216 the floor the one-surface test compares the query's max
// z against, +240 / +244 the half extents; +0xFC's tree root (-1 none); +72 /
// +76 the sizes (ints: 1 << the log2s) and +0x100 the VuWaterMapAsset (the
// entity's WaterMap; null when it names none or no asset) 0x535E10 samples.
struct water_surface_t
{
    float height;
    float below;
    float above;
    float inverse[ 16 ];
    float floor;
    float half_x;
    float half_y;
    int wave_root;
    std::int32_t size_x = 0;
    std::int32_t size_y = 0;
    const water_map_t* map = nullptr;
};

// 0x535E10 (thiscall surface; const VuVector3* point; retn 4; float in xmm0):
// 1.0 (0x6F76B8) without a map or with an empty one (+0x64 == 0); else the
// point in the surface's space (x, y as water_local's), the map cell
// u = (int)( ( ( half_x + lx ) / (float) size_x ) * (float)(int)( width - 1 ) ),
// v the same over y / height, both truncated (cvttss2si), and the byte at
// v * width + u times 0x3B808081 (1 / 255). Retail has no bounds check; a cell
// outside the map (a point outside the surface - its one caller 0x5292D0 asks
// only inside) returns false here (not recreated).
inline bool water_map_sample( const water_surface_t& s, const float* p, float& out )
{
    out = 1.0f;
    if ( !s.map || s.map->map.empty( ) )
        return true;
    const float* m = s.inverse;
    const float lx = ( ( m[ 4 ] * p[ 1 ] + p[ 0 ] * m[ 0 ] ) + m[ 8 ] * p[ 2 ] ) + m[ 12 ];
    const float ly = ( ( m[ 1 ] * p[ 0 ] + m[ 5 ] * p[ 1 ] ) + m[ 9 ] * p[ 2 ] ) + m[ 13 ];
    const float fu = ( ( s.half_x + lx ) / static_cast< float >( s.size_x ) ) *
                     static_cast< float >( static_cast< std::int32_t >( s.map->width - 1u ) );
    const float fv = ( ( s.half_y + ly ) / static_cast< float >( s.size_y ) ) *
                     static_cast< float >( static_cast< std::int32_t >( s.map->height - 1u ) );
    const std::int32_t u = _mm_cvttss_si32( _mm_set_ss( fu ) );
    const std::int32_t v = _mm_cvttss_si32( _mm_set_ss( fv ) );
    const std::uint32_t at = static_cast< std::uint32_t >( v ) * s.map->width + static_cast< std::uint32_t >( u );
    if ( at >= s.map->map.size( ) )
        return false;
    out = static_cast< float >( static_cast< std::int32_t >( s.map->map[ at ] ) ) * 0.003921568859368563f;
    return true;
}

// A wave as the walk sees it: +44 / +48 its centre, +52 its radius; the
// kernel itself is the hook.
struct water_wave_t
{
    float centre[ 2 ];
    float radius;
};

struct water_t
{
    bool skip_waves;   // +0x18 byte: the wave pass is skipped when set
    int surface_root;  // *(*(water + 0x3C)): -1 none
    std::vector< water_node_t > nodes;
    std::vector< water_surface_t > surfaces;
    std::vector< water_wave_t > waves;
};

// The query context retail builds on the caller's stack (0x68 bytes), by
// offset: +0 the vertex count, +4 .. +0x20 the aabb (+0x10 / +0x20 its w
// lanes), +0x24 .. +0x30 the centre, +0x34 the radius, +0x38 / +0x3C the
// excluded waves (four slots before +0x4C), +0x4C a forced surface, +0x50 /
// +0x54 the vertices and stride, +0x58 the per-vertex tags, +0x5C the tag
// the kernel filters on, +0x60 / +0x64 the two kernel-variant flags.
struct water_query_t
{
    int count;
    float aabb[ 8 ];
    float centre[ 4 ];
    float radius;
    int exclude_count;
    int exclude[ 4 ];
    int forced;   // a surface index, -1 for retail's null
    water_vertex_t* vertices;
    const uint32_t* tags;
    uint32_t tag;
    uint32_t flag96;
    uint32_t flag100;
    // flag96 set: ctx+0x50 / +0x54 as raw records and their stride in bytes
    // (the renderer 0x597D70's 32-byte vertices) - see world/wave.hxx's +96
    // kernels for the layout. `vertices` is unused then.
    unsigned char* records;
    int stride;
};

// One +96 record's floats by byte offset.
inline float* water_record( water_query_t& ctx, int i, int offset )
{
    return reinterpret_cast< float* >( ctx.records + static_cast< std::ptrdiff_t >( i ) * ctx.stride + offset );
}

// A wave's slot 3 (vt+12, thiscall(ctx*), retn 4): a Vu virtual - a hook.
// False when absent (not recreated).
using water_kernel_fn = bool ( * )( void* user, int wave, water_query_t& ctx );

inline float water_abs( float x )   // VuVector_abs 0x4089C0: andps 0x7FFFFFFF
{
    uint32_t u;
    std::memcpy( &u, &x, 4 );
    u &= 0x7FFFFFFFu;
    std::memcpy( &x, &u, 4 );
    return x;
}

// comiss a, b; jb -> false: a >= b, ordered.
inline bool water_ge( float a, float b )
{
    return _mm_comige_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0;
}

// The overlap test both walks open with (0x52AA60 / 0x52A8D0): rect = (min x,
// min y, max x, max y) from the ctx aabb; four comiss / jb, each false on NaN.
inline bool water_overlap( const float* rect, const float* node )
{
    return water_ge( rect[ 2 ], node[ 0 ] ) && water_ge( node[ 2 ], rect[ 0 ] ) && water_ge( rect[ 3 ], node[ 1 ] ) &&
           water_ge( node[ 3 ], rect[ 1 ] );
}

// A point in a surface's space, in retail's operand order (x from +160 * y +
// x * +144, then + +176 * z, then + +192; y and z the same shape).
inline void water_local( const water_surface_t& s, const float* p, float& lx, float& ly, float& lz )
{
    const float* m = s.inverse;
    lx = ( ( m[ 4 ] * p[ 1 ] + p[ 0 ] * m[ 0 ] ) + m[ 8 ] * p[ 2 ] ) + m[ 12 ];
    ly = ( ( m[ 1 ] * p[ 0 ] + m[ 5 ] * p[ 1 ] ) + m[ 9 ] * p[ 2 ] ) + m[ 13 ];
    lz = ( ( m[ 2 ] * p[ 0 ] + m[ 6 ] * p[ 1 ] ) + m[ 10 ] * p[ 2 ] ) + m[ 14 ];
}

// 0x52AA60 (fastcall root, rect; list*): the surfaces whose box, grown by the
// ctx radius, holds the ctx centre - at most 8, the rest dropped silently. An
// explicit stack of 256 entries (index 257 is the walk's own counter): a
// deeper walk returns false (not recreated).
inline bool water_find_surfaces( const water_t& w, const water_query_t& ctx, const float* rect, int* found, int& count )
{
    count = 0;
    if ( w.surface_root < 0 )
        return true;
    int stack[ 257 ];
    int top = 1;
    stack[ 1 ] = w.surface_root;
    while ( top )
    {
        const water_node_t& node = w.nodes[ stack[ top-- ] ];
        if ( !water_overlap( rect, node.rect ) )
            continue;
        if ( node.second >= 0 )
        {
            if ( top + 2 > 256 )
                return false;
            stack[ ++top ] = node.first;
            stack[ ++top ] = node.second;
            continue;
        }
        const water_surface_t& s = w.surfaces[ node.first ];
        float lx, ly, lz;
        water_local( s, ctx.centre, lx, ly, lz );
        const float r = ctx.radius;
        if ( !water_ge( s.half_x + r, water_abs( lx ) ) )
            continue;
        if ( !water_ge( s.half_y + r, water_abs( ly ) ) || !water_ge( s.above + r, lz ) || !water_ge( lz, -( s.below + r ) ) )
            continue;
        if ( count < 8 )
            found[ count++ ] = node.first;
    }
    return true;
}

// 0x527F70 (fastcall surface, ctx): the ctx box fits inside one surface -
// half_x >= |lx| + r, half_y >= |ly| + r, and the box's max z >= the floor.
inline bool water_contains( const water_surface_t& s, const water_query_t& ctx )
{
    float lx, ly, lz;
    water_local( s, ctx.centre, lx, ly, lz );
    return water_ge( s.half_x, water_abs( lx ) + ctx.radius ) && water_ge( s.half_y, water_abs( ly ) + ctx.radius ) &&
           water_ge( ctx.aabb[ 6 ], s.floor );
}

// 0x529ED0 (usercall height in XMM2; ctx*; retn 4), the ctx+96 == 0 branch:
// per vertex +16 / +20 / +24 = 0, +28 stale (held 0 here), +32 = height.
inline void water_fill( water_query_t& ctx, float height )
{
    if ( ctx.flag96 )
    {
        // The +96 branch: +8 = height, +0xC / +0x10 / +0x14 = 0, +0x18 = 1.0,
        // +0x1C = 0.
        for ( int i = 0; i < ctx.count; ++i )
        {
            *water_record( ctx, i, 8 ) = height;
            *water_record( ctx, i, 0xC ) = 0.0f;
            *water_record( ctx, i, 0x10 ) = 0.0f;
            *water_record( ctx, i, 0x14 ) = 0.0f;
            *water_record( ctx, i, 0x18 ) = 1.0f;
            *water_record( ctx, i, 0x1C ) = 0.0f;
        }
        return;
    }
    for ( int i = 0; i < ctx.count; ++i )
    {
        water_vertex_t& v = ctx.vertices[ i ];
        v.disp[ 0 ] = v.disp[ 1 ] = v.disp[ 2 ] = 0.0f;
        v.disp_w = 0;
        v.height = height;
    }
}

inline float water_bits( uint32_t u )
{
    float f;
    std::memcpy( &f, &u, 4 );
    return f;
}

// 0x529F70 (stdcall ctx*, surfaces*, count; a 0x1000-byte alloca for the
// tags - 1024 vertices), the ctx+96 == 0 branch: ctx+100 = 1 and the tags;
// per vertex the displacement cleared (+28 stale), height -1.7014117e38
// (0xFEFFFFFF), tag 0; the FIRST listed surface whose box holds the vertex
// (half_x >= |lx|, half_y >= |ly|, above >= lz >= -below) tags it and gives
// its height.
//
// The tags live in that alloca, which is dead once 0x529F70 returns - retail's
// later frames (0x52A8D0's, then each kernel's) reuse the top of it while the
// kernels read the tags. Vertex i's tag sits 0x1014 - 4i bytes below the
// caller's stack and 0x52A8D0 reaches 0x448 below it, so tags past index ~753
// are clobbered before any kernel runs, and a kernel's own frame reaches
// lower. Recreated here for count <= 256 only (the four hull meshes have 28 ..
// 80 vertices), where a kernel would need a 2.5 KB frame to reach a tag.
inline bool water_tag( water_query_t& ctx, const water_t& w, const int* found, int count, std::vector< uint32_t >& tags )
{
    if ( ctx.count > 256 )
        return false;   // see above
    ctx.flag100 = 1;
    tags.assign( ctx.count > 0 ? ctx.count : 0, 0 );
    ctx.tags = tags.data( );
    if ( ctx.flag96 )
    {
        // The +96 branch (0x52A1D5): the position is +0 / +4 / +8 (read
        // first), then +0xC / +0x10 = 0, +8 = 0xFEFFFFFF, +0x14 = 0, +0x18 =
        // 1.0, +0x1C = 0; the same surface test, the height into +8. The local
        // y / z sums are commuted against the base branch (same bits).
        for ( int i = 0; i < ctx.count; ++i )
        {
            const float p[ 3 ] = { *water_record( ctx, i, 0 ), *water_record( ctx, i, 4 ), *water_record( ctx, i, 8 ) };
            *water_record( ctx, i, 0xC ) = 0.0f;
            *water_record( ctx, i, 0x10 ) = 0.0f;
            *water_record( ctx, i, 8 ) = water_bits( 0xFEFFFFFFu );
            *water_record( ctx, i, 0x14 ) = 0.0f;
            *water_record( ctx, i, 0x18 ) = 1.0f;
            *water_record( ctx, i, 0x1C ) = 0.0f;
            for ( int k = 0; k < count; ++k )
            {
                const water_surface_t& s = w.surfaces[ found[ k ] ];
                float lx, ly, lz;
                water_local( s, p, lx, ly, lz );
                if ( water_ge( s.half_x, water_abs( lx ) ) && water_ge( s.half_y, water_abs( ly ) ) && water_ge( s.above, lz ) &&
                     water_ge( lz, -s.below ) )
                {
                    tags[ i ] = static_cast< uint32_t >( found[ k ] + 1 );
                    *water_record( ctx, i, 8 ) = s.height;
                    break;
                }
            }
        }
        return true;
    }
    for ( int i = 0; i < ctx.count; ++i )
    {
        water_vertex_t& v = ctx.vertices[ i ];
        v.disp[ 0 ] = v.disp[ 1 ] = 0.0f;
        v.disp[ 2 ] = 0.0f;
        v.disp_w = 0;
        v.height = water_bits( 0xFEFFFFFFu );
        for ( int k = 0; k < count; ++k )
        {
            const water_surface_t& s = w.surfaces[ found[ k ] ];
            float lx, ly, lz;
            water_local( s, v.pos, lx, ly, lz );
            if ( water_ge( s.half_x, water_abs( lx ) ) && water_ge( s.half_y, water_abs( ly ) ) && water_ge( s.above, lz ) &&
                 water_ge( lz, -s.below ) )
            {
                tags[ i ] = static_cast< uint32_t >( found[ k ] + 1 );
                v.height = s.height;
                break;
            }
        }
    }
    return true;
}

// 0x52A8D0 (fastcall root, rect; ctx**): every wave leaf overlapping the rect,
// not excluded, whose circle meets the ctx circle ((r + wave r)^2 > d^2,
// ordered): ctx+92 = the node's tag, then the kernel. The same 256-entry
// stack as the surface walk.
inline bool water_waves( const water_t& w, int root, water_query_t& ctx, const float* rect, water_kernel_fn kernel, void* user )
{
    if ( root < 0 )
        return true;
    int stack[ 257 ];
    int top = 1;
    stack[ 1 ] = root;
    while ( top )
    {
        const water_node_t& node = w.nodes[ stack[ top-- ] ];
        if ( !water_overlap( rect, node.rect ) )
            continue;
        if ( node.second >= 0 )
        {
            if ( top + 2 > 256 )
                return false;
            stack[ ++top ] = node.first;
            stack[ ++top ] = node.second;
            continue;
        }
        bool excluded = false;
        for ( int k = 0; k < ctx.exclude_count && !excluded; ++k )
            excluded = ctx.exclude[ k ] == node.first;
        if ( excluded )
            continue;
        const water_wave_t& wave = w.waves[ node.first ];
        const float dy = wave.centre[ 1 ] - ctx.centre[ 1 ];
        const float dx = wave.centre[ 0 ] - ctx.centre[ 0 ];
        const float r = ctx.radius + wave.radius;
        if ( !_mm_comigt_ss( _mm_set_ss( r * r ), _mm_set_ss( dy * dy + dx * dx ) ) )
            continue;
        ctx.tag = node.tag;
        if ( !kernel || !kernel( user, node.first, ctx ) )
            return false;   // the wave's slot 3: Vu code
    }
    return true;
}

// 0x528FC0 (thiscall water; ctx*; retn 4).
inline bool water_query( const water_t& w, water_query_t& ctx, water_kernel_fn kernel, void* user, std::vector< uint32_t >& tags )
{
    const float rect[ 4 ] = { ctx.aabb[ 0 ], ctx.aabb[ 1 ], ctx.aabb[ 4 ], ctx.aabb[ 5 ] };
    // ctx+0x4C set: 0x529ED0 with that surface's height, then its waves. No
    // caller recreated so far sets it (prepare's ctor zeroes it), so it is
    // unmeasured: not recreated.
    if ( ctx.forced >= 0 )
        return false;
    int found[ 8 ];
    int count = 0;
    if ( !water_find_surfaces( w, ctx, rect, found, count ) )
        return false;
    if ( count == 0 )
    {
        water_fill( ctx, water_bits( 0xFEFFFFFFu ) );
        return true;
    }
    if ( count == 1 && water_contains( w.surfaces[ found[ 0 ] ], ctx ) )
    {
        const water_surface_t& s = w.surfaces[ found[ 0 ] ];
        water_fill( ctx, s.height );
        return w.skip_waves || water_waves( w, s.wave_root, ctx, rect, kernel, user );
    }
    if ( !water_tag( ctx, w, found, count, tags ) )
        return false;
    if ( w.skip_waves )
        return true;
    for ( int k = 0; k < count; ++k )
    {
        if ( !water_waves( w, w.surfaces[ found[ k ] ].wave_root, ctx, rect, kernel, user ) )
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// THE ONE-POINT QUERY.
//
// 0x52AEC0 (fastcall root, const float* xy; struct { VuVector4 point; surface*
// found }*): the surface-tree walk at (x, y) - point-in-node (x >= min x, y >=
// min y, max x >= x, max y >= y, comiss / jb), the second child popped first,
// the same 256-entry stack - keeping the LAST leaf whose space holds the point
// (half_x >= |lx|, half_y >= |ly|, above >= lz >= -below). -1 none.
inline int water_point_surface( const water_t& w, const float* p )
{
    if ( w.surface_root < 0 )
        return -1;
    int found = -1;
    int stack[ 257 ];
    int top = 1;
    stack[ 1 ] = w.surface_root;
    while ( top )
    {
        const water_node_t& node = w.nodes[ stack[ top-- ] ];
        if ( !water_ge( p[ 0 ], node.rect[ 0 ] ) || !water_ge( p[ 1 ], node.rect[ 1 ] ) || !water_ge( node.rect[ 2 ], p[ 0 ] ) ||
             !water_ge( node.rect[ 3 ], p[ 1 ] ) )
            continue;
        if ( node.second >= 0 )
        {
            if ( top + 2 > 256 )
                return -2;   // retail's stack overflows: not recreated
            stack[ ++top ] = node.first;
            stack[ ++top ] = node.second;
            continue;
        }
        const water_surface_t& s = w.surfaces[ node.first ];
        float lx, ly, lz;
        water_local( s, p, lx, ly, lz );
        if ( water_ge( s.half_x, water_abs( lx ) ) && water_ge( s.half_y, water_abs( ly ) ) && water_ge( s.above, lz ) &&
             water_ge( lz, -s.below ) )
            found = node.first;
    }
    return found;
}

// 0x5292D0 (thiscall water; out*, const VuVector4* point; retn 8; returns out):
// VuWater's height at ONE point, in the renderer's +96 record form (32 bytes;
// see world/wave.hxx's +96 kernels):
//   out +0 / +4 = x, y; +8 = 0xFEFFFFFF; +0xC / +0x10 / +0x14 = 0; +0x18 = 1.0;
//   +0x1C = 0 (stored in that order: +0x10 last).
//   0x52AEC0 finds the surface; none: done. Else +8 = its height (+0x14) and
//   +0x18 = 0x535E10's map sample; then, unless water +0x18 (skip waves) is set,
//   a query ctx on the stack - 0x413500( 1 ) (aabb +-FLT_MAX, +0x60 = 1, +0x38 /
//   +0x4C / +0x58 / +0x5C / +0x64 = 0), count 1, aabb min = max = centre = the
//   point (all four lanes), radius 0, +0x50 = out (the stride +0x54 is never
//   written: one record, index 0, so it is never used) - and 0x52B0E0 walks that
//   surface's wave tree with it.
// 0x52B0E0 (fastcall root, const float* xy; ctx**) is 0x52A8D0 with the rect
// test made point-in-node: the same four ordered comparisons as water_overlap
// on the degenerate rect (x, y, x, y), so water_waves on that rect IS it (the
// exclusion list, the circle test against ctx +0x24 / +0x28 / +0x34, ctx+0x5C =
// the node's tag, slot 3).
// False when a path not recreated is reached (a kernel hook answering false, a
// stack overflow, a map cell outside the map).
inline bool water_point_query( const water_t& w, const float* point, water_kernel_fn kernel, void* user, float* out )
{
    out[ 0 ] = point[ 0 ];
    out[ 1 ] = point[ 1 ];
    out[ 2 ] = water_bits( 0xFEFFFFFFu );
    out[ 3 ] = 0.0f;
    out[ 5 ] = 0.0f;
    out[ 6 ] = 1.0f;
    out[ 7 ] = 0.0f;
    out[ 4 ] = 0.0f;
    const int found = water_point_surface( w, point );
    if ( found == -2 )
        return false;
    if ( found < 0 )
        return true;
    const water_surface_t& s = w.surfaces[ found ];
    out[ 2 ] = s.height;
    if ( !water_map_sample( s, point, out[ 6 ] ) )
        return false;
    if ( w.skip_waves )
        return true;
    water_query_t ctx{};
    ctx.count = 1;
    for ( int k = 0; k < 4; ++k )
    {
        ctx.aabb[ k ] = point[ k ];
        ctx.aabb[ 4 + k ] = point[ k ];
        ctx.centre[ k ] = point[ k ];
    }
    ctx.radius = 0.0f;
    ctx.exclude_count = 0;
    ctx.forced = -1;
    ctx.vertices = nullptr;
    ctx.tags = nullptr;
    ctx.tag = 0;
    ctx.flag96 = 1;
    ctx.flag100 = 0;
    ctx.records = reinterpret_cast< unsigned char* >( out );
    ctx.stride = 32;
    const float rect[ 4 ] = { point[ 0 ], point[ 1 ], point[ 0 ], point[ 1 ] };
    return water_waves( w, s.wave_root, ctx, rect, kernel, user );
}
