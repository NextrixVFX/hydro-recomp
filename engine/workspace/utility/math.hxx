#pragma once

#include <cmath>
#include <cstring>

inline void math_cross( float* out, const float* a, const float* b )
{
    out[ 0 ] = a[ 1 ] * b[ 2 ] - a[ 2 ] * b[ 1 ];
    out[ 1 ] = a[ 2 ] * b[ 0 ] - a[ 0 ] * b[ 2 ];
    out[ 2 ] = a[ 0 ] * b[ 1 ] - a[ 1 ] * b[ 0 ];
}

inline float math_dot( const float* a, const float* b )
{
    return a[ 0 ] * b[ 0 ] + a[ 1 ] * b[ 1 ] + a[ 2 ] * b[ 2 ];
}

inline void math_normalize( float* v )
{
    float length = std::sqrt( math_dot( v, v ) );
    if ( length < 1.0e-6f )
        return;
    v[ 0 ] /= length;
    v[ 1 ] /= length;
    v[ 2 ] /= length;
}

inline void math_identity( float* m )
{
    std::memset( m, 0, sizeof( float ) * 16 );
    m[ 0 ] = m[ 5 ] = m[ 10 ] = m[ 15 ] = 1.0f;
}

// Column-major. out = a * b, so b applies first.
inline void math_multiply( float* out, const float* a, const float* b )
{
    float result[ 16 ]{};
    for ( int column = 0; column < 4; ++column )
    {
        for ( int row = 0; row < 4; ++row )
        {
            result[ column * 4 + row ] =
                a[ row ] * b[ column * 4 ] +
                a[ 4 + row ] * b[ column * 4 + 1 ] +
                a[ 8 + row ] * b[ column * 4 + 2 ] +
                a[ 12 + row ] * b[ column * 4 + 3 ];
        }
    }
    std::memcpy( out, result, sizeof( result ) );
}

// Column-major. Rotation is degrees, X then Y then Z, then scale, then translation. Z is up.
inline void math_transform( float* m, const float* position, const float* rotation_deg, const float* scale )
{
    const float rx = rotation_deg[ 0 ] * 0.01745329251f;
    const float ry = rotation_deg[ 1 ] * 0.01745329251f;
    const float rz = rotation_deg[ 2 ] * 0.01745329251f;
    const float cx = std::cos( rx );
    const float sx = std::sin( rx );
    const float cy = std::cos( ry );
    const float sy = std::sin( ry );
    const float cz = std::cos( rz );
    const float sz = std::sin( rz );

    m[ 0 ] = cy * cz * scale[ 0 ];
    m[ 1 ] = cy * sz * scale[ 0 ];
    m[ 2 ] = -sy * scale[ 0 ];
    m[ 3 ] = 0.0f;
    m[ 4 ] = ( sx * sy * cz - cx * sz ) * scale[ 1 ];
    m[ 5 ] = ( sx * sy * sz + cx * cz ) * scale[ 1 ];
    m[ 6 ] = sx * cy * scale[ 1 ];
    m[ 7 ] = 0.0f;
    m[ 8 ] = ( cx * sy * cz + sx * sz ) * scale[ 2 ];
    m[ 9 ] = ( cx * sy * sz - sx * cz ) * scale[ 2 ];
    m[ 10 ] = cx * cy * scale[ 2 ];
    m[ 11 ] = 0.0f;
    m[ 12 ] = position[ 0 ];
    m[ 13 ] = position[ 1 ];
    m[ 14 ] = position[ 2 ];
    m[ 15 ] = 1.0f;
}

// Column-major world: yaw around Z, then scale, then translate.
inline void math_pose( float* m, const float* position, float yaw, const float* scale )
{
    float cy = std::cos( yaw );
    float sy = std::sin( yaw );
    math_identity( m );
    m[ 0 ] = cy * scale[ 0 ];
    m[ 1 ] = sy * scale[ 0 ];
    m[ 4 ] = -sy * scale[ 1 ];
    m[ 5 ] = cy * scale[ 1 ];
    m[ 10 ] = scale[ 2 ];
    m[ 12 ] = position[ 0 ];
    m[ 13 ] = position[ 1 ];
    m[ 14 ] = position[ 2 ];
}

// Left-handed look-at, stored for mul(position, matrix).
inline void math_look( float* m, const float* eye, const float* at, const float* up )
{
    float z[ 3 ] = { at[ 0 ] - eye[ 0 ], at[ 1 ] - eye[ 1 ], at[ 2 ] - eye[ 2 ] };
    math_normalize( z );
    float x[ 3 ]{};
    math_cross( x, up, z );
    math_normalize( x );
    float y[ 3 ]{};
    math_cross( y, z, x );
    // Screen X is the mirror of up × forward. Negating right after the
    // basis is built keeps up and forward, and swaps the two sides.
    x[ 0 ] = -x[ 0 ];
    x[ 1 ] = -x[ 1 ];
    x[ 2 ] = -x[ 2 ];

    math_identity( m );
    m[ 0 ] = x[ 0 ]; m[ 4 ] = x[ 1 ]; m[ 8 ] = x[ 2 ]; m[ 12 ] = -math_dot( x, eye );
    m[ 1 ] = y[ 0 ]; m[ 5 ] = y[ 1 ]; m[ 9 ] = y[ 2 ]; m[ 13 ] = -math_dot( y, eye );
    m[ 2 ] = z[ 0 ]; m[ 6 ] = z[ 1 ]; m[ 10 ] = z[ 2 ]; m[ 14 ] = -math_dot( z, eye );
}

inline void math_perspective( float* m, float fov_y, float aspect, float near_z, float far_z )
{
    float y_scale = 1.0f / std::tan( fov_y * 0.5f );
    float x_scale = y_scale / aspect;
    float range = far_z / ( far_z - near_z );
    std::memset( m, 0, sizeof( float ) * 16 );
    m[ 0 ] = x_scale;
    m[ 5 ] = y_scale;
    m[ 10 ] = range;
    m[ 11 ] = 1.0f;
    m[ 14 ] = -near_z * range;
}
