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
// One hit the dynamics world's ray test reports (Bullet's traversal and
// narrow phase - a HOOK): the collision object (a rigid body image: +0x10
// its world transform, +0xCC its shape, +0xF4 its internal type, +0x140 its
// linear velocity, +0x2CA its group, +0x2D0 its flags), the hit fraction,
// the normal (16 b) and whether it is in world space (else in the object's).
struct ai_ray_hit_t
{
    const std::uint8_t* body = nullptr;
    float fraction = 0.0f;
    float normal[ 4 ] = { };
    bool world = true;
};

// What 0x40C750 calls that is not recreated: g_vudynamicsimpl (0x75900C)
// vt+0x6C's collision world vt+0x18 rayTest (its hits, in Bullet's order),
// and the hit body's shape (+0xCC) vt+4 getAabb(body +0x10, min, max).
struct ai_avoid_hooks_t
{
    void* user = nullptr;
    int ( *ray )( void* user, const float* from, const float* to, const ai_ray_hit_t** hits ) = nullptr;
    void ( *aabb )( void* user, const std::uint8_t* body, float* min, float* max ) = nullptr;
};

// VuAiRayTestResult (vtable 0x736F7C; built on 0x40C750's stack): +4 hit,
// +8 body, +0xC fraction (1.0 first), +0x14 normal, +0x24 the body to
// ignore (the boat's), +0x28 group mask 0x11, +0x2C flag mask 0x10. (+0x10,
// the part id 0x50CE20 passes, is never read here.)
struct ai_ray_result_t
{
    bool hit = false;
    const std::uint8_t* body = nullptr;
    float fraction = 1.0f;
    float normal[ 4 ] = { };
};

// One ray through VuDynamicsRayTest's wrapper (vtable 0x74C5B4): for each
// hit, needsCollision 0x50CDF0 (internal type +0xF4 == 2, then the result's
// slot 0 0x404B60: not the ignored body, sign-extended +0x2CA & 0x11, and
// not +0x2D0 & 0x10) and addSingleResult 0x50CE20 (a local normal turned by
// the body's basis rows +0x10 / +0x20 / +0x30, w 0; the result's slot 1
// 0x404B20 keeps it when its fraction is ordered <= the kept one; hit = 1
// either way). The hit flag alone is cleared between rays.
inline void ai_ray( ai_ray_result_t& r, const std::uint8_t* ignore, const float* from, const float* to, const ai_avoid_hooks_t& hooks )
{
    const ai_ray_hit_t* hits = nullptr;
    const int n = hooks.ray ? hooks.ray( hooks.user, from, to, &hits ) : 0;
    for ( int k = 0; k < n; ++k )
    {
        const ai_ray_hit_t& h = hits[ k ];
        std::int32_t type, flags;
        std::int16_t group;
        std::memcpy( &type, h.body + 0xF4, 4 );
        std::memcpy( &group, h.body + 0x2CA, 2 );
        std::memcpy( &flags, h.body + 0x2D0, 4 );
        if ( type != 2 || h.body == ignore || ( 0x11 & static_cast< std::int32_t >( group ) ) == 0 || ( 0x10 & flags ) != 0 )
            continue;
        float n4[ 4 ];
        std::memcpy( n4, h.normal, 16 );
        if ( !h.world )
        {
            const float nx = h.normal[ 0 ], ny = h.normal[ 1 ], nz = h.normal[ 2 ];
            for ( int row = 0; row < 3; ++row )
            {
                const std::uint8_t* b = h.body + 0x10 + 0x10 * row;
                n4[ row ] = ( state_f( b, 4 ) * ny + state_f( b, 0 ) * nx ) + state_f( b, 8 ) * nz;
            }
            n4[ 3 ] = 0.0f;
        }
        if ( _mm_comige_ss( _mm_set_ss( r.fraction ), _mm_set_ss( h.fraction ) ) )
        {
            r.body = h.body;
            r.fraction = h.fraction;
            std::memcpy( r.normal, n4, 16 );
        }
        r.hit = true;
    }
}

// 0x40C750 (thiscall, the driver; pos*, target*; retn 8), VuAiDriver's
// obstacle avoidance. A ray from pos to (target x, target y, pos z), then
// while it hits (at most three steps, a fresh ray to the target after each):
// the hit body's aabb (its shape's getAabb) gives the centre c and r =
// maxss(half x, half y); the hit point H = pos + (to - pos) x fraction.
// Only a body whose velocity differs from the boat's (+0x21C +0x140) by
// more than 10 (VuMath_sqrt, ordered) is avoided. r <= 40 (0x6F77BC): the
// two points c +/- s x (+0x98 + r), s = the unit of (u_y - 0 u_z, 0 u_z -
// u_x, 0 u_x - 0 u_y) for u = the unit of pos - c; the target becomes the
// one whose direction from pos agrees more with the boat's velocity (P1
// when ordered greater, else P2). r > 40 (or NaN): with |(n_x + n_y) x 0 +
// n_z| < 0.707 (0x6F7674; the hit normal) the target becomes to - n k, k =
// n . (to - (H + +0x98 x the unit of pos - to)). Either way the target's w
// is a stale stack word (UNKNOWN: left).
inline void ai_avoid( float margin, const std::uint8_t* body, const float* pos, float* target, const ai_avoid_hooks_t& hooks )
{
    auto gt = []( float a, float b ) { return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; };   // ordered a > b
    ai_ray_result_t res;
    float to[ 3 ] = { target[ 0 ], target[ 1 ], pos[ 2 ] };
    {
        const float from4[ 4 ] = { pos[ 0 ], pos[ 1 ], pos[ 2 ], 0.0f };
        const float to4[ 4 ] = { to[ 0 ], to[ 1 ], to[ 2 ], 0.0f };
        ai_ray( res, body, from4, to4, hooks );
    }
    if ( !res.hit )
        return;
    for ( int step = 0;; ++step )
    {
        float mn[ 4 ] = { }, mx[ 4 ] = { };
        if ( hooks.aabb )
            hooks.aabb( hooks.user, res.body, mn, mx );
        const float hx = ( mx[ 0 ] - mn[ 0 ] ) * 0.5f, hy = ( mx[ 1 ] - mn[ 1 ] ) * 0.5f, hz = ( mx[ 2 ] - mn[ 2 ] ) * 0.5f;
        const float cx = mn[ 0 ] + hx, cy = mn[ 1 ] + hy, cz = mn[ 2 ] + hz;
        const float r = ai_maxss( hx, hy );
        const float px = pos[ 0 ], py = pos[ 1 ], pz = pos[ 2 ];
        const float f = res.fraction;
        const float Hx = px + ( to[ 0 ] - px ) * f, Hy = py + ( to[ 1 ] - py ) * f, Hz = pz + ( to[ 2 ] - pz ) * f;
        const float dvx = state_f( res.body, 0x140 ) - state_f( body, 0x140 );
        const float dvy = state_f( res.body, 0x144 ) - state_f( body, 0x144 );
        const float dvz = state_f( res.body, 0x148 ) - state_f( body, 0x148 );
        if ( gt( hull_sqrt( ( dvx * dvx + dvy * dvy ) + dvz * dvz ), 10.0f ) )
        {
            if ( _mm_comige_ss( _mm_set_ss( 40.0f ), _mm_set_ss( r ) ) )
            {
                const float dx = px - cx, dy = py - cy, dz = pz - cz;
                const float n = 1.0f / hull_sqrt( ( dx * dx + dy * dy ) + dz * dz );
                const float ux = n * dx, uy = n * dy, uz0 = n * dz * 0.0f;
                const float a = uy - uz0, b = uz0 - ux, c = ux * 0.0f - uy * 0.0f;
                const float m = 1.0f / hull_sqrt( ( b * b + a * a ) + c * c );
                const float R = margin + r;
                const float A = a * m, B = b * m, C = c * m;
                const float p1[ 3 ] = { A * R + cx, B * R + cy, C * R + cz };
                const float p2[ 3 ] = { cx - A * R, cy - B * R, cz - C * R };
                float e1[ 3 ] = { p1[ 0 ] - px, p1[ 1 ] - py, p1[ 2 ] - pz };
                const float n1 = 1.0f / hull_sqrt( ( e1[ 0 ] * e1[ 0 ] + e1[ 1 ] * e1[ 1 ] ) + e1[ 2 ] * e1[ 2 ] );
                float e2[ 3 ] = { p2[ 0 ] - px, p2[ 1 ] - py, p2[ 2 ] - pz };
                const float n2 = 1.0f / hull_sqrt( ( e2[ 0 ] * e2[ 0 ] + e2[ 1 ] * e2[ 1 ] ) + e2[ 2 ] * e2[ 2 ] );
                for ( int k = 0; k < 3; ++k )
                {
                    e1[ k ] = e1[ k ] * n1;
                    e2[ k ] = e2[ k ] * n2;
                }
                const float vx = state_f( body, 0x140 ), vy = state_f( body, 0x144 ), vz = state_f( body, 0x148 );
                const float iv = 1.0f / hull_sqrt( ( vx * vx + vy * vy ) + vz * vz );
                const float wx = vx * iv, wy = vy * iv, wz = vz * iv;
                const float dot1 = ( e1[ 0 ] * wx + e1[ 1 ] * wy ) + e1[ 2 ] * wz;
                const float dot2 = ( e2[ 0 ] * wx + e2[ 1 ] * wy ) + e2[ 2 ] * wz;
                const float* pick = gt( dot1, dot2 ) ? p1 : p2;
                std::memcpy( target, pick, 12 );
            }
            else
            {
                const float nx = res.normal[ 0 ], ny = res.normal[ 1 ], nz = res.normal[ 2 ];
                if ( gt( 0.70700002f, std::fabs( ( nx + ny ) * 0.0f + nz ) ) )
                {
                    const float dx = px - to[ 0 ], dy = py - to[ 1 ], dz = pz - to[ 2 ];
                    const float il = 1.0f / hull_sqrt( ( dx * dx + dy * dy ) + dz * dz );
                    const float qx = Hx + margin * ( dx * il ), qy = Hy + margin * ( dy * il ), qz = Hz + margin * ( dz * il );
                    const float k = ( nx * ( to[ 0 ] - qx ) + ny * ( to[ 1 ] - qy ) ) + nz * ( to[ 2 ] - qz );
                    target[ 0 ] = to[ 0 ] - nx * k;
                    target[ 1 ] = to[ 1 ] - ny * k;
                    target[ 2 ] = to[ 2 ] - nz * k;
                }
            }
        }
        to[ 0 ] = target[ 0 ];
        to[ 1 ] = target[ 1 ];
        to[ 2 ] = target[ 2 ];
        res.hit = false;
        {
            const float from4[ 4 ] = { pos[ 0 ], pos[ 1 ], pos[ 2 ], 0.0f };
            const float to4[ 4 ] = { to[ 0 ], to[ 1 ], to[ 2 ], 0.0f };
            ai_ray( res, body, from4, to4, hooks );
        }
        if ( !res.hit || step >= 2 )
            return;
    }
}

// Everything VuAiDriver's tick reads, by the pointers retail follows: the
// driver image, the boat (+0x5C), its body (+0x21C), transform component
// (+0x50), engine (+0x22C) and the engine's owner (engine +0), animation
// (+0x230); the other boat's transform (driver +0xBC's +0x50; null when
// +0xBC is 0); the path points (driver +0x8C / +0x90); VuBoatManager's
// boats (g 0x758FB8 +0x24 / +0x28).
struct ai_tick_t
{
    std::uint8_t* driver = nullptr;
    std::uint8_t* boat = nullptr;
    const std::uint8_t* body = nullptr;
    const std::uint8_t* solver = nullptr;   // the hull's (+0x228) +0x1CC
    const std::uint8_t* transform = nullptr;
    const std::uint8_t* engine = nullptr;
    const std::uint8_t* owner = nullptr;
    const std::uint8_t* animation = nullptr;
    const std::uint8_t* other = nullptr;
    const std::uint8_t* const* points = nullptr;
    std::int32_t count = 0;
    const std::uint8_t* const* boats = nullptr;
    std::int32_t boat_count = 0;
};

struct ai_tick_hooks_t
{
    ai_avoid_hooks_t avoid;
    ai_controls_hooks_t controls;
};

// VuAiDriver vtable slot 18 0x40B940 (thiscall, the driver; dt; retn 4).
// +0xA0 = |v| (body +0x140, VuMath_sqrt((vx^2 + vy^2) + vz^2)) signed by the
// transform's forward row (+0x60; ordered >= 0 is +1, a NaN -1). Without a
// first path point: 0x40D5A0(dt, 0, 0, no boost). Else 0x40BEC0 (into +0xA8;
// the direction goes to a dead stack local), 0x40C750, the wanted speed +0xB8
// = minss(+0x9C, (owner +0x470 x (engine +0xC x +4)) x 0.44704); with another
// boat whose direction from pos agrees with point 0's (+0xA0; dot > 0)
// boost and +0xB8 = ((engine +0xC x +4) x +0x28) x 0.44704. With +0x80 >= 2
// and a full boost (+0x310 == 16, ucomiss) the boat boosts when its +0x4CC
// is greater than the least among the boats; with no boats and +0x467 set
// it boosts once the water time +0x33C > 1. Then 0x40D110 (outputs 0 first)
// and 0x40D5A0. (The DebugDrivers byte 0x758F8A's loop does nothing.)
inline void ai_tick( const ai_tick_t& t, float dt, const ai_tick_hooks_t& hooks )
{
    std::uint8_t* driver = t.driver;
    std::uint8_t* boat = t.boat;
    const float vx = state_f( t.body, 0x140 ), vy = state_f( t.body, 0x144 ), vz = state_f( t.body, 0x148 );
    const float* fwd = reinterpret_cast< const float* >( t.transform + 0x60 );
    const float d = ( fwd[ 1 ] * vy + vx * fwd[ 0 ] ) + fwd[ 2 ] * vz;
    const float sign = _mm_comige_ss( _mm_set_ss( d ), _mm_set_ss( 0.0f ) ) ? 1.0f : -1.0f;
    state_put_f( driver, 0xA0, hull_sqrt( ( vx * vx + vy * vy ) + vz * vz ) * sign );
    const float* pos = reinterpret_cast< const float* >( t.transform + 0x80 );
    if ( t.count <= 0 || t.points[ 0 ] == nullptr )
    {
        ai_controls( driver, boat, t.animation, dt, 0.0f, 0.0f, 0, hooks.controls );
        return;
    }
    const std::uint8_t* first = t.points[ 0 ];
    float* target = reinterpret_cast< float* >( driver + 0xA8 );
    float direction[ 4 ];
    ai_path_follow( t.points, t.count, pos, target, direction );
    ai_avoid( state_f( driver, 0x98 ), t.body, pos, target, hooks.avoid );
    const float mph = ai_bits( 0x3EE4E26Du );   // 0x6F7654
    std::uint8_t boost = 0;
    const float top = ( state_f( t.owner, 0x470 ) * ( state_f( t.engine, 0xC ) * state_f( t.engine, 4 ) ) ) * mph;
    state_put_f( driver, 0xB8, ai_minss( state_f( driver, 0x9C ), top ) );
    if ( t.other )
    {
        const float dy = state_f( t.other, 0x84 ) - pos[ 1 ], dx = state_f( t.other, 0x80 ) - pos[ 0 ], dz = state_f( t.other, 0x88 ) - pos[ 2 ];
        const float il = 1.0f / hull_sqrt( ( dy * dy + dx * dx ) + dz * dz );
        const float dot = ( state_f( first, 0xA4 ) * ( il * dy ) + state_f( first, 0xA0 ) * ( il * dx ) ) + state_f( first, 0xA8 ) * ( il * dz );
        if ( dot > 0.0f )
        {
            boost = 1;
            state_put_f( driver, 0xB8, ( ( state_f( t.engine, 0xC ) * state_f( t.engine, 4 ) ) * state_f( t.engine, 0x28 ) ) * mph );
        }
    }
    std::int32_t mode;
    std::memcpy( &mode, driver + 0x80, 4 );
    if ( mode >= 2 && state_f( boat, 0x310 ) == 16.0f )
    {
        std::int32_t least = 0x7FFFFFFF;
        for ( std::int32_t k = 0; k < t.boat_count; ++k )
        {
            std::int32_t place;
            std::memcpy( &place, t.boats[ k ] + 0x4CC, 4 );
            least = least < place ? least : place;
        }
        std::int32_t mine;
        std::memcpy( &mine, boat + 0x4CC, 4 );
        if ( mine > least )
            boost = 1;
    }
    if ( t.boat_count == 0 && boat[ 0x467 ] && state_f( boat, 0x33C ) > 1.0f )
        boost = 1;
    float steer = 0.0f, throttle = 0.0f;
    ai_steer( t.body, t.solver, pos, fwd, target, state_f( driver, 0xA0 ), state_f( driver, 0xB8 ), dt, &steer, &throttle );
    ai_controls( driver, boat, t.animation, dt, steer, throttle, boost, hooks.controls );
}
