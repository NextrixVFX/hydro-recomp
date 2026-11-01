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
    // VuControlComponent's name getter 0x619F50
    bad += g_golden_lgt_name_count == 1 && g_golden_lgt_names[ 0 ].address == 0x619F50u
        && std::strcmp( g_golden_lgt_names[ 0 ].chain, control_component_name( ) ) == 0 ? 0 : 1;
    if ( bad == 0 )
        LOG_SUCCESS( "g_logic_types match the logic / script classes' %d getType slots (descriptor and chain each); VuControlComponent's name 0x619F50",
                     static_cast< int >( n ) );
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
// VuLensWaterEmitterEntity (world/lens.hxx) against retail (case lens_water): the emitter slot 0's answer bit for bit on
// drawn positions / points / radii / rates; slots 6 / 7 on the entity and a null this (the manager calls and their
// argument); the ctor's interface vtable 0x7555B0 and the two defaults (schema.hxx: 25 / 64).
#include <workspace/core/world/lens.hxx>

inline bool verify_lens_water( )
{
    logic_check_t c{ g_golden_lwe_words };
    const unsigned n = c.next( );
    int bad = 0;
    for ( unsigned v = 0; v < n; ++v )
    {
        float pos[ 3 ], pt[ 3 ];
        for ( float& x : pos )
            x = control_f( c.next( ) );
        for ( float& x : pt )
            x = control_f( c.next( ) );
        const float rad = control_f( c.next( ) ), drops = control_f( c.next( ) );
        const std::uint32_t want = c.next( );
        const std::uint32_t got = control_w( lens_water_emitter_rate( pos, rad, drops, pt ) );
        if ( got != want )
        {
            if ( bad < 6 )
                LOG_ERROR( "lens_water vector %u: 0x%08X, retail 0x%08X", v, got, want );
            ++bad;
        }
    }
    const unsigned nc = c.next( );
    for ( unsigned k = 0; k < nc; ++k )
    {
        std::vector< std::uint32_t > want( c.next( ) );
        for ( auto& x : want )
            x = c.next( );
        std::vector< std::uint32_t > got = { want[ 0 ], want[ 1 ] };
        static const int emitter_tag = 0;
        const void* emitter = want[ 1 ] ? &emitter_tag : nullptr;
        struct log_t { std::vector< std::uint32_t >* out; const void* e; } lg{ &got, &emitter_tag };
        lens_water_hooks_t h;
        h.user = &lg;
        h.add = []( void* u, const void* e ) { auto* l = static_cast< log_t* >( u ); l->out->push_back( 0u ); l->out->push_back( e == l->e ? 1u : ( e ? 2u : 0u ) ); };
        h.remove = []( void* u, const void* e ) { auto* l = static_cast< log_t* >( u ); l->out->push_back( 1u ); l->out->push_back( e == l->e ? 1u : ( e ? 2u : 0u ) ); };
        if ( want[ 0 ] == 0x5E6720u )
            lens_water_emitter_start( emitter, h );
        else
            lens_water_emitter_stop( emitter, h );
        bad += got == want ? 0 : 1;
    }
    const std::uint32_t vt = c.next( ), radius = c.next( ), drops = c.next( );
    bad += vt == 0x7555B0u && radius == 0x41C80000u && drops == 0x42800000u ? 0 : 1;
    if ( bad == 0 )
        LOG_SUCCESS( "VuLensWaterEmitterEntity matches retail: %u emitter answers (0x5E6780), slots 6 / 7 with the manager, the ctor's defaults", n );
    else
        LOG_ERROR( "lens_water FAILED %d", bad );
    return bad == 0;
}

// VuAudioEmitterEntity + 0x50FBB0 + VuAnimatedPropEntity's PlayAudioEvent (world/sound.hxx) against retail (case
// audio_emitter): every step replayed with hooks that log what the case's FMOD recorder logged (getEvent answering FMOD_OK
// and the next handle 0xE001.. unless the name holds "bad": 74 and 0xBAD0 + the low byte), compared entry for entry; the
// VuRetVal / callback answer; the state bytes +0x84 / +0x88 / +0x89 and the event +0x8C after each step; the ctor's
// defaults; the prop vectors' FMOD logs on documents parsed by c_reader (= 0x55B560).
#include <workspace/core/world/sound.hxx>

struct sound_check_log_t
{
    std::uint32_t handle = 0xE000u;
    const void* holder = nullptr;
    std::vector< std::vector< std::uint32_t > > log;

    std::uint32_t holder_code( const void* p ) const { return p == nullptr ? 0u : ( p == holder ? 1u : 2u ); }

    static void str( std::vector< std::uint32_t >& w, const std::string& s )
    {
        w.push_back( static_cast< std::uint32_t >( s.size( ) ) );
        for ( std::size_t i = 0; i < s.size( ); i += 4 )
        {
            std::uint32_t x = 0;
            std::memcpy( &x, s.data( ) + i, ( std::min )( std::size_t( 4 ), s.size( ) - i ) );
            w.push_back( x );
        }
    }

    sound_hooks_t hooks( )
    {
        sound_hooks_t h;
        h.user = this;
        h.get_event = []( void* u, const char* name, std::uint32_t mode, std::uint32_t* ev, const void* holder ) -> int
        {
            auto* l = static_cast< sound_check_log_t* >( u );
            const std::string nm( name );
            const bool ok = nm.find( "bad" ) == std::string::npos;
            ++l->handle;
            *ev = ok ? l->handle : 0xBAD0u + ( l->handle & 0xFFu );
            std::vector< std::uint32_t > w{ 0u, 1u };
            str( w, nm );
            // getEvent's out pointer is the holder's +4, or (the prop) a stack local
            w.push_back( mode );
            w.push_back( holder != nullptr && holder == l->holder ? 1u : 2u );
            w.push_back( *ev );
            l->log.push_back( w );
            return ok ? 0 : 74;
        };
        h.set_callback = []( void* u, std::uint32_t ev, std::uint32_t fn, const void* holder )
        {
            auto* l = static_cast< sound_check_log_t* >( u );
            l->log.push_back( { 1u, ev, fn, l->holder_code( holder ) } );
        };
        h.stop = []( void* u, std::uint32_t ev, bool immediate ) { static_cast< sound_check_log_t* >( u )->log.push_back( { 2u, ev, immediate ? 1u : 0u } ); };
        h.start = []( void* u, std::uint32_t ev ) { static_cast< sound_check_log_t* >( u )->log.push_back( { 3u, ev } ); };
        h.attributes = []( void* u, std::uint32_t ev, const float* p, const float* v, const float* o )
        {
            std::vector< std::uint32_t > w{ 4u, ev };
            for ( const float* x : { p, v, o } )
            {
                w.push_back( x ? 3u : 0u );
                for ( int k = 0; x && k < 3; ++k )
                    w.push_back( control_w( x[ k ] ) );
            }
            static_cast< sound_check_log_t* >( u )->log.push_back( w );
        };
        h.set_world = []( void* u, const float*, bool notify ) { static_cast< sound_check_log_t* >( u )->log.push_back( { 5u, 1u, 1u, notify ? 1u : 0u } ); };
        return h;
    }
};

inline bool verify_audio_emitter( )
{
    logic_check_t c{ g_golden_aem_words };
    sound_check_log_t lg;
    int bad = 0, steps = 0, entries = 0, reported = 0;
    auto read_log = [ & ]( ) {
        std::vector< std::vector< std::uint32_t > > want( c.next( ) );
        for ( auto& e : want )
        {
            e.resize( c.next( ) );
            for ( auto& x : e )
                x = c.next( );
        }
        return want;
    };
    const unsigned n = c.next( );
    for ( unsigned v = 0; v < n; ++v )
    {
        audio_emitter_t e;
        lg.holder = e.holder.image;
        // the ctor's defaults: +0x5C the IF vtable 0x756EA0, the empty VuString (+0x80 cap 15, +0x7C size 0, +0x6C 0),
        // Initially Active 0, the holder word 1, no event
        const std::uint32_t defaults[ 7 ] = { 0x756EA0u, 15u, static_cast< std::uint32_t >( e.event_name.size( ) ), 0u, e.initially_active,
                                              static_cast< std::uint32_t >( e.holder.image[ 0 ] | e.holder.image[ 1 ] << 8 ), e.holder.event( ) };
        e.event_name = c.str( );
        e.initially_active = static_cast< std::uint8_t >( c.next( ) );
        float pos[ 3 ];
        for ( float& x : pos )
            x = control_f( c.next( ) );
        for ( std::uint32_t d : defaults )
            bad += c.next( ) == d ? 0 : 1;
        const unsigned ns = c.next( );
        for ( unsigned s = 0; s < ns; ++s )
        {
            const unsigned op = c.next( );
            std::vector< std::uint32_t > arg( c.next( ) );
            for ( auto& x : arg )
                x = c.next( );
            const std::string text = c.str( );
            const std::uint32_t want_ret = c.next( );
            const auto want = read_log( );
            std::vector< std::uint32_t > want_state( c.next( ) );
            for ( auto& x : want_state )
                x = c.next( );
            lg.log.clear( );
            const sound_hooks_t h = lg.hooks( );
            std::uint32_t ret = 0;
            float m[ 20 ];
            switch ( op )
            {
                case 0: audio_emitter_game_init( e, pos, h ); break;
                case 1: audio_emitter_start( e, pos, h ); break;
                case 2: audio_emitter_stop( e, h ); break;
                case 3: audio_emitter_name_changed( e, pos, h ); break;
                case 4:
                    for ( int k = 0; k < 20; ++k )
                        m[ k ] = control_f( arg[ k ] );
                    audio_emitter_motion( e, m, h );
                    break;
                case 5: case 6: case 7: ret = sound_event_callback( e.holder, op == 5 ? 4u : ( op == 6 ? 2u : 0u ), h ); break;
                case 8: e.event_name = text; break;
                case 9: e.holder.image[ 1 ] = static_cast< std::uint8_t >( arg[ 0 ] ); break;
                case 10:
                    audio_emitter_destroy( e, h );
                    lg.log.push_back( { 6u, 1u } );   // the base dtor 0x500870 follows
                    break;
                default: ++bad; break;
            }
            std::vector< std::uint32_t > state{ e.initially_active, e.holder.image[ 0 ], e.holder.image[ 1 ], e.holder.event( ) };
            if ( op == 10 )
                state.insert( state.end( ), { static_cast< std::uint32_t >( e.event_name.size( ) ), 15u,
                                              e.event_name.empty( ) ? 0u : static_cast< std::uint8_t >( e.event_name[ 0 ] ) } );
            const int diff = ( ret == want_ret ? 0 : 1 ) + ( lg.log == want ? 0 : 1 ) + ( state == want_state ? 0 : 1 );
            if ( diff && reported++ < 6 )
                LOG_ERROR( "audio_emitter scenario %u step %u (op %u): ret %d log %d (%d / %d entries) state %d", v, s, op, ret != want_ret,
                           lg.log != want, static_cast< int >( lg.log.size( ) ), static_cast< int >( want.size( ) ), state != want_state );
            bad += diff;
            entries += static_cast< int >( want.size( ) );
            ++steps;
        }
    }
    const unsigned np = c.next( );
    lg.holder = nullptr;
    for ( unsigned v = 0; v < np; ++v )
    {
        const std::string name = c.str( );
        const std::string raw = c.str( );
        float m[ 16 ], s[ 3 ];
        for ( float& x : m )
            x = control_f( c.next( ) );
        for ( float& x : s )
            x = control_f( c.next( ) );
        const auto want = read_log( );
        const value_t doc = c_reader( std::vector< std::uint8_t >( raw.begin( ), raw.end( ) ) ).parse( );
        lg.log.clear( );
        if ( name == "PlayAudioEvent" )   // 0x5ED680's exact VuString compare picks the branch
            prop_play_audio_event( doc, m, s, lg.hooks( ) );
        if ( lg.log != want )
        {
            if ( reported++ < 6 )
                LOG_ERROR( "audio_emitter prop vector %u (%s): %d / %d entries", v, name.c_str( ), static_cast< int >( lg.log.size( ) ),
                           static_cast< int >( want.size( ) ) );
            ++bad;
        }
        entries += static_cast< int >( want.size( ) );
    }
    if ( bad == 0 )
        LOG_SUCCESS( "VuAudioEmitterEntity (slot 6, Start, Stop, the name hook, the motion IF, the dtor), FMOD's callback 0x50FBB0 and "
                     "VuAnimatedPropEntity's PlayAudioEvent match retail: %u scenarios / %d steps, %u prop events, %d FMOD / transform calls",
                     n, steps, np, entries );
    else
        LOG_ERROR( "audio_emitter FAILED %d", bad );
    return bad == 0;
}

// VuKeyframeMotionEntity (world/keymotion.hxx) against retail (case key_motion): per scenario the children / flags / the
// target as retail had them, each step replayed with hooks logging what the case logged (plug fires by FNV-1a, the
// target IF's slots, the "Motion" registration {0x757F58, this, 0x61A6E0}, findMotionComponent); compared: the log, the
// VuRetVal word, every state word (flags, +0x68, counts, duration, clock, the keys, the claim, the motion component's
// matrix / linear / angular xyz). Scenarios that reach an unrecreated path fail.
#include <workspace/core/world/keymotion.hxx>

inline std::uint32_t key_motion_fnv( const char* s )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( ; *s; ++s )
        h = ( h ^ static_cast< std::uint8_t >( *s ) ) * 0x01000193u;
    return h;
}

// One replay of the key_motion layout (key_curve: + the Curve Type after the flags and the +0x90 curve after each state).
inline int key_motion_replay( const unsigned* words, bool curves, const char* tag, unsigned& scenarios, int& steps )
{
    logic_check_t c{ words };
    int bad = 0, reported = 0, unrecreated = 0;
    {
        const key_motion_t d;
        const std::uint32_t want[ 11 ] = { d.initially_active, d.one_shot, d.active, d.latch, d.target ? 1u : 0u, static_cast< std::uint32_t >( d.curve ),
                                           static_cast< std::uint32_t >( d.loop | d.ease << 8 | d.affect_position << 16 | d.affect_rotation << 24 ),
                                           static_cast< std::uint32_t >( d.keys.size( ) ), static_cast< std::uint32_t >( d.capacity ), d.duration, d.clock };
        for ( std::uint32_t x : want )
            bad += c.next( ) == x ? 0 : 1;
    }
    struct env_t
    {
        std::vector< std::vector< std::uint32_t > > log;
        key_motion_target_t target;
        float mc[ 22 ] = { };
        bool has = false;
    };
    const unsigned n = c.next( );
    for ( unsigned v = 0; v < n; ++v )
    {
        std::vector< key_motion_child_t > kids( c.next( ) );
        for ( auto& k : kids )
        {
            k.keyframe = c.next( ) != 0;
            k.time = c.next( );
            for ( float& x : k.transform )
                x = control_f( c.next( ) );
        }
        const bool linked = c.next( ) != 0, has = c.next( ) != 0, tnull = c.next( ) != 0;
        float tw[ 16 ];
        for ( float& x : tw )
            x = control_f( c.next( ) );
        std::uint32_t fl[ 6 ];
        for ( auto& x : fl )
            x = c.next( );
        key_motion_t e;
        e.initially_active = static_cast< std::uint8_t >( fl[ 0 ] );
        e.one_shot = static_cast< std::uint8_t >( fl[ 1 ] );
        e.loop = static_cast< std::uint8_t >( fl[ 2 ] );
        e.ease = static_cast< std::uint8_t >( fl[ 3 ] );
        e.affect_position = static_cast< std::uint8_t >( fl[ 4 ] );
        e.affect_rotation = static_cast< std::uint8_t >( fl[ 5 ] );
        if ( curves )
            e.curve = static_cast< std::int32_t >( c.next( ) );
        env_t env;
        env.has = has;
        env.target.transform = tnull ? nullptr : tw;
        env.target.user = &env;
        env.target.begin = []( void* u ) { static_cast< env_t* >( u )->log.push_back( { 2u, 1u, 1u } ); };
        env.target.end = []( void* u ) { static_cast< env_t* >( u )->log.push_back( { 2u, 2u, 1u } ); };
        env.target.update = []( void* u, const float* m, const float* lin, const float* ang )
        {
            auto* x = static_cast< env_t* >( u );
            std::memcpy( x->mc, m, 64 );
            std::memcpy( x->mc + 16, lin, 12 );
            std::memcpy( x->mc + 19, ang, 12 );
            x->log.push_back( { 2u, 0u, 1u } );
        };
        key_motion_hooks_t h;
        h.user = &env;
        h.unrecreated = &unrecreated;
        h.fire = []( void* u, key_motion_t&, const char* plug ) { static_cast< env_t* >( u )->log.push_back( { 1u, key_motion_fnv( plug ) } ); };
        h.tick_register = []( void* u, key_motion_t&, bool on )
        {
            auto* x = static_cast< env_t* >( u );
            if ( on )
                x->log.push_back( { 3u, key_motion_fnv( "Motion" ), 0x757F58u, 1u, 0x61A6E0u } );
            else
                x->log.push_back( { 4u, 1u } );
        };
        h.find = []( void* u, key_motion_t& ) -> key_motion_target_t*
        {
            auto* x = static_cast< env_t* >( u );
            x->log.push_back( { 5u, 1u } );
            return x->has ? &x->target : nullptr;
        };
        static const int foreign = 0;
        const unsigned ns = c.next( );
        for ( unsigned s = 0; s < ns; ++s )
        {
            const unsigned op = c.next( );
            const std::uint32_t arg = c.next( );
            std::vector< std::vector< std::uint32_t > > want( c.next( ) );
            for ( auto& w : want )
            {
                w.resize( c.next( ) );
                for ( auto& x : w )
                    x = c.next( );
            }
            std::vector< std::uint32_t > want_state( c.next( ) );
            for ( auto& x : want_state )
                x = c.next( );
            env.log.clear( );
            const int before = unrecreated;
            std::uint32_t ret = arg;
            switch ( op )
            {
                case 0: key_motion_game_init( e, kids, linked, h ); break;
                case 1: key_motion_activate( e, h ); ret = 0; break;
                case 2: key_motion_deactivate( e, h ); ret = 0; break;
                case 3: key_motion_tick( e, control_f( arg ), h ); break;
                case 4: key_motion_reset( e ); break;
                case 5: env.target.claim = arg ? &foreign : nullptr; break;
                case 6: key_motion_game_release( e, h ); break;
                default: ++bad; break;
            }
            std::vector< std::uint32_t > st{ e.initially_active, e.one_shot, e.active, e.latch, e.target ? 1u : 0u, static_cast< std::uint32_t >( e.curve ),
                                             static_cast< std::uint32_t >( e.loop | e.ease << 8 | e.affect_position << 16 | e.affect_rotation << 24 ),
                                             static_cast< std::uint32_t >( e.keys.size( ) ), static_cast< std::uint32_t >( e.capacity ), e.duration, e.clock };
            // slot 7 zeroes the count only: retail's array words stay, the snapshot reads none (count 0)
            for ( const auto& k : e.keys )
                st.insert( st.end( ), k.begin( ), k.end( ) );
            st.push_back( env.target.claim == nullptr ? 0u : ( env.target.claim == &e ? 1u : 2u ) );
            for ( float x : env.mc )
                st.push_back( control_w( x ) );
            if ( curves )
            {
                st.insert( st.end( ), { static_cast< std::uint32_t >( e.cubic.size( ) ), static_cast< std::uint32_t >( e.cubic_capacity ),
                                        e.cubic_valid } );
                for ( const auto& r : e.cubic )
                    for ( int j : { 0, 1, 2, 3, 4, 5, 6, 8 } )
                        st.push_back( r[ static_cast< std::size_t >( j ) ] );
                st.insert( st.end( ), { static_cast< std::uint32_t >( e.euler.size( ) ), static_cast< std::uint32_t >( e.euler_capacity ),
                                        e.euler_valid } );
                for ( const auto& r : e.euler )
                    for ( int j : { 0, 1, 2, 4, 5, 6, 8 } )
                        st.push_back( r[ static_cast< std::size_t >( j ) ] );
            }
            const int diff = ( ( op == 1 || op == 2 ) && ret != arg ? 1 : 0 ) + ( env.log == want ? 0 : 1 ) + ( st == want_state ? 0 : 1 )
                             + ( unrecreated != before ? 1 : 0 );
            if ( diff && reported++ < 6 )
                LOG_ERROR( "%s scenario %u step %u (op %u): log %d (%d / %d) state %d (%d / %d words) unrecreated %d", tag, v, s, op,
                           env.log != want, static_cast< int >( env.log.size( ) ), static_cast< int >( want.size( ) ), st != want_state,
                           static_cast< int >( st.size( ) ), static_cast< int >( want_state.size( ) ), unrecreated - before );
            bad += diff;
            ++steps;
        }
    }
    scenarios = n;
    if ( bad )
        LOG_ERROR( "%s FAILED %d (unrecreated %d)", tag, bad, unrecreated );
    return bad;
}

inline bool verify_key_motion( )
{
    unsigned n = 0;
    int steps = 0;
    if ( key_motion_replay( g_golden_kfm_words, false, "key_motion", n, steps ) )
        return false;
    LOG_SUCCESS( "VuKeyframeMotionEntity matches retail: %u scenarios, %d steps (slots 6 / 7 / 8, Activate / Deactivate, the Motion tick on "
                 "Curve Type 0: the keys, the clock, the target's matrix and velocities)", n, steps );
    return true;
}

// Curve Types 1 / 2 (case key_curve): slot 6's position curve (0x5CEC60 -> 0x61A7A0 -> 0x61AE50: every record word but +0x1C,
// the count, capacity and valid byte), type 2's euler curve (0x5CEF20: every record word but +0xC / +0x1C) and the update's
// cubic branches of 0x5D0440, through the same replay.
inline bool verify_key_curve( )
{
    unsigned n = 0;
    int steps = 0;
    if ( key_motion_replay( g_golden_kcv_words, true, "key_curve", n, steps ) )
        return false;
    LOG_SUCCESS( "VuKeyframeMotionEntity Curve Types 1 / 2 (Cubic, Cubic Euler) match retail: %u scenarios, %d steps (the +0x90 / +0xA0 "
                 "splines - 0x5CEC60 / 0x5CEF20 / 0x61A7A0 / 0x61AE50 - and 0x5D0440's cubic branches: the target's matrix and velocities)",
                 n, steps );
    return true;
}

// The settings entities' Trigger handlers (world/settings.hxx) against retail (case settings_triggers): g_vulightmanager's
// +0xCC .. +0xF8 after each call (prefilled 0xA5A5A5A5; +0xEC, a stack word retail never wrote, excepted), the VuRetVal
// word, the substitute calls; the ctor defaults against engine entity/rig.hxx's constructs.
#include <workspace/core/entity/rig.hxx>
#include <workspace/core/world/settings.hxx>

inline bool verify_settings_triggers( )
{
    logic_check_t c{ g_golden_stt_words };
    int bad = 0;
    {
        ambient_light_t a;
        directional_light_t d;
        ambient_light_construct( a );
        directional_light_construct( d );
        const std::uint32_t want[ 4 ] = { a.color, d.front_color, d.back_color, d.specular_color };
        for ( std::uint32_t x : want )
            bad += c.next( ) == x ? 0 : 1;
    }
    const unsigned n = c.next( );
    int calls = 0;
    for ( unsigned v = 0; v < n; ++v )
    {
        const unsigned kind = c.next( );
        std::vector< std::uint32_t > in( c.next( ) );
        for ( auto& x : in )
            x = c.next( );
        std::uint32_t want_lm[ 12 ];
        for ( auto& x : want_lm )
            x = c.next( );
        const std::uint32_t want_ret = c.next( );
        std::vector< std::uint32_t > want_log( 2 * c.next( ) );
        for ( auto& x : want_log )
            x = c.next( );
        light_manager_t m;
        std::memset( &m, 0xA5, sizeof( m ) );
        std::uint32_t ret = 0xFFFFFFFFu;
        std::vector< std::uint32_t > got_log;
        static const int a_tag = 1, b_tag = 2;   // distinct values: identical constants may be folded
        const void* pick[ 3 ] = { nullptr, &a_tag, &b_tag };
        if ( kind == 0 )
            ret = ambient_light_trigger( in[ 0 ], m );
        else if ( kind == 1 )
            ret = directional_light_trigger( in.data( ), in[ 16 ], in[ 17 ], in[ 18 ], m );
        else
            ret = substitute_asset_trigger( pick[ in[ 0 ] ], pick[ in[ 1 ] ], &got_log, []( void* u, const void* x, const void* y )
                                            {
                                                auto code = []( const void* p ) { return p == nullptr ? 0u : ( p == &a_tag ? 1u : 2u ); };
                                                static_cast< std::vector< std::uint32_t >* >( u )->push_back( code( x ) );
                                                static_cast< std::vector< std::uint32_t >* >( u )->push_back( code( y ) );
                                            } );
        std::uint32_t got[ 12 ];
        std::memcpy( got, &m, 48 );
        int diff = ret == want_ret ? 0 : 1;
        for ( int k = 0; k < 12; ++k )
            diff += ( kind == 1 && k == 8 ) || got[ k ] == want_lm[ k ] ? 0 : 1;
        diff += got_log == want_log ? 0 : 1;
        calls += static_cast< int >( want_log.size( ) / 2 );
        if ( diff && bad < 6 )
            LOG_ERROR( "settings_triggers vector %u (kind %u): %d words differ", v, kind, diff );
        bad += diff;
    }
    if ( bad == 0 )
        LOG_SUCCESS( "the settings Trigger handlers match retail (VuAmbientLightEntity 0x5EF780, VuDirectionalLightEntity 0x5EEEC0 / 0x5EEEE0, "
                     "VuSubstituteAssetEntity 0x5F4BD0): %u calls, the light manager's words, %d substitutions, the ctor defaults", n, calls );
    else
        LOG_ERROR( "settings_triggers FAILED %d", bad );
    return bad == 0;
}

// VuForceAffectorEntity (world/force.hxx) against retail (case force_affector): the ctor defaults; per step the log (the
// box scaling, the ghost add / remove, the step callback, the Build tick {0x73A84C, this, 0x439ED0, 0}), the VuRetVal, the
// objects' force words after the step, the active byte and the ghost words (each row's w lane - a stack word retail never
// wrote - excepted).
#include <workspace/core/world/force.hxx>

inline bool verify_force_affector( )
{
    logic_check_t c{ g_golden_fae_words };
    int bad = 0, steps = 0, pushes = 0, reported = 0;
    {
        const force_affector_t d;
        const std::uint32_t want[ 5 ] = { d.initially_active, control_w( d.acceleration ), control_w( d.max_speed ), d.active, 0x73A88Cu };
        for ( std::uint32_t x : want )
            bad += c.next( ) == x ? 0 : 1;
    }
    struct env_t { std::vector< std::vector< std::uint32_t > > log; };
    const unsigned n = c.next( );
    for ( unsigned v = 0; v < n; ++v )
    {
        force_affector_t e;
        e.initially_active = static_cast< std::uint8_t >( c.next( ) );
        e.acceleration = control_f( c.next( ) );
        e.max_speed = control_f( c.next( ) );
        float tm[ 16 ], scale[ 3 ];
        for ( float& x : tm )
            x = control_f( c.next( ) );
        for ( float& x : scale )
            x = control_f( c.next( ) );
        env_t env;
        force_affector_hooks_t h;
        h.user = &env;
        h.set_scaling = []( void* u, const float* s )
        {
            static_cast< env_t* >( u )->log.push_back( { 1u, 1u, control_w( s[ 0 ] ), control_w( s[ 1 ] ), control_w( s[ 2 ] ), control_w( s[ 3 ] ) } );
        };
        h.add_object = []( void* u, std::uint32_t g, std::uint32_t m ) { static_cast< env_t* >( u )->log.push_back( { 2u, 1u, g, m } ); };
        h.remove_object = []( void* u ) { static_cast< env_t* >( u )->log.push_back( { 3u, 1u } ); };
        h.step_callback = []( void* u, bool add ) { static_cast< env_t* >( u )->log.push_back( { 4u, add ? 1u : 0u, 1u } ); };
        h.tick_register = []( void* u, bool on )
        {
            if ( on )
                static_cast< env_t* >( u )->log.push_back( { 5u, key_motion_fnv( "Build" ), 0x73A84Cu, 1u, 0x439ED0u, 0u } );
            else
                static_cast< env_t* >( u )->log.push_back( { 6u, 1u } );
        };
        int unrecreated = 0;
        h.unrecreated = &unrecreated;
        const unsigned ns = c.next( );
        for ( unsigned s = 0; s < ns; ++s )
        {
            const unsigned op = c.next( );
            std::vector< std::uint32_t > arg( c.next( ) );
            for ( auto& x : arg )
                x = c.next( );
            std::vector< std::uint32_t > want_ret( c.next( ) );
            for ( auto& x : want_ret )
                x = c.next( );
            std::vector< std::vector< std::uint32_t > > want_log( c.next( ) );
            for ( auto& w : want_log )
            {
                w.resize( c.next( ) );
                for ( auto& x : w )
                    x = c.next( );
            }
            std::uint32_t want_state[ 17 ];
            for ( auto& x : want_state )
                x = c.next( );
            env.log.clear( );
            std::vector< std::uint32_t > got_ret{ 0u };
            switch ( op )
            {
                case 0: force_affector_game_init( e, tm, scale, h ); break;
                case 1: force_affector_activate( e, tm, scale, h ); break;
                case 2: force_affector_deactivate( e, h ); break;
                case 3:
                {
                    for ( int k = 0; k < 16; ++k )
                        e.ghost[ k ] = control_f( arg[ static_cast< std::size_t >( k ) ] );
                    float sc[ 3 ] = { control_f( arg[ 16 ] ), control_f( arg[ 17 ] ), control_f( arg[ 18 ] ) };
                    const int nb = static_cast< int >( arg[ 20 ] );
                    std::vector< force_body_t > bodies( static_cast< std::size_t >( nb ) );
                    for ( int k = 0; k < nb; ++k )
                    {
                        const std::uint32_t* a = arg.data( ) + 21 + 14 * k;
                        force_body_t& b = bodies[ static_cast< std::size_t >( k ) ];
                        b.type = a[ 0 ];
                        for ( int j = 0; j < 3; ++j )
                        {
                            b.position[ j ] = control_f( a[ 1 + j ] );
                            b.velocity[ j ] = control_f( a[ 4 + j ] );
                            b.factor[ j ] = control_f( a[ 8 + j ] );
                            b.force[ j ] = control_f( a[ 11 + j ] );
                        }
                        b.inverse_mass = control_f( a[ 7 ] );
                    }
                    force_affector_step( e, sc, bodies.data( ), nb );
                    got_ret.clear( );
                    for ( int k = 0; k < nb; ++k )
                    {
                        const std::uint32_t* a = arg.data( ) + 21 + 14 * k;
                        for ( int j = 0; j < 3; ++j )
                        {
                            const std::uint32_t f = control_w( bodies[ static_cast< std::size_t >( k ) ].force[ j ] );
                            got_ret.push_back( f );
                            pushes += f != a[ 11 + j ] ? 1 : 0;
                        }
                    }
                    break;
                }
                case 4: force_affector_build( e, false, h ); break;
                case 5: force_affector_game_release( e, h ); break;
                default: ++bad; break;
            }
            int diff = ( got_ret == want_ret ? 0 : 1 ) + ( env.log == want_log ? 0 : 1 ) + ( e.active == want_state[ 0 ] ? 0 : 1 );
            for ( int k = 0; k < 16; ++k )
                diff += control_w( e.ghost[ k ] ) == want_state[ 1 + k ] ? 0 : 1;
            if ( diff && reported++ < 6 )
                LOG_ERROR( "force_affector scenario %u step %u (op %u): %d words differ", v, s, op, diff );
            bad += diff + unrecreated;
            unrecreated = 0;
            ++steps;
        }
    }
    if ( bad == 0 )
        LOG_SUCCESS( "VuForceAffectorEntity matches retail: %u scenarios, %d steps (slots 6 / 7, Activate / Deactivate, the step 0x439970: "
                     "%d force components changed, the Build tick)", n, steps, pushes );
    else
        LOG_ERROR( "force_affector FAILED %d", bad );
    return bad == 0;
}
