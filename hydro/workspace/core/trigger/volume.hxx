#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

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

// ---- the trigger manager's instigators (g_vutriggermanager 0x759064) --------
//
// One record per registered VuInstigatorComponent (0x2C bytes; +8 its owner
// entity, +0x10 the mask, +0x14 / +0x18 / +0x1C the offset in the owner's
// frame, +0x24 the radius, +0x28 registered): +0 the component, +4 the owner's
// transform component (owner +0x50; its world at +0x50 .. +0x8C), then the
// crossing record trigger_instigator_t reads (+8 mask, +0xC previous, +0x1C its
// radius, +0x20 current, +0x30 its radius). The words +0x18 / +0x2C are the high
// half of a movq from an uninitialised stack slot - never read, not modelled.
// `owner` is ours: the race boat index the component's +8 entity is.
struct trigger_record_t
{
    std::uint8_t* component = nullptr;
    const std::uint8_t* transform = nullptr;
    trigger_instigator_t at{};
    int owner = -1;
};

struct trigger_manager_t
{
    std::vector< trigger_record_t > instigators;   // +4 / +8 / +0xC
};

inline float trigger_word_f( const std::uint8_t* p, int at )
{
    float f;
    std::memcpy( &f, p + at, 4 );
    return f;
}

// The point every instigator step computes (0x531F50 / 0x536350 / 0x536460,
// the same sequence in each): the component's offset through the transform's
// world rows, x = ((m+0x60 oy + m+0x50 ox) + m+0x70 oz) + m+0x80 per axis.
inline void trigger_instigator_point( const std::uint8_t* component, const std::uint8_t* transform, float* out )
{
    const float ox = trigger_word_f( component, 0x14 ), oy = trigger_word_f( component, 0x18 ), oz = trigger_word_f( component, 0x1C );
    for ( int k = 0; k < 3; ++k )
        out[ k ] = ( ( trigger_word_f( transform, 0x60 + 4 * k ) * oy + trigger_word_f( transform, 0x50 + 4 * k ) * ox ) +
                     trigger_word_f( transform, 0x70 + 4 * k ) * oz ) + trigger_word_f( transform, 0x80 + 4 * k );
}

// VuInstigatorComponent's gameInitialize 0x531F50 (thiscall comp): unless
// registered (+0x28), a record {comp, transform, +0x10 mask, the point as both
// positions, +0x24 as both radii} pushed on the back (0x5366C0 grows the array
// by half, at least to the count); then +0x28 = 1 either way.
inline void trigger_instigator_initialize( trigger_manager_t& m, std::uint8_t* component, const std::uint8_t* transform, int owner )
{
    if ( component[ 0x28 ] == 0 )
    {
        trigger_record_t r;
        r.component = component;
        r.transform = transform;
        r.owner = owner;
        std::memcpy( &r.at.mask, component + 0x10, 4 );
        trigger_instigator_point( component, transform, r.at.cur );
        std::memcpy( r.at.prev, r.at.cur, sizeof( r.at.prev ) );
        r.at.radius = r.at.rate = trigger_word_f( component, 0x24 );
        m.instigators.push_back( r );
    }
    component[ 0x28 ] = 1;
}

// 0x536350 (thiscall manager; comp; retn 4): EVERY record of that component
// (the walk does not stop at the first) gets the point as both its previous
// and its current position; the radii are left.
inline void trigger_instigator_update( trigger_manager_t& m, const std::uint8_t* component )
{
    for ( trigger_record_t& r : m.instigators )
    {
        if ( r.component != component )
            continue;
        trigger_instigator_point( r.component, r.transform, r.at.cur );
        std::memcpy( r.at.prev, r.at.cur, sizeof( r.at.prev ) );
    }
}

// 0x5362A0 (thiscall manager; comp; retn 4): the FIRST record of that
// component is overwritten by the last one (rep movsd, 13 dwords) and the count
// drops by one - the order is not kept. No match: nothing. +0x28 is the
// caller's to clear.
inline void trigger_instigator_release( trigger_manager_t& m, const std::uint8_t* component )
{
    for ( std::size_t k = 0; k < m.instigators.size( ); ++k )
    {
        if ( m.instigators[ k ].component != component )
            continue;
        m.instigators[ k ] = m.instigators.back( );
        m.instigators.pop_back( );
        return;
    }
}

// The "Triggers" tick 0x536460's first loop (registered by the manager's init
// 0x536110 in phase "Triggers"): each record's previous position / radius =
// its current ones, then the current = the point of its component through its
// stored transform, the radius = the component's +0x24 now. (The rest of the
// tick - +0x1C = 1, every listed trigger's vt+0x30, +0x1C = 0, the pending
// adds appended / removes 0x4AB7D0 applied, the dev draw vt+0x34 under g
// 0x759047 - is the caller's: the trigger list is c_world's.)
inline void trigger_instigators_step( trigger_manager_t& m )
{
    for ( trigger_record_t& r : m.instigators )
    {
        std::memcpy( r.at.prev, r.at.cur, sizeof( r.at.prev ) );
        r.at.radius = r.at.rate;
        trigger_instigator_point( r.component, r.transform, r.at.cur );
        r.at.rate = trigger_word_f( r.component, 0x24 );
    }
}

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
