#pragma once

// c_world's entities - member definitions moved out of world.hxx (one class, split by concern).

// A VuTemplateAsset's whole document, what factory slot 7 0x555690 loads,
// or null when the pack has none of that name.
inline const value_t* c_world::template_document( const std::string& name )
{
    template_root( name );
    const auto found = m_templates.find( name );
    return found != m_templates.end( ) && found->second.tag == 6 ? &found->second : nullptr;
}

// The project as retail loads it (the race game 0x4B4360 -> VuProject::load
// 0x556390): the registry as start-up leaves it, the asset's AssetData
// "File" path (0x55FBE0), and the entity tree through every recreated load.
// The coronas are fed from it (each VuCoronaEntity's transform world and its
// property targets), and so are the props: every model-draw component's
// Model Asset and Color at its owner's draw matrix. Its Draw Distance is
// carried on the animated props; no draw distance or LOD switch (the
// static draw 0x5B0730's +0xA8 / +0xAC / +0xB0 tests) is applied by the
// renderer yet - a gap, not a choice. Anything below an UNKNOWN entity is
// never created, and anything whose world is unknown is not placed - both
// counted, neither stood in for. The rotators still come from visit() (the
// old walk) and adopt the tree's placements here.
inline void c_world::load_entity_tree( const char* asset, const value_t& project )
{
    std::vector< std::uint8_t > bytes;
    const std::string prefix = "VuProjectAsset/";
    std::string path;
    if ( std::strncmp( asset, prefix.c_str( ), prefix.size( ) ) == 0 && m_archive.read( "Assets/AssetData", bytes ) )
    {
        const value_t database = c_reader( bytes ).parse( );
        const value_t& file = member( member( member( database, "VuProjectAsset" ), asset + prefix.size( ) ), "File" );
        if ( file.tag == 4 )
            path = file.text;
    }
    if ( path.empty( ) )
    {
        LOG_ERROR( "entity tree: %s has no AssetData \"File\" - the project is not loaded the retail way", asset );
        return;
    }
    const entity_registry_t registry = build_entity_registry( m_archive.names( "VuTemplateAsset/" ) );
    entity_world_t world;
    world.m_registry = &registry;
    world.m_ctor_for = entity_ctor_for;
    world.m_template = [ this ]( const std::string& name ) { return template_document( name ); };
    world.m_components_for = entity_components_constructed;
    world.m_component_load = entity_load_component;
    world.m_component_mark = entity_mark_component;
    world.m_state_for = entity_state_for;
    world.m_properties_load = entity_load_properties;
    world.m_properties_mark = entity_mark_properties;
    m_tree = entity_load_project( project, path, world );
    if ( !m_tree )
    {
        LOG_ERROR( "entity tree: VuProject::load failed on %s (%s)", asset, path.c_str( ) );
        return;
    }
    int entities = 0;
    int unknown = 0;
    int unplaced = 0;
    int placed = 0;
    std::map< std::string, int > unknown_types;
    int models = 0;
    int models_unplaced = 0;
    // Every static placement, by owner, for the rotators (adopt_rotators).
    std::vector< collected_t > collected;
    std::vector< trigger_t > fresh_triggers;
    int pfx_placed = 0;
    int pfx_unplaced = 0;
    int triggers_unplaced = 0;
    // The logic / action / motion / settings consumers (engine entity/actions.hxx).
    std::vector< rotator_t > fresh_rotators;
    std::map< const entity_t*, int > rotator_of;
    std::vector< std::pair< int, std::string > > fresh_adopters;
    int rotators_unplaced = 0;
    std::vector< attach_t > fresh_attaches;
    std::map< std::string, play_t > fresh_play;
    std::map< std::string, action_t > fresh_actions;
    std::map< std::string, delay_t > fresh_delays;
    std::map< std::string, std::string > fresh_events;
    std::map< std::string, bool > fresh_booleans;
    std::map< std::string, front_end_camera_t > fresh_front_end;
    std::map< std::string, filter_t > fresh_filters;
    std::vector< std::string > fresh_initialized;
    std::set< std::string > fresh_ifs;
    std::string fresh_sky;
    // The event graph addresses an entity by its names below the project
    // root, '/'-joined.
    const auto walk = [ & ]( const auto& self, entity_t& e, const std::string& trail ) -> void
    {
        // VuRotateMotionEntity: Axis / Speed (radians) / Local, its base the
        // transform world; its DIRECT parent adopts it (recorded after the
        // parent's subtree, below - the old walk's post-order).
        if ( const auto* p = entity_action< rotate_motion_properties_t >( e ); p && e.m_type == "VuRotateMotionEntity" )
        {
            const transform_component_t* t = entity_transform( e );
            if ( !t || e.m_world_unknown )
                ++rotators_unplaced;
            else
            {
                rotator_t motion{};
                motion.path = trail;
                std::memcpy( motion.base, t->world, sizeof( motion.base ) );
                std::memcpy( motion.axis, p->axis, sizeof( motion.axis ) );
                motion.speed = p->speed;
                motion.local = p->local != 0;
                rotator_of[ &e ] = static_cast< int >( fresh_rotators.size( ) );
                fresh_rotators.push_back( motion );
            }
        }
        // VuAttachEntity: the relative position / rotation (radians) and node
        // name; its Parent / Child refs are the script component's (m_refs).
        if ( const auto* p = entity_action< attach_properties_t >( e ); p && e.m_type == "VuAttachEntity" )
        {
            attach_t join{};
            join.path = trail;
            join.at_start = p->attach_at_start != 0;
            std::memcpy( join.position, p->relative_position, sizeof( join.position ) );
            std::memcpy( join.rotation, p->relative_rotation, sizeof( join.rotation ) );
            join.node = p->node_name;
            fresh_attaches.push_back( std::move( join ) );
        }
        // VuPlayAnimationEntity: the clip and its params from the loaded
        // targets; which props it drives and what triggers it are its script
        // component's plugs, collected by the event-graph walk (m_play_links).
        if ( const auto* p = entity_action< play_animation_properties_t >( e ); p && e.m_type == "VuPlayAnimationEntity" &&
             !p->animation_asset.empty( ) )
        {
            play_t play{};
            play.clip = p->animation_asset;
            play.timed = p->timed_event_asset;
            play.path = trail;
            play.start = p->start_time;
            play.looping = p->looping != 0;
            play.one_shot = p->one_shot != 0;
            play.factor = p->time_factor;
            const auto links = m_play_links.find( trail );
            if ( links != m_play_links.end( ) )
            {
                play.trigger = links->second.first;
                for ( const std::string& target : links->second.second )
                    fresh_play.emplace( target, play );
            }
            if ( !trail.empty( ) )
            {
                action_t act{};
                act.one_shot = play.one_shot;
                fresh_actions.emplace( trail, act );
            }
        }
        if ( const auto* p = entity_action< delay_properties_t >( e ); p && e.m_type == "VuDelayEntity" )
        {
            delay_t wait{};
            wait.delay = p->delay;
            wait.window = p->random_window;
            fresh_delays.emplace( trail, wait );
        }
        if ( const auto* p = entity_action< generic_event_properties_t >( e ); p && e.m_type == "VuGenericEventEntity" )
            fresh_events.emplace( trail, p->event_name );
        if ( const auto* p = entity_action< boolean_properties_t >( e ); p && e.m_type == "VuBooleanEntity" )
            fresh_booleans.emplace( trail, p->value != 0 );
        if ( const auto* p = entity_action< front_end_camera_properties_t >( e ); p && e.m_type == "VuSetFrontEndCameraEntity" && !trail.empty( ) )
        {
            front_end_camera_t shot{};
            shot.interpolation_time = p->interpolation_time;
            shot.modify_eye = p->modify_eye != 0;
            shot.modify_target = p->modify_target != 0;
            shot.modify_far = p->modify_far != 0;
            std::memcpy( shot.eye, p->eye, sizeof( shot.eye ) );
            std::memcpy( shot.target, p->target, sizeof( shot.target ) );
            shot.far_plane = p->far_plane;
            fresh_front_end.emplace( trail, shot );
        }
        if ( const auto* p = entity_action< sky_box_properties_t >( e ); p && e.m_type == "VuSkyBoxEntity" && !p->model_asset.empty( ) )
            fresh_sky = p->model_asset;
        // The three graph nodes that carry no property: their path is the state.
        if ( e.m_ctor && e.m_type == "VuOneShotFilterEntity" )
            fresh_filters.emplace( trail, filter_t{ } );
        if ( e.m_ctor && e.m_type == "VuOnProjectInitializedEntity" )
            fresh_initialized.push_back( trail );
        if ( e.m_ctor && e.m_type == "VuIfEntity" )
            fresh_ifs.insert( trail );
        // VuTriggerPlaneEntity / VuTriggerBoxEntity (engine entity/trigger.hxx):
        // the volume is the transform - VuTriggerPlaneEntity_bounds 0x5D18B0
        // composes VuMatrix_multiply(fromScale(scale +0xB0), out, world
        // +0x50), the same product as transform_draw_matrix, and the crossing
        // test 0x5D1570 compares against the Scale (+0xB0) itself as the half
        // extent. Initially Active and Trigger Type are the loaded targets.
        if ( entity_is_trigger( e.m_type ) && e.m_state )
        {
            const transform_component_t* t = entity_transform( e );
            if ( !t || e.m_world_unknown )
                ++triggers_unplaced;
            else
            {
                const trigger_properties_t& p = static_cast< const trigger_state_t* >( e.m_state.get( ) )->value;
                trigger_t gate{};
                gate.box = e.m_type == "VuTriggerBoxEntity";
                gate.path = trail;
                transform_draw_matrix( *t, gate.world );
                for ( int axis = 0; axis < 3; ++axis )
                    gate.scale[ axis ] = t->scale[ axis ];
                if ( !model_invert( gate.inverse, gate.world ) )
                    gate.inverse[ 0 ] = 0.0f;
                gate.kind = p.trigger_type;
                gate.active = p.initially_active != 0;
                fresh_triggers.push_back( std::move( gate ) );
            }
        }
        // VuStaticPfxEntity (engine entity/pfx.hxx): what its onGameInitialize
        // 0x5EB060 hands the VuPfxSystem the pfx manager (0x75904C vt+0x28)
        // creates from its Effect Name - setScale(Pfx Scale), setColor
        // (static_pfx_color), setMatrix(the transform WORLD: scale kept out,
        // as its transform watcher 0x5EB4A0 hands it too) and start() iff
        // Initially Active. The manager's lookup is the platform side: an
        // empty name is skipped here (what retail's manager returns for it
        // is unread), and c_pfx stands in for the system.
        if ( e.m_type == "VuStaticPfxEntity" && e.m_state )
        {
            const static_pfx_t& p = static_cast< const static_pfx_state_t* >( e.m_state.get( ) )->value;
            const transform_component_t* t = entity_transform( e );
            if ( !t || e.m_world_unknown )
                ++pfx_unplaced;
            else if ( !p.effect_name.empty( ) )
            {
                float color[ 4 ];
                static_pfx_color( p, color );
                m_pfx.add( m_archive, p.effect_name, t->world, p.pfx_scale, color, p.initially_active != 0 );
                ++pfx_placed;
            }
        }
        // VuGlobalGfxSettingsEntity (engine entity/gfx.hxx): the far plane is
        // its Camera Far Plane - neither VuBoatCamera_update nor the front-end
        // camera sets one - and the fog its Fog Start / End and Fog Color's R,
        // G, B bytes / 255. The other 22 values are loaded and not consumed
        // yet (Contrast / Tint feed PostProcess/Effect and the Water * ones the
        // water shader's constants - memory water.md - neither wired), and
        // how retail applies the settings (Always Active, the ramp times, the
        // Rectangular / Elliptical volumes that blend over it) is unread: the
        // last one in tree order is applied at load, ours and flagged.
        if ( e.m_type == "VuGlobalGfxSettingsEntity" && e.m_state )
        {
            const gfx_settings_t& g = static_cast< const gfx_settings_state_t* >( e.m_state.get( ) )->value;
            m_authored_far = g.camera_far_plane;
            const float red = static_cast< float >( g.fog_color & 0xFF ) / 255.0f;
            const float green = static_cast< float >( ( g.fog_color >> 8 ) & 0xFF ) / 255.0f;
            const float blue = static_cast< float >( ( g.fog_color >> 16 ) & 0xFF ) / 255.0f;
            LOG_VERBOSE( "gfx settings %s: far %.0f  fog %.0f..%.0f  colour %.3f %.3f %.3f", trail.c_str( ), g.camera_far_plane,
                g.fog_start, g.fog_end, red, green, blue );
            // Hydroelectric authors Fog End 1800 and no Camera Far Plane, so
            // the far plane is the ctor's 500 and geometry clips long before
            // the fog reaches full density - one of the two comes from
            // somewhere not yet found. Reported, not reconciled.
            if ( g.fog_end > m_authored_far )
                LOG_WARNING( "gfx settings: Fog End %.0f is beyond the far plane %.0f - the far plane's real source has not been found",
                    g.fog_end, m_authored_far );
            c_engine::fog( g.fog_start, g.fog_end, red, green, blue );
        }
        // The rig: the hull shader's gAmbLightColor from VuAmbientLightEntity's
        // Color, gDirLightFrontColor / gDirLightBackColor from
        // VuDirectionalLightEntity's (the shader's light is TWO-SIDED), each
        // VuColor's R, G, B bytes / 255. The light carries no direction
        // property: gDirLightWorld is its transform world, and the sun
        // direction handed to the renderer is that world's -Z row - WHICH
        // axis retail uses is not reversed (the light object at +0x5C and
        // its draw are unread), so that choice is ours and flagged. Its
        // Specular Color is loaded and not consumed. With several lights
        // of a kind the last in tree order wins - retail's light manager,
        // which picks, is unread.
        if ( e.m_type == "VuAmbientLightEntity" && e.m_state )
        {
            const std::uint32_t c = static_cast< const ambient_light_state_t* >( e.m_state.get( ) )->value.color;
            for ( int k = 0; k < 3; ++k )
                m_rig.ambient( )[ k ] = static_cast< float >( ( c >> ( 8 * k ) ) & 0xFF ) / 255.0f;
            m_rig.mark_lit( );
        }
        if ( e.m_type == "VuDirectionalLightEntity" && e.m_state )
        {
            const directional_light_t& l = static_cast< const directional_light_state_t* >( e.m_state.get( ) )->value;
            for ( int k = 0; k < 3; ++k )
            {
                m_rig.front( )[ k ] = static_cast< float >( ( l.front_color >> ( 8 * k ) ) & 0xFF ) / 255.0f;
                m_rig.back( )[ k ] = static_cast< float >( ( l.back_color >> ( 8 * k ) ) & 0xFF ) / 255.0f;
            }
            const transform_component_t* t = entity_transform( e );
            if ( t && !e.m_world_unknown )
            {
                std::memcpy( m_rig.sun_world( ), t->world, sizeof( t->world ) );
                const float direction[ 3 ] = { -t->world[ 8 ], -t->world[ 9 ], -t->world[ 10 ] };
                c_engine::sun( direction );
                m_rig.mark_sun( );
            }
        }
        // The model-draw components, drawn at the matrix their owner leaves
        // in them (transform_draw_matrix: fromScale x world). An empty
        // Model Asset draws nothing.
        for ( const entity_component_t& c : e.m_components )
        {
            if ( ( c.m_vtable != k_static_model_component_vtable && c.m_vtable != k_animated_model_component_vtable ) || !c.m_state )
                continue;
            const model_draw_t& m = static_cast< const model_draw_state_t* >( c.m_state.get( ) )->value;
            if ( m.model_asset.empty( ) )
                continue;
            const transform_component_t* t = entity_transform( e );
            if ( !t || e.m_world_unknown )
            {
                ++models_unplaced;
                continue;
            }
            ++models;
            float matrix[ 16 ];
            transform_draw_matrix( *t, matrix );
            // The VuColor's R, G, B bytes.
            const float tint[ 3 ] = { static_cast< float >( m.color & 0xFF ) / 255.0f,
                static_cast< float >( ( m.color >> 8 ) & 0xFF ) / 255.0f, static_cast< float >( ( m.color >> 16 ) & 0xFF ) / 255.0f };
            if ( !m.animated )
            {
                m_collect = &collected;
                m_place_owner = trail;
                place_model( m.model_asset, matrix, 0, -1, tint );
                m_collect = nullptr;
                continue;
            }
            // The clip arrives from a VuPlayAnimationEntity; bind_animations
            // joins them.
            animated_t item{};
            item.asset = m.model_asset;
            item.path = trail;
            std::memcpy( item.world, matrix, sizeof( item.world ) );
            std::memcpy( item.tint, tint, sizeof( item.tint ) );
            item.distance = m.draw_distance;
            m_animated.push_back( std::move( item ) );
        }
        ++entities;
        unknown += e.m_ctor ? 0 : 1;
        if ( !e.m_ctor )
            ++unknown_types[ e.m_type ];
        if ( e.m_type == "VuCoronaEntity" && e.m_state )
        {
            const transform_component_t* t = entity_transform( e );
            if ( !t || e.m_world_unknown )
                ++unplaced;
            else
            {
                m_coronas.add( corona_from( trail, *t, static_cast< const corona_state_t* >( e.m_state.get( ) )->value ) );
                ++placed;
            }
        }
        for ( auto& child : e.m_children )
            self( self, *child, trail.empty( ) ? child->m_name : trail + "/" + child->m_name );
        // After the subtree, as the old walk recorded it: this entity adopts each
        // motion entity that is its DIRECT child.
        for ( auto& child : e.m_children )
        {
            const auto it = rotator_of.find( child.get( ) );
            if ( it != rotator_of.end( ) )
                fresh_adopters.emplace_back( it->second, trail );
        }
    };
    walk( walk, *m_tree, std::string( ) );
    m_rotators = std::move( fresh_rotators );
    m_adopters = std::move( fresh_adopters );
    m_attaches = std::move( fresh_attaches );
    m_play = std::move( fresh_play );
    m_actions = std::move( fresh_actions );
    m_delays = std::move( fresh_delays );
    m_events = std::move( fresh_events );
    m_booleans = std::move( fresh_booleans );
    m_front_end = std::move( fresh_front_end );
    m_filters = std::move( fresh_filters );
    m_initialized = std::move( fresh_initialized );
    m_ifs = std::move( fresh_ifs );
    if ( !fresh_sky.empty( ) )
        m_sky = fresh_sky;
    adopt_rotators( std::move( collected ) );
    for ( trigger_t& gate : fresh_triggers )
        m_triggers.add( std::move( gate ) );
    LOG_INFO( "entity tree: %s (%s) - %d entities, %d UNKNOWN (nothing below one is loaded); %d coronas placed, %d with an "
              "unknown world; %d models placed, %d with an unknown world; %d triggers, %d with an unknown world; %d static pfx placed, %d "
              "with an unknown world", asset, path.c_str( ), entities, unknown, placed, unplaced, models, models_unplaced,
        static_cast< int >( m_triggers.size( ) ), triggers_unplaced, pfx_placed, pfx_unplaced );
    // Which types' construction is not captured - what to recreate next.
    for ( const auto& pair : unknown_types )
        LOG_VERBOSE( "entity tree: UNKNOWN %-40s x%d", pair.first.c_str( ), pair.second );
}

// VuCoronaEntity's members as c_corona_entity reads them. The colour is
// the VuColor the property load packed (R, G, B, A bytes).
inline c_corona_entity::corona_t c_world::corona_from( const std::string& path, const transform_component_t& t, const corona_properties_t& c )
{
    c_corona_entity::corona_t light{};
    light.path = path;
    std::memcpy( light.world, t.world, sizeof( light.world ) );
    light.visible = c.initially_visible != 0;
    light.texture = c.texture_name;
    light.draw_distance = c.draw_distance;
    light.fade_distance = c.fade_distance;
    light.query_radius = c.query_radius;
    light.cone = c.cone;
    light.penumbra = c.penumbra;
    light.back_light = c.back_light != 0;
    for ( int k = 0; k < 4; ++k )
        light.color[ k ] = static_cast< float >( ( c.texture_color >> ( 8 * k ) ) & 0xFF ) / 255.0f;
    light.screen_space = c.screen_space != 0;
    light.size = c.texture_size;
    light.rotation_offset = c.rotation_offset;
    light.rotation_amount = c.rotation_amount;
    light.rotation_speed = c.rotation_speed;
    std::memcpy( light.axis, c.rotation_axis, sizeof( light.axis ) );
    return light;
}

inline void c_world::load_project( const char* asset )
{
    std::vector< std::uint8_t > project;
    if ( asset == nullptr || !m_archive.read( asset, project ) )
    {
        LOG_ERROR( "project %s", asset != nullptr ? asset : "" );
        return;
    }
    const value_t root = c_reader( project ).parse( );
    const value_t* project_node = find( root, "VuProject" );
    const value_t* entity = project_node != nullptr ? find( *project_node, "RootEntity" ) : nullptr;
    if ( entity == nullptr )
    {
        LOG_ERROR( "project %s has no root", asset );
        return;
    }
    float identity[ 16 ]{};
    math_identity( identity );
    visit( *entity, identity );
    load_entity_tree( asset, *project_node );
    m_rig.report( );
    for ( const auto& pair : m_type_census )
        LOG_VERBOSE( "census %-40s %d", pair.first.c_str( ), pair.second );
    // Hand the authored rig to the renderer. Until this runs the engine
    // keeps the old hardcoded lambert's constants in the rig's form, so a
    // scene with no light entities looks exactly as it did before.
    // --no-rig holds it back on purpose: it is the A/B switch that makes
    // the rig's contribution measurable against the old lambert instead
    // of merely plausible.
    if ( m_rig.lit( ) && c_engine::option( "no-rig" ) == nullptr )
        c_engine::rig( m_rig.ambient( ), m_rig.front( ), m_rig.back( ) );
    bind_animations( );
    audit_authored( );
    // The water surfaces and the sky box are counted but not built: the
    // readers that used to run here were partial and the geometry they fed
    // was invented. The count comes from the census, so the load log still
    // states exactly what the level authors and nothing draws.
    const auto water = m_type_census.find( "VuWaterSurfaceEntity" );
    LOG_SUCCESS( "project %s  draws %d  water surfaces %d (none built)  sky %s", asset, m_draws,
        water != m_type_census.end( ) ? water->second : 0,
        m_sky.empty( ) ? "-" : m_sky.c_str( ) );
}
