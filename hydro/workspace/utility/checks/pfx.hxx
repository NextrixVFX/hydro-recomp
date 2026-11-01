#pragma once

#include <workspace/core/world/pfxclass.hxx>
#include <workspace/core/world/pfxsim.hxx>
#include <workspace/core/world/pfxload.hxx>

#include <cstring>
#include <map>
#include <vector>

// The water area's particle checks (included by checks/water.hxx before verify_water_all, which runs
// verify_pfx_all). Golden tables come from hydro-tools oracle/cases/water.py through emit/water.py.

// The fountain instances' slot 2 (world/pfxclass.hxx) against retail (case fountain_init): 0x55E100 quad,
// 0x55E9B0 directional quad, 0x55E640 geom, on random particles, params, system velocities and slot 5
// matrices, the global VuRand seeded per vector. Compared: the particle's 32 words after (but the one word
// each of 0x55E9B0 / 0x55E640 takes from an unwritten stack word: +0x24 / +0x50, UNKNOWN), the VuRand's 34
// words after, and the number of matrix fetches.
inline bool verify_fountain_init( )
{
    int failed = 0, words = 0;
    const unsigned* s = g_golden_fin_words;
    for ( std::size_t index = 0; index < g_golden_fin_count; ++index, s += 169 )
    {
        const unsigned kind = s[ 0 ];
        pfx_rand_t rand;
        rand.seed = static_cast< std::int32_t >( s[ 1 ] );
        rand.previous = static_cast< std::int32_t >( s[ 2 ] );
        for ( int k = 0; k < 32; ++k )
            rand.table[ k ] = static_cast< std::int32_t >( s[ 3 + k ] );
        std::uint8_t params[ 0x100 ]{};
        std::memcpy( params + 0x98, s + 35, 64 );
        float svel[ 3 ], matrix[ 16 ];
        std::memcpy( svel, s + 51, 12 );
        std::memcpy( matrix, s + 54, 64 );
        const unsigned* before = s + 70;
        const unsigned* after = s + 102;
        const unsigned* rng_after = s + 134;
        const unsigned fetches = s[ 168 ];
        pfx_quad_particle_t p;
        std::memcpy( p.w, before, sizeof( p.w ) );
        unsigned got_fetches = 0;
        if ( kind == 0 )
            pfx_quad_fountain_init( rand, params, p );
        else if ( kind == 1 )
            pfx_directional_fountain_init( rand, params, svel, matrix, p ), got_fetches = 1;
        else
            pfx_geom_fountain_init( rand, params, matrix, p ), got_fetches = 1;
        const int unknown = kind == 1 ? 0x24 / 4 : kind == 2 ? 0x50 / 4 : -1;
        int bad = 0;
        for ( int k = 0; k < 30; ++k )
        {
            if ( k == unknown )
                continue;
            ++words;
            if ( p.w[ k ] != after[ k ] )
            {
                if ( bad < 3 )
                    LOG_ERROR( "fountain_init vector %d kind %u word +0x%X: 0x%08X, retail 0x%08X", static_cast< int >( index ), kind, 4 * k,
                        p.w[ k ], after[ k ] );
                ++bad;
            }
        }
        // a particle is 30 words in this model; retail's words +0x78 / +0x7C stay as they were
        bad += ( after[ 30 ] != before[ 30 ] || after[ 31 ] != before[ 31 ] ) ? 1 : 0;
        const std::uint32_t r[ 34 ] = { static_cast< std::uint32_t >( rand.seed ), static_cast< std::uint32_t >( rand.previous ) };
        for ( int k = 0; k < 34; ++k )
        {
            const std::uint32_t v = k < 2 ? r[ k ] : static_cast< std::uint32_t >( rand.table[ k - 2 ] );
            ++words;
            bad += v != rng_after[ k ] ? 1 : 0;
        }
        bad += got_fetches != fetches ? 1 : 0;
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "fountain_init vector %d (kind %u): %d differences", static_cast< int >( index ), kind, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "pfx_quad / directional / geom_fountain_init match the fountain instances' slot 2 0x55E100 / 0x55E9B0 / 0x55E640 "
                     "on %d vectors, %d words (particle + VuRand) exact",
            static_cast< int >( g_golden_fin_count ), words );
    else
        LOG_ERROR( "fountain_init FAILED %d of %d", failed, static_cast< int >( g_golden_fin_count ) );
    return failed == 0;
}

// The pattern instances' draw slots (world/pfxclass.hxx) against retail (case pattern_draw): geom 0x5903A0 / 0x5906A0,
// quad 0x5920B0, trail 0x58F9C0 with the model draws 0x520130 / 0x520670 and the renderers 0x58BD60 / 0x58D030 recorded.
// Rebuilt here from the recreation: every call in order - the model matrix's 16 words, the colour retail stored at
// params +0x74 and the +0x78 byte, the list element, the pass argument, the renderer and its arguments.
inline bool verify_pattern_draw( )
{
    int failed = 0, calls = 0;
    const unsigned* s = g_golden_pdr_words;
    float identity[ 16 ];
    std::memcpy( identity, s, 64 );
    s += 16;
    for ( std::size_t index = 0; index < g_golden_pdr_count; ++index )
    {
        const unsigned kind = s[ 0 ], n = s[ 1 ], space = s[ 2 ];
        float matrix[ 16 ], tint[ 4 ], scale;
        std::memcpy( matrix, s + 3, 64 );
        std::memcpy( &scale, s + 19, 4 );
        std::memcpy( tint, s + 20, 16 );
        const unsigned flag = s[ 24 ], asset = s[ 25 ], nel = s[ 26 ], arg = s[ 27 ];
        s += 28;
        std::vector< pfx_quad_particle_t > parts( n );
        for ( unsigned i = 0; i < n; ++i, s += 25 )
            std::memcpy( parts[ i ].w, s, 100 );
        std::vector< std::vector< unsigned > > want;
        const unsigned ncalls = *s++;
        for ( unsigned c = 0; c < ncalls; ++c )
        {
            want.emplace_back( s + 1, s + 1 + s[ 0 ] );
            s += 1 + s[ 0 ];
        }
        std::vector< std::vector< unsigned > > got;
        const float* d = space == 1u ? matrix : identity;
        auto words = [ & ]( std::vector< unsigned >& v, const float* m ) {
            for ( int k = 0; k < 16; ++k )
            {
                unsigned u;
                std::memcpy( &u, m + k, 4 );
                v.push_back( u );
            }
        };
        if ( kind == 0u || kind == 1u )
        {
            for ( const pfx_quad_particle_t& q : parts )
            {
                float m[ 16 ];
                pfx_geom_draw_matrix( q, d, scale, m );
                if ( kind == 0u )
                {
                    std::vector< unsigned > v{ 0u, 0x5Cu, arg };
                    words( v, m );
                    v.push_back( pfx_geom_draw_colour( q, tint ) );
                    v.push_back( flag );
                    got.push_back( v );
                }
                else
                {
                    for ( unsigned e = 0; asset != 0u && e < nel; ++e )
                    {
                        std::vector< unsigned > v{ 1u, 0x5Cu, 0x1001u + e, arg };   // the case's element k has the value 0x1001 + k
                        words( v, m );
                        got.push_back( v );
                    }
                }
            }
        }
        else if ( pfx_pattern_submits( n ) )
        {
            got.push_back( { kind, kind == 2u ? 0xCu : 0x10u, arg, 0u } );
        }
        calls += static_cast< int >( want.size( ) );
        if ( got != want )
        {
            ++failed;
            int call = -1, word = -1;
            for ( std::size_t c = 0; call < 0 && c < got.size( ) && c < want.size( ); ++c )
                for ( std::size_t k = 0; k < got[ c ].size( ) && k < want[ c ].size( ); ++k )
                    if ( got[ c ][ k ] != want[ c ][ k ] )
                    {
                        call = static_cast< int >( c ), word = static_cast< int >( k );
                        break;
                    }
            LOG_ERROR( "pattern_draw vector %d (kind %u): %d calls, retail %d; first difference call %d word %d", static_cast< int >( index ),
                kind, static_cast< int >( got.size( ) ), static_cast< int >( want.size( ) ), call, word );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "pfx_geom_draw_matrix / pfx_geom_draw_colour / pfx_pattern_submits match the draw slots 0x5903A0 / 0x5906A0 / "
                     "0x5920B0 / 0x58F9C0 on %d vectors, %d draw calls word for word",
            static_cast< int >( g_golden_pdr_count ), calls );
    else
        LOG_ERROR( "pattern_draw FAILED %d of %d", failed, static_cast< int >( g_golden_pdr_count ) );
    return failed == 0;
}

// The VuPfx pools and the system lifecycle (world/pfxsim.hxx) against retail's memory (case pfx_lifecycle): the
// resources' ctor + init, then a plan of createSystemInstance / start / allocateParticle / tick / stop / release /
// VuPfxImpl tick on hand-built node trees (real node vtables); the pattern ticks are the case's stubs, replayed. After
// the init and after every operation: the 25 resource words and every block word (but the system AABB pads +0x8C /
// +0x9C and the pattern AABB pads +0x38 / +0x48, stack words), every return, every pattern tick's pattern and dt.
struct pfx_check_sim_t
{
    std::map< std::uint32_t, std::vector< std::uint32_t > > kids;
    const unsigned* ticks = nullptr;
    int nticks = 0, bad = 0;
};

inline bool verify_pfx_lifecycle( )
{
    const unsigned* s = g_golden_plc_words;
    const std::uint32_t res = s[ 0 ];
    const std::uint32_t blocks[ 4 ] = { s[ 1 ], s[ 2 ], s[ 3 ], s[ 4 ] };
    std::uint32_t pairs[ 8 ];
    std::memcpy( pairs, s + 5, 32 );
    s += 13 + 16;
    for ( int k = 1; k < 8; k += 2 )
        pairs[ k ] = ( pairs[ k ] + 15u ) & 0xFFFFFFF0u;   // the block sizes as 0x58B9A0 rounds them (the regions)
    pfx_heap_t h;
    h.add( res, 0x64 );
    for ( int k = 0; k < 4; ++k )
        h.add( blocks[ k ], pairs[ 2 * k ] * pairs[ 2 * k + 1 ] );
    pfx_check_sim_t sim;
    const unsigned nnodes = *s++;
    std::vector< std::uint32_t > addr;
    const unsigned* ns = s;
    for ( unsigned i = 0; i < nnodes; ++i )
    {
        addr.push_back( ns[ 0 ] );
        ns += 5 + ns[ 4 ];
    }
    h.add( addr[ 0 ], 0x80u * nnodes );
    for ( unsigned i = 0; i < nnodes; ++i )
    {
        h[ s[ 0 ] ] = s[ 1 ];
        h[ s[ 0 ] + 0x30 ] = s[ 2 ];
        h[ s[ 0 ] + 0x34 ] = s[ 3 ];
        std::vector< std::uint32_t >& v = sim.kids[ s[ 0 ] ];
        for ( unsigned k = 0; k < s[ 4 ]; ++k )
            v.push_back( addr[ s[ 5 + k ] ] );
        s += 5 + s[ 4 ];
    }
    pfx_sim_hooks_t hooks;
    hooks.user = &sim;
    hooks.children = []( void* u, std::uint32_t node ) -> const std::vector< std::uint32_t >* {
        auto& m = static_cast< pfx_check_sim_t* >( u )->kids;
        auto it = m.find( node );
        return it == m.end( ) ? nullptr : &it->second;
    };
    hooks.pattern_tick = []( void* u, pfx_heap_t& hp, std::uint32_t pattern, float dt ) {
        pfx_check_sim_t& c = *static_cast< pfx_check_sim_t* >( u );
        if ( c.nticks <= 0 )
        {
            ++c.bad;
            return;
        }
        std::uint32_t d;
        std::memcpy( &d, &dt, 4 );
        c.bad += ( c.ticks[ 0 ] != pattern || c.ticks[ 1 ] != d ) ? 1 : 0;
        hp[ pattern + 0x28 ] = c.ticks[ 2 ];
        for ( int k = 0; k < 3; ++k )
        {
            hp[ pattern + 0x2C + 4 * k ] = c.ticks[ 3 + k ];
            hp[ pattern + 0x3C + 4 * k ] = c.ticks[ 6 + k ];
        }
        c.ticks += 9;
        --c.nticks;
    };
    const std::uint32_t sizes[ 4 ] = { pairs[ 0 ] * pairs[ 1 ], pairs[ 2 ] * pairs[ 3 ], pairs[ 4 ] * pairs[ 5 ], pairs[ 6 ] * pairs[ 7 ] };
    int failed = 0, compared = 0;
    auto compare = [ & ]( const unsigned* want, int step ) {
        int bad = 0;
        for ( int k = 0; k < 25; ++k, ++compared )
            if ( h[ res + 4 * k ] != want[ k ] )
            {
                if ( bad < 2 )
                    LOG_ERROR( "pfx_lifecycle step %d: resources +0x%X = 0x%08X, retail 0x%08X", step, 4 * k, h[ res + 4 * k ], want[ k ] );
                ++bad;
            }
        want += 25;
        for ( int b = 0; b < 4; ++b )
            for ( std::uint32_t o = 0; o < sizes[ b ]; o += 4, ++want )
            {
                if ( b == 0 && ( o % pairs[ 1 ] == 0x8Cu || o % pairs[ 1 ] == 0x9Cu ) )
                    continue;
                if ( b == 1 && ( o % pairs[ 3 ] == 0x38u || o % pairs[ 3 ] == 0x48u ) )   // 0x58B240 stores stack words there
                    continue;
                ++compared;
                if ( h[ blocks[ b ] + o ] != *want )
                {
                    if ( bad < 2 )
                        LOG_ERROR( "pfx_lifecycle step %d: block %d +0x%X = 0x%08X, retail 0x%08X", step, b, o, h[ blocks[ b ] + o ], *want );
                    ++bad;
                }
            }
        return bad;
    };
    const std::size_t region = 25 + ( sizes[ 0 ] + sizes[ 1 ] + sizes[ 2 ] + sizes[ 3 ] ) / 4;
    pfx_resources_ctor( h, res );
    std::uint32_t authored[ 8 ];
    std::memcpy( authored, g_golden_plc_words + 5, 32 );
    pfx_resources_init( h, res, authored, blocks );
    failed += compare( s, -1 ) ? 1 : 0;
    s += region;
    const unsigned nops = *s++;
    for ( unsigned i = 0; i < nops; ++i )
    {
        const unsigned code = *s++;
        int bad = 0;
        switch ( code )
        {
        case 0:
            bad += pfx_system_instance_create( h, res, addr[ s[ 0 ] ], hooks ) != s[ 1 ] ? 1 : 0;
            s += 2;
            break;
        case 1: pfx_system_start( h, s[ 0 ] ); s += 1; break;
        case 2:
            for ( unsigned k = 0; k < s[ 1 ]; ++k )
                bad += pfx_particle_allocate( h, res, s[ 0 ] ) != s[ 2 + k ] ? 1 : 0;
            s += 2 + s[ 1 ];
            break;
        case 3:
        {
            float dt;
            std::memcpy( &dt, s + 1, 4 );
            sim.ticks = s + 3;
            sim.nticks = static_cast< int >( s[ 2 ] );
            sim.bad = 0;
            pfx_system_tick( h, s[ 0 ], dt, hooks );
            bad += sim.bad + sim.nticks;
            s += 3 + 9 * s[ 2 ];
            break;
        }
        case 4: pfx_system_stop( h, res, s[ 0 ], s[ 1 ] != 0u ); s += 2; break;
        case 5: pfx_system_release( h, res, s[ 0 ] ); s += 1; break;
        default: pfx_impl_tick( h, res ); break;
        }
        bad += compare( s, static_cast< int >( i ) );
        s += region;
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "pfx_lifecycle step %d (operation %u): %d differences", static_cast< int >( i ), code, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuPfxResources 0x58B5B0 / 0x58B670 and the system lifecycle (0x51DA70 / 0x58B880 / 0x58A940 / 0x58B2D0 / 0x58AC90 / "
                     "0x58B4E0 / 0x58AD70 / 0x58ACC0 / 0x51DAE0 / 0x58AA70 / 0x58B3F0 / 0x51DD80) match retail's memory over %d operations, "
                     "%d words",
            static_cast< int >( nops ), compared );
    else
        LOG_ERROR( "pfx_lifecycle FAILED %d of %d steps", failed, static_cast< int >( nops ) + 1 );
    return failed == 0;
}

// VuPfx systems END TO END (world/pfxsim.hxx: pfx_system_tick with pfx_heap_pattern_tick, every process's slot 1 by
// its vtable, the fountains' emit + slot 2 on the heap) against retail running its own per-frame code (case pfx_run):
// after the init and every operation, the 25 resource words, every block word (but the stack-word pads: system +0x8C /
// +0x9C, pattern +0x38 / +0x48, and a directional fountain particle's +0x24 / a geom fountain particle's +0x50 - here
// every particle's +0x24 and +0x50 are skipped, as the case cannot tell which fountain made a particle) and the
// VuRand's 34 words; no unrecreated path may be reached.
struct pfx_check_run_t
{
    std::map< std::uint32_t, std::vector< std::uint32_t > > kids;
    pfx_frame_t frame;
    std::uint32_t r = 0;
};

inline bool verify_pfx_run( )
{
    const unsigned* s = g_golden_prn_words;
    const std::uint32_t res = s[ 0 ];
    const std::uint32_t blocks[ 4 ] = { s[ 1 ], s[ 2 ], s[ 3 ], s[ 4 ] };
    std::uint32_t authored[ 8 ], pairs[ 8 ];
    std::memcpy( authored, s + 5, 32 );
    std::memcpy( pairs, s + 5, 32 );
    for ( int k = 1; k < 8; k += 2 )
        pairs[ k ] = ( pairs[ k ] + 15u ) & 0xFFFFFFF0u;
    pfx_check_run_t run;
    run.r = res;
    const unsigned* box = s + 13;
    for ( int k = 0; k < 3; ++k )
    {
        std::memcpy( &run.frame.geom_box[ k ], box + k, 4 );
        std::memcpy( &run.frame.geom_box[ 3 + k ], box + 4 + k, 4 );
    }
    float camera[ 3 ];
    std::memcpy( camera, s + 21, 12 );
    pfx_rand_t rand;
    rand.seed = static_cast< std::int32_t >( s[ 24 ] );
    rand.previous = static_cast< std::int32_t >( s[ 25 ] );
    for ( int k = 0; k < 32; ++k )
        rand.table[ k ] = static_cast< std::int32_t >( s[ 26 + k ] );
    run.frame.cameras = camera;
    run.frame.camera_count = 1;
    run.frame.rand = &rand;
    s += 58;
    pfx_heap_t h;
    h.add( res, 0x64 );
    for ( int k = 0; k < 4; ++k )
        h.add( blocks[ k ], pairs[ 2 * k ] * pairs[ 2 * k + 1 ] );
    const unsigned nnodes = *s++;
    std::vector< std::uint32_t > addr;
    for ( const unsigned* ns = s; addr.size( ) < nnodes; )
    {
        addr.push_back( ns[ 0 ] );
        const unsigned nw = ns[ 2 ];
        ns += 4 + 2 * nw + ns[ 3 + 2 * nw ];
    }
    h.add( addr[ 0 ], 0x100u * nnodes );
    for ( unsigned i = 0; i < nnodes; ++i )
    {
        const std::uint32_t a = s[ 0 ];
        h[ a ] = s[ 1 ];
        const unsigned nw = s[ 2 ];
        for ( unsigned k = 0; k < nw; ++k )
            h[ a + s[ 3 + 2 * k ] ] = s[ 4 + 2 * k ];
        const unsigned nk = s[ 3 + 2 * nw ];
        std::vector< std::uint32_t >& v = run.kids[ a ];
        for ( unsigned k = 0; k < nk; ++k )
            v.push_back( addr[ s[ 4 + 2 * nw + k ] ] );
        s += 4 + 2 * nw + nk;
    }
    pfx_sim_hooks_t hooks;
    hooks.user = &run;
    hooks.children = []( void* u, std::uint32_t node ) -> const std::vector< std::uint32_t >* {
        auto& m = static_cast< pfx_check_run_t* >( u )->kids;
        auto it = m.find( node );
        return it == m.end( ) ? nullptr : &it->second;
    };
    hooks.pattern_tick = []( void* u, pfx_heap_t& hp, std::uint32_t pattern, float dt ) {
        pfx_check_run_t& c = *static_cast< pfx_check_run_t* >( u );
        pfx_heap_pattern_tick( hp, c.r, pattern, dt, c.frame );
    };
    const std::uint32_t sizes[ 4 ] = { pairs[ 0 ] * pairs[ 1 ], pairs[ 2 ] * pairs[ 3 ], pairs[ 4 ] * pairs[ 5 ], pairs[ 6 ] * pairs[ 7 ] };
    int compared = 0;
    auto compare = [ & ]( const unsigned* want, int step ) {
        int bad = 0;
        auto one = [ & ]( std::uint32_t got, std::uint32_t w, const char* what, std::uint32_t o ) {
            ++compared;
            if ( got == w )
                return;
            static std::map< const char*, int > logged;
            if ( logged[ what ]++ < 3 )
                LOG_ERROR( "pfx_run step %d: %s +0x%X = 0x%08X, retail 0x%08X", step, what, o, got, w );
            ++bad;
        };
        for ( std::uint32_t k = 0; k < 25; ++k )
            one( h[ res + 4 * k ], *want++, "resources", 4 * k );
        static const char* const names[ 4 ] = { "systems", "patterns", "processes", "particles" };
        for ( int b = 0; b < 4; ++b )
            for ( std::uint32_t o = 0; o < sizes[ b ]; o += 4, ++want )
            {
                const std::uint32_t in = o % pairs[ 2 * b + 1 ];
                if ( ( b == 0 && ( in == 0x8Cu || in == 0x9Cu ) ) || ( b == 1 && ( in == 0x38u || in == 0x48u ) ) ||
                     ( b == 3 && ( in == 0x24u || in == 0x50u ) ) )
                    continue;
                one( h[ blocks[ b ] + o ], *want, names[ b ], o );
            }
        one( static_cast< std::uint32_t >( rand.seed ), *want++, "VuRand", 0 );
        one( static_cast< std::uint32_t >( rand.previous ), *want++, "VuRand", 4 );
        for ( int k = 0; k < 32; ++k )
            one( static_cast< std::uint32_t >( rand.table[ k ] ), *want++, "VuRand", 8 + 4 * k );
        return bad;
    };
    const std::size_t region = 25 + ( sizes[ 0 ] + sizes[ 1 ] + sizes[ 2 ] + sizes[ 3 ] ) / 4 + 34;
    pfx_resources_ctor( h, res );
    pfx_resources_init( h, res, authored, blocks );
    int failed = compare( s, -1 ) ? 1 : 0;
    s += region;
    const unsigned nops = *s++;
    int particles = 0;
    for ( unsigned i = 0; i < nops; ++i )
    {
        const unsigned code = *s++;
        int bad = 0;
        switch ( code )
        {
        case 0: bad += pfx_system_instance_create( h, res, addr[ s[ 0 ] ], hooks ) != s[ 1 ] ? 1 : 0; s += 2; break;
        case 1: pfx_system_start( h, s[ 0 ] ); s += 1; break;
        case 3:
        {
            float dt;
            std::memcpy( &dt, s + 1, 4 );
            pfx_system_tick( h, s[ 0 ], dt, hooks );
            s += 2;
            break;
        }
        case 4: pfx_system_stop( h, res, s[ 0 ], s[ 1 ] != 0u ); s += 2; break;
        case 5: pfx_system_release( h, res, s[ 0 ] ); s += 1; break;
        case 7:
            for ( int k = 0; k < 3; ++k )
            {
                h[ s[ 0 ] + 0x4C + 4 * k ] = s[ 1 + k ];
                h[ s[ 0 ] + 0x5C + 4 * k ] = s[ 4 + k ];
            }
            s += 7;
            break;
        default: pfx_impl_tick( h, res ); break;
        }
        bad += compare( s, static_cast< int >( i ) );
        const int live = static_cast< int >( pairs[ 6 ] - h[ res + 0x3C ] );
        particles = live > particles ? live : particles;
        s += region;
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "pfx_run step %d (operation %u): %d differences", static_cast< int >( i ), code, bad );
        }
    }
    if ( run.frame.unrecreated != 0 )
    {
        ++failed;
        LOG_ERROR( "pfx_run reached %d unrecreated paths", run.frame.unrecreated );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuPfx systems end to end (create, start, the real pattern / fountain / tick-process frames, stop, release) match "
                     "retail's memory over %d operations (up to %d live particles), %d words",
            static_cast< int >( nops ), particles, compared );
    else
        LOG_ERROR( "pfx_run FAILED %d of %d steps", failed, static_cast< int >( nops ) + 1 );
    return failed == 0;
}

// The water classes' getType slots (world/surface.hxx g_water_types) against retail (case water_types): per slot the
// descriptor it returns and the chain of names its descriptor walks to.
inline bool verify_water_types( )
{
    int bad = 0;
    const std::size_t n = sizeof( g_water_types ) / sizeof( g_water_types[ 0 ] );
    for ( std::size_t i = 0; i < g_golden_wty_count; ++i )
    {
        const golden_water_type_t& g = g_golden_wty[ i ];
        const water_type_t* t = nullptr;
        for ( std::size_t k = 0; k < n; ++k )
            if ( g_water_types[ k ].address == g.address )
                t = &g_water_types[ k ];
        if ( t == nullptr || t->descriptor != g.descriptor || std::strcmp( t->chain, g.chain ) != 0 )
        {
            ++bad;
            LOG_ERROR( "water_types 0x%X: descriptor / chain differ (retail 0x%X %s)", g.address, g.descriptor, g.chain );
        }
    }
    bad += n != g_golden_wty_count ? 1 : 0;
    if ( bad == 0 )
        LOG_SUCCESS( "g_water_types match the water classes' %d getType slots (descriptor and chain each)", static_cast< int >( n ) );
    else
        LOG_ERROR( "water_types FAILED %d", bad );
    return bad == 0;
}

// The effect-asset load (world/pfxload.hxx) against retail's own (case pfx_load): the race's VuPfxAsset documents
// (Hydroelectric, Generic) read from Base.apf and loaded - every node depth first in map order (path, node vtable,
// refcount +4, +8, child count +0x10, the words +0x30 .. size with the std::string members and the VuAssetProperty
// object pointers - quad +0x98 / +0x9C, trail +0x8C, retail heap addresses - zeroed on both sides; the strings
// compared as text), the texture / model hook calls in order, and 0x51DBD0 on every node path plus misses.
inline void pfx_load_rows( const pfx_runtime_t& rt, const pfx_library_t& lib, std::uint32_t node, const std::string& path,
    std::vector< std::pair< std::string, std::uint32_t > >& out )
{
    out.emplace_back( path, node );
    auto k = lib.kids.find( node );
    if ( k != lib.kids.end( ) )
        for ( const auto& c : k->second )
            pfx_load_rows( rt, lib, c.second, path + "/" + c.first, out );
}

inline bool verify_pfx_load( )
{
    c_archive archive;
    const std::string pack = world_check_pack( );
    if ( pack.empty( ) || !archive.open( pack.c_str( ) ) )
    {
        LOG_ERROR( "pfx_load: Base.apf did not open" );
        return false;
    }
    static pfx_runtime_t rt;
    pfx_runtime_init( rt );
    pfx_library_t lib;
    struct hook_t
    {
        int project, kind;
        std::uint32_t node, offset;
        std::string text;
    };
    struct log_t
    {
        int project = 0;
        std::vector< hook_t > calls;
    } log;
    pfx_load_hooks_t hooks;
    hooks.user = &log;
    hooks.texture = []( void* u, std::uint32_t node, std::uint32_t offset, const std::string& name ) {
        log_t& l = *static_cast< log_t* >( u );
        l.calls.push_back( { l.project, 0, node, offset, name } );
    };
    hooks.model = []( void* u, std::uint32_t node, const std::string& name ) {
        log_t& l = *static_cast< log_t* >( u );
        l.calls.push_back( { l.project, 1, node, 0u, name } );
    };
    int failed = 0, words = 0;
    for ( std::size_t p = 0; p < g_golden_pld_project_count; ++p )
    {
        std::vector< std::uint8_t > blob;
        log.project = static_cast< int >( p );
        if ( !archive.read( g_golden_pld_projects[ p ].asset, blob ) ||
             pfx_load_asset( rt, lib, g_golden_pld_projects[ p ].name, blob, hooks ) == 0u )
        {
            LOG_ERROR( "pfx_load: %s did not load", g_golden_pld_projects[ p ].asset );
            return false;
        }
    }
    std::vector< std::pair< std::string, std::uint32_t > > rows;
    for ( const auto& pr : lib.projects )
        pfx_load_rows( rt, lib, pr.second, pr.first, rows );
    std::map< std::uint32_t, std::string > path_of;
    for ( const auto& r : rows )
        path_of[ r.second ] = r.first;
    if ( rows.size( ) != g_golden_pld_node_count )
    {
        LOG_ERROR( "pfx_load: %d nodes, retail %d", static_cast< int >( rows.size( ) ), static_cast< int >( g_golden_pld_node_count ) );
        return false;
    }
    std::map< int, std::map< std::uint32_t, std::string > > texts;
    for ( std::size_t i = 0; i < g_golden_pld_text_count; ++i )
        texts[ g_golden_pld_texts[ i ].row ][ g_golden_pld_texts[ i ].offset ] = g_golden_pld_texts[ i ].text;
    for ( std::size_t i = 0; i < rows.size( ); ++i )
    {
        const golden_pfx_node_t& g = g_golden_pld_nodes[ i ];
        const std::uint32_t a = rows[ i ].second;
        pfx_heap_t& h = rt.heap;
        int bad = rows[ i ].first != g.path || h[ a ] != g.vtable || h[ a + 4 ] != g.w4 || h[ a + 8 ] != g.w8 || h[ a + 0x10 ] != g.count;
        const pfx_node_type_t* t = pfx_node_type_of( h[ a ] );
        std::vector< std::uint32_t > masked, unknown;
        if ( t != nullptr )
            for ( int k = 0; k < t->prop_count; ++k )
            {
                if ( t->props[ k ].kind == pfx_prop_name || t->props[ k ].kind == pfx_prop_texture )
                    for ( std::uint32_t w = 0; w < 6; ++w )
                        masked.push_back( t->props[ k ].offset + 4u * w );
                if ( t->props[ k ].kind == pfx_prop_vec3 || t->props[ k ].kind == pfx_prop_rot3 )
                    unknown.push_back( t->props[ k ].offset + 12u );   // UNKNOWN: the stored w lane (pfxload.hxx)
            }
        if ( h[ a ] == 0x7505ACu )
            masked.insert( masked.end( ), { 0x98u, 0x9Cu } );
        if ( h[ a ] == 0x750294u )
            masked.push_back( 0x8Cu );
        for ( unsigned w = 0; w < g.nwords; ++w )
        {
            const std::uint32_t off = 0x30u + 4u * w;
            if ( std::find( unknown.begin( ), unknown.end( ), off ) != unknown.end( ) )
                continue;
            const std::uint32_t ours = std::find( masked.begin( ), masked.end( ), off ) != masked.end( ) ? 0u : h[ a + off ];
            bad += ours != g_golden_pld_words[ g.first + w ];
            ++words;
        }
        const auto& want = texts[ static_cast< int >( i ) ];
        for ( const auto& tx : want )
        {
            auto s = lib.strings.find( { a, tx.first } );
            bad += s == lib.strings.end( ) || s->second != tx.second;
        }
        if ( bad )
        {
            if ( failed < 6 )
                LOG_ERROR( "pfx_load: node %s differs from retail's load (%d)", g.path, bad );
            ++failed;
        }
    }
    int hook_bad = log.calls.size( ) != g_golden_pld_hook_count;
    for ( std::size_t i = 0; i < g_golden_pld_hook_count && i < log.calls.size( ); ++i )
    {
        const golden_pfx_hook_t& g = g_golden_pld_hooks[ i ];
        const hook_t& c = log.calls[ i ];
        hook_bad += c.project != g.project || c.kind != g.kind || path_of[ c.node ] != g.path || c.offset != g.offset || c.text != g.text;
    }
    int lookup_bad = 0;
    for ( std::size_t i = 0; i < g_golden_pld_lookup_count; ++i )
    {
        const std::uint32_t n = pfx_find( lib, g_golden_pld_lookups[ i ].query );
        lookup_bad += ( n == 0u ? std::string( ) : path_of[ n ] ) != g_golden_pld_lookups[ i ].found;
    }
    if ( failed == 0 && hook_bad == 0 && lookup_bad == 0 )
        LOG_SUCCESS( "pfx_load builds retail's node trees from %d VuPfxAsset documents: %d nodes, %d words, %d strings, %d hook calls, "
                     "%d path lookups exact",
            static_cast< int >( g_golden_pld_project_count ), static_cast< int >( rows.size( ) ), words,
            static_cast< int >( g_golden_pld_text_count ), static_cast< int >( g_golden_pld_hook_count ),
            static_cast< int >( g_golden_pld_lookup_count ) );
    else
        LOG_ERROR( "pfx_load FAILED: %d nodes, %d hook calls, %d lookups differ", failed, hook_bad, lookup_bad );
    return failed == 0 && hook_bad == 0 && lookup_bad == 0;
}

inline bool verify_pfx_all( )
{
    bool ok = true;
    ok = verify_pfx_load( ) && ok;
    ok = verify_water_types( ) && ok;
    ok = verify_pfx_run( ) && ok;
    ok = verify_pfx_lifecycle( ) && ok;
    ok = verify_pattern_draw( ) && ok;
    ok = verify_fountain_init( ) && ok;
    return ok;
}
