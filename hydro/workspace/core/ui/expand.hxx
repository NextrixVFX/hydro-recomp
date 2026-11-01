#pragma once

#include <workspace/core/ui/select.hxx>

#include <functional>
#include <string>

// VuBaseConfigEntity's slot 4 0x448120 -> 0x449640: the options document
// (+0x2A0) and the combo box records (+0x2B0) built from the entity's
// "OptionType" OptionDB entry (the asset property at +0x298: +68 the entry
// name, +72 the OptionDB asset), transliterated from the disassembly:
//   0x449B20  the copy - every key of each option but "Choices", then each
//             choice the class's slot 21 (+84) does not exclude, every key but
//             "Options", whose array is copied the same way (recursing)
//   0x44A4C0  in place: each choice merged with OptionDB ["VuDBAsset"]
//             ["References"] [its Value]; the option's ComboBoxID = the combo
//             count; one combo record pushed per option; the choices'
//             "Options" recursed
//   0x449AD0  the option vector emptied, the walk 0x44B0D0, 0x44B780, and with
//             the entity flag +0x14 & 4 slot 12 + 0x44B9C0.
// Slot 4 itself (0x448120) sets the refresh flag +0x2CC around 0x449640, so
// that walk takes the refresh branch (LastGameConfigSelection / the class's
// slot 24, and the single-player "push" next choice).

// 0x449B20 (thiscall this, const VuJsonContainer& src, VuJsonContainer* out):
// for each element of `src` (an array; anything else has none) `out` (made an
// array) grows by one element; that element gets a copy of each of the
// option's keys (std::map order) except exactly "Choices", made a map on the
// first; then for each choice not excluded: the element's "Choices" (inserted,
// made an array) grows by one; it gets a copy of each of the choice's keys
// except exactly "Options", and for "Options" (inserted) the same copy of the
// choice's own "Options".
inline void ui_config_copy_options( const value_t& src, value_t& out,
                                    const std::function< bool( const value_t& option, const value_t& choice ) >& excluded )
{
    for ( int i = 0; i < ui_json_size( src ); ++i )
    {
        const value_t& option = ui_json_element( src, i );
        const std::size_t n = out.tag == 5 ? out.list.size( ) : 0;
        value_t& dst = ui_json_at( out, n );
        for ( const std::string& key : ui_json_keys( option ) )
        {
            if ( key == "Choices" )
                continue;
            const value_t copy = ui_json_find( option, key );
            ui_json_child_s( dst, key ) = copy;
        }
        const value_t& choices = member( option, "Choices" );
        for ( int c = 0; c < ui_json_size( choices ); ++c )
        {
            const value_t& choice = ui_json_element( choices, c );
            if ( excluded( option, choice ) )
                continue;
            value_t& list = ui_json_child( dst, "Choices" );
            const std::size_t m = list.tag == 5 ? list.list.size( ) : 0;
            value_t& dc = ui_json_at( list, m );
            for ( const std::string& key : ui_json_keys( choice ) )
            {
                if ( key != "Options" )
                {
                    const value_t copy = ui_json_find( choice, key );
                    ui_json_child_s( dc, key ) = copy;
                    continue;
                }
                value_t nested;
                ui_config_copy_options( ui_json_find( choice, "Options" ), nested, excluded );
                ui_json_child_s( dc, "Options" ) = nested;
            }
        }
    }
}

// 0x44A4C0 (thiscall this, VuJsonContainer* options), in place: for each
// option (made a map): its "Choices" (inserted) and its current choice
// (0x44BD50 - the class's fallback may write into the root); each choice (made
// a map) merged with a copy of every member of References [its "Value"
// (inserted) text]; "ComboBoxID" = int (the combo count); a record { StringID =
// the option's "StringID" (inserted) text, per choice its "StringID" text and
// 0, +40..+42 = 0, +44 = the current choice, +48 = (float) that } pushed; then
// each choice's "Options" (inserted, 0x4FC090: the choice made a map) the same.
inline void ui_config_references( ui_config_entity_t& e, value_t& options, value_t& root, const ui_config_class_t& cls, const value_t* option_db )
{
    static const value_t null{ };
    const value_t& refs = option_db ? member( member( *option_db, "VuDBAsset" ), "References" ) : null;
    for ( int k = 0; k < ui_json_size( options ); ++k )
    {
        value_t& option = ui_json_make_map( ui_json_at( options, static_cast< std::size_t >( k ) ) );
        ui_json_child( option, "Choices" );
        const int current = ui_config_choice_index( option, root, cls );
        {
            value_t& choices = ui_json_child( option, "Choices" );
            for ( int j = 0; j < ui_json_size( choices ); ++j )
            {
                value_t& choice = ui_json_make_map( ui_json_at( choices, static_cast< std::size_t >( j ) ) );
                const std::string value = ui_json_text( ui_json_child( choice, "Value" ) );
                const value_t& merged = ui_json_find( refs, value );
                for ( const std::string& key : ui_json_keys( merged ) )
                {
                    const value_t copy = ui_json_find( merged, key );
                    ui_json_child_s( choice, key ) = copy;
                }
            }
        }
        ui_json_set_int( ui_json_child( option, "ComboBoxID" ), static_cast< int >( e.m_combos.size( ) ) );
        ui_config_combo_t combo;
        combo.m_string_id = ui_json_text( ui_json_child( option, "StringID" ) );
        value_t& choices = ui_json_child( option, "Choices" );
        for ( int j = 0; j < ui_json_size( choices ); ++j )
        {
            combo.m_choice_ids.push_back( ui_json_text( member( ui_json_at( choices, static_cast< std::size_t >( j ) ), "StringID" ) ) );
            combo.m_locked.push_back( 0 );
        }
        combo.m_index = current;
        combo.m_shown = static_cast< float >( current );
        e.m_combos.push_back( combo );
        for ( int i = 0; i < ui_json_size( ui_json_child( option, "Choices" ) ); ++i )
        {
            value_t& nested = ui_json_child( ui_json_at( ui_json_child( option, "Choices" ), static_cast< std::size_t >( i ) ), "Options" );
            ui_config_references( e, nested, root, cls, option_db );
        }
    }
}

// 0x449AD0 (thiscall this, bool announce): the option vector emptied, the walk
// over the document, the combo state, and when `announce` slot 12 and the
// info string.
inline bool ui_config_rewalk( ui_config_entity_t& e, ui_config_env_t& env, const value_t& options, const ui_config_class_t& cls,
                              const ui_profile_t& p, const ui_config_hooks_t& h, bool announce )
{
    e.m_options.clear( );
    ui_config_walk( e, env, options, e.m_options, cls, p );
    bool ok = ui_config_combo_state( e, *env.m_root, cls, p );
    if ( announce )
    {
        ok = ok && cls.changed( );
        ok = ok && ui_config_info( e, *env.m_root, cls, p, h );
    }
    return ok && !e.m_fault;
}

// Slot 4 0x448120: +0x2CC = 1; 0x449640: `doc` (+0x2A0) cleared; the entry = the
// OptionDB document's ["VuDBAsset"] [entry name] (lookups; `option_db` null =
// the property holds no asset: the null entry); the copy with the class's slot
// 21 (`excluded`), the references, then 0x449AD0 with (+0x14 & 4).
inline bool ui_config_build( ui_config_entity_t& e, ui_config_env_t& env, value_t& doc, const value_t* option_db, const std::string& entry,
                             const std::function< bool( const value_t&, const value_t& ) >& excluded, std::uint32_t flags,
                             const ui_config_class_t& cls, const ui_profile_t& p, const ui_config_hooks_t& h )
{
    e.m_refresh = true;
    ui_json_clear( doc );
    static const value_t null{ };
    const value_t& source = option_db ? ui_json_find( member( *option_db, "VuDBAsset" ), entry ) : null;
    ui_config_copy_options( source, doc, excluded );
    ui_config_references( e, doc, *env.m_root, cls, option_db );
    const bool ok = ui_config_rewalk( e, env, doc, cls, p, h, ( flags & 4 ) != 0 );
    e.m_refresh = false;
    return ok;
}

// VuDBAsset "AssetDB" package tests (thiscall gameutil, const VuString&):
// 0x4D3AA0 over TrackDB (gameutil +0x10), 0x4D3B80 over BoatDB (+8): the
// entry ["VuDBAsset"] [key]; null -> false; its "AssetDB" text empty -> true;
// else the asset factory 0x759004 slot +0x90 on that text (HOOK `package`).
inline bool ui_db_package_ok( const value_t* db, const std::string& key, const std::function< bool( const std::string& ) >& package )
{
    static const value_t null{ };
    const value_t& entry = db ? ui_json_find( member( *db, "VuDBAsset" ), key ) : null;
    if ( entry.tag == 0 )
        return false;
    const std::string& asset = ui_json_text( member( entry, "AssetDB" ) );
    if ( asset.empty( ) )
        return true;
    return package( asset );
}

// VuGameConfigEntity slot 21 (+84) 0x45DAD0 (stdcall option, choice): a
// choice of the option named exactly "Track" is excluded unless its track
// passes 0x4D3AA0.
inline bool ui_game_config_excluded( const value_t& option, const value_t& choice, const value_t* track_db,
                                     const std::function< bool( const std::string& ) >& package )
{
    const std::string& name = ui_json_text( member( option, "Name" ) );
    const std::string& value = ui_json_text( member( choice, "Value" ) );
    return name == "Track" && !ui_db_package_ok( track_db, value, package );
}

// VuGameConfigEntity's slot 4 (0x448120 -> 0x449640) into `doc` / the combos
// (`track_db` = gameutil +0x10's document for 0x45DAD0). Its slot 12 / 23 are
// reached only with the entity flag +0x14 & 4 - not recreated here: false.
inline bool ui_game_config_build( ui_config_entity_t& e, ui_config_env_t& env, value_t& doc, const value_t* option_db, const std::string& entry,
                                  const value_t* track_db, std::uint32_t flags, const ui_profile_t& p, const ui_config_hooks_t& h )
{
    if ( ( flags & 4 ) != 0 )
        return false;
    const ui_config_class_t cls = ui_game_config_class( e, *env.m_root, p );
    auto excluded = [ track_db, &p ]( const value_t& option, const value_t& choice ) { return ui_game_config_excluded( option, choice, track_db, p.package ); };
    return ui_config_build( e, env, doc, option_db, entry, excluded, flags, cls, p, h );
}
// ---------------------------------------------------------------------------
// VuGameConfigEntity (ctor 0x45CC80, vtable 0x73D1DC): slot 12 0x45EE20 and
// slot 6 0x45D050. Fields: +0x2E0 its 3D draw object (+0x1E: added to the 3D
// draw manager), +0x324 the trophy-shown byte.
struct ui_game_config_t2
{
    bool m_drawn = false;   // +0x2E0 -> +0x1E
    bool m_trophy = false;  // +0x324
    bool m_info_reached = false;  // slot 23 0x45DBF0 was asked for a text
};

// 0x45F710 (thiscall this): the first option of the vector whose current
// choice has a non-empty "PosterModel" text; else "".
inline std::string ui_game_config_poster( const ui_config_entity_t& e, const value_t& root, const ui_config_class_t& cls )
{
    for ( const value_t* option : e.m_options )
    {
        const std::string& poster = ui_json_text( member( ui_config_current_choice( *option, root, cls ), "PosterModel" ) );
        if ( !poster.empty( ) )
            return poster;
    }
    return "";
}

// Slot 12 (+48) 0x45EE20: for each option of the vector "OnGameConfig" (0x45F5C0:
// its Name, its current choice's Value); "OnChangePoster" (the poster
// 0x45F710, bool = some option locked: 0x449050 false); then the best placing:
// GameConfig.Mode "Championship" (exactly) -> progress +88 (GameConfig.
// Championship's text), else progress +48 (Mode, Track, Course when the mode
// is "Slalom" else "None"); a placing under 3 (signed) shows the trophy
// (+0x324 = 1, the model of table 0x7357CC: UI/Trophy_Gold / _Silver /
// _Bronze - a negative one reads before the table in retail: false), else
// +0x324 = 0.
inline bool ui_game_config_changed( const ui_config_entity_t& e, ui_game_config_t2& g, const value_t& root, const ui_config_class_t& cls,
                                    const ui_profile_t& p, const ui_config_hooks_t& h )
{
    for ( const value_t* option : e.m_options )
    {
        const std::string value = ui_json_text( member( ui_config_current_choice( *option, root, cls ), "Value" ) );
        ui_params_t params;
        params.add_string( ui_json_text( member( *option, "Name" ) ).c_str( ) );
        params.add_string( value.c_str( ) );
        ui_broadcast( h, "OnGameConfig", params );
    }
    ui_params_t poster;
    poster.add_string( ui_game_config_poster( e, root, cls ).c_str( ) );
    ui_params_add_bool( poster, !ui_config_all_unlocked( e, root, cls, p ) );
    ui_broadcast( h, "OnChangePoster", poster );
    const value_t& config = member( root, "GameConfig" );
    const std::string& mode = ui_json_text( member( config, "Mode" ) );
    int rank = 0;
    if ( mode == "Championship" )
        rank = p.champ_rank( ui_json_text( member( config, "Championship" ) ) );
    else
    {
        const std::string& track = ui_json_text( member( config, "Track" ) );
        const std::string& course = ui_json_text( member( config, "Course" ) );
        rank = p.race_rank( mode, track, mode == "Slalom" ? course : std::string( "None" ) );
    }
    if ( rank >= 3 )
    {
        g.m_trophy = false;
        return true;
    }
    if ( rank < 0 )
        return false;
    static const char* const trophies[ ] = { "UI/Trophy_Gold", "UI/Trophy_Silver", "UI/Trophy_Bronze" };
    g.m_trophy = true;
    h.trophy( trophies[ rank ] );
    return true;
}

// VuGameConfigEntity slot 23 (+92) 0x45DBF0 (stdcall VuString* out): the
// locked option's info text from root GameConfig's Mode / Track / Course /
// Championship texts (lookups; tag 4 else ""), each compared whole (0x404990
// and the inline 12-byte compare + length test):
//   Mode "Championship" -> "Unlock_" + Championship + "_Info"
//   Mode "Slalom"       -> "Unlock_" + Mode + "_" + Course + "_" + Track + "_Info"
//   Mode "RubberDucky"  -> "Unlock_Race_" + Track + "_Info"
//   any other           -> "Unlock_" + Mode + "_" + Track + "_Info"
// (std::string concatenations 0x44F310 / 0x4366B0 / 0x436700: whole texts.)
inline void ui_game_config_info_text( const value_t& root, std::string& out )
{
    const value_t& config = member( root, "GameConfig" );
    const std::string& mode = ui_json_text( member( config, "Mode" ) );
    const std::string& track = ui_json_text( member( config, "Track" ) );
    const std::string& course = ui_json_text( member( config, "Course" ) );
    const std::string& championship = ui_json_text( member( config, "Championship" ) );
    if ( mode == "Championship" )
        out = "Unlock_" + championship + "_Info";
    else if ( mode == "Slalom" )
        out = "Unlock_" + mode + "_" + course + "_" + track + "_Info";
    else if ( mode == "RubberDucky" )
        out = "Unlock_Race_" + track + "_Info";
    else
        out = "Unlock_" + mode + "_" + track + "_Info";
}

// VuGameConfigEntity's whole class for the walks (slot 12 bound to `g`; slot
// 23 0x45DBF0 above; slot 20 0x4471A0 = false; slot 17 0x45D720 is reached
// only from 0x45E300, the next_choice HOOK).
inline void ui_game_config_class_full( ui_config_class_t& c, ui_config_entity_t& e, ui_game_config_t2& g, value_t& root, const ui_profile_t& p,
                                       const ui_config_hooks_t& h )
{
    c = ui_game_config_class( e, root, p );
    c.unavailable = []( const std::string&, const std::string& ) { return false; };
    c.info = [ &g, &root ]( std::string& out )
    {
        g.m_info_reached = true;
        ui_game_config_info_text( root, out );
    };
    c.changed = [ &c, &e, &g, &root, &p, &h ]( ) { return ui_game_config_changed( e, g, root, c, p, h ); };
}

// Slot 6 (onGameInitialize) 0x45D050: the refresh 0x448140, then the draw
// object added to the 3D draw manager once (HOOK).
inline bool ui_game_config_initialize( ui_config_entity_t& e, ui_game_config_t2& g, ui_config_env_t& env, const value_t& options, const ui_profile_t& p,
                                       const ui_config_hooks_t& h )
{
    ui_config_class_t cls;
    ui_game_config_class_full( cls, e, g, *env.m_root, p, h );
    const bool ok = ui_config_refresh( e, env, options, cls, p, h );
    if ( !g.m_drawn )
    {
        h.draw_add( );
        g.m_drawn = true;
    }
    return ok;
}