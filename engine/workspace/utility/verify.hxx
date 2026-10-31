#pragma once

#include <workspace/utility/broad.hxx>
#include <workspace/utility/boat.hxx>
#include <workspace/core/assets/reader.hxx>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Check our maths against vectors captured by RUNNING the retail function.
//
// This is the differential oracle's other half. tools/oracle.py runs the
// shipped code under emulation and records what it produces; this compares
// ours against that. The claim "element for element identical" stops being an
// argument someone made while reading and becomes a number this prints.
//
// EXACT EQUALITY IS NOT THE BAR, and pretending it is would be dishonest.
// Retail computes its sines and cosines with x87 `fsincos`, which keeps 80-bit
// precision internally; ours calls std::sin and std::cos on 32-bit floats
// through SSE. The two agree to within float rounding, not to the bit. So the
// test reports the worst deviation it saw and fails on a threshold far tighter
// than anything that could matter visually, rather than claiming a bit-exact
// match it cannot have.
inline bool verify_math( )
{
    // VuMatrix_fromEuler takes RADIANS and writes only a rotation.
    // math_transform takes DEGREES and composes translation and scale too, so
    // feed it an identity translation and scale and convert the angles back.
    const float to_degrees = 57.295779513f;
    double worst = 0.0;
    int checked = 0;
    int failed = 0;

    for ( std::size_t index = 0; index < g_golden_euler_count; ++index )
    {
        const golden_euler_t& sample = g_golden_euler[ index ];
        const float degrees[ 3 ] = {
            sample.m_euler[ 0 ] * to_degrees,
            sample.m_euler[ 1 ] * to_degrees,
            sample.m_euler[ 2 ] * to_degrees
        };
        const float origin[ 3 ] = { 0.0f, 0.0f, 0.0f };
        const float unit[ 3 ] = { 1.0f, 1.0f, 1.0f };
        float ours[ 16 ]{};
        math_transform( ours, origin, degrees, unit );

        // Compare the 3x3 basis only: retail's function never writes a
        // translation, so the fourth row is ours to set and is not evidence.
        double local_worst = 0.0;
        for ( int row = 0; row < 3; ++row )
        {
            for ( int column = 0; column < 3; ++column )
            {
                const int slot = row * 4 + column;
                const double delta = std::fabs(
                    static_cast< double >( ours[ slot ] )
                    - static_cast< double >( sample.m_matrix[ slot ] ) );
                if ( delta > local_worst )
                    local_worst = delta;
            }
        }
        ++checked;
        if ( local_worst > worst )
            worst = local_worst;
        if ( local_worst > 1.0e-5 )
        {
            ++failed;
            LOG_ERROR( "math_transform differs from VuMatrix_fromEuler at euler %.4f %.4f %.4f by %.3e",
                sample.m_euler[ 0 ], sample.m_euler[ 1 ], sample.m_euler[ 2 ], local_worst );
        }
    }

    if ( failed == 0 )
    {
        LOG_SUCCESS( "math_transform matches VuMatrix_fromEuler @0x435640 on %d vectors, worst deviation %.3e",
            checked, worst );
    }
    else
    {
        LOG_ERROR( "math_transform FAILED %d of %d vectors against VuMatrix_fromEuler", failed, checked );
    }
    return failed == 0;
}

// A value_t in the text tools/oracle.py vujb_dump makes of retail's
// container: tag, payload, children; map entries in std::map order.
inline void vujb_dump( const value_t& value, std::string& out )
{
    char buffer[ 32 ];
    auto hex = [ &out ]( const std::string& text )
    {
        static const char digits[] = "0123456789abcdef";
        for ( const unsigned char c : text )
        {
            out += digits[ c >> 4 ];
            out += digits[ c & 15 ];
        }
    };
    switch ( value.tag )
    {
    case 0:
        out += "0";
        return;
    case 1:
    case 2:
    {
        std::uint32_t bits = static_cast< std::uint32_t >( value.integer );
        if ( value.tag == 2 )
            std::memcpy( &bits, &value.real, sizeof( bits ) );
        std::snprintf( buffer, sizeof( buffer ), "%u:%08x", value.tag, bits );
        out += buffer;
        return;
    }
    case 3:
        std::snprintf( buffer, sizeof( buffer ), "3:%02x", static_cast< unsigned >( value.integer ) );
        out += buffer;
        return;
    case 4:
        out += "4:";
        hex( value.text );
        out += ";";
        return;
    case 5:
        std::snprintf( buffer, sizeof( buffer ), "5:%u[", static_cast< unsigned >( value.list.size( ) ) );
        out += buffer;
        for ( const value_t& item : value.list )
            vujb_dump( item, out );
        out += "]";
        return;
    case 6:
    {
        std::vector< const std::pair< std::string, value_t >* > sorted;
        for ( const auto& field : value.object )
            sorted.push_back( &field );
        std::sort( sorted.begin( ), sorted.end( ), []( const auto* a, const auto* b ) { return a->first < b->first; } );
        std::snprintf( buffer, sizeof( buffer ), "6:%u{", static_cast< unsigned >( sorted.size( ) ) );
        out += buffer;
        for ( const auto* field : sorted )
        {
            hex( field->first );
            out += "=";
            vujb_dump( field->second, out );
        }
        out += "}";
        return;
    }
    case 7:
        std::snprintf( buffer, sizeof( buffer ), "7:%016llx", static_cast< unsigned long long >( value.int64 ) );
        out += buffer;
        return;
    default:
        std::snprintf( buffer, sizeof( buffer ), "?%u", value.tag );
        out += buffer;
        return;
    }
}

// c_reader (the 0x55B560 / 0x55B770 recreation) against retail's own read of
// the same bytes: real Base.apf documents and hand-built ones for the
// branches the pack never takes. Retail's return is not compared directly -
// a failed read leaves its root null, which the dump already shows.
inline bool verify_vujb( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_vujb_count; ++index )
    {
        const unsigned* s = g_golden_vujb_words + g_golden_vujb_offsets[ index ];
        const unsigned size = s[ 1 ];
        const unsigned char* raw = reinterpret_cast< const unsigned char* >( s + 2 );
        const std::vector< std::uint8_t > bytes( raw, raw + size );
        const unsigned* d = s + 2 + ( size + 3 ) / 4;
        const std::string want( reinterpret_cast< const char* >( d + 1 ), d[ 0 ] );
        std::string ours;
        vujb_dump( c_reader( bytes ).parse( ), ours );
        if ( ours != want )
        {
            ++failed;
            std::size_t at = 0;
            while ( at < ours.size( ) && at < want.size( ) && ours[ at ] == want[ at ] )
                ++at;
            LOG_ERROR( "VUJB document %d differs from retail's 0x55B560 at dump offset %d (ours %d chars, retail %d)",
                static_cast< int >( index ), static_cast< int >( at ), static_cast< int >( ours.size( ) ),
                static_cast< int >( want.size( ) ) );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "c_reader builds retail's 0x55B560 / 0x55B770 tree on all %d documents, every node exact",
            static_cast< int >( g_golden_vujb_count ) );
    else
        LOG_ERROR( "c_reader FAILED %d of %d documents against 0x55B560", failed, static_cast< int >( g_golden_vujb_count ) );
    return failed == 0;
}

// member( ) (0x4FBDF0) and vec3( ) (0x4FDC60) against retail's own calls on
// containers retail's 0x55B560 built from the same bytes.
inline bool verify_vujb_lookup( )
{
    int failed = 0, lookups = 0;
    for ( std::size_t index = 0; index < g_golden_vujb_lookup_count; ++index )
    {
        const unsigned* s = g_golden_vujb_lookup_words + g_golden_vujb_lookup_offsets[ index ];
        auto take = [ &s ]( )
        {
            const unsigned size = *s++;
            std::string text( reinterpret_cast< const char* >( s ), size );
            s += ( size + 3 ) / 4;
            return text;
        };
        const std::string raw = take( );
        const value_t root = c_reader( std::vector< std::uint8_t >( raw.begin( ), raw.end( ) ) ).parse( );
        const unsigned keys = *s++;
        for ( unsigned k = 0; k < keys; ++k )
        {
            const std::string key = take( );
            const std::string want = take( );
            const value_t& found = member( root, key.c_str( ) );
            std::string ours;
            if ( &found == &member( value_t{}, "" ) )
                ours = "null";
            else
                vujb_dump( found, ours );
            ++lookups;
            if ( ours != want )
            {
                ++failed;
                LOG_ERROR( "0x4FBDF0 differs on document %d key '%s': ours %s, retail %s", static_cast< int >( index ),
                    key.c_str( ), ours.c_str( ), want.c_str( ) );
            }
        }
        const unsigned ok = *s++;
        float lanes[ 3 ];
        std::memset( lanes, 0x77, sizeof( lanes ) );
        const bool ours_ok = vec3( root, lanes );
        if ( ours_ok != ( ok != 0 ) || std::memcmp( lanes, s, sizeof( lanes ) ) != 0 )
        {
            ++failed;
            unsigned bits[ 3 ];
            std::memcpy( bits, lanes, sizeof( bits ) );
            LOG_ERROR( "0x4FDC60 differs on document %d: ours %d %08x %08x %08x, retail %u %08x %08x %08x",
                static_cast< int >( index ), ours_ok ? 1 : 0, bits[ 0 ], bits[ 1 ], bits[ 2 ], ok, s[ 0 ], s[ 1 ], s[ 2 ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "member / vec3 match retail's 0x4FBDF0 / 0x4FDC60: %d lookups and %d vec3 parses, every lane exact",
            lookups, static_cast< int >( g_golden_vujb_lookup_count ) );
    else
        LOG_ERROR( "member / vec3 FAILED %d checks against 0x4FBDF0 / 0x4FDC60", failed );
    return failed == 0;
}

// Every oracle-backed check. Returns false if any failed, so --selftest can
// set the process exit code and a build script can gate on it.
inline bool verify_all( )
{
    bool ok = true;
    ok = verify_math( ) && ok;
    ok = verify_vujb( ) && ok;
    ok = verify_vujb_lookup( ) && ok;
    ok = verify_solver_row( ) && ok;
    ok = verify_iteration( ) && ok;
    ok = verify_split_iterations( ) && ok;
    ok = verify_solve_iterations( ) && ok;
    ok = verify_rotation( ) && ok;
    ok = verify_writeback( ) && ok;
    ok = verify_finish( ) && ok;
    ok = verify_anisotropic( ) && ok;
    ok = verify_friction_impulse( ) && ok;
    ok = verify_friction_setup( ) && ok;
    ok = verify_add_friction( ) && ok;
    ok = verify_contact_setup( ) && ok;
    ok = verify_convert_contact( ) && ok;
    ok = verify_solver_setup( ) && ok;
    ok = verify_solve_group( ) && ok;
    ok = verify_islands( ) && ok;
    ok = verify_solve_constraints( ) && ok;
    ok = verify_update_activation( ) && ok;
    ok = verify_integrate_transforms( ) && ok;
    ok = verify_solver_info( ) && ok;
    ok = verify_body_step( ) && ok;
    ok = verify_save_kinematic( ) && ok;
    ok = verify_step( ) && ok;
    ok = verify_manifold( ) && ok;
    ok = verify_simplex( ) && ok;
    ok = verify_support( ) && ok;
    ok = verify_mesh_support( ) && ok;
    ok = verify_gjk( ) && ok;
    ok = verify_epa_support( ) && ok;
    ok = verify_epa_project( ) && ok;
    ok = verify_epa_gjk( ) && ok;
    ok = verify_epa_enclose( ) && ok;
    ok = verify_epa_hull( ) && ok;
    ok = verify_epa_evaluate( ) && ok;
    ok = verify_epa_penetration( ) && ok;
    ok = verify_epa_distance( ) && ok;
    ok = verify_pen_depth( ) && ok;
    ok = verify_gjk_epa( ) && ok;
    ok = verify_convex_convex( ) && ok;
    ok = verify_bvh_walk( ) && ok;
    ok = verify_convex_concave( ) && ok;
    ok = verify_compound( ) && ok;
    ok = verify_dispatch( ) && ok;
    ok = verify_collision_stage( ) && ok;
    ok = verify_hull_recalc( ) && ok;
    ok = verify_mesh_recalc( ) && ok;
    ok = verify_world_collide( ) && ok;
    ok = verify_sphere_stage( ) && ok;
    ok = verify_sphere_concave( ) && ok;
    ok = verify_box_stage( ) && ok;
    ok = verify_update_aabb( ) && ok;
    ok = verify_pair_cache( ) && ok;
    ok = verify_dbvt( ) && ok;
    ok = verify_broadphase( ) && ok;
    ok = verify_body_ctor( ) && ok;
    return ok;
}
