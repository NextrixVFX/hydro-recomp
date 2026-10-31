#pragma once

#include <workspace/core/collision/box.hxx>
#include <workspace/core/boat/driver.hxx>
#include <workspace/core/boat/sections.hxx>
#include <workspace/core/boat/state.hxx>

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
    return ok;
}
