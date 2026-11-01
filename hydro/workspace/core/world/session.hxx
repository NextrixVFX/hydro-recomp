#pragma once

#include <workspace/core/boat/construct.hxx>
#include <workspace/core/boat/fleet.hxx>
#include <workspace/core/boat/load.hxx>
#include <workspace/core/boat/meshes.hxx>
#include <workspace/core/boat/nuisance.hxx>
#include <workspace/core/boat/rays.hxx>
#include <workspace/core/boat/respawn.hxx>
#include <workspace/core/boat/thunk.hxx>
#include <workspace/core/dynamics/contact.hxx>
#include <workspace/core/trigger/volume.hxx>
#include <workspace/core/ui/config.hxx>
#include <workspace/core/world/blend.hxx>
#include <workspace/core/world/bodies.hxx>
#include <workspace/core/world/collide.hxx>
#include <workspace/core/world/dynamics.hxx>
#include <workspace/core/world/kernel.hxx>
#include <workspace/core/world/lens.hxx>
#include <workspace/core/world/mine.hxx>
#include <workspace/core/world/pfxmgr.hxx>
#include <workspace/core/world/pfxsim.hxx>
#include <workspace/core/world/dynprop.hxx>
#include <workspace/core/boat/effects.hxx>
#include <workspace/core/world/race.hxx>
#include <workspace/core/world/rules.hxx>
#include <workspace/core/world/start.hxx>
#include <workspace/core/world/tick.hxx>
#include <workspace/utility/logger.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
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
//   end, the skin pass 0x421490 (world/livery.hxx) included. Each boat's
//   Decision tick 0x41EFF0 runs (world/drive.hxx, its hooks recreated or each
//   declared there), and with it the race camera 0x409900; the Viewports tick
//   0x4AB0A0 copies the viewport boat's camera into the view. Still STOPPED
//   (declared, logged once): the Build tick 0x41F330's steps 1 - 6 (its 0x4200E0 runs).
// - The signed-in player's name (the human entry's Driver Name) is a platform
//   service: empty here.

struct race_session_boat_t
{
    boat_graph_t graph;
    std::unique_ptr< race_boat_driver_t > driver;
    vu_rigid_body_t body{};
    collision_object_t collision{};   // the body's collision object in the race's dynamics world (world/dynamics.hxx)
    boat_engine_t engine{};
    boat_load_t io{};
    bool sections = false;    // 0x420A70 ran to its end
    std::string stop;         // where it stopped (UNKNOWN), when it did
    std::string name, boat, skin;
    boat_mesh_library_t meshes;   // the collision / fluids meshes the sections load created
    std::unique_ptr< boat_thunk_t > thunk;   // the step callback's context (boat/thunk.hxx), persistent
    std::vector< std::uint32_t > excluded;   // the solver's +0x34 array words this step
    std::uint8_t draw_added = 0, instigator_registered = 0;
    // The hull draw (c_world): each model instance's placements, the one shown.
    std::vector< int > placements[ 3 ];
    int shown = -2;
    // Ours: the session this boat is in (the transform watcher's 0x536350 reaches the trigger manager through it).
    struct race_session_t* owner = nullptr;
    int index = -1;
};

// What the world lends the race's ticks, refreshed by c_world every frame
// (world/drive.hxx reads it):
// - water / view: VuWater g 0x759058 (the built water and the query's view of
//   it; null: no VuWater);
// - viewports: g_vuviewportmanager 0x759060 +4 - VuActionGameMode's enter
//   0x4B9630 calls 0x530BB0(1) before its load, and 0x530BB0 stores min(n, 4)
//   (cmovl) at +4: 1 for the race;
// - control_method: g_vucontrolmethodmanager 0x758FC0 +0x18 - the input
//   layer's answer (a PLATFORM HOOK: ours answers 0, the gamepad method, as
//   the front end's ui flow does);
// - display_w / _h: g_vumetrod3d11gfx vt+0x40, the display size (PLATFORM).
// - water_tick: VuWater's "Water" tick 0x529E30 (c_world's water_built_tick +
//   the view rebuilt), run by the race tick manager's "Water" phase.
struct race_world_t
{
    water_built_t* water = nullptr;   // written by the hulls' wake update 0x417D80 (world/drive.hxx)
    const water_view_t* view = nullptr;
    std::function< void( float ) > water_tick;
    // The "Triggers" tick 0x536460's trigger half: every listed trigger's vt+0x30 over the manager's
    // instigators (c_world's trigger list - world.hxx race_triggers), run after the instigator step.
    std::function< void( ) > triggers;
    // The game mode manager's Decision tick 0x4CBFD0 -> the action mode's slot 4 0x4B99E0 (world/action.hxx, c_world's);
    // null: no action mode - the session's game tick runs as if in Run (the scaffold paths).
    std::function< void( float ) > game_mode;
    // The race HUD projects (VuRaceGame +0x738 / +0x73C / +0x740: 0 PreGame, 1 Game, 2 PostGame) - their load, gameInitialize,
    // release and graph are the world's (c_world), their tick / draw the UI's; null: no HUD projects (UNKNOWN, logged).
    std::function< void( int project, race_hud_verb_t verb, float dt ) > hud;
    // A VuEventManager broadcast reaching the world's graph (the HUD's / the level's subscribers).
    std::function< void( const char* name, const ui_params_t* params ) > broadcast;   // params null: an empty VuParams
    // The world's VuPfx runtime / VuPfxManager g 0x75905C (c_effects; null: none - the boats' pfx calls are UNKNOWN).
    pfx_runtime_t* pfx = nullptr;
    pfx_manager_t* pfx_manager = nullptr;
    const pfx_manager_ops_t* pfx_ops = nullptr;
    // A dynamic prop's Build tick 0x546130: its static model component's draw matrix (+0x13C = fromScale(transform
    // +0xB0) x world +0x50) and world aabb (0x531D10) re-made and handed to the renderer - the world's static draw of
    // that owner (c_world: the placements moved). Null: not drawn from the bodies (logged).
    std::function< void( const entity_t& owner ) > prop_draw;
    // The 3D draw manager's add / remove of an owner's static model component (g 0x7590B8 vt+0x28 / +0x2C; the
    // component's +0x1E) - the renderer's: c_world's static draws. Null: not hidden (logged).
    std::function< void( const entity_t& owner, bool on ) > prop_drawn;
    // An entity's output plug fired (VuScriptComponent findPlug 0x504770 + the plug's vt+8 = 0x5023A0): c_world's graph.
    std::function< void( const entity_t& owner, const char* plug ) > fire;
    // An entity's transform moved by a motion component (its owner's draw matrix re-made: the animated prop's 0x61DA50 /
    // the static model's) - the renderer's hand-off (c_world). Null: not redrawn (logged).
    std::function< void( const entity_t& owner ) > prop_moved;
    // The audio platform (FMOD's imports - world/sound.hxx; c_world's). Null: no audio calls made (logged).
    const sound_hooks_t* sound = nullptr;
    std::uint32_t viewports = 1;
    std::int32_t control_method = 0;
    std::int32_t display_w = 1280;
    std::int32_t display_h = 720;
};

struct race_drive_t;
struct race_contact_t;

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
    // The lens-water manager g 0x759048's emitter list (VuLensWaterManagerImpl vt+0x38 / +0x3C, render side - a
    // hook): what each VuLensWaterEmitterEntity's slot 6 0x5E6720 handed it (world/lens.hxx). Nothing reads it yet.
    std::vector< const void* > lens_emitters;
    // Counts of the stopped paths reached (UNKNOWN), logged once each.
    int stopped_decision = 0, stopped_build = 0, stopped_transform = 0, stopped_viewports = 0, stopped_fleet = 0;
    // The world's services (refreshed per frame) and each boat's Decision
    // composition (world/drive.hxx; by boat index).
    race_world_t world;
    std::vector< std::shared_ptr< race_drive_t > > drives;
    // ConstantDB's ["VuDBAsset"]["Camera"]["VerticalFOV"] (gameutil +4's
    // document, 0x4D2C60's lookups) - the race camera's fov input.
    value_t vertical_fov;
    // The viewport's camera (g 0x759060 +0x28: the 0x240 bytes the Viewports
    // tick 0x4AB0A0 copies in) and whether one was copied yet.
    std::uint8_t viewport_camera[ 0x240 ]{};
    bool viewport_set = false;
    // Each hook reached that is not recreated / not wired, by name (logged once).
    std::map< std::string, int > unwired;
    // The nuisance boats' entities (VuAiNuisanceDriverStartEntity +0x7C's children, 0x42FDC0).
    std::vector< std::unique_ptr< entity_t > > nuisance;
    // Bullet's contact-added callback for the race's dynamics world (world/drive.hxx race_contact_added ->
    // boat contact.hxx contact_added): built by race_drive_setup, installed by whatever steps the bodies.
    std::shared_ptr< race_contact_t > contact_user;
    contact_callbacks_t contact{};
    // VuCollisionManager g 0x758FBC (world/collide.hxx, the water agent's): the manager, its surface-pair tables (from
    // VuDBAsset/SurfaceTableDB, held here as the asset's document), whether its listener is in the contact manager's +4 list.
    collision_manager_t collision;
    collision_tables_t collision_tables;
    value_t collision_db;
    bool collision_listening = false;
    int collision_impacts = 0;   // dev counts (not retail): impact records the tick played, live scrape records
    int collision_scrapes = 0;
    // g_vudynamicsimpl 0x75900C: the dynamics world (world/dynamics.hxx), made before the boats as the engine
    // init 0x566230 makes it; its DynamicsSync / DynamicsKick ticks registered first in their phases.
    std::unique_ptr< race_dynamics_t > dynamics;
    // The level's static bodies (world/bodies.hxx) and their collision mesh assets.
    boat_mesh_library_t level_meshes;
    std::vector< std::unique_ptr< level_body_t > > level_bodies;
    // The level's dynamic game props made live (world/dynprop.hxx): their bodies are in level_bodies too.
    struct dyn_prop_t
    {
        entity_t* entity = nullptr;
        dynamic_prop_t* p = nullptr;
        rigid_component_t* c = nullptr;
        level_body_t* body = nullptr;
        dynamic_prop_live_t live;
        dynamic_prop_motion_t motion;
        dynamic_prop_init_t init;
        bool buoy = false;
        bool mine = false;               // VuFloatingMineEntity (world/mine.hxx)
        std::uint8_t exploded = 0, request = 0;   // the mine's +0x214 / +0x215
        bool step_added = false;         // the step callback in g_vudynamicsimpl's list (the hide removes it)
        float timer = 0.0f;              // +0x17C (the Decision tick's)
        dynamic_prop_hit_t hit;          // +0x164 / +0x174 / +0x178 (the contact callback 0x437F40's)
        std::uint8_t step_tag = 0;       // the entity's +0xB0 part: the step callback's identity in +0x48
        race_session_t* session = nullptr;
    };
    std::vector< std::unique_ptr< dyn_prop_t > > dyn_props;
    int buoy_hits = 0;                   // dev count (not retail): OnCheckPointBuoyHit broadcasts made
    int prop_hits = 0, prop_splashes = 0;   // dev counts (not retail): 0x437F40 hits taken, dynamic-prop splashes started
    int mine_explosions = 0, explosion_hits = 0;   // dev counts (not retail)
    // The level's VuBreakablePropEntity (world/breakable.hxx), live from their slot 6 0x434620.
    struct breakable_live_t
    {
        entity_t* entity = nullptr;
        breakable_t* p = nullptr;
        rigid_component_t* c = nullptr;
        level_body_t* body = nullptr;
        std::uint8_t request = 0;          // +0xD0
        float velocity[ 3 ] = { };         // +0xD4
        const void* target = nullptr;      // the weak ref's object (+0xF0): the other body's owner (a session boat's body here)
        std::uint32_t state = 0;           // the breakable component's +0xE4 (0 whole, 1 broken / debris live, 2 done)
        race_session_t* session = nullptr;
    };
    std::vector< std::unique_ptr< breakable_live_t > > breakables;
    // The level's VuKeyframeMotionEntity (world/keymotion.hxx), live from their slot 6 0x5CE9E0.
    struct key_live_t
    {
        entity_t* entity = nullptr;
        key_motion_t* k = nullptr;
        entity_t* target_entity = nullptr;   // the "Entity" ref's entity (its VuMotionComponent the target)
        key_motion_target_t target;
        race_session_t* session = nullptr;
    };
    std::vector< std::unique_ptr< key_live_t > > key_motions;
    int key_unrecreated = 0;
    int breaks = 0;   // dev count (not retail)
    // VuExplosionManagerImpl g 0x759034's listeners (+4 / +8; world/mine.hxx): the human drivers' +0x80 parts, added by
    // their slot 6 0x40F210 (boat human.hxx human_game_initialize).
    std::vector< explosion_listener_t > explosion_listeners;
    // The gfx-settings blender g 0x759430 (world/blend.hxx; 0x52F620 at the session's start) and the level's gfx-settings
    // entities' blend views (the zones in its list; the global's not listed - its timer only), made by their gameInitialize.
    gfx_blender_t blender;
    // The ray test's view of the dynamics world (boat rays.hxx; world/drive.hxx race_drive_ray).
    boat_ray_world_t rays;
    std::vector< std::unique_ptr< gfx_zone_t > > gfx_entities;
    // The event graph's race-side receivers, by the entity's path below the project root (c_world's graph keys):
    // the gfx-settings entities' "Trigger" (0x5A7230 zones / 0x5D2980 the global) and the nuisance start entities'
    // "DriverTarget" / "DriverDisable" (0x42FF90 / 0x430020).
    struct gfx_plug_t
    {
        gfx_zone_t* zone = nullptr;
        bool global = false;
    };
    std::map< std::string, gfx_plug_t > gfx_by_path;
    struct nuisance_plug_t
    {
        nuisance_start_t* state = nullptr;
        int child = -1;   // the session boat the start entity's +0x7C is (-1: none made)
    };
    std::map< std::string, nuisance_plug_t > nuisance_by_path;
    // g_vutriggermanager 0x759064's instigators (trigger/volume.hxx).
    trigger_manager_t triggers;
    // Retail's 32-bit words for the weak references the nuisance plugs link (boat nuisance.hxx vu_words_t): a
    // boat's entity is the token race_entity_token(k), its driver token + 0x1000 (+0x220 holds it), every other
    // word 0 until written. The entity repository g 0x759080 (hash -> entity) is not recreated: a VuParams
    // entity's hash word is the token here, the repository's find the identity on a boat's token.
    std::map< std::uint32_t, std::uint32_t > words;
    // THE RACE RULES (world/rules.hxx): VuRaceGame's +0x6F8 state machine and +0x744, the game fields they read, the game's
    // boats as they see them (s.game.boats' order: the first game.boats.size( ) session boats - the nuisance boats are not
    // the game's), whether the game's tick 0x482F10 still runs (slot 7 answered true) and its stopped paths.
    race_rules_t rules;
    race_rules_game_t rules_game;
    std::vector< race_rules_boat_t > rules_boats;
    bool rules_running = false;
    bool game_answer = true;   // VuGame's tick 0x482F10's answer, last frame (false: the game is done - Run's 0x4BAB50 reads it)
    int rules_frames = 0;
    // THE TEST INPUT (ours, flagged - not retail): --input-script's per-frame pad states (world/drive.hxx race_input_script_t),
    // answered by the platform input hook in place of the device; null: the device (game/input.hxx read_input). rames: the
    // session frames run (race_session_frame), the script's clock.
    std::shared_ptr< struct race_input_script_t > input_script;
    int frames = 0;
};

// The model's entity / driver tokens (see race_session_t::words).
inline std::uint32_t race_entity_token( int k )
{
    return 0x00100000u * static_cast< std::uint32_t >( k + 1 );
}
inline int race_entity_of_token( const race_session_t& s, std::uint32_t token )
{
    if ( token == 0 || ( token & 0x000FFFFFu ) != 0 )
        return -1;
    const int k = static_cast< int >( token / 0x00100000u ) - 1;
    return k >= 0 && static_cast< std::size_t >( k ) < s.boats.size( ) ? k : -1;
}

// An entity's path the way the event graph names it: its names below the project root, '/'-joined.
inline std::string race_entity_path( const entity_t* e )
{
    std::string path;
    for ( ; e && e->m_parent; e = e->m_parent )
        path = path.empty( ) ? e->m_name : e->m_name + "/" + path;
    return path;
}

// The Decision tick 0x41EFF0 and the Viewports tick 0x4AB0A0 composed on the
// session (world/drive.hxx).
inline void race_drive_setup( race_session_t& s, const value_t* constant_db );
inline void race_drive_decision( race_session_t& s, std::size_t boat, float dt );
inline void race_drive_viewports( race_session_t& s );
inline void race_drive_build( race_session_t& s, std::size_t boat, float dt );
// VuGame's tick 0x482F10 as the Decision phase reaches it (world/drive.hxx).
inline void race_session_game_mode( void* user, float dt );
// The Decision tick the game mode manager registers: the action mode when there is one (its Run state's 0x4BAB50
// calls race_session_game_mode), else race_session_game_mode itself.
inline void race_session_mode_tick( void* user, float dt )
{
    race_session_t& s = *static_cast< race_session_t* >( user );
    if ( s.world.game_mode )
        s.world.game_mode( dt );
    else
        race_session_game_mode( user, dt );
}

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

// The bytes a boat-image pointer slot points at: the referenced object's
// image PLUS the ref's offset (the body: boat +0x21C points 16 bytes into its
// btAlignedAlloc block, past the block's own unaligned address - boat
// construct.hxx; the offset was ignored before world batch 14, so the
// sections load read the body 16 bytes early).
inline std::uint8_t* race_session_image( race_session_boat_t& b, int object, int slot )
{
    if ( object < 0 || object >= static_cast< int >( b.graph.objects.size( ) ) )
        return nullptr;
    for ( const image_ref_t& r : b.graph.objects[ static_cast< std::size_t >( object ) ].refs )
        if ( r.slot == slot )
            return r.object >= 0 ? b.graph.objects[ static_cast< std::size_t >( r.object ) ].image.data( ) + r.offset : nullptr;
    return nullptr;
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
        std::memcpy( &b.body, body, 0x2D4 );   // retail's VuRigidBody is 0x2D4 bytes (sizeof is 0x2E0: alignment padding)
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

inline void race_unwired( race_session_t& s, const std::string& what );

// A VuWater wave POINTER in a race image (the hull's wake slots +0x1DC /
// +0x1E0 / +0x1E4 / +0x1E8, the solver's +0x34 array words) is a model value
// here: 0x80000000 | the wave's index in the built water (retail's heap
// address - only its identity is ever read: the thunk's exclusion compares,
// the wake update's dereference). 0 is null.
constexpr std::uint32_t k_race_wave_token = 0x80000000u;
inline std::uint32_t race_wave_token( int wave )
{
    return wave < 0 ? 0u : ( k_race_wave_token | static_cast< std::uint32_t >( wave ) );
}
// The token's wave: -1 for null, -2 when the word is not a token of a wave
// the built water holds (UNKNOWN).
inline int race_wave_of( const water_built_t* b, std::uint32_t word )
{
    if ( word == 0 )
        return -1;
    const std::uint32_t index = word & ~k_race_wave_token;
    if ( !( word & k_race_wave_token ) || !b || index >= b->waves.size( ) )
        return -2;
    return static_cast< int >( index );
}

// VuBoatEntity slot 17 0x421980 / slot 18 0x421940 (boat entity.hxx
// boat_enable / boat_disable: VuDynamicsImpl slots 15 / 11 and 16 / 12 -
// manager.hxx) on the race's dynamics world. The body's collision object is
// its compound (0x420A70's +0xCC, boat load.hxx io.compound) under the body's
// transform, its processing threshold the body's +0xC4; its motion state
// (+0x1F4 = boat +0x5C) is boat_motion_set 0x41E460 on the boat's transform
// component (+0x50). The +0x60 interface's step-callback slots: slot 0
// 0x410B30 (a ret 8 - nothing), slot 1 tickThunk 0x41E5E0 - NOT WIRED (its
// hull / fluid / engine contexts are not composed from the race's boat
// images): it stops the dynamics frame (world/dynamics.hxx).
inline void race_session_enable( race_session_t& s, race_session_boat_t& b, bool on )
{
    if ( !s.dynamics )
        return;
    race_dynamics_t& d = *s.dynamics;
    std::uint8_t* boat = b.graph.objects[ 0 ].image.data( );
    void* tick = boat + 0x60;
    if ( !b.sections )
    {
        race_unwired( s, "the boat's slot 17 / 18 on a boat whose sections load stopped (no shape) - not run (UNKNOWN)" );
        return;
    }
    if ( d.objects.find( &b.body ) == d.objects.end( ) )
    {
        float pth;
        std::memcpy( &pth, reinterpret_cast< const std::uint8_t* >( &b.body ) + 0xC4, 4 );
        b.collision = collision_object_t{ };
        b.collision.body = concave_body_t{ b.body.body.basis, nullptr, nullptr, &b.body.body, &b.io.compound, pth };
        d.objects[ &b.body ] = &b.collision;
        std::uint8_t* transform = race_session_image( b, 0, 0x50 );
        d.motion[ &b.body.body ] = [ &s, boat, transform ]( const float* basis, const float* origin ) {
            if ( !transform || !boat_motion_set( boat, transform, false, basis, origin ) )
                race_unwired( s, "the boat's setWorldTransform 0x41E460 with no transform image - UNKNOWN, not written" );
        };
        race_step_callback_t cb;
        cb.slot0 = []( float, bool ) {};   // 0x410B30: ret 8
        // Slot 1 tickThunk 0x41E5E0: the boat agent's builder (boat/thunk.hxx) on this boat's images, one persistent
        // boat_thunk_t per boat (the solver's records live in it). The water is the world's query view of VuWater,
        // refreshed per frame (none: UNKNOWN, not stepped); the gravity g_vudynamicsimpl vt+0x1C = the world's.
        // A VuWater wave pointer in the images (hull +0x1DC / +0x1E0, the solver's excluded array - the wake
        // update's, drive.hxx) is a race_wave_token: its index in the built water; any other word is refused
        // (UNKNOWN, `why`).
        b.thunk = std::make_unique< boat_thunk_t >( );
        cb.slot1 = [ &s, &b, &d ]( float dt ) {
            boat_thunk_in_t in;
            std::uint8_t* boat_image = b.graph.objects[ 0 ].image.data( );
            const int hull_k = race_session_ref( b.graph, 0, 0x228 );
            in.boat = boat_image;
            in.hull = race_session_image( b, 0, 0x228 );
            in.solver = race_session_image( b, hull_k, 0x1CC );
            in.transform = race_session_image( b, 0, 0x50 );
            in.engine = &b.engine;
            in.body = &b.body.body;
            if ( !in.hull || !in.solver || !in.transform || !s.world.view )
            {
                race_unwired( s, "the boat's tickThunk 0x41E5E0: no hull / solver / transform image or no VuWater view - the step is UNKNOWN (not run)" );
                return;
            }
            in.fluids = b.meshes.fluids_of( load_word( in.solver, 0 ) );
            boat_thunk_water( in, *s.world.view );
            in.gravity = []( void* u, float* out ) {
                std::memcpy( out, static_cast< race_dynamics_t* >( u )->world.gravity, 12 );
                return true;
            };
            in.gravity_user = &d;
            in.wave_index = [ &s ]( std::uint32_t word ) { return race_wave_of( s.world.water, word ); };
            const std::int32_t excluded = static_cast< std::int32_t >( load_word( in.solver, 0x30 ) );
            const int solver_k = race_session_ref( b.graph, hull_k, 0x1CC );
            const std::uint8_t* array = race_session_image( b, solver_k, 0x34 );
            b.excluded.clear( );
            if ( excluded > 0 && array )
                for ( std::int32_t k = 0; k < excluded; ++k )
                    b.excluded.push_back( load_word( array, 4 * k ) );
            in.solver_excluded = b.excluded.empty( ) ? nullptr : b.excluded.data( );
            if ( !boat_thunk_step( *b.thunk, in, dt ) )
                race_unwired( s, "the boat's tickThunk 0x41E5E0: " + b.thunk->why );
        };
        d.callbacks[ tick ] = cb;
    }
    const bool ok = on ? boat_enable( d.impl, boat[ 0x2A0 ], b.body, tick ) : boat_disable( d.impl, boat[ 0x2A0 ], b.body, tick );
    if ( !ok )
        race_unwired( s, on ? "the boat's slot 17 0x421980 reached a dynamics path not modelled - UNKNOWN"
                            : "the boat's slot 18 0x421940 reached a dynamics path not modelled - UNKNOWN" );
}

// The level's gameInitialize as 0x4BA060 runs it after the boat load (0x4BA322:
// VuEntity::gameInitialize 0x500B10 on the project root): post-order - each
// entity's children first (+0x38 order), then its own vt slot 6, skipped when
// +0x14 bit 2 (already initialised) is set; before that, unless +0x14 bit 3,
// g 0x759080 vt+0x20 (the entity registration - a hook here) and each
// component's vt+0x24 (VuRigidBodyComponent's: a nop). RECREATED: VuPropEntity's
// slot 6 0x546910 (world/bodies.hxx) - the level's static bodies into the race's
// dynamics world (g_vudynamicsimpl slot 15). Every other type's slot 6 is not
// recreated: counted, logged once (UNKNOWN). The collision mesh assets come
// from the pack (boat meshes.hxx's library - its factory substitution; retail's
// asset property creates them at load, 0x545200, here at the build). The
// VuRigidBody ids (the debug global 0x759030) continue from one per boat
// (FLAGGED: the boat ctor's own count is the boat agent's, seeded 0).
// The zones' "Build" tick 0x5A7210 (timer -= dt).
inline void race_session_gfx_tick( void* user, float dt )
{
    gfx_zone_tick( *static_cast< gfx_zone_t* >( user ), dt );
}

// The gfx-settings entities' slot 6 in the level walk: VuGfxSettingsEntity's
// 0x5A71B0 (VuGlobalGfxSettingsEntity: the "Build" tick 0x5A7210 registered
// unless Always Active) and the zones' 0x5D21B0 (the same, then pushed onto
// the blender's list g 0x759488 / 0x75948C). The blend view: the +0x70 record
// = the targets' +0x70 .. +0xC4 words, the timer +0xC8 = 0 (the base ctor), the
// transform component's world / scale. True when `e` is one of the three
// (UNKNOWN - logged, not built - without its state or a known world).
inline bool race_session_gfx_game_initialize( race_session_t& s, entity_t& e )
{
    const bool rect = e.m_type == "VuRectangularGfxSettingsEntity", ellip = e.m_type == "VuEllipticalGfxSettingsEntity";
    if ( !rect && !ellip && e.m_type != "VuGlobalGfxSettingsEntity" )
        return false;
    const transform_component_t* t = entity_transform( e );
    if ( !e.m_state || ( ( rect || ellip ) && ( !t || e.m_world_unknown ) ) )
    {
        race_unwired( s, "a gfx-settings entity without its state / known world - its gameInitialize not run (UNKNOWN)" );
        return true;
    }
    auto z = std::make_unique< gfx_zone_t >( );
    const gfx_settings_t& g = rect || ellip ? static_cast< gfx_zone_state_t* >( e.m_state.get( ) )->value
                                            : static_cast< gfx_settings_state_t* >( e.m_state.get( ) )->value;
    const gfx_zone_state_t* zs = rect || ellip ? static_cast< gfx_zone_state_t* >( e.m_state.get( ) ) : nullptr;
    gfx_zone_view( *z, g, ellip ? gfx_zone_kind_t::elliptical : gfx_zone_kind_t::rectangular, zs ? zs->inner : nullptr, t );
    if ( !z->always_active )
        tick_register( s.ticks, { z.get( ), race_session_gfx_tick, z.get( ) }, "Build" );
    if ( rect || ellip )
        s.blender.zones.push_back( z.get( ) );
    s.gfx_by_path[ race_entity_path( &e ) ] = { z.get( ), !( rect || ellip ) };
    s.gfx_entities.push_back( std::move( z ) );
    return true;
}

// VuParams' float add 0x405120: a {2, value} record (dropped past the 0x80 bytes).
inline void race_dyn_prop_param( std::uint8_t* params, std::int32_t& size, float v )
{
    if ( size + 8 > 0x80 )
        return;
    const std::uint32_t type = 2u;
    std::memcpy( params + size, &type, 4 );
    std::memcpy( params + size + 4, &v, 4 );
    size += 8;
}

// THE DECISION TICK of a dynamic game prop (0x4077E8 -> vt+0x40 slot 16 0x438260), read whole from the disassembly:
// +0x17C += dt (addss dt, +0x17C). A pending hit (+0x164, set by the contact callback 0x437F40 - world/dynprop.hxx
// dynamic_prop_contact): with a target (+0x174) a VuParams {Shake Magnitude, Duration, Falloff, Frequency} (+0x128
// .. +0x134, 0x405120 each) goes to the target's event map (+0xC, 0x5155C0) as FNV-1a "OnCameraShake" (0x685E80),
// then {Rumble Low, High} (+0x138 / +0x13C, the size re-zeroed) as "OnRumble" (0x685E90) - a session boat's map
// holds 0x41E7B0 / 0x41E950 (boat events.hxx boat_on_camera_shake on its camera +0x224, boat_on_rumble on its driver
// +0x220). Then with an Sfx Name (+0x120 != 0) FMOD's getEvent / set3DAttributes(transform +0x80) / "intensity"
// (0x685E9C) = +0x178 / start / release (imports 0x64C348 .. 0x64C358 - the audio PLATFORM, a hook). +0x164 = 0, the
// weak ref +0x168 released (vt+0). A pending splash (+0x181): cleared; createEffect 0x52BCD0 on g 0x75905C by the
// Splash Pfx Name (pfxmgr.hxx pfx_manager_create), the entry's serial bumped (pfx_manager_handle), its instance's
// slot 10 (fstp: discarded); a nonzero handle that looks up (pfx_manager_lookup) -> the instance's slot 0
// setPosition(transform +0x80) and slot 11 start.
inline void race_session_dyn_prop_decision( void* user, float dt )
{
    race_session_t::dyn_prop_t& d = *static_cast< race_session_t::dyn_prop_t* >( user );
    race_session_t& s = *d.session;
    d.timer = sse_add( dt, d.timer );
    if ( d.hit.pending )
    {
        if ( d.hit.target )
        {
            race_session_boat_t* boat = nullptr;
            for ( const auto& b : s.boats )
                if ( reinterpret_cast< const std::uint8_t* >( &b->body.body ) == d.hit.target )
                    boat = b.get( );
            std::uint8_t shake[ 0x84 ] = { }, rumble[ 0x84 ] = { };
            std::int32_t n = 0;
            race_dyn_prop_param( shake, n, d.p->shake_magnitude );
            race_dyn_prop_param( shake, n, d.p->shake_duration );
            race_dyn_prop_param( shake, n, d.p->shake_falloff );
            race_dyn_prop_param( shake, n, d.p->shake_frequency );
            std::memcpy( shake + 0x80, &n, 4 );
            n = 0;
            race_dyn_prop_param( rumble, n, d.p->rumble_low );
            race_dyn_prop_param( rumble, n, d.p->rumble_high );
            std::memcpy( rumble + 0x80, &n, 4 );
            if ( boat && boat->io.camera && boat->driver && !boat->driver->graph.empty( ) )
            {
                boat_on_camera_shake( boat->io.camera, shake );
                std::uint8_t* drv = boat->driver->graph[ 0 ].image.data( );
                boat_on_rumble( drv, load_word( drv, 0 ), rumble );
                LOG_VERBOSE( "race: dynamic prop '%s' hit %s: OnCameraShake / OnRumble (strength %.3f)", d.entity->m_name.c_str( ), boat->name.c_str( ),
                             d.hit.strength );
            }
            else
                race_unwired( s, "a dynamic prop's hit target is no session boat (its event map's OnCameraShake / OnRumble) - UNKNOWN, not run" );
        }
        if ( !d.p->sfx.empty( ) )
            race_unwired( s, "audio: a dynamic prop's hit Sfx (FMOD getEvent / set3DAttributes / intensity / start - platform)" );
        d.hit.pending = 0;
        d.hit.target = nullptr;
    }
    if ( d.live.splash )
    {
        d.live.splash = 0;
        if ( !s.world.pfx || !s.world.pfx_manager || !s.world.pfx_ops )
        {
            race_unwired( s, "a dynamic prop's splash 0x52BCD0 with no VuPfx runtime / manager - UNKNOWN, not run" );
            return;
        }
        pfx_manager_t& m = *s.world.pfx_manager;
        const int e = pfx_manager_create( m, d.p->splash_pfx.c_str( ), *s.world.pfx_ops );
        if ( e < 0 )
            return;
        const boat_pfx_ops_t o = boat_pfx_runtime_ops( *s.world.pfx );
        const std::uint32_t h = pfx_manager_handle( m, e );
        if ( o.duration )
            o.duration( o.user, m.entries[ static_cast< std::size_t >( e ) ].instance );
        if ( h == 0u )
            return;
        const int x = pfx_manager_lookup( m, h );
        const transform_component_t* t = entity_transform( *d.entity );
        if ( x < 0 || !t )
            return;
        const std::uint32_t inst = m.entries[ static_cast< std::size_t >( x ) ].instance;
        o.set_position( o.user, inst, t->world + 12 );
        o.start( o.user, inst );
        ++s.prop_splashes;
        LOG_VERBOSE( "race: dynamic prop '%s' splash '%s' started", d.entity->m_name.c_str( ), d.p->splash_pfx.c_str( ) );
    }
}

// The Build tick 0x546130 (thiscall; dt; retn 4 - dt unread), read whole: matrix = VuMatrix_multiply(
// VuMatrix_fromScale 0x407DB0(transform +0xB0), out, transform +0x50) (transform_draw_matrix) into the static model
// component (+0x6C) +0x13C, then 0x531D10(component, the LOD 0 instance's (+0xCC) scene +0x5C aabb or the zero aabb
// 0x7593F0, +0x13C) - the world's (race_world_t::prop_draw).
inline void race_session_dyn_prop_build( void* user, float )
{
    race_session_t::dyn_prop_t& d = *static_cast< race_session_t::dyn_prop_t* >( user );
    if ( d.session->world.prop_draw )
        d.session->world.prop_draw( *d.entity );
    else
        race_unwired( *d.session, "a dynamic prop's Build tick 0x546130 with no renderer hand-off (prop_draw) - not drawn from its body" );
}

// The mine's hide vt+0x34 0x438220: the step callback out of g_vudynamicsimpl (vt+0x30), then 0x545F20 - shown (+0x8C)
// -> 0, the draw component off the draw manager (the renderer's prop_drawn), the body out of the world when added.
inline void race_session_mine_hide( race_session_t::dyn_prop_t& d )
{
    race_session_t& s = *d.session;
    if ( d.step_added )
    {
        vu_dynamics_remove_step_callback( s.dynamics->impl, &d.step_tag );
        d.step_added = false;
    }
    if ( !d.init.shown )
        return;
    d.init.shown = 0;
    if ( s.world.prop_drawn )
        s.world.prop_drawn( *d.entity, false );
    else
        race_unwired( s, "the mine's hide 0x545F20: no renderer hand-off (prop_drawn) - still drawn" );
    if ( d.body->in_world )
    {
        vu_dynamics_remove_rigid_body( s.dynamics->impl, d.body->body );
        d.body->in_world = false;
    }
}

// The mine's Decision tick 0x438EA0 (world/mine.hxx mine_tick): the hide; the explosion part 0x547BB0 - its params
// (+0 the position: the transform's row 3, +0x10 .. +0x30 the nine Explosion floats) to the explosion manager's add
// 0x510180 (explosion_add over the human listeners), the pfx 0x52BCD0 by "Explosion Pfx System Name" (slot 10 discarded,
// slot 2 setMatrix( the transform world ), slot 11 start) and FMOD's event (platform); the point wave 0x528E10 on the
// world's VuWater (its returned reference dropped: 2 -> 1); "OnExplode".
inline void race_session_mine_tick( void* user, float )
{
    race_session_t::dyn_prop_t& d = *static_cast< race_session_t::dyn_prop_t* >( user );
    race_session_t& s = *d.session;
    const transform_component_t* t = entity_transform( *d.entity );
    if ( !t || d.entity->m_world_unknown )
    {
        if ( !d.exploded && d.request )
            race_unwired( s, "a mine's tick 0x438EA0 on an entity whose world is unknown - UNKNOWN, not run" );
        return;
    }
    mine_tick_hooks_t h;
    h.hide = [ &d ] { race_session_mine_hide( d ); };
    h.explode = [ &s, &d ]( const float* m ) {
        std::uint8_t params[ 0x38 ] = { };
        std::memcpy( params, m + 12, 16 );
        std::memcpy( params + 0x10, d.p->explosion, 36 );
        s.explosion_hits += explosion_add( params, s.explosion_listeners, hull_sqrt );
        if ( !d.p->explosion_pfx.empty( ) && s.world.pfx && s.world.pfx_manager && s.world.pfx_ops )
        {
            pfx_manager_t& pm = *s.world.pfx_manager;
            const int e = pfx_manager_create( pm, d.p->explosion_pfx.c_str( ), *s.world.pfx_ops );
            if ( e >= 0 )
            {
                const boat_pfx_ops_t o = boat_pfx_runtime_ops( *s.world.pfx );
                const std::uint32_t hd = pfx_manager_handle( pm, e );
                if ( o.duration )
                    o.duration( o.user, pm.entries[ static_cast< std::size_t >( e ) ].instance );
                const int x = hd ? pfx_manager_lookup( pm, hd ) : -1;
                if ( x >= 0 )
                {
                    const std::uint32_t inst = pm.entries[ static_cast< std::size_t >( x ) ].instance;
                    o.set_matrix( o.user, inst, m );
                    o.start( o.user, inst );
                }
            }
        }
        else if ( !d.p->explosion_pfx.empty( ) )
            race_unwired( s, "a mine's explosion pfx 0x52BCD0 with no VuPfx runtime / manager - UNKNOWN, not run" );
        if ( !d.p->explosion_audio.empty( ) )
            race_unwired( s, "audio: a mine's explosion event (FMOD getEvent / set3DAttributes / start - platform)" );
    };
    h.point_wave = [ &s ]( const std::uint32_t* desc ) {
        if ( !s.world.water )
        {
            race_unwired( s, "a mine's point wave 0x528E10 with no VuWater - UNKNOWN, not made" );
            return;
        }
        water_create_point_wave( *s.world.water, desc );
        s.world.water->waves.back( ).set( 4, 1 );   // the mine's reference dropped (dec +4: 2 -> 1, not deleted)
    };
    h.on_explode = [ &s, &d ] {
        if ( s.world.fire )
            s.world.fire( *d.entity, "OnExplode" );
    };
    if ( mine_tick( d.exploded, d.request, t->world, d.p->wave, h ) )
    {
        ++s.mine_explosions;
        LOG_INFO( "race: mine '%s' exploded (sync %d) - %d listener hits so far", d.entity->m_name.c_str( ), s.dynamics ? s.dynamics->syncs : -1,
                  s.explosion_hits );
    }
}

// The keyframe motion's hooks (world/keymotion.hxx): the outputs through the world's graph, the tick registration as the
// session holds it (registered once at game init; the tick runs slot 14 only while active, so an unregistered state is
// the inactive one), find = the ref's VuMotionComponent (the target set up at game init), the unrecreated count.
inline key_motion_hooks_t race_session_key_hooks( race_session_t::key_live_t& k )
{
    key_motion_hooks_t h;
    h.user = &k;
    h.fire = []( void* u, key_motion_t&, const char* plug ) {
        auto& x = *static_cast< race_session_t::key_live_t* >( u );
        if ( x.session->world.fire )
            x.session->world.fire( *x.entity, plug );
    };
    h.tick_register = []( void*, key_motion_t&, bool ) { };
    h.find = []( void* u, key_motion_t& ) -> key_motion_target_t* {
        auto& x = *static_cast< race_session_t::key_live_t* >( u );
        return x.target_entity ? &x.target : nullptr;
    };
    h.unrecreated = &k.session->key_unrecreated;
    return h;
}

// The "Motion" tick 0x61A6E0 -> slot 14 0x5CFE60 while active.
inline void race_session_key_tick( void* user, float dt )
{
    auto& k = *static_cast< race_session_t::key_live_t* >( user );
    const int before = k.session->key_unrecreated;
    key_motion_tick( *k.k, dt, race_session_key_hooks( k ) );
    if ( k.session->key_unrecreated != before )
        race_unwired( *k.session, "a keyframe motion reached a path not recreated (Curve Type / the key search below every key) - UNKNOWN" );
}

// VuKeyframeMotionEntity's slot 6 0x5CE9E0 (key_motion_game_init): its VuKeyframeEntity children in order (Time, the
// transform world), the "Entity" ref (its script component's ref) -> the target entity's VuMotionComponent (vtable
// 0x74E984; 0x5F5810 answers null without one). The target's IF (VuAnimatedPropEntity's 0x756228 / the props' 0x73A13C
// shape): slot 0 = setWorldTransform of the owner (race.hxx entity_set_world, notify) then the owner's draw matrix
// re-made (prop_moved: the animated component 0x61DA50 / the static model's); the rigid component's kinematic step
// 0x544C50 and the +0x38 call are NOT recreated (counted); slots 1 / 2 (0x546D80 / 0x546DB0: the rigid component's +0xD0
// bit 1 set / cleared and copied to its body) - no body is built for an animated prop here (UNKNOWN when one is).
inline void race_session_key_motion( race_session_t& s, entity_t& e )
{
    key_motion_state_t* st = static_cast< key_motion_state_t* >( e.m_state.get( ) );
    if ( st->curve_authored )
    {
        race_unwired( s, "a VuKeyframeMotionEntity authoring Curve Type (VuStaticIntEnumProperty 0x74C354's read 0x503600 unread) - UNKNOWN, not run" );
        return;
    }
    auto k = std::make_unique< race_session_t::key_live_t >( );
    k->entity = &e;
    k->k = &st->value;
    k->session = &s;
    bool linked = false;
    if ( const script_component_t* sc = script_of( e ) )
        for ( const script_ref_t& ref : sc->m_refs )
            if ( std::string( ref.m_row->m_name ) == "Entity" && ref.m_target )
            {
                linked = true;
                for ( const entity_component_t& c : ref.m_target->m_components )
                    if ( c.m_vtable == 0x74E984u )
                        k->target_entity = ref.m_target;
            }
    if ( k->target_entity )
    {
        const transform_component_t* tt = entity_transform( *k->target_entity );
        k->target.transform = tt ? tt->world : nullptr;
        k->target.user = k.get( );
        k->target.update = []( void* u, const float* m, const float*, const float* ) {
            auto& x = *static_cast< race_session_t::key_live_t* >( u );
            race_session_t& rs = *x.session;
            if ( !entity_set_world( *x.target_entity, m, true, { } ) )
            {
                race_unwired( rs, "a keyframe motion's target whose world is not modelled - setWorldTransform UNKNOWN, not written" );
                return;
            }
            if ( rs.world.prop_moved )
                rs.world.prop_moved( *x.target_entity );
            race_unwired( rs, "a motion target's rigid component kinematic step 0x544C50 / its +0x38 call (not recreated)" );
        };
    }
    std::vector< key_motion_child_t > kids;
    for ( auto& c : e.m_children )
    {
        key_motion_child_t kc;
        kc.keyframe = c->m_type == "VuKeyframeEntity";
        if ( kc.keyframe && c->m_state )
        {
            const float t = static_cast< keyframe_state_t* >( c->m_state.get( ) )->time;
            std::memcpy( &kc.time, &t, 4 );
        }
        if ( const transform_component_t* ct = entity_transform( *c ) )
            std::memcpy( kc.transform, ct->world, 64 );
        kids.push_back( kc );
    }
    race_session_t::key_live_t* kp = k.get( );
    s.key_motions.push_back( std::move( k ) );
    tick_register( s.ticks, { kp, race_session_key_tick, kp }, "Motion" );
    key_motion_game_init( *kp->k, kids, linked, race_session_key_hooks( *kp ) );
}

// VuBreakablePropEntity's Decision tick 0x434A20 (dt), read whole (world/breakable.hxx's head): the breakable
// component's 0x543640(dt) - its debris fade timer (+0xE8, state +0xE4 1 -> 2 below 0, the debris draw removed): the
// component's own properties and debris are the renderer's (0x51E790 / 0x5436B0 not recreated), so only the state is
// kept here. With a request (+0xD0): unless the component's state is already non-zero - 0x546A20 (the static draw off,
// the body out of the world when added), the debris start 0x51E790 (UNKNOWN: the renderer's), state 1, the draw add;
// with a target: "OnBreak" (its entity parameter not carried by our fire), "OnCameraShake" {Magnitude, Duration,
// Falloff, Frequency} and "OnRumble" {Low, High} to the target's event map (0x501160; a session boat's: boat events.hxx);
// the pfx by Pfx Name (0x52BCD0; slot 10 discarded; slot 2 setMatrix the transform world, slot 3 the kept velocity
// +0xD4 (its 4th word UNKNOWN: 0 here), slot 11 start); FMOD by Sfx Name (platform). Then +0xD0 = 0 and the weak ref
// released either way.
inline void race_session_breakable_tick( void* user, float )
{
    race_session_t::breakable_live_t& b = *static_cast< race_session_t::breakable_live_t* >( user );
    race_session_t& s = *b.session;
    if ( !b.request )
        return;
    if ( b.state == 0 )
    {
        if ( s.world.prop_drawn )
            s.world.prop_drawn( *b.entity, false );
        if ( b.body->in_world )
        {
            vu_dynamics_remove_rigid_body( s.dynamics->impl, b.body->body );
            b.body->in_world = false;
        }
        race_unwired( s, "a breakable's debris 0x51E790 / its draw (Vu3dDrawBreakableModelComponent - the renderer's, not recreated)" );
        b.state = 1;
        ++s.breaks;
        if ( b.target )
        {
            if ( s.world.fire )
                s.world.fire( *b.entity, "OnBreak" );
            race_session_boat_t* boat = nullptr;
            for ( const auto& x : s.boats )
                if ( &x->body.body == b.target )
                    boat = x.get( );
            if ( boat && boat->io.camera && boat->driver && !boat->driver->graph.empty( ) )
            {
                std::uint8_t shake[ 0x84 ] = { }, rumble[ 0x84 ] = { };
                std::int32_t n = 0;
                for ( int k = 0; k < 4; ++k )
                    race_dyn_prop_param( shake, n, b.p->shake[ k ] );
                std::memcpy( shake + 0x80, &n, 4 );
                n = 0;
                race_dyn_prop_param( rumble, n, b.p->rumble[ 0 ] );
                race_dyn_prop_param( rumble, n, b.p->rumble[ 1 ] );
                std::memcpy( rumble + 0x80, &n, 4 );
                boat_on_camera_shake( boat->io.camera, shake );
                std::uint8_t* drv = boat->driver->graph[ 0 ].image.data( );
                boat_on_rumble( drv, load_word( drv, 0 ), rumble );
            }
        }
        const transform_component_t* t = entity_transform( *b.entity );
        if ( !b.p->pfx.empty( ) && s.world.pfx && s.world.pfx_manager && s.world.pfx_ops && t )
        {
            pfx_manager_t& pm = *s.world.pfx_manager;
            const int e = pfx_manager_create( pm, b.p->pfx.c_str( ), *s.world.pfx_ops );
            if ( e >= 0 )
            {
                const boat_pfx_ops_t o = boat_pfx_runtime_ops( *s.world.pfx );
                const std::uint32_t hd = pfx_manager_handle( pm, e );
                if ( o.duration )
                    o.duration( o.user, pm.entries[ static_cast< std::size_t >( e ) ].instance );
                const int x = hd ? pfx_manager_lookup( pm, hd ) : -1;
                if ( x >= 0 )
                {
                    const std::uint32_t inst = pm.entries[ static_cast< std::size_t >( x ) ].instance;
                    std::uint32_t v[ 4 ] = { };
                    std::memcpy( v, b.velocity, 12 );
                    o.set_matrix( o.user, inst, t->world );
                    o.set_velocity( o.user, inst, v );
                    o.start( o.user, inst );
                }
            }
        }
        if ( !b.p->sfx.empty( ) )
            race_unwired( s, "audio: a breakable's Sfx (FMOD getEvent / set3DAttributes / start - platform)" );
        LOG_INFO( "race: breakable '%s' broke (sync %d)%s", b.entity->m_name.c_str( ), s.dynamics ? s.dynamics->syncs : -1, b.target ? " - by a boat" : "" );
    }
    b.request = 0;
    b.target = nullptr;
}

// One dynamic game prop's gameInitialize (world/dynprop.hxx, verified by the cases dynamic_prop_bodies /
// dynamic_prop_step) made live: postLoad 0x5459F0's component step (retail runs it at load; nothing reads the
// component in between), 0x437580, the body into the dynamics world (vt+0x3C), the motion state's
// setWorldTransform (vt+8 0x545CF0: world = translation(-centre) x 0x410B40(the body) -> the transform's
// setWorldTransform steps, entity_set_world; its last call, the +0x68 attach component's vt+0x38, is a hook not
// run), the step callback (vt+0x2C: slot 0 0x410B30 a ret 8, slot 1 0x437730 with the world's water) and the
// Decision tick. The show's anchor (+0x144 = the world's translation, +0x154 = 0x531000 of it). The Build tick
// 0x546130 (the draw matrix for the renderer) is not registered: the renderer is not fed from it (a hook).
inline bool race_session_dynamic_prop( race_session_t& s, entity_t& e, const bodies_hooks_t& hooks, std::uint32_t& id )
{
    race_dynamics_t& dyn = *s.dynamics;
    rigid_component_state_t* rs = nullptr;
    for ( entity_component_t& c : e.m_components )
        if ( c.m_vtable == k_rigid_body_component_vtable )
            rs = static_cast< rigid_component_state_t* >( c.m_state.get( ) );
    transform_component_t* t = entity_transform( e );
    if ( !rs || !t || !e.m_state || e.m_world_unknown )
    {
        race_unwired( s, "a dynamic game prop without its component state / known world - gameInitialize 0x437580 not run (UNKNOWN)" );
        return false;
    }
    auto d = std::make_unique< race_session_t::dyn_prop_t >( );
    d->entity = &e;
    d->session = &s;
    d->p = &static_cast< dynamic_prop_state_t* >( e.m_state.get( ) )->value;
    d->buoy = static_cast< dynamic_prop_state_t* >( e.m_state.get( ) )->buoy;
    d->mine = static_cast< dynamic_prop_state_t* >( e.m_state.get( ) )->mine;
    if ( d->mine )
        d->p->hard_hit = 0;   // the mine's slot 6 0x438CF0 begins with +0x108 = 0, before 0x437580
    d->c = &rs->value;
    dynamic_prop_post_load( *d->p, *d->c );
    const collision_mesh_asset_t* asset = nullptr;
    if ( ( d->c->collision_type == 6 || d->c->collision_type == 7 ) && !d->c->mesh_asset.empty( ) &&
         boat_mesh_exists( &s.level_meshes, "VuCollisionMeshAsset", d->c->mesh_asset ) )
        asset = s.level_meshes.collision_of( boat_mesh_create( &s.level_meshes, "VuCollisionMeshAsset", d->c->mesh_asset ) );
    auto b = std::make_unique< level_body_t >( );
    b->name = e.m_name;
    d->body = b.get( );
    std::string why;
    if ( !dynamic_prop_game_initialize( *b, *d->c, *d->p, *t, asset, id, hooks, d->motion, d->init, why ) )
    {
        race_unwired( s, e.m_type + " '" + e.m_name + "': " + why + " (UNKNOWN, no body)" );
        return false;
    }
    std::memcpy( d->live.anchor, t->world + 12, 12 );
    quat_from_rows( d->live.anchor_q, t->world );
    level_body_t* lb = b.get( );
    race_session_t::dyn_prop_t* dp = d.get( );
    // The mine's rest of 0x438CF0 (world/mine.hxx): retail sets group 0x80 on the body and its broadphase proxy after
    // 0x437580 added it; here it is written before the add below, which builds the proxy from the body's +0x2CA - the
    // same end state (ORDER OURS, flagged).
    if ( d->mine )
        mine_game_initialize( *d->c, reinterpret_cast< std::uint8_t* >( &lb->body ), nullptr );
    if ( d->init.added )
    {
        dyn.objects[ &lb->body ] = &lb->collision;
        if ( !vu_dynamics_add_rigid_body( dyn.impl, lb->body ) )
            race_unwired( s, "addRigidBody (g_vudynamicsimpl vt+0x3C) refused a dynamic prop's body" );
    }
    dyn.motion[ &lb->body.body ] = [ &s, dp ]( const float* basis, const float* origin ) {
        float m[ 16 ], tr[ 16 ], world[ 16 ];
        for ( int row = 0; row < 3; ++row )
        {
            for ( int k = 0; k < 3; ++k )
                m[ 4 * row + k ] = basis[ 4 * k + row ];
            m[ 4 * row + 3 ] = 0.0f;
        }
        std::memcpy( m + 12, origin, 12 );
        m[ 15 ] = 1.0f;
        vu_identity( tr );
        tr[ 12 ] = dp_neg( dp->c->centre[ 0 ] );
        tr[ 13 ] = dp_neg( dp->c->centre[ 1 ] );
        tr[ 14 ] = dp_neg( dp->c->centre[ 2 ] );
        hull_multiply( tr, world, m );
        if ( !entity_set_world( *dp->entity, world, false, { } ) )
            race_unwired( s, "a dynamic prop's setWorldTransform 0x545CF0 on an entity whose world is not modelled - UNKNOWN, not written" );
    };
    if ( d->init.step_callback )
    {
        void* tag = &dp->step_tag;
        race_step_callback_t cb;
        cb.slot0 = []( float, bool ) {};   // 0x410B30: ret 8
        cb.slot1 = [ &s, dp ]( float ) {
            const dynamic_prop_water_fn water = [ &s ]( const float* point, water_vertex_t& out ) {
                out = water_vertex_t{ };
                return s.world.view && water_point_height( s.world.view->water, point, water_view_kernel, const_cast< water_view_t* >( s.world.view ), out );
            };
            if ( !dynamic_prop_step( *dp->p, dp->live, dp->c->centre, reinterpret_cast< std::uint8_t* >( &dp->body->body ), water ) )
                race_unwired( s, "a dynamic prop's step callback 0x437730: the water query 0x529140 reached a path not recreated / no VuWater view - its forces UNKNOWN" );
        };
        dyn.callbacks[ tag ] = cb;
        if ( !vu_dynamics_add_step_callback( dyn.impl, tag ) )
            race_unwired( s, "g_vudynamicsimpl vt+0x2C refused a dynamic prop's step callback" );
        else
            dp->step_added = true;
    }
    // The Decision delegate (0x4077E8 -> vt+0x40) reaches the MINE's 0x438EA0 for a mine; the mine then registers it
    // once more itself (0x438D73) - two registrations when 0x437580 made one.
    if ( d->init.decision_tick )
        tick_register( s.ticks, { dp, d->mine ? race_session_mine_tick : race_session_dyn_prop_decision, dp }, "Decision" );
    if ( d->mine )
        tick_register( s.ticks, { dp, race_session_mine_tick, dp }, "Decision" );
    if ( d->init.build_tick )
        tick_register( s.ticks, { dp, race_session_dyn_prop_build, dp }, "Build" );
    s.level_bodies.push_back( std::move( b ) );
    s.dyn_props.push_back( std::move( d ) );
    return true;
}

inline void race_session_level_bodies( race_session_t& s, entity_t& tree, const c_archive& archive )
{
    if ( !s.dynamics )
        return;
    race_dynamics_t& d = *s.dynamics;
    s.level_meshes.read_user = const_cast< c_archive* >( &archive );
    s.level_meshes.read = []( void* u, const std::string& asset, std::vector< std::uint8_t >& blob ) {
        return static_cast< const c_archive* >( u )->read( asset.c_str( ), blob );
    };
    s.level_meshes.surfaces = s.surfaces_built ? &s.surfaces : nullptr;
    bodies_hooks_t hooks;
    hooks.user = &s;
    hooks.surface = []( void* u, const std::string& n ) -> std::uint8_t {
        race_session_t& r = *static_cast< race_session_t* >( u );
        return r.surfaces_built ? surface_id( r.surfaces, n.c_str( ) ) : 0;
    };
    hooks.gravity = []( void* u, float* g ) { std::memcpy( g, static_cast< race_session_t* >( u )->dynamics->world.gravity, 12 ); };
    std::uint32_t id = static_cast< std::uint32_t >( s.boats.size( ) );
    std::map< std::string, int > unknown;
    int added = 0, dynamic = 0;
    const auto walk = [ & ]( const auto& self, entity_t& e ) -> void
    {
        for ( auto& c : e.m_children )
            self( self, *c );
        if ( race_session_gfx_game_initialize( s, e ) )
            return;
        if ( dynamic_prop_class( e.m_type ) )   // the mine's slot 6 0x438CF0 included (race_session_dynamic_prop)
        {
            if ( race_session_dynamic_prop( s, e, hooks, id ) )
                ++dynamic;
            return;
        }
        // The logic entities' slot 6 (world/logic.hxx): VuSwitchEntity's 0x5E24E0 (state = Initial State); every other
        // logic class's slot 6 is 0x401560 (a bare ret - read from each vtable's +0x18).
        if ( logic_state_t* ls = logic_state_of( e ) )
        {
            if ( e.m_type == "VuSwitchEntity" )
                logic_switch_init( ls->value );
            return;
        }
        // VuLensWaterEmitterEntity's slot 6 0x5E6720 (world/lens.hxx): its emitter IF to the lens-water manager.
        if ( e.m_type == "VuLensWaterEmitterEntity" )
        {
            lens_water_hooks_t lh;
            lh.user = &s;
            lh.add = []( void* u, const void* emitter ) { static_cast< race_session_t* >( u )->lens_emitters.push_back( emitter ); };
            lens_water_emitter_start( &e, lh );
            return;
        }
        if ( e.m_type == "VuKeyframeMotionEntity" && e.m_state )
        {
            race_session_key_motion( s, e );
            return;
        }
        // VuAudioEmitterEntity's slot 6 0x5F3B30 (world/sound.hxx audio_emitter_game_init: plays when Initially Active).
        if ( e.m_type == "VuAudioEmitterEntity" && e.m_state )
        {
            const transform_component_t* et = entity_transform( e );
            if ( !s.world.sound || !et || e.m_world_unknown )
                race_unwired( s, "a VuAudioEmitterEntity's gameInitialize 0x5F3B30 with no audio platform / known world - not run" );
            else
                audio_emitter_game_init( static_cast< audio_emitter_state_t* >( e.m_state.get( ) )->value, et->world + 12, *s.world.sound );
            return;
        }
        const bool breakable = e.m_type == "VuBreakablePropEntity";
        if ( e.m_type != "VuPropEntity" && !breakable )
        {
            ++unknown[ e.m_type.empty( ) ? std::string( "(UNKNOWN node)" ) : e.m_type ];
            return;
        }
        rigid_component_state_t* rs = nullptr;
        for ( entity_component_t& c : e.m_components )
            if ( c.m_vtable == k_rigid_body_component_vtable )
                rs = static_cast< rigid_component_state_t* >( c.m_state.get( ) );
        const transform_component_t* t = entity_transform( e );
        if ( !rs || !t || !e.m_state || e.m_world_unknown )
        {
            race_unwired( s, "a VuPropEntity without its component state / known world - its gameInitialize 0x546910 not run (UNKNOWN)" );
            return;
        }
        rigid_component_t& c = rs->value;
        const collision_mesh_asset_t* asset = nullptr;
        if ( !c.mesh_asset.empty( ) && boat_mesh_exists( &s.level_meshes, "VuCollisionMeshAsset", c.mesh_asset ) )
            asset = s.level_meshes.collision_of( boat_mesh_create( &s.level_meshes, "VuCollisionMeshAsset", c.mesh_asset ) );
        auto b = std::make_unique< level_body_t >( );
        b->name = e.m_name;
        b->owner = &e;
        level_body_t* bp = b.get( );
        std::string why;
        const std::uint8_t visible = breakable ? static_cast< breakable_state_t* >( e.m_state.get( ) )->value.visible
                                               : static_cast< prop_entity_state_t* >( e.m_state.get( ) )->visible;
        // The breakable's ctor 0x433F80 ends with the rigid component's +0xF4 = its +0x7C part (the contact callback) and
        // +0xD8 |= 1 - component state the body is built from.
        if ( breakable )
        {
            c.word_f4 = 1;
            c.flags_d8 |= 1u;
        }
        const bool ok = bodies_prop_game_initialize( *bp, c, visible, asset, t->world, t->scale, id, hooks, [] { /* g 0x7590B8 vt+0x28: the renderer */ },
            [ & ]( level_body_t& lb ) {
                d.objects[ &lb.body ] = &lb.collision;
                return vu_dynamics_add_rigid_body( d.impl, lb.body );
            },
            why );
        if ( !ok )
            race_unwired( s, "VuPropEntity '" + e.m_name + "': " + why + " (UNKNOWN, no body)" );
        if ( bp->in_world )
            ++added;
        // VuBreakablePropEntity's slot 6 0x434620: 0x546910 above, then the "Decision" tick 0x434A20 and the "Build" tick
        // 0x434D90 (the debris draw 0x5436B0 - the renderer's: not registered).
        if ( breakable && ok )
        {
            auto br = std::make_unique< race_session_t::breakable_live_t >( );
            br->entity = &e;
            br->p = &static_cast< breakable_state_t* >( e.m_state.get( ) )->value;
            br->c = &c;
            br->body = bp;
            br->session = &s;
            tick_register( s.ticks, { br.get( ), race_session_breakable_tick, br.get( ) }, "Decision" );
            s.breakables.push_back( std::move( br ) );
        }
        s.level_bodies.push_back( std::move( b ) );
    };
    walk( walk, tree );
    LOG_INFO( "race: the level's gameInitialize: %d static bodies added to the dynamics world (VuPropEntity 0x546910), %d dynamic game props live (0x437580), %d lens-water emitters (0x5E6720)", added, dynamic,
              static_cast< int >( s.lens_emitters.size( ) ) );
    std::string rest;
    for ( const auto& u : unknown )
        rest += ( rest.empty( ) ? "" : ", " ) + u.first + " x" + std::to_string( u.second );
    if ( !rest.empty( ) )
        race_unwired( s, "the level's gameInitialize (0x500B10): slot 6 not recreated for " + rest + " (UNKNOWN)" );
}

inline void race_contact_counts( const race_session_t& s, int& calls, int& ran, int& meshes );   // world/drive.hxx

// DEV TRACE - NOT RETAIL (no retail function; only with --verbose, logs
// nothing else and writes nothing): at sync counts 1 / 10 / 30 / 60 / 120 /
// 240 / 480, each boat's transform world position (+0x50 row 3) and the water
// under it as the recreated queries answer - the 0x52AEC0 surface that holds
// the point (-1 none: between surfaces, e.g. the Hydroelectric spillway
// drop) and its height (+0x14), the 0x52ACB0 surface under it (the Build
// tick's), and 0x529140's out +0x20 (the height with its waves).
inline void race_session_dev_trace( const race_session_t& s, int before )
{
    const int n = s.dynamics->syncs;
    // Ours (dev trace, a test aid): --trace-boat N prints the human boats' transform position and forward row
    // (world +16) every N syncs - the input a waypoint-following --input-script is generated from.
    static const int every = [ ]( ) {
        const char* v = c_engine::option( "trace-boat" );
        return v ? std::atoi( v ) : 0;
    }( );
    if ( every > 0 && n != before && n == every && s.track )
        for ( std::size_t w = 0; w < s.track->waypoints.size( ); ++w )
        {
            const track_waypoint_t& p = s.track->waypoints[ w ];
            LOG_INFO( "boat trace: waypoint %d pos %.4f %.4f %.4f next %d %d width %.3f checkpoint %d", static_cast< int >( w ), p.position[ 0 ],
                      p.position[ 1 ], p.position[ 2 ], p.next_a, p.next_b, p.p.width, track_checkpoint_index( *s.track, static_cast< int >( w ) ) );
        }
    if ( every > 0 && n != before && n % every == 0 )
        for ( std::size_t k = 0; k < s.boats.size( ); ++k )
        {
            const std::uint8_t* t = race_session_image( *s.boats[ k ], 0, 0x50 );
            bool human = false;
            for ( const void* h : s.manager.lists.humans.boats )
                human = human || h == s.boats[ k ]->graph.objects[ 0 ].image.data( );
            if ( !t || !human )
                continue;
            float m[ 16 ];
            std::memcpy( m, t + 0x50, 64 );
            LOG_INFO( "boat trace: sync %d frame %d %s pos %.4f %.4f %.4f fwd %.5f %.5f %.5f", n, s.frames, s.boats[ k ]->name.c_str( ), m[ 12 ], m[ 13 ], m[ 14 ],
                      m[ 4 ], m[ 5 ], m[ 6 ] );
        }
    if ( !g_verbose || n == before ||
         !( n == 1 || n == 10 || n == 30 || n == 60 || n == 120 || n == 240 || n == 480 || n == 960 || n == 1920 || n == 3840 ) )
        return;
    {
        int calls = 0, ran = 0, meshes = 0;
        race_contact_counts( s, calls, ran, meshes );
        LOG_VERBOSE( "race (dev trace): sync %d: contact-added callbacks %d, 0x5751F0 run %d (%d with obj1's collision mesh); "
                     "VuCollisionManager impacts played %d (%d pending), scrape records %d", n, calls, ran, meshes, s.collision_impacts,
                     static_cast< int >( s.collision.impacts.count ), s.collision_scrapes );
    }
    if ( s.world.water )
    {
        const int wakes = water_live_wakes( *s.world.water );
        LOG_VERBOSE( "race (dev trace): sync %d: VuWater holds %d wave record(s), %d live wake / flat wake (water_live_wakes)", n,
                     static_cast< int >( s.world.water->waves.size( ) ), wakes );
    }
    // The live dynamic props (dev trace): each one's transform world position, in water, OnCheckPointBuoyHit so far.
    for ( const auto& dp : s.dyn_props )
        if ( const transform_component_t* t = entity_transform( *dp->entity ) )
            LOG_VERBOSE( "race (dev trace): sync %d: dynamic prop '%s' at (%.3f, %.3f, %.3f) in water %d (buoy hits so far %d)", n,
                         dp->entity->m_name.c_str( ), t->world[ 12 ], t->world[ 13 ], t->world[ 14 ], dp->live.in_water, s.buoy_hits );
    for ( const auto& b : s.boats )
    {
        const std::uint8_t* t = race_session_image( *b, 0, 0x50 );
        if ( !t )
            continue;
        float q[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
        std::memcpy( q, t + 0x50 + 48, 12 );
        if ( !s.world.view )
        {
            LOG_VERBOSE( "race (dev trace): sync %d: %s at (%.3f, %.3f, %.3f), no VuWater", n, b->name.c_str( ), q[ 0 ], q[ 1 ], q[ 2 ] );
            continue;
        }
        const water_t& w = s.world.view->water;
        const int holds = water_point_surface( w, q );
        const int under = water_surface_under( w, q, water_bits( 0xFEFFFFFFu ) );
        water_vertex_t o{};
        const bool known = water_point_height( w, q, water_view_kernel, const_cast< water_view_t* >( s.world.view ), o );
        LOG_VERBOSE( "race (dev trace): sync %d: %s at (%.3f, %.3f, %.3f) - water: holds surface %d (height %.1f), under %d, "
                     "0x529140 %s %.3f",
                     n, b->name.c_str( ), q[ 0 ], q[ 1 ], q[ 2 ], holds, holds >= 0 ? w.surfaces[ holds ].height : 0.0f, under,
                     known ? "" : "UNKNOWN", holds >= 0 ? o.height : 0.0f );
    }
}

// The transform watcher's 0x536350 (the instigator update, trigger/volume.hxx) for a boat of the session.
inline boat_transform_hooks_t race_session_transform_hooks( race_session_boat_t& b )
{
    boat_transform_hooks_t h;
    h.user = &b;
    h.instigator_update = []( void* u ) {
        race_session_boat_t& r = *static_cast< race_session_boat_t* >( u );
        if ( r.owner )
            trigger_instigator_update( r.owner->triggers, r.io.instigator );
    };
    return h;
}

// VuInstigatorComponent's gameInitialize 0x531F50 on the boat's +0x70 (its gameInitialize's last call).
inline void race_session_register_instigator( race_session_t& s, race_session_boat_t& b )
{
    std::uint8_t* t = race_session_image( b, 0, 0x50 );
    if ( !b.io.instigator || !t )
    {
        race_unwired( s, "the instigator's gameInitialize 0x531F50 on a boat with no instigator / transform image - not registered (UNKNOWN)" );
        return;
    }
    trigger_instigator_initialize( s.triggers, b.io.instigator, t, b.index );
    b.instigator_registered = b.io.instigator[ 0x28 ];
}

// The "Triggers" phase: the trigger manager's tick 0x536460 (registered by its init 0x536110 at the engine's
// start) - the instigator step, then every listed trigger's vt+0x30 (the world's list).
inline void race_session_triggers( void* user, float )
{
    race_session_t& s = *static_cast< race_session_t* >( user );
    trigger_instigators_step( s.triggers );
    if ( s.world.triggers )
        s.world.triggers( );
    else
        race_unwired( s, "the Triggers tick 0x536460 with no trigger list (no world) - the crossing tests not run (UNKNOWN)" );
}

// The DynamicsSync / DynamicsKick ticks 0x4F8A20 / 0x4F8A90 (world/dynamics.hxx).
inline void race_session_dynamics_sync( void* user, float dt )
{
    race_session_t& s = *static_cast< race_session_t* >( user );
    const int before = s.dynamics->syncs;
    race_dynamics_sync( *s.dynamics, dt );
    race_session_dev_trace( s, before );
}

// The "Water" phase: VuWater's tick 0x529E30 (the world's water_tick).
inline void race_session_water( void* user, float dt )
{
    race_session_t& s = *static_cast< race_session_t* >( user );
    if ( s.world.water_tick )
        s.world.water_tick( dt );
    else
        race_unwired( s, "VuWater's Water tick 0x529E30 with no VuWater (no water_tick) - not run (UNKNOWN)" );
}

inline void race_session_dynamics_kick( void* user, float dt )
{
    race_session_t& s = *static_cast< race_session_t* >( user );
    race_dynamics_kick( *s.dynamics, dt );
    if ( !s.dynamics->stopped.empty( ) )
        race_unwired( s, s.dynamics->stopped );
}

// The Decision / Build delegates the boat's gameInitialize registers (0x41EFF0
// / 0x41F330 behind the thunks 0x4077ED / 0x404AB2):
// - Decision 0x41EFF0 = boat_decision_tick (core/boat state.hxx) composed on
//   the boat's objects with its hooks by world/drive.hxx (race_drive_decision);
//   a boat whose sections load stopped stays STOPPED (UNKNOWN, logged once).
// - Build 0x41F330 (world/build.hxx, read whole): its last step 0x4200E0
//   (boat_build_audio, verified) runs; steps 1 - 6 are declared (drive.hxx).
struct race_session_tick_t
{
    race_session_t* session;
    race_session_boat_t* boat;
    std::size_t index;
};

inline void race_session_decision( void* user, float dt )
{
    race_session_tick_t& t = *static_cast< race_session_tick_t* >( user );
    if ( t.boat->sections && t.index < t.session->drives.size( ) && t.session->drives[ t.index ] )
    {
        race_drive_decision( *t.session, t.index, dt );
        return;
    }
    if ( t.session->stopped_decision++ == 0 )
        LOG_WARNING( t.boat->sections ? "race: VuBoatEntity Decision tick 0x41EFF0 STOPPED (UNKNOWN) - %s's Decision composition did not build"
                                      : "race: VuBoatEntity Decision tick 0x41EFF0 STOPPED (UNKNOWN) - %s's sections load did not complete",
                     t.boat->name.c_str( ) );
}

inline void race_session_build( void* user, float dt )
{
    race_session_tick_t& t = *static_cast< race_session_tick_t* >( user );
    if ( t.boat->sections && t.index < t.session->drives.size( ) && t.session->drives[ t.index ] )
    {
        race_drive_build( *t.session, t.index, dt );
        return;
    }
    if ( t.session->stopped_build++ == 0 )
        LOG_WARNING( t.boat->sections ? "race: VuBoatEntity Build tick 0x41F330 STOPPED (UNKNOWN) - %s's Decision composition did not build"
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

// The dynamics world's persistent manifolds as VuCollisionManager's scrape tick reads them (g_vudynamicsimpl vt+0x6C
// -> the dispatcher's count / i-th, in the dispatcher's order): the body images (solver_rigid_t is the image's first
// 672 bytes), a boat body's owner = its session boat's image (the +0x2C0 word is a 32-bit image, not a pointer here),
// the shape's +8 mesh (race_contact_level_mesh for a level body; a boat's compound has none), the point cache
// (btManifoldPoint images, 0x120 stride). A manifold whose boat-flagged body is no session boat, or whose body is
// neither a boat nor a level body, is UNKNOWN: false (the caller does not run the tick on a partial list).
inline bool race_session_manifolds( race_session_t& s, std::vector< collision_manifold_t >& out )
{
    out.clear( );
    if ( !s.dynamics )
        return true;
    for ( const solver_manifold_t* m : s.dynamics->collision.dispatcher.manifolds )
    {
        collision_manifold_t mf;
        bool known[ 2 ] = { false, false };
        const solver_rigid_t* bodies[ 2 ] = { m->body0, m->body1 };
        for ( int k = 0; k < 2; ++k )
        {
            const std::uint8_t*& owner = k == 0 ? mf.owner0 : mf.owner1;
            const collision_mesh_asset_t*& mesh = k == 0 ? mf.mesh0 : mf.mesh1;
            for ( const auto& b : s.boats )
                if ( &b->body.body == bodies[ k ] )
                {
                    owner = b->graph.objects[ 0 ].image.data( );
                    known[ k ] = true;
                }
            for ( const auto& lb : s.level_bodies )
                if ( !known[ k ] && &lb->body.body == bodies[ k ] )
                {
                    mesh = lb->type == 7 ? lb->asset : nullptr;   // = drive.hxx race_contact_level_mesh (0x5445C0 / 0x5444D5)
                    known[ k ] = true;
                }
        }
        if ( !known[ 0 ] || !known[ 1 ] )
            return false;
        mf.body0 = reinterpret_cast< const std::uint8_t* >( m->body0 );
        mf.body1 = reinterpret_cast< const std::uint8_t* >( m->body1 );
        std::uint16_t g0, g1;
        std::memcpy( &g0, mf.body0 + 0x2CA, 2 );
        std::memcpy( &g1, mf.body1 + 0x2CA, 2 );
        if ( ( ( g0 & 0x10u ) && !mf.owner0 ) || ( ( g1 & 0x10u ) && !mf.owner1 ) )
            return false;
        mf.contacts = m->num_contacts;
        mf.points = reinterpret_cast< const std::uint8_t* >( m->points );
        out.push_back( mf );
    }
    return true;
}

// VuCollisionManager's Decision tick 0x4AD970 (registered by its init 0x4AD110) = collide.hxx collision_manager_tick:
// the two FMOD groups (EventSystem::getGroup), the impacts 0x4ADA10, the scrapes 0x4ADC00 over the dynamics world's
// manifolds, the dev stats page. HOOKS: every FMOD call (audio, a platform hook - no group / event made, so the
// sound steps do nothing), the dev page (a dev hook, not made); the effects on the world's VuPfxManager / runtime.
// With no VuPfx runtime / manager, or a manifold the session cannot name (UNKNOWN), the tick is not run.
inline void race_session_collision( void* user, float )
{
    race_session_t& s = *static_cast< race_session_t* >( user );
    if ( !s.world.pfx || !s.world.pfx_manager || !s.world.pfx_ops )
    {
        race_unwired( s, "VuCollisionManager's tick 0x4AD970 with no VuPfx runtime / manager - UNKNOWN, not run" );
        return;
    }
    std::vector< collision_manifold_t > manifolds;
    if ( !race_session_manifolds( s, manifolds ) )
    {
        race_unwired( s, "VuCollisionManager's tick 0x4AD970: a manifold body that is no session boat / level body - UNKNOWN, not run" );
        return;
    }
    collision_impact_hooks_t ik;
    ik.user = &s;
    ik.get_event = []( void* u, const char*, std::uint32_t& ) {
        race_unwired( *static_cast< race_session_t* >( u ), "audio: VuCollisionManager's impact sound (FMOD getEvent - platform) - no event made" );
        return false;
    };
    collision_scrape_hooks_t sk;
    sk.user = &s;
    sk.get_event = []( void* u, std::uint32_t, const char*, std::uint32_t& event ) {
        race_unwired( *static_cast< race_session_t* >( u ), "audio: VuCollisionManager's scrape sound (FMOD getEvent - platform) - no event made" );
        event = 0u;   // the hook writes the record's event word: no event
    };
    collision_tick_hooks_t th;
    th.user = &s;
    th.get_group = []( void* u, const char*, std::uint32_t& ) {
        race_unwired( *static_cast< race_session_t* >( u ), "audio: VuCollisionManager's FMOD event groups (EventSystem::getGroup - platform) - none" );
    };
    s.collision_impacts += s.collision.impacts.count;
    const int unread = collision_manager_tick( s.collision, s.collision_tables, manifolds.data( ), static_cast< std::int32_t >( manifolds.size( ) ),
                                               *s.world.pfx_manager, *s.world.pfx_ops, s.world.pfx->heap, s.world.pfx->resources, ik, sk, th );
    s.collision_scrapes = s.collision.scrapes.count;
    if ( unread )
        race_unwired( s, "VuCollisionManager's scrapes 0x4ADC00 read past a collision mesh's arrays (UNKNOWN in retail) - the body's surface byte kept" );
}

// VuCollisionManager's reset 0x4AD2B0, called by VuGame's 0x482D50 (its teardown, after gameRelease 0x500B80) -
// 0x482D50 is NOT recreated and no session path reaches it yet: this is its call, for when it is.
inline void race_session_collision_reset( race_session_t& s )
{
    if ( !s.world.pfx || !s.world.pfx_manager )
        return;
    collision_scrape_hooks_t sk;
    collision_manager_reset( s.collision, *s.world.pfx_manager, s.world.pfx->heap, s.world.pfx->resources, sk );
}

// The Viewports phase's boat manager tick 0x4AB0A0 (boat_manager_viewport_tick,
// core/boat manager.hxx, composed by world/drive.hxx): the viewport boat's
// camera image (the Decision tick's camera step 0x409900) into the viewport.
inline void race_session_viewports( void* user, float )
{
    race_drive_viewports( *static_cast< race_session_t* >( user ) );
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
                                const c_archive& archive, entity_t& tree, const track_manager_t* track, fleet_random_t& random,
                                const std::function< void( ) >& project_initialized = { } )
{
    race_world_t lent = std::move( s.world );   // the world's services, set by c_world before the start (the HUD projects)
    s = race_session_t{ };
    s.world = std::move( lent );
    s.dynamics = std::make_unique< race_dynamics_t >( );   // the engine init's VuDynamicsImpl (0x4F8050) + 0x4024E0's gravity
    race_dynamics_init( *s.dynamics, &s.contact );
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
        b->owner = &s;
        b->index = static_cast< int >( s.boats.size( ) );
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
                                    r.io.instigator, race_session_transform_hooks( r ) );
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
    tick_register( s.ticks, { s.dynamics.get( ), race_session_dynamics_sync, &s }, "DynamicsSync" );   // 0x4F8050's registrations
    tick_register( s.ticks, { s.dynamics.get( ), race_session_dynamics_kick, &s }, "DynamicsKick" );
    // VuCollisionManager (g 0x758FBC): the game init 0x4024E0 creates it (0x4ACD30 at 0x4026B9: the ctor 0x4ACDF0) and calls
    // its init 0x4AD110 at 0x4026D4 - before the AI manager's (0x402828), so its Decision tick precedes the AI's. The init's
    // hooks: addContactListener (g_vudynamicsimpl slot 13) = this session's contact-added path hands every record to it
    // (world/drive.hxx race_contact_added); the asset manager's "SurfaceTableDB" = VuDBAsset/SurfaceTableDB's document from
    // the pack; the Decision registration. Its +0x14 = surface_count of the session's surface table (built above).
    collision_manager_construct( s.collision, s.collision_tables );
    {
        std::vector< std::uint8_t > bytes;
        if ( archive.read( "VuDBAsset/SurfaceTableDB", bytes ) )
            s.collision_db = c_reader( bytes ).parse( );
        else
            LOG_WARNING( "race: VuDBAsset/SurfaceTableDB not in the pack - the collision manager's tables load from the null container" );
        collision_init_hooks_t ch;
        ch.user = &s;
        ch.add_listener = []( void* u ) { static_cast< race_session_t* >( u )->collision_listening = true; };
        ch.asset = []( void* u, const char* name ) -> const value_t* {
            race_session_t& x = *static_cast< race_session_t* >( u );
            return std::strcmp( name, "SurfaceTableDB" ) == 0 && x.collision_db.tag == 6 ? &x.collision_db : nullptr;
        };
        ch.register_tick = []( void* u, const char* phase ) {
            race_session_t& x = *static_cast< race_session_t* >( u );
            tick_register( x.ticks, { &x.collision, race_session_collision, &x }, phase );
        };
        collision_manager_init( s.collision_tables, s.surfaces, ch );
        LOG_INFO( "race: VuCollisionManager init 0x4AD110 - %d surface types, the four SurfaceTableDB tables loaded, its Decision tick registered",
                  static_cast< int >( s.collision_tables.types ) );
    }
    tick_register( s.ticks, { &s.fleet, race_session_fleet, &s }, "Decision" );
    // The game mode manager's Decision tick 0x4CBFD0 (its init 0x4CBCD0, slot 5): the action mode's Run state 0x4BAB50 ->
    // VuGame's tick 0x482F10 (race_session_game_mode). The order is read (world batch 21): the game init 0x4024E0 creates
    // each system, pushes it on the back of its list (0x403120) and calls its init vt+0x14 AT ONCE - the AI manager (g
    // 0x758F8C, 0x403310 at 0x402808, init 0x4042E0 at 0x402828: 'Decision' -> 0x404331's handler) long before the game
    // mode manager (g 0x758FF0, 0x4CBA30 at 0x402A6C, init 0x4CBCD0 at 0x402A90: 'Decision' at 0x4CBE4C); the list's
    // later walk (0x402AB2) calls each one's vt+4 - 0x401560 (nothing) for both. So the AI's Decision tick runs first.
    tick_register( s.ticks, { &s.rules, race_session_mode_tick, &s }, "Decision" );
    tick_register( s.ticks, { &s.manager, race_session_viewports, &s }, "Viewports" );
    // VuWater's tick 0x529E30 in "Water" (VuWater made by the engine init, before the race: c_world's
    // water_tick - surface.hxx water_built_tick and the queries' view).
    tick_register( s.ticks, { &s.world, race_session_water, &s }, "Water" );
    // The trigger manager's tick 0x536460 in "Triggers" (its init 0x536110, the engine's start).
    tick_register( s.ticks, { &s.triggers, race_session_triggers, &s }, "Triggers" );

    // The level's gameInitialize walk (after the tick manager: the gfx-settings entities register their Build ticks),
    // on the blender as the engine's init 0x52F620 leaves it.
    gfx_blender_init( s.blender );
    race_session_level_bodies( s, tree, archive );

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

    // 0x4BA060 after the project's gameInitialize 0x500B10: 0x501260(root, "OnProjectInitialized") - the event every
    // VuOnProjectInitializedEntity answers by firing its "Trigger" (the world's graph) - then VuGame's begin.
    if ( project_initialized )
        project_initialized( );

    // VuGame's begin 0x4829A0.
    s.start.laps = s.game.laps;
    s.start.difficulty = s.game.difficulty;
    s.start.root_name = tree.m_name;
    // The delegates' users (stable addresses: the tick manager holds them).
    static std::vector< std::unique_ptr< race_session_tick_t > > tick_users;
    tick_users.clear( );
    for ( std::size_t k = 0; k < s.boats.size( ); ++k )
    {
        race_session_boat_t& b = *s.boats[ k ];
        tick_users.push_back( std::make_unique< race_session_tick_t >( race_session_tick_t{ &s, &b, k } ) );
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
    // The placement's notify: the transform's watcher +0xC0 = the transform-changed delegate 0x421670
    // (boat_transform_changed: the body to the new world), as the nuisance boats' place_child runs it.
    sh.transform_changed = [ &s ]( int k )
    {
        race_session_boat_t& r = *s.boats[ static_cast< std::size_t >( k ) ];
        std::uint8_t* t = race_session_image( r, 0, 0x50 );
        if ( !r.sections || !t )
        {
            ++s.stopped_transform;
            return;
        }
        boat_transform_changed( r.graph.objects[ 0 ].image.data( ), reinterpret_cast< const float* >( t + 0x50 ), r.body, r.io.camera,
                                r.io.instigator, race_session_transform_hooks( r ) );
    };
    sh.game_initialize = [ &s ]( int k )
    {
        race_session_boat_t& b = *s.boats[ static_cast< std::size_t >( k ) ];
        race_boat_init_hooks_t ih;
        ih.manager = &s.manager.lists;
        ih.entry = s.start_boats[ static_cast< std::size_t >( k ) ].boat;
        ih.kind = s.start_boats[ static_cast< std::size_t >( k ) ].kind;
        race_session_tick_t* user = tick_users[ static_cast< std::size_t >( k ) ].get( );
        ih.slot17 = [ &s, &b ] { race_session_enable( s, b, true ); };
        ih.instigator = [ &s, &b ] { race_session_register_instigator( s, b ); };
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
    // VuRaceGame's own part (world/rules.hxx): the ctor 0x489030's state machine, built here (after the base fields above -
    // the ctor ran before the boat load); its first state's enter is slot 5 0x4896A0's "state enter" step below, and its
    // tick the game's slot 7, reached from VuGame's tick 0x482F10 in the Decision phase (race_session_game_mode).
    race_rules_construct( s.rules );
    s.rules.hooks.hud = [ &s ]( int project, race_hud_verb_t verb, float dt )
    {
        if ( s.world.hud )
            s.world.hud( project, verb, dt );
        else
            race_unwired( s, "the race HUD projects (VuRaceGame +0x738 / +0x73C / +0x740) with no world to load them - not run (UNKNOWN)" );
    };
    s.rules.hooks.broadcast = [ &s ]( const char* name, bool value )
    {
        // The VuParams 0x48AB70 builds: one bool (0x4051A0: type 3, the byte - 5 bytes).
        ui_params_t p;
        ui_params_add_bool( p, value );
        if ( s.world.broadcast )
            s.world.broadcast( name, &p );
    };
    s.rules.hooks.platform = [ &s ]( const std::string& call ) { race_unwired( s, "platform services: " + call + " (saves / online / achievements / audio - hooks)" ); };
    s.rules.track = s.game.track;
    {
        // VuGame's ctor 0x481630: +0x674 = ConstantDB VuDBAsset / HUD / ChallengeTextDuration - an int (1, cvtdq2ps), a float (2), an int64
        // (7, fild / fstp), else 0.
        const value_t& v = member( member( member( *db.constant_db, "VuDBAsset" ), "HUD" ), "ChallengeTextDuration" );
        s.rules.split_duration = v.tag == 1 ? static_cast< float >( v.integer ) : v.tag == 2 ? v.real : v.tag == 7 ? static_cast< float >( v.int64 ) : 0.0f;
    }
    s.rules.unrecreated = [ &s ]( std::uint32_t address, const char* what )
    {
        char text[ 160 ];
        std::snprintf( text, sizeof( text ), "%s 0x%X - not recreated, not run (UNKNOWN)", what, address );
        race_unwired( s, text );
    };
    for ( std::size_t k = 0; k < s.game.boats.size( ) && k < s.start_boats.size( ); ++k )
    {
        race_rules_boat_t rb;
        rb.boat = s.start_boats[ k ].boat;
        rb.transform = s.start_boats[ k ].transform;
        rb.driver = s.start_boats[ k ].driver;
        rb.human = s.start_boats[ k ].cls.slot[ 0 ];
        s.rules_boats.push_back( rb );
    }
    s.rules.boats = &s.rules_boats;
    s.rules.game = &s.rules_game;
    sh.call = [ &s ]( const char* row )
    {
        // Slot 5 0x4896A0: "(whatever the track) the game's +0x6F8 state machine enters its first state" - PreGame's enter
        // 0x48A7F0, before the +0x4CC loop (which it does not read).
        // VuGame's begin 0x4829A0: its two machines (+0x0C from +0x18, +0x67C from +0x688) enter their first states.
        if ( std::strcmp( row, "state enter fsm+0x18" ) == 0 )
            vugame_fsm_begin( s.rules.event );
        if ( std::strcmp( row, "state enter fsm+0x688" ) == 0 )
            vugame_fsm_begin( s.rules.split );
        if ( std::strcmp( row, "state enter fsm+0x704" ) == 0 )
        {
            s.rules_game.circuit = s.start.circuit;
            s.rules_game.length = s.start.length;
            s.rules_game.laps = s.start.laps;
            race_rules_start( s.rules );
        }
    };
    race_begin( s.start, s.start_boats, s.manager, *track, *db.track_db, *db.constant_db, sh );
    // The game's +0x720 order as slot 5 left it, and each boat's +0x4D8 (race_start_boat_t::checkpoint) as begin set it.
    s.rules_game.order = s.start.order;
    s.rules_game.order.resize( s.rules_boats.size( ) );
    for ( std::size_t k = 0; k < s.rules_boats.size( ); ++k )
        s.rules_boats[ k ].checkpoint = s.start_boats[ k ].checkpoint;
    s.rules_running = true;

    // VuAiNuisanceDriverStartEntity's gameInitialize 0x42FDC0 (boat nuisance.hxx) on each one in the tree, in
    // pre-order: its child VuBoatEntity (operator new(0x520) + 0x41B710 + ctor 0x41B850 - the object the
    // registry's create 0x41B690 makes) named name + "Boat", loaded from the slot's document (VuEntity::load
    // 0x5009D0, then the postLoad 0x41CFF0 above: a VuAiNuisanceDriver named name + "Driver"), hashed from
    // 0x811C9DC5, put at the start entity's world by 0x5344A0(notify 1), then initialised (0x500B10 -> the
    // boat's 0x41D350). ORDER NOT READ: where the project's gameInitialize walk falls against VuGame's begin -
    // the children are made after begin here (flagged).
    // 0x5344A0 (thiscall transform; matrix*, notify; retn 8): world +0x50 = the matrix; +0xA0 = (atan2(m6,
    // m10), asinf(clamp(-m2)), atan2(m1, m0)) (respawn_euler), +0xAC a stack word (UNKNOWN - 0 here); 0x534830
    // with no parent (local +0x10 = world, +0x90 = the euler); 0x534C00(notify) - the boat has no children;
    // with notify the watcher +0xC0 = the transform-changed delegate 0x421670.
    {
        entity_world_t bw = race_boat_world( registry, hooks );
        std::vector< entity_t* > starts;
        const auto find = [ & ]( const auto& self, entity_t& e ) -> void
        {
            if ( e.m_type == k_nuisance_start_class && e.m_state )
                starts.push_back( &e );
            for ( auto& child : e.m_children )
                self( self, *child );
        };
        find( find, tree );
        for ( entity_t* start : starts )
        {
            const transform_component_t* st = entity_transform( *start );
            std::unique_ptr< entity_t > child = entity_create( "VuBoatEntity", bw );
            if ( !child || !st || start->m_world_unknown )
            {
                LOG_WARNING( "race: %s's nuisance boat 0x42FDC0 STOPPED (UNKNOWN) - %s", start->m_name.c_str( ),
                             child ? "the start entity's world is unknown" : "VuBoatEntity could not be created" );
                continue;
            }
            const std::size_t before = s.boats.size( );
            nuisance_start_hooks_t nh;
            nh.name_child = [ & ]( const std::string& n ) { child->m_name = n; };
            nh.load_child = [ & ]( const value_t& doc ) {
                entity_load( *child, &doc, bw );
                if ( hooks.post_load )
                    hooks.post_load( *child, doc );
            };
            nh.hash_child = [ & ]( std::uint32_t seed ) { race_entity_hash( *child, seed ); };
            nh.place_child = [ & ]( bool notify ) {
                if ( s.boats.size( ) != before + 1 )
                    return;
                race_session_boat_t& r = *s.boats[ before ];
                std::uint8_t* t = race_session_image( r, 0, 0x50 );
                if ( !t )
                    return;
                std::memcpy( t + 0x50, st->world, 64 );
                float e[ 3 ];
                respawn_euler( st->world, e );
                std::memcpy( t + 0xA0, e, 12 );
                image_put32( t, 0xAC, 0 );   // a stack word in retail - UNKNOWN
                std::memmove( t + 0x10, t + 0x50, 64 );
                std::memmove( t + 0x90, t + 0xA0, 16 );
                if ( notify && r.sections )
                    boat_transform_changed( r.graph.objects[ 0 ].image.data( ), reinterpret_cast< const float* >( t + 0x50 ), r.body, r.io.camera,
                                            r.io.instigator, race_session_transform_hooks( r ) );
                else if ( notify )
                    ++s.stopped_transform;
            };
            nh.add_child = [ & ] {
                if ( s.boats.size( ) != before + 1 || !s.boats[ before ]->driver || s.boats[ before ]->driver->graph.empty( ) )
                    return;
                race_session_boat_t& b = *s.boats[ before ];
                tick_users.push_back( std::make_unique< race_session_tick_t >( race_session_tick_t{ &s, &b, before } ) );
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
                race_boat_init_hooks_t ih;
                ih.manager = &s.manager.lists;
                ih.entry = sb.boat;
                ih.kind = sb.kind;
                race_session_tick_t* user = tick_users.back( ).get( );
                ih.slot17 = [ &s, &b ] { race_session_enable( s, b, true ); };
                ih.instigator = [ &s, &b ] { race_session_register_instigator( s, b ); };
                race_boat_game_initialize( &b, s.ticks, { &b, race_session_decision, user }, { &b, race_session_build, user }, b.draw_added, ih );
            };
            nuisance_start_game_initialize( start->m_name, *static_cast< const nuisance_start_t* >( start->m_state.get( ) ), nh );
            s.nuisance_by_path[ race_entity_path( start ) ] = { static_cast< nuisance_start_t* >( start->m_state.get( ) ),
                                                                s.boats.size( ) == before + 1 ? static_cast< int >( before ) : -1 };
            if ( s.boats.size( ) == before + 1 )
                LOG_INFO( "race: nuisance boat %s (%s) from %s (0x42FDC0)", child->m_name.c_str( ), s.boats[ before ]->boat.c_str( ),
                          start->m_name.c_str( ) );
            s.nuisance.push_back( std::move( child ) );
        }
        if ( !starts.empty( ) )
            LOG_INFO( "race: %d VuAiNuisanceDriverStartEntity, %d nuisance boat(s) made", static_cast< int >( starts.size( ) ),
                      static_cast< int >( s.nuisance.size( ) ) );
    }
    // The model's words for the weak references (race_session_t::words): each boat's +0x220 = its driver, the
    // driver's +0 = its vtable.
    for ( std::size_t k = 0; k < s.boats.size( ); ++k )
        if ( s.boats[ k ]->driver && !s.boats[ k ]->driver->graph.empty( ) )
        {
            const std::uint32_t e = race_entity_token( static_cast< int >( k ) );
            s.words[ e + 0x220 ] = e + 0x1000u;
            s.words[ e + 0x1000u ] = load_word( s.boats[ k ]->driver->graph[ 0 ].image.data( ), 0 );
        }
    race_drive_setup( s, db.constant_db );
    LOG_INFO( "race: the dynamics world holds %d rigid bod(ies) / %d collision object(s) / %d step callback(s) after the boats' slot 17 0x421980; gravity z 0x%08X",
              static_cast< int >( s.dynamics->world.bodies.size( ) ), static_cast< int >( s.dynamics->collision.objects.size( ) ),
              static_cast< int >( s.dynamics->impl.step_callbacks.size( ) ), [ & ] { std::uint32_t u; std::memcpy( &u, &s.dynamics->world.gravity[ 2 ], 4 ); return u; }( ) );
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
    ++s.frames;
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
