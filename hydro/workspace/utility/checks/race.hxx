#pragma once

#include <workspace/core/boat/camera.hxx>
#include <workspace/core/boat/respawn.hxx>
#include <workspace/core/boat/anim.hxx>
#include <workspace/core/boat/human.hxx>
#include <workspace/core/boat/pilots.hxx>
#include <workspace/core/boat/manager.hxx>

#include <algorithm>
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

// The respawn 0x41D790 / 0x41DA90 against retail's own runs on Hydroelectric's track (oracle case
// boat_respawn): the track built by track_build_from_tree (as verify_track_build), each vector's blocks fed to
// boat_respawn with the scripted water answers; every word of the six blocks after (but the stale ones) and the
// hook log (order, the water points, the event hash) compared.
inline bool verify_boat_respawn( )
{
    const std::unique_ptr< entity_t > tree = track_document_tree( );
    track_manager_t tm;
    if ( !tree || !track_build_from_tree( *tree, tm ) )
    {
        LOG_ERROR( "boat_respawn: the track did not build" );
        return false;
    }
    static const int sizes[ 6 ] = { 0x520, 0xC8, 0x2D4, 0x10, 0x308, 0x2C };
    struct run_t
    {
        const unsigned* heights = nullptr;
        unsigned next = 0;
        std::vector< unsigned > log;
    } run;
    boat_respawn_hooks_t hooks;
    hooks.user = &run;
    hooks.water = []( void* u, const float* p ) {
        run_t& r = *static_cast< run_t* >( u );
        r.log.push_back( 1 );
        for ( int k = 0; k < 3; ++k )
        {
            std::uint32_t w;
            std::memcpy( &w, p + k, 4 );
            r.log.push_back( w );
        }
        const unsigned bits = r.next < 8 ? r.heights[ r.next ] : 0xFF7FFFFFu;
        ++r.next;
        return respawn_bits( bits );
    };
    hooks.transform_children = []( void* u ) { static_cast< run_t* >( u )->log.push_back( 2 ); };
    hooks.instigator = []( void* u ) { static_cast< run_t* >( u )->log.push_back( 3 ); };
    hooks.enable = []( void* u ) { static_cast< run_t* >( u )->log.push_back( 6 ); };
    hooks.driver_50 = []( void* u ) { static_cast< run_t* >( u )->log.push_back( 7 ); };
    hooks.recovered = []( void* u, std::uint32_t hash ) {
        run_t& r = *static_cast< run_t* >( u );
        r.log.push_back( 4 );
        r.log.push_back( 5 );
        r.log.push_back( hash );
    };
    const unsigned* g = g_golden_boat_respawn_words;
    int compared = 0, failed = 0, bad = 0;
    const unsigned nv = *g++;
    for ( unsigned v = 0; v < nv; ++v )
    {
        std::vector< std::uint8_t > blocks[ 6 ];
        for ( int k = 0; k < 6; ++k )
        {
            blocks[ k ].assign( reinterpret_cast< const std::uint8_t* >( g ), reinterpret_cast< const std::uint8_t* >( g ) + sizes[ k ] );
            g += sizes[ k ] / 4;
        }
        const bool has_proxy = *g++ != 0;
        const std::int32_t checkpoint = static_cast< std::int32_t >( *g++ );
        run.heights = g;
        run.next = 0;
        run.log.clear( );
        g += 8;
        const unsigned used = *g++;
        vu_rigid_body_t body;
        std::memcpy( &body, blocks[ 2 ].data( ), 0x2D4 );
        boat_respawn_t r;
        r.boat = blocks[ 0 ].data( );
        r.transform = blocks[ 1 ].data( );
        r.body = &body;
        r.proxy = has_proxy ? blocks[ 3 ].data( ) : nullptr;
        r.camera = blocks[ 4 ].data( );
        r.instigator = blocks[ 5 ].data( );
        r.checkpoint = checkpoint;
        boat_respawn( tm, r, hooks );
        std::memcpy( blocks[ 2 ].data( ), &body, 0x2D4 );
        const int failed_before = failed;
        const unsigned* after[ 6 ];
        for ( int k = 0; k < 6; ++k )
        {
            after[ k ] = g;
            g += sizes[ k ] / 4;
        }
        for ( int k = 0; k < 6; ++k )
        {
            std::vector< bool > stale( sizes[ k ] / 4, false );
            const unsigned ns = *g++;
            for ( unsigned j = 0; j < ns; ++j )
                stale[ *g++ ] = true;
            for ( int w = 0; w < sizes[ k ] / 4; ++w )
            {
                if ( stale[ w ] )
                    continue;
                std::uint32_t mine;
                std::memcpy( &mine, blocks[ k ].data( ) + 4 * w, 4 );
                ++compared;
                if ( mine != after[ k ][ w ] && ++failed <= 8 )
                    LOG_ERROR( "boat_respawn vector %u block %d +0x%X: 0x%08X, retail 0x%08X", v, k, 4 * w, mine, after[ k ][ w ] );
            }
        }
        std::vector< unsigned > expect;
        const unsigned nl = *g++;
        for ( unsigned j = 0; j < nl; ++j )
        {
            const unsigned code = *g++;
            expect.push_back( code );
            if ( code == 1 )
                for ( int k = 0; k < 3; ++k )
                    expect.push_back( *g++ );
            else if ( code == 5 )
                expect.push_back( *g++ );
        }
        ++compared;
        if ( ( expect != run.log || used != run.next ) && ++failed <= 8 )
            LOG_ERROR( "boat_respawn vector %u: the hook log differs (%zu words vs retail %zu; water %u vs %u)", v, run.log.size( ), expect.size( ), run.next, used );
        bad += failed != failed_before ? 1 : 0;
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "The respawn 0x41D790 (slot 22, the sector walk, 0x4D29F0) and the reset 0x41DA90 match retail on "
                     "Hydroelectric's track: %u vectors, %d values, bit for bit", nv, compared );
        return true;
    }
    LOG_ERROR( "boat_respawn FAILED %d of %d values (%d of %u vectors)", failed, compared, bad, nv );
    return false;
}

// The animation tick 0x40FFF0 (sequences on one object, retail's state carried) and the wake parameters
// 0x417F80 against retail (oracle case boat_anim): the object's and its controls' words after, the hook and
// FMOD streams; the 0x417F80 out block but its stale words.
inline bool verify_boat_anim( )
{
    const unsigned* g = g_golden_boat_anim_words;
    auto fl = []( unsigned w ) { float v; std::memcpy( &v, &w, 4 ); return v; };
    struct log_t
    {
        float s64 = 0.0f, s68 = 0.0f;
        const unsigned* create = nullptr;
        std::vector< unsigned > a, b;
    } lg;
    boat_anim_hooks_t hooks;
    hooks.user = &lg;
    hooks.driver_64 = []( void* u ) { return static_cast< log_t* >( u )->s64; };
    hooks.driver_68 = []( void* u ) { return static_cast< log_t* >( u )->s68; };
    hooks.control_516300 = []( void* u ) { static_cast< log_t* >( u )->a.push_back( 1 ); };
    hooks.event_create = []( void* u, int slot, const char* ) {
        log_t& l = *static_cast< log_t* >( u );
        const unsigned ok = l.create[ slot ];
        l.a.insert( l.a.end( ), { 2u, static_cast< unsigned >( slot ), ok } );
        return ok != 0;
    };
    hooks.event_start = []( void* u, int ) { static_cast< log_t* >( u )->b.push_back( 3 ); };   // retail starts slot k's event even when it is null
    hooks.voice = []( void* u, const char* ) { static_cast< log_t* >( u )->a.push_back( 4 ); };
    hooks.event_update = []( void* u, int slot, const float* p, const float* v, const float* values ) {
        log_t& l = *static_cast< log_t* >( u );
        l.b.push_back( 5 );
        l.b.push_back( static_cast< unsigned >( slot ) );
        for ( int k = 0; k < 6; ++k )
        {
            std::uint32_t w;
            std::memcpy( &w, k < 3 ? p + k : v + k - 3, 4 );
            l.b.push_back( w );
        }
        for ( int k = 0; k < 3; ++k )
        {
            std::uint32_t w;
            std::memcpy( &w, values + k, 4 );
            l.b.push_back( 6 );
            l.b.push_back( w );
        }
    };
    int compared = 0, failed = 0, ticks = 0, bad = 0;
    auto cmp = [ & ]( const char* what, int a, int b, std::uint32_t ours, std::uint32_t theirs ) {
        ++compared;
        if ( ours != theirs && ++failed <= 8 )
            LOG_ERROR( "boat_anim %s %d +0x%X: 0x%08X, retail 0x%08X", what, a, b, ours, theirs );
    };
    const unsigned ns = *g++;
    for ( unsigned s = 0; s < ns; ++s )
    {
        const unsigned nt = *g++;
        bool events[ 3 ];
        for ( int k = 0; k < 3; ++k )
            events[ k ] = *g++ != 0;
        g += 3;   // the names' lengths (the emitter checked retail's names)
        for ( unsigned t = 0; t < nt; ++t, ++ticks )
        {
            const int failed_before = failed;
            const float dt = fl( *g++ );
            lg.s64 = fl( *g++ );
            lg.s68 = fl( *g++ );
            bool present[ 3 ];
            for ( int k = 0; k < 3; ++k )
                present[ k ] = *g++ != 0;
            std::uint8_t assets[ 3 ][ 0x20 ] = { };
            for ( int k = 0; k < 3; ++k )
                std::memcpy( assets[ k ] + 0x18, g++, 4 );
            std::uint8_t boat[ 0x520 ] = { };
            std::memcpy( boat + 0x310, g++, 4 );
            boat[ 0x32F ] = static_cast< std::uint8_t >( *g++ );
            lg.create = g;
            g += 3;
            float vel[ 4 ], pos[ 4 ];
            std::memcpy( vel, g, 16 );
            g += 4;
            std::memcpy( pos, g, 16 );
            g += 4;
            std::uint8_t engine[ 0x100 ] = { };
            std::memcpy( engine + 0xEC, g, 12 );
            g += 3;
            std::uint8_t obj[ 0xE0 ], ctl[ 3 ][ 0x20 ];
            std::memcpy( obj, g, 0xE0 );
            g += 0xE0 / 4;
            for ( int k = 0; k < 3; ++k, g += 8 )
                std::memcpy( ctl[ k ], g, 0x20 );
            boat_anim_t a;
            a.anim = obj;
            a.boat = boat;
            for ( int k = 0; k < 3; ++k )
            {
                a.control[ k ] = present[ k ] ? ctl[ k ] : nullptr;
                a.asset[ k ] = assets[ k ];
                a.event[ k ] = events[ k ];
            }
            a.position = pos;
            a.velocity = vel;
            a.engine = engine;
            lg.a.clear( );
            lg.b.clear( );
            boat_anim_tick( a, dt, hooks );
            for ( int w = 0; w < 0xE0 / 4; ++w, ++g )
            {
                std::uint32_t mine;
                std::memcpy( &mine, obj + 4 * w, 4 );
                cmp( "object", ticks, 4 * w, mine, *g );
            }
            for ( int k = 0; k < 3; ++k )
                for ( int w = 0; w < 8; ++w, ++g )
                {
                    std::uint32_t mine;
                    std::memcpy( &mine, ctl[ k ] + 4 * w, 4 );
                    cmp( "control", ticks * 10 + k, 4 * w, mine, *g );
                }
            const unsigned na = *g++;
            const std::vector< unsigned > ea( g, g + na );
            g += na;
            const unsigned nb = *g++;
            const std::vector< unsigned > eb( g, g + nb );
            g += nb;
            cmp( "hook stream", ticks, 0, ea == lg.a ? 0u : 1u, 0u );
            cmp( "fmod stream", ticks, 0, eb == lg.b ? 0u : 1u, 0u );
            bad += failed != failed_before ? 1 : 0;
        }
    }
    const unsigned nw = *g++;
    for ( unsigned v = 0; v < nw; ++v )
    {
        float m[ 16 ];
        std::memcpy( m, g, 64 );
        g += 16;
        std::uint8_t hull[ 0xA8 ] = { };
        std::memcpy( hull + 0x98, g, 16 );
        g += 4;
        std::uint8_t body[ 0x160 ] = { };
        std::memcpy( body + 0x140, g, 32 );
        g += 8;
        std::uint8_t solver[ 0x60 ] = { };
        std::memcpy( solver + 0x5C, g++, 4 );
        std::uint8_t out[ 0x30 ] = { };
        boat_wake_params( hull, m, body, solver, out );
        const unsigned* theirs = g;
        g += 12;
        const unsigned stale = *g++;
        for ( int w = 0; w < 12; ++w )
            if ( !( stale >> w & 1 ) )
            {
                std::uint32_t mine;
                std::memcpy( &mine, out + 4 * w, 4 );
                cmp( "wake", static_cast< int >( v ), 4 * w, mine, theirs[ w ] );
            }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "The animation tick 0x40FFF0 (%d ticks in %u sequences) and the wake parameters 0x417F80 (%u vectors) "
                     "match retail: %d values, bit for bit", ticks, ns, nw, compared );
        return true;
    }
    LOG_ERROR( "boat_anim FAILED %d of %d values (%d of %d ticks)", failed, compared, bad, ticks );
    return false;
}

// VuHumanDriver's tick 0x40F270 against retail's own runs on Hydroelectric's track (oracle case human_tick):
// each tick's blocks fed to human_tick with the scripted input, accelerometer and settings; the driver / boat /
// body / camera words after (but the stale ones; the driver's +0x84 as the sector index) and the hook stream
// compared.
inline bool verify_human_tick( )
{
    const std::unique_ptr< entity_t > tree = track_document_tree( );
    track_manager_t tm;
    if ( !tree || !track_build_from_tree( *tree, tm ) )
    {
        LOG_ERROR( "human_tick: the track did not build" );
        return false;
    }
    static const int sizes[ 8 ] = { 0xD0, 0x520, 0xC8, 0x2D4, 0x308, 0x80, 0x1D0, 0x60 };
    static const int after_of[ 4 ] = { 0, 1, 3, 4 };
    struct run_t
    {
        float axes[ 3 ] = { };
        std::uint8_t held = 0, jump = 0, recover = 0;
        float acc[ 4 ] = { };
        bool acc_ok = false;
        std::vector< unsigned > log;
    } run;
    auto code = []( const char* n ) -> unsigned {
        static const char* names[ 6 ] = { "Brake", "Throttle", "Steering", "Boost", "Jump", "Recover" };
        for ( unsigned k = 0; k < 6; ++k )
            if ( std::strcmp( n, names[ k ] ) == 0 )
                return k + 1;
        return 99;
    };
    static unsigned ( *s_code )( const char* ) = nullptr;
    s_code = code;
    human_hooks_t hooks;
    hooks.user = &run;
    hooks.axis = []( void* u, std::int32_t pad, const char* n ) {
        run_t& r = *static_cast< run_t* >( u );
        const unsigned c = s_code( n );
        r.log.insert( r.log.end( ), { 1u, static_cast< unsigned >( pad ), c } );
        return c >= 1 && c <= 3 ? r.axes[ c - 1 ] : 0.0f;
    };
    hooks.held = []( void* u, std::int32_t pad, const char* n ) -> std::uint8_t {
        run_t& r = *static_cast< run_t* >( u );
        const unsigned c = s_code( n );
        r.log.insert( r.log.end( ), { 2u, static_cast< unsigned >( pad ), c } );
        return c == 4 ? r.held : 0;
    };
    hooks.pressed = []( void* u, std::int32_t pad, const char* n ) -> std::uint8_t {
        run_t& r = *static_cast< run_t* >( u );
        const unsigned c = s_code( n );
        r.log.insert( r.log.end( ), { 3u, static_cast< unsigned >( pad ), c } );
        return c == 5 ? r.jump : c == 6 ? r.recover : 0;
    };
    hooks.accel = []( void* u, float* out ) {
        run_t& r = *static_cast< run_t* >( u );
        r.log.push_back( 4 );
        std::memcpy( out, r.acc, 16 );
        return r.acc_ok;
    };
    hooks.rumble = []( void* u, float lo, float hi ) {
        run_t& r = *static_cast< run_t* >( u );
        std::uint32_t a, b;
        std::memcpy( &a, &lo, 4 );
        std::memcpy( &b, &hi, 4 );
        r.log.insert( r.log.end( ), { 5u, 0u, 6u, a, b } );
    };
    hooks.voice = []( void* u, const char* line ) {
        run_t& r = *static_cast< run_t* >( u );
        r.log.insert( r.log.end( ), { 7u, std::strcmp( line, "NeedBoostBeforeJump" ) == 0 ? 1u : std::strcmp( line, "NeedPickUpPowerBoost" ) == 0 ? 2u : 99u } );
    };
    hooks.misfire = []( void* u ) { static_cast< run_t* >( u )->log.push_back( 8 ); };
    hooks.jump_refused = []( void* u ) { static_cast< run_t* >( u )->log.push_back( 9 ); };
    hooks.jump_sound = []( void* u ) { static_cast< run_t* >( u )->log.push_back( 10 ); };
    hooks.menu_change = []( void* u ) {
        run_t& r = *static_cast< run_t* >( u );
        r.log.insert( r.log.end( ), { 11u, 12u } );
    };
    const unsigned* g = g_golden_human_tick_words;
    int compared = 0, failed = 0, bad = 0, ticks = 0;
    const unsigned ns = *g++;
    for ( unsigned s = 0; s < ns; ++s )
    {
        const unsigned nt = *g++;
        for ( unsigned t = 0; t < nt; ++t, ++ticks )
        {
            float dt;
            std::memcpy( &dt, g++, 4 );
            human_env_t env;
            env.control_method = static_cast< std::int32_t >( *g++ );
            env.brake_only_0 = static_cast< std::uint8_t >( *g++ );
            env.brake_only_1 = static_cast< std::uint8_t >( *g++ );
            std::memcpy( &env.tilt_blend, g++, 4 );
            env.rumble = static_cast< std::uint8_t >( *g++ );
            std::memcpy( run.axes, g, 12 );
            g += 3;
            run.held = static_cast< std::uint8_t >( *g++ );
            run.jump = static_cast< std::uint8_t >( *g++ );
            run.recover = static_cast< std::uint8_t >( *g++ );
            std::memcpy( run.acc, g, 16 );
            g += 4;
            run.acc_ok = *g++ != 0;
            int sector = static_cast< std::int32_t >( *g++ );
            const int sector_after = static_cast< std::int32_t >( *g++ );
            std::vector< std::uint8_t > blocks[ 8 ];
            for ( int k = 0; k < 8; ++k )
            {
                blocks[ k ].assign( reinterpret_cast< const std::uint8_t* >( g ), reinterpret_cast< const std::uint8_t* >( g ) + sizes[ k ] );
                g += sizes[ k ] / 4;
            }
            human_pilot_t pilot;
            pilot.driver = blocks[ 0 ].data( );
            pilot.sector = sector;
            human_tick_t tick;
            tick.boat = blocks[ 1 ].data( );
            tick.transform = blocks[ 2 ].data( );
            tick.body = blocks[ 3 ].data( );
            tick.camera = blocks[ 4 ].data( );
            tick.animation = blocks[ 5 ].data( );
            tick.solver = blocks[ 7 ].data( );
            run.log.clear( );
            human_driver_tick( tm, pilot, tick, dt, env, hooks );
            const int failed_before = failed;
            const unsigned* after[ 4 ];
            for ( int k = 0; k < 4; ++k )
            {
                after[ k ] = g;
                g += sizes[ after_of[ k ] ] / 4;
            }
            for ( int k = 0; k < 4; ++k )
            {
                const int n = sizes[ after_of[ k ] ] / 4;
                std::vector< bool > stale( n, false );
                const unsigned nst = *g++;
                for ( unsigned j = 0; j < nst; ++j )
                    stale[ *g++ ] = true;
                if ( k == 0 )
                    stale[ 0x84 / 4 ] = true;   // the sector pointer: compared as the index below
                for ( int w = 0; w < n; ++w )
                {
                    if ( stale[ w ] )
                        continue;
                    std::uint32_t mine;
                    std::memcpy( &mine, blocks[ after_of[ k ] ].data( ) + 4 * w, 4 );
                    ++compared;
                    if ( mine != after[ k ][ w ] && ++failed <= 8 )
                        LOG_ERROR( "human_tick %u/%u block %d +0x%X: 0x%08X, retail 0x%08X", s, t, after_of[ k ], 4 * w, mine, after[ k ][ w ] );
                }
            }
            ++compared;
            if ( pilot.sector != sector_after && ++failed <= 8 )
                LOG_ERROR( "human_tick %u/%u: sector %d, retail %d", s, t, pilot.sector, sector_after );
            std::vector< unsigned > expect;
            const unsigned nl = *g++;
            expect.assign( g, g + nl );
            g += nl;
            ++compared;
            if ( expect != run.log && ++failed <= 8 )
                LOG_ERROR( "human_tick %u/%u: the hook stream differs (%zu words vs retail %zu)", s, t, run.log.size( ), expect.size( ) );
            bad += failed != failed_before ? 1 : 0;
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "VuHumanDriver's tick 0x40F270 (0x40F690 input, 0x40FA90 wrong way, 0x40FC60 + 0x41DEC0, 0x41D6B0, 0x40FD30, "
                     "slot 21) matches retail on Hydroelectric's track: %d ticks, %d values, bit for bit", ticks, compared );
        return true;
    }
    LOG_ERROR( "human_tick FAILED %d of %d values (%d of %d ticks)", failed, compared, bad, ticks );
    return false;
}

// VuGhostDriver's tick 0x40EBA0 against retail on Hydroelectric's track (oracle case ghost_tick): the +0x80
// sector after, for each position and sector before.
inline bool verify_ghost_tick( )
{
    const std::unique_ptr< entity_t > tree = track_document_tree( );
    track_manager_t tm;
    if ( !tree || !track_build_from_tree( *tree, tm ) )
    {
        LOG_ERROR( "ghost_tick: the track did not build" );
        return false;
    }
    const unsigned* g = g_golden_ghost_tick_words;
    const unsigned nv = *g++;
    int failed = 0;
    for ( unsigned v = 0; v < nv; ++v )
    {
        std::uint8_t transform[ 0xC8 ] = { };
        std::memcpy( transform + 0x80, g, 16 );
        g += 4;
        ghost_pilot_t p;
        p.sector = static_cast< std::int32_t >( *g++ );
        const int expect = static_cast< std::int32_t >( *g++ );
        ghost_driver_tick( tm, p, transform );
        if ( p.sector != expect && ++failed <= 8 )
            LOG_ERROR( "ghost_tick vector %u: sector %d, retail %d", v, p.sector, expect );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "VuGhostDriver's tick 0x40EBA0 matches retail on Hydroelectric's track: %u vectors", nv );
        return true;
    }
    LOG_ERROR( "ghost_tick FAILED %d of %u vectors", failed, nv );
    return false;
}

// VuAiUiDriver's tick 0x40E5C0 against retail on Hydroelectric's track (oracle case ui_tick): each tick's blocks,
// plan and VuRandom fed to ui_driver_tick; the driver (but its boat / plan pointers) and boat words after, the plan,
// VuRandom and the hook calls compared.
inline bool verify_ui_tick( )
{
    const std::unique_ptr< entity_t > tree = track_document_tree( );
    track_manager_t tm;
    if ( !tree || !track_build_from_tree( *tree, tm ) )
    {
        LOG_ERROR( "ui_tick: the track did not build" );
        return false;
    }
    static const int sizes[ 7 ] = { 0xD4, 0x520, 0xC8, 0x2D4, 0x40, 0x80, 0x60 };
    static std::vector< unsigned > s_log;
    ai_tick_hooks_t hooks;
    hooks.controls.misfire = []( void* ) { s_log.push_back( 1 ); };
    hooks.controls.voice = []( void*, const char* line ) { s_log.push_back( std::strcmp( line, "NeedPickUpPowerBoost" ) == 0 ? 2u : 99u ); };
    const unsigned* g = g_golden_ui_tick_words;
    int compared = 0, failed = 0, bad = 0, ticks = 0;
    const unsigned ns = *g++;
    for ( unsigned s = 0; s < ns; ++s )
    {
        const unsigned nt = *g++;
        for ( unsigned t = 0; t < nt; ++t, ++ticks )
        {
            float dt;
            std::memcpy( &dt, g++, 4 );
            fleet_random_t rng;
            std::memcpy( &rng, g, 136 );
            g += 34;
            ui_pilot_t p;
            const unsigned np = *g++;
            for ( unsigned j = 0; j < np; ++j )
                p.plan.sectors.push_back( static_cast< std::int32_t >( *g++ ) );
            std::vector< std::uint8_t > blocks[ 7 ];
            for ( int k = 0; k < 7; ++k )
            {
                blocks[ k ].assign( reinterpret_cast< const std::uint8_t* >( g ), reinterpret_cast< const std::uint8_t* >( g ) + sizes[ k ] );
                g += sizes[ k ] / 4;
            }
            p.driver = blocks[ 0 ].data( );
            const std::uint8_t* boats[ 1 ] = { blocks[ 1 ].data( ) };
            ai_tick_t tick;
            tick.boat = blocks[ 1 ].data( );
            tick.transform = blocks[ 2 ].data( );
            tick.body = blocks[ 3 ].data( );
            tick.engine = blocks[ 4 ].data( );
            tick.owner = blocks[ 1 ].data( );
            tick.animation = blocks[ 5 ].data( );
            tick.solver = blocks[ 6 ].data( );
            tick.boats = boats;
            tick.boat_count = 1;
            s_log.clear( );
            ui_driver_tick( tm, p, tick, rng, dt, hooks );
            const int failed_before = failed;
            const unsigned* after[ 2 ] = { g, g + sizes[ 0 ] / 4 };
            g += ( sizes[ 0 ] + sizes[ 1 ] ) / 4;
            for ( int k = 0; k < 2; ++k )
            {
                const int n = sizes[ k ] / 4;
                std::vector< bool > stale( n, false );
                const unsigned nst = *g++;
                for ( unsigned j = 0; j < nst; ++j )
                    stale[ *g++ ] = true;   // the stale stack words (e.g. the path target's w lane, ai.hxx)
                for ( int w = 0; w < n; ++w )
                {
                    // and the boat and the plan's own words (its sectors are compared below), and +0xB4: the path
                    // target's w lane, which 0x40BEC0's first branch copies from a dead stack word (ai.hxx: UNKNOWN) -
                    // here the inner 0x40D5A0's k = dt x 10 left in the same tick's frame, so no stack fill shows it
                    if ( stale[ w ] || ( k == 0 && ( w == 0x5C / 4 || w == 0xB4 / 4 || ( w >= 0x84 / 4 && w <= 0x94 / 4 ) ) ) )
                        continue;
                    std::uint32_t mine;
                    std::memcpy( &mine, blocks[ k ].data( ) + 4 * w, 4 );
                    ++compared;
                    if ( mine != after[ k ][ w ] && ++failed <= 8 )
                        LOG_ERROR( "ui_tick %u/%u block %d +0x%X: 0x%08X, retail 0x%08X", s, t, k, 4 * w, mine, after[ k ][ w ] );
                }
            }
            std::vector< int > plan;
            const unsigned na = *g++;
            for ( unsigned j = 0; j < na; ++j )
                plan.push_back( static_cast< std::int32_t >( *g++ ) );
            const std::int32_t depth = static_cast< std::int32_t >( *g++ );
            ++compared;
            if ( ( plan != p.plan.sectors || depth != p.plan.depth ) && ++failed <= 8 )
                LOG_ERROR( "ui_tick %u/%u: the plan differs (%zu sectors, retail %zu)", s, t, p.plan.sectors.size( ), plan.size( ) );
            ++compared;
            if ( std::memcmp( &rng, g, 136 ) != 0 && ++failed <= 8 )
                LOG_ERROR( "ui_tick %u/%u: VuRandom differs", s, t );
            g += 34;
            const unsigned nl = *g++;
            ++compared;
            if ( std::vector< unsigned >( g, g + nl ) != s_log && ++failed <= 8 )
                LOG_ERROR( "ui_tick %u/%u: the hook calls differ", s, t );
            g += nl;
            bad += failed != failed_before ? 1 : 0;
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "VuAiUiDriver's tick 0x40E5C0 (0x40B940, the plan update 0x40E800, VuRandomTrackPlan) matches retail on "
                     "Hydroelectric's track: %d ticks, %d values, bit for bit", ticks, compared );
        return true;
    }
    LOG_ERROR( "ui_tick FAILED %d of %d values (%d of %d ticks)", failed, compared, bad, ticks );
    return false;
}

// The Human / UI / Nuisance / Ghost driver constructions (boat/driver.hxx) against retail's postLoad branches
// (oracle case boat_drivers): every object's size, every written byte, every pointer by what it points at. The
// human's method-map / event-tree nodes and the DB key temporaries (retail's objects 7..) are not objects of ours:
// its map head (object 1) is compared by bytes only, and the registration by the map node holding the name's hash
// and the delegate, and an event-tree node holding the hash.
inline bool verify_boat_drivers( )
{
    const unsigned* cdb = g_golden_vujb_words + g_golden_vujb_offsets[ 0 ];
    const unsigned char* cdb_raw = reinterpret_cast< const unsigned char* >( cdb + 2 );
    const value_t constant_db = c_reader( std::vector< std::uint8_t >( cdb_raw, cdb_raw + cdb[ 1 ] ) ).parse( );
    const human_driver_graph_t human = human_driver_construct( constant_db );
    const std::vector< image_object_t > mine[ 4 ] = { human.objects, ui_driver_construct( ), nuisance_driver_construct( ), ghost_driver_construct( ) };
    static const char* names[ 4 ] = { "Human", "UI", "Nuisance", "Ghost" };
    const unsigned* s = g_golden_boat_drivers_words;
    const unsigned kinds = *s++;
    int failed = 0, bytes = 0, objects = 0;
    auto fail = [ &failed ]( const char* kind, const char* what, int object, int at ) {
        if ( ++failed <= 8 )
            LOG_ERROR( "boat_drivers %s: %s at object %d, byte %d", kind, what, object, at );
    };
    for ( unsigned kind = 0; kind < kinds && kind < 4; ++kind )
    {
        const std::vector< image_object_t >& g = mine[ kind ];
        const unsigned count = *s++;
        const bool partial = kind == 0;
        if ( partial ? count < g.size( ) : count != g.size( ) )
            fail( names[ kind ], "object count", static_cast< int >( count ), static_cast< int >( g.size( ) ) );
        bool map_node = false, tree_node = false;
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
            if ( partial && k >= g.size( ) )
            {
                std::uint32_t w[ 6 ] = { };
                std::memcpy( w, want, size < 24 ? size : 24 );
                const std::uint32_t hash = human.methods[ 0 ].first;
                if ( size == 24 && w[ 4 ] == hash )
                    for ( const auto& r : refs )
                        map_node = map_node || ( r.slot == 20 && r.object == human.methods[ 0 ].second && r.offset == 0 );
                if ( size == 20 && w[ 2 ] == hash )
                    tree_node = true;
                continue;
            }
            ++objects;
            if ( size != g[ k ].image.size( ) )
            {
                fail( names[ kind ], "size", static_cast< int >( k ), static_cast< int >( g[ k ].image.size( ) ) );
                continue;
            }
            for ( unsigned at = 0; at < size; ++at )
            {
                if ( !mask[ at ] )
                    continue;
                ++bytes;
                if ( g[ k ].image[ at ] != want[ at ] )
                    fail( names[ kind ], "byte", static_cast< int >( k ), static_cast< int >( at ) );
            }
            if ( partial && k == 1 )
                continue;   // the map head: its links go to the nodes
            std::vector< image_ref_t > ours = g[ k ].refs;
            auto by_slot = []( const image_ref_t& x, const image_ref_t& y ) { return x.slot < y.slot; };
            std::sort( ours.begin( ), ours.end( ), by_slot );
            std::sort( refs.begin( ), refs.end( ), by_slot );
            if ( !( ours == refs ) )
                fail( names[ kind ], "pointers", static_cast< int >( k ), 0 );
        }
        if ( partial && !( map_node && tree_node ) )
            fail( names[ kind ], "the OnStartLightsOut registration", -1, 0 );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "postLoad's Human 0x40ED50 / UI 0x40B690 + 0x737CDC / Nuisance 0x41D280 + 0x40D880 / Ghost 0x40EB50 drivers match "
                     "retail: %d objects, %d bytes, every pointer, the human's method registration", objects, bytes );
        return true;
    }
    LOG_ERROR( "boat_drivers FAILED %d checks", failed );
    return false;
}

// VuAiNuisanceDriver's tick 0x40DA90 against retail on Hydroelectric's track (oracle case nuisance_tick): each
// tick's blocks, plan, target sector and VuRandom fed to nuisance_driver_tick with the scripted line list; the
// driver (but its pointer words: the boat, the plan's, the weak reference's - those by being cleared or not - and
// the target's sector, compared as an index; +0xE0 the aim's w lane, a dead stack word) and boat words, the plan,
// VuRandom and the hook stream compared.
inline bool verify_nuisance_tick( )
{
    const std::unique_ptr< entity_t > tree = track_document_tree( );
    track_manager_t tm;
    if ( !tree || !track_build_from_tree( *tree, tm ) )
    {
        LOG_ERROR( "nuisance_tick: the track did not build" );
        return false;
    }
    static const int sizes[ 9 ] = { 0x104, 0x520, 0xC8, 0x2D4, 0x40, 0x80, 0x60, 0xC8, 0x2D4 };
    struct run_t
    {
        int kind = 0, lines = 0;
        std::vector< unsigned > log;
        std::vector< unsigned > handles;
        std::size_t next = 0;
        char names[ 8 ][ 8 ] = { };
    };
    static run_t run;
    nuisance_hooks_t hooks;
    hooks.user = &run;
    hooks.ai.controls.misfire = []( void* ) { run.log.push_back( 1 ); };
    hooks.ai.controls.voice = []( void*, const char* ) { run.log.push_back( 2 ); };
    hooks.release_target = []( void* ) { run.log.push_back( 9 ); };
    hooks.line_count = []( void* ) {
        run.log.push_back( 3 );
        return run.kind == 5 ? run.lines : -1;
    };
    // the case's list: "Line<k>", element 2 not a string, element 4 an empty one
    hooks.line = []( void*, int k ) -> const char* { return k == 2 || k == 4 ? "" : run.names[ k ]; };
    hooks.line_event = []( void*, const char* name ) -> std::uint32_t {
        const std::uint32_t h = run.next < run.handles.size( ) ? run.handles[ run.next++ ] : 0u;
        run.log.insert( run.log.end( ), { 4u, static_cast< unsigned >( name[ 4 ] - '0' ), h } );
        return h;
    };
    hooks.start = []( void*, std::uint32_t e ) { run.log.insert( run.log.end( ), { 5u, e } ); };
    hooks.attributes = []( void*, std::uint32_t e, const float* pos, const float* vel ) {
        run.log.insert( run.log.end( ), { 6u, e } );
        for ( const float* v : { pos, vel } )
            for ( int k = 0; k < 3; ++k )
            {
                std::uint32_t w;
                std::memcpy( &w, v + k, 4 );
                run.log.push_back( w );
            }
    };
    hooks.stop = []( void*, std::uint32_t e ) { run.log.insert( run.log.end( ), { 8u, e } ); };
    for ( int k = 0; k < 8; ++k )
        std::snprintf( run.names[ k ], 8, "Line%d", k );
    const unsigned* g = g_golden_nuisance_tick_words;
    int compared = 0, failed = 0, bad = 0, ticks = 0;
    const unsigned ns = *g++;
    for ( unsigned s = 0; s < ns; ++s )
    {
        const unsigned nt = *g++;
        run.kind = static_cast< int >( *g++ );
        run.lines = static_cast< int >( *g++ );
        for ( unsigned t = 0; t < nt; ++t, ++ticks )
        {
            float dt;
            std::memcpy( &dt, g++, 4 );
            fleet_random_t rng;
            std::memcpy( &rng, g, 136 );
            g += 34;
            nuisance_pilot_t p;
            const unsigned np = *g++;
            for ( unsigned j = 0; j < np; ++j )
                p.plan.sectors.push_back( static_cast< std::int32_t >( *g++ ) );
            p.target_sector = static_cast< std::int32_t >( *g++ );
            std::vector< std::uint8_t > blocks[ 9 ];
            for ( int k = 0; k < 9; ++k )
            {
                blocks[ k ].assign( reinterpret_cast< const std::uint8_t* >( g ), reinterpret_cast< const std::uint8_t* >( g ) + sizes[ k ] );
                g += sizes[ k ] / 4;
            }
            p.driver = blocks[ 0 ].data( );
            p.target = human_i32( p.driver, 0xF0 ) != 0;
            const std::uint8_t* boats[ 1 ] = { blocks[ 1 ].data( ) };
            ai_tick_t tick;
            tick.boat = blocks[ 1 ].data( );
            tick.transform = blocks[ 2 ].data( );
            tick.body = blocks[ 3 ].data( );
            tick.engine = blocks[ 4 ].data( );
            tick.owner = blocks[ 1 ].data( );
            tick.animation = blocks[ 5 ].data( );
            tick.solver = blocks[ 6 ].data( );
            tick.boats = boats;
            tick.boat_count = 1;
            nuisance_target_t target;
            target.transform = blocks[ 7 ].data( );
            target.body = blocks[ 8 ].data( );
            const unsigned* after[ 2 ] = { g, g + sizes[ 0 ] / 4 };
            g += ( sizes[ 0 ] + sizes[ 1 ] ) / 4;
            std::vector< int > plan;
            const unsigned na = *g++;
            for ( unsigned j = 0; j < na; ++j )
                plan.push_back( static_cast< std::int32_t >( *g++ ) );
            const int target_sector = static_cast< std::int32_t >( *g++ );
            const unsigned* rng_after = g;
            g += 34;
            const unsigned nl = *g++;
            const std::vector< unsigned > expect( g, g + nl );
            g += nl;
            run.log.clear( );
            run.handles.clear( );
            run.next = 0;
            for ( std::size_t j = 0; j < expect.size( ); )
            {
                const unsigned c = expect[ j ];
                if ( c == 4 )
                    run.handles.push_back( expect[ j + 2 ] );
                j += c == 4 ? 3 : c == 5 || c == 8 ? 2 : c == 6 ? 8 : 1;
            }
            nuisance_driver_tick( tm, p, tick, target, rng, dt, hooks );
            const int failed_before = failed;
            for ( int k = 0; k < 2; ++k )
            {
                for ( int w = 0; w < sizes[ k ] / 4; ++w )
                {
                    const int at = 4 * w;
                    if ( k == 0 && ( at == 0x5C || ( at >= 0x84 && at <= 0x94 ) || at == 0xE0 || at == 0xF4 ||
                                     ( ( at == 0xE8 || at == 0xEC || at == 0xF0 ) && after[ 0 ][ w ] != 0 ) ) )
                        continue;
                    std::uint32_t mine;
                    std::memcpy( &mine, blocks[ k ].data( ) + at, 4 );
                    ++compared;
                    if ( mine != after[ k ][ w ] && ++failed <= 8 )
                        LOG_ERROR( "nuisance_tick %u/%u block %d +0x%X: 0x%08X, retail 0x%08X", s, t, k, at, mine, after[ k ][ w ] );
                }
            }
            compared += 4;
            if ( ( plan != p.plan.sectors || target_sector != p.target_sector || p.target != ( after[ 0 ][ 0xF0 / 4 ] != 0 ) ) && ++failed <= 8 )
                LOG_ERROR( "nuisance_tick %u/%u: plan / target sector %d (retail %d) / target differ", s, t, p.target_sector, target_sector );
            if ( std::memcmp( &rng, rng_after, 136 ) != 0 && ++failed <= 8 )
                LOG_ERROR( "nuisance_tick %u/%u: VuRandom differs", s, t );
            if ( expect != run.log && ++failed <= 8 )
                LOG_ERROR( "nuisance_tick %u/%u: the hook stream differs (%zu words vs retail %zu)", s, t, run.log.size( ), expect.size( ) );
            bad += failed != failed_before ? 1 : 0;
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "VuAiNuisanceDriver's tick 0x40DA90 (0x40E060, 0x40E280, the line pick) matches retail on Hydroelectric's track: "
                     "%d ticks, %d values, bit for bit", ticks, compared );
        return true;
    }
    LOG_ERROR( "nuisance_tick FAILED %d of %d values (%d of %d ticks)", failed, compared, bad, ticks );
    return false;
}

// The drivers' slots 23 / 24 / 25 / 26 against retail (oracle case driver_slots): the x87 answers bit for bit, the
// sector answers by what they point at.
inline bool verify_driver_slots( )
{
    const unsigned* g = g_golden_driver_slots_words;
    const unsigned nv = *g++;
    static const int sizes[ 5 ] = { 0x104, 0x520, 0xC8, 0x2D4, 0x40 };
    int failed = 0;
    for ( unsigned v = 0; v < nv; ++v )
    {
        std::vector< std::uint8_t > blocks[ 5 ];
        for ( int k = 0; k < 5; ++k )
        {
            blocks[ k ].assign( reinterpret_cast< const std::uint8_t* >( g ), reinterpret_cast< const std::uint8_t* >( g ) + sizes[ k ] );
            g += sizes[ k ] / 4;
        }
        const unsigned n = *g++, first = *g++;
        const std::uint8_t* boat = blocks[ 1 ].data( );
        auto bits = []( float x ) { std::uint32_t u; std::memcpy( &u, &x, 4 ); return u; };
        fleet_plan_t plan;
        for ( unsigned k = 0; k < n; ++k )
            plan.sectors.push_back( k == 0 && first ? 7 : k == 0 ? -1 : 8 );
        human_pilot_t hp;
        hp.sector = 5;
        ghost_pilot_t gp;
        gp.sector = 6;
        const float ai = ai_driver_throttle( blocks[ 0 ].data( ), boat, blocks[ 4 ].data( ), boat );
        const unsigned mine[ 7 ] = { bits( driver_steer_value( boat ) ), bits( driver_entity_throttle( boat, blocks[ 2 ].data( ), blocks[ 4 ].data( ), blocks[ 3 ].data( ) ) ),
                                     bits( ai ), bits( ai ), ai_driver_sector( plan ) >= 0 ? 1u : 0u, human_driver_sector( hp ) == 5 ? 1u : 0u,
                                     ghost_driver_sector( gp ) == 6 ? 1u : 0u };
        static const char* names[ 7 ] = { "slot 26", "slot 24 0x40EB00", "AI slot 24", "AI slot 25", "AI slot 23", "human slot 23", "ghost slot 23" };
        for ( int k = 0; k < 7; ++k, ++g )
            if ( mine[ k ] != *g && ++failed <= 8 )
                LOG_ERROR( "driver_slots vector %u %s: 0x%08X, retail 0x%08X", v, names[ k ], mine[ k ], *g );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "The drivers' slots 26 0x40EB20, 24 / 25 0x40EB00, AI 24 0x40BD40 / 25 0x40BDD0, 23 0x40BD20 / 0x40ED30 / 0x40EB40 "
                     "match retail: %u vectors x 7", nv );
        return true;
    }
    LOG_ERROR( "driver_slots FAILED %d of %u values", failed, nv * 7 );
    return false;
}

// VuBoatManager's add 0x4AAF60 / remove 0x4AB040 against retail (oracle case boat_manager): the driver's kind
// from its vtable (boat_driver_kind), every operation replayed, then +0x58 and the five lists - sizes, the
// capacities retail's growth keeps, the entries in order.
inline bool verify_boat_manager( )
{
    const unsigned* g = g_golden_boat_manager_words;
    const unsigned ns = *g++;
    int failed = 0, compared = 0, ops = 0;
    for ( unsigned s = 0; s < ns; ++s )
    {
        const unsigned nb = *g++;
        std::vector< boat_driver_kind_t > kinds( nb );
        std::vector< int > ids( nb );
        for ( unsigned i = 0; i < nb; ++i, ++g )
        {
            ids[ i ] = static_cast< int >( i );
            if ( !boat_driver_kind( *g, kinds[ i ] ) && ++failed <= 8 )
                LOG_ERROR( "boat_manager sequence %u: no kind for the vtable 0x%X", s, *g );
        }
        boat_manager_t m;
        const unsigned no = *g++;
        for ( unsigned o = 0; o < no; ++o, ++ops )
        {
            const unsigned op = *g++, b = *g++, has_pre = *g++;
            const std::int32_t pre = static_cast< std::int32_t >( *g++ );
            if ( has_pre )
                m.viewport = pre;
            if ( op == 0 )
                boat_manager_add( m, &ids[ b ], kinds[ b ] );
            else
                boat_manager_remove( m, &ids[ b ] );
            ++compared;
            if ( static_cast< unsigned >( m.viewport ) != *g && ++failed <= 8 )
                LOG_ERROR( "boat_manager sequence %u op %u: viewport %d, retail %d", s, o, m.viewport, static_cast< int >( *g ) );
            ++g;
            const boat_manager_list_t* lists[ 5 ] = { &m.all, &m.racers, &m.humans, &m.ais, &m.ghosts };
            for ( int k = 0; k < 5; ++k )
            {
                const unsigned size = *g++, cap = *g++;
                compared += 2;
                if ( lists[ k ]->boats.size( ) != size && ++failed <= 8 )
                    LOG_ERROR( "boat_manager sequence %u op %u list %d: size %zu, retail %u", s, o, k, lists[ k ]->boats.size( ), size );
                if ( static_cast< unsigned >( lists[ k ]->capacity ) != cap && ++failed <= 8 )
                    LOG_ERROR( "boat_manager sequence %u op %u list %d: capacity %d, retail %u", s, o, k, lists[ k ]->capacity, cap );
                for ( unsigned j = 0; j < size; ++j, ++g )
                {
                    ++compared;
                    const void* want = *g < nb ? &ids[ *g ] : nullptr;
                    if ( ( j >= lists[ k ]->boats.size( ) || lists[ k ]->boats[ j ] != want ) && ++failed <= 8 )
                        LOG_ERROR( "boat_manager sequence %u op %u list %d entry %u differs (retail boat %u)", s, o, k, j, *g );
                }
            }
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "VuBoatManager add 0x4AAF60 / remove 0x4AB040 (+0x415690, 0x4AB7D0) match retail: %d operations, %d values", ops, compared );
        return true;
    }
    LOG_ERROR( "boat_manager FAILED %d of %d values", failed, compared );
    return false;
}

inline bool verify_race_all( )
{
    bool ok = true;
    ok = verify_boat_camera( ) && ok;
    ok = verify_boat_respawn( ) && ok;
    ok = verify_boat_anim( ) && ok;
    ok = verify_human_tick( ) && ok;
    ok = verify_ghost_tick( ) && ok;
    ok = verify_ui_tick( ) && ok;
    ok = verify_boat_drivers( ) && ok;
    ok = verify_nuisance_tick( ) && ok;
    ok = verify_driver_slots( ) && ok;
    ok = verify_boat_manager( ) && ok;
    return ok;
}
