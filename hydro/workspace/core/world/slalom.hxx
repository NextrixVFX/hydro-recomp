#pragma once

// VuSlalomGateEntity (vtable 0x73B3E8, +0x5C 0x73B3D8; create 0x43F250 = new 0xEC, 0x43F2D0, ctor 0x43F390) and the
// course system of VuSlalomGame (vtable 0x73FA6C, ctor 0x48BEC0) that walks its gates - transliterated from the
// disassembly (world batch 29).
//
// The gate's ctor: "Diameter" +0x6C (VuFloatProperty 0x737484, 20.0 = 0x41A00000), "Model Asset" +0x70 /
// "Reflection Model Asset" +0x88 (VuAssetNameProperty 0x74D690 over 0x406B70: "Props/Racing/RingC" /
// "Props/Racing/RingC_Reflection"; their change callback 0x43FE60 - the model instances +0xA8 / +0xC8, the
// renderer's), "Allow Recovery" +0xA0 (VuBoolProperty 0x737450, 1); a Vu3dLayoutComponent +0x60 (its aabb -0.5 ..
// 0.5 in x / z), a VuScriptComponent +0x64 whose VuScriptRef "Next" (type 0x736430 = VuSlalomGateEntity) is +0xA4,
// a VuMotionComponent +0x68, the transform's watcher 0x43FE60; +0xE8 the previous gate (0 - VuSlalomGame's slot 5
// links it). Draw / motion (0x43FE60, 0x43FF90) are the renderer's - not here.

#include <workspace/core/entity/node.hxx>
#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

struct slalom_gate_props_t
{
    float diameter = 20.0f;                                  // +0x6C
    std::string model = "Props/Racing/RingC";                // +0x70
    std::string reflection = "Props/Racing/RingC_Reflection";   // +0x88
    std::uint8_t allow_recovery = 1;                         // +0xA0
};

struct slalom_gate_state_t
{
    slalom_gate_props_t value;
    slalom_gate_props_t defaults;
};

inline std::shared_ptr< void > slalom_gate_state_for( const std::string& cls )
{
    return cls == "VuSlalomGateEntity" ? std::make_shared< slalom_gate_state_t >( ) : nullptr;
}

inline slalom_gate_state_t* slalom_gate_state_of( const entity_t& e )
{
    return e.m_type == "VuSlalomGateEntity" && e.m_state ? static_cast< slalom_gate_state_t* >( e.m_state.get( ) ) : nullptr;
}

// The four nodes' vt+8 loads in the ctor's order (case slalom_gate_properties).
inline void slalom_gate_load_properties( entity_t& e, const value_t& p )
{
    slalom_gate_state_t* s = slalom_gate_state_of( e );
    if ( !s )
        return;
    property_float( p, "Diameter", s->value.diameter, s->defaults.diameter, false );
    property_string( p, "Model Asset", s->value.model, s->defaults.model );
    property_string( p, "Reflection Model Asset", s->value.reflection, s->defaults.reflection );
    property_bool( p, "Allow Recovery", s->value.allow_recovery, s->defaults.allow_recovery );
}

inline void slalom_gate_mark_properties( entity_t& e )
{
    if ( slalom_gate_state_t* s = slalom_gate_state_of( e ) )
        s->defaults = s->value;
}
