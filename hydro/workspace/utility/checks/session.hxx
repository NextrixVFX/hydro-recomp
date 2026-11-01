#pragma once

#include <workspace/core/world/race.hxx>
#include <workspace/core/world/strings.hxx>
#include <workspace/core/world/background.hxx>

// The world area's second checks file (checks/world.hxx is at its size limit): the race session's
// pieces from world batch 24 on. Included at the end of checks/world.hxx; verify_session_all runs
// from verify_world_all.

// logic_load_properties (world/race.hxx: the logic entities' property sets on the tree through
// race_entity_world, as the race loads them) against retail's VuEntity::load 0x5009D0 on every logic
// entity with a property in the level templates and projects plus hand-built ones (oracle case
// logic_properties), bit for bit: the words from +0x60 and the VuString.
inline bool verify_logic_properties( )
{
    const unsigned char* raw = reinterpret_cast< const unsigned char* >( g_golden_logic_properties_document );
    static std::vector< value_t > documents;
    documents.push_back( c_reader( std::vector< std::uint8_t >( raw, raw + g_golden_logic_properties_document_bytes ) ).parse( ) );
    static const entity_registry_t registry = build_entity_registry( { } );
    entity_world_t world = race_entity_world( registry, nullptr );
    const std::unique_ptr< entity_t > tree = entity_load_project( member( documents.back( ), "VuProject" ), "Data/Projects/Check.vuprj", world );
    std::map< std::string, entity_t* > by_name;
    if ( tree )
        for ( auto& child : tree->m_children )
            by_name[ child->m_name ] = child.get( );
    int failed = 0;
    int compared = 0;
    for ( std::size_t k = 0; k < g_golden_logic_properties_count; ++k )
    {
        const golden_entity_targets_t& want = g_golden_logic_properties[ k ];
        const auto found = by_name.find( want.m_child );
        logic_state_t* s = found != by_name.end( ) && found->second->m_type == want.m_type ? logic_state_of( *found->second ) : nullptr;
        if ( !s )
        {
            LOG_ERROR( "logic properties: '%s' (%s) missing or without state", want.m_source, want.m_child );
            ++failed;
            continue;
        }
        const std::string type = want.m_type;
        const bool text = type == "VuSetStringEntity" || type == "VuStringEntity" || type == "VuIsSkuEntity" || type == "VuIsEventTypeEntity";
        std::vector< std::uint32_t > ours;
        if ( type == "VuSetBooleanEntity" )
            ours = { s->value.words[ 0 ] & 0xFFu };
        else if ( type == "VuRandomTriggerEntity" )
            ours.assign( s->value.words, s->value.words + 8 );
        else if ( type == "VuCounterEntity" )
            ours = { s->value.words[ 0 ], s->value.words[ 1 ] };
        else if ( !text )
            ours = { s->value.words[ 0 ] };
        world_check_words( "logic properties", want, ours, failed, compared );
        if ( text )
        {
            ++compared;
            if ( s->value.text != want.m_texts[ 0 ] && ++failed <= 8 )
                LOG_ERROR( "logic properties: '%s' Value is \"%s\", retail \"%s\"", want.m_source, s->value.text.c_str( ), want.m_texts[ 0 ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "logic_load_properties matches the logic entities' property loads (ctors 0x5DC910 / 0x5DD710 / 0x5E2F40 / 0x5E32C0 / "
                     "0x5E3640 / 0x5E5240 / 0x5E10E0 / 0x5E21B0 / 0x5E4C00, VuEntity::load 0x5009D0) on %d entities: %d values, bit for bit",
            static_cast< int >( g_golden_logic_properties_count ), compared );
    else
        LOG_ERROR( "the logic entities' property loads FAILED %d checks", failed );
    return failed == 0;
}

// dynamic_prop_contact (world/dynprop.hxx) against retail's 0x437F40 run whole (case prop_contact): the two body
// images built from the vector's words (origin +0x40, linear +0x140, angular +0x150, flags +0xD8, group +0x2CA),
// the record's point / normal; compared: +0x164, +0x178, +0x17C and the weak ref's target (the owner or none).
inline bool verify_prop_contact( )
{
    int failed = 0;
    for ( std::size_t k = 0; k < g_golden_prop_contact_count; ++k )
    {
        const golden_prop_contact_t& v = g_golden_prop_contact[ k ];
        std::uint8_t own[ 0x2D8 ] = { }, other[ 0x2D8 ] = { };
        const auto fill = [ ]( std::uint8_t* b, const unsigned* w ) {
            static const int at[ 9 ] = { 0x40, 0x44, 0x48, 0x140, 0x144, 0x148, 0x150, 0x154, 0x158 };
            for ( int i = 0; i < 9; ++i )
                std::memcpy( b + at[ i ], &w[ i ], 4 );
            std::memcpy( b + 0xD8, &w[ 9 ], 4 );
            const std::uint16_t g = static_cast< std::uint16_t >( w[ 10 ] );
            std::memcpy( b + 0x2CA, &g, 2 );
        };
        fill( own, v.own );
        fill( other, v.other );
        dynamic_prop_t p{ };
        std::memcpy( &p.threshold_mph, &v.threshold, 4 );
        dynamic_prop_hit_t hit;
        hit.pending = static_cast< std::uint8_t >( v.pending );
        std::memcpy( &hit.strength, &v.strength, 4 );
        float timer;
        std::memcpy( &timer, &v.timer, 4 );
        float point[ 4 ] = { }, normal[ 4 ] = { };
        std::memcpy( point, v.point, 12 );
        std::memcpy( normal, v.normal, 12 );
        dynamic_prop_contact( p, hit, timer, own, other, point, normal, v.owner != 0 );
        std::uint32_t s, t;
        std::memcpy( &s, &hit.strength, 4 );
        std::memcpy( &t, &timer, 4 );
        const int target = hit.target == other ? 1 : ( hit.target ? 2 : 0 );
        if ( hit.pending != v.out_pending || s != v.out_strength || t != v.out_timer || target != v.out_target )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "prop contact: vector %d: pending %d strength 0x%08X timer 0x%08X target %d, retail %d 0x%08X 0x%08X %d",
                           static_cast< int >( k ), hit.pending, s, t, target, v.out_pending, v.out_strength, v.out_timer, v.out_target );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "dynamic_prop_contact matches VuDynamicGamePropEntity's contact callback 0x437F40 (0x4AD340, the weak ref 0x433B70) on %d "
                     "contacts: the hit, strength, timer and target, bit for bit", static_cast< int >( g_golden_prop_contact_count ) );
    else
        LOG_ERROR( "dynamic_prop_contact FAILED on %d of %d contacts", failed, static_cast< int >( g_golden_prop_contact_count ) );
    return failed == 0;
}

// string_db_reload / string_db_get (world/strings.hxx) against retail's VuStringDBImpl (case string_db): per load
// (four real languages from the pack, two hand-built documents; g 0x7359FC seeded two ways) the result, the
// word-wrap flags, g 0x73579A, the map's entry count and digest (FNV-1a over the entries by hash) and getString on
// the probe keys.
inline bool verify_string_db( )
{
    c_archive archive;
    const std::string pack = world_check_pack( );
    if ( pack.empty( ) || !archive.open( pack.c_str( ) ) )
    {
        LOG_ERROR( "string db: the pack is not readable" );
        return false;
    }
    int failed = 0;
    for ( std::size_t k = 0; k < g_golden_string_db_count; ++k )
    {
        const golden_string_db_t& want = g_golden_string_db[ k ];
        value_t doc;
        bool have = false;
        if ( want.document )
        {
            const unsigned char* raw = reinterpret_cast< const unsigned char* >( want.document );
            doc = c_reader( std::vector< std::uint8_t >( raw, raw + want.document_bytes ) ).parse( );
            have = true;
        }
        else
        {
            std::vector< std::uint8_t > blob;
            have = archive.read( ( std::string( "VuStringAsset/" ) + want.language ).c_str( ), blob ) && pfx_asset_document( blob, doc );
        }
        string_db_t db;
        db.language = want.language;
        db.wrap_flags = want.flags_in;
        const bool r = string_db_reload( db, [ & ]( const std::string& ) -> const value_t* { return have ? &doc : nullptr; } );
        std::uint32_t h = 0x811C9DC5u;
        const auto mix = [ &h ]( std::uint8_t b ) { h = ( h ^ b ) * 0x1000193u; };
        for ( const auto& e : db.strings )
        {
            for ( int b = 0; b < 4; ++b )
                mix( static_cast< std::uint8_t >( e.first >> ( 8 * b ) ) );
            for ( const char c : e.second )
                mix( static_cast< std::uint8_t >( c ) );
            mix( 0 );
        }
        bool bad = ( r ? 1 : 0 ) != want.result || db.wrap_flags != want.flags || db.flag_73579a != want.flag_73579a ||
                   static_cast< int >( db.strings.size( ) ) != want.count || h != want.digest;
        for ( int g = 0; g < want.gets; ++g )
            if ( string_db_get( db, want.get[ g ].key ) != want.get[ g ].value )
            {
                bad = true;
                LOG_ERROR( "string db %s: getString('%s') = '%s', retail '%s'", want.language, want.get[ g ].key, string_db_get( db, want.get[ g ].key ).c_str( ),
                           want.get[ g ].value );
            }
        if ( bad && ++failed <= 8 )
            LOG_ERROR( "string db %s (flags in 0x%08X): result %d flags 0x%08X 0x73579A %d entries %d digest 0x%08X, retail %d 0x%08X %d %d 0x%08X",
                       want.language, want.flags_in, r ? 1 : 0, db.wrap_flags, db.flag_73579a, static_cast< int >( db.strings.size( ) ), h, want.result,
                       want.flags, want.flag_73579a, want.count, want.digest );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "string_db_reload / string_db_get match VuStringDBImpl's reload 0x53BB40 / loader 0x53D210 / getString 0x53B830 on %d loads "
                     "(the map, the word-wrap flags, the probes), bit for bit", static_cast< int >( g_golden_string_db_count ) );
    else
        LOG_ERROR( "the string db FAILED on %d of %d loads", failed, static_cast< int >( g_golden_string_db_count ) );
    return failed == 0;
}

// front_end_trigger (world/background.hxx: VuSetFrontEndCameraEntity's Trigger 0x408F90) against retail's handler on
// 24 entity images (every Modify combination x 3 value sets; oracle case front_end_trigger): the broadcasts in order -
// the name hash, the VuParams size word and its defined bytes - and the answer (VuRetVal 0: graph.hxx returns retval_t{}).
inline bool verify_front_end_trigger( )
{
    int failed = 0;
    for ( std::size_t k = 0; k < g_golden_front_end_trigger_count; ++k )
    {
        const golden_front_end_trigger_t& want = g_golden_front_end_trigger[ k ];
        front_end_trigger_t e;
        std::memcpy( &e.interpolation, &want.interpolation, 4 );
        for ( int m = 0; m < 3; ++m )
            e.modify[ m ] = static_cast< std::uint8_t >( want.modify[ m ] );
        std::memcpy( e.eye, want.eye, 16 );
        std::memcpy( e.target, want.target, 16 );
        std::memcpy( &e.far_plane, &want.far_plane, 4 );
        int n = 0;
        bool bad = false;
        front_end_trigger( e, [ & ]( std::uint32_t hash, const std::uint8_t* p ) {
            std::int32_t size;
            std::memcpy( &size, p + 0x80, 4 );
            if ( n >= want.count )
            {
                bad = true;
                ++n;
                return;
            }
            const golden_front_end_broadcast_t& b = g_golden_front_end_broadcasts[ want.first + n++ ];
            std::uint8_t expect[ 20 ];
            std::memcpy( expect, b.words, 20 );
            if ( hash != b.hash || size != b.size || size < 0 || size > 20 || std::memcmp( p, expect, static_cast< std::size_t >( size ) ) != 0 )
                bad = true;
        } );
        bad = bad || n != want.count || want.retval != 0u || want.eax_is_out != 1;
        if ( bad && ++failed <= 8 )
            LOG_ERROR( "front end trigger %d (modify %d %d %d): %d broadcasts, retail %d (or a hash / size / byte differs)", static_cast< int >( k ),
                       want.modify[ 0 ], want.modify[ 1 ], want.modify[ 2 ], n, want.count );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "front_end_trigger matches VuSetFrontEndCameraEntity's Trigger 0x408F90 on %d entity images (broadcast order, hashes, "
                     "VuParams bytes and sizes, VuRetVal 0), bit for bit", static_cast< int >( g_golden_front_end_trigger_count ) );
    else
        LOG_ERROR( "the front end trigger FAILED on %d of %d images", failed, static_cast< int >( g_golden_front_end_trigger_count ) );
    return failed == 0;
}

inline bool verify_session_all( )
{
    bool ok = true;
    ok = verify_front_end_trigger( ) && ok;
    ok = verify_logic_properties( ) && ok;
    ok = verify_string_db( ) && ok;
    ok = verify_prop_contact( ) && ok;
    return ok;
}
