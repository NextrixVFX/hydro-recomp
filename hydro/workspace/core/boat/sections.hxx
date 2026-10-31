#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/boat/parts.hxx>
#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

// The BoatDB section readers. postLoad's 0x420A70 calls each of them twice -
// with BoatDB Default's section, then with the boat's own - so a key the boat
// leaves out keeps Default's value, and one neither has keeps the ctor's.
// Each reader is a run of four shapes Vu inlines over its JSON container
// (0x4FBDF0 = container[key], the static null 0x7593A0 when absent):
//
//   float   tag 1 -> cvtdq2ps, 2 -> the bits, 7 -> fild / fstp; else unchanged
//   vec3    0x4FDC60 into the field (X / Y / Z, earlier lanes kept on a miss)
//   colour  0x4FDEC0 into the field (stored only when R, G and B all parse)
//   string  tag 4 -> 0x401DB0 assign(node's string, 0, npos)
//
// The listings were read into their ops by hydro-tools/codegen/sections.py,
// which accepts only those four shapes and stops at anything else; the stops
// (the driver-class branches) were read by hand. The objects are the ctor
// images of parts.hxx / construct.hxx; their std::string fields are kept by
// offset in `boat_strings_t` (MSVC's string storage is the library's, served
// by std::string - not modelled byte for byte).

using boat_strings_t = std::map< int, std::string >;

// The five driver classes' predicate slots 12..17 (vtable +0x30..+0x44), each
// 0x4052D0 (`xor al, al; ret`) or 0x404BD0 (`mov al, 1; ret`), read off the
// vtables: VuAiDriver 0x737B5C, VuHumanDriver 0x737F44, VuAiUiDriver
// 0x737CDC, VuAiNuisanceDriver 0x737C4C, VuGhostDriver 0x737DDC.
struct boat_driver_t
{
    bool slot[ 6 ];   // [k] = vtable slot 12 + k
};

inline boat_driver_t boat_driver_of( std::uint32_t vtable )
{
    switch ( vtable )
    {
    case 0x00737B5Cu: return { { false, true, true, false, false, false } };
    case 0x00737F44u: return { { true, false, true, false, false, false } };
    case 0x00737CDCu: return { { false, true, true, true, false, false } };
    case 0x00737C4Cu: return { { false, true, true, false, true, false } };
    case 0x00737DDCu: return { { false, false, true, false, false, true } };
    default: return { { false, false, false, false, false, false } };
    }
}

inline void section_float( std::uint8_t* image, int at, const value_t& value )
{
    parts_db_float( image, at, value );
}

inline void section_vec3( std::uint8_t* image, int at, const value_t& value )
{
    float lanes[ 3 ];
    std::memcpy( lanes, image + at, sizeof( lanes ) );
    vec3( value, lanes );
    std::memcpy( image + at, lanes, sizeof( lanes ) );
}

// 0x4FDEC0: R, G, B required (any other tag: false, nothing stored), A
// optional (0xFF); each channel an int, a float truncated or an int64's low
// dword (property_channel), packed R, G, B, A from the lowest byte up.
inline void section_colour( std::uint8_t* image, int at, const value_t& value )
{
    std::int32_t r, g, b;
    if ( !property_channel( member( value, "R" ), r ) || !property_channel( member( value, "G" ), g ) ||
         !property_channel( member( value, "B" ), b ) )
        return;
    std::int32_t a = 0xFF;
    property_channel( member( value, "A" ), a );
    image_put32( image, at, ( static_cast< std::uint32_t >( r ) & 0xFF ) | ( ( static_cast< std::uint32_t >( g ) & 0xFF ) << 8 ) |
                            ( ( static_cast< std::uint32_t >( b ) & 0xFF ) << 16 ) | ( ( static_cast< std::uint32_t >( a ) & 0xFF ) << 24 ) );
}

// Tag 4 -> assign. (Retail skips the assign when the node's string IS the
// field - never true for a document's node.)
inline void section_string( boat_strings_t& strings, int at, const value_t& value )
{
    if ( value.tag == 4 )
        strings[ at ] = value.text;
}

// VuBoatDB_readCameraSection 0x409AF0 (thiscall, the 0x409750 camera; retn 4).
inline void boat_read_camera( std::uint8_t* camera, const value_t& section )
{
    section_vec3( camera, 0x244, member( section, "Target Local Offset" ) );
    section_float( camera, 0x254, member( section, "Target World Z Offset" ) );
    section_float( camera, 0x258, member( section, "Ideal Distance" ) );
    section_float( camera, 0x25C, member( section, "Ideal Pitch" ) );
    section_float( camera, 0x264, member( section, "Lag Distance" ) );
    section_float( camera, 0x268, member( section, "Max Roll Angle" ) );
    section_float( camera, 0x26C, member( section, "Roll Period" ) );
    section_vec3( camera, 0x270, member( section, "Cockpit Cam Offset" ) );
    section_vec3( camera, 0x280, member( section, "Nose Cam Offset" ) );
    section_vec3( camera, 0x290, member( section, "Reverse Cam Offset" ) );
}

// VuBoatDB_readHullSection 0x417480 (thiscall, the 0x415D60 hull - its +0 the
// boat, whose +0x220 is the driver; retn 4).
inline void boat_read_hull( std::uint8_t* hull, boat_strings_t& strings, const value_t& section, const boat_driver_t& driver )
{
    section_string( strings, 0x4, member( section, "Fluids Mesh Asset" ) );
    section_vec3( hull, 0x1C, member( section, "Hydrodynamic Center" ) );
    section_vec3( hull, 0x2C, member( section, "Aerodynamic Center" ) );
    section_vec3( hull, 0x3C, member( section, "Stability" ) );
    static const struct { int at; const char* key; } floats[] = {
        { 0x4C, "Water Drag Density" }, { 0x50, "Water Buoyancy Density" }, { 0x54, "Air Density" },
        { 0x58, "Density Ratio" }, { 0x5C, "Density Ratio Speed" }, { 0x60, "Lin Vel Speed 0" },
        { 0x64, "Lin Vel Factor 0" }, { 0x68, "Lin Vel Speed 1" }, { 0x6C, "Lin Vel Factor 1" },
        { 0x70, "Lat Skin Friction Coeff" }, { 0x74, "Long Skin Friction Coeff" }, { 0x78, "Power Slide Coeff" },
        { 0x7C, "Slow Steering Boat Speed" }, { 0x80, "Slow Steering Speed" }, { 0x84, "Slow Steering Accel" },
        { 0x88, "Fast Steering Boat Speed" }, { 0x8C, "Fast Steering Speed" }, { 0x90, "Fast Steering Accel" },
        { 0x94, "Aero Lift" }, { 0x98, "Wake Magnitude" }, { 0x1D0, "Wake Range Start Ratio" },
        { 0x1D4, "Wake Range Decay Ratio" }, { 0x1D8, "Wake Physics Scale" } };
    for ( const auto& f : floats )
        section_float( hull, f.at, member( section, f.key ) );
    section_vec3( hull, 0x9C, member( section, "Wake Position" ) );
    section_vec3( hull, 0xAC, member( section, "Air Control Speed" ) );
    section_vec3( hull, 0xBC, member( section, "Air Control Accel" ) );
    section_float( hull, 0xCC, member( section, "Air Control Start" ) );
    section_float( hull, 0xD0, member( section, "Air Control Max" ) );
    section_float( hull, 0x158, member( member( section, "Audio" ), "Bounce Threshold" ) );
    // 0x417BA4: the audio set by the driver class - slots 15 and 13 "MenuAI",
    // slot 15 "MenuPlayer", slots 12 and 14 "Player", else "AI".
    const value_t& audio = member( section, "Audio" );
    const char* set = driver.slot[ 3 ] && driver.slot[ 1 ] ? "MenuAI"
                    : driver.slot[ 3 ]                     ? "MenuPlayer"
                    : driver.slot[ 0 ] && driver.slot[ 2 ] ? "Player"
                                                           : "AI";
    const value_t& events = member( audio, set );
    section_string( strings, 0xD4, member( events, "SlipEvent" ) );
    section_string( strings, 0xEC, member( events, "BounceEvent" ) );
    section_string( strings, 0x104, member( events, "BigBounceEvent" ) );
    section_string( strings, 0x11C, member( events, "WindEvent" ) );
    section_float( hull, 0x154, member( events, "SlipThreshold" ) );
    section_float( hull, 0x158, member( events, "BounceThreshold" ) );
}

// VuBoatDB_readDefaultSection 0x4205A0 = VuBoatEntity vtable slot 15
// (thiscall, the boat - its +0x220 the driver; retn 4).
inline void boat_read_default( std::uint8_t* boat, boat_strings_t& strings, const value_t& section, const boat_driver_t& driver )
{
    section_string( strings, 0xC4, member( section, "Model Asset" ) );
    section_string( strings, 0xDC, member( section, "LOD 1 Model Asset" ) );
    section_string( strings, 0xF4, member( section, "LOD 2 Model Asset" ) );
    section_float( boat, 0x10C, member( section, "LOD 1 Distance" ) );
    section_float( boat, 0x110, member( section, "LOD 2 Distance" ) );
    section_colour( boat, 0x114, member( section, "Ambient Color" ) );
    section_colour( boat, 0x118, member( section, "Diffuse Color" ) );
    section_string( strings, 0x11C, member( section, "Surface Type" ) );
    section_float( boat, 0x134, member( section, "Mass" ) );
    section_float( boat, 0x144, member( section, "InertiaFactor" ) );
    section_float( boat, 0x138, member( section, "Min Jump Speed" ) );
    section_float( boat, 0x13C, member( section, "Vertical Jump Speed" ) );
    section_float( boat, 0x140, member( section, "Destruction Speed" ) );
    section_string( strings, 0x148, member( section, "Collision Mesh Asset" ) );
    section_string( strings, 0x160, member( section, "Class" ) );
    section_string( strings, 0x178, member( section, "StringID" ) );
    section_vec3( boat, 0x190, member( member( section, "SlalomBounds" ), "Pos" ) );
    section_float( boat, 0x1A0, member( member( section, "SlalomBounds" ), "Radius" ) );
    // 0x420950: the audio set - slots 12 and 14 "Player", else "AI".
    const value_t& events = member( member( section, "Audio" ), driver.slot[ 0 ] && driver.slot[ 2 ] ? "Player" : "AI" );
    section_string( strings, 0x1A4, member( events, "BoostMisfire" ) );
    section_string( strings, 0x1BC, member( events, "Jump" ) );
    section_string( strings, 0x1D4, member( events, "JumpMisfire" ) );
    section_string( strings, 0x1EC, member( events, "Destruction" ) );
    section_string( strings, 0x204, member( events, "Drafting" ) );
}
