#pragma once

#include <impl/engine/engine.hxx>
#include <workspace/core/assets/archive.hxx>
#include <workspace/core/assets/reader.hxx>
#include <workspace/core/assets/tree.hxx>
#include <workspace/utility/random.hxx>
#include <workspace/utility/logger.hxx>
#include <workspace/utility/math.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <xmmintrin.h>

// THE PFX TICK NODES' PER-FRAME SLOT 1, transliterated one particle at a time
// (each retail function walks the owning pattern instance's list, node +0xC
// -> +0x20, next at particle +0, and applies this body to every particle).
// The particle's words are addressed by retail's byte offsets: +0x18 .. +0x20
// velocity, +0x34 alpha, +0x38 scale, +0x3C age, +0x4C world scale z; the
// node's params by theirs, from +0x30.
//
// NOT YET WIRED INTO c_pfx::simulate below, whose own versions predate these
// and DIFFER: its dampen keeps 1 - Amount * dt with no clamp, where retail
// clamps Amount * dt to 1.0 with minss first. Switching simulate over moves
// what the bench draws and needs its own measured still change.
struct pfx_tick_particle_t
{
    float velocity[ 3 ];   // +0x18
    float alpha;           // +0x34
    float scale;           // +0x38
    float age;             // +0x3C
    float world_scale_z;   // +0x4C
};

// VuPfxTickDampenVelocityInstance @0x58e4b0: k = minss( Amount * dt, 1 ) once
// per call, then past the start delay every velocity component times 1 - k.
inline void pfx_tick_dampen( pfx_tick_particle_t& p, const float* params, float dt )
{
    const float k = _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( params[ 0 ] * dt ), _mm_set_ss( 1.0f ) ) );
    if ( !_mm_comigt_ss( _mm_set_ss( p.age ), _mm_set_ss( params[ 1 ] ) ) )
        return;
    const float keep = 1.0f - k;
    p.velocity[ 0 ] = p.velocity[ 0 ] * keep;
    p.velocity[ 1 ] = keep * p.velocity[ 1 ];
    p.velocity[ 2 ] = keep * p.velocity[ 2 ];
}

// VuPfxTickLinearAccelerationInstance @0x58e810: past the delay (+0x40), the
// velocity gains the acceleration (+0x30 .. +0x38) times dt.
inline void pfx_tick_accelerate( pfx_tick_particle_t& p, const float* params, float dt )
{
    if ( !_mm_comigt_ss( _mm_set_ss( p.age ), _mm_set_ss( params[ 4 ] ) ) )
        return;
    p.velocity[ 0 ] = p.velocity[ 0 ] + params[ 0 ] * dt;
    p.velocity[ 1 ] = p.velocity[ 1 ] + params[ 1 ] * dt;
    p.velocity[ 2 ] = p.velocity[ 2 ] + params[ 2 ] * dt;
}

// The delay test VuPfxTickScale / TickAlpha / TickWorldScaleZ share: the rate
// (+0x34) times dt once the age has passed the delay (+0x30) - comiss
// ( delay - age ), 0; jb - else exactly 0, which is still ADDED (so a -0.0
// member becomes +0.0).
inline float pfx_tick_rate( const pfx_tick_particle_t& p, const float* params, float dt )
{
    return _mm_comilt_ss( _mm_set_ss( params[ 0 ] - p.age ), _mm_setzero_ps( ) ) || std::isnan( params[ 0 ] - p.age )
        ? params[ 1 ] * dt
        : 0.0f;
}

inline void pfx_tick_scale( pfx_tick_particle_t& p, const float* params, float dt )        // @0x590da0
{
    p.scale = p.scale + pfx_tick_rate( p, params, dt );
}

inline void pfx_tick_alpha( pfx_tick_particle_t& p, const float* params, float dt )        // @0x591360
{
    p.alpha = p.alpha + pfx_tick_rate( p, params, dt );
}

inline void pfx_tick_world_scale_z( pfx_tick_particle_t& p, const float* params, float dt ) // @0x590b40
{
    p.world_scale_z = pfx_tick_rate( p, params, dt ) + p.world_scale_z;
}

// VuPfxTickAlphaInOutInstance @0x5910e0: the fade-in rate (+0x34) applies
// WHILE the age has not passed +0x30 (the opposite sense to the others), the
// fade-out rate (+0x3C) once it has passed +0x38; both added in that order.
inline void pfx_tick_alpha_in_out( pfx_tick_particle_t& p, const float* params, float dt )
{
    const float in_gap = params[ 0 ] - p.age;
    const bool before = !( _mm_comilt_ss( _mm_set_ss( in_gap ), _mm_setzero_ps( ) ) || std::isnan( in_gap ) );
    const float alpha = p.alpha + ( before ? params[ 1 ] * dt : 0.0f );
    const float out_gap = params[ 2 ] - p.age;
    const bool after = _mm_comilt_ss( _mm_set_ss( out_gap ), _mm_setzero_ps( ) ) || std::isnan( out_gap );
    p.alpha = alpha + ( after ? params[ 3 ] * dt : 0.0f );
}

// The spring body both spring constraints run per particle (particle words by
// retail offset / 4: +0x08 position, +0x18 velocity, +0x3C age): past the start
// delay (age > params +0x38, comiss / jbe) each velocity component
//   v += ( ( target - pos ) * k - ( v - target_velocity ) * damping ) * dt
// with k = params +0x30, damping = +0x34; x is stored as v + term, y and z as
// term + v (retail's addss operand orders; the same bits).
inline void pfx_spring_particle( float* p, const float* params, const float* target, const float* target_velocity, float dt )
{
    if ( !_mm_comigt_ss( _mm_set_ss( p[ 0x3C / 4 ] ), _mm_set_ss( params[ 2 ] ) ) )
        return;
    const float k = params[ 0 ];
    const float damping = params[ 1 ];
    float term[ 3 ];
    for ( int i = 0; i < 3; ++i )
        term[ i ] = ( ( ( target[ i ] - p[ 2 + i ] ) * k ) - ( ( p[ 6 + i ] - target_velocity[ i ] ) * damping ) ) * dt;
    p[ 6 ] = p[ 6 ] + term[ 0 ];
    p[ 7 ] = term[ 1 ] + p[ 7 ];
    p[ 8 ] = term[ 2 ] + p[ 8 ];
}

// VuPfxSpringConstraintInstance's slot 1 @0x58e1c0 (thiscall node; float dt;
// retn 4), per particle of the pattern's list (node +0xC -> +0x20, next +0):
// the target is the params' offset (+0x3C .. +0x44) plus the node's own
// (+0x14 .. +0x1C), summed per particle in that order; there is no target
// velocity - retail's ( v * damping ) is ( v - 0 ) * damping here, the same
// bits. `params` from +0x30.
inline void pfx_tick_spring( float* p, const float* params, const float* node_offset, float dt )
{
    const float target[ 3 ] = { params[ 3 ] + node_offset[ 0 ], params[ 4 ] + node_offset[ 1 ], params[ 5 ] + node_offset[ 2 ] };
    const float still[ 3 ] = { 0.0f, 0.0f, 0.0f };
    pfx_spring_particle( p, params, target, still, dt );
}

// VuPfxQuadPatternInstance's tick @0x591c30 over an index model of retail's
// particle lists. A particle is retail's 30 words from +0 (+0 next, +4 prev as
// indices, -1 null; +0x08 pos, +0x18 velocity, +0x34 alpha, +0x38 scale,
// +0x3C age, +0x40 life, +0x44 rotation, +0x48 spin, +0x4C world z). The
// pattern: +0x20 / +0x24 head / tail, +0x28 count, the AABB at +0x2C .. +0x44
// (min x y z at +0x2C, max x y z at +0x3C; the pad words +0x38 / +0x48 come
// from stack words retail never writes: UNKNOWN, not reproduced). The global free pool
// (g_vupfximpl 0x75904c, +8: head +0x38, tail +0x34, count +0x3C) takes the
// retired, pushed at its head.
struct pfx_quad_particle_t
{
    std::uint32_t w[ 30 ]{};
    float f( int offset ) const { float v; std::memcpy( &v, &w[ offset / 4 ], 4 ); return v; }
    void set( int offset, float v ) { std::memcpy( &w[ offset / 4 ], &v, 4 ); }
};

struct pfx_quad_pattern_t
{
    std::vector< pfx_quad_particle_t >* particles = nullptr;
    int head = -1;
    int tail = -1;
    int count = 0;
    float aabb[ 6 ]{};   // min x, y, z, max x, y, z
};

struct pfx_pool_t
{
    int head = -1;   // +0x38
    int tail = -1;   // +0x34
    int count = 0;   // +0x3C
};

// system: clock +0xA4, instance scale +0xA8, matrix +0x1C (16 floats);
// params: Space +0x34, Start Delay +0x38. `children` ticks the child nodes
// (each node's slot 1 with dt - Vu virtuals, a hook).
// The geom pattern's tick @0x58fea0 is the same body with three differences,
// all read from its disassembly: the rotation is three angles (+0x44 ..
// +0x4C) advanced by an angular velocity (+0x54 .. +0x5C); world z is NOT a
// retirement test; and the AABB radius is particle scale * instance scale *
// the half-diagonal of the model's box - params +0x6C's asset +0x5C .. +0x74,
// or the CRT-initialised static at 0x7593f0 when there is none - computed
// sqrt( (hy*hy + hx*hx) + hz*hz ).
inline void pfx_pattern_tick( pfx_quad_pattern_t& p, pfx_pool_t& pool, float clock, float instance_scale,
    const float* system_matrix, int space, float start_delay, float dt, void ( *children )( void*, float ), void* user,
    const float* geom_box )
{
    std::vector< pfx_quad_particle_t >& v = *p.particles;
    auto link = [ & ]( int n, int slot ) { return static_cast< int >( v[ n ].w[ slot ] ); };
    if ( _mm_comigt_ss( _mm_set_ss( clock ), _mm_set_ss( start_delay ) ) )
    {
        for ( int n = p.head; n >= 0; n = link( n, 0 ) )
        {
            pfx_quad_particle_t& q = v[ n ];
            const float vy = q.f( 0x1C ) * dt;
            const float vz = q.f( 0x20 ) * dt;
            q.set( 0x08, ( dt * q.f( 0x18 ) ) + q.f( 0x08 ) );
            q.set( 0x0C, q.f( 0x0C ) + vy );
            q.set( 0x10, q.f( 0x10 ) + vz );
            q.set( 0x3C, dt + q.f( 0x3C ) );
            if ( geom_box == nullptr )
            {
                q.set( 0x44, ( q.f( 0x48 ) * dt ) + q.f( 0x44 ) );
            }
            else
            {
                q.set( 0x44, ( dt * q.f( 0x54 ) ) + q.f( 0x44 ) );
                q.set( 0x48, ( q.f( 0x58 ) * dt ) + q.f( 0x48 ) );
                q.set( 0x4C, ( q.f( 0x5C ) * dt ) + q.f( 0x4C ) );
            }
        }
        children( user, dt );
        for ( int n = p.head; n >= 0; )
        {
            const int next = link( n, 0 );
            pfx_quad_particle_t& q = v[ n ];
            auto below = []( float x ) { return !_mm_comige_ss( _mm_set_ss( x ), _mm_setzero_ps( ) ); };   // jb: < 0 or NaN
            const bool dead = below( q.f( 0x40 ) - q.f( 0x3C ) ) || below( q.f( 0x34 ) ) || below( q.f( 0x38 ) ) ||
                              ( geom_box == nullptr && below( q.f( 0x4C ) ) );
            if ( dead )
            {
                const int prev = link( n, 1 );
                if ( n == p.head )
                    p.head = next;
                if ( n == p.tail )
                    p.tail = prev;
                if ( next >= 0 )
                    v[ next ].w[ 1 ] = static_cast< std::uint32_t >( prev );
                if ( prev >= 0 )
                    v[ prev ].w[ 0 ] = static_cast< std::uint32_t >( next );
                q.w[ 0 ] = 0xFFFFFFFFu;
                --p.count;
                // push on the pool's head; retail also writes the old head's next (+0) to this one - a
                // node outside this model when it is not one of these particles
                q.w[ 1 ] = static_cast< std::uint32_t >( pool.head );
                if ( pool.head >= 0 && pool.head < static_cast< int >( v.size( ) ) )
                    v[ pool.head ].w[ 0 ] = static_cast< std::uint32_t >( n );
                if ( pool.head == -1 )
                    pool.tail = n;
                ++pool.count;
                pool.head = n;
            }
            n = next;
        }
    }
    if ( p.count == 0 )
    {
        for ( float& a : p.aabb )
            a = 0.0f;
        return;
    }
    static const float identity[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };   // 0x7593b0
    const float* m = space == 1 ? system_matrix : identity;
    const float big = 3.4028235e38f;
    float lo[ 3 ] = { big, big, big };
    float hi[ 3 ] = { -big, -big, -big };
    auto mn = []( float a, float b ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); };
    auto mx = []( float a, float b ) { return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); };
    float half_diagonal = 0.0f;
    if ( geom_box != nullptr )
    {
        const float hy = ( geom_box[ 4 ] - geom_box[ 1 ] ) * 0.5f;
        const float hx = ( geom_box[ 3 ] - geom_box[ 0 ] ) * 0.5f;
        const float hz = ( geom_box[ 5 ] - geom_box[ 2 ] ) * 0.5f;
        half_diagonal = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( ( hy * hy ) + ( hx * hx ) ) + ( hz * hz ) ) ) );
    }
    for ( int n = p.head; n >= 0; n = link( n, 0 ) )
    {
        const pfx_quad_particle_t& q = v[ n ];
        const float px = q.f( 0x08 );
        const float py = q.f( 0x0C );
        const float pz = q.f( 0x10 );
        const float x = ( ( ( m[ 0 ] * px ) + ( m[ 4 ] * py ) ) + ( m[ 8 ] * pz ) ) + m[ 12 ];
        const float y = ( ( ( m[ 1 ] * px ) + ( m[ 5 ] * py ) ) + ( m[ 9 ] * pz ) ) + m[ 13 ];
        const float z = ( ( ( m[ 2 ] * px ) + ( m[ 6 ] * py ) ) + ( pz * m[ 10 ] ) ) + m[ 14 ];
        const float r = geom_box == nullptr ? ( instance_scale * 0.5f ) * q.f( 0x38 )
                                            : ( q.f( 0x38 ) * instance_scale ) * half_diagonal;
        lo[ 0 ] = mn( lo[ 0 ], x - r );
        lo[ 1 ] = mn( lo[ 1 ], y - r );
        lo[ 2 ] = mn( lo[ 2 ], z - r );
        hi[ 0 ] = mx( hi[ 0 ], x + r );
        hi[ 1 ] = mx( hi[ 1 ], y + r );
        hi[ 2 ] = mx( hi[ 2 ], z + r );
    }
    p.aabb[ 0 ] = lo[ 0 ];
    p.aabb[ 1 ] = lo[ 1 ];
    p.aabb[ 2 ] = lo[ 2 ];
    p.aabb[ 3 ] = hi[ 0 ];
    p.aabb[ 4 ] = hi[ 1 ];
    p.aabb[ 5 ] = hi[ 2 ];
}

// The global VuRand the fountains draw from (0x736810 the Park-Miller state,
// 0x736814 the previous output, 0x736818 the 32-entry Bays-Durham table),
// inline in 0x55d030 at every draw: state = 16807*s - 0x7FFFFFFF*(s / 127773)
// in wrapping 32-bit arithmetic, + 0x7FFFFFFF when negative; the table slot is
// previous / 2^26 (a truncating signed divide), the previous takes the slot's
// old value and the slot takes the new state; the float is
// minss( (float)previous * 4.656613e-10, 0.99999988 ).
struct pfx_rand_t
{
    std::int32_t seed = 0;
    std::int32_t previous = 0;
    std::int32_t table[ 32 ]{};

    float next( )
    {
        const std::int32_t s = seed;
        std::int32_t v = static_cast< std::int32_t >( 16807u * static_cast< std::uint32_t >( s ) -
                                                     0x7FFFFFFFu * static_cast< std::uint32_t >( s / 127773 ) );
        if ( v < 0 )
            v += 0x7FFFFFFF;
        seed = v;
        const int slot = previous / 0x4000000;
        previous = table[ slot ];
        table[ slot ] = v;
        return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( static_cast< float >( previous ) * 4.6566129e-10f ),
            _mm_set_ss( 0.99999988f ) ) );
    }
};

// VuPfxEmitFountain_emit @0x55d030, the shared slot 1 of the quad /
// directional / geom fountains. node: +0x14 alive count, +0x18 accumulator,
// +0x1C count scale, +0x20 rate scale, +0x24 alpha scale; params by retail
// offset (`param( off )`): +0x30 Rate, +0x34 Max Particle Count, +0x38 / +0x3C
// life, +0x40 / +0x44 the two RGBA bytes, +0x48 / +0x4C scale, +0x50 .. /
// +0x60 .. velocity, +0x70 .. / +0x80 .. position (each a min / max pair the
// draw blends as (1 - r) * min + max * r), +0x90 the water snap flag, +0x94
// Draw Distance. Allocation (the pattern's slot 4), the matrix (its slot 5),
// the node's own slot 2 and the water snap are hooks. The snap (0x55D99B):
// after slot 2, when the +0x90 byte is set and g 0x759058 (VuWater) is not
// null, 0x529140 on the particle's +8 position and its +0x10 (z) = that
// answer's +0x20 (world/query.hxx water_point_height) - `snap` is told the
// particle only when the byte is set and decides on the VuWater (null hook:
// nothing happens).
struct pfx_fountain_node_t
{
    int alive = 0;
    float accumulator = 0.0f;
    float count_scale = 1.0f;
    float rate_scale = 1.0f;
    float alpha_scale = 1.0f;
};

struct pfx_fountain_hooks_t
{
    void* user = nullptr;
    pfx_quad_particle_t* ( *allocate )( void* user ) = nullptr;
    const float* ( *matrix )( void* user ) = nullptr;
    void ( *init )( void* user, pfx_quad_particle_t& p ) = nullptr;
    void ( *snap )( void* user, pfx_quad_particle_t& p ) = nullptr;
};

inline void pfx_fountain_emit( pfx_fountain_node_t& node, const std::uint8_t* params, pfx_rand_t& rand,
    const float* cameras, int camera_count, const float* system_position, const float* system_velocity, float dt,
    const pfx_fountain_hooks_t& hooks )
{
    auto param = [ & ]( int off ) { float v; std::memcpy( &v, params + off, 4 ); return v; };
    std::int32_t max_count;
    std::memcpy( &max_count, params + 0x34, 4 );
    // fld; fistp: round to nearest even
    const int limit = _mm_cvtsd_si32( _mm_set_sd( static_cast< double >( static_cast< float >( max_count ) * node.count_scale ) ) );
    const float rate = node.rate_scale * param( 0x30 );
    if ( max_count > 0 && node.alive >= limit )
        return;
    node.accumulator = ( rate * dt ) + node.accumulator;
    if ( !_mm_comigt_ss( _mm_set_ss( node.accumulator ), _mm_setzero_ps( ) ) )
        return;
    const float draw = param( 0x94 ) * param( 0x94 );
    do
    {
        bool seen = false;
        for ( int k = 0; k < camera_count; ++k )
        {
            const float* c = cameras + 3 * k;
            const float dx = c[ 0 ] - system_position[ 0 ];
            const float dy = c[ 1 ] - system_position[ 1 ];
            const float dz = c[ 2 ] - system_position[ 2 ];
            if ( _mm_comigt_ss( _mm_set_ss( draw ), _mm_set_ss( ( ( dx * dx ) + ( dy * dy ) ) + ( dz * dz ) ) ) )
                seen = true;
        }
        pfx_quad_particle_t* p = seen ? hooks.allocate( hooks.user ) : nullptr;
        if ( p != nullptr )
        {
            auto blend = [ & ]( int lo, int hi ) {
                const float r = rand.next( );
                return ( ( 1.0f - r ) * param( lo ) ) + ( param( hi ) * r );
            };
            p->set( 0x3C, node.accumulator / rate );
            p->set( 0x40, blend( 0x38, 0x3C ) );
            const float r = rand.next( );
            const float k = 0.0039215689f;
            float rgba[ 4 ];
            for ( int c = 0; c < 4; ++c )
                rgba[ c ] = ( ( static_cast< float >( params[ 0x40 + c ] ) * k ) * ( 1.0f - r ) ) +
                            ( ( static_cast< float >( params[ 0x44 + c ] ) * k ) * r );
            for ( int c = 0; c < 4; ++c )
                p->set( 0x28 + 4 * c, rgba[ c ] );
            p->set( 0x34, node.alpha_scale * p->f( 0x34 ) );
            p->set( 0x38, blend( 0x48, 0x4C ) );
            p->set( 0x08, blend( 0x70, 0x80 ) );
            p->set( 0x0C, blend( 0x74, 0x84 ) );
            p->set( 0x10, blend( 0x78, 0x88 ) );
            p->set( 0x18, blend( 0x50, 0x60 ) );
            p->set( 0x1C, blend( 0x54, 0x64 ) );
            p->set( 0x20, blend( 0x58, 0x68 ) );
            const float* m = hooks.matrix( hooks.user );
            const float px = p->f( 0x08 );
            const float py = p->f( 0x0C );
            const float pz = p->f( 0x10 );
            p->set( 0x08, ( ( ( m[ 0 ] * px ) + ( py * m[ 4 ] ) ) + ( pz * m[ 8 ] ) ) + m[ 12 ] );
            p->set( 0x0C, ( ( ( px * m[ 1 ] ) + ( py * m[ 5 ] ) ) + ( pz * m[ 9 ] ) ) + m[ 13 ] );
            p->set( 0x10, ( ( ( m[ 2 ] * px ) + ( m[ 6 ] * py ) ) + ( pz * m[ 10 ] ) ) + m[ 14 ] );
            const float vx = p->f( 0x18 );
            const float vy = p->f( 0x1C );
            const float vz = p->f( 0x20 );
            p->set( 0x18, system_velocity[ 0 ] + ( ( ( m[ 0 ] * vx ) + ( vy * m[ 4 ] ) ) + ( vz * m[ 8 ] ) ) );
            p->set( 0x1C, system_velocity[ 1 ] + ( ( ( vx * m[ 1 ] ) + ( vy * m[ 5 ] ) ) + ( vz * m[ 9 ] ) ) );
            p->set( 0x20, system_velocity[ 2 ] + ( ( ( m[ 2 ] * vx ) + ( m[ 6 ] * vy ) ) + ( vz * m[ 10 ] ) ) );
            hooks.init( hooks.user, *p );
            if ( params[ 0x90 ] != 0u && hooks.snap != nullptr )
                hooks.snap( hooks.user, *p );
            ++node.alive;
            if ( limit > 0 && node.alive >= limit )
                return;
        }
        node.accumulator = node.accumulator - 1.0f;
    } while ( _mm_comigt_ss( _mm_set_ss( node.accumulator ), _mm_setzero_ps( ) ) );
}

inline void pfx_quad_pattern_tick( pfx_quad_pattern_t& p, pfx_pool_t& pool, float clock, float instance_scale,
    const float* system_matrix, int space, float start_delay, float dt, void ( *children )( void*, float ), void* user )
{
    pfx_pattern_tick( p, pool, clock, instance_scale, system_matrix, space, start_delay, dt, children, user, nullptr );
}
// The old c_pfx (VuStaticPfxEntity's stand-in simulation and draw) is gone: the world's pfx is world/effects.hxx on the
// verified runtime (pfxsim.hxx / pfxload.hxx / pfxmgr.hxx). Its last version is in git history (world batch 11).
