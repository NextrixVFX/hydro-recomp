#pragma once

#include <workspace/core/boat/hull.hxx>
#include <workspace/core/boat/steer.hxx>
#include <workspace/core/boat/update.hxx>
#include <workspace/core/boat/tick.hxx>
#include <workspace/core/boat/math.hxx>
#include <workspace/core/boat/entity.hxx>
#include <workspace/utility/golden.hxx>
#include <workspace/utility/logger.hxx>

#include <cstring>
#include <vector>

// The boat's game-side checks against retail (memory/audit.md "Batch 9b"..).

// The oracle's fake slot 3 (tools/oracle.py case_fluid_prepare): logs (wave,
// ctx+92, ctx+100) and adds k to the height, k * 0.25 to disp x and -k to
// disp z of every vertex whose tag matches ctx+92 (all when ctx+100 is 0).
struct fluid_fake_t
{
    std::vector< float > k;
    std::vector< uint32_t > log;
};

inline bool fluid_fake_kernel( void* user, int wave, water_query_t& ctx )
{
    fluid_fake_t& f = *static_cast< fluid_fake_t* >( user );
    f.log.push_back( static_cast< uint32_t >( wave ) );
    f.log.push_back( ctx.tag );
    f.log.push_back( ctx.flag100 );
    const float k = f.k[ wave ];
    for ( int i = 0; i < ctx.count; ++i )
    {
        if ( ctx.flag100 && ctx.tags[ i ] != ctx.tag )
            continue;
        water_vertex_t& v = ctx.vertices[ i ];
        v.disp[ 0 ] = v.disp[ 0 ] + k * 0.25f;
        v.disp[ 2 ] = v.disp[ 2 ] - k;
        v.height = v.height + k;
    }
    return true;
}

// VuFluidSolver_prepare @0x524450 (with the water query @0x528FC0) against
// retail: every vertex and edge record word except +12 / +28 (retail's stale
// stack), the any byte, the least depth, and the kernel calls in order.
inline bool verify_fluid_prepare( )
{
    int failed = 0, calls = 0, tagged = 0, submerged = 0, crossing = 0, empty = 0;
    for ( std::size_t index = 0; index < g_golden_fluid_prepare_count; ++index )
    {
        const unsigned* s = g_golden_fluid_prepare_words + g_golden_fluid_prepare_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        auto f = [ & ]( ) { return water_bits( *s++ ); };
        float m[ 16 ];
        for ( float& x : m )
            x = f( );
        fluids_mesh_t mesh;
        for ( float& x : mesh.aabb_min )
            x = f( );
        for ( float& x : mesh.aabb_max )
            x = f( );
        const unsigned nv = u( );
        mesh.verts.resize( 4 * nv );
        for ( float& x : mesh.verts )
            x = f( );
        const unsigned ne = u( );
        mesh.edges.resize( 2 * ne );
        for ( uint32_t& x : mesh.edges )
            x = u( );
        water_t water;
        water.skip_waves = u( ) != 0;
        water.surface_root = static_cast< int >( u( ) );
        water.nodes.resize( u( ) );
        for ( water_node_t& n : water.nodes )
        {
            for ( float& x : n.rect )
                x = f( );
            n.first = static_cast< int >( u( ) );
            n.second = static_cast< int >( u( ) );
            n.tag = u( );
        }
        water.surfaces.resize( u( ) );
        for ( water_surface_t& w : water.surfaces )
        {
            w.height = f( );
            w.below = f( );
            w.above = f( );
            for ( float& x : w.inverse )
                x = f( );
            w.floor = f( );
            w.half_x = f( );
            w.half_y = f( );
            w.wave_root = static_cast< int >( u( ) );
        }
        fluid_fake_t fake;
        water.waves.resize( u( ) );
        for ( water_wave_t& w : water.waves )
        {
            w.centre[ 0 ] = f( );
            w.centre[ 1 ] = f( );
            w.radius = f( );
            fake.k.push_back( f( ) );
        }
        fluid_solver_t solver{ };
        solver.mesh = &mesh;
        solver.exclude.resize( u( ) );
        for ( int& x : solver.exclude )
            x = static_cast< int >( u( ) );
        solver.vertices.resize( nv );
        solver.edges.resize( ne );
        std::memset( solver.vertices.data( ), 0x77, nv * sizeof( water_vertex_t ) );
        std::memset( solver.edges.data( ), 0x77, ne * sizeof( fluid_edge_t ) );
        const char* what = nullptr;
        unsigned bad = 0;
        if ( !fluid_prepare( solver, water, m, fluid_fake_kernel, &fake ) )
            what = "a path not recreated was reached";
        auto records = [ & ]( const void* ours, unsigned n, const char* name )
        {
            const unsigned* o = static_cast< const unsigned* >( ours );
            for ( unsigned k = 0; k < 10 * n; ++k )
            {
                const unsigned r = u( );
                if ( !what && k % 10 != 3 && k % 10 != 7 && o[ k ] != r )
                {
                    what = name;
                    bad = k;
                }
            }
        };
        records( solver.vertices.data( ), nv, "a vertex record word" );
        records( solver.edges.data( ), ne, "an edge record word" );
        const unsigned any = u( ), least = u( ), ncalls = u( );
        uint32_t ours_least;
        std::memcpy( &ours_least, &solver.min_depth, 4 );
        if ( !what && ( solver.any_submerged ? 1u : 0u ) != any )
            what = "the any-submerged byte";
        if ( !what && ours_least != least )
            what = "the least depth";
        if ( !what && fake.log.size( ) != 3 * ncalls )
            what = "the number of kernel calls";
        for ( unsigned k = 0; k < 3 * ncalls; ++k )
        {
            const unsigned r = u( );
            if ( !what && fake.log[ k ] != r )
                what = "a kernel call";
            if ( k % 3 == 2 && r )
                ++tagged;
        }
        calls += static_cast< int >( ncalls );
        for ( const water_vertex_t& v : solver.vertices )
            submerged += static_cast< int >( v.submerged );
        for ( const fluid_edge_t& e : solver.edges )
            crossing += e.submerged == 1 ? 1 : 0;
        empty += water.surfaces.empty( ) ? 1 : 0;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "fluid prepare vector %d: %s (word %u)", static_cast< int >( index ), what, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuFluidSolver_prepare + the water query (surface walk, containment, tagger, wave walk) match retail over %d "
                     "vectors, every written word exact (%d kernel calls, %d tagged; %d submerged vertices, %d crossing edges; "
                     "%d with no water)",
            static_cast< int >( g_golden_fluid_prepare_count ), calls, tagged, submerged, crossing, empty );
    else
        LOG_ERROR( "VuFluidSolver_prepare FAILED %d of %d vectors against @0x524450", failed,
            static_cast< int >( g_golden_fluid_prepare_count ) );
    return failed == 0;
}

// The oracle's g_vudynamicsimpl vt+28: the chosen gravity.
inline bool fluid_fake_gravity( void* user, float* out )
{
    std::memcpy( out, user, 12 );
    return true;
}

// VuFluidSolver_solve @0x5249F0 against retail: the solver's words +84 ..
// +255 (pre-filled 0x77 on both sides), except +92 (prepare's least depth,
// not solve's) and every vector's w lane (solve's stale stack).
inline bool verify_fluid_solve( )
{
    int failed = 0, skipped = 0, wet = 0;
    for ( std::size_t index = 0; index < g_golden_fluid_solve_count; ++index )
    {
        const unsigned* s = g_golden_fluid_solve_words + g_golden_fluid_solve_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        auto f = [ & ]( ) { return water_bits( *s++ ); };
        fluid_solver_t solver{ };
        for ( float& x : solver.offset )
            x = f( );
        solver.drag = f( );
        solver.buoyancy = f( );
        solver.velocity_scale = f( );
        for ( float& x : solver.axis_drag )
            x = f( );
        solver.floor_z = f( );
        fluid_body_t body;
        for ( float& x : body.transform )
            x = f( );
        for ( float& x : body.linear )
            x = f( );
        for ( float& x : body.angular )
            x = f( );
        body.inverse_mass = f( );
        float gravity[ 3 ];
        for ( float& x : gravity )
            x = f( );
        solver.vertices.resize( u( ) );
        for ( water_vertex_t& v : solver.vertices )
            for ( int k = 0; k < 10; ++k )
                reinterpret_cast< unsigned* >( &v )[ k ] = u( );
        solver.edges.resize( u( ) );
        for ( fluid_edge_t& e : solver.edges )
            for ( int k = 0; k < 10; ++k )
                reinterpret_cast< unsigned* >( &e )[ k ] = u( );
        fluids_mesh_t mesh;
        mesh.triangles.resize( 15 * u( ) );
        for ( uint32_t& x : mesh.triangles )
            x = u( );
        mesh.total_area = f( );
        solver.mesh = &mesh;
        std::memset( &solver.volume, 0x77, 4 );
        std::memset( &solver.area, 0x77, 4 );
        std::memset( solver.acc, 0x77, sizeof( solver.acc ) );
        const char* what = nullptr;
        int bad = -1;
        if ( !fluid_solve( solver, body, fluid_fake_gravity, gravity ) )
            what = "a path not recreated was reached";
        unsigned ours[ 43 ];
        std::memcpy( &ours[ 0 ], &solver.volume, 4 );
        std::memcpy( &ours[ 1 ], &solver.area, 4 );
        ours[ 2 ] = 0x77777777u;
        std::memcpy( &ours[ 3 ], solver.acc, sizeof( solver.acc ) );
        for ( int k = 0; k < 43; ++k )
        {
            const unsigned r = u( );
            const int offset = 84 + 4 * k;
            if ( what || offset == 92 || ( offset >= 96 && ( offset - 96 ) % 16 == 12 ) )
                continue;
            if ( ours[ k ] != r )
            {
                what = "a solver word";
                bad = offset;
            }
        }
        skipped += _mm_comieq_ss( _mm_set_ss( body.inverse_mass ), _mm_setzero_ps( ) ) ? 1 : 0;
        wet += solver.volume != 0.0f ? 1 : 0;   // signed: 0x4157A0 gives -1 for an up-facing normal
        if ( what )
        {
            ++failed;
            LOG_ERROR( "fluid solve vector %d: %s (+%d)", static_cast< int >( index ), what, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuFluidSolver_solve matches retail over %d vectors, every written word exact (%d with a buoyant volume - signed -, %d with "
                     "zero inverse mass)",
            static_cast< int >( g_golden_fluid_solve_count ), wet, skipped );
    else
        LOG_ERROR( "VuFluidSolver_solve FAILED %d of %d vectors against @0x5249F0", failed, static_cast< int >( g_golden_fluid_solve_count ) );
    return failed == 0;
}

// VuBoatHull's applyAeroForces 0x418510, the angular damping 0x418860 and
// applyPowerSlideForces 0x418BD0 against retail: the six force / torque
// lanes after each, every bit (each call starts from the same pre-fill).
inline bool verify_hull_forces( )
{
    int failed = 0, aero = 0, slide = 0;
    for ( std::size_t index = 0; index < g_golden_hull_forces_count; ++index )
    {
        const unsigned* s = g_golden_hull_forces_words + g_golden_hull_forces_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        auto f = [ & ]( ) { return water_bits( *s++ ); };
        hull_t h;
        for ( float& x : h.aero_point )
            x = f( );
        for ( float& x : h.damping )
            x = f( );
        h.aero = f( );
        h.slide = f( );
        h.aero_scale = f( );
        h.aero_off = u( ) != 0;
        hull_body_t b;
        for ( float& x : b.transform )
            x = f( );
        for ( float& x : b.inverse_inertia )
            x = f( );
        for ( float& x : b.linear )
            x = f( );
        for ( float& x : b.angular )
            x = f( );
        b.inverse_mass = f( );
        fluid_solver_t solver{ };
        solver.volume = f( );
        for ( int k = 0; k < 3; ++k )
            solver.acc[ 32 + k ] = f( );
        for ( int k = 0; k < 3; ++k )
            solver.acc[ 36 + k ] = f( );
        float m[ 16 ];
        for ( float& x : m )
            x = f( );
        unsigned pre[ 6 ];
        for ( unsigned& x : pre )
            x = u( );
        const char* what = nullptr;
        for ( int call = 0; call < 3; ++call )
        {
            float lanes[ 6 ];
            std::memcpy( lanes, pre, 24 );
            if ( call == 0 )
                hull_aero( h, b, solver, lanes, lanes + 3 );
            else if ( call == 1 )
                hull_damping( h, b, m, lanes + 3 );
            else
                hull_slide( h, b, solver, m, lanes );
            for ( int k = 0; k < 6; ++k )
            {
                unsigned o;
                std::memcpy( &o, &lanes[ k ], 4 );
                const unsigned r = u( );
                if ( !what && o != r )
                    what = call == 0 ? "the aero force / torque" : call == 1 ? "the damping torque" : "the power slide force";
                if ( call == 0 && k == 0 && r != pre[ 0 ] )
                    ++aero;
                if ( call == 2 && k == 0 && r != pre[ 0 ] )
                    ++slide;
            }
        }
        if ( what )
        {
            ++failed;
            LOG_ERROR( "hull forces vector %d: %s", static_cast< int >( index ), what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuBoatHull applyAeroForces / the angular damping / applyPowerSlideForces match retail over %d vectors, every "
                     "lane exact (%d moved by aero, %d by the slide)",
            static_cast< int >( g_golden_hull_forces_count ), aero, slide );
    else
        LOG_ERROR( "VuBoatHull forces FAILED %d of %d vectors", failed, static_cast< int >( g_golden_hull_forces_count ) );
    return failed == 0;
}

// VuBoatHull_applyFluidForces @0x418220 end to end (prepare + solve over one
// flat surface, the box mesh) against retail: the solver's +4 .. +47 (the
// parameters it set; +16 / +44 its stale stack, masked), +84 / +88 and the
// six force / torque lanes.
inline bool verify_hull_fluid( )
{
    int failed = 0, wet = 0;
    for ( std::size_t index = 0; index < g_golden_hull_fluid_count; ++index )
    {
        const unsigned* s = g_golden_hull_fluid_words + g_golden_hull_fluid_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        auto f = [ & ]( ) { return water_bits( *s++ ); };
        hull_fluid_t h;
        for ( float& x : h.offset )
            x = f( );
        for ( float& x : h.boat_centre )
            x = f( );
        h.drag = f( );
        h.buoyancy = f( );
        h.factor_min = f( );
        h.factor_speed = f( );
        for ( float& x : h.curve )
            x = f( );
        for ( float& x : h.axis )
            x = f( );
        fluid_body_t body;
        for ( float& x : body.transform )
            x = f( );
        for ( float& x : body.linear )
            x = f( );
        for ( float& x : body.angular )
            x = f( );
        body.inverse_mass = f( );
        const float height = f( );
        float m[ 16 ];
        for ( float& x : m )
            x = f( );
        float lanes[ 6 ];
        for ( float& x : lanes )
            x = f( );
        fluids_mesh_t mesh;
        mesh.verts.resize( 4 * u( ) );
        for ( float& x : mesh.verts )
            x = f( );
        mesh.edges.resize( 2 * u( ) );
        for ( uint32_t& x : mesh.edges )
            x = u( );
        mesh.triangles.resize( 15 * u( ) );
        for ( uint32_t& x : mesh.triangles )
            x = u( );
        mesh.total_area = f( );
        for ( int k = 0; k < 3; ++k )
        {
            float lo = mesh.verts[ k ], hi = mesh.verts[ k ];
            for ( std::size_t v = 0; v < mesh.verts.size( ); v += 4 )
            {
                lo = mesh.verts[ v + k ] < lo ? mesh.verts[ v + k ] : lo;
                hi = mesh.verts[ v + k ] > hi ? mesh.verts[ v + k ] : hi;
            }
            mesh.aabb_min[ k ] = lo;   // the box's own extents, as the case writes them
            mesh.aabb_max[ k ] = hi;
        }
        mesh.aabb_min[ 3 ] = mesh.aabb_max[ 3 ] = 1.0f;
        // One flat surface around the boat: identity rotation, translated by
        // the body's origin, extents 400, depths 50, floor -100, no waves.
        water_t water;
        water.skip_waves = false;
        water.surface_root = 0;
        water_node_t node{ };
        node.rect[ 0 ] = body.transform[ 12 ] - 500.0f;
        node.rect[ 1 ] = body.transform[ 13 ] - 500.0f;
        node.rect[ 2 ] = body.transform[ 12 ] + 500.0f;
        node.rect[ 3 ] = body.transform[ 13 ] + 500.0f;
        node.first = 0;
        node.second = -1;
        water.nodes.push_back( node );
        water_surface_t surface{ };
        surface.height = height;
        surface.below = surface.above = 50.0f;
        surface.inverse[ 0 ] = surface.inverse[ 5 ] = surface.inverse[ 10 ] = surface.inverse[ 15 ] = 1.0f;
        surface.inverse[ 12 ] = -body.transform[ 12 ];
        surface.inverse[ 13 ] = -body.transform[ 13 ];
        surface.inverse[ 14 ] = -body.transform[ 14 ];
        surface.floor = -100.0f;
        surface.half_x = surface.half_y = 400.0f;
        surface.wave_root = -1;
        water.surfaces.push_back( surface );
        fluid_solver_t solver{ };
        solver.mesh = &mesh;
        solver.vertices.resize( mesh.verts.size( ) / 4 );
        solver.edges.resize( mesh.edges.size( ) / 2 );
        std::memset( solver.vertices.data( ), 0x77, solver.vertices.size( ) * sizeof( water_vertex_t ) );
        std::memset( solver.edges.data( ), 0x77, solver.edges.size( ) * sizeof( fluid_edge_t ) );
        std::memset( &solver.volume, 0x77, 4 );
        std::memset( &solver.area, 0x77, 4 );
        std::memset( solver.acc, 0x77, sizeof( solver.acc ) );
        solver.floor_z = 0.0f;
        fluid_fake_t fake;
        float gravity[ 3 ] = { 0.0f, 0.0f, -19.612f };
        const char* what = nullptr;
        if ( !hull_fluid( h, body, solver, water, m, lanes, lanes + 3, fluid_fake_kernel, &fake, fluid_fake_gravity, gravity ) )
            what = "a path not recreated was reached";
        unsigned ours[ 11 ];
        std::memcpy( &ours[ 0 ], solver.offset, 12 );
        ours[ 3 ] = 0;
        std::memcpy( &ours[ 4 ], &solver.drag, 4 );
        std::memcpy( &ours[ 5 ], &solver.buoyancy, 4 );
        std::memcpy( &ours[ 6 ], &solver.velocity_scale, 4 );
        std::memcpy( &ours[ 7 ], solver.axis_drag, 12 );
        ours[ 10 ] = 0;
        for ( int k = 0; k < 11; ++k )
        {
            const unsigned r = u( );
            if ( !what && k != 3 && k != 10 && ours[ k ] != r )
                what = "a solver parameter";
        }
        unsigned va[ 2 ];
        std::memcpy( &va[ 0 ], &solver.volume, 4 );
        std::memcpy( &va[ 1 ], &solver.area, 4 );
        for ( unsigned x : va )
            if ( u( ) != x && !what )
                what = "the volume / area";
        for ( float x : lanes )
        {
            unsigned o;
            std::memcpy( &o, &x, 4 );
            if ( u( ) != o && !what )
                what = "a force / torque lane";
        }
        wet += solver.volume != 0.0f ? 1 : 0;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "hull fluid vector %d: %s", static_cast< int >( index ), what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuBoatHull_applyFluidForces (prepare + solve over a flat surface) matches retail over %d vectors, every lane "
                     "exact (%d wet)",
            static_cast< int >( g_golden_hull_fluid_count ), wet );
    else
        LOG_ERROR( "VuBoatHull_applyFluidForces FAILED %d of %d vectors", failed, static_cast< int >( g_golden_hull_fluid_count ) );
    return failed == 0;
}

// 0x4157C0 (the in-place 4x4 inverse) against retail: all 16 words, every
// bit - rigid, scaled, dense, singular and NaN inputs.
inline bool verify_hull_invert( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_hull_invert_count; ++index )
    {
        const unsigned* s = g_golden_hull_invert_words + g_golden_hull_invert_offsets[ index ];
        float m[ 16 ];
        std::memcpy( m, s, 64 );
        hull_invert( m );
        if ( std::memcmp( m, s + 16, 64 ) != 0 )
        {
            ++failed;
            LOG_ERROR( "hull invert vector %d differs", static_cast< int >( index ) );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "0x4157C0 (the in-place 4x4 inverse, translated by tools/sse2c.py) matches retail over %d matrices, every word exact",
            static_cast< int >( g_golden_hull_invert_count ) );
    else
        LOG_ERROR( "0x4157C0 FAILED %d of %d matrices", failed, static_cast< int >( g_golden_hull_invert_count ) );
    return failed == 0;
}

// VuBoatHull_applySteering @0x418EE0 against retail: the body's angular
// velocity (+336, all four words) after it.
inline bool verify_hull_steer( )
{
    int failed = 0, off = 0;
    for ( std::size_t index = 0; index < g_golden_hull_steer_count; ++index )
    {
        const unsigned* s = g_golden_hull_steer_words + g_golden_hull_steer_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        auto f = [ & ]( ) { return water_bits( *s++ ); };
        hull_steer_t h;
        h.speed_lo = f( );
        h.angle_lo = f( );
        h.speed_hi = f( );
        h.angle_hi = f( );
        h.angle_deep = f( );
        h.depth_lo = f( );
        h.depth_hi = f( );
        const float depth = f( );
        h.input = f( );
        h.input_off = u( );
        fluid_body_t body{ };
        for ( float& x : body.linear )
            x = f( );
        for ( float& x : body.angular )
            x = f( );
        const float w_in = f( );   // the angular velocity's w: overwritten
        float m[ 16 ];
        for ( float& x : m )
            x = f( );
        float out[ 4 ];
        hull_steer( h, body, depth, m, out );
        // The mechanical translation (steer.hxx), in place on the angular
        // velocity as retail is, from the same inputs.
        float gen[ 4 ] = { body.angular[ 0 ], body.angular[ 1 ], body.angular[ 2 ], w_in };
        const float lin[ 4 ] = { body.linear[ 0 ], body.linear[ 1 ], body.linear[ 2 ], 0.0f };
        hull_steer_gen( h, lin, gen, depth, m );
        off += h.input_off ? 1 : 0;
        if ( std::memcmp( out, s, 16 ) != 0 || std::memcmp( gen, s, 16 ) != 0 )
        {
            ++failed;
            LOG_ERROR( "hull steer vector %d differs", static_cast< int >( index ) );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuBoatHull_applySteering - hand-written AND asm2c-generated - match retail over %d vectors, the angular velocity exact (%d with the input off)",
            static_cast< int >( g_golden_hull_steer_count ), off );
    else
        LOG_ERROR( "VuBoatHull_applySteering FAILED %d of %d vectors", failed, static_cast< int >( g_golden_hull_steer_count ) );
    return failed == 0;
}

// VuBoatHull_update @0x416D20 - the asm2c-generated hull_update_gen - end to
// end against retail: the whole 672-byte rigid body after, hull +0x174 /
// +0x178, and the solver's volume, area, least depth and vectors (w lanes
// masked: retail's stale stack).
inline bool verify_hull_update( )
{
    int failed = 0, damped = 0, wet = 0;
    for ( std::size_t index = 0; index < g_golden_hull_update_count; ++index )
    {
        const unsigned* s = g_golden_hull_update_words + g_golden_hull_update_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        auto f = [ & ]( ) { return water_bits( *s++ ); };
        hull_fluid_t hf;
        for ( float& x : hf.offset )
            x = f( );
        for ( float& x : hf.boat_centre )
            x = f( );
        hf.drag = f( );
        hf.buoyancy = f( );
        hf.factor_min = f( );
        hf.factor_speed = f( );
        for ( float& x : hf.curve )
            x = f( );
        for ( float& x : hf.axis )
            x = f( );
        static solver_rigid_t b;
        std::memcpy( &b, s, sizeof( b ) );
        s += sizeof( b ) / 4;
        const float height = f( );
        hull_t h;
        for ( float& x : h.aero_point )
            x = f( );
        for ( float& x : h.damping )
            x = f( );
        h.aero = f( );
        h.slide = f( );
        h.aero_scale = f( );
        h.aero_off = u( ) != 0;
        hull_steer_t hs;
        hs.speed_lo = f( );
        hs.angle_lo = f( );
        hs.speed_hi = f( );
        hs.angle_hi = f( );
        hs.angle_deep = f( );
        hs.depth_lo = f( );
        hs.depth_hi = f( );
        hs.input = f( );
        hs.input_off = u( );
        const float f15C = f( ), f160 = f( ), f164 = f( );
        const uint8_t f174 = static_cast< uint8_t >( u( ) );
        const float f178 = f( ), tick = f( );
        fluids_mesh_t mesh;
        mesh.verts.resize( 4 * u( ) );
        for ( float& x : mesh.verts )
            x = f( );
        mesh.edges.resize( 2 * u( ) );
        for ( uint32_t& x : mesh.edges )
            x = u( );
        mesh.triangles.resize( 15 * u( ) );
        for ( uint32_t& x : mesh.triangles )
            x = u( );
        mesh.total_area = f( );
        for ( int k = 0; k < 3; ++k )
        {
            float lo = mesh.verts[ k ], hi = mesh.verts[ k ];
            for ( std::size_t v = 0; v < mesh.verts.size( ); v += 4 )
            {
                lo = mesh.verts[ v + k ] < lo ? mesh.verts[ v + k ] : lo;
                hi = mesh.verts[ v + k ] > hi ? mesh.verts[ v + k ] : hi;
            }
            mesh.aabb_min[ k ] = lo;
            mesh.aabb_max[ k ] = hi;
        }
        mesh.aabb_min[ 3 ] = mesh.aabb_max[ 3 ] = 1.0f;
        // The same one flat surface around the boat as verify_hull_fluid.
        water_t water;
        water.skip_waves = false;
        water.surface_root = 0;
        water_node_t node{ };
        node.rect[ 0 ] = b.origin[ 0 ] - 500.0f;
        node.rect[ 1 ] = b.origin[ 1 ] - 500.0f;
        node.rect[ 2 ] = b.origin[ 0 ] + 500.0f;
        node.rect[ 3 ] = b.origin[ 1 ] + 500.0f;
        node.first = 0;
        node.second = -1;
        water.nodes.push_back( node );
        water_surface_t surface{ };
        surface.height = height;
        surface.below = surface.above = 50.0f;
        surface.inverse[ 0 ] = surface.inverse[ 5 ] = surface.inverse[ 10 ] = surface.inverse[ 15 ] = 1.0f;
        surface.inverse[ 12 ] = -b.origin[ 0 ];
        surface.inverse[ 13 ] = -b.origin[ 1 ];
        surface.inverse[ 14 ] = -b.origin[ 2 ];
        surface.floor = -100.0f;
        surface.half_x = surface.half_y = 400.0f;
        surface.wave_root = -1;
        water.surfaces.push_back( surface );
        fluid_solver_t solver{ };
        solver.mesh = &mesh;
        solver.vertices.resize( mesh.verts.size( ) / 4 );
        solver.edges.resize( mesh.edges.size( ) / 2 );
        std::memset( solver.vertices.data( ), 0x77, solver.vertices.size( ) * sizeof( water_vertex_t ) );
        std::memset( solver.edges.data( ), 0x77, solver.edges.size( ) * sizeof( fluid_edge_t ) );
        std::memset( &solver.volume, 0x77, 4 );
        std::memset( &solver.area, 0x77, 4 );
        std::memset( &solver.min_depth, 0x77, 4 );
        std::memset( solver.acc, 0x77, sizeof( solver.acc ) );
        solver.floor_z = 0.0f;
        fluid_fake_t fake;
        float gravity[ 3 ] = { 0.0f, 0.0f, -19.612f };
        hull_ctx_t c{ h, hf, hs, f15C, f160, f164, f178, f174, b, solver, water, fluid_fake_kernel, &fake, fluid_fake_gravity, gravity, tick };
        const char* what = nullptr;
        int bad = -1;
        if ( !hull_update_gen( c ) )
            what = "a path not recreated was reached";
        const unsigned char* ours = reinterpret_cast< const unsigned char* >( &b );
        const unsigned char* retail = reinterpret_cast< const unsigned char* >( s );
        for ( int k = 0; k < 672 && !what; k += 4 )
        {
            const bool w_lane = ( k >= 432 && k < 464 && ( k - 432 ) % 16 == 12 ) || ( k >= 336 && k < 352 && k == 348 );
            if ( !w_lane && std::memcmp( ours + k, retail + k, 4 ) != 0 )
            {
                what = "a rigid body word";
                bad = k;
            }
        }
        s += 168;
        const unsigned h15C = u( ), h160 = u( ), h164 = u( ), h178 = u( ), h174 = u( );
        unsigned o178;
        std::memcpy( &o178, &c.f178, 4 );
        ( void )h15C;
        ( void )h160;
        ( void )h164;
        if ( !what && ( o178 != h178 || c.f174 != h174 ) )
            what = "hull +0x174 / +0x178";
        unsigned so[ 43 ];
        std::memcpy( &so[ 0 ], &solver.volume, 4 );
        std::memcpy( &so[ 1 ], &solver.area, 4 );
        std::memcpy( &so[ 2 ], &solver.min_depth, 4 );
        std::memcpy( &so[ 3 ], solver.acc, sizeof( solver.acc ) );
        for ( int k = 0; k < 43; ++k )
        {
            const unsigned r = u( );
            const int offset = 84 + 4 * k;
            if ( !what && !( offset >= 96 && ( offset - 96 ) % 16 == 12 ) && so[ k ] != r )
            {
                what = "a solver word";
                bad = offset;
            }
        }
        damped += f164 == 0.0f ? 1 : 0;
        wet += solver.volume != 0.0f ? 1 : 0;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "hull update vector %d: %s (+%d)", static_cast< int >( index ), what, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuBoatHull_update (asm2c-generated, calling the verified force methods) matches retail end to end over %d vectors: "
                     "the whole rigid body, the hull flags, the solver (%d through damping / slide / steering, %d wet)",
            static_cast< int >( g_golden_hull_update_count ), damped, wet );
    else
        LOG_ERROR( "VuBoatHull_update FAILED %d of %d vectors against @0x416D20", failed, static_cast< int >( g_golden_hull_update_count ) );
    return failed == 0;
}

// 0x414E90 (the boat tick path's force, asm2c-generated) against retail: the
// whole 672-byte rigid body after.
inline bool verify_boat_tick( )
{
    int failed = 0, moved = 0;
    for ( std::size_t index = 0; index < g_golden_boat_tick_count; ++index )
    {
        const unsigned* s = g_golden_boat_tick_words + g_golden_boat_tick_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        auto f = [ & ]( ) { return water_bits( *s++ ); };
        static solver_rigid_t b;
        float volume = 0.0f;
        tick_ctx_t c{ 0, 0, 0, &volume, 0, 0, 0, 0, 0, 0, b };
        c.f18 = f( );
        c.f1C = f( );
        c.boat_32C = static_cast< uint8_t >( u( ) );
        volume = f( );
        c.o22C_04 = f( );
        c.o22C_0C = f( );
        c.o22C_28 = f( );
        c.o22C0_30C = f( );
        c.o22C0_470 = f( );
        c.o22C0_32C = static_cast< uint8_t >( u( ) );
        float m[ 16 ];
        for ( float& x : m )
            x = f( );
        std::memcpy( &b, s, sizeof( b ) );
        s += sizeof( b ) / 4;
        boat_tick_414E90( c, m );
        int bad = -1;
        for ( int k = 0; k < 672 && bad < 0; k += 4 )
            if ( std::memcmp( reinterpret_cast< const unsigned char* >( &b ) + k, reinterpret_cast< const unsigned char* >( s ) + k, 4 ) != 0 )
                bad = k;
        moved += std::memcmp( b.total_force, s - 168 + 108, 12 ) != 0 ? 1 : 0;
        if ( bad >= 0 )
        {
            ++failed;
            LOG_ERROR( "boat tick vector %d: rigid body word +%d", static_cast< int >( index ), bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "0x414E90 (the boat tick path's force, asm2c-generated) matches retail over %d vectors, the whole rigid body exact "
                     "(%d with the force applied)",
            static_cast< int >( g_golden_boat_tick_count ), moved );
    else
        LOG_ERROR( "0x414E90 FAILED %d of %d vectors", failed, static_cast< int >( g_golden_boat_tick_count ) );
    return failed == 0;
}

// 0x40B5B0 (the linear step) against retail: 64 triplets with ties, empty /
// reversed ranges and NaN in each lane, every bit.
inline bool verify_linstep( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_linstep_count; ++index )
    {
        const unsigned* s = g_golden_linstep_words + g_golden_linstep_offsets[ index ];
        const float r = vu_linstep( water_bits( s[ 0 ] ), water_bits( s[ 1 ] ), water_bits( s[ 2 ] ) );
        unsigned o;
        std::memcpy( &o, &r, 4 );
        if ( o != s[ 3 ] )
        {
            ++failed;
            LOG_ERROR( "linstep vector %d: ours 0x%08X, retail 0x%08X", static_cast< int >( index ), o, s[ 3 ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "0x40B5B0 (the linear step) matches retail over %d triplets, every bit (ties, empty / reversed ranges, NaN)",
            static_cast< int >( g_golden_linstep_count ) );
    else
        LOG_ERROR( "0x40B5B0 FAILED %d of %d", failed, static_cast< int >( g_golden_linstep_count ) );
    return failed == 0;
}

// 0x41D570 (asm2c-generated) against retail: the returned float over its four
// paths, every bit.
inline bool verify_boat_d570( )
{
    int failed = 0, paths[ 4 ] = { };
    for ( std::size_t index = 0; index < g_golden_boat_d570_count; ++index )
    {
        const unsigned* s = g_golden_boat_d570_words + g_golden_boat_d570_offsets[ index ];
        static solver_rigid_t b;
        boat_view_t c{ water_bits( s[ 0 ] ), s[ 1 ], static_cast< uint8_t >( s[ 2 ] ), static_cast< uint8_t >( s[ 3 ] ), { }, 0, 0, 0, b };
        for ( int k = 0; k < 3; ++k )
            c.o50_60[ k ] = water_bits( s[ 4 + k ] );
        c.o22C_04 = water_bits( s[ 7 ] );
        c.o22C_0C = water_bits( s[ 8 ] );
        c.o22C0_470 = water_bits( s[ 9 ] );
        for ( int k = 0; k < 4; ++k )
            b.linear_velocity[ k ] = water_bits( s[ 10 + k ] );
        const float r = boat_41D570( c );
        unsigned o;
        std::memcpy( &o, &r, 4 );
        ++paths[ c.b32F ? 0 : c.i308 ? 1 : c.b32C ? 2 : 3 ];
        if ( o != s[ 14 ] )
        {
            ++failed;
            LOG_ERROR( "0x41D570 vector %d: ours 0x%08X, retail 0x%08X", static_cast< int >( index ), o, s[ 14 ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "0x41D570 (asm2c-generated) matches retail over %d vectors, every bit (paths %d / %d / %d / %d)",
            static_cast< int >( g_golden_boat_d570_count ), paths[ 0 ], paths[ 1 ], paths[ 2 ], paths[ 3 ] );
    else
        LOG_ERROR( "0x41D570 FAILED %d of %d", failed, static_cast< int >( g_golden_boat_d570_count ) );
    return failed == 0;
}

// 0x415220 (the O +0x30 curve, asm2c-generated with integer values) against
// retail: the returned float, every bit.
inline bool verify_curve( )
{
    int failed = 0, sampled = 0;
    for ( std::size_t index = 0; index < g_golden_curve_count; ++index )
    {
        const unsigned* s = g_golden_curve_words + g_golden_curve_offsets[ index ];
        float table[ 9 ];
        for ( int k = 0; k < 9; ++k )
            table[ k ] = water_bits( s[ 7 + k ] );
        o22c_view_t c{ water_bits( s[ 0 ] ), water_bits( s[ 1 ] ), water_bits( s[ 2 ] ), water_bits( s[ 3 ] ), water_bits( s[ 4 ] ),
            water_bits( s[ 5 ] ), table, static_cast< int32_t >( s[ 6 ] ), water_bits( s[ 16 ] ), water_bits( s[ 17 ] ),
            static_cast< uint8_t >( s[ 18 ] ) };
        const float x = water_bits( s[ 19 ] );
        const float r = o22c_415220( c, x );
        unsigned o;
        std::memcpy( &o, &r, 4 );
        sampled += ( x >= 0.0f && c.count34 > 0 ) ? 1 : 0;
        if ( o != s[ 20 ] )
        {
            ++failed;
            LOG_ERROR( "0x415220 vector %d: ours 0x%08X, retail 0x%08X", static_cast< int >( index ), o, s[ 20 ] );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "0x415220 (the O +0x30 curve, asm2c-generated with integer values) matches retail over %d vectors, every bit "
                     "(%d past the table guards)",
            static_cast< int >( g_golden_curve_count ), sampled );
    else
        LOG_ERROR( "0x415220 FAILED %d of %d", failed, static_cast< int >( g_golden_curve_count ) );
    return failed == 0;
}

// 0x531000 (the quaternion from a matrix, asm2c-generated with a stack int
// array) against retail: q's 4 words, every bit (pre-filled 0x77 both sides).
inline bool verify_quat( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_quat_count; ++index )
    {
        const unsigned* s = g_golden_quat_words + g_golden_quat_offsets[ index ];
        float a[ 16 ], q[ 4 ];
        std::memcpy( a, s, 64 );
        std::memset( q, 0x77, 16 );
        quat_from_rows( q, a );
        if ( std::memcmp( q, s + 16, 16 ) != 0 )
        {
            ++failed;
            LOG_ERROR( "0x531000 vector %d differs", static_cast< int >( index ) );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "0x531000 (the quaternion from a matrix, asm2c-generated) matches retail over %d matrices, every word "
                     "(positive trace, each largest-diagonal branch, scaled, NaN)",
            static_cast< int >( g_golden_quat_count ) );
    else
        LOG_ERROR( "0x531000 FAILED %d of %d", failed, static_cast< int >( g_golden_quat_count ) );
    return failed == 0;
}

// 0x41B4E0 (the quaternion inverse-multiply, asm2c-generated) against retail:
// the 4 words, every bit (pre-filled 0x77 both sides).
inline bool verify_qmul( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_qmul_count; ++index )
    {
        const unsigned* s = g_golden_qmul_words + g_golden_qmul_offsets[ index ];
        float a[ 4 ], b[ 4 ], out[ 4 ];
        std::memcpy( a, s, 16 );
        std::memcpy( b, s + 4, 16 );
        std::memset( out, 0x77, 16 );
        quat_inverse_mul( out, a, b );
        if ( std::memcmp( out, s + 8, 16 ) != 0 )
        {
            ++failed;
            LOG_ERROR( "0x41B4E0 vector %d differs", static_cast< int >( index ) );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "0x41B4E0 (the quaternion inverse-multiply, asm2c-generated) matches retail over %d vectors, every word (zero, tiny, "
                     "huge, NaN)",
            static_cast< int >( g_golden_qmul_count ) );
    else
        LOG_ERROR( "0x41B4E0 FAILED %d of %d", failed, static_cast< int >( g_golden_qmul_count ) );
    return failed == 0;
}

// 0x531330 (the quaternion to axis-angle, asm2c-generated) against retail:
// axis[0..2] and the angle, every bit (axis[3] is retail's stale stack). The
// acos is std::acos against the CRT's - see tools/maps/qaxis.py.
inline bool verify_qaxis( )
{
    int failed = 0;
    for ( std::size_t index = 0; index < g_golden_qaxis_count; ++index )
    {
        const unsigned* s = g_golden_qaxis_words + g_golden_qaxis_offsets[ index ];
        float q[ 4 ], axis[ 4 ], angle[ 1 ];
        std::memcpy( q, s, 16 );
        std::memset( axis, 0x77, 16 );
        std::memset( angle, 0x77, 4 );
        quat_to_axis_angle( q, axis, angle );
        if ( std::memcmp( axis, s + 4, 12 ) != 0 || std::memcmp( angle, s + 8, 4 ) != 0 )
        {
            ++failed;
            LOG_ERROR( "0x531330 vector %d differs", static_cast< int >( index ) );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "0x531330 (the quaternion to axis-angle, asm2c-generated) matches retail over %d vectors, every compared word "
                     "(w < 0, the identity path, |w| > 1, NaN)",
            static_cast< int >( g_golden_qaxis_count ) );
    else
        LOG_ERROR( "0x531330 FAILED %d of %d", failed, static_cast< int >( g_golden_qaxis_count ) );
    return failed == 0;
}

// 0x414760 (the per-record water force on the boat's tick path, asm2c-
// generated with its loop, byte-offset records and the water query) against
// retail: the whole 672-byte rigid body after, the 8-byte records at O +0xF8
// (pre-filled 0x77 both sides) and the fake kernel's calls in order.
// With `tick`, 0x414010 instead (the same image plus the body's basis, the
// boat centre, O +0x18 / +0x1C and the solver volume; it builds the matrix
// and calls 0x414760 then 0x414E90).
// With `thunk` (mode 2), tickThunk 0x41E5E0: the hull update's image too
// (verify_hull_update's fields, mesh and solver) on the same boat, body and
// water; the body is compared with the hull update's stale w lanes masked,
// then the hull flags and the solver words as verify_hull_update does.
template < class Offsets >
inline bool verify_boat_records( const unsigned* words, const Offsets* offsets, std::size_t count, int mode )
{
    const bool tick = mode >= 1, thunk = mode == 2;
    const char* name = thunk ? "tickThunk 0x41E5E0" : tick ? "0x414010" : "0x414760";
    int failed = 0, records = 0, submerged = 0, calls = 0, branch[ 3 ] = { }, gated = 0;
    for ( std::size_t index = 0; index < count; ++index )
    {
        const unsigned* s = words + offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        auto f = [ & ]( ) { return water_bits( *s++ ); };
        const unsigned nbytes = u( );
        std::vector< uint8_t > points( 4 * u( ) + 32 );
        std::memcpy( points.data( ), s, points.size( ) - 32 );
        s += ( points.size( ) - 32 ) / 4;
        const float f14 = f( ), f24 = f( );
        float of[ 6 ];
        for ( float& x : of )
            x = f( );
        const int32_t n = static_cast< int32_t >( u( ) );
        float table[ 9 ];
        for ( float& x : table )
            x = f( );
        const float p30C = f( ), p470 = f( ), f2FC = f( );
        const uint32_t i308 = u( );
        const uint8_t b32C = static_cast< uint8_t >( u( ) ), b32F = static_cast< uint8_t >( u( ) );
        float qv[ 3 ];
        for ( float& x : qv )
            x = f( );
        float m[ 16 ];
        for ( float& x : m )
            x = f( );
        const bool wet = u( ) != 0;
        const float height = f( );
        water_t water;
        water.skip_waves = false;
        water.surface_root = wet ? 0 : -1;
        water.nodes.resize( u( ) );
        for ( water_node_t& node : water.nodes )
        {
            for ( float& x : node.rect )
                x = f( );
            node.first = static_cast< int >( u( ) );
            node.second = static_cast< int >( u( ) );
            node.tag = u( );
        }
        const int wave_root = static_cast< int >( u( ) );
        fluid_fake_t fake;
        water.waves.resize( u( ) );
        for ( water_wave_t& w : water.waves )
        {
            w.centre[ 0 ] = f( );
            w.centre[ 1 ] = f( );
            w.radius = f( );
            fake.k.push_back( f( ) );
        }
        const int32_t ex0 = static_cast< int32_t >( u( ) ), ex1 = static_cast< int32_t >( u( ) );
        float f18 = 0.0f, f1C = 0.0f, volume = 0.0f, centre[ 3 ] = { };
        if ( tick )
        {
            f18 = f( );
            f1C = f( );
            volume = f( );
            for ( float& x : centre )
                x = f( );
        }
        float dt = 0.0f, f15C = 0.0f, f160 = 0.0f, f164 = 0.0f, f178 = 0.0f, g19[ 6 ] = { };
        uint8_t f174 = 0, b374 = 0;
        hull_fluid_t hf{ };
        hull_t h{ };
        hull_steer_t hs{ };
        fluids_mesh_t mesh;
        if ( thunk )
        {
            dt = f( );
            for ( float& x : hf.offset )
                x = f( );
            for ( float& x : hf.boat_centre )
                x = f( );
            hf.drag = f( );
            hf.buoyancy = f( );
            hf.factor_min = f( );
            hf.factor_speed = f( );
            for ( float& x : hf.curve )
                x = f( );
            for ( float& x : hf.axis )
                x = f( );
            for ( float& x : h.aero_point )
                x = f( );
            for ( float& x : h.damping )
                x = f( );
            h.aero = f( );
            h.slide = f( );
            h.aero_scale = f( );
            h.aero_off = u( ) != 0;
            hs.speed_lo = f( );
            hs.angle_lo = f( );
            hs.speed_hi = f( );
            hs.angle_hi = f( );
            hs.angle_deep = f( );
            hs.depth_lo = f( );
            hs.depth_hi = f( );
            hs.input = f( );
            hs.input_off = i308;   // boat +0x308: the same dword 0x41D570 reads
            f15C = f( );
            f160 = f( );
            f164 = f( );
            f174 = static_cast< uint8_t >( u( ) );
            f178 = f( );
            mesh.verts.resize( 4 * u( ) );
            for ( float& x : mesh.verts )
                x = f( );
            mesh.edges.resize( 2 * u( ) );
            for ( uint32_t& x : mesh.edges )
                x = u( );
            mesh.triangles.resize( 15 * u( ) );
            for ( uint32_t& x : mesh.triangles )
                x = u( );
            mesh.total_area = f( );
            for ( int k = 0; k < 3; ++k )
            {
                float lo = mesh.verts[ k ], hi = mesh.verts[ k ];
                for ( std::size_t v = 0; v < mesh.verts.size( ); v += 4 )
                {
                    lo = mesh.verts[ v + k ] < lo ? mesh.verts[ v + k ] : lo;
                    hi = mesh.verts[ v + k ] > hi ? mesh.verts[ v + k ] : hi;
                }
                mesh.aabb_min[ k ] = lo;
                mesh.aabb_max[ k ] = hi;
            }
            mesh.aabb_min[ 3 ] = mesh.aabb_max[ 3 ] = 1.0f;
            b374 = static_cast< uint8_t >( u( ) );
            for ( float& x : g19 )
                x = f( );
        }
        static solver_rigid_t b;
        std::memcpy( &b, s, 672 );
        s += 168;
        water_surface_t surface{ };
        surface.height = height;
        surface.below = surface.above = 50.0f;
        surface.inverse[ 0 ] = surface.inverse[ 5 ] = surface.inverse[ 10 ] = surface.inverse[ 15 ] = 1.0f;
        for ( int k = 0; k < 3; ++k )
            surface.inverse[ 12 + k ] = -b.origin[ k ];
        surface.floor = -100.0f;
        surface.half_x = surface.half_y = 400.0f;
        surface.wave_root = wave_root;
        water.surfaces.push_back( surface );
        boat_view_t boat{ f2FC, i308, b32C, b32F, { qv[ 0 ], qv[ 1 ], qv[ 2 ] }, of[ 0 ], of[ 1 ], p470, b };
        const o22c_view_t o{ of[ 0 ], of[ 1 ], of[ 2 ], of[ 3 ], of[ 4 ], of[ 5 ], table, n, p30C, p470, b32C };
        uint8_t out[ 64 ];
        std::memset( out, 0x77, sizeof( out ) );
        b4760_ctx_t c{ f14, f24, points.data( ), points.data( ) + nbytes, out, ex0, ex1, b, boat, o, water, fluid_fake_kernel, &fake };
        fluid_solver_t solver{ };
        solver.mesh = &mesh;
        solver.vertices.resize( mesh.verts.size( ) / 4 );
        solver.edges.resize( mesh.edges.size( ) / 2 );
        std::memset( solver.vertices.data( ), 0x77, solver.vertices.size( ) * sizeof( water_vertex_t ) );
        std::memset( solver.edges.data( ), 0x77, solver.edges.size( ) * sizeof( fluid_edge_t ) );
        std::memset( &solver.volume, 0x77, 4 );
        std::memset( &solver.area, 0x77, 4 );
        std::memset( &solver.min_depth, 0x77, 4 );
        std::memset( solver.acc, 0x77, sizeof( solver.acc ) );
        solver.floor_z = 0.0f;
        float gravity[ 3 ] = { 0.0f, 0.0f, -19.612f };
        tick_ctx_t t{ f18, f1C, b32C, thunk ? &solver.volume : &volume, of[ 0 ], of[ 1 ], of[ 4 ], p30C, p470, b32C, b };
        b4010_ctx_t c4010{ o, thunk ? hf.boat_centre : centre, c, t };
        hull_ctx_t hc{ h, hf, hs, f15C, f160, f164, f178, f174, b, solver, water, fluid_fake_kernel, &fake, fluid_fake_gravity, gravity, dt };
        b19c0_ctx_t c19{ g19[ 0 ], g19[ 1 ], g19[ 2 ], { g19[ 3 ], g19[ 4 ], g19[ 5 ] }, b };
        thunk_ctx_t tc{ hc, c4010, c19, b374 };
        gated += b374;
        float row6 = m[ 6 ];
        if ( tick )
            row6 = b.basis[ 9 ];   // 0x410B40's row 1 z
        const char* what = nullptr;
        if ( !( thunk ? boat_tick_thunk( tc, dt ) : tick ? boat_414010( c4010 ) : boat_414760( c, m ) ) )
            what = "a path not recreated was reached";
        int bad = -1;
        for ( int k = 0; k < 672 && !what; k += 4 )
        {
            // the hull update's stale w lanes (verify_hull_update's mask)
            const bool w_lane = thunk && ( ( k >= 432 && k < 464 && ( k - 432 ) % 16 == 12 ) || k == 348 );
            if ( !w_lane && std::memcmp( reinterpret_cast< const unsigned char* >( &b ) + k, s + k / 4, 4 ) != 0 )
            {
                what = "a rigid body word";
                bad = k;
            }
        }
        if ( !what && std::memcmp( out, s + 168, 64 ) != 0 )
            what = "an 8-byte record";
        s += 168 + 16;
        if ( thunk )
        {
            unsigned o178;
            std::memcpy( &o178, &hc.f178, 4 );
            if ( u( ) != o178 && !what )
                what = "hull +0x178";
            if ( u( ) != hc.f174 && !what )
                what = "hull +0x174";
            unsigned so[ 43 ];
            std::memcpy( &so[ 0 ], &solver.volume, 4 );
            std::memcpy( &so[ 1 ], &solver.area, 4 );
            std::memcpy( &so[ 2 ], &solver.min_depth, 4 );
            std::memcpy( &so[ 3 ], solver.acc, sizeof( solver.acc ) );
            for ( int k = 0; k < 43; ++k )
            {
                const unsigned r = u( );
                const int offset = 84 + 4 * k;
                if ( !what && !( offset >= 96 && ( offset - 96 ) % 16 == 12 ) && so[ k ] != r )
                {
                    what = "a solver word";
                    bad = offset;
                }
            }
        }
        const unsigned ncalls = u( );
        if ( !what && fake.log.size( ) != 3 * ncalls )
            what = "the number of kernel calls";
        for ( unsigned k = 0; k < 3 * ncalls; ++k )
            if ( u( ) != ( k < fake.log.size( ) ? fake.log[ k ] : ~0u ) && !what )
                what = "a kernel call";
        const int nrec = static_cast< int >( nbytes ) >> 5;
        records += nrec;
        calls += static_cast< int >( ncalls );
        for ( int r = 0; r < nrec; ++r )
            submerged += out[ 8 * r ] == 1 ? 1 : 0;
        if ( nrec > 0 )
            ++branch[ row6 > 0.865f ? 0 : row6 > 0.5f ? 1 : 2 ];
        if ( what )
        {
            ++failed;
            LOG_ERROR( "%s vector %d: %s (+%d)", name, static_cast< int >( index ), what, bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "%s (%s, asm2c-generated) matches retail over %d vectors, the body and the records exact (%d records, %d "
                     "submerged, %d kernel calls; force branches %d / %d / %d; %d through 0x4219C0)",
            name, thunk ? "the hull update, 0x414010, and 0x4219C0 behind the +0x374 gate, on one boat" : tick ? "the boat tick body: the matrix, 0x414760, 0x414E90" : "the per-record water force: a loop, byte-offset records, the water query",
            static_cast< int >( count ), records, submerged, calls, branch[ 0 ], branch[ 1 ], branch[ 2 ], gated );
    if ( failed )
        LOG_ERROR( "%s FAILED %d of %d", name, failed, static_cast< int >( count ) );
    return failed == 0;
}

inline bool verify_boat_4760( )
{
    return verify_boat_records( g_golden_boat_4760_words, g_golden_boat_4760_offsets, g_golden_boat_4760_count, 0 );
}

inline bool verify_boat_4010( )
{
    return verify_boat_records( g_golden_boat_4010_words, g_golden_boat_4010_offsets, g_golden_boat_4010_count, 1 );
}

inline bool verify_tick_thunk( )
{
    return verify_boat_records( g_golden_tick_thunk_words, g_golden_tick_thunk_offsets, g_golden_tick_thunk_count, 2 );
}

// VuBoatEntity slot 17 0x421980 with VuDynamicsImpl slots 15 / 11 / 25
// (engine dynamics/manager.hxx) against retail over the real vtable: the
// hook calls in order (the thread wait, the world's addRigidBody with the
// group / mask, each listener), body +0xD8, the boat's +0x2A0, +0xD and the
// step-callback list. Objects are logged as indices: the body 1, the boat
// +0x60 interface 2, listener k 10 + k, the thread object 20, the world 30.
struct boat_enable_log_t
{
    std::vector< uint32_t > words;
};

inline bool verify_boat_enable( )
{
    int failed = 0, added = 0, waited = 0;
    for ( std::size_t index = 0; index < g_golden_boat_enable_count; ++index )
    {
        const unsigned* s = g_golden_boat_enable_words + g_golden_boat_enable_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        uint8_t enabled = static_cast< uint8_t >( u( ) );
        vu_dynamics_t d{ };
        d.pending = static_cast< uint8_t >( u( ) );
        d.thread = u( );
        static vu_rigid_body_t b;
        std::memset( &b, 0, sizeof( b ) );
        body_word( b, 0xD8 ) = u( );
        const uint16_t group = static_cast< uint16_t >( u( ) ), mask = static_cast< uint16_t >( u( ) );
        std::memcpy( reinterpret_cast< unsigned char* >( &b ) + 0x2CA, &group, 2 );
        std::memcpy( reinterpret_cast< unsigned char* >( &b ) + 0x2CC, &mask, 2 );
        const unsigned nl = u( ), ns = u( );
        for ( unsigned k = 0; k < nl; ++k )
            d.listeners.push_back( reinterpret_cast< void* >( static_cast< uintptr_t >( 10 + k ) ) );
        for ( unsigned k = 0; k < ns; ++k )
            d.step_callbacks.push_back( reinterpret_cast< void* >( static_cast< uintptr_t >( u( ) ) ) );
        boat_enable_log_t log;
        d.hooks.user = &log;
        d.hooks.sync = [ ]( void*, vu_dynamics_t& dd ) { return vu_dynamics_sync( dd ); };
        d.hooks.wait = [ ]( void* user, uint32_t handle, int32_t timeout )
        {
            auto& w = static_cast< boat_enable_log_t* >( user )->words;
            w.insert( w.end( ), { 1u, 20u, handle, static_cast< uint32_t >( timeout ), 0u } );
            return true;
        };
        d.hooks.world_add = [ ]( void* user, vu_rigid_body_t&, int32_t g, int32_t m )
        {
            auto& w = static_cast< boat_enable_log_t* >( user )->words;
            w.insert( w.end( ), { 2u, 30u, 1u, static_cast< uint32_t >( g ), static_cast< uint32_t >( m ) } );
            return true;
        };
        d.hooks.listener = [ ]( void* user, void* l, vu_rigid_body_t& )
        {
            auto& w = static_cast< boat_enable_log_t* >( user )->words;
            w.insert( w.end( ), { 3u, static_cast< uint32_t >( reinterpret_cast< uintptr_t >( l ) ), 1u, 0u, 0u } );
            return true;
        };
        const char* what = nullptr;
        if ( !boat_enable( d, enabled, b, reinterpret_cast< void* >( static_cast< uintptr_t >( 2 ) ) ) )
            what = "a path not recreated was reached";
        if ( u( ) != body_word( b, 0xD8 ) && !what )
            what = "body +0xD8";
        if ( u( ) != enabled && !what )
            what = "boat +0x2A0";
        if ( u( ) != d.pending && !what )
            what = "+0xD";
        const unsigned size = u( ), nv = u( );
        if ( ( size != d.step_callbacks.size( ) || nv != d.step_callbacks.size( ) ) && !what )
            what = "the step list's size";
        for ( unsigned k = 0; k < nv; ++k )
        {
            const unsigned r = u( );
            if ( !what && ( k >= d.step_callbacks.size( ) || r != static_cast< uint32_t >( reinterpret_cast< uintptr_t >( d.step_callbacks[ k ] ) ) ) )
                what = "a step callback";
        }
        const unsigned nlog = u( );
        if ( 5 * nlog != log.words.size( ) && !what )
            what = "the number of hook calls";
        for ( unsigned k = 0; k < 5 * nlog; ++k )
            if ( u( ) != ( k < log.words.size( ) ? log.words[ k ] : ~0u ) && !what )
                what = "a hook call";
        added += nv > ns ? 1 : 0;
        for ( std::size_t k = 0; k < log.words.size( ); k += 5 )
            waited += log.words[ k ] == 1u ? 1 : 0;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "boat enable vector %d: %s", static_cast< int >( index ), what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuBoatEntity slot 17 0x421980 + VuDynamicsImpl slots 15 / 11 / 25 match retail over %d vectors: the hook calls "
                     "in order, the flags, the step list (%d enabled now, %d thread waits)",
            static_cast< int >( g_golden_boat_enable_count ), added, waited );
    else
        LOG_ERROR( "boat enable FAILED %d of %d", failed, static_cast< int >( g_golden_boat_enable_count ) );
    return failed == 0;
}

// VuMatrix_fromEuler 0x435640 (asm2c-generated, its three fsincos through
// x87_sincos - retail's own x87 sequence on this FPU) against retail's code
// run NATIVELY (tools/x86run via tools/x87golden.py; the Unicorn oracle
// computes fsincos through the host's libm and differs on 18 of these 1024 -
// every one a huge or infinite angle): the 16 words, every bit.
inline bool verify_euler_x87( )
{
    int failed = 0, nans = 0;
    for ( std::size_t index = 0; index < g_golden_euler_x87_count; ++index )
    {
        const unsigned* s = g_golden_euler_x87_words + g_golden_euler_x87_offsets[ index ];
        float e[ 3 ], m[ 16 ];
        std::memcpy( e, s, 12 );
        std::memset( m, 0x77, 64 );
        vu_matrix_from_euler( m, e );
        if ( std::memcmp( m, s + 3, 64 ) != 0 )
        {
            ++failed;
            LOG_ERROR( "0x435640 vector %d differs from the native run", static_cast< int >( index ) );
        }
        nans += m[ 0 ] != m[ 0 ] || m[ 5 ] != m[ 5 ] || m[ 10 ] != m[ 10 ] ? 1 : 0;
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuMatrix_fromEuler 0x435640 (asm2c-generated, x87 fsincos on the FPU) matches retail run NATIVELY over %d angle "
                     "triples, every bit (%d with NaN lanes: |angle| >= 2^63 / inf)",
            static_cast< int >( g_golden_euler_x87_count ), nans );
    else
        LOG_ERROR( "0x435640 FAILED %d of %d against the native run", failed, static_cast< int >( g_golden_euler_x87_count ) );
    return failed == 0;
}

// 0x4219C0 (the boat's force + torque toward its +0x3E8 euler orientation, asm2c-generated; its fromEuler is x87
// fsincos on the FPU) against retail: the whole 672-byte body after. The
// reference is the Unicorn oracle, kept to angles in [-pi, pi] where it
// matched the FPU on every one of tools/golden_x87.json's 1006 normal-range
// triples - a sample, not a proof.
inline bool verify_boat_19c0( )
{
    int failed = 0, moved = 0;
    for ( std::size_t index = 0; index < g_golden_boat_19c0_count; ++index )
    {
        const unsigned* s = g_golden_boat_19c0_words + g_golden_boat_19c0_offsets[ index ];
        static solver_rigid_t b;
        b19c0_ctx_t c{ water_bits( s[ 0 ] ), water_bits( s[ 1 ] ), water_bits( s[ 2 ] ),
            { water_bits( s[ 3 ] ), water_bits( s[ 4 ] ), water_bits( s[ 5 ] ) }, b };
        std::memcpy( &b, s + 6, 672 );
        boat_4219C0( c );
        int bad = -1;
        for ( int k = 0; k < 672 && bad < 0; k += 4 )
            if ( std::memcmp( reinterpret_cast< const unsigned char* >( &b ) + k, s + 6 + 168 + k / 4, 4 ) != 0 )
                bad = k;
        moved += std::memcmp( s + 6 + 112, s + 6 + 168 + 112, 12 ) != 0 ? 1 : 0;   // the torque (+448) moved
        if ( bad >= 0 )
        {
            ++failed;
            LOG_ERROR( "0x4219C0 vector %d: body word +%d", static_cast< int >( index ), bad );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "0x4219C0 (the boat's force + torque toward its +0x3E8 euler orientation, asm2c-generated, x87 fromEuler on the FPU) matches retail over %d vectors, "
                     "the whole body exact (%d torques moved; angles in [-pi, pi] - see the comment)",
            static_cast< int >( g_golden_boat_19c0_count ), moved );
    else
        LOG_ERROR( "0x4219C0 FAILED %d of %d", failed, static_cast< int >( g_golden_boat_19c0_count ) );
    return failed == 0;
}

// VuBoatEntity slot 18 0x421940 with VuDynamicsImpl slots 16 / 12 / 25
// against retail over the real vtable: the hook calls in order (the thread
// wait, the dispatcher's count and manifolds, the world's removeRigidBody,
// each listener's slot 1), every body's +0xE4 / +0xE8 (the woken ones),
// +0x2A0, +0xD and the step list after std::list::remove. Indices as in
// tools/oracle.py case_boat_disable: bodies 1..6 (1 the boat's), the boat
// +0x60 interface 100, listener k 10 + k, the thread 20, the world 30, the
// dispatcher 31.
struct boat_disable_log_t
{
    std::vector< uint32_t > words;
    vu_rigid_body_t* bodies;
    std::vector< std::pair< int, int > > pairs;
};

inline bool verify_boat_disable( )
{
    int failed = 0, woken = 0, removed = 0;
    static vu_rigid_body_t bodies[ 6 ];
    for ( std::size_t index = 0; index < g_golden_boat_disable_count; ++index )
    {
        const unsigned* s = g_golden_boat_disable_words + g_golden_boat_disable_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        uint8_t enabled = static_cast< uint8_t >( u( ) );
        vu_dynamics_t d{ };
        d.pending = static_cast< uint8_t >( u( ) );
        d.thread = u( );
        const unsigned nl = u( ), np = u( );
        boat_disable_log_t log;
        log.bodies = bodies;
        for ( unsigned k = 0; k < np; ++k )
        {
            const int a = static_cast< int >( u( ) );
            log.pairs.emplace_back( a, static_cast< int >( u( ) ) );
        }
        std::memset( bodies, 0, sizeof( bodies ) );
        for ( auto& b : bodies )
        {
            body_word( b, 0xE4 ) = u( );
            body_word( b, 0xE8 ) = u( );
        }
        for ( unsigned k = 0; k < nl; ++k )
            d.listeners.push_back( reinterpret_cast< void* >( static_cast< uintptr_t >( 10 + k ) ) );
        const unsigned nv = u( );
        for ( unsigned k = 0; k < nv; ++k )
            d.step_callbacks.push_back( reinterpret_cast< void* >( static_cast< uintptr_t >( u( ) ) ) );
        auto words = [ ]( void* user ) -> std::vector< uint32_t >& { return static_cast< boat_disable_log_t* >( user )->words; };
        d.hooks.user = &log;
        d.hooks.sync = [ ]( void*, vu_dynamics_t& dd ) { return vu_dynamics_sync( dd ); };
        d.hooks.wait = [ ]( void* user, uint32_t handle, int32_t timeout )
        {
            static_cast< boat_disable_log_t* >( user )->words.insert( static_cast< boat_disable_log_t* >( user )->words.end( ),
                { 1u, 20u, handle, static_cast< uint32_t >( timeout ), 0u } );
            return true;
        };
        d.hooks.manifold_count = [ ]( void* user )
        {
            auto& l = *static_cast< boat_disable_log_t* >( user );
            l.words.insert( l.words.end( ), { 6u, 31u, 0u, 0u, 0u } );
            return static_cast< int >( l.pairs.size( ) );
        };
        d.hooks.manifold_bodies = [ ]( void* user, int i, vu_rigid_body_t*& b0, vu_rigid_body_t*& b1 )
        {
            auto& l = *static_cast< boat_disable_log_t* >( user );
            l.words.insert( l.words.end( ), { 7u, 31u, static_cast< uint32_t >( i ), 0u, 0u } );
            b0 = &l.bodies[ l.pairs[ i ].first ];
            b1 = &l.bodies[ l.pairs[ i ].second ];
            return true;
        };
        d.hooks.world_remove = [ ]( void* user, vu_rigid_body_t& b )
        {
            auto& l = *static_cast< boat_disable_log_t* >( user );
            l.words.insert( l.words.end( ), { 4u, 30u, static_cast< uint32_t >( &b - l.bodies + 1 ), 0u, 0u } );
            return true;
        };
        d.hooks.listener_remove = [ ]( void* user, void* lp, vu_rigid_body_t& b )
        {
            auto& l = *static_cast< boat_disable_log_t* >( user );
            l.words.insert( l.words.end( ), { 5u, static_cast< uint32_t >( reinterpret_cast< uintptr_t >( lp ) ),
                static_cast< uint32_t >( &b - l.bodies + 1 ), 0u, 0u } );
            return true;
        };
        ( void )words;
        const char* what = nullptr;
        if ( !boat_disable( d, enabled, bodies[ 0 ], reinterpret_cast< void* >( static_cast< uintptr_t >( 100 ) ) ) )
            what = "a path not recreated was reached";
        for ( auto& b : bodies )
        {
            if ( u( ) != body_word( b, 0xE4 ) && !what )
                what = "a body's +0xE4";
            if ( u( ) != body_word( b, 0xE8 ) && !what )
                what = "a body's +0xE8";
        }
        if ( u( ) != enabled && !what )
            what = "boat +0x2A0";
        if ( u( ) != d.pending && !what )
            what = "+0xD";
        const unsigned size = u( ), no = u( );
        if ( ( size != d.step_callbacks.size( ) || no != d.step_callbacks.size( ) ) && !what )
            what = "the step list's size";
        for ( unsigned k = 0; k < no; ++k )
        {
            const unsigned r = u( );
            if ( !what && ( k >= d.step_callbacks.size( ) || r != static_cast< uint32_t >( reinterpret_cast< uintptr_t >( d.step_callbacks[ k ] ) ) ) )
                what = "a step callback";
        }
        const unsigned nlog = u( );
        if ( 5 * nlog != log.words.size( ) && !what )
            what = "the number of hook calls";
        for ( unsigned k = 0; k < 5 * nlog; ++k )
            if ( u( ) != ( k < log.words.size( ) ? log.words[ k ] : ~0u ) && !what )
                what = "a hook call";
        removed += static_cast< int >( nv ) - static_cast< int >( d.step_callbacks.size( ) );
        for ( auto& b : bodies )
            woken += body_word( b, 0xE8 ) == 0 ? 1 : 0;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "boat disable vector %d: %s", static_cast< int >( index ), what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuBoatEntity slot 18 0x421940 + VuDynamicsImpl slots 16 / 12 / 25 match retail over %d vectors: the hook calls in "
                     "order, the woken bodies, the step list (%d bodies woken, %d step entries removed)",
            static_cast< int >( g_golden_boat_disable_count ), woken, removed );
    else
        LOG_ERROR( "boat disable FAILED %d of %d", failed, static_cast< int >( g_golden_boat_disable_count ) );
    return failed == 0;
}

// VuBoatEntity slot 16 0x421870 (the physics teardown) with
// removeChildShapeByIndex 0x527030 against retail: the hook calls in order (a
// shape's slot 0, the body's slot 2, g 0x759004's vt+0x24), boat +0x290 ..
// +0x298, +0x134, +0x2B0, the compound's count / revision and its tree's root.
// +0x29C is retail's unwritten stack word (not compared). NOT MODELLED: the
// btDbvt's allocation bookkeeping (deletenode's btAlignedFree of the old
// m_free, m_free = the leaf, --m_leaves) - retail's log entries for that free
// (kind 4) are counted against "an old m_free existed", not reproduced.
struct boat_teardown_log_t
{
    std::vector< uint32_t > words;
};

inline bool verify_boat_teardown( )
{
    int failed = 0, compounds = 0, frees = 0;
    for ( std::size_t index = 0; index < g_golden_boat_teardown_count; ++index )
    {
        const unsigned* s = g_golden_boat_teardown_words + g_golden_boat_teardown_offsets[ index ];
        auto u = [ & ]( ) { return *s++; };
        auto f = [ & ]( ) { return water_bits( *s++ ); };
        const unsigned kind = u( ), has_child = u( ), has_free = u( ), handle = u( );
        float centre[ 4 ];
        for ( float& x : centre )
            x = f( );
        float f134 = f( );
        const int rev = static_cast< int >( u( ) );
        uint32_t h2B0 = handle;
        static vu_rigid_body_t b;
        compound_shape_t c;
        c.revision = rev;
        static const convex_shape_t child_tag{ };
        if ( kind >= 3 )
        {
            compound_child_t r{ };
            r.shape = has_child ? &child_tag : nullptr;
            c.children.push_back( r );
            c.has_root = true;
        }
        boat_teardown_log_t log;
        static int shape_tag, child_dummy;
        ( void )child_dummy;
        boat_teardown_t t{ centre, f134, h2B0, b, kind ? &shape_tag : nullptr, kind == 1 ? 8 : 31, kind >= 2 ? &c : nullptr, { } };
        t.hooks.user = &log;
        t.hooks.delete_shape = [ ]( void* user, const void* shape )
        {
            const uint32_t id = shape == &child_tag ? 2u : 1u;
            static_cast< boat_teardown_log_t* >( user )->words.insert( static_cast< boat_teardown_log_t* >( user )->words.end( ), { 1u, id, 1u, 0u } );
            return true;
        };
        t.hooks.body_slot2 = [ ]( void* user, vu_rigid_body_t& )
        {
            static_cast< boat_teardown_log_t* >( user )->words.insert( static_cast< boat_teardown_log_t* >( user )->words.end( ), { 2u, 3u, 0u, 0u } );
            return true;
        };
        t.hooks.release = [ ]( void* user, uint32_t hnd )
        {
            static_cast< boat_teardown_log_t* >( user )->words.insert( static_cast< boat_teardown_log_t* >( user )->words.end( ), { 3u, 4u, hnd, 0u } );
            return true;
        };
        const char* what = nullptr;
        if ( !boat_teardown( t ) )
            what = "a path not recreated was reached";
        unsigned w[ 3 ];
        std::memcpy( w, centre, 12 );
        for ( int k = 0; k < 3; ++k )
            if ( u( ) != w[ k ] && !what )
                what = "boat +0x290 .. +0x298";
        unsigned o134;
        std::memcpy( &o134, &f134, 4 );
        if ( u( ) != o134 && !what )
            what = "boat +0x134";
        if ( u( ) != h2B0 && !what )
            what = "boat +0x2B0";
        const unsigned count = u( ), revision = u( ), root = u( ), mfree = u( ), leaves = u( );
        ( void )mfree;
        ( void )leaves;
        if ( kind >= 2 && !what )
        {
            if ( count != c.children.size( ) )
                what = "the compound's count";
            else if ( static_cast< int >( revision ) != c.revision )
                what = "the compound's revision";
            else if ( ( root != 0 ) != c.has_root )
                what = "the tree's root";
        }
        const unsigned nlog = u( );
        std::vector< uint32_t > retail;
        int free_calls = 0;
        for ( unsigned k = 0; k < nlog; ++k )
        {
            const unsigned e0 = u( ), e1 = u( ), e2 = u( ), e3 = u( );
            if ( e0 == 4u )
            {
                ++free_calls;   // the btDbvt bookkeeping - not modelled
                continue;
            }
            retail.insert( retail.end( ), { e0, e1, e2, e3 } );
        }
        if ( retail != log.words && !what )
            what = "a hook call";
        if ( kind >= 3 && free_calls != static_cast< int >( has_free ) && !what )
            what = "retail's free count disagrees with the case (the harness)";
        compounds += kind >= 2 ? 1 : 0;
        frees += free_calls;
        if ( what )
        {
            ++failed;
            LOG_ERROR( "boat teardown vector %d: %s", static_cast< int >( index ), what );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "VuBoatEntity slot 16 0x421870 + removeChildShapeByIndex 0x527030 (one child) match retail over %d vectors: the hook "
                     "calls in order, the boat words, the compound (%d compounds; %d btDbvt frees - bookkeeping NOT modelled)",
            static_cast< int >( g_golden_boat_teardown_count ), compounds, frees );
    else
        LOG_ERROR( "boat teardown FAILED %d of %d", failed, static_cast< int >( g_golden_boat_teardown_count ) );
    return failed == 0;
}