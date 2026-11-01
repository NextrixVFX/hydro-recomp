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

#include <workspace/core/world/surface.hxx>

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
