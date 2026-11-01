#pragma once

#include <workspace/core/assets/archive.hxx>
#include <workspace/core/assets/reader.hxx>
#include <workspace/core/assets/tree.hxx>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

// The asset package database (boat batch 25, the asset loaders): what createAsset's miss path (factory.hxx) walks.
// A package (0xA0 bytes, ctor 0x4ECC90, dtor 0x4ED930) is one <name>.apf: +0 its name, +0x18 the pack's sku text,
// +0x30 / +0x34 the pack header's two words (+0x38 the LicenseMask, which only the loose-file loader 0x4EFBB0 sets),
// +0x3C the asset map type -> name -> entry, +0x44 the pack file (archive.hxx open_pack / read_asset). Each entry is
// the AssetData document's container for that asset plus a used byte (+0x10 of the value: set by createAsset's hit).
// Recreated here: the AssetData filter 0x55F090 (with its variable store 0x55EFE0 / 0x55FA10 / 0x55F800, the parse
// 0x55F670 and the variable lookup 0x55F520), the per-type load 0x4F1DC0, the sku pick 0x4F1CF0 and the language
// pick 0x55FC50. The package load 0x4F0090 itself, the patches and the pack read + load are the factory's.

// The filter evaluator 0x4F1DC0 builds on its stack: +0 the variables (48-byte { name, value } records), +12 the
// result byte, +16 the error text, +40 the token cursor. The tokens (28-byte { type, text }, push_back 0x4F27F0) are
// 0x55F090's own local vector.
class c_asset_filter
{
public:
    // 0x55EFE0 ( name, value ) = 0x55FA10 + 0x55F800: push_back, no replace (the lookup takes the first).
    void set( const std::string& name, const std::string& value )
    {
        m_variables.emplace_back( name, value );
    }

    // 0x55F090: the error text cleared, the text tokenized, the end token pushed, the result parsed (0x55F670) into
    // the +12 byte; a cursor not left on the end token appends "missing parenthesis?". Answers the error text being
    // empty. A character the tokenizer does not know appends "Expression parsing error : " and the rest of the text
    // and answers false at once, the result byte untouched. Spaces (0x20 only) separate tokens; "==" 1, "!=" 2, "&&" 3,
    // "||" 4, '(' 5, ')' 6, '!' 7, ',' 8, a "StartsWith" prefix 9 (strncmp of ten characters: "StartsWithX" is 9 then
    // the identifier X), an isalnum run 0 (the "C" locale; bytes >= 0x80 reach the CRT's table unchecked: not modelled).
    bool evaluate( const char* text )
    {
        m_error.clear( );
        m_tokens.clear( );
        const char* p = text;
        while ( *p != 0 )
        {
            const char c = *p;
            if ( c == ' ' )
            {
                ++p;
                continue;
            }
            if ( c == '=' && p[ 1 ] == '=' )
                push( 1, p, 2 );
            else if ( c == '!' && p[ 1 ] == '=' )
                push( 2, p, 2 );
            else if ( c == '&' && p[ 1 ] == '&' )
                push( 3, p, 2 );
            else if ( c == '|' && p[ 1 ] == '|' )
                push( 4, p, 2 );
            else if ( c == '(' )
                push( 5, p, 1 );
            else if ( c == ')' )
                push( 6, p, 1 );
            else if ( c == '!' )
                push( 7, p, 1 );
            else if ( c == ',' )
                push( 8, p, 1 );
            else if ( std::strncmp( p, "StartsWith", 10 ) == 0 )
                push( 9, p, 10 );
            else if ( alnum( c ) )
            {
                token_t t{ 0, {} };
                do
                    t.text.push_back( *p++ );
                while ( alnum( *p ) );
                m_tokens.push_back( std::move( t ) );
            }
            else
            {
                m_error += "Expression parsing error : ";
                m_error += p;
                return false;
            }
        }
        m_tokens.push_back( { 10, {} } );
        m_cursor = 0;
        m_result = parse( );
        if ( m_tokens[ m_cursor ].type != 10 )
            m_error += "Expression parsing error, missing parenthesis?";
        return m_error.empty( );
    }

    bool result( ) const
    {
        return m_result;
    }

    const std::string& error( ) const
    {
        return m_error;
    }

private:
    struct token_t
    {
        int type;
        std::string text;
    };

    void push( int type, const char*& p, int length )
    {
        m_tokens.push_back( { type, {} } );
        p += length;
    }

    // isalnum's truth in the "C" locale (the call sites test zero / non-zero).
    static bool alnum( char c )
    {
        return ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' );
    }

    // 0x55F520: the first variable of the name, else a static empty string.
    const std::string& variable( const std::string& name ) const
    {
        static const std::string empty;
        for ( const auto& v : m_variables )
        {
            if ( v.first == name )
                return v.second;
        }
        return empty;
    }

    // 0x55F670, recursive descent on the cursor. A term: identifier == / != identifier (the variable against the
    // second's text, three tokens), '(' expression ')', "!(" expression ')' (negated), StartsWith ( identifier ,
    // identifier ) (strncmp of the variable against the second identifier, its length - six tokens); anything else is
    // false and consumes nothing. A missing ')' appends "Expression missing closing parenthesis.\n" and answers false.
    // Then "&&" / "||" take the rest of the expression as their right side (right-associative, both sides always
    // evaluated): && answers false when the right side is false, else the left; || true when the right side is true,
    // else the left. The look-aheads never pass the end token: each is read only after the one before it matched.
    bool parse( )
    {
        bool value = false;
        const token_t* t = &m_tokens[ m_cursor ];
        bool term = false;
        if ( t[ 0 ].type == 0 && ( t[ 1 ].type == 1 || t[ 1 ].type == 2 ) && t[ 2 ].type == 0 )
        {
            const bool equal = variable( t[ 0 ].text ) == t[ 2 ].text;
            value = t[ 1 ].type == 1 ? equal : !equal;
            m_cursor += 3;
            term = true;
        }
        if ( !term && t[ 0 ].type == 5 )
        {
            m_cursor += 1;
            value = parse( );
            if ( m_tokens[ m_cursor ].type != 6 )
            {
                m_error += "Expression missing closing parenthesis.\n";
                return false;
            }
            m_cursor += 1;
            term = true;
        }
        if ( !term && t[ 0 ].type == 7 && t[ 1 ].type == 5 )
        {
            m_cursor += 2;
            value = !parse( );
            if ( m_tokens[ m_cursor ].type != 6 )
            {
                m_error += "Expression missing closing parenthesis.\n";
                return false;
            }
            m_cursor += 1;
            term = true;
        }
        if ( !term && t[ 0 ].type == 9 && t[ 1 ].type == 5 && t[ 2 ].type == 0 && t[ 3 ].type == 8 && t[ 4 ].type == 0
             && t[ 5 ].type == 6 )
        {
            value = std::strncmp( variable( t[ 2 ].text ).c_str( ), t[ 4 ].text.c_str( ), t[ 4 ].text.size( ) ) == 0;
            m_cursor += 6;
        }
        const int next = m_tokens[ m_cursor ].type;
        if ( next == 3 )
        {
            m_cursor += 1;
            if ( !parse( ) )
                return false;
            return value;
        }
        if ( next == 4 )
        {
            m_cursor += 1;
            if ( parse( ) )
                return true;
            return value;
        }
        return value;
    }

    std::vector< std::pair< std::string, std::string > > m_variables;
    bool m_result = false;
    std::string m_error;
    std::vector< token_t > m_tokens;
    std::size_t m_cursor = 0;
};

// One package's asset entry: the AssetData container (copied, 0x4FB9B0) and the used byte.
struct asset_entry_t
{
    value_t data;
    bool used = false;
};

struct asset_package_t
{
    std::string name;
    std::string sku;
    std::uint32_t words[ 3 ]{ 0xFFFFFFFFu, 0xFFFFFFFFu, 0u };
    // +0x3C. std::map< VuString, std::map< VuString, entry > > in retail too (find 0x4F22B0, operator[] 0x4F2100 /
    // 0x4F28D0): the same order.
    std::map< std::string, std::map< std::string, asset_entry_t > > assets;
    c_archive pack;

    // 0x4F0550's / createAsset's lookup: type, then name; null when either is missing.
    asset_entry_t* entry( const std::string& type, const std::string& name )
    {
        auto t = assets.find( type );
        if ( t == assets.end( ) )
            return nullptr;
        auto n = t->second.find( name );
        return n == t->second.end( ) ? nullptr : &n->second;
    }
};

// The members of an object container in std::map order (retail's container map), as 0x4FC000 walks them by index.
inline std::vector< const std::pair< std::string, value_t >* > asset_sorted_members( const value_t& v )
{
    std::vector< const std::pair< std::string, value_t >* > out;
    if ( v.tag != 6 )
        return out;
    for ( const auto& m : v.object )
        out.push_back( &m );
    std::sort( out.begin( ), out.end( ), []( const auto* a, const auto* b ) { return a->first < b->first; } );
    return out;
}

// 0x4F1DC0, thiscall on the package ( type, container of that type's assets ). The type's name map is made (0x4F2100
// operator[]) whatever follows. Each member, in key order: an entry whose "Filter" is not text (4) goes in (0x4F28D0
// operator[] + 0x4FB9B0 copy: a name already there is overwritten, its used byte kept); with a Filter text it goes in
// unfiltered when the package's sku is exactly "Dev", else the evaluator with "sku" = the package's sku and
// "platform" = "Metro" decides - an expression error answers false (the package is dropped), a false result skips
// the entry.
inline bool asset_package_load_type( asset_package_t& package, const std::string& type, const value_t& container )
{
    auto& names = package.assets[ type ];
    for ( const auto* member : asset_sorted_members( container ) )
    {
        const value_t& entry = member->second;
        const value_t& filter = ::member( entry, "Filter" );
        bool take = true;
        if ( filter.tag == 4 && package.sku != "Dev" )
        {
            c_asset_filter evaluator;
            evaluator.set( "sku", package.sku );
            evaluator.set( "platform", "Metro" );
            if ( !evaluator.evaluate( filter.text.c_str( ) ) )
                return false;
            take = evaluator.result( );
        }
        if ( take )
            names[ member->first ].data = entry;
    }
    return true;
}

// The container's member of a key, or null (0x4F22B0 on a map container; anything else has none).
inline const value_t* asset_member_of( const value_t& v, const std::string& key )
{
    if ( v.tag != 6 )
        return nullptr;
    for ( const auto& m : v.object )
    {
        if ( m.first == key )
            return &m.second;
    }
    return nullptr;
}

// 0x4F1CF0 ( platform, sku, entry ), stdcall: the entry, or its [platform] member when it is a map holding one; then
// that one's ["Skus"][sku] when "Skus" is a map holding the sku, else that one.
inline const value_t& asset_pick_sku( const std::string& platform, const std::string& sku, const value_t& entry )
{
    const value_t* v = &entry;
    if ( const value_t* p = asset_member_of( entry, platform ) )
        v = p;
    if ( const value_t* skus = asset_member_of( ::member( *v, "Skus" ), sku ) )
        return *skus;
    return *v;
}

// 0x55FC50 ( info, language ), thiscall on the asset: +0x38 = a copy of the info, +0x20 = the language; then the
// language is cleared when info["Langs"][language] is missing or null (tag 0).
inline void asset_pick_language( value_t& info_out, std::string& lang_out, const value_t& info, const std::string& language )
{
    info_out = info;
    lang_out = language.c_str( );
    const value_t* l = asset_member_of( ::member( info, "Langs" ), lang_out );
    if ( l == nullptr || l->tag == 0 )
        lang_out.clear( );
}
