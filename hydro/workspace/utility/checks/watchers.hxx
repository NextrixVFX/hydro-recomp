#pragma once

#include <workspace/core/world/watchers.hxx>

// THE WATER ENTITIES' CONTROL / MOTION HANDLERS (world/watchers.hxx) against retail on Hydroelectric's built water.

// water_ocean_control / water_directional_control against the ocean entities' 0x5D8930 and VuDirectionalWaveEntity's
// 0x5D9D90 -> 0x5D9DD0 (case water_controls): every such entity, two rounds, its control records given new values.
// Compared per call: the entity's property words, the wave's known words (+24 / +40, the bounds' stack words, and +0x40,
// the entry list pointer, excepted);
// an ocean: one slot 2 tick (0x59CC00: ocean_tick + ocean_field_rebuild on b.oceans' params) - alive, the current
// heights' FNV-1a 64, the clock words; a directional: the layout box (the w lanes excepted), the wave's entry list, the
// pool, every surface's wave tree (summary + FNV-1a 64).
inline bool verify_water_controls( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "water_controls: the document did not load" );
        return false;
    }
    water_built_t b{};
    water_build_world( *tree, b, []( entity_t& e ) { return entity_transform( e ); } );
    std::vector< entity_t* > order;
    const auto post = [ & ]( const auto& self, entity_t& e ) -> void {
        for ( auto& child : e.m_children )
            self( self, *child );
        if ( water_kind_of( e.m_type ) != water_kind_none )
            order.push_back( &e );
    };
    post( post, *tree );
    const unsigned* s = g_golden_wct_words;
    const unsigned n = *s++;
    int failed = 0, oceans = 0, directionals = 0, compared = 0;
    for ( unsigned c = 0; c < n; ++c )
    {
        const unsigned index = *s++;
        const int wave = static_cast< int >( *s++ );
        const bool ocean = *s++ != 0;
        const unsigned nrec = *s++;
        const unsigned* records = s;
        s += nrec;
        const unsigned* props = s;
        s += 10;
        const unsigned nwords = *s++;
        const unsigned* words = s;
        s += nwords;
        int bad = 0;
        if ( index >= order.size( ) || !order[ index ]->m_state || !entity_transform( *order[ index ] ) )
        {
            LOG_ERROR( "water_controls: entity %u missing", index );
            return false;
        }
        entity_t& e = *order[ index ];
        water_entity_state_t& st = *static_cast< water_entity_state_t* >( e.m_state.get( ) );
        water_layout_box_t layout{};
        if ( ocean )
        {
            ++oceans;
            bad += water_ocean_control( b, st, wave, records ) ? 0 : 1;
        }
        else
        {
            ++directionals;
            water_directional_control( b, st, wave, *entity_transform( e ), records, layout );
        }
        for ( int k = 2; k < 10; ++k )   // +0x68 .. +0x84 (+0x60 / +0x64 are component pointers)
            if ( st.value.known[ k ] )
            {
                ++compared;
                bad += st.value.word[ k ] != props[ k ];
            }
        const water_words_t& w = b.waves[ static_cast< std::size_t >( wave ) ];
        for ( unsigned k = 0; k < nwords && k < 80; ++k )
            if ( w.known[ k ] && k != 6 && k != 10 && k != 16 )   // +0x40: the entry list's pointer
            {
                ++compared;
                bad += w.word[ k ] != words[ k ];
            }
        if ( ocean )
        {
            water_words_t& o = b.waves[ static_cast< std::size_t >( wave ) ];
            water_built_t::ocean_t* field = water_ocean_of( b, wave );
            ocean_clock_t clock{};
            std::memcpy( &clock.time, &o.word[ 0x88 / 4 ], 8 );
            clock.current = static_cast< int >( o.word[ 0xA4 / 4 ] );
            clock.previous = static_cast< int >( o.word[ 0xA8 / 4 ] );
            std::memcpy( &clock.buffer_time[ 0 ], &o.word[ 0xB0 / 4 ], 8 );
            std::memcpy( &clock.buffer_time[ 1 ], &o.word[ 0xC0 / 4 ], 8 );
            struct call_t
            {
                water_built_t::ocean_t* field;
                ocean_clock_t* clock;
            } call{ field, &clock };
            const bool alive = ocean_tick( clock, static_cast< int >( o.word[ 1 ] ), 1.0f / 60.0f,
                []( void* u ) {
                    call_t& k = *static_cast< call_t* >( u );
                    if ( k.field && k.field->field.n )
                        ocean_field_rebuild( k.field->field, k.field->params, k.clock->time, k.clock->current );
                },
                &call );
            std::uint32_t t[ 2 ];
            std::memcpy( t, &clock.time, 8 );
            o.set( 0x88, t[ 0 ] );
            o.set( 0x8C, t[ 1 ] );
            o.set( 0xA4, static_cast< std::uint32_t >( clock.current ) );
            o.set( 0xA8, static_cast< std::uint32_t >( clock.previous ) );
            for ( int k = 0; k < 2; ++k )
            {
                std::memcpy( t, &clock.buffer_time[ k ], 8 );
                o.set( 0xB0 + 16 * k, t[ 0 ] );
                o.set( 0xB4 + 16 * k, t[ 1 ] );
            }
            bad += ( alive ? 1u : 0u ) != *s++;
            std::uint64_t hv = 0;
            if ( field && field->field.n )
            {
                const std::vector< float >& hs = field->field.heights[ clock.current ];
                hv = water_fnv64( hs.data( ), hs.size( ) );
            }
            bad += static_cast< unsigned >( hv & 0xFFFFFFFFu ) != s[ 0 ] || static_cast< unsigned >( hv >> 32 ) != s[ 1 ];
            s += 2;
            for ( int k = 0; k < 16; ++k, ++s )
                if ( o.known[ 0x88 / 4 + k ] )
                {
                    ++compared;
                    bad += o.word[ 0x88 / 4 + k ] != *s;
                }
            compared += 3;
        }
        else
        {
            for ( int k = 0; k < 8; ++k, ++s )
                if ( layout.known[ k ] )
                {
                    ++compared;
                    bad += layout.word[ k ] != *s;
                }
            const unsigned ne = *s++;
            int at = b.hold.waves[ static_cast< std::size_t >( wave ) ].entries;
            for ( unsigned k = 0; k < ne; ++k, s += 2 )
            {
                bad += at < 0 || static_cast< unsigned >( at ) != s[ 0 ] || static_cast< unsigned >( b.hold.pool[ at ].surface ) != s[ 1 ];
                if ( at >= 0 )
                    at = b.hold.pool[ at ].next;
            }
            bad += at != -1;
            bad += static_cast< unsigned >( b.hold.pool_count ) != *s++;
            bad += static_cast< unsigned >( b.hold.pool_free ) != *s++;
            const unsigned nt = *s++;
            for ( unsigned k = 0; k < nt; ++k, s += 9 )
            {
                const std::vector< unsigned > tw = water_tree_words( b.hold.surfaces[ s[ 0 ] ].waves );
                const std::uint64_t hv = water_fnv64( tw.data( ) + 6, tw.size( ) - 6 );
                for ( int j = 0; j < 6; ++j )
                    bad += tw[ static_cast< std::size_t >( j ) ] != s[ 1 + j ];
                bad += static_cast< unsigned >( hv & 0xFFFFFFFFu ) != s[ 7 ] || static_cast< unsigned >( hv >> 32 ) != s[ 8 ];
                compared += 8;
            }
        }
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "water_controls: call %u (entity %u, wave %d, %s): %d differences", c, index, wave, ocean ? "ocean" : "directional", bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "water_ocean_control / water_directional_control match 0x5D8930 and 0x5D9D90 / 0x5D9DD0 on Hydroelectric's built water: "
                     "%d ocean calls (each then ticked: heights hashed), %d directional (re-described, re-held, layout box), %d values",
            oceans, directionals, compared );
    else
        LOG_ERROR( "water_controls FAILED %d of %u", failed, n );
    return failed == 0;
}

// VuDirectionalFlowWaveEntity / VuWaterDirectionalFlowWave against retail on Hydroelectric's built water (case flow_wave):
// six synthetic entities (their transforms and properties are the case's inputs). Per entity: water_build_entity
// (water_kind_flow: slot 6 0x5D66D0 -> 0x5D6810 / 0x5289B0 / 0x59C3C0 / 0x59C4C0) - the wave's known words (+24 / +40 the
// bounds' stack words, +0x40 the entry pointer excepted), its entries, the wave list, the pool, every surface's wave tree
// (summary + FNV-1a 64); water_view_kernel on 24 vertices per mode (slot 3 0x59C390: base 0x59C7C0, tag 0x59C920, +96
// nothing) - every vertex word; water_flow_refresh (slot 2 0x5D66C0 -> 0x5D6740) - the same state and the layout box (w
// lanes excepted); on every other entity water_entity_release (slot 7 0x5D6710): destroyed, the list, pool, trees.
inline bool flow_check_state( const water_built_t& b, int wave, const unsigned*& s, bool words, int& compared )
{
    int bad = 0;
    if ( words )
    {
        const water_words_t& w = b.waves[ static_cast< std::size_t >( wave ) ];
        for ( int k = 0; k < 64; ++k )
            if ( w.known[ k ] && k != 6 && k != 10 && k != 16 )
            {
                ++compared;
                bad += w.word[ k ] != s[ k ];
            }
        s += 64;
    }
    const unsigned ne = *s++;
    int at = b.hold.waves[ static_cast< std::size_t >( wave ) ].entries;
    for ( unsigned k = 0; k < ne; ++k, s += 2 )
    {
        bad += at < 0 || static_cast< unsigned >( at ) != s[ 0 ] || static_cast< unsigned >( b.hold.pool[ at ].surface ) != s[ 1 ];
        if ( at >= 0 )
            at = b.hold.pool[ at ].next;
    }
    bad += words && at != -1;   // after a release the hold's entry index is stale (retail: the freed wave's word)
    const unsigned nl = *s++;
    at = b.hold.list;
    for ( unsigned k = 0; k < nl; ++k, s += 2 )
    {
        bad += static_cast< unsigned >( at ) != s[ 0 ];
        if ( at < 0 )
            continue;
        bad += static_cast< unsigned >( b.hold.pool[ at ].leaf ) != s[ 1 ];
        at = b.hold.pool[ at ].next;
    }
    bad += at != -1;
    bad += static_cast< unsigned >( b.hold.pool_count ) != *s++;
    bad += static_cast< unsigned >( b.hold.pool_free ) != *s++;
    const unsigned nt = *s++;
    for ( unsigned k = 0; k < nt; ++k, s += 9 )
    {
        const std::vector< unsigned > tw = water_tree_words( b.hold.surfaces[ s[ 0 ] ].waves );
        const std::uint64_t hv = water_fnv64( tw.data( ) + 6, tw.size( ) - 6 );
        for ( int j = 0; j < 6; ++j )
            bad += tw[ static_cast< std::size_t >( j ) ] != s[ 1 + j ];
        bad += static_cast< unsigned >( hv & 0xFFFFFFFFu ) != s[ 7 ] || static_cast< unsigned >( hv >> 32 ) != s[ 8 ];
        compared += 8;
    }
    return bad;
}

inline bool verify_flow_wave( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "flow_wave: the document did not load" );
        return false;
    }
    water_built_t b{};
    water_build_world( *tree, b, []( entity_t& e ) { return entity_transform( e ); } );
    const unsigned* s = g_golden_fwv_words;
    const unsigned n = *s++;
    int failed = 0, compared = 0, vertices = 0, releases = 0;
    std::vector< std::pair< int, const unsigned* > > released;
    for ( unsigned c = 0; c < n; ++c )
    {
        int bad = 0;
        const int index = static_cast< int >( *s++ );
        const std::shared_ptr< water_entity_state_t > st = water_entity_state_for( "VuDirectionalFlowWaveEntity" );
        for ( int k = 0; k < 6; ++k )
            st->value.word[ k ] = *s++;
        const unsigned* transform = s;
        s += k_water_transform_words;
        water_input_t in{};
        in.entity = st->value.word;
        in.transform = transform;
        water_build_entity( b, water_kind_flow, in );
        const int wave = static_cast< int >( b.waves.size( ) ) - 1;
        bad += wave != index;
        bad += flow_check_state( b, wave, s, true, compared );
        water_view_t view;
        water_view_build( b, view );
        for ( int m = 0; m < 3; ++m )
        {
            const unsigned mode = *s++;
            const unsigned* tags = s;
            s += 24;
            water_vertex_t v[ 24 ];
            std::memcpy( v, s, sizeof( v ) );
            s += 240;
            water_query_t ctx{};
            ctx.count = 24;
            ctx.vertices = v;
            ctx.tags = tags;
            ctx.tag = 1;
            ctx.flag96 = mode == 2 ? 1u : 0u;
            ctx.flag100 = mode == 1 ? 1u : 0u;
            bad += water_view_kernel( &view, wave, ctx ) ? 0 : 1;
            bad += std::memcmp( v, s, sizeof( v ) ) != 0;
            s += 240;
            compared += 240;
            vertices += 24;
        }
        for ( int k = 0; k < 6; ++k )
            st->value.word[ k ] = *s++;
        const unsigned* transform2 = s;
        s += k_water_transform_words;
        water_layout_box_t layout{};
        water_flow_refresh( b, *st, wave, transform2, layout );
        bad += flow_check_state( b, wave, s, true, compared );
        for ( int k = 0; k < 8; ++k, ++s )
            if ( layout.known[ k ] )
            {
                ++compared;
                bad += layout.word[ k ] != *s;
            }
        if ( *s++ )
        {
            // retail releases after all six are made (the case's order): the block kept for then
            released.emplace_back( wave, s );
            const unsigned* q = s + 4;   // +0x78, the dtor's this, the dtor count, +4
            q += 1 + 2 * q[ 0 ];
            q += 1 + 2 * q[ 0 ];
            q += 2;
            q += 1 + 9 * q[ 0 ];
            s = q;
        }
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "flow_wave: entity %u (wave %d): %d differences", c, index, bad );
        }
    }
    for ( const auto& r : released )
    {
        ++releases;
        const int wave = r.first;
        const unsigned* q = r.second;
        const unsigned e78 = *q++, dtor_this = *q++, dtors = *q++, ref = *q++;
        water_entity_release( b, wave );   // slot 7 0x5D6710; the entity's +0x78 = 0 is the caller's
        const bool gone = wave < static_cast< int >( b.wave_gone.size( ) ) && b.wave_gone[ static_cast< std::size_t >( wave ) ];
        int bad = e78 != 0u || ( gone ? 1u : 0u ) != dtors || ( gone && dtor_this != 1u );
        bad += gone ? ref != 0xFFFFFFFFu : ref != b.waves[ static_cast< std::size_t >( wave ) ].word[ 1 ];
        bad += flow_check_state( b, wave, q, false, compared );
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "flow_wave: the release of wave %d: %d differences", wave, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuDirectionalFlowWaveEntity / VuWaterDirectionalFlowWave (slot 6 0x5D66D0: 0x5D6810, 0x5289B0, 0x59C3C0, 0x59C4C0; slot 3 "
                     "0x59C390; slot 2 0x5D66C0 -> 0x5D6740; slot 7 0x5D6710) match retail on Hydroelectric's built water: %u entities, %d "
                     "vertices in three modes, %d releases, %d values plus every wave tree",
            n, vertices, releases, compared );
    else
        LOG_ERROR( "flow_wave FAILED %d of %u", failed, n );
    return failed == 0;
}

// water_surface_motion (0x5DC3B0 = entity_set_world + the watcher water_surface_refresh / water_ramp_refresh) against retail
// on Hydroelectric's built water (case water_surface_motion): every surface / ramp entity in entity order handed a new world
// (rotated about z and moved) through its motion component. Compared per call: the transform's world, the surface's known
// words (+220 / +236 the box's stack w lanes excepted), the ramp wave's known words (+0x40 the entry pointer excepted), the
// draw and layout boxes (w lanes excepted) and the draw update, the surface tree word for word, every surface's wave tree
// (summary + FNV-1a 64), the wave list, the pool.
inline bool verify_water_surface_motion( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "water_surface_motion: the document did not load" );
        return false;
    }
    water_built_t b{};
    water_build_world( *tree, b, []( entity_t& e ) { return entity_transform( e ); } );
    std::vector< entity_t* > order;
    const auto post = [ & ]( const auto& self, entity_t& e ) -> void {
        for ( auto& child : e.m_children )
            self( self, *child );
        if ( water_kind_of( e.m_type ) != water_kind_none )
            order.push_back( &e );
    };
    post( post, *tree );
    const unsigned* s = g_golden_wsm_words;
    const unsigned n = *s++;
    int failed = 0, ramps = 0, compared = 0, updates = 0;
    for ( unsigned c = 0; c < n; ++c )
    {
        const unsigned index = *s++;
        const int surface = static_cast< int >( *s++ );
        const int wave = static_cast< int >( *s++ );
        const bool registered = *s++ != 0;
        float matrix[ 16 ];
        std::memcpy( matrix, s, 64 );
        s += 16;
        const unsigned* world = s;
        s += 16;
        if ( index >= order.size( ) )
        {
            LOG_ERROR( "water_surface_motion: entity %u missing", index );
            return false;
        }
        entity_t& e = *order[ index ];
        int bad = 0;
        const water_built_t::entity_ref_t* r = water_entity_of( b, e );
        bad += !r || r->surface != surface || r->wave != wave;
        ramps += wave >= 0 ? 1 : 0;
        water_surface_boxes_t boxes{};
        bad += water_surface_motion( b, e, matrix, registered, boxes ) ? 0 : 1;
        const transform_component_t* t = entity_transform( e );
        for ( int k = 0; k < 16; ++k )
        {
            std::uint32_t u;
            std::memcpy( &u, &t->world[ k ], 4 );
            bad += u != world[ k ];
        }
        compared += 16;
        const unsigned nw = *s++;
        const water_words_t& sw = b.surfaces[ static_cast< std::size_t >( surface ) ];
        for ( unsigned k = 0; k < nw && k < 80; ++k )
            if ( sw.known[ k ] && k != 55 && k != 59 )
            {
                ++compared;
                bad += sw.word[ k ] != s[ k ];
            }
        s += nw;
        const unsigned nv = *s++;
        if ( wave >= 0 )
        {
            const water_words_t& ww = b.waves[ static_cast< std::size_t >( wave ) ];
            for ( unsigned k = 0; k < nv && k < 80; ++k )
                if ( ww.known[ k ] && k != 16 )
                {
                    ++compared;
                    bad += ww.word[ k ] != s[ k ];
                }
        }
        s += nv;
        for ( int k = 0; k < 8; ++k )
            if ( boxes.draw.known[ k ] )
            {
                ++compared;
                bad += boxes.draw.word[ k ] != s[ k ];
            }
        s += 8;
        for ( int k = 0; k < 8; ++k )
            if ( boxes.layout.known[ k ] )
            {
                ++compared;
                bad += boxes.layout.word[ k ] != s[ k ];
            }
        s += 8;
        updates += boxes.draw_update ? 1 : 0;
        bad += boxes.draw_update != registered;
        const unsigned ns = *s++;
        bad += water_tree_words( b.hold.surface_tree ) != std::vector< unsigned >( s, s + ns );
        compared += static_cast< int >( ns );
        s += ns;
        const unsigned nt = *s++;
        for ( unsigned k = 0; k < nt; ++k, s += 9 )
        {
            const std::vector< unsigned > tw = water_tree_words( b.hold.surfaces[ s[ 0 ] ].waves );
            const std::uint64_t hv = water_fnv64( tw.data( ) + 6, tw.size( ) - 6 );
            for ( int j = 0; j < 6; ++j )
                bad += tw[ static_cast< std::size_t >( j ) ] != s[ 1 + j ];
            bad += static_cast< unsigned >( hv & 0xFFFFFFFFu ) != s[ 7 ] || static_cast< unsigned >( hv >> 32 ) != s[ 8 ];
            compared += 8;
        }
        const unsigned nl = *s++;
        int at = b.hold.list;
        for ( unsigned k = 0; k < nl; ++k, s += 2 )
        {
            bad += static_cast< unsigned >( at ) != s[ 0 ];
            if ( at < 0 )
                continue;
            bad += static_cast< unsigned >( b.hold.pool[ at ].leaf ) != s[ 1 ];
            at = b.hold.pool[ at ].next;
        }
        bad += at != -1;
        bad += static_cast< unsigned >( b.hold.pool_count ) != *s++;
        bad += static_cast< unsigned >( b.hold.pool_free ) != *s++;
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "water_surface_motion: call %u (entity %u, surface %d, wave %d): %d differences", c, index, surface, wave, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "water_surface_motion matches 0x5DC3B0 (0x5344A0 + the watchers 0x5DB610 / 0x5D70A0: 0x535C50 + 0x529990 / 0x529860, "
                     "0x5A2370) on Hydroelectric's built water: %u entities (%d ramps, %d draw updates), %d values plus every wave tree",
            n, ramps, updates, compared );
    else
        LOG_ERROR( "water_surface_motion FAILED %d of %u", failed, n );
    return failed == 0;
}
// pfx_static_motion (world/watchers.hxx) against VuStaticPfxEntity's 0x5EB4C0 (case static_pfx_motion): 12 entities with
// no parent, the instance on every other. Compared: the transform's local, world, rotation and world euler (lanes 0 - 2;
// the fourth is a stack word) bit for bit, and the setMatrix calls (count, the instance, the 16 words).
inline bool verify_static_pfx_motion( )
{
    const unsigned* s = g_golden_spm_words;
    const unsigned n = *s++;
    int failed = 0, told = 0;
    for ( unsigned v = 0; v < n; ++v )
    {
        float matrix[ 16 ];
        std::memcpy( matrix, s, 64 );
        s += 16;
        const unsigned inst = *s++;
        const unsigned nt = *s++;
        const unsigned* after = s;
        s += nt;
        const unsigned nc = *s++;
        entity_t owner;
        auto st = std::make_shared< transform_state_t >( );
        std::memset( &st->c, 0x77, sizeof( st->c ) );
        owner.m_components.push_back( entity_component_t{ "VuTransformComponent", k_transform_component_vtable, st } );
        pfx_static_entity_t p{};
        p.instance = inst ? 0x1234u : 0u;
        struct log_t
        {
            std::vector< std::vector< unsigned > > calls;
        } log;
        pfx_static_ops_t o{};
        o.user = &log;
        o.set_matrix = []( void* u, std::uint32_t instance, const float* m ) {
            std::vector< unsigned > c( 17 );
            c[ 0 ] = instance == 0x1234u ? 1u : 0u;
            std::memcpy( &c[ 1 ], m, 64 );
            static_cast< log_t* >( u )->calls.push_back( c );
        };
        int bad = pfx_static_motion( owner, p, o, matrix ) ? 0 : 1;
        const transform_component_t& c = st->c;
        for ( int w = 0; w < 40 && w < static_cast< int >( nt ); ++w )
        {
            if ( w == 35 || w == 39 )
                continue;
            const float* f = w < 16 ? &c.local[ w ] : w < 32 ? &c.world[ w - 16 ] : w < 36 ? &c.rotation[ w - 32 ] : &c.world_rotation[ w - 36 ];
            bad += std::memcmp( f, &after[ w ], 4 ) != 0;
        }
        bad += log.calls.size( ) != nc;
        for ( unsigned k = 0; k < nc; ++k, s += 17 )
            if ( k < log.calls.size( ) )
                bad += log.calls[ k ] != std::vector< unsigned >( s, s + 17 );
        told += static_cast< int >( nc );
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "static_pfx_motion: vector %u: %d differences", v, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "pfx_static_motion matches VuStaticPfxEntity's motion handler 0x5EB4C0 over %u vectors (%d setMatrix calls): local, world, "
                     "both eulers bit for bit", n, told );
    else
        LOG_ERROR( "static_pfx_motion FAILED %d of %u", failed, n );
    return failed == 0;
}
// g_pfx_types (world/pfxclass.hxx) against the three getType slots (case pfx_types: called, the chain walked).
inline bool verify_pfx_types( )
{
    int bad = 0;
    const std::size_t n = sizeof( g_pfx_types ) / sizeof( g_pfx_types[ 0 ] );
    for ( std::size_t i = 0; i < g_golden_pty_count; ++i )
    {
        const golden_water_type_t& g = g_golden_pty[ i ];
        const water_type_t* t = nullptr;
        for ( std::size_t k = 0; k < n; ++k )
            if ( g_pfx_types[ k ].address == g.address )
                t = &g_pfx_types[ k ];
        if ( t == nullptr || t->descriptor != g.descriptor || std::strcmp( t->chain, g.chain ) != 0 )
        {
            ++bad;
            LOG_ERROR( "pfx_types 0x%X: descriptor / chain differ (retail 0x%X %s)", g.address, g.descriptor, g.chain );
        }
    }
    bad += n != g_golden_pty_count ? 1 : 0;
    if ( bad == 0 )
        LOG_SUCCESS( "g_pfx_types match the pfx classes' %d getType slots 0x552D40 / 0x5CAA20 / 0x5CB950 (descriptor and chain each)",
            static_cast< int >( n ) );
    else
        LOG_ERROR( "pfx_types FAILED %d", bad );
    return bad == 0;
}

// water_point_entity_layout / water_point_entity_emit / water_texture_settings_apply against 0x5D4CB0 / 0x5D4B40 / 0x5D2B00
// (case water_small): per point entity the layout box (w lanes excepted), the pending byte and the answer; per settings
// call the renderer block's 10 words around the copy and the answer.
inline bool verify_water_small( )
{
    const unsigned* s = g_golden_wsl_words;
    int bad = 0;
    const unsigned np = *s++;
    for ( unsigned v = 0; v < np; ++v, s += 11 )
    {
        water_point_entity_t e{};
        e.desc[ 7 ] = s[ 0 ];
        water_layout_box_t box{};
        water_point_entity_layout( e, box );
        for ( int k = 0; k < 8; ++k )
            bad += box.known[ k ] && box.word[ k ] != s[ 1 + k ];
        const std::uint32_t out = water_point_entity_emit( e );
        bad += ( e.pending ? 1u : 0u ) != s[ 9 ] || out != s[ 10 ];
    }
    const unsigned nt = *s++;
    for ( unsigned v = 0; v < nt; ++v, s += 20 )
    {
        std::uint32_t block[ 10 ];
        for ( int k = 0; k < 10; ++k )
            block[ k ] = 0x11111111u;
        const std::uint32_t out = water_texture_settings_apply( s[ 8 ] != 0, s, block + 1 );
        for ( int k = 0; k < 10; ++k )
            bad += block[ k ] != s[ 9 + k ];
        bad += out != s[ 19 ];
    }
    if ( bad == 0 )
        LOG_SUCCESS( "water_point_entity_layout / _emit and water_texture_settings_apply match 0x5D4CB0 / 0x5D4B40 / 0x5D2B00: %u point entities, "
                     "%u settings calls", np, nt );
    else
        LOG_ERROR( "water_small FAILED %d", bad );
    return bad == 0;
}
// water_wave_motion / water_wave_refresh against the wave entities' transform watchers 0x5D8D70 / 0x5D9390 / 0x5D7E30 /
// 0x5D36C0 reached through setWorldTransform 0x5344A0 (case wave_refresh): every rectangular / circular ocean, whirlpool and
// banked turn entity of Hydroelectric's built water, two rounds (round 2 with new property words first, the first
// rectangular ocean's Complexity lowered by one). Compared per call: the transform's world, the wave's known words (+24 /
// +40, the bounds' stack words, and +0x40 the entry pointer, excepted; an ocean's +0xB8 / +0xBC / +0xC8 / +0xCC buffer
// pointers and +0x90 .. +0xA0 array pointers are not words the C++ holds), an ocean's heights and patches of both buffers
// (FNV-1a 64), the layout box (w lanes excepted), the wave's entry list, the pool, every surface's wave tree.
inline bool verify_wave_refresh( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "wave_refresh: the document did not load" );
        return false;
    }
    water_built_t b{};
    water_build_world( *tree, b, []( entity_t& e ) { return entity_transform( e ); } );
    std::vector< entity_t* > order;
    const auto post = [ & ]( const auto& self, entity_t& e ) -> void {
        for ( auto& child : e.m_children )
            self( self, *child );
        if ( water_kind_of( e.m_type ) != water_kind_none )
            order.push_back( &e );
    };
    post( post, *tree );
    const unsigned* s = g_golden_wrf_words;
    const unsigned n = *s++;
    int failed = 0, oceans = 0, compared = 0, hashes = 0;
    for ( unsigned c = 0; c < n; ++c )
    {
        const unsigned index = *s++;
        const int wave = static_cast< int >( *s++ );
        const unsigned round = *s++;
        const unsigned* props = s;
        s += 13;
        float matrix[ 16 ];
        std::memcpy( matrix, s, 64 );
        s += 16;
        const unsigned* world = s;
        s += 16;
        const unsigned nw = *s++;
        const unsigned* words = s;
        s += nw;
        const unsigned nf = *s++;
        const unsigned* field = s;
        s += nf;
        if ( index >= order.size( ) || !order[ index ]->m_state )
        {
            LOG_ERROR( "wave_refresh: entity %u missing", index );
            return false;
        }
        entity_t& e = *order[ index ];
        water_entity_state_t& st = *static_cast< water_entity_state_t* >( e.m_state.get( ) );
        for ( int k = 0; k < 13 && k < k_water_entity_words; ++k )
            if ( st.value.known[ k ] )
                st.value.word[ k ] = props[ k ];
        int bad = 0;
        const water_built_t::entity_ref_t* r = water_entity_of( b, e );
        bad += !r || r->wave != wave;
        water_layout_box_t layout{};
        bad += water_wave_motion( b, e, matrix, layout ) ? 0 : 1;
        const transform_component_t* t = entity_transform( e );
        for ( int k = 0; k < 16; ++k )
        {
            std::uint32_t u;
            std::memcpy( &u, &t->world[ k ], 4 );
            bad += u != world[ k ];
        }
        compared += 16;
        const water_words_t& w = b.waves[ static_cast< std::size_t >( wave ) ];
        const bool ocean = nf != 0;
        for ( unsigned k = 0; k < nw && k < 64; ++k )
        {
            const bool pointer = ocean && ( ( k >= 0x90 / 4 && k <= 0xA0 / 4 ) || k == 0xB8 / 4 || k == 0xBC / 4 || k == 0xC8 / 4 || k == 0xCC / 4 );
            if ( w.known[ k ] && k != 6 && k != 10 && k != 16 && !pointer )
            {
                ++compared;
                bad += w.word[ k ] != words[ k ];
            }
        }
        if ( ocean )
        {
            ++oceans;
            const water_built_t::ocean_t* o = water_ocean_of( b, wave );
            for ( int k = 0; k < 2; ++k )
            {
                const std::uint64_t hv = o ? water_fnv64( o->field.heights[ k ].data( ), o->field.heights[ k ].size( ) ) : 0u;
                const std::uint64_t pv = o ? water_fnv64( o->field.patches[ k ].data( ), o->field.patches[ k ].size( ) ) : 0u;
                bad += static_cast< unsigned >( hv & 0xFFFFFFFFu ) != field[ 4 * k ] || static_cast< unsigned >( hv >> 32 ) != field[ 4 * k + 1 ];
                bad += static_cast< unsigned >( pv & 0xFFFFFFFFu ) != field[ 4 * k + 2 ] || static_cast< unsigned >( pv >> 32 ) != field[ 4 * k + 3 ];
                hashes += 2;
            }
        }
        for ( int k = 0; k < 8; ++k, ++s )
            if ( layout.known[ k ] )
            {
                ++compared;
                bad += layout.word[ k ] != *s;
            }
        const unsigned ne = *s++;
        int at = b.hold.waves[ static_cast< std::size_t >( wave ) ].entries;
        for ( unsigned k = 0; k < ne; ++k, s += 2 )
        {
            bad += at < 0 || static_cast< unsigned >( at ) != s[ 0 ] || static_cast< unsigned >( b.hold.pool[ at ].surface ) != s[ 1 ];
            if ( at >= 0 )
                at = b.hold.pool[ at ].next;
        }
        bad += at != -1;
        bad += static_cast< unsigned >( b.hold.pool_count ) != *s++;
        bad += static_cast< unsigned >( b.hold.pool_free ) != *s++;
        const unsigned nt = *s++;
        for ( unsigned k = 0; k < nt; ++k, s += 9 )
        {
            const std::vector< unsigned > tw = water_tree_words( b.hold.surfaces[ s[ 0 ] ].waves );
            const std::uint64_t hv = water_fnv64( tw.data( ) + 6, tw.size( ) - 6 );
            for ( int j = 0; j < 6; ++j )
                bad += tw[ static_cast< std::size_t >( j ) ] != s[ 1 + j ];
            bad += static_cast< unsigned >( hv & 0xFFFFFFFFu ) != s[ 7 ] || static_cast< unsigned >( hv >> 32 ) != s[ 8 ];
            compared += 8;
        }
        if ( bad )
        {
            ++failed;
            LOG_ERROR( "wave_refresh: call %u (entity %u, wave %d, round %u): %d differences", c, index, wave, round, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "water_wave_motion / water_wave_refresh match the wave entities' watchers 0x5D8D70 / 0x5D9390 / 0x5D7E30 / 0x5D36C0 "
                     "(0x59D9B0 / 0x59DDC0 / 0x5A1580 / 0x5A4EF0) through setWorldTransform on Hydroelectric's built water: %u calls (%d "
                     "ocean, one complexity change), %d values, %d field hashes",
            n, oceans, compared, hashes );
    else
        LOG_ERROR( "wave_refresh FAILED %d of %u", failed, n );
    return failed == 0;
}

// g_water_types_game (world/pfxclass.hxx) against the six game-tier water getType slots (case water_types_game).
inline bool verify_water_types_game( )
{
    int bad = 0;
    const std::size_t n = sizeof( g_water_types_game ) / sizeof( g_water_types_game[ 0 ] );
    for ( std::size_t i = 0; i < g_golden_wtg_count; ++i )
    {
        const golden_water_type_t& g = g_golden_wtg[ i ];
        const water_type_t* t = nullptr;
        for ( std::size_t j = 0; j < n; ++j )
            if ( g_water_types_game[ j ].address == g.address )
                t = &g_water_types_game[ j ];
        if ( t == nullptr || t->descriptor != g.descriptor || std::strcmp( t->chain, g.chain ) != 0 )
        {
            ++bad;
            LOG_ERROR( "water_types_game 0x%X: descriptor / chain differ (retail 0x%X %s)", g.address, g.descriptor, g.chain );
        }
    }
    bad += n != g_golden_wtg_count ? 1 : 0;
    if ( bad == 0 )
        LOG_SUCCESS( "g_water_types_game match the game-tier water classes' %d getType slots (descriptor and chain each)", static_cast< int >( n ) );
    else
        LOG_ERROR( "water_types_game FAILED %d", bad );
    return bad == 0;
}

inline bool verify_watchers_all( )
{
    bool ok = true;
    ok = verify_water_types_game( ) && ok;
    ok = verify_flow_wave( ) && ok;
    ok = verify_wave_refresh( ) && ok;
    ok = verify_water_controls( ) && ok;
    ok = verify_water_surface_motion( ) && ok;
    ok = verify_static_pfx_motion( ) && ok;
    ok = verify_pfx_types( ) && ok;
    ok = verify_water_small( ) && ok;
    return ok;
}
