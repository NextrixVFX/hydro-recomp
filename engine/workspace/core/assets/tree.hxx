#pragma once

#include <workspace/utility/math.hxx>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// The parsed form of a VUJB document - the binary tree every VuProjectAsset,
// VuTemplateAsset and VuDBAsset in the pack decodes to. reader.hxx turns bytes
// into one of these; everything else in the build reads them through the three
// lookups below.
struct value_t
{
    enum class kind_t { none, integer, real, text, list, object, int64 };
    kind_t kind = kind_t::none;
    // Retail's own type tag (+0 of its 16-byte node): what tells a bool (3)
    // from an int (1) while both still read through `integer`.
    std::uint32_t tag = 0;
    int integer = 0;
    float real = 0.0f;
    std::int64_t int64 = 0;
    std::string text;
    std::vector< value_t > list;
    std::vector< std::pair< std::string, value_t > > object;
};

// Named lookup in an object node. A missing key returns null, which callers
// distinguish from a key authored AT its default - checking by pointer rather
// than by value is the difference between "absent" and "authored", and the two
// look identical through a fallback.
inline const value_t* find( const value_t& value, const char* key )
{
    if ( value.kind != value_t::kind_t::object )
        return nullptr;
    for ( const auto& field : value.object )
    {
        if ( field.first == key )
            return &field.second;
    }
    return nullptr;
}

inline const value_t* find( const value_t& value, const std::string& key )
{
    return find( value, key.c_str( ) );
}

// The container lookup @0x4FBDF0: a map's member by key (std::map::find
// 0x4F22B0, the key built through strlen), else the static null container
// 0x7593A0 - for a missing key and for anything that is not a map.
inline const value_t& member( const value_t& value, const char* key )
{
    static const value_t null{};
    if ( value.tag != 6 )
        return null;
    for ( const auto& field : value.object )
    {
        if ( field.first == key )
            return field.second;
    }
    return null;
}

// The vec3 parse @0x4FDC60: X, Y, Z through 0x4FBDF0, each an int (1), a
// float's bits (2) or an int64 (7, x87 fild + fstp: one rounding); any other
// tag returns false at once, with the lanes before it already written.
inline bool vec3( const value_t& value, float* out )
{
    const char* const axes[ 3 ] = { "X", "Y", "Z" };
    for ( int lane = 0; lane < 3; ++lane )
    {
        const value_t& part = member( value, axes[ lane ] );
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

// A numeric read: the int / float / int64 lanes of the vector parses @0x4FDBC0
// / 0x4FDC60 / 0x4FDD30. Retail never reads a number out of text.
inline float number( const value_t* value, float fallback )
{
    if ( value == nullptr )
        return fallback;
    if ( value->kind == value_t::kind_t::real )
        return value->real;
    if ( value->kind == value_t::kind_t::integer )
        return static_cast< float >( value->integer );
    if ( value->kind == value_t::kind_t::int64 )
        return static_cast< float >( value->int64 );
    return fallback;
}

// A sub-object, or an empty node when the key is absent, so a caller can read
// X/Y/Z off the result without testing for null three times.
inline const value_t& axis( const value_t& properties, const char* key )
{
    const value_t* value = find( properties, key );
    static const value_t empty{};
    return value != nullptr ? *value : empty;
}

// A VuTransformComponent's Position / Rotation / Scale, composed the way
// VuMatrix_fromEuler @0x435640 composes them. A scale component authored as
// zero is taken as one: the format writes an absent scale as 0.
inline void read_transform( const value_t& properties, float* matrix )
{
    const float position[ 3 ] = {
        number( find( axis( properties, "Position" ), "X" ), 0.0f ),
        number( find( axis( properties, "Position" ), "Y" ), 0.0f ),
        number( find( axis( properties, "Position" ), "Z" ), 0.0f )
    };
    const float rotation[ 3 ] = {
        number( find( axis( properties, "Rotation" ), "X" ), 0.0f ),
        number( find( axis( properties, "Rotation" ), "Y" ), 0.0f ),
        number( find( axis( properties, "Rotation" ), "Z" ), 0.0f )
    };
    auto unit = []( float value )
    {
        return value == 0.0f ? 1.0f : value;
    };
    const float scale[ 3 ] = {
        unit( number( find( axis( properties, "Scale" ), "X" ), 1.0f ) ),
        unit( number( find( axis( properties, "Scale" ), "Y" ), 1.0f ) ),
        unit( number( find( axis( properties, "Scale" ), "Z" ), 1.0f ) )
    };
    math_transform( matrix, position, rotation, scale );
}
