#pragma once

#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

#include <workspace/core/dynamics/row.hxx>
#include <workspace/core/dynamics/world.hxx>

// Building a body, recreated from retail's disassembly (memory/audit.md
// "Batch 9a"; scratchpad dis_rb.txt): the VuRigidBody ctor 0x527B80 over the
// btCollisionObject ctor 0x5470A0, setupRigidBody 0x50D1D0, setMassProps
// 0x50E040 and updateInertiaTensor 0x50E1D0 (world.hxx). The object is
// retail's 0x2D4-byte VuRigidBody image: solver_rigid_t is its first 672 bytes
// (+672 m_contactSolverType, +676 m_frictionSolverType, then the Vu fields in
// `tail`). POINTER words - +200 broadphase handle, +204 / +212 the shape and
// root shape, +500 the motion state, +516 the constraint-ref data, +704 the
// owner - are kept as 0 here (the caller holds the pointers); the check masks
// them.
struct vu_rigid_body_t
{
    solver_rigid_t body;
    unsigned char tail[ 0x2D4 - sizeof( solver_rigid_t ) ];
};
static_assert( sizeof( solver_rigid_t ) == 672, "btRigidBody's first 672 bytes" );

inline uint32_t& body_word( vu_rigid_body_t& b, int offset )
{
    return *reinterpret_cast< uint32_t* >( reinterpret_cast< unsigned char* >( &b ) + offset );
}

inline unsigned char& body_byte( vu_rigid_body_t& b, int offset )
{
    return reinterpret_cast< unsigned char* >( &b )[ offset ];
}

// btRigidBodyConstructionInfo as setupRigidBody reads it, by offset: +0 mass,
// +4 motion state, +0x10 the start transform (64 b), +0x50 the shape, +0x60
// the local inertia, +0x70 / +0x74 linear / angular damping, +0x78 friction,
// +0x7C restitution, +0x80 / +0x84 the sleeping thresholds, +0x88 the
// additional damping byte, +0x8C .. +0x98 its factor and thresholds.
inline float cinfo_float( const unsigned char* c, int offset )
{
    float f;
    std::memcpy( &f, c + offset, 4 );
    return f;
}

inline uint32_t cinfo_word( const unsigned char* c, int offset )
{
    uint32_t u;
    std::memcpy( &u, c + offset, 4 );
    return u;
}

// The motion state's getWorldTransform (vt+4, thiscall(transform*)) - Vu code:
// a hook. Returns false when the hook is absent (not recreated).
using body_motion_get_fn = bool ( * )( void* user, uint32_t motion_state, float* transform );

// btCollisionObject ctor @0x5470A0: +0xB0 anisotropic friction (1, 1, 1, 0),
// +0xC0 0, +0xC4 contact processing threshold 1e18, +0xC8 .. +0xD4 0, +0xD8
// flags 1, +0xDC / +0xE0 island tag / companion -1, +0xE4 activation 1, +0xE8
// 0, +0xEC friction 0.5, +0xF0 0, +0xF4 internal type 1, +0xF8 0, +0xFC hit
// fraction 1.0, +0x100 .. +0x108 0, then setIdentity 0x413280 on +16 (basis
// identity, origin 0 - 16 dwords).
inline void collision_object_construct( vu_rigid_body_t& b )
{
    static const uint32_t words[ 23 ] = { 0x3F800000u, 0x3F800000u, 0x3F800000u, 0, 0, 0x5D5E0B6Bu, 0, 0, 0, 0, 1, 0xFFFFFFFFu,
        0xFFFFFFFFu, 1, 0, 0x3F000000u, 0, 1, 0, 0x3F800000u, 0, 0, 0 };
    for ( int k = 0; k < 23; ++k )
        body_word( b, 0xB0 + 4 * k ) = words[ k ];
    static const float identity[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 };
    std::memcpy( reinterpret_cast< unsigned char* >( &b ) + 16, identity, 64 );
}

// setMassProps @0x50E040 (thiscall; mass in XMM1, inertia*; retn 4): `ucomiss
// mass, 0; lahf; test ah, 44h; jp` - an ORDERED zero mass sets flags (+216)
// bit 1 and invMass (+352) 0, anything else (NaN included) clears the bit and
// invMass = 1 / mass; gravity (+384) = gravity acceleration (+400) * mass, w
// 0; each inertia lane: ordered 0 -> 0, else 1 / x, stored z (+424), x (+416),
// y (+420), +428 0; invMass vector (+624) = linear factor (+368) * invMass, w 0.
inline void rigid_set_mass_props( solver_rigid_t& b, float mass, const float* inertia )
{
    auto zero = []( float x ) { return _mm_comieq_ss( _mm_set_ss( x ), _mm_setzero_ps( ) ) != 0; };   // ordered ==
    if ( zero( mass ) )
    {
        b.collision_flags |= 1;
        b.inverse_mass = 0.0f;
    }
    else
    {
        b.collision_flags &= ~1;
        b.inverse_mass = 1.0f / mass;
    }
    b.gravity[ 0 ] = b.gravity_acceleration[ 0 ] * mass;
    b.gravity[ 1 ] = b.gravity_acceleration[ 1 ] * mass;
    b.gravity[ 2 ] = b.gravity_acceleration[ 2 ] * mass;
    b.gravity[ 3 ] = 0.0f;
    const float iz = zero( inertia[ 2 ] ) ? 0.0f : 1.0f / inertia[ 2 ];
    const float iy = zero( inertia[ 1 ] ) ? 0.0f : 1.0f / inertia[ 1 ];
    const float ix = zero( inertia[ 0 ] ) ? 0.0f : 1.0f / inertia[ 0 ];
    b.inv_inertia_local[ 2 ] = iz;
    b.inv_inertia_local[ 0 ] = ix;
    b.inv_inertia_local[ 1 ] = iy;
    b.inv_inertia_local[ 3 ] = 0.0f;
    b.inverse_mass_vector[ 0 ] = b.linear_factor[ 0 ] * b.inverse_mass;
    b.inverse_mass_vector[ 1 ] = b.linear_factor[ 1 ] * b.inverse_mass;
    b.inverse_mass_vector[ 2 ] = b.linear_factor[ 2 ] * b.inverse_mass;
    b.inverse_mass_vector[ 3 ] = 0.0f;
}

// btClamped as setupRigidBody inlines it: `comiss 0, x; jbe` -> 0 when 0 > x;
// else `comiss x, 1; cmovbe` -> x when x <= 1 OR unordered (NaN passes), else 1.
inline float body_clamp01( float x )
{
    if ( _mm_comigt_ss( _mm_setzero_ps( ), _mm_set_ss( x ) ) )
        return 0.0f;
    return _mm_comigt_ss( _mm_set_ss( x ), _mm_set_ss( 1.0f ) ) ? 1.0f : x;
}

// setupRigidBody @0x50D1D0 (thiscall; cinfo*; retn 4), in retail's store order.
inline bool rigid_setup( vu_rigid_body_t& vb, const unsigned char* cinfo, uint32_t& unique_id, body_motion_get_fn motion,
    void* user )
{
    solver_rigid_t& b = vb.body;
    b.internal_type = 2;
    std::memset( b.linear_velocity, 0, 16 );
    std::memset( b.angular_velocity, 0, 16 );
    b.angular_factor[ 0 ] = b.angular_factor[ 1 ] = b.angular_factor[ 2 ] = 1.0f;
    b.angular_factor[ 3 ] = 0.0f;
    b.linear_factor[ 0 ] = b.linear_factor[ 1 ] = b.linear_factor[ 2 ] = 1.0f;
    b.linear_factor[ 3 ] = 0.0f;
    std::memset( b.gravity, 0, 16 );
    std::memset( b.gravity_acceleration, 0, 16 );
    std::memset( b.total_force, 0, 16 );
    std::memset( b.total_torque, 0, 16 );
    b.linear_damping = body_clamp01( cinfo_float( cinfo, 0x70 ) );
    b.angular_damping = body_clamp01( cinfo_float( cinfo, 0x74 ) );
    b.linear_sleeping_threshold = cinfo_float( cinfo, 0x80 );
    b.angular_sleeping_threshold = cinfo_float( cinfo, 0x84 );
    const uint32_t ms = cinfo_word( cinfo, 0x04 );
    body_word( vb, 0x1F4 ) = 0;   // +500 the motion state pointer: held by the caller
    body_word( vb, 0x2A0 ) = 0;
    body_word( vb, 0x2A4 ) = 0;
    b.additional_damping = cinfo[ 0x88 ];
    b.additional_damping_factor = cinfo_float( cinfo, 0x8C );
    b.additional_linear_threshold_sqr = cinfo_float( cinfo, 0x90 );
    b.additional_angular_threshold_sqr = cinfo_float( cinfo, 0x94 );
    b.additional_angular_damping_factor = cinfo_float( cinfo, 0x98 );
    if ( ms )
    {
        if ( !motion || !motion( user, ms, b.basis ) )
            return false;   // the motion state's getWorldTransform (vt+4): Vu code
    }
    else
        std::memcpy( b.basis, cinfo + 0x10, 64 );
    std::memcpy( b.interpolation_basis, b.basis, 64 );
    std::memset( b.interpolation_linear, 0, 16 );
    std::memset( b.interpolation_angular, 0, 16 );
    b.friction = cinfo_float( cinfo, 0x78 );
    b.restitution = cinfo_float( cinfo, 0x7C );
    // vt+8 setCollisionShape (VuRigidBody vt 0x74CFF0 slot 2 = 0x50FB90): +204 /
    // +212 = the shape - pointer words, held by the caller.
    body_word( vb, 0x210 ) = unique_id;   // m_debugBodyId from the global 0x759030, then ++
    unique_id = unique_id + 1;
    rigid_set_mass_props( b, cinfo_float( cinfo, 0x00 ), reinterpret_cast< const float* >( cinfo + 0x60 ) );
    body_update_inertia_tensor( b );
    b.rigidbody_flags = 0;
    std::memset( b.delta_linear, 0, 16 );
    std::memset( b.delta_angular, 0, 16 );
    b.inverse_mass_vector[ 0 ] = b.linear_factor[ 0 ] * b.inverse_mass;
    b.inverse_mass_vector[ 1 ] = b.linear_factor[ 1 ] * b.inverse_mass;
    b.inverse_mass_vector[ 2 ] = b.linear_factor[ 2 ] * b.inverse_mass;
    b.inverse_mass_vector[ 3 ] = 0.0f;
    std::memset( b.push_velocity, 0, 16 );
    std::memset( b.turn_velocity, 0, 16 );
    return true;
}

// The VuRigidBody ctor @0x527B80 (thiscall; cinfo*, owner, group u16, mask
// u16; retn 10h): the btCollisionObject ctor; the btRigidBody vtable; the
// constraint-ref array (+520 owns byte 1, +516 / +508 / +512 0); setupRigidBody;
// +704 the owner (a pointer: held by the caller), +714 group, +716 mask; the
// VuRigidBody vtable; +708 0, +712 byte 0, +720 0.
inline bool vu_rigid_body_construct( vu_rigid_body_t& vb, const unsigned char* cinfo, short group, short mask, uint32_t& unique_id,
    body_motion_get_fn motion, void* user )
{
    std::memset( &vb, 0, sizeof( vb ) );   // retail's object memory before the ctor: the caller's (UNKNOWN); 0 here
    collision_object_construct( vb );
    body_byte( vb, 0x208 ) = 1;
    body_word( vb, 0x204 ) = 0;
    body_word( vb, 0x1FC ) = 0;
    body_word( vb, 0x200 ) = 0;
    if ( !rigid_setup( vb, cinfo, unique_id, motion, user ) )
        return false;
    body_word( vb, 0x2C0 ) = 0;   // +704 the owner pointer: held by the caller
    std::memcpy( reinterpret_cast< unsigned char* >( &vb ) + 0x2CA, &group, 2 );
    std::memcpy( reinterpret_cast< unsigned char* >( &vb ) + 0x2CC, &mask, 2 );
    body_word( vb, 0x2C4 ) = 0;
    body_byte( vb, 0x2C8 ) = 0;
    body_word( vb, 0x2D0 ) = 0;
    return true;
}
