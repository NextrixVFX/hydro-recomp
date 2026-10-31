#pragma once

#include <workspace/core/world/tree.hxx>

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
// banked turn, whirlpool, directional and bump entity of Hydroelectric (loaded by retail's VuEntity::load):
// every word the recreated descriptor + constructor defines must equal the word retail's wave object holds -
// for the directional and bump that includes both 16-word frames (the matrix and its inverse).
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
        case 4: w = directional_create( s ); break;
        case 5: w = bump_create( s ); break;
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
        LOG_SUCCESS( "the wave entities' creation (0x5d8ce0 / 0x5d9340 / 0x5d35f0 / 0x5d7d90 / 0x5d9d40 / 0x5d3e70 -> the wave ctors) matches retail on %d Hydroelectric entities, %d words (%d of an unknown kind)",
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

// The pfx tick nodes' per-particle bodies against retail's slot 1s on 12 linked particles per sample: every
// particle's 32 words after the tick (the five members the bodies write, the rest untouched).
inline bool verify_pfx_ticks( )
{
    int failed = 0;
    int particles = 0;
    for ( std::size_t index = 0; index < g_golden_pfx_ticks_count; ++index )
    {
        const golden_pfx_tick_t& g = g_golden_pfx_ticks[ index ];
        int bad = 0;
        for ( int i = 0; i < 12; ++i )
        {
            std::uint32_t words[ 32 ];
            std::memcpy( words, g_golden_pfx_tick_words + g.m_words + 32 * i, sizeof( words ) );
            auto get = [ & ]( int offset ) { return wave_bits( words[ offset / 4 ] ); };
            auto put = [ & ]( int offset, float value ) { std::memcpy( &words[ offset / 4 ], &value, 4 ); };
            pfx_tick_particle_t p{ { get( 0x18 ), get( 0x1C ), get( 0x20 ) }, get( 0x34 ), get( 0x38 ), get( 0x3C ), get( 0x4C ) };
            switch ( g.m_kind )
            {
            case 0: pfx_tick_dampen( p, g.m_params, g.m_dt ); break;
            case 1: pfx_tick_accelerate( p, g.m_params, g.m_dt ); break;
            case 2: pfx_tick_scale( p, g.m_params, g.m_dt ); break;
            case 3: pfx_tick_alpha( p, g.m_params, g.m_dt ); break;
            case 4: pfx_tick_alpha_in_out( p, g.m_params, g.m_dt ); break;
            default: pfx_tick_world_scale_z( p, g.m_params, g.m_dt ); break;
            }
            put( 0x18, p.velocity[ 0 ] );
            put( 0x1C, p.velocity[ 1 ] );
            put( 0x20, p.velocity[ 2 ] );
            put( 0x34, p.alpha );
            put( 0x38, p.scale );
            put( 0x3C, p.age );
            put( 0x4C, p.world_scale_z );
            ++particles;
            if ( std::memcmp( words, g_golden_pfx_tick_words + g.m_words + 32 * 12 + 32 * i, sizeof( words ) ) != 0 )
                ++bad;
        }
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "pfx tick kind %d dt %.6g: %d of 12 particles differ from retail", g.m_kind, g.m_dt, bad );
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the pfx tick nodes (dampen @0x58e4b0, acceleration @0x58e810, scale @0x590da0, alpha @0x591360, alpha in/out @0x5910e0, world scale z @0x590b40) match retail on %d samples, %d particles",
            static_cast< int >( g_golden_pfx_ticks_count ), particles );
    }
    else
    {
        LOG_ERROR( "the pfx tick nodes FAILED %d of %d samples", failed, static_cast< int >( g_golden_pfx_ticks_count ) );
    }
    return failed == 0;
}

// The wave frame's matrix helpers against retail: 0x40b5f0 (rotation about z, x87 fsincos under Unicorn),
// VuMatrix_fromScale @0x407db0 and VuMatrix_identity @0x410a40 - all 16 words of each output.
inline bool verify_matrix_helpers( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_matrix_helpers_count; ++index )
    {
        const golden_matrix_helper_t& g = g_golden_matrix_helpers[ index ];
        float m[ 16 ];
        if ( g.m_fn == 0 )
            wave_matrix_rotation_z( m, g.m_in[ 0 ] );
        else if ( g.m_fn == 1 )
            wave_matrix_scale( m, g.m_in );
        else
            wave_matrix_identity( m );
        if ( std::memcmp( m, g.m_words, 64 ) != 0 )
        {
            ++failed;
            LOG_ERROR( "matrix helper %d on %.9g differs from retail", g.m_fn, g.m_in[ 0 ] );
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "the wave frame's matrix helpers (0x40b5f0 / 0x407db0 / 0x410a40) match retail on %d outputs, all 16 words",
            static_cast< int >( g_golden_matrix_helpers_count ) );
    }
    else
    {
        LOG_ERROR( "the matrix helpers FAILED %d of %d", failed, static_cast< int >( g_golden_matrix_helpers_count ) );
    }
    return failed == 0;
}

// water_tree_* against VuWater's 2D tree (ctor @0x598c70, chunk growth @0x598e50, leaf insert @0x598de0 ->
// @0x599010 / @0x598f00, removal @0x599220) on 170 operations crossing two chunk growths: every operation's
// return and, at the end, every node (rect words, parent, children, tag - stale words of freed nodes
// included) and the header.
inline bool verify_water_tree( )
{
    water_tree_t t{};
    water_tree_init( t );
    int failed = 0;
    for ( std::size_t i = 0; i < g_golden_tree_ops_count; ++i )
    {
        const golden_tree_op_t& op = g_golden_tree_ops[ i ];
        const int r = op.m_op == 0 ? water_tree_add( t, op.m_arg, op.m_rect ) : water_tree_remove( t, op.m_arg );
        if ( r != op.m_result )
        {
            if ( ++failed <= 3 )
                LOG_ERROR( "water tree op %d (%d): ours %d retail %d", static_cast< int >( i ), op.m_op, r, op.m_result );
        }
    }
    int bad_nodes = 0;
    if ( t.nodes.size( ) != g_golden_tree_nodes_count )
    {
        ++failed;
        LOG_ERROR( "water tree: %d nodes, retail %d", static_cast< int >( t.nodes.size( ) ), static_cast< int >( g_golden_tree_nodes_count ) );
    }
    else
    {
        for ( std::size_t k = 0; k < g_golden_tree_nodes_count; ++k )
        {
            const golden_tree_node_t& g = g_golden_tree_nodes[ k ];
            const water_tree_node_t& n = t.nodes[ k ];
            if ( std::memcmp( n.rect, g.m_rect, 16 ) != 0 || n.parent != g.m_parent || n.first != g.m_first ||
                 n.second != g.m_second || n.tag != g.m_tag )
            {
                if ( ++bad_nodes <= 3 )
                    LOG_ERROR( "water tree node %d differs (parent %d / %d, first %d / %d, second %d / %d)",
                        static_cast< int >( k ), n.parent, g.m_parent, n.first, g.m_first, n.second, g.m_second );
            }
        }
    }
    const int header[ 6 ] = { t.root, t.free, t.chunk, t.leaves, t.used, t.free_count };
    const bool header_ok = std::memcmp( header, g_golden_tree_header, sizeof( header ) ) == 0;
    if ( failed == 0 && bad_nodes == 0 && header_ok )
    {
        LOG_SUCCESS( "water_tree matches VuWater's 2D tree (@0x598c70 / @0x598e50 / @0x598de0 / @0x599010 / @0x598f00 / @0x599220) on %d operations, %d final nodes and the header",
            static_cast< int >( g_golden_tree_ops_count ), static_cast< int >( g_golden_tree_nodes_count ) );
        return true;
    }
    LOG_ERROR( "water_tree FAILED: %d op returns, %d nodes, header %s", failed, bad_nodes, header_ok ? "ok" : "differs" );
    return false;
}

// water_hold_add / water_hold_remove against VuWater's add @0x529a90 (walks @0x52b4d0 / @0x52b3e0, the
// per-surface test @0x529c60) and removal @0x529b90 on 4 hand-built surfaces and 30 waves: after every op
// each wave tree's node count and the pool count; at the end every tree's nodes and header, every wave's
// entry list and the pool.
inline bool verify_water_waves( )
{
    water_hold_t w{};
    water_tree_init( w.surface_tree );
    w.surfaces.resize( 4 );
    for ( int i = 0; i < 4; ++i )
    {
        const golden_ww_surface_t& g = g_golden_ww_surfaces[ i ];
        water_held_surface_t& s = w.surfaces[ i ];
        std::memcpy( s.rows, g.m_rows, 64 );
        s.z_low = g.m_z[ 0 ];
        s.z_high = g.m_z[ 1 ];
        s.half[ 0 ] = g.m_half[ 0 ];
        s.half[ 1 ] = g.m_half[ 1 ];
        water_tree_init( s.waves );
        water_tree_add( w.surface_tree, i, g.m_rect );
    }
    w.pool.resize( 200 );
    for ( int i = 0; i < 200; ++i )
        w.pool[ i ].next = i + 1 < 200 ? i + 1 : -1;
    w.pool_free = 0;
    w.pool_count = 200;
    for ( std::size_t k = 0; k < g_golden_ww_waves_count; ++k )
    {
        water_held_wave_t v{};
        const float* b = g_golden_ww_waves[ k ].m_bounds;
        std::memcpy( v.box, b, 32 );
        v.centre[ 0 ] = b[ 8 ];
        v.centre[ 1 ] = b[ 9 ];
        v.radius = b[ 10 ];
        v.point = g_golden_ww_waves[ k ].m_flag & 1;
        w.waves.push_back( v );
    }
    int failed = 0;
    for ( std::size_t i = 0; i < g_golden_ww_ops_count; ++i )
    {
        const golden_ww_op_t& op = g_golden_ww_ops[ i ];
        if ( op.m_op == 0 )
            water_hold_add( w, op.m_wave );
        else
            water_hold_remove( w, op.m_wave );
        const int after[ 5 ] = { w.surfaces[ 0 ].waves.used, w.surfaces[ 1 ].waves.used, w.surfaces[ 2 ].waves.used,
            w.surfaces[ 3 ].waves.used, w.pool_count };
        if ( std::memcmp( after, op.m_after, sizeof( after ) ) != 0 && ++failed <= 3 )
            LOG_ERROR( "water waves op %d (%d wave %d): the counts differ from retail", static_cast< int >( i ), op.m_op, op.m_wave );
    }
    int bad = 0;
    for ( int k = 0; k < 5; ++k )
    {
        const water_tree_t& t = k == 0 ? w.surface_tree : w.surfaces[ k - 1 ].waves;
        const golden_ww_tree_t& g = g_golden_ww_trees[ k ];
        const int header[ 6 ] = { t.root, t.free, t.chunk, t.leaves, t.used, t.free_count };
        if ( std::memcmp( header, &g, sizeof( header ) ) != 0 || static_cast< int >( t.nodes.size( ) ) != g.m_count )
        {
            ++bad;
            continue;
        }
        for ( int n = 0; n < g.m_count; ++n )
        {
            const golden_tree_node_t& gn = g_golden_ww_nodes[ g.m_nodes + n ];
            const water_tree_node_t& m = t.nodes[ n ];
            // a leaf's object and a wave-tree leaf's tag were retail pointers, mapped in the golden to
            // 0x20000 + surface / 0x10000 + wave
            const int base = k == 0 ? 0x20000 : 0x10000;
            const int want_first = gn.m_first >= 0x10000 ? gn.m_first - base : gn.m_first;
            const std::uint32_t want_tag = gn.m_tag >= 0x20000u ? gn.m_tag - 0x20000u : gn.m_tag;
            const int first = m.first;
            const std::uint32_t tag = m.tag;
            if ( std::memcmp( m.rect, gn.m_rect, 16 ) != 0 || m.parent != gn.m_parent || first != want_first ||
                 m.second != gn.m_second || tag != want_tag )
                ++bad;
        }
    }
    for ( std::size_t k = 0; k < g_golden_ww_waves_count; ++k )
    {
        const int start = g_golden_ww_list_heads[ 2 * k ];
        const int count = g_golden_ww_list_heads[ 2 * k + 1 ];
        int e = w.waves[ k ].entries;
        for ( int j = 0; j < count; ++j, e = e >= 0 ? w.pool[ e ].next : -1 )
        {
            const int* g = g_golden_ww_list_entries + 3 * ( start + j );
            if ( e < 0 || w.pool[ e ].surface != g[ 0 ] || w.pool[ e ].leaf != g[ 1 ] || e != g[ 2 ] )
                ++bad;
        }
        if ( e >= 0 )
            ++bad;
    }
    if ( w.pool_count != g_golden_ww_pool[ 0 ] || w.pool_free != g_golden_ww_pool[ 1 ] )
        ++bad;
    if ( failed == 0 && bad == 0 )
    {
        LOG_SUCCESS( "water_hold matches VuWater's wave add / removal (@0x529a90 / @0x52b4d0 / @0x52b3e0 / @0x529c60 / @0x529b90) on %d ops: every count after each, every final tree node, entry list and the pool",
            static_cast< int >( g_golden_ww_ops_count ) );
        return true;
    }
    LOG_ERROR( "water_hold FAILED: %d ops' counts, %d final items", failed, bad );
    return false;
}

// pfx_quad_pattern_tick against VuPfxQuadPatternInstance's tick @0x591c30 on 8 hand-built patterns: every
// particle's 30 words after, the pattern's list and AABB (pads excluded), the pool and the child calls.
struct quad_pattern_record_t
{
    std::vector< std::uint32_t > calls;
};

// The model box the geom golden carries (min x y z, max x y z); the quad golden has none.
inline const float* pattern_box( const golden_geom_pattern_t& g ) { return g.m_box; }
inline const float* pattern_box( const golden_quad_pattern_t& ) { return nullptr; }

template < typename G >
inline int pattern_tick_failures( const G* table, std::size_t count, const unsigned* words, const unsigned* calls, bool geom )
{
    int failed = 0;
    for ( std::size_t index = 0; index < count; ++index )
    {
        const G& g = table[ index ];
        const float* box = geom ? pattern_box( g ) : nullptr;
        std::vector< pfx_quad_particle_t > parts( g.m_count );
        for ( int i = 0; i < g.m_count; ++i )
        {
            std::memcpy( parts[ i ].w, words + g.m_words + 30 * i, 120 );
            parts[ i ].w[ 0 ] = static_cast< std::uint32_t >( i + 1 < g.m_count ? i + 1 : -1 );
            parts[ i ].w[ 1 ] = static_cast< std::uint32_t >( i > 0 ? i - 1 : -1 );
        }
        pfx_quad_pattern_t p{};
        p.particles = &parts;
        p.head = g.m_count ? 0 : -1;
        p.tail = g.m_count ? g.m_count - 1 : -1;
        p.count = g.m_count;
        pfx_pool_t pool{};
        pool.head = pool.tail = g.m_pool_pre ? 20 : -1;
        pool.count = g.m_pool_pre;
        quad_pattern_record_t rec{};
        auto children = []( void* u, float dt ) {
            std::uint32_t bits;
            std::memcpy( &bits, &dt, 4 );
            auto& r = *static_cast< quad_pattern_record_t* >( u );
            r.calls.insert( r.calls.end( ), { 0u, bits, 1u, bits } );
        };
        pfx_pattern_tick( p, pool, g.m_clock, g.m_inst_scale, g.m_matrix, g.m_space, g.m_delay, g.m_dt, children, &rec, box );
        bool ok = true;
        for ( int i = 0; i < g.m_count && ok; ++i )
            ok = std::memcmp( parts[ i ].w, words + g.m_words + 30 * ( g.m_count + i ), 120 ) == 0;
        ok = ok && static_cast< std::uint32_t >( p.head ) == g.m_pattern[ 0 ] && static_cast< std::uint32_t >( p.tail ) == g.m_pattern[ 1 ] &&
             static_cast< std::uint32_t >( p.count ) == g.m_pattern[ 2 ];
        const int slots[ 6 ] = { 3, 4, 5, 7, 8, 9 };
        for ( int k = 0; k < 6 && ok; ++k )
        {
            std::uint32_t bits;
            std::memcpy( &bits, &p.aabb[ k ], 4 );
            ok = bits == g.m_pattern[ slots[ k ] ];
        }
        // the golden marks "still the pre-existing node" -9; with no pre-existing node that is null
        auto pool_id = [ & ]( int n ) { return ( g.m_pool_pre ? n == 20 : n == -1 ) ? -9 : n; };
        ok = ok && pool_id( pool.tail ) == g.m_pool[ 0 ] && pool_id( pool.head ) == g.m_pool[ 1 ] && pool.count == g.m_pool[ 2 ];
        ok = ok && rec.calls.size( ) == static_cast< std::size_t >( 2 * g.m_calls ) &&
             ( rec.calls.empty( ) || std::memcmp( rec.calls.data( ), calls + g.m_call,
                                         rec.calls.size( ) * 4 ) == 0 );
        if ( !ok )
        {
            ++failed;
            LOG_ERROR( "pfx pattern tick (%s) sample %d differs from retail", geom ? "geom" : "quad", static_cast< int >( index ) );
        }
    }
    return failed;
}

inline bool verify_quad_pattern_tick( )
{
    const int failed = pattern_tick_failures( g_golden_quad_patterns, g_golden_quad_patterns_count, g_golden_quad_pattern_words,
        g_golden_quad_pattern_calls, false );
    if ( failed == 0 )
    {
        LOG_SUCCESS( "pfx_quad_pattern_tick matches VuPfxQuadPatternInstance's tick @0x591c30 on %d patterns (particles, list, AABB, pool, child ticks)",
            static_cast< int >( g_golden_quad_patterns_count ) );
    }
    else
    {
        LOG_ERROR( "pfx_quad_pattern_tick FAILED %d of %d", failed, static_cast< int >( g_golden_quad_patterns_count ) );
    }
    return failed == 0;
}

// pfx_pattern_tick in its geom form against VuPfxGeomPatternInstance's tick @0x58fea0, the same comparison.
inline bool verify_geom_pattern_tick( )
{
    const int failed = pattern_tick_failures( g_golden_geom_patterns, g_golden_geom_patterns_count, g_golden_geom_pattern_words,
        g_golden_geom_pattern_calls, true );
    if ( failed == 0 )
    {
        LOG_SUCCESS( "pfx_pattern_tick (geom) matches VuPfxGeomPatternInstance's tick @0x58fea0 on %d patterns (particles, list, AABB, pool, child ticks)",
            static_cast< int >( g_golden_geom_patterns_count ) );
    }
    else
    {
        LOG_ERROR( "pfx_pattern_tick (geom) FAILED %d of %d", failed, static_cast< int >( g_golden_geom_patterns_count ) );
    }
    return failed == 0;
}

// pfx_fountain_emit against VuPfxEmitFountain_emit @0x55d030 on 10 scenarios: every particle it filled (32
// words), the node's count and accumulator after, the global VuRand's 34 words after, and the number of
// allocations and slot-2 calls.
struct fountain_record_t
{
    std::vector< pfx_quad_particle_t > parts;
    int allow = 0;
    int next = 0;
    int allocs = 0;
    int inits = 0;
    const float* matrix = nullptr;
};

inline bool verify_fountain_emit( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_fountains_count; ++index )
    {
        const golden_fountain_t& g = g_golden_fountains[ index ];
        std::uint8_t params[ 0x98 ]{};
        std::memcpy( params, g.m_params, sizeof( g.m_params ) );
        std::memcpy( params + 0x34, &g.m_max, 4 );
        std::memcpy( params + 0x40, g.m_colours, 8 );
        params[ 0x90 ] = 0;
        pfx_rand_t rand{};
        rand.seed = g.m_seed;
        rand.previous = g.m_prev;
        std::memcpy( rand.table, g.m_table, sizeof( rand.table ) );
        pfx_fountain_node_t node{};
        node.alive = g.m_alive;
        node.accumulator = g.m_node[ 0 ];
        node.count_scale = g.m_node[ 1 ];
        node.rate_scale = g.m_node[ 2 ];
        node.alpha_scale = g.m_node[ 3 ];
        fountain_record_t rec{};
        rec.parts.resize( g.m_allow );
        rec.allow = g.m_allow;
        rec.matrix = g.m_matrix;
        pfx_fountain_hooks_t hooks{};
        hooks.user = &rec;
        hooks.allocate = []( void* u ) -> pfx_quad_particle_t* {
            auto& r = *static_cast< fountain_record_t* >( u );
            ++r.allocs;
            return r.next < r.allow ? &r.parts[ r.next++ ] : nullptr;
        };
        hooks.matrix = []( void* u ) { return static_cast< fountain_record_t* >( u )->matrix; };
        hooks.init = []( void* u, pfx_quad_particle_t& ) { ++static_cast< fountain_record_t* >( u )->inits; };
        pfx_fountain_emit( node, params, rand, g.m_camera, g.m_cameras, g.m_system, g.m_system + 3, g.m_dt, hooks );
        bool ok = rec.next == g.m_count && rec.allocs == g.m_allocs && rec.inits == g.m_inits && node.alive == g.m_alive_after &&
                  std::memcmp( &node.accumulator, &g.m_accumulator_after, 4 ) == 0;
        for ( int i = 0; i < rec.next && ok; ++i )
            ok = std::memcmp( rec.parts[ i ].w, g_golden_fountain_words + g.m_words + 32 * i, 120 ) == 0;
        std::uint32_t after[ 34 ];
        std::memcpy( after, &rand.seed, 4 );
        std::memcpy( after + 1, &rand.previous, 4 );
        std::memcpy( after + 2, rand.table, 128 );
        ok = ok && std::memcmp( after, g.m_rand_after, sizeof( after ) ) == 0;
        if ( !ok )
        {
            ++failed;
            LOG_ERROR( "pfx_fountain_emit scenario %d differs from @0x55d030 (particles %d / %d, allocs %d / %d)",
                static_cast< int >( index ), rec.next, g.m_count, rec.allocs, g.m_allocs );
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "pfx_fountain_emit matches VuPfxEmitFountain_emit @0x55d030 on %d scenarios (particles, node, VuRand, calls)",
            static_cast< int >( g_golden_fountains_count ) );
    }
    else
    {
        LOG_ERROR( "pfx_fountain_emit FAILED %d of %d", failed, static_cast< int >( g_golden_fountains_count ) );
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
    ok = verify_pfx_ticks( ) && ok;
    ok = verify_matrix_helpers( ) && ok;
    ok = verify_water_tree( ) && ok;
    ok = verify_water_waves( ) && ok;
    ok = verify_quad_pattern_tick( ) && ok;
    ok = verify_geom_pattern_tick( ) && ok;
    ok = verify_fountain_emit( ) && ok;
    return ok;
}
