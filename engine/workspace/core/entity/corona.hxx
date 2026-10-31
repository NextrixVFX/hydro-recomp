#pragma once

#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstring>

// VuCoronaEntity (create 0x5E8C80, ctor 0x5E8D80): the members its properties
// are bound to, at retail's offsets (listed for matching the disassembly, not
// used to index anything). The property set, its classes and the defaults are
// schema.hxx's g_schema_VuCoronaEntity - captured by running the ctor under
// emulation; the defaults are VuCoronaParams_ctor 0x61CE60's.
//
// "Texture Name" (+0x74, VuAssetNameProperty) is NOT here: its load is unread,
// and its change reaches VuCoronaParams' texture load 0x61CF00, which builds a
// material through the graphics device - the rendering platform layer.
struct corona_properties_t
{
    std::uint8_t initially_visible;      // +0x70
    float rotation_speed;                // +0x8C  radians
    float rotation_axis[ 3 ];            // +0x90
    float draw_distance;                 // +0xA0
    float fade_distance;                 // +0xA4
    float query_radius;                  // +0xA8
    float cone;                          // +0xAC  radians
    float penumbra;                      // +0xB0  radians
    std::uint8_t back_light;             // +0xB4  "Enable Back Light"
    std::uint32_t texture_color;         // +0xB5
    std::uint8_t screen_space;           // +0xB9  "Texture Size Screen Space"
    float texture_size;                  // +0xBC
    float rotation_offset;               // +0xC0  radians
    float rotation_amount;               // +0xC4  radians
};

inline float corona_bits( std::uint32_t b )
{
    float f;
    std::memcpy( &f, &b, sizeof( f ) );
    return f;
}

// The ctor's defaults (schema.hxx's captured raw bits).
inline void corona_construct( corona_properties_t& c )
{
    c.initially_visible = 1;
    c.rotation_speed = 0.0f;
    c.rotation_axis[ 0 ] = 0.0f;
    c.rotation_axis[ 1 ] = 0.0f;
    c.rotation_axis[ 2 ] = corona_bits( 0x3F800000u );
    c.draw_distance = corona_bits( 0x7F7FFFFFu );
    c.fade_distance = 0.0f;
    c.query_radius = corona_bits( 0x3F800000u );
    c.cone = corona_bits( 0x40C90FDBu );
    c.penumbra = 0.0f;
    c.back_light = 0;
    c.texture_color = 0xFFFFFFFFu;
    c.screen_space = 0;
    c.texture_size = corona_bits( 0x41200000u );
    c.rotation_offset = 0.0f;
    c.rotation_amount = corona_bits( 0x40490FDBu );
}

// The property loads in the ctor's registration order (schema.hxx's), each
// defaulting to its target's value at registration.
inline void corona_load( corona_properties_t& c, const value_t& properties )
{
    corona_properties_t d;
    corona_construct( d );
    property_bool( properties, "Initially Visible", c.initially_visible, d.initially_visible );
    property_float( properties, "Rotation Speed", c.rotation_speed, d.rotation_speed, true );
    property_vector3( properties, "Rotation Axis", c.rotation_axis, d.rotation_axis );
    property_float( properties, "Draw Distance", c.draw_distance, d.draw_distance, false );
    property_float( properties, "Fade Distance", c.fade_distance, d.fade_distance, false );
    property_float( properties, "Query Radius", c.query_radius, d.query_radius, false );
    property_float( properties, "Cone Angle", c.cone, d.cone, true );
    property_float( properties, "Penumbra Angle", c.penumbra, d.penumbra, true );
    property_bool( properties, "Enable Back Light", c.back_light, d.back_light );
    property_color( properties, "Texture Color", c.texture_color, d.texture_color );
    property_bool( properties, "Texture Size Screen Space", c.screen_space, d.screen_space );
    property_float( properties, "Texture Size", c.texture_size, d.texture_size, false );
    property_float( properties, "Rotation Offset", c.rotation_offset, d.rotation_offset, true );
    property_float( properties, "Rotation Amount", c.rotation_amount, d.rotation_amount, true );
}
