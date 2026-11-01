#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/ui/config.hxx>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

// VuStringDBImpl (vtable 0x74D61C; ctor 0x53B5A0), read from the disassembly and
// verified against retail (case string_db, hydro checks/session.hxx
// verify_string_db). Layout: +4 the languages (std::vector<std::string>, slot 5
// 0x53B700: the factory's getAssetNames("VuStringAsset") 0x53DFA0), +0x10 the
// string map (hash -> VuString; +0x14 its count), +0x30 the language
// (VuString), +0x48 the CJK byte. The globals it writes: g 0x7359FC (the word-wrap
// flags; image value 1) and the byte g 0x73579A (image value 1; the loader
// stores 0 - who reads it is unread).
//
// NOT here: slot 5 0x53B700's language source - g_vumetrosys 0x759000 vt+0x48
// (the PLATFORM's language, a hook) unless the dev config g 0x75901C answers
// "Language" (compiled out of retail: its getParam answers the static null) -
// the caller hands the language in.
struct string_db_t
{
    std::vector< std::string > languages;           // +4
    std::map< std::uint32_t, std::string > strings;   // +0x10 (a hash map: its order is never observable here)
    std::string language;                           // +0x30
    std::uint8_t cjk = 0;                           // +0x48
    std::uint32_t wrap_flags = 1;                   // g 0x7359FC
    std::uint8_t flag_73579a = 1;                   // g 0x73579A
};

// The VuStringAsset of a language: the asset factory's vt+0x1C create("VuStringAsset" [0x736AF8], name) through
// 0x53E040 - its load slot 5 0x5C1270 is VuDBAsset's (pfxload.hxx pfx_asset_document), the document its +0x58.
// Null: no such asset (retail would read +0x58 of null - unreached, the language is checked first).
using string_db_asset_fn = std::function< const value_t*( const std::string& language ) >;

// FNV-1a over the key's bytes (basis 0x811C9DC5, prime 0x1000193; movzx: unsigned bytes).
inline std::uint32_t string_db_hash( const std::string& key )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( const char c : key )
        h = ( h ^ static_cast< std::uint8_t >( c ) ) * 0x1000193u;
    return h;
}

// 0x43E030 (thiscall string; ptr, pos, count; ret 0xC): the first occurrence of
// `count` bytes - the pos argument is never read, the search always starts at 0;
// count 0 answers 0, a count above the size (or an empty string) -1.
inline std::size_t string_db_find( const std::string& s, const std::string& key )
{
    if ( key.empty( ) )
        return 0u;
    if ( s.empty( ) || key.size( ) > s.size( ) )
        return std::string::npos;
    return s.find( key );
}

// The loader 0x53D210 (thiscall db, language; ret 4), whole: g 0x73579A = 0; the
// asset's document; "Options" / "CharCodes" / "Strings" (0x4FBDF0 each); with the map
// empty (+0x14 == 0) Options' WW_PROHIBITION / WW_NOHANGULWRAP - each only as a bool
// (tag 3) - set / clear bit 0 / bit 2 of g 0x7359FC; for every Strings key in the
// map's order (0x4FBE90 / 0x4FC000): map[FNV-1a(key)] (find-or-insert 0x53D500) =
// the value when a string (tag 4), else the static empty 0x819950 (0x401DB0 assign);
// then for every CharCodes key in order with its string (else empty), in every map
// value: while find(key) (0x43E030) hits, replace(pos, key length, value, 0, npos)
// (0x407450) - a value that contains its own key, or an empty key, never ends, as in
// retail; the asset released (factory vt+0x24). Returns true (al = 1); false here only
// when the asset is missing (retail faults).
inline bool string_db_load( string_db_t& db, const string_db_asset_fn& asset )
{
    db.flag_73579a = 0;
    const value_t* doc = asset ? asset( db.language ) : nullptr;
    if ( !doc )
        return false;
    const value_t& options = member( *doc, "Options" );
    const value_t& codes = member( *doc, "CharCodes" );
    const value_t& strings = member( *doc, "Strings" );
    if ( db.strings.empty( ) )
    {
        const value_t& p = member( options, "WW_PROHIBITION" );
        if ( p.tag == 3 )
            db.wrap_flags = static_cast< std::uint8_t >( p.integer ) ? ( db.wrap_flags | 1u ) : ( db.wrap_flags & ~1u );
        const value_t& h = member( options, "WW_NOHANGULWRAP" );
        if ( h.tag == 3 )
            db.wrap_flags = static_cast< std::uint8_t >( h.integer ) ? ( db.wrap_flags | 4u ) : ( db.wrap_flags & ~4u );
    }
    for ( const std::string& key : ui_json_keys( strings ) )
        db.strings[ string_db_hash( key ) ] = ui_json_text( member( strings, key.c_str( ) ) );
    for ( const std::string& key : ui_json_keys( codes ) )
    {
        const std::string& value = ui_json_text( member( codes, key.c_str( ) ) );
        for ( auto& entry : db.strings )
            for ( std::size_t at = string_db_find( entry.second, key ); at != std::string::npos; at = string_db_find( entry.second, key ) )
                entry.second.replace( at, key.size( ), value );
    }
    return true;
}

// Slot 15 0x53BB40: the map cleared (0x53DA30) and reset (0x4F35A0: 0x10 buckets,
// +0x24 = 7, +0x28 = 8 - its internals), then the loader on the language.
inline bool string_db_reload( string_db_t& db, const string_db_asset_fn& asset )
{
    db.strings.clear( );
    return string_db_load( db, asset );
}

// Slot 11 0x53B9C0 (setLanguage): the same language (VuString compare) -> true and
// nothing else; else +0x30 = it, unless it is one of the languages (+4, std::find)
// "en-us" (0x6CE4B0); +0x48 = it is "ja-jp" / "ko-kr" / "zh-tw" / "zh-cn"; then slot 15.
inline bool string_db_set_language( string_db_t& db, const std::string& language, const string_db_asset_fn& asset )
{
    if ( language == db.language )
        return true;
    db.language = language;
    if ( std::find( db.languages.begin( ), db.languages.end( ), db.language ) == db.languages.end( ) )
        db.language = "en-us";
    db.cjk = 0;
    for ( const char* c : { "ja-jp", "ko-kr", "zh-tw", "zh-cn" } )
        if ( db.language == c )
            db.cjk = 1;
    return string_db_reload( db, asset );
}

// Slot 6 0x53B830 (getString): FNV-1a of the key, the map's find (0x5389B0) -> the
// VuString; none -> slot 9 0x53B920, the function-static "MISSING_STRING" (0x6CE4A0).
inline const std::string& string_db_get( const string_db_t& db, const std::string& key )
{
    static const std::string missing = "MISSING_STRING";
    const auto found = db.strings.find( string_db_hash( key ) );
    return found != db.strings.end( ) ? found->second : missing;
}
