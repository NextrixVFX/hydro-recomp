#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/boat/math.hxx>
#include <workspace/core/boat/hull.hxx>

#include <cstdint>
#include <cstring>

// VuTransformComponent (vtable 0x74D324), the part every placed entity's
// position comes from. The members mirror retail's from +0x10 on:
//   +0x10 local matrix - its translation row (+0x40) IS the "Position"
//         property's storage, there is no separate position vector;
//   +0x50 world matrix;
//   +0x90 "Rotation", in RADIANS (the property converts on load);
//   +0xA0 the world euler;
//   +0xB0 "Scale" - NOT in either matrix: each consumer applies it.
// The fourth lane of each vector is whatever the property's parse left on the
// stack in retail; nothing reads it, so it is not modelled.
struct transform_component_t
{
    float local[ 16 ];
    float world[ 16 ];
    float rotation[ 4 ];
    float world_rotation[ 4 ];
    float scale[ 4 ];
    // The world euler under a parent is CRT atan2 / asinf of the world
    // matrix (0x534AA0) - not recreated, so with a parent it is unknown.
    bool world_rotation_known = true;
};

// The ctor 0x534180: both matrices the identity 0x7593B0, rotation and world
// euler 0, scale 1. Its 0x534680 registers Position, Rotation and Scale, in
// that order, each with its target's value at that moment as the default.
inline void transform_construct( transform_component_t& c )
{
    vu_identity( c.local );
    vu_identity( c.world );
    for ( int lane = 0; lane < 3; ++lane )
    {
        c.rotation[ lane ] = 0.0f;
        c.world_rotation[ lane ] = 0.0f;
        c.scale[ lane ] = 1.0f;
    }
    c.rotation[ 3 ] = c.world_rotation[ 3 ] = c.scale[ 3 ] = 0.0f;
    c.world_rotation_known = true;
}

// One VuBasicProperty<VuVector3> load, slot 2 0x409280: the target is reset
// to the property's default, then slot 4 0x409330 parses the member through
// 0x4FDC60 (false -> the default stays), converts through slot 9 (Rotation's
// VuRotation3dProperty multiplies each lane by 0x3C8EFA35; the plain vector
// property copies) and stores only if a lane differs - ucomiss, so -0 equals
// +0 and keeps the default's sign, and a NaN always differs.
inline void transform_property( const value_t& properties, const char* name, float* target, const float* fallback,
    bool radians )
{
    for ( int lane = 0; lane < 3; ++lane )
        target[ lane ] = fallback[ lane ];
    float parsed[ 3 ];
    if ( !vec3( member( properties, name ), parsed ) )
        return;
    if ( radians )
    {
        float to_radians;
        const std::uint32_t bits = 0x3C8EFA35u;
        std::memcpy( &to_radians, &bits, sizeof( to_radians ) );
        for ( float& lane : parsed )
            lane *= to_radians;
    }
    if ( target[ 0 ] != parsed[ 0 ] || target[ 1 ] != parsed[ 1 ] || target[ 2 ] != parsed[ 2 ] )
    {
        for ( int lane = 0; lane < 3; ++lane )
            target[ lane ] = parsed[ lane ];
    }
}

// The component's property loads, in registration order. The defaults are the
// ctor's values - what each target held when 0x534680 registered it.
inline void transform_load( transform_component_t& c, const value_t& properties )
{
    const float origin[ 3 ] = { 0.0f, 0.0f, 0.0f };
    const float unit[ 3 ] = { 1.0f, 1.0f, 1.0f };
    transform_property( properties, "Position", c.local + 12, origin, false );
    transform_property( properties, "Rotation", c.rotation, origin, true );
    transform_property( properties, "Scale", c.scale, unit, false );
}

// 0x534AA0: with no parent the world IS the local and the world euler the
// rotation; with one, world = local * the parent component's world (0x4078C0)
// and the world euler is not recreated.
inline void transform_world( transform_component_t& c, const float* parent_world )
{
    if ( parent_world == nullptr )
    {
        std::memcpy( c.world, c.local, sizeof( c.world ) );
        std::memcpy( c.world_rotation, c.rotation, sizeof( c.world_rotation ) );
        c.world_rotation_known = true;
        return;
    }
    float product[ 16 ];
    hull_multiply( c.local, product, parent_world );
    std::memcpy( c.world, product, sizeof( c.world ) );
    c.world_rotation_known = false;
}

// The recalc 0x534340 (slots 3 / 4; 0x5343F0 is the same body): keep the
// translation row's x, y, z, rebuild local = identity * fromEuler(rotation)
// (0x410A40, 0x435640, 0x4078C0), put x, y, z back, then 0x534AA0. Its last
// call, 0x534C00(1), walks the owner's children - which this model has none
// of yet.
inline void transform_recalc( transform_component_t& c, const float* parent_world )
{
    const float x = c.local[ 12 ], y = c.local[ 13 ], z = c.local[ 14 ];
    vu_identity( c.local );
    float euler[ 16 ];
    vu_matrix_from_euler( euler, c.rotation );
    float product[ 16 ];
    hull_multiply( c.local, product, euler );
    std::memcpy( c.local, product, sizeof( c.local ) );
    c.local[ 12 ] = x;
    c.local[ 13 ] = y;
    c.local[ 14 ] = z;
    transform_world( c, parent_world );
}

// The model-draw components' draw matrix (Vu3dDrawStaticModelComponent +0x13C,
// which its draw 0x5B0730 hands to the model instance's draw 0x520130;
// Vu3dDrawAnimatedModelComponent +0x104, drawn by 0x61DB60 -> 0x532AD0). The
// OWNER computes it, never the component: VuPropEntity's postLoad (vt+8 / +0xC
// 0x433C50 -> 0x546AE0), VuDynamicPropEntity's (0x5459F0 -> 0x545FF0, also
// 0x546130), VuAnimatedPropEntity's (0x5ED420 into 0x61DA50) - and the same
// method again as the transform's watcher (+0xC0, installed by the ctor's
// 0x546E80 / 0x546260; 0x534C00 fires it on each descendant a parent's recalc
// 0x534340 moves - that recalc passes notify 1 and the walk hands it down), so
// the matrix always follows the final world. Each is
// VuMatrix_multiply(this = VuMatrix_fromScale(scale +0xB0), out, world +0x50):
// the Scale the transform keeps out of both matrices is applied HERE, first.
// VuMatrix_fromScale 0x407DB0 copies the three lanes' bits onto a zero matrix
// with m[15] = 1.0.
inline void transform_draw_matrix( const transform_component_t& c, float* out )
{
    float scale[ 16 ] = {};
    scale[ 0 ] = c.scale[ 0 ];
    scale[ 5 ] = c.scale[ 1 ];
    scale[ 10 ] = c.scale[ 2 ];
    const std::uint32_t one = 0x3F800000u;
    std::memcpy( &scale[ 15 ], &one, sizeof( one ) );
    hull_multiply( scale, out, c.world );
}
