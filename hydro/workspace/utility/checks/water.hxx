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

inline bool verify_water_all( )
{
    bool ok = true;
    ok = verify_ocean_kernels( ) && ok;
    return ok;
}
