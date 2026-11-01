#pragma once

#include <workspace/core/boat/state.hxx>

#include <cmath>
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

// Retail's `fld dword; fsincos; fstp dword (cos); fstp dword (sin)` on this
// machine's FPU (engine/x87.asm).
extern "C" void x87_sincos( float x, float* c, float* s );

inline float ai_bits( std::uint32_t u )
{
    float f;
    std::memcpy( &f, &u, 4 );
    return f;
}

// __libm_sse2_asinf 0x631CCA. SUBSTITUTION: the CRT DLL's asinf is served by
// the host's DOUBLE asin rounded once to float (what boat_transform_changed
// measured to match retail; the float std::asin did not).
inline float ai_asinf( float x )
{
    return static_cast< float >( std::asin( static_cast< double >( x ) ) );
}

// 0x40D110 (thiscall, the driver; pos*, heading*, target*, speed, wanted,
// dt, steer*, throttle*; retn 20h), VuAiDriver's steering and throttle.
// Nothing at all unless dt > 0 (ordered; dt is not read again). d = the xy
// direction to the target (normalised by VuMath_sqrt(dy^2 + dx^2)); the
// heading's xy, normalised only when fy^2 + fx^2 > 2^-23 (0x6F7584; its z
// then 1/len x 0, else 0); a0 = asinf(maxss(-1, minss(1, fx dy - fy dx))) x
// 57.29578 (0x6F77CC). With the body's (boat +0x21C) planar speed^2 (+0x140:
// vx^2 + vy^2) > 5 (0x6F7768) and the solver (hull +0x1CC) +0x54 > 0 the
// heading becomes the velocity's direction (z = 1/len x 0). The heading is
// turned by 0x40B5F0's rotation about z by the angular velocity's z (+0x158)
// x 0.25 (0x6F762C): r = (M4 fy + M0 fx) + M8 fz, (M5 fy + M1 fx) + M9 fz;
// c = maxss(-1, minss(1, dy rx - ry dx)), a1 = asinf(c) x 57.29578. Unless
// a1 a0 >= 1 (or NaN) c is faded by |a0| (VuVector_abs 0x4089C0): 1 up to 5,
// 1 - (|a0| - 5) x 0.2 below 10 (a NaN too), 0 from 10. steer =
// maxss(-1, minss(1, c x -4)); throttle = 1 when wanted > speed, else -1
// when speed x 0.75 > wanted, else 0.
inline void ai_steer( const std::uint8_t* body, const std::uint8_t* solver, const float* pos, const float* heading, const float* target,
    float speed, float wanted, float dt, float* steer, float* throttle )
{
    if ( !_mm_comigt_ss( _mm_set_ss( dt ), _mm_set_ss( 0.0f ) ) )
        return;
    float dy = target[ 1 ] - pos[ 1 ];
    float dx = target[ 0 ] - pos[ 0 ];
    const float id = 1.0f / hull_sqrt( dy * dy + dx * dx );
    dx = dx * id;
    dy = dy * id;
    float fx = heading[ 0 ], fy = heading[ 1 ], fz = 0.0f;
    const float q = fy * fy + fx * fx;
    if ( q > ai_bits( 0x34000000u ) )
    {
        const float iq = 1.0f / hull_sqrt( q );
        fx = fx * iq;
        fy = fy * iq;
        fz = iq * 0.0f;
    }
    const float deg = ai_bits( 0x42652EE1u );
    const float a0 = ai_asinf( ai_maxss( -1.0f, ai_minss( 1.0f, fx * dy - fy * dx ) ) ) * deg;
    const float vx = state_f( body, 0x140 ), vy = state_f( body, 0x144 );
    const float v2 = vx * vx + vy * vy;
    if ( v2 > 5.0f && state_f( solver, 0x54 ) > 0.0f )
    {
        const float iv = 1.0f / hull_sqrt( v2 );
        fx = iv * vx;
        fy = iv * vy;
        fz = iv * 0.0f;
    }
    float m[ 16 ];
    {
        float c, s;
        x87_sincos( state_f( body, 0x158 ) * 0.25f, &c, &s );
        const float r[ 16 ] = { c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };   // 0x40B5F0
        std::memcpy( m, r, 64 );
    }
    const float rx = ( m[ 4 ] * fy + m[ 0 ] * fx ) + m[ 8 ] * fz;
    const float ry = ( m[ 5 ] * fy + m[ 1 ] * fx ) + m[ 9 ] * fz;
    const float c = ai_maxss( -1.0f, ai_minss( 1.0f, dy * rx - ry * dx ) );
    const float a1 = ai_asinf( c ) * deg;
    float r = c;
    if ( 1.0f > a1 * a0 )
    {
        const float a = std::fabs( a0 );
        float fade;
        if ( 5.0f >= a )
            fade = 0.0f;
        else if ( !( a >= 10.0f ) )
            fade = ( a - 5.0f ) * 0.2f;
        else
            fade = 1.0f;   // retail: 1.0 - 1.0 computed, x c
        r = c * ( 1.0f - fade );
    }
    *steer = ai_maxss( -1.0f, ai_minss( 1.0f, r * -4.0f ) );
    if ( wanted > speed )
        *throttle = 1.0f;
    else if ( speed * 0.75f > wanted )
        *throttle = -1.0f;
    else
        *throttle = 0.0f;
}
// 0x40BEC0 (thiscall, the driver; pos*, target*, direction*; retn 0Ch),
// VuAiDriver's path follow over the path points (driver +0x8C / +0x90; the
// tick calls it only with at least one). Point fields: +0 / +4 and +0x10 /
// +0x14 two xy marks, +0x40 / +0x70 two planes (x, y, z, d), +0x80 its start,
// +0x90 its end, +0xA0 its direction (16 b), +0xB0 its length, +0xC0 / +0xC4
// the widths at start / end.
//
// On point 0: t = d0 / (d1 + d0) (the planes at pos), u = 1 - t, the
// segment point c = u start + t end, its width u w0 + t w1; s =
// maxss(0, minss(1, (|pos - c|xy / width - 0.5) x 2)), the look-ahead L =
// ((1 - s) + s) x 75 (0x6F77D4). When point 0's remaining length +0xB0 x u
// exceeds L: target = c + direction x L (xyz; its w is never written - a
// stale stack word, UNKNOWN: left as it was) and direction = point 0's.
// Otherwise rem = L - +0xB0 u and, for each point i >= 1 (fewer: nothing
// written at all): best = 1, and for every j in 1..i the rays from pos to
// 1000 along pos -> mark A (+0 / +4) and pos -> mark B (+0x10 / +0x14) are
// crossed with point i's segment (start -> end, xy): a determinant below
// FLT_MIN (0x6F7574) in magnitude, or either parameter outside [0, 1]
// (ordered), misses; the first ray that hits (A's, else B's) gives best =
// minss(best, |hit - start| / |segment|). v = minss(maxss(best, (20 -
// (L - rem)) / len_i), rem / len_i) (0x6F77A0; 1 / len multiplied). When
// 1 > v: target = (1 - v) start + v end with w 0, direction = point i's,
// done. Else target = point i's end (16 b), direction = its, rem -= len_i,
// and the next point.
inline void ai_path_follow( const std::uint8_t* const* points, std::int32_t count, const float* pos, float* target, float* direction )
{
    auto at = [ & ]( std::int32_t k ) -> const std::uint8_t* { return k < count ? points[ k ] : nullptr; };
    auto ge = []( float a, float b ) { return _mm_comige_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; };   // ordered a >= b
    const std::uint8_t* p = count > 0 ? points[ 0 ] : nullptr;
    const float px = pos[ 0 ], py = pos[ 1 ], pz = pos[ 2 ];
    const float d0 = ( ( state_f( p, 0x44 ) * py + state_f( p, 0x40 ) * px ) + state_f( p, 0x48 ) * pz ) + state_f( p, 0x4C );
    const float d1 = ( ( state_f( p, 0x74 ) * py + px * state_f( p, 0x70 ) ) + state_f( p, 0x78 ) * pz ) + state_f( p, 0x7C );
    const float t = d0 / ( d1 + d0 );
    const float u = 1.0f - t;
    const float cx = u * state_f( p, 0x80 ) + state_f( p, 0x90 ) * t;
    const float cy = state_f( p, 0x84 ) * u + state_f( p, 0x94 ) * t;
    const float ex = px - cx, ey = py - cy;
    const float dist = hull_sqrt( ex * ex + ey * ey );
    const float width = state_f( p, 0xC0 ) * u + state_f( p, 0xC4 ) * t;
    const float s = ai_maxss( 0.0f, ai_minss( 1.0f, ( dist / width - 0.5f ) * 2.0f ) );
    const float look = ( ( 1.0f - s ) + s ) * 75.0f;
    const float left0 = state_f( p, 0xB0 ) * u;
    if ( left0 > look )
    {
        target[ 0 ] = state_f( p, 0xA0 ) * look + cx;
        target[ 1 ] = state_f( p, 0xA4 ) * look + cy;
        target[ 2 ] = state_f( p, 0xA8 ) * look + ( state_f( p, 0x88 ) * u + state_f( p, 0x98 ) * t );
        std::memcpy( direction, p + 0xA0, 16 );
        return;
    }
    float rem = look - left0;
    for ( std::int32_t i = 1; i < count; ++i )
    {
        const std::uint8_t* q = at( i );
        const float q80 = state_f( q, 0x80 ), q84 = state_f( q, 0x84 );
        const float sx_ = state_f( q, 0x90 ) - q80, sy_ = state_f( q, 0x94 ) - q84;   // the segment (ex, ey in retail's frame)
        float best = 1.0f;
        for ( std::int32_t j = 1; j <= i; ++j )
        {
            const std::uint8_t* r = at( j );
            const float ax = state_f( r, 0 ) - px, ay = state_f( r, 4 ) - py;
            const float ia = 1.0f / hull_sqrt( ay * ay + ax * ax );
            const float Ay = ia * ay * 1000.0f + py, Ax = ia * ax * 1000.0f + px;
            const float bx = state_f( r, 0x10 ) - px, by = state_f( r, 0x14 ) - py;
            const float ib = 1.0f / hull_sqrt( by * by + bx * bx );
            const float Bx = px + ib * bx * 1000.0f, By = py + ib * by * 1000.0f;
            bool hit = false;
            float hx = 0.0f, hy = 0.0f;
            {
                const float rx = Ax - px, ry = Ay - py;
                const float det = sx_ * ry - sy_ * rx;
                if ( !( ai_bits( 0x00800000u ) > std::fabs( det ) ) )
                {
                    const float id = 1.0f / det;
                    const float wy = q84 - py, wx = q80 - px;
                    const float s1 = ( wy * rx - wx * ry ) * id;
                    const float s2 = ( wy * sx_ - wx * sy_ ) * id;
                    if ( ge( s1, 0.0f ) && ge( 1.0f, s1 ) && ge( s2, 0.0f ) && ge( 1.0f, s2 ) )
                    {
                        hit = true;
                        hx = q80 + sx_ * s1;
                        hy = q84 + sy_ * s1;
                    }
                }
            }
            if ( !hit )
            {
                const float ty = By - py, tx = Bx - px;
                const float det = sx_ * ty - sy_ * tx;
                if ( !( ai_bits( 0x00800000u ) > std::fabs( det ) ) )
                {
                    const float id = 1.0f / det;
                    const float wy = q84 - py, wx = q80 - px;
                    const float s1 = ( wy * tx - wx * ty ) * id;
                    const float s2 = ( wy * sx_ - wx * sy_ ) * id;
                    if ( ge( s1, 0.0f ) && ge( 1.0f, s1 ) && ge( s2, 0.0f ) && ge( 1.0f, s2 ) )
                    {
                        hit = true;
                        hx = q80 + s1 * sx_;
                        hy = q84 + s1 * sy_;
                    }
                }
            }
            if ( hit )
            {
                const float d = hull_sqrt( ( hy - q84 ) * ( hy - q84 ) + ( hx - q80 ) * ( hx - q80 ) );
                const float len = hull_sqrt( sy_ * sy_ + sx_ * sx_ );
                best = ai_minss( best, d / len );
            }
        }
        const float inv = 1.0f / state_f( q, 0xB0 );
        const float v = ai_minss( ai_maxss( best, ( 20.0f - ( look - rem ) ) * inv ), inv * rem );
        if ( 1.0f > v )
        {
            const float w = 1.0f - v;
            target[ 0 ] = q80 * w + v * state_f( q, 0x90 );
            target[ 1 ] = q84 * w + state_f( q, 0x94 ) * v;
            target[ 2 ] = state_f( q, 0x88 ) * w + state_f( q, 0x98 ) * v;
            target[ 3 ] = 0.0f;
            std::memcpy( direction, q + 0xA0, 16 );
            return;
        }
        std::memcpy( target, q + 0x90, 16 );
        std::memcpy( direction, q + 0xA0, 16 );
        rem = rem - state_f( q, 0xB0 );
    }
}