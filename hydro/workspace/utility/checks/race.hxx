#pragma once

#include <workspace/core/boat/camera.hxx>

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

// The boat area's race-time checks (the camera, the respawn, the animation tick, the drivers): included by
// check.hxx after checks/boat.hxx. verify_race_all runs them all.

// VuBoatCamera_tick 0x409900 against retail's own runs (oracle case boat_camera): sequences of ticks on one
// camera image (retail's ctor 0x409750 plus authored fields), each tick's boat / env / scripted ray hits fed
// to boat_camera_tick and the camera image after compared word for word (but the stale-stack words the
// case's two stack fills disagree on), plus every ray's from / to.
inline bool verify_boat_camera( )
{
    const unsigned* g = g_golden_boat_camera_words;
    struct ray_script_t
    {
        std::vector< std::vector< ai_ray_hit_t > > rays;
        std::vector< std::array< float, 8 > > seen;
        std::size_t next = 0;
    } script;
    auto ray = []( void* user, const float* from, const float* to, const ai_ray_hit_t** hits ) -> int
    {
        ray_script_t& s = *static_cast< ray_script_t* >( user );
        std::array< float, 8 > rec{ from[ 0 ], from[ 1 ], from[ 2 ], 0.0f, to[ 0 ], to[ 1 ], to[ 2 ], 0.0f };
        s.seen.push_back( rec );
        if ( s.next >= s.rays.size( ) )
            return 0;
        std::vector< ai_ray_hit_t >& h = s.rays[ s.next++ ];
        *hits = h.data( );
        return static_cast< int >( h.size( ) );
    };
    auto u = []( float v ) { std::uint32_t w; std::memcpy( &w, &v, 4 ); return w; };
    auto fl = []( unsigned w ) { float v; std::memcpy( &v, &w, 4 ); return v; };
    int compared = 0, failed = 0, ticks = 0, bad_ticks = 0;
    const unsigned ns = *g++;
    for ( unsigned s = 0; s < ns; ++s )
    {
        const unsigned nt = *g++;
        std::uint8_t cam[ 0x308 ];
        std::memcpy( cam, g, 0x308 );
        g += 0x308 / 4;
        for ( unsigned t = 0; t < nt; ++t, ++ticks )
        {
            const float dt = fl( *g++ );
            float world[ 16 ];
            std::memcpy( world, g, 64 );
            g += 16;
            camera_boat_t boat;
            boat.world = world;
            boat.destroyed = *g++ != 0;
            boat.debris_count = static_cast< std::int32_t >( *g++ );
            std::uint8_t piece[ 0x1C0 ];
            std::memcpy( piece, g, 0x1C0 );
            g += 0x1C0 / 4;
            boat.piece = piece;
            float vel[ 4 ];
            std::memcpy( vel, g, 16 );
            g += 4;
            boat.velocity = vel;
            std::uint8_t engine[ 0x10 ] = { };
            std::memcpy( engine + 4, g, 4 );
            std::memcpy( engine + 0xC, g + 1, 4 );
            boat.engine = engine;
            boat.engine_owner_470 = fl( g[ 2 ] );
            boat.far_plane = fl( g[ 3 ] );
            g += 4;
            camera_env_t env;
            env.driver_34 = *g++ != 0;
            env.control_method = static_cast< std::int32_t >( *g++ );
            env.display_w = static_cast< std::int32_t >( *g++ );
            env.display_h = static_cast< std::int32_t >( *g++ );
            env.vertical_fov.type = static_cast< std::int32_t >( *g++ );
            env.vertical_fov.payload = g[ 0 ] | ( static_cast< std::uint64_t >( g[ 1 ] ) << 32 );
            g += 2;
            const unsigned nb = *g++;
            std::vector< std::vector< std::uint8_t > > bodies( nb );
            for ( unsigned k = 0; k < nb; ++k, g += 0x300 / 4 )
                bodies[ k ].assign( reinterpret_cast< const std::uint8_t* >( g ), reinterpret_cast< const std::uint8_t* >( g ) + 0x300 );
            script.rays.assign( 3, { } );
            script.seen.clear( );
            script.next = 0;
            for ( int r = 0; r < 3; ++r )
            {
                const unsigned nh = *g++;
                for ( unsigned j = 0; j < nh; ++j, g += 7 )
                {
                    ai_ray_hit_t h;
                    h.body = bodies[ g[ 0 ] ].data( );
                    h.fraction = fl( g[ 1 ] );
                    std::memcpy( h.normal, g + 2, 16 );
                    h.world = g[ 6 ] != 0;
                    script.rays[ r ].push_back( h );
                }
            }
            env.user = &script;
            env.ray = ray;
            // the shake's eye / target w lanes are a stack slot the mode function's frame left (UNKNOWN here)
            const bool shook = state_f( cam, 0x2E8 ) > 0.0f;
            const int failed_before = failed;
            const bool ran = boat_camera_tick( cam, boat, env, dt );
            const unsigned* after = g;
            g += 0x308 / 4;
            const unsigned* mask = g;
            g += 7;
            ++compared;
            if ( !ran )
            {
                ++failed;
                if ( failed <= 8 )
                    LOG_ERROR( "boat_camera seq %u tick %u: the tick refused (mode UNKNOWN)", s, t );
            }
            for ( int w = 0; w < 0x308 / 4; ++w )
            {
                if ( mask[ w / 32 ] >> ( w % 32 ) & 1 )
                    continue;
                if ( shook && ( 4 * w == 0x0C || 4 * w == 0x1C || 4 * w == 0x2C0 ) )
                    continue;
                std::uint32_t mine;
                std::memcpy( &mine, cam + 4 * w, 4 );
                ++compared;
                if ( mine != after[ w ] )
                {
                    ++failed;
                    if ( failed <= 8 )
                        LOG_ERROR( "boat_camera seq %u tick %u +0x%X: 0x%08X, retail 0x%08X", s, t, 4 * w, mine, after[ w ] );
                }
            }
            // carry retail's state (the stale words included) into the next tick, as the case does
            std::memcpy( cam, after, 0x308 );
            const unsigned nc = *g++;
            ++compared;
            if ( nc != script.seen.size( ) )
            {
                ++failed;
                if ( failed <= 8 )
                    LOG_ERROR( "boat_camera seq %u tick %u: %zu rays, retail %u", s, t, script.seen.size( ), nc );
            }
            for ( unsigned c = 0; c < nc; ++c, g += 8 )
                for ( int k = 0; k < 8 && c < script.seen.size( ); ++k )
                {
                    if ( k == 3 || k == 7 )
                        continue;
                    ++compared;
                    if ( u( script.seen[ c ][ k ] ) != g[ k ] )
                    {
                        ++failed;
                        if ( failed <= 8 )
                            LOG_ERROR( "boat_camera seq %u tick %u ray %u lane %d: 0x%08X, retail 0x%08X", s, t, c, k, u( script.seen[ c ][ k ] ), g[ k ] );
                    }
                }
            bad_ticks += failed != failed_before ? 1 : 0;
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "VuBoatCamera_tick 0x409900 (the five modes 0x409D00 / 0x40A4D0 / 0x40AAC0 / 0x40AC10 / 0x40AD60, the "
                     "shake 0x40AEB0, the collision 0x40B090, the projection 0x50BBE0 / view 0x50BDA0 / frustum "
                     "0x50C550 + 0x57CBA0) matches retail: %d ticks in %u sequences, %d values, bit for bit",
            ticks, ns, compared );
        return true;
    }
    LOG_ERROR( "boat_camera FAILED %d of %d values (%d of %d ticks)", failed, compared, bad_ticks, ticks );
    return false;
}

inline bool verify_race_all( )
{
    bool ok = true;
    ok = verify_boat_camera( ) && ok;
    return ok;
}
