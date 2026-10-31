#pragma once

#include <impl/engine/engine.hxx>
#include <workspace/core/assets/animation.hxx>
#include <workspace/core/assets/model.hxx>
#include <workspace/utility/math.hxx>

#include <cmath>
#include <cstring>
#include <vector>

// Posing a skinned model: the bone palette for one frame, and the CPU skin
// that writes the posed vertices into a streamed buffer. Shared by the boat
// hulls and the animated props, which differ only in which clip drives them
// and whether a steering delta is layered on top.

// Determinant of a column-major affine matrix's 3x3 basis. It measures
// the volume scale, so it is the matrix-side equivalent of the extent
// test the streamed props are scored with.
inline float determinant( const float* m )
{
    return m[ 0 ] * ( m[ 5 ] * m[ 10 ] - m[ 9 ] * m[ 6 ] )
        - m[ 4 ] * ( m[ 1 ] * m[ 10 ] - m[ 9 ] * m[ 2 ] )
        + m[ 8 ] * ( m[ 1 ] * m[ 6 ] - m[ 5 ] * m[ 2 ] );
}

// The bind mesh kept for anything carrying a per-vertex binding: the rest
// vertices, their four-bone bindings, and the draw state the posed copy is
// submitted with.
struct skinned_mesh_t
{
    std::vector< mesh_vertex_t > bind;
    std::vector< model_skin_t > skin;
    std::vector< unsigned short > indices;
    int texture = -1;
    int detail = -1;
    int alpha_test = 0;
};

// Build the bone palette for one pose. Shared by the boat hulls and the
// animated props, which differ only in which clip drives them and whether
// a steering delta is layered on top.
//
// The two poses do NOT share a space. The bind pose is absolute, so it
// composes directly, but a clip stores each bone RELATIVE TO ITS PARENT
// and must be chained. Measured, not assumed: chaining clip frame 0
// reproduces the bind pose on 41 of 41 bones with a worst offset of
// 0.0000, where taking it directly matched only 14 of 41 and missed by up
// to 3.74 units.
inline void build_palette( std::vector< float >& palette,
    const std::vector< model_bone_t >& bind, const c_clip& clip, int frame,
    const c_clip* turn, int extreme, float swing )
{
    palette.assign( bind.size( ) * 16, 0.0f );
    for ( std::size_t b = 0; b < bind.size( ); ++b )
    {
        const clip_pose_t& pose = clip.at( frame, static_cast< int >( b ) );
        float local[ 16 ]{};
        model_compose( local, pose.position, pose.rotation, pose.scale );

        if ( turn != nullptr )
        {
            // Interpolate between the centre frame and whichever extreme
            // the steering points at. A plain normalised lerp rather than
            // a slerp: over a 20 degree arc the two differ by well under
            // a tenth of a degree, and the target cannot spare the trig.
            const clip_pose_t& centre = turn->at( 1, static_cast< int >( b ) );
            const clip_pose_t& swung = turn->at( extreme, static_cast< int >( b ) );
            float q[ 4 ];
            for ( int c = 0; c < 4; ++c )
                q[ c ] = centre.rotation[ c ] + ( swung.rotation[ c ] - centre.rotation[ c ] ) * swing;
            const float length = std::sqrt( q[ 0 ] * q[ 0 ] + q[ 1 ] * q[ 1 ]
                + q[ 2 ] * q[ 2 ] + q[ 3 ] * q[ 3 ] );
            // Every bone in Vector's clip measured unit length, so this
            // is a safety net rather than a filter: it stops a
            // zero-filled record in some other boat's clip from
            // collapsing a bone, and an identity delta passes through it
            // to multiply harmlessly.
            if ( length > 0.5f )
            {
                for ( int c = 0; c < 4; ++c )
                    q[ c ] /= length;
                const float origin[ 3 ]{ 0.0f, 0.0f, 0.0f };
                const float unit[ 3 ]{ 1.0f, 1.0f, 1.0f };
                float delta[ 16 ]{};
                model_compose( delta, origin, q, unit );
                // Order is forced, not chosen: the delta has no
                // translation, so local * delta swivels the motor about
                // its own origin and leaves it attached, where delta *
                // local would sweep it around the parent instead.
                float composed[ 16 ];
                math_multiply( composed, local, delta );
                std::memcpy( local, composed, sizeof( local ) );
            }
        }

        const int parent = bind[ b ].parent;
        if ( parent >= 0 && static_cast< std::size_t >( parent ) < b )
            math_multiply( palette.data( ) + b * 16, palette.data( ) + parent * 16, local );
        else
            std::memcpy( palette.data( ) + b * 16, local, sizeof( local ) );
    }
    // Now fold in the inverse bind, in place: palette[b] becomes
    // animatedWorld[b] * inverse(bindWorld[b]). Done after the chain so
    // children still read their parent's world matrix above.
    for ( std::size_t b = 0; b < bind.size( ); ++b )
    {
        float world[ 16 ];
        std::memcpy( world, palette.data( ) + b * 16, sizeof( world ) );
        math_multiply( palette.data( ) + b * 16, world, bind[ b ].inverse );
    }
}

// Skinning runs on the CPU and streams the result, rather than uploading
// a bone palette and skinning in the vertex shader as the original does
// through gMatrixArray. That would mean changing the engine vertex
// format, the input layout and the shaders; this reuses the dynamic
// vertex path that already streams the larger water mesh every frame. A
// hull is 6024 vertices against the water's 7921, and on the Atom target
// the integrated GPU is the scarcer resource.
//
// Returns the buffer it wrote so a caller can score the result against
// the bind pose; nullptr if the geometry could not be opened.
inline mesh_vertex_t* skin_geometry( int geometry, const skinned_mesh_t& source,
    const std::vector< model_bone_t >& bind, const std::vector< float >& palette )
{
    int count = 0;
    mesh_vertex_t* target = c_engine::edit( geometry, count );
    if ( target == nullptr || count < static_cast< int >( source.bind.size( ) ) )
        return nullptr;

    for ( std::size_t v = 0; v < source.bind.size( ); ++v )
    {
        const mesh_vertex_t& rest = source.bind[ v ];
        const model_skin_t& bound = source.skin[ v ];
        float px = 0.0f, py = 0.0f, pz = 0.0f;
        float nx = 0.0f, ny = 0.0f, nz = 0.0f;
        float applied = 0.0f;
        for ( int slot = 0; slot < 4; ++slot )
        {
            const float weight = bound.weight[ slot ];
            if ( weight <= 0.0f )
                continue;
            applied += weight;
            const std::size_t index = bound.bone[ slot ];
            if ( index >= bind.size( ) )
                continue;
            const float* m = palette.data( ) + index * 16;
            px += weight * ( m[ 0 ] * rest.position[ 0 ] + m[ 4 ] * rest.position[ 1 ] + m[ 8 ] * rest.position[ 2 ] + m[ 12 ] );
            py += weight * ( m[ 1 ] * rest.position[ 0 ] + m[ 5 ] * rest.position[ 1 ] + m[ 9 ] * rest.position[ 2 ] + m[ 13 ] );
            pz += weight * ( m[ 2 ] * rest.position[ 0 ] + m[ 6 ] * rest.position[ 1 ] + m[ 10 ] * rest.position[ 2 ] + m[ 14 ] );
            nx += weight * ( m[ 0 ] * rest.normal[ 0 ] + m[ 4 ] * rest.normal[ 1 ] + m[ 8 ] * rest.normal[ 2 ] );
            ny += weight * ( m[ 1 ] * rest.normal[ 0 ] + m[ 5 ] * rest.normal[ 1 ] + m[ 9 ] * rest.normal[ 2 ] );
            nz += weight * ( m[ 2 ] * rest.normal[ 0 ] + m[ 6 ] * rest.normal[ 1 ] + m[ 10 ] * rest.normal[ 2 ] );
        }
        mesh_vertex_t& out = target[ v ];
        out = rest;
        // A vertex can carry no usable influence - all four weight bytes
        // zero, or every index out of range. Writing the accumulated zero
        // would collapse it onto the model origin. Leave those at their
        // bind position.
        if ( applied <= 0.0f )
            continue;
        out.position[ 0 ] = px;
        out.position[ 1 ] = py;
        out.position[ 2 ] = pz;
        const float length = std::sqrt( nx * nx + ny * ny + nz * nz );
        if ( length > 0.0001f )
        {
            out.normal[ 0 ] = nx / length;
            out.normal[ 1 ] = ny / length;
            out.normal[ 2 ] = nz / length;
        }
    }
    c_engine::stream( geometry );
    return target;
}
