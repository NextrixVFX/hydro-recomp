#pragma once

#include <workspace/core/boat/fleet.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>

// The power-ups (VuPowerUpEntity, ctor 0x43B180, vtable 0x73AE54; with the
// VuRigidBodyContactCallback part at +0x5C (vtable 0x73AE4C) and the
// VuMotionComponentIF part at +0x60 (0x73AE3C)). VuPowerUpBoostEntity (ctor
// 0x43D860, vtable 0x73AED4) adds +0x138 "Randomize for Multiplayer" (1),
// +0x13C the boost energy (0; slot 14 0x43DA90 reads "BoostEnergy" from the
// PowerUpDB entry) and +0x140 the multiplayer byte (0; slot 13 0x43D960 sets
// it, with the "MPBoost" entry's pfx / audio, when +0x138 and the boat
// manager's human count +0x28 > 1). The images here are retail's layout.

inline float powerup_f( const std::uint8_t* p, int at )
{
    float v;
    std::memcpy( &v, p + at, 4 );
    return v;
}

// fistp (x87, round to nearest) of a float: the integer indefinite
// 0x80000000 for a NaN or a value outside int32.
inline std::int32_t powerup_fistp( float x )
{
    const double r = std::nearbyint( static_cast< double >( x ) );
    if ( !( r >= -2147483648.0 && r <= 2147483647.0 ) )
        return static_cast< std::int32_t >( 0x80000000u );
    return static_cast< std::int32_t >( r );
}

// cvttss2si: truncation, 0x80000000 for a NaN or out of range.
inline std::int32_t powerup_cvtt( float x )
{
    if ( !( x >= -2147483648.0f && x < 2147483648.0f ) )
        return static_cast< std::int32_t >( 0x80000000u );
    return static_cast< std::int32_t >( x );
}

// VuPowerUpBoostEntity slot 15 0x43DB40 (thiscall boat; retn 4): what a
// picked-up boost gives the boat. e = +0x13C; with the multiplayer byte
// +0x140: b = boat +0x474, n = fistp((1 - b) + b * 7) (the subtraction and
// the product in float, then added), the global VuRandom 0x736810's draw r,
// k = n - 1 + cvttss2si(r * 3) (32-bit wraparound), k = min(k, 8) then
// max(k, 1) (cmovg / cmovl), e = (float)k * 2. Then boat +0x314 = e, boat
// +0x318 = 0; returns true.
inline bool powerup_boost_apply( const std::uint8_t* powerup, std::uint8_t* boat, fleet_random_t& random )
{
    float e = powerup_f( powerup, 0x13C );
    if ( powerup[ 0x140 ] )
    {
        const float b = powerup_f( boat, 0x474 );
        const std::int32_t n = powerup_fistp( ( 1.0f - b ) + b * 7.0f );
        const float r = random.draw( );
        std::int32_t k = static_cast< std::int32_t >( static_cast< std::uint32_t >( n ) - 1u + static_cast< std::uint32_t >( powerup_cvtt( r * 3.0f ) ) );
        if ( k > 8 )
            k = 8;
        if ( k < 1 )
            k = 1;
        e = static_cast< float >( k ) * 2.0f;
    }
    std::memcpy( boat + 0x314, &e, 4 );
    std::memset( boat + 0x318, 0, 4 );
    return true;
}

// ---- VuPowerUpEntity ------------------------------------------------------
//
// The ctor 0x43B180's fields and defaults (properties in registration
// order: "Initially Enabled", "Type" (a PowerUpDB entry name), "Reflect",
// "Handle Wave Height", "Height Above Water", "Voice Over Entry"; plugs
// "Enable" 0x43B140 -> vt+0x34, "Disable" 0x43B160 -> vt+0x30). The PowerUpDB
// entry's StaticPfx / ApplyPfx / Radius / RecoverTime / AudioEvent /
// ScaleSpring... fill the rest (0x43C1F0). +0x50 is the transform component
// (its position +0x80..+0x8C), +0x64 the 3d layout component, +0x114 the
// rigid body (0x43C700: a btSphereShape of +0xE0, contact callback +0x5C).
struct powerup_t
{
    std::uint8_t initially_enabled = 1;   // +0x70
    std::string type;                     // +0x74
    std::uint8_t reflect = 0;             // +0x8C
    std::uint8_t handle_wave_height = 0;  // +0x8D
    float height_above_water = 0.0f;      // +0x90
    std::string voice_over;               // +0x94 ("Voice Over Entry", VoiceOverDB)
    std::string static_pfx;               // +0xB0
    std::string apply_pfx;                // +0xC8
    float radius = 1.0f;                  // +0xE0
    float recover_time = 5.0f;            // +0xE4 (the per-boat pickup cooldown)
    std::string audio_event;              // +0xE8
    float spring = 1.0f;                  // +0x100 ScaleSpringCoeff
    float damping = 1.0f;                 // +0x104 ScaleDampingCoeff
    float scale_initial = 1.0f;           // +0x108 ScaleInitial
    float scale_initial_velocity = 0.0f;  // +0x10C ScaleInitialVelocity
    std::uint32_t pfx = 0;                // +0x110 the StaticPfx's handle (0x43C5E0)
    // +0x118 std::map< entity id, double > (head 0x43CEB0, operator[]
    // 0x43C980): the time each boat last touched it. SUBSTITUTION: std::map
    // for MSVC's tree - the same ordered unique-key semantics, operator[]
    // inserting 0.0.
    std::map< std::uint32_t, double > last_pickup;
    double time = 0.0;                    // +0x120
    std::uint8_t in_world = 0;            // +0x128 (slot 13 sets, slot 12 clears)
    std::uint8_t active = 1;              // +0x129 (0: the game mode is in DisabledInGames)
    std::uint32_t pickup = 0;             // +0x12C the id of the boat that touched it (0 none)
    float scale_velocity = 0.0f;          // +0x130
    float scale = 1.0f;                   // +0x134
};

// What the Decision tick 0x43BDB0 calls that is not the power-up's own:
//   water_height    0x529140 on g_vuwater 0x759058 with the transform
//                   component's +0x80..+0x8C (16 bytes): its result's +0x20
//                   (0xFEFFFFFF when no water answers - the water area's);
//   moved           0x534830, 0x534C00(1) on the component, then its +0xC0
//                   object's vt+8 when set (the entity framework);
//   find            g_vu_entity_repository 0x759080 vt+0x28(id);
//   is_boat         the entity's vt+4 RTTI chain holds VuBoatEntity off_73628C;
//   apply           the power-up's own vt+0x3C (slot 15; the boost's 0x43DB40);
//   apply_pfx       the ApplyPfx: g_vupfxmanager 0x75905C's 0x52BAC0(name, 1),
//                   0x52BB20, the effect's placement at the component's
//                   position and its VuPfxBoatSpringConstraint pointed at the
//                   boat (the pfx system's);
//   driver_38 / _30 the boat's driver (+0x220) vt+0x38, then vt+0x30 (only
//                   when vt+0x38 answered true). vt+0x38 is 0x404BD0 (mov
//                   al, 1) in all five driver vtables; vt+0x30 is
//                   boat_driver_kind's human;
//   sound           FMOD getEvent(AudioEvent, 1) on g_vuaudio 0x759010 +8, on
//                   success set3DAttributes(the position, 0, 0) and start (the
//                   audio device);
//   voice           g_vuvoiceovermanagerimpl 0x758FEC vt+0x1C(boat, Voice Over
//                   Entry);
//   pfx_scale       the handle +0x110 through the pfx manager's table (+0x0C
//                   mask, +0x04 count, +0x08 entries, +0x10 shift; the entry's
//                   +0x14 bit 2 and +0x70 serial): the effect +0x68's vt+0x10
//                   with the scale.
struct powerup_tick_hooks_t
{
    std::function< float( const float* position ) > water_height;
    std::function< void( ) > moved;
    std::function< void*( std::uint32_t id ) > find;
    std::function< bool( void* entity ) > is_boat;
    std::function< bool( void* boat ) > apply;
    std::function< void( void* boat ) > apply_pfx;
    std::function< bool( void* boat ) > driver_38;
    std::function< bool( void* boat ) > driver_30;
    std::function< void( const float* position ) > sound;
    std::function< void( void* boat ) > voice;
    std::function< void( std::uint32_t handle, float scale ) > pfx_scale;
};

// The Decision tick 0x43BDB0 (VuMethod1<VuPowerUpEntity, void, float>,
// registered by gameInitialize 0x43B9D0 in "Decision"; thiscall dt). position
// is the transform component's +0x80..+0x8C (x, y, z, w; the bob writes x, y,
// z). time += (double)dt always; the rest only while in the world (+0x128):
//   Handle Wave Height: h = water(position); z = Height Above Water + h; when
//     z > -65536 (comiss / jbe: not for NaN): x, y written back (unchanged),
//     z, then the moved hooks.
//   a pending pickup (+0x12C): the entity it names, +0x12C = 0; when it is a
//     VuBoatEntity and slot 15 accepts it: the ApplyPfx, scale = ScaleInitial,
//     velocity = ScaleInitialVelocity, and - when the driver's vt+0x38 and
//     vt+0x30 both answer true (the local human) - the sound and the voice.
//   the scale spring (single precision, in this order): a = (1 - s) * k -
//     d * v; v' = a * dt + v; s' = max(((a * 0.5) * dt + v) * dt + s, 0)
//     (maxss: NaN and -0 give +0); then pfx_scale(+0x110, s').
// A repository that finds nothing would be dereferenced in retail: UNKNOWN
// (false, nothing after it done).
inline bool powerup_decision_tick( powerup_t& p, float* position, float dt, const powerup_tick_hooks_t& hooks )
{
    p.time = static_cast< double >( dt ) + p.time;
    if ( !p.in_world )
        return true;
    if ( p.handle_wave_height )
    {
        const float h = hooks.water_height ? hooks.water_height( position ) : 0.0f;
        const float z = p.height_above_water + h;
        if ( z > -65536.0f )
        {
            position[ 2 ] = z;
            if ( hooks.moved )
                hooks.moved( );
        }
    }
    if ( p.pickup )
    {
        void* entity = hooks.find ? hooks.find( p.pickup ) : nullptr;
        p.pickup = 0;
        if ( !entity )
            return false;
        if ( hooks.is_boat && hooks.is_boat( entity ) && hooks.apply && hooks.apply( entity ) )
        {
            if ( hooks.apply_pfx )
                hooks.apply_pfx( entity );
            p.scale = p.scale_initial;
            p.scale_velocity = p.scale_initial_velocity;
            if ( hooks.driver_38 && hooks.driver_38( entity ) && hooks.driver_30 && hooks.driver_30( entity ) )
            {
                if ( hooks.sound )
                    hooks.sound( position );
                if ( hooks.voice )
                    hooks.voice( entity );
            }
        }
    }
    const float s = p.scale, v = p.scale_velocity;
    const float a = ( 1.0f - s ) * p.spring - p.damping * v;
    p.scale_velocity = a * dt + v;
    const float next = ( ( a * 0.5f ) * dt + v ) * dt + s;
    p.scale = next > 0.0f ? next : 0.0f;
    if ( hooks.pfx_scale )
        hooks.pfx_scale( p.pfx, p.scale );
    return true;
}
