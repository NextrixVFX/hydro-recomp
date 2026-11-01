#pragma once

// VuFloatingMineEntity (vtables 0x73A6CC / +0x5C 0x73A6B8 / +0xB0 0x73A6AC / +0xB4 0x73A6A4; ctor 0x438690 over
// VuDynamicGamePropEntity's 0x4367E0) - its behaviour, transliterated, and the explosion manager it reaches.
// The property set is world/dynprop.hxx's (dynamic_prop_t's mine half, case mine_properties).
//
// Slot 6 0x438CF0 (gameInitialize): +0x108 (Hard Hit) = 0, then VuDynamicGamePropEntity's 0x437580
// (dynprop.hxx dynamic_prop_game_initialize - whose ticks / callback reach the MINE's slots through the thunks:
// 0x4077E8 -> vt+0x40 = 0x438EA0, the +0xB4 part's slot 0 = 0x438E10); then mine_game_initialize below; then
// "Decision" (0x6875CC) registered with the delegate {0x73A684, this, 0x4077E8} - vt+0x40 0x438EA0 again.
// Slot 7 0x438DD0 (gameRelease): the ticks unregistered (g 0x759008 vt+0x28), the hide vt+0x34, 0x544A20 on the
// rigid component (its body's release).
// The hide (slot 13 0x438220): g_vudynamicsimpl 0x75900C vt+0x30( entity +0xB0 ) - the step callback removed -
// then 0x545F20: when shown (+0x8C): +0x8C = 0, the draw component (+0x6C) off the 3D draw manager when its +0x1E
// (vt+0x2C; +0x1E = 0), the body out of the world when the component's +0x104 (vt+0x40( +0x100 ); +0x104 = 0).
// The input plug "Detonate" (0x6874AC; init 0x439020, handler thunk 0x4390BE -> vt+0x44 = 0x438DF0): +0x215 = 1,
// VuRetVal 0. The output "OnExplode" (0x68751C).

#include <workspace/core/entity/rigid.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>
#include <xmmintrin.h>

// The rest of 0x438CF0 after 0x437580 (thiscall entity): the rigid component (+0x70) +0xF4 = entity +0xB4 (the
// contact callback; the body's +0x2C4 when there is one); group (+0xD4 word) = 0x80, on the body +0x2CA and its
// broadphase proxy's +4 (the caller's: `proxy_group`); +0xD8 |= 2, copied to the body's +0x2D0.
inline void mine_game_initialize( rigid_component_t& c, std::uint8_t* body, const std::function< void( std::uint16_t ) >& proxy_group )
{
    c.word_f4 = 1;   // = entity +0xB4: a pointer word, the marker 1 here (as dynprop.hxx)
    c.group = 0x80;
    if ( body )
    {
        std::memcpy( body + 0x2CA, &c.group, 2 );
        if ( proxy_group )
            proxy_group( c.group );
    }
    c.flags_d8 |= 2u;
    if ( body )
        std::memcpy( body + 0x2D0, &c.flags_d8, 4 );
}

// The contact callback 0x438E10 (thiscall on the entity's +0xB4 part; the contact record; retn 4), whole. The
// record's other body (+8): no owner (+0x2C0 null) -> nothing. An owner whose vt+4 type chain reaches VuBoatEntity
// 0x73628C: its driver (+0x220) vt+0x38 decides (`driver_answer`; 0x404BD0, true, in all five driver vtables).
// Any other owner: the other body's +0x2D0 bit 0. Deciding yes: entity +0x215 = 1 (the explode request), the
// component's +0xD0 |= 4 and copied to its body's +0xD8 (CF_NO_CONTACT_RESPONSE: nothing collides with it after).
struct mine_contact_t
{
    bool owner = false;          // other body +0x2C0 non-null
    bool owner_is_boat = false;  // its RTTI chain reaches 0x73628C
    bool driver_answer = false;  // the boat's driver vt+0x38
    std::uint32_t other_flags = 0;   // other body +0x2D0
};

// True: the request was set (and the flags written).
inline bool mine_contact( const mine_contact_t& in, std::uint8_t& request, rigid_component_t& c, std::uint8_t* body )
{
    if ( !in.owner )
        return false;
    const bool yes = in.owner_is_boat ? in.driver_answer : ( in.other_flags & 1u ) != 0u;
    if ( !yes )
        return false;
    request = 1;
    c.flags_d0 |= 4u;
    if ( body )
        std::memcpy( body + 0xD8, &c.flags_d0, 4 );
    return true;
}

// ---- VuExplosionManagerImpl (vtable 0x74C6AC, g 0x759034) ----------------------------------------------------
// Its listeners (a VuArray: +4 data, +8 count; add vt+0x1C 0x510290 = push_back, remove vt+0x20 0x5102C0) are
// VuExplosionManager::VuExplosionListener (vtable 0x737F20: slot 0 position(out*), slot 1 radius (x87), slot 2
// hit(params*, scale)) - in this game only VuHumanDriver's (+0x80; boat human.hxx human_explosion_*).
struct explosion_listener_t
{
    void* user = nullptr;
    void ( *position )( void* user, float* out4 ) = nullptr;
    float ( *radius )( void* user ) = nullptr;
    void ( *hit )( void* user, const std::uint8_t* params, float scale ) = nullptr;
};

// The explosion's params (the mine's +0x184 part): +0 position (16 bytes), +0x10 Outer Radius, +0x14 Inner Radius,
// +0x18 .. +0x30 the shake / rumble / lens values the listener reads.
// vt+0x18 0x510180 (thiscall manager; params*; retn 4), whole: for each listener in order: p = slot 0; d =
// VuMath_sqrt( ((y - py)^2 + (x - px)^2) + (z - pz)^2 ) (each difference explosion - listener) - slot 1's radius;
// m = maxss( inner, outer ); hit only when m > d (comiss / jbe: ordered); scale = 1.0, or when d > inner
// 1 - (d - inner) / (outer - inner); slot 2( params, scale ). `sqrt` is VuMath_sqrt 0x40B5A0 (the caller's).
inline int explosion_add( const std::uint8_t* params, const std::vector< explosion_listener_t >& listeners, float ( *sqrt_fn )( float ) )
{
    float e[ 3 ], outer, inner;
    std::memcpy( e, params, 12 );
    std::memcpy( &outer, params + 0x10, 4 );
    std::memcpy( &inner, params + 0x14, 4 );
    int hits = 0;
    for ( const explosion_listener_t& l : listeners )
    {
        float p[ 4 ] = { };
        l.position( l.user, p );
        const float dx = e[ 0 ] - p[ 0 ], dy = e[ 1 ] - p[ 1 ], dz = e[ 2 ] - p[ 2 ];
        const float sum = _mm_cvtss_f32( _mm_add_ss( _mm_add_ss( _mm_mul_ss( _mm_set_ss( dy ), _mm_set_ss( dy ) ),
                                                                 _mm_mul_ss( _mm_set_ss( dx ), _mm_set_ss( dx ) ) ),
                                                     _mm_mul_ss( _mm_set_ss( dz ), _mm_set_ss( dz ) ) ) );
        const float r = l.radius( l.user );
        const float d = _mm_cvtss_f32( _mm_sub_ss( _mm_set_ss( sqrt_fn( sum ) ), _mm_set_ss( r ) ) );
        const __m128 m = _mm_max_ss( _mm_set_ss( inner ), _mm_set_ss( outer ) );
        if ( !_mm_comigt_ss( m, _mm_set_ss( d ) ) )
            continue;
        float scale = 1.0f;
        if ( _mm_comigt_ss( _mm_set_ss( d ), _mm_set_ss( inner ) ) )
            scale = _mm_cvtss_f32( _mm_sub_ss( _mm_set_ss( 1.0f ), _mm_div_ss( _mm_sub_ss( _mm_set_ss( d ), _mm_set_ss( inner ) ),
                                                                                 _mm_sub_ss( _mm_set_ss( outer ), _mm_set_ss( inner ) ) ) ) );
        l.hit( l.user, params, scale );
        ++hits;
    }
    return hits;
}

// The mine's Decision tick 0x438EA0 (vt+0x40; dt unread; retn 4): with no explosion yet (+0x214 clear) and a
// request (+0x215): the hide vt+0x34; the explosion part's 0x547BB0( transform +0x50 ) - its position = the
// matrix's row 3 (+0x30, 16 bytes) into the part's +0, the explosion manager's vt+0x18( the part ), the pfx
// (createEffect 0x52BCD0 by "Explosion Pfx System Name", its matrix / start) and FMOD's event by "Explosion
// Audio Event Name" at that position (platform); the point wave: the transform's +0x80 .. +0x8F into +0x1E8 and
// VuWater_createPointWave 0x528E10( +0x1E8 ) (its reference dropped: --refcount, deleted at 0); the word +0x214 =
// 1 (exploded set, the request cleared); "OnExplode" fired (VuScriptComponent findPlug 0x504770, plug vt+8).
struct mine_tick_hooks_t
{
    std::function< void( ) > hide;                                     // vt+0x34 0x438220
    std::function< void( const float* matrix16 ) > explode;            // 0x547BB0 on the part
    std::function< void( const std::uint32_t* desc11 ) > point_wave;   // 0x528E10 (+ the reference drop)
    std::function< void( ) > on_explode;                               // fire "OnExplode"
};

inline bool mine_tick( std::uint8_t& exploded, std::uint8_t& request, const float* world16, const float* wave7, const mine_tick_hooks_t& h )
{
    if ( exploded || !request )
        return false;
    if ( h.hide )
        h.hide( );
    if ( h.explode )
        h.explode( world16 );
    std::uint32_t desc[ 11 ];
    std::memcpy( desc, world16 + 12, 16 );   // the transform's +0x80 .. +0x8F: row 3 of its world (+0x50)
    std::memcpy( desc + 4, wave7, 28 );
    if ( h.point_wave )
        h.point_wave( desc );
    exploded = 1;
    request = 0;
    if ( h.on_explode )
        h.on_explode( );
    return true;
}
