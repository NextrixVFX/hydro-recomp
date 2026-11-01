#pragma once

#include <workspace/core/boat/hull.hxx>
#include <workspace/core/boat/meshes.hxx>
#include <workspace/core/boat/sections.hxx>
#include <workspace/core/boat/tick.hxx>
#include <workspace/core/world/kernel.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// The boat's step callback tickThunk 0x41E5E0 (the +0x60 interface's slot 1:
// thiscall(boat + 0x60, dt), retn 4) composed on a race boat's images: the
// thunk_ctx_t (hull.hxx) and every view under it, read from the offsets the
// verified units read them at (the oracle cases hull_update / boat_4760 /
// boat_4010 / tick_thunk wrote their images at exactly these):
//
//   hull (boat +0x228): hull_fluid_t +0x1C offset, +0x4C drag, +0x50
//     buoyancy, +0x58 / +0x5C the factor min / speed, +0x60 .. +0x6C the
//     curve, +0x70 / +0x74 the axis drags (its boat centre = boat +0x290);
//     hull_t +0x2C aero point, +0x3C damping, +0x54 aero, +0x78 slide, +0x94
//     aero scale (its aero_off = boat +0x495); hull_steer_t +0x7C / +0x80 /
//     +0x88 / +0x8C / +0xB4 / +0xCC / +0xD0 (input = boat +0x300, input_off
//     = boat +0x308); +0x15C / +0x160 / +0x164 / +0x174 (byte) / +0x178; +0x1CC
//     the solver; +0x1DC / +0x1E0 the two waves the records' query excludes.
//   solver (hull +0x1CC): +0 the fluids mesh asset, +0x30 / +0x34 its own
//     excluded waves (count, array pointer: solver_excluded), +0x50 any-submerged, +0x54 volume, +0x58 area, +0x5C
//     least depth, +0x60 .. +0xFF the solve vectors, +0x100 the floor z.
//   engine O (boat +0x22C, boat_engine_t): +4 / +0xC / +0x10 / +0x14 / +0x18 /
//     +0x1C / +0x20 / +0x24 / +0x28 / +0x2C floats, the Power Curve (+0x30
//     data, +0x34 count), the Drives (+0x3C .. +0x40, 32-byte records), the
//     +0xF8 8-byte per-drive records (held here: `records`).
//   boat: +0x134, +0x290 .. +0x298, +0x2FC, +0x300, +0x308, +0x30C, +0x32C,
//     +0x32F, +0x3D4 (0x4219C0's gate = X +0x374), +0x3D8, +0x3DC, +0x3E8 ..
//     the euler angles, +0x470, +0x495; +0x50's transform component +0x60 ..
//     +0x68; +0x21C the body (solver_rigid_t, worked on in place).
//
// The solver's record arrays and solve outputs live in boat_thunk_t across
// steps (retail's are the solver's own memory: the load's 0x5243A0 zeroes
// them); after each step the solver words (+0x50, +0x54 .. +0xFF) and the
// hull's +0x15C .. +0x178 are written back to the images. A wave pointer this
// composition cannot name (an excluded wave: `wave_index` absent or -2) is
// UNKNOWN: the build refuses (false, `why`).
struct boat_thunk_in_t
{
    const std::uint8_t* boat = nullptr;
    std::uint8_t* hull = nullptr;                 // boat +0x228
    std::uint8_t* solver = nullptr;               // hull +0x1CC
    const std::uint8_t* transform = nullptr;      // boat +0x50
    const boat_engine_t* engine = nullptr;        // boat +0x22C
    solver_rigid_t* body = nullptr;               // boat +0x21C
    const fluids_mesh_asset_t* fluids = nullptr;  // the solver +0's asset (null: no mesh)
    const water_t* water = nullptr;               // VuWater g 0x759058 as the query sees it
    water_kernel_fn kernel = nullptr;             // a wave's slot 3
    void* kernel_user = nullptr;
    fluid_gravity_fn gravity = nullptr;           // g_vudynamicsimpl vt+0x1C
    void* gravity_user = nullptr;
    std::function< int( std::uint32_t ) > wave_index;   // a VuWater wave pointer -> its index (-2 unknown)
    // The solver's excluded waves: +0x30 the count, +0x34 a POINTER to the array of wave pointers (VuFluidSolver_prepare
    // 0x524450: mov edi, [esi+0x30] ... mov edx, [esi+0x34]; [edx + 4k]) - its words, resolved by the caller.
    const std::uint32_t* solver_excluded = nullptr;
};

struct boat_thunk_t
{
    fluids_mesh_t mesh;
    fluid_solver_t solver{ };
    std::vector< std::uint8_t > records;   // engine +0xF8 (8 bytes per record)
    bool started = false;
    hull_t hull{ };
    hull_fluid_t fluid{ };
    hull_steer_t steer{ };
    std::unique_ptr< boat_view_t > boat_view;
    std::unique_ptr< o22c_view_t > o;
    std::unique_ptr< b4760_ctx_t > rec;
    std::unique_ptr< tick_ctx_t > tick;
    std::unique_ptr< b4010_ctx_t > c4010;
    std::unique_ptr< hull_ctx_t > hc;
    std::unique_ptr< b19c0_ctx_t > c19;
    std::unique_ptr< thunk_ctx_t > thunk;
    float centre[ 3 ] = { };
    std::string why;
};

inline float thunk_f( const std::uint8_t* p, int at )
{
    float f;
    std::memcpy( &f, p + at, 4 );
    return f;
}

inline std::uint32_t thunk_u( const std::uint8_t* p, int at )
{
    std::uint32_t u;
    std::memcpy( &u, p + at, 4 );
    return u;
}

inline bool thunk_wave( const boat_thunk_in_t& in, std::uint32_t word, int& out, boat_thunk_t& t )
{
    if ( word == 0 )
    {
        out = -1;
        return true;
    }
    out = in.wave_index ? in.wave_index( word ) : -2;
    if ( out < 0 )
    {
        t.why = "an excluded wave the composition cannot name (UNKNOWN)";
        return false;
    }
    return true;
}

// Builds (or refreshes, each step: the views are values read now) the
// context. False (t.why) when an image is missing or a wave is unnamed.
inline bool boat_thunk_build( boat_thunk_t& t, const boat_thunk_in_t& in )
{
    t.why.clear( );
    if ( !in.boat || !in.hull || !in.solver || !in.transform || !in.engine || !in.body || !in.water )
    {
        t.why = "an image tickThunk 0x41E5E0 reads is missing";
        return false;
    }
    const std::uint8_t* b = in.boat;
    const std::uint8_t* h = in.hull;
    const std::uint8_t* s = in.solver;
    const std::uint8_t* e = in.engine->object.image;
    if ( !t.started )
    {
        // The solver's mesh and records as the load left them (0x5243A0).
        t.mesh = fluids_mesh_t{ };
        t.solver = fluid_solver_t{ };
        if ( in.fluids )
        {
            const fluids_mesh_asset_t& a = *in.fluids;
            std::memcpy( t.mesh.aabb_min, a.aabb, 16 );
            std::memcpy( t.mesh.aabb_max, a.aabb + 4, 16 );
            t.mesh.verts.resize( 4 * static_cast< std::size_t >( a.vertices.size ) );
            std::memcpy( t.mesh.verts.data( ), a.vertices.bytes.data( ), 16 * static_cast< std::size_t >( a.vertices.size ) );
            t.mesh.edges.resize( 2 * static_cast< std::size_t >( a.edges.size ) );
            std::memcpy( t.mesh.edges.data( ), a.edges.bytes.data( ), 8 * static_cast< std::size_t >( a.edges.size ) );
            t.mesh.triangles.resize( 15 * static_cast< std::size_t >( a.records.size ) );
            std::memcpy( t.mesh.triangles.data( ), a.records.bytes.data( ), 60 * static_cast< std::size_t >( a.records.size ) );
            std::memcpy( &t.mesh.total_area, &a.w9c, 4 );
            t.solver.mesh = &t.mesh;
            t.solver.vertices.assign( static_cast< std::size_t >( a.vertices.size ), water_vertex_t{ } );
            t.solver.edges.assign( static_cast< std::size_t >( a.edges.size ), fluid_edge_t{ } );
        }
        t.solver.any_submerged = s[ 0x50 ] != 0;
        t.solver.volume = thunk_f( s, 0x54 );
        t.solver.area = thunk_f( s, 0x58 );
        t.solver.min_depth = thunk_f( s, 0x5C );
        std::memcpy( t.solver.acc, s + 0x60, sizeof( t.solver.acc ) );
        t.solver.floor_z = thunk_f( s, 0x100 );
        t.records.assign( 8 * in.engine->records, 0 );
        t.started = true;
    }
    if ( in.engine->records < in.engine->drives.size( ) )
    {
        t.why = "the engine's +0xF8 records are fewer than its Drives (0x414760 would write past them - UNKNOWN)";
        return false;
    }
    t.solver.exclude.clear( );
    const std::int32_t excluded = static_cast< std::int32_t >( thunk_u( s, 0x30 ) );
    if ( excluded > 0 )
    {
        if ( !in.solver_excluded )
        {
            t.why = "the solver's excluded waves (+0x30 count, +0x34 array) with no array given - UNKNOWN";
            return false;
        }
        for ( std::int32_t k = 0; k < excluded; ++k )
        {
            int w = -1;
            if ( !thunk_wave( in, in.solver_excluded[ k ], w, t ) )
                return false;
            t.solver.exclude.push_back( w );   // a null word: -1 (retail's null pointer, which no wave matches)
        }
    }
    int ex0 = -1, ex1 = -1;
    if ( !thunk_wave( in, thunk_u( h, 0x1DC ), ex0, t ) || !thunk_wave( in, thunk_u( h, 0x1E0 ), ex1, t ) )
        return false;
    const std::int32_t count = static_cast< std::int32_t >( thunk_u( e, 0x34 ) );
    if ( count < 0 || static_cast< std::size_t >( count ) > in.engine->curve.size( ) )
    {
        t.why = "the Power Curve's +0x34 count exceeds its data (UNKNOWN)";
        return false;
    }
    // hull_fluid_t / hull_t / hull_steer_t by their hull offsets
    for ( int k = 0; k < 3; ++k )
    {
        t.fluid.offset[ k ] = thunk_f( h, 0x1C + 4 * k );
        t.fluid.boat_centre[ k ] = thunk_f( b, 0x290 + 4 * k );
        t.centre[ k ] = t.fluid.boat_centre[ k ];
        t.hull.aero_point[ k ] = thunk_f( h, 0x2C + 4 * k );
        t.hull.damping[ k ] = thunk_f( h, 0x3C + 4 * k );
    }
    t.fluid.drag = thunk_f( h, 0x4C );
    t.fluid.buoyancy = thunk_f( h, 0x50 );
    t.fluid.factor_min = thunk_f( h, 0x58 );
    t.fluid.factor_speed = thunk_f( h, 0x5C );
    for ( int k = 0; k < 4; ++k )
        t.fluid.curve[ k ] = thunk_f( h, 0x60 + 4 * k );
    t.fluid.axis[ 0 ] = thunk_f( h, 0x70 );
    t.fluid.axis[ 1 ] = thunk_f( h, 0x74 );
    t.hull.aero = thunk_f( h, 0x54 );
    t.hull.slide = thunk_f( h, 0x78 );
    t.hull.aero_scale = thunk_f( h, 0x94 );
    t.hull.aero_off = b[ 0x495 ] != 0;
    t.steer.speed_lo = thunk_f( h, 0x7C );
    t.steer.angle_lo = thunk_f( h, 0x80 );
    t.steer.speed_hi = thunk_f( h, 0x88 );
    t.steer.angle_hi = thunk_f( h, 0x8C );
    t.steer.angle_deep = thunk_f( h, 0xB4 );
    t.steer.depth_lo = thunk_f( h, 0xCC );
    t.steer.depth_hi = thunk_f( h, 0xD0 );
    t.steer.input = thunk_f( b, 0x300 );
    t.steer.input_off = thunk_u( b, 0x308 );
    solver_rigid_t& body = *in.body;
    // the views
    t.boat_view = std::make_unique< boat_view_t >( boat_view_t{ thunk_f( b, 0x2FC ), thunk_u( b, 0x308 ), b[ 0x32C ], b[ 0x32F ],
        { thunk_f( in.transform, 0x60 ), thunk_f( in.transform, 0x64 ), thunk_f( in.transform, 0x68 ) }, thunk_f( e, 0x4 ), thunk_f( e, 0xC ),
        thunk_f( b, 0x470 ), body } );
    const float* table = in.engine->curve.empty( ) ? nullptr : reinterpret_cast< const float* >( in.engine->curve.data( ) );
    t.o = std::make_unique< o22c_view_t >( o22c_view_t{ thunk_f( e, 0x4 ), thunk_f( e, 0xC ), thunk_f( e, 0x10 ), thunk_f( e, 0x20 ),
        thunk_f( e, 0x28 ), thunk_f( e, 0x2C ), table, count, thunk_f( b, 0x30C ), thunk_f( b, 0x470 ), b[ 0x32C ] } );
    t.records.resize( 8 * in.engine->records, 0 );
    std::uint8_t* points = in.engine->drives.empty( ) ? nullptr
                                                     : const_cast< std::uint8_t* >( reinterpret_cast< const std::uint8_t* >( in.engine->drives.data( ) ) );
    t.rec = std::make_unique< b4760_ctx_t >( b4760_ctx_t{ thunk_f( e, 0x14 ), thunk_f( e, 0x24 ), points,
        points ? points + 32 * in.engine->drives.size( ) : nullptr, t.records.empty( ) ? nullptr : t.records.data( ), ex0, ex1, body,
        *t.boat_view, *t.o, *in.water, in.kernel, in.kernel_user } );
    t.tick = std::make_unique< tick_ctx_t >( tick_ctx_t{ thunk_f( e, 0x18 ), thunk_f( e, 0x1C ), b[ 0x32C ], &t.solver.volume, thunk_f( e, 0x4 ),
        thunk_f( e, 0xC ), thunk_f( e, 0x28 ), thunk_f( b, 0x30C ), thunk_f( b, 0x470 ), b[ 0x32C ], body } );
    t.c4010 = std::make_unique< b4010_ctx_t >( b4010_ctx_t{ *t.o, t.centre, *t.rec, *t.tick } );
    t.hc = std::make_unique< hull_ctx_t >( hull_ctx_t{ t.hull, t.fluid, t.steer, thunk_f( h, 0x15C ), thunk_f( h, 0x160 ), thunk_f( h, 0x164 ),
        thunk_f( h, 0x178 ), h[ 0x174 ], body, t.solver, *in.water, in.kernel, in.kernel_user, in.gravity, in.gravity_user, 0.0f } );
    t.c19 = std::make_unique< b19c0_ctx_t >( b19c0_ctx_t{ thunk_f( b, 0x134 ), thunk_f( b, 0x3D8 ), thunk_f( b, 0x3DC ),
        { thunk_f( b, 0x3E8 ), thunk_f( b, 0x3EC ), thunk_f( b, 0x3F0 ) }, body } );
    t.thunk = std::make_unique< thunk_ctx_t >( thunk_ctx_t{ *t.hc, *t.c4010, *t.c19, b[ 0x3D4 ] } );
    return true;
}

// One step: tickThunk 0x41E5E0 (boat tick.hxx boat_tick_thunk) on the built
// context, then the solver's and the hull's words written back. False: a
// path not recreated was reached (UNKNOWN).
inline bool boat_thunk_run( boat_thunk_t& t, const boat_thunk_in_t& in, float dt )
{
    if ( !t.thunk )
        return false;
    const bool ok = boat_tick_thunk( *t.thunk, dt );
    std::uint8_t* s = in.solver;
    s[ 0x50 ] = t.solver.any_submerged ? 1 : 0;
    std::memcpy( s + 0x54, &t.solver.volume, 4 );
    std::memcpy( s + 0x58, &t.solver.area, 4 );
    std::memcpy( s + 0x5C, &t.solver.min_depth, 4 );
    std::memcpy( s + 0x60, t.solver.acc, sizeof( t.solver.acc ) );
    std::memcpy( in.hull + 0x15C, &t.hc->f15C, 4 );
    std::memcpy( in.hull + 0x160, &t.hc->f160, 4 );
    std::memcpy( in.hull + 0x164, &t.hc->f164, 4 );
    in.hull[ 0x174 ] = t.hc->f174;
    std::memcpy( in.hull + 0x178, &t.hc->f178, 4 );
    return ok;
}

// The race's step: build then run (the step callback's slot 1). The world
// lends the water as its query view (water_view_kernel answers the waves).
inline bool boat_thunk_step( boat_thunk_t& t, const boat_thunk_in_t& in, float dt )
{
    return boat_thunk_build( t, in ) && boat_thunk_run( t, in, dt );
}

inline void boat_thunk_water( boat_thunk_in_t& in, const water_view_t& view )
{
    in.water = &view.water;
    in.kernel = water_view_kernel;
    in.kernel_user = const_cast< water_view_t* >( &view );
}
