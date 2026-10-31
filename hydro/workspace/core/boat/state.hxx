#pragma once

#include <workspace/core/boat/hull.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>

// Small per-frame state helpers of the boat's Decision tick (VuBoatEntity
// vtable slot 13 0x41EFF0), on the boat image (construct.hxx's offsets) and
// its rigid body (boat +0x21C; the body's broadphase proxy at body +0xC8).

inline float state_f( const std::uint8_t* b, int at )
{
    float v;
    std::memcpy( &v, b + at, 4 );
    return v;
}

inline void state_put_f( std::uint8_t* b, int at, float v )
{
    std::memcpy( b + at, &v, 4 );
}

// x87 `fld dword; fistp dword` under the default control word (round to
// nearest even): an out-of-range value or a NaN gives the integer
// indefinite 0x80000000. (std::nearbyint in our default rounding mode - the
// same rounding; the conversion is exact either way.)
inline std::int32_t state_fistp( float x )
{
    const double r = std::nearbyint( static_cast< double >( x ) );
    if ( !( r >= -2147483648.0 && r <= 2147483647.0 ) )
        return static_cast< std::int32_t >( 0x80000000u );
    return static_cast< std::int32_t >( r );
}

// 0x41FF70 (__usercall: ecx the boat, xmm1 dt): while +0x464 is set, the
// flash bit +0x465 = round(t * 10) is odd (0x6F778C; the remainder idiom's
// own flags feed setne), then t (+0x460) -= dt; once t < 0 the body's
// +0x2CC and its proxy's +6 (when there is a proxy) become 0xFFFF, +0x464 =
// 0 and t = 0.
inline void boat_flash_timer( std::uint8_t* boat, std::uint8_t* body, std::uint8_t* proxy, float dt )
{
    if ( boat[ 0x464 ] == 0 )
        return;
    const float t = state_f( boat, 0x460 );
    const std::int32_t n = state_fistp( t * 10.0f );
    boat[ 0x465 ] = ( n % 2 ) != 0 ? 1 : 0;
    const float left = t - dt;
    state_put_f( boat, 0x460, left );
    if ( 0.0f > left )
    {
        const std::uint16_t all = 0xFFFF;
        std::memcpy( body + 0x2CC, &all, 2 );
        if ( proxy )
            std::memcpy( proxy + 6, &all, 2 );
        boat[ 0x464 ] = 0;
        state_put_f( boat, 0x460, 0.0f );
    }
}

// 0x420010 (thiscall, the boat): the beached test. With +0x388 set: the
// air time +0x338 == 0 clears +0x388 / +0x389 (a u16 store), anything else
// (a NaN too: ucomiss / lahf / test ah, 44h / jp) clears +0x389. Without it:
// an air time > 0 that (unless +0x389 is set) also exceeds
// BeachedTimeInAirThreshold +0x390, and a body speed |v| (+0x140, VuMath_sqrt
// of (x^2 + y^2) + z^2) below BeachedSpeedThreshold +0x38C x 0.44704
// (0x6F7654, mph -> m/s) sets +0x388 = 1, +0x389 = 0; every other way out
// clears +0x389.
inline void boat_beached_test( std::uint8_t* boat, const std::uint8_t* body )
{
    const float air = state_f( boat, 0x338 );
    if ( boat[ 0x388 ] != 0 )
    {
        if ( air == 0.0f )
        {
            boat[ 0x388 ] = 0;
            boat[ 0x389 ] = 0;
            return;
        }
        boat[ 0x389 ] = 0;
        return;
    }
    if ( air > 0.0f && ( boat[ 0x389 ] != 0 || air > state_f( boat, 0x390 ) ) )
    {
        const float x = state_f( body, 0x140 ), y = state_f( body, 0x144 ), z = state_f( body, 0x148 );
        const float speed = hull_sqrt( ( x * x + y * y ) + z * z );
        const std::uint32_t mph_bits = 0x3EE4E26Du;   // 0x6F7654
        float mph;
        std::memcpy( &mph, &mph_bits, 4 );
        const float limit = state_f( boat, 0x38C ) * mph;
        if ( limit > speed )
        {
            boat[ 0x388 ] = 1;
            boat[ 0x389 ] = 0;
            return;
        }
    }
    boat[ 0x389 ] = 0;
}
// 0x41FDA0 (__usercall: ecx the boat, xmm1 dt), the jump. Jumping (+0x495):
// the timer +0x498 = maxss(0, t - dt) (0 only when 0 > t; a NaN t stays
// NaN), and at exactly 0 (ucomiss / jp) with the hull's fluid solver
// (`solver` = [hull +0x1CC]) +0x54 > 0 the jump ends. Not jumping, with
// +0x308 == 0 and a jump requested (+0x305): a central impulse
// mass (+0x134) x maxss(Vertical Jump Speed (+0x13C) x 0.44704 - up . v, 0)
// along the solver's up (+0xE0) into the body's velocity (+0x140 += inverse
// mass +0x160 x (linear factor +0x170 x impulse)), +0x495 = 1, +0x498 = 1,
// then the jump sound 0x41E100 - FMOD's event start on the "Jump" string
// (+0x1BC): an audio HOOK, reported through the return value.
inline bool boat_jump( std::uint8_t* boat, std::uint8_t* body, const std::uint8_t* solver, float dt )
{
    if ( boat[ 0x495 ] != 0 )
    {
        const float t = state_f( boat, 0x498 ) - dt;
        const float zero = 0.0f;
        const float m = zero > t ? zero : t;   // maxss xmm0 (0), xmm1 (t)
        state_put_f( boat, 0x498, m );
        if ( m == 0.0f && state_f( solver, 0x54 ) > 0.0f )
            boat[ 0x495 ] = 0;
        return false;
    }
    std::uint32_t pending;
    std::memcpy( &pending, boat + 0x308, 4 );
    if ( pending != 0 || boat[ 0x305 ] == 0 )
        return false;
    const std::uint32_t mph_bits = 0x3EE4E26Du;   // 0x6F7654
    float mph;
    std::memcpy( &mph, &mph_bits, 4 );
    const float ux = state_f( solver, 0xE0 ), uy = state_f( solver, 0xE4 ), uz = state_f( solver, 0xE8 );
    const float vx = state_f( body, 0x140 ), vy = state_f( body, 0x144 ), vz = state_f( body, 0x148 );
    const float want = state_f( boat, 0x13C ) * mph;
    const float d = want - ( ( ux * vx + uy * vy ) + uz * vz );
    const float zero = 0.0f;
    const float lift = d > zero ? d : zero;   // maxss xmm1 (d), xmm0 (0)
    const float j = state_f( boat, 0x134 ) * lift;
    const float ix = ux * j, iz = uz * j, iy = uy * j;
    const float tx = state_f( body, 0x170 ) * ix;
    const float tz = state_f( body, 0x178 ) * iz;
    const float ty = state_f( body, 0x174 ) * iy;
    const float inverse_mass = state_f( body, 0x160 );
    state_put_f( body, 0x140, inverse_mass * tx + vx );
    state_put_f( body, 0x144, inverse_mass * ty + vy );
    state_put_f( body, 0x148, inverse_mass * tz + vz );
    boat[ 0x495 ] = 1;
    state_put_f( boat, 0x498, 1.0f );
    return true;   // HOOK: 0x41E100, the jump sound
}