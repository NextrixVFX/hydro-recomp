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
    world.m_components_for = entity_components_scripted;
    world.m_component_load = entity_load_component;
    world.m_component_post = entity_post_load_component;
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
    std::map< std::string, std::string > fresh_types;
    std::map< std::string, std::array< float, 16 > > fresh_worlds;
    std::map< std::string, int > fresh_census;
    // THE EVENT GRAPH, as the script components' loads resolved it (engine
    // entity/script.hxx, verify_script_graph): every plug's +0x50 list in link
    // order - outputs and inputs alike, as retail holds them - and every bound
    // ref's target, each end named by its path. A connection or ref that did
    // not resolve is not in the tree, so it is not here either.
    std::map< const entity_t*, std::string > path_of;
    const auto name_all = [ & ]( const auto& self, entity_t& e, const std::string& trail ) -> void
    {
        path_of[ &e ] = trail;
        for ( auto& child : e.m_children )
            self( self, *child, trail.empty( ) ? child->m_name : trail + "/" + child->m_name );
    };
    name_all( name_all, *m_tree, std::string( ) );
    std::map< std::pair< std::string, std::string >, std::vector< edge_t > > fresh_graph;
    std::map< std::pair< std::string, std::string >, std::string > fresh_refs;
    int graph_links = 0;
    const auto walk = [ & ]( const auto& self, entity_t& e, const std::string& trail ) -> void
    {
        if ( const script_component_t* sc = script_of( e ); sc && !trail.empty( ) )
        {
            for ( const script_plug_t& plug : sc->m_plugs )
            {
                if ( plug.m_links.empty( ) )
                    continue;
                std::vector< edge_t >& edges = fresh_graph[ std::make_pair( trail, std::string( plug.m_row->m_name ) ) ];
                for ( const script_link_t& link : plug.m_links )
                    edges.push_back( edge_t{ path_of[ link.m_entity ], script_of( *link.m_entity )->m_plugs[ link.m_plug ].m_row->m_name } );
                graph_links += static_cast< int >( plug.m_links.size( ) );
            }
            for ( const script_ref_t& ref : sc->m_refs )
                if ( ref.m_target )
                    fresh_refs[ std::make_pair( trail, std::string( ref.m_row->m_name ) ) ] = path_of[ ref.m_target ];
        }
        // The class behind each path (what the graph audit and ref_target read)
        // and the per-class census, from the tree.
        if ( !trail.empty( ) )
        {
            fresh_types[ trail ] = e.m_type;
            ++fresh_census[ e.m_type ];
            const transform_component_t* t = entity_transform( e );
            if ( t && !e.m_world_unknown )
            {
                std::array< float, 16 > w{};
                std::memcpy( w.data( ), t->world, sizeof( t->world ) );
                fresh_worlds[ trail ] = w;
            }
        }
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
        // component's links - its "PlayAnimation" output's ends, and the first
        // end of its "Trigger" input.
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
            if ( const script_component_t* sc = script_of( e ) )
            {
                const int trigger = script_find_plug( *sc, "Trigger" );
                const int drives = script_find_plug( *sc, "PlayAnimation" );
                if ( trigger >= 0 && !sc->m_plugs[ trigger ].m_links.empty( ) )
                    play.trigger = path_of[ sc->m_plugs[ trigger ].m_links.front( ).m_entity ];
                if ( drives >= 0 )
                    for ( const script_link_t& link : sc->m_plugs[ drives ].m_links )
                        fresh_play.emplace( path_of[ link.m_entity ], play );
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
        // VuWaterTextureSettingsEntity (ctor 0x5D2BD0): its loaded floats into
        // the ocean's detail field - the consumer c_ocean is the water area's;
        // only the property source moved. Complexity is not one of the
        // entity's properties (the detail field is built at a fixed 6), and
        // TimeFactor is loaded and not consumed.
        if ( const auto* p = entity_action< water_texture_properties_t >( e ); p && e.m_type == "VuWaterTextureSettingsEntity" )
        {
            m_wave_height = p->height_factor;
            m_wave_scale = p->normal_texture_scale;
            m_wave_wind = p->wind_speed;
            m_wave_size = p->world_size;
            m_ocean.add( 6.0f, m_wave_size, 0.0f, m_wave_wind, p->directional_power, p->suppression_wave_length, m_wave_height, p->gravity,
                nullptr, nullptr, 1.0f, 1.0f, true );
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
                std::memcpy( gate.frame, t->world, sizeof( gate.frame ) );
                for ( int axis = 0; axis < 3; ++axis )
                    gate.half[ axis ] = t->scale[ axis ];
                gate.mask = trigger_type_mask( p.trigger_type );
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
            // A static component naming only LOD 1 / LOD 2 still places those (its LOD 0 holds no scene).
            if ( m.model_asset.empty( ) && ( m.animated || ( m.lod1_model_asset.empty( ) && m.lod2_model_asset.empty( ) ) ) )
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
                // The Model Asset watcher's rebuild 0x5B09E0: the LOD 0 instance (+0xBC) takes the Model Asset - or
                // the LOD 1 asset when g_vumetrod3d11gfx's vt+0x1C (0x5135E0: its +4 byte) is set and LOD 1 is
                // named; +4 is 0 from the ctor 0x5AC8C0 and its setter vt+0x18 (0x5135D0) has no caller found, so
                // the Model Asset - then LOD 1 (+0xDC) and LOD 2 (+0xFC). Every instance shares the draw matrix,
                // so each is collected for the rotators. An instance naming nothing holds no scene and draws
                // nothing (0x520130: +0x10 null -> return).
                static_draw_item_t item{};
                item.draw = m;
                const std::string* names[ 3 ] = { &m.model_asset, &m.lod1_model_asset, &m.lod2_model_asset };
                m_collect = &collected;
                m_place_owner = trail;
                for ( int instance = 0; instance < 3; ++instance )
                    if ( !names[ instance ]->empty( ) )
                        place_model( *names[ instance ], matrix, 0, -1, tint, nullptr, -1, &item.placements[ instance ] );
                m_collect = nullptr;
                // 0x531D10 with the LOD 0 instance's scene aabb (its +0x10 scene's +0x5C), or the zero aabb
                // 0x7593F0 when it holds no scene; +0x1E's draw-manager hand-off is the renderer's (a hook).
                const auto box = m_scene_aabbs.find( m.model_asset );
                scene_component_aabb( item.aabb, box != m_scene_aabbs.end( ) ? box->second.data( ) : scene_zero_aabb( ), matrix, false );
                for ( int instance = 0; instance < 3; ++instance )
                    for ( int handle : item.placements[ instance ] )
                        c_engine::show( handle, false );
                m_static_draws.push_back( std::move( item ) );
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
    m_graph = std::move( fresh_graph );
    m_refs = std::move( fresh_refs );
    LOG_INFO( "event graph: %u linked plugs, %d link entries (each connection on both ends), %u bound refs - from the tree",
        static_cast< unsigned >( m_graph.size( ) ), graph_links, static_cast< unsigned >( m_refs.size( ) ) );
    m_seat_world = std::move( fresh_worlds );
    m_entity_type = std::move( fresh_types );
    m_type_census = std::move( fresh_census );
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
    // VuWater, built from the tree: each water entity's slot 6 in VuEntity::
    // gameInitialize 0x500B10's order (children first), from the project root.
    // ASSUMED, not read: that the race calls 0x500B10 on the project root -
    // which caller does is unread. Point waves are made later by their tick.
    // An entity it cannot build is counted, never stood in for.
    m_water = water_built_t{ };
    m_wave_owner.clear( );
    m_water_missing = 0;
    water_build_world( *m_tree, m_water, []( entity_t& e ) { return entity_transform( e ); } );
    LOG_INFO( "water: %u surfaces, %u waves built from the tree; %d water entities not built", static_cast< unsigned >( m_water.surfaces.size( ) ),
        static_cast< unsigned >( m_water.waves.size( ) ), m_water.skipped );
    water_view_build( m_water, m_water_view );
    // The point waves' descriptors (VuPointWaveEntity_tick 0x5D4C50: transform +0x80, then the entity's seven
    // properties), emitted when the graph reaches their Emit plug. An entity without a known world is skipped.
    m_point_waves.clear( );
    for ( const auto& named : path_of )
    {
        entity_t& e = *const_cast< entity_t* >( named.first );
        const point_wave_properties_t* p = entity_action< point_wave_properties_t >( e );
        const transform_component_t* t = e.m_type == "VuPointWaveEntity" ? entity_transform( e ) : nullptr;
        if ( !p || !t || e.m_world_unknown )
            continue;
        point_wave_source_t source{};
        std::memcpy( source.position, t->world + 12, 16 );
        std::memcpy( source.properties, p->word, 28 );
        m_point_waves.emplace( named.second, source );
    }
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
