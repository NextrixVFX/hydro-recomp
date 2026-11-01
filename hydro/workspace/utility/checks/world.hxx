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
    world.m_components_for = entity_components_scripted;
    world.m_component_load = entity_load_component;
    world.m_component_post = entity_post_load_component;
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
        world_check_words( "triggers", want, { t.initially_active, trigger_type_mask( t.trigger_type ) }, failed, compared );
        ++compared;
        if ( t.trigger_type != want.m_texts[ 0 ] && ++failed <= 8 )
            LOG_ERROR( "triggers: '%s' Trigger Type is '%s', retail '%s'", want.m_source, t.trigger_type.c_str( ), want.m_texts[ 0 ] );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "trigger_load matches VuTriggerEntity's loads (ctor 0x61C6A0) on %d trigger planes / boxes: %d values, bit for bit",
            static_cast< int >( g_golden_triggers_count ), compared );
    else
        LOG_ERROR( "the triggers' property loads FAILED %d checks", failed );
    return failed == 0;
}

// VuStaticPfxEntity's four property loads (engine entity/pfx.hxx through the
// tree's entity_load_properties) and its onGameInitialize 0x5EB060 hand-off to
// the particle system (oracle case load_static_pfx: the template entities plus
// three hand-built): the targets bit for bit, then what hydro hands c_pfx -
// setScale = Pfx Scale, setColor = static_pfx_color, setMatrix = the transform
// world (scale kept out), start iff Initially Active.
inline bool verify_static_pfx( )
{
    std::map< std::string, entity_t* > by_name;
    const std::unique_ptr< entity_t > tree = world_check_tree( g_golden_static_pfx_document, g_golden_static_pfx_document_bytes, by_name );
    int failed = 0;
    int compared = 0;
    const auto word = []( float f )
    {
        std::uint32_t b;
        std::memcpy( &b, &f, sizeof( b ) );
        return b;
    };
    for ( std::size_t k = 0; k < g_golden_static_pfx_count; ++k )
    {
        const golden_entity_targets_t& want = g_golden_static_pfx[ k ];
        const golden_pfx_handoff_t& hand = g_golden_static_pfx_handoff[ k ];
        const auto found = by_name.find( want.m_child );
        const transform_component_t* t = found != by_name.end( ) ? entity_transform( *found->second ) : nullptr;
        if ( found == by_name.end( ) || !found->second->m_state || found->second->m_type != want.m_type || !t )
        {
            LOG_ERROR( "static pfx: '%s' (%s) missing, without state or without a transform", want.m_source, want.m_child );
            ++failed;
            continue;
        }
        const static_pfx_t& p = static_cast< const static_pfx_state_t* >( found->second->m_state.get( ) )->value;
        world_check_words( "static pfx", want, { p.initially_active, word( p.pfx_scale ), p.pfx_color }, failed, compared );
        ++compared;
        if ( p.effect_name != want.m_texts[ 0 ] && ++failed <= 8 )
            LOG_ERROR( "static pfx: '%s' Effect Name is '%s', retail '%s'", want.m_source, p.effect_name.c_str( ), want.m_texts[ 0 ] );
        float color[ 4 ];
        static_pfx_color( p, color );
        std::vector< std::uint32_t > ours = { static_cast< std::uint32_t >( p.initially_active != 0 ), word( p.pfx_scale ) };
        std::vector< std::uint32_t > theirs = { static_cast< std::uint32_t >( hand.m_started ), hand.m_scale };
        for ( int c = 0; c < 4; ++c )
        {
            ours.push_back( word( color[ c ] ) );
            theirs.push_back( hand.m_color[ c ] );
        }
        for ( int w = 0; w < 16; ++w )
        {
            ours.push_back( word( t->world[ w ] ) );
            theirs.push_back( hand.m_matrix[ w ] );
        }
        for ( std::size_t w = 0; w < ours.size( ); ++w )
        {
            ++compared;
            if ( ours[ w ] != theirs[ w ] && ++failed <= 8 )
                LOG_ERROR( "static pfx: '%s' hand-off word %d is 0x%08X, retail 0x%08X", want.m_source, static_cast< int >( w ), ours[ w ], theirs[ w ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "static_pfx_load and the hand-off to the particle system match VuStaticPfxEntity (ctor 0x5EAA40, onGameInitialize "
                     "0x5EB060) on %d entities: %d values, bit for bit", static_cast< int >( g_golden_static_pfx_count ), compared );
    else
        LOG_ERROR( "the static pfx loads / hand-off FAILED %d checks", failed );
    return failed == 0;
}

// An action entity's loaded targets as the oracle dumps them (load_actions):
// non-text targets as words in node order (a VuVector3 as three), texts apart.
inline bool world_action_words( const entity_t& e, std::vector< std::uint32_t >& w, std::vector< std::string >& texts )
{
    const auto f = []( float v )
    {
        std::uint32_t b;
        std::memcpy( &b, &v, sizeof( b ) );
        return b;
    };
    const auto vec = [ & ]( const float* v )
    {
        for ( int k = 0; k < 3; ++k )
            w.push_back( f( v[ k ] ) );
    };
    if ( const auto* p = entity_action< delay_properties_t >( e ); p && e.m_type == "VuDelayEntity" )
        w = { f( p->delay ), f( p->random_window ) };
    else if ( const auto* p = entity_action< boolean_properties_t >( e ); p && e.m_type == "VuBooleanEntity" )
        w = { p->value };
    else if ( const auto* p = entity_action< generic_event_properties_t >( e ); p && e.m_type == "VuGenericEventEntity" )
        texts = { p->event_name };
    else if ( const auto* p = entity_action< rotate_motion_properties_t >( e ); p && e.m_type == "VuRotateMotionEntity" )
    {
        w = { p->initially_active, p->one_shot };
        vec( p->axis );
        w.push_back( f( p->speed ) );
        w.push_back( p->local );
    }
    else if ( const auto* p = entity_action< attach_properties_t >( e ); p && e.m_type == "VuAttachEntity" )
    {
        w = { p->attach_at_start };
        vec( p->relative_position );
        vec( p->relative_rotation );
        texts = { p->node_name };
    }
    else if ( const auto* p = entity_action< play_animation_properties_t >( e ); p && e.m_type == "VuPlayAnimationEntity" )
    {
        texts = { p->animation_asset, p->timed_event_asset };
        w = { f( p->start_time ), f( p->blend_time ), f( p->time_factor ), p->looping, p->one_shot };
    }
    else if ( const auto* p = entity_action< sky_box_properties_t >( e ); p && e.m_type == "VuSkyBoxEntity" )
        texts = { p->model_asset };
    else if ( const auto* p = entity_action< water_texture_properties_t >( e ); p && e.m_type == "VuWaterTextureSettingsEntity" )
        w = { f( p->gravity ), f( p->world_size ), f( p->wind_speed ), f( p->directional_power ), f( p->suppression_wave_length ),
            f( p->height_factor ), f( p->time_factor ), f( p->normal_texture_scale ) };
    else if ( const auto* p = entity_action< front_end_camera_properties_t >( e ); p && e.m_type == "VuSetFrontEndCameraEntity" )
    {
        w = { f( p->interpolation_time ), p->modify_eye, p->modify_target, p->modify_far };
        vec( p->eye );
        vec( p->target );
        w.push_back( f( p->far_plane ) );
    }
    else if ( const auto* p = entity_action< point_wave_properties_t >( e ); p && e.m_type == "VuPointWaveEntity" )
        for ( float x : p->word )
            w.push_back( f( x ) );
    else
        return false;
    return true;
}

// The action / logic / motion / settings entities' property loads (engine
// entity/actions.hxx, through types.hxx's action table and the tree's
// entity_load_properties) against retail's VuEntity::load (oracle case
// load_actions: ten types from the templates and projects plus hand-built).
inline bool verify_actions( )
{
    std::map< std::string, entity_t* > by_name;
    const std::unique_ptr< entity_t > tree = world_check_tree( g_golden_actions_document, g_golden_actions_document_bytes, by_name );
    int failed = 0;
    int compared = 0;
    for ( std::size_t k = 0; k < g_golden_actions_count; ++k )
    {
        const golden_entity_targets_t& want = g_golden_actions[ k ];
        const auto found = by_name.find( want.m_child );
        std::vector< std::uint32_t > ours;
        std::vector< std::string > texts;
        if ( found == by_name.end( ) || found->second->m_type != want.m_type || !world_action_words( *found->second, ours, texts ) )
        {
            LOG_ERROR( "actions: '%s' (%s) missing or without state", want.m_source, want.m_child );
            ++failed;
            continue;
        }
        world_check_words( "actions", want, ours, failed, compared );
        for ( std::size_t t = 0; t < texts.size( ); ++t )
        {
            ++compared;
            if ( texts[ t ] != want.m_texts[ t ] && ++failed <= 8 )
                LOG_ERROR( "actions: '%s' text %d is '%s', retail '%s'", want.m_source, static_cast< int >( t ), texts[ t ].c_str( ), want.m_texts[ t ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the action / logic / settings loads match retail's VuEntity::load on %d entities of 10 types: %d values, bit for bit",
            static_cast< int >( g_golden_actions_count ), compared );
    else
        LOG_ERROR( "the action / logic / settings loads FAILED %d checks", failed );
    return failed == 0;
}

// trigger_plane_crossing (hydro trigger/volume.hxx) against
// VuTriggerPlaneEntity_testCrossings 0x5D1570 run on seeded planes and
// instigators (oracle case trigger_plane_test): per (plane, instigator) whether
// it fires and which plug, one failure per pair.
inline bool verify_trigger_plane( )
{
    int failed = 0;
    int pairs = 0;
    int fires = 0;
    for ( std::size_t p = 0; p < g_golden_trigger_planes_count; ++p )
    {
        const golden_trigger_plane_t& plane = g_golden_trigger_planes[ p ];
        float world[ 16 ], scale[ 3 ];
        std::memcpy( world, plane.m_world, sizeof( world ) );
        std::memcpy( scale, plane.m_scale, sizeof( scale ) );
        for ( int k = 0; k < plane.m_count; ++k )
        {
            const golden_trigger_instigator_t& g = g_golden_trigger_instigators[ plane.m_first + k ];
            trigger_instigator_t who{};
            who.mask = g.m_mask;
            std::memcpy( who.prev, g.m_prev, sizeof( who.prev ) );
            std::memcpy( &who.radius, &g.m_radius, 4 );
            std::memcpy( who.cur, g.m_cur, sizeof( who.cur ) );
            std::memcpy( &who.rate, &g.m_rate, 4 );
            const int ours = trigger_plane_crossing( world, scale, plane.m_mask, who );
            ++pairs;
            fires += g.m_fired >= 0;
            if ( ours != g.m_fired && ++failed <= 8 )
                LOG_ERROR( "trigger plane: plane %d instigator %d gives %d, retail %d", static_cast< int >( p ), k, ours, g.m_fired );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "trigger_plane_crossing matches VuTriggerPlaneEntity_testCrossings 0x5D1570 on %d planes: %d plane / instigator pairs, %d "
                     "crossings fired", static_cast< int >( g_golden_trigger_planes_count ), pairs, fires );
    else
        LOG_ERROR( "trigger_plane_crossing FAILED %d of %d pairs", failed, pairs );
    return failed == 0;
}

// static_model_select (engine entity/draw.hxx) against
// Vu3dDrawStaticModelComponent's draw 0x5B0730 on seeded inputs (oracle case
// static_model_draw): which model instance is drawn (or none) and the three
// words written into it, one failure per vector.
inline bool verify_static_model_draw( )
{
    const auto f = []( unsigned w )
    {
        float v;
        std::memcpy( &v, &w, sizeof( v ) );
        return v;
    };
    int failed = 0;
    int drawn = 0;
    for ( std::size_t k = 0; k < g_golden_static_draws_count; ++k )
    {
        const golden_static_draw_t& g = g_golden_static_draws[ k ];
        model_draw_t c;
        model_draw_construct( c, false );
        c.color = g.m_color;
        c.draw_distance = f( g.m_draw );
        c.lod0_draw_distance = f( g.m_lod0 );
        c.lod1_draw_distance = f( g.m_lod1 );
        c.rejection_scale_modifier = f( g.m_reject );
        c.dynamic_lighting = static_cast< std::uint8_t >( g.m_dynamic );
        const float lo[ 3 ] = { f( g.m_min[ 0 ] ), f( g.m_min[ 1 ] ), f( g.m_min[ 2 ] ) };
        const float hi[ 3 ] = { f( g.m_max[ 0 ] ), f( g.m_max[ 1 ] ), f( g.m_max[ 2 ] ) };
        const float eye[ 3 ] = { f( g.m_eye[ 0 ] ), f( g.m_eye[ 1 ] ), f( g.m_eye[ 2 ] ) };
        const static_draw_t ours = static_model_select( c, lo, hi, eye, g.m_reflection != 0, g.m_skip != 0, f( g.m_fade ) );
        std::uint32_t rejection;
        std::memcpy( &rejection, &ours.rejection, 4 );
        const bool same = ours.instance == g.m_instance &&
                          ( ours.instance < 0 || ( rejection == g.m_w14 && ours.color == g.m_w18 && ours.dynamic_lighting == g.m_b1c ) );
        drawn += g.m_instance >= 0;
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "static model draw: vector %d draws %d (0x%08X 0x%08X %d), retail %d (0x%08X 0x%08X %d)", static_cast< int >( k ), ours.instance,
                rejection, ours.color, ours.dynamic_lighting, g.m_instance, g.m_w14, g.m_w18, g.m_b1c );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "static_model_select matches Vu3dDrawStaticModelComponent's draw 0x5B0730 on %d vectors (%d drawn): instance, rejection, "
                     "colour and lighting bit for bit", static_cast< int >( g_golden_static_draws_count ), drawn );
    else
        LOG_ERROR( "static_model_select FAILED %d vectors", failed );
    return failed == 0;
}

// The event graph as VuScriptComponent's loads resolve it (engine
// entity/script.hxx: 0x503D70 / 0x504320, 0x502430, 0x542830, 0x5049E0)
// against retail's VuProject::load on Games/HydroelectricRace (its level's
// templates served) and a hand-built project (oracle case script_graph): the
// tree pre-order (parent, name) and, per script component, every plug's links
// in +0x50 order, every ref's target and the +0x28 referrers. One failure per
// entity that differs in any of it.
inline bool verify_script_graph( )
{
    static std::map< std::string, value_t > templates;
    if ( templates.empty( ) )
        for ( std::size_t k = 0; k < g_golden_script_templates_count; ++k )
        {
            const golden_script_document_t& d = g_golden_script_templates[ k ];
            const unsigned char* raw = reinterpret_cast< const unsigned char* >( d.m_words );
            templates[ d.m_name ] = c_reader( std::vector< std::uint8_t >( raw, raw + d.m_bytes ) ).parse( );
        }
    std::vector< std::string > names;
    for ( const auto& pair : templates )
        names.push_back( pair.first );
    const entity_registry_t registry = build_entity_registry( names );
    entity_world_t world;
    world.m_registry = &registry;
    world.m_ctor_for = entity_ctor_for;
    world.m_template = []( const std::string& name ) -> const value_t*
    {
        const auto found = templates.find( name );
        return found != templates.end( ) ? &found->second : nullptr;
    };
    world.m_components_for = entity_components_scripted;
    world.m_component_load = entity_load_component;
    world.m_component_post = entity_post_load_component;
    world.m_component_mark = entity_mark_component;
    world.m_state_for = entity_state_for;
    world.m_properties_load = entity_load_properties;
    world.m_properties_mark = entity_mark_properties;
    int failed = 0;
    int entities = 0;
    int links = 0;
    int bound = 0;
    for ( std::size_t p = 0; p < g_golden_script_projects_count; ++p )
    {
        const golden_script_project_t& want = g_golden_script_projects[ p ];
        const unsigned char* raw = reinterpret_cast< const unsigned char* >( want.m_words );
        const value_t document = c_reader( std::vector< std::uint8_t >( raw, raw + want.m_bytes ) ).parse( );
        const std::unique_ptr< entity_t > tree = entity_load_project( member( document, "VuProject" ), want.m_path, world );
        std::vector< entity_t* > order;
        std::vector< int > parent_of;
        std::map< const entity_t*, int > index_of;
        const auto walk = [ & ]( const auto& self, entity_t& e, int parent ) -> void
        {
            index_of[ &e ] = static_cast< int >( order.size( ) );
            order.push_back( &e );
            parent_of.push_back( parent );
            const int here = index_of[ &e ];
            for ( auto& child : e.m_children )
                self( self, *child, here );
        };
        if ( tree )
            walk( walk, *tree, -1 );
        if ( static_cast< int >( order.size( ) ) != want.m_count )
        {
            LOG_ERROR( "script graph: %s has %d entities, retail %d", want.m_asset, static_cast< int >( order.size( ) ), want.m_count );
            ++failed;
            continue;
        }
        const auto index = [ & ]( const entity_t* e ) { const auto it = index_of.find( e ); return it != index_of.end( ) ? it->second : -1; };
        for ( int k = 0; k < want.m_count; ++k )
        {
            const golden_script_entity_t& g = g_golden_script_entities[ want.m_first + k ];
            entity_t& e = *order[ k ];
            ++entities;
            const script_component_t* sc = script_of( e );
            bool same = e.m_name == g.m_name && parent_of[ k ] == g.m_parent && ( sc != nullptr ) == ( g.m_script != 0 );
            if ( same && sc )
            {
                same = static_cast< int >( sc->m_plugs.size( ) ) == g.m_plugs && static_cast< int >( sc->m_refs.size( ) ) == g.m_refs &&
                       static_cast< int >( sc->m_referrers.size( ) ) == g.m_referrers;
                for ( int i = 0; same && i < g.m_plugs; ++i )
                {
                    const golden_script_plug_t& gp = g_golden_script_plugs[ g.m_first_plug + i ];
                    const script_plug_t& ours = sc->m_plugs[ i ];
                    same = ours.m_row->m_name == std::string( gp.m_name ) && static_cast< int >( ours.m_links.size( ) ) == gp.m_count;
                    for ( int l = 0; same && l < gp.m_count; ++l )
                    {
                        const golden_script_end_t& gl = g_golden_script_links[ gp.m_first + l ];
                        const script_link_t& link = ours.m_links[ l ];
                        same = index( link.m_entity ) == gl.m_entity && script_of( *link.m_entity )->m_plugs[ link.m_plug ].m_row->m_name == std::string( gl.m_name );
                        ++links;
                    }
                }
                for ( int i = 0; same && i < g.m_refs; ++i )
                {
                    const golden_script_ref_t& gr = g_golden_script_refs[ g.m_first_ref + i ];
                    same = sc->m_refs[ i ].m_row->m_name == std::string( gr.m_name ) && index( sc->m_refs[ i ].m_target ) == gr.m_target;
                    bound += gr.m_target >= 0;
                }
                for ( int i = 0; same && i < g.m_referrers; ++i )
                {
                    const golden_script_end_t& gr = g_golden_script_referrers[ g.m_first_referrer + i ];
                    const script_referrer_t& r = sc->m_referrers[ i ];
                    same = index( r.m_entity ) == gr.m_entity && script_of( *r.m_entity )->m_refs[ r.m_ref ].m_row->m_name == std::string( gr.m_name );
                }
            }
            if ( !same && ++failed <= 8 )
                LOG_ERROR( "script graph: %s entity %d '%s' differs from retail ('%s', parent %d, script %d)", want.m_asset, k, e.m_name.c_str( ),
                    g.m_name, g.m_parent, g.m_script );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuScriptComponent's loads (0x503D70 / 0x504320: connections 0x502430, refs 0x542830, ref connections 0x5049E0) match "
                     "retail on %d entities of %d projects: %d link entries (each connection on both ends) and %d bound refs, in order", entities,
            static_cast< int >( g_golden_script_projects_count ), links, bound );
    else
        LOG_ERROR( "script graph FAILED on %d entities", failed );
    return failed == 0;
}

// animated_model_select against Vu3dDrawAnimatedModelComponent's draw 0x61DB60
// (oracle case animated_model_draw): whether the instance is drawn and the
// colour / lighting written first, one failure per vector.
inline bool verify_animated_model_draw( )
{
    int failed = 0;
    int drawn = 0;
    for ( std::size_t k = 0; k < g_golden_animated_draws_count; ++k )
    {
        const golden_animated_draw_t& g = g_golden_animated_draws[ k ];
        model_draw_t c;
        model_draw_construct( c, true );
        c.color = g.m_color;
        c.draw_distance = draw_bits( g.m_draw );
        c.dynamic_lighting = static_cast< std::uint8_t >( g.m_dynamic );
        float lo[ 3 ], hi[ 3 ], eye[ 3 ];
        for ( int a = 0; a < 3; ++a )
        {
            lo[ a ] = draw_bits( g.m_min[ a ] );
            hi[ a ] = draw_bits( g.m_max[ a ] );
            eye[ a ] = draw_bits( g.m_eye[ a ] );
        }
        const animated_draw_t ours = animated_model_select( c, lo, hi, eye, g.m_skip != 0 );
        const bool same = ours.drawn == ( g.m_drawn != 0 ) && ( !ours.drawn || ( ours.color == g.m_wf8 && ours.dynamic_lighting == g.m_bfc ) );
        drawn += g.m_drawn;
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "animated model draw: vector %d drawn %d (0x%08X %d), retail %d (0x%08X %d)", static_cast< int >( k ), ours.drawn ? 1 : 0,
                ours.color, ours.dynamic_lighting, g.m_drawn, g.m_wf8, g.m_bfc );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "animated_model_select matches Vu3dDrawAnimatedModelComponent's draw 0x61DB60 on %d vectors (%d drawn): drawn, colour "
                     "and lighting bit for bit", static_cast< int >( g_golden_animated_draws_count ), drawn );
    else
        LOG_ERROR( "animated_model_select FAILED %d vectors", failed );
    return failed == 0;
}

// The digests case_static_scene_load takes of retail's VuGfxStaticScene (FNV-1a 32 over little-endian words and
// NUL-ended strings), built here from hydro's scene_t in the same order.
struct scene_digest_t
{
    std::uint32_t h = 0x811C9DC5u;
    void bytes( const void* data, std::size_t count )
    {
        const std::uint8_t* p = static_cast< const std::uint8_t* >( data );
        for ( std::size_t k = 0; k < count; ++k )
            h = ( h ^ p[ k ] ) * 0x01000193u;
    }
    void word( std::uint32_t w ) { bytes( &w, 4 ); }
    void floats( const float* f, int count ) { bytes( f, 4 * static_cast< std::size_t >( count ) ); }
    void text( const std::string& s ) { bytes( s.c_str( ), s.size( ) + 1 ); }
};

// Base.apf next to the exe or up to eight folders above it (c_world::pack_path's walk).
inline std::string world_check_pack( )
{
#if defined( _WIN32 )
    char module_path[ MAX_PATH ]{};
    GetModuleFileNameA( nullptr, module_path, MAX_PATH );
    std::string directory = module_path;
    for ( int step = 0; step < 9; ++step )
    {
        const auto slash = directory.find_last_of( "\\/" );
        if ( slash == std::string::npos )
            break;
        directory.resize( slash );
        for ( const char* leaf : { "\\HydroThunderUWP\\Base.apf", "\\Base.apf" } )
            if ( GetFileAttributesA( ( directory + leaf ).c_str( ) ) != INVALID_FILE_ATTRIBUTES )
                return directory + leaf;
    }
#endif
    return {};
}

// scene_load_static (hydro assets/scene.hxx) against retail's VuGfxStaticScene load 0x560C70 (oracle case
// static_scene_load: VuStaticModelAsset's load 0x542C30 run on real blobs). Each blob is read from Base.apf here;
// compared: the nine totals, the scene aabb (min / max x y z), and digests of the shaders (index, flag, the six
// bytes, mode, declaration, the three material tables), the D3D11 stages retail created (its stage cache: a key
// already made is not made again), the meshes + parts, the chunks + their buffer bytes, and the node tree (name,
// matrix, mesh, children, aabb). One failure per asset.
inline bool verify_static_scene_load( )
{
    c_archive archive;
    const std::string pack = world_check_pack( );
    if ( pack.empty( ) || !archive.open( pack.c_str( ) ) )
    {
        LOG_ERROR( "verify_static_scene_load: Base.apf not found" );
        return false;
    }
    int failed = 0;
    for ( std::size_t k = 0; k < g_golden_static_scenes_count; ++k )
    {
        const golden_static_scene_t& want = g_golden_static_scenes[ k ];
        std::vector< std::uint8_t > blob;
        scene_t scene;
        const std::string name = std::string( "VuStaticModelAsset/" ) + want.m_asset;
        if ( !archive.read( name.c_str( ), blob ) || !scene_load_static( blob.data( ), blob.size( ), scene ) )
        {
            LOG_ERROR( "static scene '%s': scene_load_static failed", want.m_asset );
            ++failed;
            continue;
        }
        const std::uint32_t totals[ 9 ] = { scene.mesh_count, scene.part_count, scene.vertex_count, scene.triangle_count,
            scene.shader_count, scene.node_total, scene.instance_total, scene.vertex_total, scene.triangle_total };
        std::uint32_t aabb[ 6 ];
        std::memcpy( aabb, scene.aabb, 12 );
        std::memcpy( aabb + 3, scene.aabb + 4, 12 );
        scene_digest_t shaders, stages, meshes, chunks, tree;
        std::vector< std::uint32_t > made;
        for ( const scene_shader_t& s : scene.shaders )
        {
            shaders.word( s.index );
            shaders.word( s.flag );
            shaders.bytes( s.bytes, 6 );
            shaders.word( s.mode );
            shaders.bytes( s.elements.data( ), 16 * s.elements.size( ) );
            for ( const scene_table_t& t : s.tables )
            {
                shaders.word( static_cast< std::uint32_t >( t.constants.size( ) ) );
                for ( const scene_constant_t& c : t.constants )
                {
                    shaders.text( c.name );
                    shaders.word( c.type );
                    for ( std::uint32_t w : c.value )
                        shaders.word( w );
                }
                shaders.word( static_cast< std::uint32_t >( t.textures.size( ) ) );
                for ( const scene_texture_t& x : t.textures )
                {
                    shaders.text( x.name );
                    shaders.text( x.slot );
                    shaders.word( x.value );
                    shaders.text( x.asset );
                }
            }
            const scene_stage_t* both[ 2 ] = { &s.program.vertex, &s.program.pixel };
            for ( std::uint32_t type = 0; type < 2; ++type )
            {
                const scene_stage_t& st = *both[ type ];
                if ( std::find( made.begin( ), made.end( ), st.key ) != made.end( ) )
                    continue;
                made.push_back( st.key );
                scene_digest_t code;
                code.bytes( blob.data( ) + st.bytecode.offset, st.bytecode.size );
                stages.word( type );
                stages.word( st.bytecode.size );
                stages.word( code.h );
                stages.word( st.constants );
            }
        }
        for ( const scene_mesh_t& m : scene.meshes )
        {
            meshes.text( m.name );
            meshes.word( static_cast< std::uint32_t >( m.parts.size( ) ) );
            meshes.floats( m.aabb, 8 );
            for ( const scene_part_t& p : m.parts )
            {
                for ( std::uint32_t w : { p.chunk, p.shader, p.first_vertex, p.last_vertex, p.vertices, p.first_index, p.triangles } )
                    meshes.word( w );
                meshes.floats( p.aabb, 8 );
            }
        }
        for ( const scene_chunk_t& c : scene.chunks )
        {
            chunks.word( c.stride );
            chunks.word( c.vertices.size );
            chunks.word( c.indices );
            chunks.bytes( blob.data( ) + c.vertices.offset, c.vertices.size );
            chunks.bytes( blob.data( ) + c.index_bytes.offset, c.index_bytes.size );
        }
        for ( const scene_node_t& n : scene.nodes )
        {
            tree.text( n.name );
            tree.floats( n.matrix, 16 );
            tree.word( n.mesh >= 0 ? static_cast< std::uint32_t >( n.mesh ) : 0xFFFFFFFFu );
            tree.word( static_cast< std::uint32_t >( n.children.size( ) ) );
            tree.floats( n.aabb, 3 );
            tree.floats( n.aabb + 4, 3 );
        }
        const int counts[ 4 ] = { static_cast< int >( scene.shaders.size( ) ), static_cast< int >( scene.meshes.size( ) ),
            static_cast< int >( scene.chunks.size( ) ), static_cast< int >( made.size( ) ) };
        const char* what = nullptr;
        if ( std::memcmp( totals, want.m_totals, sizeof( totals ) ) != 0 )
            what = "totals";
        else if ( std::memcmp( counts, want.m_counts, sizeof( counts ) ) != 0 )
            what = "counts";
        else if ( std::memcmp( aabb, want.m_aabb, sizeof( aabb ) ) != 0 )
            what = "scene aabb";
        else if ( shaders.h != want.m_shaders )
            what = "shaders";
        else if ( stages.h != want.m_stages )
            what = "stages";
        else if ( meshes.h != want.m_meshes )
            what = "meshes";
        else if ( chunks.h != want.m_chunks )
            what = "chunks";
        else if ( tree.h != want.m_tree )
            what = "node tree";
        if ( what != nullptr && ++failed <= 8 )
            LOG_ERROR( "static scene '%s': %s differ from retail", want.m_asset, what );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "scene_load_static matches VuGfxStaticScene's load (0x542C30 -> 0x560C70 / 0x5C1560 / 0x5C24D0 / 0x5610F0) "
                     "on %d real VuStaticModelAssets: totals, scene aabb, shaders, stages, meshes, chunks, node tree",
            static_cast< int >( g_golden_static_scenes_count ) );
    else
        LOG_ERROR( "scene_load_static FAILED on %d of %d assets", failed, static_cast< int >( g_golden_static_scenes_count ) );
    return failed == 0;
}

inline bool verify_world_all( )
{
    bool ok = true;
    ok = verify_static_scene_load( ) && ok;
    ok = verify_animated_model_draw( ) && ok;
    ok = verify_script_graph( ) && ok;
    ok = verify_prop_draw_matrix( ) && ok;
    ok = verify_light_rig( ) && ok;
    ok = verify_gfx_settings( ) && ok;
    ok = verify_triggers( ) && ok;
    ok = verify_static_pfx( ) && ok;
    ok = verify_actions( ) && ok;
    ok = verify_trigger_plane( ) && ok;
    ok = verify_static_model_draw( ) && ok;
    return ok;
}
