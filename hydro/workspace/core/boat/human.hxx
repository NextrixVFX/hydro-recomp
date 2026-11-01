#pragma once

#include <workspace/core/boat/ai.hxx>
#include <workspace/core/boat/track.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <xmmintrin.h>

// VuHumanDriver (vtable 0x737F44, 27 slots + RTTI; ctor 0x40ED50; the
// explosion listener interface at +0x80, vtable 0x737F30): the player's
// driver. Its tick (slot 18, vt+0x48 = 0x40F270) reads the player's input
// through the control-method manager and writes the boat's controls - the
// same +0x2FC throttle / +0x300 steer / +0x304 boost the AI's 0x40D5A0
// writes - plus the jump / recover requests, the camera roll and the
// wrong-way meter. The driver image (0xD0) as the ctor leaves it: +0x5C the
// boat, +0x78 the checkpoint id (-1), +0x84 the current sector (0), +0x88 the
// wrong-way time (0), +0x8C the wrong-way flag (0), +0x90 / +0x94 the DB's
// AccelerometerSteeringExtents [0] / [1] (10 / 30 by default), +0x98
// AccelerometerCameraDamping (1), +0x9C GamePadCameraTilt (0), +0xA0
// HUD/WrongWaySpeedThreshold x 0.44704, +0xA4 cos(HUD/WrongWayAngleThreshold
// x pi/180), +0xA8 HUD/WrongWayInertiaTime, +0xAC the camera roll (0), +0xB8
// the recover request (0), +0xB9 (1) / +0xBA (0) and +0xBC / +0xC0 / +0xC4 the
// boost-jump window 0x40FC60 reads, +0xC8 / +0xCC the rumble pair (0).
//
// What it reaches that is NOT recreated, each a hook (human_hooks_t): the
// input manager's named channels (g_vuinputmanagerimpl 0x759038 vt+0x18
// 0x510E80 the axis value, vt+0x20 0x510EC0 / vt+0x24 0x510EE0 the button's
// +0x10 / +0x11 bytes, through the lookups 0x511510 / 0x5115A0 - the device
// poll fills them: the INPUT DEVICE / INPUT LAYER), the accelerometer
// (g_vumetroaccel 0x75903C vt+0x18), the gamepad's rumble (g_vumetrogamepad
// 0x759040 vt+0x18 (pad 0) then that object's vt+0), the audio (0x41E060 the
// BoostMisfire sound, 0x41E1A0 the jump-refused sound, 0x41E100 the jump
// sound, the FMOD event "Global/UI_and_Menu/menu_change" 0x746D4C) and the
// voice-over manager (g 0x758FEC vt+0x1C).

// The settings the tick reads (not hooks - plain values of their singletons):
// the control method g_vucontrolmethodmanager 0x758FC0 +0x18, and
// g_vusettingsmanager 0x758FE0 +0x2C / +0x2D (the brake-only bytes of
// methods 0 / 1), +0x30 (the accelerometer extents' blend), +0x34 (rumble on).
struct human_env_t
{
    std::int32_t control_method = 0;
    std::uint8_t brake_only_0 = 0;
    std::uint8_t brake_only_1 = 0;
    float tilt_blend = 0.0f;
    std::uint8_t rumble = 0;
};

struct human_hooks_t
{
    void* user = nullptr;
    float ( *axis )( void* user, std::int32_t pad, const char* name ) = nullptr;            // vt+0x18
    std::uint8_t ( *held )( void* user, std::int32_t pad, const char* name ) = nullptr;     // vt+0x20
    std::uint8_t ( *pressed )( void* user, std::int32_t pad, const char* name ) = nullptr;  // vt+0x24
    bool ( *accel )( void* user, float* out ) = nullptr;                                     // 16 b; lane 0 read
    void ( *rumble )( void* user, float low, float high ) = nullptr;
    void ( *misfire )( void* user ) = nullptr;         // 0x41E060
    void ( *voice )( void* user, const char* line ) = nullptr;
    void ( *jump_refused )( void* user ) = nullptr;    // 0x41E1A0
    void ( *jump_sound )( void* user ) = nullptr;      // 0x41E100
    void ( *menu_change )( void* user ) = nullptr;     // the FMOD event's getEvent + start
};

// Everything the tick reads, by the pointers retail follows from the driver:
// the boat (+0x5C), its transform component (+0x50: +0x60 the forward row,
// +0x80 the position), body (+0x21C: +0x140 velocity, +0x160 inverse mass,
// +0x170 linear factor - written by 0x41DEC0), camera (+0x224: +0x2CC /
// +0x2D0 written), the hull's (+0x228) solver (+0x1CC: +0x54), the animation
// (+0x230: +0x78). `sector` is the driver's +0x84 as a sector index (-1 = 0).
struct human_tick_t
{
    std::uint8_t* driver = nullptr;
    std::uint8_t* boat = nullptr;
    const std::uint8_t* transform = nullptr;
    std::uint8_t* body = nullptr;
    std::uint8_t* camera = nullptr;
    const std::uint8_t* solver = nullptr;
    const std::uint8_t* animation = nullptr;
    int* sector = nullptr;
};

inline bool human_ge( float a, float b ) { return _mm_comige_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; }   // ordered a >= b
inline bool human_gt( float a, float b ) { return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; }   // ordered a > b

inline std::int32_t human_i32( const std::uint8_t* b, int at )
{
    std::int32_t v;
    std::memcpy( &v, b + at, 4 );
    return v;
}

// 0x41DEC0 (thiscall, the boat): the boost jump's impulse. f = the forward
// row's x / y (component +0x60 / +0x64) x 35.7632 (0x6F77B4, 80 mph in m/s);
// with b = +0x134 and u = +0x13C x 0.44704: velocity += inverse mass (+0x160)
// x ((b f.x) x +0x170, +0x174 x (b f.y), +0x178 x (b u)); the boost +0x310 =
// maxss(0, minss(16, +0x310 + 4)) (0x6F7798 / 0x6F7754); then the jump sound
// 0x41E100.
inline void boat_jump_impulse( std::uint8_t* boat, const std::uint8_t* transform, std::uint8_t* body, const human_hooks_t& hooks )
{
    const float k = ai_bits( 0x420F0D84u );
    const float b = state_f( boat, 0x134 );
    const float up = state_f( boat, 0x13C ) * ai_bits( 0x3EE4E26Du );
    const float y = state_f( transform, 0x64 ) * k;
    const float x = state_f( transform, 0x60 ) * k;
    const float ey = state_f( body, 0x174 ) * ( b * y );
    const float ez = state_f( body, 0x178 ) * ( b * up );
    const float ex = ( b * x ) * state_f( body, 0x170 );
    const float im = state_f( body, 0x160 );
    state_put_f( body, 0x140, state_f( body, 0x140 ) + im * ex );
    state_put_f( body, 0x144, state_f( body, 0x144 ) + im * ey );
    state_put_f( body, 0x148, state_f( body, 0x148 ) + im * ez );
    state_put_f( boat, 0x310, ai_maxss( 0.0f, ai_minss( 16.0f, state_f( boat, 0x310 ) + 4.0f ) ) );
    if ( hooks.jump_sound )
        hooks.jump_sound( hooks.user );
}

// 0x41D6B0 (thiscall, the boat): the jump request. Nothing while +0x308 is
// set. Allowed unless +0x138 x 0.44704 is (ordered) above the planar speed
// VuMath_sqrt(vx^2 + vy^2), or the solver's +0x54 is an ordered zero (ucomiss
// / lahf / test ah, 44h / jp). With 0 < +0x310 (or a NaN) and allowed: +0x305
// = 1. Otherwise the jump-refused sound 0x41E1A0 and, with 0 >= +0x328
// (ordered), the voice line "NeedBoostBeforeJump".
inline void boat_jump_request( std::uint8_t* boat, const std::uint8_t* body, const std::uint8_t* solver, const human_hooks_t& hooks )
{
    if ( human_i32( boat, 0x308 ) != 0 )
        return;
    const float vx = state_f( body, 0x140 ), vy = state_f( body, 0x144 );
    const float speed = hull_sqrt( vx * vx + vy * vy );
    bool ok = !human_gt( state_f( boat, 0x138 ) * ai_bits( 0x3EE4E26Du ), speed );
    if ( state_f( solver, 0x54 ) == 0.0f )
        ok = false;
    if ( !human_ge( 0.0f, state_f( boat, 0x310 ) ) && ok )
    {
        boat[ 0x305 ] = 1;
        return;
    }
    if ( hooks.jump_refused )
        hooks.jump_refused( hooks.user );
    if ( human_ge( 0.0f, state_f( boat, 0x328 ) ) && hooks.voice )
        hooks.voice( hooks.user, "NeedBoostBeforeJump" );
}

// 0x40F690 (__usercall: ecx the driver, xmm1 dt): the input. Brake-only is
// method 0's settings +0x2C, method 1's +0x2D, else 1: then throttle = 1 -
// Brake x 2, else Throttle - Brake (both axes of pad 0; the axis values
// arrive on x87 and are stored as floats). Steering (axis), Boost (held),
// Jump and Recover (pressed; Recover also when +0xB8). The roll = (+0x9C x
// pi/180) x steering unless the boat's +0x46B, else 0. Under method 2 the roll
// is 0 and, when the accelerometer answers: e = (((1 - s) x +0x90) + +0x94 x
// s) x pi/180 (s = settings +0x30), a = asinf(maxss(-1, minss(1, accel.x))),
// steering = maxss(-1, minss(1, maxss(-1, minss(1, a / e)))) and the roll =
// maxss(-e, minss(e, a)); without an answer steering is 0. Then +0x2FC =
// throttle, +0x300 = steering, the boost edge as 0x40D5A0's (BoostMisfire
// unless the animation's +0x78 == 2; the voice line when 0 >= +0x328 ordered
// and +0x4B4 > 1), +0x304 = boost, +0x305 = 0, the jump 0x41D6B0 when
// pressed, +0x306 = recover. The roll is smoothed: k = minss(+0x98 x dt, 1),
// +0xAC = (1 - k) x +0xAC + k x roll, written to the camera's +0x2CC (its
// +0x2D0 byte = 0). A recover with +0x308 == 0 and +0x464 clear plays
// "menu_change".
inline void human_input( const human_tick_t& t, float dt, const human_env_t& env, const human_hooks_t& hooks )
{
    std::uint8_t* drv = t.driver;
    std::uint8_t* boat = t.boat;
    std::uint8_t brake_only = 1;
    if ( env.control_method == 0 )
        brake_only = env.brake_only_0;
    if ( env.control_method == 1 )
        brake_only = env.brake_only_1;
    auto axis = [ & ]( const char* name ) { return hooks.axis ? hooks.axis( hooks.user, 0, name ) : 0.0f; };
    float throttle;
    if ( brake_only )
        throttle = 1.0f - axis( "Brake" ) * 2.0f;
    else
    {
        const float a = axis( "Throttle" );
        const float b = axis( "Brake" );
        throttle = a - b;
    }
    float steering = axis( "Steering" );
    const std::uint8_t boost = hooks.held ? hooks.held( hooks.user, 0, "Boost" ) : 0;
    const std::uint8_t jump = hooks.pressed ? hooks.pressed( hooks.user, 0, "Jump" ) : 0;
    std::uint8_t recover = hooks.pressed ? hooks.pressed( hooks.user, 0, "Recover" ) : 0;
    if ( drv[ 0xB8 ] )
        recover = 1;
    float roll = 0.0f;
    if ( !boat[ 0x46B ] )
        roll = ( state_f( drv, 0x9C ) * ai_bits( 0x3C8EFA35u ) ) * steering;
    if ( env.control_method == 2 )
    {
        roll = 0.0f;
        float acc[ 4 ] = { };
        if ( hooks.accel && hooks.accel( hooks.user, acc ) )
        {
            const float s = env.tilt_blend;
            const float e = ( ( 1.0f - s ) * state_f( drv, 0x90 ) + state_f( drv, 0x94 ) * s ) * ai_bits( 0x3C8EFA35u );
            const float a = ai_asinf( ai_maxss( -1.0f, ai_minss( 1.0f, acc[ 0 ] ) ) );
            steering = ai_maxss( -1.0f, ai_minss( 1.0f, ai_maxss( -1.0f, ai_minss( 1.0f, a / e ) ) ) );
            roll = ai_maxss( -e, ai_minss( e, a ) );
        }
        else
            steering = 0.0f;
    }
    state_put_f( boat, 0x2FC, throttle );
    state_put_f( boat, 0x300, steering );
    if ( boost && boat[ 0x304 ] == 0 && human_i32( boat, 0x308 ) == 0 )
    {
        if ( human_i32( t.animation, 0x78 ) != 2 && hooks.misfire )
            hooks.misfire( hooks.user );
        if ( human_ge( 0.0f, state_f( boat, 0x328 ) ) && state_f( boat, 0x4B4 ) > 1.0f && hooks.voice )
            hooks.voice( hooks.user, "NeedPickUpPowerBoost" );
    }
    boat[ 0x304 ] = boost;
    boat[ 0x305 ] = 0;
    if ( jump )
        boat_jump_request( boat, t.body, t.solver, hooks );
    boat[ 0x306 ] = recover;
    const float k = ai_minss( state_f( drv, 0x98 ) * dt, 1.0f );
    const float smooth = ( 1.0f - k ) * state_f( drv, 0xAC ) + k * roll;
    state_put_f( drv, 0xAC, smooth );
    state_put_f( t.camera, 0x2CC, smooth );
    t.camera[ 0x2D0 ] = 0;
    if ( recover && human_i32( boat, 0x308 ) == 0 && boat[ 0x464 ] == 0 && hooks.menu_change )
        hooks.menu_change( hooks.user );
}

// 0x40FA90 (__usercall: ecx the driver, xmm1 dt): the wrong-way meter. Up when
// the driver has a sector whose waypoint shows the wrong way (+0x8C "Show
// Wrong Way"), the boat's +0x32F is clear, the speed VuMath_sqrt((vx^2 + vy^2)
// + vz^2) > +0xA0, the velocity's cosine to the sector's direction (+0xA0)
// ((d.x v.x + d.y v.y) + d.z v.z) / speed < +0xA4 and 0 > the forward row's
// dot with it (all ordered): +0x88 += dt, else -= dt; +0x88 = maxss(0,
// minss(+0xA8, +0x88)). The flag +0x8C clears at 0 >= +0x88 and sets at +0x88
// >= +0xA8 (ordered).
inline void human_wrong_way( const human_tick_t& t, const std::uint8_t* sector, bool show, float dt )
{
    std::uint8_t* drv = t.driver;
    bool up = false;
    if ( sector && show && !t.boat[ 0x32F ] )
    {
        const float vx = state_f( t.body, 0x140 ), vy = state_f( t.body, 0x144 ), vz = state_f( t.body, 0x148 );
        const float speed = hull_sqrt( ( vx * vx + vy * vy ) + vz * vz );
        if ( human_gt( speed, state_f( drv, 0xA0 ) ) )
        {
            const float dx = state_f( sector, 0xA0 ), dy = state_f( sector, 0xA4 ), dz = state_f( sector, 0xA8 );
            const float q = ( ( dx * vx + dy * vy ) + dz * vz ) / speed;
            if ( human_gt( state_f( drv, 0xA4 ), q ) )
            {
                const float f = ( state_f( t.transform, 0x64 ) * dy + state_f( t.transform, 0x60 ) * dx ) + state_f( t.transform, 0x68 ) * dz;
                up = human_gt( 0.0f, f );
            }
        }
    }
    const float v = up ? state_f( drv, 0x88 ) + dt : state_f( drv, 0x88 ) - dt;
    const float limit = state_f( drv, 0xA8 );
    const float c = ai_maxss( 0.0f, ai_minss( limit, v ) );
    state_put_f( drv, 0x88, c );
    if ( drv[ 0x8C ] )
    {
        if ( human_ge( 0.0f, c ) )
            drv[ 0x8C ] = 0;
    }
    else if ( human_ge( c, limit ) )
        drv[ 0x8C ] = 1;
}

// 0x40FC60 (__usercall: ecx the driver, xmm1 dt; the tick calls it while
// +0xB9): +0xC4 = 1 while the boat boosts (+0x304), else -= dt; +0xBC += dt
// while +0xC4 >= 0 (ordered), else 0. While 0 < +0xC0 (or a NaN): with +0xBC
// > 0 and 0.35 > +0xC0 + +0xBC (0x6F7648) the boost jump 0x41DEC0 and +0xB9 =
// 0, +0xBA = 1 (one word store); then +0xC0 -= dt and +0xB9 = 0 once 0 >=
// +0xC0 (ordered).
inline void human_boost_window( const human_tick_t& t, float dt, const human_hooks_t& hooks )
{
    std::uint8_t* drv = t.driver;
    if ( t.boat[ 0x304 ] )
        state_put_f( drv, 0xC4, 1.0f );
    else
        state_put_f( drv, 0xC4, state_f( drv, 0xC4 ) - dt );
    if ( human_ge( state_f( drv, 0xC4 ), 0.0f ) )
        state_put_f( drv, 0xBC, dt + state_f( drv, 0xBC ) );
    else
        state_put_f( drv, 0xBC, 0.0f );
    const float c0 = state_f( drv, 0xC0 );
    if ( human_ge( 0.0f, c0 ) )
        return;
    const float bc = state_f( drv, 0xBC );
    if ( human_gt( bc, 0.0f ) && human_gt( ai_bits( 0x3EB33333u ), c0 + bc ) )
    {
        boat_jump_impulse( t.boat, t.transform, t.body, hooks );
        drv[ 0xB9 ] = 0;
        drv[ 0xBA ] = 1;
    }
    const float v = state_f( drv, 0xC0 ) - dt;
    state_put_f( drv, 0xC0, v );
    if ( human_ge( 0.0f, v ) )
        drv[ 0xB9 ] = 0;
}

// 0x40FD30 (thiscall, the driver): the rumble. Under method 0 with the
// settings' +0x34: the gamepad's (pad 0) vt+0(+0xC8, maxss(+0xCC, minss(boat
// +0x3F8, 0.2))) (0x6F7620). +0xC8 = +0xCC = 0 always.
inline void human_rumble( const human_tick_t& t, const human_env_t& env, const human_hooks_t& hooks )
{
    std::uint8_t* drv = t.driver;
    const float m = ai_minss( state_f( t.boat, 0x3F8 ), ai_bits( 0x3E4CCCCDu ) );
    if ( env.control_method == 0 && env.rumble && hooks.rumble )
        hooks.rumble( hooks.user, state_f( drv, 0xC8 ), ai_maxss( state_f( drv, 0xCC ), m ) );
    state_put_f( drv, 0xC8, 0.0f );
    state_put_f( drv, 0xCC, 0.0f );
}

// The sector refresh 0x40F270 and the ghost's 0x40EBA0 share: the sector is
// re-found (the track manager's slot 21 0x4D09F0 with the position and the
// checkpoint id) when there is none or the least of its four planes at the
// position ((n.y p.y + n.x p.x) + n.z p.z + d; minss from FLT_MAX) is below 0
// or a NaN (comiss / jae).
inline void human_sector_refresh( const track_manager_t& tm, const float* pos, int& sector, std::int32_t checkpoint )
{
    bool refind = sector < 0;
    if ( !refind )
    {
        const std::uint8_t* q = tm.sectors[ sector ].image;
        float m = 3.4028235e38f;
        for ( int k = 0; k < 4; ++k )
        {
            const int at = 0x40 + 16 * k;
            const float d = ( ( state_f( q, at + 4 ) * pos[ 1 ] + state_f( q, at ) * pos[ 0 ] ) + state_f( q, at + 8 ) * pos[ 2 ] ) + state_f( q, at + 12 );
            m = track_minss( m, d );
        }
        refind = !human_ge( m, 0.0f );
    }
    if ( refind )
        sector = track_nearest( tm, pos, checkpoint, false );
}

// VuHumanDriver slot 18 0x40F270 (thiscall, the driver; dt; retn 4). With dt >
// 0 (ordered) the input 0x40F690, the wrong-way meter 0x40FA90 and, while
// +0xB9, 0x40FC60; then +0xB8 = 0 and the rumble 0x40FD30. The sector is
// re-found (the track manager's slot 21 0x4D09F0 with the position, its w
// lane copied, and +0x78) when there is none or the least of its four planes
// at the position (minss from FLT_MAX) is below 0 or a NaN. Last the boat's
// wrong-way display +0x51C = maxss(0, minss(1, ((+0x8C ? 1 : -1) x 2) x dt +
// +0x51C)).
inline void human_tick( const track_manager_t& tm, const human_tick_t& t, float dt, const human_env_t& env, const human_hooks_t& hooks )
{
    std::uint8_t* drv = t.driver;
    if ( human_gt( dt, 0.0f ) )
    {
        human_input( t, dt, env, hooks );
        const int s = *t.sector;
        const std::uint8_t* image = s >= 0 ? tm.sectors[ s ].image : nullptr;
        const bool show = s >= 0 && tm.sectors[ s ].waypoint >= 0 && tm.waypoints[ tm.sectors[ s ].waypoint ].p.show_wrong_way != 0;
        human_wrong_way( t, image, show, dt );
        if ( drv[ 0xB9 ] )
            human_boost_window( t, dt, hooks );
    }
    drv[ 0xB8 ] = 0;
    human_rumble( t, env, hooks );
    human_sector_refresh( tm, reinterpret_cast< const float* >( t.transform + 0x80 ), *t.sector, human_i32( drv, 0x78 ) );
    const float sign = drv[ 0x8C ] ? 1.0f : -1.0f;
    state_put_f( t.boat, 0x51C, ai_maxss( 0.0f, ai_minss( 1.0f, ( sign * 2.0f ) * dt + state_f( t.boat, 0x51C ) ) ) );
}

// The driver step of the player's boat's Decision tick (0x41EFF0 calls the
// driver's vt+0x48; VuHumanDriver's is 0x40F270): human_tick on the pilot's
// driver image and its sector. As fleet_driver_tick for an AI boat.
struct human_pilot_t
{
    std::uint8_t* driver = nullptr;
    int sector = -1;
};

inline void human_driver_tick( const track_manager_t& tm, human_pilot_t& p, human_tick_t t, float dt, const human_env_t& env,
    const human_hooks_t& hooks )
{
    t.driver = p.driver;
    t.sector = &p.sector;
    human_tick( tm, t, dt, env, hooks );
}

// boat_decision_tick's driver_tick hook for the player's boat (state.hxx's
// boat_decision_hooks_t: driver_tick = human_decision_driver, user = one of
// these).
struct human_decision_driver_t
{
    const track_manager_t* track = nullptr;
    human_pilot_t* pilot = nullptr;
    human_tick_t tick;
    human_env_t env;
    human_hooks_t hooks;
};

inline void human_decision_driver( void* user, float dt )
{
    human_decision_driver_t* d = static_cast< human_decision_driver_t* >( user );
    human_driver_tick( *d->track, *d->pilot, d->tick, dt, d->env, d->hooks );
}

// ---- The explosion listener (driver +0x80, vtable 0x737F30) and slots 6 / 7 / 20 ----
//
// Slot 6 0x40F210 (gameInitialize) / slot 7 0x40F240 (gameRelease): the
// explosion manager g_vuexplosionmanagerimpl 0x759034's vt+0x1C / vt+0x20
// with the listener (the driver + 0x80; a null driver passes 0) - hooks
// here. The listener's slots reach the driver's boat through its +0x5C
// (the listener's -0x24).

// Listener slot 0 0x40F4F0 (thiscall; out*; retn 4): the boat's position -
// its transform component's (+0x50) +0x80..+0x8F, all four lanes.
inline void human_explosion_position( const std::uint8_t* transform, float out[ 4 ] )
{
    std::memcpy( out, transform + 0x80, 16 );
}

// Listener slot 1 0x40F520 (a float in st0): VuMath_sqrt(((y x 0.5)^2 +
// (x x 0.5)^2) + (z x 0.5)^2) x 0.5 with (x, y, z) = the boat's +0x280.. less
// +0x270.. (the aabb's max less its min).
inline float human_explosion_radius( const std::uint8_t* boat )
{
    const float y = ( state_f( boat, 0x284 ) - state_f( boat, 0x274 ) ) * 0.5f;
    const float x = ( state_f( boat, 0x280 ) - state_f( boat, 0x270 ) ) * 0.5f;
    const float z = ( state_f( boat, 0x288 ) - state_f( boat, 0x278 ) ) * 0.5f;
    return hull_sqrt( ( y * y + x * x ) + z * z ) * 0.5f;
}

// Listener slot 2 0x40F5A0 (thiscall; the explosion's params*, scale; retn
// 8). m = params +0x18 x scale. On the boat's camera (+0x224): when its
// shake timer +0x2E8 is ordered <= 0 (a NaN is not), or m is ordered above the
// amplitude +0x2E4: +0x2E4 = m, +0x2EC = params +0x20 (the duration), +0x2F0
// = params +0x24 (the frequency), +0x2E8 = params +0x1C + params +0x20,
// +0x2F4.. = (0, 0, 1). The rumble pair: driver +0xC8 = maxss(+0xC8, params
// +0x28 x scale), +0xCC = maxss(+0xCC, params +0x2C x scale). Then the lens
// water manager g 0x759048's vt+0x28(params +0x30) - `lens_water`, a hook.
inline void human_explosion_hit( std::uint8_t* drv, std::uint8_t* cam, const std::uint8_t* params, float scale,
                                 const std::function< void( float ) >& lens_water )
{
    const float m = state_f( params, 0x18 ) * scale;
    if ( human_ge( 0.0f, state_f( cam, 0x2E8 ) ) || human_gt( m, state_f( cam, 0x2E4 ) ) )
    {
        const float duration = state_f( params, 0x20 );
        state_put_f( cam, 0x2E4, m );
        state_put_f( cam, 0x2EC, duration );
        std::memcpy( cam + 0x2F0, params + 0x24, 4 );
        state_put_f( cam, 0x2F4, 0.0f );
        state_put_f( cam, 0x2E8, state_f( params, 0x1C ) + duration );
        state_put_f( cam, 0x2F8, 0.0f );
        state_put_f( cam, 0x2FC, 1.0f );
    }
    state_put_f( drv, 0xC8, ai_maxss( state_f( drv, 0xC8 ), state_f( params, 0x28 ) * scale ) );
    state_put_f( drv, 0xCC, ai_maxss( state_f( drv, 0xCC ), state_f( params, 0x2C ) * scale ) );
    if ( lens_water )
        lens_water( state_f( params, 0x30 ) );
}

// Slot 20 0x40F4A0 (the recovery notice): g_vueventmanager 0x759028's
// broadcast 0x505690(FNV-1a("OnHumanRecovered") from 0x811C9DC5, an empty
// VuParams - its +0x80 size 0) - `broadcast`, a hook.
inline std::uint32_t human_event_hash( const char* name )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( ; *name; ++name )
        h = ( h ^ static_cast< unsigned char >( *name ) ) * 0x1000193u;
    return h;
}

inline void human_recovered( const std::function< void( std::uint32_t hash, std::uint32_t params_size ) >& broadcast )
{
    if ( broadcast )
        broadcast( human_event_hash( "OnHumanRecovered" ), 0u );
}
// Slots 6 / 7: the listener (the driver + 0x80, or null for a null driver)
// to the explosion manager's vt+0x1C (add) / vt+0x20 (remove).
inline void human_game_initialize( std::uint8_t* drv, const std::function< void( std::uint8_t* listener ) >& add_listener )
{
    if ( add_listener )
        add_listener( drv ? drv + 0x80 : nullptr );
}

inline void human_game_release( std::uint8_t* drv, const std::function< void( std::uint8_t* listener ) >& remove_listener )
{
    if ( remove_listener )
        remove_listener( drv ? drv + 0x80 : nullptr );
}