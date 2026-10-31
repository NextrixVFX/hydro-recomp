#pragma once

#include <cmath>
#include <cstdint>
#include <string>

// One entry of the trigger manager's instigator list (0x759064: +4 the array,
// +8 the count, 0x34 bytes each): +8 its mask, +0xC the previous position,
// +0x1C a radius, +0x20 the current position, +0x30 the radius's rate.
struct trigger_instigator_t
{
    std::uint32_t mask;
    float prev[ 3 ];
    float radius;
    float cur[ 3 ];
    float rate;
};

// VuTriggerPlaneEntity_testCrossings 0x5D1570 for one instigator, operation
// for operation. `world` is the trigger transform's world (+0x50: X row 0..2,
// the plane NORMAL row 4..6, Z row 8..10, centre 12..14; scale NOT in it) and
// `scale` its Scale (+0xB0); `mask` the entity's +0x80. Returns -1 (no
// crossing), else the flag handed to VuTriggerEntity_fireCrossing 0x61CB70:
// 1 = "OnEnter" (the previous signed distance was > 0), 0 = "OnExit".
inline int trigger_plane_crossing( const float* world, const float* scale, std::uint32_t mask, const trigger_instigator_t& who )
{
    if ( ( who.mask & mask ) == 0 )
        return -1;
    const float n0 = world[ 4 ], n1 = world[ 5 ], n2 = world[ 6 ];
    const float c0 = world[ 12 ], c1 = world[ 13 ], c2 = world[ 14 ];
    const float d = -( ( n0 * c0 + n1 * c1 ) + n2 * c2 );
    const float a = ( ( who.prev[ 0 ] * n0 + who.prev[ 1 ] * n1 ) + who.prev[ 2 ] * n2 ) + d;
    const float b = ( ( who.cur[ 0 ] * n0 + who.cur[ 1 ] * n1 ) + who.cur[ 2 ] * n2 ) + d;
    // comiss 0, b * a; jb: a positive product (or a NaN) is no crossing.
    if ( !( 0.0f >= b * a ) )
        return -1;
    const bool enter = a > 0.0f;
    if ( enter == ( b > 0.0f ) )
        return -1;
    const float dq0 = who.cur[ 0 ] - who.prev[ 0 ], dq1 = who.cur[ 1 ] - who.prev[ 1 ], dq2 = who.cur[ 2 ] - who.prev[ 2 ];
    const float num = ( n0 * ( c0 - who.prev[ 0 ] ) + n1 * ( c1 - who.prev[ 1 ] ) ) + n2 * ( c2 - who.prev[ 2 ] );
    const float den = ( n0 * dq0 + n1 * dq1 ) + n2 * dq2;
    const float t = num / den;
    const float r1 = ( dq1 * t + who.prev[ 1 ] ) - c1;
    const float r0 = ( dq0 * t + who.prev[ 0 ] ) - c0;
    const float r2 = ( dq2 * t + who.prev[ 2 ] ) - c2;
    // VuVector_abs 0x4089C0 on each in-plane projection, less its half extent.
    const float e0 = std::fabs( ( world[ 1 ] * r1 + world[ 0 ] * r0 ) + world[ 2 ] * r2 ) - scale[ 0 ];
    const float e2 = std::fabs( ( world[ 9 ] * r1 + world[ 8 ] * r0 ) + world[ 10 ] * r2 ) - scale[ 2 ];
    const float outside = e0 > e2 ? e0 : e2; // maxss e0, e2
    const float radius = who.rate * t + who.radius;
    if ( !( radius > outside ) )
        return -1;
    return enter ? 1 : 0;
}

// A VuTriggerPlaneEntity or VuTriggerBoxEntity. Its volume IS its transform:
// a unit plane one unit thick in local Y, or a unit box, scaled and rotated
// into place.
struct trigger_t
{
    std::string path;
    std::string kind;
    // The transform's world (scale kept out) and Scale, and the Trigger Type
    // mask its postLoad 0x5D13E0 sets (+0x80 = 0x536180(type): 1 << the
    // type's index in the 0x75913C enum, 0 when it is not registered).
    float frame[ 16 ]{};
    float half[ 3 ]{ 1.0f, 1.0f, 1.0f };
    std::uint32_t mask = 0;
    float world[ 16 ]{};
    float inverse[ 16 ]{};
    // The authored Scale per axis, which is the volume's half extent.
    float scale[ 3 ]{ 1.0f, 1.0f, 1.0f };
    bool box = false;
    bool active = true;
    // The signed distance to the volume last frame. A plane is ONE UNIT thick
    // while a boat covers far more than that per frame, so a containment test
    // would simply miss it; what has to be detected is the SIGN CHANGE of that
    // distance across the step.
    float side = 0.0f;
    bool primed = false;
};

// A world point expressed in the volume's own space, where the plane is the
// XZ square from -1 to +1 and the normal is local Y.
inline void trigger_local( const trigger_t& gate, const float* at, float* out )
{
    const float* m = gate.inverse;
    out[ 0 ] = m[ 0 ] * at[ 0 ] + m[ 4 ] * at[ 1 ] + m[ 8 ] * at[ 2 ] + m[ 12 ];
    out[ 1 ] = m[ 1 ] * at[ 0 ] + m[ 5 ] * at[ 1 ] + m[ 9 ] * at[ 2 ] + m[ 13 ];
    out[ 2 ] = m[ 2 ] * at[ 0 ] + m[ 6 ] * at[ 1 ] + m[ 10 ] * at[ 2 ] + m[ 14 ];
}

// Signed distance from a world point to one trigger's volume - the exact
// quantity each testCrossings function tracks across a step, and whose sign
// change IS the crossing.
//
// PLANE, VuTriggerPlaneEntity_testCrossings @0x5d1570: the signed plane
// distance dot(normal, p) + d, normal being row 1 of the unscaled matrix. In
// plane-local space that is local Y (Scale.Y is authored 1 on every plane, and
// a positive scale cannot change a sign anyway).
//
// BOX, VuTriggerBoxEntity_testCrossings @0x5d1ab0: the OBB's own signed
// distance - the maximum over the three local axes of
// |dot(p - centre, row_i)| - halfExtent_i - and then minus the probe's radius.
// Through our inverse the projection is local_i * scale_i and the half extent
// is scale_i itself, so the term is scale_i * (|local_i| - 1): the same number
// with one multiply fewer, not a different formula.
//
// THE PROBE RADIUS IS NOT ZERO IN RETAIL. Both tests subtract the radius of
// the crossing sphere, which lives at VuInstigatorComponent+36 and is copied
// into the probe record by VuInstigatorComponent_register @0x531e10 (record+28
// previous, +48 current, both the same value). That component is built in
// code, not authored, and the boat writes the radius from somewhere in
// 0x41da90 / 0x421670 which has NOT been read yet - so our probe is a point.
// FLAGGED: a known gap of the boat's own radius at every volume boundary, not
// an invented constant.
//
// SCALE IS A HALF EXTENT - derived, not assumed. VuTriggerPlaneEntity_bounds
// @0x5d18b0 takes the 4x4 at shape+80 and multiplies it by the diagonal scale
// matrix VuMatrix_fromScale @0x407db0 builds from the float3 at shape+176. So
// the matrix at +80 is the UNSCALED rotation and translation, and +176/+184 -
// what the crossing test compares against - are Scale.x and Scale.z. Once the
// full transform is divided out the bound is +-1.0, not +-0.5.
//
// The matrix layout corroborates it: row 0 (+80..88) is the X axis, row 1
// (+96..104) is the plane NORMAL - which is why the authored Scale.Y is always
// 1 - and row 3 (+128..136) the centre.
inline float trigger_distance( const trigger_t& gate, const float* at, float probe_radius )
{
    float local[ 3 ];
    trigger_local( gate, at, local );
    if ( !gate.box )
        return local[ 1 ];
    float distance = gate.scale[ 0 ] * ( std::fabs( local[ 0 ] ) - 1.0f );
    for ( int axis = 1; axis < 3; ++axis )
    {
        const float term = gate.scale[ axis ] * ( std::fabs( local[ axis ] ) - 1.0f );
        if ( term > distance )
            distance = term;
    }
    return distance - probe_radius;
}
