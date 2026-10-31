#pragma once

#include <workspace/core/boat/driver.hxx>
#include <workspace/core/boat/sections.hxx>

// The boat area's oracle-backed checks (one file per parallel agent; included by check.hxx after every
// shared check, so anything above is usable here). verify_boat_all runs them all.

// The BoatDB section readers (boat/sections.hxx) against retail's own runs
// (oracle case boat_sections): 0x409AF0 / 0x417480 / 0x4205A0 / 0x410360 on blocks filled
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
            else if ( reader == 2 )
                boat_read_default( image.data( ), strings, *section, driver );
            else
                boat_read_animation( image.data( ), strings, *section, driver );
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
        LOG_SUCCESS( "BoatDB section readers 0x409AF0 / 0x417480 / 0x4205A0 / 0x410360 match retail: %d vectors, %d bytes, %d strings",
                     static_cast< int >( g_golden_boat_sections_count ), bytes, strings_compared );
    else
        LOG_ERROR( "BoatDB section readers FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_sections_count ) );
    return failed == 0;
}

// boat_load_engine / boat_read_engine (boat/sections.hxx) against retail's
// 0x413700 + 0x4140C0 on an engine object from its own ctor 0x413590 (oracle
// case boat_engine): every BoatDB boat (its Engine, then Default's) and the
// hand-built sections under each driver class. The object's bytes outside its
// pointer and string words, the power curve's floats (and its size and
// capacity, in the image), the drives' records (w lanes 3 / 7 are an unset
// stack word: skipped), the +0xF8 record count and the five strings. Bytes
// neither the ctor nor the load writes (they differ between retail's 0xCD and
// 0x5A fills) are skipped.
inline bool verify_boat_engine( )
{
    const unsigned* d = g_golden_boat_sections_boatdb;
    const unsigned char* raw = reinterpret_cast< const unsigned char* >( d + 1 );
    const value_t boatdb = c_reader( std::vector< std::uint8_t >( raw, raw + d[ 0 ] ) ).parse( );
    d = g_golden_boat_engine_synthetic;
    raw = reinterpret_cast< const unsigned char* >( d + 1 );
    const value_t synthetic = c_reader( std::vector< std::uint8_t >( raw, raw + d[ 0 ] ) ).parse( );
    auto text = []( const unsigned*& s )
    {
        const unsigned length = *s++;
        std::string out( reinterpret_cast< const char* >( s ), length );
        s += ( length + 3 ) / 4;
        return out;
    };
    // Pointer words (+0 the owner is the oracle's fake boat, 0x30003000) and the string fields.
    auto masked = []( int at )
    {
        return ( at >= 0x30 && at < 0x34 ) || ( at >= 0x3C && at < 0x48 ) || ( at >= 0xF8 && at < 0x104 ) ||
               ( at >= 0x4C && at < 0x4C + 5 * 24 );
    };
    int failed = 0, compared = 0;
    for ( std::size_t index = 0; index < g_golden_boat_engine_count; ++index )
    {
        const unsigned* s = g_golden_boat_engine_words + g_golden_boat_engine_offsets[ index ];
        const value_t& doc = *s++ == 0 ? boatdb : synthetic;
        const boat_driver_t driver = boat_driver_of( *s++ );
        const value_t* sections[ 2 ];
        for ( auto& section : sections )
        {
            section = &doc;
            const unsigned keys = *s++;
            for ( unsigned k = 0; k < keys; ++k )
                section = &member( *section, text( s ).c_str( ) );
        }
        const unsigned char* want = reinterpret_cast< const unsigned char* >( s );
        s += 0x104 / 4;
        const unsigned char* written = reinterpret_cast< const unsigned char* >( s );
        s += 0x104 / 4;
        std::vector< std::uint32_t > curve( *s++ );
        for ( auto& w : curve )
            w = *s++;
        std::vector< std::array< std::uint32_t, 8 > > drives( *s++ );
        for ( auto& r : drives )
            for ( auto& w : r )
                w = *s++;
        const unsigned records = *s++;
        std::vector< std::pair< int, std::string > > strings( *s++ );
        for ( auto& w : strings )
        {
            w.first = static_cast< int >( *s++ );
            w.second = text( s );
        }
        boat_engine_t e;
        boat_413590( e.object, 0x30003000u );
        boat_load_engine( e, *sections[ 0 ], *sections[ 1 ], driver, 0 );
        int bad = -1;
        for ( int at = 0; at < 0x104 && bad < 0; ++at )
            if ( written[ at ] && !masked( at ) && e.object.image[ at ] != want[ at ] )
                bad = at;
        if ( bad < 0 && e.curve != curve )
            bad = 0x1030;
        if ( bad < 0 && e.drives.size( ) != drives.size( ) )
            bad = 0x103C;
        for ( std::size_t k = 0; bad < 0 && k < drives.size( ); ++k )
            for ( int lane : { 0, 1, 2, 4, 5, 6 } )
                if ( e.drives[ k ][ lane ] != drives[ k ][ lane ] )
                    bad = 0x103C;
        if ( bad < 0 && e.records != records )
            bad = 0x10F8;
        for ( const auto& w : strings )
            if ( bad < 0 && e.strings[ w.first ] != w.second )
                bad = 0x10000 + w.first;
        ++compared;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat engine vector %d differs from retail at 0x%X (0x1xxxx: that container / string field)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuBoatDB_loadEngine 0x413700 / readEngineSection 0x4140C0 match retail: %d vectors (bytes, power curve, "
                     "drives, records, strings)", compared );
    else
        LOG_ERROR( "boat engine FAILED %d of %d vectors", failed, compared );
    return failed == 0;
}

// ai_driver_construct (boat/driver.hxx) against retail's AI-driver build in
// postLoad (oracle case boat_ai_driver): every object's size, every written
// byte (retail's 0xCD / 0x5A runs agree), every pointer by what it points at.
inline bool verify_boat_ai_driver( )
{
    const std::vector< image_object_t > g = ai_driver_construct( );
    const unsigned* s = g_golden_boat_ai_driver_words;
    const unsigned count = *s++;
    int failed = 0, bytes = 0;
    auto fail = [ &failed ]( const char* what, int object, int at )
    {
        if ( ++failed <= 4 )
            LOG_ERROR( "AI driver graph: %s at object %d, byte %d", what, object, at );
    };
    if ( count != g.size( ) )
    {
        LOG_ERROR( "AI driver graph: retail built %u objects, ours %d", count, static_cast< int >( g.size( ) ) );
        return false;
    }
    for ( unsigned k = 0; k < count; ++k )
    {
        const unsigned size = *s++;
        const unsigned char* want = reinterpret_cast< const unsigned char* >( s );
        s += ( size + 3 ) / 4;
        const unsigned char* mask = reinterpret_cast< const unsigned char* >( s );
        s += ( size + 3 ) / 4;
        std::vector< image_ref_t > refs( *s++ );
        for ( auto& r : refs )
        {
            r = { static_cast< int >( s[ 0 ] ), s[ 1 ] == 1 ? AI_DRIVER_REF_BOAT : static_cast< int >( s[ 2 ] ), static_cast< int >( s[ 3 ] ) };
            s += 4;
        }
        if ( size != g[ k ].image.size( ) )
        {
            fail( "size", static_cast< int >( k ), static_cast< int >( g[ k ].image.size( ) ) );
            continue;
        }
        for ( unsigned at = 0; at < size; ++at )
        {
            if ( !mask[ at ] )
                continue;
            ++bytes;
            if ( g[ k ].image[ at ] != want[ at ] )
            {
                fail( "byte", static_cast< int >( k ), static_cast< int >( at ) );
                break;
            }
        }
        std::vector< image_ref_t > ours = g[ k ].refs;
        auto by_slot = []( const image_ref_t& x, const image_ref_t& y ) { return x.slot < y.slot; };
        std::sort( ours.begin( ), ours.end( ), by_slot );
        std::sort( refs.begin( ), refs.end( ), by_slot );
        if ( ours != refs )
            fail( "pointer slots", static_cast< int >( k ), 0 );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "postLoad's AI driver (0x41D2E0 + VuAiDriver ctor 0x40B690) matches retail: %u objects, %d bytes, every pointer",
                     count, bytes );
    else
        LOG_ERROR( "AI driver graph FAILED %d checks", failed );
    return failed == 0;
}

inline bool verify_boat_all( )
{
    bool ok = true;
    ok = verify_boat_sections( ) && ok;
    ok = verify_boat_engine( ) && ok;
    ok = verify_boat_ai_driver( ) && ok;
    return ok;
}
