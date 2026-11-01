#pragma once

#include <workspace/core/boat/fleet.hxx>
#include <workspace/core/assets/tree.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

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

// ---- The contact callback and the per-boat cooldown ----------------------
//
// VuRigidBodyContactCallback slot 0 0x43C8C0 (thiscall on the +0x5C part;
// the contact; retn 4): the other body (contact +8)'s owner (+0x2C0) - none,
// or one whose vt+4 RTTI chain lacks VuBoatEntity off_73628C: nothing. Else
// id = its entity id (0x500D50: FNV-1a of the long name), t = map[id]
// (operator[] 0x43C980: absent -> inserted as 0.0), and when (time - t) >
// (double)RecoverTime (comisd / jbe: strict, false for NaN) +0x12C = id; then
// map[id] = time. The Decision tick applies +0x12C on its next run.
inline void powerup_contact( powerup_t& p, bool owner_is_boat, std::uint32_t id )
{
    if ( !owner_is_boat )
        return;
    const double last = p.last_pickup[ id ];
    if ( p.time - last > static_cast< double >( p.recover_time ) )
        p.pickup = id;
    p.last_pickup[ id ] = p.time;
}

// ---- gameInitialize (slot 6 0x43B9D0) ------------------------------------
//
// The game mode's name (0x4D31D0 on g_vugameutil 0x758FF8) against every
// element of the PowerUpDB entry's "DisabledInGames" list (std::string
// equality: same length, same bytes; an element that is not text compares
// as the empty string 0x819950 - pass it as ""): a match -> +0x129 = 0 and
// nothing else (no body, no tick, no seeding). Otherwise the body 0x43C700
// (a btSphereShape of Radius, the contact callback +0x5C, body +0xD8 |= 4 -
// the dynamics: a hook), the Decision tick registered (tick manager vt+0x20,
// "Decision": a hook), slot 13 (vt+0x34) when Initially Enabled, then every
// racer's (the boat manager's +0x18 list, count +0x1C) entity id seeded into
// the map at -FLT_MAX as a double (0xC7EFFFFFE0000000).
struct powerup_init_hooks_t
{
    std::function< void( ) > create_body;
    std::function< void( ) > register_tick;
    std::function< void( ) > enable;   // vt+0x34: powerup_add_to_world, or the boost's 0x43D960
};

inline void powerup_game_initialize( powerup_t& p, const std::string& game_mode, const std::vector< std::string >& disabled_in_games,
    const std::vector< std::uint32_t >& racer_ids, const powerup_init_hooks_t& hooks )
{
    for ( const std::string& g : disabled_in_games )
        if ( g == game_mode )
        {
            p.active = 0;
            return;
        }
    if ( hooks.create_body )
        hooks.create_body( );
    if ( hooks.register_tick )
        hooks.register_tick( );
    if ( p.initially_enabled && hooks.enable )
        hooks.enable( );
    std::uint64_t bits = 0xC7EFFFFFE0000000ull;
    double lowest;
    std::memcpy( &lowest, &bits, 8 );
    for ( std::uint32_t id : racer_ids )
        p.last_pickup[ id ] = lowest;
}

// ---- Into / out of the world (slots 13 / 12 / 7) --------------------------
//
// What these call that is not the power-up's: the StaticPfx 0x43C5E0 (the pfx
// manager's create 0x52BCD0 by name -> the handle +0x110; on a live handle the
// effect placed at the transform, given the scale +0x134 (vt+0x10), started
// (vt+0x2C) and its +0x64 object's +0x1C = Reflect - the pfx system's), its
// release 0x43C6A0 (with a handle: effect vt+0x30(1) when live, 0x52BD50;
// then +0x110 = 0), the dynamics world's add / remove of the body (g
// 0x75900C vt+0x3C / vt+0x40), and the body's destruction / the tick's
// unregistration (gameRelease).
struct powerup_world_hooks_t
{
    std::function< std::uint32_t( const std::string& static_pfx, std::uint8_t reflect, float scale ) > create_pfx;
    std::function< void( std::uint32_t handle ) > release_pfx;
    std::function< void( ) > add_body;
    std::function< void( ) > remove_body;
    std::function< void( ) > destroy_body;    // slot 7: the body's +0xCC shape deleted, then the body (vt+4(1)), +0x114 = 0
    std::function< void( ) > unregister_tick; // slot 7: tick manager vt+0x28(this)
};

// Slot 13 0x43BD40 (the "Enable" plug): when active and not in the world:
// +0x134 = +0x130 = 0, the StaticPfx, the body added, +0x128 = 1.
inline void powerup_add_to_world( powerup_t& p, const powerup_world_hooks_t& hooks )
{
    if ( !p.active || p.in_world )
        return;
    p.scale = 0.0f;
    p.scale_velocity = 0.0f;
    p.pfx = hooks.create_pfx ? hooks.create_pfx( p.static_pfx, p.reflect, p.scale ) : 0;
    if ( hooks.add_body )
        hooks.add_body( );
    p.in_world = 1;
}

// Slot 12 0x43BD00 (the "Disable" plug, and a pickup's end): when active and
// in the world: the pfx released (+0x110 = 0 when it was set), the body
// removed, +0x128 = 0.
inline void powerup_remove_from_world( powerup_t& p, const powerup_world_hooks_t& hooks )
{
    if ( !p.active || !p.in_world )
        return;
    if ( p.pfx )
    {
        if ( hooks.release_pfx )
            hooks.release_pfx( p.pfx );
        p.pfx = 0;
    }
    if ( hooks.remove_body )
        hooks.remove_body( );
    p.in_world = 0;
}

// Slot 7 0x43BCA0 (gameRelease): when active: slot 12 (virtual - every type's
// is 0x43BD00), the body destroyed when there is one, the tick unregistered.
inline void powerup_game_release( powerup_t& p, bool has_body, const powerup_world_hooks_t& hooks )
{
    if ( !p.active )
        return;
    powerup_remove_from_world( p, hooks );
    if ( has_body && hooks.destroy_body )
        hooks.destroy_body( );
    if ( hooks.unregister_tick )
        hooks.unregister_tick( );
}

// ---- The PowerUpDB entry (slot 2 0x43B9C0 -> 0x43C1F0) --------------------
//
// The entry = PowerUpDB["VuDBAsset"][Type] (the Type property +0xAC: its
// asset +0x48, 0x4FBDF0 then std::map find 0x4F22B0; absent -> the null
// container). StaticPfx / ApplyPfx / AudioEvent assigned when text (tag 4);
// Radius, RecoverTime, ScaleSpringCoeff, ScaleDampingCoeff, ScaleInitial,
// ScaleInitialVelocity read as an int (1, converted), a float's bits (2) or
// an int64 (7, converted); anything else leaves the field. Then (hooks): the
// 3d layout component +0x64's bounds +0x18..+0x34 = (-r, -r, -r, r, r, r)
// pairs, and - with the entity's +0x14 bit 2 (in a game) - slot 12, the body
// destroyed and rebuilt (0x43C700), slot 13 again when it was in the world;
// finally slot 14 (vt+0x38: the boost's BoostEnergy read; nothing in the base).
inline void powerup_db_number( const value_t& v, float& out )
{
    if ( v.tag == 1 )
        out = static_cast< float >( v.integer );
    else if ( v.tag == 2 )
        std::memcpy( &out, &v.real, 4 );
    else if ( v.tag == 7 )
        out = static_cast< float >( v.int64 );
}

inline const value_t& powerup_db_entry( const value_t& powerup_db, const std::string& type )
{
    return member( member( powerup_db, "VuDBAsset" ), type.c_str( ) );
}

inline void powerup_load_db( powerup_t& p, const value_t& entry )
{
    const value_t& static_pfx = member( entry, "StaticPfx" );
    if ( static_pfx.tag == 4 )
        p.static_pfx = static_pfx.text;
    const value_t& apply_pfx = member( entry, "ApplyPfx" );
    if ( apply_pfx.tag == 4 )
        p.apply_pfx = apply_pfx.text;
    powerup_db_number( member( entry, "Radius" ), p.radius );
    powerup_db_number( member( entry, "RecoverTime" ), p.recover_time );
    const value_t& audio = member( entry, "AudioEvent" );
    if ( audio.tag == 4 )
        p.audio_event = audio.text;
    powerup_db_number( member( entry, "ScaleSpringCoeff" ), p.spring );
    powerup_db_number( member( entry, "ScaleDampingCoeff" ), p.damping );
    powerup_db_number( member( entry, "ScaleInitial" ), p.scale_initial );
    powerup_db_number( member( entry, "ScaleInitialVelocity" ), p.scale_initial_velocity );
}

// VuPowerUpBoostEntity slot 14 0x43DA90: the same entry's "BoostEnergy" into
// +0x13C (1 / 2 / 7 as above).
inline void powerup_boost_load_energy( float& energy, const value_t& entry )
{
    powerup_db_number( member( entry, "BoostEnergy" ), energy );
}
// ---- VuPowerUpTriggerEntity (ctor 0x43E160, vtable 0x73AFE4) --------------
//
// The base power-up plus an "OnTrigger" output plug (0x505400(2)). Slot 15
// 0x43E240 (thiscall boat; retn 4): the boat's driver (+0x220) vt+0x30 (the
// human answer) false -> returns it (0); else when vt+0x38 (true in all five
// driver vtables) the plug is fired with VuParams(boat) (0x505150,
// VuScriptComponent_findPlug 0x504770 on +0x6C "OnTrigger", the plug's vt+8 -
// the script system: the `fire` hook); returns 1.
inline bool powerup_trigger_apply( bool driver_30, bool driver_38, const std::function< void( ) >& fire )
{
    if ( !driver_30 )
        return false;
    if ( driver_38 && fire )
        fire( );
    return true;
}