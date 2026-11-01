#pragma once

#include <workspace/core/boat/anim.hxx>
#include <workspace/core/boat/camera.hxx>
#include <workspace/core/boat/fleet.hxx>
#include <workspace/core/boat/human.hxx>
#include <workspace/core/boat/manager.hxx>
#include <workspace/core/boat/respawn.hxx>
#include <workspace/core/boat/state.hxx>
#include <workspace/core/game/input.hxx>
#include <workspace/core/world/kernel.hxx>
#include <workspace/core/world/query.hxx>
#include <workspace/core/world/session.hxx>
#include <workspace/core/world/tree.hxx>
#include <workspace/utility/logger.hxx>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <xmmintrin.h>

// The race's per-boat Decision tick (VuBoatEntity slot 13 0x41EFF0 =
// boat_decision_tick, core/boat state.hxx) and the boat manager's Viewports
// tick 0x4AB0A0 (boat_manager_viewport_tick, core/boat manager.hxx), composed
// on the session's boat objects. Each hook 0x41EFF0 calls is one of:
//
//   RECREATED, run:  the driver's tick vt+0x48 - VuHumanDriver 0x40F270
//                    (human_decision_driver) and VuAiDriver 0x40B940
//                    (fleet_driver_tick); the camera 0x409900
//                    (boat_camera_tick); the respawn 0x41D790 (boat_respawn);
//                    VuWater's wake lookup 0x5296C0 (water_best_wake); the
//                    water under a point 0x529140 (water_point_height).
//   NOT RECREATED / NOT WIRED (counted, each logged once by name - never a
//   stand-in): the animation tick 0x40FFF0 (recreated as boat_anim_tick, but
//   its three controls at anim +0x08 / +0x0C / +0x10 come from the asset
//   loads 0x410610 gameInitialize hands to the animation system - a hook here
//   - so its inputs are UNKNOWN and it does not run); slots 17 / 18 0x421980 /
//   0x421940 (boat_enable / boat_disable need the dynamics world
//   g_vudynamicsimpl, not instantiated in the race); 0x410FF0, 0x41E240, the
//   debris pieces' follow / removal; the driver tick of a UI / nuisance /
//   ghost driver (none in a Race roster - their ticks are the boat agent's);
//   the hull's splash pfx 0x52BCD0, the lens-water manager g 0x759048, the
//   wake update 0x417D80; the driver's vt+0x50, the trigger manager 0x536350,
//   the transform's child walk 0x534C00(0) on the boat and the
//   "OnBoatRecovered" broadcast inside the respawn.
//   PLATFORM HOOKS (not copied - BRIEF): FMOD (every get_event answers false:
//   no event is made, so no audio calls follow), the stats / voice-over
//   service g 0x758FEC, the gamepad rumble, the accelerometer, the input
//   device (pad 0's named channels answered from game/input.hxx's
//   read_input: Throttle = the right trigger / W, Brake = the left trigger /
//   S, Steering = the left stick / A-D, Boost = the right shoulder / shift;
//   Jump and Recover have no binding: 0), the display size.
//   NOT WIRED, ANSWERING "NO HIT" (UNKNOWN - flagged, logged once): the
//   dynamics world's ray test (g_vudynamicsimpl vt+0x6C's collision world
//   vt+0x18) the camera's collision 0x40B090 and the AI's avoidance 0x40C750
//   cast - no collision world holds the level in the race yet.
//
// The settings manager's bytes the human tick reads (g_vusettingsmanager
// 0x758FE0 +0x2C / +0x2D / +0x30 / +0x34) are a SAVE-side value (scope: saves
// are re-made): 0 here, flagged.

// The engine's +0xF8 std::vector of 8-byte records: 0x413700 resizes it to
// the drive count through 0x4153A0, whose growth 0x415700 writes each new
// record as {byte 0, float 0}; 0x414760 refills them per tick (in the Build
// tick - not recreated). The engine image's +0xF8 / +0xFC words are the
// vector's begin / end: model addresses 0 and 8n (only their difference is
// read, by 0x413830).
struct race_drive_t
{
    race_session_t* session = nullptr;
    std::size_t index = 0;
    std::vector< std::uint8_t > records;
    boat_decision_t d{};
    boat_decision_hooks_t hooks{};
    // The driver step, by class.
    bool human = false, ai = false;
    int pilot = -1;   // the fleet pilot of an AI boat
    human_pilot_t human_pilot;
    human_decision_driver_t human_driver;
    camera_env_t camera_env;
    boat_respawn_t respawn{};
    boat_respawn_hooks_t respawn_hooks{};
};

inline void race_unwired( race_session_t& s, const std::string& what )
{
    if ( s.unwired[ what ]++ == 0 )
        LOG_WARNING( "race: %s", what.c_str( ) );
}

// g 0x759058's first surface's +0xD8 (boat_decision_t::floor). VuWater +0x20
// is a std::list of the surfaces (+0x24 its size): VuWater's surface add
// 0x5284C0 pushes each new surface on the BACK (0x4031F0 before the head) and
// then sorts the whole list with std::list::sort 0x52A670 and the predicate
// 0x527F50 - a < b when b's +0x14 (the height) is greater (comiss / seta). A
// stable sort run after each add leaves first the EARLIEST-added surface of
// the least height. Null with no surface (+0x24 == 0). A NaN height makes the
// predicate inconsistent: UNKNOWN (`unknown` set, null returned).
inline const float* race_drive_floor( const water_built_t* water, bool& unknown )
{
    unknown = false;
    if ( !water || water->surfaces.empty( ) )
        return nullptr;
    std::size_t best = 0;
    for ( std::size_t k = 0; k < water->surfaces.size( ); ++k )
    {
        const float h = water->surfaces[ k ].f( 20 );
        if ( h != h || !water->surfaces[ k ].known[ 20 / 4 ] || !water->surfaces[ k ].known[ 216 / 4 ] )
        {
            unknown = true;
            return nullptr;
        }
        if ( k > 0 && _mm_comigt_ss( _mm_set_ss( water->surfaces[ best ].f( 20 ) ), _mm_set_ss( h ) ) )
            best = k;
    }
    return reinterpret_cast< const float* >( &water->surfaces[ best ].word[ 216 / 4 ] );
}

// The VerticalFOV container as 0x4D2C60 reads it (+0 the type, +8 the payload).
inline camera_db_value_t race_drive_fov_value( const value_t& v )
{
    camera_db_value_t r;
    r.type = static_cast< std::int32_t >( v.tag );
    if ( v.tag == 1 )
        r.payload = static_cast< std::uint32_t >( v.integer );
    else if ( v.tag == 2 )
    {
        std::uint32_t bits;
        std::memcpy( &bits, &v.real, 4 );
        r.payload = bits;
    }
    else if ( v.tag == 7 )
        r.payload = static_cast< std::uint64_t >( v.int64 );
    return r;
}

// ---- the hooks -------------------------------------------------------------

inline race_drive_t& race_drive_of( void* user )
{
    return *static_cast< race_drive_t* >( user );
}

inline race_session_boat_t& race_drive_boat( race_drive_t& r )
{
    return *r.session->boats[ r.index ];
}

// The dynamics world's ray test: not wired (see the head).
inline int race_drive_ray( void* user, const float*, const float*, const ai_ray_hit_t** hits )
{
    race_unwired( *static_cast< race_session_t* >( user ), "the dynamics world's ray test (g_vudynamicsimpl vt+0x6C, vt+0x18) is NOT WIRED - "
                                                          "camera / AI rays answer no hit (UNKNOWN)" );
    *hits = nullptr;
    return 0;
}

// 0x5296C0 on the probe point with the callback {vtable, point x, y,
// direction x, y}: the finder starts at 0 (retail's [ebp-8]); water +0x18
// (skip waves) is the build's 0.
inline float race_drive_wake( void* user, const boat_probe_io_t& io )
{
    race_drive_t& r = race_drive_of( user );
    const race_world_t& w = r.session->world;
    if ( !w.water )
    {
        race_unwired( *r.session, "VuWater 0x5296C0 asked with no VuWater built (UNKNOWN answer 0)" );
        return 0.0f;
    }
    water_wake_finder_t f;
    std::memcpy( f.point, &io.callback[ 1 ], 8 );
    std::memcpy( f.direction, &io.callback[ 3 ], 8 );
    f.best = 0.0f;
    water_best_wake( w.water->hold, false, io.point, f );
    return f.best;
}

// 0x529140's out +0x20 under a point (respawn.hxx's water hook).
inline float race_drive_water( void* user, const float* point )
{
    race_drive_t& r = race_drive_of( user );
    const race_world_t& w = r.session->world;
    water_vertex_t out{};
    if ( !w.view )
        return respawn_bits( 0xFEFFFFFFu );
    if ( !water_point_height( w.view->water, point, water_view_kernel, const_cast< water_view_t* >( w.view ), out ) )
        race_unwired( *r.session, "0x529140 reached a water path not recreated - the respawn's water answer is UNKNOWN" );
    return out.height;
}

inline void race_drive_driver( void* user, float dt )
{
    race_drive_t& r = race_drive_of( user );
    race_session_t& s = *r.session;
    if ( r.human )
    {
        human_decision_driver( &r.human_driver, dt );
        return;
    }
    if ( r.ai && r.pilot >= 0 )
    {
        std::vector< const std::uint8_t* > transforms;
        for ( const race_start_boat_t& b : s.start_boats )
            transforms.push_back( b.transform );
        std::vector< const std::uint8_t* > humans;
        for ( const void* b : s.manager.lists.humans.boats )
            humans.push_back( static_cast< const std::uint8_t* >( b ) );
        ai_tick_t t;
        t.boat = r.d.boat;
        t.body = r.d.body;
        t.solver = r.d.solver;
        t.transform = r.d.transform;
        t.engine = r.d.engine;
        t.owner = r.d.boat;   // engine +0 is the boat
        t.animation = r.d.animation;
        t.boats = humans.empty( ) ? nullptr : humans.data( );
        t.boat_count = static_cast< std::int32_t >( humans.size( ) );
        ai_tick_hooks_t h;
        h.avoid.user = &s;
        h.avoid.ray = race_drive_ray;
        h.avoid.aabb = nullptr;   // reached only after a hit
        h.controls.user = &s;
        h.controls.misfire = []( void* u ) { race_unwired( *static_cast< race_session_t* >( u ), "audio: the BoostMisfire sound 0x41E060 (FMOD - platform)" ); };
        h.controls.voice = []( void* u, const char* ) { race_unwired( *static_cast< race_session_t* >( u ), "the voice-over manager g 0x758FEC vt+0x1C (not wired)" ); };
        fleet_driver_tick( *s.track, s.pilots[ static_cast< std::size_t >( r.pilot ) ], t, transforms.data( ), dt, h );
        return;
    }
    race_unwired( s, "a driver tick of a class not wired here (UI / nuisance / ghost, or an AI boat with no pilot) - UNKNOWN" );
}

inline void race_drive_camera( void* user, float dt )
{
    race_drive_t& r = race_drive_of( user );
    const boat_decision_t& d = r.d;
    camera_boat_t cb;
    cb.world = reinterpret_cast< const float* >( d.transform + 0x50 );
    cb.destroyed = d.boat[ 0x46B ] != 0;
    std::memcpy( &cb.debris_count, d.debris + 0xA0, 4 );
    if ( cb.destroyed && cb.debris_count > 0 )
    {
        // The first piece's image (debris +0x9C's array) - the pieces' readers are hooks.
        race_unwired( *r.session, "the camera on a destroyed boat with debris pieces (their images are not modelled) - UNKNOWN, the camera tick skipped" );
        return;
    }
    cb.velocity = reinterpret_cast< const float* >( d.body + 0x140 );
    cb.engine = d.engine;
    cb.engine_owner_470 = state_f( d.boat, 0x470 );
    cb.far_plane = state_f( d.boat, 0x408 );
    r.camera_env.control_method = r.session->world.control_method;
    r.camera_env.display_w = r.session->world.display_w;
    r.camera_env.display_h = r.session->world.display_h;
    if ( !boat_camera_tick( d.camera, cb, r.camera_env, dt ) )
        race_unwired( *r.session, "the camera's mode +0x2A0 is none of 0 / 1 / 2 under control method 2 - UNKNOWN, nothing written" );
}

inline void race_drive_respawn( void* user )
{
    race_drive_t& r = race_drive_of( user );
    std::memcpy( &r.respawn.checkpoint, r.d.driver + 0x78, 4 );
    boat_respawn( *r.session->track, r.respawn, r.respawn_hooks );
}

// ---- setup -----------------------------------------------------------------

inline void race_drive_setup( race_session_t& s, const value_t* constant_db )
{
    s.drives.assign( s.boats.size( ), nullptr );
    if ( constant_db )
        s.vertical_fov = member( member( member( *constant_db, "VuDBAsset" ), "Camera" ), "VerticalFOV" );
    for ( std::size_t k = 0; k < s.boats.size( ); ++k )
    {
        race_session_boat_t& b = *s.boats[ k ];
        std::uint8_t* transform = race_session_image( b, 0, 0x50 );
        if ( !b.sections || !transform || !b.driver || b.driver->graph.empty( ) )
            continue;
        auto r = std::make_shared< race_drive_t >( );
        r->session = &s;
        r->index = k;
        std::uint8_t* engine = b.engine.object.image;
        r->records.assign( 8 * b.engine.records, 0 );
        image_put32( engine, 0xF8, 0 );
        image_put32( engine, 0xFC, static_cast< std::uint32_t >( 8 * b.engine.records ) );
        std::uint8_t* body = reinterpret_cast< std::uint8_t* >( &b.body );
        std::uint8_t* driver = b.driver->graph[ 0 ].image.data( );
        const std::uint32_t vtable = load_word( driver, 0 );
        if ( load_word( body, 0xC8 ) != 0 )
        {
            // body +0xC8, the broadphase proxy: none is modelled (no dynamics world).
            LOG_WARNING( "race: %s's body +0xC8 broadphase proxy is set (0x%08X) but not modelled - Decision STOPPED (UNKNOWN)", b.name.c_str( ),
                         load_word( body, 0xC8 ) );            continue;
        }
        boat_decision_t& d = r->d;
        d.boat = b.graph.objects[ 0 ].image.data( );
        d.body = body;
        d.proxy = nullptr;
        d.driver = driver;
        d.slots = boat_driver_of( vtable );
        d.human = vtable == 0x737F44u;
        d.camera = b.io.camera;
        d.hull = b.io.hull;
        d.hull_names = &b.io.hull_strings;
        d.engine = engine;
        d.engine_names = &b.engine.strings;
        d.records = r->records.empty( ) ? nullptr : r->records.data( );
        d.animation = b.io.animation;
        d.debris = b.io.damage;
        d.transform = transform;
        d.solver = b.io.solver;
        d.local_players = s.world.viewports;
        d.floor = nullptr;
        // The driver step.
        r->human = d.human;
        r->ai = vtable == 0x737B5Cu;
        for ( std::size_t p = 0; p < s.pilots.size( ); ++p )
            if ( s.pilots[ p ].boat == static_cast< int >( k ) )
                r->pilot = static_cast< int >( p );
        r->human_pilot.driver = driver;
        r->human_pilot.sector = s.start_boats[ k ].sector;
        human_decision_driver_t& hd = r->human_driver;
        hd.track = s.track;
        hd.pilot = &r->human_pilot;
        hd.tick.boat = d.boat;
        hd.tick.transform = transform;
        hd.tick.body = body;
        hd.tick.camera = d.camera;
        hd.tick.solver = d.solver;
        hd.tick.animation = d.animation;
        hd.env.control_method = s.world.control_method;
        hd.hooks.user = &s;
        hd.hooks.axis = []( void*, std::int32_t, const char* name ) -> float {
            const input_t in = read_input( );
            if ( std::strcmp( name, "Throttle" ) == 0 )
                return in.throttle;
            if ( std::strcmp( name, "Brake" ) == 0 )
                return in.brake;
            if ( std::strcmp( name, "Steering" ) == 0 )
                return in.steer;
            return 0.0f;
        };
        hd.hooks.held = []( void*, std::int32_t, const char* name ) -> std::uint8_t {
            return std::strcmp( name, "Boost" ) == 0 && read_input( ).boost ? 1 : 0;
        };
        hd.hooks.pressed = []( void*, std::int32_t, const char* ) -> std::uint8_t { return 0; };
        hd.hooks.misfire = []( void* u ) { race_unwired( *static_cast< race_session_t* >( u ), "audio: the BoostMisfire sound 0x41E060 (FMOD - platform)" ); };
        hd.hooks.voice = []( void* u, const char* ) { race_unwired( *static_cast< race_session_t* >( u ), "the voice-over manager g 0x758FEC vt+0x1C (not wired)" ); };
        hd.hooks.jump_refused = []( void* u ) { race_unwired( *static_cast< race_session_t* >( u ), "audio: the jump-refused sound 0x41E1A0 (FMOD - platform)" ); };
        hd.hooks.jump_sound = []( void* u ) { race_unwired( *static_cast< race_session_t* >( u ), "audio: the jump sound 0x41E100 (FMOD - platform)" ); };
        hd.hooks.menu_change = []( void* u ) { race_unwired( *static_cast< race_session_t* >( u ), "audio: menu_change (FMOD - platform)" ); };
        // The camera's environment (0x409900's outside reads).
        r->camera_env.driver_34 = d.slots.slot[ 1 ];
        r->camera_env.vertical_fov = race_drive_fov_value( s.vertical_fov );
        r->camera_env.user = &s;
        r->camera_env.ray = race_drive_ray;
        // The respawn's views: the boat 0x481CE0 creates has no parent (race_load_boats).
        r->respawn.boat = d.boat;
        r->respawn.transform = transform;
        r->respawn.transform_parent = false;
        r->respawn.body = &b.body;
        r->respawn.proxy = nullptr;
        r->respawn.camera = d.camera;
        r->respawn.instigator = b.io.instigator;
        r->respawn_hooks.user = r.get( );
        r->respawn_hooks.water = race_drive_water;
        r->respawn_hooks.transform_children = []( void* u ) { race_unwired( *race_drive_of( u ).session, "the respawn's 0x534C00(0) on the boat's transform (not wired)" ); };
        r->respawn_hooks.instigator = []( void* u ) { race_unwired( *race_drive_of( u ).session, "the trigger manager's 0x536350 (instigator update, not wired)" ); };
        r->respawn_hooks.enable = []( void* u ) { race_unwired( *race_drive_of( u ).session, "the boat's slot 17 0x421980 (boat_enable: no dynamics world in the race)" ); };
        r->respawn_hooks.driver_50 = []( void* u ) { race_unwired( *race_drive_of( u ).session, "the driver's vt+0x50 after a respawn (not recreated)" ); };
        r->respawn_hooks.recovered = []( void* u, std::uint32_t ) { race_unwired( *race_drive_of( u ).session, "the OnBoatRecovered broadcast (VuEventManager, not wired)" ); };
        // The Decision tick's hooks.
        boat_decision_hooks_t& h = r->hooks;
        h.user = r.get( );
        h.wake_lookup = race_drive_wake;
        h.jump_sound = []( void* u ) { race_unwired( *race_drive_of( u ).session, "audio: the jump sound 0x41E100 (FMOD - platform)" ); };
        h.driver_tick = race_drive_driver;
        h.animation_tick = []( void* u, float ) {
            race_unwired( *race_drive_of( u ).session, "the animation tick 0x40FFF0: its controls (anim +0x08 / +0x0C / +0x10) come from 0x410610's asset "
                                                       "loads (a hook) - UNKNOWN, not run" );
        };
        h.camera_tick = race_drive_camera;
        h.respawn = race_drive_respawn;
        h.enable = []( void* u ) { race_unwired( *race_drive_of( u ).session, "the boat's slot 17 0x421980 (boat_enable: no dynamics world in the race)" ); };
        h.disable = []( void* u ) { race_unwired( *race_drive_of( u ).session, "the boat's slot 18 0x421940 (boat_disable: no dynamics world in the race)" ); };
        h.debris_410FF0 = []( void* u ) { race_unwired( *race_drive_of( u ).session, "the debris 0x410FF0 (not recreated)" ); };
        h.boat_41E240 = []( void* u ) { race_unwired( *race_drive_of( u ).session, "the boat's 0x41E240 (not recreated)" ); };
        h.piece_follow = []( void* u ) { race_unwired( *race_drive_of( u ).session, "the debris pieces' follow 0x5344A0 / 0x50E660 (not recreated)" ); };
        h.piece_remove = []( void* u, int ) { race_unwired( *race_drive_of( u ).session, "the debris pieces' removal (0x411950's dynamics calls, not recreated)" ); };
        h.boost.user = r.get( );
        h.boost.sound = []( void* u, const std::string& ) { race_unwired( *race_drive_of( u ).session, "audio: the boost sounds (FMOD - platform)" ); };
        h.boost.stats = []( void* u, const char* ) { race_unwired( *race_drive_of( u ).session, "the stats service g 0x758FEC vt+0x1C (platform)" ); };
        h.hull.user = r.get( );
        h.hull.fmod.user = r.get( );
        h.hull.fmod.get_event = []( void* u, int, const std::string&, std::uint32_t* ) {
            race_unwired( *race_drive_of( u ).session, "audio: FMOD getEvent (platform) - no event made" );
            return false;
        };
        h.hull.splash = []( void* u, const std::string& ) { race_unwired( *race_drive_of( u ).session, "the hull's splash pfx 0x52BCD0 (not wired)" ); };
        h.hull.lens = []( void* u, float ) { race_unwired( *race_drive_of( u ).session, "the lens-water manager g 0x759048 (not wired)" ); };
        h.hull.wake = []( void* u ) { race_unwired( *race_drive_of( u ).session, "the hull's wake update 0x417D80 (not recreated)" ); };
        s.drives[ k ] = r;
    }
    int made = 0;
    for ( const auto& r : s.drives )
        made += r ? 1 : 0;
    LOG_INFO( "race: the Decision tick 0x41EFF0 composed on %d of %d boat(s)", made, static_cast< int >( s.boats.size( ) ) );
}

// ---- per frame ---------------------------------------------------------------

inline void race_drive_decision( race_session_t& s, std::size_t boat, float dt )
{
    race_drive_t& r = *s.drives[ boat ];
    bool unknown = false;
    r.d.floor = race_drive_floor( s.world.water, unknown );
    if ( unknown )
        race_unwired( s, "a VuWater surface height is NaN - 0x52A670's order (the floor) is UNKNOWN" );
    r.d.local_players = s.world.viewports;
    r.human_driver.env.control_method = s.world.control_method;
    boat_decision_tick( r.d, dt, r.hooks );
}

// 0x4AB0A0 on the session's boat manager: the chosen boat's camera image
// (0x240 bytes of *(boat +0x224)) into the viewport (g 0x759060 +0x28); +0x467
// set on the viewport boat, cleared on the rest.
inline void race_drive_viewports( race_session_t& s )
{
    boat_viewport_hooks_t hooks;
    hooks.boat_camera = [ &s ]( const void* boat ) {
        for ( const auto& b : s.boats )
            if ( b->graph.objects[ 0 ].image.data( ) == boat && b->io.camera )
            {
                std::memcpy( s.viewport_camera, b->io.camera, sizeof( s.viewport_camera ) );
                s.viewport_set = true;
                return;
            }
        race_unwired( s, "the viewport boat has no camera image - UNKNOWN" );
    };
    hooks.viewport_camera = [ &s ]( const std::uint8_t* ) { race_unwired( s, "the boat manager's override camera +0x2C0 (who sets it is not established)" ); };
    hooks.debug_camera = [ &s ] { race_unwired( s, "the boat manager's debug camera (dev layer)" ); };
    hooks.lens_scale = [ &s ]( float ) { race_unwired( s, "the viewport's lens scale +0x278 (not consumed)" ); };
    hooks.lens_water = [ &s ]( float ) { race_unwired( s, "the lens-water manager g 0x759048 vt+0x2C (not wired)" ); };
    hooks.local = []( const void* boat, std::uint8_t local ) { const_cast< std::uint8_t* >( static_cast< const std::uint8_t* >( boat ) )[ 0x467 ] = local; };
    hooks.listener = [ &s ] { race_unwired( s, "the FMOD listener + the ActiveCameraTrigger's position (0x534830 / 0x534C00) (not wired)" ); };
    hooks.trigger = [ &s ]( bool ) { race_unwired( s, "the ActiveCameraTrigger's registration (the trigger manager, not wired)" ); };
    boat_viewport_t out;
    if ( !boat_manager_viewport_tick( s.manager.lists, out, hooks ) )
        race_unwired( s, "the boat manager's viewport index +0x58 is negative - UNKNOWN" );
}
