#pragma once

#include <cstddef>
#include <cstdint>

// THE VuPfx NODE CLASSES' OWN SLOTS - the constant getters and the placement
// creates every node class carries (read from each vtable, census/vtref.py
// "VuPfx"; memory/particles.md maps the per-frame slot 1 of the instances).
// A node class's vtable:
//   slot 1  getType: a static VuRTTI descriptor (+0 the name, +4 the parent;
//           statically initialised .data, no CRT initializer);
//   slot 6  the instance's size (VuPfxProcess / VuPfxPattern: _purecall);
//   slot 7  the instance's placement create (thiscall node; void* memory;
//           retn 4): nothing for null memory, else the instance's words;
//   slot 8  (patterns) the particle's size; slot 9 (patterns) the particle's
//           placement init.
// An instance's slot 0 is its reset (VuPfxSystemInstance's start 0x58AC90 runs
// it on every pattern, a pattern on every child): pfx_*_reset below.
// Words are the instance's / particle's dwords by offset / 4; a create writes
// only the words named.

// 0x58B240 VuPfxPatternInstance's ctor (the patterns' slot 7 calls it first):
// vtable 0x750068, +0x14 .. +0x28 0 (the child list, the particle list, the
// count), the AABB min +0x2C .. +0x34 and max +0x3C .. +0x44 0 - its pad words
// +0x38 / +0x48 are stack words the ctor never sets (UNKNOWN: not written).
inline void pfx_pattern_instance_ctor( std::uint32_t* w )
{
    w[ 0 ] = 0x750068u;
    for ( int o = 0x14; o <= 0x28; o += 4 )
        w[ o / 4 ] = 0u;
    for ( int o : { 0x2C, 0x30, 0x34, 0x3C, 0x40, 0x44 } )
        w[ o / 4 ] = 0u;
}

// 0x58E8A0 the particle's base init (the patterns' slot 9 calls it first):
// position +8 .. +0x10 and velocity +0x18 .. +0x20 0, the colour +0x28 ..
// +0x34 and scale +0x38 1.0, age +0x3C and life +0x40 0.
inline void pfx_particle_init( std::uint32_t* w )
{
    for ( int o : { 0x08, 0x0C, 0x10, 0x18, 0x1C, 0x20, 0x3C, 0x40 } )
        w[ o / 4 ] = 0u;
    for ( int o = 0x28; o <= 0x38; o += 4 )
        w[ o / 4 ] = 0x3F800000u;
}

// Slot 7 of the tick processes that keep no state of their own (the dampen /
// linear acceleration / world scale z / scale / alpha in-out / alpha ticks:
// 0x58E340, 0x58E5C0, 0x5909D0, 0x590C30, 0x590E90, 0x5911F0): the instance's
// vtable only.
inline void pfx_create_vtable_only( std::uint32_t* w, std::uint32_t vtable )
{
    w[ 0 ] = vtable;
}

// Slot 7 of the fountains (0x55DAA0 quad, 0x55E2D0 geom, 0x55E970 directional
// quad; 0x4CD490 the boat thrust's adds +0x28 = 0): alive +0x14 and the
// accumulator +0x18 0, the count / rate / alpha scales +0x1C .. +0x24 1.0, then
// the vtable (world/pfx.hxx pfx_fountain_node_t's defaults).
inline void pfx_create_fountain( std::uint32_t* w, std::uint32_t vtable, bool boat_thrust )
{
    w[ 0x14 / 4 ] = 0u;
    w[ 0x18 / 4 ] = 0u;
    w[ 0x1C / 4 ] = 0x3F800000u;
    w[ 0x20 / 4 ] = 0x3F800000u;
    w[ 0x24 / 4 ] = 0x3F800000u;
    w[ 0 ] = vtable;
    if ( boat_thrust )
        w[ 0x28 / 4 ] = 0u;   // the boat
}

// The four create kinds that remain, each by its address.
inline void pfx_create_class( std::uint32_t address, std::uint32_t* w )
{
    switch ( address )
    {
    case 0x4CD6B0:   // VuPfxBoatSpringConstraint: vtable, +0x14 the boat 0
        w[ 0 ] = 0x746AD4u;
        w[ 0x14 / 4 ] = 0u;
        break;
    case 0x58DE80:   // VuPfxSpringConstraint: vtable, the offset +0x14 .. +0x1C 0
        w[ 0 ] = 0x750128u;
        w[ 0x14 / 4 ] = w[ 0x18 / 4 ] = w[ 0x1C / 4 ] = 0u;
        break;
    case 0x4CDF30:   // VuPfxEmitBoatHull: vtable, +0x14 the boat and +0x1C the accumulator 0 (+0x18 the fluids untouched)
        w[ 0 ] = 0x746B90u;
        w[ 0x14 / 4 ] = 0u;
        w[ 0x1C / 4 ] = 0u;
        break;
    case 0x58E9A0:   // VuPfxTrailPattern: the base ctor, vtable, +0x4C .. +0x54 0
        pfx_pattern_instance_ctor( w );
        w[ 0 ] = 0x7502C0u;
        w[ 0x4C / 4 ] = w[ 0x50 / 4 ] = w[ 0x54 / 4 ] = 0u;
        break;
    case 0x58FAA0:   // VuPfxGeomPattern
        pfx_pattern_instance_ctor( w );
        w[ 0 ] = 0x750354u;
        break;
    case 0x591470:   // VuPfxQuadPattern
        pfx_pattern_instance_ctor( w );
        w[ 0 ] = 0x7505D8u;
        break;
    case 0x55DAA0: pfx_create_fountain( w, 0x746AE0u, false ); break;
    case 0x55E2D0: pfx_create_fountain( w, 0x74EF1Cu, false ); break;
    case 0x55E970: pfx_create_fountain( w, 0x74EEE8u, false ); break;
    case 0x4CD490: pfx_create_fountain( w, 0x746AF0u, true ); break;
    case 0x58E340: pfx_create_vtable_only( w, 0x7501B0u ); break;
    case 0x58E5C0: pfx_create_vtable_only( w, 0x750240u ); break;
    case 0x5909D0: pfx_create_vtable_only( w, 0x7503E8u ); break;
    case 0x590C30: pfx_create_vtable_only( w, 0x75043Cu ); break;
    case 0x590E90: pfx_create_vtable_only( w, 0x7504BCu ); break;
    case 0x5911F0: pfx_create_vtable_only( w, 0x750558u ); break;
    default: break;
    }
}

// The patterns' slot 9 (thiscall pattern; particle*; retn 4): the base init,
// then the pattern's own words.
inline void pfx_particle_class( std::uint32_t address, std::uint32_t* w )
{
    pfx_particle_init( w );
    switch ( address )
    {
    case 0x58E9D0:   // VuPfxTrailPattern
        w[ 0x54 / 4 ] = 0u;
        break;
    case 0x58FAD0:   // VuPfxGeomPattern: the angles +0x44 .. +0x4C and angular velocity +0x54 .. +0x5C 0
        for ( int o : { 0x44, 0x48, 0x4C, 0x54, 0x58, 0x5C } )
            w[ o / 4 ] = 0u;
        break;
    case 0x5914A0:   // VuPfxQuadPattern: +0x44 rotation, +0x48 spin 0, +0x4C 1.0, +0x50 .. +0x58 0
        w[ 0x44 / 4 ] = 0u;
        w[ 0x48 / 4 ] = 0u;
        w[ 0x4C / 4 ] = 0x3F800000u;
        w[ 0x50 / 4 ] = w[ 0x54 / 4 ] = w[ 0x58 / 4 ] = 0u;
        break;
    default: break;
    }
}

// The instances' slot 0 (reset):
//   0x55D020 the fountains' (quad, directional quad, geom, boat thrust): alive
//            +0x14 and the accumulator +0x18 0;
//   0x4CE750 the boat hull's: the accumulator +0x1C 0;
//   0x58FE80 the quad / geom patterns': slot 0 on every child node instance
//            (the +0x14 list, next at +4: Vu virtuals - `child( user, k )`);
//   0x58F080 the trail pattern's: the same walk, then +0x4C .. +0x54 0.
inline void pfx_fountain_reset( std::uint32_t* w )
{
    w[ 0x14 / 4 ] = 0u;
    w[ 0x18 / 4 ] = 0u;
}

inline void pfx_hull_reset( std::uint32_t* w )
{
    w[ 0x1C / 4 ] = 0u;
}

inline void pfx_pattern_reset( std::uint32_t* w, bool trail, int children, void ( *child )( void* user, int k ), void* user )
{
    for ( int k = 0; k < children; ++k )
        child( user, k );
    if ( trail )
        w[ 0x4C / 4 ] = w[ 0x50 / 4 ] = w[ 0x54 / 4 ] = 0u;
}

// One node class: its vtable, slot 1 (the address and the descriptor it
// returns, the descriptor chain's names), slots 6 / 7 / 8 / 9 (0 when the slot
// is _purecall 0x6310B4 or past the vtable's end) and the instance's slot 0.
struct pfx_class_t
{
    const char* name;
    std::uint32_t vtable;
    std::uint32_t get_type;
    std::uint32_t descriptor;
    const char* chain;
    std::uint32_t size_slot;
    std::uint32_t instance_size;
    std::uint32_t create;
    std::uint32_t particle_slot;
    std::uint32_t particle_size;
    std::uint32_t particle;
    std::uint32_t reset;   // the instance vtable's slot 0 (0: a nullsub or none)
};

inline const pfx_class_t g_pfx_classes[] = {
    { "VuPfxNode", 0x746A70u, 0x4CD380u, 0x7369CCu, "VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxProcess", 0x746B00u, 0x4CD390u, 0x7369D4u, "VuPfxProcess|VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxGroup", 0x74CD14u, 0x51C400u, 0x736A08u, "VuPfxGroup|VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxSystem", 0x74FFD4u, 0x58A610u, 0x736A10u, "VuPfxSystem|VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxPattern", 0x750088u, 0x58AFE0u, 0x736A18u, "VuPfxPattern|VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxEmitFountain", 0x74EF50u, 0x55C8A0u, 0x7369BCu, "VuPfxEmitFountain|VuPfxProcess|VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxEmitQuadFountain", 0x74EF2Cu, 0x55C8B0u, 0x7369B4u, "VuPfxEmitQuadFountain|VuPfxEmitFountain|VuPfxProcess|VuPfxNode",
        0x55DA90u, 0x28u, 0x55DAA0u, 0, 0, 0, 0x55D020u },
    { "VuPfxEmitGeomFountain", 0x74EEF8u, 0x55C8C0u, 0x7369A4u, "VuPfxEmitGeomFountain|VuPfxEmitFountain|VuPfxProcess|VuPfxNode",
        0x55DA90u, 0x28u, 0x55E2D0u, 0, 0, 0, 0x55D020u },
    { "VuPfxEmitDirectionalQuadFountain", 0x74EEC4u, 0x55C8D0u, 0x7369ACu,
        "VuPfxEmitDirectionalQuadFountain|VuPfxEmitQuadFountain|VuPfxEmitFountain|VuPfxProcess|VuPfxNode", 0x55DA90u, 0x28u, 0x55E970u, 0,
        0, 0, 0x55D020u },
    { "VuPfxEmitBoatThrustQuadFountain", 0x746AB0u, 0x4CD3E0u, 0x7367D0u,
        "VuPfxEmitBoatThrustQuadFountain|VuPfxEmitQuadFountain|VuPfxEmitFountain|VuPfxProcess|VuPfxNode", 0x4CD480u, 0x2Cu, 0x4CD490u, 0,
        0, 0, 0x55D020u },
    { "VuPfxEmitBoatHull", 0x746B6Cu, 0x4CDE90u, 0x7367D8u, "VuPfxEmitBoatHull|VuPfxProcess|VuPfxNode", 0x4CDF20u, 0x20u, 0x4CDF30u, 0, 0, 0,
        0x4CE750u },
    { "VuPfxBoatSpringConstraint", 0x746A8Cu, 0x4CD3F0u, 0x7367C8u, "VuPfxBoatSpringConstraint|VuPfxProcess|VuPfxNode", 0x4CD6A0u,
        0x18u, 0x4CD6B0u, 0, 0, 0, 0 },
    { "VuPfxSpringConstraint", 0x750104u, 0x58DDE0u, 0x736A20u, "VuPfxSpringConstraint|VuPfxProcess|VuPfxNode", 0x58DE70u, 0x24u,
        0x58DE80u, 0, 0, 0, 0 },
    { "VuPfxTickDampenVelocity", 0x75018Cu, 0x58E2A0u, 0x736A28u, "VuPfxTickDampenVelocity|VuPfxProcess|VuPfxNode", 0x58E330u, 0x14u,
        0x58E340u, 0, 0, 0, 0 },
    { "VuPfxTickLinearAcceleration", 0x75021Cu, 0x58E530u, 0x736A30u, "VuPfxTickLinearAcceleration|VuPfxProcess|VuPfxNode", 0x58E330u,
        0x14u, 0x58E5C0u, 0, 0, 0, 0 },
    { "VuPfxTickWorldScaleZ", 0x7503C4u, 0x590940u, 0x736A48u, "VuPfxTickWorldScaleZ|VuPfxProcess|VuPfxNode", 0x58E330u, 0x14u,
        0x5909D0u, 0, 0, 0, 0 },
    { "VuPfxTickScale", 0x750448u, 0x590BA0u, 0x736A50u, "VuPfxTickScale|VuPfxProcess|VuPfxNode", 0x58E330u, 0x14u, 0x590C30u, 0, 0, 0, 0 },
    { "VuPfxTickAlphaInOut", 0x7504C8u, 0x590E00u, 0x736A58u, "VuPfxTickAlphaInOut|VuPfxProcess|VuPfxNode", 0x58E330u, 0x14u, 0x590E90u,
        0, 0, 0, 0 },
    { "VuPfxTickAlpha", 0x750534u, 0x591160u, 0x736A60u, "VuPfxTickAlpha|VuPfxProcess|VuPfxNode", 0x58E330u, 0x14u, 0x5911F0u, 0, 0, 0, 0 },
    { "VuPfxTrailPattern", 0x750294u, 0x58E890u, 0x736A38u, "VuPfxTrailPattern|VuPfxPattern|VuPfxNode", 0x58E990u, 0x58u, 0x58E9A0u,
        0x58E990u, 0x58u, 0x58E9D0u, 0x58F080u },
    { "VuPfxGeomPattern", 0x750328u, 0x58F9F0u, 0x736A40u, "VuPfxGeomPattern|VuPfxPattern|VuPfxNode", 0x58FA90u, 0x4Cu, 0x58FAA0u,
        0x58FAC0u, 0x64u, 0x58FAD0u, 0x58FE80u },
    { "VuPfxQuadPattern", 0x7505ACu, 0x5913C0u, 0x736A68u, "VuPfxQuadPattern|VuPfxPattern|VuPfxNode", 0x58FA90u, 0x4Cu, 0x591470u,
        0x591490u, 0x5Cu, 0x5914A0u, 0x58FE80u },
};

inline const pfx_class_t* pfx_class_find( const char* name )
{
    for ( const pfx_class_t& c : g_pfx_classes )
    {
        const char* a = c.name;
        const char* b = name;
        while ( *a && *a == *b )
            ++a, ++b;
        if ( *a == 0 && *b == 0 )
            return &c;
    }
    return nullptr;
}
