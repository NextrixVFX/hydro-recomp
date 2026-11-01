#pragma once

#include <workspace/core/boat/hull.hxx>
#include <workspace/core/boat/load.hxx>
#include <workspace/core/boat/math.hxx>
#include <workspace/core/boat/state.hxx>
#include <workspace/core/boat/track.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

// The boat's respawn 0x41D790 and the reset it ends in, 0x41DA90, on the
// boat's image (0x520) and its parts. Boat fields: +0x270 / +0x280 its aabb,
// +0x290 the body offset, +0x338, +0x344 no-track-placement byte, +0x348 the
// recovery matrix (64 b), +0x3D4 / +0x3D8 / +0x3E8 the held orientation,
// +0x460 (>= 3 after a reset), +0x464 the collision-group latch, +0x468, +0x46B
// destroyed.

// The views the two functions touch, each read where retail reads it:
// transform = the transform component (boat +0x50; +0x10 local, +0x50 world,
// +0x80 its position row, +0x90 local euler, +0xA0 world euler);
// transform_parent = its owner's (+0x08) +0x34 is set (0x534830's parented
// branch); body = boat +0x21C (+0x2CC its group word), proxy = body +0xC8 (or
// nullptr; +6 a word); camera = boat +0x224; instigator = boat +0x70;
// checkpoint = the driver's (boat +0x220) +0x78.
struct boat_respawn_t
{
    std::uint8_t* boat = nullptr;
    std::uint8_t* transform = nullptr;
    bool transform_parent = false;
    vu_rigid_body_t* body = nullptr;
    std::uint8_t* proxy = nullptr;
    std::uint8_t* camera = nullptr;
    const std::uint8_t* instigator = nullptr;
    std::int32_t checkpoint = -1;
};

// What the two call that is not recreated: g_vuwater 0x529140 (thiscall out*,
// point*) - the surface under a point; water answers the float its out +0x20
// holds (retail compares it with -1.7014117e38, 0xFEFFFFFF: none at or below);
// 0x534830's parented branch (the local from the parent's world) and
// 0x534C00(0) (the transform's child walk) - the world's transform machinery;
// g_vutriggermanager 0x536350(instigator); the boat's vt+0x44 (slot 17,
// boat_enable); the driver's vt+0x50; 0x505150 (the params: type 11, the
// boat's entity id 0x500D50) + VuEventManager_broadcast (g 0x759028) of the
// FNV-1a hash of "OnBoatRecovered" - the event system.
struct boat_respawn_hooks_t
{
    void* user = nullptr;
    float ( *water )( void* user, const float* point ) = nullptr;
    void ( *transform_parent )( void* user ) = nullptr;
    void ( *transform_children )( void* user ) = nullptr;
    void ( *instigator )( void* user ) = nullptr;
    void ( *enable )( void* user ) = nullptr;
    void ( *driver_50 )( void* user ) = nullptr;
    void ( *recovered )( void* user, std::uint32_t hash ) = nullptr;
};

inline float respawn_bits( std::uint32_t u )
{
    float f;
    std::memcpy( &f, &u, 4 );
    return f;
}

// The water's answer under a point (none: the limit itself).
inline float respawn_water( const boat_respawn_hooks_t& hooks, const float* point )
{
    return hooks.water ? hooks.water( hooks.user, point ) : respawn_bits( 0xFEFFFFFFu );
}

// (atan2(m6, m10), asinf(maxss(-1, minss(1, -m2))), atan2(m1, m0)).
// SUBSTITUTION: the CRT's __libm_sse2_atan2 / asinf served by the host's
// double functions rounded once (as boat_transform_changed).
inline void respawn_euler( const float* m, float* e )
{
    const float v = support_negate( m[ 2 ] );
    const float c = _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( -1.0f ), _mm_min_ss( _mm_set_ss( 1.0f ), _mm_set_ss( v ) ) ) );
    e[ 0 ] = static_cast< float >( std::atan2( static_cast< double >( m[ 6 ] ), static_cast< double >( m[ 10 ] ) ) );
    e[ 1 ] = static_cast< float >( std::asin( static_cast< double >( c ) ) );
    e[ 2 ] = static_cast< float >( std::atan2( static_cast< double >( m[ 1 ] ), static_cast< double >( m[ 0 ] ) ) );
}

// 0x4D29F0 (fastcall: ecx point*, edx dir*; the boat's aabb* and out* on the
// stack, the caller pops): r = dir x (0,0,1) normalised, u = r x dir
// normalised; out = (r, 0) (dir xyz, 0) (u, 0) (point, 1); then the water under
// the point: above the limit, h + (aabb max z - min z) x 0.5 x 0.5 replaces
// out[14] when greater (both comiss, ordered).
inline void respawn_frame( const float* point, const float* dir, const std::uint8_t* aabb, float* out, const boat_respawn_hooks_t& hooks )
{
    const float v5 = dir[ 2 ] * 0.0f;
    const float v6 = dir[ 1 ];
    const float rx0 = v6 - v5;
    const float ry0 = v5 - dir[ 0 ];
    const float rz0 = dir[ 0 ] * 0.0f - v6 * 0.0f;
    const float len = std::sqrt( ( ry0 * ry0 + rx0 * rx0 ) + rz0 * rz0 );
    const float rz = ( 1.0f / len ) * rz0;
    const float rx = ( 1.0f / len ) * ( v6 - v5 );
    const float ry = ( 1.0f / len ) * ry0;
    const float uy = dir[ 0 ] * rz - dir[ 2 ] * rx;
    const float ux = dir[ 2 ] * ry - dir[ 1 ] * rz;
    const float uz = dir[ 1 ] * rx - dir[ 0 ] * ry;
    const float len2 = std::sqrt( ( uy * uy + ux * ux ) + uz * uz );
    const float m[ 16 ] = { ( 1.0f / len ) * rx0, ry, rz, 0.0f, dir[ 0 ], dir[ 1 ], dir[ 2 ], 0.0f,
                            ( 1.0f / len2 ) * ux, ( 1.0f / len2 ) * uy, ( 1.0f / len2 ) * uz, 0.0f,
                            point[ 0 ], point[ 1 ], point[ 2 ], 1.0f };
    std::memcpy( out, m, 64 );
    const float h = respawn_water( hooks, point );
    if ( h > respawn_bits( 0xFEFFFFFFu ) )
    {
        const float v = ( ( state_f( aabb, 0x18 ) - state_f( aabb, 0x08 ) ) * 0.5f ) * 0.5f + h;
        if ( v > out[ 14 ] )
            out[ 14 ] = v;
    }
}

inline void boat_respawn( const track_manager_t& tm, boat_respawn_t& r, const boat_respawn_hooks_t& hooks );

// 0x41DA90 (thiscall boat; pos*, euler*, a word never read, byte a7; retn
// 10h), the reset: +0x460 = maxss(+0x460, 3); the first time (+0x464) the
// body's +0x2CC and its proxy's +6 group words = 0xFFEF; W = identity x
// fromEuler(euler) (0x410A40, 0x435640, 0x4078C0) with row 3 = pos, its z the
// water's height when above the limit; the transform's world = W, its world
// euler (+0xA0..; +0xAC a stack word, UNKNOWN - 0 here), 0x534830 (no parent:
// local = world, local euler = world euler) and 0x534C00(0); R = translation
// (+0x290) x W; the body's velocities 0, setCenterOfMassTransform 0x50E660(R as
// a btTransform, 0x410C30); camera +0x2C4 = 1; with a7 and the instigator
// registered (+0x28) 0x536350; destroyed (+0x46B): cleared, the respawn
// 0x41D790 again, the boat's vt+0x44; +0x468 = 0; with +0x3D4: +0x3D8 = R's row 3
// and +0x3E8 = R's euler (+0x3F4 a stack word, UNKNOWN - 0 here); +0x338 = 0.
inline void boat_reset( const track_manager_t& tm, boat_respawn_t& r, const float* pos, const float* euler, bool a7, const boat_respawn_hooks_t& hooks )
{
    std::uint8_t* boat = r.boat;
    state_put_f( boat, 0x460, _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( state_f( boat, 0x460 ) ), _mm_set_ss( 3.0f ) ) ) );
    if ( boat[ 0x464 ] == 0 )
    {
        boat[ 0x464 ] = 1;
        const std::uint16_t group = 0xFFEF;
        std::memcpy( reinterpret_cast< std::uint8_t* >( r.body ) + 0x2CC, &group, 2 );
        if ( r.proxy )
            std::memcpy( r.proxy + 6, &group, 2 );
    }
    float id[ 16 ], f[ 16 ], w[ 16 ];
    vu_identity( id );
    vu_matrix_from_euler( f, euler );
    hull_multiply( id, w, f );
    w[ 12 ] = pos[ 0 ];
    w[ 13 ] = pos[ 1 ];
    w[ 14 ] = pos[ 2 ];
    const float h = respawn_water( hooks, pos );
    if ( h > respawn_bits( 0xFEFFFFFFu ) )
        w[ 14 ] = h;
    std::uint8_t* t = r.transform;
    std::memcpy( t + 0x50, w, 64 );
    float e[ 3 ];
    respawn_euler( w, e );
    std::memcpy( t + 0xA0, e, 12 );
    image_put32( t, 0xAC, 0 );   // a stack word in retail - UNKNOWN
    if ( r.transform_parent )
    {
        if ( hooks.transform_parent )
            hooks.transform_parent( hooks.user );
    }
    else
    {
        std::memmove( t + 0x10, t + 0x50, 64 );
        std::memmove( t + 0x90, t + 0xA0, 16 );
    }
    if ( hooks.transform_children )
        hooks.transform_children( hooks.user );
    float tr[ 16 ], rm[ 16 ];
    hull_translation( tr, reinterpret_cast< const float* >( boat + 0x290 ) );
    hull_multiply( tr, rm, w );
    solver_rigid_t& b = r.body->body;
    std::memset( b.linear_velocity, 0, 16 );
    std::memset( b.angular_velocity, 0, 16 );
    float basis[ 12 ], origin[ 4 ];
    for ( int row = 0; row < 3; ++row )
    {
        for ( int k = 0; k < 3; ++k )
            basis[ 4 * row + k ] = rm[ row + 4 * k ];
        basis[ 4 * row + 3 ] = 0.0f;
    }
    origin[ 0 ] = rm[ 12 ];
    origin[ 1 ] = rm[ 13 ];
    origin[ 2 ] = rm[ 14 ];
    origin[ 3 ] = 0.0f;
    body_set_center_of_mass_transform( b, basis, origin );
    r.camera[ 0x2C4 ] = 1;
    if ( a7 && r.instigator[ 0x28 ] && hooks.instigator )
        hooks.instigator( hooks.user );
    if ( boat[ 0x46B ] )
    {
        boat[ 0x46B ] = 0;
        boat_respawn( tm, r, hooks );
        if ( hooks.enable )
            hooks.enable( hooks.user );
    }
    boat[ 0x468 ] = 0;
    if ( boat[ 0x3D4 ] )
    {
        std::memcpy( boat + 0x3D8, rm + 12, 16 );
        float re[ 3 ];
        respawn_euler( rm, re );
        std::memcpy( boat + 0x3E8, re, 12 );
        image_put32( boat, 0x3F4, 0 );   // a stack word in retail - UNKNOWN
    }
    image_put32( boat, 0x338, 0 );
}

// FNV-1a 32 of a string (0x811C9DC5, x 0x01000193), as 0x41D790 inlines it.
inline std::uint32_t respawn_fnv( const char* s )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( ; *s; ++s )
        h = ( h ^ static_cast< std::uint8_t >( *s ) ) * 0x01000193u;
    return h;
}

// 0x41D790 (thiscall boat; ret), the respawn: the track manager's slot 22
// (track_nearest: the transform's position, the driver's checkpoint, Allow
// Recovery) - none: nothing. M = the boat's +0x348. Unless +0x344: from that
// sector back along prev[0] (+0xE4 / +0xE8) while its waypoint lacks Allow
// Recovery (+0x8B), the point on its +0x80 -> +0x90 segment nearest the
// position (t = maxss(0, minss(1, num / den))) and M = 0x4D29F0(point, the
// sector's +0xA0 dir). Then 0x41DA90(M's row 3, M's euler, a7 = 0), the
// driver's vt+0x50, the "OnBoatRecovered" event.
inline void boat_respawn( const track_manager_t& tm, boat_respawn_t& r, const boat_respawn_hooks_t& hooks )
{
    std::uint8_t* boat = r.boat;
    const float* p = reinterpret_cast< const float* >( r.transform + 0x80 );
    int s = track_nearest( tm, p, r.checkpoint, true );
    if ( s < 0 )
        return;
    float m[ 16 ];
    std::memcpy( m, boat + 0x348, 64 );
    if ( !boat[ 0x344 ] )
    {
        auto allow = [ & ]( int k ) { return tm.waypoints[ tm.sectors[ k ].waypoint ].p.allow_recovery != 0; };
        if ( !allow( s ) )
            do
            {
                if ( tm.sectors[ s ].prev.empty( ) )
                    break;
                s = tm.sectors[ s ].prev[ 0 ];
            } while ( !allow( s ) );
        const std::uint8_t* q = tm.sectors[ s ].image;
        const float ax = state_f( q, 0x80 ), ay = state_f( q, 0x84 ), az = state_f( q, 0x88 );
        const float dx = state_f( q, 0x90 ) - ax, dy = state_f( q, 0x94 ) - ay, dz = state_f( q, 0x98 ) - az;
        const float num = ( dy * ( p[ 1 ] - ay ) + dx * ( p[ 0 ] - ax ) ) + dz * ( p[ 2 ] - az );
        const float den = ( dx * dx + dy * dy ) + dz * dz;
        const float t = _mm_cvtss_f32( _mm_max_ss( _mm_setzero_ps( ), _mm_min_ss( _mm_set_ss( 1.0f ), _mm_set_ss( num / den ) ) ) );
        const float point[ 4 ] = { ax + dx * t, ay + dy * t, az + dz * t, 0.0f };
        respawn_frame( point, reinterpret_cast< const float* >( q + 0xA0 ), boat + 0x270, m, hooks );
    }
    float e[ 3 ];
    respawn_euler( m, e );
    boat_reset( tm, r, m + 12, e, false, hooks );
    if ( hooks.driver_50 )
        hooks.driver_50( hooks.user );
    if ( hooks.recovered )
        hooks.recovered( hooks.user, respawn_fnv( "OnBoatRecovered" ) );
}
