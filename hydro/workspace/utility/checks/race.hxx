#pragma once

#include <workspace/core/boat/camera.hxx>
#include <workspace/core/boat/respawn.hxx>
#include <workspace/core/boat/anim.hxx>
#include <workspace/core/boat/human.hxx>

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

inline bool verify_race_all( )
{
    bool ok = true;
    ok = verify_boat_camera( ) && ok;
    ok = verify_boat_respawn( ) && ok;
    ok = verify_boat_anim( ) && ok;
    ok = verify_human_tick( ) && ok;
    return ok;
}
