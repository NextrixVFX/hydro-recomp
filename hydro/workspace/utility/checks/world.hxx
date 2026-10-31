#pragma once

// The world area's oracle-backed checks (one file per parallel agent; included by check.hxx after every
// shared check, so anything above is usable here). verify_world_all runs them all.

// transform_draw_matrix (hydro world/transform.hxx) against the draw matrix
// retail's prop owners leave in their model-draw component (oracle case
// prop_draw_matrix): every level / template prop of each class that carries a
// Vu3dDrawStaticModelComponent or Vu3dDrawAnimatedModelComponent, loaded alone,
// plus a hand-built nested set whose parents move the children after their own
// postLoad (the transform watcher's path). Inputs are retail's transform world
// and scale; all 16 words compared, one failure per prop.
inline bool verify_prop_draw_matrix( )
{
    int failed = 0;
    for ( std::size_t k = 0; k < g_golden_prop_matrices_count; ++k )
    {
        const golden_prop_matrix_t& want = g_golden_prop_matrices[ k ];
        transform_component_t c;
        transform_construct( c );
        std::memcpy( c.world, want.m_world, sizeof( c.world ) );
        std::memcpy( c.scale, want.m_scale, 3 * sizeof( float ) );
        float ours[ 16 ];
        transform_draw_matrix( c, ours );
        if ( std::memcmp( ours, want.m_matrix, sizeof( ours ) ) != 0 && ++failed <= 8 )
        {
            int w = 0;
            while ( w < 15 && std::memcmp( &ours[ w ], &want.m_matrix[ w ], 4 ) == 0 )
                ++w;
            std::uint32_t got;
            std::memcpy( &got, &ours[ w ], 4 );
            LOG_ERROR( "prop draw matrix: '%s' word %d is 0x%08X, retail 0x%08X", want.m_path, w, got, want.m_matrix[ w ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "transform_draw_matrix matches the prop owners' draw matrix (0x546AE0 / 0x545FF0 / 0x5ED420: fromScale x world) "
                     "on %d props (%d hand-built nested), 16 words each, bit for bit",
            static_cast< int >( g_golden_prop_matrices_count ), static_cast< int >( g_golden_prop_matrices_synthetic ) );
    else
        LOG_ERROR( "transform_draw_matrix FAILED on %d props", failed );
    return failed == 0;
}

inline bool verify_world_all( )
{
    bool ok = true;
    ok = verify_prop_draw_matrix( ) && ok;
    return ok;
}
