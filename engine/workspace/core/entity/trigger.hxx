#pragma once

#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <string>

// VuTriggerEntity's property set (ctor 0x61C6A0, vtable 0x758084), shared by
// VuTriggerPlaneEntity (create 0x5D1410, ctor 0x5D1490, vtable 0x752D34) and
// VuTriggerBoxEntity (create 0x5D1A30: the base ctor, then vtable 0x752D9C -
// no properties of its own):
//   "Initially Active"  VuBasicProperty<bool,2> (0x737450) on +0x64, set to 1;
//   "Trigger Type"      VuConstStringEnumProperty (0x7570F8) on the std::string
//                       +0x68. Its load / value read / filter are the string
//                       property's (0x406C20 / 0x406CD0 / 0x406AB0: a string
//                       node only, copied, stored if it differs) - the load
//                       does NOT check the value against the enum. The ctor
//                       assigns the target from the enum's first value: the
//                       vector at 0x75913C that start-up's 0x4D9260 (called
//                       from the game init 0x4024E0) fills with "Human Boat",
//                       "AI Boat", "Boat", "Camera" in that order.
// The ctor also sets +0x80 = 1 and +0x84 = 0x100 (runtime state, not loaded),
// its components (0x50B8D0, VuScriptComponent) and the Activate / Deactivate
// input plugs (0x61CCB0 / 0x61CD20), OnEnter / OnExit outputs - hooks here.
struct trigger_properties_t
{
    std::uint8_t initially_active;
    std::string trigger_type;
};

inline void trigger_construct( trigger_properties_t& t )
{
    t.initially_active = 1;
    t.trigger_type = "Human Boat";
}

inline void trigger_load( trigger_properties_t& t, const value_t& properties, const trigger_properties_t& d )
{
    property_bool( properties, "Initially Active", t.initially_active, d.initially_active );
    property_string( properties, "Trigger Type", t.trigger_type, d.trigger_type );
}
