#pragma once

#include <workspace/core/ui/config.hxx>
#include <workspace/core/ui/flow.hxx>

#include <cstdint>
#include <cstring>

// The front end's selectors: the entities that WRITE what the GameData
// builders (config.hxx) read from gameutil +80 - BoatConfig.CurPlayerIndex,
// BoatConfig.PlayerBoats[i], GameConfig.<option> - transliterated from the
// disassembly.
//
// VuSetBoatConfigPlayerEntity (ctor 0x44ECF0 over VuEntity 0x500710, vtable
// 0x73C654; no properties of its own): one VuScriptComponent (order 150,
// enabled) with two input plugs bound through 0x44F400 (the output
// initialiser, then VuScriptInputPlug 0x737138 and a VuMethod1 0x73C634):
//   "SetSinglePlayer" -> 0x44EF10
//   "SetIndex"        -> 0x44EEA0
// No output plugs, no broadcast; both return VuRetVal 0 (its type word only:
// the second dword is never written).

// VuParams' int read 0x4237B0 (thiscall on the reader { cursor, remaining }):
// nothing left -> 0; else the record's type word (4 bytes, memcpy_s) and, only
// when it is 0 (an int), the type and the 4-byte value are stepped over and
// the value returned; any other type -> 0, nothing consumed. The reads stay
// inside VuParams' 128 bytes + its size word (+128) for every record the
// buffer can hold; `m_at` past that is not reached by any VuParams (size <=
// 128).
inline int ui_params_read_int( ui_params_reader_t& r )
{
    if ( r.m_remaining <= 0 )
        return 0;
    std::uint32_t type = 0xFFFFFFFFu;
    std::memcpy( &type, r.m_at, 4 );
    if ( type != 0 )
        return 0;
    r.m_at += 4;
    r.m_remaining -= 4;
    int value = 0;
    std::memcpy( &value, r.m_at, 4 );
    r.m_at += 4;
    r.m_remaining -= 4;
    return value;
}

// An int member: 0x4FBB10 clear, tag 1, the value (+8).
inline void ui_json_set_int( value_t& v, int value )
{
    ui_json_clear( v );
    v.tag = 1;
    v.kind = value_t::kind_t::integer;
    v.integer = value;
}

// A bool member: 0x4FBB10 clear, tag 3, the byte (+8).
inline void ui_json_set_bool( value_t& v, bool value )
{
    ui_json_clear( v );
    v.tag = 3;
    v.kind = value_t::kind_t::integer;
    v.integer = value ? 1 : 0;
}

// "SetIndex" 0x44EEA0 (stdcall VuRetVal* out, const VuParams& p): the first
// record read as an int (0x4237B0 over { p, p +128 }) -> gameutil +80
// (`root`) ["BoatConfig"] ["CurPlayerIndex"] (0x4FC090 twice: each made a map
// if it is not) = that int.
inline std::uint32_t ui_set_boat_player_index( value_t& root, const ui_params_t& p )
{
    // 0x4237B0's reader takes VuParams' bytes and its size word together.
    std::uint8_t view[ 132 ];
    std::memcpy( view, p.m_bytes, 128 );
    std::memcpy( view + 128, &p.m_size, 4 );
    ui_params_reader_t r( p );
    r.m_at = view;
    const int index = ui_params_read_int( r );
    ui_json_set_int( ui_json_child( ui_json_child( root, "BoatConfig" ), "CurPlayerIndex" ), index );
    return 0;
}

// "SetSinglePlayer" 0x44EF10 (stdcall VuRetVal* out, const VuParams&; the
// params are never read): root ["BoatConfig"] ["CurPlayerIndex"] = int 0;
// then for k = 0 .. 3 (the loop steps a byte offset 0, 16, 32, 48 < 64): root
// made a map (inline 0x4FBB10 + tag 6 + a fresh map when it is not one),
// ["BoatConfig"] (0x4FC970) made a map, ["PlayerBoats"] made an array (tag 5,
// a fresh vector), grown to k + 1 null elements when k >= its size
// (0x4FC880), element k made a map, its ["Active"] = bool (k == 0). Other
// members of the elements, and elements past the fourth, are kept.
inline std::uint32_t ui_set_single_player( value_t& root )
{
    ui_json_set_int( ui_json_child( ui_json_child( root, "BoatConfig" ), "CurPlayerIndex" ), 0 );
    for ( int k = 0; k < 4; ++k )
    {
        value_t& boats = ui_json_child( ui_json_child( root, "BoatConfig" ), "PlayerBoats" );
        value_t& entry = ui_json_at( boats, static_cast< std::size_t >( k ) );
        ui_json_set_bool( ui_json_child( entry, "Active" ), k == 0 );
    }
    return 0;
}
