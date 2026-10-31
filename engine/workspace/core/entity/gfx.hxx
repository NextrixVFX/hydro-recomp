#pragma once

#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstring>

// VuGfxSettingsEntity's property set (base ctor 0x5A6520, vtable 0x750AF4), as
// VuGlobalGfxSettingsEntity (create 0x5D29B0, ctor 0x5D2A30, vtable 0x752FD4)
// carries it: the base registers 26 nodes, every one a verified class - bool
// VuBasicProperty (0x737450), float (0x737484), VuColor (0x739DA8) - bound to
// +0x60 .. +0xC4 in this order; the global ctor then adds "Use" (vtable
// 0x753010, load slot 2 = 0x4021A0, a nop). The base stores Always Active 1,
// the three times 1.0 and Fog Start / End 500 / 1000 itself and builds the
// block from +0x70 with 0x41B400; the defaults below are the targets as the
// ctor leaves them, captured by running it under emulation (engine
// entity/schema.hxx g_schema_VuGlobalGfxSettingsEntity) and re-checked by the
// oracle case load_gfx_settings on a bare entity.
struct gfx_settings_t
{
    std::uint8_t always_active;          // +0x60
    float ramp_up_time;                  // +0x64
    float duration;                      // +0x68
    float ramp_down_time;                // +0x6C
    float camera_far_plane;              // +0x70
    std::uint32_t clear_color;           // +0x74
    float fog_start;                     // +0x78
    float fog_end;                       // +0x7C
    std::uint32_t fog_color;             // +0x80
    std::uint32_t contrast;              // +0x84
    std::uint32_t tint;                  // +0x88
    float gamma_min;                     // +0x8C
    float gamma_max;                     // +0x90
    float gamma_curve;                   // +0x94
    std::uint32_t water_ambient_color;   // +0x98
    std::uint32_t water_diffuse_color;   // +0x9C
    std::uint32_t water_foam_ambient_color; // +0xA0
    std::uint32_t water_foam_diffuse_color; // +0xA4
    float water_foam_texture_size;       // +0xA8
    std::uint32_t water_decal_ambient_color; // +0xAC
    std::uint32_t water_decal_diffuse_color; // +0xB0
    float water_depth_foam_value;        // +0xB4
    float water_depth_foam_intensity;    // +0xB8
    std::uint32_t pfx_ambient_color;     // +0xBC
    std::uint32_t pfx_diffuse_color;     // +0xC0
    std::uint32_t foliage_tint;          // +0xC4
};

inline float gfx_bits( std::uint32_t b )
{
    float f;
    std::memcpy( &f, &b, sizeof( f ) );
    return f;
}

inline void gfx_settings_construct( gfx_settings_t& g )
{
    g.always_active = 1;
    g.ramp_up_time = gfx_bits( 0x3F800000u );
    g.duration = gfx_bits( 0x3F800000u );
    g.ramp_down_time = gfx_bits( 0x3F800000u );
    g.camera_far_plane = gfx_bits( 0x43FA0000u );
    g.clear_color = 0xFF000000u;
    g.fog_start = gfx_bits( 0x43FA0000u );
    g.fog_end = gfx_bits( 0x447A0000u );
    g.fog_color = 0xFF000000u;
    g.contrast = 0xFF000000u;
    g.tint = 0xFFFFFFFFu;
    g.gamma_min = gfx_bits( 0x00000000u );
    g.gamma_max = gfx_bits( 0x3F800000u );
    g.gamma_curve = gfx_bits( 0x3F800000u );
    g.water_ambient_color = 0xC0423C2Du;
    g.water_diffuse_color = 0xC05A503Cu;
    g.water_foam_ambient_color = 0xFF808080u;
    g.water_foam_diffuse_color = 0xFFFFFFFFu;
    g.water_foam_texture_size = gfx_bits( 0x41600000u );
    g.water_decal_ambient_color = 0xFF808080u;
    g.water_decal_diffuse_color = 0xFFFFFFFFu;
    g.water_depth_foam_value = gfx_bits( 0x3F000000u );
    g.water_depth_foam_intensity = gfx_bits( 0x3F000000u );
    g.pfx_ambient_color = 0xFFC0C0C0u;
    g.pfx_diffuse_color = 0xFFFFFFFFu;
    g.foliage_tint = 0xFFFFFFFFu;
}

// The 26 loads in node order, each reset first to its node's default `d`.
inline void gfx_settings_load( gfx_settings_t& g, const value_t& p, const gfx_settings_t& d )
{
    property_bool( p, "Always Active", g.always_active, d.always_active );
    property_float( p, "Ramp Up Time", g.ramp_up_time, d.ramp_up_time, false );
    property_float( p, "Duration", g.duration, d.duration, false );
    property_float( p, "Ramp Down Time", g.ramp_down_time, d.ramp_down_time, false );
    property_float( p, "Camera Far Plane", g.camera_far_plane, d.camera_far_plane, false );
    property_color( p, "Clear Color", g.clear_color, d.clear_color );
    property_float( p, "Fog Start", g.fog_start, d.fog_start, false );
    property_float( p, "Fog End", g.fog_end, d.fog_end, false );
    property_color( p, "Fog Color", g.fog_color, d.fog_color );
    property_color( p, "Contrast", g.contrast, d.contrast );
    property_color( p, "Tint", g.tint, d.tint );
    property_float( p, "Gamma Min", g.gamma_min, d.gamma_min, false );
    property_float( p, "Gamma Max", g.gamma_max, d.gamma_max, false );
    property_float( p, "Gamma Curve", g.gamma_curve, d.gamma_curve, false );
    property_color( p, "Water Ambient Color", g.water_ambient_color, d.water_ambient_color );
    property_color( p, "Water Diffuse Color", g.water_diffuse_color, d.water_diffuse_color );
    property_color( p, "Water Foam Ambient Color", g.water_foam_ambient_color, d.water_foam_ambient_color );
    property_color( p, "Water Foam Diffuse Color", g.water_foam_diffuse_color, d.water_foam_diffuse_color );
    property_float( p, "Water Foam Texture Size", g.water_foam_texture_size, d.water_foam_texture_size, false );
    property_color( p, "Water Decal Ambient Color", g.water_decal_ambient_color, d.water_decal_ambient_color );
    property_color( p, "Water Decal Diffuse Color", g.water_decal_diffuse_color, d.water_decal_diffuse_color );
    property_float( p, "Water Depth Foam Value", g.water_depth_foam_value, d.water_depth_foam_value, false );
    property_float( p, "Water Depth Foam Intensity", g.water_depth_foam_intensity, d.water_depth_foam_intensity, false );
    property_color( p, "Pfx Ambient Color", g.pfx_ambient_color, d.pfx_ambient_color );
    property_color( p, "Pfx Diffuse Color", g.pfx_diffuse_color, d.pfx_diffuse_color );
    property_color( p, "Foliage Tint", g.foliage_tint, d.foliage_tint );
}

// The targets as words, +0x60 .. +0xC4 in order (the bool as its byte).
inline void gfx_settings_words( const gfx_settings_t& g, std::uint32_t* out )
{
    const auto w = []( float f )
    {
        std::uint32_t b;
        std::memcpy( &b, &f, sizeof( b ) );
        return b;
    };
    const std::uint32_t words[ 26 ] = { g.always_active, w( g.ramp_up_time ), w( g.duration ), w( g.ramp_down_time ), w( g.camera_far_plane ),
        g.clear_color, w( g.fog_start ), w( g.fog_end ), g.fog_color, g.contrast, g.tint, w( g.gamma_min ), w( g.gamma_max ), w( g.gamma_curve ),
        g.water_ambient_color, g.water_diffuse_color, g.water_foam_ambient_color, g.water_foam_diffuse_color, w( g.water_foam_texture_size ),
        g.water_decal_ambient_color, g.water_decal_diffuse_color, w( g.water_depth_foam_value ), w( g.water_depth_foam_intensity ),
        g.pfx_ambient_color, g.pfx_diffuse_color, g.foliage_tint };
    std::memcpy( out, words, sizeof( words ) );
}
