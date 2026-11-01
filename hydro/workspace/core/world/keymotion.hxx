#pragma once

// VuKeyframeMotionEntity (vtable 0x752C44, 0xE0 bytes: create 0x5CE2D0 = pre-zero 0x5CE390 + ctor 0x5CE400 over the
// VuMotionEntity ctor 0x61A000) - moves its "Entity" ref's motion component through the transforms of its
// VuKeyframeEntity children (getType descriptor 0x736BA0). Hydroelectric authors 12 (the trains, the dam gate, the ramps,
// the trigger pulses, the helicopter's flight - its ref is the whole VuAnimatedPropEntity "Helicopter").
// Verified against retail: hydro checks/logic.hxx verify_key_motion (case key_motion).
//
// VuMotionEntity (vtable 0x757F6C; slots 12 / 13 / 14 pure): +0x5C VuScriptComponent (order 150), +0x60 the VuScriptRef
// "Entity" (its +0x18 the link, link +8 the entity), +0x64 "Initially Active" (0), +0x65 "One Shot" (0), +0x68 the target's
// VuMotionComponent (VuEntity_findMotionComponent 0x5F5810 at slot 6), +0x6C active, +0x6D the one-shot latch; inputs
// "Activate" 0x61A4F0 / "Deactivate" 0x61A5D0, outputs "OnActivated" / "OnDeactivated". Slot 6 0x61A420 registers the
// "Motion" tick {0x757F58, this, 0x61A6E0} (g_vutickmanager vt+0x20), slot 7 0x61A4B0, slot 8 0x61A4E0 (+0x6D = 0).
// VuKeyframeMotionEntity: +0x70 "Curve Type" (VuStaticIntEnumProperty, table 0x752C18; 0), +0x74 "Loop" (0), +0x75
// "Ease In/Out" (1), +0x76 "Affect Position" (1), +0x77 "Affect Rotation" (1) (the word 0x01010100); +0x78 a
// Vu3dLayoutComponent (bounds +-1e9, its draw 0x5CF9B0: DEV); +0x7C / +0x80 / +0x84 a VuArray of 36-byte keys (capacity 8):
// {time, position x y z w (the child transform +0x80 .. +0x8C), quaternion x y z w (0x531000 of the child's +0x50)};
// +0x88 the duration, +0x8C the clock; +0x90 .. +0xDC the spline curves of types 1 / 2 / 3; outputs "OnLoop", "OnDone".
// Slots 6 0x5CE9E0, 7 0x5CEA40, 12 0x5CFE50 (clock = 0), 13 nullsub, 14 0x5CFE60 (the update).
//
// Curve Type (table 0x752C18: 0 "Linear", 1 "Cubic", 2 "Cubic Euler", 3 "Spline"): all four recreated - the position curve
// 0x5CEC60 -> 0x61A7A0 -> 0x61AE50, type 2's euler curve 0x5CEF20, type 3's splines 0x5CF800 -> 0x61B120 / 0x61B9E0, and
// 0x5D0440's branches (0x61B600, 0x61C550, 0x5316C0) - verify_key_curve, case key_curve. Every shipped project's
// VuKeyframeMotionEntity authors Linear or Cubic (census: 33 Cubic in 9 levels, none in Hydroelectric; no 2 or 3 anywhere).
// UNKNOWN (counted): a type 3 entity with 2 or 3 keys (its splines stay empty; retail evaluates through a null array).
// UNKNOWN (counted): the update's key search when the clock is below every key (a stack word retail never wrote when not
// looping) - and the velocities' w lanes (stack words; the hook gets 0).
// SUBSTITUTION: the qsort import (keyframe.hxx control_qsort); __libm_sse2_atan2 / asinf in the angular velocity
// (transform.hxx transform_euler's CRT double narrowed).

#include <workspace/core/boat/hull.hxx>
#include <workspace/core/boat/math.hxx>
#include <workspace/core/world/frontcam.hxx>
#include <workspace/core/world/keyframe.hxx>
#include <workspace/core/world/transform.hxx>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// The ref's VuMotionComponent as the entity sees it (the target side is the world's).
struct key_motion_target_t
{
    const void* claim = nullptr;            // +0x14: the motion entity driving it (one at a time)
    const float* transform = nullptr;        // its owner's transform component +0x50 (16 words: +0x80 the position); null = none
    void* user = nullptr;
    // +0x10's interface: slot 1 at activation, slot 2 at deactivation; slot 0 after 0x5D0AB0 wrote +0x18 (the matrix,
    // 16), +0x58 (linear velocity, 4) and +0x68 (angular velocity, 4).
    void ( *begin )( void* user ) = nullptr;
    void ( *end )( void* user ) = nullptr;
    void ( *update )( void* user, const float* matrix, const float* linear, const float* angular ) = nullptr;
};

struct key_motion_t
{
    std::uint8_t initially_active = 0, one_shot = 0;         // +0x64 / +0x65
    key_motion_target_t* target = nullptr;                   // +0x68
    std::uint8_t active = 0, latch = 0;                      // +0x6C / +0x6D
    std::int32_t curve = 0;                                  // +0x70
    std::uint8_t loop = 0, ease = 1, affect_position = 1, affect_rotation = 1;   // +0x74 .. +0x77
    std::vector< std::array< std::uint32_t, 9 > > keys;     // +0x7C (+0x80 the count)
    std::int32_t capacity = 8;                               // +0x84
    std::uint32_t duration = 0, clock = 0;                   // +0x88 / +0x8C
    // +0x90 the Curve Type 1 / 2 position curve (0x5CEC60): a VuArray of 36-byte records {x y z w (the key's position), the
    // second derivatives x y z (0x61A7A0), +0x1C never written (aligned_malloc's bytes: UNKNOWN, held 0 here), the time};
    // +0x94 the count, +0x98 the capacity (the ctor: 0), +0x9C set by 0x61A7A0.
    std::vector< std::array< std::uint32_t, 9 > > cubic;
    std::int32_t cubic_capacity = 0;
    std::uint8_t cubic_valid = 0;
    // +0xA0 the Curve Type 2 rotation curve (0x5CEF20): the same records over the keys' unwrapped eulers {x y z, +0xC a stack
    // word retail never wrote (UNKNOWN, held 0), the second derivatives, +0x1C UNKNOWN, the time}; +0xA4 count, +0xA8
    // capacity, +0xAC valid.
    std::vector< std::array< std::uint32_t, 9 > > euler;
    std::int32_t euler_capacity = 0;
    std::uint8_t euler_valid = 0;
    // Curve Type 3 ("Spline", 0x5CF800): +0xB0 the position spline - 76-byte segments (+0xB4 count, +0xB8 capacity) {t0, t1,
    // 1 / (t1 - t0), P (4), the cubic's c1 / c2 / c3 (3 each, each followed by a stack word retail never wrote: words 10 / 14
    // / 18 UNKNOWN, held 0)}, +0xBC the cumulative arc lengths (count + 1 floats; +0xC4 capacity), +0xC8 the total, +0xCC
    // valid; +0xD0 the rotation spline - 76-byte segments {t0, t1, 1 / (t1 - t0), q_i, a_i, b_i, q_i+1} (+0xD8 capacity),
    // +0xDC valid.
    std::vector< std::array< std::uint32_t, 19 > > spline;
    std::int32_t spline_capacity = 0;
    std::vector< std::uint32_t > lengths;
    std::int32_t lengths_capacity = 0;
    std::uint32_t total = 0;
    std::uint8_t spline_valid = 0;
    std::vector< std::array< std::uint32_t, 19 > > squad;
    std::int32_t squad_capacity = 0;
    std::uint8_t squad_valid = 0;
};

// One VuKeyframeEntity child as slot 6 reads it: its +0x60 Time and its transform's +0x50 matrix (+0x80 .. +0x8C).
struct key_motion_child_t
{
    bool keyframe = false;   // its getType chain holds 0x736BA0
    std::uint32_t time = 0;
    float transform[ 16 ] = { };
};

struct key_motion_hooks_t
{
    void* user = nullptr;
    void ( *fire )( void* user, key_motion_t& e, const char* plug ) = nullptr;   // an output, empty VuParams
    void ( *tick_register )( void* user, key_motion_t& e, bool on ) = nullptr;   // vt+0x20 "Motion" / vt+0x28
    // VuEntity_findMotionComponent 0x5F5810 on the linked ref's entity (null: it has none).
    key_motion_target_t* ( *find )( void* user, key_motion_t& e ) = nullptr;
    int* unrecreated = nullptr;
};

inline void key_motion_unrecreated( const key_motion_hooks_t& h )
{
    if ( h.unrecreated )
        ++*h.unrecreated;
}

// "Activate" 0x61A4F0 (VuRetVal void): with a target, no latch and the target unclaimed: claim it, its IF slot 1, active,
// One Shot -> latch, slot 12 (0x5CFE50: clock = 0), "OnActivated".
inline void key_motion_activate( key_motion_t& e, const key_motion_hooks_t& h )
{
    key_motion_target_t* t = e.target;
    if ( !t || e.latch || t->claim )
        return;
    t->claim = &e;
    if ( t->begin )
        t->begin( t->user );
    e.active = 1;
    if ( e.one_shot )
        e.latch = 1;
    e.clock = 0;
    if ( h.fire )
        h.fire( h.user, e, "OnActivated" );
}

// "Deactivate" 0x61A5D0: when active: inactive, slot 13 (nullsub), "OnDeactivated", the target's claim dropped, its IF
// slot 2. (Retail does not test the target for null here.)
inline void key_motion_deactivate( key_motion_t& e, const key_motion_hooks_t& h )
{
    if ( !e.active )
        return;
    e.active = 0;
    if ( h.fire )
        h.fire( h.user, e, "OnDeactivated" );
    key_motion_target_t* t = e.target;
    if ( !t )
    {
        key_motion_unrecreated( h );
        return;
    }
    t->claim = nullptr;
    if ( t->end )
        t->end( t->user );
}

// 0x5D0AB0 (thiscall; matrix, linear, angular - each written into the target's motion component): with the target's
// transform: Affect Position off -> the matrix's translation = the transform's position (+0x80 .. +0x88) and the linear
// velocity 0; Affect Rotation off -> the matrix = the transform's +0x50 (16 words) with the translation x y z kept and
// the angular velocity 0. Then the IF slot 0.
inline void key_motion_apply( key_motion_t& e, float* m, float* linear, float* angular )
{
    key_motion_target_t* t = e.target;
    if ( t->transform )
    {
        if ( !e.affect_position )
        {
            m[ 12 ] = t->transform[ 12 ];
            m[ 13 ] = t->transform[ 13 ];
            m[ 14 ] = t->transform[ 14 ];
            linear[ 0 ] = linear[ 1 ] = linear[ 2 ] = 0.0f;
        }
        if ( !e.affect_rotation )
        {
            const float x = m[ 12 ], y = m[ 13 ], z = m[ 14 ];
            std::memcpy( m, t->transform, 64 );
            m[ 12 ] = x;
            m[ 13 ] = y;
            m[ 14 ] = z;
            angular[ 0 ] = angular[ 1 ] = angular[ 2 ] = 0.0f;
        }
    }
    if ( t->update )
        t->update( t->user, m, linear, angular );
}

// 0x5D0120 (key i): the matrix of key i's quaternion (0x531180) with its position as the translation, zero velocities.
inline void key_motion_set_key( key_motion_t& e, int i )
{
    const auto& k = e.keys[ static_cast< std::size_t >( i ) ];
    float q[ 4 ], m[ 16 ];
    for ( int j = 0; j < 4; ++j )
        q[ j ] = control_f( k[ 5 + j ] );
    front_rig_matrix( q, m );
    m[ 12 ] = control_f( k[ 1 ] );
    m[ 13 ] = control_f( k[ 2 ] );
    m[ 14 ] = control_f( k[ 3 ] );
    float lin[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f }, ang[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
    key_motion_apply( e, m, lin, ang );
}

// 0x61AE50 (stdcall: x, y, n, yp1, ypn, y2 out, u scratch) - Numerical Recipes' `spline` in float: an end slope whose double
// is above 9.9e29 (0x6F7838; comisd / jbe: a NaN takes the clamped side) gives the natural end; the inner ratios and the
// back substitution exactly as retail orders them (1 / p once, multiplied; y2[k] = y2[k + 1] y2[k] + u[k]).
inline void key_motion_spline( const float* x, const float* y, int n, float yp1, float ypn, float* y2, float* u )
{
    const float one = 1.0f, three = 3.0f;
    if ( static_cast< double >( yp1 ) > 9.9e29 )
    {
        y2[ 0 ] = 0.0f;
        u[ 0 ] = 0.0f;
    }
    else
    {
        y2[ 0 ] = -0.5f;
        const float inv = one / ( x[ 1 ] - x[ 0 ] );
        u[ 0 ] = ( ( y[ 1 ] - y[ 0 ] ) * inv - yp1 ) * ( inv * three );
    }
    for ( int i = 1; i <= n - 2; ++i )
    {
        const float sig = ( x[ i ] - x[ i - 1 ] ) / ( x[ i + 1 ] - x[ i - 1 ] );
        const float p = one / ( y2[ i - 1 ] * sig + 2.0f );
        y2[ i ] = ( sig - one ) * p;
        const float d = ( y[ i + 1 ] - y[ i ] ) / ( x[ i + 1 ] - x[ i ] ) - ( y[ i ] - y[ i - 1 ] ) / ( x[ i ] - x[ i - 1 ] );
        u[ i ] = ( ( d * 6.0f ) / ( x[ i + 1 ] - x[ i - 1 ] ) - u[ i - 1 ] * sig ) * p;
    }
    float qn = 0.0f, un = 0.0f;
    if ( !( static_cast< double >( ypn ) > 9.9e29 ) )
    {
        const float inv = one / ( x[ n - 1 ] - x[ n - 2 ] );
        qn = 0.5f;
        un = ( ypn - ( y[ n - 1 ] - y[ n - 2 ] ) * inv ) * ( inv * three );
    }
    y2[ n - 1 ] = ( un - u[ n - 2 ] * qn ) / ( y2[ n - 2 ] * qn + one );
    for ( int k = n - 2; k >= 0; --k )
        y2[ k ] = y2[ k + 1 ] * y2[ k ] + u[ k ];
}

// 0x61A7A0 (thiscall on the curve +0x90; start / end slopes, 3 floats each): fewer than 2 records -> false; per lane x / y / z
// the times (record +0x20) and the lane's values through 0x61AE50 (scratch = the static 0x759898: times, u, values, y2 -
// four runs of n floats), y2 into the record's +0x10 / +0x14 / +0x18; +0xC (the valid byte) = 1.
using key_motion_curve_t = std::vector< std::array< std::uint32_t, 9 > >;

inline bool key_motion_cubic_fit( key_motion_curve_t& curve, std::uint8_t& valid, const float* start, const float* end )
{
    const int n = static_cast< int >( curve.size( ) );
    if ( n < 2 )
        return false;
    std::vector< float > x( n ), y( n ), y2( n ), u( n );
    for ( int i = 0; i < n; ++i )
        x[ i ] = control_f( curve[ i ][ 8 ] );
    for ( int lane = 0; lane < 3; ++lane )
    {
        for ( int i = 0; i < n; ++i )
            y[ i ] = control_f( curve[ i ][ lane ] );
        key_motion_spline( x.data( ), y.data( ), n, start[ lane ], end[ lane ], y2.data( ), u.data( ) );
        for ( int i = 0; i < n; ++i )
            curve[ i ][ 4 + lane ] = control_w( y2[ i ] );
    }
    valid = 1;
    return true;
}

// 0x5CEC60 (Curve Types 1 and 2: the position curve): count 0, valid 0; with 2 keys or more: the capacity raised to n
// (a fresh block, nothing copied), one record per key (position x y z w, time); the slopes: Ease In/Out on -> both 0; off ->
// start = (P_1 - P_0) (1 / (T_1 - T_0)), end = (P_n-1 - P_n-2) (1 / (T_n-1 - T_n-2)) per lane x y z, each 0 unless the later
// time is above the earlier (comiss / jbe: NaN -> 0); then 0x61A7A0.
inline void key_motion_cubic_setup( key_motion_t& e )
{
    e.cubic.clear( );
    e.cubic_valid = 0;
    const int n = static_cast< int >( e.keys.size( ) );
    if ( n < 2 )
        return;
    if ( n > e.cubic_capacity )
        e.cubic_capacity = n;
    for ( const auto& k : e.keys )
        e.cubic.push_back( { k[ 1 ], k[ 2 ], k[ 3 ], k[ 4 ], 0u, 0u, 0u, 0u, k[ 0 ] } );
    float start[ 3 ] = { 0.0f, 0.0f, 0.0f }, end[ 3 ] = { 0.0f, 0.0f, 0.0f };
    if ( !e.ease )
    {
        auto slope = [ & ]( int a, int b, float* out )
        {
            const auto& ka = e.keys[ static_cast< std::size_t >( a ) ];
            const auto& kb = e.keys[ static_cast< std::size_t >( b ) ];
            const float ta = control_f( ka[ 0 ] ), tb = control_f( kb[ 0 ] );
            if ( !_mm_comigt_ss( _mm_set_ss( tb ), _mm_set_ss( ta ) ) )
                return;
            const float inv = 1.0f / ( tb - ta );
            for ( int j = 0; j < 3; ++j )
                out[ j ] = inv * ( control_f( kb[ 1 + j ] ) - control_f( ka[ 1 + j ] ) );
        };
        slope( 0, 1, start );
        slope( n - 2, n - 1, end );
    }
    key_motion_cubic_fit( e.cubic, e.cubic_valid, start, end );
}

// A key's euler as 0x5CEF20 takes it: the matrix of its quaternion (0x531180), then x = atan2( m6, m10 ), y = asin( maxss( -1,
// minss( 1, -m2 ) ) ), z = atan2( m1, m0 ) - transform_euler's (0x5344A0's) arithmetic. The per-key eulers use x87 fpatan for
// the atan2s and the slopes __libm_sse2_atan2: both the host double narrowed once here (SUBSTITUTION, as Unicorn computes
// fpatan and the oracle serves the import).
inline void key_motion_key_euler( const std::array< std::uint32_t, 9 >& k, float* e )
{
    float q[ 4 ], m[ 16 ];
    for ( int j = 0; j < 4; ++j )
        q[ j ] = control_f( k[ 5 + j ] );
    front_rig_matrix( q, m );
    transform_euler( m, e );
}

// 0x5CEF20 (Curve Type 2: the rotation curve +0xA0): count 0, valid 0; with 2 keys or more: each key's euler into a scratch
// VuArray (16-byte entries, 0x51C1F0; lane 3 the stack word esp+0x2C - UNKNOWN), unwrapped in key order per lane:
// d = ((e_i - e_i-1) + pi) x 1/2pi (0x6F7750 / 0x6F7618), k = fistp( d - 0.5 ) (x87: the float d less 0.5, round to nearest
// even), e_i = (((d - (float) k) x 2pi) - pi) + e_i-1 (0x6F7778); the capacity raised to n; one record per key (the unwrapped
// euler, the time); slopes with Ease In/Out off: start = (E_0 - E_1) (1 / (T_1 - T_0)), end = (E_n-2 - E_n-1) (1 / (T_n-1 -
// T_n-2)) - the RAW eulers (not unwrapped), earlier minus later, each 0 unless the later time is above (comiss / jbe); then
// 0x61A7A0 on +0xA0.
inline void key_motion_euler_setup( key_motion_t& e )
{
    e.euler.clear( );
    e.euler_valid = 0;
    const int n = static_cast< int >( e.keys.size( ) );
    if ( n < 2 )
        return;
    std::vector< std::array< float, 3 > > eu( n );
    for ( int i = 0; i < n; ++i )
        key_motion_key_euler( e.keys[ static_cast< std::size_t >( i ) ], eu[ i ].data( ) );
    const float pi = control_f( 0x40490FDBu ), inv2pi = control_f( 0x3E22F983u ), twopi = control_f( 0x40C90FDBu );
    for ( int i = 1; i < n; ++i )
        for ( int j = 0; j < 3; ++j )
        {
            const float d = ( ( eu[ i ][ j ] - eu[ i - 1 ][ j ] ) + pi ) * inv2pi;
            const double r = std::nearbyint( static_cast< double >( d ) - 0.5 );
            const std::int32_t k = r >= -2147483648.0 && r < 2147483648.0 ? static_cast< std::int32_t >( r ) : INT32_MIN;
            eu[ i ][ j ] = ( ( ( d - static_cast< float >( k ) ) * twopi ) - pi ) + eu[ i - 1 ][ j ];
        }
    if ( n > e.euler_capacity )
        e.euler_capacity = n;
    for ( int i = 0; i < n; ++i )
        e.euler.push_back( { control_w( eu[ i ][ 0 ] ), control_w( eu[ i ][ 1 ] ), control_w( eu[ i ][ 2 ] ), 0u, 0u, 0u, 0u, 0u,
                             e.keys[ static_cast< std::size_t >( i ) ][ 0 ] } );
    float start[ 3 ] = { 0.0f, 0.0f, 0.0f }, end[ 3 ] = { 0.0f, 0.0f, 0.0f };
    if ( !e.ease )
    {
        auto slope = [ & ]( int a, int b, float* out )
        {
            const auto& ka = e.keys[ static_cast< std::size_t >( a ) ];
            const auto& kb = e.keys[ static_cast< std::size_t >( b ) ];
            const float ta = control_f( ka[ 0 ] ), tb = control_f( kb[ 0 ] );
            if ( !_mm_comigt_ss( _mm_set_ss( tb ), _mm_set_ss( ta ) ) )
                return;
            float ea[ 3 ], eb[ 3 ];
            key_motion_key_euler( ka, ea );
            key_motion_key_euler( kb, eb );
            const float inv = 1.0f / ( tb - ta );
            for ( int j = 0; j < 3; ++j )
                out[ j ] = ( ea[ j ] - eb[ j ] ) * inv;
        };
        slope( 0, 1, start );
        slope( n - 2, n - 1, end );
    }
    key_motion_cubic_fit( e.euler, e.euler_valid, start, end );
}

// The x87 fsin / fcos / fsincos on a float, stored to a float (SUBSTITUTION: the host double, as Unicorn measures them).
inline float key_x87_sin( float a ) { return static_cast< float >( std::sin( static_cast< double >( a ) ) ); }
inline float key_x87_cos( float a ) { return static_cast< float >( std::cos( static_cast< double >( a ) ) ); }

// VuArray growth (0x61B970 / 0x5133B0 / 0x51C1F0): n above the capacity -> max( n, cap + cap / 2 ).
inline void key_motion_grow( std::int32_t& cap, std::int32_t n )
{
    if ( n > cap )
    {
        std::int32_t g = cap + cap / 2;
        if ( n > g )
            g = n;
        if ( g > cap )
            cap = g;
    }
}

// 0x61B120 (thiscall on +0xB0; 20-byte records {x y z w, t}, n; retn 8): n < 4 -> false; n - 3 segments (0x61B970); segment i
// over records i .. i + 3 (P0 .. P3): d = P2 - P1, k = (t2 - t1) 2, s1 = (k / (t2 - t0)) 0.5, s2 = (k / (t3 - t1)) 0.5,
// m1 = d s1 + (P1 - P0) s1, m2 = (P3 - P2) s2 + d s2 (per lane); the segment {t1, t2, 1 / (t2 - t1), P1 (4 words), c1 = m1,
// c2 = (3 d - 2 m1) - m2, c3 = (-2 d + m1) + m2}; then the lengths (count + 1, [0] = 0): per segment the 5-point Gauss-Legendre
// sum (nodes 0x757FA8, weights 0x757FBC) of |c1 + (2 c2 + 3 c3 s) s| (VuMath_sqrt( (y^2 + x^2) + z^2 )), cumulative; the total
// = the last; valid = 1.
inline bool key_motion_spline_fit( key_motion_t& e, const std::vector< std::array< float, 5 > >& r )
{
    const int n = static_cast< int >( r.size( ) );
    if ( n < 4 )
        return false;
    const int count = n - 3;
    key_motion_grow( e.spline_capacity, count );
    e.spline.assign( static_cast< std::size_t >( count ), std::array< std::uint32_t, 19 >{ } );
    for ( int i = 0; i < count; ++i )
    {
        const auto &p0 = r[ i ], &p1 = r[ i + 1 ], &p2 = r[ i + 2 ], &p3 = r[ i + 3 ];
        const float dx = p2[ 0 ] - p1[ 0 ], dy = p2[ 1 ] - p1[ 1 ], dz = p2[ 2 ] - p1[ 2 ];
        const float k = ( p2[ 4 ] - p1[ 4 ] ) * 2.0f;
        const float s1 = ( k / ( p2[ 4 ] - p0[ 4 ] ) ) * 0.5f;
        const float m1x = dx * s1 + ( p1[ 0 ] - p0[ 0 ] ) * s1;
        const float m1y = dy * s1 + ( p1[ 1 ] - p0[ 1 ] ) * s1;
        const float m1z = dz * s1 + ( p1[ 2 ] - p0[ 2 ] ) * s1;
        const float s2 = ( k / ( p3[ 4 ] - p1[ 4 ] ) ) * 0.5f;
        const float m2x = ( p3[ 0 ] - p2[ 0 ] ) * s2 + dx * s2;
        const float m2y = ( p3[ 1 ] - p2[ 1 ] ) * s2 + dy * s2;
        const float m2z = ( p3[ 2 ] - p2[ 2 ] ) * s2 + dz * s2;
        auto& g = e.spline[ static_cast< std::size_t >( i ) ];
        g[ 0 ] = control_w( p1[ 4 ] );
        g[ 1 ] = control_w( p2[ 4 ] );
        g[ 2 ] = control_w( 1.0f / ( p2[ 4 ] - p1[ 4 ] ) );
        for ( int j = 0; j < 4; ++j )
            g[ 3 + j ] = control_w( p1[ j ] );
        g[ 7 ] = control_w( m1x );
        g[ 8 ] = control_w( m1y );
        g[ 9 ] = control_w( m1z );
        g[ 11 ] = control_w( ( dx * 3.0f - m1x * 2.0f ) - m2x );
        g[ 12 ] = control_w( ( dy * 3.0f - m1y * 2.0f ) - m2y );
        g[ 13 ] = control_w( ( dz * 3.0f - m1z * 2.0f ) - m2z );
        g[ 15 ] = control_w( ( dx * -2.0f + m1x ) + m2x );
        g[ 16 ] = control_w( ( dy * -2.0f + m1y ) + m2y );
        g[ 17 ] = control_w( ( dz * -2.0f + m1z ) + m2z );
    }
    key_motion_grow( e.lengths_capacity, count + 1 );
    e.lengths.assign( static_cast< std::size_t >( count + 1 ), 0u );
    static const std::uint32_t nodes[ 5 ] = { 0x3D4024C8u, 0x3E6C4DC0u, 0x3F000000u, 0x3F44EC90u, 0x3F73FDB3u };
    static const std::uint32_t weights[ 5 ] = { 0x3DF29CF6u, 0x3E750ED1u, 0x3E91A2B4u, 0x3E750ED1u, 0x3DF29CF6u };
    for ( int i = 0; i < count; ++i )
    {
        const auto& g = e.spline[ static_cast< std::size_t >( i ) ];
        auto f = [ & ]( int k ) { return control_f( g[ static_cast< std::size_t >( k ) ] ); };
        const float bx = f( 11 ) * 2.0f, by = f( 12 ) * 2.0f, bz = f( 13 ) * 2.0f;
        float len = 0.0f;
        for ( int q = 0; q < 5; ++q )
        {
            const float s = control_f( nodes[ q ] );
            const float s3 = s * 3.0f;
            const float tz = bz + f( 17 ) * s3;
            const float tx = s * ( bx + f( 15 ) * s3 );
            const float ty = s * ( by + f( 16 ) * s3 );
            const float vx = f( 7 ) + tx, vy = f( 8 ) + ty, vz = f( 9 ) + s * tz;
            len = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( vy * vy + vx * vx ) + vz * vz ) ) ) * control_f( weights[ q ] ) + len;
        }
        e.lengths[ static_cast< std::size_t >( i + 1 ) ] = control_w( control_f( e.lengths[ static_cast< std::size_t >( i ) ] ) + len );
    }
    e.total = e.lengths[ static_cast< std::size_t >( count ) ];
    e.spline_valid = 1;
    return true;
}

// The "log" 0x61B9E0 takes of q * conj( p ) (retail's own arithmetic: theta = x87 fcos of the product's w - not acos -,
// v = theta (x, y, z) / fsin( theta ) when that sine is above 0, else 0). p = (a0..a3), q = (b0..b3).
inline void key_motion_squad_log( const float* p, const float* q, float* out )
{
    const float na0 = -p[ 0 ], na1 = -p[ 1 ], na2 = -p[ 2 ], a3 = p[ 3 ];
    const float w = ( ( q[ 3 ] * a3 - q[ 0 ] * na0 ) - q[ 1 ] * na1 ) - q[ 2 ] * na2;
    const float x = ( ( q[ 0 ] * a3 + q[ 3 ] * na0 ) + q[ 1 ] * na2 ) - q[ 2 ] * na1;
    const float y = ( ( q[ 1 ] * a3 + q[ 3 ] * na1 ) + q[ 2 ] * na0 ) - q[ 0 ] * na2;
    const float z = ( ( q[ 2 ] * a3 + q[ 3 ] * na2 ) + q[ 0 ] * na1 ) - q[ 1 ] * na0;
    const float theta = key_x87_cos( w );
    const float s = key_x87_sin( theta );
    if ( _mm_comigt_ss( _mm_set_ss( s ), _mm_setzero_ps( ) ) )
    {
        const float inv = 1.0f / s;
        out[ 0 ] = ( theta * x ) * inv;
        out[ 1 ] = ( y * theta ) * inv;
        out[ 2 ] = ( z * theta ) * inv;
    }
    else
        out[ 0 ] = out[ 1 ] = out[ 2 ] = 0.0f;
}

// 0x61B9E0 (thiscall on +0xD0; 20-byte records {qx qy qz qw, t}, n; retn 8): n < 4 -> false; n - 3 segments; first the
// records' hemisphere in place (q_k+1 negated when ((x x' + y y') + z' z) + w w' < 0, in order); segment i over A = q_i, B, C,
// D = q_i+3: L1 = log(B, A), L2 = log(C, B), L3 = log(D, C); k = (t_C - t_B) 2, s1 = (k / (t_C - t_A)) 0.5, s2 = (k / (t_D -
// t_B)) 0.5; v = (((L2 s1) + (L1 s1)) - L2) 0.5, w = (L2 - ((L3 s2) + (L2 s2))) 0.5; each through an "exp" (r = |v|, x87
// fsincos, (v sin r) / r when r > 0), a_i = B x exp( v ), b_i = C x exp( w ) in retail's lane order; the segment {t_B, t_C,
// 1 / (t_C - t_B), B, a_i, b_i, C}; valid = 1.
inline bool key_motion_squad_fit( key_motion_t& e, std::vector< std::array< float, 5 > > r )
{
    const int n = static_cast< int >( r.size( ) );
    if ( n < 4 )
        return false;
    const int count = n - 3;
    key_motion_grow( e.squad_capacity, count );
    e.squad.assign( static_cast< std::size_t >( count ), std::array< std::uint32_t, 19 >{ } );
    for ( int k = 0; k + 1 < n; ++k )
    {
        auto &p = r[ k ], &q = r[ k + 1 ];
        const float d = ( ( p[ 0 ] * q[ 0 ] + p[ 1 ] * q[ 1 ] ) + q[ 2 ] * p[ 2 ] ) + p[ 3 ] * q[ 3 ];
        if ( _mm_comigt_ss( _mm_setzero_ps( ), _mm_set_ss( d ) ) )
            for ( int j = 0; j < 4; ++j )
                q[ j ] = -q[ j ];
    }
    for ( int i = 0; i < count; ++i )
    {
        const float *A = r[ i ].data( ), *B = r[ i + 1 ].data( ), *C = r[ i + 2 ].data( ), *D = r[ i + 3 ].data( );
        float L1[ 3 ], L2[ 3 ], L3[ 3 ];
        key_motion_squad_log( A, B, L1 );
        key_motion_squad_log( B, C, L2 );
        key_motion_squad_log( C, D, L3 );
        const float k = ( C[ 4 ] - B[ 4 ] ) * 2.0f;
        const float s1 = ( k / ( C[ 4 ] - A[ 4 ] ) ) * 0.5f;
        const float s2 = ( k / ( D[ 4 ] - B[ 4 ] ) ) * 0.5f;
        const float vy = ( ( L2[ 1 ] * s1 + L1[ 1 ] * s1 ) - L2[ 1 ] ) * 0.5f;
        const float vx = ( ( L2[ 0 ] * s1 + L1[ 0 ] * s1 ) - L2[ 0 ] ) * 0.5f;
        const float vz = ( ( L2[ 2 ] * s1 + L1[ 2 ] * s1 ) - L2[ 2 ] ) * 0.5f;
        float ex = 0.0f, ey = 0.0f, ez = 0.0f;
        const float rv = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( vy * vy + vx * vx ) + vz * vz ) ) );
        const float cv = key_x87_cos( rv ), sv = key_x87_sin( rv );
        if ( _mm_comigt_ss( _mm_set_ss( rv ), _mm_setzero_ps( ) ) )
        {
            const float inv = 1.0f / rv;
            ex = ( vx * sv ) * inv;
            ey = ( vy * sv ) * inv;
            ez = ( vz * sv ) * inv;
        }
        const float wxs = L3[ 0 ] * s2 + L2[ 0 ] * s2, wzs = L3[ 2 ] * s2 + L2[ 2 ] * s2;
        const float wy = ( L2[ 1 ] - ( L3[ 1 ] * s2 + L2[ 1 ] * s2 ) ) * 0.5f;
        const float wx = ( L2[ 0 ] - wxs ) * 0.5f;
        const float wz = ( L2[ 2 ] - wzs ) * 0.5f;
        float fx = 0.0f, fy = 0.0f, fz = 0.0f;
        const float rw = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( wy * wy + wx * wx ) + wz * wz ) ) );
        const float cw = key_x87_cos( rw ), sw = key_x87_sin( rw );
        if ( _mm_comigt_ss( _mm_set_ss( rw ), _mm_setzero_ps( ) ) )
        {
            const float inv = 1.0f / rw;
            fx = ( wx * sw ) * inv;
            fy = ( wy * sw ) * inv;
            fz = ( wz * sw ) * inv;
        }
        auto& g = e.squad[ static_cast< std::size_t >( i ) ];
        g[ 0 ] = control_w( B[ 4 ] );
        g[ 1 ] = control_w( C[ 4 ] );
        g[ 2 ] = control_w( 1.0f / ( C[ 4 ] - B[ 4 ] ) );
        for ( int j = 0; j < 4; ++j )
        {
            g[ 3 + j ] = control_w( B[ j ] );
            g[ 15 + j ] = control_w( C[ j ] );
        }
        const float b0 = B[ 0 ], b1 = B[ 1 ], b2 = B[ 2 ], b3 = B[ 3 ];
        g[ 7 ] = control_w( ( ( b0 * cv + b3 * ex ) + b2 * ey ) - b1 * ez );
        g[ 8 ] = control_w( ( ( b1 * cv + b3 * ey ) + b0 * ez ) - b2 * ex );
        g[ 9 ] = control_w( ( ( b2 * cv + b3 * ez ) + b1 * ex ) - b0 * ey );
        g[ 10 ] = control_w( ( ( b3 * cv - b0 * ex ) - b1 * ey ) - b2 * ez );
        const float c0 = C[ 0 ], c1 = C[ 1 ], c2 = C[ 2 ], c3 = C[ 3 ];
        g[ 11 ] = control_w( ( ( c3 * fx + c0 * cw ) + c2 * fy ) - c1 * fz );
        g[ 12 ] = control_w( ( ( c3 * fy + c1 * cw ) + c0 * fz ) - c2 * fx );
        g[ 13 ] = control_w( ( ( c3 * fz + c2 * cw ) + c1 * fx ) - c0 * fy );
        g[ 14 ] = control_w( ( ( c3 * cw - c0 * fx ) - c1 * fy ) - c2 * fz );
    }
    e.squad_valid = 1;
    return true;
}

// 0x5CF800 (Curve Type 3's set-up): both splines' arrays freed and zeroed (counts, capacities, the total, the valid bytes);
// with 4 keys or more: 0x61B120 on {key position x y z w, time} and 0x61B9E0 on {key quaternion, time}.
inline void key_motion_spline_setup( key_motion_t& e )
{
    e.spline.clear( );
    e.spline_capacity = 0;
    e.lengths.clear( );
    e.lengths_capacity = 0;
    e.total = 0;
    e.spline_valid = 0;
    e.squad.clear( );
    e.squad_capacity = 0;
    e.squad_valid = 0;
    const int n = static_cast< int >( e.keys.size( ) );
    if ( n < 4 )
        return;
    std::vector< std::array< float, 5 > > p( n ), q( n );
    for ( int i = 0; i < n; ++i )
    {
        const auto& k = e.keys[ static_cast< std::size_t >( i ) ];
        for ( int j = 0; j < 4; ++j )
        {
            p[ i ][ j ] = control_f( k[ 1 + j ] );
            q[ i ][ j ] = control_f( k[ 5 + j ] );
        }
        p[ i ][ 4 ] = q[ i ][ 4 ] = control_f( k[ 0 ] );
    }
    key_motion_spline_fit( e, p );
    key_motion_squad_fit( e, q );
}

// A segment search (0x61B600 / 0x61C550): t <= seg 0's t0 (comiss / jbe: NaN too) -> 0, u = 0; the last's t1 <= t -> the
// last, u = 1; else the first segment whose t1 is above t, u = (t - t0) x its 1 / (t1 - t0).
inline int key_motion_segment( const std::vector< std::array< std::uint32_t, 19 > >& s, float t, float& u )
{
    const int n = static_cast< int >( s.size( ) );
    if ( !_mm_comigt_ss( _mm_set_ss( t ), _mm_set_ss( control_f( s[ 0 ][ 0 ] ) ) ) )
    {
        u = 0.0f;
        return 0;
    }
    if ( !_mm_comigt_ss( _mm_set_ss( control_f( s[ static_cast< std::size_t >( n - 1 ) ][ 1 ] ) ), _mm_set_ss( t ) ) )
    {
        u = 1.0f;
        return n - 1;
    }
    int i = 0;
    while ( i < n && !_mm_comigt_ss( _mm_set_ss( control_f( s[ static_cast< std::size_t >( i ) ][ 1 ] ) ), _mm_set_ss( t ) ) )
        ++i;
    u = ( t - control_f( s[ static_cast< std::size_t >( i ) ][ 0 ] ) ) * control_f( s[ static_cast< std::size_t >( i ) ][ 2 ] );
    return i;
}

// 0x5316C0 (ecx out, edx a, [esp+4] b, xmm3 t; no hemisphere test): theta = acosf( minss( a . b, 1 ) ) (the CRT import, its
// double narrowed - SUBSTITUTION); theta > FLT_EPSILON and 3.1415925 (0x6F774C) > theta -> a sin( theta - theta t ) / s +
// b sin( theta t ) / s (s = x87 fsin( theta ), 1 / s once); else a.
inline void key_motion_slerp_plain( float* out, const float* a, const float* b, float t )
{
    const float d = ( ( b[ 0 ] * a[ 0 ] + a[ 1 ] * b[ 1 ] ) + b[ 2 ] * a[ 2 ] ) + a[ 3 ] * b[ 3 ];
    const float theta = static_cast< float >( std::acos( static_cast< double >( _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( d ), _mm_set_ss( 1.0f ) ) ) ) ) );
    if ( !_mm_comigt_ss( _mm_set_ss( theta ), _mm_set_ss( control_f( 0x34000000u ) ) ) ||
         !_mm_comigt_ss( _mm_set_ss( control_f( 0x40490FDAu ) ), _mm_set_ss( theta ) ) )
    {
        std::memcpy( out, a, 16 );
        return;
    }
    const float inv = 1.0f / key_x87_sin( theta );
    const float tt = theta * t;
    const float wa = key_x87_sin( theta - tt ) * inv;
    const float wb = key_x87_sin( tt ) * inv;
    const float o0 = a[ 0 ] * wa + b[ 0 ] * wb;
    const float o1 = a[ 1 ] * wa + b[ 1 ] * wb;
    const float o2 = wa * a[ 2 ] + b[ 2 ] * wb;
    const float o3 = a[ 3 ] * wa + wb * b[ 3 ];
    out[ 0 ] = o0;
    out[ 1 ] = o1;
    out[ 2 ] = o2;
    out[ 3 ] = o3;
}

// 0x61C550 (thiscall on +0xD0; out quaternion; t in xmm2): the segment (key_motion_segment), k = (1 - u) (u 2), then
// 0x5314C0( slerp_plain( q_i, q_i+1, u ), slerp_plain( a_i, b_i, u ), k ) - squad.
inline void key_motion_squad_eval( const key_motion_t& e, float t, float* q )
{
    float u;
    const int i = key_motion_segment( e.squad, t, u );
    const auto& g = e.squad[ static_cast< std::size_t >( i ) ];
    float w[ 16 ];
    for ( int j = 0; j < 16; ++j )
        w[ j ] = control_f( g[ 3 + j ] );
    const float k = ( 1.0f - u ) * ( u * 2.0f );
    float s1[ 4 ], s2[ 4 ];
    key_motion_slerp_plain( s1, w + 4, w + 8, u );
    key_motion_slerp_plain( s2, w, w + 12, u );
    front_rig_slerp( q, s2, s1, k );
}

// Numerical Recipes' `splint` on a +0x90 / +0xA0 curve, 0x5D0440's operand order (types 1 and 2 alike).
inline void key_motion_splint( const key_motion_curve_t& c, int a, int b, float u, float* out )
{
    const auto& ca = c[ static_cast< std::size_t >( a ) ];
    const auto& cb = c[ static_cast< std::size_t >( b ) ];
    const float r = 1.0f - u;
    const float h = control_f( cb[ 8 ] ) - control_f( ca[ 8 ] );
    const float h2 = h * h;
    const float wb = ( u * u ) * u - u, wa = ( r * r ) * r - r;
    const float sixth = control_f( 0x3E2AAAABu );   // 0x6F761C
    for ( int j = 0; j < 3; ++j )
    {
        const float curv = wa * control_f( ca[ 4 + j ] ) + wb * control_f( cb[ 4 + j ] );
        const float line = control_f( ca[ j ] ) * r + control_f( cb[ j ] ) * u;
        out[ j ] = ( h2 * curv ) * sixth + line;
    }
}

// 0x5D0440 (keys a, b, time t): u = (t - T_a) / (T_b - T_a) (identity first: VuMatrix_identity 0x410A40). Curve Types 0 / 1:
// the rotation = the matrix of slerp( q_a, q_b, u ) (0x5314C0, 0x531180); the translation: type 0 P_a (1 - u) + P_b u per
// lane, type 1 key_motion_splint on the +0x90 curve. Type 2: the translation the same splint on +0x90, the rotation
// VuMatrix_multiply( identity, VuMatrix_fromEuler( splint on +0xA0 ) ) (0x4078C0, 0x435640) - the product kept (its zeros'
// signs are the multiply's).
inline bool key_motion_pose( const key_motion_t& e, int a, int b, float t, float* m )
{
    const auto& ka = e.keys[ static_cast< std::size_t >( a ) ];
    const auto& kb = e.keys[ static_cast< std::size_t >( b ) ];
    const float u = ( t - control_f( ka[ 0 ] ) ) / ( control_f( kb[ 0 ] ) - control_f( ka[ 0 ] ) );
    if ( e.curve != 0 && e.curve != 1 && e.curve != 2 && e.curve != 3 )
        return false;
    if ( e.curve == 3 )
    {
        // 0x61B600 on +0xB0 with the time itself, the cubic in u; 0x61C550's squad; 0x531180. Fewer than 4 keys: both splines
        // are empty and retail reads through a null array (UNKNOWN).
        if ( e.spline.empty( ) || e.squad.empty( ) )
            return false;
        float su;
        const auto& g = e.spline[ static_cast< std::size_t >( key_motion_segment( e.spline, t, su ) ) ];
        auto f = [ & ]( int k ) { return control_f( g[ static_cast< std::size_t >( k ) ] ); };
        float q[ 4 ];
        key_motion_squad_eval( e, t, q );
        front_rig_matrix( q, m );
        m[ 12 ] = ( ( f( 7 ) + ( f( 11 ) + f( 15 ) * su ) * su ) * su ) + f( 3 );
        m[ 13 ] = f( 4 ) + ( f( 8 ) + ( f( 12 ) + f( 16 ) * su ) * su ) * su;
        m[ 14 ] = f( 5 ) + ( f( 9 ) + ( f( 13 ) + f( 17 ) * su ) * su ) * su;
        ( void ) u;
        return true;
    }
    if ( e.curve != 0 && static_cast< std::size_t >( b ) >= e.cubic.size( ) )
        return false;   // retail reads past the +0x90 array (fewer than 2 keys never reaches here)
    if ( e.curve == 2 && static_cast< std::size_t >( b ) >= e.euler.size( ) )
        return false;
    const float r = 1.0f - u;
    float p[ 3 ];
    if ( e.curve == 0 )
    {
        p[ 0 ] = control_f( ka[ 1 ] ) * r + control_f( kb[ 1 ] ) * u;
        p[ 1 ] = control_f( ka[ 2 ] ) * r + control_f( kb[ 2 ] ) * u;
        p[ 2 ] = control_f( ka[ 3 ] ) * r + control_f( kb[ 3 ] ) * u;
    }
    else
        key_motion_splint( e.cubic, a, b, u, p );
    if ( e.curve == 2 )
    {
        float angles[ 3 ], id[ 16 ], rot[ 16 ];
        key_motion_splint( e.euler, a, b, u, angles );
        vu_identity( id );
        vu_matrix_from_euler( rot, angles );
        hull_multiply( id, m, rot );
    }
    else
    {
        float qa[ 4 ], qb[ 4 ], q[ 4 ];
        for ( int j = 0; j < 4; ++j )
        {
            qa[ j ] = control_f( ka[ 5 + j ] );
            qb[ j ] = control_f( kb[ 5 + j ] );
        }
        front_rig_slerp( q, qa, qb, u );
        front_rig_matrix( q, m );
    }
    m[ 12 ] = p[ 0 ];
    m[ 13 ] = p[ 1 ];
    m[ 14 ] = p[ 2 ];
    return true;
}

// 0x5D01B0 (keys a, b, weight w; time t in xmm3): M1 = pose( t ), M2 = pose( w x 0.001 + t ); linear = (M2 - M1)
// translation x 999.99994; angular = (euler( M2 ) - euler( M1 )) x 999.99994 (the euler of 0x5344A0); apply M1.
inline void key_motion_interpolate( key_motion_t& e, int a, int b, float w, float t, const key_motion_hooks_t& h )
{
    float m1[ 16 ], m2[ 16 ];
    if ( !key_motion_pose( e, a, b, t, m1 ) || !key_motion_pose( e, a, b, w * control_f( 0x3A83126Fu ) + t, m2 ) )
    {
        key_motion_unrecreated( h );
        return;
    }
    const float k = control_f( 0x4479FFFFu );   // 0x6F7824 = 999.99994; 0x6F75B0 = 0.001 above
    float lin[ 4 ] = { ( m2[ 12 ] - m1[ 12 ] ) * k, ( m2[ 13 ] - m1[ 13 ] ) * k, ( m2[ 14 ] - m1[ 14 ] ) * k, 0.0f };
    float e1[ 3 ], e2[ 3 ];
    transform_euler( m1, e1 );
    transform_euler( m2, e2 );
    float ang[ 4 ] = { ( e2[ 0 ] - e1[ 0 ] ) * k, ( e2[ 1 ] - e1[ 1 ] ) * k, ( e2[ 2 ] - e1[ 2 ] ) * k, 0.0f };
    key_motion_apply( e, m1, lin, ang );
}

// Slot 14 0x5CFE60 (the update, dt): no keys -> Deactivate; one key or FLT_EPSILON > duration (comiss: a NaN duration goes
// on) -> key 0 + Deactivate. clock >= duration: Loop -> clock = dur x (q - fistp( q - 0.5 )), q = clock / dur, and OnLoop
// after; else the last key, Deactivate, "OnDone". Ease In/Out with Curve Type 0 / 3: u = clock / dur, e = (u 3) u - ((u 2)
// u) u, time = dur e, w = dur ((6e) / dur - ((6e) e) / dur) (each "/ dur" a multiply by 1 / dur); else time = clock, w = 1.
// The key = the last i < n - 1 with time >= T_i (comiss / cmovae); then 0x5D01B0( i, i + 1, w ) and clock += dt.
inline void key_motion_update( key_motion_t& e, float dt, const key_motion_hooks_t& h )
{
    const int n = static_cast< int >( e.keys.size( ) );
    if ( n == 0 )
    {
        key_motion_deactivate( e, h );
        return;
    }
    float dur = control_f( e.duration );
    if ( n == 1 || _mm_comigt_ss( _mm_set_ss( control_f( 0x34000000u ) ), _mm_set_ss( dur ) ) )
    {
        key_motion_set_key( e, 0 );
        key_motion_deactivate( e, h );
        return;
    }
    float clock = control_f( e.clock );
    bool looped = false;
    int key = -1;
    if ( _mm_comige_ss( _mm_set_ss( clock ), _mm_set_ss( dur ) ) )
    {
        if ( !e.loop )
        {
            key_motion_set_key( e, n - 1 );
            key_motion_deactivate( e, h );
            if ( h.fire )
                h.fire( h.user, e, "OnDone" );
            return;
        }
        const float q = clock / dur;
        const double r = std::nearbyint( static_cast< double >( q ) - 0.5 );
        const std::int32_t i = r >= -2147483648.0 && r < 2147483648.0 ? static_cast< std::int32_t >( r ) : INT32_MIN;
        key = i;   // the fistp's slot is also the key search's starting value
        dur = control_f( e.duration );
        e.clock = control_w( dur * ( q - static_cast< float >( i ) ) );
        looped = true;
    }
    float t = control_f( e.clock );
    float w = 1.0f;
    if ( e.ease && ( e.curve == 0 || e.curve == 3 ) )
    {
        const float inv = 1.0f / dur;
        const float u = inv * t;
        const float ev = ( u * 3.0f ) * u - ( ( u * 2.0f ) * u ) * u;
        const float six = ev * 6.0f;
        w = dur * ( six * inv - ( six * ev ) * inv );
        t = dur * ev;
    }
    bool found = looped;
    for ( int i = 0; i < n - 1; ++i )
    {
        if ( _mm_comige_ss( _mm_set_ss( t ), _mm_set_ss( control_f( e.keys[ static_cast< std::size_t >( i ) ][ 0 ] ) ) ) )
        {
            key = i;
            found = true;
        }
    }
    if ( !found || key < 0 || key + 1 >= n )
        key_motion_unrecreated( h );
    else
        key_motion_interpolate( e, key, key + 1, w, t, h );
    e.clock = control_w( control_f( e.clock ) + dt );
    if ( looped && h.fire )
        h.fire( h.user, e, "OnLoop" );
}

// The "Motion" tick 0x61A6E0: slot 14 while active.
inline void key_motion_tick( key_motion_t& e, float dt, const key_motion_hooks_t& h )
{
    if ( e.active )
        key_motion_update( e, dt, h );
}

// 0x5CEA90: the keys from the VuKeyframeEntity children in child order (the VuArray growth max( n, cap + cap / 2 )),
// qsort'ed by time (0x5CCB60), rebased to the first; the duration = the last key's time.
inline void key_motion_gather( key_motion_t& e, const std::vector< key_motion_child_t >& children )
{
    e.keys.clear( );
    for ( const key_motion_child_t& c : children )
    {
        if ( !c.keyframe )
            continue;
        std::array< std::uint32_t, 9 > k{};
        k[ 0 ] = c.time;
        for ( int j = 0; j < 4; ++j )
            k[ 1 + j ] = control_w( c.transform[ 12 + j ] );
        float q[ 4 ];
        quat_from_rows( q, c.transform );
        for ( int j = 0; j < 4; ++j )
            k[ 5 + j ] = control_w( q[ j ] );
        const std::int32_t want = static_cast< std::int32_t >( e.keys.size( ) ) + 1;
        if ( want > e.capacity )
        {
            std::int32_t grow = e.capacity + e.capacity / 2;
            if ( want > grow )
                grow = want;
            if ( grow > e.capacity )
                e.capacity = grow;
        }
        e.keys.push_back( k );
    }
    const int n = static_cast< int >( e.keys.size( ) );
    if ( n == 0 )
        return;
    std::vector< std::pair< std::uint32_t, int > > order;
    for ( int i = 0; i < n; ++i )
        order.emplace_back( e.keys[ static_cast< std::size_t >( i ) ][ 0 ], i );
    control_qsort( order, n );
    std::vector< std::array< std::uint32_t, 9 > > sorted;
    for ( const auto& o : order )
        sorted.push_back( e.keys[ static_cast< std::size_t >( o.second ) ] );
    e.keys = sorted;
    const float first = control_f( e.keys[ 0 ][ 0 ] );
    for ( auto& k : e.keys )
        k[ 0 ] = control_w( control_f( k[ 0 ] ) - first );
    e.duration = e.keys.back( )[ 0 ];
}

// Slot 6 0x5CE9E0 (game initialize): gather; Curve Types 1 / 2 / 3 set up their splines (1: 0x5CEC60; 2: 0x5CEC60 then 0x5CEF20;
// 3: 0x5CF800); then
// VuMotionEntity's 0x61A420: the "Motion" tick registered; with a linked ref, +0x68 = its entity's motion component
// (hooks.find, null when it has none; unlinked: +0x68 unchanged); Initially Active -> Activate.
inline void key_motion_game_init( key_motion_t& e, const std::vector< key_motion_child_t >& children, bool linked,
                                  const key_motion_hooks_t& h )
{
    key_motion_gather( e, children );
    if ( e.curve == 1 || e.curve == 2 )
        key_motion_cubic_setup( e );
    if ( e.curve == 2 )
        key_motion_euler_setup( e );
    if ( e.curve == 3 )
        key_motion_spline_setup( e );
    if ( h.tick_register )
        h.tick_register( h.user, e, true );
    if ( linked )
        e.target = h.find ? h.find( h.user, e ) : nullptr;
    if ( e.initially_active )
        key_motion_activate( e, h );
}

// Slot 7 0x5CEA40 (game release): Deactivate, the tick unregistered, the key count and the spline counts zeroed.
inline void key_motion_game_release( key_motion_t& e, const key_motion_hooks_t& h )
{
    key_motion_deactivate( e, h );
    if ( h.tick_register )
        h.tick_register( h.user, e, false );
    e.keys.clear( );
    e.cubic.clear( );     // +0x94 = 0
    e.cubic_valid = 0;    // +0x9C = 0 (+0xA4 / +0xAC are left as they are)
}

// Slot 8 0x61A4E0 (reset): the one-shot latch cleared.
inline void key_motion_reset( key_motion_t& e )
{
    e.latch = 0;
}
