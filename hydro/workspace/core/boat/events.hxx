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
    if ( !boat_rtti_is( drv_vtable, 0x73623Cu ) )
        return;
    state_put_f( drv, 0xC8, ai_maxss( state_f( drv, 0xC8 ), a ) );
    state_put_f( drv, 0xCC, ai_maxss( state_f( drv, 0xCC ), b ) );
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