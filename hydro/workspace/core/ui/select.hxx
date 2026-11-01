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

// ---------------------------------------------------------------------------
// VuBaseConfigEntity (ctor 0x4471C0, vtable 0x73C4D4) - the base of the game
// and boat selectors (VuGameConfigEntity ctor 0x45CC80 / vtable 0x73D1DC,
// VuBoatConfigEntity 0x44C9F0 / 0x73C69C). Its options document (+0x2A0,
// built by slot 4 0x448120 -> 0x449640 from the "OptionType" OptionDB entry
// through 0x449B20 / 0x44A4C0: NOT recreated - an input here) is an array of
// options { Name, ComboBoxID, Choices: [ { Value, Options? } ] }. THE
// GameConfig WRITER is the option walk 0x44B0D0: GameConfig[option Name] =
// the current choice's Value, for every option, recursing into that choice's
// own Options - so "Track", "Course", "Championship" are written here, by name
// (no literal "Track" store exists). The current choice is 0x44BD50: the first
// choice whose Value equals GameConfig[Name] (0x4FC230), else the class's
// slot 22 (+88) fallback - 0x4471B0 = 0 for VuGameConfigEntity: before any
// selection the FIRST choice. (Before any selection gameutil +80 itself is
// null: VuGameUtil's ctor 0x4D2810 zeroes +0x50.)

// An array element (the inline test every caller makes: tag 5, 0 <= i < size)
// else the static null container 0x7593A0.
inline const value_t& ui_json_element( const value_t& v, int i )
{
    static const value_t null{ };
    if ( v.tag == 5 && i >= 0 && static_cast< std::size_t >( i ) < v.list.size( ) )
        return v.list[ static_cast< std::size_t >( i ) ];
    return null;
}

// 0x4FBC40: an array's size, 0 for anything else.
inline int ui_json_size( const value_t& v )
{
    return v.tag == 5 ? static_cast< int >( v.list.size( ) ) : 0;
}

// 0x4FCAB0 (a map's member by a VuString key, the whole string; inserted null
// when absent) after 0x4FC6B0.
inline value_t& ui_json_child_s( value_t& v, const std::string& key )
{
    ui_json_make_map( v );
    for ( auto& field : v.object )
        if ( field.first == key )
            return field.second;
    v.object.emplace_back( key, value_t{ } );
    return v.object.back( ).second;
}

// 0x4FC450 asInt (1 the int, 2 cvttss2si, 7 the low dword, else 0).
inline int ui_json_as_int( const value_t& v )
{
    if ( v.tag == 1 )
        return v.integer;
    if ( v.tag == 2 )
        return static_cast< int >( v.real );
    if ( v.tag == 7 )
        return static_cast< int >( static_cast< std::uint32_t >( static_cast< std::uint64_t >( v.int64 ) ) );
    return 0;
}

// 0x4FC230 (thiscall a, b): different tags -> false; 0 true; 1 asInt ==; 2 the
// floats by ucomiss (NaN unequal); 3 the bytes; 4 the strings (0x407790:
// compare of the shorter length, then the lengths); 5 same size, then every
// element pair (a_i, b_i); 6 same size, then for each i the i-th keys in
// std::map order (0x407400: any difference fails) and a[key] == b[key]
// (0x4FBDA0 lookups by a's key); 7 the int64s (0x4FC520). Tags above 7 false.
inline bool ui_json_equal( const value_t& a, const value_t& b )
{
    if ( a.tag != b.tag )
        return false;
    switch ( a.tag )
    {
    case 0:
        return true;
    case 1:
        return ui_json_as_int( a ) == ui_json_as_int( b );
    case 2:
        return a.real == b.real;
    case 3:
        return ( a.integer & 0xFF ) == ( b.integer & 0xFF );
    case 4:
        return a.text == b.text;
    case 5:
        if ( a.list.size( ) != b.list.size( ) )
            return false;
        for ( std::size_t i = 0; i < a.list.size( ); ++i )
            if ( !ui_json_equal( a.list[ i ], b.list[ i ] ) )
                return false;
        return true;
    case 6:
    {
        if ( a.object.size( ) != b.object.size( ) )
            return false;
        const std::vector< std::string > ka = ui_json_keys( a ), kb = ui_json_keys( b );
        for ( std::size_t i = 0; i < ka.size( ); ++i )
        {
            if ( ka[ i ] != kb[ i ] )
                return false;
            if ( !ui_json_equal( ui_json_find( a, ka[ i ] ), ui_json_find( b, ka[ i ] ) ) )
                return false;
        }
        return true;
    }
    case 7:
        return a.int64 == b.int64;
    default:
        return false;
    }
}

// What the selectors consult that is not theirs. All HOOKS (profile side /
// platform): `unlock` = g_vuunlockmanagerimpl 0x758FE8 slot +92 / +96 (two
// strings: c unused) and +100 / +104 (three); `license` = the license
// manager 0x758FAC slot +0x18; `next_choice` = VuGameConfigEntity's slot 26
// (+104) 0x45E300 (READ - a walk over the progress manager 0x758FD8 +48 and
// the unlock manager - NOT recreated): true with the choice text to select.
struct ui_profile_t
{
    std::function< bool( int slot, const std::string& a, const std::string& b, const std::string& c ) > unlock;
    std::function< bool( ) > license;
    std::function< bool( const value_t& option, std::string& out ) > next_choice;
};

// The selector entity's fields the walk reads.
struct ui_config_entity_t
{
    bool m_single_player = false;  // +0x274 "Single Player"
    bool m_force_online = false;   // +0x275 "Force Online"
    bool m_refresh = false;        // +0x2CC, set around 0x448140's refresh
};

// gameutil (g 0x758FF8): +0x50 the game-data root, +0x60 a byte that stops
// both of the walk's profile branches, +0x64 the last screen operation.
struct ui_config_env_t
{
    value_t* m_root = nullptr;
    bool m_busy = false;
    std::string m_op;
};

// VuGameConfigEntity slot 24 (+96) 0x45E210: root ["LastGameConfigSelection"]
// [name] (lookups, nothing created) as a string, else "".
inline std::string ui_game_config_last( const value_t& root, const std::string& name )
{
    return ui_json_text( ui_json_find( member( root, "LastGameConfigSelection" ), name ) );
}

// VuGameConfigEntity slot 25 (+100) 0x45E2B0: root ["LastGameConfigSelection"]
// (0x4FC090, made a map) [name] (0x4FCAB0) = the text up to its first NUL.
inline void ui_game_config_remember( value_t& root, const std::string& name, const std::string& value )
{
    ui_json_set_string( ui_json_child_s( ui_json_child( root, "LastGameConfigSelection" ), name ), value.c_str( ) );
}

// VuGameConfigEntity slot 18 (+72) 0x45D0C0: whether a choice is locked. Not
// with Force Online, nor when root ["Online"] is anything but null. Mode =
// GameConfig.Mode's text. "Track": Race / TimeTrial -> !unlock+100(mode,
// value, ""); Slalom -> none of unlock+100(mode, value, CourseA / CourseB /
// CourseC) (in that order, stopping at the first true); RubberDucky / Ghost ->
// !unlock+100("Race", value, ""); any other mode false. "Course": only for
// Slalom, !unlock+100(mode, GameConfig.Track's text, value). "Championship":
// !unlock+92(name, value). Anything else false.
inline bool ui_game_config_locked( const ui_config_entity_t& e, const value_t& root, const std::string& name, const std::string& value,
                                   const ui_profile_t& p )
{
    if ( e.m_force_online || member( root, "Online" ).tag != 0 )
        return false;
    const std::string mode = ui_json_text( member( member( root, "GameConfig" ), "Mode" ) );
    if ( name == "Track" )
    {
        if ( mode == "Race" || mode == "TimeTrial" )
            return !p.unlock( 100, mode, value, "" );
        if ( mode == "Slalom" )
            return !( p.unlock( 100, mode, value, "CourseA" ) || p.unlock( 100, mode, value, "CourseB" ) || p.unlock( 100, mode, value, "CourseC" ) );
        if ( mode == "RubberDucky" || mode == "Ghost" )
            return !p.unlock( 100, "Race", value, "" );
        return false;
    }
    if ( name == "Course" )
        return mode == "Slalom" && !p.unlock( 100, mode, ui_json_text( member( member( root, "GameConfig" ), "Track" ) ), value );
    if ( name == "Championship" )
        return !p.unlock( 92, name, value, "" );
    return false;
}

// VuGameConfigEntity slot 19 (+76) 0x45D4C0 (asked only when the license hook
// answers true): "Track": Race / TimeTrial -> !unlock+104(mode, value, ""),
// other modes -> mode == "Ghost"; "Course": Slalom only, !unlock+104(mode,
// Track's text, value); "Championship": !unlock+96(name, value); else false.
inline bool ui_game_config_locked_trial( const value_t& root, const std::string& name, const std::string& value, const ui_profile_t& p )
{
    const std::string mode = ui_json_text( member( member( root, "GameConfig" ), "Mode" ) );
    if ( name == "Track" )
    {
        if ( mode != "Race" && mode != "TimeTrial" )
            return mode == "Ghost";
        return !p.unlock( 104, mode, value, "" );
    }
    if ( name == "Course" )
        return mode == "Slalom" && !p.unlock( 104, mode, ui_json_text( member( member( root, "GameConfig" ), "Track" ) ), value );
    if ( name == "Championship" )
        return !p.unlock( 96, name, value, "" );
    return false;
}

// 0x44BD50 (thiscall this, option): the index of the first choice whose
// "Value" equals root ["GameConfig"] [Name] (lookups only), else slot 22
// (+88) - VuGameConfigEntity's 0x4471B0 returns 0.
inline int ui_config_choice_index( const value_t& option, const value_t& root )
{
    const std::string& name = ui_json_text( member( option, "Name" ) );
    const value_t& current = ui_json_find( member( root, "GameConfig" ), name );
    const value_t& choices = member( option, "Choices" );
    for ( int i = 0; i < ui_json_size( choices ); ++i )
        if ( ui_json_equal( member( ui_json_element( choices, i ), "Value" ), current ) )
            return i;
    return 0;
}

// 0x44B0D0 (thiscall this, const VuJsonContainer& options), for
// VuGameConfigEntity's virtuals. For each element of `options` (an array):
//   the option is pushed onto the entity's option vector (+0x2BC);
//   with the refresh flag and gameutil +0x60 clear: if Single Player and
//     gameutil +0x64 == "push", next_choice (+104) -> remember (+100) its
//     answer; then GameConfig[Name] (0x4FC090, 0x4FC6B0, 0x4FCAB0) = the
//     remembered selection (+96);
//   the current choice (0x44BD50) and a copy of its Value's text;
//   without the refresh flag and with gameutil +0x60 clear: unless locked
//     (+72), and (license hook answering true) unless locked (+76) - the
//     license is asked every time - remember (+100) the Value;
//   root made a map, GameConfig (0x4FC970) made a map, [Name] = the Value's
//     text up to its first NUL;
//   the walk again over that choice's "Options" (same entity, same vector).
inline void ui_config_apply_options( const ui_config_entity_t& e, ui_config_env_t& env, const value_t& options,
                                     std::vector< const value_t* >& pushed, const ui_profile_t& p )
{
    value_t& root = *env.m_root;
    for ( int i = 0; i < ui_json_size( options ); ++i )
    {
        const value_t& option = ui_json_element( options, i );
        pushed.push_back( &option );
        const std::string& name = ui_json_text( member( option, "Name" ) );
        if ( e.m_refresh && !env.m_busy )
        {
            if ( e.m_single_player && env.m_op == "push" )
            {
                std::string out;
                if ( p.next_choice( option, out ) )
                    ui_game_config_remember( root, name, out );
            }
            value_t& slot = ui_json_child_s( ui_json_child( root, "GameConfig" ), name );
            ui_json_set_string( slot, ui_game_config_last( root, name ).c_str( ) );
        }
        const int index = ui_config_choice_index( option, root );
        const value_t& choice = ui_json_element( member( option, "Choices" ), index );
        const std::string value = ui_json_text( member( choice, "Value" ) );
        if ( !e.m_refresh && !env.m_busy )
        {
            const bool open = !ui_game_config_locked( e, root, name, value, p );
            const bool trial = p.license( ) ? !ui_game_config_locked_trial( root, name, value, p ) : true;
            if ( open && trial )
                ui_game_config_remember( root, name, value );
        }
        ui_json_set_string( ui_json_child_s( ui_json_child( root, "GameConfig" ), name ), value.c_str( ) );
        ui_config_apply_options( e, env, member( choice, "Options" ), pushed, p );
    }
}

// ---------------------------------------------------------------------------
// VuBoatConfigEntity (ctor 0x44C9F0 over VuBaseConfigEntity, vtable 0x73C69C):
// the boat selector edits GameConfig.Boat / .Skin through the option walk and
// hands them to BoatConfig.PlayerBoats[player].Properties. Its slot 6
// (onGameInitialize) 0x44CD10: player (+0x2E4) = BoatConfig.CurPlayerIndex
// (1 the int, 2 cvttss2si, 7 the low dword, else 0); 0x44E010 on
// PlayerBoats[player].Properties (lookups); the refresh 0x448140 (NOT
// recreated for this class: its walk runs the boat virtuals 0x44D150 /
// 0x44D2C0 / 0x44D970 / 0x44EA00 / 0x44EAD0 / 0x44EB20, unread); 0x44E090;
// PlayerBoats[player].Properties = that; the OnChangeDockedBoat /
// OnPlayerBoatConfig broadcasts and the stat plugs 0x44E510 (not recreated).
// Recreated here: 0x44E010, 0x44E090 and slot 7 (onGameRelease) 0x44D090.

// 0x44E010 (stdcall const VuJsonContainer& props): root ["GameConfig"]
// ["Boat"] = a copy of props ["Boat"] (0x4FB9B0: any type), then ["Skin"] the
// same. (Both read before the writes here: props lives in root's BoatConfig,
// which the GameConfig writes never touch.)
inline void ui_boat_config_take( value_t& root, const value_t& props )
{
    const value_t boat = member( props, "Boat" );
    const value_t skin = member( props, "Skin" );
    ui_json_child( ui_json_child( root, "GameConfig" ), "Boat" ) = boat;
    ui_json_child( ui_json_child( root, "GameConfig" ), "Skin" ) = skin;
}

// 0x44E090 (thiscall this, VuJsonContainer* out): out ["Boat"] = a copy of
// root GameConfig.Boat (lookups); with at most one option in the entity's
// option vector (+0x2BC .. +0x2C0) out ["Skin"] = "", else a copy of
// GameConfig.Skin.
inline void ui_boat_config_give( value_t& out, const value_t& root, std::size_t options )
{
    ui_json_child( out, "Boat" ) = member( member( root, "GameConfig" ), "Boat" );
    if ( options <= 1 )
        ui_json_set_string( ui_json_child( out, "Skin" ), "" );
    else
        ui_json_child( out, "Skin" ) = member( member( root, "GameConfig" ), "Skin" );
}

// Slot 7 0x44D090 (onGameRelease): a fresh container from 0x44E090, then root
// ["BoatConfig"] ["PlayerBoats"] (0x4FBC60: made an array, grown to player + 1
// when player >= size - a SIGNED compare) [player] ["Properties"] = a copy of
// it; then 0x53FA60(this + 92), the touch callback's removal (HOOK: platform
// input). A negative player indexes before the array in retail (its write
// lands outside it): false, nothing written.
inline bool ui_boat_config_release( value_t& root, int player, std::size_t options )
{
    if ( player < 0 )
        return false;
    value_t props;
    ui_boat_config_give( props, root, options );
    value_t& boats = ui_json_child( ui_json_child( root, "BoatConfig" ), "PlayerBoats" );
    ui_json_child( ui_json_at( boats, static_cast< std::size_t >( player ) ), "Properties" ) = props;
    return true;
}
