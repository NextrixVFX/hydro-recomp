#pragma once

#include <workspace/core/world/pfx.hxx>
#include <workspace/core/boat/tick.hxx>
#include <workspace/core/boat/fluid.hxx>
#include <workspace/core/boat/hull.hxx>

#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

// THE BOAT'S OWN PARTICLE EMITTERS - the two VuPfx emit nodes whose slot 1 is
// not the shared VuPfxEmitFountain_emit 0x55D030 (memory/particles.md):
//   VuPfxEmitBoatThrustQuadFountainInstance 0x4CD5B0 (here);
//   VuPfxEmitBoatHullInstance 0x4CE760 (here: pfx_boat_hull_emit).

// VuPfxEmitBoatThrustQuadFountainInstance's emit 0x4CD5B0 (thiscall node;
// float dt; retn 4), the part before the fountain: with a boat at node +0x28,
// the rate scale (+0x20) = maxss( 0x41D570( boat ), 0 ) - 0 for a negative or
// NaN answer - and then, when the node's params +0xC8 (a signed index) is >= 0
// and the byte of record [index] (8-byte records at boat +0x22C -> +0xF8) is
// 0, +0x20 = 0. Without a boat +0x20 is left as it is. `records` is that +0xF8
// array (null only when the boat is).
inline void pfx_boat_thrust_rate( pfx_fountain_node_t& node, const std::uint8_t* params, boat_view_t* boat, const std::uint8_t* records )
{
    if ( !boat )
        return;
    node.rate_scale = _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( boat_41D570( *boat ) ), _mm_setzero_ps( ) ) );
    std::int32_t index;
    std::memcpy( &index, params + 0xC8, 4 );
    if ( index >= 0 && records[ static_cast< std::size_t >( index ) * 8 ] == 0 )
        node.rate_scale = 0.0f;
}

// VuPfxBoatSpringConstraintInstance's slot 1 0x4CD980 (thiscall node; float
// dt; retn 4): the target and target velocity are the boat's (node +0x14):
// its transform's world translation (boat +0x50 -> +0x80 .. +0x88) and its
// rigid body's linear velocity (boat +0x21C -> +0x140 .. +0x148), both read
// once per call; all zero without a boat. Then per particle of the pattern's
// list world/pfx.hxx's pfx_spring_particle. `boat_position` / `boat_velocity`
// null for retail's null boat.
inline void pfx_tick_boat_spring( float* p, const float* params, const float* boat_position, const float* boat_velocity, float dt )
{
    const float zero[ 3 ] = { 0.0f, 0.0f, 0.0f };
    pfx_spring_particle( p, params, boat_position ? boat_position : zero, boat_velocity ? boat_velocity : zero, dt );
}

// VuPfxEmitBoatHullInstance's emit 0x4CE760 (thiscall node; float dt; retn 4;
// a 0x2BB8-byte frame) - THE HULL SPRAY. Node: +0xC the pattern instance, +0x10
// the params, +0x14 the boat, +0x18 the boat's VuFluidsObject (the fluid solver:
// [0] the VuFluidsMeshAsset, +0x44 the edge records), +0x1C the accumulator.
// What the boat gives it (pfx_hull_boat_t):
//   +0x50 -> +0x50 the transform (16 floats); +0x49C .. +0x4A8 an offset (x, y,
//   z and a fourth word, which lands in every particle's +0x24);
//   +0x21C the rigid body: +0x40 .. position, +0x140 .. linear, +0x150 ..
//   angular velocity (pfx_hull_body_t);
//   byte +0x46B and +0x238: when the byte is set and (+0x238) +0xA0 > 0 (a signed
//   count), the matrix is ((+0x238) +0x9C)[0] + 8 and the body that part's +0x208.
// Params: +0x30 / +0x34 life, +0x38 / +0x3C scale, +0x40 / +0x44 and +0x48 /
// +0x4C two blended words (particle +0x44 / +0x48), +0x50 / +0x54 the speed
// range, +0x58 the rate, +0x5C .. +0x5F the RGBA bytes, +0x60 the along-the-
// surface and +0x64 the surface-normal launch factors, +0x68 / +0x6C a blended
// word (particle +0x50), +0x70 the draw distance.
struct pfx_hull_body_t
{
    float position[ 3 ];   // body +0x40
    float linear[ 3 ];     // body +0x140
    float angular[ 3 ];    // body +0x150
};

struct pfx_hull_boat_t
{
    float transform[ 16 ];     // +0x50 -> +0x50
    float offset[ 4 ];         // +0x49C .. +0x4A8
    std::uint8_t part_flag;    // +0x46B
    std::int32_t part_count;   // +0x238 -> +0xA0
    float part_matrix[ 16 ];   // (+0x238 -> +0x9C)[0] + 8
    pfx_hull_body_t body;      // +0x21C
    pfx_hull_body_t part_body; // (+0x238 -> +0x9C)[0] -> +0x208
};

// The calls 0x4CE760 makes out of this file, as hooks:
//   prepare  = VuFluidSolver_prepare 0x524450 on the node's fluids with the
//              matrix (boat/fluid.hxx fluid_prepare - it fills solver.edges);
//   point    = VuWater's one-point query 0x5292D0 on g 0x759058 (world/query.hxx
//              water_point_query: 8 words out); false = not answered (counted);
//   allocate = the pattern instance's slot 4 (VuPfxPatternInstance::
//              allocateParticle 0x58B4E0); null = refused;
//   picked   = optional, told which sample each particle took (-1: none, sample
//              0's ends are used) - for the check.
struct pfx_hull_hooks_t
{
    void* user = nullptr;
    void ( *prepare )( void* user, const float* matrix ) = nullptr;
    bool ( *point )( void* user, const float* point, float* out ) = nullptr;
    pfx_quad_particle_t* ( *allocate )( void* user ) = nullptr;
    void ( *picked )( void* user, int sample ) = nullptr;
};

// One waterline sample, retail's 0x54-byte stack record (at most 128, at
// esp+0x1B0): the two edge records' first 32 bytes (crossing point + its w
// word, displacement + its w word), the triangle's world normal (its w word is
// a stack word retail never writes: 0 here), and length x relative speed.
struct pfx_hull_sample_t
{
    std::uint32_t a[ 4 ];
    std::uint32_t b[ 4 ];
    std::uint32_t a_disp[ 4 ];
    std::uint32_t b_disp[ 4 ];
    float normal[ 4 ];
    float weight;
};

// The loop's tallies, for the check (0x4CEF3F: the samples kept, their weight).
struct pfx_hull_tally_t
{
    int samples = 0;
    float weight = 0.0f;
    bool reached = false;
};

// 0x4CE760, in retail's order and operation order. Returns the number of point
// queries the hook did not answer (0 = every answer known). A particle's +0x14
// (the position's w) is written only when no sample was picked (sample 0's
// first end's w word); when one was, retail writes a stack word it never set
// (esp+0x11C): UNKNOWN, left as the allocator gave it.
inline int pfx_boat_hull_emit( float& accumulator, const std::uint8_t* params, const pfx_hull_boat_t* boat, fluid_solver_t& solver,
    pfx_rand_t& rand, const float* cameras, int camera_count, float dt, const pfx_hull_hooks_t& hooks, pfx_hull_tally_t* tally = nullptr )
{
    auto prm = [ & ]( int off ) { float v; std::memcpy( &v, params + off, 4 ); return v; };
    auto gt = []( float a, float b ) { return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0; };   // comiss; ja / jbe
    int unknown = 0;
    if ( !boat )
        return 0;
    // comiss FLT_EPSILON (0x6F7584), dt; jae -> return
    if ( _mm_comige_ss( _mm_set_ss( 1.1920929e-07f ), _mm_set_ss( dt ) ) )
        return 0;
    float t[ 16 ], m[ 16 ];
    hull_translation( t, boat->offset );   // 0x410AB0
    hull_multiply( boat->transform, m, t );   // VuMatrix_multiply 0x4078C0
    const pfx_hull_body_t* body = &boat->body;
    if ( boat->part_flag != 0 && boat->part_count > 0 )
    {
        std::memcpy( m, boat->part_matrix, 64 );
        body = &boat->part_body;
    }
    // Any viewport (g 0x759060: +4 count, 0x274-byte entries, position at
    // +0x28) within the draw distance of the matrix's translation.
    if ( camera_count <= 0 )
        return 0;
    const float draw = prm( 0x70 ) * prm( 0x70 );
    bool seen = false;
    for ( int k = 0; k < camera_count; ++k )
    {
        const float* c = cameras + 3 * k;
        const float dx = c[ 0 ] - m[ 12 ];
        const float dy = c[ 1 ] - m[ 13 ];
        const float dz = c[ 2 ] - m[ 14 ];
        if ( gt( draw, ( ( dx * dx ) + ( dy * dy ) ) + ( dz * dz ) ) )
            seen = true;
    }
    if ( !seen )
        return 0;
    float sum_weight = 0.0f, sum_length = 0.0f, total = 0.0f;
    int n = 0;
    hooks.prepare( hooks.user, m );
    const std::vector< uint32_t >& tris = solver.mesh->triangles;
    const int count = static_cast< int >( tris.size( ) / 15 );
    if ( count <= 0 )
        return 0;
    const fluid_edge_t* e = solver.edges.data( );
    const float* L = body->linear;
    const float* W = body->angular;
    const float* B = body->position;
    pfx_hull_sample_t samples[ 128 ];
    for ( int i = 0; i < count; ++i )
    {
        const uint32_t* r = tris.data( ) + 15 * i;
        float n0, n1, n2, c0, c1, c2;
        std::memcpy( &n0, r + 7, 4 );   // +0x1C .. +0x24 the local normal
        std::memcpy( &n1, r + 8, 4 );
        std::memcpy( &n2, r + 9, 4 );
        std::memcpy( &c0, r + 11, 4 );   // +0x2C .. +0x34 a local point
        std::memcpy( &c1, r + 12, 4 );
        std::memcpy( &c2, r + 13, 4 );
        if ( !gt( ( ( n0 * c0 ) + ( n1 * c1 ) ) + ( n2 * c2 ), 0.0f ) )
            continue;
        const float rx = ( ( ( ( m[ 0 ] * c0 ) + ( m[ 4 ] * c1 ) ) + ( m[ 8 ] * c2 ) ) + m[ 12 ] ) - B[ 0 ];
        const float ry = ( ( ( ( m[ 1 ] * c0 ) + ( m[ 5 ] * c1 ) ) + ( m[ 9 ] * c2 ) ) + m[ 13 ] ) - B[ 1 ];
        const float rz = ( ( ( ( m[ 2 ] * c0 ) + ( m[ 6 ] * c1 ) ) + ( m[ 10 ] * c2 ) ) + m[ 14 ] ) - B[ 2 ];
        const float wnx = ( ( m[ 0 ] * n0 ) + ( m[ 4 ] * n1 ) ) + ( m[ 8 ] * n2 );
        const float wny = ( ( m[ 1 ] * n0 ) + ( m[ 5 ] * n1 ) ) + ( m[ 9 ] * n2 );
        const float wnz = ( ( m[ 2 ] * n0 ) + ( m[ 6 ] * n1 ) ) + ( m[ 10 ] * n2 );
        const float vx = L[ 0 ] + ( ( W[ 1 ] * rz ) - ( W[ 2 ] * ry ) );
        const float vy = L[ 1 ] + ( ( W[ 2 ] * rx ) - ( W[ 0 ] * rz ) );
        const float vz = L[ 2 ] + ( ( W[ 0 ] * ry ) - ( W[ 1 ] * rx ) );
        if ( !gt( ( ( vy * wny ) + ( vx * wnx ) ) + ( vz * wnz ), 0.0f ) )
            continue;
        // +0x0C / +0x10 / +0x14: the triangle's three edges; +0x24 of an edge
        // record its crossing flag (a dword).
        const fluid_edge_t& e0 = e[ static_cast< std::int32_t >( r[ 3 ] ) ];
        const fluid_edge_t& e1 = e[ static_cast< std::int32_t >( r[ 4 ] ) ];
        const fluid_edge_t& e2 = e[ static_cast< std::int32_t >( r[ 5 ] ) ];
        if ( ( e2.submerged | e0.submerged | e1.submerged ) == 0u )
            continue;
        const fluid_edge_t* ea;
        const fluid_edge_t* eb;
        if ( e0.submerged != 0u )
        {
            ea = &e0;
            eb = e1.submerged != 0u ? &e1 : &e2;
        }
        else
        {
            ea = &e1;
            eb = &e2;
        }
        pfx_hull_sample_t s;
        std::memcpy( s.a, ea, 16 );
        std::memcpy( s.b, eb, 16 );
        std::memcpy( s.a_disp, reinterpret_cast< const std::uint8_t* >( ea ) + 16, 16 );
        std::memcpy( s.b_disp, reinterpret_cast< const std::uint8_t* >( eb ) + 16, 16 );
        const float* A = ea->pos;
        const float* Bp = eb->pos;
        const float mx = ( ( A[ 0 ] + Bp[ 0 ] ) * 0.5f ) - B[ 0 ];
        const float my = ( ( A[ 1 ] + Bp[ 1 ] ) * 0.5f ) - B[ 1 ];
        const float mz = ( ( A[ 2 ] + Bp[ 2 ] ) * 0.5f ) - B[ 2 ];
        const float ux = L[ 0 ] + ( ( W[ 1 ] * mz ) - ( W[ 2 ] * my ) );
        const float uy = L[ 1 ] + ( ( W[ 2 ] * mx ) - ( W[ 0 ] * mz ) );
        const float uz = L[ 2 ] + ( ( W[ 0 ] * my ) - ( W[ 1 ] * mx ) );
        const float dx = ux - ( ( ea->disp[ 0 ] + eb->disp[ 0 ] ) * 0.5f );
        const float dy = uy - ( ( ea->disp[ 1 ] + eb->disp[ 1 ] ) * 0.5f );
        const float dz = uz - ( ( ea->disp[ 2 ] + eb->disp[ 2 ] ) * 0.5f );
        const float speed = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( ( dy * dy ) + ( dx * dx ) ) + ( dz * dz ) ) ) );   // VuMath_sqrt 0x40B5A0
        const float ex = Bp[ 0 ] - A[ 0 ];
        const float ey = Bp[ 1 ] - A[ 1 ];
        const float ez = Bp[ 2 ] - A[ 2 ];
        const float length = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( ( ey * ey ) + ( ex * ex ) ) + ( ez * ez ) ) ) );
        const float weight = length * speed;
        s.normal[ 0 ] = wnx;
        s.normal[ 1 ] = wny;
        s.normal[ 2 ] = wnz;
        s.normal[ 3 ] = 0.0f;   // esp+0x4C: never written - UNKNOWN (never read back either)
        s.weight = weight;
        if ( n < 128 )
        {
            samples[ n ] = s;
            total = total + weight;
            ++n;
        }
        sum_weight = weight + sum_weight;
        sum_length = length + sum_length;
    }
    if ( tally )
    {
        tally->samples = n;
        tally->weight = total;
        tally->reached = true;
    }
    if ( total == 0.0f )   // ucomiss; lahf; test ah, 44h; jnp: equal and ordered
        return 0;
    const float average = sum_weight / sum_length;
    const float lo = prm( 0x50 );
    if ( gt( lo, average ) )
        return 0;
    const float f = ( average - lo ) / ( prm( 0x54 ) - lo );
    const float clamped = _mm_cvtss_f32( _mm_max_ss( _mm_setzero_ps( ), _mm_min_ss( _mm_set_ss( 1.0f ), _mm_set_ss( f ) ) ) );
    const float rate = prm( 0x58 ) * clamped;
    accumulator = ( rate * dt ) + accumulator;
    if ( !gt( accumulator, 0.0f ) )
        return 0;
    auto blend = [ & ]( int lo_off, int hi_off ) {
        const float r = rand.next( );
        return ( ( 1.0f - r ) * prm( lo_off ) ) + ( prm( hi_off ) * r );
    };
    do
    {
        pfx_quad_particle_t* p = hooks.allocate( hooks.user );
        if ( p != nullptr )
        {
            p->set( 0x3C, accumulator / rate );
            p->set( 0x40, blend( 0x30, 0x34 ) );
            for ( int c = 0; c < 4; ++c )
                p->set( 0x28 + 4 * c, static_cast< float >( params[ 0x5C + c ] ) * 0.0039215689f );
            p->set( 0x38, blend( 0x38, 0x3C ) );
            // The sample: a draw over the kept samples' weights.
            float target = rand.next( ) * total;
            int pick = 0;
            while ( pick < n && !gt( samples[ pick ].weight, target ) )
            {
                target = target - samples[ pick ].weight;
                ++pick;
            }
            float P[ 3 ], V[ 3 ], N[ 3 ];
            if ( pick < n )   // 0x4CF868: blend the two ends
            {
                const pfx_hull_sample_t& s = samples[ pick ];
                const float u = target / s.weight;
                const float om = 1.0f - u;
                for ( int c = 0; c < 3; ++c )
                {
                    float a, b, da, db;
                    std::memcpy( &a, &s.a[ c ], 4 );
                    std::memcpy( &b, &s.b[ c ], 4 );
                    std::memcpy( &da, &s.a_disp[ c ], 4 );
                    std::memcpy( &db, &s.b_disp[ c ], 4 );
                    P[ c ] = ( a * om ) + ( b * u );
                    V[ c ] = ( om * da ) + ( db * u );
                    N[ c ] = s.normal[ c ];
                }
            }
            else   // 0x4CF29A: sample 0's first end as it is
            {
                const pfx_hull_sample_t& s = samples[ 0 ];
                std::memcpy( P, s.a, 12 );
                std::memcpy( V, s.a_disp, 12 );
                std::memcpy( N, s.normal, 12 );
                p->w[ 0x14 / 4 ] = s.a[ 3 ];
            }
            if ( hooks.picked )
                hooks.picked( hooks.user, pick < n ? pick : -1 );
            p->set( 0x08, P[ 0 ] );
            p->set( 0x0C, P[ 1 ] );
            p->set( 0x10, P[ 2 ] );
            const float rx = P[ 0 ] - B[ 0 ];
            const float ry = P[ 1 ] - B[ 1 ];
            const float rz = P[ 2 ] - B[ 2 ];
            const float bx = L[ 0 ] + ( ( rz * W[ 1 ] ) - ( ry * W[ 2 ] ) );
            const float by = L[ 1 ] + ( ( rx * W[ 2 ] ) - ( rz * W[ 0 ] ) );
            const float bz = L[ 2 ] + ( ( ry * W[ 0 ] ) - ( rx * W[ 1 ] ) );
            // The point's w word is +0x14's (UNKNOWN on the blended path: 0
            // here); the query reads x, y, z.
            float point[ 4 ] = { P[ 0 ], P[ 1 ], P[ 2 ], 0.0f };
            if ( pick >= n )
                std::memcpy( &point[ 3 ], &samples[ 0 ].a[ 3 ], 4 );
            float out[ 8 ] = { };
            if ( !hooks.point( hooks.user, point, out ) )
                ++unknown;
            // The surface normal from the query's gradient (+0xC / +0x10).
            const float inv = 1.0f / _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( ( out[ 3 ] * out[ 3 ] ) + ( out[ 4 ] * out[ 4 ] ) ) + 1.0f ) ) );
            const float sx = inv * out[ 3 ];
            const float sy = inv * out[ 4 ];
            const float sz = inv;
            const float d = ( ( N[ 1 ] * sy ) + ( N[ 0 ] * sx ) ) + ( N[ 2 ] * sz );
            const float tx = N[ 0 ] - ( sx * d );
            const float ty = N[ 1 ] - ( sy * d );
            const float tz = N[ 2 ] - ( sz * d );
            const float it = 1.0f / _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( ( ty * ty ) + ( tx * tx ) ) + ( tz * tz ) ) ) );
            const float along = prm( 0x60 );
            const float up = prm( 0x64 );
            const float Dx = ( ( tx * it ) * along ) + ( sx * up );
            const float Dy = ( ( ty * it ) * along ) + ( sy * up );
            const float Dz = ( ( tz * it ) * along ) + ( sz * up );
            // VuMath_sqrt twice: the square root of the body point's speed.
            const float s = _mm_cvtss_f32( _mm_sqrt_ss( _mm_sqrt_ss( _mm_set_ss( ( ( by * by ) + ( bx * bx ) ) + ( bz * bz ) ) ) ) );
            p->set( 0x18, ( V[ 0 ] + ( s * Dx ) ) + bx );
            p->set( 0x1C, ( V[ 1 ] + ( Dy * s ) ) + by );
            p->set( 0x20, ( V[ 2 ] + ( Dz * s ) ) + bz );
            std::memcpy( &p->w[ 0x24 / 4 ], &boat->offset[ 3 ], 4 );   // esp+0xEC: still boat +0x4A8 from the matrix set-up
            p->set( 0x44, blend( 0x40, 0x44 ) );
            p->set( 0x48, blend( 0x48, 0x4C ) );
            p->set( 0x50, blend( 0x68, 0x6C ) );
        }
        accumulator = accumulator - 1.0f;
    } while ( gt( accumulator, 0.0f ) );
    return unknown;
}

// 0x4CD5B0 whole: the rate, then VuPfxEmitFountain_emit 0x55D030 with dt.
inline void pfx_boat_thrust_emit( pfx_fountain_node_t& node, const std::uint8_t* params, boat_view_t* boat, const std::uint8_t* records,
    pfx_rand_t& rand, const float* cameras, int camera_count, const float* system_position, const float* system_velocity, float dt,
    const pfx_fountain_hooks_t& hooks )
{
    pfx_boat_thrust_rate( node, params, boat, records );
    pfx_fountain_emit( node, params, rand, cameras, camera_count, system_position, system_velocity, dt, hooks );
}
