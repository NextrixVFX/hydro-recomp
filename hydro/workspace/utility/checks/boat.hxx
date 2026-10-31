#pragma once

#include <workspace/core/boat/sections.hxx>

// The boat area's oracle-backed checks (one file per parallel agent; included by check.hxx after every
// shared check, so anything above is usable here). verify_boat_all runs them all.

// The BoatDB section readers (boat/sections.hxx) against retail's own runs
// (oracle case boat_sections): 0x409AF0 / 0x417480 / 0x4205A0 on blocks filled
// 0xCD (an empty std::string at each string field), with BoatDB's Default then
// each boat's section, and a hand-built document's sections (every key and
// tag; then the tags the readers reject) under each driver class. Every byte
// outside the string fields, and every string field's text.
inline bool verify_boat_sections( )
{
    auto document = []( const unsigned* words )
    {
        const unsigned char* raw = reinterpret_cast< const unsigned char* >( words + 1 );
        return c_reader( std::vector< std::uint8_t >( raw, raw + words[ 0 ] ) ).parse( );
    };
    const value_t docs[ 2 ] = { document( g_golden_boat_sections_boatdb ), document( g_golden_boat_sections_synthetic ) };
    auto text = []( const unsigned*& s )
    {
        const unsigned length = *s++;
        std::string out( reinterpret_cast< const char* >( s ), length );
        s += ( length + 3 ) / 4;
        return out;
    };
    int failed = 0, bytes = 0, strings_compared = 0;
    for ( std::size_t index = 0; index < g_golden_boat_sections_count; ++index )
    {
        const unsigned* s = g_golden_boat_sections_words + g_golden_boat_sections_offsets[ index ];
        const unsigned reader = *s++;
        const value_t& doc = docs[ *s++ ];
        const boat_driver_t driver = boat_driver_of( *s++ );
        std::vector< std::vector< std::string > > paths( *s++ );
        for ( auto& path : paths )
        {
            path.resize( *s++ );
            for ( auto& key : path )
                key = text( s );
        }
        const unsigned size = *s++;
        std::vector< std::uint8_t > image( size ), want( size );
        std::memcpy( image.data( ), s, size );
        s += ( size + 3 ) / 4;
        std::memcpy( want.data( ), s, size );
        s += ( size + 3 ) / 4;
        std::vector< std::pair< int, std::string > > want_strings( *s++ );
        boat_strings_t strings;
        for ( auto& w : want_strings )
        {
            w.first = static_cast< int >( *s++ );
            w.second = text( s );
            strings[ w.first ] = "";
        }
        for ( const auto& path : paths )
        {
            const value_t* section = &doc;
            for ( const auto& key : path )
                section = &member( *section, key.c_str( ) );
            if ( reader == 0 )
                boat_read_camera( image.data( ), *section );
            else if ( reader == 1 )
                boat_read_hull( image.data( ), strings, *section, driver );
            else
                boat_read_default( image.data( ), strings, *section, driver );
        }
        int bad = -1;
        for ( unsigned at = 0; at < size && bad < 0; ++at )
        {
            bool in_string = false;
            for ( const auto& w : want_strings )
                in_string = in_string || ( static_cast< int >( at ) >= w.first && static_cast< int >( at ) < w.first + 24 );
            if ( !in_string && image[ at ] != want[ at ] )
                bad = static_cast< int >( at );
            bytes += in_string ? 0 : 1;
        }
        for ( const auto& w : want_strings )
        {
            ++strings_compared;
            if ( strings[ w.first ] != w.second && bad < 0 )
                bad = 0x10000 + w.first;
        }
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat section reader %u vector %d differs from retail at byte 0x%X (0x1xxxx: that string field)",
                       reader, static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "BoatDB section readers 0x409AF0 / 0x417480 / 0x4205A0 match retail: %d vectors, %d bytes, %d strings",
                     static_cast< int >( g_golden_boat_sections_count ), bytes, strings_compared );
    else
        LOG_ERROR( "BoatDB section readers FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_sections_count ) );
    return failed == 0;
}

inline bool verify_boat_all( )
{
    bool ok = true;
    ok = verify_boat_sections( ) && ok;
    return ok;
}
