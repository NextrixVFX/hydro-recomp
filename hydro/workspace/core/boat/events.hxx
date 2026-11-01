#pragma once

#include <workspace/core/boat/human.hxx>
#include <workspace/core/boat/respawn.hxx>
#include <workspace/core/boat/rtti.hxx>

#include <functional>

#include <cstdint>
#include <cstring>

// VuBoatEntity's event handlers: VuMethod1 delegates the ctor 0x41B850
// registers on the entity (thiscall(boat, VuParams const&), retn 4). The
// boat's +0x220 is its driver, +0x224 its camera (images: the caller's).

// The x86 SSE scalar ops' NaN rule, spelled out (a compiler may commute a * b
// or a + b, which changes which NaN survives): a NaN first operand (the
// destination) wins, quieted; else a NaN second operand; else the IEEE result
// (an invalid one is the default NaN 0xFFC00000 either way). NOTE: Unicorn -
// the oracle - picks between two NaN operands by the x87 rule instead (the
// larger significand, then the positive one); the cases draw one NaN pattern
// so their golden words are the hardware's.
inline float sse_nan_pick( float a, float b, bool& picked )
{
    std::uint32_t w;
    picked = true;
    if ( a != a )
        std::memcpy( &w, &a, 4 );
    else if ( b != b )
        std::memcpy( &w, &b, 4 );
    else
    {
        picked = false;
        return 0.0f;
    }
    w |= 0x00400000u;
    float r;
    std::memcpy( &r, &w, 4 );
    return r;
}
inline float sse_mul( float a, float b ) { bool p; const float n = sse_nan_pick( a, b, p ); return p ? n : a * b; }
inline float sse_add( float a, float b ) { bool p; const float n = sse_nan_pick( a, b, p ); return p ? n : a + b; }
inline float sse_sub( float a, float b ) { bool p; const float n = sse_nan_pick( a, b, p ); return p ? n : a - b; }

// The VuParams read both handlers inline: with the size word (+0x80, signed)
// > 0, the type word at the cursor; type 2 (a float) -> its value at +4, the
// cursor + 8 and the size - 8. Any other type reads nothing and leaves the
// cursor (so every later read sees the same word). 0 when nothing is read.
inline float boat_param_float( const std::uint8_t*& at, std::int32_t& left )
{
    float value = 0.0f;
    std::uint32_t type;
    if ( left > 0 )
    {
        std::memcpy( &type, at, 4 );
        if ( type == 2 )
        {
            std::memcpy( &value, at + 4, 4 );
            at += 8;
            left -= 8;
        }
    }
    return value;
}

// OnCameraShake 0x41E7B0: four floats a, b, c, d. On the camera: when its
// shake timer +0x2E8 is ordered <= 0 (comiss 0, +0x2E8; jae) or a is ordered
// above the amplitude +0x2E4 (comiss a, +0x2E4; jbe skips): +0x2E4 = a,
// +0x2EC = c, +0x2F4 / +0x2F8 = 0, +0x2FC = 1.0, +0x2E8 = c + b, +0x2F0 = d.
inline void boat_on_camera_shake( std::uint8_t* cam, const std::uint8_t* params )
{
    std::int32_t left;
    std::memcpy( &left, params + 0x80, 4 );
    const std::uint8_t* at = params;
    const float a = boat_param_float( at, left );
    const float b = boat_param_float( at, left );
    const float c = boat_param_float( at, left );
    const float d = boat_param_float( at, left );
    if ( human_ge( 0.0f, state_f( cam, 0x2E8 ) ) || human_gt( a, state_f( cam, 0x2E4 ) ) )
    {
        state_put_f( cam, 0x2E4, a );
        state_put_f( cam, 0x2EC, c );
        state_put_f( cam, 0x2F4, 0.0f );
        state_put_f( cam, 0x2F8, 0.0f );
        state_put_f( cam, 0x2FC, 1.0f );
        state_put_f( cam, 0x2E8, sse_add( c, b ) );
        state_put_f( cam, 0x2F0, d );
    }
}

// 0x41E2E0 (thiscall boat; the pair in xmm1 / xmm2): only for a driver whose
// vt+4 type chain holds VuHumanDriver's descriptor 0x73623C - +0xC8 =
// maxss(+0xC8, a), +0xCC = maxss(+0xCC, b).
inline void boat_rumble( std::uint8_t* drv, std::uint32_t drv_vtable, float a, float b )
{
    boat_driver_raise( drv, boat_rtti_is( drv_vtable, 0x73623Cu ), a, b );   // the one recreation of 0x41E2E0 (state.hxx)
}

// OnRumble 0x41E950: two floats, then 0x41E2E0 with them.
inline void boat_on_rumble( std::uint8_t* drv, std::uint32_t drv_vtable, const std::uint8_t* params )
{
    std::int32_t left;
    std::memcpy( &left, params + 0x80, 4 );
    const std::uint8_t* at = params;
    const float a = boat_param_float( at, left );
    const float b = boat_param_float( at, left );
    boat_rumble( drv, drv_vtable, a, b );
}

// ---- btRigidBody's impulse pair, compiled into the boat's code (called by
// the contact handler 0x41E620 and 0x625260), on the body's image: +0x110 ..
// +0x138 the inverse world inertia (rows of 4), +0x140 the linear and +0x150
// the angular velocity, +0x160 the inverse mass, +0x170 the linear and
// +0x260 the angular factor. Each product / sum in the disassembly's operand
// order (sse_mul / sse_add / sse_sub). ----

// applyTorqueImpulse 0x4220C0 (thiscall(body, torque*), retn 4): row k =
// ((M[k][1] t.y + t.x M[k][0]) + M[k][2] t.z) (row 0's first product is
// t.x x M; rows 1 / 2's are M x t.x), the angular velocity += F[k] x row k.
inline void body_apply_torque_impulse( std::uint8_t* b, const float* t )
{
    const float r0 = sse_add( sse_add( sse_mul( state_f( b, 0x114 ), t[ 1 ] ), sse_mul( t[ 0 ], state_f( b, 0x110 ) ) ), sse_mul( state_f( b, 0x118 ), t[ 2 ] ) );
    const float r1 = sse_add( sse_add( sse_mul( state_f( b, 0x124 ), t[ 1 ] ), sse_mul( state_f( b, 0x120 ), t[ 0 ] ) ), sse_mul( state_f( b, 0x128 ), t[ 2 ] ) );
    const float r2 = sse_add( sse_add( sse_mul( state_f( b, 0x134 ), t[ 1 ] ), sse_mul( state_f( b, 0x130 ), t[ 0 ] ) ), sse_mul( state_f( b, 0x138 ), t[ 2 ] ) );
    const float a0 = sse_mul( state_f( b, 0x260 ), r0 ), a1 = sse_mul( state_f( b, 0x264 ), r1 ), a2 = sse_mul( state_f( b, 0x268 ), r2 );
    state_put_f( b, 0x150, sse_add( state_f( b, 0x150 ), a0 ) );
    state_put_f( b, 0x154, sse_add( state_f( b, 0x154 ), a1 ) );
    state_put_f( b, 0x158, sse_add( state_f( b, 0x158 ), a2 ) );
}

// applyImpulse 0x4221C0 (thiscall(body, impulse*, rel_pos*), retn 8): unless
// the inverse mass m is == 0 (ucomiss / lahf / test ah 0x44 / jnp: a NaN
// goes on): s = impulse x linear factor; the linear velocity: x = +0x140 + m
// s.x, y = m s.y + +0x144, z = m s.z + +0x148; then (the angular factor's
// address test, always true) applyTorqueImpulse(rel x s).
inline void body_apply_impulse( std::uint8_t* b, const float* impulse, const float* rel )
{
    const float m = state_f( b, 0x160 );
    if ( !( m != 0.0f ) )
        return;
    const float sx = sse_mul( impulse[ 0 ], state_f( b, 0x170 ) ), sz = sse_mul( impulse[ 2 ], state_f( b, 0x178 ) ),
                sy = sse_mul( impulse[ 1 ], state_f( b, 0x174 ) );
    const float vx = sse_mul( m, sx ), vy = sse_mul( m, sy ), vz = sse_mul( m, sz );
    state_put_f( b, 0x148, sse_add( vz, state_f( b, 0x148 ) ) );
    state_put_f( b, 0x140, sse_add( state_f( b, 0x140 ), vx ) );
    state_put_f( b, 0x144, sse_add( vy, state_f( b, 0x144 ) ) );
    const float torque[ 3 ] = { sse_sub( sse_mul( rel[ 1 ], sz ), sse_mul( rel[ 2 ], sy ) ), sse_sub( sse_mul( rel[ 2 ], sx ), sse_mul( rel[ 0 ], sz ) ),
                                sse_sub( sse_mul( rel[ 0 ], sy ), sse_mul( rel[ 1 ], sx ) ) };
    body_apply_torque_impulse( b, torque );
}
// VuBoatEntity's btMotionState::setWorldTransform 0x41E460 (vtable 0x7383C4
// slot 2; the get is construct.hxx's boat_motion_get) is world/dynamics.hxx's
// boat_motion_set - one recreation; it performs 0x534830 instead of hooking it.
// ---- VuBoatEntity's contact handler 0x41E620 and the two body functions it
// calls, on the bodies' images (solver_rigid_t's offsets: +0xD8 flags, +0x40
// the origin, +0x110 .. +0x138 the inverse world inertia rows, +0x140 the
// linear / +0x150 the angular velocity, +0x160 the inverse mass; then +0x2CA
// the short and +0x2D0 the flags VuRigidBody adds). Each operation in its
// disassembly's operand order (sse_mul / sse_add / sse_sub: dst first). ----

// 0x4AD340 (ecx body A, edx body B; point*, normal* - cdecl, the caller pops;
// result in xmm0): 0 when either body's flags +0xD8 & 4; else v = the
// velocity at the point of A minus B's (w x r + v, r = p - origin), x =
// (-(n.y v.y + v.x n.x) + n.z v.z) - 5 (0x6F7768)) x 0.022222223
// (0x6F75E8), then minss(1, x) and maxss(0, that).
inline float body_contact_strength( const std::uint8_t* a, const std::uint8_t* b, const float* p, const float* n )
{
    std::uint32_t fa, fb;
    std::memcpy( &fa, a + 0xD8, 4 );
    std::memcpy( &fb, b + 0xD8, 4 );
    if ( ( fa & 4 ) || ( fb & 4 ) )
        return 0.0f;
    const float bx = state_f( b, 0x150 ), by = state_f( b, 0x154 ), bz = state_f( b, 0x158 );
    const float rby = sse_sub( p[ 1 ], state_f( b, 0x44 ) ), rbx = sse_sub( p[ 0 ], state_f( b, 0x40 ) ), rbz = sse_sub( p[ 2 ], state_f( b, 0x48 ) );
    const float raz = sse_sub( p[ 2 ], state_f( a, 0x48 ) );
    const float vbx = sse_add( sse_sub( sse_mul( rbz, by ), sse_mul( rby, bz ) ), state_f( b, 0x140 ) );
    const float vbz = sse_add( sse_sub( sse_mul( rby, bx ), sse_mul( rbx, by ) ), state_f( b, 0x148 ) );
    const float vby = sse_add( sse_sub( sse_mul( rbx, bz ), sse_mul( rbz, bx ) ), state_f( b, 0x144 ) );
    const float ax = state_f( a, 0x150 ), ay = state_f( a, 0x154 ), az = state_f( a, 0x158 );
    const float rax = sse_sub( p[ 0 ], state_f( a, 0x40 ) ), ray = sse_sub( p[ 1 ], state_f( a, 0x44 ) );
    const float vax = sse_add( sse_sub( sse_mul( raz, ay ), sse_mul( ray, az ) ), state_f( a, 0x140 ) );
    const float vay = sse_add( sse_sub( sse_mul( rax, az ), sse_mul( raz, ax ) ), state_f( a, 0x144 ) );
    const float vaz = sse_add( sse_sub( sse_mul( ray, ax ), sse_mul( rax, ay ) ), state_f( a, 0x148 ) );
    const float dx = sse_sub( vax, vbx ), dy = sse_sub( vay, vby ), dz = sse_sub( vaz, vbz );
    float x = sse_add( sse_add( sse_mul( n[ 1 ], dy ), sse_mul( dx, n[ 0 ] ) ), sse_mul( n[ 2 ], dz ) );
    x = sse_mul( sse_sub( support_negate( x ), 5.0f ), support_bits( 0x3CB60B61u ) );
    const float lo = _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( 1.0f ), _mm_set_ss( x ) ) );
    return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( 0.0f ), _mm_set_ss( lo ) ) );
}

// 0x534DA0 (ecx body A, edx body B; point*, normal*; xmm0): 0 when either
// flags +0xD8 & 4; vn = (A's velocity at the point - B's) . n (the same
// terms as above in this function's own order); 0 unless 0 > vn (`comiss;
// jbe`); else -vn / ((B's ((I_B (rB x n)) x rB) . n + 1/m_B) + (A's (same) +
// 1/m_A)) - the impulse that stops the approach (no restitution).
inline float body_contact_impulse( const std::uint8_t* a, const std::uint8_t* b, const float* p, const float* n )
{
    std::uint32_t fa, fb;
    std::memcpy( &fa, a + 0xD8, 4 );
    std::memcpy( &fb, b + 0xD8, 4 );
    if ( ( fa & 4 ) || ( fb & 4 ) )
        return 0.0f;
    const float bx = state_f( b, 0x150 ), by = state_f( b, 0x154 ), bz = state_f( b, 0x158 );
    const float rby = sse_sub( p[ 1 ], state_f( b, 0x44 ) ), rbx = sse_sub( p[ 0 ], state_f( b, 0x40 ) ), rbz = sse_sub( p[ 2 ], state_f( b, 0x48 ) );
    const float vbx = sse_add( state_f( b, 0x140 ), sse_sub( sse_mul( rbz, by ), sse_mul( rby, bz ) ) );
    const float vby = sse_add( state_f( b, 0x144 ), sse_sub( sse_mul( rbx, bz ), sse_mul( rbz, bx ) ) );
    const float vbz = sse_add( state_f( b, 0x148 ), sse_sub( sse_mul( rby, bx ), sse_mul( rbx, by ) ) );
    const float ax = state_f( a, 0x150 ), ay = state_f( a, 0x154 ), az = state_f( a, 0x158 );
    const float ray = sse_sub( p[ 1 ], state_f( a, 0x44 ) ), rax = sse_sub( p[ 0 ], state_f( a, 0x40 ) ), raz = sse_sub( p[ 2 ], state_f( a, 0x48 ) );
    const float vax = sse_add( state_f( a, 0x140 ), sse_sub( sse_mul( raz, ay ), sse_mul( ray, az ) ) );
    const float vay = sse_add( state_f( a, 0x144 ), sse_sub( sse_mul( rax, az ), sse_mul( raz, ax ) ) );
    const float vaz = sse_add( state_f( a, 0x148 ), sse_sub( sse_mul( ray, ax ), sse_mul( rax, ay ) ) );
    const float vn = sse_add( sse_add( sse_mul( sse_sub( vax, vbx ), n[ 0 ] ), sse_mul( sse_sub( vay, vby ), n[ 1 ] ) ),
                              sse_mul( sse_sub( vaz, vbz ), n[ 2 ] ) );
    if ( !( 0.0f > vn ) )
        return 0.0f;
    // A: k = rA x n, t = I_A k (rows +0x110 / +0x120 / +0x130), u = t x rA.
    const float kx = sse_sub( sse_mul( ray, n[ 2 ] ), sse_mul( raz, n[ 1 ] ) );
    const float ky = sse_sub( sse_mul( raz, n[ 0 ] ), sse_mul( rax, n[ 2 ] ) );
    const float kz = sse_sub( sse_mul( rax, n[ 1 ] ), sse_mul( ray, n[ 0 ] ) );
    const float t0 = sse_add( sse_add( sse_mul( state_f( a, 0x114 ), ky ), sse_mul( kx, state_f( a, 0x110 ) ) ), sse_mul( state_f( a, 0x118 ), kz ) );
    const float t1 = sse_add( sse_add( sse_mul( state_f( a, 0x124 ), ky ), sse_mul( kx, state_f( a, 0x120 ) ) ), sse_mul( state_f( a, 0x128 ), kz ) );
    const float t2 = sse_add( sse_add( sse_mul( state_f( a, 0x134 ), ky ), sse_mul( kx, state_f( a, 0x130 ) ) ), sse_mul( state_f( a, 0x138 ), kz ) );
    const float ux = sse_sub( sse_mul( t1, raz ), sse_mul( t2, ray ) );
    const float uz = sse_sub( sse_mul( t0, ray ), sse_mul( t1, rax ) );
    const float uy = sse_sub( sse_mul( t2, rax ), sse_mul( t0, raz ) );
    // B: j = rB x n, s = I_B j, w = s x rB.
    const float jx = sse_sub( sse_mul( rby, n[ 2 ] ), sse_mul( rbz, n[ 1 ] ) );
    const float jy = sse_sub( sse_mul( rbz, n[ 0 ] ), sse_mul( rbx, n[ 2 ] ) );
    const float jz = sse_sub( sse_mul( rbx, n[ 1 ] ), sse_mul( rby, n[ 0 ] ) );
    const float s0 = sse_add( sse_add( sse_mul( state_f( b, 0x114 ), jy ), sse_mul( jx, state_f( b, 0x110 ) ) ), sse_mul( state_f( b, 0x118 ), jz ) );
    const float s1 = sse_add( sse_add( sse_mul( state_f( b, 0x124 ), jy ), sse_mul( state_f( b, 0x120 ), jx ) ), sse_mul( state_f( b, 0x128 ), jz ) );
    const float s2 = sse_add( sse_add( sse_mul( state_f( b, 0x134 ), jy ), sse_mul( state_f( b, 0x130 ), jx ) ), sse_mul( state_f( b, 0x138 ), jz ) );
    const float wx = sse_sub( sse_mul( s1, rbz ), sse_mul( s2, rby ) );
    const float wy = sse_sub( sse_mul( s2, rbx ), sse_mul( s0, rbz ) );
    const float wz = sse_sub( sse_mul( s0, rby ), sse_mul( s1, rbx ) );
    const float db = sse_add( sse_add( sse_add( sse_mul( wy, n[ 1 ] ), sse_mul( wx, n[ 0 ] ) ), sse_mul( wz, n[ 2 ] ) ), state_f( b, 0x160 ) );
    const float da = sse_add( sse_add( sse_add( sse_mul( uy, n[ 1 ] ), sse_mul( ux, n[ 0 ] ) ), sse_mul( uz, n[ 2 ] ) ), state_f( a, 0x160 ) );
    return support_negate( vn ) / sse_add( db, da );
}

// The contact handler 0x41E620 (VuBoatEntity's btCollisionObject contact
// interface; thiscall this = boat +0x64; the contact*; retn 4): contact +8 the
// other body, +0xC the point, +0x1C the normal. Nothing when the other's
// flags +0xD8 & 4. s = 0x4AD340(the boat's body, the other); 0 < s (`comiss;
// jbe`) -> 0x41E2E0(boat, 2 s (0x6F76F8), 0). When the driver's (boat +0x220)
// vt+0x34 answers true and the other's +0x2D0 & 0x40: j = 0x534DA0 and
// applyImpulse 0x4221C0(n j, point - the body's origin) on the boat's body.
// Then boat +0x468 = 1 when the other's +0x2CA == 128 or (!(+0x2D0 & 1) and
// 0x534DA0 / boat +0x134 > boat +0x140 x 0.44704 (0x6F7654), `comiss; jbe`);
// boat +0x389 = 1 always.
inline void boat_contact_handler( std::uint8_t* boat, std::uint8_t* body, const std::uint8_t* other, const float* point, const float* normal,
                                  bool driver_34, bool human, std::uint8_t* driver )
{
    std::uint32_t of;
    std::memcpy( &of, other + 0xD8, 4 );
    if ( of & 4 )
        return;
    const float s = body_contact_strength( body, other, point, normal );
    if ( s > 0.0f )
        boat_driver_raise( driver, human, sse_mul( s, 2.0f ), 0.0f );
    if ( driver_34 && ( other[ 0x2D0 ] & 0x40 ) )
    {
        const float j = body_contact_impulse( body, other, point, normal );
        const float impulse[ 4 ] = { sse_mul( normal[ 0 ], j ), sse_mul( normal[ 1 ], j ), sse_mul( normal[ 2 ], j ), 0.0f };
        const float rel[ 4 ] = { sse_sub( point[ 0 ], state_f( body, 0x40 ) ), sse_sub( point[ 1 ], state_f( body, 0x44 ) ),
                                 sse_sub( point[ 2 ], state_f( body, 0x48 ) ), 0.0f };
        body_apply_impulse( body, impulse, rel );
    }
    std::int16_t group;
    std::memcpy( &group, other + 0x2CA, 2 );
    bool hit = group == 128;
    if ( !hit && !( other[ 0x2D0 ] & 1 ) )
    {
        const float j = body_contact_impulse( body, other, point, normal );
        hit = j / state_f( boat, 0x134 ) > sse_mul( state_f( boat, 0x140 ), support_bits( 0x3EE4E26Du ) );
    }
    if ( hit )
        boat[ 0x468 ] = 1;
    boat[ 0x389 ] = 1;
}