#pragma once

// The world area's oracle-backed checks (one file per parallel agent; included by check.hxx after every
// shared check, so anything above is usable here). verify_world_all runs them all.

// transform_draw_matrix (hydro world/transform.hxx) against the draw matrix
// retail's prop owners leave in their model-draw component (oracle case
// prop_draw_matrix): every level / template prop of each class that carries a
// Vu3dDrawStaticModelComponent or Vu3dDrawAnimatedModelComponent, loaded alone,
// plus a hand-built nested set whose parents move the children after their own
// postLoad (the transform watcher's path). Inputs are retail's transform world
// and scale; all 16 words compared, one failure per prop.
inline bool verify_prop_draw_matrix( )
{
    int failed = 0;
    for ( std::size_t k = 0; k < g_golden_prop_matrices_count; ++k )
    {
        const golden_prop_matrix_t& want = g_golden_prop_matrices[ k ];
        transform_component_t c;
        transform_construct( c );
        std::memcpy( c.world, want.m_world, sizeof( c.world ) );
        std::memcpy( c.scale, want.m_scale, 3 * sizeof( float ) );
        float ours[ 16 ];
        transform_draw_matrix( c, ours );
        if ( std::memcmp( ours, want.m_matrix, sizeof( ours ) ) != 0 && ++failed <= 8 )
        {
            int w = 0;
            while ( w < 15 && std::memcmp( &ours[ w ], &want.m_matrix[ w ], 4 ) == 0 )
                ++w;
            std::uint32_t got;
            std::memcpy( &got, &ours[ w ], 4 );
            LOG_ERROR( "prop draw matrix: '%s' word %d is 0x%08X, retail 0x%08X", want.m_path, w, got, want.m_matrix[ w ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "transform_draw_matrix matches the prop owners' draw matrix (0x546AE0 / 0x545FF0 / 0x5ED420: fromScale x world) "
                     "on %d props (%d hand-built nested), 16 words each, bit for bit",
            static_cast< int >( g_golden_prop_matrices_count ), static_cast< int >( g_golden_prop_matrices_synthetic ) );
    else
        LOG_ERROR( "transform_draw_matrix FAILED on %d props", failed );
    return failed == 0;
}

// A golden entity-targets document (the hand-built project of real entities an
// entity_targets_case loaded through retail's VuEntity::load) loaded through
// hydro's tree: VuProject::load (entity_load_project) with the verified
// callbacks, as world.hxx's load_entity_tree runs it. Each child by name.
inline std::unique_ptr< entity_t > world_check_tree( const unsigned* words, std::size_t bytes, std::map< std::string, entity_t* >& by_name )
{
    const unsigned char* raw = reinterpret_cast< const unsigned char* >( words );
    static std::vector< value_t > documents;
    documents.push_back( c_reader( std::vector< std::uint8_t >( raw, raw + bytes ) ).parse( ) );
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
    std::unique_ptr< entity_t > tree = entity_load_project( member( documents.back( ), "VuProject" ), "Data/Projects/Check.vuprj", world );
    if ( tree )
        for ( auto& child : tree->m_children )
            by_name[ child->m_name ] = child.get( );
    return tree;
}

// Compare one child's modelled words against the golden row; logs and counts.
inline void world_check_words( const char* what, const golden_entity_targets_t& want, const std::vector< std::uint32_t >& ours, int& failed,
    int& compared )
{
    if ( static_cast< int >( ours.size( ) ) != want.m_count )
    {
        LOG_ERROR( "%s: '%s' (%s) has %d targets, retail %d", what, want.m_source, want.m_child, static_cast< int >( ours.size( ) ), want.m_count );
        ++failed;
        return;
    }
    for ( int k = 0; k < want.m_count; ++k )
    {
        ++compared;
        if ( ours[ k ] != want.m_values[ k ] && ++failed <= 8 )
            LOG_ERROR( "%s: '%s' target %d is 0x%08X, retail 0x%08X", what, want.m_source, k, ours[ k ], want.m_values[ k ] );
    }
}

// VuAmbientLightEntity / VuDirectionalLightEntity's property loads (engine
// entity/rig.hxx through the tree's entity_load_properties) against retail's
// VuEntity::load on every such entity in the level templates plus hand-built
// ones (oracle case load_light_rig), bit for bit.
inline bool verify_light_rig( )
{
    std::map< std::string, entity_t* > by_name;
    const std::unique_ptr< entity_t > tree = world_check_tree( g_golden_light_rig_document, g_golden_light_rig_document_bytes, by_name );
    int failed = 0;
    int compared = 0;
    for ( std::size_t k = 0; k < g_golden_light_rig_count; ++k )
    {
        const golden_entity_targets_t& want = g_golden_light_rig[ k ];
        const auto found = by_name.find( want.m_child );
        if ( found == by_name.end( ) || !found->second->m_state || found->second->m_type != want.m_type )
        {
            LOG_ERROR( "light rig: '%s' (%s) missing or without state", want.m_source, want.m_child );
            ++failed;
            continue;
        }
        std::vector< std::uint32_t > ours;
        if ( found->second->m_type == "VuAmbientLightEntity" )
            ours = { static_cast< const ambient_light_state_t* >( found->second->m_state.get( ) )->value.color };
        else
        {
            const directional_light_t& l = static_cast< const directional_light_state_t* >( found->second->m_state.get( ) )->value;
            ours = { l.front_color, l.back_color, l.specular_color };
        }
        world_check_words( "light rig", want, ours, failed, compared );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ambient_light_load / directional_light_load match VuAmbientLightEntity / VuDirectionalLightEntity's loads (ctors "
                     "0x5EF580 / 0x5EEA90) on %d lights: %d values, bit for bit", static_cast< int >( g_golden_light_rig_count ), compared );
    else
        LOG_ERROR( "the light rig's property loads FAILED %d checks", failed );
    return failed == 0;
}

// VuGlobalGfxSettingsEntity's 26 property loads (engine entity/gfx.hxx through
// the tree's entity_load_properties) against retail's VuEntity::load on every
// such entity in the level templates and projects plus two hand-built ones
// (oracle case load_gfx_settings), bit for bit.
inline bool verify_gfx_settings( )
{
    std::map< std::string, entity_t* > by_name;
    const std::unique_ptr< entity_t > tree = world_check_tree( g_golden_gfx_settings_document, g_golden_gfx_settings_document_bytes, by_name );
    int failed = 0;
    int compared = 0;
    for ( std::size_t k = 0; k < g_golden_gfx_settings_count; ++k )
    {
        const golden_entity_targets_t& want = g_golden_gfx_settings[ k ];
        const auto found = by_name.find( want.m_child );
        if ( found == by_name.end( ) || !found->second->m_state || found->second->m_type != want.m_type )
        {
            LOG_ERROR( "gfx settings: '%s' (%s) missing or without state", want.m_source, want.m_child );
            ++failed;
            continue;
        }
        std::vector< std::uint32_t > ours( 26 );
        gfx_settings_words( static_cast< const gfx_settings_state_t* >( found->second->m_state.get( ) )->value, ours.data( ) );
        world_check_words( "gfx settings", want, ours, failed, compared );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "gfx_settings_load matches VuGlobalGfxSettingsEntity's 26 loads (base ctor 0x5A6520) on %d entities: %d values, bit for bit",
            static_cast< int >( g_golden_gfx_settings_count ), compared );
    else
        LOG_ERROR( "the gfx settings' property loads FAILED %d checks", failed );
    return failed == 0;
}

// VuTriggerEntity's two property loads (engine entity/trigger.hxx through the
// tree's entity_load_properties) against retail's VuEntity::load on the
// VuTriggerPlaneEntity / VuTriggerBoxEntity of the templates plus five
// hand-built ones (oracle case load_triggers, start-up's 0x4D9260 run first so
// the Trigger Type default is the enum's first value), bit for bit.
inline bool verify_triggers( )
{
    std::map< std::string, entity_t* > by_name;
    const std::unique_ptr< entity_t > tree = world_check_tree( g_golden_triggers_document, g_golden_triggers_document_bytes, by_name );
    int failed = 0;
    int compared = 0;
    for ( std::size_t k = 0; k < g_golden_triggers_count; ++k )
    {
        const golden_entity_targets_t& want = g_golden_triggers[ k ];
        const auto found = by_name.find( want.m_child );
        if ( found == by_name.end( ) || !found->second->m_state || found->second->m_type != want.m_type )
        {
            LOG_ERROR( "triggers: '%s' (%s) missing or without state", want.m_source, want.m_child );
            ++failed;
            continue;
        }
        const trigger_properties_t& t = static_cast< const trigger_state_t* >( found->second->m_state.get( ) )->value;
        world_check_words( "triggers", want, { t.initially_active }, failed, compared );
        ++compared;
        if ( t.trigger_type != want.m_text && ++failed <= 8 )
            LOG_ERROR( "triggers: '%s' Trigger Type is '%s', retail '%s'", want.m_source, t.trigger_type.c_str( ), want.m_text );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "trigger_load matches VuTriggerEntity's loads (ctor 0x61C6A0) on %d trigger planes / boxes: %d values, bit for bit",
            static_cast< int >( g_golden_triggers_count ), compared );
    else
        LOG_ERROR( "the triggers' property loads FAILED %d checks", failed );
    return failed == 0;
}

inline bool verify_world_all( )
{
    bool ok = true;
    ok = verify_prop_draw_matrix( ) && ok;
    ok = verify_light_rig( ) && ok;
    ok = verify_gfx_settings( ) && ok;
    ok = verify_triggers( ) && ok;
    return ok;
}
