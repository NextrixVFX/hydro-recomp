#pragma once

#include <workspace/core/world/query.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// The VuFluidSolver's prepare pass, recreated from retail's disassembly
// (memory/audit.md "Batch 9b"; scratchpad dis_fp.txt): VuFluidSolver_prepare
// @0x524450 (thiscall; transform*; retn 4) over the query ctx ctor 0x413500,
// the transformed-aabb grow 0x5175E0 with its corner list 0x5177A0, VuMin
// 0x403260 / VuMax 0x403280 (minss / maxss), VuVector_abs 0x4089C0,
// VuMath_sqrt 0x40B5A0, and the water query (query.hxx).
//
// It places the fluids mesh's vertices in the world, asks the water for each
// one's height and wave displacement, flags the submerged ones and clips
// every edge that crosses the waterline.

// VuMin 0x403260: minss( a, b ) - a when a < b, else b (NaN either side: b).
inline float fluid_min( float a, float b )
{
    return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) );
}

// VuMax 0x403280: maxss( a, b ) - a when a > b, else b.
inline float fluid_max( float a, float b )
{
    return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) );
}

// The VuFluidsMeshAsset fields the solver reads: +0x58 / +0x68 the local aabb
// min / max (x, y, z, w), +0x78 / +0x7C the vertices (16-byte x, y, z, w) and
// their count, +0x84 / +0x88 the edges (two u32 vertex indices) and their
// count.
struct fluids_mesh_t
{
    float aabb_min[ 4 ];
    float aabb_max[ 4 ];
    std::vector< float > verts;   // 4 floats per vertex
    std::vector< uint32_t > edges;   // 2 per edge
    // +0x90 / +0x94 the triangles (60-byte records: +0 / +4 / +8 vertex
    // indices, +12 / +16 / +20 edge indices - edge 0 joins vertices 0-1, 1
    // joins 1-2, 2 joins 2-0 as solve uses them -, +24 the area, +28 .. +36
    // the local normal; +40 .. +59 unread) and +0x9C the total area.
    std::vector< uint32_t > triangles;   // 15 words per triangle
    float total_area;
};

// An edge's waterline record (the solver's +0x44 array, 40-byte stride): the
// crossing point, +12 stale, the interpolated displacement, +28 stale, the
// interpolated height, +36 the crossing flag (the two ends' submerged flags
// XORed).
using fluid_edge_t = water_vertex_t;

// The VuFluidSolver fields prepare reads and writes: +0 the mesh (null:
// prepare does nothing), +0x30 / +0x34 the waves excluded from the query (the
// boat's own), +0x38 the vertex records, +0x44 the edge records, +0x50 the
// "any vertex submerged" byte, +0x5C the least depth (z - water height).
struct fluid_solver_t
{
    const fluids_mesh_t* mesh;
    std::vector< int > exclude;
    std::vector< water_vertex_t > vertices;
    std::vector< fluid_edge_t > edges;
    bool any_submerged;
    float min_depth;
    // Read by solve: +4 the mesh's offset in the body, +20 the drag factor,
    // +24 the buoyancy factor, +28 the velocity scale, +32 / +36 / +40 the
    // per-axis drag factors, +256 the least normal drag z.
    float offset[ 3 ];
    float drag;
    float buoyancy;
    float velocity_scale;
    float axis_drag[ 3 ];
    float floor_z;
    // Written by solve: +84 the submerged volume, +88 the submerged area, and
    // the vectors +96 .. +255 (acc[ ( offset - 96 ) / 4 ]): +96 the force,
    // +112 the torque, +128 / +144 buoyancy force / torque, +160 / +176 drag
    // force / torque, +192 the planing force, +208 zero, +224 the water
    // normal, +240 the mean flow. Every w lane is solve's stale stack.
    float volume;
    float area;
    float acc[ 40 ];
};

// The VuRigidBody fields solve reads: +16 the btTransform (basis rows, then
// the origin at +48), +320 / +336 the linear / angular velocity, +352 the
// inverse mass.
struct fluid_body_t
{
    float transform[ 16 ];
    float linear[ 3 ];
    float angular[ 3 ];
    float inverse_mass;
};

// g_vudynamicsimpl's vt+28 (thiscall(out*), retn 4) - the gravity: a Vu
// virtual, a hook. False when absent.
using fluid_gravity_fn = bool ( * )( void* user, float* out );

// 0x5175E0 (thiscall aabb; local aabb*, transform*; retn 8): the eight
// corners from 0x5177A0 - x varies fastest (min, max), then y, then z - each
// put through the transform (row-vector: x' = ((m0*x + y*m4) + m8*z) + m12,
// y' = ((m1*x + m5*y) + m9*z) + m13, z' = ((x*m2 + y*m6) + z*m10) + m14) and
// folded in: min = VuMin( p, min ), max = VuMax( p, max ). The w lanes take
// this function's stale stack (held as they were here).
inline void fluid_grow( float* aabb, const float* local_min, const float* local_max, const float* m )
{
    for ( int k = 0; k < 8; ++k )
    {
        const float x = ( k & 1 ) ? local_max[ 0 ] : local_min[ 0 ];
        const float y = ( k & 2 ) ? local_max[ 1 ] : local_min[ 1 ];
        const float z = ( k & 4 ) ? local_max[ 2 ] : local_min[ 2 ];
        const float px = ( ( m[ 0 ] * x + y * m[ 4 ] ) + m[ 8 ] * z ) + m[ 12 ];
        const float py = ( ( m[ 1 ] * x + m[ 5 ] * y ) + m[ 9 ] * z ) + m[ 13 ];
        const float pz = ( ( x * m[ 2 ] + y * m[ 6 ] ) + z * m[ 10 ] ) + m[ 14 ];
        aabb[ 0 ] = fluid_min( px, aabb[ 0 ] );
        aabb[ 1 ] = fluid_min( py, aabb[ 1 ] );
        aabb[ 2 ] = fluid_min( pz, aabb[ 2 ] );
        aabb[ 4 ] = fluid_max( px, aabb[ 4 ] );
        aabb[ 5 ] = fluid_max( py, aabb[ 5 ] );
        aabb[ 6 ] = fluid_max( pz, aabb[ 6 ] );
    }
}

// VuFluidSolver_prepare @0x524450, in retail's order.
inline bool fluid_prepare( fluid_solver_t& s, const water_t& water, const float* m, water_kernel_fn kernel, void* user )
{
    const fluids_mesh_t* mesh = s.mesh;
    if ( !mesh )
        return true;
    s.min_depth = water_bits( 0x7F7FFFFFu );   // FLT_MAX
    const int count = static_cast< int >( mesh->verts.size( ) / 4 );
    const int edges = static_cast< int >( mesh->edges.size( ) / 2 );
    if ( static_cast< int >( s.vertices.size( ) ) < count || static_cast< int >( s.edges.size( ) ) < edges )
        return false;   // retail's arrays are sized by the solver's ctor: not recreated
    // Each vertex placed in the world; +12 stale, displacement 0 (+28 stale),
    // the height and flag left for the query.
    for ( int i = 0; i < count; ++i )
    {
        const float* p = &mesh->verts[ 4 * i ];
        water_vertex_t& v = s.vertices[ i ];
        const float x = p[ 0 ], y = p[ 1 ], z = p[ 2 ];
        v.pos[ 0 ] = ( ( m[ 0 ] * x + y * m[ 4 ] ) + z * m[ 8 ] ) + m[ 12 ];
        v.pos[ 1 ] = ( ( x * m[ 1 ] + y * m[ 5 ] ) + z * m[ 9 ] ) + m[ 13 ];
        v.pos[ 2 ] = ( ( x * m[ 2 ] + y * m[ 6 ] ) + z * m[ 10 ] ) + m[ 14 ];
        v.pos_w = 0;
        v.disp[ 0 ] = v.disp[ 1 ] = v.disp[ 2 ] = 0.0f;
        v.disp_w = 0;
    }
    // The ctx (0x413500 then rebuilt): the aabb reset and grown by the mesh's
    // local aabb, the centre is the local aabb's centre placed in the world,
    // the radius the local aabb's xy half-diagonal (VuMath_sqrt: sqrtss).
    water_query_t ctx{ };
    const float cx = ( mesh->aabb_max[ 0 ] + mesh->aabb_min[ 0 ] ) * 0.5f;
    const float cy = ( mesh->aabb_max[ 1 ] + mesh->aabb_min[ 1 ] ) * 0.5f;
    const float cz = ( mesh->aabb_max[ 2 ] + mesh->aabb_min[ 2 ] ) * 0.5f;
    ctx.centre[ 0 ] = ( ( m[ 4 ] * cy + m[ 0 ] * cx ) + m[ 8 ] * cz ) + m[ 12 ];
    ctx.centre[ 1 ] = ( ( m[ 5 ] * cy + m[ 1 ] * cx ) + m[ 9 ] * cz ) + m[ 13 ];
    ctx.centre[ 2 ] = ( ( m[ 6 ] * cy + m[ 2 ] * cx ) + m[ 10 ] * cz ) + m[ 14 ];
    ctx.count = count;
    for ( int k = 0; k < 3; ++k )
    {
        ctx.aabb[ k ] = water_bits( 0x7F7FFFFFu );
        ctx.aabb[ 4 + k ] = water_bits( 0xFF7FFFFFu );
    }
    fluid_grow( ctx.aabb, mesh->aabb_min, mesh->aabb_max, m );
    const float hy = ( mesh->aabb_max[ 1 ] - mesh->aabb_min[ 1 ] ) * 0.5f;
    const float hx = ( mesh->aabb_max[ 0 ] - mesh->aabb_min[ 0 ] ) * 0.5f;
    ctx.radius = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( hy * hy + hx * hx ) ) );
    // The exclusions copied with no bound: a fifth lands on +0x4C (the forced
    // surface), later ones on the vertex pointer and the flags - not recreated.
    if ( s.exclude.size( ) > 4 )
        return false;
    ctx.exclude_count = static_cast< int >( s.exclude.size( ) );
    for ( int k = 0; k < ctx.exclude_count; ++k )
        ctx.exclude[ k ] = s.exclude[ k ];
    ctx.forced = -1;
    ctx.vertices = s.vertices.data( );
    std::vector< uint32_t > tags;
    if ( !water_query( water, ctx, kernel, user, tags ) )
        return false;
    // Submerged: 0 > z - height (comiss; ordered); the least depth by minss(
    // least, depth ); the byte is the OR of the flags.
    uint32_t any = 0;
    for ( int i = 0; i < count; ++i )
    {
        water_vertex_t& v = s.vertices[ i ];
        const float d = v.pos[ 2 ] - v.height;
        v.submerged = _mm_comigt_ss( _mm_setzero_ps( ), _mm_set_ss( d ) ) ? 1u : 0u;
        s.min_depth = fluid_min( s.min_depth, d );
        any |= v.submerged;
    }
    s.any_submerged = any != 0;
    // Each edge: the flag is the ends' XOR; a crossing edge gets the point at
    // t = -d0 / (d1 - d0) between the ends (a + (b - a) * t, lane by lane),
    // the displacement and height likewise; +12 / +28 stale. A non-crossing
    // edge keeps everything but the flag.
    for ( int e = 0; e < edges; ++e )
    {
        const water_vertex_t& a = s.vertices[ mesh->edges[ 2 * e ] ];
        const water_vertex_t& b = s.vertices[ mesh->edges[ 2 * e + 1 ] ];
        fluid_edge_t& out = s.edges[ e ];
        out.submerged = b.submerged ^ a.submerged;
        if ( !out.submerged )
            continue;
        const float d0 = a.pos[ 2 ] - a.height;
        const float t = -d0 / ( ( b.pos[ 2 ] - b.height ) - d0 );
        out.pos[ 0 ] = ( b.pos[ 0 ] - a.pos[ 0 ] ) * t + a.pos[ 0 ];
        out.pos[ 1 ] = ( b.pos[ 1 ] - a.pos[ 1 ] ) * t + a.pos[ 1 ];
        out.pos[ 2 ] = ( b.pos[ 2 ] - a.pos[ 2 ] ) * t + a.pos[ 2 ];
        out.pos_w = 0;
        out.disp[ 0 ] = a.disp[ 0 ] + ( b.disp[ 0 ] - a.disp[ 0 ] ) * t;
        out.disp[ 1 ] = a.disp[ 1 ] + ( b.disp[ 1 ] - a.disp[ 1 ] ) * t;
        out.disp[ 2 ] = a.disp[ 2 ] + ( b.disp[ 2 ] - a.disp[ 2 ] ) * t;
        out.disp_w = 0;
        out.height = ( b.height - a.height ) * t + a.height;
    }
    return true;
}

// 0x4157A0 (usercall xmm0, xmm1, xmm2): `comiss x0, 0; jb` - x2 when x0 < 0
// or unordered, else x1.
inline float fluid_select( float x0, float x1, float x2 )
{
    return _mm_comilt_ss( _mm_set_ss( x0 ), _mm_setzero_ps( ) ) || std::isnan( x0 ) ? x2 : x1;
}

// One submerged piece of a triangle as solve's stack list holds it (10
// words): three points (vertex or edge records), their displacements (the
// records' +16) and heights (+32), and the area.
struct fluid_piece_t
{
    const water_vertex_t* p[ 3 ];
    float area;
};

// Twice a triangle's area, the clip pieces' formula: (P - A) x (Q - A) in
// retail's terms - sqrt( ((dz1*dx2 - dx1*dz2)^2 + (dy1*dz2 - dz1*dy2)^2) +
// (dx1*dy2 - dy1*dx2)^2 ).
inline float fluid_piece_length( const water_vertex_t& a, const water_vertex_t& p, const water_vertex_t& q )
{
    const float dz1 = p.pos[ 2 ] - a.pos[ 2 ], dy1 = p.pos[ 1 ] - a.pos[ 1 ], dx1 = p.pos[ 0 ] - a.pos[ 0 ];
    const float dy2 = q.pos[ 1 ] - a.pos[ 1 ], dz2 = q.pos[ 2 ] - a.pos[ 2 ], dx2 = q.pos[ 0 ] - a.pos[ 0 ];
    const float y = dz1 * dx2 - dx1 * dz2;
    const float x = dy1 * dz2 - dz1 * dy2;
    const float z = dx1 * dy2 - dy1 * dx2;
    return _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( y * y + x * x ) + z * z ) ) );
}

// VuFluidSolver_solve @0x5249F0 (thiscall; body*; retn 4), from the
// disassembly: nothing when the inverse mass is an ORDERED zero (`ucomiss;
// lahf; test ah, 44h; jnp`). M = translate( offset ) * the body's transform
// (0x410AB0, 0x410B40 - the basis transposed into Vu rows -, VuMatrix_multiply
// 0x4078C0); for every triangle with a submerged vertex: the submerged
// pieces (the whole triangle, or the clip by its crossing edges), and per
// piece the water-surface normal, the flow, the drag (normal part only when
// moving into the water, the tangential part per axis), the buoyant prism.
inline bool fluid_solve( fluid_solver_t& s, const fluid_body_t& body, fluid_gravity_fn gravity, void* user )
{
    if ( _mm_comieq_ss( _mm_set_ss( body.inverse_mass ), _mm_setzero_ps( ) ) )
        return true;
    for ( int k = 0; k < 40; ++k )
        s.acc[ k ] = 0.0f;   // +128 .. +255 (and the w lanes: stale, held 0)
    const fluids_mesh_t& mesh = *s.mesh;
    const int count = static_cast< int >( mesh.triangles.size( ) / 15 );
    // 0x410B40: rows (m0, m4, m8, 0), (m1, m5, m9, 0), (m2, m6, m10, 0),
    // (origin, 1); 0x410AB0: identity with row 3 = (offset, 1); 0x4078C0:
    // out[4i+j] = ((T[4i]*B[j] + T[4i+1]*B[4+j]) + T[4i+2]*B[8+j]) + T[4i+3]*B[12+j].
    const float* t = body.transform;
    const float b[ 16 ] = { t[ 0 ], t[ 4 ], t[ 8 ], 0.0f, t[ 1 ], t[ 5 ], t[ 9 ], 0.0f, t[ 2 ], t[ 6 ], t[ 10 ], 0.0f, t[ 12 ], t[ 13 ],
        t[ 14 ], 1.0f };
    const float tr[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, s.offset[ 0 ], s.offset[ 1 ], s.offset[ 2 ], 1 };
    float m[ 16 ];
    for ( int i = 0; i < 4; ++i )
        for ( int j = 0; j < 4; ++j )
            m[ 4 * i + j ] = ( ( tr[ 4 * i ] * b[ j ] + tr[ 4 * i + 1 ] * b[ 4 + j ] ) + tr[ 4 * i + 2 ] * b[ 8 + j ] ) + tr[ 4 * i + 3 ] * b[ 12 + j ];
    const float vx = s.velocity_scale * body.linear[ 0 ];
    const float vy = s.velocity_scale * body.linear[ 1 ];
    const float vz = s.velocity_scale * body.linear[ 2 ];
    const float wx = body.angular[ 0 ], wy = body.angular[ 1 ], wz = body.angular[ 2 ];
    const float mass = 1.0f / body.inverse_mass;
    float g[ 4 ];
    if ( !gravity || !gravity( user, g ) )
        return false;   // g_vudynamicsimpl vt+28: Vu code
    s.volume = 0.0f;
    s.area = 0.0f;
    float* a = s.acc + 8;   // a[ k ] = solver float [ 32 + k ] (+128 + 4k)
    const float eps = water_bits( 0x34000000u );   // FLT_EPSILON
    for ( int ti = 0; ti < count; ++ti )
    {
        const uint32_t* tri = &mesh.triangles[ 15 * ti ];
        const water_vertex_t* v0 = &s.vertices[ tri[ 0 ] ];
        const water_vertex_t* v1 = &s.vertices[ tri[ 1 ] ];
        const water_vertex_t* v2 = &s.vertices[ tri[ 2 ] ];
        const float tnx = water_bits( tri[ 7 ] ), tny = water_bits( tri[ 8 ] ), tnz = water_bits( tri[ 9 ] );
        const uint32_t wet = v0->submerged + ( v1->submerged + v2->submerged );
        const float nwx = ( m[ 0 ] * tnx + m[ 4 ] * tny ) + m[ 8 ] * tnz;
        const float nwy = ( m[ 1 ] * tnx + m[ 5 ] * tny ) + m[ 9 ] * tnz;
        const float nwz = ( m[ 2 ] * tnx + m[ 6 ] * tny ) + m[ 10 ] * tnz;
        if ( wet == 0 )
            continue;
        fluid_piece_t pieces[ 3 ];
        int n = 0;
        if ( wet == 3 )
        {
            pieces[ 0 ] = { { v0, v1, v2 }, water_bits( tri[ 6 ] ) };
            n = 1;
        }
        else
        {
            // The rotation onto (A, B, C) with P on A-B and Q on C-A.
            const fluid_edge_t* e0 = &s.edges[ tri[ 3 ] ];
            const fluid_edge_t* e1 = &s.edges[ tri[ 4 ] ];
            const fluid_edge_t* e2 = &s.edges[ tri[ 5 ] ];
            const water_vertex_t *A, *B, *C, *P, *Q;
            if ( !e0->submerged )
            {
                A = v2, B = v0, C = v1, P = e2, Q = e1;
            }
            else if ( e2->submerged )
            {
                A = v0, B = v1, C = v2, P = e0, Q = e2;
            }
            else
            {
                A = v1, B = v2, C = v0, P = e1, Q = e0;
            }
            if ( A->submerged )
                pieces[ n++ ] = { { A, P, Q }, fluid_piece_length( *A, *P, *Q ) * 0.5f };
            if ( B->submerged )
                pieces[ n++ ] = { { B, Q, P }, fluid_piece_length( *B, *Q, *P ) * 0.5f };
            if ( C->submerged )
                pieces[ n++ ] = { { C, Q, B }, fluid_piece_length( *C, *Q, *B ) * 0.5f };
            if ( n <= 0 )
                continue;
        }
        for ( int pi = 0; pi < n; ++pi )
        {
            const water_vertex_t& p0 = *pieces[ pi ].p[ 0 ];
            const water_vertex_t& p1 = *pieces[ pi ].p[ 1 ];
            const water_vertex_t& p2 = *pieces[ pi ].p[ 2 ];
            const float area = pieces[ pi ].area;
            const float h0 = p0.height, h1 = p1.height, h2 = p2.height;
            // The water surface over the piece: (p.x, p.y, height).
            {
                const float ay = p1.pos[ 1 ] - p0.pos[ 1 ], ax = p1.pos[ 0 ] - p0.pos[ 0 ];
                const float by = p2.pos[ 1 ] - p0.pos[ 1 ], bx = p2.pos[ 0 ] - p0.pos[ 0 ];
                const float ah = h1 - h0, bh = h2 - h0;
                const float cx = ay * bh - ah * by;
                const float cy = ah * bx - ax * bh;
                const float cz = ax * by - ay * bx;
                const float l2 = ( cy * cy + cx * cx ) + cz * cz;
                if ( _mm_comigt_ss( _mm_set_ss( l2 ), _mm_set_ss( eps ) ) )
                {
                    const float len = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( l2 ) ) );
                    const float inv = 1.0f / len;
                    float nx = inv * cx, ny = inv * cy, nz = inv * cz;
                    if ( !_mm_comige_ss( _mm_set_ss( nz ), _mm_setzero_ps( ) ) )
                    {
                        nx = -nx;
                        ny = -ny;
                        nz = -nz;
                    }
                    const float w = fluid_min( area, len * 0.5f );
                    a[ 24 ] = a[ 24 ] + w * nx;
                    a[ 25 ] = a[ 25 ] + w * ny;
                    a[ 26 ] = a[ 26 ] + w * nz;
                }
            }
            const float third = water_bits( 0x3EAAAAABu );
            const float cx = ( p2.pos[ 0 ] + ( p0.pos[ 0 ] + p1.pos[ 0 ] ) ) * third;
            const float cy = ( p2.pos[ 1 ] + ( p0.pos[ 1 ] + p1.pos[ 1 ] ) ) * third;
            const float cz = ( p2.pos[ 2 ] + ( p0.pos[ 2 ] + p1.pos[ 2 ] ) ) * third;
            const float dx = ( p2.disp[ 0 ] + ( p0.disp[ 0 ] + p1.disp[ 0 ] ) ) * third;
            const float dy = ( p2.disp[ 1 ] + ( p0.disp[ 1 ] + p1.disp[ 1 ] ) ) * third;
            const float dz = ( p2.disp[ 2 ] + ( p0.disp[ 2 ] + p1.disp[ 2 ] ) ) * third;
            const float rx = cx - m[ 12 ], ry = cy - m[ 13 ], rz = cz - m[ 14 ];
            const float ux = ( ( rz * wy - ry * wz ) + vx ) - dx;
            const float uy = ( ( rx * wz - rz * wx ) + vy ) - dy;
            const float uz = ( ( ry * wx - rx * wy ) + vz ) - dz;
            a[ 28 ] = area * dx + a[ 28 ];
            a[ 29 ] = area * dy + a[ 29 ];
            a[ 30 ] = area * dz + a[ 30 ];
            const float speed = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( uy * uy + ux * ux ) + uz * uz ) ) );
            const float qy = -speed * uy, qx = -speed * ux, qz = -speed * uz;
            const float dot = ( nwx * qx + qy * nwy ) + nwz * qz;
            const float nqx = dot * nwx, nqy = dot * nwy, nqz = dot * nwz;
            const float ty = qy - nqy, tx = qx - nqx, tz = qz - nqz;
            const float l2 = ( m[ 9 ] * ty + m[ 8 ] * tx ) + m[ 10 ] * tz;
            const float l1 = ( m[ 5 ] * ty + m[ 4 ] * tx ) + m[ 6 ] * tz;
            const float l0 = ( m[ 1 ] * ty + m[ 0 ] * tx ) + m[ 2 ] * tz;
            const float c0 = s.axis_drag[ 0 ], c1 = s.axis_drag[ 1 ], c2 = s.axis_drag[ 2 ];
            const float sx = l2 * ( c2 * m[ 8 ] ) + ( l0 * ( c0 * m[ 0 ] ) + l1 * ( c1 * m[ 4 ] ) );
            const float sy = l2 * ( c2 * m[ 9 ] ) + ( l0 * ( c0 * m[ 1 ] ) + l1 * ( c1 * m[ 5 ] ) );
            const float sz = l2 * ( c2 * m[ 10 ] ) + ( l0 * ( c0 * m[ 2 ] ) + l1 * ( c1 * m[ 6 ] ) );
            float kx = nqx, ky = nqy, kz = nqz;
            if ( _mm_comigt_ss( _mm_setzero_ps( ), _mm_set_ss( ( nwx * ux + uy * nwy ) + nwz * uz ) ) )
                kx = ky = kz = 0.0f;   // moving out of the water: no normal part
            const float kzm = fluid_max( s.floor_z, kz );
            const float k = ( area * 0.5f ) * s.drag;
            const float fx = k * ( sx + kx ), fy = k * ( ky + sy ), fz = k * ( kzm + sz );
            a[ 8 ] = a[ 8 ] + fx;
            a[ 9 ] = a[ 9 ] + fy;
            a[ 10 ] = a[ 10 ] + fz;
            a[ 12 ] = a[ 12 ] + ( fz * ry - fy * rz );
            a[ 13 ] = a[ 13 ] + ( fx * rz - fz * rx );
            a[ 14 ] = a[ 14 ] + ( fy * rx - fx * ry );
            // The buoyant prism between the piece and the water.
            const float A0 = p2.pos[ 0 ] - p1.pos[ 0 ], B0 = p1.pos[ 1 ] - p0.pos[ 1 ], C0 = p2.pos[ 1 ] - p1.pos[ 1 ];
            const float D0 = p0.pos[ 0 ] - p2.pos[ 0 ], E0 = p1.pos[ 0 ] - p0.pos[ 0 ], F0 = p0.pos[ 1 ] - p2.pos[ 1 ];
            const float z0 = h0 - p0.pos[ 2 ], z1 = h1 - p1.pos[ 2 ], z2 = h2 - p2.pos[ 2 ];
            const float sixth = water_bits( 0x3E2AAAABu );
            const float V0 = ( water_abs( F0 * E0 - B0 * D0 ) * z0 ) * sixth;
            const float V1 = ( water_abs( A0 * B0 - C0 * E0 ) * z1 ) * sixth;
            const float V2 = ( water_abs( C0 * D0 - A0 * F0 ) * z2 ) * sixth;
            const float vol = ( V1 + V0 ) + V2;
            if ( !_mm_comigt_ss( _mm_set_ss( vol ), _mm_set_ss( water_bits( 0x00800000u ) ) ) )
                continue;
            const float Z0 = z0 + p0.pos[ 2 ];
            const float Z1 = ( z0 + p1.pos[ 2 ] ) + z1;
            const float Z2 = ( ( z0 + p2.pos[ 2 ] ) + z1 ) + z2;
            const float Sx = V2 * p2.pos[ 0 ] + ( V1 * p1.pos[ 0 ] + V0 * p0.pos[ 0 ] );
            const float Sy = V2 * p2.pos[ 1 ] + ( V1 * p1.pos[ 1 ] + V0 * p0.pos[ 1 ] );
            const float Sz = V2 * Z2 + ( V1 * Z1 + V0 * Z0 );
            const float inv = 1.0f / vol;
            const float bx = ( ( ( p0.pos[ 0 ] + p1.pos[ 0 ] ) + p2.pos[ 0 ] ) + inv * Sx ) * 0.25f;
            const float by = ( ( ( p0.pos[ 1 ] + p1.pos[ 1 ] ) + p2.pos[ 1 ] ) + inv * Sy ) * 0.25f;
            const float bz = ( ( ( p1.pos[ 2 ] + p0.pos[ 2 ] ) + p2.pos[ 2 ] ) + inv * Sz ) * 0.25f;
            const float sv = fluid_select( nwz, -1.0f, 1.0f ) * vol;
            const float q = -( s.buoyancy * sv );
            const float gx = q * g[ 0 ], gy = q * g[ 1 ], gz = q * g[ 2 ];
            a[ 0 ] = a[ 0 ] + gx;
            a[ 1 ] = a[ 1 ] + gy;
            a[ 2 ] = a[ 2 ] + gz;
            const float ox = bx - m[ 12 ], oy = by - m[ 13 ], oz = bz - m[ 14 ];
            a[ 4 ] = ( oy * gz - oz * gy ) + a[ 4 ];
            a[ 5 ] = a[ 5 ] + ( oz * gx - ox * gz );
            a[ 6 ] = a[ 6 ] + ( ox * gy - oy * gx );
            s.volume = s.volume + sv;
            s.area = area + s.area;
        }
    }
    // The water normal (no area: (0, 0, 1)); the mean flow.
    const float n2 = ( a[ 24 ] * a[ 24 ] + a[ 25 ] * a[ 25 ] ) + a[ 26 ] * a[ 26 ];
    if ( !_mm_comigt_ss( _mm_set_ss( n2 ), _mm_set_ss( eps ) ) )
    {
        a[ 24 ] = a[ 25 ] = 0.0f;
        a[ 26 ] = 1.0f;
    }
    else
    {
        const float inv = 1.0f / _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( n2 ) ) );
        a[ 24 ] = inv * a[ 24 ];
        const float nz = inv * a[ 26 ];
        a[ 25 ] = inv * a[ 25 ];
        a[ 26 ] = nz;
    }
    if ( _mm_comigt_ss( _mm_set_ss( s.area ), _mm_set_ss( eps ) ) )
    {
        const float inv = 1.0f / s.area;
        a[ 28 ] = inv * a[ 28 ];
        const float fz = inv * a[ 30 ];
        a[ 29 ] = inv * a[ 29 ];
        a[ 30 ] = fz;
    }
    // Planing: the scaled velocity INTO the water normal.
    const float into = -( ( a[ 24 ] * vx + a[ 25 ] * vy ) + a[ 26 ] * vz );
    if ( _mm_comigt_ss( _mm_set_ss( into ), _mm_setzero_ps( ) ) )
    {
        const float p = ( ( s.area / mesh.total_area ) * ( ( into * 10.0f ) * mass ) ) * into;
        a[ 16 ] = a[ 16 ] + a[ 24 ] * p;
        a[ 17 ] = a[ 17 ] + a[ 25 ] * p;
        a[ 18 ] = a[ 18 ] + a[ 26 ] * p;
    }
    // The totals: force = planing + (drag + buoyancy); torque = (drag +
    // buoyancy) + the zero +208.
    s.acc[ 0 ] = a[ 16 ] + ( a[ 8 ] + a[ 0 ] );
    s.acc[ 1 ] = a[ 17 ] + ( a[ 9 ] + a[ 1 ] );
    s.acc[ 2 ] = a[ 18 ] + ( a[ 10 ] + a[ 2 ] );
    s.acc[ 4 ] = ( a[ 12 ] + a[ 4 ] ) + a[ 20 ];
    s.acc[ 5 ] = a[ 21 ] + ( a[ 13 ] + a[ 5 ] );
    s.acc[ 6 ] = a[ 22 ] + ( a[ 14 ] + a[ 6 ] );
    return true;
}