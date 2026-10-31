#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstring>

// More property classes' loads - the ones the front end's entities carry - on
// the same contract as property.hxx (load = reset to the default, value read,
// filter, store only if it differs; vt+0x2C and the watcher are hooks).

// cvttss2si: truncate toward zero; NaN and anything outside int32 give the
// "integer indefinite" 0x80000000.
inline std::int32_t property_truncate( float value )
{
    if ( !( value >= -2147483648.0f && value < 2147483648.0f ) )
        return static_cast< std::int32_t >( 0x80000000u );
    return static_cast< std::int32_t >( value );
}

// The int read shared by VuBasicProperty<int,0> 0x4351C0, VuStaticIntEnumProperty
// 0x503600 (after its name -> value step) and the JSON asInt 0x4FC450: tag 1 an
// int, tag 2 a float truncated (cvttss2si), tag 7 an int64's low dword; any
// other tag - false (0x4351C0 / 0x503600 store nothing).
inline bool property_int_value( const value_t& value, std::int32_t& out )
{
    if ( value.tag == 1 )
        out = value.integer;
    else if ( value.tag == 2 )
        out = property_truncate( value.real );
    else if ( value.tag == 7 )
        out = static_cast< std::int32_t >( static_cast< std::uint64_t >( value.int64 ) & 0xFFFFFFFFu );
    else
        return false;
    return true;
}

// VuBasicProperty<int,0> (vtable 0x73A218): load 0x406FE0 (the 4-byte load:
// target +0x1C = default +0x14, then vt+0x10), value read 0x4351C0, filter
// 0x435180 (copies), stored only if it differs.
inline void property_int( const value_t& properties, const char* name, std::int32_t& target, std::int32_t fallback )
{
    target = fallback;
    std::int32_t parsed;
    if ( !property_int_value( member( properties, name ), parsed ) )
        return;
    if ( target != parsed )
        target = parsed;
}

// One choice of a VuStaticIntEnumProperty's table (node +0x20: 8-byte entries
// {const char* name, int value}, ended by a null name - vt+0x30 0x5039C0
// counts them, vt+0x34 0x5035D0 / vt+0x38 0x5035E0 read one).
struct property_choice_t
{
    const char* m_name;
    std::int32_t m_value;
};

// VuStaticIntEnumProperty (vtable 0x74C354): load 0x406FE0, value read 0x503600
// = 0x5037C0's conversion, then the int read and store. 0x5037C0: a STRING is
// looked up by exact name (memcmp of the shorter length 0x404AC0, then the
// lengths) -> an int of that choice's value, or an int 0 when no name matches
// (so an unknown name stores 0); an INT (tag 1 only) becomes a string - "" or
// the matching choice's name - which the int read then refuses (nothing
// stored); any other tag gives the null container (nothing stored).
inline void property_enum( const value_t& properties, const char* name, std::int32_t& target, std::int32_t fallback,
    const property_choice_t* choices, int count )
{
    target = fallback;
    const value_t& value = member( properties, name );
    if ( value.tag != 4 )
        return;
    std::int32_t parsed = 0;
    for ( int k = 0; k < count; ++k )
        if ( value.text == choices[ k ].m_name )
        {
            parsed = choices[ k ].m_value;
            break;
        }
    if ( target != parsed )
        target = parsed;
}

// The float loads with a scaling filter: VuPercentageProperty (vtable
// 0x73B2DC) = the float's load 0x406FE0 / value read 0x407060 with the filter
// 0x43E580 (x 0x3C23D70A, 0.01, one mulss). The angle (x 0x3C8EFA35) is
// property_float's.
inline void property_percentage( const value_t& properties, const char* name, float& target, float fallback )
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
    float scale;
    const std::uint32_t bits = 0x3C23D70Au;
    std::memcpy( &scale, &bits, sizeof( scale ) );
    parsed *= scale;
    if ( target != parsed )
        target = parsed;
}

// The lanes of the vector2 parse 0x4FDBC0 ("X", "Y") and the rect parse
// 0x4FDFE0 ("X", "Y", "W", "H"): each an int (cvtdq2ps), a float's bits or an
// int64 (x87 fild + fstp); any other tag: false at once (the lanes written
// before stay in the caller's temporary).
inline bool property_lanes( const value_t& value, const char* const* keys, int count, float* out )
{
    for ( int lane = 0; lane < count; ++lane )
    {
        const value_t& part = member( value, keys[ lane ] );
        if ( part.tag == 1 )
            out[ lane ] = static_cast< float >( part.integer );
        else if ( part.tag == 2 )
            std::memcpy( &out[ lane ], &part.real, sizeof( float ) );
        else if ( part.tag == 7 )
            out[ lane ] = static_cast< float >( part.int64 );
        else
            return false;
    }
    return true;
}

// VuBasicProperty<VuVector2,6> (vtable 0x738A10): load 0x426510 (target +0x24 =
// default +0x14 / +0x18), value read 0x4265A0 via 0x4FDBC0, filter 0x4263C0
// (copies), stored when either lane differs (ucomiss: NaN differs, -0 == +0).
// VuBasicProperty<VuRect,9> (vtable 0x73D6F0): load 0x409280 (target +0x34 =
// default +0x14..+0x23), value read 0x463570 via 0x4FDFE0, filter 0x409260,
// stored when any of the four lanes differs.
inline void property_floats( const value_t& properties, const char* name, float* target, const float* fallback, int count )
{
    static const char* const vector2[ 2 ] = { "X", "Y" };
    static const char* const rect[ 4 ] = { "X", "Y", "W", "H" };
    for ( int lane = 0; lane < count; ++lane )
        target[ lane ] = fallback[ lane ];
    float parsed[ 4 ];
    if ( !property_lanes( member( properties, name ), count == 2 ? vector2 : rect, count, parsed ) )
        return;
    bool differs = false;
    for ( int lane = 0; lane < count; ++lane )
        differs = differs || target[ lane ] != parsed[ lane ];
    if ( differs )
        for ( int lane = 0; lane < count; ++lane )
            target[ lane ] = parsed[ lane ];
}
