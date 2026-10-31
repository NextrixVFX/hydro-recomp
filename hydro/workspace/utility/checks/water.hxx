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

// The wave entities' wave creation against retail's slot 6 on every rectangular ocean, circular ocean,
// banked turn and whirlpool entity of Hydroelectric (loaded by retail's VuEntity::load): every word the
// recreated descriptor + constructor defines must equal the word retail's wave object holds. The
// directional and bump entities are captured but NOT recreated (their constructors build a matrix through
// 0x40b5f0 / 0x410ab0 / 0x4157c0, unread) - they are counted, not checked.
inline bool verify_wave_bridges( )
{
    int checked = 0;
    int words = 0;
    int failed = 0;
    int skipped = 0;
    for ( std::size_t index = 0; index < g_golden_wave_bridges_count; ++index )
    {
        const golden_wave_bridge_t& g = g_golden_wave_bridges[ index ];
        wave_source_t s{};
        s.entity = g.m_entity;
        s.transform = g.m_transform;
        wave_image_t w{};
        switch ( g.m_kind )
        {
        case 0: w = rectangular_ocean_create( s ); break;
        case 1: w = circular_ocean_create( s ); break;
        case 2: w = banked_turn_create( s ); break;
        case 3: w = whirlpool_create( s ); break;
        default: ++skipped; continue;
        }
        ++checked;
        int bad = 0;
        for ( int k = 0; k < 64; ++k )
        {
            if ( !w.known[ k ] )
                continue;
            ++words;
            if ( k >= g.m_words || w.word[ k ] != g.m_wave[ k ] )
            {
                if ( ++bad <= 3 )
                {
                    LOG_ERROR( "wave creation %s (kind %d) +%d: ours 0x%08X retail 0x%08X", g.m_name, g.m_kind, 4 * k,
                        w.word[ k ], k < g.m_words ? g.m_wave[ k ] : 0u );
                }
            }
        }
        if ( bad )
            ++failed;
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the wave entities' creation (0x5d8ce0 / 0x5d9340 / 0x5d35f0 / 0x5d7d90 -> the wave ctors) matches retail on %d Hydroelectric entities, %d words (%d directional / bump captured, not recreated)",
            checked, words, skipped );
    }
    else
    {
        LOG_ERROR( "wave creation FAILED on %d of %d entities", failed, checked );
    }
    return failed == 0;
}

// water_tick against VuWater's "Water" tick @0x529e30 on fake wave lists: the exact event sequence (each
// wave's slot 2 with its scaled dt, the tree removals, the destructor calls, the renderer's kick) and the
// final list, pool and refcounts. The waves' answers are the scenario's inputs.
struct water_tick_record_t
{
    const float* specs = nullptr;
    std::vector< int > cursor;
    std::vector< std::uint32_t > events;
};

inline bool verify_water_tick( )
{
    int failed = 0;
    int events_total = 0;
    for ( std::size_t index = 0; index < g_golden_water_tick_count; ++index )
    {
        const golden_water_tick_t& g = g_golden_water_tick[ index ];
        water_tick_record_t rec{};
        rec.specs = g_golden_water_tick_specs + g.m_spec;
        rec.cursor.assign( g.m_specs, 0 );
        std::vector< float > scale( g.m_specs );
        std::vector< int > refs( g.m_specs );
        for ( int k = 0; k < g.m_specs; ++k )
        {
            scale[ k ] = rec.specs[ 5 * k ];
            refs[ k ] = static_cast< int >( rec.specs[ 5 * k + 1 ] );
        }
        const int* state = g_golden_water_tick_state + g.m_state;
        water_list_t list{};
        list.nodes.resize( g.m_nodes + 2 );
        for ( int i = 0; i < g.m_nodes; ++i )
            list.nodes[ i ] = { state[ i ], i + 1 < g.m_nodes ? i + 1 : -1 };
        list.nodes[ g.m_nodes ] = { 0, g.m_nodes + 1 };
        list.nodes[ g.m_nodes + 1 ] = { 0, -1 };
        list.used = g.m_nodes ? 0 : -1;
        list.free = g.m_nodes;
        list.free_count = 2;
        water_tick_hooks_t hooks{};
        hooks.user = &rec;
        hooks.tick = []( void* u, int wave, float dt ) {
            auto& r = *static_cast< water_tick_record_t* >( u );
            std::uint32_t bits;
            std::memcpy( &bits, &dt, 4 );
            r.events.insert( r.events.end( ), { 0u, static_cast< std::uint32_t >( wave ), bits } );
            const int answer = static_cast< int >( r.specs[ 5 * wave + 3 + r.cursor[ wave ]++ ] );
            return answer != 0;
        };
        hooks.remove = []( void* u, int wave ) {
            static_cast< water_tick_record_t* >( u )->events.insert(
                static_cast< water_tick_record_t* >( u )->events.end( ), { 1u, static_cast< std::uint32_t >( wave ), 1u } );
        };
        hooks.destroy = []( void* u, int wave, int flag ) {
            static_cast< water_tick_record_t* >( u )->events.insert( static_cast< water_tick_record_t* >( u )->events.end( ),
                { 2u, static_cast< std::uint32_t >( wave ), static_cast< std::uint32_t >( flag ) } );
        };
        hooks.render = []( void* u ) {
            static_cast< water_tick_record_t* >( u )->events.insert(
                static_cast< water_tick_record_t* >( u )->events.end( ), { 3u, 0xFFFFFFFFu, 1u } );
        };
        water_tick( list, scale.data( ), refs.data( ), g.m_dt, hooks );

        auto node_id = [ & ]( int n ) { return n < 0 ? -1 : ( n >= g.m_nodes ? 100 + ( n - g.m_nodes ) : n ); };
        bool ok = rec.events.size( ) == static_cast< std::size_t >( 3 * g.m_events ) &&
                  std::memcmp( rec.events.data( ), g_golden_water_tick_events + g.m_event,
                      rec.events.size( ) * sizeof( std::uint32_t ) ) == 0;
        ok = ok && list.free_count == g.m_free_count && node_id( list.free ) == g.m_free_head &&
             node_id( list.used ) == g.m_used_head;
        for ( int i = 0; i < g.m_nodes && ok; ++i )
            ok = node_id( list.nodes[ i ].next ) == state[ g.m_nodes + i ];
        for ( int k = 0; k < g.m_specs && ok; ++k )
            ok = refs[ k ] == state[ 2 * g.m_nodes + k ];
        events_total += g.m_events;
        if ( !ok )
        {
            ++failed;
            LOG_ERROR( "water_tick scenario %d differs from @0x529e30 (%d events ours, %d retail)",
                static_cast< int >( index ), static_cast< int >( rec.events.size( ) / 3 ), g.m_events );
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "water_tick matches VuWater's Water tick @0x529e30 on %d scenarios (%d events, list / pool / refcounts)",
            static_cast< int >( g_golden_water_tick_count ), events_total );
    }
    else
    {
        LOG_ERROR( "water_tick FAILED %d of %d scenarios", failed, static_cast< int >( g_golden_water_tick_count ) );
    }
    return failed == 0;
}

// The waves' slot 2 ticks against retail's: al, the members each tick writes (as words), and the calls it
// makes - the point wave's one-shot callback and the ocean's four rebuild calls.
inline bool verify_wave_ticks( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_wave_ticks_count; ++index )
    {
        const golden_wave_tick_t& g = g_golden_wave_ticks[ index ];
        int calls = 0;
        auto count = []( void* u ) { ++*static_cast< int* >( u ); };
        bool alive = false;
        bool ok = true;
        auto word = [ & ]( int offset ) { return g.m_after[ offset / 4 ]; };
        auto same = [ & ]( int offset, float value ) {
            std::uint32_t bits;
            std::memcpy( &bits, &value, 4 );
            return bits == word( offset );
        };
        switch ( g.m_kind )
        {
        case 0:
        {
            float clock = g.m_in[ 0 ];
            alive = whirlpool_tick( clock, g.m_refs, g.m_dt );
            ok = same( 0x74, clock );
            break;
        }
        case 1:
        {
            float clock = g.m_in[ 1 ];
            alive = directional_tick( clock, g.m_refs, g.m_dt );
            ok = same( 0x7C, clock );
            break;
        }
        case 2: alive = static_wave_tick( g.m_refs ); break;
        case 3:
        {
            point_wave_life_t w{};
            w.rate = g.m_in[ 1 ];
            w.tail_width = g.m_in[ 2 ];
            w.outer_radius = g.m_in[ 3 ];
            w.elapsed = g.m_in[ 4 ];
            w.has_callback = g.m_point[ 0 ] != 0;
            w.expired = g.m_point[ 1 ] != 0;
            alive = point_wave_tick( w, g.m_refs, g.m_dt, count, &calls );
            ok = same( 0x78, w.elapsed ) && ( word( 0x90 ) & 0xFFu ) == ( w.expired ? 1u : 0u );
            break;
        }
        default:
        {
            ocean_clock_t w{};
            w.time = g.m_ocean[ 0 ];
            w.buffer_time[ 0 ] = g.m_ocean[ 1 ];
            w.buffer_time[ 1 ] = g.m_ocean[ 2 ];
            w.current = g.m_index[ 0 ];
            w.previous = g.m_index[ 1 ];
            int rebuilds = 0;
            alive = ocean_tick( w, g.m_refs, g.m_dt, count, &rebuilds );
            calls = rebuilds * 4;   // retail's four rebuild calls per rebuild
            double t;
            double b0;
            double b1;
            std::memcpy( &t, &g.m_after[ 0x88 / 4 ], 8 );
            std::memcpy( &b0, &g.m_after[ 0xB0 / 4 ], 8 );
            std::memcpy( &b1, &g.m_after[ 0xC0 / 4 ], 8 );
            ok = std::memcmp( &t, &w.time, 8 ) == 0 && std::memcmp( &b0, &w.buffer_time[ 0 ], 8 ) == 0 &&
                 std::memcmp( &b1, &w.buffer_time[ 1 ], 8 ) == 0 &&
                 word( 0xA4 ) == static_cast< std::uint32_t >( w.current ) &&
                 word( 0xA8 ) == static_cast< std::uint32_t >( w.previous );
            break;
        }
        }
        ok = ok && alive == ( g.m_al != 0 ) && calls == g.m_calls;
        if ( !ok )
        {
            ++failed;
            LOG_ERROR( "wave tick kind %d refs %d dt %.6g: ours al %d calls %d, retail al %d calls %d", g.m_kind, g.m_refs,
                g.m_dt, alive, calls, g.m_al, g.m_calls );
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the wave ticks match retail's slot 2s (@0x5a1680 / @0x59aad0 / @0x59c380 / @0x5a34b0 / @0x59cc00) on %d samples",
            static_cast< int >( g_golden_wave_ticks_count ) );
    }
    else
    {
        LOG_ERROR( "the wave ticks FAILED %d of %d samples", failed, static_cast< int >( g_golden_wave_ticks_count ) );
    }
    return failed == 0;
}

inline bool verify_water_all( )
{
    bool ok = true;
    ok = verify_ocean_kernels( ) && ok;
    ok = verify_wake_kernels( ) && ok;
    ok = verify_wave_bridges( ) && ok;
    ok = verify_water_tick( ) && ok;
    ok = verify_wave_ticks( ) && ok;
    return ok;
}
