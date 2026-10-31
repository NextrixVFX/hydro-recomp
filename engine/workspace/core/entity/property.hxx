#pragma once

#include <workspace/core/assets/tree.hxx>

#include <cstdint>
#include <cstring>
#include <string>

// The VuBasicProperty loads a property node runs when its entity loads its
// "Properties" (each node's vt+8). Every class follows one contract:
//   load (vt+8):        the target is reset to the node's default (+0x14), then
//                       the value read (vt+0x10) gets properties[name] and the
//                       node's notify byte (+9);
//   value read (vt+0x10): parse by tag (a tag it does not take: nothing stored),
//                       filter through vt+0x24, and store only if the result
//                       differs from the target; then vt+0x2C (0x401560, a nop
//                       for all four) and, when notify is set and a watcher is
//                       attached (+0xC), the watcher's vt+8 - a hook.
// The node's +0x15 / +0x18 copy of the target afterwards is the node's own state,
// not the entity's, and is not modelled.

// VuBasicProperty<bool,2> (vtable 0x737450): load 0x407220, value read 0x407290 -
// only a bool node (tag 3) is taken; the filter 0x406B60 copies.
inline void property_bool( const value_t& properties, const char* name, std::uint8_t& target, std::uint8_t fallback )
{
    target = fallback;
    const value_t& value = member( properties, name );
    if ( value.tag != 3 )
        return;
    const std::uint8_t parsed = static_cast< std::uint8_t >( value.integer );
    if ( target != parsed )
        target = parsed;
}

// VuBasicProperty<float,1> (vtable 0x737484) and VuAngleProperty (0x73A490): load
// 0x406FE0, value read 0x407060 - an int (cvtdq2ps), a float, or an int64 (x87
// fild + fstp: one rounding); the filter is 0x406B20 (copies) for the float and
// 0x4357B0 for the angle (x 0x3C8EFA35, degrees to radians). The compare is
// ucomiss + lahf / test 0x44: -0 equals +0 (no store) and a NaN always stores.
inline void property_float( const value_t& properties, const char* name, float& target, float fallback, bool angle )
{
    target = fallback;
    const value_t& value = member( properties, name );
    float parsed;
    if ( value.tag == 1 )
        parsed = static_cast< float >( value.integer );
    else if ( value.tag == 2 )
        parsed = value.real;
    else if ( value.tag == 7 )
        parsed = static_cast< float >( value.int64 );
    else
        return;
    if ( angle )
    {
        float to_radians;
        const std::uint32_t bits = 0x3C8EFA35u;
        std::memcpy( &to_radians, &bits, sizeof( to_radians ) );
        parsed *= to_radians;
    }
    if ( target != parsed )
        target = parsed;
}

// VuPercentageProperty and VuStaticIntEnumProperty live in entity/values.hxx
// (property_percentage, property_choice_t, property_enum) - recreated twice,
// independently, by two readings of 0x43E580 / 0x503600 / 0x5037C0 that agreed.

// VuBasicProperty<VuVector3,8>: load 0x409280, value read 0x409330 through the
// vec3 parse 0x4FDC60 (false: nothing stored, the lanes it wrote before failing
// stay in its own temporary); the filter copies; stored only if a lane differs
// (ucomiss per lane). The target's fourth lane is not touched by the parse and
// is not modelled.
inline void property_vector3( const value_t& properties, const char* name, float* target, const float* fallback )
{
    for ( int lane = 0; lane < 3; ++lane )
        target[ lane ] = fallback[ lane ];
    float parsed[ 3 ];
    if ( !vec3( member( properties, name ), parsed ) )
        return;
    if ( target[ 0 ] != parsed[ 0 ] || target[ 1 ] != parsed[ 1 ] || target[ 2 ] != parsed[ 2 ] )
        for ( int lane = 0; lane < 3; ++lane )
            target[ lane ] = parsed[ lane ];
}

// The string properties (VuAssetNameProperty, vtable 0x74D690, among them): load
// 0x406C20 resets the target (+0x44) to the default (+0x14); the value read
// 0x406CD0 takes only a string node (tag 4), copies it through the filter
// 0x406AB0, and stores only if it differs (memcmp of the shorter length, then
// the lengths); then vt+0x2C (0x401560, a nop) and, with the notify byte set
// and a watcher attached, the watcher's vt+8 - a hook (a corona's texture
// load 0x61CF00 is one).
inline void property_string( const value_t& properties, const char* name, std::string& target, const std::string& fallback )
{
    target = fallback;
    const value_t& value = member( properties, name );
    if ( value.tag != 4 )
        return;
    if ( target != value.text )
        target = value.text;
}

// One channel of the colour parse 0x4FDEC0: an int, a float truncated
// (cvttss2si), or an int64's low dword; false for any other tag.
inline bool property_channel( const value_t& value, std::int32_t& out )
{
    if ( value.tag == 1 )
        out = value.integer;
    else if ( value.tag == 2 )
        out = static_cast< std::int32_t >( value.real );
    else if ( value.tag == 7 )
        out = static_cast< std::int32_t >( static_cast< std::uint64_t >( value.int64 ) & 0xFFFFFFFFu );
    else
        return false;
    return true;
}

// VuBasicProperty<VuColor,6> (vtable 0x739DA8): load 0x406FE0, value read
// 0x432F70 through 0x4FDEC0 - "R", "G", "B" required (one missing: nothing
// stored), "A" optional (else 0xFF), each channel's low byte packed R, G, B, A
// from the lowest byte up; the filter 0x432F20 copies.
inline void property_color( const value_t& properties, const char* name, std::uint32_t& target, std::uint32_t fallback )
{
    target = fallback;
    const value_t& value = member( properties, name );
    std::int32_t r, g, b;
    if ( !property_channel( member( value, "R" ), r ) || !property_channel( member( value, "G" ), g ) ||
         !property_channel( member( value, "B" ), b ) )
        return;
    std::int32_t a = 0xFF;
    property_channel( member( value, "A" ), a );
    const std::uint32_t parsed = ( static_cast< std::uint32_t >( r ) & 0xFF ) | ( ( static_cast< std::uint32_t >( g ) & 0xFF ) << 8 ) |
                                 ( ( static_cast< std::uint32_t >( b ) & 0xFF ) << 16 ) | ( ( static_cast< std::uint32_t >( a ) & 0xFF ) << 24 );
    if ( target != parsed )
        target = parsed;
}
