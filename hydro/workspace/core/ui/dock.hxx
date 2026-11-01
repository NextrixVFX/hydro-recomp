#pragma once

#include <workspace/core/ui/expand.hxx>

#include <cstdint>
#include <string>

// VuBoatConfigEntity (ctor 0x44C9F0 over VuBaseConfigEntity 0x4471C0, vtable
// 0x73C69C): the boat selector ("the docked boat"). Its class virtuals for the
// base's walks, its slot 12 (OnChangeDockedBoat, the stat plugs, the player's
// boat broadcast) and its slot 6 onGameInitialize 0x44CD10, transliterated
// from the disassembly. Fields: +0x2E0 "Allow Special Boats", +0x2E4 the
// player index. Output plugs (ctor): the four stat plugs named by the table
// 0x7357BC (Handling, Acceleration, TopSpeed, AirControl) and
// "OnPlayerSignedOut".

// What the boat selector reads besides the root: SkinDB (gameutil +12) and
// BoatDB (gameutil +8) as their documents' "VuDBAsset" maps.
struct ui_dock_t
{
    ui_config_entity_t m_base;
    int m_player = 0;               // +0x2E4
    bool m_allow_special = false;   // +0x2E0
    const value_t* m_boat_db = nullptr;
    const value_t* m_skin_db = nullptr;
    const value_t* m_champ_db = nullptr;   // gameutil +20 (slot 21 only)
};

// A DB entry: document ["VuDBAsset"] (lookup) then std::map::find by the
// whole text (node +40), else the null container.
inline const value_t& ui_db_entry( const value_t* db, const std::string& key )
{
    static const value_t null{ };
    return db ? ui_json_find( member( *db, "VuDBAsset" ), key ) : null;
}


// Slot 17 (+68) 0x44D410 (stdcall name, value): with the progress manager set
// and the name "Boat", its slot +132 (value); otherwise true.
inline bool ui_dock_blocked( const std::string& name, const std::string& value, const ui_profile_t& p )
{
    if ( p.has_progress && name == "Boat" )
        return p.progress( value );
    return true;
}

// Slot 18 (+72) 0x44D150 (thiscall this, name, value): online (Force Online
// +0x275, or root ["Online"] anything but null) - "Boat" is never locked;
// offline - "Skin" is locked when unlock +92("Boat", GameConfig.Boat's text)
// answers false. Then: locked = !unlock +92(name, value).
inline bool ui_dock_locked( const ui_dock_t& d, const value_t& root, const std::string& name, const std::string& value, const ui_profile_t& p )
{
    if ( d.m_base.m_force_online || member( root, "Online" ).tag != 0 )
    {
        if ( name == "Boat" )
            return false;
    }
    else if ( name == "Skin" )
    {
        const std::string boat = ui_json_text( member( member( root, "GameConfig" ), "Boat" ) );
        if ( !p.unlock( 92, "Boat", boat, "" ) )
            return true;
    }
    return !p.unlock( 92, name, value, "" );
}

// Slot 20 (+80) 0x44D2F0 (stdcall name, value): only for the name exactly
// "Boat": root ["Online"] ["AvailableBoats"] (lookups) an array - an empty one
// -> true; true unless an element's asInt equals the FNV-1a of the value
// (0x811C9DC5 / 0x1000193 over its bytes up to the NUL); anything else false.
inline bool ui_dock_unavailable( const value_t& root, const std::string& name, const std::string& value )
{
    if ( name != "Boat" )
        return false;
    const value_t& boats = member( member( root, "Online" ), "AvailableBoats" );
    if ( boats.tag != 5 )
        return false;
    const int hash = static_cast< int >( fsm_hash( value.c_str( ) ) );
    for ( int j = 0; j < ui_json_size( boats ); ++j )
        if ( ui_json_as_int( ui_json_element( boats, j ) ) == hash )
            return false;
    return true;
}

// Slot 22 (+88) 0x44D970 (thiscall this, option, current): only for the
// option named exactly "Skin" (else 0). (SkinDB is looked up by the current
// text and the result dropped.) The team = root ["BoatConfig"]
// ["PlayerBoats"] [player] ["Properties"] ["Team"] - each made (0x4FC090,
// 0x4FBC60: a map / an array grown to player + 1; a negative player writes
// before the array in retail: a fault here) - as text; the first choice whose
// SkinDB entry's "Team" text equals it, else 0.
inline int ui_dock_fallback( ui_dock_t& d, value_t& root, const value_t& option )
{
    if ( ui_json_text( member( option, "Name" ) ) != "Skin" )
        return 0;
    if ( d.m_player < 0 )
    {
        d.m_base.m_fault = true;
        return 0;
    }
    value_t& boats = ui_json_child( ui_json_child( root, "BoatConfig" ), "PlayerBoats" );
    const std::string team =
        ui_json_text( ui_json_child( ui_json_child( ui_json_at( boats, static_cast< std::size_t >( d.m_player ) ), "Properties" ), "Team" ) );
    const value_t& choices = member( option, "Choices" );
    for ( int i = 0; i < ui_json_size( choices ); ++i )
    {
        const std::string& value = ui_json_text( member( ui_json_element( choices, i ), "Value" ) );
        if ( ui_json_text( member( ui_db_entry( d.m_skin_db, value ), "Team" ) ) == team )
            return i;
    }
    return 0;
}

// Slot 24 (+96) 0x44EA00: root ["LastBoatConfigSelection"] [player] (array
// element-or-null) [name] (find) as text.
inline std::string ui_dock_last( const ui_dock_t& d, const value_t& root, const std::string& name )
{
    return ui_json_text( ui_json_find( ui_json_element( member( root, "LastBoatConfigSelection" ), d.m_player ), name ) );
}

// Slot 25 (+100) 0x44EAD0: root ["LastBoatConfigSelection"] (made an array,
// grown to player + 1 - a negative player writes outside in retail: a fault)
// [player] made a map, [name] = the value's text up to its NUL.
inline void ui_dock_remember( ui_dock_t& d, value_t& root, const std::string& name, const std::string& value )
{
    if ( d.m_player < 0 )
    {
        d.m_base.m_fault = true;
        return;
    }
    value_t& entry = ui_json_at( ui_json_child( root, "LastBoatConfigSelection" ), static_cast< std::size_t >( d.m_player ) );
    ui_json_set_string( ui_json_child_s( entry, name ), value.c_str( ) );
}

// Slot 23 (+92) 0x44DD90: the first option of the vector whose current
// choice is locked (slot 18) gives "Unlock_" + its Value + "_Info".
inline void ui_dock_info( const ui_dock_t& d, const value_t& root, const ui_config_class_t& cls, std::string& out )
{
    for ( const value_t* option : d.m_base.m_options )
    {
        const value_t& choice = ui_config_current_choice( *option, root, cls );
        const std::string value = ui_json_text( member( choice, "Value" ) );
        if ( cls.locked( ui_json_text( member( *option, "Name" ) ), value ) )
        {
            out = "Unlock_" + value + "_Info";
            return;
        }
    }
}

// Slot 26 (+104) 0x44EB20 (thiscall this, option, VuString* out): only for an
// option named exactly "Boat": the first choice neither blocked (slot 17) nor
// locked (slot 18) - its Value text out, true; else false.
inline bool ui_dock_next_choice( const value_t& option, const ui_config_class_t& cls, std::string& out )
{
    const std::string& name = ui_json_text( member( option, "Name" ) );
    if ( name != "Boat" )
        return false;
    const value_t& choices = member( option, "Choices" );
    for ( int j = 0; j < ui_json_size( choices ); ++j )
    {
        const std::string& value = ui_json_text( member( ui_json_element( choices, j ), "Value" ) );
        if ( !cls.blocked( name, value ) && !cls.locked( name, value ) )
        {
            out = value;
            return true;
        }
    }
    return false;
}

// 0x44E510 (thiscall this, const VuJsonContainer& given): BoatDB [given.Boat's
// text] ["Stats"]: for each key (std::map order; the count re-read every
// step) a VuParams int record of asInt(Stats[key]) fired through the plug of
// that name (VuScriptComponent_findPlug on this +96). False when no such
// output plug exists (retail calls through null).
inline bool ui_dock_stats( const ui_dock_t& d, const value_t& given, const ui_config_hooks_t& h )
{
    const value_t& stats = member( ui_db_entry( d.m_boat_db, ui_json_text( member( given, "Boat" ) ) ), "Stats" );
    for ( const std::string& key : ui_json_keys( stats ) )
    {
        ui_params_t p;
        ui_params_add( p, 0, static_cast< std::uint32_t >( ui_json_as_int( ui_json_find( stats, key ) ) ) );
        if ( !h.fire( key, p ) )
            return false;
    }
    return true;
}

// 0x44E6E0 (thiscall this, given): "OnPlayerBoatConfig" with the player
// (+0x2E4, int), given.Boat's and given.Skin's texts, and the team: -1, or
// for a SkinDB entry of the skin that is not null, "Team" "Blue" 0 / "Red" 1.
inline void ui_dock_player_boat( const ui_dock_t& d, const value_t& given, const ui_config_hooks_t& h )
{
    const std::string& boat = ui_json_text( member( given, "Boat" ) );
    const std::string& skin = ui_json_text( member( given, "Skin" ) );
    int team = -1;
    const value_t& entry = ui_db_entry( d.m_skin_db, skin );
    if ( entry.tag != 0 )
    {
        const std::string& t = ui_json_text( member( entry, "Team" ) );
        if ( t == "Blue" )
            team = 0;
        else if ( t == "Red" )
            team = 1;
    }
    ui_params_t p;
    ui_params_add( p, 0, static_cast< std::uint32_t >( d.m_player ) );
    p.add_string( boat.c_str( ) );
    p.add_string( skin.c_str( ) );
    ui_params_add( p, 0, static_cast< std::uint32_t >( team ) );
    ui_broadcast( h, "OnPlayerBoatConfig", p );
}

// "OnChangeDockedBoat" (inlined in slot 12 0x44E2B0 and in 0x44CD10): the
// given container's Boat and Skin texts (0x4FC090 on it: both exist after
// 0x44E090) and a bool = some option locked (0x449050 false) or - only
// asked when none is - some unavailable (0x449350 false); then the stat plugs
// and OnPlayerBoatConfig.
inline bool ui_dock_announce( const ui_dock_t& d, const value_t& root, const value_t& given, const ui_config_class_t& cls,
                              const ui_profile_t& p, const ui_config_hooks_t& h )
{
    ui_params_t changed;
    changed.add_string( ui_json_text( member( given, "Boat" ) ).c_str( ) );
    changed.add_string( ui_json_text( member( given, "Skin" ) ).c_str( ) );
    const bool locked = !ui_config_all_unlocked( d.m_base, root, cls, p ) || !ui_config_all_available( d.m_base, root, cls );
    ui_params_add_bool( changed, locked );
    ui_broadcast( h, "OnChangeDockedBoat", changed );
    if ( !ui_dock_stats( d, given, h ) )
        return false;
    ui_dock_player_boat( d, given, h );
    return true;
}

// Slot 12 (+48) 0x44E2B0: 0x44E090's container, then the announcement.
inline bool ui_dock_changed( ui_dock_t& d, value_t& root, const ui_config_class_t& cls, const ui_profile_t& p, const ui_config_hooks_t& h )
{
    value_t given;
    ui_boat_config_give( given, root, d.m_base.m_options.size( ) );
    return ui_dock_announce( d, root, given, cls, p, h );
}

// VuBoatConfigEntity's virtuals into `c` (held by the caller: the virtuals
// that call other virtuals reach them through it).
inline void ui_dock_class( ui_config_class_t& c, ui_dock_t& d, ui_config_env_t& env, const ui_profile_t& p, const ui_config_hooks_t& h )
{
    value_t& root = *env.m_root;
    c.blocked = [ &p ]( const std::string& name, const std::string& value ) { return ui_dock_blocked( name, value, p ); };
    c.locked = [ &d, &root, &p ]( const std::string& name, const std::string& value ) { return ui_dock_locked( d, root, name, value, p ); };
    c.locked_trial = [ &p ]( const std::string& name, const std::string& value ) { return !p.unlock( 96, name, value, "" ); };  // 0x44D2C0
    c.unavailable = [ &root ]( const std::string& name, const std::string& value ) { return ui_dock_unavailable( root, name, value ); };
    c.fallback = [ &d, &root ]( const value_t& option, const std::string& ) { return ui_dock_fallback( d, root, option ); };
    c.last = [ &d, &root ]( const std::string& name ) { return ui_dock_last( d, root, name ); };
    c.remember = [ &d, &root ]( const std::string& name, const std::string& value ) { ui_dock_remember( d, root, name, value ); };
    c.info = [ &c, &d, &root ]( std::string& out ) { ui_dock_info( d, root, c, out ); };
    c.next_choice = [ &c ]( const value_t& option, std::string& out ) { return ui_dock_next_choice( option, c, out ); };
    c.changed = [ &c, &d, &root, &p, &h ]( ) { return ui_dock_changed( d, root, c, p, h ); };
}

// Slot 6 onGameInitialize 0x44CD10: player (+0x2E4) = asInt(root
// ["BoatConfig"] ["CurPlayerIndex"]) (lookups; 1 the int, 2 cvttss2si, 7 the
// low dword, else 0); 0x44E010 on PlayerBoats[player] (element-or-null)
// ["Properties"]; the refresh 0x448140; 0x44E090's container -> root
// ["BoatConfig"] ["PlayerBoats"] [player] ["Properties"] (a negative player
// writes before the array in retail: false); the announcement again; the
// voice "Global/Voice/vo_menu_ChooseYourBoat" (FMOD getEvent / start: HOOK).
// `options` is the options document (+0x2A0, built by slot 4 - an input).
inline bool ui_dock_initialize( ui_dock_t& d, ui_config_env_t& env, const value_t& options, const ui_profile_t& p, const ui_config_hooks_t& h )
{
    value_t& root = *env.m_root;
    ui_config_class_t cls;
    ui_dock_class( cls, d, env, p, h );
    d.m_player = ui_json_as_int( member( member( root, "BoatConfig" ), "CurPlayerIndex" ) );
    const value_t props = member( ui_json_element( member( member( root, "BoatConfig" ), "PlayerBoats" ), d.m_player ), "Properties" );
    ui_boat_config_take( root, props );
    if ( !ui_config_refresh( d.m_base, env, options, cls, p, h ) )
        return false;
    value_t given;
    ui_boat_config_give( given, root, d.m_base.m_options.size( ) );
    if ( d.m_player < 0 )
        return false;
    value_t& boats = ui_json_child( ui_json_child( root, "BoatConfig" ), "PlayerBoats" );
    ui_json_child( ui_json_at( boats, static_cast< std::size_t >( d.m_player ) ), "Properties" ) = given;
    if ( !ui_dock_announce( d, root, given, cls, p, h ) )
        return false;
    h.sound( "Global/Voice/vo_menu_ChooseYourBoat" );
    return !d.m_base.m_fault;
}

// 0x4D3D40 (thiscall gameutil, boat, mode): BoatDB's entry for the boat; null
// -> false; false when an element of its "DisallowedModes" (an array) has the
// mode as its text; else true.
inline bool ui_dock_mode_allowed( const ui_dock_t& d, const std::string& boat, const std::string& mode )
{
    const value_t& entry = ui_db_entry( d.m_boat_db, boat );
    if ( entry.tag == 0 )
        return false;
    const value_t& modes = member( entry, "DisallowedModes" );
    for ( int j = 0; j < ui_json_size( modes ); ++j )
        if ( ui_json_text( ui_json_element( modes, j ) ) == mode )
            return false;
    return true;
}

// Slot 21 (+84) 0x44D450 (thiscall this, option, choice): whether slot 4's copy
// drops the choice. Mode = GameConfig.Mode's text, Course = GameConfig.Course's.
// "Boat" (exactly): BoatDB's Class text for the value; kept only when the
// package test 0x4D3B80 passes, the class is not "S" unless "Allow Special
// Boats" (+0x2E0), and 0x4D3D40 allows the mode - then Slalom with Course
// "CourseA" / "CourseB" / "CourseC" keeps only the class "A" / "B" / "C"
// (exactly), Championship keeps only the class equal to ChampDB [GameConfig.
// Championship's text] ["Class"]'s text; others keep it. "Skin": SkinDB's entry
// for the value - RubberDucky drops one with no "Team" (tag 0), any other mode
// drops one with a "Team". Anything else is kept.
inline bool ui_dock_excluded( const ui_dock_t& d, const value_t& root, const value_t& option, const value_t& choice,
                              const std::function< bool( const std::string& ) >& package )
{
    const std::string& mode = ui_json_text( member( member( root, "GameConfig" ), "Mode" ) );
    const std::string& course = ui_json_text( member( member( root, "GameConfig" ), "Course" ) );
    const std::string& name = ui_json_text( member( option, "Name" ) );
    const std::string& value = ui_json_text( member( choice, "Value" ) );
    if ( name == "Boat" )
    {
        const std::string& cls = ui_json_text( member( ui_db_entry( d.m_boat_db, value ), "Class" ) );
        if ( !ui_db_package_ok( d.m_boat_db, value, package ) || ( !d.m_allow_special && cls == "S" ) || !ui_dock_mode_allowed( d, value, mode ) )
            return true;
        if ( mode == "Slalom" )
        {
            if ( course == "CourseA" && cls != "A" )
                return true;
            if ( course == "CourseB" && cls != "B" )
                return true;
            if ( course == "CourseC" && cls != "C" )
                return true;
            return false;
        }
        if ( mode == "Championship" )
        {
            const std::string& champ = ui_json_text( member( member( root, "GameConfig" ), "Championship" ) );
            return cls != ui_json_text( member( ui_db_entry( d.m_champ_db, champ ), "Class" ) );
        }
        return false;
    }
    if ( name == "Skin" )
    {
        const value_t& entry = ui_db_entry( d.m_skin_db, value );
        if ( mode == "RubberDucky" )
            return member( entry, "Team" ).tag == 0;
        return member( entry, "Team" ).tag != 0;
    }
    return false;
}

// VuBoatConfigEntity's slot 4 (0x448120 -> 0x449640) into `doc` / the combos.
inline bool ui_dock_build( ui_dock_t& d, ui_config_env_t& env, value_t& doc, const value_t* option_db, const std::string& entry, std::uint32_t flags,
                           const ui_profile_t& p, const ui_config_hooks_t& h )
{
    ui_config_class_t cls;
    ui_dock_class( cls, d, env, p, h );
    value_t& root = *env.m_root;
    auto excluded = [ &d, &root, &p ]( const value_t& option, const value_t& choice ) { return ui_dock_excluded( d, root, option, choice, p.package ); };
    return ui_config_build( d.m_base, env, doc, option_db, entry, excluded, flags, cls, p, h );
}