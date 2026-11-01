#pragma once

#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstring>
#include <string>

// VuRigidBodyComponent (vtable 0x74DAE4, ctor 0x543890): its property set and
// the runtime words its owner and createRigidBody 0x544100 read, recreated
// from the ctor's disassembly (world batch 16). Registration order (the +4
// node list) and targets:
//   "Collision Type"   +0x10 VuStaticIntEnumProperty (vt 0x74C354, choices 0x74DA98)
//   "Collision Mesh Asset" +0x14 VuAssetProperty<VuCollisionMeshAsset> (vt 0x74DA60;
//                      ctor 0x545120 - its load is VuAssetNameProperty's 0x406C20)
//   "Collision Size"   +0x30 vector3 (vt 0x737A84)   default (2, 2, 2)
//   "Collision Radius" +0x2C float                   default 1
//   "Collision Height" +0x40 float                   default 2
//   "Surface Type"     +0x44 VuDBEntryProperty ("SurfaceDB", ctor 0x506A70, vt
//                      0x74C4C0 - its load is VuAssetNameProperty's 0x406C20)
//   "Collision Offset" +0x5C vector3                 default 0
//   "Linear Damping" +0x6C / "Angular Damping" +0x70 float, default 0
//   "Linear Sleeping Threshold" +0x74 (0x3F4CCCCD) / "Angular ..." +0x78 (1) float
//   "Initially Active" +0x7C bool                    default 0
// The ctor's runtime words: +0xD0 = 1 (the collision flags handed to the body),
// +0xD4 / +0xD6 group / mask 0, +0xD8 0, +0xDC .. +0xE4 the centre of mass 0,
// +0xEC mass 0, +0xF0 motion state 0, +0xF4 0; +0xA0 the VuStridingMesh (vt
// 0x7380C4) with its scaling +0xB0 = (1, 1, 1, 0).
// postLoad (vt+0xC 0x544040 -> 0x545010) sets the nodes' editor visibility
// bytes (node +8 - dev UI) and rebuilds the body ONLY when one exists (+0x100):
// on a load before gameInitialize there is none, so it changes nothing here.
struct rigid_component_t
{
    std::int32_t collision_type = 0;
    std::string mesh_asset;
    float radius = 1.0f;
    float size[ 3 ] = { 2.0f, 2.0f, 2.0f };
    float height = 2.0f;
    std::string surface;
    float offset[ 3 ] = { 0.0f, 0.0f, 0.0f };
    float linear_damping = 0.0f;
    float angular_damping = 0.0f;
    float linear_sleep = 0.8f;    // 0x3F4CCCCD
    float angular_sleep = 1.0f;
    std::uint8_t initially_active = 0;
    // runtime words
    std::uint32_t flags_d0 = 1;
    std::uint16_t group = 0, mask = 0;
    std::uint32_t flags_d8 = 0;
    float centre[ 3 ] = { 0.0f, 0.0f, 0.0f };
    float mass = 0.0f;
    std::uint32_t word_f4 = 0;
};

inline float rigid_bits( std::uint32_t u )
{
    float f;
    std::memcpy( &f, &u, 4 );
    return f;
}

inline void rigid_component_construct( rigid_component_t& c )
{
    c = rigid_component_t{ };
    c.linear_sleep = rigid_bits( 0x3F4CCCCDu );
}

// The choice table 0x74DA98 (name, value): none 0, sphere 1, box 2, cylinder x
// 3, cylinder y 4, cylinder z 5, convex hull 6, triangle mesh 7.
inline const char* const g_rigid_collision_types[ 8 ] = { "none", "sphere", "box", "cylinder x", "cylinder y", "cylinder z", "convex hull",
                                                         "triangle mesh" };

// VuStaticIntEnumProperty's load (vt+8 0x406FE0: target = the node's default;
// vt+0x10 0x503600 on data[name]; the target copied back): 0x5037C0 converts
// the datum - a STRING (tag 4) becomes {int, 0} and then the value of the
// first choice whose name equals it exactly (0x404AC0 over min(length) + the
// lengths equal); an INT becomes the matching choice's NAME (a string). 0x503600
// stores only an int / float (cvttss2si) / int64 (low word) result - so a
// string datum stores its choice's value (0 if none matches), a number or any
// other datum stores nothing. The filter vt+0x24 (0x435180) is the identity;
// store only if it differs; vt+0x2C is a nop; the watcher (notify) a hook.
inline void property_enum( const value_t& properties, const char* name, std::int32_t& target, std::int32_t fallback,
                           const char* const* choices, const std::int32_t* values, int count )
{
    target = fallback;
    const value_t& v = member( properties, name );
    if ( v.tag != 4 )
        return;
    std::int32_t r = 0;
    for ( int k = 0; k < count; ++k )
        if ( v.text == choices[ k ] )
        {
            r = values[ k ];
            break;
        }
    if ( target != r )
        target = r;
}

// The component's property loads in node order, each reset to its node's
// default `d` first.
inline void rigid_component_load( rigid_component_t& c, const value_t& properties, const rigid_component_t& d )
{
    static const std::int32_t values[ 8 ] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    property_enum( properties, "Collision Type", c.collision_type, d.collision_type, g_rigid_collision_types, values, 8 );
    property_string( properties, "Collision Mesh Asset", c.mesh_asset, d.mesh_asset );
    property_vector3( properties, "Collision Size", c.size, d.size );
    property_float( properties, "Collision Radius", c.radius, d.radius, false );
    property_float( properties, "Collision Height", c.height, d.height, false );
    property_string( properties, "Surface Type", c.surface, d.surface );
    property_vector3( properties, "Collision Offset", c.offset, d.offset );
    property_float( properties, "Linear Damping", c.linear_damping, d.linear_damping, false );
    property_float( properties, "Angular Damping", c.angular_damping, d.angular_damping, false );
    property_float( properties, "Linear Sleeping Threshold", c.linear_sleep, d.linear_sleep, false );
    property_float( properties, "Angular Sleeping Threshold", c.angular_sleep, d.angular_sleep, false );
    property_bool( properties, "Initially Active", c.initially_active, d.initially_active );
}
