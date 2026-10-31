#pragma once

#include <workspace/core/entity/property.hxx>

#include <cstdint>

// The scene light entities' property sets, read from their ctors.
//
// VuAmbientLightEntity (create 0x5EF510, ctor 0x5EF580, vtable 0x756594): one
// VuBasicProperty<VuColor,6> (vtable 0x739DA8) "Color" bound to +0x60, which the
// ctor sets to 0xFF333333 before registering it; then "Use" (vtable 0x753010),
// whose load (slot 2) is 0x4021A0 - a nop, so it reads nothing; then its
// VuScriptComponent.
//
// VuDirectionalLightEntity (create 0x5EEA10, ctor 0x5EEA90, vtable 0x7564DC):
// three colours of the same class - "Front Color" +0x6C (0xFFCCCCCC), "Back
// Color" +0x70 (0xFF404040), "Specular Color" +0x74 (0xFFFFFFFF) - then "Use"
// (the nop load again, its handler 0x5EEEE0 attached through 0x43CF30). Its
// components (VuScriptComponent, 0x50B8D0, 0x552CD0) and the light object at
// +0x5C (vtable 0x7564CC) are not part of the load and are hooks here.
struct ambient_light_t
{
    std::uint32_t color;
};

struct directional_light_t
{
    std::uint32_t front_color;
    std::uint32_t back_color;
    std::uint32_t specular_color;
};

inline void ambient_light_construct( ambient_light_t& l )
{
    l.color = 0xFF333333u;
}

inline void directional_light_construct( directional_light_t& l )
{
    l.front_color = 0xFFCCCCCCu;
    l.back_color = 0xFF404040u;
    l.specular_color = 0xFFFFFFFFu;
}

// The property loads in registration order, each reset first to its node's
// default (`d`: the ctor's values until a template mark copies the loaded ones).
inline void ambient_light_load( ambient_light_t& l, const value_t& properties, const ambient_light_t& d )
{
    property_color( properties, "Color", l.color, d.color );
}

inline void directional_light_load( directional_light_t& l, const value_t& properties, const directional_light_t& d )
{
    property_color( properties, "Front Color", l.front_color, d.front_color );
    property_color( properties, "Back Color", l.back_color, d.back_color );
    property_color( properties, "Specular Color", l.specular_color, d.specular_color );
}
