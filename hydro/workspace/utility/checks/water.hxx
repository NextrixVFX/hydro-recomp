#pragma once

#include <workspace/core/world/tree.hxx>
#include <workspace/core/world/kernel.hxx>
#include <workspace/core/world/spray.hxx>

#include <map>
#include <string>

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

// water_best_wake against VuWater's wake lookup @0x5296c0 (surface lookup @0x52aec0, wave walk @0x52b270,
// VuBestWakeFinder @0x421e50, the record blend @0x52bf10): the hold built through water_hold_add as retail's
// 0x529a90 built it (verified by verify_water_waves), then 48 queries; the finder's +0x14 after each.
inline bool verify_wake_lookup( )
{
    water_hold_t w{};
    water_tree_init( w.surface_tree );
    w.surfaces.resize( g_golden_wl_surfaces_count );
    for ( std::size_t i = 0; i < g_golden_wl_surfaces_count; ++i )
    {
        const golden_wl_surface_t& g = g_golden_wl_surfaces[ i ];
        water_held_surface_t& s = w.surfaces[ i ];
        std::memcpy( s.rows, g.m_rows, 64 );
        s.below = g.m_below;
        s.above = g.m_above;
        s.z_low = g.m_z[ 0 ];
        s.z_high = g.m_z[ 1 ];
        s.half[ 0 ] = g.m_half[ 0 ];
        s.half[ 1 ] = g.m_half[ 1 ];
        water_tree_init( s.waves );
        water_tree_add( w.surface_tree, static_cast< int >( i ), g.m_rect );
    }
    w.pool.resize( 200 );
    for ( int i = 0; i < 200; ++i )
        w.pool[ i ].next = i + 1 < 200 ? i + 1 : -1;
    w.pool_free = 0;
    w.pool_count = 200;
    for ( std::size_t k = 0; k < g_golden_wl_waves_count; ++k )
    {
        const golden_wl_wave_t& g = g_golden_wl_waves[ k ];
        water_held_wave_t v{};
        std::memcpy( v.box, g.m_bounds, 32 );
        v.centre[ 0 ] = g.m_bounds[ 8 ];
        v.centre[ 1 ] = g.m_bounds[ 9 ];
        v.radius = g.m_bounds[ 10 ];
        v.wake = g.m_kind == 0;
        v.wake_data.inner_ratio = g.m_head[ 0 ];
        v.wake_data.outer_ratio = g.m_head[ 1 ];
        v.wake_data.scale = g.m_head[ 2 ];
        v.wake_data.a = wake_record( g.m_a );
        v.wake_data.b = wake_record( g.m_b );
        w.waves.push_back( v );
        water_hold_add( w, static_cast< int >( k ) );
    }
    int failed = 0;
    int live = 0;
    for ( std::size_t i = 0; i < g_golden_wl_queries_count; ++i )
    {
        const golden_wl_query_t& q = g_golden_wl_queries[ i ];
        water_wake_finder_t f{};
        f.point[ 0 ] = q.m_finder[ 0 ];
        f.point[ 1 ] = q.m_finder[ 1 ];
        f.direction[ 0 ] = q.m_finder[ 2 ];
        f.direction[ 1 ] = q.m_finder[ 3 ];
        f.best = q.m_finder[ 4 ];
        water_best_wake( w, q.m_skip != 0, q.m_point, f );
        if ( q.m_best != q.m_finder[ 4 ] )
            ++live;
        if ( std::memcmp( &f.best, &q.m_best, 4 ) != 0 && ++failed <= 3 )
            LOG_ERROR( "water_best_wake query %d: ours %.9g retail %.9g", static_cast< int >( i ), f.best, q.m_best );
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "water_best_wake matches VuWater's wake lookup @0x5296c0 (0x52aec0 / 0x52b270 / 0x421e50 / 0x52bf10) on %d queries (%d changing the answer)",
            static_cast< int >( g_golden_wl_queries_count ), live );
        return true;
    }
    LOG_ERROR( "water_best_wake FAILED %d of %d queries", failed, static_cast< int >( g_golden_wl_queries_count ) );
    return false;
}

// water_build_world (world/surface.hxx) against retail on Hydroelectric's water: child 8 "Water" of
// VuTemplateAsset/Levels/Hydroelectric re-encoded as a project, loaded by hydro's tree (entity_load_project
// with the verified callbacks) and by retail's VuEntity::load, then every water entity's slot 6 in
// gameInitialize order (oracle case water_build). Compared: each water entity's loaded property words and
// transform words (+ the world euler of 0x534AA0), every surface's and wave's recreated words, the surface tree
// and every surface's wave tree node for node, the wave list, each wave's entry list and the pool.
inline std::unique_ptr< entity_t > water_build_document_tree( )
{
    const unsigned char* raw = reinterpret_cast< const unsigned char* >( g_golden_water_build_document );
    static const value_t document =
        c_reader( std::vector< std::uint8_t >( raw, raw + g_golden_water_build_document_bytes ) ).parse( );
    static const entity_registry_t registry = build_entity_registry( { } );
    entity_world_t world;
    world.m_registry = &registry;
    world.m_ctor_for = entity_ctor_for;
    world.m_template = []( const std::string& ) -> const value_t* { return nullptr; };
    world.m_components_for = entity_components_constructed;
    world.m_component_load = entity_load_component;
    world.m_component_mark = entity_mark_component;
    world.m_state_for = entity_state_for;
    world.m_properties_load = entity_load_properties;
    world.m_properties_mark = entity_mark_properties;
    return entity_load_project( member( document, "VuProject" ), "Data/Projects/Water.vuprj", world );
}

inline bool verify_water_build( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "water_build: the document did not load" );
        return false;
    }
    water_built_t built{};
    water_build_world( *tree, built, []( entity_t& e ) { return entity_transform( e ); } );
    int failed = 0;
    int compared = 0;
    auto cmp = [ & ]( const char* what, int a, int b, std::uint32_t ours, std::uint32_t theirs ) {
        ++compared;
        if ( ours != theirs && ++failed <= 12 )
            LOG_ERROR( "water_build %s %d word %d: ours 0x%08X retail 0x%08X", what, a, b, ours, theirs );
    };
    // The water entities in visit order: their loads and transform words.
    std::vector< entity_t* > order;
    const auto post = [ & ]( const auto& self, entity_t& e ) -> void {
        for ( auto& child : e.m_children )
            self( self, *child );
        if ( water_kind_of( e.m_type ) != water_kind_none )
            order.push_back( &e );
    };
    post( post, *tree );
    if ( order.size( ) != g_golden_water_entities_count )
    {
        LOG_ERROR( "water_build: %d water entities, retail %d", static_cast< int >( order.size( ) ), static_cast< int >( g_golden_water_entities_count ) );
        return false;
    }
    int props = 0;
    for ( std::size_t i = 0; i < order.size( ); ++i )
    {
        const golden_water_entity_t& g = g_golden_water_entities[ i ];
        entity_t& e = *order[ i ];
        const water_entity_state_t* s = static_cast< const water_entity_state_t* >( e.m_state.get( ) );
        const transform_component_t* t = entity_transform( e );
        if ( !s || !t || static_cast< int >( water_kind_of( e.m_type ) ) != g.m_kind || e.m_name != g.m_name )
        {
            LOG_ERROR( "water_build: entity %d is %s '%s', retail kind %d '%s' (or has no state / transform)", static_cast< int >( i ),
                e.m_type.c_str( ), e.m_name.c_str( ), g.m_kind, g.m_name );
            ++failed;
            continue;
        }
        for ( int k = 0; k < k_water_entity_words; ++k )
            if ( s->value.known[ k ] )
            {
                cmp( "entity property", static_cast< int >( i ), 0x60 + 4 * k, s->value.word[ k ], g.m_words[ k ] );
                ++props;
            }
        std::uint32_t words[ k_water_transform_words ];
        water_transform_words( *t, words );
        for ( int k = 0; k < k_water_transform_words; ++k )
            if ( k != 35 && k != 39 && k != 43 )
                cmp( "entity transform", static_cast< int >( i ), 0x10 + 4 * k, words[ k ], g.m_transform[ k ] );
    }
    // The objects.
    if ( built.surfaces.size( ) != g_golden_water_surfaces_count || built.waves.size( ) != g_golden_water_waves_count )
    {
        LOG_ERROR( "water_build: %d surfaces / %d waves, retail %d / %d", static_cast< int >( built.surfaces.size( ) ),
            static_cast< int >( built.waves.size( ) ), static_cast< int >( g_golden_water_surfaces_count ),
            static_cast< int >( g_golden_water_waves_count ) );
        return false;
    }
    for ( std::size_t i = 0; i < built.surfaces.size( ); ++i )
        for ( int k = 0; k < 65; ++k )
            if ( built.surfaces[ i ].known[ k ] )
                cmp( "surface", static_cast< int >( i ), 4 * k, built.surfaces[ i ].word[ k ], g_golden_water_surfaces[ i ][ k ] );
    for ( std::size_t i = 0; i < built.waves.size( ); ++i )
        for ( int k = 0; k < 80; ++k )
            if ( built.waves[ i ].known[ k ] && k != 64 / 4 )   // +0x40 the entry list: compared as the lists below
                cmp( "wave", static_cast< int >( i ), 4 * k, built.waves[ i ].word[ k ], k < g_golden_water_waves[ i ].m_words ? g_golden_water_waves[ i ].m_word[ k ] : 0xDEADBEEFu );
    // The trees: the surface tree, then each surface's wave tree.
    int nodes = 0;
    for ( std::size_t i = 0; i < g_golden_water_trees_count; ++i )
    {
        const golden_water_tree_t& g = g_golden_water_trees[ i ];
        const water_tree_t& t = i == 0 ? built.hold.surface_tree : built.hold.surfaces[ i - 1 ].waves;
        const int id = static_cast< int >( i );
        cmp( "tree root", id, 0, static_cast< std::uint32_t >( t.root ), static_cast< std::uint32_t >( g.m_root ) );
        cmp( "tree free", id, 0, static_cast< std::uint32_t >( t.free ), static_cast< std::uint32_t >( g.m_free ) );
        cmp( "tree leaves", id, 0, static_cast< std::uint32_t >( t.leaves ), static_cast< std::uint32_t >( g.m_leaves ) );
        cmp( "tree used", id, 0, static_cast< std::uint32_t >( t.used ), static_cast< std::uint32_t >( g.m_used ) );
        cmp( "tree free count", id, 0, static_cast< std::uint32_t >( t.free_count ), static_cast< std::uint32_t >( g.m_free_count ) );
        cmp( "tree size", id, 0, static_cast< std::uint32_t >( t.nodes.size( ) ), static_cast< std::uint32_t >( g.m_count ) );
        for ( int n = 0; n < g.m_count && n < static_cast< int >( t.nodes.size( ) ); ++n )
        {
            const golden_water_node_t& gn = g_golden_water_nodes[ g.m_first + n ];
            const water_tree_node_t& o = t.nodes[ n ];
            std::uint32_t r[ 4 ];
            std::memcpy( r, o.rect, 16 );
            for ( int k = 0; k < 4; ++k )
                cmp( "tree node rect", id, n, r[ k ], gn.m_rect[ k ] );
            cmp( "tree node parent", id, n, static_cast< std::uint32_t >( o.parent ), static_cast< std::uint32_t >( gn.m_parent ) );
            cmp( "tree node first", id, n, static_cast< std::uint32_t >( o.first ), static_cast< std::uint32_t >( gn.m_first ) );
            cmp( "tree node second", id, n, static_cast< std::uint32_t >( o.second ), static_cast< std::uint32_t >( gn.m_second ) );
            cmp( "tree node tag", id, n, o.tag, static_cast< std::uint32_t >( gn.m_tag ) );
            ++nodes;
        }
    }
    // The wave list from its head, each wave's entries, the pool.
    std::size_t at = 0;
    for ( int e = built.hold.list; e >= 0; e = built.hold.pool[ e ].next, ++at )
    {
        if ( at >= g_golden_water_list_count )
            break;
        cmp( "list pool", static_cast< int >( at ), 0, static_cast< std::uint32_t >( e ), static_cast< std::uint32_t >( g_golden_water_list[ at ][ 0 ] ) );
        cmp( "list wave", static_cast< int >( at ), 0, static_cast< std::uint32_t >( built.hold.pool[ e ].leaf ),
            static_cast< std::uint32_t >( g_golden_water_list[ at ][ 1 ] ) );
    }
    cmp( "list length", 0, 0, static_cast< std::uint32_t >( at ), static_cast< std::uint32_t >( g_golden_water_list_count ) );
    for ( std::size_t i = 0; i < built.hold.waves.size( ); ++i )
    {
        int k = 0;
        const int first = g_golden_water_entry_range[ i ][ 0 ];
        const int count = g_golden_water_entry_range[ i ][ 1 ];
        for ( int e = built.hold.waves[ i ].entries; e >= 0; e = built.hold.pool[ e ].next, ++k )
        {
            if ( k >= count )
                break;
            const golden_water_entry_t& g = g_golden_water_entries[ first + k ];
            cmp( "entry pool", static_cast< int >( i ), k, static_cast< std::uint32_t >( e ), static_cast< std::uint32_t >( g.m_pool ) );
            cmp( "entry surface", static_cast< int >( i ), k, static_cast< std::uint32_t >( built.hold.pool[ e ].surface ),
                static_cast< std::uint32_t >( g.m_surface ) );
            cmp( "entry leaf", static_cast< int >( i ), k, static_cast< std::uint32_t >( built.hold.pool[ e ].leaf ), static_cast< std::uint32_t >( g.m_leaf ) );
        }
        cmp( "entries", static_cast< int >( i ), 0, static_cast< std::uint32_t >( k ), static_cast< std::uint32_t >( count ) );
    }
    cmp( "pool count", 0, 0, static_cast< std::uint32_t >( built.hold.pool_count ), static_cast< std::uint32_t >( g_golden_water_pool[ 0 ] ) );
    cmp( "pool free", 0, 0, static_cast< std::uint32_t >( built.hold.pool_free ), static_cast< std::uint32_t >( g_golden_water_pool[ 1 ] ) );
    if ( failed == 0 )
    {
        LOG_SUCCESS( "water_build_world matches retail's game initialise of Hydroelectric's water (0x5DB510 / 0x5D6F80 / the wave entities' slot 6, "
                     "0x5284C0 / 0x5359E0 / 0x529860 / 0x528D70 / 0x529A90): %d entities (%d property words), %d surfaces, %d waves, %d tree "
                     "nodes, %d values, bit for bit",
            static_cast< int >( order.size( ) ), props, static_cast< int >( built.surfaces.size( ) ), static_cast< int >( built.waves.size( ) ),
            nodes, compared );
        return true;
    }
    LOG_ERROR( "water_build FAILED %d of %d values", failed, compared );
    return false;
}

struct water_query_stats_t
{
    int vertices = 0, moved = 0, calls = 0, tagged = 0, skipped = 0;
};

// The queries of one golden stream (emit/water.py _wq_stream) through query.hxx's water_query on `view`, its kernel
// hook water_view_kernel with the calls logged; returns the queries that differ.
// f96: the renderer's mode (ctx+0x60 = 1, 32-byte records, 8 words each, all compared). `unread`: per wave, true when its
// kernel for this mode is not recreated - a query whose retail slot 3 calls include one is SKIPPED (counted), not compared.
inline int water_query_vectors( const water_view_t& view, const unsigned* words, const unsigned* offsets, std::size_t count, const char* name,
    water_query_stats_t& st, bool f96 = false, const std::vector< bool >* unread = nullptr )
{
    struct logged_t
    {
        const water_view_t* view;
        std::vector< std::uint32_t > log;
    } logged{ &view, {} };
    const water_kernel_fn kernel = []( void* user, int wave, water_query_t& ctx ) {
        logged_t& l = *static_cast< logged_t* >( user );
        l.log.push_back( static_cast< std::uint32_t >( wave ) );
        l.log.push_back( ctx.tag );
        l.log.push_back( ctx.flag100 );
        return water_view_kernel( const_cast< water_view_t* >( l.view ), wave, ctx );
    };
    int failed = 0;
    for ( std::size_t index = 0; index < count; ++index )
    {
        const unsigned* s = words + offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        water_query_t ctx{};
        ctx.count = static_cast< int >( u( ) );
        for ( float& x : ctx.aabb )
            x = water_bits( u( ) );
        for ( float& x : ctx.centre )
            x = water_bits( u( ) );
        ctx.radius = water_bits( u( ) );
        ctx.exclude_count = static_cast< int >( u( ) );
        for ( int k = 0; k < ctx.exclude_count; ++k )
            ctx.exclude[ k ] = static_cast< int >( u( ) );
        ctx.forced = -1;
        const int nw = f96 ? 8 : 10;
        std::vector< water_vertex_t > v( ctx.count );
        std::vector< std::uint32_t > raw( static_cast< std::size_t >( ctx.count ) * 8, 0x77777777u );
        std::memset( v.data( ), 0x77, v.size( ) * sizeof( water_vertex_t ) );
        for ( int i = 0; i < ctx.count; ++i )
        {
            water_vertex_t& x = v[ static_cast< std::size_t >( i ) ];
            for ( int k = 0; k < 3; ++k )
            {
                const unsigned p = u( );
                x.pos[ k ] = water_bits( p );
                raw[ static_cast< std::size_t >( i ) * 8 + static_cast< std::size_t >( k ) ] = p;
            }
            x.pos_w = 0x3F800000u;
        }
        ctx.vertices = v.data( );
        if ( f96 )
        {
            ctx.flag96 = 1;
            ctx.records = reinterpret_cast< unsigned char* >( raw.data( ) );
            ctx.stride = 32;
        }
        logged.log.clear( );
        std::vector< std::uint32_t > tags;
        const char* what = nullptr;
        int bad = -1;
        const bool reached = water_query( view.water, ctx, kernel, &logged, tags );
        const std::uint32_t* ours = f96 ? raw.data( ) : reinterpret_cast< const std::uint32_t* >( v.data( ) );
        for ( int k = 0; k < nw * ctx.count; ++k )
        {
            const unsigned r = u( );
            if ( !what && ( f96 || k % 10 != 7 ) && ours[ k ] != r )
            {
                what = "a vertex record word";
                bad = k;
            }
            if ( k % nw == ( f96 ? 3 : 4 ) && r != 0u )
                ++st.moved;
        }
        st.vertices += ctx.count;
        if ( u( ) != ctx.flag100 && !what )
            what = "ctx+0x64";
        st.tagged += ctx.flag100 ? 1 : 0;
        const unsigned n = u( );
        st.calls += static_cast< int >( n );
        if ( logged.log.size( ) != 3 * n && !what )
            what = "the number of slot 3 calls";
        bool skip = false;
        for ( unsigned k = 0; k < 3 * n; ++k )
        {
            const unsigned r = u( );
            if ( unread && k % 3 == 0 && r < unread->size( ) && ( *unread )[ r ] )
                skip = true;
            if ( !what && k < logged.log.size( ) && logged.log[ k ] != r )
            {
                what = "a slot 3 call";
                bad = static_cast< int >( k );
            }
        }
        if ( skip )
        {
            ++st.skipped;
            continue;
        }
        if ( !reached )
            what = "a path not recreated was reached";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "%s vector %d: %s (%d)", name, static_cast< int >( index ), what, bad );
        }
    }
    return failed;
}

// The water query 0x528FC0 on Hydroelectric's built water against retail (case water_query): query.hxx's water_query
// over world/kernel.hxx's water_view_build of water_build_world's result, every wave's slot 3 the kernel hook
// water_view_kernel (the five kinds' verified kernels and their tag variants) - retail's own slot 3s under the
// oracle. Compared: every vertex record word except +28 (0x529ED0's / 0x529F70's stale stack), ctx+0x64 after, and
// the slot 3 calls in order (wave, ctx+0x5C, ctx+0x64).
inline bool verify_water_query( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "water_query: the document did not load" );
        return false;
    }
    water_built_t built{};
    water_build_world( *tree, built, []( entity_t& e ) { return entity_transform( e ); } );
    water_view_t view{};
    water_view_build( built, view );
    water_query_stats_t st{};
    const int failed = water_query_vectors( view, g_golden_water_query_words, g_golden_water_query_offsets, g_golden_water_query_count,
        "water query", st );
    if ( failed == 0 )
        LOG_SUCCESS( "water_query + water_view_kernel match retail's 0x528FC0 on Hydroelectric's built water over %d queries (%d vertices, %d "
                     "with disp x moved; %d slot 3 calls, %d queries tagged), every record word, the calls in order",
            static_cast< int >( g_golden_water_query_count ), st.vertices, st.moved, st.calls, st.tagged );
    else
        LOG_ERROR( "water_query FAILED %d of %d queries against 0x528FC0", failed, static_cast< int >( g_golden_water_query_count ) );
    return failed == 0;
}

// The query in the renderer's +96 mode (case water_query96): water_build_world's water plus 12 point waves
// (water_create_point_wave, each ticked once), query.hxx's +96 branches of 0x529ED0 / 0x529F70 and the waves' +96
// kernels through water_view_kernel. Compared: all 8 words of every record, ctx+0x64, the slot 3 calls in order. A query
// reaching a kind whose +96 kernel is not recreated yet is skipped and counted.
inline bool verify_water_query96( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "water_query96: the document did not load" );
        return false;
    }
    water_built_t built{};
    water_build_world( *tree, built, []( entity_t& e ) { return entity_transform( e ); } );
    for ( std::size_t k = 0; k < g_golden_wq96_point_count; ++k )
    {
        const unsigned* p = g_golden_wq96_points + 12 * k;
        const int wave = water_create_point_wave( built, p );
        water_words_t& w = built.waves[ static_cast< std::size_t >( wave ) ];
        point_wave_life_t life{};
        life.tail_width = w.f( 92 );
        life.outer_radius = w.f( 104 );
        life.elapsed = w.f( 120 );
        life.rate = w.f( 124 );
        point_wave_tick( life, static_cast< int >( w.word[ 1 ] ), water_bits( p[ 11 ] ), nullptr, nullptr );
        w.setf( 120, life.elapsed );
        w.word[ 144 / 4 ] = ( w.word[ 144 / 4 ] & ~0xFFu ) | ( life.expired ? 1u : 0u );
    }
    water_view_t view{};
    water_view_build( built, view );
    std::vector< bool > unread( built.waves.size( ) );
    for ( std::size_t i = 0; i < built.waves.size( ); ++i )
    {
        const water_kind_t k = built.wave_kind[ i ];
        unread[ i ] = !( k == water_kind_whirlpool || k == water_kind_ramp || k == water_kind_point || k == water_kind_bump || k == water_kind_banked ||
                         k == water_kind_directional );
    }
    water_query_stats_t st{};
    const int failed = water_query_vectors( view, g_golden_wq96_words, g_golden_wq96_offsets, g_golden_wq96_count, "query96", st, true, &unread );
    if ( failed == 0 )
        LOG_SUCCESS( "water_query + the +96 kernels (whirlpool 0x5A1F80 / 0x5A2160, ramp 0x5A2FC0 / 0x5A3160, point 0x5A39B0 / 0x5A3CA0, "
                     "bump 0x5A4610 / 0x5A49A0, banked turn 0x5A5AB0 / 0x5A5F10, directional 0x59B860 / 0x59BDE0; 0x529ED0 / 0x529F70's +96 "
                     "branches) match retail's 0x528FC0 in the renderer's mode: %d of %d queries compared (%d "
                     "vertices, %d with a displacement; %d slot 3 calls, %d tagged), %d skipped (a +96 kernel not recreated)",
            static_cast< int >( g_golden_wq96_count ) - st.skipped, static_cast< int >( g_golden_wq96_count ), st.vertices, st.moved, st.calls,
            st.tagged, st.skipped );
    else
        LOG_ERROR( "water_query96 FAILED %d of %d queries", failed, static_cast< int >( g_golden_wq96_count ) );
    return failed == 0;
}

// VuWater_createPointWave 0x528E10 on Hydroelectric's built water against retail (case point_waves): 24 point waves'
// words after water_create_point_wave and after point_wave_tick (0x5A34B0; no expiry callback), except +24 / +40
// (stack), +0x40 (a pointer: the entry list is compared instead) and +0x91 .. +0x93 (heap); each wave's entry list;
// the wave list; then the queries around them (verify_water_query's comparison, the point kernel 0x5A36A0 / 0x5A3820
// among the slot 3s).
inline bool verify_point_waves( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "point_waves: the document did not load" );
        return false;
    }
    water_built_t built{};
    water_build_world( *tree, built, []( entity_t& e ) { return entity_transform( e ); } );
    int failed = 0, compared = 0, entries = 0;
    auto cmp = [ & ]( const char* what, int a, int b, std::uint32_t ours, std::uint32_t theirs ) {
        ++compared;
        if ( ours != theirs && ++failed <= 12 )
            LOG_ERROR( "point_waves %s %d word %d: ours 0x%08X retail 0x%08X", what, a, b, ours, theirs );
    };
    auto words = [ & ]( const char* what, int i, const water_words_t& w, const unsigned* r ) {
        for ( int k = 0; k < 37; ++k )
        {
            const int at = 4 * k;
            if ( at == 24 || at == 40 || at == 64 )
                continue;
            cmp( what, i, at, at == 144 ? ( w.word[ k ] & 0xFFu ) : w.word[ k ], at == 144 ? ( r[ k ] & 0xFFu ) : r[ k ] );
        }
    };
    std::vector< const unsigned* > after( g_golden_pw_wave_count );
    std::vector< int > index( g_golden_pw_wave_count );
    for ( std::size_t i = 0; i < g_golden_pw_wave_count; ++i )
    {
        const unsigned* s = g_golden_pw_wave_words + g_golden_pw_wave_offsets[ i ];
        index[ i ] = water_create_point_wave( built, s );
        water_words_t& w = built.waves[ static_cast< std::size_t >( index[ i ] ) ];
        words( "created", static_cast< int >( i ), w, s + 12 );
        point_wave_life_t life{};
        life.tail_width = w.f( 92 );
        life.outer_radius = w.f( 104 );
        life.elapsed = w.f( 120 );
        life.rate = w.f( 124 );
        life.has_callback = w.word[ 140 / 4 ] != 0;
        life.expired = ( w.word[ 144 / 4 ] & 0xFFu ) != 0;
        point_wave_tick( life, static_cast< int >( w.word[ 1 ] ), water_bits( s[ 11 ] ), nullptr, nullptr );
        w.setf( 120, life.elapsed );
        w.word[ 144 / 4 ] = ( w.word[ 144 / 4 ] & ~0xFFu ) | ( life.expired ? 1u : 0u );
        after[ i ] = s + 12 + 37;
    }
    // The entry lists and the wave list once every wave is made (retail's dump order).
    for ( std::size_t i = 0; i < g_golden_pw_wave_count; ++i )
    {
        const unsigned* s = after[ i ];
        words( "ticked", static_cast< int >( i ), built.waves[ static_cast< std::size_t >( index[ i ] ) ], s );
        s += 37;
        const unsigned n = *s++;
        int e = built.hold.waves[ static_cast< std::size_t >( index[ i ] ) ].entries;
        for ( unsigned k = 0; k < n; ++k, s += 3 )
        {
            cmp( "entry pool", static_cast< int >( i ), static_cast< int >( k ), static_cast< std::uint32_t >( e ), s[ 0 ] );
            if ( e < 0 )
                break;
            cmp( "entry surface", static_cast< int >( i ), static_cast< int >( k ), static_cast< std::uint32_t >( built.hold.pool[ e ].surface ), s[ 1 ] );
            cmp( "entry leaf", static_cast< int >( i ), static_cast< int >( k ), static_cast< std::uint32_t >( built.hold.pool[ e ].leaf ), s[ 2 ] );
            e = built.hold.pool[ e ].next;
            ++entries;
        }
        cmp( "entries end", static_cast< int >( i ), 0, static_cast< std::uint32_t >( e ), 0xFFFFFFFFu );
    }
    int e = built.hold.list;
    for ( std::size_t k = 0; k < g_golden_pw_wave_list_count; ++k )
    {
        cmp( "list pool", static_cast< int >( k ), 0, static_cast< std::uint32_t >( e ), static_cast< std::uint32_t >( g_golden_pw_wave_list[ k ][ 0 ] ) );
        if ( e < 0 )
            break;
        cmp( "list wave", static_cast< int >( k ), 0, static_cast< std::uint32_t >( built.hold.pool[ e ].leaf ),
            static_cast< std::uint32_t >( g_golden_pw_wave_list[ k ][ 1 ] ) );
        e = built.hold.pool[ e ].next;
    }
    cmp( "list end", 0, 0, static_cast< std::uint32_t >( e ), 0xFFFFFFFFu );
    water_view_t view{};
    water_view_build( built, view );
    water_query_stats_t st{};
    const int qfailed = water_query_vectors( view, g_golden_pw_query_words, g_golden_pw_query_offsets, g_golden_pw_query_count,
        "point query", st );
    if ( failed == 0 && qfailed == 0 )
    {
        LOG_SUCCESS( "water_create_point_wave / point_wave_tick / the point kernel match retail's 0x528E10 / 0x5A3340 / 0x5A34B0 / 0x5A36A0 "
                     "on Hydroelectric's built water: %d waves (%d values, %d entries, the wave list), %d queries (%d vertices, %d slot 3 "
                     "calls, %d tagged), bit for bit",
            static_cast< int >( g_golden_pw_wave_count ), compared, entries, static_cast< int >( g_golden_pw_query_count ), st.vertices,
            st.calls, st.tagged );
        return true;
    }
    LOG_ERROR( "point_waves FAILED %d of %d values, %d of %d queries", failed, compared, qfailed, static_cast< int >( g_golden_pw_query_count ) );
    return false;
}

// The built water's tick against retail's 0x529E30 (case water_built_tick): water_build_world's water plus 20 point
// waves (water_create_point_wave; two in three with the expiry callback, answered by water_point_wave_expired as
// VuPointWaveEntity's OnExpired 0x5D4B60 does), then 10 ticks of water_built_tick, waves 11 / 15's refcount set to 1
// before tick 3 as in the case. Compared per tick: the hook events in order (rebuild / render / expire / plug /
// destroy), which waves stay listed, their known words (not +24 / +40 / +0x40; +0x8C as set or not; +0x90's low byte),
// the wave list, the pool, every listed wave's entry list; after the last tick every surface's wave tree node for
// node; and the unrecreated paths water_built_tick reports (the two unread dtors at tick 3, nothing else).
inline bool verify_water_built_tick( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "water_built_tick: the document did not load" );
        return false;
    }
    water_built_t built{};
    water_build_world( *tree, built, []( entity_t& e ) { return entity_transform( e ); } );
    for ( std::size_t k = 0; k < g_golden_wbt_point_count; ++k )
    {
        const unsigned* p = g_golden_wbt_points + 12 * k;
        const int wave = water_create_point_wave( built, p );
        if ( p[ 11 ] )
            built.waves[ static_cast< std::size_t >( wave ) ].word[ 0x8C / 4 ] = 1u;   // the callback: set, its pointer unknown
    }
    int failed = 0, compared = 0, events = 0, words = 0;
    auto cmp = [ & ]( const char* what, int a, int b, std::uint32_t ours, std::uint32_t theirs ) {
        ++compared;
        if ( ours != theirs && ++failed <= 12 )
            LOG_ERROR( "water_built_tick %s %d / %d: ours 0x%08X retail 0x%08X", what, a, b, ours, theirs );
    };
    std::vector< std::uint32_t > log;
    water_built_hooks_t hooks{};
    hooks.user = &log;
    hooks.expire = []( void* u, water_built_t& b, int wave ) {
        auto& l = *static_cast< std::vector< std::uint32_t >* >( u );
        l.insert( l.end( ), { 2u, static_cast< std::uint32_t >( wave ) } );
        const bool was = wave < static_cast< int >( b.wave_gone.size( ) ) && b.wave_gone[ static_cast< std::size_t >( wave ) ];
        water_point_wave_expired( b, wave );
        if ( !was && wave < static_cast< int >( b.wave_gone.size( ) ) && b.wave_gone[ static_cast< std::size_t >( wave ) ] )
            l.insert( l.end( ), { 4u, static_cast< std::uint32_t >( wave ) } );
        l.insert( l.end( ), { 3u, 0xFFFFFFFFu } );   // the "OnExpired" plug
    };
    hooks.rebuild = []( void* u, int wave ) {
        static_cast< std::vector< std::uint32_t >* >( u )->insert( static_cast< std::vector< std::uint32_t >* >( u )->end( ),
            { 0u, static_cast< std::uint32_t >( wave ) } );
    };
    hooks.render = []( void* u ) {
        static_cast< std::vector< std::uint32_t >* >( u )->insert( static_cast< std::vector< std::uint32_t >* >( u )->end( ), { 1u, 0xFFFFFFFFu } );
    };
    hooks.destroyed = []( void* u, int wave ) {
        static_cast< std::vector< std::uint32_t >* >( u )->insert( static_cast< std::vector< std::uint32_t >* >( u )->end( ),
            { 4u, static_cast< std::uint32_t >( wave ) } );
    };
    for ( std::size_t t = 0; t < g_golden_wbt_count; ++t )
    {
        const int ti = static_cast< int >( t );
        if ( t == 3 )
            for ( int k : { 11, 15 } )
                built.waves[ static_cast< std::size_t >( k ) ].word[ 1 ] = 1u;
        const unsigned* s = g_golden_wbt_words + g_golden_wbt_offsets[ t ];
        const float dt = water_bits( *s++ );
        log.clear( );
        const int missing = water_built_tick( built, dt, hooks );
        cmp( "unrecreated paths", ti, 0, static_cast< std::uint32_t >( missing ), t == 3 ? 2u : 0u );
        const unsigned n_events = *s++;
        cmp( "event count", ti, 0, static_cast< std::uint32_t >( log.size( ) / 2 ), n_events );
        for ( unsigned k = 0; k < 2 * n_events; ++k, ++s )
            cmp( "event", ti, static_cast< int >( k ), k < log.size( ) ? log[ k ] : 0xDEADBEEFu, *s );
        events += static_cast< int >( n_events );
        std::vector< int > live;
        for ( int e = built.hold.list; e >= 0; e = built.hold.pool[ e ].next )
            live.push_back( built.hold.pool[ e ].leaf );
        std::sort( live.begin( ), live.end( ) );
        const unsigned n_live = *s++;
        cmp( "listed waves", ti, 0, static_cast< std::uint32_t >( live.size( ) ), n_live );
        for ( unsigned i = 0; i < n_live; ++i )
        {
            const int wave = static_cast< int >( *s++ );
            const unsigned n = *s++;
            cmp( "listed wave", ti, static_cast< int >( i ), i < live.size( ) ? static_cast< std::uint32_t >( live[ i ] ) : 0xDEADBEEFu,
                static_cast< std::uint32_t >( wave ) );
            const water_words_t& w = built.waves[ static_cast< std::size_t >( wave ) ];
            const bool point = built.wave_kind[ static_cast< std::size_t >( wave ) ] == water_kind_point;
            for ( unsigned k = 0; k < n && k < 80; ++k )
            {
                const unsigned at = 4 * k;
                if ( at == 24 || at == 40 || at == 64 )
                    continue;
                if ( point && at == 0x8C )
                    cmp( "point callback", ti, wave, w.word[ k ] != 0, s[ k ] != 0 );
                else if ( point && at == 0x90 )
                    cmp( "point expired", ti, wave, w.word[ k ] & 0xFFu, s[ k ] & 0xFFu );
                else if ( w.known[ k ] || point )
                {
                    cmp( "wave word", ti, wave * 1000 + static_cast< int >( at ), w.word[ k ], s[ k ] );
                    ++words;
                }
            }
            s += n;
        }
        const unsigned n_list = *s++;
        int e = built.hold.list;
        for ( unsigned k = 0; k < n_list; ++k, s += 2 )
        {
            cmp( "list pool", ti, static_cast< int >( k ), static_cast< std::uint32_t >( e ), s[ 0 ] );
            if ( e < 0 )
                break;
            cmp( "list wave", ti, static_cast< int >( k ), static_cast< std::uint32_t >( built.hold.pool[ e ].leaf ), s[ 1 ] );
            e = built.hold.pool[ e ].next;
        }
        cmp( "list end", ti, 0, static_cast< std::uint32_t >( e ), 0xFFFFFFFFu );
        cmp( "pool count", ti, 0, static_cast< std::uint32_t >( built.hold.pool_count ), *s++ );
        cmp( "pool free", ti, 0, static_cast< std::uint32_t >( built.hold.pool_free ), *s++ );
        for ( unsigned i = 0; i < n_live && i < live.size( ); ++i )
        {
            const unsigned n = *s++;
            int x = built.hold.waves[ static_cast< std::size_t >( live[ i ] ) ].entries;
            for ( unsigned k = 0; k < n; ++k, s += 2 )
            {
                cmp( "entry pool", ti, live[ i ], static_cast< std::uint32_t >( x ), s[ 0 ] );
                if ( x < 0 )
                    break;
                cmp( "entry surface", ti, live[ i ], static_cast< std::uint32_t >( built.hold.pool[ x ].surface ), s[ 1 ] );
                x = built.hold.pool[ x ].next;
            }
            cmp( "entries end", ti, live[ i ], static_cast< std::uint32_t >( x ), 0xFFFFFFFFu );
        }
    }
    int nodes = 0;
    for ( std::size_t i = 0; i < g_golden_wbt_trees_count && i < built.hold.surfaces.size( ); ++i )
    {
        const golden_water_tree_t& g = g_golden_wbt_trees[ i ];
        const water_tree_t& t = built.hold.surfaces[ i ].waves;
        const int id = static_cast< int >( i );
        cmp( "tree root", id, 0, static_cast< std::uint32_t >( t.root ), static_cast< std::uint32_t >( g.m_root ) );
        cmp( "tree free", id, 0, static_cast< std::uint32_t >( t.free ), static_cast< std::uint32_t >( g.m_free ) );
        cmp( "tree leaves", id, 0, static_cast< std::uint32_t >( t.leaves ), static_cast< std::uint32_t >( g.m_leaves ) );
        cmp( "tree used", id, 0, static_cast< std::uint32_t >( t.used ), static_cast< std::uint32_t >( g.m_used ) );
        cmp( "tree free count", id, 0, static_cast< std::uint32_t >( t.free_count ), static_cast< std::uint32_t >( g.m_free_count ) );
        cmp( "tree size", id, 0, static_cast< std::uint32_t >( t.nodes.size( ) ), static_cast< std::uint32_t >( g.m_count ) );
        for ( int n = 0; n < g.m_count && n < static_cast< int >( t.nodes.size( ) ); ++n )
        {
            const golden_water_node_t& gn = g_golden_wbt_nodes[ g.m_first + n ];
            const water_tree_node_t& o = t.nodes[ n ];
            std::uint32_t r[ 4 ];
            std::memcpy( r, o.rect, 16 );
            for ( int k = 0; k < 4; ++k )
                cmp( "tree node rect", id, n, r[ k ], gn.m_rect[ k ] );
            cmp( "tree node parent", id, n, static_cast< std::uint32_t >( o.parent ), static_cast< std::uint32_t >( gn.m_parent ) );
            cmp( "tree node first", id, n, static_cast< std::uint32_t >( o.first ), static_cast< std::uint32_t >( gn.m_first ) );
            cmp( "tree node second", id, n, static_cast< std::uint32_t >( o.second ), static_cast< std::uint32_t >( gn.m_second ) );
            cmp( "tree node tag", id, n, o.tag, static_cast< std::uint32_t >( gn.m_tag ) );
            ++nodes;
        }
    }
    if ( failed == 0 )
    {
        LOG_SUCCESS( "water_built_tick matches retail's 0x529E30 on Hydroelectric's built water + 20 point waves over %d ticks (%d hook "
                     "events, %d wave words, %d tree nodes, %d values; the slot 2s 0x59C380 / 0x59CC00 / 0x5A1680 / 0x59AAD0 / "
                     "0x5A34B0, OnExpired 0x5D4B60's wave half, 0x529800 / 0x5A3310), bit for bit",
            static_cast< int >( g_golden_wbt_count ), events, words, nodes, compared );
        return true;
    }
    LOG_ERROR( "water_built_tick FAILED %d of %d values", failed, compared );
    return false;
}

// The ocean +96 kernels (case ocean_kernels96): ocean_patch_vertex96 (and the evaluator 0x5A0F70 inside it) against
// 0x59F840 / 0x59FB80 / 0x5A0920 / 0x5A0C40 on the hand-built fields - flow x, flow y, foam (untouched) and height of
// every vertex, bit for bit; the tag variants run the same body only where the vertex's tag equals ctx+0x5C (1).
inline bool verify_ocean_kernels96( )
{
    int checked = 0, failed = 0, live = 0;
    for ( std::size_t index = 0; index < g_golden_ok96_kernels_count; ++index )
    {
        const golden_ocean_kernel_t& g = g_golden_ok96_kernels[ index ];
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
        wave.patches[ 0 ] = g_golden_ok96_patches + g.m_patches;
        wave.patches[ 1 ] = g_golden_ok96_patches + g.m_patches + g.m_patch_count;
        wave.centre_x = g.m_centre[ 0 ];
        wave.centre_y = g.m_centre[ 1 ];
        for ( int k = 0; k < 4; ++k )
            wave.shape[ k ] = g.m_shape[ k ];
        for ( int i = 0; i < g.m_point_count; ++i )
        {
            float flow[ 2 ] = { 0.25f, -0.5f };
            const float foam = 1.5f;
            float height = 2.0f;
            if ( !g.m_variant || g_golden_ok96_tags[ g.m_tags + i ] == 1u )
                ocean_patch_vertex96( wave, g.m_kind == 1, g_golden_ok96_points[ g.m_points + 2 * i ], g_golden_ok96_points[ g.m_points + 2 * i + 1 ],
                    height, flow );
            const float ours[ 4 ] = { flow[ 0 ], flow[ 1 ], foam, height };
            const float* want = g_golden_ok96_out + g.m_out + 4 * i;
            ++checked;
            if ( want[ 3 ] != 2.0f )
                ++live;
            if ( std::memcmp( ours, want, sizeof( ours ) ) != 0 && ++failed <= 6 )
                LOG_ERROR( "ocean_patch_vertex96 %s variant %d point %d: ours %.9g %.9g %.9g retail %.9g %.9g %.9g", g.m_kind ? "circular" : "rectangular",
                    g.m_variant, i, ours[ 0 ], ours[ 1 ], ours[ 3 ], want[ 0 ], want[ 1 ], want[ 3 ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ocean_patch_vertex96 + ocean_patch_eval_d match the ocean +96 kernels 0x59F840 / 0x5A0920 (tag 0x59FB80 / 0x5A0C40, "
                     "evaluator 0x5A0F70) bit for bit on %d vertices (%d producing output)",
            checked, live );
    else
        LOG_ERROR( "ocean_patch_vertex96 FAILED %d of %d vertices", failed, checked );
    return failed == 0;
}

// FNV-1a 64 over words' little-endian bytes (the oracle's _fnv64).
inline std::uint64_t water_fnv64( const void* data, std::size_t words )
{
    const unsigned char* p = static_cast< const unsigned char* >( data );
    std::uint64_t h = 0xCBF29CE484222325ull;
    for ( std::size_t i = 0; i < words * 4; ++i )
    {
        h ^= p[ i ];
        h *= 0x100000001B3ull;
    }
    return h;
}

// THE OCEAN FIELD on Hydroelectric's 9 ocean waves (case ocean_field): water_build_world's ocean waves (their field
// built by world/field.hxx's ocean_field_update as the ctor 0x59CB10 -> 0x59CCB0 does), then three water_built_tick
// (0x59CC00's rebuild: EvolveField 0x59D4D0, rlft3 0x608BB0 + fourn 0x608880, 0x59D640, BuildBezierPatches 0x59D6C0),
// then each wave's slot 3 through water_view_kernel in both modes. Compared: the parameter words, the clock words after
// the ctor and each tick, FNV-1a 64 of every array (omega, h0, cells, data, speq, both buffers' heights and patches
// after the ctor; data, speq and the current buffer after each tick), the first wave's arrays word for word, and every
// slot 3 output (24 points per wave per mode) bit for bit.
inline bool verify_ocean_field( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "ocean_field: the document did not load" );
        return false;
    }
    water_built_t built{};
    water_build_world( *tree, built, []( entity_t& e ) { return entity_transform( e ); } );
    std::vector< int > oceans;
    for ( std::size_t i = 0; i < built.waves.size( ); ++i )
        if ( built.wave_kind[ i ] == water_kind_rectangular || built.wave_kind[ i ] == water_kind_circular )
            oceans.push_back( static_cast< int >( i ) );
    int failed = 0, compared = 0;
    std::size_t hashed = 0;
    std::map< std::string, int > tally;   // failures by what, for the FAILED line
    auto cmp = [ & ]( const char* what, int wave, int at, std::uint64_t ours, std::uint64_t theirs ) {
        ++compared;
        if ( ours != theirs )
            ++tally[ what ];
        if ( ours != theirs && ++failed <= 12 )
            LOG_ERROR( "ocean_field %s wave %d / %d: ours 0x%llX retail 0x%llX", what, wave, at, static_cast< unsigned long long >( ours ),
                static_cast< unsigned long long >( theirs ) );
    };
    if ( oceans.size( ) != g_golden_of_count )
    {
        LOG_ERROR( "ocean_field: %u ocean waves built, retail %u", static_cast< unsigned >( oceans.size( ) ), static_cast< unsigned >( g_golden_of_count ) );
        return false;
    }
    const std::size_t per_words = 16 + 9 * g_golden_of_ticks;
    const std::size_t per_hash = 9 + 4 * g_golden_of_ticks;
    const int clock_at[ 8 ] = { 0x88, 0x8C, 0xA4, 0xA8, 0xB0, 0xB4, 0xC0, 0xC4 };
    auto check_clock = [ & ]( const char* what, int k, const water_words_t& w, const unsigned* want ) {
        for ( int j = 0; j < 8; ++j )
            cmp( what, k, j, w.word[ clock_at[ j ] / 4 ], want[ j ] );
    };
    for ( std::size_t k = 0; k < oceans.size( ); ++k )
    {
        const int ki = static_cast< int >( k );
        const water_words_t& w = built.waves[ static_cast< std::size_t >( oceans[ k ] ) ];
        const unsigned* gw = g_golden_of_words + per_words * k;
        const unsigned long long* gh = g_golden_of_hash + per_hash * k;
        cmp( "kind", ki, 0, built.wave_kind[ static_cast< std::size_t >( oceans[ k ] ) ] == water_kind_circular ? 1u : 0u, g_golden_of_kind[ k ] );
        for ( int j = 0; j < 8; ++j )
            cmp( "parameter", ki, j, w.word[ 0x4C / 4 + j ], gw[ j ] );
        check_clock( "ctor clock", ki, w, gw + 8 );
        const water_built_t::ocean_t* o = water_ocean_of( built, oceans[ k ] );
        if ( !o || o->field.n == 0 )
        {
            cmp( "field built", ki, 0, 0, 1 );
            continue;
        }
        const ocean_field_t& f = o->field;
        const std::pair< const void*, std::size_t > arrays[ 9 ] = { { f.omega.data( ), f.omega.size( ) }, { f.h0.data( ), f.h0.size( ) },
            { f.cells.data( ), f.cells.size( ) }, { f.data.data( ), f.data.size( ) }, { f.speq.data( ), f.speq.size( ) },
            { f.heights[ 0 ].data( ), f.heights[ 0 ].size( ) }, { f.patches[ 0 ].data( ), f.patches[ 0 ].size( ) },
            { f.heights[ 1 ].data( ), f.heights[ 1 ].size( ) }, { f.patches[ 1 ].data( ), f.patches[ 1 ].size( ) } };
        static const char* const names[ 9 ] = { "omega", "h0", "cells", "data", "speq", "heights0", "patches0", "heights1", "patches1" };
        for ( int j = 0; j < 9; ++j )
        {
            cmp( names[ j ], ki, 0, water_fnv64( arrays[ j ].first, arrays[ j ].second ), gh[ j ] );
            hashed += arrays[ j ].second;
        }
        if ( k == 0 )
        {
            auto words = [ & ]( const char* what, const void* ours, const unsigned* want, std::size_t count ) {
                const unsigned* u = static_cast< const unsigned* >( ours );
                for ( std::size_t i = 0; i < count; ++i )
                    cmp( what, 0, static_cast< int >( i ), u[ i ], want[ i ] );
            };
            words( "omega word", f.omega.data( ), g_golden_of_first_omega, g_golden_of_first_omega_count );
            words( "h0 word", f.h0.data( ), g_golden_of_first_h0, g_golden_of_first_h0_count );
            words( "data word", f.data.data( ), g_golden_of_first_data, g_golden_of_first_data_count );
            words( "heights word", f.heights[ 0 ].data( ), g_golden_of_first_heights0, g_golden_of_first_heights0_count );
            words( "patch word", f.patches[ 0 ].data( ), g_golden_of_first_patches0, g_golden_of_first_patches0_count );
        }
    }
    water_built_hooks_t hooks{};
    hooks.render = []( void* ) {};
    for ( std::size_t t = 0; t < g_golden_of_ticks; ++t )
    {
        const int missing = water_built_tick( built, g_golden_of_dt[ t ], hooks );
        cmp( "unrecreated paths", -1, static_cast< int >( t ), static_cast< std::uint64_t >( missing ), 0 );
        for ( std::size_t k = 0; k < oceans.size( ); ++k )
        {
            const int ki = static_cast< int >( k );
            const water_words_t& w = built.waves[ static_cast< std::size_t >( oceans[ k ] ) ];
            const unsigned* gw = g_golden_of_words + per_words * k + 16 + 9 * t;
            const unsigned long long* gh = g_golden_of_hash + per_hash * k + 9 + 4 * t;
            bool listed = false;
            for ( int e = built.hold.list; e >= 0; e = built.hold.pool[ e ].next )
                listed = listed || built.hold.pool[ e ].leaf == oceans[ k ];
            cmp( "alive", ki, static_cast< int >( t ), listed ? 1u : 0u, gw[ 0 ] );
            check_clock( "tick clock", ki, w, gw + 1 );
            const water_built_t::ocean_t* o = water_ocean_of( built, oceans[ k ] );
            if ( !o || o->field.n == 0 )
                continue;
            const ocean_field_t& f = o->field;
            const int cur = static_cast< int >( w.word[ 0xA4 / 4 ] & 1u );
            cmp( "tick data", ki, static_cast< int >( t ), water_fnv64( f.data.data( ), f.data.size( ) ), gh[ 0 ] );
            cmp( "tick speq", ki, static_cast< int >( t ), water_fnv64( f.speq.data( ), f.speq.size( ) ), gh[ 1 ] );
            cmp( "tick heights", ki, static_cast< int >( t ), water_fnv64( f.heights[ cur ].data( ), f.heights[ cur ].size( ) ), gh[ 2 ] );
            cmp( "tick patches", ki, static_cast< int >( t ), water_fnv64( f.patches[ cur ].data( ), f.patches[ cur ].size( ) ), gh[ 3 ] );
            hashed += f.data.size( ) + f.speq.size( ) + f.heights[ cur ].size( ) + f.patches[ cur ].size( );
        }
    }
    water_view_t view{};
    water_view_build( built, view );
    const std::size_t np = g_golden_of_points_per;
    int outputs = 0, live = 0;
    for ( std::size_t k = 0; k < oceans.size( ); ++k )
    {
        const int ki = static_cast< int >( k );
        const float* pts = g_golden_of_points + 2 * np * k;
        std::vector< water_vertex_t > v( np );
        for ( std::size_t i = 0; i < np; ++i )
        {
            v[ i ] = water_vertex_t{};
            v[ i ].pos[ 0 ] = pts[ 2 * i ];
            v[ i ].pos[ 1 ] = pts[ 2 * i + 1 ];
            v[ i ].disp[ 0 ] = 0.25f;
            v[ i ].disp[ 1 ] = -0.5f;
            v[ i ].disp[ 2 ] = 1.5f;
            v[ i ].height = 2.0f;
        }
        water_query_t ctx{};
        ctx.count = static_cast< int >( np );
        ctx.vertices = v.data( );
        cmp( "slot 3 answered", ki, 0, water_view_kernel( &view, oceans[ k ], ctx ) ? 1u : 0u, 1u );
        for ( std::size_t i = 0; i < np; ++i )
        {
            const float ours[ 4 ] = { v[ i ].disp[ 0 ], v[ i ].disp[ 1 ], v[ i ].disp[ 2 ], v[ i ].height };
            const float* want = g_golden_of_base + 4 * ( np * k + i );
            for ( int j = 0; j < 4; ++j )
            {
                std::uint32_t a, b;
                std::memcpy( &a, &ours[ j ], 4 );
                std::memcpy( &b, &want[ j ], 4 );
                cmp( "base out", ki, static_cast< int >( 4 * i + j ), a, b );
            }
            ++outputs;
            live += want[ 3 ] != 2.0f ? 1 : 0;
        }
        std::vector< unsigned char > rec( np * 32, 0x77 );
        for ( std::size_t i = 0; i < np; ++i )
        {
            const float r[ 6 ] = { pts[ 2 * i ], pts[ 2 * i + 1 ], 2.0f, 0.25f, -0.5f, 1.5f };
            std::memcpy( &rec[ 32 * i ], r, sizeof( r ) );
        }
        water_query_t c96{};
        c96.count = static_cast< int >( np );
        c96.flag96 = 1;
        c96.records = rec.data( );
        c96.stride = 32;
        cmp( "slot 3 +96 answered", ki, 0, water_view_kernel( &view, oceans[ k ], c96 ) ? 1u : 0u, 1u );
        for ( std::size_t i = 0; i < np; ++i )
        {
            const float* want = g_golden_of_k96 + 5 * ( np * k + i );
            for ( int j = 0; j < 5; ++j )
            {
                std::uint32_t a, b;
                std::memcpy( &a, &rec[ 32 * i + 8 + 4 * ( j < 4 ? j : 3 ) ], 4 );
                std::memcpy( &b, &want[ j ], 4 );
                cmp( "+96 out", ki, static_cast< int >( 5 * i + j ), a, b );
            }
            ++outputs;
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ocean field (world/field.hxx: 0x59CCB0's arrays, cells 0x59D010, dispersion 0x59D170, BuildSpectrum 0x59D220 + ran1 / "
                     "Vu_GaussianDeviate 0x50FD90, EvolveField 0x59D4D0, rlft3 0x608BB0 + fourn 0x608880, heights 0x59D640, "
                     "BuildBezierPatches 0x59D6C0; the tick 0x59CC00's rebuild) matches retail on Hydroelectric's %d ocean waves: %d "
                     "comparisons, %u array words hashed, %d slot 3 outputs (%d base points with a height)",
            static_cast< int >( oceans.size( ) ), compared, static_cast< unsigned >( hashed ), outputs, live );
    else
    {
        std::string by;
        for ( const auto& t : tally )
            by += " " + t.first + "=" + std::to_string( t.second );
        LOG_ERROR( "ocean_field FAILED %d of %d comparisons:%s", failed, compared, by.c_str( ) );
    }
    return failed == 0;
}

// The wakes' +96 kernels (case wake_kernels96): wake_vertex96 against 0x52D1D0 (tag 0x52DCC0) and flat_wake_vertex96
// against 0x52E7D0 (tag 0x52EAE0) on hand-built waves - height, flow x, flow y and foam of every record, bit for bit;
// the tag variants run the same body only where the record's tag equals ctx+0x5C (1).
inline bool verify_wake_kernels96( )
{
    int checked = 0, failed = 0, live = 0;
    const std::size_t np = g_golden_wk96_points_per;
    for ( std::size_t index = 0; index < g_golden_wk96_count; ++index )
    {
        const bool flat = g_golden_wk96_kind[ 2 * index ] == 1u;
        const bool tagged = g_golden_wk96_kind[ 2 * index + 1 ] == 1u;
        const float* p = g_golden_wk96_params + 29 * index;
        wake_wave_t wave{};
        wave.inner_ratio = p[ 0 ];
        wave.outer_ratio = p[ 1 ];
        wave.scale = p[ 2 ];
        std::memcpy( &wave.a, p + 3, sizeof( wave.a ) );
        std::memcpy( &wave.b, p + 16, sizeof( wave.b ) );
        for ( std::size_t i = 0; i < np; ++i )
        {
            const float x = g_golden_wk96_points[ 2 * ( np * index + i ) ];
            const float y = g_golden_wk96_points[ 2 * ( np * index + i ) + 1 ];
            float height = 2.0f;
            float flow[ 2 ] = { 0.25f, -0.5f };
            float foam = 1.5f;
            if ( !tagged || g_golden_wk96_tags[ np * index + i ] == 1u )
            {
                if ( flat )
                    flat_wake_vertex96( wave, x, y, foam );
                else
                    wake_vertex96( wave, x, y, height, flow, foam );
            }
            const float ours[ 4 ] = { height, flow[ 0 ], flow[ 1 ], foam };
            const float* want = g_golden_wk96_out + 4 * ( np * index + i );
            ++checked;
            if ( want[ 3 ] != 1.5f )
                ++live;
            if ( std::memcmp( ours, want, sizeof( ours ) ) != 0 && ++failed <= 6 )
                LOG_ERROR( "wake +96 %s variant %d set %d point %d: ours %.9g %.9g %.9g %.9g retail %.9g %.9g %.9g %.9g", flat ? "flat" : "wake",
                    tagged ? 1 : 0, static_cast< int >( index ), static_cast< int >( i ), ours[ 0 ], ours[ 1 ], ours[ 2 ], ours[ 3 ], want[ 0 ], want[ 1 ],
                    want[ 2 ], want[ 3 ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "wake_vertex96 / flat_wake_vertex96 match the wakes' +96 kernels 0x52D1D0 / 0x52E7D0 (tag 0x52DCC0 / 0x52EAE0) bit for "
                     "bit on %d records (%d producing output)",
            checked, live );
    else
        LOG_ERROR( "wake +96 kernels FAILED %d of %d records", failed, checked );
    return failed == 0;
}

// The water maps and VuWater's one-point query against retail (case water_point). Hydroelectric's water built by
// water_build_world, its maps fetched from Base.apf by name through water_build_maps (the fetch is the asset manager
// hook) and loaded by water_map_load (0x5C8650); then
//   1. per surface: the WaterMap name, the asset's +0x58 / +0x5C / +0x64, each byte array's size and FNV-1a 64, every
//      entry's +0 / +4 / size / FNV - against retail's own load of the same bytes;
//   2. water_map_sample (0x535E10) at points inside every mapped surface - the float bits;
//   3. water_point_query (0x5292D0 over 0x52AEC0 / 0x535E10 / 0x52B0E0, kernels water_view_kernel): all 8 out words
//      and the slot 3 calls in order; the last queries with water +0x18 set.
inline std::uint64_t water_check_fnv( const std::vector< std::uint8_t >& v )
{
    std::uint64_t x = 0xCBF29CE484222325ull;
    for ( std::uint8_t b : v )
        x = ( x ^ b ) * 0x100000001B3ull;
    return x;
}

inline bool verify_water_point( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    c_archive archive;
    const std::string pack = world_check_pack( );
    if ( !tree || pack.empty( ) || !archive.open( pack.c_str( ) ) )
    {
        LOG_ERROR( "water_point: the document or Base.apf did not load" );
        return false;
    }
    water_built_t built{};
    water_build_world( *tree, built, []( entity_t& e ) { return entity_transform( e ); } );
    const int missing = water_build_maps(
        built,
        []( void* user, const std::string& asset, std::vector< std::uint8_t >& blob ) {
            return static_cast< c_archive* >( user )->read( asset.c_str( ), blob );
        },
        &archive );
    int failed = 0;
    // 1. the loads
    int words = 0;
    if ( built.map_names.size( ) != g_golden_wpt_surfaces )
    {
        LOG_ERROR( "water_point: %d surfaces built, retail %d", static_cast< int >( built.map_names.size( ) ), static_cast< int >( g_golden_wpt_surfaces ) );
        return false;
    }
    const unsigned* m = g_golden_wpt_maps;
    for ( std::size_t k = 0; k < g_golden_wpt_surfaces; ++k )
    {
        const char* what = nullptr;
        if ( built.map_names[ k ] != g_golden_wpt_names[ k ] )
            what = "the WaterMap name";
        const water_map_t* map = built.maps[ k ].get( );
        const unsigned loaded = *m++;
        if ( !what && ( loaded != 0 ) != ( map != nullptr ) )
            what = "loaded or not";
        if ( loaded )
        {
            std::vector< unsigned > ours;
            if ( map )
            {
                ours = { map->width, map->height, static_cast< unsigned >( map->map.size( ) ) };
                for ( const std::vector< std::uint8_t >* a : { &map->map, &map->second, &map->third } )
                {
                    const std::uint64_t h = water_check_fnv( *a );
                    ours.insert( ours.end( ), { static_cast< unsigned >( a->size( ) ), static_cast< unsigned >( h ), static_cast< unsigned >( h >> 32 ) } );
                }
                ours.push_back( static_cast< unsigned >( map->entries.size( ) ) );
                for ( const water_map_t::entry_t& e : map->entries )
                {
                    const std::uint64_t h = water_check_fnv( e.bytes );
                    ours.insert( ours.end( ), { e.a, e.b, static_cast< unsigned >( e.bytes.size( ) ), static_cast< unsigned >( h ),
                                                  static_cast< unsigned >( h >> 32 ) } );
                }
            }
            const std::size_t n = 13 + 5 * static_cast< std::size_t >( m[ 12 ] );
            if ( !what && ours.size( ) != n )
                what = "the entry count";
            for ( std::size_t i = 0; i < n; ++i )
                if ( !what && ours[ i ] != m[ i ] )
                    what = "an asset word";
            words += static_cast< int >( n );
            m += n;
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "water_point: surface %d (%s): %s", static_cast< int >( k ), g_golden_wpt_names[ k ], what );
        }
    }
    water_view_t view{};
    water_view_build( built, view );
    // 2. the samples
    int nonzero = 0;
    for ( std::size_t k = 0; k < g_golden_wpt_sample_count; ++k )
    {
        const unsigned* s = g_golden_wpt_samples + 5 * k;
        const float p[ 3 ] = { water_bits( s[ 1 ] ), water_bits( s[ 2 ] ), water_bits( s[ 3 ] ) };
        float r = 0.0f;
        const bool reached = water_map_sample( view.water.surfaces[ s[ 0 ] ], p, r );
        std::uint32_t bits;
        std::memcpy( &bits, &r, 4 );
        nonzero += s[ 4 ] != 0u ? 1 : 0;
        if ( !reached || bits != s[ 4 ] )
        {
            ++failed;
            LOG_ERROR( "water_point: sample %d on surface %u: 0x%08X, retail 0x%08X", static_cast< int >( k ), s[ 0 ], bits, s[ 4 ] );
        }
    }
    // 3. the point queries
    struct logged_t
    {
        const water_view_t* view;
        std::vector< std::uint32_t > log;
    } logged{ &view, {} };
    const water_kernel_fn kernel = []( void* user, int wave, water_query_t& ctx ) {
        logged_t& l = *static_cast< logged_t* >( user );
        l.log.push_back( static_cast< std::uint32_t >( wave ) );
        l.log.push_back( ctx.tag );
        l.log.push_back( ctx.flag100 );
        return water_view_kernel( const_cast< water_view_t* >( l.view ), wave, ctx );
    };
    int found = 0, calls = 0, skipped = 0;
    for ( std::size_t index = 0; index < g_golden_wpt_count; ++index )
    {
        const unsigned* s = g_golden_wpt_words + g_golden_wpt_offsets[ index ];
        view.water.skip_waves = s[ 0 ] != 0u;
        skipped += s[ 0 ] ? 1 : 0;
        float point[ 4 ];
        for ( int k = 0; k < 4; ++k )
            point[ k ] = water_bits( s[ 1 + k ] );
        std::uint32_t out[ 8 ];
        std::memset( out, 0x77, sizeof( out ) );
        logged.log.clear( );
        const bool reached = water_point_query( view.water, point, kernel, &logged, reinterpret_cast< float* >( out ) );
        const char* what = reached ? nullptr : "a path not recreated was reached";
        for ( int k = 0; k < 8; ++k )
            if ( !what && out[ k ] != s[ 5 + k ] )
                what = "an out word";
        found += s[ 5 + 2 ] != 0xFEFFFFFFu ? 1 : 0;
        const unsigned n = s[ 13 ];
        calls += static_cast< int >( n );
        if ( !what && logged.log.size( ) != 3 * n )
            what = "the number of slot 3 calls";
        for ( unsigned k = 0; k < 3 * n && !what; ++k )
            if ( logged.log[ k ] != s[ 14 + k ] )
                what = "a slot 3 call";
        if ( what )
        {
            ++failed;
            LOG_ERROR( "water_point: query %d: %s", static_cast< int >( index ), what );
        }
    }
    view.water.skip_waves = false;
    if ( failed == 0 )
        LOG_SUCCESS( "water_build_maps / water_map_load (VuWaterMapAsset 0x5C8650: %d surfaces, %d asset words incl. FNV-1a of every "
                     "array, %d names unfetched), water_map_sample (0x535E10: %d samples, %d non-zero) and water_point_query (0x5292D0 / "
                     "0x52AEC0 / 0x52B0E0: %d queries, %d on a surface, %d slot 3 calls, %d with waves skipped) match retail",
            static_cast< int >( g_golden_wpt_surfaces ), words, missing, static_cast< int >( g_golden_wpt_sample_count ), nonzero,
            static_cast< int >( g_golden_wpt_count ), found, calls, skipped );
    else
        LOG_ERROR( "water_point FAILED %d", failed );
    return failed == 0;
}

// pfx_boat_thrust_rate (world/spray.hxx) against VuPfxEmitBoatThrustQuadFountainInstance's emit 0x4CD5B0 (case
// boat_thrust): fake nodes and boats, the fountain 0x55D030 a recorded hook. Compared: the rate scale (+0x20) the
// fountain receives, bit for bit, and that it received dt.
inline bool verify_boat_thrust( )
{
    int failed = 0, zeroed = 0, live = 0, boatless = 0;
    for ( std::size_t index = 0; index < g_golden_bth_count; ++index )
    {
        const unsigned* s = g_golden_bth_words + 24 * index;
        static solver_rigid_t b;
        boat_view_t c{ water_bits( s[ 1 ] ), s[ 2 ], static_cast< uint8_t >( s[ 3 ] ), static_cast< uint8_t >( s[ 4 ] ), { }, 0, 0, 0, b };
        for ( int k = 0; k < 3; ++k )
            c.o50_60[ k ] = water_bits( s[ 5 + k ] );
        c.o22C_04 = water_bits( s[ 8 ] );
        c.o22C_0C = water_bits( s[ 9 ] );
        c.o22C0_470 = water_bits( s[ 10 ] );
        for ( int k = 0; k < 4; ++k )
            b.linear_velocity[ k ] = water_bits( s[ 11 + k ] );
        std::uint8_t params[ 0x100 ]{};
        std::memcpy( params + 0xC8, &s[ 15 ], 4 );
        std::uint8_t records[ 32 ]{};
        for ( int k = 0; k < 4; ++k )
            records[ 8 * k ] = static_cast< std::uint8_t >( s[ 16 + k ] );
        pfx_fountain_node_t node{};
        node.rate_scale = water_bits( s[ 20 ] );
        pfx_boat_thrust_rate( node, params, s[ 0 ] ? &c : nullptr, s[ 0 ] ? records : nullptr );
        unsigned o;
        std::memcpy( &o, &node.rate_scale, 4 );
        boatless += s[ 0 ] ? 0 : 1;
        zeroed += o == 0u ? 1 : 0;
        live += o != 0u && s[ 0 ] ? 1 : 0;
        if ( o != s[ 22 ] || s[ 23 ] != s[ 21 ] )
        {
            ++failed;
            LOG_ERROR( "boat_thrust vector %d: rate 0x%08X, retail 0x%08X", static_cast< int >( index ), o, s[ 22 ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "pfx_boat_thrust_rate matches VuPfxEmitBoatThrustQuadFountainInstance 0x4CD5B0 on %d vectors (%d with a live "
                     "rate, %d zero, %d boatless), the rate scale bit for bit",
            static_cast< int >( g_golden_bth_count ), live, zeroed, boatless );
    else
        LOG_ERROR( "boat_thrust FAILED %d of %d", failed, static_cast< int >( g_golden_bth_count ) );
    return failed == 0;
}

// pfx_tick_spring (world/pfx.hxx) and pfx_tick_boat_spring (world/spray.hxx) against the spring constraints' slot 1
// 0x58E1C0 / 0x4CD980 (case pfx_springs): 12 linked particles per vector, every particle word after the tick.
inline bool verify_pfx_springs( )
{
    int failed = 0, moved = 0;
    const unsigned* s = g_golden_psp_words;
    for ( std::size_t index = 0; index < g_golden_psp_count; ++index )
    {
        const bool boat = s[ 0 ] != 0u;
        const bool present = s[ 1 ] != 0u;
        const float dt = water_bits( s[ 2 ] );
        float params[ 6 ], offset[ 3 ], pos[ 3 ], vel[ 3 ];
        for ( int k = 0; k < 6; ++k )
            params[ k ] = water_bits( s[ 3 + k ] );
        for ( int k = 0; k < 3; ++k )
        {
            offset[ k ] = water_bits( s[ 9 + k ] );
            pos[ k ] = water_bits( s[ 12 + k ] );
            vel[ k ] = water_bits( s[ 15 + k ] );
        }
        s += 18;
        int bad = 0;
        for ( int i = 0; i < 12; ++i, s += 64 )
        {
            std::uint32_t w[ 32 ];
            std::memcpy( w, s, sizeof( w ) );
            float* p = reinterpret_cast< float* >( w );
            if ( boat )
                pfx_tick_boat_spring( p, params, present ? pos : nullptr, present ? vel : nullptr, dt );
            else
                pfx_tick_spring( p, params, offset, dt );
            moved += std::memcmp( s, s + 32, 128 ) != 0 ? 1 : 0;
            bad += std::memcmp( w, s + 32, 128 ) != 0 ? 1 : 0;
        }
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "pfx_springs vector %d (%s): %d particles differ", static_cast< int >( index ), boat ? "boat" : "spring", bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "pfx_tick_spring / pfx_tick_boat_spring match VuPfxSpringConstraintInstance 0x58E1C0 and "
                     "VuPfxBoatSpringConstraintInstance 0x4CD980 on %d vectors x 12 particles (%d moved), every word",
            static_cast< int >( g_golden_psp_count ), moved );
    else
        LOG_ERROR( "pfx_springs FAILED %d of %d", failed, static_cast< int >( g_golden_psp_count ) );
    return failed == 0;
}

inline bool verify_water_all( )
{
    bool ok = true;
    ok = verify_pfx_springs( ) && ok;
    ok = verify_boat_thrust( ) && ok;
    ok = verify_water_point( ) && ok;
    ok = verify_wake_kernels96( ) && ok;
    ok = verify_ocean_field( ) && ok;
    ok = verify_ocean_kernels96( ) && ok;
    ok = verify_water_built_tick( ) && ok;
    ok = verify_water_query96( ) && ok;
    ok = verify_water_build( ) && ok;
    ok = verify_water_query( ) && ok;
    ok = verify_point_waves( ) && ok;
    ok = verify_ocean_kernels( ) && ok;
    ok = verify_wake_kernels( ) && ok;
    ok = verify_wave_bridges( ) && ok;
    ok = verify_water_tick( ) && ok;
    ok = verify_wave_ticks( ) && ok;
    ok = verify_pfx_ticks( ) && ok;
    ok = verify_matrix_helpers( ) && ok;
    ok = verify_water_tree( ) && ok;
    ok = verify_water_waves( ) && ok;
    ok = verify_wake_lookup( ) && ok;
    ok = verify_quad_pattern_tick( ) && ok;
    ok = verify_geom_pattern_tick( ) && ok;
    ok = verify_fountain_emit( ) && ok;
    return ok;
}
