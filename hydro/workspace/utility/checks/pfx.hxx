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

inline bool verify_pfx_all( )
{
    bool ok = true;
    ok = verify_fountain_init( ) && ok;
    return ok;
}
