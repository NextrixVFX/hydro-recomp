#pragma once

// THE RACE'S WAKE, water side (world/wake.hxx) against retail (case water_wake): two hulls - a plain wake and a flat one -
// driven along Hydroelectric's largest surface by water_wake_update (0x417D80's water half: 0x5287D0 / 0x528850, the
// ctor 0x52BEB0, the bounds 0x52C720, 0x529B90 / 0x529A90, the previous segment's release) with the records retail's run
// was handed, then water_built_tick (0x529E30; the wakes' slot 2 0x52C210). Compared per frame: both hulls' slots, the
// solver words the plain wake's caller writes, the wake dtors run, every listed wake wave's known words, the wave list,
// the pool, every listed wake's entries; at the end every surface's wave tree; and the two classes' type chains against
// the lookup flag (only VuWaterWakeWave 0x7368B8 is evaluated by 0x5296C0).

inline std::uint32_t water_bits_of( float f )
{
    std::uint32_t u;
    std::memcpy( &u, &f, 4 );
    return u;
}

inline bool verify_water_wake( )
{
    const std::unique_ptr< entity_t > tree = water_build_document_tree( );
    if ( !tree )
    {
        LOG_ERROR( "water_wake: the document did not load" );
        return false;
    }
    water_built_t b{};
    water_build_world( *tree, b, []( entity_t& e ) { return entity_transform( e ); } );
    int failed = 0, compared = 0, words = 0, made = 0;
    auto cmp = [ & ]( const char* what, int a, int c, std::uint32_t ours, std::uint32_t theirs ) {
        ++compared;
        if ( ours != theirs && ++failed <= 12 )
            LOG_ERROR( "water_wake %s %d / %d: ours 0x%08X retail 0x%08X", what, a, c, ours, theirs );
    };
    const unsigned* s = g_golden_wwk_words;
    const unsigned frames = *s++;
    const unsigned base = *s++;
    cmp( "built waves", 0, 0, static_cast< std::uint32_t >( b.waves.size( ) ), base );
    const unsigned n0 = *s++, n1 = *s++;
    bool chain_wake[ 2 ] = { false, false };
    for ( unsigned k = 0; k < n0 + n1; ++k, ++s )
        chain_wake[ k < n0 ? 0 : 1 ] = chain_wake[ k < n0 ? 0 : 1 ] || *s == 0x7368B8u;
    const std::uint32_t desc[ 2 ][ 3 ] = { { water_bits_of( 0.35f ), water_bits_of( 0.8f ), water_bits_of( 1.25f ) },
        { water_bits_of( 0.2f ), water_bits_of( 0.6f ), water_bits_of( 0.9f ) } };
    water_wake_slots_t plain[ 2 ], flat[ 2 ];
    std::uint32_t solver[ 2 ][ 3 ] = { { 0u, 0xFFFFFFFFu, 0xFFFFFFFFu }, { 0u, 0xFFFFFFFFu, 0xFFFFFFFFu } };
    water_built_hooks_t hooks{};
    hooks.render = []( void* ) {};
    const float dt = 1.0f / 30.0f;
    auto ix = []( int w ) { return static_cast< std::uint32_t >( w ); };
    for ( unsigned f = 0; f < frames; ++f )
    {
        const int fi = static_cast< int >( f );
        std::vector< std::uint8_t > before = b.wave_gone;
        before.resize( b.waves.size( ), 0 );
        for ( int k = 0; k < 2; ++k )
        {
            const unsigned* rec = s + 13 * k;
            const std::size_t count = b.waves.size( );
            water_wake_slots_t& slots = k == 1 ? flat[ k ] : plain[ k ];
            if ( water_wake_update( b, desc[ k ], rec, k == 1, slots ) == 1 )
            {
                solver[ k ][ 0 ] = 2u;
                solver[ k ][ 1 ] = ix( slots.current );
                solver[ k ][ 2 ] = ix( slots.previous );
            }
            for ( std::size_t w = count; w < b.waves.size( ); ++w, ++made )
                cmp( "lookup flag", fi, static_cast< int >( w ), b.hold.waves[ w ].wake ? 1u : 0u, chain_wake[ k ] ? 1u : 0u );
        }
        s += 26;
        before.resize( b.waves.size( ), 0 );
        const int missing = water_built_tick( b, dt, hooks );
        cmp( "unrecreated paths", fi, 0, static_cast< std::uint32_t >( missing ), 0u );
        for ( int k = 0; k < 2; ++k )
        {
            const std::uint32_t ours[ 4 ] = { ix( plain[ k ].current ), ix( plain[ k ].previous ), ix( flat[ k ].current ), ix( flat[ k ].previous ) };
            for ( int j = 0; j < 4; ++j )
                cmp( "slot", fi, 4 * k + j, ours[ j ], *s++ );
        }
        for ( int k = 0; k < 2; ++k )
            for ( int j = 0; j < 3; ++j )
                cmp( "solver", fi, 3 * k + j, solver[ k ][ j ], *s++ );
        b.wave_gone.resize( b.waves.size( ), 0 );
        std::vector< std::uint32_t > gone;
        for ( std::size_t w = 0; w < b.waves.size( ); ++w )
            if ( b.wave_gone[ w ] && !before[ w ] )
                gone.push_back( static_cast< std::uint32_t >( w ) );
        const unsigned nd = *s++;
        cmp( "dtor count", fi, 0, static_cast< std::uint32_t >( gone.size( ) ), nd );
        for ( unsigned k = 0; k < nd; ++k, ++s )
            cmp( "dtor", fi, static_cast< int >( k ), k < gone.size( ) ? gone[ k ] : 0xDEADBEEFu, *s );
        std::vector< int > live;
        for ( int e = b.hold.list; e >= 0; e = b.hold.pool[ e ].next )
            if ( b.hold.pool[ e ].leaf >= static_cast< int >( base ) )
                live.push_back( b.hold.pool[ e ].leaf );
        std::sort( live.begin( ), live.end( ) );
        const unsigned nl = *s++;
        cmp( "listed wakes", fi, 0, static_cast< std::uint32_t >( live.size( ) ), nl );
        for ( unsigned i = 0; i < nl; ++i, s += 1 + 48 )
        {
            const unsigned wave = s[ 0 ];
            cmp( "listed wake", fi, static_cast< int >( i ), i < live.size( ) ? ix( live[ i ] ) : 0xDEADBEEFu, wave );
            if ( wave >= b.waves.size( ) )
                continue;
            const water_words_t& w = b.waves[ wave ];
            for ( unsigned k = 0; k < 48; ++k )
                if ( w.known[ k ] && 4 * k != 24 && 4 * k != 40 && 4 * k != 0x40 )   // +0x40 the entry list's head pointer: compared as entries
                {
                    cmp( "wake word", fi, static_cast< int >( wave * 1000 + 4 * k ), w.word[ k ], s[ 1 + k ] );
                    ++words;
                }
        }
        const unsigned n_list = *s++;
        int e = b.hold.list;
        for ( unsigned k = 0; k < n_list; ++k, s += 2 )
        {
            cmp( "list pool", fi, static_cast< int >( k ), ix( e ), s[ 0 ] );
            if ( e < 0 )
                break;
            cmp( "list wave", fi, static_cast< int >( k ), ix( b.hold.pool[ e ].leaf ), s[ 1 ] );
            e = b.hold.pool[ e ].next;
        }
        cmp( "list end", fi, 0, ix( e ), 0xFFFFFFFFu );
        cmp( "pool count", fi, 0, static_cast< std::uint32_t >( b.hold.pool_count ), *s++ );
        cmp( "pool free", fi, 0, static_cast< std::uint32_t >( b.hold.pool_free ), *s++ );
        for ( unsigned i = 0; i < nl; ++i )
        {
            const unsigned n = *s++;
            int x = i < live.size( ) ? b.hold.waves[ static_cast< std::size_t >( live[ i ] ) ].entries : -1;
            for ( unsigned k = 0; k < n; ++k, s += 2 )
            {
                cmp( "entry pool", fi, static_cast< int >( i ), ix( x ), s[ 0 ] );
                if ( x < 0 )
                    continue;
                cmp( "entry surface", fi, static_cast< int >( i ), ix( b.hold.pool[ x ].surface ), s[ 1 ] );
                x = b.hold.pool[ x ].next;
            }
            cmp( "entries end", fi, static_cast< int >( i ), ix( x ), 0xFFFFFFFFu );
        }
    }
    cmp( "golden consumed", 0, 0, s == g_golden_wwk_words + g_golden_wwk_size ? 1u : 0u, 1u );
    int nodes = 0;
    for ( std::size_t i = 0; i < g_golden_wwk_trees_count && i < b.hold.surfaces.size( ); ++i )
    {
        const golden_water_tree_t& g = g_golden_wwk_trees[ i ];
        const water_tree_t& t = b.hold.surfaces[ i ].waves;
        const int id = static_cast< int >( i );
        cmp( "tree root", id, 0, ix( t.root ), ix( g.m_root ) );
        cmp( "tree free", id, 0, ix( t.free ), ix( g.m_free ) );
        cmp( "tree leaves", id, 0, ix( t.leaves ), ix( g.m_leaves ) );
        cmp( "tree used", id, 0, ix( t.used ), ix( g.m_used ) );
        cmp( "tree free count", id, 0, ix( t.free_count ), ix( g.m_free_count ) );
        cmp( "tree size", id, 0, static_cast< std::uint32_t >( t.nodes.size( ) ), ix( g.m_count ) );
        for ( int n = 0; n < g.m_count && n < static_cast< int >( t.nodes.size( ) ); ++n )
        {
            const golden_water_node_t& gn = g_golden_wwk_nodes[ g.m_first + n ];
            const water_tree_node_t& o = t.nodes[ static_cast< std::size_t >( n ) ];
            std::uint32_t r[ 4 ];
            std::memcpy( r, o.rect, 16 );
            for ( int k = 0; k < 4; ++k )
                cmp( "tree node rect", id, n, r[ k ], gn.m_rect[ k ] );
            cmp( "tree node parent", id, n, ix( o.parent ), ix( gn.m_parent ) );
            cmp( "tree node first", id, n, ix( o.first ), ix( gn.m_first ) );
            cmp( "tree node second", id, n, ix( o.second ), ix( gn.m_second ) );
            cmp( "tree node tag", id, n, o.tag, ix( gn.m_tag ) );
            ++nodes;
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "water_wake_update (0x417D80's water half: 0x5287D0 / 0x528850 / 0x52BEB0 / 0x52C720, the re-insert, the "
                     "segment roll) + the wakes' tick 0x52C210 match retail on Hydroelectric's built water: %u frames, %d wake waves "
                     "made, %d wake words, %d tree nodes, %d values, bit for bit",
            frames, made, words, nodes, compared );
    else
        LOG_ERROR( "water_wake FAILED %d of %d values", failed, compared );
    return failed == 0;
}
