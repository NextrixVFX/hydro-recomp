#pragma once

#include <workspace/core/boat/state.hxx>

#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

// VuAiDriver's tick (driver vtable slot 18, vt+0x48 = 0x40B940) and its
// closure, on the driver's image (0xD4 bytes; ctor 0x40B690, load 0x40B8A0)
// and the boat's (driver +0x5C). Driver fields read here: +0x5C the boat,
// +0x80 an int (>= 2 enables the lead test), +0x8C / +0x90 the path points
// (pointer array, count), +0x98 the avoidance margin, +0x9C the top speed,
// +0xA0 the signed speed, +0xA8 the target (16 b), +0xB8 the wanted speed,
// +0xBC another boat (or 0), +0xC0 / +0xC4 and +0xC8 / +0xCC two springs.

inline float ai_minss( float a, float b ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); }
inline float ai_maxss( float a, float b ) { return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); }

inline std::uint8_t* ai_ptr( const std::uint8_t* b, int at )
{
    std::uint32_t v;
    std::memcpy( &v, b + at, 4 );
    return reinterpret_cast< std::uint8_t* >( static_cast< std::uintptr_t >( v ) );
}

// What 0x40D5A0 calls that is not recreated: the BoostMisfire sound 0x41E060
// (FMOD's event start on the boat's +0x1A4 string - an audio hook) and the
// voice-over manager g 0x758FEC vt+0x1C (boat, "NeedPickUpPowerBoost").
struct ai_controls_hooks_t
{
    void* user = nullptr;
    void ( *misfire )( void* user ) = nullptr;
    void ( *voice )( void* user, const char* line ) = nullptr;
};

// One critically damped step (0x40D5A0's two halves, written twice in
// retail): k = dt x 10 (0x6F778C), n = 1 / ((k x 0.48 x k + (k + 1)) +
// k x 0.235 x k x k) (0x6F7658 / 0x6F7624), e = x - target, t = (e x 10 +
// v) x dt; v = (v - t x 10) x n, x = (t + e) x n + target.
inline void ai_spring( std::uint8_t* driver, int at, float dt, float target )
{
    const float k = dt * 10.0f;
    const float n = 1.0f / ( ( ( k * 0.47999999f ) * k + ( k + 1.0f ) ) + ( ( k * 0.235f ) * k ) * k );
    const float e = state_f( driver, at ) - target;
    float v = state_f( driver, at + 4 );
    const float t = ( e * 10.0f + v ) * dt;
    v = ( v - t * 10.0f ) * n;
    state_put_f( driver, at + 4, v );
    state_put_f( driver, at, ( t + e ) * n + target );
}

// 0x40D5A0 (__usercall: ecx the driver, xmm1 dt, xmm2 steer, xmm3
// throttle, a byte on the stack - boost; retn 4): the springs +0xC0 (to
// steer) and +0xC8 (to throttle); the boat's +0x300 = maxss(-1, minss(1,
// +0xC0)), +0x2FC the same of +0xC8. Boosting with +0x304 clear and +0x308
// == 0: unless the animation (boat +0x230) +0x78 == 2 the BoostMisfire
// sound, and when the boost peak +0x328 is ordered <= 0 and +0x4B4 > 1 the
// voice line. Then +0x304 = boost, +0x306 = +0x46B.
inline void ai_controls( std::uint8_t* driver, std::uint8_t* boat, const std::uint8_t* animation, float dt, float steer, float throttle, std::uint8_t boost, const ai_controls_hooks_t& hooks )
{
    ai_spring( driver, 0xC0, dt, steer );
    ai_spring( driver, 0xC8, dt, throttle );
    state_put_f( boat, 0x300, ai_maxss( -1.0f, ai_minss( 1.0f, state_f( driver, 0xC0 ) ) ) );
    state_put_f( boat, 0x2FC, ai_maxss( -1.0f, ai_minss( 1.0f, state_f( driver, 0xC8 ) ) ) );
    std::int32_t w308;
    std::memcpy( &w308, boat + 0x308, 4 );
    if ( boost && boat[ 0x304 ] == 0 && w308 == 0 )
    {
        std::int32_t anim78;
        std::memcpy( &anim78, animation + 0x78, 4 );
        if ( anim78 != 2 && hooks.misfire )
            hooks.misfire( hooks.user );
        if ( _mm_comige_ss( _mm_set_ss( 0.0f ), _mm_set_ss( state_f( boat, 0x328 ) ) ) && state_f( boat, 0x4B4 ) > 1.0f && hooks.voice )
            hooks.voice( hooks.user, "NeedPickUpPowerBoost" );
    }
    boat[ 0x304 ] = boost;
    boat[ 0x306 ] = boat[ 0x46B ];
}
