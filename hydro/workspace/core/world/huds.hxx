#pragma once

// c_world's race HUD projects - member definitions (one class, split by concern).
//
// VuRaceGame's ctor 0x489030 loads three projects through 0x4B4360 (the same
// VuProject load the race project takes: AssetData "File", VuProject::load
// 0x556390): +0x738 HUDs/RacePreGameHUD, +0x73C HUDs/RaceGameHUD, +0x740
// HUDs/RacePostGameHUD. Its states initialise / release / tick them (world/
// rules.hxx). Here they are loaded AS DATA - the entity trees through the
// verified loader, their event graphs merged into m_graph (each path prefixed
// "<project>:" so no HUD path can meet a level path) - and run as far as the
// world's graph runs: the PreGame HUD's #HUD/Countdown is a VuMultiDelayEntity
// (Delays 1 / 2 / 3 / 4 s) started by OnProjectInitialized, whose output "4"
// triggers VuStartLightsOutAction -> the "OnStartLightsOut" broadcast. Every
// HUD element (VuUITextEntity / VuHUDElementEntity Show / Hide, the HUD's
// VuHUDEntity tick vt+0x30 / draw vt+0x34, VuAudioEventEntity Start) is the UI
// agent's / the audio layer's: counted unhandled or logged, never drawn here.

inline void c_world::load_huds( )
{
    const char* const* k_names = k_race_hud_projects;   // the ctor 0x489030's three loads (world/rules.hxx)
    m_multi_delays.clear( );
    m_start_lights.clear( );
    for ( int p = 0; p < 3; ++p )
    {
        hud_project_t& h = m_huds[ p ];
        h = hud_project_t{ };
        h.name = k_names[ p ];
        const std::string asset = std::string( "VuProjectAsset/" ) + k_names[ p ];
        std::vector< std::uint8_t > bytes;
        if ( !m_archive.read( asset.c_str( ), bytes ) )
        {
            LOG_WARNING( "race HUD: %s not in the pack - not loaded (UNKNOWN)", asset.c_str( ) );
            continue;
        }
        const value_t root = c_reader( bytes ).parse( );
        const value_t* project = find( root, "VuProject" );
        if ( project )
            h.tree = race_load_tree( m_archive, asset.c_str( ), *project, [ this ]( const std::string& name ) { return template_document( name ); } );
        if ( !h.tree )
        {
            LOG_WARNING( "race HUD: VuProject::load failed on %s - not loaded (UNKNOWN)", asset.c_str( ) );
            continue;
        }
        const std::string prefix = h.name + ":";
        // The UI agent's driver (core/ui overlay.hxx): the same document through the UI's scripted world.
        {
            const entity_registry_t registry = build_entity_registry( m_archive.names( "VuTemplateAsset/" ) );
            h.ui_tree = ui_hud_load( *project, race_project_file( m_archive, asset.c_str( ) ), registry,
                                     [ this ]( const std::string& name ) { return template_document( name ); } );
            if ( h.ui_tree )
            {
                const auto trails = [ & ]( const auto& self, entity_t& e, const std::string& trail ) -> void
                {
                    h.ui_trail[ &e ] = trail;
                    for ( auto& child : e.m_children )
                        self( self, *child, trail.empty( ) ? child->m_name : trail + "/" + child->m_name );
                };
                trails( trails, *h.ui_tree, std::string( ) );
                ui_bind_hooks_t hooks;
                const int index = p;
                // The world-owned inputs: VuMultiDelayEntity "In" 0x5E0660 (the world's clock); VuAudioEventEntity Start /
                // Stop (the audio device - a platform hook).
                hooks.m_external = [ this, index, prefix ]( entity_t& e, const char* plug, const ui_params_t& ) {
                    hud_project_t& hp = m_huds[ index ];
                    const auto at = hp.ui_trail.find( &e );
                    const auto clock = at != hp.ui_trail.end( ) ? m_multi_delays.find( prefix + at->second ) : m_multi_delays.end( );
                    if ( clock != m_multi_delays.end( ) && std::strcmp( plug, "In" ) == 0 )
                        multi_delay_in( clock->second.state );
                    else
                        race_unwired( m_race, std::string( "the race HUD's external input " ) + e.m_type + " . " + plug + " (audio: a platform hook) - not run" );
                    return ui_ret_t{ };
                };
                hooks.m_broadcast_out = [ this ]( std::uint32_t hash, const ui_params_t& ) {
                    if ( hash == fsm_hash( "OnStartLightsOut" ) )
                        race_broadcast( "OnStartLightsOut" );
                    else
                        race_unwired( m_race, "a race HUD broadcast with no world subscriber recreated - not run" );
                };
                // m_string_db (VuStringDB slot 6, the toast's template) is not wired: no string DB here (UNKNOWN on use).
                h.ui = std::make_unique< c_ui_race_hud >( *h.ui_tree, hooks );
            }
            else
                LOG_WARNING( "race HUD: %s did not load through the UI's world - the world graph runs it (fallback)", h.name.c_str( ) );
        }
        std::map< const entity_t*, std::string > path_of;
        const auto name_all = [ & ]( const auto& self, entity_t& e, const std::string& trail ) -> void
        {
            path_of[ &e ] = trail;
            for ( auto& child : e.m_children )
                self( self, *child, trail.empty( ) ? child->m_name : trail + "/" + child->m_name );
        };
        name_all( name_all, *h.tree, std::string( ) );
        int entities = 0, unknown = 0, links = 0;
        const auto walk = [ & ]( const auto& self, entity_t& e, const std::string& trail ) -> void
        {
            const std::string at = prefix + trail;
            if ( !trail.empty( ) )
            {
                ++entities;
                unknown += e.m_ctor ? 0 : 1;
                m_entity_type[ at ] = e.m_type;
                if ( const script_component_t* sc = h.ui ? nullptr : script_of( e ) )   // with the UI driver its graph runs the HUD
                    for ( const script_plug_t& plug : sc->m_plugs )
                    {
                        if ( plug.m_links.empty( ) )
                            continue;
                        std::vector< edge_t >& edges = m_graph[ std::make_pair( at, std::string( plug.m_row->m_name ) ) ];
                        for ( const script_link_t& link : plug.m_links )
                            edges.push_back( edge_t{ prefix + path_of[ link.m_entity ], script_of( *link.m_entity )->m_plugs[ link.m_plug ].m_row->m_name } );
                        links += static_cast< int >( plug.m_links.size( ) );
                    }
                if ( e.m_ctor && e.m_type == "VuOnProjectInitializedEntity" )
                    h.initialized.push_back( at );
                if ( e.m_ctor && e.m_type == "VuStartLightsOutAction" )
                    m_start_lights.insert( at );
                if ( e.m_type == k_multi_delay_class && e.m_state )
                {
                    multi_delay_node_t& n = m_multi_delays[ at ];
                    std::memcpy( n.state.delay, static_cast< const multi_delay_state_t* >( e.m_state.get( ) )->value, sizeof( n.state.delay ) );
                    n.world = this;
                    n.path = at;
                    h.delays.push_back( at );
                }
            }
            for ( auto& child : e.m_children )
                self( self, *child, trail.empty( ) ? child->m_name : trail + "/" + child->m_name );
        };
        walk( walk, *h.tree, std::string( ) );
        LOG_INFO( "race HUD: %s loaded as data - %d entities (%d UNKNOWN types), %d graph links, %d OnProjectInitialized, %d multi delays",
                  h.name.c_str( ), entities, unknown, links, static_cast< int >( h.initialized.size( ) ), static_cast< int >( h.delays.size( ) ) );
    }
}

// VuMultiDelayEntity's Decision tick 0x5E06B0 (registered by its gameInitialize 0x5E0610).
inline void c_world::multi_delay_ticked( void* user, float dt )
{
    multi_delay_node_t& n = *static_cast< multi_delay_node_t* >( user );
    multi_delay_tick( n.state, dt, [ & ]( int output, const std::uint8_t* params ) {
        LOG_INFO( "race HUD: %s output %d fired (Delay %.2f)", n.path.c_str( ), output + 1, n.state.delay[ output ] );
        for ( hud_project_t& h : n.world->m_huds )
            if ( h.ui && n.path.compare( 0, h.name.size( ) + 1, h.name + ":" ) == 0 )
            {
                ui_params_t p;
                if ( params )
                {
                    std::memcpy( p.m_bytes, params, 128 );
                    std::memcpy( &p.m_size, params + 128, 4 );
                }
                h.ui->fire( h.ui_tree->m_name + "/" + n.path.substr( h.name.size( ) + 1 ), std::to_string( output + 1 ).c_str( ), p );
                return;
            }
        n.world->fire( n.path, std::to_string( output + 1 ), 0, params );
    } );
}

// The state handlers' HUD verbs (world/rules.hxx race_rules_hooks_t::hud).
// initialize: when the root's +0x14 bit 2 is clear (not yet live) - the
// repository add (g_vu_entity_repository vt+0x18: not recreated, nothing it
// holds is read here), the project's gameInitialize 0x500B10 (of its types,
// VuMultiDelayEntity's 0x5E0610 - its Decision tick - is recreated; the
// others' are not: the HUD elements' are the UI's), then 0x501260
// "OnProjectInitialized" (each VuOnProjectInitializedEntity's Trigger, tree
// pre-order). release: when live - gameRelease 0x500B80 (the multi delays'
// ticks unregistered: slot 7 0x43A420), the repository remove. tick: the
// VuHUDEntity root's vt+0x30 - the UI agent's (logged).
inline void c_world::hud( int project, race_hud_verb_t verb, float dt )
{
    if ( project < 0 || project > 2 || !m_huds[ project ].tree )
    {
        race_unwired( m_race, "a race HUD project that did not load (UNKNOWN) - its initialise / tick / release not run" );
        return;
    }
    hud_project_t& h = m_huds[ project ];
    if ( verb == race_hud_verb_t::initialize )
    {
        if ( h.live )
            return;
        h.live = true;
        for ( const std::string& at : h.delays )
        {
            multi_delay_node_t& n = m_multi_delays[ at ];
            if ( !n.registered && tick_register( m_race.ticks, { &n, &c_world::multi_delay_ticked, &n }, "Decision" ) )
                n.registered = true;
        }
        if ( !h.ui )
            race_unwired( m_race, "the race HUD projects' gameInitialize 0x500B10 of every type but VuMultiDelayEntity (the HUD elements - the UI's) - not run" );
        LOG_INFO( "race HUD: %s initialised (gameInitialize, OnProjectInitialized x%d)", h.name.c_str( ), static_cast< int >( h.initialized.size( ) ) );
        if ( h.ui )
        {
            hud_env( h );
            h.ui->initialize( );   // gameInitialize 0x500B10 + 0x501260 "OnProjectInitialized" through the UI's binding
            return;
        }
        for ( const std::string& at : h.initialized )
            fire( at, "Trigger" );
        return;
    }
    if ( verb == race_hud_verb_t::release )
    {
        if ( !h.live )
            return;
        h.live = false;
        for ( const std::string& at : h.delays )
        {
            multi_delay_node_t& n = m_multi_delays[ at ];
            tick_unregister_all( m_race.ticks, &n );
            n.registered = false;
        }
        if ( h.ui )
            h.ui->release( );
        LOG_INFO( "race HUD: %s released", h.name.c_str( ) );
        return;
    }
    if ( h.ui )
    {
        hud_env( h );
        h.ui->tick( dt );   // VuHUDEntity slot 12 0x423A20
        race_unwired( m_race, "the race HUD's draw (VuHUDEntity slot 13 0x423A70: c_ui_race_hud::draw) - no HUD render pass in the race yet" );
        return;
    }
    race_unwired( m_race, "the race HUD's VuHUDEntity tick vt+0x30 / draw vt+0x34 (the HUD's update and drawing - the UI agent's) - not run here" );
}

// VuEventManager_broadcast 0x505690 of an event the race's code subscribes to. "OnStartLightsOut" (VuStartLightsOutAction's
// 0x405470, an empty VuParams): VuRaceGame's 0x48A780 (the condition StartLightsOut) and every VuHumanDriver's 0x40F680
// (driver +0xC0 = 0x3E800000, 0.25) - the only two subscribers in a race (the string's six copies: the ghost / slalom / time
// trial games' ctors hold the others). The handler tree's order is not modelled: the two writes are independent.
inline void c_world::race_broadcast( const char* name )
{
    if ( std::strcmp( name, "OnStartLightsOut" ) == 0 )
    {
        race_rules_on_start_lights_out( m_race.rules );
        for ( const race_start_boat_t& b : m_race.start_boats )
            if ( b.cls.slot[ 0 ] && b.driver )
            {
                const std::uint32_t quarter = 0x3E800000u;
                std::memcpy( b.driver + 0xC0, &quarter, 4 );
            }
        LOG_INFO( "race: OnStartLightsOut broadcast (sync %d) - the race starts on the next game tick", m_race.dynamics ? m_race.dynamics->syncs : -1 );
        return;
    }
    race_unwired( m_race, std::string( "a broadcast with no subscriber recreated: " ) + name );
}

// ui_hud_env_fill before each HUD call: the rules' boats (their 0x520 images), the human's index, the count, no
// settings (the units: g 0x758FE0 - not wired), the track's built / circuit answers; the control method g 0x758FC0
// +0x18 is not wired (0).
inline void c_world::hud_env( hud_project_t& h )
{
    std::vector< const std::uint8_t* > images;
    std::int32_t local = -1;
    for ( std::size_t k = 0; k < m_race.rules_boats.size( ); ++k )
    {
        images.push_back( m_race.rules_boats[ k ].boat );
        for ( const void* b : m_race.manager.lists.humans.boats )
            if ( b == m_race.rules_boats[ k ].boat && local < 0 )
                local = static_cast< std::int32_t >( k );
    }
    ui_hud_env_t& env = h.ui->env( );
    ui_hud_env_fill( env, h.boats, images, local, static_cast< std::int32_t >( images.size( ) ), nullptr );
    env.m_track_built = m_race.track && track_is_built( *m_race.track );
    env.m_track_circuit = env.m_track_built && track_is_circuit( *m_race.track );
    env.m_control_method = 0;
}