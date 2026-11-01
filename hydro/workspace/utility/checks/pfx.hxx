#pragma once

#include <workspace/core/world/pfxclass.hxx>

#include <cstring>

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

inline bool verify_pfx_all( )
{
    bool ok = true;
    ok = verify_pattern_draw( ) && ok;
    ok = verify_fountain_init( ) && ok;
    return ok;
}
