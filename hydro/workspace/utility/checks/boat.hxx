#pragma once

#include <workspace/core/collision/box.hxx>
#include <workspace/core/collision/cylinder.hxx>
#include <workspace/core/collision/sphere.hxx>
#include <workspace/core/boat/ai.hxx>
#include <workspace/core/boat/driver.hxx>
#include <workspace/core/boat/load.hxx>
#include <workspace/core/boat/sections.hxx>
#include <workspace/core/boat/state.hxx>
#include <workspace/core/boat/track.hxx>

#include <array>
#include <cstdio>

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

// ai_driver_load / driver_entity_load (boat/driver.hxx) against retail's
// 0x40B8A0 / 0x40EAC0 (oracle case boat_driver_load): every byte of the
// driver block outside its +0x60 string, and the string's text.
inline bool verify_boat_driver_load( )
{
    const unsigned* d = g_golden_boat_driver_load_synthetic;
    const unsigned char* raw = reinterpret_cast< const unsigned char* >( d + 1 );
    const value_t doc = c_reader( std::vector< std::uint8_t >( raw, raw + d[ 0 ] ) ).parse( );
    auto text = []( const unsigned*& s )
    {
        const unsigned length = *s++;
        std::string out( reinterpret_cast< const char* >( s ), length );
        s += ( length + 3 ) / 4;
        return out;
    };
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_boat_driver_load_count; ++index )
    {
        const unsigned* s = g_golden_boat_driver_load_words + g_golden_boat_driver_load_offsets[ index ];
        const unsigned address = *s++;
        const std::string name = text( s );
        std::uint8_t boat[ 0x290 ]{};
        std::memcpy( boat + 0x270, s, 16 );
        std::memcpy( boat + 0x280, s + 4, 16 );
        s += 8;
        std::uint8_t image[ 0xD4 ];
        std::memcpy( image, s, 0xD4 );
        s += 0xD4 / 4;
        const unsigned char* want = reinterpret_cast< const unsigned char* >( s );
        s += 0xD4 / 4;
        const std::string want_string = text( s );
        boat_strings_t strings{ { 0x60, "" } };
        if ( address == 0x40B8A0u )
            ai_driver_load( image, strings, member( doc, name.c_str( ) ), boat );
        else
            driver_entity_load( strings, member( doc, name.c_str( ) ) );
        int bad = -1;
        for ( int at = 0; at < 0xD4 && bad < 0; ++at )
            if ( ( at < 0x60 || at >= 0x78 ) && image[ at ] != want[ at ] )
                bad = at;
        if ( bad < 0 && strings[ 0x60 ] != want_string )
            bad = 0x10060;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "driver load 0x%X vector %d differs from retail at byte 0x%X", address, static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the drivers' load 0x40B8A0 / 0x40EAC0 match retail: %d vectors", static_cast< int >( g_golden_boat_driver_load_count ) );
    else
        LOG_ERROR( "driver load FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_driver_load_count ) );
    return failed == 0;
}

// boat_flash_timer / boat_beached_test (boat/state.hxx) against retail's
// 0x41FF70 / 0x420010 (oracle case boat_tick_state): every byte of the boat,
// body and proxy blocks after.
inline bool verify_boat_tick_state( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_boat_tick_state_count; ++index )
    {
        const unsigned* s = g_golden_boat_tick_state_words + g_golden_boat_tick_state_offsets[ index ];
        const unsigned function = *s++;
        float dt;
        std::memcpy( &dt, s++, 4 );
        std::vector< std::uint8_t > boat( 0x520 ), body( 0x300 ), proxy( 0x10 );
        std::memcpy( boat.data( ), s, 0x520 );
        std::memcpy( body.data( ), s + 0x520 / 4, 0x300 );
        std::memcpy( proxy.data( ), s + ( 0x520 + 0x300 ) / 4, 0x10 );
        s += ( 0x520 + 0x300 + 0x10 ) / 4;
        const unsigned char* want = reinterpret_cast< const unsigned char* >( s );
        std::uint32_t proxy_word;
        std::memcpy( &proxy_word, body.data( ) + 0xC8, 4 );
        if ( function == 0x41FF70u )
            boat_flash_timer( boat.data( ), body.data( ), proxy_word ? proxy.data( ) : nullptr, dt );
        else
            boat_beached_test( boat.data( ), body.data( ) );
        int bad = -1;
        for ( int at = 0; at < 0x520 && bad < 0; ++at )
            if ( boat[ at ] != want[ at ] )
                bad = at;
        for ( int at = 0; at < 0x300 && bad < 0; ++at )
            if ( body[ at ] != want[ 0x520 + at ] )
                bad = 0x1000 + at;
        for ( int at = 0; at < 0x10 && bad < 0; ++at )
            if ( proxy[ at ] != want[ 0x820 + at ] )
                bad = 0x2000 + at;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat tick state 0x%X vector %d differs from retail at 0x%X (0x1xxx body, 0x2xxx proxy)", function,
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the boat's tick state helpers 0x41FF70 / 0x420010 match retail: %d vectors", static_cast< int >( g_golden_boat_tick_state_count ) );
    else
        LOG_ERROR( "boat tick state FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_tick_state_count ) );
    return failed == 0;
}

// boat_jump (boat/state.hxx) against retail's 0x41FDA0 (oracle case
// boat_jump): every byte of the boat, body, hull and solver blocks after,
// and whether the jump sound ran.
inline bool verify_boat_jump( )
{
    const int sizes[ 4 ] = { 0x520, 0x300, 0x1F0, 0x104 };
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_boat_jump_count; ++index )
    {
        const unsigned* s = g_golden_boat_jump_words + g_golden_boat_jump_offsets[ index ];
        float dt;
        std::memcpy( &dt, s++, 4 );
        const unsigned sounds = *s++;
        std::vector< std::uint8_t > blocks[ 4 ];
        for ( int k = 0; k < 4; ++k )
        {
            blocks[ k ].assign( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + sizes[ k ] );
            s += sizes[ k ] / 4;
        }
        const bool sound = boat_jump( blocks[ 0 ].data( ), blocks[ 1 ].data( ), blocks[ 3 ].data( ), dt );
        int bad = sound == ( sounds != 0 ) && sounds <= 1 ? -1 : 0x7FFF;
        for ( int k = 0; k < 4 && bad < 0; ++k )
        {
            const unsigned char* want = reinterpret_cast< const unsigned char* >( s );
            for ( int at = 0; at < sizes[ k ] && bad < 0; ++at )
                if ( blocks[ k ][ at ] != want[ at ] )
                    bad = 0x1000 * k + at;
            s += sizes[ k ] / 4;
        }
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat jump vector %d differs from retail at 0x%X (block x 0x1000 + byte; 0x7FFF the sound)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the boat's jump 0x41FDA0 matches retail: %d vectors", static_cast< int >( g_golden_boat_jump_count ) );
    else
        LOG_ERROR( "boat jump FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_jump_count ) );
    return failed == 0;
}

// btBoxShape's virtuals (engine collision/box.hxx) against retail's 0x5480C0 /
// 0x548A00 / 0x548900 / 0x548840 / 0x5487D0 / 0x548740 / 0x548650 / 0x548580 /
// 0x548460 / 0x548370 / 0x5481A0 / 0x548250 (oracle case
// box_shape, one case for the class): the shape's 0x40 bytes after and every
// output word.
inline bool verify_box_shape( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_box_shape_count; ++index )
    {
        const unsigned* s = g_golden_box_shape_words + g_golden_box_shape_offsets[ index ];
        const unsigned function = *s++;
        std::uint8_t shape[ 0x40 ];
        std::memcpy( shape, s, 0x40 );
        s += 0x10;
        const unsigned in_bytes = *s++;
        std::vector< float > in( in_bytes / 4 );
        std::memcpy( in.data( ), s, in_bytes );
        s += in_bytes / 4;
        const unsigned char* want_shape = reinterpret_cast< const unsigned char* >( s );
        s += 0x10;
        const unsigned out_bytes = *s++;
        const unsigned char* want_out = reinterpret_cast< const unsigned char* >( s );
        std::vector< float > out( out_bytes / 4 + 8, 0.0f );
        std::memset( out.data( ), 0xCD, out.size( ) * 4 );   // the oracle's fill: what a function leaves alone
        const int arg = in_bytes == 4 ? *reinterpret_cast< const int* >( in.data( ) ) : 0;
        switch ( function )
        {
        case 0x548650u: box_preferred_direction( arg, out.data( ) ); break;
        case 0x548250u: box_plane_equation( shape, out.data( ), arg ); break;
        case 0x548370u: box_plane( shape, out.data( ), out.data( ) + 4, arg ); break;
        case 0x548460u: box_vertex( shape, arg, out.data( ) ); break;
        case 0x548580u: box_edge( shape, arg, out.data( ), out.data( ) + 4 ); break;
        case 0x5481A0u:
        {
            const bool inside = box_is_inside( shape, in.data( ), in[ 3 ] );
            reinterpret_cast< std::uint8_t* >( out.data( ) )[ 0 ] = inside ? 1 : 0;
            break;
        }
        case 0x5480C0u: box_local_inertia( shape, in[ 0 ], out.data( ) ); break;
        case 0x548A00u: box_set_local_scaling( shape, in.data( ) ); break;
        case 0x548900u: box_set_margin( shape, in[ 0 ] ); break;
        case 0x548840u: box_support( shape, in.data( ), out.data( ) ); break;
        case 0x5487D0u: box_support_without_margin( shape, in.data( ), out.data( ) ); break;
        default: box_support_batched( shape, in.data( ), out.data( ), static_cast< int >( in.size( ) / 4 ) ); break;
        }
        int bad = -1;
        for ( int at = 0; at < 0x40 && bad < 0; ++at )
            if ( shape[ at ] != want_shape[ at ] )
                bad = at;
        if ( bad < 0 && out_bytes && std::memcmp( out.data( ), want_out, out_bytes ) != 0 )
            bad = 0x100;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "btBoxShape 0x%X vector %d differs from retail at 0x%X (0x100: the output)", function,
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btBoxShape's 12 virtuals (0x5480C0 .. 0x548250) match retail: %d vectors",
                     static_cast< int >( g_golden_box_shape_count ) );
    else
        LOG_ERROR( "btBoxShape virtuals FAILED %d of %d vectors", failed, static_cast< int >( g_golden_box_shape_count ) );
    return failed == 0;
}

// The cylinders' virtuals (engine collision/cylinder.hxx) against retail's
// (oracle case cylinder_shape): the shape's 0x50 bytes after and every output
// word, except the w lanes 0x5AE950 and the batched forms copy from an
// unwritten stack word.
inline bool verify_cylinder_shape( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_cylinder_shape_count; ++index )
    {
        const unsigned* s = g_golden_cylinder_shape_words + g_golden_cylinder_shape_offsets[ index ];
        const unsigned function = *s++;
        std::uint8_t shape[ 0x50 ];
        std::memcpy( shape, s, 0x50 );
        s += 0x14;
        const unsigned in_bytes = *s++;
        std::vector< float > in( in_bytes / 4 );
        std::memcpy( in.data( ), s, in_bytes );
        s += in_bytes / 4;
        const unsigned char* want_shape = reinterpret_cast< const unsigned char* >( s );
        s += 0x14;
        const unsigned out_bytes = *s++;
        const unsigned char* want_out = reinterpret_cast< const unsigned char* >( s );
        std::vector< float > out( out_bytes / 4 + 4, 0.0f );
        std::memset( out.data( ), 0xCD, out.size( ) * 4 );
        std::int32_t up;
        std::memcpy( &up, shape + 0x40, 4 );
        const cylinder_axes_t axes = cylinder_axes( up );
        bool w_residue = false;
        switch ( function )
        {
        case 0x5AE2D0u: cylinder_local_inertia( shape, in[ 0 ], out.data( ) ); break;
        case 0x5AE950u: cylinder_support( shape, axes, in.data( ), out.data( ) ); w_residue = true; break;
        case 0x5AE560u: case 0x5AE430u: case 0x5AE4C0u: cylinder_support_without_margin( shape, axes, in.data( ), out.data( ) ); break;
        default:
            cylinder_support_batched( shape, axes, in.data( ), out.data( ), static_cast< int >( in.size( ) / 4 ) );
            w_residue = true;
            break;
        }
        int bad = -1;
        for ( int at = 0; at < 0x50 && bad < 0; ++at )
            if ( shape[ at ] != want_shape[ at ] )
                bad = at;
        const unsigned char* ours = reinterpret_cast< const unsigned char* >( out.data( ) );
        for ( unsigned at = 0; at < out_bytes && bad < 0; ++at )
            if ( !( w_residue && at % 16 >= 12 ) && ours[ at ] != want_out[ at ] )
                bad = 0x100 + static_cast< int >( at );
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "cylinder 0x%X vector %d differs from retail at 0x%X (0x1xx: output byte)", function,
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btCylinderShape / X / Z virtuals (0x5AE2D0, 0x5AE950, 0x5AE560 / 430 / 4C0, 0x5AE600 / 7A0 / 6D0) match retail: %d vectors",
                     static_cast< int >( g_golden_cylinder_shape_count ) );
    else
        LOG_ERROR( "cylinder virtuals FAILED %d of %d vectors", failed, static_cast< int >( g_golden_cylinder_shape_count ) );
    return failed == 0;
}

// btSphereShape's virtuals (engine collision/sphere.hxx) against
// retail's (oracle case sphere_shape): the shape after and every output word.
inline bool verify_sphere_shape( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_sphere_shape_count; ++index )
    {
        const unsigned* s = g_golden_sphere_shape_words + g_golden_sphere_shape_offsets[ index ];
        const unsigned function = *s++;
        std::uint8_t shape[ 0x40 ];
        std::memcpy( shape, s, 0x40 );
        s += 0x10;
        const unsigned in_bytes = *s++;
        std::vector< float > in( in_bytes / 4 + 1 );
        std::memcpy( in.data( ), s, in_bytes );
        s += in_bytes / 4;
        const unsigned char* want_shape = reinterpret_cast< const unsigned char* >( s );
        s += 0x10;
        const unsigned out_bytes = *s++;
        const unsigned char* want_out = reinterpret_cast< const unsigned char* >( s );
        std::vector< float > out( out_bytes / 4 + 4, 0.0f );
        std::memset( out.data( ), 0xCD, out.size( ) * 4 );
        switch ( function )
        {
        case 0x5535A0u: sphere_local_inertia( shape, in[ 0 ], out.data( ) ); break;
        case 0x553360u: sphere_support( shape, in.data( ), out.data( ) ); break;
        case 0x5532F0u: sphere_support_without_margin( out.data( ) ); break;
        default: sphere_support_batched( out.data( ), static_cast< int >( in_bytes / 16 ) ); break;
        }
        int bad = -1;
        for ( int at = 0; at < 0x40 && bad < 0; ++at )
            if ( shape[ at ] != want_shape[ at ] )
                bad = at;
        if ( bad < 0 && std::memcmp( out.data( ), want_out, out_bytes ) != 0 )
            bad = 0x100;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "sphere 0x%X vector %d differs from retail at 0x%X (0x100: the output)", function, static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btSphereShape's virtuals 0x5535A0 / 0x553360 / 0x5532F0 / 0x553320 match retail: %d vectors",
                     static_cast< int >( g_golden_sphere_shape_count ) );
    else
        LOG_ERROR( "sphere virtuals FAILED %d of %d vectors", failed, static_cast< int >( g_golden_sphere_shape_count ) );
    return failed == 0;
}

// compound_local_inertia / the compound's setMargin (engine collision/
// compound.hxx) against retail's 0x5275F0 / 0x527B00 on compounds retail
// built (oracle case compound_inertia): the margin word and the inertia.
inline bool verify_compound_inertia( )
{
    const unsigned* s = g_golden_compound_inertia_words;
    const unsigned count = *s++;
    auto f = []( unsigned w ) { float v; std::memcpy( &v, &w, 4 ); return v; };
    int failed = 0;
    for ( unsigned index = 0; index < count; ++index )
    {
        const bool has_child = *s++ != 0;
        convex_shape_t box{};
        box.type = 0;
        for ( int k = 0; k < 3; ++k )
        {
            box.local_scaling[ k ] = 1.0f;
            box.implicit[ k ] = f( s[ k ] );
        }
        box.margin = f( s[ 3 ] );
        s += 4;
        float transform[ 16 ];
        for ( auto& x : transform )
            x = f( *s++ );
        compound_shape_t c;
        c.margin = f( *s++ );   // setMargin 0x527B00
        const float mass = f( *s++ );
        const unsigned margin_after = *s++;
        std::uint32_t inertia[ 4 ] = { 0xCDCDCDCDu, 0xCDCDCDCDu, 0xCDCDCDCDu, 0xCDCDCDCDu };
        bool bad = std::memcmp( &c.margin, &margin_after, 4 ) != 0;
        if ( has_child && compound_add_child( c, transform, box ) != 1 )
            bad = true;
        compound_local_inertia( c, mass, reinterpret_cast< float* >( inertia ) );
        bad = bad || std::memcmp( inertia, s, 16 ) != 0;
        s += 4;
        if ( bad && ++failed <= 4 )
            LOG_ERROR( "compound inertia vector %u differs from retail", index );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "btCompoundShape calculateLocalInertia 0x5275F0 / setMargin 0x527B00 match retail: %u vectors", count );
    else
        LOG_ERROR( "compound inertia FAILED %d of %u vectors", failed, count );
    return failed == 0;
}

// boat_nose_probe (boat/state.hxx) against retail's 0x41F740 (oracle case
// boat_nose_probe; VuWater's lookup intercepted and answered): the six blocks
// after, what the lookup was handed, and whether the event fired.
inline bool verify_boat_nose_probe( )
{
    const int sizes[ 6 ] = { 0x520, 0x300, 0x1F0, 0x104, 0xC8, 0x104 };
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_boat_nose_probe_count; ++index )
    {
        const unsigned* s = g_golden_boat_nose_probe_words + g_golden_boat_nose_probe_offsets[ index ];
        float answer;
        std::memcpy( &answer, s++, 4 );
        const bool asked = *s++ != 0;
        const unsigned* point = s;
        const unsigned* callback = s + 3;
        s += 8;
        const bool event = *s++ != 0;
        std::vector< std::uint8_t > b[ 6 ];
        for ( int k = 0; k < 6; ++k )
        {
            b[ k ].assign( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + sizes[ k ] );
            s += sizes[ k ] / 4;
        }
        boat_probe_io_t io;
        boat_nose_probe( b[ 0 ].data( ), b[ 1 ].data( ), b[ 3 ].data( ), b[ 4 ].data( ) + 0x50, b[ 5 ].data( ), 0.016666668f, answer, io );
        int bad = io.asked != asked || io.event != event ? 0x7000 : -1;
        if ( bad < 0 && asked && ( std::memcmp( io.point, point, 12 ) != 0 || std::memcmp( io.callback, callback, 20 ) != 0 ) )
            bad = 0x7001;
        for ( int k = 0; k < 6 && bad < 0; ++k )
        {
            for ( int at = 0; at < sizes[ k ] && bad < 0; ++at )
                if ( b[ k ][ at ] != reinterpret_cast< const std::uint8_t* >( s )[ at ] )
                    bad = 0x1000 * k + at;
            s += sizes[ k ] / 4;
        }
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat nose probe vector %d differs from retail at 0x%X (block x 0x1000 + byte; 0x7000 calls, 0x7001 the probe)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the Decision sub-tick 0x41F740 matches retail (VuWater's lookup answered): %d vectors", static_cast< int >( g_golden_boat_nose_probe_count ) );
    else
        LOG_ERROR( "boat nose probe FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_nose_probe_count ) );
    return failed == 0;
}

// boat_load_sections (boat/load.hxx) against retail's 0x420A70 RUN WHOLE on a
// boat from its own ctor (oracle case boat_load_sections; every BoatDB boat
// under the AI driver, six under other classes): the boat, body, camera,
// hull, solver, engine, animation, damage and instigator blocks after (bytes
// retail's two fills agree on, outside string fields and the engine's
// container words), every string field's text, the engine's curve / drives /
// records, the hull shape's image, the compound (count, local aabb, revision,
// margin, the child record, the root leaf), the solver's record arrays, and
// the sequences of hook calls and asset-factory requests.
inline bool verify_boat_load_sections( )
{
    const unsigned* d = g_golden_boat_sections_boatdb;
    const unsigned char* raw = reinterpret_cast< const unsigned char* >( d + 1 );
    const value_t boatdb = c_reader( std::vector< std::uint8_t >( raw, raw + d[ 0 ] ) ).parse( );
    auto text = []( const unsigned*& s )
    {
        const unsigned length = *s++;
        std::string out( reinterpret_cast< const char* >( s ), length );
        s += ( length + 3 ) / 4;
        return out;
    };
    struct block_t { std::vector< std::uint8_t > before, after, mask; };
    auto bytes = []( const unsigned*& s, unsigned n )
    {
        std::vector< std::uint8_t > out( n );
        std::memcpy( out.data( ), s, n );
        s += ( n + 3 ) / 4;
        return out;
    };
    struct ctx_t
    {
        std::vector< std::string > hooks, requests;
        std::uint32_t asset = 0, fluid = 0;
        int fluid_vertices = 0, fluid_edges = 0;
        striding_mesh_t mesh{};
        std::vector< std::array< float, 4 > > vertices;
        std::vector< std::uint16_t > indices;
    };
    int failed = 0, compared = 0;
    for ( std::size_t index = 0; index < g_golden_boat_load_sections_count; ++index )
    {
        const unsigned* s = g_golden_boat_load_sections_words + g_golden_boat_load_sections_offsets[ index ];
        const std::string name = text( s );
        const std::uint32_t vtable = *s++, boat_address = *s++, shape_address = *s++, compound_address = *s++;
        const std::uint32_t root_address = *s++, array0 = *s++, array1 = *s++;
        ctx_t ctx;
        ctx.asset = *s++;
        ctx.fluid = *s++;
        ctx.fluid_vertices = static_cast< int >( *s++ );
        ctx.fluid_edges = static_cast< int >( *s++ );
        const std::uint32_t pfx_flags = *s++;
        ctx.vertices.resize( *s++ );
        std::memcpy( ctx.vertices.data( ), s, 16 * ctx.vertices.size( ) );
        s += 4 * ctx.vertices.size( );
        ctx.indices.resize( *s++ );
        std::memcpy( ctx.indices.data( ), s, 2 * ctx.indices.size( ) );
        s += ( 2 * ctx.indices.size( ) + 3 ) / 4;
        std::vector< std::string > want_hooks( *s++ );
        for ( auto& h : want_hooks )
        {
            const unsigned address = *s++;
            const std::string self = text( s );
            const std::string arg = text( s );
            char buf[ 32 ];
            std::snprintf( buf, sizeof( buf ), "0x%X ", address );
            h = ( address ? std::string( buf ) + self + " " : std::string( "surface " ) ) + arg;
        }
        std::vector< std::string > want_requests( *s++ );
        for ( auto& r : want_requests )
            r = text( s );
        block_t blocks[ 9 ];
        for ( auto& bl : blocks )
        {
            const unsigned n = *s++;
            bl.before = bytes( s, n );
            bl.after = bytes( s, n );
            bl.mask = bytes( s, n );
        }
        std::vector< std::pair< int, std::string > > want_texts[ 4 ];
        for ( auto& t : want_texts )
        {
            t.resize( *s++ );
            for ( auto& w : t )
            {
                w.first = static_cast< int >( *s++ );
                w.second = text( s );
            }
        }
        std::vector< std::uint32_t > curve( *s++ );
        for ( auto& w : curve )
            w = *s++;
        std::vector< std::array< std::uint32_t, 8 > > drives( *s++ );
        for ( auto& r : drives )
            for ( auto& w : r )
                w = *s++;
        const unsigned records = *s++;
        block_t extra[ 4 ];   // compound, children, root, shape
        for ( auto& bl : extra )
        {
            const unsigned n = *s++;
            bl.after = bytes( s, n );
            bl.mask = bytes( s, n );
        }
        std::uint32_t arrays[ 6 ];
        for ( auto& w : arrays )
            w = *s++;
        const unsigned array_bytes[ 2 ] = { s[ 0 ], s[ 2 ] }, array_sums[ 2 ] = { s[ 1 ], s[ 3 ] };
        s += 4;

        ctx.mesh.vertices = reinterpret_cast< const float ( * )[ 4 ] >( ctx.vertices.data( ) );
        ctx.mesh.num_vertices = static_cast< int >( ctx.vertices.size( ) );
        ctx.mesh.indices = ctx.indices.data( );
        ctx.mesh.index_count = static_cast< int >( ctx.indices.size( ) );
        std::vector< std::uint8_t > img[ 9 ];
        for ( int k = 0; k < 9; ++k )
            img[ k ] = blocks[ k ].before;
        vu_rigid_body_t body;
        std::memcpy( &body, img[ 1 ].data( ), sizeof( body ) );
        boat_engine_t engine;
        std::memcpy( engine.object.image, img[ 5 ].data( ), 0x104 );
        boat_load_t io{};
        io.boat = img[ 0 ].data( );
        io.boat_address = boat_address;
        io.body = &body;
        io.camera = img[ 2 ].data( );
        io.hull = img[ 3 ].data( );
        io.solver = img[ 4 ].data( );
        io.engine = &engine;
        io.animation = img[ 6 ].data( );
        io.pfx_owner_flags = pfx_flags;
        io.damage = img[ 7 ].data( );
        io.instigator = img[ 8 ].data( );
        for ( int at : { 0xC4, 0xDC, 0xF4, 0x11C, 0x148, 0x160, 0x178, 0x1A4, 0x1BC, 0x1D4, 0x1EC, 0x204 } )
            io.boat_strings[ at ] = "";
        for ( int at : { 0x4, 0xD4, 0xEC, 0x104, 0x11C } )
            io.hull_strings[ at ] = "";
        for ( int at : { 0x14, 0x2C, 0x44, 0x7C, 0x94, 0xAC } )
            io.animation_strings[ at ] = "";
        io.boatdb = &boatdb;
        io.driver = boat_driver_of( vtable );
        io.dynamics = true;
        io.gravity[ 0 ] = 0.0f;
        io.gravity[ 1 ] = 0.0f;
        io.gravity[ 2 ] = -14.715f;
        io.gravity[ 3 ] = 0.0f;
        io.shape_address = shape_address;
        io.compound_address = compound_address;
        io.root_address = root_address;
        io.array_address[ 0 ] = array0;
        io.array_address[ 1 ] = array1;
        io.hooks.user = &ctx;
        io.hooks.exists = []( void* u, const char* type, const std::string& n )
        {
            ctx_t& c = *static_cast< ctx_t* >( u );
            // The oracle's factory answers from Base.apf's directory: the case's asset served is the answer.
            const bool collision = std::strcmp( type, "VuCollisionMeshAsset" ) == 0;
            const bool yes = collision ? c.asset != 0 : c.fluid != 0;
            c.requests.push_back( std::string( "exists " ) + type + "/" + n + " = " + ( yes ? "1" : "0" ) );
            return yes;
        };
        io.hooks.create = []( void* u, const char* type, const std::string& n )
        {
            ctx_t& c = *static_cast< ctx_t* >( u );
            c.requests.push_back( std::string( type ) + "/" + n );
            return std::strcmp( type, "VuCollisionMeshAsset" ) == 0 ? c.asset : c.fluid;
        };
        io.hooks.release = []( void* u, std::uint32_t a )
        {
            char buf[ 32 ];
            std::snprintf( buf, sizeof( buf ), "release 0x%X", a );
            static_cast< ctx_t* >( u )->requests.push_back( buf );
            return true;
        };
        io.hooks.call = []( void* u, std::uint32_t address, const char* self, const std::string& arg )
        {
            char buf[ 32 ];
            std::snprintf( buf, sizeof( buf ), "0x%X ", address );
            static_cast< ctx_t* >( u )->hooks.push_back( std::string( buf ) + self + " " + arg );
        };
        io.hooks.surface = []( void* u, const std::string& n ) -> std::uint8_t
        {
            static_cast< ctx_t* >( u )->hooks.push_back( "surface " + n );
            // The case's answers (the surface-type table is a hook): Metal 3, Wood 5, Duck 9, else 1.
            return n == "Metal" ? 3 : n == "Wood" ? 5 : n == "Duck" ? 9 : 1;
        };
        io.hooks.mesh = []( void* u, std::uint32_t a, striding_mesh_t& out )
        {
            ctx_t& c = *static_cast< ctx_t* >( u );
            if ( a != c.asset )
                return false;
            out = c.mesh;
            return true;
        };
        io.hooks.fluid_counts = []( void* u, std::uint32_t a, int& v, int& e )
        {
            ctx_t& c = *static_cast< ctx_t* >( u );
            if ( a != c.fluid )
                return false;
            v = c.fluid_vertices;
            e = c.fluid_edges;
            return true;
        };
        int bad = boat_load_sections( io ) ? -1 : 0x9000;
        std::memcpy( img[ 1 ].data( ), &body, sizeof( body ) );
        std::memcpy( img[ 5 ].data( ), engine.object.image, 0x104 );
        // Masked: string fields (24 bytes), the engine's container words.
        const std::vector< int > strings[ 9 ] = {
            { 0x74, 0x8C, 0xA4, 0xC4, 0xDC, 0xF4, 0x11C, 0x148, 0x160, 0x178, 0x1A4, 0x1BC, 0x1D4, 0x1EC, 0x204 }, {}, {},
            { 0x4, 0xD4, 0xEC, 0x104, 0x11C, 0x17C }, {}, { 0x4C, 0x64, 0x7C, 0x94, 0xAC }, { 0x14, 0x2C, 0x44, 0x7C, 0x94, 0xAC },
            { 0x78 }, {} };
        for ( int k = 0; k < 9 && bad < 0; ++k )
            for ( int at = 0; at < static_cast< int >( img[ k ].size( ) ) && bad < 0; ++at )
            {
                bool skip = !blocks[ k ].mask[ at ];
                for ( int o : strings[ k ] )
                    skip = skip || ( at >= o && at < o + 24 );
                if ( k == 5 )
                    skip = skip || ( at >= 0x30 && at < 0x34 ) || ( at >= 0x3C && at < 0x48 ) || ( at >= 0xF8 && at < 0x104 );
                if ( !skip && img[ k ][ at ] != blocks[ k ].after[ at ] )
                    bad = 0x1000 * k + at;
            }
        const boat_strings_t* got[ 4 ] = { &io.boat_strings, &io.hull_strings, &io.animation_strings, &engine.strings };
        for ( int k = 0; k < 4 && bad < 0; ++k )
            for ( const auto& w : want_texts[ k ] )
            {
                const auto it = got[ k ]->find( w.first );
                if ( bad < 0 && ( it == got[ k ]->end( ) ? std::string( ) : it->second ) != w.second )
                    bad = 0xA000 + 0x1000 * k + w.first;
            }
        if ( bad < 0 && ( engine.curve != curve || engine.records != records || engine.drives.size( ) != drives.size( ) ) )
            bad = 0xE000;
        for ( std::size_t k = 0; bad < 0 && k < drives.size( ); ++k )
            for ( int lane : { 0, 1, 2, 4, 5, 6 } )
                if ( engine.drives[ k ][ lane ] != drives[ k ][ lane ] )
                    bad = 0xE001;
        if ( bad < 0 && ( ctx.hooks != want_hooks || ctx.requests != want_requests ) )
            bad = 0xF000;
        // The shape image, the compound and its child / root leaf (retail's words on retail's mask).
        auto masked_equal = [ & ]( const block_t& bl, int at, const void* p, int n )
        {
            const std::uint8_t* q = static_cast< const std::uint8_t* >( p );
            for ( int k = 0; k < n; ++k )
                if ( bl.mask[ at + k ] && bl.after[ at + k ] != q[ k ] )
                    return false;
            return true;
        };
        if ( bad < 0 && io.has_shape != !extra[ 3 ].after.empty( ) )
            bad = 0xF100;
        for ( int at = 0; bad < 0 && io.has_shape && at < 0x90; ++at )
            if ( extra[ 3 ].mask[ at ] && extra[ 3 ].after[ at ] != io.shape_image[ at ] )
                bad = 0x10000 + at;   // 0x1xxxx: the shape image's byte
        if ( bad < 0 )
        {
            const compound_shape_t& c = io.compound;
            const std::uint32_t count = static_cast< std::uint32_t >( c.children.size( ) );
            const std::int32_t revision = c.revision;
            if ( !masked_equal( extra[ 0 ], 0x10, &count, 4 ) || !masked_equal( extra[ 0 ], 0x20, c.local_aabb_min, 16 ) ||
                 !masked_equal( extra[ 0 ], 0x30, c.local_aabb_max, 16 ) || !masked_equal( extra[ 0 ], 0x44, &revision, 4 ) ||
                 !masked_equal( extra[ 0 ], 0x48, &c.margin, 4 ) || extra[ 1 ].after.size( ) != 80 * count )
                bad = 0xF200;
            if ( bad < 0 && count == 1 )
            {
                const compound_child_t& r = c.children[ 0 ];
                const std::uint32_t words[ 3 ] = { shape_address, static_cast< std::uint32_t >( r.type ), root_address };
                if ( !masked_equal( extra[ 1 ], 0, r.transform, 64 ) || !masked_equal( extra[ 1 ], 64, &words[ 0 ], 8 ) ||
                     !masked_equal( extra[ 1 ], 72, &r.margin, 4 ) || !masked_equal( extra[ 1 ], 76, &words[ 2 ], 4 ) )
                    bad = 0xF201;
                const std::int32_t data = c.root_data;
                if ( bad < 0 && ( !masked_equal( extra[ 2 ], 0, c.root_min, 16 ) || !masked_equal( extra[ 2 ], 16, c.root_max, 16 ) ||
                                  !masked_equal( extra[ 2 ], 0x24, &data, 4 ) ) )
                    bad = 0xF202;
            }
        }
        // The record arrays: retail's six words are the solver image's +0x38..+0x4C (compared above); their
        // contents are 40 x size zero bytes.
        for ( int k = 0; k < 2 && bad < 0; ++k )
            if ( array_bytes[ k ] != 40 * arrays[ 3 * k + 1 ] || array_sums[ k ] != 0 )
                bad = 0xF300 + k;
        ++compared;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat load sections vector %d (%s) differs from retail at 0x%X (block x 0x1000 + byte; 0xAxxx strings, "
                       "0xE engine containers, 0xF000 hooks / requests, 0xF1 shape, 0xF2 compound, 0xF3 arrays, 0x9000 refused)",
                       static_cast< int >( index ), name.c_str( ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuBoatDB_loadAllSections 0x420A70 (run whole; model / pfx / debris / skin loads hooked) matches retail: %d vectors",
                     compared );
    else
        LOG_ERROR( "boat load sections FAILED %d of %d vectors", failed, compared );
    return failed == 0;
}

// boat_transform_changed (boat/load.hxx) against retail's 0x421670 (oracle
// case boat_transform; the instigator manager's 0x536350 intercepted and
// counted): the five blocks after (the boat's +0x3F4, a stack word retail
// never writes, skipped) and the hook's count. The CRT atan2 / asinf
// substitution is measured here.
inline bool verify_boat_transform( )
{
    const int sizes[ 5 ] = { 0x520, 0xC8, 0x2D4, 0x308, 0x2C };
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_boat_transform_count; ++index )
    {
        const unsigned* s = g_golden_boat_transform_words + g_golden_boat_transform_offsets[ index ];
        const unsigned manager = *s++;
        std::vector< std::uint8_t > b[ 5 ];
        for ( int k = 0; k < 5; ++k )
        {
            b[ k ].assign( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + sizes[ k ] );
            s += sizes[ k ] / 4;
        }
        vu_rigid_body_t body;
        std::memcpy( &body, b[ 2 ].data( ), sizeof( body ) );
        unsigned calls = 0;
        boat_transform_hooks_t hooks;
        hooks.user = &calls;
        hooks.instigator_update = []( void* u ) { ++*static_cast< unsigned* >( u ); };
        float world[ 16 ];
        std::memcpy( world, b[ 1 ].data( ) + 0x50, 64 );
        boat_transform_changed( b[ 0 ].data( ), world, body, b[ 3 ].data( ), b[ 4 ].data( ), hooks );
        std::memcpy( b[ 2 ].data( ), &body, sizeof( body ) );
        int bad = calls != manager ? 0x7000 : -1;
        for ( int k = 0; k < 5 && bad < 0; ++k )
        {
            for ( int at = 0; at < sizes[ k ] && bad < 0; ++at )
                if ( !( k == 0 && at >= 0x3F4 && at < 0x3F8 ) && b[ k ][ at ] != reinterpret_cast< const std::uint8_t* >( s )[ at ] )
                    bad = 0x1000 * k + at;
            s += sizes[ k ] / 4;
        }
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat transform vector %d differs from retail at 0x%X (block x 0x1000 + byte; 0x7000 the hook count)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the transform-changed delegate 0x421670 matches retail (CRT atan2 / asinf as std::): %d vectors",
                     static_cast< int >( g_golden_boat_transform_count ) );
    else
        LOG_ERROR( "boat transform FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_transform_count ) );
    return failed == 0;
}

// boat_boost (boat/state.hxx) against retail's 0x41F9D0 (oracle case
// boat_boost; the FMOD imports and the stats event intercepted): the boat and
// animation blocks after, and the platform calls by kind and name.
inline bool verify_boat_boost( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_boat_boost_count; ++index )
    {
        const unsigned* s = g_golden_boat_boost_words + g_golden_boat_boost_offsets[ index ];
        float dt;
        std::memcpy( &dt, s++, 4 );
        const boat_driver_t driver = boat_driver_of( *s++ );
        const unsigned players = *s++;
        std::vector< std::string > want( *s++ );
        for ( auto& w : want )
        {
            const unsigned kind = *s++;
            const unsigned length = *s++;
            w = ( kind ? "stats " : "sound " ) + std::string( reinterpret_cast< const char* >( s ), length );
            s += ( length + 3 ) / 4;
        }
        std::vector< std::uint8_t > boat( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + 0x520 );
        s += 0x520 / 4;
        std::vector< std::uint8_t > anim( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + 0xDC );
        s += 0xDC / 4;
        std::vector< std::string > got;
        boat_boost_hooks_t hooks;
        hooks.user = &got;
        hooks.sound = []( void* u, const std::string& e ) { static_cast< std::vector< std::string >* >( u )->push_back( "sound " + e ); };
        hooks.stats = []( void* u, const char* n ) { static_cast< std::vector< std::string >* >( u )->push_back( std::string( "stats " ) + n ); };
        boat_boost( boat.data( ), anim.data( ), driver.slot[ 0 ], driver.slot[ 2 ], players, dt, hooks );
        int bad = got != want ? 0x7000 : -1;
        for ( int at = 0; at < 0x520 && bad < 0; ++at )
            if ( boat[ at ] != reinterpret_cast< const std::uint8_t* >( s )[ at ] )
                bad = at;
        s += 0x520 / 4;
        for ( int at = 0; at < 0xDC && bad < 0; ++at )
            if ( anim[ at ] != reinterpret_cast< const std::uint8_t* >( s )[ at ] )
                bad = 0x1000 + at;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat boost vector %d differs from retail at 0x%X (0x1xxx the animation; 0x7000 the platform calls)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the Decision sub-tick 0x41F9D0 (boost; FMOD / stats hooked) matches retail: %d vectors",
                     static_cast< int >( g_golden_boat_boost_count ) );
    else
        LOG_ERROR( "boat boost FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_boost_count ) );
    return failed == 0;
}

// boat_engine_audio (boat/state.hxx) against retail's 0x413830 (oracle case
// boat_engine_audio; the FMOD imports intercepted and normalised): the six
// blocks after and the FMOD calls word for word, the events answered with
// retail's recorded results and handles.
struct boat_engine_audio_log_t
{
    std::vector< unsigned > got;
    std::vector< std::pair< unsigned, unsigned > > answers;   // (ok, handle) per event, in order
    std::size_t next = 0;
    void text( const char* s, std::size_t n )
    {
        got.push_back( static_cast< unsigned >( n ) );
        for ( std::size_t k = 0; k < n; k += 4 )
        {
            unsigned w = 0;
            std::memcpy( &w, s + k, n - k < 4 ? n - k : 4 );
            got.push_back( w );
        }
    }
};

// The golden call list at `s` (count, then per call its kind and fields, as
// the emitter writes them): the words, the events' recorded answers into
// log.answers, `s` advanced past it. Kinds 5 (splash: text), 6 (lens: one
// word) and 7 (wake) are the hull tick's; 8 .. 19 the Decision tick's (8 / 9
// text, 10 eight words, 12 / 13 / 14 one, the rest none).
inline std::vector< unsigned > boat_audio_calls( const unsigned*& s, boat_engine_audio_log_t& log )
{
    const unsigned count = *s++;
    const unsigned* calls = s;
    for ( unsigned k = 0; k < count; ++k )
    {
        const unsigned kind = *s++;
        if ( kind == 0 )
        {
            s += 1;
            s += 1 + ( *s + 3 ) / 4;
            log.answers.push_back( { s[ 0 ], s[ 1 ] } );
            s += 2;
        }
        else if ( kind == 3 )
            s += 7;
        else if ( kind == 4 )
        {
            s += 1;
            s += 1 + ( *s + 3 ) / 4;
            s += 1;
        }
        else if ( kind == 5 || kind == 8 || kind == 9 )
            s += 1 + ( *s + 3 ) / 4;
        else if ( kind == 10 )
            s += 8;
        else if ( kind == 1 || kind == 2 || kind == 6 || kind == 12 || kind == 13 || kind == 14 )
            s += 1;
    }
    return std::vector< unsigned >( calls, s );
}

inline boat_engine_audio_hooks_t boat_audio_log_hooks( boat_engine_audio_log_t& log )
{
    boat_engine_audio_hooks_t hooks;
    hooks.user = &log;
    hooks.release = []( void* u, std::uint32_t h ) { auto& l = *static_cast< boat_engine_audio_log_t* >( u ); l.got.push_back( 1 ); l.got.push_back( h ); };
    hooks.get_event = []( void* u, int slot, const std::string& name, std::uint32_t* h )
    {
        auto& l = *static_cast< boat_engine_audio_log_t* >( u );
        const auto a = l.next < l.answers.size( ) ? l.answers[ l.next++ ] : std::pair< unsigned, unsigned >( 0, 0 );
        *h = a.second;
        l.got.push_back( 0 );
        l.got.push_back( static_cast< unsigned >( slot ) );
        l.text( name.data( ), name.size( ) );
        l.got.push_back( a.first );
        l.got.push_back( a.second );
        return a.first != 0;
    };
    hooks.start = []( void* u, std::uint32_t h ) { auto& l = *static_cast< boat_engine_audio_log_t* >( u ); l.got.push_back( 2 ); l.got.push_back( h ); };
    hooks.attributes = []( void* u, std::uint32_t h, const float* p, const float* v )
    {
        auto& l = *static_cast< boat_engine_audio_log_t* >( u );
        l.got.push_back( 3 );
        l.got.push_back( h );
        for ( int k = 0; k < 6; ++k )
        {
            unsigned w;
            std::memcpy( &w, k < 3 ? p + k : v + k - 3, 4 );
            l.got.push_back( w );
        }
    };
    hooks.parameter = []( void* u, std::uint32_t h, const char* name, float value )
    {
        auto& l = *static_cast< boat_engine_audio_log_t* >( u );
        l.got.push_back( 4 );
        l.got.push_back( h );
        l.text( name, std::strlen( name ) );
        unsigned w;
        std::memcpy( &w, &value, 4 );
        l.got.push_back( w );
    };
    return hooks;
}

inline bool verify_boat_engine_audio( )
{
    const int sizes[ 6 ] = { 0x520, 0x2D4, 0xC8, 0x104, 0x104, 0x40 };
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_boat_engine_audio_count; ++index )
    {
        const unsigned* s = g_golden_boat_engine_audio_words + g_golden_boat_engine_audio_offsets[ index ];
        float dt;
        std::memcpy( &dt, s++, 4 );
        boat_strings_t names;
        for ( int at : { 0x64, 0x7C, 0x94, 0xAC } )
        {
            const unsigned n = *s++;
            names[ at ] = std::string( reinterpret_cast< const char* >( s ), n );
            s += ( n + 3 ) / 4;
        }
        boat_engine_audio_log_t log;
        const std::vector< unsigned > want = boat_audio_calls( s, log );
        std::vector< std::uint8_t > b[ 6 ];
        for ( int k = 0; k < 6; ++k )
        {
            b[ k ].assign( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + sizes[ k ] );
            s += sizes[ k ] / 4;
        }
        solver_rigid_t body{ };
        std::memcpy( &body, b[ 1 ].data( ), sizeof( body ) < 0x2D4 ? sizeof( body ) : 0x2D4 );
        const boat_engine_audio_hooks_t hooks = boat_audio_log_hooks( log );
        boat_engine_audio( b[ 4 ].data( ), names, b[ 0 ].data( ), body, b[ 2 ].data( ), b[ 3 ].data( ), b[ 5 ].data( ), dt, hooks );
        std::memcpy( b[ 1 ].data( ), &body, sizeof( body ) < 0x2D4 ? sizeof( body ) : 0x2D4 );
        int bad = log.got != want ? 0x7000 : -1;
        for ( int k = 0; k < 6; ++k )
        {
            for ( int at = 0; at < sizes[ k ] && bad < 0; ++at )
                if ( b[ k ][ at ] != reinterpret_cast< const std::uint8_t* >( s )[ at ] )
                    bad = 0x1000 * k + at;
            s += sizes[ k ] / 4;
        }
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat engine audio vector %d differs from retail at 0x%X (block x 0x1000 + byte; 0x7000 the FMOD calls)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the engine's audio tick 0x413830 (FMOD hooked) matches retail: %d vectors",
                     static_cast< int >( g_golden_boat_engine_audio_count ) );
    else
        LOG_ERROR( "boat engine audio FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_engine_audio_count ) );
    return failed == 0;
}

// boat_hull_tick (boat/state.hxx) against retail's 0x416630 (oracle case
// boat_hull_tick; FMOD, the splash effect, the lens water and the wake update
// intercepted): the eight blocks after and the calls word for word. The
// driver IS-A VuHumanDriver test is the vtable (0x737F44) - retail walks the
// type chain, the vectors use all five driver classes.
inline bool verify_boat_hull_tick( )
{
    const int sizes[ 8 ] = { 0x520, 0x2D4, 0xC8, 0x104, 0x104, 0x1F0, 0xD0, 0x308 };
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_boat_hull_tick_count; ++index )
    {
        const unsigned* s = g_golden_boat_hull_tick_words + g_golden_boat_hull_tick_offsets[ index ];
        float dt;
        std::memcpy( &dt, s++, 4 );
        const bool human = *s++ == 0x737F44u;
        boat_strings_t names;
        for ( int at : { 0xEC, 0x17C } )
        {
            const unsigned n = *s++;
            names[ at ] = std::string( reinterpret_cast< const char* >( s ), n );
            s += ( n + 3 ) / 4;
        }
        boat_engine_audio_log_t log;
        const std::vector< unsigned > want = boat_audio_calls( s, log );
        std::vector< std::uint8_t > b[ 8 ];
        for ( int k = 0; k < 8; ++k )
        {
            b[ k ].assign( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + sizes[ k ] );
            s += sizes[ k ] / 4;
        }
        solver_rigid_t body{ };
        std::memcpy( &body, b[ 1 ].data( ), sizeof( body ) < 0x2D4 ? sizeof( body ) : 0x2D4 );
        boat_hull_tick_hooks_t hooks;
        hooks.fmod = boat_audio_log_hooks( log );
        hooks.user = &log;
        hooks.splash = []( void* u, const std::string& name )
        {
            auto& l = *static_cast< boat_engine_audio_log_t* >( u );
            l.got.push_back( 5 );
            l.text( name.data( ), name.size( ) );
        };
        hooks.lens = []( void* u, float amount )
        {
            auto& l = *static_cast< boat_engine_audio_log_t* >( u );
            unsigned w;
            std::memcpy( &w, &amount, 4 );
            l.got.push_back( 6 );
            l.got.push_back( w );
        };
        hooks.wake = []( void* u ) { static_cast< boat_engine_audio_log_t* >( u )->got.push_back( 7 ); };
        boat_hull_tick( b[ 5 ].data( ), names, b[ 0 ].data( ), body, b[ 2 ].data( ), b[ 3 ].data( ), b[ 4 ].data( ), b[ 6 ].data( ),
                        human, b[ 7 ].data( ), dt, hooks );
        std::memcpy( b[ 1 ].data( ), &body, sizeof( body ) < 0x2D4 ? sizeof( body ) : 0x2D4 );
        int bad = log.got != want ? 0x9000 : -1;
        for ( int k = 0; k < 8; ++k )
        {
            for ( int at = 0; at < sizes[ k ] && bad < 0; ++at )
                if ( b[ k ][ at ] != reinterpret_cast< const std::uint8_t* >( s )[ at ] )
                    bad = 0x1000 * k + at;
            s += sizes[ k ] / 4;
        }
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat hull tick vector %d differs from retail at 0x%X (block x 0x1000 + byte; 0x9000 the calls)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the hull's Decision tick 0x416630 (FMOD / splash pfx / lens water / wake update hooked) matches retail: %d vectors",
                     static_cast< int >( g_golden_boat_hull_tick_count ) );
    else
        LOG_ERROR( "boat hull tick FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_hull_tick_count ) );
    return failed == 0;
}

// boat_decision_tick (boat/state.hxx) against retail's whole Decision tick
// 0x41EFF0 (oracle case boat_decision: every recreated sub-tick ran for real
// in retail, the rest intercepted): the twelve blocks after and every call
// word for word, the wake lookup answered with the vector's chosen value.
struct boat_decision_log_t
{
    boat_engine_audio_log_t log;
    float answer = 0.0f;
};

inline bool verify_boat_decision( )
{
    const int sizes[ 12 ] = { 0x520, 0x2D4, 0xC8, 0x104, 0x104, 0x40, 0x1F0, 0xD0, 0x308, 0xDC, 0xD0, 0x10 };
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_boat_decision_count; ++index )
    {
        const unsigned* s = g_golden_boat_decision_words + g_golden_boat_decision_offsets[ index ];
        float dt;
        std::memcpy( &dt, s++, 4 );
        const unsigned vtable = *s++;
        const unsigned players = *s++;
        const bool has_floor = *s++ != 0;
        float floor, answer;
        std::memcpy( &floor, s++, 4 );
        std::memcpy( &answer, s++, 4 );
        boat_strings_t hull_names, engine_names;
        for ( int at : { 0xEC, 0x17C, 0x64, 0x7C, 0x94, 0xAC } )
        {
            const unsigned n = *s++;
            ( at == 0xEC || at == 0x17C ? hull_names : engine_names )[ at ] = std::string( reinterpret_cast< const char* >( s ), n );
            s += ( n + 3 ) / 4;
        }
        boat_decision_log_t dl;
        dl.answer = answer;
        auto& log = dl.log;
        const std::vector< unsigned > want = boat_audio_calls( s, log );
        std::vector< std::uint8_t > b[ 12 ];
        for ( int k = 0; k < 12; ++k )
        {
            b[ k ].assign( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + sizes[ k ] );
            s += sizes[ k ] / 4;
        }
        std::uint32_t proxy_word;
        std::memcpy( &proxy_word, b[ 1 ].data( ) + 0xC8, 4 );
        boat_decision_t d{ b[ 0 ].data( ), b[ 1 ].data( ), proxy_word ? b[ 11 ].data( ) : nullptr, b[ 7 ].data( ), boat_driver_of( vtable ),
                           vtable == 0x737F44u, b[ 8 ].data( ), b[ 6 ].data( ), &hull_names, b[ 4 ].data( ), &engine_names, b[ 5 ].data( ),
                           b[ 9 ].data( ), b[ 10 ].data( ), b[ 2 ].data( ), b[ 3 ].data( ), players, has_floor ? &floor : nullptr };
        boat_decision_hooks_t hooks;
        hooks.hull.fmod = boat_audio_log_hooks( log );
        hooks.user = hooks.hull.user = hooks.boost.user = &dl;
        hooks.wake_lookup = []( void* u, const boat_probe_io_t& io )
        {
            auto& l = static_cast< boat_decision_log_t* >( u )->log;
            l.got.push_back( 10 );
            for ( int k = 0; k < 3; ++k )
            {
                unsigned w;
                std::memcpy( &w, &io.point[ k ], 4 );
                l.got.push_back( w );
            }
            for ( int k = 0; k < 5; ++k )
                l.got.push_back( io.callback[ k ] );
            return static_cast< boat_decision_log_t* >( u )->answer;
        };
        hooks.hull.splash = []( void* u, const std::string& name ) { auto& l = static_cast< boat_decision_log_t* >( u )->log; l.got.push_back( 5 ); l.text( name.data( ), name.size( ) ); };
        hooks.hull.lens = []( void* u, float v ) { auto& l = static_cast< boat_decision_log_t* >( u )->log; unsigned w; std::memcpy( &w, &v, 4 ); l.got.push_back( 6 ); l.got.push_back( w ); };
        hooks.hull.wake = []( void* u ) { static_cast< boat_decision_log_t* >( u )->log.got.push_back( 7 ); };
        hooks.boost.sound = []( void* u, const std::string& e ) { auto& l = static_cast< boat_decision_log_t* >( u )->log; l.got.push_back( 8 ); l.text( e.data( ), e.size( ) ); };
        hooks.boost.stats = []( void* u, const char* n ) { auto& l = static_cast< boat_decision_log_t* >( u )->log; l.got.push_back( 9 ); l.text( n, std::strlen( n ) ); };
        hooks.jump_sound = []( void* u ) { static_cast< boat_decision_log_t* >( u )->log.got.push_back( 11 ); };
        hooks.driver_tick = []( void* u, float v ) { auto& l = static_cast< boat_decision_log_t* >( u )->log; unsigned w; std::memcpy( &w, &v, 4 ); l.got.push_back( 12 ); l.got.push_back( w ); };
        hooks.animation_tick = []( void* u, float v ) { auto& l = static_cast< boat_decision_log_t* >( u )->log; unsigned w; std::memcpy( &w, &v, 4 ); l.got.push_back( 13 ); l.got.push_back( w ); };
        hooks.camera_tick = []( void* u, float v ) { auto& l = static_cast< boat_decision_log_t* >( u )->log; unsigned w; std::memcpy( &w, &v, 4 ); l.got.push_back( 14 ); l.got.push_back( w ); };
        hooks.respawn = []( void* u ) { static_cast< boat_decision_log_t* >( u )->log.got.push_back( 15 ); };
        hooks.enable = []( void* u ) { static_cast< boat_decision_log_t* >( u )->log.got.push_back( 16 ); };
        hooks.disable = []( void* u ) { static_cast< boat_decision_log_t* >( u )->log.got.push_back( 17 ); };
        hooks.debris_410FF0 = []( void* u ) { static_cast< boat_decision_log_t* >( u )->log.got.push_back( 18 ); };
        hooks.boat_41E240 = []( void* u ) { static_cast< boat_decision_log_t* >( u )->log.got.push_back( 19 ); };
        boat_decision_tick( d, dt, hooks );
        int bad = log.got != want ? 0xD000 : -1;
        for ( int k = 0; k < 12; ++k )
        {
            for ( int at = 0; at < sizes[ k ] && bad < 0; ++at )
                if ( b[ k ][ at ] != reinterpret_cast< const std::uint8_t* >( s )[ at ] )
                    bad = 0x1000 * k + at;
            s += sizes[ k ] / 4;
        }
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "boat decision vector %d differs from retail at 0x%X (block x 0x1000 + byte; 0xD000 the calls)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the Decision tick 0x41EFF0 (its recreated sub-ticks composed; the rest hooked) matches retail: %d vectors",
                     static_cast< int >( g_golden_boat_decision_count ) );
    else
        LOG_ERROR( "boat decision FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_decision_count ) );
    return failed == 0;
}

// The blocks of an AI-closure vector: `count` blocks of `sizes`, before then
// after, from the golden words at `s` (advanced past them).
struct ai_blocks_t
{
    std::vector< std::uint8_t > b[ 4 ];
    const std::uint8_t* want[ 4 ] = { };
};

inline ai_blocks_t ai_read_blocks( const unsigned*& s, const int* sizes, int count )
{
    ai_blocks_t r;
    for ( int k = 0; k < count; ++k )
    {
        r.b[ k ].assign( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + sizes[ k ] );
        s += sizes[ k ] / 4;
    }
    for ( int k = 0; k < count; ++k )
    {
        r.want[ k ] = reinterpret_cast< const std::uint8_t* >( s );
        s += sizes[ k ] / 4;
    }
    return r;
}

// The first differing byte (block x 0x1000 + byte), or -1.
inline int ai_first_diff( const ai_blocks_t& r, const int* sizes, int count )
{
    for ( int k = 0; k < count; ++k )
        for ( int at = 0; at < sizes[ k ]; ++at )
            if ( r.b[ k ][ at ] != r.want[ k ][ at ] )
                return 0x1000 * k + at;
    return -1;
}

// ai_controls (boat/ai.hxx) against retail's 0x40D5A0 (oracle case
// ai_controls): every byte of the driver, boat and animation blocks after,
// and the hooks' calls in order.
inline bool verify_ai_controls( )
{
    const int sizes[ 3 ] = { 0xD4, 0x520, 0xDC };
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_ai_controls_count; ++index )
    {
        const unsigned* s = g_golden_ai_controls_words + g_golden_ai_controls_offsets[ index ];
        float dt, steer, throttle;
        std::memcpy( &dt, s++, 4 );
        std::memcpy( &steer, s++, 4 );
        std::memcpy( &throttle, s++, 4 );
        const std::uint8_t boost = static_cast< std::uint8_t >( *s++ );
        ai_blocks_t r = ai_read_blocks( s, sizes, 3 );
        std::vector< unsigned > calls;
        ai_controls_hooks_t hooks;
        hooks.user = &calls;
        hooks.misfire = []( void* u ) { static_cast< std::vector< unsigned >* >( u )->push_back( 1 ); };
        hooks.voice = []( void* u, const char* line ) { static_cast< std::vector< unsigned >* >( u )->push_back( std::strcmp( line, "NeedPickUpPowerBoost" ) == 0 ? 2u : 99u ); };
        ai_controls( r.b[ 0 ].data( ), r.b[ 1 ].data( ), r.b[ 2 ].data( ), dt, steer, throttle, boost, hooks );
        int bad = ai_first_diff( r, sizes, 3 );
        const unsigned n = *s++;
        if ( bad < 0 && ( n != calls.size( ) || std::memcmp( s, calls.data( ), 4 * n ) != 0 ) )
            bad = 0xD000;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "ai controls vector %d differs from retail at 0x%X (block x 0x1000 + byte; 0xD000 the calls)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuAiDriver's control output 0x40D5A0 matches retail: %d vectors", static_cast< int >( g_golden_ai_controls_count ) );
    else
        LOG_ERROR( "ai controls FAILED %d of %d vectors", failed, static_cast< int >( g_golden_ai_controls_count ) );
    return failed == 0;
}

// ai_steer (boat/ai.hxx) against retail's 0x40D110 (oracle case ai_steer):
// both outputs' bits (left 0xCDCDCDCD when dt <= 0).
inline bool verify_ai_steer( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_ai_steer_count; ++index )
    {
        const unsigned* s = g_golden_ai_steer_words + g_golden_ai_steer_offsets[ index ];
        float v[ 12 ], speed, wanted, dt;
        std::memcpy( v, s, 48 );
        s += 12;
        std::memcpy( &speed, s++, 4 );
        std::memcpy( &wanted, s++, 4 );
        std::memcpy( &dt, s++, 4 );
        std::uint8_t body[ 0x300 ] = { }, solver[ 0x104 ] = { };
        std::memcpy( body + 0x140, s, 0x20 );
        s += 8;
        std::memcpy( solver + 0x54, s++, 4 );
        unsigned out[ 2 ] = { 0xCDCDCDCDu, 0xCDCDCDCDu };
        ai_steer( body, solver, v, v + 4, v + 8, speed, wanted, dt, reinterpret_cast< float* >( &out[ 0 ] ), reinterpret_cast< float* >( &out[ 1 ] ) );
        if ( ( out[ 0 ] != s[ 0 ] || out[ 1 ] != s[ 1 ] ) && ++failed <= 4 )
            LOG_ERROR( "ai steer vector %d: steer 0x%08X throttle 0x%08X, retail 0x%08X 0x%08X", static_cast< int >( index ), out[ 0 ], out[ 1 ], s[ 0 ], s[ 1 ] );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuAiDriver's steering / throttle 0x40D110 matches retail (CRT asinf as the double asin): %d vectors",
                     static_cast< int >( g_golden_ai_steer_count ) );
    else
        LOG_ERROR( "ai steer FAILED %d of %d vectors", failed, static_cast< int >( g_golden_ai_steer_count ) );
    return failed == 0;
}

// ai_path_follow (boat/ai.hxx) against retail's 0x40BEC0 (oracle case
// ai_path_follow): the target and direction words after, but the stale
// stack words retail never writes (the mask).
inline bool verify_ai_path_follow( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_ai_path_follow_count; ++index )
    {
        const unsigned* s = g_golden_ai_path_follow_words + g_golden_ai_path_follow_offsets[ index ];
        const std::int32_t count = static_cast< std::int32_t >( *s++ );
        std::vector< const std::uint8_t* > points;
        for ( std::int32_t k = 0; k < count; ++k, s += 0xD0 / 4 )
            points.push_back( reinterpret_cast< const std::uint8_t* >( s ) );
        float pos[ 4 ];
        std::memcpy( pos, s, 16 );
        s += 4;
        unsigned out[ 8 ];
        for ( unsigned& w : out )
            w = 0xCDCDCDCDu;
        ai_path_follow( points.data( ), count, pos, reinterpret_cast< float* >( out ), reinterpret_cast< float* >( out + 4 ) );
        const unsigned stale = s[ 8 ];
        int bad = -1;
        for ( int w = 0; w < 8 && bad < 0; ++w )
            if ( !( stale & ( 1u << w ) ) && out[ w ] != s[ w ] )
                bad = w;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "ai path follow vector %d (%d points) differs from retail at word %d (0..3 target): 0x%08X, retail 0x%08X",
                       static_cast< int >( index ), static_cast< int >( count ), bad, out[ bad ], s[ bad ] );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuAiDriver's path follow 0x40BEC0 matches retail: %d vectors", static_cast< int >( g_golden_ai_path_follow_count ) );
    else
        LOG_ERROR( "ai path follow FAILED %d of %d vectors", failed, static_cast< int >( g_golden_ai_path_follow_count ) );
    return failed == 0;
}

// ai_avoid (boat/ai.hxx) against retail's 0x40C750 (oracle case ai_avoid):
// the hooks replay the case's scripted world - each ray call returns its
// table, getAabb its body's box - and record what they were asked; the
// target words after (but the stale ones), the ray calls' from / to and the
// getAabb bodies must all be retail's.
struct ai_avoid_world_t
{
    std::vector< std::vector< std::uint8_t > > bodies;
    std::vector< std::array< float, 6 > > boxes;
    std::vector< std::vector< ai_ray_hit_t > > rays;
    std::vector< std::array< unsigned, 8 > > calls;
    std::vector< unsigned > aabbs;
};

inline bool verify_ai_avoid( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_ai_avoid_count; ++index )
    {
        const unsigned* s = g_golden_ai_avoid_words + g_golden_ai_avoid_offsets[ index ];
        float margin, pos[ 4 ], target[ 4 ];
        std::memcpy( &margin, s++, 4 );
        std::memcpy( pos, s, 16 );
        std::memcpy( target, s + 4, 16 );
        s += 8;
        ai_avoid_world_t w;
        const unsigned nb = *s++;
        for ( unsigned k = 0; k < nb; ++k )
        {
            w.bodies.emplace_back( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + 0x300 );
            s += 0x300 / 4;
            std::array< float, 6 > box;
            std::memcpy( box.data( ), s, 24 );
            s += 6;
            w.boxes.push_back( box );
        }
        const unsigned nr = *s++;
        for ( unsigned r = 0; r < nr; ++r )
        {
            std::vector< ai_ray_hit_t > hits( *s++ );
            for ( ai_ray_hit_t& h : hits )
            {
                h.body = w.bodies[ *s++ ].data( );
                std::memcpy( &h.fraction, s++, 4 );
                std::memcpy( h.normal, s, 16 );
                s += 4;
                h.world = *s++ != 0;
            }
            w.rays.push_back( hits );
        }
        ai_avoid_hooks_t hooks;
        hooks.user = &w;
        hooks.ray = []( void* u, const float* from, const float* to, const ai_ray_hit_t** hits ) -> int
        {
            ai_avoid_world_t& w = *static_cast< ai_avoid_world_t* >( u );
            std::array< unsigned, 8 > c;
            std::memcpy( c.data( ), from, 16 );
            std::memcpy( c.data( ) + 4, to, 16 );
            const std::size_t k = w.calls.size( );
            w.calls.push_back( c );
            if ( k >= w.rays.size( ) )
                return 0;
            *hits = w.rays[ k ].data( );
            return static_cast< int >( w.rays[ k ].size( ) );
        };
        hooks.aabb = []( void* u, const std::uint8_t* body, float* mn, float* mx )
        {
            ai_avoid_world_t& w = *static_cast< ai_avoid_world_t* >( u );
            for ( std::size_t k = 0; k < w.bodies.size( ); ++k )
                if ( w.bodies[ k ].data( ) == body )
                {
                    std::memcpy( mn, w.boxes[ k ].data( ), 12 );
                    std::memcpy( mx, w.boxes[ k ].data( ) + 3, 12 );
                    w.aabbs.push_back( static_cast< unsigned >( k ) );
                }
        };
        ai_avoid( margin, w.bodies[ 0 ].data( ), pos, target, hooks );
        unsigned out[ 4 ];
        std::memcpy( out, target, 16 );
        const unsigned* want = s;
        const unsigned stale = s[ 4 ];
        s += 5;
        int bad = -1;
        for ( int k = 0; k < 4 && bad < 0; ++k )
            if ( !( stale & ( 1u << k ) ) && out[ k ] != want[ k ] )
                bad = k;
        const unsigned nc = *s++;
        if ( bad < 0 && nc != w.calls.size( ) )
            bad = 0x100;
        for ( unsigned k = 0; k < nc && bad < 0; ++k )
            if ( std::memcmp( w.calls[ k ].data( ), s + 8 * k, 32 ) != 0 )
                bad = 0x100 + 1 + static_cast< int >( k );
        s += 8 * nc;
        const unsigned na = *s++;
        if ( bad < 0 && ( na != w.aabbs.size( ) || std::memcmp( s, w.aabbs.data( ), 4 * na ) != 0 ) )
            bad = 0x200;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "ai avoid vector %d differs from retail at %d (0..3 target word, 0x100 + k ray call k, 0x200 the getAabb calls)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuAiDriver's obstacle avoidance 0x40C750 (its ray-test result logic; the world's traversal hooked) matches retail: %d vectors",
                     static_cast< int >( g_golden_ai_avoid_count ) );
    else
        LOG_ERROR( "ai avoid FAILED %d of %d vectors", failed, static_cast< int >( g_golden_ai_avoid_count ) );
    return failed == 0;
}

// ai_tick (boat/ai.hxx) against retail's whole 0x40B940 (oracle case
// ai_tick): the driver (but its stale words) and boat blocks after, the ray
// calls' from / to and the controls hooks' calls, with the scripted world
// replayed as in verify_ai_avoid.
inline bool verify_ai_tick( )
{
    auto take = []( const unsigned*& s, std::size_t bytes ) {
        std::vector< std::uint8_t > v( reinterpret_cast< const std::uint8_t* >( s ), reinterpret_cast< const std::uint8_t* >( s ) + bytes );
        s += bytes / 4;
        return v;
    };
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_ai_tick_count; ++index )
    {
        const unsigned* s = g_golden_ai_tick_words + g_golden_ai_tick_offsets[ index ];
        float dt;
        std::memcpy( &dt, s++, 4 );
        std::vector< std::uint8_t > anim( 0x80 ), solver( 0x104 ), other( 0xA0 );
        std::memcpy( anim.data( ) + 0x78, s++, 4 );
        std::memcpy( solver.data( ) + 0x54, s++, 4 );
        std::vector< std::uint8_t > driver = take( s, 0xD4 ), boat = take( s, 0x520 ), eng = take( s, 0x40 ), xf = take( s, 0xA0 );
        ai_avoid_world_t w;
        w.bodies.push_back( take( s, 0x300 ) );
        std::vector< std::vector< std::uint8_t > > points( *s++ );
        std::vector< const std::uint8_t* > point_ptrs;
        for ( auto& p : points )
        {
            p = take( s, 0xD0 );
            point_ptrs.push_back( p.data( ) );
        }
        const bool has_other = *s++ != 0;
        std::memcpy( other.data( ) + 0x80, s, 16 );
        s += 4;
        std::vector< std::vector< std::uint8_t > > boats( *s++ );
        std::vector< const std::uint8_t* > boat_ptrs;
        for ( auto& b : boats )
        {
            const unsigned place = *s++;
            if ( place == 0xFFFFFFFFu )
            {
                boat_ptrs.push_back( boat.data( ) );
                continue;
            }
            b.assign( 0x4D0, 0 );
            std::memcpy( b.data( ) + 0x4CC, &place, 4 );
            boat_ptrs.push_back( b.data( ) );
        }
        for ( int k = 0; k < 2; ++k )
        {
            w.bodies.push_back( take( s, 0x300 ) );
            std::array< float, 6 > box;
            std::memcpy( box.data( ), s, 24 );
            s += 6;
            w.boxes.push_back( box );
        }
        w.boxes.insert( w.boxes.begin( ), std::array< float, 6 >{ } );   // the boat's own body: never asked
        for ( int r = 0; r < 4; ++r )
        {
            std::vector< ai_ray_hit_t > hits( *s++ );
            for ( ai_ray_hit_t& h : hits )
            {
                h.body = w.bodies[ 1 + *s++ ].data( );
                std::memcpy( &h.fraction, s++, 4 );
                std::memcpy( h.normal, s, 16 );
                s += 4;
                h.world = true;
            }
            w.rays.push_back( hits );
        }
        ai_tick_t t;
        t.driver = driver.data( );
        t.boat = boat.data( );
        t.body = w.bodies[ 0 ].data( );
        t.solver = solver.data( );
        t.transform = xf.data( );
        t.engine = eng.data( );
        t.owner = boat.data( );
        t.animation = anim.data( );
        t.other = has_other ? other.data( ) : nullptr;
        t.points = point_ptrs.data( );
        t.count = static_cast< std::int32_t >( points.size( ) );
        t.boats = boat_ptrs.data( );
        t.boat_count = static_cast< std::int32_t >( boats.size( ) );
        struct sink_t
        {
            ai_avoid_world_t* w;
            std::vector< unsigned > calls;
        } sink{ &w, { } };
        ai_tick_hooks_t hooks;
        hooks.avoid.user = &w;
        hooks.avoid.ray = []( void* u, const float* from, const float* to, const ai_ray_hit_t** hits ) -> int
        {
            ai_avoid_world_t& w = *static_cast< ai_avoid_world_t* >( u );
            std::array< unsigned, 8 > c;
            std::memcpy( c.data( ), from, 16 );
            std::memcpy( c.data( ) + 4, to, 16 );
            const std::size_t k = w.calls.size( );
            w.calls.push_back( c );
            if ( k >= w.rays.size( ) )
                return 0;
            *hits = w.rays[ k ].data( );
            return static_cast< int >( w.rays[ k ].size( ) );
        };
        hooks.avoid.aabb = []( void* u, const std::uint8_t* body, float* mn, float* mx )
        {
            ai_avoid_world_t& w = *static_cast< ai_avoid_world_t* >( u );
            for ( std::size_t k = 0; k < w.bodies.size( ); ++k )
                if ( w.bodies[ k ].data( ) == body )
                {
                    std::memcpy( mn, w.boxes[ k ].data( ), 12 );
                    std::memcpy( mx, w.boxes[ k ].data( ) + 3, 12 );
                }
        };
        hooks.controls.user = &sink;
        hooks.controls.misfire = []( void* u ) { static_cast< sink_t* >( u )->calls.push_back( 1 ); };
        hooks.controls.voice = []( void* u, const char* line ) { static_cast< sink_t* >( u )->calls.push_back( std::strcmp( line, "NeedPickUpPowerBoost" ) == 0 ? 2u : 99u ); };
        ai_tick( t, dt, hooks );
        const std::uint8_t* want_driver = reinterpret_cast< const std::uint8_t* >( s );
        s += 0xD4 / 4;
        const std::uint8_t* want_boat = reinterpret_cast< const std::uint8_t* >( s );
        s += 0x520 / 4;
        std::vector< bool > stale( 0xD4 / 4, false );
        const unsigned ns = *s++;
        for ( unsigned k = 0; k < ns; ++k )
            stale[ *s++ ] = true;
        int bad = -1;
        for ( int at = 0; at < 0xD4 && bad < 0; ++at )
            if ( !stale[ at / 4 ] && driver[ at ] != want_driver[ at ] )
                bad = at;
        for ( int at = 0; at < 0x520 && bad < 0; ++at )
            if ( boat[ at ] != want_boat[ at ] )
                bad = 0x1000 + at;
        const unsigned nc = *s++;
        if ( bad < 0 && nc != w.calls.size( ) )
            bad = 0xC000;
        for ( unsigned k = 0; k < nc && bad < 0; ++k )
            if ( std::memcmp( w.calls[ k ].data( ), s + 8 * k, 32 ) != 0 )
                bad = 0xC001 + static_cast< int >( k );
        s += 8 * nc;
        const unsigned nh = *s++;
        if ( bad < 0 && ( nh != sink.calls.size( ) || std::memcmp( s, sink.calls.data( ), 4 * nh ) != 0 ) )
            bad = 0xD000;
        if ( bad >= 0 && ++failed <= 4 )
            LOG_ERROR( "ai tick vector %d differs from retail at 0x%X (driver byte; 0x1xxx boat; 0xC00k ray call k; 0xD000 the hooks)",
                       static_cast< int >( index ), bad );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuAiDriver's tick 0x40B940 (its closure composed; the world's ray traversal / getAabb / audio / voice hooked) matches retail: %d vectors",
                     static_cast< int >( g_golden_ai_tick_count ) );
    else
        LOG_ERROR( "ai tick FAILED %d of %d vectors", failed, static_cast< int >( g_golden_ai_tick_count ) );
    return failed == 0;
}

// track_build_from_tree (boat/track.hxx) against retail's VuTrackManagerImpl on
// Hydroelectric's AI waypoints (oracle case track_build): the level's child 0
// ("AI") under a bare scene root, loaded by hydro's tree (entity_load_project
// with the verified callbacks plus the waypoint's property state) and by
// retail's VuEntity::load, every waypoint's slot 6 0x432400 in gameInitialize
// order, then slot 7 0x4CFFD0. Compared: each waypoint's words +0x5C..+0xA4
// (its postLoad 0x432170's edge points, properties, sector ids, normalised
// probabilities; the stale lanes +0x68 / +0x78, the component pointers +0x7C /
// +0x80 and +0x8D..+0x8F skipped), every sector's 0x4D0F90 image (but its
// stale lanes and pointer words), its waypoint and next / prev lists, the
// checkpoint list, the start / end, length, state, and the queries.
inline std::unique_ptr< entity_t > track_document_tree( )
{
    const unsigned char* raw = reinterpret_cast< const unsigned char* >( g_golden_track_document );
    static const value_t document = c_reader( std::vector< std::uint8_t >( raw, raw + g_golden_track_document_bytes ) ).parse( );
    static const entity_registry_t registry = build_entity_registry( { } );
    entity_world_t world;
    world.m_registry = &registry;
    world.m_ctor_for = entity_ctor_for;
    world.m_template = []( const std::string& ) -> const value_t* { return nullptr; };
    world.m_components_for = entity_components_scripted;
    world.m_component_load = entity_load_component;
    world.m_component_post = entity_post_load_component;
    world.m_component_mark = entity_mark_component;
    world.m_state_for = []( const std::string& cls ) { auto s = waypoint_state_for( cls ); return s ? s : entity_state_for( cls ); };
    world.m_properties_load = []( entity_t& e, const value_t& p ) { waypoint_load_properties( e, p ); entity_load_properties( e, p ); };
    world.m_properties_mark = []( entity_t& e ) { waypoint_mark_properties( e ); entity_mark_properties( e ); };
    return entity_load_project( member( document, "VuProject" ), "Data/Projects/Track.vuprj", world );
}

inline bool verify_track_build( )
{
    const std::unique_ptr< entity_t > tree = track_document_tree( );
    track_manager_t tm;
    if ( !tree || !track_build_from_tree( *tree, tm ) )
    {
        LOG_ERROR( "track_build: the document did not load or the build was refused" );
        return false;
    }
    int failed = 0, compared = 0;
    auto cmp = [ & ]( const char* what, int a, int b, std::uint32_t ours, std::uint32_t theirs ) {
        ++compared;
        if ( ours != theirs && ++failed <= 12 )
            LOG_ERROR( "track_build %s %d +0x%X: ours 0x%08X retail 0x%08X", what, a, b, ours, theirs );
    };
    if ( tm.waypoints.size( ) != g_golden_track_waypoint_count || tm.sectors.size( ) != g_golden_track_sector_count )
    {
        LOG_ERROR( "track_build: %d waypoints / %d sectors, retail %d / %d", static_cast< int >( tm.waypoints.size( ) ),
            static_cast< int >( tm.sectors.size( ) ), static_cast< int >( g_golden_track_waypoint_count ), static_cast< int >( g_golden_track_sector_count ) );
        return false;
    }
    const auto bits = []( float f ) { std::uint32_t u; std::memcpy( &u, &f, 4 ); return u; };
    for ( std::size_t i = 0; i < tm.waypoints.size( ); ++i )
    {
        const track_waypoint_t& w = tm.waypoints[ i ];
        const unsigned* g = g_golden_track_waypoints + 20 * i;
        const int id = static_cast< int >( i );
        if ( w.entity->m_name != g_golden_track_waypoint_names[ i ] && ++failed <= 12 )
            LOG_ERROR( "track_build waypoint %d is '%s', retail '%s'", id, w.entity->m_name.c_str( ), g_golden_track_waypoint_names[ i ] );
        for ( int k = 0; k < 3; ++k )
        {
            cmp( "waypoint", id, 0x5C + 4 * k, bits( w.right[ k ] ), g[ k ] );
            cmp( "waypoint", id, 0x6C + 4 * k, bits( w.left[ k ] ), g[ 4 + k ] );
        }
        cmp( "waypoint", id, 0x84, bits( w.p.speed_hint ), g[ 10 ] );
        cmp( "waypoint", id, 0x88, static_cast< std::uint32_t >( w.p.start | ( w.p.checkpoint << 8 ) | ( w.p.finish << 16 ) | ( w.p.allow_recovery << 24 ) ),
            g[ 11 ] );
        cmp( "waypoint", id, 0x8C, w.p.show_wrong_way, g[ 12 ] & 0xFF );
        cmp( "waypoint", id, 0x90, bits( w.p.width ), g[ 13 ] );
        cmp( "waypoint", id, 0x94, bits( w.p.height ), g[ 14 ] );
        cmp( "waypoint", id, 0x98, static_cast< std::uint32_t >( w.sector_a ), g[ 15 ] );
        cmp( "waypoint", id, 0x9C, static_cast< std::uint32_t >( w.sector_b ), g[ 16 ] );
        cmp( "waypoint", id, 0xA0, bits( w.p.branch_a ), g[ 17 ] );
        cmp( "waypoint", id, 0xA4, bits( w.p.branch_b ), g[ 18 ] );
    }
    const unsigned* g = g_golden_track_sectors;
    for ( std::size_t i = 0; i < tm.sectors.size( ); ++i )
    {
        const track_sector_t& s = tm.sectors[ i ];
        const int id = static_cast< int >( i );
        const std::uint64_t stale = g[ 60 ] | ( static_cast< std::uint64_t >( g[ 61 ] ) << 32 );
        for ( int k = 0; k < 60; ++k )
        {
            const int at = 4 * k;
            if ( at == 0x0C || at == 0x1C || at == 0x2C || at == 0x3C || at == 0xD4 || at == 0xD8 || at == 0xE0 || at == 0xE4 || at == 0xEC ||
                 ( stale >> k & 1 ) )
                continue;
            cmp( "sector", id, at, track_u32( s.image, at ), g[ k ] );
        }
        cmp( "sector waypoint", id, 0xD4, static_cast< std::uint32_t >( s.waypoint ), g[ 62 ] );
        const unsigned nn = g[ 63 ], np = g[ 64 ];
        cmp( "sector next count", id, 0xDC, static_cast< std::uint32_t >( s.next.size( ) ), nn );
        cmp( "sector prev count", id, 0xE8, static_cast< std::uint32_t >( s.prev.size( ) ), np );
        for ( unsigned k = 0; k < nn && k < s.next.size( ); ++k )
            cmp( "sector next", id, static_cast< int >( k ), static_cast< std::uint32_t >( s.next[ k ] ), g[ 65 + k ] );
        for ( unsigned k = 0; k < np && k < s.prev.size( ); ++k )
            cmp( "sector prev", id, static_cast< int >( k ), static_cast< std::uint32_t >( s.prev[ k ] ), g[ 65 + nn + k ] );
        g += 65 + nn + np;
    }
    cmp( "checkpoint count", 0, 0, static_cast< std::uint32_t >( tm.checkpoints.size( ) ), static_cast< std::uint32_t >( g_golden_track_checkpoint_count ) );
    for ( std::size_t k = 0; k < g_golden_track_checkpoint_count && k < tm.checkpoints.size( ); ++k )
        cmp( "checkpoint", static_cast< int >( k ), 0, static_cast< std::uint32_t >( tm.checkpoints[ k ] ), static_cast< std::uint32_t >( g_golden_track_checkpoints[ k ] ) );
    cmp( "start", 0, 0x14, static_cast< std::uint32_t >( tm.start ), static_cast< std::uint32_t >( g_golden_track_summary[ 0 ] ) );
    cmp( "end", 0, 0x18, static_cast< std::uint32_t >( tm.end ), static_cast< std::uint32_t >( g_golden_track_summary[ 1 ] ) );
    cmp( "length", 0, 0x1C, bits( tm.length ), g_golden_track_manager[ 7 ] );
    cmp( "checkpoint list count", 0, 0x20, static_cast< std::uint32_t >( tm.checkpoints.size( ) ), g_golden_track_manager[ 8 ] );
    cmp( "state", 0, 0x30, static_cast< std::uint32_t >( tm.state ), g_golden_track_manager[ 12 ] );
    cmp( "sector count", 0, 0x4, static_cast< std::uint32_t >( tm.sectors.size( ) ), g_golden_track_manager[ 1 ] );
    cmp( "slot 7", 0, 0, 1u, static_cast< std::uint32_t >( g_golden_track_summary[ 2 ] ) );
    cmp( "slot 8", 0, 0, track_is_built( tm ) ? 1u : 0u, static_cast< std::uint32_t >( g_golden_track_summary[ 3 ] ) );
    cmp( "slot 9", 0, 0, track_is_circuit( tm ) ? 1u : 0u, static_cast< std::uint32_t >( g_golden_track_summary[ 4 ] ) );
    cmp( "slot 11", 0, 0, static_cast< std::uint32_t >( track_start_sector( tm ) ), static_cast< std::uint32_t >( g_golden_track_summary[ 5 ] ) );
    cmp( "slot 12", 0, 0, static_cast< std::uint32_t >( track_end_sector( tm ) ), static_cast< std::uint32_t >( g_golden_track_summary[ 6 ] ) );
    cmp( "slot 14", 0, 0, static_cast< std::uint32_t >( track_first_checkpoint( tm ) ), static_cast< std::uint32_t >( g_golden_track_summary[ 7 ] ) );
    if ( failed == 0 )
    {
        LOG_SUCCESS( "VuTrackManagerImpl (waypoint postLoad 0x432170 / slot 6 0x432400 -> 0x4CFAF0 / 0x4D0F90 / 0x4D0940 / 0x4CFD60, build "
                     "0x4CFFD0, queries) matches retail on Hydroelectric: %d waypoints, %d sectors, %d checkpoints, %d values, bit for bit",
            static_cast< int >( tm.waypoints.size( ) ), static_cast< int >( tm.sectors.size( ) ), static_cast< int >( tm.checkpoints.size( ) ), compared );
        return true;
    }
    LOG_ERROR( "track_build FAILED %d of %d values", failed, compared );
    return false;
}

inline bool verify_boat_all( )
{
    bool ok = true;
    ok = verify_boat_sections( ) && ok;
    ok = verify_boat_engine( ) && ok;
    ok = verify_boat_ai_driver( ) && ok;
    ok = verify_boat_driver_load( ) && ok;
    ok = verify_boat_tick_state( ) && ok;
    ok = verify_boat_jump( ) && ok;
    ok = verify_box_shape( ) && ok;
    ok = verify_cylinder_shape( ) && ok;
    ok = verify_sphere_shape( ) && ok;
    ok = verify_compound_inertia( ) && ok;
    ok = verify_boat_nose_probe( ) && ok;
    ok = verify_boat_load_sections( ) && ok;
    ok = verify_boat_transform( ) && ok;
    ok = verify_boat_boost( ) && ok;
    ok = verify_boat_engine_audio( ) && ok;
    ok = verify_boat_hull_tick( ) && ok;
    ok = verify_boat_decision( ) && ok;
    ok = verify_ai_controls( ) && ok;
    ok = verify_ai_steer( ) && ok;
    ok = verify_ai_path_follow( ) && ok;
    ok = verify_ai_avoid( ) && ok;
    ok = verify_ai_tick( ) && ok;
    ok = verify_track_build( ) && ok;
    return ok;
}
