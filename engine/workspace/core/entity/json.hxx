#pragma once

#include <workspace/core/assets/tree.hxx>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <emmintrin.h>

// VuJsonContainer (16 bytes in retail: +0 the tag - 0 null, 1 int, 2 float, 3
// bool, 4 string, 5 array, 6 object, 7 int64 - +8 / +0xC the payload: the
// value, or a pointer to a VuString / std::vector<VuJsonContainer> /
// std::map<VuString, VuJsonContainer>) over value_t (assets/tree.hxx; its tag
// is retail's). The members no other file recreates; the ones that exist are
// named here so a caller finds them:
//   0x4FBDF0 member (const lookup)        assets/tree.hxx member( )
//   0x4FDC60 vec3 parse                   assets/tree.hxx vec3( )
//   0x4FC550 getValue( int& )             entity/values.hxx property_int_value( )
//   0x4FBB10 clear / 0x4FC090 member-or-insert: json_clear / json_member below
//   (and, separately, ui/config.hxx ui_json_clear / ui_json_child: see the
//   DUPLICATES note at the end of this file).
// SUBSTITUTION: retail's std::vector / std::map internals (0x4FCC30 erase,
// 0x4FD360 copy-assign, 0x4FCFB0 uninitialised copy, 0x4FD840 the map insert)
// are value_t's std::vector members; a map's order is std::map's - the keys
// sorted by std::string's operator< (char_traits<char>::compare, unsigned).
// Verified against retail: hydro checks/water.hxx verify_json_access (case
// json_access: retail's own containers, built by its VUJB read 0x55B560).

// 0x4FBB10: the payload freed, tag 0.
inline void json_clear( value_t& j )
{
    j = value_t{ };
}

// 0x4FC090 (0x4FC6B0: anything not an object is cleared and becomes an empty
// one; then the map's find-or-insert): the member, a null one inserted when
// absent. The object vector is kept in key order.
inline value_t& json_member( value_t& j, const std::string& key )
{
    if ( j.tag != 6 )
    {
        json_clear( j );
        j.tag = 6;
        j.kind = value_t::kind_t::object;
    }
    auto at = j.object.begin( );
    while ( at != j.object.end( ) && at->first < key )
        ++at;
    if ( at != j.object.end( ) && at->first == key )
        return at->second;
    return j.object.insert( at, { key, value_t{ } } )->second;
}

inline void json_set_int( value_t& j, std::int32_t v )
{
    json_clear( j );
    j.tag = 1, j.kind = value_t::kind_t::integer, j.integer = v;
}

inline void json_set_float( value_t& j, float v )
{
    json_clear( j );
    j.tag = 2, j.kind = value_t::kind_t::real, j.real = v;
}

// The low and high dwords of an int64 payload.
inline std::uint32_t json_lo( const value_t& j )
{
    return static_cast< std::uint32_t >( static_cast< std::uint64_t >( j.int64 ) );
}

// 0x4FC470 asFloat: 1 the int (cvtdq2ps), 2 the float, 7 the int64 (x87 fild
// qword + fstp dword: one rounding to nearest), anything else +0 (xorps).
inline float json_as_float( const value_t& j )
{
    if ( j.tag == 1 )
        return static_cast< float >( j.integer );
    if ( j.tag == 2 )
        return j.real;
    if ( j.tag == 7 )
        return static_cast< float >( j.int64 );
    return 0.0f;
}

// 0x4FC500 asCString: a string's c_str (the VuString's inline buffer when its
// capacity +0x14 is below 16, else its heap pointer), anything else the empty
// literal 0x64D20B.
inline const char* json_as_cstring( const value_t& j )
{
    return j.tag == 4 ? j.text.c_str( ) : "";
}

// MSVC's _ftol2 0x631EB6 (x87: fistp of the rounded value, then the
// difference's sign corrects it toward zero): truncation, and the integer
// indefinite 0x8000000000000000 for NaN and |x| >= 2^63 - what cvttss2si r64
// gives.
inline std::int64_t json_ftol2( float x )
{
    return _mm_cvttss_si64( _mm_set_ss( x ) );
}

// 0x4FC520 asInt64: 1 the int sign-extended (cdq), 2 _ftol2 of the float, 7 the
// int64, anything else 0.
inline std::int64_t json_as_int64( const value_t& j )
{
    if ( j.tag == 1 )
        return j.integer;
    if ( j.tag == 2 )
        return json_ftol2( j.real );
    if ( j.tag == 7 )
        return j.int64;
    return 0;
}

// 0x4FC5E0 getValue( int64& ): asInt64's three tags write `out` and answer
// true; anything else false, `out` untouched.
inline bool json_get_int64( const value_t& j, std::int64_t& out )
{
    if ( j.tag != 1 && j.tag != 2 && j.tag != 7 )
        return false;
    out = json_as_int64( j );
    return true;
}

// 0x4FBD30 append: anything that is not an array is cleared and becomes an
// empty one (tag 5, a fresh vector); then 0x4FBC60( size ): grown by one null
// element, that element answered.
inline value_t& json_append( value_t& j )
{
    if ( j.tag != 5 )
    {
        json_clear( j );
        j.tag = 5;
        j.kind = value_t::kind_t::list;
    }
    j.list.emplace_back( );
    return j.list.back( );
}

// 0x4FBF30 getMemberKeys (thiscall container, std::vector<VuString>* out;
// the vector constructed empty): an object's keys in map order (0x431390
// push_back each), else none.
inline std::vector< std::string > json_member_keys( const value_t& j )
{
    std::vector< std::string > keys;
    if ( j.tag != 6 )
        return keys;
    for ( const auto& field : j.object )
        keys.push_back( field.first );
    std::sort( keys.begin( ), keys.end( ) );
    return keys;
}

// One lane of the vector parses: 1 the int (cvtdq2ps), 2 the float's bits, 7
// the int64 (fild + fstp); false for any other tag, the lane untouched.
inline bool json_lane( const value_t& part, float& out )
{
    if ( part.tag == 1 )
        out = static_cast< float >( part.integer );
    else if ( part.tag == 2 )
        std::memcpy( &out, &part.real, 4 );
    else if ( part.tag == 7 )
        out = static_cast< float >( part.int64 );
    else
        return false;
    return true;
}

// 0x4FDD30 vec4 parse (fastcall container, float* out): "X", "Y", "Z", "W"
// through 0x4FBDF0 (tree.hxx member), each a lane; the first lane that is not a
// number answers false with the lanes before it written.
inline bool json_vec4( const value_t& j, float* out )
{
    static const char* const axes[ 4 ] = { "X", "Y", "Z", "W" };
    for ( int lane = 0; lane < 4; ++lane )
        if ( !json_lane( member( j, axes[ lane ] ), out[ lane ] ) )
            return false;
    return true;
}

// 0x4FDE40 matrix parse (fastcall container, VuMatrix* out): the rows "X",
// "Y", "Z", "T" (0x4FBDF0 each) through 0x4FDD30 into out + 0 / 4 / 8 / 12;
// false at the first row that fails, the rows before it (and that row's
// leading lanes) written.
inline bool json_matrix( const value_t& j, float* out )
{
    static const char* const rows[ 4 ] = { "X", "Y", "Z", "T" };
    for ( int row = 0; row < 4; ++row )
        if ( !json_vec4( member( j, rows[ row ] ), out + 4 * row ) )
            return false;
    return true;
}

// DUPLICATES (for the integrator): VuJsonContainer is recreated three times
// over value_t -
//   (a) this file + assets/tree.hxx (member / vec3) + entity/values.hxx
//       (property_int_value);
//   (b) ui/config.hxx ui_json_clear / _make_map / _child (0x4FC090) /
//       _set_string (0x4FC640) / _at (0x4FBC60) / _erase (0x4FC140) / _find
//       (0x4F22B0) / _text / _keys (0x4FBE90 / 0x4FC000) and ui/select.hxx
//       ui_json_set_int / _set_bool / _element (0x4FBC10) / _size (0x4FBC40) /
//       _child_s (0x4FCAB0) / _as_int (0x4FC450) / _equal (0x4FC230).
// They agree on every answer; the one difference is the object vector's order
// after an insertion (ui_json_child appends, json_member keeps key order) -
// invisible to every lookup and to the keys walks (both sort). Proposal: keep
// this engine file as the one copy, move (b)'s functions here under the json_
// names, and leave the ui_ names as one-line forwards until their callers move.
