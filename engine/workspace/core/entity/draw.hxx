#pragma once

#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstring>
#include <string>

// The model-draw components' property targets: Vu3dDrawStaticModelComponent
// (vtable 0x7519AC) and Vu3dDrawAnimatedModelComponent (vtable 0x758240). The
// property set - names, classes, registration order - is each component's
// node list (+4, next +0x10) as its ctor leaves it, captured by running the
// prop types' create fns under emulation (hydro-tools census/createprobe.py);
// the defaults are the targets straight after create (oracle case
// load_prop_models). Every class is a verified load: bool VuBasicProperty
// (0x737450), float (0x737484), VuColor (0x739DA8), VuAssetNameProperty
// (0x74D690).
//
// A Model Asset change notifies the component's watcher, which loads the
// model through the asset factory (0x5340B0) and rebuilds the draw state
// against the graphics device (0x5B09E0, g_vumetrod3d11gfx) - the rendering
// side, a hook: the renderer loads the named model itself.
struct model_draw_t
{
    bool animated;
    std::uint8_t reflect;
    std::uint8_t shadow;
    std::string model_asset;
    std::string lod1_model_asset;      // static only
    std::string lod2_model_asset;      // static only
    std::string reflection_model_asset; // static only
    std::uint32_t color;               // VuColor bytes R, G, B, A
    float draw_distance;
    float lod0_draw_distance;          // static only
    float lod1_draw_distance;          // static only
    float rejection_scale_modifier;    // static only
    std::uint8_t dynamic_lighting;
};

inline float draw_bits( std::uint32_t b )
{
    float f;
    std::memcpy( &f, &b, sizeof( f ) );
    return f;
}

// The values a fresh create leaves (load_prop_models' depth -1 rows).
inline void model_draw_construct( model_draw_t& c, bool animated )
{
    c.animated = animated;
    c.reflect = 0;
    c.shadow = 1;
    c.model_asset.clear( );
    c.lod1_model_asset.clear( );
    c.lod2_model_asset.clear( );
    c.reflection_model_asset.clear( );
    c.color = 0xFFFFFFFFu;
    c.draw_distance = draw_bits( 0x7F7FFFFFu );
    c.lod0_draw_distance = draw_bits( 0x7F7FFFFFu );
    c.lod1_draw_distance = draw_bits( 0x7F7FFFFFu );
    c.rejection_scale_modifier = draw_bits( 0x3F800000u );
    c.dynamic_lighting = 0;
}

// The component's property loads in node order, each reset first to its
// node's default `d` (the ctor's, until a template mark makes the loaded
// values the defaults).
inline void model_draw_load( model_draw_t& c, const value_t& properties, const model_draw_t& d )
{
    property_bool( properties, "Reflect", c.reflect, d.reflect );
    property_bool( properties, "Shadow", c.shadow, d.shadow );
    property_string( properties, "Model Asset", c.model_asset, d.model_asset );
    if ( !c.animated )
    {
        property_string( properties, "LOD 1 Model Asset", c.lod1_model_asset, d.lod1_model_asset );
        property_string( properties, "LOD 2 Model Asset", c.lod2_model_asset, d.lod2_model_asset );
        property_string( properties, "Reflection Model Asset", c.reflection_model_asset, d.reflection_model_asset );
    }
    property_color( properties, "Color", c.color, d.color );
    property_float( properties, "Draw Distance", c.draw_distance, d.draw_distance, false );
    if ( !c.animated )
    {
        property_float( properties, "LOD 0 Draw Distance", c.lod0_draw_distance, d.lod0_draw_distance, false );
        property_float( properties, "LOD 1 Draw Distance", c.lod1_draw_distance, d.lod1_draw_distance, false );
        property_float( properties, "Rejection Scale Modifier", c.rejection_scale_modifier, d.rejection_scale_modifier, false );
    }
    property_bool( properties, "Dynamic Lighting", c.dynamic_lighting, d.dynamic_lighting );
}
