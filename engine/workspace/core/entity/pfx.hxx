#pragma once

#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstring>
#include <string>

// VuStaticPfxEntity's property set (create 0x5EA940, ctor 0x5EAA40, vtable
// 0x755E80), in node order: "Initially Active" VuBasicProperty<bool,2>
// (0x737450) on +0x88 = 1; "Effect Name" VuStringProperty (0x7374B8, built by
// 0x406B70 - load / value read / filter 0x406C20 / 0x406CD0 / 0x406AB0, the
// verified string contract) on the std::string +0x70 = ""; "Pfx Scale" float
// (0x737484) on +0x8C = 1.0; "Pfx Color" VuColor (0x739DA8) on +0x90 =
// 0xFFFFFFFF. Effect Name, Pfx Scale and Pfx Color carry the watcher 0x5EB390
// (attached through 0x43CF30; the load's notify byte is 0, so a load never
// fires it). Then the plugs (Start / Stop / Kill...), its components and the
// transform watcher 0x5EB4A0 - hooks here.
struct static_pfx_t
{
    std::uint8_t initially_active;
    std::string effect_name;
    float pfx_scale;
    std::uint32_t pfx_color;
};

inline void static_pfx_construct( static_pfx_t& p )
{
    p.initially_active = 1;
    p.effect_name.clear( );
    const std::uint32_t one = 0x3F800000u;
    std::memcpy( &p.pfx_scale, &one, sizeof( one ) );
    p.pfx_color = 0xFFFFFFFFu;
}

inline void static_pfx_load( static_pfx_t& p, const value_t& properties, const static_pfx_t& d )
{
    property_bool( properties, "Initially Active", p.initially_active, d.initially_active );
    property_string( properties, "Effect Name", p.effect_name, d.effect_name );
    property_float( properties, "Pfx Scale", p.pfx_scale, d.pfx_scale, false );
    property_color( properties, "Pfx Color", p.pfx_color, d.pfx_color );
}

// What the entity hands its VuPfxSystem, as its onGameInitialize (slot 6,
// 0x5EB060) and its property watcher 0x5EB390 both build it: the colour as four
// floats, each byte (R, G, B, A from the low one up) times 0x3B808081 (the
// float nearest 1/255) - a multiply, cvtdq2ps then mulss, not a division.
inline void static_pfx_color( const static_pfx_t& p, float* out )
{
    float k;
    const std::uint32_t bits = 0x3B808081u;
    std::memcpy( &k, &bits, sizeof( k ) );
    for ( int lane = 0; lane < 4; ++lane )
        out[ lane ] = static_cast< float >( static_cast< std::int32_t >( ( p.pfx_color >> ( 8 * lane ) ) & 0xFF ) ) * k;
}
