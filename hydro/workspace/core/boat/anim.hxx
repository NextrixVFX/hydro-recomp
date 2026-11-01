#pragma once

#include <workspace/core/boat/state.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

// The boat's animation tick 0x40FFF0 (the Decision tick's animation_tick hook)
// and the hull's wake-emitter parameters 0x417F80.

// The views 0x40FFF0 reads, each where retail reads it: anim = the object
// (boat +0x230; +0x5C..+0x70 rates, +0x78 the boost state, +0xC4 / +0xCC /
// +0xD4 three event slots, their FMOD events at +0xC8 / +0xD0 / +0xD8);
// control[ k ] = anim +0x08 / +0x0C / +0x10 (or nullptr; +0x18 time, +0x1C
// target) and asset[ k ] = that control's +0x10 (+0x18 its duration); names =
// the std::strings at anim +0x7C / +0x94 / +0xAC; event[ k ] = the FMOD event
// at +0xC8 + 8k is set; position = the transform's +0x80, velocity = the
// body's +0x140; engine = boat +0x22C (+0xEC / +0xF0 / +0xF4).
struct boat_anim_t
{
    std::uint8_t* anim = nullptr;
    const std::uint8_t* boat = nullptr;
    std::uint8_t* control[ 3 ] = { };
    const std::uint8_t* asset[ 3 ] = { };
    const char* names[ 3 ] = { "", "", "" };
    bool event[ 3 ] = { };
    const float* position = nullptr;
    const float* velocity = nullptr;
    const std::uint8_t* engine = nullptr;
};

// What 0x40FFF0 calls that is not recreated: the driver's (boat +0x220) vt+0x64
// / vt+0x68 (x87 results, stored as floats), 0x516300(anim +0x0C's control,
// 0.0) (the animation system), 0x50FBF0 on event slot k with a name (the audio
// event's create - true starts it: FMOD::Event::start, a platform hook), the
// voice-over manager's vt+0x1C(boat, "BoostReady"), and per set FMOD event
// set3DAttributes(position, velocity, 0) and the three parameters (the name
// at 0x66108C, "speed", "load") from the engine's +0xEC / +0xF0 / +0xF4.
struct boat_anim_hooks_t
{
    void* user = nullptr;
    float ( *driver_64 )( void* user ) = nullptr;
    float ( *driver_68 )( void* user ) = nullptr;
    void ( *control_516300 )( void* user ) = nullptr;
    bool ( *event_create )( void* user, int slot, const char* name ) = nullptr;
    void ( *event_start )( void* user, int slot ) = nullptr;
    void ( *voice )( void* user, const char* line ) = nullptr;
    void ( *event_update )( void* user, int slot, const float* position, const float* velocity, const float* values ) = nullptr;
};

inline bool anim_eq( float a, float b )   // ucomiss; lahf; test ah, 44h; jp - ordered equal
{
    return _mm_ucomieq_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0 && a == b;
}

// 0x40FFF0 (__usercall: ecx the object, xmm1 dt). Control +0x08: s = the
// driver's vt+0x64; s > 0 (ordered) picks +0x64, else +0x5C and s = -s; target
// = (1 - s) x +0x60 + pick x s; its time += (+0x68, negated unless the target
// is ordered > the time) x dt. Control +0x0C: its target = ((vt+0x68 + 1) x 0.5)
// x its asset's duration, then 0x516300. Control +0x10 (absent: state 2), with
// boost = boat +0x310 > 0 (ordered) and not +0x32F, by the state +0x78: 0 -
// boosting: time = duration / +0x6C, slot 0's event, state 1; 1 - its target
// equals the duration: "BoostReady", state 2; 2 - not boosting: time =
// -(duration / +0x70), slots 1 and 2, state 3; 3 - its target equals 0: state 0.
// Then the three events' 3D attributes and parameters.
inline void boat_anim_tick( boat_anim_t& a, float dt, const boat_anim_hooks_t& hooks )
{
    std::uint8_t* o = a.anim;
    if ( a.control[ 0 ] )
    {
        float s = hooks.driver_64 ? hooks.driver_64( hooks.user ) : 0.0f;
        std::uint8_t* c = a.control[ 0 ];
        const float t = state_f( c, 0x18 );
        float pick;
        if ( _mm_comigt_ss( _mm_set_ss( s ), _mm_setzero_ps( ) ) )
            pick = state_f( o, 0x64 );
        else
        {
            pick = state_f( o, 0x5C );
            std::uint32_t u;
            std::memcpy( &u, &s, 4 );
            u ^= 0x80000000u;
            std::memcpy( &s, &u, 4 );
        }
        const float target = ( 1.0f - s ) * state_f( o, 0x60 ) + pick * s;
        float rate = state_f( o, 0x68 );
        if ( !_mm_comigt_ss( _mm_set_ss( target ), _mm_set_ss( t ) ) )
        {
            std::uint32_t u;
            std::memcpy( &u, &rate, 4 );
            u ^= 0x80000000u;
            std::memcpy( &rate, &u, 4 );
        }
        state_put_f( c, 0x18, rate * dt + t );
    }
    if ( a.control[ 1 ] )
    {
        const float v = hooks.driver_68 ? hooks.driver_68( hooks.user ) : 0.0f;
        state_put_f( a.control[ 1 ], 0x1C, ( ( v + 1.0f ) * 0.5f ) * state_f( a.asset[ 1 ], 0x18 ) );
        if ( hooks.control_516300 )
            hooks.control_516300( hooks.user );
    }
    auto start = [ & ]( int slot ) {
        const bool ok = hooks.event_create && hooks.event_create( hooks.user, slot, a.names[ slot ] );
        if ( ok && hooks.event_start )
            hooks.event_start( hooks.user, slot );
    };
    std::uint8_t* c = a.control[ 2 ];
    if ( !c )
        image_put32( o, 0x78, 2 );
    else
    {
        const bool boost = _mm_comigt_ss( _mm_set_ss( state_f( a.boat, 0x310 ) ), _mm_setzero_ps( ) ) && a.boat[ 0x32F ] == 0;
        std::uint32_t st;
        std::memcpy( &st, o + 0x78, 4 );
        const float duration = state_f( a.asset[ 2 ], 0x18 );
        switch ( st )
        {
        case 0:
            if ( boost )
            {
                state_put_f( c, 0x18, duration / state_f( o, 0x6C ) );
                start( 0 );
                image_put32( o, 0x78, 1 );
            }
            break;
        case 1:
            if ( anim_eq( state_f( c, 0x1C ), duration ) )
            {
                if ( hooks.voice )
                    hooks.voice( hooks.user, "BoostReady" );
                image_put32( o, 0x78, 2 );
            }
            break;
        case 2:
            if ( !boost )
            {
                float q = duration / state_f( o, 0x70 );
                std::uint32_t u;
                std::memcpy( &u, &q, 4 );
                u ^= 0x80000000u;
                std::memcpy( &q, &u, 4 );
                state_put_f( c, 0x18, q );
                start( 1 );
                start( 2 );
                image_put32( o, 0x78, 3 );
            }
            break;
        case 3:
            if ( anim_eq( state_f( c, 0x1C ), 0.0f ) )
                image_put32( o, 0x78, 0 );
            break;
        default:
            break;
        }
    }
    float vel[ 4 ];
    std::memcpy( vel, a.velocity, 16 );
    const float values[ 3 ] = { state_f( a.engine, 0xEC ), state_f( a.engine, 0xF0 ), state_f( a.engine, 0xF4 ) };
    for ( int k = 0; k < 3; ++k )
        if ( a.event[ k ] && hooks.event_update )
            hooks.event_update( hooks.user, k, a.position, vel, values );
}

// 0x417F80 (thiscall: the hull - +0x00 the boat, +0x98 a scale, +0x9C a local
// point, +0x1CC the solver; out* on the stack; retn 4): P = the point placed by
// the boat's world (*(boat +0x50) +0x50), r = P - the world's origin (each lane
// re-subtracted from P), v = the body's (boat +0x21C) +0x140 + w x r in xy; the
// xy speed L, the direction normalised when L > 0 (ordered); k = maxss(0,
// minss(1, 1 - solver +0x5C x 0.5)) x maxss(0, minss(1, (L - 1) x 0.11111111)).
// Out: +0x00 P (+0x0C left), +0x10 the direction, +0x18 2k, +0x1C k, +0x20 k x
// 0.5 x +0x98, +0x24 12k, +0x28 10k + 0.1, +0x2C 8.
inline void boat_wake_params( const std::uint8_t* hull, const float* m, const std::uint8_t* body, const std::uint8_t* solver, std::uint8_t* out )
{
    const float ox = state_f( hull, 0x9C ), oy = state_f( hull, 0xA0 ), oz = state_f( hull, 0xA4 );
    const float v6 = ( ox * m[ 1 ] + oy * m[ 5 ] ) + oz * m[ 9 ];
    const float px = ( ( ox * m[ 0 ] + oy * m[ 4 ] ) + oz * m[ 8 ] ) + m[ 12 ];
    const float v8 = ( ox * m[ 2 ] + oy * m[ 6 ] ) + oz * m[ 10 ];
    const float py = v6 + m[ 13 ];
    const float pz = v8 + m[ 14 ];
    const float rz = pz - m[ 14 ];
    const float wx = state_f( body, 0x150 ), wy = state_f( body, 0x154 ), wz = state_f( body, 0x158 );
    const float vx = state_f( body, 0x140 ) + ( wy * rz - wz * ( py - m[ 13 ] ) );
    const float vy = state_f( body, 0x144 ) + ( wz * ( px - m[ 12 ] ) - rz * wx );
    const float len = std::sqrt( vy * vy + vx * vx );
    float nx = vx, ny = vy;
    if ( _mm_comigt_ss( _mm_set_ss( len ), _mm_setzero_ps( ) ) )
    {
        nx = vx * ( 1.0f / len );
        ny = vy * ( 1.0f / len );
    }
    auto clamp01 = []( float x ) {
        return _mm_cvtss_f32( _mm_max_ss( _mm_setzero_ps( ), _mm_min_ss( _mm_set_ss( 1.0f ), _mm_set_ss( x ) ) ) );
    };
    const float c1 = clamp01( ( len - 1.0f ) * 0.11111111f );
    const float c2 = clamp01( 1.0f - state_f( solver, 0x5C ) * 0.5f );
    const float k = c2 * c1;
    state_put_f( out, 0x10, nx );
    state_put_f( out, 0x14, ny );
    state_put_f( out, 0x00, px );
    state_put_f( out, 0x04, py );
    state_put_f( out, 0x08, pz );
    state_put_f( out, 0x1C, k );
    image_put32( out, 0x2C, 0x41000000u );
    state_put_f( out, 0x20, ( k * 0.5f ) * state_f( hull, 0x98 ) );
    state_put_f( out, 0x18, k * 2.0f );
    state_put_f( out, 0x24, k * 12.0f );
    state_put_f( out, 0x28, k * 10.0f + 0.1f );
}
