#pragma once

// The water area's oracle-backed checks (one file per parallel agent; included by check.hxx after every
// shared check, so anything above is usable here). verify_water_all runs them all.

// ocean_patch_vertex against the rectangular and circular ocean waves' base kernels @0x59ed30 / @0x59fed0
// and their ctx+100 variants @0x59f2b0 / @0x5a03f0, on hand-built double-buffered patch fields. Every
// operation is IEEE-exact (mul, add, sub, div, sqrtss, the double divide and its narrowing), so the bar is
// bit exactness. The +100 variants run the same body only where the vertex's tag equals ctx+92 (1 here);
// the walk below is that filter.
inline bool verify_ocean_kernels( )
{
    int checked = 0;
    int failed = 0;
    int live = 0;
    for ( std::size_t index = 0; index < g_golden_ocean_kernels_count; ++index )
    {
        const golden_ocean_kernel_t& g = g_golden_ocean_kernels[ index ];
        ocean_patch_t wave{};
        wave.shift = g.m_shift;
        wave.mask = ( 1 << g.m_shift ) - 1;
        wave.scale = g.m_scale;
        wave.sine = g.m_sine;
        wave.cosine = g.m_cosine;
        wave.now = g.m_now;
        wave.current = g.m_current;
        wave.previous = g.m_previous;
        wave.time[ 0 ] = g.m_times[ 0 ];
        wave.time[ 1 ] = g.m_times[ 1 ];
        wave.patches[ 0 ] = g_golden_ocean_patches + g.m_patches;
        wave.patches[ 1 ] = g_golden_ocean_patches + g.m_patches + g.m_patch_count;
        wave.centre_x = g.m_centre[ 0 ];
        wave.centre_y = g.m_centre[ 1 ];
        for ( int k = 0; k < 4; ++k )
            wave.shape[ k ] = g.m_shape[ k ];
        for ( int i = 0; i < g.m_point_count; ++i )
        {
            float displacement[ 3 ] = { 0.25f, -0.5f, 1.5f };
            float height = 2.0f;
            if ( !g.m_variant || g_golden_ocean_tags[ g.m_tags + i ] == 1u )
            {
                ocean_patch_vertex( wave, g.m_kind == 1, g_golden_ocean_points[ g.m_points + 2 * i ],
                    g_golden_ocean_points[ g.m_points + 2 * i + 1 ], displacement, height );
            }
            const float ours[ 4 ] = { displacement[ 0 ], displacement[ 1 ], displacement[ 2 ], height };
            const float* want = g_golden_ocean_out + g.m_out + 4 * i;
            ++checked;
            if ( want[ 3 ] != 2.0f )
                ++live;
            if ( std::memcmp( ours, want, sizeof( ours ) ) != 0 )
            {
                ++failed;
                if ( failed <= 6 )
                {
                    LOG_ERROR( "ocean_patch_vertex %s variant %d point %.6g %.6g: ours %.9g %.9g retail %.9g %.9g",
                        g.m_kind ? "circular" : "rectangular", g.m_variant, g_golden_ocean_points[ g.m_points + 2 * i ],
                        g_golden_ocean_points[ g.m_points + 2 * i + 1 ], ours[ 2 ], ours[ 3 ], want[ 2 ], want[ 3 ] );
                }
            }
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "ocean_patch_vertex matches the rectangular / circular ocean kernels @0x59ed30 / @0x59fed0 (+100 @0x59f2b0 / @0x5a03f0) bit for bit on %d vertices (%d producing output)",
            checked, live );
    }
    else
    {
        LOG_ERROR( "ocean_patch_vertex FAILED %d of %d vertices against the ocean kernels", failed, checked );
    }
    return failed == 0;
}

inline wake_record_t wake_record( const float* f )
{
    wake_record_t r{};
    std::memcpy( &r, f, sizeof( r ) );
    return r;
}

// wake_vertex against VuWaterWakeWave's base kernel @0x52ca60 and its ctx+100 variant @0x52ce20, the flat
// wake's slot 3 @0x52ca30 with ctx+96 clear (it must leave every vertex alone), and wake_tick against the
// shared tick @0x52c210 (al and both ages). Bit exact; the sine is fsincos under Unicorn (see wave.hxx).
inline bool verify_wake_kernels( )
{
    int checked = 0;
    int failed = 0;
    int live = 0;
    for ( std::size_t index = 0; index < g_golden_wake_kernels_count; ++index )
    {
        const golden_wake_kernel_t& g = g_golden_wake_kernels[ index ];
        wake_wave_t wave{};
        wave.inner_ratio = g.m_head[ 0 ];
        wave.outer_ratio = g.m_head[ 1 ];
        wave.scale = g.m_head[ 2 ];
        wave.a = wake_record( g.m_a );
        wave.b = wake_record( g.m_b );
        for ( int i = 0; i < g.m_point_count; ++i )
        {
            float displacement[ 3 ] = { 0.25f, -0.5f, 1.5f };
            float height = 2.0f;
            if ( !g.m_flat && ( !g.m_variant || g_golden_wake_tags[ g.m_tags + i ] == 1u ) )
            {
                wake_vertex( wave, g_golden_wake_points[ g.m_points + 2 * i ],
                    g_golden_wake_points[ g.m_points + 2 * i + 1 ], displacement, height );
            }
            const float ours[ 4 ] = { displacement[ 0 ], displacement[ 1 ], displacement[ 2 ], height };
            const float* want = g_golden_wake_out + g.m_out + 4 * i;
            ++checked;
            if ( want[ 3 ] != 2.0f )
                ++live;
            if ( std::memcmp( ours, want, sizeof( ours ) ) != 0 )
            {
                ++failed;
                if ( failed <= 6 )
                {
                    LOG_ERROR( "wake_vertex flat %d variant %d point %.6g %.6g: ours %.9g retail %.9g", g.m_flat,
                        g.m_variant, g_golden_wake_points[ g.m_points + 2 * i ],
                        g_golden_wake_points[ g.m_points + 2 * i + 1 ], ours[ 3 ], want[ 3 ] );
                }
            }
        }
    }
    int ticks_failed = 0;
    for ( std::size_t index = 0; index < g_golden_wake_ticks_count; ++index )
    {
        const golden_wake_tick_t& g = g_golden_wake_ticks[ index ];
        wake_wave_t wave{};
        wave.a = wake_record( g.m_a );
        wave.b = wake_record( g.m_b );
        const bool alive = wake_tick( wave, g.m_mode, g.m_dt );
        const float ages[ 2 ] = { wave.a.age, wave.b.age };
        if ( alive != ( g.m_alive != 0 ) || std::memcmp( ages, g.m_ages, sizeof( ages ) ) != 0 )
        {
            ++ticks_failed;
            LOG_ERROR( "wake_tick mode %d dt %.6g: ours %d %.9g %.9g retail %d %.9g %.9g", g.m_mode, g.m_dt, alive,
                ages[ 0 ], ages[ 1 ], g.m_alive, g.m_ages[ 0 ], g.m_ages[ 1 ] );
        }
    }
    if ( failed == 0 && ticks_failed == 0 )
    {
        LOG_SUCCESS( "wake_vertex matches VuWaterWakeWave's kernels @0x52ca60 / +100 @0x52ce20 and the flat wake's @0x52ca30 bit for bit on %d vertices (%d producing output); wake_tick matches @0x52c210 on %d ticks",
            checked, live, static_cast< int >( g_golden_wake_ticks_count ) );
    }
    else
    {
        LOG_ERROR( "wake kernels FAILED %d of %d vertices, %d ticks", failed, checked, ticks_failed );
    }
    return failed == 0 && ticks_failed == 0;
}

inline bool verify_water_all( )
{
    bool ok = true;
    ok = verify_ocean_kernels( ) && ok;
    ok = verify_wake_kernels( ) && ok;
    return ok;
}
