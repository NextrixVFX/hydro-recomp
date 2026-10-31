#pragma once

#include <workspace/core/dynamics/row.hxx>
#include <workspace/core/dynamics/writeback.hxx>

// The CRT's __libm_sse2_powf (a DLL import - its code is not in the image).
// Evaluated as the float rounding of the double result, as the oracle's stub
// computes it - NOT proven bit-equal to the CRT, as with writeback_sinf.
inline float world_powf( float x, float y ) { return static_cast< float >( std::pow( static_cast< double >( x ), static_cast< double >( y ) ) ); }

// btRigidBody::integrateVelocities @0x50E490 (ecx body, timeStep in xmm1),
// from the disassembly: nothing for a static or kinematic body (flags & 3);
// linear += force * (invMass * dt); angular += (invInertiaWorld * torque) *
// dt, each row (r1 ty + r0 tx) + r2 tz; then if |angular| * dt > pi/2
// (0x3FC90FDB, ordered >) angular *= (pi/2 / dt) / |angular|.
inline void body_integrate_velocities( solver_rigid_t& b, float dt )
{
    if ( ( b.collision_flags & 3 ) != 0 )
        return;
    const float s = b.inverse_mass * dt;
    b.linear_velocity[ 0 ] = b.linear_velocity[ 0 ] + b.total_force[ 0 ] * s;
    b.linear_velocity[ 1 ] = b.linear_velocity[ 1 ] + b.total_force[ 1 ] * s;
    b.linear_velocity[ 2 ] = b.linear_velocity[ 2 ] + b.total_force[ 2 ] * s;
    const float* m = b.inv_inertia_world;
    const float* t = b.total_torque;
    const float r0 = ( m[ 1 ] * t[ 1 ] + m[ 0 ] * t[ 0 ] ) + m[ 2 ] * t[ 2 ];
    const float r1 = ( m[ 5 ] * t[ 1 ] + m[ 4 ] * t[ 0 ] ) + m[ 6 ] * t[ 2 ];
    const float r2 = ( m[ 9 ] * t[ 1 ] + m[ 8 ] * t[ 0 ] ) + m[ 10 ] * t[ 2 ];
    const float ax = r0 * dt + b.angular_velocity[ 0 ];
    const float ay = b.angular_velocity[ 1 ] + r1 * dt;
    const float az = b.angular_velocity[ 2 ] + r2 * dt;
    b.angular_velocity[ 0 ] = ax;
    b.angular_velocity[ 1 ] = ay;
    b.angular_velocity[ 2 ] = az;
    const float speed = std::sqrt( ( ay * ay + ax * ax ) + az * az );
    if ( speed * dt > 1.57079637f )             // 0x3FC90FDB
    {
        const float k = ( 1.57079637f / dt ) / speed;
        b.angular_velocity[ 0 ] = ax * k;
        b.angular_velocity[ 1 ] = ay * k;
        b.angular_velocity[ 2 ] = az * k;
    }
}

// btRigidBody::applyDamping @0x50DCE0 (ecx body, timeStep in xmm1), from the
// disassembly: linear *= powf(1 - linearDamping, dt), angular *=
// powf(1 - angularDamping, dt). With m_additionalDamping: both velocities
// *= the factor if ang^2 < angThreshold^2 AND lin^2 < linThreshold^2
// (angular tested first, ordered compares); then Bullet's quirk, kept - the
// LINEAR speed is compared against m_linearDamping and the angular speed
// against m_angularDamping: below it, a speed over 0.005 loses 0.005 along
// its direction, otherwise all four lanes are zeroed.
inline void body_apply_damping( solver_rigid_t& b, float dt )
{
    const float lp = world_powf( 1.0f - b.linear_damping, dt );
    b.linear_velocity[ 0 ] = b.linear_velocity[ 0 ] * lp;
    b.linear_velocity[ 1 ] = lp * b.linear_velocity[ 1 ];
    b.linear_velocity[ 2 ] = lp * b.linear_velocity[ 2 ];
    const float ap = world_powf( 1.0f - b.angular_damping, dt );
    float* av = b.angular_velocity;
    av[ 0 ] = av[ 0 ] * ap;
    av[ 1 ] = ap * av[ 1 ];
    av[ 2 ] = ap * av[ 2 ];
    if ( !b.additional_damping )
        return;

    float* lv = b.linear_velocity;
    const float ang2 = ( av[ 1 ] * av[ 1 ] + av[ 0 ] * av[ 0 ] ) + av[ 2 ] * av[ 2 ];
    if ( b.additional_angular_threshold_sqr > ang2 )
    {
        const float lin2 = ( lv[ 0 ] * lv[ 0 ] + lv[ 1 ] * lv[ 1 ] ) + lv[ 2 ] * lv[ 2 ];
        if ( b.additional_linear_threshold_sqr > lin2 )
        {
            const float f = b.additional_damping_factor;
            av[ 0 ] = f * av[ 0 ];
            av[ 1 ] = f * av[ 1 ];
            av[ 2 ] = f * av[ 2 ];
            lv[ 0 ] = lv[ 0 ] * f;
            lv[ 1 ] = f * lv[ 1 ];
            lv[ 2 ] = f * lv[ 2 ];
        }
    }

    // 0x3BA3D70A, the 0.005 damping step.
    const float step = 0.00499999989f;
    const float speed = std::sqrt( ( lv[ 0 ] * lv[ 0 ] + lv[ 1 ] * lv[ 1 ] ) + lv[ 2 ] * lv[ 2 ] );
    if ( b.linear_damping > speed )
    {
        if ( speed > step )
        {
            const float k = 1.0f / speed;
            const float dx = ( lv[ 0 ] * k ) * step;
            const float dy = ( lv[ 1 ] * k ) * step;
            const float dz = ( lv[ 2 ] * k ) * step;
            lv[ 0 ] = lv[ 0 ] - dx;
            lv[ 1 ] = lv[ 1 ] - dy;
            lv[ 2 ] = lv[ 2 ] - dz;
        }
        else
            lv[ 0 ] = lv[ 1 ] = lv[ 2 ] = lv[ 3 ] = 0.0f;
    }
    const float aspeed = std::sqrt( ( av[ 0 ] * av[ 0 ] + av[ 1 ] * av[ 1 ] ) + av[ 2 ] * av[ 2 ] );
    if ( b.angular_damping > aspeed )
    {
        if ( aspeed > step )
        {
            const float k = 1.0f / aspeed;
            const float dx = ( av[ 0 ] * k ) * step;
            const float dy = ( av[ 1 ] * k ) * step;
            const float dz = ( av[ 2 ] * k ) * step;
            av[ 0 ] = av[ 0 ] - dx;
            av[ 1 ] = av[ 1 ] - dy;
            av[ 2 ] = av[ 2 ] - dz;
        }
        else
            av[ 0 ] = av[ 1 ] = av[ 2 ] = av[ 3 ] = 0.0f;
    }
}

// btDiscreteDynamicsWorld::predictUnconstraintMotion @0x6031B0 (ecx world,
// timeStep on the stack) over the non-static rigid bodies: for a body
// neither static nor kinematic, integrateVelocities THEN applyDamping (2.79's
// order), then predictIntegratedTransform inlined into the interpolation
// transform (+80..+143) - the same integrate_transform as everywhere else.
inline void world_predict_unconstraint_motion( solver_rigid_t* const* bodies, int count, float dt )
{
    for ( int i = 0; i < count; ++i )
    {
        solver_rigid_t& b = *bodies[ i ];
        if ( ( b.collision_flags & 3 ) != 0 )
            continue;
        body_integrate_velocities( b, dt );
        body_apply_damping( b, dt );
        integrate_transform( b.basis, b.origin, b.linear_velocity, b.angular_velocity, dt, b.interpolation_basis,
            b.interpolation_origin );
    }
}

// btDiscreteDynamicsWorld::applyGravity @0x600540: for every non-static
// rigid body not island-sleeping (2) / disable-simulation (5) and neither
// static nor kinematic, totalForce += linearFactor * m_gravity.
inline void world_apply_gravity( solver_rigid_t* const* bodies, int count )
{
    for ( int i = 0; i < count; ++i )
    {
        solver_rigid_t& b = *bodies[ i ];
        if ( b.activation_state == 2 || b.activation_state == 5 || ( b.collision_flags & 3 ) != 0 )
            continue;
        for ( int k = 0; k < 3; ++k )
            b.total_force[ k ] = b.total_force[ k ] + b.linear_factor[ k ] * b.gravity[ k ];
    }
}

// btDiscreteDynamicsWorld::clearForces @0x6004C0: totalForce and
// totalTorque (all eight lanes) zeroed on every non-static rigid body.
inline void world_clear_forces( solver_rigid_t* const* bodies, int count )
{
    for ( int i = 0; i < count; ++i )
        for ( int k = 0; k < 4; ++k )
            bodies[ i ]->total_force[ k ] = bodies[ i ]->total_torque[ k ] = 0.0f;
}

// The CRT's __libm_sse2_acosf - a DLL import, as world_powf: float(acos(double)),
// NOT proven bit-equal to the CRT.
inline float world_acosf( float x ) { return static_cast< float >( std::acos( static_cast< double >( x ) ) ); }

// btTransformUtil::calculateDiffAxisAngle @0x50F260 (ecx transform0, edx
// transform1, axis and angle out), from the disassembly. Bases are rows
// of four. dmat = basis1 * inverse(basis0) - the inverse by cofactors
// (cofac(r1,c1,r2,c2) = m[r1][c1] m[r2][c2] - m[r1][c2] m[r2][c1], 1/det
// with det = (m00 co.x + m01 co.y) + m02 co.z), the product
// C[i][j] = (A[i][0] B[0][j] + A[i][1] B[1][j]) + A[i][2] B[2][j]; then
// getRotation, the quaternion scaled by 1/sqrt(|q|^2), w clamped to
// [-1, 1] (-1 > w first), angle = acosf(w) * 2; the axis scaled by
// 1/sqrt(|axis|^2), or (1, 0, 0, 0) when FLT_EPSILON^2 (0x28800000) >
// |axis|^2.
inline void transform_diff_axis_angle( const float* basis0, const float* basis1, float* axis, float& angle )
{
    const float* m = basis0;
    auto at = [ m ]( int r, int c ) { return m[ 4 * r + c ]; };
    auto cofac = [ &at ]( int r1, int c1, int r2, int c2 ) { return at( r1, c1 ) * at( r2, c2 ) - at( r1, c2 ) * at( r2, c1 ); };
    const float cx = cofac( 1, 1, 2, 2 );
    const float cy = cofac( 1, 2, 2, 0 );
    const float cz = cofac( 1, 0, 2, 1 );
    const float s = 1.0f / ( ( at( 0, 0 ) * cx + at( 0, 1 ) * cy ) + at( 0, 2 ) * cz );
    const float inv[ 3 ][ 3 ] = {
        { cx * s, cofac( 0, 2, 2, 1 ) * s, cofac( 0, 1, 1, 2 ) * s },
        { cy * s, cofac( 0, 0, 2, 2 ) * s, cofac( 0, 2, 1, 0 ) * s },
        { cz * s, cofac( 0, 1, 2, 0 ) * s, cofac( 0, 0, 1, 1 ) * s } };
    float dmat[ 12 ];
    for ( int i = 0; i < 3; ++i )
    {
        for ( int j = 0; j < 3; ++j )
            dmat[ 4 * i + j ] = ( basis1[ 4 * i ] * inv[ 0 ][ j ] + basis1[ 4 * i + 1 ] * inv[ 1 ][ j ] )
                + basis1[ 4 * i + 2 ] * inv[ 2 ][ j ];
        dmat[ 4 * i + 3 ] = 0.0f;
    }
    float q[ 4 ];
    basis_rotation( dmat, q );
    const float k = 1.0f / std::sqrt( ( ( q[ 0 ] * q[ 0 ] + q[ 1 ] * q[ 1 ] ) + q[ 2 ] * q[ 2 ] ) + q[ 3 ] * q[ 3 ] );
    const float x = q[ 0 ] * k;
    const float y = q[ 1 ] * k;
    const float z = q[ 2 ] * k;
    float w = q[ 3 ] * k;
    if ( -1.0f > w )
        w = -1.0f;
    else if ( w > 1.0f )
        w = 1.0f;
    angle = world_acosf( w ) * 2.0f;
    const float len = ( x * x + y * y ) + z * z;
    if ( 1.42108547e-14f > len )
    {
        axis[ 0 ] = 1.0f;
        axis[ 1 ] = axis[ 2 ] = axis[ 3 ] = 0.0f;
        return;
    }
    const float r = 1.0f / std::sqrt( len );
    axis[ 0 ] = r * x;
    axis[ 1 ] = r * y;
    axis[ 2 ] = r * z;
    axis[ 3 ] = 0.0f;
}

// btTransformUtil::calculateVelocity @0x50F7D0 (ecx transform0, edx
// transform1, timeStep in xmm2, linear and angular out on the stack): both
// MULTIPLY by 1 / timeStep - linear (1/dt) * (origin1 - origin0), angular
// (1/dt) * (axis * angle); w lanes 0.
inline void transform_calculate_velocity( const float* basis0, const float* origin0, const float* basis1,
    const float* origin1, float dt, float* linear, float* angular )
{
    const float inv = 1.0f / dt;
    linear[ 0 ] = inv * ( origin1[ 0 ] - origin0[ 0 ] );
    linear[ 1 ] = inv * ( origin1[ 1 ] - origin0[ 1 ] );
    linear[ 2 ] = inv * ( origin1[ 2 ] - origin0[ 2 ] );
    linear[ 3 ] = 0.0f;
    float axis[ 4 ];
    float angle;
    transform_diff_axis_angle( basis0, basis1, axis, angle );
    angular[ 0 ] = inv * ( axis[ 0 ] * angle );
    angular[ 1 ] = inv * ( axis[ 1 ] * angle );
    angular[ 2 ] = inv * ( axis[ 2 ] * angle );
    angular[ 3 ] = 0.0f;
}

// The motion state's getWorldTransform (vtable slot 1 of the object at
// body +500) is Vu's code, not Bullet's: it is a caller-supplied hook here,
// writing the body's world transform (+16 basis, +64 origin). Null means
// the body has no motion state - retail then keeps the current transform.
using body_motion_fn = void ( * )( void* user, solver_rigid_t& body );

// btRigidBody::saveKinematicState @0x50DB40 (ecx body, timeStep in xmm1):
// skipped when timeStep == 0 (ordered: a NaN step proceeds); the motion
// state's transform; calculateVelocity from the interpolation transform
// to it into the linear / angular velocity; the interpolation velocities
// and transform take the new ones.
inline void body_save_kinematic_state( solver_rigid_t& b, float dt, body_motion_fn motion, void* user )
{
    if ( dt == 0.0f )
        return;
    if ( motion )
        motion( user, b );
    transform_calculate_velocity( b.interpolation_basis, b.interpolation_origin, b.basis, b.origin, dt,
        b.linear_velocity, b.angular_velocity );
    for ( int i = 0; i < 4; ++i )
    {
        b.interpolation_linear[ i ] = b.linear_velocity[ i ];
        b.interpolation_angular[ i ] = b.angular_velocity[ i ];
    }
    for ( int i = 0; i < 12; ++i )
        b.interpolation_basis[ i ] = b.basis[ i ];
    for ( int i = 0; i < 4; ++i )
        b.interpolation_origin[ i ] = b.origin[ i ];
}

// btDiscreteDynamicsWorld::saveKinematicState @0x6002E0 (ecx world,
// timeStep on the stack) over the COLLISION OBJECT array (+8 / +16): a
// rigid body (internalType & 2) not island-sleeping and kinematic
// (flags & 2). `motion` is consulted only for bodies with a motion state
// (has_motion[i]).
inline void world_save_kinematic_state( solver_rigid_t* const* objects, const char* has_motion, int count, float dt,
    body_motion_fn motion, void* user )
{
    for ( int i = 0; i < count; ++i )
    {
        solver_rigid_t& o = *objects[ i ];
        if ( ( o.internal_type & 2 ) == 0 || o.activation_state == 2 || ( o.collision_flags & 2 ) == 0 )
            continue;
        body_save_kinematic_state( o, dt, has_motion[ i ] ? motion : nullptr, user );
    }
}

// btRigidBody::updateInertiaTensor @0x50E1D0: basis * diag(invInertiaLocal)
// * basis^T into +272, from the disassembly. Entry [r][c] is
// (b[c][0] * s[r][0] + b[c][1] * s[r][1]) + b[c][2] * s[r][2] with
// s[r][k] = invInertiaLocal[k] * b[r][k]; the w lanes are zeroed.
inline void body_update_inertia_tensor( solver_rigid_t& b )
{
    const float* m = b.basis;
    const float* d = b.inv_inertia_local;
    float s[ 3 ][ 3 ];
    for ( int r = 0; r < 3; ++r )
        for ( int k = 0; k < 3; ++k )
            s[ r ][ k ] = d[ k ] * m[ 4 * r + k ];
    float w[ 12 ];
    for ( int r = 0; r < 3; ++r )
    {
        for ( int c = 0; c < 3; ++c )
            w[ 4 * r + c ] = ( m[ 4 * c ] * s[ r ][ 0 ] + m[ 4 * c + 1 ] * s[ r ][ 1 ] ) + m[ 4 * c + 2 ] * s[ r ][ 2 ];
        w[ 4 * r + 3 ] = 0.0f;
    }
    for ( int i = 0; i < 12; ++i )
        b.inv_inertia_world[ i ] = w[ i ];
}

// btRigidBody::setCenterOfMassTransform @0x50E660, which integrateTransforms
// calls for proceedToTransform: the interpolation transform takes the OLD
// transform for a static or kinematic body (flags & 3 - retail's test, see
// audit.md's btRigidBody table) and the new one otherwise; the
// interpolation velocities take the current ones; then the new transform
// and the inertia tensor.
inline void body_set_center_of_mass_transform( solver_rigid_t& b, const float* basis, const float* origin )
{
    const bool keep = ( b.collision_flags & 3 ) != 0;
    for ( int i = 0; i < 12; ++i )
        b.interpolation_basis[ i ] = keep ? b.basis[ i ] : basis[ i ];
    for ( int i = 0; i < 4; ++i )
        b.interpolation_origin[ i ] = keep ? b.origin[ i ] : origin[ i ];
    for ( int i = 0; i < 4; ++i )
    {
        b.interpolation_linear[ i ] = b.linear_velocity[ i ];
        b.interpolation_angular[ i ] = b.angular_velocity[ i ];
    }
    for ( int i = 0; i < 12; ++i )
        b.basis[ i ] = basis[ i ];
    for ( int i = 0; i < 4; ++i )
        b.origin[ i ] = origin[ i ];
    body_update_inertia_tensor( b );
}

// btDiscreteDynamicsWorld::integrateTransforms @0x602870 (ecx world,
// timeStep on the stack, retn 4) over the non-static rigid bodies (+204 /
// +212; NOT null-checked here, unlike updateActivationState): hit fraction
// 1; skip island-sleeping (2), disable-simulation (5), static or kinematic;
// predictIntegratedTransform inlined (integrate_transform with the linear /
// angular velocity); then proceedToTransform.
//
// The CCD branch - dispatchInfo.m_useContinuous (world +44) set, the
// squared ccdMotionThreshold (+260) != 0 (ordered: a NaN threshold passes)
// and the squared motion > it, for a CONVEX shape (type < 20) - sweeps a
// sphere through the collision world (0x602720 / 0x43D700 / 0x54F8E0 /
// resolveSingleCollision 0x625260). That path is NOT recreated: the
// function returns false the moment a body reaches it, before touching
// that body. `shape_types[i]` is body i's collision shape type (+204 ->
// +4). The boat's compound shape (>= 20) never reaches it (audit.md batch 4).
inline bool world_integrate_transforms( solver_rigid_t* const* bodies, const int* shape_types, int count, float time_step,
    bool use_continuous )
{
    for ( int i = 0; i < count; ++i )
    {
        solver_rigid_t& b = *bodies[ i ];
        b.hit_fraction = 1.0f;
        if ( b.activation_state == 2 || b.activation_state == 5 || ( b.collision_flags & 3 ) != 0 )
            continue;
        float basis[ 12 ];
        float origin[ 4 ];
        integrate_transform( b.basis, b.origin, b.linear_velocity, b.angular_velocity, time_step, basis, origin );
        const float dx = origin[ 0 ] - b.origin[ 0 ];
        const float dy = origin[ 1 ] - b.origin[ 1 ];
        const float dz = origin[ 2 ] - b.origin[ 2 ];
        const float motion = ( dy * dy + dx * dx ) + dz * dz;
        if ( use_continuous )
        {
            const float threshold = b.ccd_motion_threshold * b.ccd_motion_threshold;
            if ( threshold != 0.0f && motion > threshold && shape_types[ i ] < 20 )
                return false;   // CCD motion clamping - not recreated
        }
        body_set_center_of_mass_transform( b, basis, origin );
    }
    return true;
}

// btDiscreteDynamicsWorld's per-step body passes, recreated from retail
// 2.79. Retail wraps each in a CProfileSample; timing only, NOT recreated.

// btDiscreteDynamicsWorld::updateActivationState @0x601640 (ecx world,
// timeStep in xmm1) over the world's non-static rigid bodies (count +204,
// data +212; a null entry is skipped). btRigidBody::updateDeactivation and
// wantsSleeping are inlined. Read from the disassembly:
//  - the sleeping test is !(threshold^2 > |v|^2) for either velocity, so a
//    NaN velocity resets the timer;
//  - gDeactivationTime is the .rdata constant 2.0 (0x6F76F8 = 0x40000000),
//    compared as time > 2.0 - there is no "== 0" early-out to model;
//  - gDisableDeactivation is the .bss byte 0x758FFF (zero at load; any
//    later writer UNKNOWN), taken here as an argument.
inline void world_update_activation_state( solver_rigid_t* const* bodies, int count, float time_step,
    bool disable_deactivation )
{
    for ( int i = 0; i < count; ++i )
    {
        solver_rigid_t* b = bodies[ i ];
        if ( !b )
            continue;
        const int state = b->activation_state;
        if ( state != 2 && state != 4 )
        {
            const float* lv = b->linear_velocity;
            const float* av = b->angular_velocity;
            const float lin = ( lv[ 0 ] * lv[ 0 ] + lv[ 1 ] * lv[ 1 ] ) + lv[ 2 ] * lv[ 2 ];
            const float ang = ( av[ 0 ] * av[ 0 ] + av[ 1 ] * av[ 1 ] ) + av[ 2 ] * av[ 2 ];
            const float lt = b->linear_sleeping_threshold * b->linear_sleeping_threshold;
            const float at = b->angular_sleeping_threshold * b->angular_sleeping_threshold;
            if ( lt > lin && at > ang )
                b->deactivation_time = b->deactivation_time + time_step;
            else
            {
                b->deactivation_time = 0.0f;
                if ( state != 5 )
                    b->activation_state = 0;
            }
        }

        const int now = b->activation_state;
        if ( now == 4 )
            continue;
        if ( !disable_deactivation && ( now == 2 || now == 3 || b->deactivation_time > 2.0f ) )
        {
            if ( ( b->collision_flags & 3 ) != 0 )
            {
                if ( now != 5 )
                    b->activation_state = 2;
            }
            else
            {
                if ( now == 1 )
                    b->activation_state = 3;
                if ( b->activation_state == 2 )
                {
                    for ( int k = 0; k < 4; ++k )
                    {
                        b->angular_velocity[ k ] = 0.0f;
                        b->linear_velocity[ k ] = 0.0f;
                    }
                }
            }
        }
        else if ( now != 5 )
            b->activation_state = 1;
    }
}
