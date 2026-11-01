#pragma once

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

inline bool verify_watchers_all( )
{
    bool ok = true;
    ok = verify_water_controls( ) && ok;
    return ok;
}
