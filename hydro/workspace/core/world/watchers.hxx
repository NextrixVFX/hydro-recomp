#pragma once

// THE WATER ENTITIES' MOTION / CONTROL HANDLERS and the transform watchers
// they lead to - what moves a built surface or wave after the level load.
//
// Two interfaces, both a three-slot vtable whose slot 0 is the handler (slots
// 1 / 2 the nullsub 0x401560), thiscall on the interface's sub-object:
//
//   VuMotionComponentIF (entity +0x5C, the point wave's +0x60): told by its
//     VuMotionComponent (vtable 0x74E984, ctor 0x552CD0: +8 the owner, +0x10
//     the interface, +0x18 a matrix seeded from the identity 0x7593B0) - the
//     handler hands that matrix to the transform's setWorldTransform 0x5344A0
//     (world/race.hxx entity_set_world), which fires the transform's watcher.
//   VuControlComponentIF (entity +0x5C; vtable 0x753B80 base): told by its
//     VuControlComponent (vtable 0x757EA0, built inline in the entity ctors:
//     +8 the owner, +0x10 the interface, +0x18 .. +0x20 a std::vector of
//     12-byte channel records {name, value, value} - 0x5AD8C0's push_back of
//     each authored property's value twice). The handlers read each record's
//     +8: `records` below is that vector's data, three words per record.
//
// The handlers' vtables (slot 0): VuPointWaveEntity 0x753434 -> 0x5D4C30,
// VuWaterRampEntity / VuWaterSurfaceEntity 0x75378C / 0x753F08 -> 0x5DC3B0,
// the oceans 0x753AD8 / 0x753B24 / 0x753B70 / 0x753B90 -> 0x5D8930,
// VuDirectionalWaveEntity 0x753CE0 -> 0x5D9D90, VuStaticPfxEntity 0x755EB4 ->
// 0x5EB4C0 (world/pfxstatic.hxx's entity).

#include <workspace/core/world/race.hxx>   // entity_set_world (and, through game/types.hxx, world/surface.hxx)
#include <workspace/core/world/pfxstatic.hxx>

#include <functional>

#include <cstdint>
#include <cstring>

// After 0x529B90 took a wave out of the trees and before 0x529A90 puts it back:
// the hold's copy of what retail reads from the wave object itself (its box,
// centre, radius, centre-test bit); the entry list is the removal's (empty).
inline void water_hold_resync( water_built_t& b, int wave )
{
    water_held_wave_t& h = b.hold.waves[ static_cast< std::size_t >( wave ) ];
    const water_held_wave_t was = h;
    h = water_held_from( b.waves[ static_cast< std::size_t >( wave ) ].word );
    h.entries = was.entries;
    h.wake = was.wake;
    h.wake_data = was.wake_data;
}

// ---------------------------------------------------------------------------
// The ocean entities' control handler 0x5D8930 (thiscall on entity +0x5C):
// record 0's +8 (the control component +0x64 -> +0x18 -> [0] +8) to the
// entity's Height Multiplier +0x84, then the wave the entity's slot 13 answers
// (0x5D81A0 = +0x94 for the rectangular / circular entities, 0x5D8180 = +0x88
// for the infinite one) gets it at +0x68 - movss both, the bits unchanged. The
// field's heights 0x59D640 read +0x68 at every rebuild, so b.oceans[].params
// follows. `wave` -1 (the entity's word null): retail writes through it
// unconditionally - UNKNOWN, false and only the entity's word written.
inline bool water_ocean_control( water_built_t& b, water_entity_state_t& s, int wave, const std::uint32_t* records )
{
    const std::uint32_t v = records[ 2 ];
    s.value.word[ ( 0x84 - 0x60 ) / 4 ] = v;
    s.value.known[ ( 0x84 - 0x60 ) / 4 ] = true;
    if ( wave < 0 )
        return false;
    b.waves[ static_cast< std::size_t >( wave ) ].set( 0x68, v );
    if ( water_built_t::ocean_t* o = water_ocean_of( b, wave ) )
        o->params = ocean_params_of( b.waves[ static_cast< std::size_t >( wave ) ].word );
    return true;
}

// ---------------------------------------------------------------------------
// VuDirectionalWaveEntity. Its Vu3dLayoutComponent (entity +0x60) box +0x18 ..
// +0x37 as 0x5D9DD0 writes it: (-0.5, -0.5, -Max Height, w) and (0.5, 0.5,
// Max Height, w); both w lanes are stack words that function never writes
// (UNKNOWN, known false).
struct water_layout_box_t
{
    std::uint32_t word[ 8 ]{};
    bool known[ 8 ]{};
};

// 0x5D9DD0 (thiscall entity) - also the transform's watcher (the ctor's
// 0x5DA2A0 installs it at transform +0xC0): when the entity holds a wave
// (+0x80): a zeroed 48-byte descriptor filled by 0x5D9EB0 from the transform
// and the properties, the wave re-described by 0x59ACD0 (the clock kept), its
// bounds 0x59AE00, out of VuWater's trees 0x529B90 and back in 0x529A90; then,
// wave or not, the layout box. `wave` -1 = none.
inline void water_directional_refresh( water_built_t& b, const water_entity_state_t& s, int wave, const transform_component_t& t,
                                       water_layout_box_t& layout )
{
    if ( wave >= 0 )
    {
        std::uint32_t transform[ k_water_transform_words ];
        water_transform_words( t, transform );
        water_input_t in{};
        in.entity = s.value.word;
        in.transform = transform;
        std::uint32_t d[ 12 ] = {};
        directional_desc( in.source( ), d );
        water_words_t& w = b.waves[ static_cast< std::size_t >( wave ) ];
        wave_image_t image{};
        for ( int k = 0; k < 64; ++k )
        {
            image.word[ k ] = w.word[ k ];
            image.known[ k ] = w.known[ k ];
        }
        directional_set( image, d );
        wave_rect_bounds( image );
        for ( int k = 0; k < 64; ++k )
        {
            w.word[ k ] = image.word[ k ];
            w.known[ k ] = image.known[ k ];
        }
        water_hold_remove( b.hold, wave );
        water_hold_resync( b, wave );
        water_hold_add( b.hold, wave );
    }
    const std::uint32_t h = s.value.word[ ( 0x6C - 0x60 ) / 4 ];
    const std::uint32_t box[ 8 ] = { 0xBF000000u, 0xBF000000u, h ^ 0x80000000u, 0u, 0x3F000000u, 0x3F000000u, h, 0u };   // xorps 0x6F7CD0
    for ( int k = 0; k < 8; ++k )
    {
        layout.word[ k ] = box[ k ];
        layout.known[ k ] = k != 3 && k != 7;
    }
}

// VuDirectionalFlowWaveEntity's 0x5D6740 (thiscall entity) - its slot 2
// 0x5D66C0 (a call, retn 4) and the transform's watcher (the ctor 0x5D6410
// installs it): with a wave (+0x78) a zeroed 0x34-byte descriptor from 0x5D6810
// (wave.hxx flow_desc), 0x59C3C0 (flow_set), the bounds 0x59C4C0, out of the
// trees 0x529B90 and back 0x529A90; then, wave or not, its Vu3dLayoutComponent
// (entity +0x5C) box +0x18 .. +0x37 = (-0.5, -0.5, 0, w) (0.5, 0.5, 0, w), both
// w lanes stack words never written (UNKNOWN). Verified: case flow_wave.
inline void water_flow_refresh( water_built_t& b, const water_entity_state_t& s, int wave, const std::uint32_t* transform,
                                water_layout_box_t& layout )
{
    if ( wave >= 0 )
    {
        water_input_t in{};
        in.entity = s.value.word;
        in.transform = transform;
        std::uint32_t d[ 13 ] = {};
        flow_desc( in.source( ), d );
        water_words_t& w = b.waves[ static_cast< std::size_t >( wave ) ];
        wave_image_t image{};
        for ( int k = 0; k < 64; ++k )
        {
            image.word[ k ] = w.word[ k ];
            image.known[ k ] = w.known[ k ];
        }
        flow_set( image, d );
        wave_rect_bounds( image, true );
        for ( int k = 0; k < 64; ++k )
        {
            w.word[ k ] = image.word[ k ];
            w.known[ k ] = image.known[ k ];
        }
        water_hold_remove( b.hold, wave );
        water_hold_resync( b, wave );
        water_hold_add( b.hold, wave );
    }
    const std::uint32_t box[ 8 ] = { 0xBF000000u, 0xBF000000u, 0u, 0u, 0x3F000000u, 0x3F000000u, 0u, 0u };
    for ( int k = 0; k < 8; ++k )
    {
        layout.word[ k ] = box[ k ];
        layout.known[ k ] = k != 3 && k != 7;
    }
}

// The control handler 0x5D9D90 (thiscall on entity +0x5C): records 0..4's +8
// (control component +0x68 -> +0x18) to the entity's Max Height +0x6C, Speed
// +0x70, Period +0x74, Longitudinal +0x78 and Lateral Decay Ratio +0x7C (plain
// dword moves), then 0x5D9DD0 (a tail jump).
inline void water_directional_control( water_built_t& b, water_entity_state_t& s, int wave, const transform_component_t& t,
                                       const std::uint32_t* records, water_layout_box_t& layout )
{
    for ( int k = 0; k < 5; ++k )
    {
        s.value.word[ ( 0x6C - 0x60 ) / 4 + k ] = records[ 3 * k + 2 ];
        s.value.known[ ( 0x6C - 0x60 ) / 4 + k ] = true;
    }
    water_directional_refresh( b, s, wave, t, layout );
}

// ---------------------------------------------------------------------------
// THE WAVE ENTITIES' TRANSFORM WATCHERS (each the entity's slot 12, reached
// through the transform's +0xC0 thunk 0x4077E3 when setWorldTransform 0x5344A0
// notifies): with the entity's wave, a zeroed descriptor built the way slot 6
// built it, the wave re-described and re-held (VuWater 0x529B90 then 0x529A90);
// then, wave or not, the Vu3dLayoutComponent's local box +0x18 .. +0x37 (both w
// lanes stack words never written: UNKNOWN). `wave` -1 = none.
//   VuRectangularOceanWaveEntity 0x5D8D70 (wave +0x94, layout +0x88):
//     buildRegionDesc 0x5D8E20, 0x59D9B0 = Update 0x59CCB0 (ocean_modify + the
//     field), +208 .. +239 = desc 8 .. 15, bounds 0x59DBF0; box (-0.5, -0.5, 0)
//     .. (0.5, 0.5, 1.0).
//   VuCircularOceanWaveEntity 0x5D9390 (wave +0x94, layout +0x88, Radius
//     +0x8C): 0x5D9440, 0x59DDC0 = Update, +208 .. +231 = desc 8 .. 13, the
//     bounds inline; box (-R, -R, 0) .. (R, R, 1.0).
//   VuWhirlpoolWaveEntity 0x5D7E30 (wave +0x78, layout +0x5C): the position
//     and +0x60 .. +0x77, 0x5A1580 (whirlpool_set; the clock +116 kept); box
//     (-R, -R, -Depth) .. (R, R, 0), R = +0x60, Depth = +0x68.
//   VuBankedTurnWaveEntity 0x5D36C0 (wave +0x74, layout +0x5C): the position,
//     the transform's +0xA8, +0x60 .. +0x73, 0x5A4EF0 (banked_turn_set); box
//     (-R, -R, 0) .. (R, R, Height), R = +0x60, Height = +0x68.
// -R is xorps with 0x6F7CD0 (the bits of +R, sign flipped). An ocean whose
// new Complexity is 0 answers false (ocean_modify: UNKNOWN); every other call
// true.
inline bool water_wave_refresh( water_built_t& b, const water_entity_state_t& s, int wave, const transform_component_t& t,
                                water_layout_box_t& layout )
{
    const water_kind_t kind = s.kind;
    bool known = true;
    if ( wave >= 0 )
    {
        std::uint32_t transform[ k_water_transform_words ];
        water_transform_words( t, transform );
        water_input_t in{};
        in.entity = s.value.word;
        in.transform = transform;
        water_words_t& w = b.waves[ static_cast< std::size_t >( wave ) ];
        wave_image_t image{};
        for ( int k = 0; k < 64; ++k )
        {
            image.word[ k ] = w.word[ k ];
            image.known[ k ] = w.known[ k ];
        }
        std::uint32_t d[ 16 ] = {};
        if ( kind == water_kind_rectangular || kind == water_kind_circular )
        {
            const bool rect = kind == water_kind_rectangular;
            if ( rect )
                rectangular_ocean_desc( in.source( ), d );
            else
                circular_ocean_desc( in.source( ), d );
            known = ocean_modify( image, d );
            for ( int k = 0; k < ( rect ? 8 : 6 ); ++k )
                image.set( 208 + 4 * k, d[ 8 + k ] );
            if ( rect )
                rectangular_ocean_bounds( image );
            else
                circular_ocean_bounds( image );
        }
        else if ( kind == water_kind_whirlpool )
        {
            whirlpool_desc( in.source( ), d );
            whirlpool_set( image, d );
        }
        else if ( kind == water_kind_banked )
        {
            banked_turn_desc( in.source( ), d );
            banked_turn_set( image, d );
        }
        else
            return false;
        for ( int k = 0; k < 64; ++k )
        {
            w.word[ k ] = image.word[ k ];
            w.known[ k ] = image.known[ k ];
        }
        if ( water_built_t::ocean_t* o = ( kind == water_kind_rectangular || kind == water_kind_circular ) ? water_ocean_of( b, wave ) : nullptr )
        {
            o->params = ocean_params_of( w.word );
            double time;
            std::memcpy( &time, &w.word[ 0x88 / 4 ], 8 );
            if ( known )
                ocean_field_update( o->field, o->params, time );
            else
                o->field = ocean_field_t{};
        }
        water_hold_remove( b.hold, wave );
        water_hold_resync( b, wave );
        water_hold_add( b.hold, wave );
    }
    std::uint32_t r = 0x3F000000u;
    std::uint32_t lo_z = 0u;
    std::uint32_t hi_z = 0x3F800000u;
    if ( kind == water_kind_circular )
        r = s.value.word[ ( 0x8C - 0x60 ) / 4 ];
    else if ( kind == water_kind_whirlpool || kind == water_kind_banked )
    {
        r = s.value.word[ 0 ];
        const std::uint32_t z = s.value.word[ ( 0x68 - 0x60 ) / 4 ];
        lo_z = kind == water_kind_whirlpool ? z ^ 0x80000000u : 0u;
        hi_z = kind == water_kind_whirlpool ? 0u : z;
    }
    const std::uint32_t box[ 8 ] = { r ^ 0x80000000u, r ^ 0x80000000u, lo_z, 0u, r, r, hi_z, 0u };
    for ( int k = 0; k < 8; ++k )
    {
        layout.word[ k ] = box[ k ];
        layout.known[ k ] = k != 3 && k != 7;
    }
    return known;
}

// The watcher behind a setWorldTransform of one of those entities (any
// caller's - an attach, a motion entity, the editor): entity_set_world with
// notify, then water_wave_refresh. False when entity_set_world refuses or the
// entity is not one of the four kinds built.
inline const water_built_t::entity_ref_t* water_entity_of( const water_built_t& b, const entity_t& e );
inline bool water_wave_motion( water_built_t& b, entity_t& e, const float* matrix, water_layout_box_t& layout )
{
    const water_built_t::entity_ref_t* r = water_entity_of( b, e );
    const water_entity_state_t* s = static_cast< const water_entity_state_t* >( e.m_state.get( ) );
    if ( !s || ( s->kind != water_kind_rectangular && s->kind != water_kind_circular && s->kind != water_kind_whirlpool && s->kind != water_kind_banked ) )
        return false;
    const int wave = r ? r->wave : -1;
    bool known = true;
    const bool ran = entity_set_world( e, matrix, true, [ & ]( ) {
        if ( const transform_component_t* t = entity_transform( e ) )
            known = water_wave_refresh( b, *s, wave, *t, layout );
    } );
    return ran && known;
}

// ---------------------------------------------------------------------------
// The built-water slot of an entity water_build_world built (null: not built).
inline const water_built_t::entity_ref_t* water_entity_of( const water_built_t& b, const entity_t& e )
{
    for ( const water_built_t::entity_ref_t& r : b.entities )
        if ( r.entity == &e )
            return &r;
    return nullptr;
}

// ---------------------------------------------------------------------------
// VuWaterSurfaceEntity's transform watcher = its slot 12 0x5DB610 (the ctor's
// 0x5DC5F0 puts the thunk 0x4077E3 - `jmp [vtable + 0x30]` - at transform
// +0xC0; the ctor also runs it once). What it leaves outside the water: the
// Vu3dDrawComponent's (+0x60) world box +0x24 .. +0x43 and the
// Vu3dLayoutComponent's (+0x64) local box +0x18 .. +0x37 (w lanes: stack words
// never written - UNKNOWN), and, when the draw component's +0x1E byte
// (registered) is set, g_vu3ddrawmanager 0x7590B8 vt+0x30( draw ) - the
// renderer's update: `draw_update`, the caller's.
struct water_surface_boxes_t
{
    water_layout_box_t draw;
    water_layout_box_t layout;
    bool draw_update = false;
};

// 0x5DB610: the descriptor 0x5DBA90 (water_surface_desc); when the entity has
// its surface (+0x128): 0x535C50 on it (water_surface_set) and, the surface
// being in the surface tree (+0xF8), its tail - VuWater 0x529990 (out of the
// trees, its wave tree's nodes freed, the waves' entries into it pooled) then
// 0x529860 (back in, every listed wave tested again). Then the boxes: lo =
// ((float) X Size * -0.5, (float) Y Size * -0.5, -Max Wave Depth), hi = (X *
// 0.5, Y * 0.5, Max Wave Height); draw = 0x41B2A0( lo, hi, the transform's
// world +0x50 ); layout = lo, then (hi.x, hi.y, maxss( Max Wave Height,
// Reflection Height +0x80 )). `surface` -1 = none.
inline void water_surface_refresh( water_built_t& b, const water_entity_state_t& s, int surface, const transform_component_t& t,
                                   bool draw_registered, water_surface_boxes_t& out )
{
    if ( surface >= 0 )
    {
        std::uint32_t transform[ k_water_transform_words ];
        water_transform_words( t, transform );
        water_input_t in{};
        in.entity = s.value.word;
        in.transform = transform;
        std::uint32_t d[ 14 ];
        water_surface_desc( in, d );
        water_words_t& w = b.surfaces[ static_cast< std::size_t >( surface ) ];
        water_surface_set( w, d );
        if ( b.hold.surfaces[ static_cast< std::size_t >( surface ) ].leaf >= 0 )
        {
            water_hold_drop_surface( b.hold, surface );
            water_hold_surface_insert( b.hold, w, surface );
        }
    }
    auto f = [ & ]( int offset ) { return wave_bits( s.value.word[ ( offset - 0x60 ) / 4 ] ); };
    const float sx = static_cast< float >( static_cast< std::int32_t >( s.value.word[ ( 0x70 - 0x60 ) / 4 ] ) );
    const float sy = static_cast< float >( static_cast< std::int32_t >( s.value.word[ ( 0x74 - 0x60 ) / 4 ] ) );
    std::uint32_t depth = s.value.word[ ( 0x78 - 0x60 ) / 4 ] ^ 0x80000000u;   // xorps 0x6F7CD0
    const float lo[ 3 ] = { sx * -0.5f, sy * -0.5f, wave_bits( depth ) };
    const float hi[ 3 ] = { sx * 0.5f, sy * 0.5f, f( 0x7C ) };
    float blo[ 3 ];
    float bhi[ 3 ];
    water_box_transform( lo, hi, t.world, blo, bhi );
    const float top = _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( hi[ 2 ] ), _mm_set_ss( f( 0x80 ) ) ) );
    const float draw[ 8 ] = { blo[ 0 ], blo[ 1 ], blo[ 2 ], 0.0f, bhi[ 0 ], bhi[ 1 ], bhi[ 2 ], 0.0f };
    const float layout[ 8 ] = { lo[ 0 ], lo[ 1 ], lo[ 2 ], 0.0f, hi[ 0 ], hi[ 1 ], top, 0.0f };
    for ( int k = 0; k < 8; ++k )
    {
        std::memcpy( &out.draw.word[ k ], &draw[ k ], 4 );
        std::memcpy( &out.layout.word[ k ], &layout[ k ], 4 );
        out.draw.known[ k ] = out.layout.known[ k ] = k != 3 && k != 7;
    }
    out.draw_update = draw_registered;
}

// VuWaterRampEntity's slot 12 0x5D70A0: 0x5DB610, then 0x5D70B0 - when the
// entity has its wave (+0x140): the 44-byte descriptor (memset; the
// transform's +0x80 position, (float) X / Y Size, Z Size +0x134, 0, the
// transform's +0xA8, Transition Ratio +0x138, Flow Speed +0x13C:
// ramp_wave_desc) and 0x5A2370 on the wave - 0x5A2570 (ramp_wave_set), the
// bounds 0x5A28F0, VuWater 0x529B90 then 0x529A90.
inline void water_ramp_refresh( water_built_t& b, const water_entity_state_t& s, int surface, int wave, const transform_component_t& t,
                                bool draw_registered, water_surface_boxes_t& out )
{
    water_surface_refresh( b, s, surface, t, draw_registered, out );
    if ( wave < 0 )
        return;
    std::uint32_t transform[ k_water_transform_words ];
    water_transform_words( t, transform );
    water_input_t in{};
    in.entity = s.value.word;
    in.transform = transform;
    std::uint32_t d[ 11 ];
    ramp_wave_desc( in, d );
    water_words_t& w = b.waves[ static_cast< std::size_t >( wave ) ];
    ramp_wave_set( w, d );
    ramp_wave_bounds( w );
    water_hold_remove( b.hold, wave );
    water_hold_resync( b, wave );
    water_hold_add( b.hold, wave );
}

// ---------------------------------------------------------------------------
// THE MOTION HANDLERS. Both are 0x5344A0( the transform (entity +0x50), the
// motion component's +0x18 matrix, notify 1 ) = entity_set_world (world/
// race.hxx), whose watcher is the transform's +0xC0:
//   0x5DC3B0 (VuWaterSurfaceEntity / VuWaterRampEntity, this = entity +0x5C;
//     the motion component at +0x6C): the watcher is the entity's slot 12 -
//     water_surface_refresh / water_ramp_refresh (by the entity's class).
//   0x5D4C30 (VuPointWaveEntity, this = entity +0x60; the component at +0x6C):
//     its ctor installs no transform watcher (+0xC4 = 7 only; its 0x5D4D30 is
//     the layout's draw callback, debug draw): none. The point wave reads the
//     transform's +0x80 at its next emit (water_point_entity_tick).
// False when entity_set_world refuses (no modelled transform, or a parent of
// unknown world): nothing written.
inline bool water_surface_motion( water_built_t& b, entity_t& e, const float* motion_matrix, bool draw_registered, water_surface_boxes_t& out )
{
    const water_built_t::entity_ref_t* r = water_entity_of( b, e );
    const water_entity_state_t* s = static_cast< const water_entity_state_t* >( e.m_state.get( ) );
    const int surface = r ? r->surface : -1;
    const int wave = r ? r->wave : -1;
    const bool ramp = s && s->kind == water_kind_ramp;
    return entity_set_world( e, motion_matrix, true, [ & ]( ) {
        const transform_component_t* t = entity_transform( e );
        if ( !s || !t )
            return;
        if ( ramp )
            water_ramp_refresh( b, *s, surface, wave, *t, draw_registered, out );
        else
            water_surface_refresh( b, *s, surface, *t, draw_registered, out );
    } );
}

inline bool water_point_entity_motion( entity_t& e, const float* motion_matrix )
{
    return entity_set_world( e, motion_matrix, true, std::function< void( ) >( ) );
}
// VuStaticPfxEntity's motion handler 0x5EB4C0 (thiscall on entity +0x5C; the
// motion component at +0x6C): setWorldTransform INLINED - the matrix copied
// to the transform's world, the world euler, 0x534830 and the child walk
// 0x534C00 with notify 0 (no watcher fired: entity_set_world with notify
// false) - then, with an instance (+0x94), the instance's slot 2 (setMatrix
// 0x58AB00) on the same matrix. False when entity_set_world refuses (nothing
// of the transform written; the instance is still told, as retail's).
inline bool pfx_static_motion( entity_t& e, const pfx_static_entity_t& p, const pfx_static_ops_t& o, const float* motion_matrix )
{
    const bool ran = entity_set_world( e, motion_matrix, false, std::function< void( ) >( ) );
    if ( p.instance != 0 )
        o.set_matrix( o.user, p.instance, motion_matrix );
    return ran;
}
// ---------------------------------------------------------------------------
// SMALL HANDLERS.
//
// VuPointWaveEntity's Emit plug 0x5D4B40 (thiscall entity; VuRetVal* out,
// params; retn 8): the pending byte +0x9C = 1 (the tick 0x5D4C50 emits on it),
// the answer the empty VuRetVal (type word 0).
inline std::uint32_t water_point_entity_emit( water_point_entity_t& e )
{
    e.pending = true;
    return 0u;
}

// VuPointWaveEntity's Range End watcher 0x5D4CB0 (the ctor installs it on
// that property, +0x8C = desc word 7 here): its Vu3dLayoutComponent (+0x64)
// box +0x18 .. +0x37 = (-R, -R, -0.0 (0x80000000), w) .. (R, R, 0, w) - -R by
// xorps 0x6F7CD0; both w lanes are stack words never written (UNKNOWN).
inline void water_point_entity_layout( const water_point_entity_t& e, water_layout_box_t& layout )
{
    const std::uint32_t r = e.desc[ 7 ];
    const std::uint32_t box[ 8 ] = { r ^ 0x80000000u, r ^ 0x80000000u, 0x80000000u, 0u, r, r, 0u, 0u };
    for ( int k = 0; k < 8; ++k )
    {
        layout.word[ k ] = box[ k ];
        layout.known[ k ] = k != 3 && k != 7;
    }
}

// VuWaterTextureSettingsEntity's plug 0x5D2B00 (thiscall entity; out,
// params; retn 8): when g_vuwater (0x759058) exists, the entity's +0x60 ..
// +0x7F (four movq) to the water renderer's (water +0x1C -> [0]) settings
// +4 .. +0x23 - the renderer is the platform's: `settings` is that block
// (8 words), untouched without the water. Answers the empty VuRetVal (0).
inline std::uint32_t water_texture_settings_apply( bool water, const std::uint32_t* entity_words, std::uint32_t* settings )
{
    if ( water )
        std::memcpy( settings, entity_words, 32 );
    return 0u;
}