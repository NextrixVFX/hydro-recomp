#pragma once

#include <workspace/core/world/query.hxx>
#include <workspace/core/world/surface.hxx>

#include <cstdint>
#include <cstring>
#include <vector>

// THE WATER QUERY ON A BUILT WATER: query.hxx's water_t flattened from what
// water_build_world leaves (surface.hxx's water_built_t), and the wave slot 3
// query.hxx leaves as a hook - each wave's vt+12 - dispatched on the wave's
// kind to the verified per-vertex kernels of wave.hxx.
//
// SLOT 3, read for every kind built here (the vtables 0x75093C ramp, 0x750768
// directional, 0x7509B4 bump, 0x7509F4 banked turn, 0x750900 whirlpool): the
// same four-way switch on ctx+0x60 / ctx+0x64 -
//
//                    +0x60 == 0          +0x60 != 0
//   +0x64 == 0       base kernel         +96 kernel (the renderer's)
//   +0x64 != 0       the tag variant     +96 tag variant
//
//   ramp         0x5A23B0 -> 0x5A2C10 / 0x5A2DE0 (0x5A2FC0 / 0x5A3160)
//   directional  0x59AAF0 -> 0x59B110 / 0x59B4C0 (0x59B860 / 0x59BDE0)
//   bump         0x5A3FE0 -> 0x5A4140 / 0x5A43A0 (0x5A4610 / 0x5A49A0)
//   banked turn  0x5A5040 -> 0x5A55C0 / 0x5A5830 (0x5A5AB0 / 0x5A5F10)
//   whirlpool    0x5A16A0 -> 0x5A1AD0 / 0x5A1D20 (0x5A1F80 / 0x5A2160)
//   point        0x5A3510 -> 0x5A36A0 / 0x5A3820 (0x5A39B0 / 0x5A3CA0), vtable 0x750978
//
// THE TAG VARIANT IS THE BASE BODY BEHIND ONE TEST, read by an instruction
// diff of each pair (hydro-tools census/vdiff.py): per vertex i,
// `if ( ((u32*) ctx+0x58)[ i ] != ctx+0x5C ) continue;` - the rest differs
// only in register / stack-slot allocation and in the operand order of a few
// commutative mulss / addss (directional 0x59B644, ramp 0x5A2F63 / 0x5A2F73),
// which gives the same bits unless BOTH operands are NaN (SSE then keeps the
// first's payload). That NaN-payload case is not modelled.
//
// Every base kernel adds into the vertex records (+16 the displacement, +32
// the height) and loops i = 0 .. ctx+0 over ctx+0x50 with stride ctx+0x54 (40
// here, query.hxx's water_vertex_t). The +96 kernels (wave.hxx's *_vertex96,
// each a separate body; its tag variant the same behind the tag test) work on
// the renderer's raw records (ctx.records / ctx.stride): +8 height, +0xC /
// +0x10 the in-plane gradient, +0x14 foam.
//
// The rectangular / circular ocean waves (slot 3 0x59DA30 / 0x59DEC0, the same
// switch: 0x59ED30 / 0x59F2B0 (0x59F840 / 0x59FB80), 0x59FED0 / 0x5A03F0
// (0x5A0920 / 0x5A0C40)) read the build's field (world/field.hxx, b.oceans) as
// the view snapshot of the wave's clock words - rebuild the view after every
// water_built_tick.
//
// NOT RECREATED, so the hook answers false (UNKNOWN) and the query with it:
//   - wake / flow waves: none is made by water_build_world and their creation
//     at run time is not recreated. Point waves come from
//     water_create_point_wave (surface.hxx, 0x528E10).

struct water_wave_kernel_t
{
    water_kind_t kind = water_kind_none;
    whirlpool_t whirlpool{};
    directional_t directional{};
    ramp_t ramp{};
    bump_t bump{};
    banked_turn_t banked{};
    point_wave_t point{};
    ocean_patch_t ocean{};
    wake_wave_t wake{};   // the wakes (world/wake.hxx): +0x4C .. +0xBF   // patches[] point into the build's field (b.oceans): null = not built
};

// The rectangular / circular ocean kernels' members (wave.hxx's ocean_patch_t
// note names each offset) from the wave's words, the patch buffers from its
// field (world/field.hxx).
inline void water_ocean_kernel( water_kind_t kind, const water_words_t& w, const ocean_field_t& f, ocean_patch_t& k )
{
    k.shift = static_cast< int >( w.word[ 108 / 4 ] );
    k.mask = static_cast< int >( w.word[ 116 / 4 ] );
    k.scale = w.f( 120 );
    k.sine = w.f( 128 );
    k.cosine = w.f( 132 );
    std::memcpy( &k.now, &w.word[ 136 / 4 ], 8 );
    k.current = static_cast< int >( w.word[ 164 / 4 ] );
    k.previous = static_cast< int >( w.word[ 168 / 4 ] );
    std::memcpy( &k.time[ 0 ], &w.word[ 176 / 4 ], 8 );
    std::memcpy( &k.time[ 1 ], &w.word[ 192 / 4 ], 8 );
    // the infinite ocean (0xD0 bytes) has no centre / footprint words
    k.centre_x = kind == water_kind_infinite ? 0.0f : w.f( 208 );
    k.centre_y = kind == water_kind_infinite ? 0.0f : w.f( 212 );
    const int shape = kind == water_kind_rectangular ? 4 : kind == water_kind_circular ? 2 : 0;
    for ( int i = 0; i < shape; ++i )
        k.shape[ i ] = w.f( 224 + 4 * i );
    k.patches[ 0 ] = f.n ? f.patches[ 0 ].data( ) : nullptr;
    k.patches[ 1 ] = f.n ? f.patches[ 1 ].data( ) : nullptr;
}

// The members each kernel reads, from the wave's words by retail offset (the
// offsets are wave.hxx's, one per kernel note).
inline water_wave_kernel_t water_wave_kernel( water_kind_t kind, const water_words_t& w )
{
    water_wave_kernel_t k{};
    k.kind = kind;
    switch ( kind )
    {
    case water_kind_whirlpool:   // 0x5A1AD0
        k.whirlpool.centre_x = w.f( 76 );
        k.whirlpool.centre_y = w.f( 80 );
        k.whirlpool.outer_radius = w.f( 92 );
        k.whirlpool.inner_radius = w.f( 96 );
        k.whirlpool.depth = w.f( 100 );
        k.whirlpool.angular_speed = w.f( 104 );
        k.whirlpool.linear_speed = w.f( 108 );
        k.whirlpool.foaminess = w.f( 112 );   // the +96 kernel 0x5A1F80
        break;
    case water_kind_directional:   // 0x59B110
        k.directional.amplitude = w.f( 104 );
        k.directional.wavenumber = w.f( 108 );
        k.directional.phase_scale = w.f( 112 );
        k.directional.edge_b = w.f( 116 );
        k.directional.edge_a = w.f( 120 );
        k.directional.clock = w.f( 124 );
        for ( int i = 0; i < 16; ++i )
            k.directional.transform[ i ] = w.f( 192 + 4 * i );
        break;
    case water_kind_ramp:   // 0x5A2C10
        k.ramp.blend_height = w.f( 100 );
        k.ramp.band = w.f( 112 );
        k.ramp.along_x = w.f( 188 );
        k.ramp.along_y = w.f( 204 );
        k.ramp.along_offset = w.f( 236 );
        k.ramp.curve = w.f( 248 );
        k.ramp.slope = w.f( 252 );
        k.ramp.grad_x = w.f( 256 );   // the +96 kernel 0x5A2FC0
        k.ramp.grad_y = w.f( 260 );
        for ( int i = 0; i < 3; ++i )
        {
            k.ramp.flow_inner[ i ] = w.f( 280 + 4 * i );
            k.ramp.flow_outer[ i ] = w.f( 296 + 4 * i );
        }
        break;
    case water_kind_bump:   // 0x5A4140
        k.bump.amplitude = w.f( 104 );
        k.bump.edge_a = w.f( 108 );
        for ( int i = 0; i < 16; ++i )
            k.bump.transform[ i ] = w.f( 176 + 4 * i );
        break;
    case water_kind_banked:   // 0x5A55C0
        k.banked.centre_x = w.f( 76 );
        k.banked.centre_y = w.f( 80 );
        k.banked.outer_radius = w.f( 96 );
        k.banked.inner_radius = w.f( 100 );
        k.banked.amplitude = w.f( 104 );
        k.banked.edge = w.f( 112 );
        k.banked.direction_x = w.f( 116 );
        k.banked.direction_y = w.f( 120 );
        k.banked.extent = w.f( 124 );
        break;
    case water_kind_wake:
    case water_kind_flat_wake: k.wake = water_wake_of( w ); break;
    case water_kind_point:   // 0x5A36A0
        k.point.centre_x = w.f( 76 );
        k.point.centre_y = w.f( 80 );
        k.point.tail_width = w.f( 92 );
        k.point.amplitude = w.f( 96 );
        k.point.inner_radius = w.f( 100 );
        k.point.outer_radius = w.f( 104 );
        k.point.phase_scale = w.f( 112 );
        k.point.clock = w.f( 120 );
        k.point.rate = w.f( 124 );
        k.point.amplitude_scale = w.f( 128 );
        k.point.slope = w.f( 132 );
        k.point.speed = w.f( 108 );   // the +96 kernel 0x5A39B0
        k.point.foaminess = w.f( 116 );
        k.point.inv_amplitude = w.f( 136 );
        break;
    default: break;
    }
    return k;
}

// What the query runs on: the flattened water and, per wave (the hold's
// index), its kernel members.
struct water_view_t
{
    water_t water;
    std::vector< water_wave_kernel_t > kernels;
};

// One tree's nodes appended to the flat array; returns the root's new index
// (-1 none). A wave-tree leaf's tag is its surface (water_hold_test's tag) as
// query.hxx's surface index + 1; a surface-tree node's tag is unused.
inline int water_flatten_tree( const water_tree_t& t, bool waves, std::vector< water_node_t >& out )
{
    const int base = static_cast< int >( out.size( ) );
    for ( const water_tree_node_t& n : t.nodes )
    {
        water_node_t q{};
        std::memcpy( q.rect, n.rect, sizeof( q.rect ) );
        const bool leaf = n.second < 0;
        q.first = leaf ? n.first : n.first + base;
        q.second = leaf ? -1 : n.second + base;
        q.tag = waves && leaf ? n.tag + 1u : 0u;
        out.push_back( q );
    }
    return t.root < 0 ? -1 : t.root + base;
}

// water_t from a build: the surface tree (water +0x3C), then each surface's
// fields - +20 the height, +40 / +44 below / above, +144 the world-to-surface
// rows, +216 the floor (the z band's low end), +240 / +244 the half extents,
// +72 / +76 the sizes, +0x100 its water map (b.maps, null until
// water_build_maps) - and its wave tree (+0xFC); each wave's centre (+44 /
// +48) and radius (+52).
// water +0x18 (skip waves) is the build's zero (VuWater's ctor).
inline void water_view_build( const water_built_t& b, water_view_t& v )
{
    v = water_view_t{};
    v.water.skip_waves = false;
    v.water.surface_root = water_flatten_tree( b.hold.surface_tree, false, v.water.nodes );
    for ( std::size_t i = 0; i < b.hold.surfaces.size( ); ++i )
    {
        const water_words_t& s = b.surfaces[ i ];
        water_surface_t q{};
        q.height = s.f( 20 );
        q.below = s.f( 40 );
        q.above = s.f( 44 );
        for ( int k = 0; k < 16; ++k )
            q.inverse[ k ] = s.f( 144 + 4 * k );
        q.floor = s.f( 216 );
        q.half_x = s.f( 240 );
        q.half_y = s.f( 244 );
        q.size_x = static_cast< std::int32_t >( s.word[ 72 / 4 ] );
        q.size_y = static_cast< std::int32_t >( s.word[ 76 / 4 ] );
        q.map = i < b.maps.size( ) ? b.maps[ i ].get( ) : nullptr;   // +0x100 (water_build_maps)
        q.wave_root = water_flatten_tree( b.hold.surfaces[ i ].waves, true, v.water.nodes );
        v.water.surfaces.push_back( q );
    }
    for ( std::size_t i = 0; i < b.hold.waves.size( ); ++i )
    {
        const water_held_wave_t& h = b.hold.waves[ i ];
        water_wave_t q{};
        q.centre[ 0 ] = h.centre[ 0 ];
        q.centre[ 1 ] = h.centre[ 1 ];
        q.radius = h.radius;
        v.water.waves.push_back( q );
        v.kernels.push_back( water_wave_kernel( b.wave_kind[ i ], b.waves[ i ] ) );
        const water_kind_t kind = b.wave_kind[ i ];
        if ( kind == water_kind_rectangular || kind == water_kind_circular || kind == water_kind_infinite )
            if ( const water_built_t::ocean_t* o = water_ocean_of( b, static_cast< int >( i ) ) )
                water_ocean_kernel( kind, b.waves[ i ], o->field, v.kernels.back( ).ocean );
    }
}

// The wave's slot 3 (query.hxx's water_kernel_fn; user = the water_view_t).
inline bool water_view_kernel( void* user, int wave, water_query_t& ctx )
{
    const water_view_t& v = *static_cast< const water_view_t* >( user );
    const water_wave_kernel_t& k = v.kernels[ wave ];
    if ( ctx.flag96 )
    {
        // The +96 kernels (wave.hxx), on the raw records.
        switch ( k.kind )
        {
        case water_kind_whirlpool:
        case water_kind_directional:
        case water_kind_ramp:
        case water_kind_bump:
        case water_kind_banked:
        case water_kind_wake:
        case water_kind_flat_wake:
        case water_kind_point: break;
        case water_kind_rectangular:
        case water_kind_circular:
        case water_kind_infinite:
            if ( !k.ocean.patches[ 0 ] )
                return false;   // no field (complexity 0)
            break;
        default: return false;   // a kind not built
        }
        for ( int i = 0; i < ctx.count; ++i )
        {
            if ( ctx.flag100 && ctx.tags[ i ] != ctx.tag )
                continue;
            if ( k.kind == water_kind_wake || k.kind == water_kind_flat_wake )
            {
                // 0x52D1D0 / 0x52DCC0, flat 0x52E7D0 / 0x52EAE0
                if ( k.kind == water_kind_wake )
                    wake_vertex96( k.wake, *water_record( ctx, i, 0 ), *water_record( ctx, i, 4 ), *water_record( ctx, i, 8 ),
                        water_record( ctx, i, 0xC ), *water_record( ctx, i, 0x14 ) );
                else
                    flat_wake_vertex96( k.wake, *water_record( ctx, i, 0 ), *water_record( ctx, i, 4 ), *water_record( ctx, i, 0x14 ) );
                continue;
            }
            if ( k.kind == water_kind_infinite )
            {
                // SampleHeight 0x59E9E0 / SampleHeight2 0x59EB80
                ocean_infinite_vertex96( k.ocean, *water_record( ctx, i, 0 ), *water_record( ctx, i, 4 ), *water_record( ctx, i, 8 ),
                    water_record( ctx, i, 0xC ) );
                continue;
            }
            if ( k.kind == water_kind_rectangular || k.kind == water_kind_circular )
            {
                // 0x59F840 / 0x59FB80, 0x5A0920 / 0x5A0C40: height and flow only
                ocean_patch_vertex96( k.ocean, k.kind == water_kind_circular, *water_record( ctx, i, 0 ), *water_record( ctx, i, 4 ),
                    *water_record( ctx, i, 8 ), water_record( ctx, i, 0xC ) );
                continue;
            }
            const float x = *water_record( ctx, i, 0 );
            const float y = *water_record( ctx, i, 4 );
            float& height = *water_record( ctx, i, 8 );
            float* flow = water_record( ctx, i, 0xC );
            float& foam = *water_record( ctx, i, 0x14 );
            switch ( k.kind )
            {
            case water_kind_whirlpool: whirlpool_vertex96( k.whirlpool, x, y, height, flow, foam ); break;
            case water_kind_directional: directional_vertex96( k.directional, x, y, height, flow ); break;
            case water_kind_ramp: ramp_vertex96( k.ramp, x, y, height, flow ); break;
            case water_kind_bump: bump_vertex96( k.bump, x, y, height, flow ); break;
            case water_kind_banked: banked_turn_vertex96( k.banked, x, y, height, flow ); break;
            default: point_wave_vertex96( k.point, x, y, height, flow, foam ); break;
            }
        }
        return true;
    }
    switch ( k.kind )
    {
    case water_kind_whirlpool:
    case water_kind_directional:
    case water_kind_ramp:
    case water_kind_bump:
    case water_kind_banked:
    case water_kind_wake:
    case water_kind_point: break;
    case water_kind_flat_wake: return true;   // slot 3 0x52CA30 does nothing without ctx+0x60
    case water_kind_rectangular:
    case water_kind_circular:
    case water_kind_infinite:
        if ( !k.ocean.patches[ 0 ] )
            return false;   // no field (complexity 0)
        break;
    default: return false;   // a kind not built
    }
    for ( int i = 0; i < ctx.count; ++i )
    {
        if ( ctx.flag100 && ctx.tags[ i ] != ctx.tag )
            continue;
        water_vertex_t& x = ctx.vertices[ i ];
        switch ( k.kind )
        {
        case water_kind_wake:   // 0x52CA60 / 0x52CE20
            wake_vertex( k.wake, x.pos[ 0 ], x.pos[ 1 ], x.disp, x.height );
            break;
        case water_kind_infinite:   // 0x59E070 / 0x59E520
            ocean_infinite_vertex( k.ocean, x.pos[ 0 ], x.pos[ 1 ], x.disp, x.height );
            break;
        case water_kind_rectangular:   // 0x59ED30 / 0x59F2B0
        case water_kind_circular:      // 0x59FED0 / 0x5A03F0
            ocean_patch_vertex( k.ocean, k.kind == water_kind_circular, x.pos[ 0 ], x.pos[ 1 ], x.disp, x.height );
            break;
        case water_kind_whirlpool: whirlpool_vertex( k.whirlpool, x.pos[ 0 ], x.pos[ 1 ], x.disp, x.height ); break;
        case water_kind_directional: directional_vertex( k.directional, x.pos[ 0 ], x.pos[ 1 ], x.disp, x.height ); break;
        case water_kind_ramp: ramp_vertex( k.ramp, x.pos[ 0 ], x.pos[ 1 ], x.disp, x.height ); break;
        case water_kind_bump: bump_vertex( k.bump, x.pos[ 0 ], x.pos[ 1 ], x.disp, x.height ); break;
        case water_kind_banked: banked_turn_vertex( k.banked, x.pos[ 0 ], x.pos[ 1 ], x.disp, x.height ); break;
        default: point_wave_vertex( k.point, x.pos[ 0 ], x.pos[ 1 ], x.disp, x.height ); break;
        }
    }
    return true;
}
