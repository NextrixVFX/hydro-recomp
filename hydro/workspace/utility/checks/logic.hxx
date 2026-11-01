#pragma once

// The logic entities (world/logic.hxx) against retail (case logic_plugs): per vector the state / text / params / VuRand
// set as retail had them, the input plug dispatched through g_logic_plugs by (vtable, name), every output fire answered
// from the drawn list as retail's 0x5023A0 would ({0, 0} for a void plug after its link ran; the link's answer for a
// typed one). Compared: the VuRetVal words (a type-4 value by text), the fires (name, params size and bytes), the answers
// consumed, the state words / text after, the VuRand after. The "*ctor" rows: logic_construct's words against the ctor's.

#include <workspace/core/assets/reader.hxx>
#include <workspace/core/world/logic.hxx>

#include <string>
#include <vector>

struct logic_check_t
{
    const unsigned* s;

    unsigned next( ) { return *s++; }

    std::string str( )
    {
        const unsigned n = next( );
        std::string out( reinterpret_cast< const char* >( s ), n );
        s += ( n + 3 ) / 4;
        return out;
    }
};

struct logic_check_log_t
{
    std::vector< std::pair< std::uint32_t, std::string > > answers;   // type, value word or text
    std::vector< std::uint32_t > values;
    std::size_t used = 0;
    std::vector< std::string > fired;
    std::vector< ui_params_t > params;
    std::string texts[ 16 ];
};

inline bool verify_logic_plugs( )
{
    logic_check_t c{ g_golden_lgp_words };
    const unsigned n = c.next( );
    int bad = 0, plugs = 0, ctors = 0, fires = 0, reported = 0;
    for ( unsigned i = 0; i < n; ++i )
    {
        const std::string cls = c.str( );
        const std::string plug = c.str( );
        const std::uint32_t vt = logic_vtable_of( cls.c_str( ) );
        if ( plug == "*ctor" )
        {
            const unsigned vtable = c.next( ), size = c.next( ), nw = c.next( );
            const logic_entity_t e = logic_construct( vt );
            bad += vt != vtable;
            for ( unsigned k = 0; k < nw; ++k )
            {
                const unsigned want = c.next( );
                // the string classes: +0x60 .. +0x6F the inline bytes, +0x70 size, +0x74 capacity 15 (an empty VuString)
                const unsigned have = ( cls == "VuStringEntity" || cls == "VuSetStringEntity" ) ? ( k == 5 ? 15u : 0u ) : e.words[ k ];
                bad += have != want;
            }
            bad += ( cls == "VuStringEntity" || cls == "VuSetStringEntity" ) && !e.text.empty( );
            ( void )size;
            ++ctors;
            continue;
        }
        const int bad_before = bad;
        logic_entity_t e = logic_construct( vt );
        const unsigned ns = c.next( );
        for ( unsigned k = 0; k < ns; ++k )
            e.words[ k ] = c.next( );
        if ( c.next( ) )
            e.text = c.str( );
        ui_params_t in{ };
        in.m_size = static_cast< int >( c.next( ) );
        std::memcpy( in.m_bytes, c.s, 128 );
        c.s += 32;
        logic_check_log_t L;
        const unsigned na = c.next( );
        for ( unsigned k = 0; k < na; ++k )
        {
            const unsigned t = c.next( );
            if ( t == 4u )
                L.answers.push_back( { t, c.str( ) } ), L.values.push_back( 0u );
            else
                L.answers.push_back( { t, std::string( ) } ), L.values.push_back( c.next( ) );
        }
        pfx_rand_t rnd;
        rnd.seed = static_cast< std::int32_t >( c.next( ) );
        rnd.previous = static_cast< std::int32_t >( c.next( ) );
        for ( int k = 0; k < 32; ++k )
            rnd.table[ k ] = static_cast< std::int32_t >( c.next( ) );
        const unsigned rt = c.next( ), rv = c.next( );
        std::string rtext;
        const bool has_text = c.next( ) != 0u;
        if ( has_text )
            rtext = c.str( );
        const unsigned used = c.next( );
        logic_hooks_t h;
        h.user = &L;
        h.rand = &rnd;
        h.fire = []( void* u, logic_entity_t& ent, const char* name, const ui_params_t& p ) -> logic_retval_t {
            logic_check_log_t& l = *static_cast< logic_check_log_t* >( u );
            l.fired.push_back( name );
            l.params.push_back( p );
            logic_retval_t r{ };
            if ( l.used < l.answers.size( ) )
            {
                const std::size_t k = l.used++;
                const logic_output_t* o = logic_output( ent.vtable, name );
                if ( o && o->type != 0u )
                {
                    r.type = l.answers[ k ].first;
                    r.value = l.values[ k ];
                    if ( r.type == 4u )
                    {
                        l.texts[ k % 16 ] = l.answers[ k ].second;
                        r.text = l.texts[ k % 16 ].c_str( );
                    }
                }
            }
            return r;
        };
        logic_retval_t out{ 0xA5A5A5A5u, 0xA5A5A5A5u, nullptr };
        bool ok = true;
        if ( plug == "*init" )
            logic_switch_init( e );
        else
            ok = logic_dispatch( e, plug.c_str( ), out, in, h );
        bad += !ok;
        bad += out.type != rt;
        // The VuMethod1 invoke 0x4050E0 copies the handler's LOCAL VuRetVal out: a word (or the bool's upper three bytes)
        // the handler does not write is retail's stale stack - compared only where the C++ wrote it.
        if ( has_text )
            bad += !out.text || rtext != out.text;
        else if ( out.type == 3u )
            bad += ( out.value & 0xFFu ) != ( rv & 0xFFu );
        else if ( out.value != 0xA5A5A5A5u )
            bad += out.value != rv;
        bad += L.used != used;
        const unsigned nf = c.next( );
        bad += L.fired.size( ) != nf;
        for ( unsigned k = 0; k < nf; ++k )
        {
            const std::string name = c.str( );
            const unsigned size = c.next( );
            const std::string bytes = c.str( );
            if ( k < L.fired.size( ) )
            {
                const ui_params_t& p = L.params[ k ];
                bad += L.fired[ k ] != name;
                bad += static_cast< unsigned >( p.m_size ) != size;
                bad += bytes.size( ) > 128 || std::memcmp( bytes.data( ), p.m_bytes, bytes.size( ) ) != 0;
            }
            ++fires;
        }
        const unsigned na2 = c.next( );
        for ( unsigned k = 0; k < na2; ++k )
            bad += e.words[ k ] != c.next( );
        if ( c.next( ) )
            bad += e.text != c.str( );
        bad += static_cast< unsigned >( rnd.seed ) != c.next( );
        bad += static_cast< unsigned >( rnd.previous ) != c.next( );
        for ( int k = 0; k < 32; ++k )
            bad += static_cast< unsigned >( rnd.table[ k ] ) != c.next( );
        ++plugs;
        if ( bad != bad_before && reported++ < 12 )
            LOG_ERROR( "logic_plugs: %s %s vector %u: bad %d (ret %08X %08X want %08X %08X; fires %u want %u; used %u want %u)", cls.c_str( ),
                       plug.c_str( ), i, bad - bad_before, out.type, out.value, rt, rv, static_cast< unsigned >( L.fired.size( ) ), nf,
                       static_cast< unsigned >( L.used ), used );
    }
    if ( bad == 0 )
        LOG_SUCCESS( "world/logic.hxx matches the engine's logic entities' handlers (18 classes, 27 input plugs + the switch's slot 6) on "
                     "%d fired plugs (%d output fires) and %d ctor images", plugs, fires, ctors );
    else
        LOG_ERROR( "logic_plugs FAILED %d", bad );
    return bad == 0;
}

// g_logic_types (world/logic.hxx) against the classes' getType slots run by retail (case logic_types).
inline bool verify_logic_types( )
{
    int bad = 0;
    const std::size_t n = sizeof( g_logic_types ) / sizeof( g_logic_types[ 0 ] );
    for ( std::size_t i = 0; i < g_golden_lgt_count; ++i )
    {
        const golden_logic_type_t& g = g_golden_lgt[ i ];
        const logic_type_t* t = nullptr;
        for ( std::size_t j = 0; j < n; ++j )
            if ( g_logic_types[ j ].address == g.address )
                t = &g_logic_types[ j ];
        if ( t == nullptr || t->descriptor != g.descriptor || std::strcmp( t->chain, g.chain ) != 0 )
        {
            ++bad;
            LOG_ERROR( "logic_types 0x%X: descriptor / chain differ (retail 0x%X %s)", g.address, g.descriptor, g.chain );
        }
    }
    bad += n != g_golden_lgt_count ? 1 : 0;
    if ( bad == 0 )
        LOG_SUCCESS( "g_logic_types match the logic / script classes' %d getType slots (descriptor and chain each)", static_cast< int >( n ) );
    else
        LOG_ERROR( "logic_types FAILED %d", bad );
    return bad == 0;
}

// The game-tier filters (case logic_game): the environment as retail had it - the document parsed by c_reader (= 0x55B560)
// as gameutil +0x50, the SKU string, the repository's table (a missing hash answers the first entity, as the case's stub
// did) and the entity / driver words. Compared: the VuRetVal type (the value word is the handler's stale stack), the
// hashes the repository was asked for, the fires (name, params size and bytes), the text after; nothing unrecreated.
struct logic_game_check_t
{
    std::vector< std::uint32_t > hashes, entities, asked;
    std::vector< std::pair< std::uint32_t, std::uint32_t > > words;
    std::vector< std::string > fired;
    std::vector< ui_params_t > params;
};

inline bool verify_logic_game( )
{
    logic_check_t c{ g_golden_lgg_words };
    const unsigned n = c.next( );
    int bad = 0, fires = 0, reported = 0, unrecreated = 0;
    for ( unsigned i = 0; i < n; ++i )
    {
        const int bad_before = bad;
        const std::string cls = c.str( );
        const std::string plug = c.str( );
        logic_entity_t e = logic_construct( logic_vtable_of( cls.c_str( ) ) );
        if ( c.next( ) )
            e.text = c.str( );
        value_t doc{ };
        const bool has_doc = c.next( ) != 0u;
        if ( has_doc )
        {
            const std::string raw = c.str( );
            doc = c_reader( std::vector< std::uint8_t >( raw.begin( ), raw.end( ) ) ).parse( );
        }
        std::string sku;
        const bool has_sku = c.next( ) != 0u;
        if ( has_sku )
            sku = c.str( );
        logic_game_check_t L;
        const unsigned ne = c.next( );
        for ( unsigned k = 0; k < ne; ++k )
        {
            const std::uint32_t hash = c.next( ), entity = c.next( ), vtable = c.next( ), driver = c.next( ), dvt = c.next( );
            L.hashes.push_back( hash );
            L.entities.push_back( entity );
            L.words.push_back( { entity, vtable } );
            L.words.push_back( { entity + 0x220u, driver } );
            L.words.push_back( { driver, dvt } );
        }
        ui_params_t in{ };
        in.m_size = static_cast< int >( c.next( ) );
        std::memcpy( in.m_bytes, c.s, 128 );
        c.s += 32;
        const unsigned rt = c.next( );
        c.next( );
        logic_game_t g;
        g.game_data = has_doc ? &doc : nullptr;
        g.sku = has_sku ? &sku : nullptr;
        g.user = &L;
        g.unrecreated = &unrecreated;
        g.find = []( void* u, std::uint32_t hash ) -> std::uint32_t {
            logic_game_check_t& l = *static_cast< logic_game_check_t* >( u );
            l.asked.push_back( hash );
            for ( std::size_t k = 0; k < l.hashes.size( ); ++k )
                if ( l.hashes[ k ] == hash )
                    return l.entities[ k ];
            return l.entities.empty( ) ? 0u : l.entities[ 0 ];
        };
        g.word = []( void* u, std::uint32_t address ) -> std::uint32_t {
            for ( const auto& w : static_cast< logic_game_check_t* >( u )->words )
                if ( w.first == address )
                    return w.second;
            return 0u;
        };
        logic_hooks_t h;
        h.user = &L;
        h.game = &g;
        h.fire = []( void* u, logic_entity_t&, const char* name, const ui_params_t& p ) -> logic_retval_t {
            logic_game_check_t& l = *static_cast< logic_game_check_t* >( u );
            l.fired.push_back( name );
            l.params.push_back( p );
            return logic_retval_t{ };
        };
        logic_retval_t out{ 0xA5A5A5A5u, 0xA5A5A5A5u, nullptr };
        bad += !logic_dispatch( e, plug.c_str( ), out, in, h );
        bad += out.type != rt;
        const unsigned na = c.next( );
        bad += L.asked.size( ) != na;
        for ( unsigned k = 0; k < na; ++k )
        {
            const unsigned want = c.next( );
            bad += k < L.asked.size( ) && L.asked[ k ] != want;
        }
        const unsigned nf = c.next( );
        bad += L.fired.size( ) != nf;
        for ( unsigned k = 0; k < nf; ++k )
        {
            const std::string name = c.str( );
            const unsigned size = c.next( );
            const std::string bytes = c.str( );
            if ( k < L.fired.size( ) )
            {
                bad += L.fired[ k ] != name;
                bad += static_cast< unsigned >( L.params[ k ].m_size ) != size;
                bad += bytes.size( ) > 128 || std::memcmp( bytes.data( ), L.params[ k ].m_bytes, bytes.size( ) ) != 0;
            }
            ++fires;
        }
        if ( c.next( ) )
            bad += e.text != c.str( );
        if ( bad != bad_before && reported++ < 12 )
            LOG_ERROR( "logic_game: %s vector %u: bad %d (fires %u want %u)", cls.c_str( ), i, bad - bad_before,
                       static_cast< unsigned >( L.fired.size( ) ), nf );
    }
    // the text classes' ctor images (VuIsEventTypeEntity's "AttractEvent", VuIsSkuEntity's empty SKU)
    const unsigned nc = c.next( );
    for ( unsigned k = 0; k < nc; ++k )
    {
        const std::string cls = c.str( );
        const std::string want = c.next( ) ? c.str( ) : std::string( );
        bad += logic_construct( logic_vtable_of( cls.c_str( ) ) ).text != want;
    }
    bad += unrecreated;
    if ( bad == 0 )
        LOG_SUCCESS( "world/logic.hxx matches VuIsTegraEntity 0x42DAA0 / VuIsSkuEntity 0x42CFF0 / VuHumanFilterEntity 0x42D370 / "
                     "VuIsEventTypeEntity 0x42CB00 on %u fired "
                     "plugs (%d output fires; documents, SKUs, repository tables and driver kinds as retail had them)", n, fires );
    else
        LOG_ERROR( "logic_game FAILED %d (%d unrecreated paths reached)", bad, unrecreated );
    return bad == 0;
}

// VuControlEntity (world/keyframe.hxx) against retail (case control_entity): per scenario the children, the component's
// records, the ref / component / templated inputs, the six bools and the real-time step as retail had them, the
// documents parsed by c_reader (= 0x55B560); each step replayed on control_entity_t with hooks logging what the case's
// stubs logged. Compared after every step: the state words (the entity, every keyframe's time / VuArray / property
// list by name hash, the component's +0x14 and records) and the log (fires, interface slots, tick registrations);
// nothing may count unrecreated.
#include <workspace/core/world/keyframe.hxx>

struct control_check_t
{
    std::vector< std::vector< std::uint32_t > > log;
};

inline std::uint32_t control_check_fnv( const std::string& s )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( const unsigned char c : s )
        h = ( h ^ c ) * 0x01000193u;
    return h;
}

inline std::vector< std::uint32_t > control_check_state( const control_entity_t& e, const control_component_t* comp )
{
    std::vector< std::uint32_t > w = { e.initially_active, e.one_shot, e.network_sync, e.loop, e.ease, e.real_time, e.active, e.done };
    w.push_back( e.target == nullptr ? 0u : ( e.target == comp ? 1u : 2u ) );
    w.push_back( static_cast< std::uint32_t >( e.keys.size( ) ) );
    w.push_back( static_cast< std::uint32_t >( e.capacity ) );
    for ( const auto& k : e.keys )
    {
        w.push_back( k.first );
        w.push_back( static_cast< std::uint32_t >( k.second ) );
    }
    w.push_back( e.duration );
    w.push_back( e.time );
    for ( const control_child_t& ch : e.children )
    {
        if ( !ch.keyframe )
            continue;
        w.push_back( ch.frame.time );
        w.push_back( static_cast< std::uint32_t >( ch.frame.values.size( ) ) );
        w.push_back( static_cast< std::uint32_t >( ch.frame.capacity ) );
        w.insert( w.end( ), ch.frame.values.begin( ), ch.frame.values.end( ) );
        w.push_back( static_cast< std::uint32_t >( ch.frame.properties.size( ) ) );
        for ( const control_property_t& p : ch.frame.properties )
        {
            w.push_back( control_check_fnv( p.name ) );
            w.push_back( p.fallback );
            w.push_back( p.current );
        }
    }
    if ( comp )
    {
        w.push_back( comp->controller == nullptr ? 0u : ( comp->controller == &e ? 1u : 2u ) );
        for ( int i = 0; i < comp->count( ); ++i )
        {
            w.push_back( comp->records[ 3 * i + 1 ] );
            w.push_back( comp->records[ 3 * i + 2 ] );
        }
    }
    return w;
}

inline bool verify_control_entity( )
{
    logic_check_t c{ g_golden_cte_words };
    const unsigned n = c.next( );
    int bad = 0, steps = 0, reported = 0, unrecreated = 0;
    for ( unsigned v = 0; v < n; ++v )
    {
        control_entity_t e;
        const unsigned nk = c.next( );
        for ( unsigned k = 0; k < nk; ++k )
        {
            control_child_t ch;
            ch.keyframe = c.next( ) != 0;
            ch.name = c.str( );
            e.children.push_back( ch );
        }
        control_component_t comp;
        const unsigned nr = c.next( );
        for ( unsigned r = 0; r < nr; ++r )
        {
            comp.names.push_back( c.str( ) );
            comp.records.push_back( 0u );
            comp.records.push_back( c.next( ) );
            comp.records.push_back( c.next( ) );
        }
        const bool linked = c.next( ) != 0, has = c.next( ) != 0, templated = c.next( ) != 0;
        std::uint8_t bools[ 6 ];
        for ( auto& b : bools )
            b = static_cast< std::uint8_t >( c.next( ) );
        const std::uint32_t rt = c.next( );
        const std::string doc_bytes = c.str( );
        value_t doc = c_reader( std::vector< std::uint8_t >( doc_bytes.begin( ), doc_bytes.end( ) ) ).parse( );
        value_t tdoc;
        if ( c.next( ) )
        {
            const std::string t = c.str( );
            tdoc = c_reader( std::vector< std::uint8_t >( t.begin( ), t.end( ) ) ).parse( );
        }
        control_check_t log;
        control_component_t* cp = has ? &comp : nullptr;
        comp.user = &log;
        comp.changed = []( void* u, control_component_t& ) { static_cast< control_check_t* >( u )->log.push_back( { 2u, 0u } ); };
        comp.begin = []( void* u, control_component_t& ) { static_cast< control_check_t* >( u )->log.push_back( { 2u, 1u } ); };
        comp.end = []( void* u, control_component_t& ) { static_cast< control_check_t* >( u )->log.push_back( { 2u, 2u } ); };
        struct env_t { control_check_t* log; float rt; } env{ &log, control_f( rt ) };
        control_hooks_t h;
        h.user = &env;
        h.unrecreated = &unrecreated;
        h.fire = []( void* u, control_entity_t&, const char* plug ) { static_cast< env_t* >( u )->log->log.push_back( { 1u, control_check_fnv( plug ) } ); };
        h.tick_register = []( void* u, control_entity_t&, bool on )
        {
            if ( on )
                static_cast< env_t* >( u )->log->log.push_back( { 3u, control_check_fnv( "Motion" ), 0x7529F8u, 1u, 0x5CD9F0u } );
            else
                static_cast< env_t* >( u )->log->log.push_back( { 4u, 1u } );
        };
        h.real_dt = []( void* u ) { return static_cast< env_t* >( u )->rt; };
        const control_component_t* bound = linked && has ? &comp : nullptr;
        const unsigned ns = c.next( );
        for ( unsigned s = 0; s < ns; ++s )
        {
            const unsigned op = c.next( );
            const unsigned a0 = c.next( ), a1 = c.next( ), a2 = c.next( );
            std::vector< std::vector< std::uint32_t > > want_log( c.next( ) );
            for ( auto& l : want_log )
            {
                l.resize( c.next( ) );
                for ( auto& x : l )
                    x = c.next( );
            }
            std::vector< std::uint32_t > want( c.next( ) );
            for ( auto& x : want )
                x = c.next( );
            log.log.clear( );
            int expect_ret = -1;
            switch ( op )
            {
                case 0:
                    e.initially_active = bools[ 0 ]; e.one_shot = bools[ 1 ]; e.network_sync = bools[ 2 ];
                    e.loop = bools[ 3 ]; e.ease = bools[ 4 ]; e.real_time = bools[ 5 ];
                    break;
                case 1: control_load( e, doc ); break;
                case 2: control_load_template( e, tdoc ); break;
                case 3: control_post_load( e, bound, templated ); break;
                case 4: control_game_init( e, linked, linked ? cp : nullptr, h ); break;
                case 5: control_game_release( e, h ); break;
                case 6: control_reset( e ); break;
                case 7: control_rebind( e, bound ); break;
                case 8: control_child_added( e, static_cast< int >( a0 ), bound ); break;
                case 9: control_activate( e, h ); expect_ret = static_cast< int >( a0 ); break;
                case 10: control_deactivate( e, h ); expect_ret = static_cast< int >( a0 ); break;
                case 11: control_tick( e, control_f( a0 ), h ); break;
                case 12:
                {
                    std::uint8_t p[ 8 ];
                    std::memcpy( p, &a0, 4 );
                    std::memcpy( p + 4, &a1, 4 );
                    control_manual_tick( e, p, static_cast< std::int32_t >( a2 ), h );
                    break;
                }
                default: ++bad; break;
            }
            ++steps;
            const std::vector< std::uint32_t > got = control_check_state( e, has ? &comp : nullptr );
            int diff = expect_ret > 0 ? 1 : 0;   // the VuRetVal type word retail wrote (0: void)
            for ( std::size_t i = 0; i < ( std::max )( got.size( ), want.size( ) ); ++i )
                diff += i >= got.size( ) || i >= want.size( ) || got[ i ] != want[ i ] ? 1 : 0;
            diff += log.log == want_log ? 0 : 1;
            if ( diff && reported++ < 6 )
                LOG_ERROR( "control_entity scenario %u step %u (op %u): %d words differ (state %d / %d words, log %d / %d)", v, s, op, diff,
                           static_cast< int >( got.size( ) ), static_cast< int >( want.size( ) ), static_cast< int >( log.log.size( ) ),
                           static_cast< int >( want_log.size( ) ) );
            bad += diff;
        }
    }
    bad += unrecreated;
    if ( bad == 0 )
        LOG_SUCCESS( "VuControlEntity matches retail: %u scenarios, %d steps (slots 2 / 3 / 4 / 6 / 7 / 8 / 9, 0x5CD990, Activate, Deactivate, OnManualTick, the tick; every state word and log entry)", n, steps );
    else
        LOG_ERROR( "control_entity FAILED %d (unrecreated %d)", bad, unrecreated );
    return bad == 0;
}