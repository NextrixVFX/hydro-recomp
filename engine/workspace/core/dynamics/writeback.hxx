#pragma once

#include <workspace/core/dynamics/row.hxx>

#include <cmath>

// Bullet 2.79's velocity writebacks, run by solveGroupCacheFriendlyFinish
// (@0x5744F0) on every rigid body once the iterations are done. Recreated
// from the disassembly.

// The CRT's __libm_sse2_sinf / __libm_sse2_cosf. Retail imports them from a
// DLL, so their code is not in the image and cannot be read. Evaluated here
// as the float rounding of the double result - the same thing the oracle's
// stub computes - which is NOT proven to match the CRT bit for bit.
inline float writeback_sinf( float x ) { return static_cast< float >( std::sin( static_cast< double >( x ) ) ); }
inline float writeback_cosf( float x ) { return static_cast< float >( std::cos( static_cast< double >( x ) ) ); }

// btMatrix3x3::getRotation @0x50F8E0 (thiscall basis, out quaternion x y z
// w, retn 4). `m` is the basis as retail lays it out: three rows of four.
inline void basis_rotation( const float* m, float* q )
{
    const float trace = ( m[ 0 ] + m[ 5 ] ) + m[ 10 ];
    if ( trace > 0.0f )
    {
        const float s = std::sqrt( trace + 1.0f );
        const float inv = 0.5f / s;
        q[ 0 ] = ( m[ 9 ] - m[ 6 ] ) * inv;
        q[ 1 ] = ( m[ 2 ] - m[ 8 ] ) * inv;
        q[ 2 ] = ( m[ 4 ] - m[ 1 ] ) * inv;
        q[ 3 ] = s * 0.5f;
        return;
    }
    // The largest diagonal: comiss / setnbe / cmova, so an unordered compare
    // picks the lower index.
    int i = 0;
    if ( m[ 5 ] > m[ 0 ] )
        i = ( m[ 10 ] > m[ 5 ] ) ? 2 : 1;
    else if ( m[ 10 ] > m[ 0 ] )
        i = 2;
    const int j = ( i + 1 ) % 3;
    const int k = ( i + 2 ) % 3;
    float t[ 3 ];
    const float s = std::sqrt( ( ( m[ 5 * i ] - m[ 5 * j ] ) - m[ 5 * k ] ) + 1.0f );
    t[ i ] = s * 0.5f;
    const float inv = 0.5f / s;
    const float w = ( m[ 4 * k + j ] - m[ 4 * j + k ] ) * inv;
    t[ j ] = ( m[ 4 * j + i ] + m[ 4 * i + j ] ) * inv;
    t[ k ] = ( m[ 4 * k + i ] + m[ 4 * i + k ] ) * inv;
    q[ 0 ] = t[ 0 ];
    q[ 1 ] = t[ 1 ];
    q[ 2 ] = t[ 2 ];
    q[ 3 ] = w;
}

// The velocity half both writebacks share: linear = linear + delta, angular =
// delta + angular, w lanes zeroed.
inline void writeback_velocities( solver_rigid_t& b )
{
    const float lx = b.linear_velocity[ 0 ] + b.delta_linear[ 0 ];
    const float ly = b.linear_velocity[ 1 ] + b.delta_linear[ 1 ];
    const float lz = b.linear_velocity[ 2 ] + b.delta_linear[ 2 ];
    b.linear_velocity[ 0 ] = lx;
    b.linear_velocity[ 1 ] = ly;
    b.linear_velocity[ 2 ] = lz;
    b.linear_velocity[ 3 ] = 0.0f;
    const float ax = b.delta_angular[ 0 ] + b.angular_velocity[ 0 ];
    const float ay = b.angular_velocity[ 1 ] + b.delta_angular[ 1 ];
    const float az = b.angular_velocity[ 2 ] + b.delta_angular[ 2 ];
    b.angular_velocity[ 0 ] = ax;
    b.angular_velocity[ 1 ] = ay;
    b.angular_velocity[ 2 ] = az;
    b.angular_velocity[ 3 ] = 0.0f;
}

// internalWritebackVelocity @0x574AC0 (ecx body): velocities only, and only
// for a dynamic body (inverse mass != 0; an unordered compare proceeds).
inline void writeback_velocity( solver_rigid_t& b )
{
    if ( b.inverse_mass != 0.0f )
        writeback_velocities( b );
}

// btTransformUtil::integrateTransform as retail inlines it - identically in
// internalWritebackVelocity(timeStep) @0x50E810 (push / turn velocities) and
// integrateTransforms @0x602870 (linear / angular velocities), both read
// from the disassembly. `basis` / `origin` in, `out_basis` / `out_origin`
// out (rows of four, w lanes zero); in and out may alias.
inline void integrate_transform( const float* basis, const float* origin, const float* push, const float* turn,
    float dt, float* out_basis, float* out_origin )
{
    const float ox = push[ 0 ] * dt + origin[ 0 ];
    const float oy = origin[ 1 ] + push[ 1 ] * dt;
    const float oz = origin[ 2 ] + push[ 2 ] * dt;

    float angle = std::sqrt( ( turn[ 0 ] * turn[ 0 ] + turn[ 1 ] * turn[ 1 ] ) + turn[ 2 ] * turn[ 2 ] );
    if ( angle * dt > 0.78539819f )         // 0x3F490FDB, ANGULAR_MOTION_THRESHOLD
        angle = 0.78539819f / dt;
    float axis[ 3 ];
    if ( 0.001f > angle )
    {
        // Taylor expansion of sin(0.5 * angle * dt) / angle.
        const float f = dt * 0.5f - ( ( ( ( dt * dt ) * dt ) * 0.020833334f ) * angle ) * angle;
        axis[ 0 ] = f * turn[ 0 ];
        axis[ 1 ] = f * turn[ 1 ];
        axis[ 2 ] = f * turn[ 2 ];
    }
    else
    {
        // The two arguments associate differently in retail: sinf gets
        // (angle * 0.5) * dt, cosf (angle * dt) * 0.5.
        const float s = writeback_sinf( ( angle * 0.5f ) * dt ) / angle;
        axis[ 0 ] = s * turn[ 0 ];
        axis[ 1 ] = s * turn[ 1 ];
        axis[ 2 ] = s * turn[ 2 ];
    }
    const float c = writeback_cosf( ( angle * dt ) * 0.5f );

    float q0[ 4 ];
    basis_rotation( basis, q0 );
    const float x0 = q0[ 0 ], y0 = q0[ 1 ], z0 = q0[ 2 ], w0 = q0[ 3 ];
    const float dx = axis[ 0 ], dy = axis[ 1 ], dz = axis[ 2 ];

    // dorn * orn0.
    float x = ( ( x0 * c + w0 * dx ) + z0 * dy ) - y0 * dz;
    float y = ( ( y0 * c + w0 * dy ) + x0 * dz ) - z0 * dx;
    float z = ( ( z0 * c + w0 * dz ) + y0 * dx ) - x0 * dy;
    float w = ( ( w0 * c - x0 * dx ) - y0 * dy ) - z0 * dz;

    const float inv = 1.0f / std::sqrt( ( ( y * y + x * x ) + z * z ) + w * w );
    x = x * inv;
    y = y * inv;
    z = z * inv;
    w = w * inv;

    // btMatrix3x3::setRotation.
    const float s = 2.0f / ( ( ( y * y + x * x ) + z * z ) + w * w );
    const float xs = x * s, ys = y * s, zs = z * s;
    const float wx = w * xs, wy = w * ys, wz = w * zs;
    const float xx = x * xs, xy = x * ys, xz = x * zs;
    const float yy = y * ys, yz = y * zs, zz = z * zs;

    out_basis[ 0 ] = 1.0f - ( zz + yy );
    out_basis[ 1 ] = xy - wz;
    out_basis[ 2 ] = xz + wy;
    out_basis[ 3 ] = 0.0f;
    out_basis[ 4 ] = xy + wz;
    out_basis[ 5 ] = 1.0f - ( zz + xx );
    out_basis[ 6 ] = yz - wx;
    out_basis[ 7 ] = 0.0f;
    out_basis[ 8 ] = xz - wy;
    out_basis[ 9 ] = yz + wx;
    out_basis[ 10 ] = 1.0f - ( yy + xx );
    out_basis[ 11 ] = 0.0f;
    out_origin[ 0 ] = ox;
    out_origin[ 1 ] = oy;
    out_origin[ 2 ] = oz;
    out_origin[ 3 ] = 0.0f;
}

// internalWritebackVelocity(timeStep) @0x50E810 (ecx body, timeStep in
// xmm1): the velocities, then the world transform integrated by the PUSH
// and TURN velocities.
inline void writeback_velocity_split( solver_rigid_t& b, float dt )
{
    if ( !( b.inverse_mass != 0.0f ) )
        return;
    writeback_velocities( b );
    integrate_transform( b.basis, b.origin, b.push_velocity, b.turn_velocity, dt, b.basis, b.origin );
}
