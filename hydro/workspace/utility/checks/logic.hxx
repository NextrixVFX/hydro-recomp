#pragma once

// The logic entities (world/logic.hxx) against retail (case logic_plugs): per vector the state / text / params / VuRand
// set as retail had them, the input plug dispatched through g_logic_plugs by (vtable, name), every output fire answered
// from the drawn list as retail's 0x5023A0 would ({0, 0} for a void plug after its link ran; the link's answer for a
// typed one). Compared: the VuRetVal words (a type-4 value by text), the fires (name, params size and bytes), the answers
// consumed, the state words / text after, the VuRand after. The "*ctor" rows: logic_construct's words against the ctor's.

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
