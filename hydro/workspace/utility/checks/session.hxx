#pragma once

#include <workspace/core/world/race.hxx>

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
        std::vector< std::uint32_t > ours;
        if ( type == "VuSetBooleanEntity" )
            ours = { s->value.words[ 0 ] & 0xFFu };
        else if ( type == "VuRandomTriggerEntity" )
            ours.assign( s->value.words, s->value.words + 8 );
        else if ( type == "VuCounterEntity" )
            ours = { s->value.words[ 0 ], s->value.words[ 1 ] };
        else if ( type != "VuSetStringEntity" && type != "VuStringEntity" )
            ours = { s->value.words[ 0 ] };
        world_check_words( "logic properties", want, ours, failed, compared );
        if ( type == "VuSetStringEntity" || type == "VuStringEntity" )
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

inline bool verify_session_all( )
{
    bool ok = true;
    ok = verify_logic_properties( ) && ok;
    return ok;
}
