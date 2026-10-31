#pragma once

#include <workspace/core/boat/hull.hxx>
#include <workspace/core/boat/sections.hxx>
#include <workspace/core/boat/tick.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <xmmintrin.h>

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
// What 0x41F740 hands its two outside callees: VuWater's surface lookup
// 0x5296C0 (on g 0x759058; the WATER side, not recreated - its answer is an
// INPUT here, UNKNOWN until the water agent recreates it) gets the probe
// point and a callback {vtable 0x738424, point x, y, direction x, y} whose
// +0x14 it fills; g 0x758FEC's vt+0x1C(boat, "Drafting") is a platform
// service call (HOOK).
struct boat_probe_io_t
{
    bool asked = false;
    float point[ 3 ] = {};
    std::uint32_t callback[ 5 ] = {};
    bool event = false;
};

// 0x41F740 (__usercall: ecx the boat, xmm1 dt). +0x30C = 0; in the water
// (the hull solver's +0x54 > 0) with a horizontal speed (vy^2 + vx^2 > 0):
// the nose point = the transform component's world (`world` = its +0x50)
// applied to (0, boat +0x284, 0), the direction v / |v|, and +0x30C = the
// lookup's answer. Then, above 0.2 (0x6F7620) and with |v| (vx^2 + vy^2)
// over boat +0x470 x (engine +0xC x engine +4) x 0.44704 x 0.5, the timer
// +0x480 += dt, the event firing when it crosses 4.0 (0x6F7754); every
// other way out zeroes the timer.
inline void boat_nose_probe( std::uint8_t* boat, const std::uint8_t* body, const std::uint8_t* solver, const std::uint8_t* world,
    const std::uint8_t* engine, float dt, float water_answer, boat_probe_io_t& io )
{
    state_put_f( boat, 0x30C, 0.0f );
    const float vx = state_f( body, 0x140 ), vy = state_f( body, 0x144 );
    if ( state_f( solver, 0x54 ) > 0.0f && vy * vy + vx * vx > 0.0f )
    {
        const float inverse = 1.0f / hull_sqrt( vy * vy + vx * vx );
        const float reach = state_f( boat, 0x284 );
        const float zero = 0.0f;
        auto t = [ & ]( int k ) { return state_f( world, 4 * k ); };
        const float dx = inverse * vx, dy = inverse * vy;
        const float px = ( ( t( 4 ) * reach + t( 0 ) * zero ) + t( 8 ) * zero ) + t( 12 );
        const float py = ( ( t( 5 ) * reach + t( 1 ) * zero ) + t( 9 ) * zero ) + t( 13 );
        const float pz = ( ( t( 6 ) * reach + t( 2 ) * zero ) + t( 10 ) * zero ) + t( 14 );
        io.asked = true;
        io.point[ 0 ] = px;
        io.point[ 1 ] = py;
        io.point[ 2 ] = pz;
        io.callback[ 0 ] = 0x00738424u;
        std::memcpy( &io.callback[ 1 ], &px, 4 );
        std::memcpy( &io.callback[ 2 ], &py, 4 );
        std::memcpy( &io.callback[ 3 ], &dx, 4 );
        std::memcpy( &io.callback[ 4 ], &dy, 4 );
        state_put_f( boat, 0x30C, water_answer );   // UNKNOWN: VuWater 0x5296C0's answer
    }
    if ( state_f( boat, 0x30C ) > 0.2f )
    {
        const float speed = hull_sqrt( vx * vx + vy * vy );
        const std::uint32_t mph_bits = 0x3EE4E26Du;
        float mph;
        std::memcpy( &mph, &mph_bits, 4 );
        const float limit = ( ( state_f( boat, 0x470 ) * ( state_f( engine, 0xC ) * state_f( engine, 4 ) ) ) * mph ) * 0.5f;
        if ( speed > limit )
        {
            const float before = state_f( boat, 0x480 );
            const float after = before + dt;
            state_put_f( boat, 0x480, after );
            if ( 4.0f > before && after >= 4.0f )
                io.event = true;   // HOOK: g 0x758FEC vt+0x1C(boat, "Drafting")
            return;
        }
    }
    state_put_f( boat, 0x480, 0.0f );
}
// 0x41F9D0 (__usercall: ecx the boat, xmm1 dt), the boost sub-tick of the
// Decision tick. Energy +0x310 is fed from the pending +0x314 by min(pending,
// dt * 8) (`minss`: a NaN rate passes); the clock +0x318 advances by dt and a
// change of fistp(clock * 4) (round to nearest) sets +0x31C to the new count,
// +0x324 = 1.0, +0x320 = 2.0; no pending -> clock 0. While +0x324 > 0 it runs
// down to max(t - dt, 0) and +0x320 to max(v - dt * 4, 1.0) (`maxss`: NaN ->
// the constant); else +0x31C = 0. Boosting (+0x32C): energy -= dt, the flag
// cleared when ordered 0 >= energy, +0x308 set, +0x46B set or +0x304 clear;
// +0x47C += dt. Not boosting: energy > 0, the animation's +0x78 == 2, +0x308
// 0, +0x46B 0 and +0x304 set start it (+0x32C = +0x32D = 1); +0x47C = 0. Then
// energy = max(0, min(16, e)) and the peak +0x328 = max(peak, energy). For a
// driver with slots 14 and 12 (the human player): the FMOD UI sounds - a
// platform HOOK (`sound`, the event name; retail: _itoa_s / strcpy_s /
// strcat_s, FMOD getEvent 0x64C348 then start 0x64C354 when it succeeds) -
// "Global/UI_and_Menu/boost_x" + the new count when it changed; then while
// boosting with one local player (g 0x759060 +4 == 1): energy below 3 ->
// +0x478 -= dt and at ordered 0 >= it, +0x478 = 0.5 (0.25 below 1.5) and the
// low-boost beep; else +0x478 = 0. Crossing from >= 3 to < 3 with +0x47C >
// 6: g 0x758FEC vt+0x1C(boat, "BoostLow") (`stats`, a platform service).
struct boat_boost_hooks_t
{
    void* user = nullptr;
    void ( *sound )( void* user, const std::string& event ) = nullptr;
    void ( *stats )( void* user, const char* name ) = nullptr;
};

inline void boat_boost( std::uint8_t* boat, const std::uint8_t* animation, bool slot12, bool slot14, std::uint32_t local_players,
    float dt, const boat_boost_hooks_t& hooks )
{
    auto gt = []( float a, float b ) { return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; };   // ordered a > b
    auto ge = []( float a, float b ) { return _mm_comige_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; };   // ordered a >= b
    auto minss = []( float a, float b ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); };
    auto maxss = []( float a, float b ) { return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); };
    const float old_energy = state_f( boat, 0x310 );
    float pending = state_f( boat, 0x314 );
    std::int32_t count = 0;
    if ( gt( pending, 0.0f ) )
    {
        const float step = minss( dt * 8.0f, pending );
        state_put_f( boat, 0x310, old_energy + step );
        state_put_f( boat, 0x314, pending - step );
        const std::int32_t a = state_fistp( state_f( boat, 0x318 ) * 4.0f );
        const float clock = state_f( boat, 0x318 ) + dt;
        state_put_f( boat, 0x318, clock );
        const std::int32_t b = state_fistp( clock * 4.0f );
        if ( a != b )
        {
            count = b;
            std::memcpy( boat + 0x31C, &b, 4 );
            state_put_f( boat, 0x324, 1.0f );
            state_put_f( boat, 0x320, 2.0f );
        }
    }
    else
        image_put32( boat, 0x318, 0 );
    if ( gt( state_f( boat, 0x324 ), 0.0f ) )
    {
        state_put_f( boat, 0x324, maxss( state_f( boat, 0x324 ) - dt, 0.0f ) );
        state_put_f( boat, 0x320, maxss( state_f( boat, 0x320 ) - dt * 4.0f, 1.0f ) );
    }
    else
        image_put32( boat, 0x31C, 0 );
    std::int32_t w308;
    std::memcpy( &w308, boat + 0x308, 4 );
    if ( boat[ 0x32C ] )
    {
        const float e = state_f( boat, 0x310 ) - dt;
        state_put_f( boat, 0x310, e );
        if ( ge( 0.0f, e ) || w308 != 0 || boat[ 0x46B ] || !boat[ 0x304 ] )
            boat[ 0x32C ] = 0;
        state_put_f( boat, 0x47C, state_f( boat, 0x47C ) + dt );
    }
    else
    {
        std::int32_t a78;
        std::memcpy( &a78, animation + 0x78, 4 );
        if ( gt( state_f( boat, 0x310 ), 0.0f ) && a78 == 2 && w308 == 0 && !boat[ 0x46B ] && boat[ 0x304 ] )
            boat[ 0x32C ] = boat[ 0x32D ] = 1;
        image_put32( boat, 0x47C, 0 );
    }
    const float energy = maxss( 0.0f, minss( 16.0f, state_f( boat, 0x310 ) ) );
    state_put_f( boat, 0x310, energy );
    state_put_f( boat, 0x328, maxss( state_f( boat, 0x328 ), energy ) );
    if ( !slot14 || !slot12 )
        return;
    if ( count && hooks.sound )
        hooks.sound( hooks.user, "Global/UI_and_Menu/boost_x" + std::to_string( count ) );
    if ( !boat[ 0x32C ] || local_players != 1 )
        return;
    const float now = state_f( boat, 0x310 );
    if ( gt( 3.0f, now ) )
    {
        const float t = state_f( boat, 0x478 ) - dt;
        state_put_f( boat, 0x478, t );
        if ( ge( 0.0f, t ) )
        {
            state_put_f( boat, 0x478, gt( 1.5f, now ) ? 0.25f : 0.5f );
            if ( hooks.sound )
                hooks.sound( hooks.user, "Global/UI_and_menu/low_boost_beep" );
        }
    }
    else
        image_put32( boat, 0x478, 0 );
    if ( ge( old_energy, 3.0f ) && gt( 3.0f, state_f( boat, 0x310 ) ) && gt( state_f( boat, 0x47C ), 6.0f ) && hooks.stats )
        hooks.stats( hooks.user, "BoostLow" );
}

// The FMOD side of the engine's audio tick - a platform HOOK, not copied:
// the FMOD event API through the imports 0x64C334 (setCallback), 0x64C340
// (release), 0x64C348 (EventSystem getEvent on g 0x759010 +8, mode 1),
// 0x64C354 (start), 0x64C358 (set3DAttributes), 0x64C350 / 0x64C34C
// (getParameter / setValue). `release` = setCallback(h, 0, 0) then
// release(h, 0); `get_event` = getEvent into *handle, and on FMOD_OK
// setCallback(h, 0x50FBB0, the slot), true on FMOD_OK; `parameter` =
// getParameter(h, name) and, when that answers FMOD_OK, setValue(value).
struct boat_engine_audio_hooks_t
{
    void* user = nullptr;
    void ( *release )( void* user, std::uint32_t handle ) = nullptr;
    bool ( *get_event )( void* user, int slot, const std::string& name, std::uint32_t* handle ) = nullptr;
    void ( *start )( void* user, std::uint32_t handle ) = nullptr;
    void ( *attributes )( void* user, std::uint32_t handle, const float* position, const float* velocity ) = nullptr;
    void ( *parameter )( void* user, std::uint32_t handle, const char* name, float value ) = nullptr;
};

// 0x50FBF0 (thiscall, an event slot of the engine at `slot`, its handle at
// slot +4; name, a second argument it never reads; retn 8): a held handle
// is released and zeroed; an empty name answers false; else getEvent into
// the handle, true on FMOD_OK.
inline bool boat_audio_event_create( std::uint8_t* engine, int slot, const std::string& name, const boat_engine_audio_hooks_t& hooks )
{
    std::uint32_t handle;
    std::memcpy( &handle, engine + slot + 4, 4 );
    if ( handle )
    {
        if ( hooks.release )
            hooks.release( hooks.user, handle );
        image_put32( engine, slot + 4, 0 );
    }
    if ( name.empty( ) )
        return false;
    std::uint32_t got = 0;
    const bool ok = hooks.get_event && hooks.get_event( hooks.user, slot, name, &got );
    image_put32( engine, slot + 4, got );
    return ok;
}

// 0x413830 (__usercall: ecx the engine - boat +0x22C, its +0 the boat; xmm1
// dt), the engine's audio tick of the Decision tick. Nothing below
// 1.1920929e-07 (0x6F7584, ordered). +0xF0 = maxss(0, minss(1, |v.xy| /
// (boat +0x470 x (+0xC x +4) x 0.44704))); +0xF4 = the mean of the +0xF8
// records' floats (0 with none; the count's unsigned double conversion);
// 0x41D570's answer r (negative: -(r x +0x20)) less mean x 0.5 (x 0.2 more
// while boosting, boat +0x32C) picks the rate: above +0xEC 0.5, else -0.5;
// +0xEC = maxss(0, minss(1, ((1 - mean) + 1) x rate x dt + +0xEC)). Then the
// FMOD events (slots +0xCC StartEvent +0x64, +0xD4 RevEvent +0x7C, +0xDC
// BoostInitiate +0x94, +0xE4 BoostLoop +0xAC; flags +0x48 / +0x49) and, for
// each of the five handles +0xC8 .. +0xE8, the 3D attributes (transform
// component +0x80, body velocity) and the parameters rpm (+0xEC), speed
// (+0xF0), load (+0xF4), height (the hull solver's +0x5C).
inline void boat_engine_audio( std::uint8_t* engine, const boat_strings_t& names, const std::uint8_t* boat, solver_rigid_t& body,
    const std::uint8_t* transform, const std::uint8_t* solver, const std::uint8_t* records, float dt, const boat_engine_audio_hooks_t& hooks )
{
    auto gt = []( float a, float b ) { return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; };   // ordered a > b
    auto ge = []( float a, float b ) { return _mm_comige_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; };   // ordered a >= b
    auto minss = []( float a, float b ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); };
    auto maxss = []( float a, float b ) { return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); };
    auto word = [ & ]( int at ) { std::uint32_t v; std::memcpy( &v, engine + at, 4 ); return v; };
    auto name = [ & ]( int at ) { const auto it = names.find( at ); return it == names.end( ) ? std::string( ) : it->second; };
    if ( gt( water_bits( 0x34000000u ), dt ) )   // 0x6F7584
        return;
    const float vx = body.linear_velocity[ 0 ], vy = body.linear_velocity[ 1 ];
    const float speed = hull_sqrt( vy * vy + vx * vx );
    const float limit = ( state_f( boat, 0x470 ) * ( state_f( engine, 0xC ) * state_f( engine, 4 ) ) ) * water_bits( 0x3EE4E26Du );
    state_put_f( engine, 0xF4, 0.0f );
    state_put_f( engine, 0xF0, maxss( 0.0f, minss( 1.0f, speed / limit ) ) );
    const std::int32_t n = static_cast< std::int32_t >( word( 0xFC ) - word( 0xF8 ) ) >> 3;
    if ( n != 0 )
    {
        float sum = 0.0f;
        for ( std::int32_t k = 0; k < n; ++k )
        {
            sum = sum + state_f( records, 8 * k + 4 );
            state_put_f( engine, 0xF4, sum );
        }
        const float count = static_cast< float >( static_cast< double >( static_cast< std::uint32_t >( n ) ) );
        state_put_f( engine, 0xF4, state_f( engine, 0xF4 ) / count );
    }
    std::uint32_t i308;
    std::memcpy( &i308, boat + 0x308, 4 );
    boat_view_t view{ state_f( boat, 0x2FC ), i308, boat[ 0x32C ], boat[ 0x32F ], { }, state_f( engine, 4 ), state_f( engine, 0xC ),
                      state_f( boat, 0x470 ), body };
    for ( int k = 0; k < 3; ++k )
        view.o50_60[ k ] = state_f( transform, 0x60 + 4 * k );
    float r = boat_41D570( view );
    if ( gt( 0.0f, r ) )
        r = -( r * state_f( engine, 0x20 ) );
    const float mean = state_f( engine, 0xF4 );
    float drop = mean * 0.5f;
    if ( boat[ 0x32C ] )
        drop = drop * 0.2f;
    const float rate = gt( r - drop, state_f( engine, 0xEC ) ) ? 0.5f : -0.5f;
    const float rpm = maxss( 0.0f, minss( 1.0f, ( ( ( 1.0f - mean ) + 1.0f ) * rate ) * dt + state_f( engine, 0xEC ) ) );
    state_put_f( engine, 0xEC, rpm );
    auto release = [ & ]( int at )
    {
        const std::uint32_t h = word( at );
        if ( h )
        {
            if ( hooks.release )
                hooks.release( hooks.user, h );
            image_put32( engine, at, 0 );
        }
    };
    auto start = [ & ]( int at ) { if ( hooks.start ) hooks.start( hooks.user, word( at ) ); };
    // 0x413A4D: the start event, released once the mean is ordered <= 0,
    // created and started below rpm 0.1 (0x6F7610) with a mean above it.
    if ( word( 0xD0 ) )
    {
        if ( ge( 0.0f, mean ) )
            release( 0xD0 );
    }
    else if ( gt( 0.1f, rpm ) && gt( mean, 0.1f ) )
    {
        if ( boat_audio_event_create( engine, 0xCC, name( 0x64 ), hooks ) )
            start( 0xD0 );
    }
    // 0x413ACB: the rev event, on +0x308 with boat +0x2FC >= 1 (ordered).
    if ( engine[ 0x48 ] == 0 )
    {
        if ( i308 != 0 && ge( state_f( boat, 0x2FC ), 1.0f ) )
        {
            if ( boat_audio_event_create( engine, 0xD4, name( 0x7C ), hooks ) )
                start( 0xD8 );
            engine[ 0x48 ] = 1;
        }
    }
    else if ( i308 == 0 || ge( 0.0f, state_f( boat, 0x2FC ) ) )
        engine[ 0x48 ] = 0;
    // 0x413B33: the boost pair while boosting (+0x32C).
    if ( engine[ 0x49 ] == 0 )
    {
        if ( boat[ 0x32C ] )
        {
            if ( boat_audio_event_create( engine, 0xDC, name( 0x94 ), hooks ) )
                start( 0xE0 );
            if ( boat_audio_event_create( engine, 0xE4, name( 0xAC ), hooks ) )
                start( 0xE8 );
            engine[ 0x49 ] = 1;
        }
    }
    else if ( boat[ 0x32C ] == 0 )
    {
        release( 0xE0 );
        release( 0xE8 );
        engine[ 0x49 ] = 0;
    }
    // 0x413C1B: the five handles' attributes and parameters.
    float position[ 3 ], velocity[ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        position[ k ] = state_f( transform, 0x80 + 4 * k );
        velocity[ k ] = body.linear_velocity[ k ];
    }
    const float height = state_f( solver, 0x5C );
    for ( int k = 0; k < 5; ++k )
    {
        const std::uint32_t h = word( 0xC8 + 8 * k );
        if ( !h )
            continue;
        if ( hooks.attributes )
            hooks.attributes( hooks.user, h, position, velocity );
        if ( !hooks.parameter )
            continue;
        hooks.parameter( hooks.user, h, "rpm", state_f( engine, 0xEC ) );
        hooks.parameter( hooks.user, h, "speed", state_f( engine, 0xF0 ) );
        hooks.parameter( hooks.user, h, "load", state_f( engine, 0xF4 ) );
        hooks.parameter( hooks.user, h, "height", height );
    }
}
