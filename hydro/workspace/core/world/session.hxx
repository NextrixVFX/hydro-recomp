#pragma once

#include <workspace/core/boat/construct.hxx>
#include <workspace/core/boat/fleet.hxx>
#include <workspace/core/boat/load.hxx>
#include <workspace/core/boat/meshes.hxx>
#include <workspace/core/ui/config.hxx>
#include <workspace/core/world/start.hxx>
#include <workspace/core/world/tick.hxx>
#include <workspace/utility/logger.hxx>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// The race switched on in the running game (world batch 11): the chain the
// world's start units were verified for, composed in retail's order -
//
//   the front end's GameData (ui_front_end_game_data, core/ui config.hxx) from
//   --mode / --race / --course / --boat / --skin (no defaults: without --mode,
//   --boat and --skin there is no race and nothing here runs - the gates' bench
//   passes none of them);
//   VuGame's boat load 0x481CE0 (race_load_boats) with the boat's postLoad
//   0x41CFF0 (race_boat_post_load) on each boat's ctor graph (boat_construct
//   0x41B850) - the driver built and loaded, then 0x420A70 (boat_load_sections)
//   and 0x421670 (boat_transform_changed) on the graph's objects;
//   VuGame's begin 0x4829A0 (race_begin: VuRaceGame slots 13 / 5) on the
//   world's track with the tree's VuGridPositionEntity worlds (0x43A740), each
//   boat's gameInitialize 0x41D350 registering its Decision / Build delegates
//   in the tick manager and adding it to the boat manager (0x4AAF60);
//   per frame, VuTickManagerImpl's tick 0x4F72F0 (tick_run) over its phases.
//
// STOPPED / UNKNOWN (logged, never stood in for):
// - VuRandom 0x736810 is the one global c_world seeds at its engine init as
//   retail's 0x566230 does (fleet_random_seed on the platform clock; a --bench
//   run's clock answers a fixed 0): "Race" builds its AI roster from it.
// - 0x420A70 loads the collision / fluids meshes (the boat agent's meshes.hxx)
//   and, with the surface-type table (surface.hxx, from SurfaceDB), runs to its
//   end, the skin pass 0x421490 (world/livery.hxx) included. Still STOPPED, each
//   counted and logged once: each boat's Decision 0x41EFF0 (recreated, not yet
//   wired) and Build 0x41F330 (not recreated) ticks, and with them the race
//   camera 0x409900 and the Viewports tick 0x4AB0A0.
// - The signed-in player's name (the human entry's Driver Name) is a platform
//   service: empty here.

struct race_session_boat_t
{
    boat_graph_t graph;
    std::unique_ptr< race_boat_driver_t > driver;
    vu_rigid_body_t body{};
    boat_engine_t engine{};
    boat_load_t io{};
    bool sections = false;    // 0x420A70 ran to its end
    std::string stop;         // where it stopped (UNKNOWN), when it did
    std::string name, boat, skin;
    boat_mesh_library_t meshes;   // the collision / fluids meshes the sections load created
    std::uint8_t draw_added = 0, instigator_registered = 0;
    // The hull draw (c_world): each model instance's placements, the one shown.
    std::vector< int > placements[ 3 ];
    int shown = -2;
};

struct race_session_t
{
    bool active = false;
    std::string mode;
    value_t game_data;
    race_game_t game;
    race_start_game_t start;
    race_boat_manager_t manager;
    std::vector< std::unique_ptr< race_session_boat_t > > boats;
    std::vector< race_start_boat_t > start_boats;
    tick_manager_t ticks;
    double clock = 0.0;
    const track_manager_t* track = nullptr;
    fleet_manager_t fleet;
    std::vector< fleet_pilot_t > pilots;
    std::vector< fleet_boat_t > fleet_boats;
    // THE global VuRandom 0x736810 (c_world's, seeded once by the engine init
    // 0x566230; the pfx emitters draw from the same one): the roster picks and
    // the AI manager's plans.
    fleet_random_t* random = nullptr;
    // The surface-type table (g_vudynamicsimpl +0x38, VuDynamicsContactManagerImpl ctor 0x574EC0: the boat
    // agent's surface.hxx) from VuDBAsset/SurfaceDB; built when the session starts.
    surface_table_t surfaces;
    bool surfaces_built = false;
    // Counts of the stopped paths reached (UNKNOWN), logged once each.
    int stopped_decision = 0, stopped_build = 0, stopped_transform = 0, stopped_viewports = 0, stopped_fleet = 0;
};

// The graph object a boat-image pointer slot refers to (-1 none).
inline int race_session_ref( const boat_graph_t& g, int object, int slot )
{
    if ( object < 0 || object >= static_cast< int >( g.objects.size( ) ) )
        return -1;
    for ( const image_ref_t& r : g.objects[ static_cast< std::size_t >( object ) ].refs )
        if ( r.slot == slot )
            return r.object;
    return -1;
}

inline std::uint8_t* race_session_image( race_session_boat_t& b, int object, int slot )
{
    const int k = race_session_ref( b.graph, object, slot );
    return k >= 0 ? b.graph.objects[ static_cast< std::size_t >( k ) ].image.data( ) : nullptr;
}

// 0x420A70 on the boat's graph objects (boat +0x21C body, +0x224 camera, +0x228
// hull and its +0x1CC solver, +0x22C engine, +0x230 animation, +0x238 damage,
// +0x70 instigator; [[+0x234] +0x30] +0x14 the pfx owner's flags). The asset
// factory's doesAssetExist answers from the pack's directory (as the oracle's
// factory does); create hands a handle; the collision mesh / fluids count
// readers are absent - UNKNOWN, the load stops (b.stop says where).
inline bool race_session_sections( race_session_boat_t& b, const value_t& boatdb, const c_archive& archive, const surface_table_t* surfaces )
{
    std::uint8_t* boat = b.graph.objects[ 0 ].image.data( );
    std::uint8_t* body = race_session_image( b, 0, 0x21C );
    std::uint8_t* hull = race_session_image( b, 0, 0x228 );
    const int hull_k = race_session_ref( b.graph, 0, 0x228 );
    std::uint8_t* solver = race_session_image( b, hull_k, 0x1CC );
    std::uint8_t* engine = race_session_image( b, 0, 0x22C );
    const int pfx = race_session_ref( b.graph, 0, 0x234 );
    std::uint8_t* pfx_owner = race_session_image( b, pfx, 0x30 );
    boat_load_t& io = b.io;
    io = boat_load_t{ };
    io.boat = boat;
    io.boat_address = 0;   // retail's allocation address: read only by the shape (after the mesh - not reached)
    if ( body )
        std::memcpy( &b.body, body, sizeof( b.body ) );
    if ( engine )
        std::memcpy( b.engine.object.image, engine, sizeof( b.engine.object.image ) );
    io.body = &b.body;
    io.camera = race_session_image( b, 0, 0x224 );
    io.hull = hull;
    io.solver = solver;
    io.engine = &b.engine;
    io.animation = race_session_image( b, 0, 0x230 );
    io.pfx_owner_flags = pfx_owner ? load_word( pfx_owner, 0x14 ) : 0;
    io.damage = race_session_image( b, 0, 0x238 );
    io.instigator = race_session_image( b, 0, 0x70 );
    if ( !io.camera || !io.hull || !io.solver || !io.animation || !io.damage || !io.instigator || !b.driver || b.driver->graph.empty( ) )
    {
        b.stop = "the boat graph lacks an object 0x420A70 reads";
        return false;
    }
    for ( int at : { 0xC4, 0xDC, 0xF4, 0x11C, 0x148, 0x160, 0x178, 0x1A4, 0x1BC, 0x1D4, 0x1EC, 0x204 } )
        io.boat_strings[ at ] = "";
    for ( int at : { 0x4, 0xD4, 0xEC, 0x104, 0x11C } )
        io.hull_strings[ at ] = "";
    for ( int at : { 0x14, 0x2C, 0x44, 0x7C, 0x94, 0xAC } )
        io.animation_strings[ at ] = "";
    io.boatdb = &boatdb;
    io.driver = boat_driver_of( load_word( b.driver->graph[ 0 ].image.data( ), 0 ) );
    // g_vudynamicsimpl is live in a race; its surface table (slot 2) and the
    // world's gravity are not wired: the surface hook is absent, so a load that
    // got that far would stop there rather than use a stand-in.
    io.dynamics = true;
    struct ctx_t
    {
        race_session_boat_t* b;
        const c_archive* archive;
        std::uint32_t next = 1;
    };
    static thread_local ctx_t ctx{ };
    ctx = ctx_t{ &b, &archive, 1 };
    io.hooks.user = &ctx;
    io.hooks.exists = []( void* u, const char* type, const std::string& n )
    {
        const ctx_t& c = *static_cast< ctx_t* >( u );
        const std::string full = std::string( type ) + "/" + n;
        for ( const std::string& rest : c.archive->names( full ) )   // names() strips the prefix: exact = ""
            if ( rest.empty( ) )
                return true;
        return false;
    };
    // The asset factory's create / release (g 0x759004 slots 7 / 9): the VuCollisionMeshAsset / VuFluidsMeshAsset
    // the load creates come from the boat agent's mesh library (core/boat/meshes.hxx: the 1:1 loads 0x51AF20 /
    // 0x523770; its name list and refcounted cache are its flagged substitution); every other create hands a
    // handle from 0x10000 (the rendering assets' loads are hooks); every other release is the null one 0x421870
    // makes on a fresh boat. The surface-type table (g_vudynamicsimpl +0x38, slot 2 0x5754D0 through 0x4F88A0:
    // the boat agent's surface.hxx, built from SurfaceDB) answers both the collision mesh materials' byte +40 and
    // the body's +0x2C8; without one (SurfaceDB unreadable) io.hooks.surface stays null and the load stops there.
    b.meshes = boat_mesh_library_t{ };
    b.meshes.surfaces = surfaces;
    b.meshes.read_user = const_cast< c_archive* >( &archive );
    b.meshes.read = []( void* u, const std::string& asset, std::vector< std::uint8_t >& blob ) {
        return static_cast< const c_archive* >( u )->read( asset.c_str( ), blob );
    };
    io.hooks.create = []( void* u, const char* type, const std::string& n ) -> std::uint32_t {
        ctx_t& c = *static_cast< ctx_t* >( u );
        if ( std::strcmp( type, "VuCollisionMeshAsset" ) == 0 || std::strcmp( type, "VuFluidsMeshAsset" ) == 0 )
            return boat_mesh_create( &c.b->meshes, type, n );
        return 0x10000u + c.next++;
    };
    io.hooks.release = []( void* u, std::uint32_t asset ) {
        if ( asset == 0 )
            return true;
        return asset < 0x10000u && boat_mesh_release( &static_cast< ctx_t* >( u )->b->meshes, asset );
    };
    io.hooks.surface = nullptr;
    if ( surfaces != nullptr )
        io.hooks.surface = []( void* u, const std::string& name ) -> std::uint8_t {
            return surface_id( *static_cast< ctx_t* >( u )->b->meshes.surfaces, name.c_str( ) );
        };
    io.hooks.mesh = []( void* u, std::uint32_t a, striding_mesh_t& out ) {
        const collision_mesh_asset_t* m = static_cast< ctx_t* >( u )->b->meshes.collision_of( a );
        if ( !m )
            return false;
        collision_mesh_striding( *m, out );
        return true;
    };
    io.hooks.fluid_counts = []( void* u, std::uint32_t a, int& vertices, int& edges ) {
        boat_load_hooks_t lib{ };
        boat_mesh_hooks( static_cast< ctx_t* >( u )->b->meshes, lib );
        return lib.fluid_counts( lib.user, a, vertices, edges );
    };
    io.hooks.fluid_aabb = []( void* u, std::uint32_t a, float* box ) {
        boat_load_hooks_t lib{ };
        boat_mesh_hooks( static_cast< ctx_t* >( u )->b->meshes, lib );
        return lib.fluid_aabb( lib.user, a, box );
    };
    const bool ok = boat_load_sections( io );
    if ( !ok && b.stop.empty( ) )
        b.stop = io.hooks.surface == nullptr
            ? "the surface-type table (g_vudynamicsimpl +0x38 slot 2, for '" + io.boat_strings[ 0x11C ] + "': not recreated) or a path before it ("
                + std::to_string( b.meshes.entries.size( ) ) + " mesh asset(s) loaded: " + ( b.meshes.entries.empty( ) ? std::string( "none" ) : b.meshes.entries[ 0 ].key )
                + ( b.meshes.entries.size( ) > 1 ? ", " + b.meshes.entries[ 1 ].key : std::string( ) ) + ")"
            : "a 0x420A70 path not modelled (boat load.hxx returned false)";
    return ok;
}

// The Decision / Build delegates the boat's gameInitialize registers (0x41EFF0
// / 0x41F330 behind the thunks 0x4077ED / 0x404AB2). With the sections load
// complete (the surface-type table, boat batch 11) they are reached, and both
// are still STOPPED here - counted, logged once each:
// - Decision 0x41EFF0 IS recreated (core/boat state.hxx boat_decision_tick)
//   but NOT WIRED: its boat_decision_t (hull / engine strings, the engine's
//   +0xF8 records, the water floor) and its hooks (the driver tick vt+0x48,
//   the animation tick 0x40FFF0, the camera 0x409900 = boat_camera_tick, the
//   respawn, slots 17 / 18, the debris paths) are not composed yet.
// - Build 0x41F330 is NOT RECREATED anywhere (it moves the draw matrix +0x49C
//   and feeds the dynamics).
struct race_session_tick_t
{
    race_session_t* session;
    race_session_boat_t* boat;
};

inline void race_session_decision( void* user, float )
{
    race_session_tick_t& t = *static_cast< race_session_tick_t* >( user );
    if ( t.session->stopped_decision++ == 0 )
        LOG_WARNING( t.boat->sections ? "race: VuBoatEntity Decision tick 0x41EFF0 STOPPED - recreated (boat_decision_tick) but not wired "
                                        "for %s yet"
                                      : "race: VuBoatEntity Decision tick 0x41EFF0 STOPPED (UNKNOWN) - %s's sections load did not complete",
                     t.boat->name.c_str( ) );
}

inline void race_session_build( void* user, float )
{
    race_session_tick_t& t = *static_cast< race_session_tick_t* >( user );
    if ( t.session->stopped_build++ == 0 )
        LOG_WARNING( t.boat->sections ? "race: VuBoatEntity Build tick 0x41F330 STOPPED (UNKNOWN) - not recreated (%s)"
                                      : "race: VuBoatEntity Build tick 0x41F330 STOPPED (UNKNOWN) - %s's sections load did not complete",
                     t.boat->name.c_str( ) );
}

// The AI manager's tick 0x403900 (fleet_tick), in Decision ahead of the boats
// (the order the boat agent's ai_race case runs - its registration is not read).
inline void race_session_fleet( void* user, float dt )
{
    race_session_t& s = *static_cast< race_session_t* >( user );
    fleet_tick( s.fleet, *s.track, s.pilots, s.fleet_boats, *s.random, dt );
}

// The Viewports phase's boat manager tick 0x4AB0A0 (boat_manager_viewport_tick,
// core/boat manager.hxx) reads the viewport boat's camera image, which only
// the Decision tick's camera step 0x409900 computes: STOPPED with the Decision
// tick (counted, logged once) - the view stays on dev_vantage's orbit / freecam.
inline void race_session_viewports( void* user, float )
{
    race_session_t& s = *static_cast< race_session_t* >( user );
    if ( s.stopped_viewports++ == 0 && !s.boats.empty( ) )
        LOG_WARNING( "race: the Viewports tick 0x4AB0A0 and the race camera 0x409900 STOPPED - %s", s.boats[ 0 ]->sections
                         ? "the Decision tick that runs the camera is not wired yet"
                         : "the Camera section is read after the sections load's stop" );
}
// A boat property's text into the boat image's inline std::string (+0x74 /
// +0x8C / +0xA4 - VuEntity::load's targets): false for a text retail would
// keep on the heap (16 characters or more - not modelled by the image).
inline bool race_session_text( std::uint8_t* boat, int at, const std::string& text )
{
    if ( text.size( ) > 15 )
        return false;
    image_empty_string( boat, at );
    parts_string( boat, at, text );
    return true;
}

// What the session reads from the pack (the databases' document roots).
struct race_session_dbs_t
{
    const value_t* boat_db = nullptr;
    const value_t* track_db = nullptr;
    const value_t* constant_db = nullptr;
};

// The front end's GameData (ui_front_end_game_data, core/ui config.hxx) from
// --mode / --race / --course / --boat / --skin - built BEFORE the project load,
// as retail's front end leaves it for VuActionGameMode's load 0x4BA060 (whose
// project choice 0x4D3480 reads its Level / GameType / Course). False (logged)
// when an option is missing or the builder is not recreated. The roster picks
// draw from the shared VuRandom 0x736810 in whatever state the run has left it.
inline bool race_session_game_data( const std::string& mode, const std::string& track_name, const std::string& course, const std::string& boat,
                                    const std::string& skin, const race_session_dbs_t& db, fleet_random_t& random, value_t& out )
{
    if ( mode.empty( ) || boat.empty( ) || skin.empty( ) || track_name.empty( ) )
    {
        LOG_WARNING( "race: boats UNKNOWN - --mode, --race, --boat and --skin are all required (no defaults); no race" );
        return false;
    }
    if ( !db.boat_db || !db.track_db || !db.constant_db )
    {
        LOG_ERROR( "race: BoatDB / TrackDB / ConstantDB not readable from the pack; no race" );
        return false;
    }
    ui_game_env_t env;
    env.boat_db = db.boat_db;
    env.track_db = db.track_db;
    env.random = &random;
    ui_game_config_t built;
    out = ui_front_end_game_data( mode, track_name, course, boat, skin, env, &built );
    if ( !built.built )
    {
        LOG_WARNING( "race: GameData UNKNOWN - mode '%s' reaches builder 0x%X, %s; no race", mode.c_str( ), built.builder,
                     built.builder ? "not recreated" : "none (not a front-end event type)" );
        return false;
    }
    return true;
}

// The start (0x4BA060 after the project load, for GameType "Race": the
// VuRaceGame ctor, the boat load + postLoad, begin) on the GameData
// race_session_game_data built. `tree` the race project's entity tree, `track`
// the world's track manager built from it. False (logged) when a boat cannot
// be made.
inline bool race_session_start( race_session_t& s, const std::string& mode, const value_t& game_data, const race_session_dbs_t& db,
                                const c_archive& archive, entity_t& tree, const track_manager_t* track, fleet_random_t& random )
{
    s = race_session_t{ };
    s.random = &random;
    s.game_data = game_data;
    race_game_construct( s.game );   // the VuRaceGame ctor 0x489030 (its base 0x480BA0's values)
    {
        std::vector< std::uint8_t > bytes;
        if ( archive.read( "VuDBAsset/SurfaceDB", bytes ) )
        {
            const value_t surface_db = c_reader( bytes ).parse( );
            s.surfaces_built = surface_table_build( s.surfaces, &surface_db );
        }
        if ( !s.surfaces_built )
            LOG_WARNING( "race: VuDBAsset/SurfaceDB did not build the surface-type table - the sections load will stop at it" );
        else
            LOG_INFO( "race: surface-type table built from SurfaceDB: %d types", static_cast< int >( surface_count( s.surfaces ) ) );
    }
    s.mode = mode;
    s.track = track;

    // VuGame's boat load 0x481CE0 + each boat's postLoad 0x41CFF0.
    value_t util;
    ui_json_child( util, "GameData" ) = s.game_data;
    const entity_registry_t registry = build_entity_registry( { } );
    race_load_hooks_t hooks;
    bool made = true;
    hooks.post_load = [ & ]( entity_t& e, const value_t& entry )
    {
        auto b = std::make_unique< race_session_boat_t >( );
        const race_boat_state_t& st = *static_cast< const race_boat_state_t* >( e.m_state.get( ) );
        b->name = e.m_name;
        b->boat = st.boat;
        b->skin = st.skin;
        b->graph = boat_construct( *db.constant_db );
        std::uint8_t* image = b->graph.objects[ 0 ].image.data( );
        // The Boat text into +0x8C (0x420A70 reads it there); Driver Type (+0x74)
        // and Skin (+0xA4) stay in the entity's state - their readers here are
        // postLoad's compare (given the state's text) and the skin pass (a hook).
        if ( !race_session_text( image, 0x8C, st.boat ) )
        {
            LOG_WARNING( "race: %s's Boat '%s' is 16+ characters (a heap std::string in +0x8C - not modelled); no race", e.m_name.c_str( ),
                         st.boat.c_str( ) );
            made = false;
        }
        race_session_boat_t& r = *b;
        race_post_load_hooks_t ph;
        ph.load_sections = [ &r, &db, &archive, &s ] { r.sections = race_session_sections( r, *db.boat_db, archive, s.surfaces_built ? &s.surfaces : nullptr ); };
        ph.transform_changed = [ &r, &s ]
        {
            std::uint8_t* t = race_session_image( r, 0, 0x50 );
            if ( !r.sections || !t )
            {
                ++s.stopped_transform;
                return;
            }
            boat_transform_changed( r.graph.objects[ 0 ].image.data( ), reinterpret_cast< const float* >( t + 0x50 ), r.body, r.io.camera,
                                    r.io.instigator, boat_transform_hooks_t{ } );
        };
        const bool known = race_boat_post_load( image, st.driver_type, entry, *db.constant_db, r.driver, ph );
        if ( !known )
        {
            LOG_WARNING( "race: %s's driver '%s' is UNKNOWN (no modelled class); no race", e.m_name.c_str( ), st.driver_type.c_str( ) );
            made = false;
        }
        s.boats.push_back( std::move( b ) );
    };
    if ( !race_load_boats( s.game, s.game_data, util, *db.track_db, registry, hooks ) || !made || s.boats.empty( ) )
    {
        LOG_WARNING( "race: the boat load 0x481CE0 made no usable boat; no race" );
        return false;
    }
    for ( const auto& b : s.boats )
    {
        if ( b->sections )
            LOG_SUCCESS( "race: %s (%s, skin %s): sections load 0x420A70 complete", b->name.c_str( ), b->boat.c_str( ), b->skin.c_str( ) );
        else
            LOG_WARNING( "race: %s (%s, skin %s): sections load 0x420A70 STOPPED (UNKNOWN) at %s - its physics, ticks and camera stop; the "
                         "hull is placed at its start transform", b->name.c_str( ), b->boat.c_str( ), b->skin.c_str( ), b->stop.c_str( ) );
    }
    if ( s.stopped_transform )
        LOG_WARNING( "race: the transform-changed delegate 0x421670 STOPPED (UNKNOWN) on %d boat(s) - it reads the sections load's +0x290 centre",
                     s.stopped_transform );

    // The tick manager (ctor 0x4F6F60, slot 5 0x4F7050) and the AI manager's tick.
    tick_manager_init( s.ticks );
    tick_register( s.ticks, { &s.fleet, race_session_fleet, &s }, "Decision" );
    tick_register( s.ticks, { &s.manager, race_session_viewports, &s }, "Viewports" );

    // The grid: every VuGridPositionEntity's transform world, through 0x43A740,
    // in the tree's pre-order (the project's gameInitialize walk; the list is
    // sorted by name after each add, so only equal names would depend on it).
    const auto walk = [ & ]( const auto& self, entity_t& e ) -> void
    {
        if ( e.m_type == "VuGridPositionEntity" && !e.m_world_unknown )
            if ( const transform_component_t* t = entity_transform( e ) )
            {
                race_grid_entry_t g;
                g.name = e.m_name;
                std::memcpy( g.world, t->world, 64 );
                race_grid_add( s.manager, g );
            }
        for ( auto& child : e.m_children )
            self( self, *child );
    };
    walk( walk, tree );

    // VuGame's begin 0x4829A0.
    s.start.laps = s.game.laps;
    s.start.difficulty = s.game.difficulty;
    s.start.root_name = tree.m_name;
    static std::vector< race_session_tick_t > tick_users;
    tick_users.assign( s.boats.size( ), race_session_tick_t{ } );
    for ( std::size_t k = 0; k < s.boats.size( ); ++k )
    {
        race_session_boat_t& b = *s.boats[ k ];
        tick_users[ k ] = { &s, &b };
        race_start_boat_t sb;
        sb.boat = b.graph.objects[ 0 ].image.data( );
        sb.transform = race_session_image( b, 0, 0x50 );
        sb.engine = b.engine.object.image;
        sb.driver = b.driver->graph[ 0 ].image.data( );
        const std::uint32_t vt = load_word( sb.driver, 0 );
        sb.cls = boat_driver_of( vt );
        boat_driver_kind( vt, sb.kind );
        s.start_boats.push_back( sb );
        s.fleet_boats.push_back( { reinterpret_cast< const float* >( sb.transform + 0x50 ), 0.0f, 0.0f } );
    }
    if ( !track )
    {
        LOG_WARNING( "race: the world's track did not build; begin 0x4829A0 not run" );
        return false;
    }
    race_start_hooks_t sh;
    sh.transform_changed = [ &s ]( int k )
    {
        if ( !s.boats[ static_cast< std::size_t >( k ) ]->sections )
            ++s.stopped_transform;
    };
    sh.game_initialize = [ &s ]( int k )
    {
        race_session_boat_t& b = *s.boats[ static_cast< std::size_t >( k ) ];
        race_boat_init_hooks_t ih;
        ih.manager = &s.manager.lists;
        ih.entry = s.start_boats[ static_cast< std::size_t >( k ) ].boat;
        ih.kind = s.start_boats[ static_cast< std::size_t >( k ) ].kind;
        race_session_tick_t* user = &tick_users[ static_cast< std::size_t >( k ) ];
        race_boat_game_initialize( &b, s.ticks, { &b, race_session_decision, user }, { &b, race_session_build, user }, b.draw_added, ih );
    };
    sh.add_driver = [ &s ]( int k )
    {
        // Slot 11 0x403590 (boat, 0, 0) for an AI boat's driver.
        fleet_pilot_t p;
        p.driver = s.start_boats[ static_cast< std::size_t >( k ) ].driver;
        p.boat = k;
        p.sector = s.start_boats[ static_cast< std::size_t >( k ) ].sector;
        s.pilots.push_back( p );
        fleet_boat_t& fb = s.fleet_boats[ static_cast< std::size_t >( k ) ];
        fb.engine_4 = state_f( s.start_boats[ static_cast< std::size_t >( k ) ].engine, 4 );
        fb.engine_c = state_f( s.start_boats[ static_cast< std::size_t >( k ) ].engine, 0xC );
        fleet_add_driver( s.fleet, s.fleet_boats, static_cast< int >( s.pilots.size( ) ) - 1, k, 0.0f, 0.0f );
    };
    sh.fleet_start = [ &s ]
    {
        fleet_start( s.fleet, *s.track, s.pilots, s.fleet_boats, *s.random );
    };
    sh.fleet_run = [ &s ] { fleet_run( s.fleet ); };
    race_begin( s.start, s.start_boats, s.manager, *track, *db.track_db, *db.constant_db, sh );
    int placed = 0;
    for ( const race_start_boat_t& b : s.start_boats )
        placed += b.transform ? 1 : 0;
    LOG_SUCCESS( "race: %s on %s - %d boat(s) from GameData, %d grid position(s), %d lap(s), difficulty %s, begin 0x4829A0 run "
                 "(humans %d, AI %d)", mode.c_str( ), s.game.track.c_str( ), static_cast< int >( s.boats.size( ) ),
                 static_cast< int >( s.manager.grid.size( ) ), s.game.laps, s.game.difficulty.c_str( ),
                 static_cast< int >( s.manager.lists.humans.boats.size( ) ), static_cast< int >( s.manager.lists.ais.boats.size( ) ) );
    s.active = true;
    return true;
}

// Per frame: VuTickManagerImpl's tick 0x4F72F0 with the clock advanced by our
// frame time (the clock g_vumetrosys vt+0x28 is the platform layer).
inline void race_session_frame( race_session_t& s, float dt )
{
    if ( !s.active )
        return;
    s.clock += static_cast< double >( dt );
    tick_run( s.ticks, s.clock );
}

// VuBoatEntity's draw 0x41EAE0 (slot 12, behind the draw delegate 0x4077E3),
// its model choice on the normal pass: nothing when the pass's +0x2D is set,
// the debug draw 0x41ED20 when g 0x758F92 is (dev only - not modelled), else
// 0x4121C0 (the damage pieces - a hook), nothing when +0x464 && +0x465, no
// hull when +0x46B; the matrix = the transform's world (+0x50) . 0x410AB0
// (translation by boat +0x49C) - 0x4078C0 with the world as `this`; d = the
// squared distance of the transform's +0x80 origin from the eye ((y + x) + z
// order); the reflection pass (+0x18) takes instance 2 unless +0x466; else
// d > (+0x110)^2 instance 2 (+0x2AC, the static LOD 2), d > (+0x10C)^2
// instance 1 (+0x2A8), else instance 0 (+0x2A4). -1: no hull drawn. The
// instance colour (0x41B310: +0x114 / +0x118 lerped by +0x2B4) and the alpha
// byte (+0x2C8 x 0x6F7808, 0x80 with +0x466) are not consumed by our renderer.
inline int race_boat_draw_instance( const std::uint8_t* boat, const std::uint8_t* transform, const float* eye )
{
    if ( ( boat[ 0x464 ] && boat[ 0x465 ] ) || boat[ 0x46B ] )
        return -1;
    const float dx = state_f( transform, 0x80 ) - eye[ 0 ];
    const float dy = state_f( transform, 0x84 ) - eye[ 1 ];
    const float dz = state_f( transform, 0x88 ) - eye[ 2 ];
    const float d = ( dy * dy + dx * dx ) + dz * dz;
    const float lod2 = state_f( boat, 0x110 ), lod1 = state_f( boat, 0x10C );
    if ( _mm_comigt_ss( _mm_set_ss( d ), _mm_set_ss( lod2 * lod2 ) ) )
        return 2;
    if ( _mm_comigt_ss( _mm_set_ss( d ), _mm_set_ss( lod1 * lod1 ) ) )
        return 1;
    return 0;
}

inline void race_boat_draw_matrix( const std::uint8_t* boat, const std::uint8_t* transform, float* out )
{
    float world[ 16 ], tr[ 16 ];
    std::memcpy( world, transform + 0x50, 64 );
    hull_translation( tr, reinterpret_cast< const float* >( boat + 0x49C ) );
    hull_multiply( world, out, tr );
}
