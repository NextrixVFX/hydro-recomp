#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/entity/json.hxx>
#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

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

// ---------------------------------------------------------------------------
// THE SAVE SIDE of the property classes (the slots the loads above leave):
//   slot 3 save (vt+0xC)    : json[name] = slot 5 unless the target equals the
//                             node's default +0x14 (0x407020 float / angle /
//                             percentage: ucomiss, NaN saves; 0x407260 bool byte;
//                             0x4092E0 vector3 / rotation3d, 0x426560 vector2: any
//                             lane, ucomiss; 0x432F40 colour, 0x435190 int / int
//                             enum: the dword).
//   slot 5 write (vt+0x14)  : the target through slot 10 into a json value
//                             (0x407100 float 2, 0x407300 bool 3, 0x435240 int 1,
//                             0x4093F0 vector3 {X Y Z} floats, 0x426640 vector2
//                             {X Y}, 0x432FE0 -> 0x4FE890 colour {R G B} ints and A
//                             unless 0xFF, 0x5036C0 int enum: an int, then 0x5037C0
//                             turns it into the first choice's name with that value,
//                             or "" - a string).
//   slot 6 write default    : the same on the node's +0x14 (0x407140, 0x407330,
//                             0x4094B0, 0x4266D0, 0x433000, 0x435270, 0x503740).
//   slot 8 reset (vt+0x20)  : the target = the value the last load left (0x406B10
//                             +0x18; 0x406B50 bool +0x15; 0x409240 vector3 /
//                             rotation3d / rect +0x24 .. +0x33, all four lanes;
//                             0x4263B0 vector2 +0x1C / +0x20).
//   slot 10 filter out      : angle 0x4357D0 x 57.29578 (0x6F77CC), percentage
//                             0x43E5A0 x 100 (0x6F77E0), rotation3d 0x45CB90 the
//                             three lanes x 57.29578; the others copy.
// A json value here is a value_t with retail's tag (1 int, 2 float, 3 bool, 4
// string, 6 object); 0x4FBB10 clears it, 0x4FC090 makes it an object (0x4FC6B0)
// and finds or inserts the member - std::map's key order (the object vector kept
// sorted by key).
enum class property_class_t : std::uint8_t
{
    real, angle, percentage, boolean, integer, int_enum, color, vector2, vector3, rotation3d
};

struct property_node_t
{
    property_class_t cls = property_class_t::real;
    std::string name;
    std::uint32_t words[ 8 ]{};    // the node's +0x14 .. +0x33
    std::uint32_t target[ 4 ]{};   // the target's words (a bool: its low byte)
    const property_choice_t* choices = nullptr;
    int choice_count = 0;
};

// json_clear 0x4FBB10 / json_member 0x4FC090 / json_set_int / json_set_float: entity/json.hxx.

inline float property_bits_float( std::uint32_t w )
{
    float f;
    std::memcpy( &f, &w, 4 );
    return f;
}

// Slot 5 / 6's body on `source` (the target's words, or the node's +0x14 ..).
inline void property_write_words( const property_node_t& n, const std::uint32_t* source, value_t& j )
{
    switch ( n.cls )
    {
    case property_class_t::real:
        json_set_float( j, property_bits_float( source[ 0 ] ) );
        break;
    case property_class_t::angle:
        json_set_float( j, property_bits_float( source[ 0 ] ) * 57.295780181884766f );
        break;
    case property_class_t::percentage:
        json_set_float( j, property_bits_float( source[ 0 ] ) * 100.0f );
        break;
    case property_class_t::boolean:
        json_clear( j );
        j.tag = 3, j.kind = value_t::kind_t::integer, j.integer = static_cast< int >( source[ 0 ] & 0xFFu );
        break;
    case property_class_t::integer:
        json_set_int( j, static_cast< std::int32_t >( source[ 0 ] ) );
        break;
    case property_class_t::int_enum:
    {
        json_clear( j );
        j.tag = 4, j.kind = value_t::kind_t::text;
        for ( int k = 0; k < n.choice_count; ++k )
            if ( n.choices[ k ].m_value == static_cast< std::int32_t >( source[ 0 ] ) )
            {
                j.text = n.choices[ k ].m_name;
                break;
            }
        break;
    }
    case property_class_t::color:
    {
        static const char* const keys[ 4 ] = { "R", "G", "B", "A" };
        for ( int k = 0; k < 4; ++k )
        {
            const std::uint32_t b = ( source[ 0 ] >> ( 8 * k ) ) & 0xFFu;
            if ( k == 3 && b == 0xFFu )
                break;
            json_set_int( json_member( j, keys[ k ] ), static_cast< std::int32_t >( b ) );
        }
        break;
    }
    case property_class_t::vector2:
    case property_class_t::vector3:
    case property_class_t::rotation3d:
    {
        static const char* const keys[ 3 ] = { "X", "Y", "Z" };
        const int lanes = n.cls == property_class_t::vector2 ? 2 : 3;
        for ( int k = 0; k < lanes; ++k )
        {
            float v = property_bits_float( source[ k ] );
            if ( n.cls == property_class_t::rotation3d )
                v = v * 57.295780181884766f;
            json_set_float( json_member( j, keys[ k ] ), v );
        }
        break;
    }
    }
}

inline void property_write( const property_node_t& n, value_t& j )   // slot 5
{
    property_write_words( n, n.target, j );
}

inline void property_write_default( const property_node_t& n, value_t& j )   // slot 6
{
    property_write_words( n, n.words, j );
}

inline void property_save( const property_node_t& n, value_t& j )   // slot 3
{
    // ucomiss + lahf / test ah, 0x44 / jnp: only ordered-equal lanes count as unchanged.
    auto same = []( std::uint32_t a, std::uint32_t b ) {
        return property_bits_float( a ) == property_bits_float( b );   // false on NaN, true for -0 / +0
    };
    bool unchanged = true;
    switch ( n.cls )
    {
    case property_class_t::real:
    case property_class_t::angle:
    case property_class_t::percentage:
        unchanged = same( n.target[ 0 ], n.words[ 0 ] );
        break;
    case property_class_t::boolean:
        unchanged = ( n.target[ 0 ] & 0xFFu ) == ( n.words[ 0 ] & 0xFFu );
        break;
    case property_class_t::vector2:
        unchanged = same( n.target[ 0 ], n.words[ 0 ] ) && same( n.target[ 1 ], n.words[ 1 ] );
        break;
    case property_class_t::vector3:
    case property_class_t::rotation3d:
        unchanged = same( n.target[ 0 ], n.words[ 0 ] ) && same( n.target[ 1 ], n.words[ 1 ] ) && same( n.target[ 2 ], n.words[ 2 ] );
        break;
    default:
        unchanged = n.target[ 0 ] == n.words[ 0 ];
        break;
    }
    if ( !unchanged )
        property_write( n, json_member( j, n.name ) );
}

inline void property_reset( property_node_t& n )   // slot 8
{
    switch ( n.cls )
    {
    case property_class_t::boolean:
        n.target[ 0 ] = ( n.target[ 0 ] & ~0xFFu ) | ( ( n.words[ 0 ] >> 8 ) & 0xFFu );   // byte +0x15
        break;
    case property_class_t::vector2:
        n.target[ 0 ] = n.words[ 2 ], n.target[ 1 ] = n.words[ 3 ];
        break;
    case property_class_t::vector3:
    case property_class_t::rotation3d:
        for ( int k = 0; k < 4; ++k )
            n.target[ k ] = n.words[ 4 + k ];
        break;
    default:
        n.target[ 0 ] = n.words[ 1 ];
        break;
    }
}

// Slot 1 (the getType) of the three classes whose slot 1 is in no other file: VuBasicProperty<VuVector2> 0x426390 = 7,
// VuStaticIntEnumProperty 0x5035C0 = 5, VuNotifyProperty 0x5D2940 = 10.
inline int property_type_vector2( ) { return 7; }
inline int property_type_int_enum( ) { return 5; }
inline int property_type_notify( ) { return 10; }

// VuNotifyProperty's value read (slot 4, 0x5D2950: json, notify byte; retn 8): nothing is read or stored - only, when the
// notify byte is set and a watcher is attached (+0xC), the watcher's vt+8 (a hook). Returns whether it is called.
inline bool property_notify_read( bool has_watcher, bool notify )
{
    return notify && has_watcher;
}

// VuStaticStringEnumProperty's choice count (slot 12, 0x5039E0): the null-ended name array at +0x48.
inline int property_string_enum_count( const char* const* names )
{
    int n = 0;
    while ( names[ n ] )
        ++n;
    return n;
}

// The canonical text the property_save check compares (case property_save's _json_dump): ~ none, i int, f float bits,
// b bool byte, s'..' string, {key:value,...} an object in key order.
inline std::string property_json_text( const value_t& j )
{
    char b[ 32 ];
    switch ( j.tag )
    {
    case 0: return "~";
    case 1: std::snprintf( b, sizeof( b ), "i%d", j.integer ); return b;
    case 2:
    {
        std::uint32_t w;
        std::memcpy( &w, &j.real, 4 );
        std::snprintf( b, sizeof( b ), "f%08X", w );
        return b;
    }
    case 3: std::snprintf( b, sizeof( b ), "b%d", j.integer & 0xFF ); return b;
    case 4: return "s'" + j.text + "'";
    case 6:
    {
        std::string s = "{";
        for ( std::size_t k = 0; k < j.object.size( ); ++k )
            s += ( k ? "," : "" ) + j.object[ k ].first + ":" + property_json_text( j.object[ k ].second );
        return s + "}";
    }
    default: std::snprintf( b, sizeof( b ), "?%u", j.tag ); return b;
    }
}