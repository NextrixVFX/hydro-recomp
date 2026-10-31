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

// VuStaticPfxEntity. The effect document is a VuPfxAsset. A quad pattern
// integrates, then runs its child processes in file order (fountain spawn,
// then the ticks), then drops a particle whose life, alpha, scale, or
// world-Z has run out. Quads face the camera. Modulate multiplies the
// tile over the soft texture.
class c_pfx
{
public:
    int add( const c_archive& archive, const std::string& effect, const float* world, float scale, const float color[ 4 ], bool active )
    {
        const std::size_t slash = effect.find( '/' );
        if ( slash == std::string::npos || world == nullptr || color == nullptr )
            return -1;

        const value_t* document = load( archive, effect.substr( 0, slash ) );
        if ( document == nullptr )
            return -1;
        const value_t* system = walk( *document, effect.substr( slash + 1 ) );
        const value_t* patterns = system != nullptr ? find( *system, "ChildNodes" ) : nullptr;
        if ( patterns == nullptr || patterns->kind != value_t::kind_t::object )
            return -1;

        instance_t instance{};
        std::memcpy( instance.world, world, sizeof( instance.world ) );
        // VuStaticPfxEntity's setScale(Pfx Scale) as handed (0x5EB060). The
        // "0 means 1" that stood here was invented; no level authors a 0.
        instance.scale = scale;
        std::memcpy( instance.color, color, sizeof( instance.color ) );
        // An entity that starts active is one whose start() has run, which is
        // state 1 with the clock at zero. One that does not is state 0, and a
        // state 0 system's tick returns immediately.
        instance.state = active ? 1 : 0;
        instance.elapsed = 0.0f;
        instance.duration = property( *system, "Duration", 0.0f );
        for ( const auto& field : patterns->object )
        {
            const value_t* type = find( field.second, "Type" );
            if ( type == nullptr )
                continue;
            if ( type->text == "VuPfxQuadPattern" )
                instance.patterns.push_back( pattern_from( field.second ) );
            else if ( type->text == "VuPfxGeomPattern" )
                instance.patterns.push_back( geom_from( field.second ) );
        }
        // An effect whose patterns are all of a kind we do not build yet is
        // dropped silently otherwise, and that is exactly how the windmill
        // propellers went missing: they are VuPfxGeomPattern (a spinning
        // MESH), not VuPfxQuadPattern. Name the gap once per effect.
        if ( instance.patterns.empty( ) )
        {
            if ( m_unsupported.insert( effect ).second )
            {
                for ( const auto& field : patterns->object )
                {
                    const value_t* type = find( field.second, "Type" );
                    LOG_WARNING( "pfx %s is not built: child \"%s\" is a %s", effect.c_str( ),
                        field.first.c_str( ), type != nullptr ? type->text.c_str( ) : "(no type)" );
                }
            }
            return -1;
        }
        // The three lifecycle properties, now consumed. Left at verbose so
        // which effects carry one stays inspectable without a rebuild.
        if ( instance.duration != 0.0f )
            LOG_VERBOSE( "pfx %s: Duration %.3f", effect.c_str( ), instance.duration );
        for ( const pattern_t& pattern : instance.patterns )
        {
            if ( pattern.start_delay != 0.0f || pattern.max_count != 0 )
            {
                LOG_VERBOSE( "pfx %s: pattern Start Delay %.3f, Max Particle Count %d",
                    effect.c_str( ), pattern.start_delay, pattern.max_count );
            }
        }
        m_instances.push_back( std::move( instance ) );
        LOG_INFO( "pfx %s", effect.c_str( ) );
        return static_cast< int >( m_instances.size( ) ) - 1;
    }

    void move( int index, const float* world, float rate, float speed )
    {
        if ( world == nullptr || index < 0 || index >= static_cast< int >( m_instances.size( ) ) )
            return;
        instance_t& instance = m_instances[ static_cast< std::size_t >( index ) ];
        std::memcpy( instance.world, world, sizeof( instance.world ) );
        instance.scale = rate > 0.05f ? rate : 0.05f;
        instance.speed = speed;
    }

    void clear( )
    {
        m_instances.clear( );
    }

    // The submerged hull edges for this frame, six floats per segment: two
    // boat-local endpoints. VuFluidsMeshAsset ships an explicit EDGE LIST
    // alongside its vertices - an edge list a triangle mesh would not need -
    // which is what shows the emitter walks edges. Each segment's length
    // becomes its share of the spawn, so a long submerged edge throws more
    // spray than a short one and an edge out of the water throws none.
    void waterline( int index, const float* segments, int count )
    {
        if ( index < 0 || index >= static_cast< int >( m_instances.size( ) ) )
            return;
        instance_t& instance = m_instances[ static_cast< std::size_t >( index ) ];
        instance.segments.clear( );
        instance.cumulative.clear( );
        instance.submerged = 0.0f;
        if ( segments == nullptr || count <= 0 )
            return;
        instance.segments.assign( segments, segments + static_cast< std::size_t >( count ) * 6 );
        instance.cumulative.reserve( static_cast< std::size_t >( count ) );
        for ( int edge = 0; edge < count; ++edge )
        {
            const float* s = segments + static_cast< std::size_t >( edge ) * 6;
            const float dx = s[ 3 ] - s[ 0 ];
            const float dy = s[ 4 ] - s[ 1 ];
            const float dz = s[ 5 ] - s[ 2 ];
            instance.submerged += std::sqrt( dx * dx + dy * dy + dz * dz );
            instance.cumulative.push_back( instance.submerged );
        }
    }

    // How much hull edge is in the water right now, for the acceptance test.
    float submerged( int index ) const
    {
        if ( index < 0 || index >= static_cast< int >( m_instances.size( ) ) )
            return 0.0f;
        return m_instances[ static_cast< std::size_t >( index ) ].submerged;
    }

    // Live particle count across every instance, so emission can be scored
    // rather than eyeballed.
    int live( ) const
    {
        int total = 0;
        for ( const instance_t& instance : m_instances )
        {
            for ( const pattern_t& pattern : instance.patterns )
            {
                for ( const fountain_t& fountain : pattern.fountains )
                    total += static_cast< int >( fountain.particles.size( ) );
            }
        }
        return total;
    }

    // `model` resolves a geom pattern's Model Asset to its drawable parts.
    // c_pfx owns the simulation; the MODEL CACHE lives in c_world, so the
    // parts arrive through a callback exactly as the texture ids already do.
    template < typename Texture, typename Model >
    void draw( float dt, const float* eye, const float* target, Texture&& texture, Model&& model )
    {
        if ( dt < 0.0f )
            dt = 0.0f;
        if ( dt > 0.1f )
            dt = 0.1f;
        c_engine::drop( 11 );
        if ( eye == nullptr || target == nullptr )
            return;

        float right[ 3 ]{};
        float up[ 3 ]{};
        facing( eye, target, right, up );
        if ( m_card < 0 )
            m_card = card( );

        for ( instance_t& instance : m_instances )
        {
            tick_system( instance, eye, dt );
            // The draw passes are the system's slots 14 and 15, and neither
            // looks at the state - they are gated only on a global pfx enable
            // flag. A state 0 system holds no particles, so nothing is drawn
            // for it either way, and a state 2 one must still draw the
            // particles it is waiting out.
            for ( const pattern_t& pattern : instance.patterns )
            {
                if ( pattern.geom )
                {
                    draw_geom( pattern, instance, model );
                    continue;
                }
                const int soft = texture( pattern.texture );
                const int tile = texture( pattern.tile );
                const float surface[ 4 ] = { pattern.near_min, pattern.near_max, pattern.scroll_u, pattern.tile_scale };
                for ( const fountain_t& fountain : pattern.fountains )
                {
                    for ( const particle_t& particle : fountain.particles )
                    {
                        if ( particle.alpha <= 0.0f || particle.scale <= 0.0f )
                            continue;
                        float placed[ 16 ]{};
                        orient( placed, particle, right, up );
                        const float color[ 4 ] = {
                            particle.rgb[ 0 ] * instance.color[ 0 ],
                            particle.rgb[ 1 ] * instance.color[ 1 ],
                            particle.rgb[ 2 ] * instance.color[ 2 ],
                            particle.alpha * instance.color[ 3 ]
                        };
                        const float bias[ 2 ] = { particle.tile_u, particle.tile_v };
                        c_engine::place( m_card, soft, 0, 6, placed, color, 11, surface, tile, pattern.scroll_v, bias );
                    }
                }
            }
        }

    }

private:
    struct particle_t
    {
        float position[ 3 ]{};
        float velocity[ 3 ]{};
        // A GEOM particle is a model instance, so it needs a full 3-axis
        // orientation and angular velocity - not the quad's single rotation
        // about the view axis. VuPfxEmitGeomFountain_ctor @0x55e310 declares
        // Min/Max Rotation and Min/Max Angular Velocity as VuRotation3dProperty.
        float orient3[ 3 ]{};
        float angular[ 3 ]{};
        float rgb[ 3 ]{ 1.0f, 1.0f, 1.0f };
        float alpha = 1.0f;
        float scale = 1.0f;
        float age = 0.0f;
        float life = 1.0f;
        float rotation = 0.0f;
        float spin = 0.0f;
        float world_z = 0.0f;
        float tile_u = 0.0f;
        float tile_v = 0.0f;
    };

    struct fountain_t
    {
        float spawn_rate = 10.0f;
        float spawn_distance = 500.0f;
        int max_count = 0;
        // The emitter scales its particle ceiling by a per-instance factor at
        // this+28, separate from the rate scale at this+32. WHAT WRITES IT HAS
        // NOT BEEN FOUND, so this is 1.0 and the ceiling is the authored
        // Max Count exactly. Named and flagged rather than folded into the
        // rate scale, which would be a guess that two unrelated members are
        // the same number.
        float count_scale = 1.0f;
        float life[ 2 ]{ 1.0f, 1.0f };
        float scale[ 2 ]{ 1.0f, 1.0f };
        float position_min[ 3 ]{};
        float position_max[ 3 ]{};
        float velocity_min[ 3 ]{};
        float velocity_max[ 3 ]{};
        float rotation[ 2 ]{};
        float spin[ 2 ]{ 1.0f, 1.0f };
        // The geom fountain's four VuRotation3dProperty fields. THE SAME FOUR
        // NAMES ARE SCALARS ON THE QUAD FOUNTAIN (rotation/spin above), so
        // these are read per axis and must not share that reader.
        float rot_min[ 3 ]{};
        float rot_max[ 3 ]{};
        float ang_min[ 3 ]{};
        float ang_max[ 3 ]{};
        float world_z[ 2 ]{};
        bool hull = false;
        float min_speed = 0.0f;
        float max_speed = 40.0f;
        float color_min[ 4 ]{ 1.0f, 1.0f, 1.0f, 1.0f };
        float color_max[ 4 ]{ 1.0f, 1.0f, 1.0f, 1.0f };
        float tile_u[ 2 ]{};
        float tile_v[ 2 ]{};
        float accumulator = 0.0f;
        std::vector< particle_t > particles;
    };

    struct pattern_t
    {
        // VuPfxGeomPattern_ctor @0x58fb20: "Model Asset" (VuAssetNameProperty),
        // "Rejection Scale Modifier" (default 0x3F800000 = 1.0 in this[21]),
        // "Dynamic Lighting". The pattern holds a VuStaticModelInstance, so a
        // geom particle DRAWS A MODEL instead of a camera-facing card.
        // READ BUT NOT YET CONSUMED, both from the pattern's own params and
        // both confirmed twice over - once in the decompilation, once against
        // the schema, which puts them at exactly these offsets on
        // VuPfxQuadPattern, VuPfxGeomPattern and VuPfxTrailPattern alike.
        //
        // "Start Delay" (params +0x38) gates the WHOLE pattern tick in
        // VuPfxQuadPatternInstance_tick @0x591c30 and
        // VuPfxGeomPatternInstance_tick @0x58fea0:
        //     if ( systemInstance->elapsed > params->startDelay ) { ... }
        // Nothing happens below it - no integration, no child node ticks, no
        // retirement. This is NOT the tick nodes' "Start Delay" at node
        // +0x40, which is compared against a PARTICLE'S AGE. Two different
        // properties that share a name at different levels.
        //
        // "Max Particle Count" (params +0x30) is enforced by
        // VuPfxPatternInstance::allocateParticle @0x58b4e0, not by the
        // emitter: a count of 0 means unlimited, otherwise the allocator
        // refuses once the PATTERN holds that many. The emitter's own limit
        // that spawn() already honours is a different one, read from the
        // EMIT NODE's params at +0x34 and scaled by its count scale.
        float start_delay = 0.0f;
        int max_count = 0;
        bool geom = false;
        std::string model;
        float rejection_scale = 1.0f;
        bool lighting = false;
        std::string texture;
        std::string tile;
        float tile_scale = 1.0f;
        float scroll_u = 0.0f;
        float scroll_v = 0.0f;
        float near_min = 0.0f;
        float near_max = 0.0f;
        bool fade_in = false;
        float fade_in_duration = 0.0f;
        float fade_in_rate = 0.0f;
        float fade_out_time = 0.0f;
        float fade_out_rate = 0.0f;
        bool fade_alpha = false;
        float alpha_delay = 0.0f;
        float alpha_rate = 0.0f;
        bool accelerate = false;
        float accel_delay = 0.0f;
        float accel[ 3 ]{};
        struct scale_tick_t
        {
            float delay = 0.0f;
            float rate = 0.0f;
        };
        std::vector< scale_tick_t > scales;
        bool dampen = false;
        float dampen_delay = 0.0f;
        float dampen_amount = 0.0f;
        bool stretch = false;
        float stretch_delay = 0.0f;
        float stretch_rate = 0.0f;
        std::vector< fountain_t > fountains;
    };

    struct instance_t
    {
        float world[ 16 ]{};
        float scale = 1.0f;
        float speed = 0.0f;
        float color[ 4 ]{ 1.0f, 1.0f, 1.0f, 1.0f };
        // The system's state word, +124 on a VuPfxSystemInstance: 1 emits,
        // 2 finishes without emitting, 0 is idle. start() @0x58ac90 sets 1
        // and zeroes the clock; stop(false) @0x58acc0 sets 2 and stop(true)
        // sets 0 after freeing every particle. This replaces the `active`
        // bool, which could not express the finishing state at all.
        int state = 0;
        // +164, the system's own clock. Zeroed by start(), so it measures
        // time since ACTIVATION, not since load.
        float elapsed = 0.0f;
        // Submerged hull edges in boat-local space, six floats per segment
        // (two endpoints), refreshed every frame by the caller, plus the
        // running sum of their lengths. The hull emitter walks EDGES and
        // weights them by length, so sampling has to be proportional to
        // length rather than uniform over a point list - see waterline().
        std::vector< float > segments;
        std::vector< float > cumulative;
        float submerged = 0.0f;
        // READ BUT NOT YET CONSUMED. VuPfxSystem's ONE property, "Duration"
        // (params +0x30, default 0.0), drives the system's three-state word
        // in VuPfxSystemInstance::tick @0x58ad70:
        //
        //   if ( state != 0 )
        //   {
        //       elapsed += dt;
        //       for each pattern: tick( dt ); accumulate its AABB and count
        //       if ( state == 2 && count == 0 )                  state = 0;
        //       if ( Duration > 0 && state == 1 && elapsed >= Duration )
        //                                                        state = 2;
        //   }
        //
        // State 1 emits, 2 finishes without emitting, 0 is idle;
        // VuPfxPatternInstance::allocateParticle @0x58b4e0 returns NULL
        // unless the state is 1, which is the ONLY thing that stops
        // emission - neither pattern tick nor the emitter reads the state.
        // start() @0x58ac90 sets state 1 and zeroes elapsed, so the clock is
        // per-activation. stop(true) @0x58acc0 frees every particle and goes
        // straight to 0; stop(false) goes to 2. A Duration of 0 never
        // expires, and 0 is the constructor's default.
        float duration = 0.0f;
        std::vector< pattern_t > patterns;
    };

    // One geom particle, placed as GEOMETRY. Its matrix is the particle's
    // Euler orientation scaled by its own scale, translated to the particle
    // position and then carried by the instance's world transform - the same
    // composition order the quad path uses, with a model in place of the card.
    //
    // Group 11 is the pfx placement group that drop(11) clears at the top of
    // every draw, so placing each frame leaks nothing.
    template < typename Model >
    void draw_geom( const pattern_t& pattern, const instance_t& instance, Model&& model )
    {
        if ( pattern.model.empty( ) )
            return;
        const auto& parts = model( pattern.model );
        if ( parts.empty( ) )
            return;
        for ( const fountain_t& fountain : pattern.fountains )
        {
            for ( const particle_t& particle : fountain.particles )
            {
                if ( particle.alpha <= 0.0f || particle.scale <= 0.0f )
                    continue;
                // VuPfxGeomPatternInstance's draw @0x5906a0 composes, in this
                // order: a uniform SCALE (particle scale x the pattern's),
                // then the particle's Euler ROTATION (VuMatrix_fromEuler on
                // the particle's +68 field), then a TRANSLATION by the
                // particle's position, then the emitter's matrix - so the
                // particle's position is EMITTER-LOCAL there.
                //
                // Our spawn() stores positions in WORLD space (it transforms
                // the authored offset by the emitter matrix up front, which is
                // what the quad path relies on), so the same result is reached
                // by composing the emitter's ROTATION BASIS with the particle's
                // own rotation and taking the translation from the particle.
                // FLAGGED as an algebraic rearrangement, not a different
                // transform: identical output, one matrix multiply fewer.
                float local[ 16 ]{};
                euler_scaled( local, particle.orient3, particle.scale * instance.scale );
                float basis[ 16 ]{};
                std::memcpy( basis, instance.world, sizeof( basis ) );
                basis[ 12 ] = basis[ 13 ] = basis[ 14 ] = 0.0f;
                float placed[ 16 ]{};
                math_multiply( placed, basis, local );
                placed[ 12 ] = particle.position[ 0 ];
                placed[ 13 ] = particle.position[ 1 ];
                placed[ 14 ] = particle.position[ 2 ];
                const float color[ 4 ] = {
                    particle.rgb[ 0 ] * instance.color[ 0 ],
                    particle.rgb[ 1 ] * instance.color[ 1 ],
                    particle.rgb[ 2 ] * instance.color[ 2 ],
                    particle.alpha * instance.color[ 3 ]
                };
                for ( const auto& part : parts )
                {
                    if ( part.geometry < 0 )
                        continue;
                    float node[ 16 ]{};
                    math_multiply( node, placed, part.node );
                    c_engine::place( part.geometry, part.texture, part.index_start,
                        part.index_count, node, color, 11, nullptr, part.detail );
                }
            }
        }
    }

    // Z * Y * X Euler, uniformly scaled. The authored values are degrees and
    // were converted on the way in.
    static void euler_scaled( float* matrix, const float* angles, float scale )
    {
        const float sx = std::sin( angles[ 0 ] ), cx = std::cos( angles[ 0 ] );
        const float sy = std::sin( angles[ 1 ] ), cy = std::cos( angles[ 1 ] );
        const float sz = std::sin( angles[ 2 ] ), cz = std::cos( angles[ 2 ] );
        math_identity( matrix );
        matrix[ 0 ] = ( cy * cz ) * scale;
        matrix[ 1 ] = ( cy * sz ) * scale;
        matrix[ 2 ] = ( -sy ) * scale;
        matrix[ 4 ] = ( sx * sy * cz - cx * sz ) * scale;
        matrix[ 5 ] = ( sx * sy * sz + cx * cz ) * scale;
        matrix[ 6 ] = ( sx * cy ) * scale;
        matrix[ 8 ] = ( cx * sy * cz + sx * sz ) * scale;
        matrix[ 9 ] = ( cx * sy * sz - sx * cz ) * scale;
        matrix[ 10 ] = ( cx * cy ) * scale;
    }

    static const value_t* properties_of( const value_t& node )
    {
        const value_t* properties = find( node, "Properties" );
        return properties != nullptr ? properties : &node;
    }

    static float property( const value_t& node, const char* key, float fallback )
    {
        return number( find( *properties_of( node ), key ), fallback );
    }

    static std::string text_property( const value_t& node, const char* key )
    {
        const value_t* value = find( *properties_of( node ), key );
        if ( value != nullptr && value->kind == value_t::kind_t::text )
            return value->text;
        return {};
    }

    static float channel( const value_t& node, const char* key, const char* axis, float fallback )
    {
        const value_t* value = find( *properties_of( node ), key );
        if ( value == nullptr )
            return fallback;
        const value_t* component = find( *value, axis );
        return component != nullptr ? number( component, fallback ) : fallback;
    }

    // The emitter draws from ran1, the same Park-Miller-behind-a-shuffle
    // generator the ocean's spectrum uses: VuPfxEmitFountain_emit @0x55D030
    // inlines its step over the globals at 0x736810 (seed), 0x736814 (last)
    // and 0x736818 (the 32 entry table). What stood here before was an
    // invented linear congruential generator (1664525 / 1013904223), which
    // produces a completely different sequence from retail's.
    static float unit( )
    {
        return m_random.unit( );
    }

    // (1 - t) * min + max * t, in the order the emitter writes it.
    static float range( float min_value, float max_value )
    {
        return m_random.range( min_value, max_value );
    }

    static void transform_point( const float* matrix, const float* local, float* out )
    {
        out[ 0 ] = matrix[ 0 ] * local[ 0 ] + matrix[ 4 ] * local[ 1 ] + matrix[ 8 ] * local[ 2 ] + matrix[ 12 ];
        out[ 1 ] = matrix[ 1 ] * local[ 0 ] + matrix[ 5 ] * local[ 1 ] + matrix[ 9 ] * local[ 2 ] + matrix[ 13 ];
        out[ 2 ] = matrix[ 2 ] * local[ 0 ] + matrix[ 6 ] * local[ 1 ] + matrix[ 10 ] * local[ 2 ] + matrix[ 14 ];
    }

    static void transform_direction( const float* matrix, const float* local, float* out )
    {
        out[ 0 ] = matrix[ 0 ] * local[ 0 ] + matrix[ 4 ] * local[ 1 ] + matrix[ 8 ] * local[ 2 ];
        out[ 1 ] = matrix[ 1 ] * local[ 0 ] + matrix[ 5 ] * local[ 1 ] + matrix[ 9 ] * local[ 2 ];
        out[ 2 ] = matrix[ 2 ] * local[ 0 ] + matrix[ 6 ] * local[ 1 ] + matrix[ 10 ] * local[ 2 ];
    }

    static void facing( const float* eye, const float* target, float* right, float* up )
    {
        float forward[ 3 ] = { target[ 0 ] - eye[ 0 ], target[ 1 ] - eye[ 1 ], target[ 2 ] - eye[ 2 ] };
        math_normalize( forward );
        const float world_up[ 3 ] = { 0.0f, 0.0f, 1.0f };
        math_cross( right, world_up, forward );
        math_normalize( right );
        math_cross( up, forward, right );
        right[ 0 ] = -right[ 0 ];
        right[ 1 ] = -right[ 1 ];
        right[ 2 ] = -right[ 2 ];
    }

    static void orient( float* matrix, const particle_t& particle, const float* right, const float* up )
    {
        const float cosine = std::cos( particle.rotation );
        const float sine = std::sin( particle.rotation );
        math_identity( matrix );
        for ( int index = 0; index < 3; ++index )
        {
            const float axis_x = right[ index ] * cosine + up[ index ] * sine;
            const float axis_y = up[ index ] * cosine - right[ index ] * sine;
            const float flatten = particle.world_z > 0.0f ? particle.world_z : 1.0f;
            matrix[ index ] = axis_x * particle.scale;
            matrix[ 4 + index ] = axis_y * particle.scale * flatten;
        }
        matrix[ 12 ] = particle.position[ 0 ];
        matrix[ 13 ] = particle.position[ 1 ];
        matrix[ 14 ] = particle.position[ 2 ];
    }

    static int card( )
    {
        mesh_vertex_t vertices[ 4 ]{};
        const float corner[ 4 ][ 2 ] = { { -0.5f, -0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f }, { -0.5f, 0.5f } };
        const float uv[ 4 ][ 2 ] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };
        for ( int index = 0; index < 4; ++index )
        {
            vertices[ index ].position[ 0 ] = corner[ index ][ 0 ];
            vertices[ index ].position[ 1 ] = corner[ index ][ 1 ];
            vertices[ index ].normal[ 2 ] = 1.0f;
            vertices[ index ].uv[ 0 ] = uv[ index ][ 0 ];
            vertices[ index ].uv[ 1 ] = uv[ index ][ 1 ];
            vertices[ index ].color[ 0 ] = 1.0f;
            vertices[ index ].color[ 1 ] = 1.0f;
            vertices[ index ].color[ 2 ] = 1.0f;
            vertices[ index ].color[ 3 ] = 1.0f;
        }
        const unsigned short indices[ 6 ] = { 0, 1, 2, 0, 2, 3 };
        return c_engine::geometry( vertices, 4, indices, 6 );
    }

    const value_t* load( const c_archive& archive, const std::string& asset )
    {
        const auto existing = m_documents.find( asset );
        if ( existing != m_documents.end( ) )
            return &existing->second;

        std::vector< std::uint8_t > bytes;
        const std::string path = "VuPfxAsset/" + asset;
        value_t parsed{};
        if ( !archive.read( path.c_str( ), bytes ) )
        {
            LOG_ERROR( "pfx %s", path.c_str( ) );
            m_documents.emplace( asset, parsed );
            return nullptr;
        }
        parsed = c_reader( bytes ).parse( );
        const auto inserted = m_documents.emplace( asset, std::move( parsed ) );
        return &inserted.first->second;
    }

    static const value_t* walk( const value_t& root, const std::string& path )
    {
        const value_t* cursor = find( root, "ChildNodes" );
        std::size_t start = 0;
        const value_t* node = nullptr;
        while ( cursor != nullptr && start < path.size( ) )
        {
            const std::size_t slash = path.find( '/', start );
            const std::string part = path.substr( start, slash == std::string::npos ? std::string::npos : slash - start );
            node = find( *cursor, part );
            if ( node == nullptr )
                return nullptr;
            if ( slash == std::string::npos )
                return node;
            start = slash + 1;
            cursor = find( *node, "ChildNodes" );
        }
        return node;
    }

    static fountain_t fountain_from( const value_t& node, bool hull )
    {
        fountain_t fountain{};
        fountain.hull = hull;
        fountain.spawn_rate = property( node, hull ? "Max Spawn Per Second" : "Spawn Per Second", hull ? 100.0f : 10.0f );
        fountain.spawn_distance = property( node, "Spawn Distance", 500.0f );
        fountain.max_count = static_cast< int >( property( node, "Max Spawn Count", 0.0f ) );
        fountain.life[ 0 ] = property( node, "Min Lifespan", 1.0f );
        fountain.life[ 1 ] = property( node, "Max Lifespan", fountain.life[ 0 ] );
        fountain.scale[ 0 ] = property( node, "Min Scale", 1.0f );
        fountain.scale[ 1 ] = property( node, "Max Scale", fountain.scale[ 0 ] );
        const char* axes[ 3 ] = { "X", "Y", "Z" };
        for ( int index = 0; index < 3; ++index )
        {
            fountain.position_min[ index ] = channel( node, "Min Position", axes[ index ], 0.0f );
            fountain.position_max[ index ] = channel( node, "Max Position", axes[ index ], fountain.position_min[ index ] );
            fountain.velocity_min[ index ] = channel( node, "Min Linear Velocity", axes[ index ], 0.0f );
            fountain.velocity_max[ index ] = channel( node, "Max Linear Velocity", axes[ index ], fountain.velocity_min[ index ] );
        }
        const float degrees = 0.01745329252f;
        fountain.rotation[ 0 ] = property( node, "Min Rotation", 0.0f ) * degrees;
        fountain.rotation[ 1 ] = property( node, "Max Rotation", 0.0f ) * degrees;
        fountain.spin[ 0 ] = property( node, "Min Angular Velocity", 57.2957795f ) * degrees;
        fountain.spin[ 1 ] = property( node, "Max Angular Velocity", 57.2957795f ) * degrees;
        fountain.world_z[ 0 ] = property( node, "Min World Scale Z", 0.0f );
        fountain.world_z[ 1 ] = property( node, "Max World Scale Z", fountain.world_z[ 0 ] );
        if ( hull )
        {
            fountain.min_speed = property( node, "Min Spawn Boat Speed", 0.0f );
            fountain.max_speed = property( node, "Max Spawn Boat Speed", 40.0f );
            // The hull emitter has no authored world-Z range. The scale-Z tick
            // flattens the card from full size down to nothing.
            if ( fountain.world_z[ 0 ] == 0.0f && fountain.world_z[ 1 ] == 0.0f )
            {
                fountain.world_z[ 0 ] = 1.0f;
                fountain.world_z[ 1 ] = 1.0f;
            }
        }
        const char* channels[ 4 ] = { "R", "G", "B", "A" };
        for ( int index = 0; index < 4; ++index )
        {
            fountain.color_min[ index ] = channel( node, "Min Color", channels[ index ], 255.0f ) / 255.0f;
            fountain.color_max[ index ] = channel( node, "Max Color", channels[ index ], 255.0f ) / 255.0f;
        }
        fountain.tile_u[ 0 ] = property( node, "Min Tile Offset U", 0.0f );
        fountain.tile_u[ 1 ] = property( node, "Max Tile Offset U", fountain.tile_u[ 0 ] );
        fountain.tile_v[ 0 ] = property( node, "Min Tile Offset V", 0.0f );
        fountain.tile_v[ 1 ] = property( node, "Max Tile Offset V", fountain.tile_v[ 0 ] );
        return fountain;
    }

    // VuPfxGeomPattern, from VuPfxGeomPattern_ctor @0x58fb20 and the registry
    // @0x51c840. Its allowed child processes are the SAME tick family the quad
    // pattern registers (VuPfxTickAlpha, VuPfxTickAlphaInOut, VuPfxTickScale)
    // plus VuPfxEmitGeomFountain, so the shared reader below handles the ticks
    // and only the fountain and the three pattern properties differ.
    static pattern_t geom_from( const value_t& node )
    {
        pattern_t pattern = pattern_from( node );
        pattern.geom = true;
        pattern.model = text_property( node, "Model Asset" );
        pattern.rejection_scale = property( node, "Rejection Scale Modifier", 1.0f );
        pattern.lighting = property( node, "Dynamic Lighting", 0.0f ) > 0.5f;
        const value_t* children = find( node, "ChildNodes" );
        if ( children == nullptr || children->kind != value_t::kind_t::object )
            return pattern;
        for ( const auto& field : children->object )
        {
            const value_t* type = find( field.second, "Type" );
            if ( type == nullptr || type->kind != value_t::kind_t::text )
                continue;
            if ( type->text != "VuPfxEmitGeomFountain" )
            {
                // pattern_from above already walked these same children and
                // named anything it could not build, so the emitter is the
                // only thing left to pick out here.
                continue;
            }
            // The spawn schema is the shared VuPfxEmitFountain base
            // (@0x55c8e0), so fountain_from reads all of it; only the four
            // VuRotation3dProperty fields are the geom fountain's own, and
            // they are VEC3s OF DEGREES - checked at the property class's
            // vtable slots 9/10 (x0.017453292 in, x57.29578 out).
            fountain_t fountain = fountain_from( field.second, false );
            const char* axes[ 3 ] = { "X", "Y", "Z" };
            for ( int axis = 0; axis < 3; ++axis )
            {
                fountain.rot_min[ axis ] = channel( field.second, "Min Rotation", axes[ axis ], 0.0f ) * 0.017453292f;
                fountain.rot_max[ axis ] = channel( field.second, "Max Rotation", axes[ axis ], 0.0f ) * 0.017453292f;
                fountain.ang_min[ axis ] = channel( field.second, "Min Angular Velocity", axes[ axis ], 0.0f ) * 0.017453292f;
                fountain.ang_max[ axis ] = channel( field.second, "Max Angular Velocity", axes[ axis ], 0.0f ) * 0.017453292f;
            }
            // fountain_from also read those four names as SCALARS, which the
            // quad fountain authors but this one does not. Clear them so the
            // card rotation cannot fight the 3-axis orientation.
            fountain.rotation[ 0 ] = fountain.rotation[ 1 ] = 0.0f;
            fountain.spin[ 0 ] = fountain.spin[ 1 ] = 0.0f;
            pattern.fountains.push_back( std::move( fountain ) );
        }
        return pattern;
    }

    static pattern_t pattern_from( const value_t& node )
    {
        pattern_t pattern{};
        pattern.start_delay = property( node, "Start Delay", 0.0f );
        pattern.max_count = static_cast< int >( property( node, "Max Particle Count", 0.0f ) );
        pattern.texture = text_property( node, "Texture Asset" );
        pattern.tile = text_property( node, "Tile Texture Asset" );
        pattern.tile_scale = property( node, "Tile Scale", 1.0f );
        pattern.scroll_u = property( node, "Tile Scroll Speed U", 0.0f );
        pattern.scroll_v = property( node, "Tile Scroll Speed V", 0.0f );
        pattern.near_min = property( node, "Near Fade Min", 0.0f );
        pattern.near_max = property( node, "Near Fade Max", pattern.near_min );
        const value_t* children = find( node, "ChildNodes" );
        if ( children == nullptr || children->kind != value_t::kind_t::object )
            return pattern;
        for ( const auto& field : children->object )
        {
            const value_t* type = find( field.second, "Type" );
            if ( type == nullptr || type->kind != value_t::kind_t::text )
                continue;
            if ( type->text == "VuPfxEmitQuadFountain" || type->text == "VuPfxEmitBoatThrustQuadFountain" )
                pattern.fountains.push_back( fountain_from( field.second, false ) );
            else if ( type->text == "VuPfxEmitBoatHull" )
                pattern.fountains.push_back( fountain_from( field.second, true ) );
            else if ( type->text == "VuPfxTickAlphaInOut" )
            {
                pattern.fade_in = true;
                pattern.fade_in_duration = property( field.second, "Fade In Duration", 0.0f );
                pattern.fade_in_rate = property( field.second, "Fade In Rate", 0.0f );
                pattern.fade_out_time = property( field.second, "Fade Out Start Time", 0.0f );
                pattern.fade_out_rate = property( field.second, "Fade Out Rate", 0.0f );
            }
            else if ( type->text == "VuPfxTickAlpha" )
            {
                pattern.fade_alpha = true;
                pattern.alpha_delay = property( field.second, "Start Delay", 0.0f );
                pattern.alpha_rate = property( field.second, "Rate", 0.0f );
            }
            else if ( type->text == "VuPfxTickLinearAcceleration" )
            {
                pattern.accelerate = true;
                pattern.accel_delay = property( field.second, "Start Delay", 0.0f );
                pattern.accel[ 0 ] = property( field.second, "Accel X", 0.0f );
                pattern.accel[ 1 ] = property( field.second, "Accel Y", 0.0f );
                pattern.accel[ 2 ] = property( field.second, "Accel Z", 0.0f );
            }
            else if ( type->text == "VuPfxTickScale" )
            {
                pattern_t::scale_tick_t tick{};
                tick.delay = property( field.second, "Start Delay", 0.0f );
                // VuPercentageProperty stores the file number times 0.01.
                tick.rate = property( field.second, "Rate", 0.0f ) * 0.01f;
                pattern.scales.push_back( tick );
            }
            else if ( type->text == "VuPfxTickDampenVelocity" )
            {
                pattern.dampen = true;
                pattern.dampen_delay = property( field.second, "Start Delay", 0.0f );
                pattern.dampen_amount = property( field.second, "Amount", 0.0f );
            }
            else if ( type->text == "VuPfxTickWorldScaleZ" )
            {
                pattern.stretch = true;
                pattern.stretch_delay = property( field.second, "Start Delay", 0.0f );
                pattern.stretch_rate = property( field.second, "Rate", 0.0f ) * 0.01f;
            }
            // The geom emitter is a child like any other, but geom_from picks
            // it up in its own pass over these same children.
            else if ( type->text == "VuPfxEmitGeomFountain" )
                continue;
            // A CHILD NODE WE DO NOT BUILD IS A GAP IN THE AUDIT, NOT JUST IN
            // THE EFFECT. Every pattern's emitters and tick processes are its
            // children; dropping one silently is how the geom pattern went
            // unnoticed for so long. Name each unknown type once.
            else
                unbuilt( type->text );
        }
        return pattern;
    }

    // Warn once per pfx node type this parser has no handler for.
    static void unbuilt( const std::string& type )
    {
        static std::set< std::string > seen;
        if ( seen.insert( type ).second )
            LOG_WARNING( "pfx node %s has no handler - its effect is built without it", type.c_str( ) );
    }

    static void spawn( fountain_t& fountain, const float* world, float age, const instance_t& instance, float speed )
    {
        particle_t particle{};
        if ( fountain.hull )
        {
            // Pick a submerged edge in proportion to its LENGTH, then a
            // uniform point along it. Walking the cumulative length table is
            // what makes the spray follow the authored waterline instead of
            // clustering wherever the mesh happens to have more vertices -
            // the previous stand-in picked a vertex uniformly and nudged it
            // by an invented +-0.12, which both over-weighted dense corners
            // and left the long flat runs bare.
            const int count = static_cast< int >( instance.cumulative.size( ) );
            if ( count <= 0 || instance.submerged <= 0.0f )
                return;
            const float want = unit( ) * instance.submerged;
            int pick = 0;
            while ( pick + 1 < count && instance.cumulative[ static_cast< std::size_t >( pick ) ] < want )
                ++pick;
            const float* s = instance.segments.data( ) + static_cast< std::size_t >( pick ) * 6;
            const float along = unit( );
            const float local[ 3 ] = {
                s[ 0 ] + ( s[ 3 ] - s[ 0 ] ) * along,
                s[ 1 ] + ( s[ 4 ] - s[ 1 ] ) * along,
                s[ 2 ] + ( s[ 5 ] - s[ 2 ] ) * along
            };
            transform_point( world, local, particle.position );
            // The hull point is already moving with the boat. Dampen then stops
            // the card in the world, so the boat leaves a sheet of mist behind.
            particle.velocity[ 0 ] = world[ 4 ] * speed;
            particle.velocity[ 1 ] = world[ 5 ] * speed;
            particle.velocity[ 2 ] = 0.0f;
        }
        else
        {
            float local[ 3 ]{};
            float velocity[ 3 ]{};
            for ( int index = 0; index < 3; ++index )
            {
                local[ index ] = range( fountain.position_min[ index ], fountain.position_max[ index ] );
                velocity[ index ] = range( fountain.velocity_min[ index ], fountain.velocity_max[ index ] );
            }
            transform_point( world, local, particle.position );
            transform_direction( world, velocity, particle.velocity );
        }
        particle.life = range( fountain.life[ 0 ], fountain.life[ 1 ] );
        particle.scale = range( fountain.scale[ 0 ], fountain.scale[ 1 ] );
        // REMOVED, all three invented: scale x2, alpha x0.45, velocity x0.2
        // for hull particles. The real hull emitter is
        // VuPfxEmitBoatHullInstance_emit @0x4CE760 - its own function, not the
        // shared VuPfxEmitFountain_emit @0x55D030 that the quad, directional
        // and geom fountains all share at slot 1. It reads the boat's rigid
        // body at +540 and a flag at +1131, so it cannot be transliterated
        // until VuBoatEntity exists.
        //
        // Nothing reaches this path today in any case: waterline(), move() and
        // submerged() lost their only callers with the boat, so a hull
        // fountain never receives an edge list and its rate stays zero.
        particle.rotation = range( fountain.rotation[ 0 ], fountain.rotation[ 1 ] );
        particle.spin = range( fountain.spin[ 0 ], fountain.spin[ 1 ] );
        for ( int axis = 0; axis < 3; ++axis )
        {
            particle.orient3[ axis ] = range( fountain.rot_min[ axis ], fountain.rot_max[ axis ] );
            particle.angular[ axis ] = range( fountain.ang_min[ axis ], fountain.ang_max[ axis ] );
        }
        particle.world_z = range( fountain.world_z[ 0 ], fountain.world_z[ 1 ] );
        particle.tile_u = range( fountain.tile_u[ 0 ], fountain.tile_u[ 1 ] );
        particle.tile_v = range( fountain.tile_v[ 0 ], fountain.tile_v[ 1 ] );
        for ( int index = 0; index < 3; ++index )
            particle.rgb[ index ] = range( fountain.color_min[ index ], fountain.color_max[ index ] );
        particle.alpha = range( fountain.color_min[ 3 ], fountain.color_max[ 3 ] );
        particle.age = age;
        if ( particle.life > 0.0f && particle.age > particle.life )
            particle.age = 0.0f;
        fountain.particles.push_back( particle );
    }

    // How many particles a pattern holds - retail's pattern instance keeps
    // this at +40, incremented by the allocator and decremented by the
    // retirement loop. Ours is the sum over the fountains' vectors, which is
    // the same number by construction.
    static int pattern_count( const pattern_t& pattern )
    {
        int count = 0;
        for ( const fountain_t& fountain : pattern.fountains )
            count += static_cast< int >( fountain.particles.size( ) );
        return count;
    }

    // VuPfxSystemInstance::tick @0x58ad70, vtable slot 13, transliterated.
    //
    //   if ( state != 0 )
    //   {
    //       elapsed += dt;
    //       for each pattern: tick( dt ); count += pattern->count;
    //       if ( state == 2 && count == 0 )                          state = 0;
    //       if ( Duration > 0 && state == 1 && elapsed >= Duration ) state = 2;
    //   }
    //
    // THE CLOCK ADVANCES BEFORE THE FIRST PATTERN TICK - the increment sits
    // in the for-init - so a pattern's Start Delay gate sees the already
    // advanced value, and a Start Delay equal to one frame's dt has already
    // elapsed by the time the first tick reads it.
    //
    // The AABB the retail tick also accumulates here, from each pattern's own
    // +44..+52 and +60..+68, is deliberately not built: nothing in this build
    // culls, so there is no consumer for it.
    static void tick_system( instance_t& instance, const float* eye, float dt )
    {
        if ( instance.state == 0 )
            return;
        instance.elapsed += dt;
        int count = 0;
        for ( pattern_t& pattern : instance.patterns )
        {
            simulate( pattern, instance, eye, dt );
            count += pattern_count( pattern );
        }
        if ( instance.state == 2 && count == 0 )
            instance.state = 0;
        if ( instance.duration > 0.0f && instance.state == 1
            && instance.elapsed >= instance.duration )
        {
            instance.state = 2;
        }
    }

    static void simulate( pattern_t& pattern, const instance_t& instance, const float* eye, float dt )
    {
        // VuPfxQuadPatternInstance_tick @0x591c30 and
        // VuPfxGeomPatternInstance_tick @0x58fea0 both open with
        //     if ( systemInstance->elapsed > params->startDelay )
        // and EVERYTHING is inside it - the integration, the child node ticks
        // and the retirement alike. A pattern whose Start Delay has not
        // elapsed is completely frozen, not merely silent.
        if ( instance.elapsed <= pattern.start_delay )
            return;
        for ( fountain_t& fountain : pattern.fountains )
        {
            for ( particle_t& particle : fountain.particles )
            {
                particle.position[ 0 ] += particle.velocity[ 0 ] * dt;
                particle.position[ 1 ] += particle.velocity[ 1 ] * dt;
                particle.position[ 2 ] += particle.velocity[ 2 ] * dt;
                particle.age += dt;
                particle.rotation += particle.spin * dt;
                for ( int axis = 0; axis < 3; ++axis )
                    particle.orient3[ axis ] += particle.angular[ axis ] * dt;
            }
        }

        const float origin[ 3 ] = { instance.world[ 12 ], instance.world[ 13 ], instance.world[ 14 ] };
        const float dx = origin[ 0 ] - eye[ 0 ];
        const float dy = origin[ 1 ] - eye[ 1 ];
        const float dz = origin[ 2 ] - eye[ 2 ];
        const float distance2 = dx * dx + dy * dy + dz * dz;
        for ( fountain_t& fountain : pattern.fountains )
        {
            const bool in_range = fountain.spawn_distance * fountain.spawn_distance > distance2;
            float rate = fountain.spawn_rate * instance.scale;
            if ( fountain.hull )
            {
                // UNVERIFIED. Ramping the rate linearly between Min and Max
                // Speed is plausible and is NOT yet read out of
                // VuPfxEmitBoatHullInstance_emit @0x4CE760. Left in place only
                // because the path is unreachable without a boat; it must be
                // read before anything depends on it.
                const float span = fountain.max_speed - fountain.min_speed;
                float blend = 0.0f;
                if ( span > 0.01f && instance.speed > fountain.min_speed )
                {
                    blend = ( instance.speed - fountain.min_speed ) / span;
                    if ( blend > 1.0f )
                        blend = 1.0f;
                }
                rate = fountain.spawn_rate * blend;
                // No hull edge in the water means no spray, however fast the
                // boat is nominally going.
                if ( instance.submerged <= 0.0f )
                    rate = 0.0f;
            }
            // VuPfxEmitFountain_emit @0x55D030, transliterated.
            //
            //   limit = (int)(maxCount * countScale)
            //   if ( maxCount <= 0 || alive < limit )
            //       accumulator += rate * dt
            //       if ( accumulator > 0 )
            //           do {
            //               if ( anyViewportInRange ) {
            //                   spawn( age = accumulator / rate )
            //                   if ( limit > 0 && ++alive >= limit ) break
            //               }
            //               accumulator -= 1.0
            //           } while ( accumulator > 0 )
            //
            // THE DECREMENT IS OUTSIDE THE RANGE TEST. An emitter that is out
            // of range still burns its accumulator down; ours only decremented
            // when it spawned, so a distant emitter banked a debt and emptied
            // it in one burst on coming into range.
            //
            // There is no per-frame spawn cap and no absolute particle
            // ceiling. The `made < 8` and `alive >= 48` that stood here were
            // both invented.
            //
            // NAMED UNKNOWN: retail keeps this emitter's own count in the
            // EMIT NODE at +20 and increments it on every successful
            // allocation, but nothing yet read decrements it - the retirement
            // loop decrements the PATTERN's count at +40 and leaves +20
            // alone. So the emitter's limit may be a LIFETIME total rather
            // than a concurrent one, and whichever it is has not been
            // established. Ours recounts the live particles each frame, which
            // is the concurrent reading. Unresolved, not chosen.
            const int limit = static_cast< int >(
                static_cast< float >( fountain.max_count ) * fountain.count_scale );
            int alive = static_cast< int >( fountain.particles.size( ) );
            if ( fountain.max_count <= 0 || alive < limit )
            {
                fountain.accumulator += rate * dt;
                if ( fountain.accumulator > 0.0f )
                {
                    do
                    {
                        // Retail tests EVERY viewport and emits if any one is
                        // within Spawn Distance. The scene viewer has a single
                        // viewport, so one eye is the same test here - it will
                        // stop being so the moment there is a second view.
                        if ( in_range )
                        {
                            // VuPfxPatternInstance::allocateParticle @0x58b4e0,
                            // slot 4, is the allocation the emitter performs
                            // here, and it is the ONLY thing that stops
                            // emission: neither pattern tick nor the emitter
                            // itself reads the system state.
                            //
                            //   if ( system->state != 1 )                 return NULL;
                            //   if ( max != 0 && pattern->count >= max )  return NULL;
                            //   p = allocFromPool(); if ( !p ) return NULL;
                            //   init( p ); return p;
                            //
                            // ITS "Max Particle Count" IS THE PATTERN'S, at
                            // params +0x30, counted over the whole pattern -
                            // not the emit node's own limit above, which is a
                            // different property scaled by the node's count
                            // scale at +28.
                            const bool allocated = instance.state == 1
                                && ( pattern.max_count == 0
                                    || pattern_count( pattern ) < pattern.max_count );
                            // MEASURED, 2026-10-30: over 820 frames of
                            // Hydroelectric this refused ZERO allocations -
                            // the emit node's own limit already holds the
                            // rotors, turbines and whirlpools at one particle,
                            // so the pattern cap is redundant for this level's
                            // content rather than inert. The Start Delay gate
                            // above froze 646 pattern ticks, all inside the
                            // first 0.3 s, and two Duration systems expired
                            // between frames 200 and 400.
                            if ( allocated )
                            {
                                const float age = rate > 0.0f ? fountain.accumulator / rate : 0.0f;
                                spawn( fountain, instance.world, age, instance, instance.speed );
                                // BOTH OF THESE SIT INSIDE THE SUCCESS BRANCH
                                // in @0x55d030: a refused allocation does not
                                // count against the emitter's limit and cannot
                                // take the break, but the accumulator below is
                                // decremented either way.
                                ++alive;
                                if ( limit > 0 && alive >= limit )
                                    break;
                            }
                        }
                        fountain.accumulator -= 1.0f;
                    }
                    while ( fountain.accumulator > 0.0f );
                }
            }
        }

        for ( fountain_t& fountain : pattern.fountains )
        {
            for ( particle_t& particle : fountain.particles )
            {
                if ( pattern.fade_in )
                {
                    if ( particle.age <= pattern.fade_in_duration )
                        particle.alpha += pattern.fade_in_rate * dt;
                    if ( particle.age > pattern.fade_out_time )
                        particle.alpha += pattern.fade_out_rate * dt;
                }
                if ( pattern.fade_alpha && particle.age > pattern.alpha_delay )
                    particle.alpha += pattern.alpha_rate * dt;
                if ( pattern.dampen && particle.age > pattern.dampen_delay )
                {
                    float keep = 1.0f - pattern.dampen_amount * dt;
                    if ( keep < 0.0f )
                        keep = 0.0f;
                    particle.velocity[ 0 ] *= keep;
                    particle.velocity[ 1 ] *= keep;
                    particle.velocity[ 2 ] *= keep;
                }
                // VuPfxTickLinearAccelerationInstance_tick @0x58E810 adds the
                // authored acceleration DIRECTLY to the velocity:
                //
                //   if ( age > node[16] )            // Start Delay, 0x40
                //       velocity[0] += node[12] * dt // Accel X, 0x30
                //       velocity[1] += node[13] * dt // Accel Y, 0x34
                //       velocity[2] += node[14] * dt // Accel Z, 0x38
                //
                // There is NO rotation into world space. This used to call
                // transform_direction with the emitter's matrix first, which
                // is an invention - it turns an authored downward acceleration
                // into whatever direction the emitter happens to face.
                if ( pattern.accelerate && particle.age > pattern.accel_delay )
                {
                    particle.velocity[ 0 ] += pattern.accel[ 0 ] * dt;
                    particle.velocity[ 1 ] += pattern.accel[ 1 ] * dt;
                    particle.velocity[ 2 ] += pattern.accel[ 2 ] * dt;
                }
                for ( const pattern_t::scale_tick_t& tick : pattern.scales )
                {
                    if ( particle.age > tick.delay )
                        particle.scale += tick.rate * dt;
                }
                if ( pattern.stretch && particle.age > pattern.stretch_delay )
                    particle.world_z += pattern.stretch_rate * dt;
            }
            std::size_t kept = 0;
            for ( std::size_t index = 0; index < fountain.particles.size( ); ++index )
            {
                const particle_t& particle = fountain.particles[ index ];
                if ( particle.life - particle.age < 0.0f || particle.alpha < 0.0f || particle.scale < 0.0f || particle.world_z < 0.0f )
                    continue;
                if ( kept != index )
                    fountain.particles[ kept ] = fountain.particles[ index ];
                ++kept;
            }
            fountain.particles.resize( kept );
        }
    }

    std::map< std::string, value_t > m_documents;
    std::vector< instance_t > m_instances;
    // Effects reported as unbuilt, so each is named once.
    std::set< std::string > m_unsupported;
    int m_card = -1;
    // Retail has ONE of these as a global, shared with the ocean's spectrum.
    // Ours is per-subsystem, so our draws do not interleave with the ocean's
    // the way retail's do - a known structural difference, flagged in
    // random.hxx, not something to change on a guess.
    static inline c_random m_random{};
};
