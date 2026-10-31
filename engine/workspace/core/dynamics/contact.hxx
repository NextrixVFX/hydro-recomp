#pragma once

#include <workspace/core/dynamics/finish.hxx>

#include <cmath>
#include <cstring>

// Bullet 2.79's persistent contact manifold and btManifoldResult, recreated
// from retail (memory/audit.md "Batch 8a"). The point is the 288-byte 2.79
// btManifoldPoint (solver_contact_point_t) with btConstraintRow[3] at +192;
// the 2.79 point has no rolling friction.
//
// The three contact callbacks are globals in retail (gContactAddedCallback
// 0x7590D8, gContactProcessedCallback 0x7590D0, gContactDestroyedCallback
// 0x7590D4) whose targets are Vu code; they are hooks here, never invented.
struct contact_callbacks_t
{
    void* user = nullptr;
    void ( *added )( void* user, solver_contact_point_t& cp, solver_rigid_t* object0, int part0, int index0,
        solver_rigid_t* object1, int part1, int index1 ) = nullptr;
    void ( *processed )( void* user, solver_contact_point_t& cp, solver_rigid_t* body0, solver_rigid_t* body1 ) = nullptr;
    void ( *destroyed )( void* user, unsigned user_data ) = nullptr;
};

// The btManifoldResult members addContactPoint reads, at retail's offsets in
// comments: +4 manifold, +16 / +80 the two objects' transforms (rows of
// four, then the origin), +144 / +148 the objects, +152..+164 part / index.
struct manifold_result_t
{
    solver_manifold_t* manifold;            // +4
    float basis0[ 12 ];                     // +16
    float origin0[ 4 ];                     // +64
    float basis1[ 12 ];                     // +80
    float origin1[ 4 ];                     // +128
    solver_rigid_t* object0;                // +144
    solver_rigid_t* object1;                // +148
    int part_id0, part_id1, index0, index1; // +152 / +156 / +160 / +164
};

// btTransform::invXform as retail inlines it in addContactPoint: v = p -
// origin; out[k] = (basis[4 + k] * v.y + basis[k] * v.x) + basis[8 + k] * v.z.
inline void contact_inv_xform( const float* basis, const float* origin, const float* p, float* out )
{
    const float vx = p[ 0 ] - origin[ 0 ];
    const float vy = p[ 1 ] - origin[ 1 ];
    const float vz = p[ 2 ] - origin[ 2 ];
    for ( int k = 0; k < 3; ++k )
        out[ k ] = ( basis[ 4 + k ] * vy + basis[ k ] * vx ) + basis[ 8 + k ] * vz;
    out[ 3 ] = 0.0f;
}

// btTransform * point as refreshContactPoints inlines it: out[r] =
// ((basis[4r] p.x + basis[4r + 1] p.y) + basis[4r + 2] p.z) + origin[r].
inline void contact_xform( const float* basis, const float* origin, const float* p, float* out )
{
    for ( int r = 0; r < 3; ++r )
        out[ r ] = ( ( basis[ 4 * r ] * p[ 0 ] + basis[ 4 * r + 1 ] * p[ 1 ] ) + basis[ 4 * r + 2 ] * p[ 2 ] ) + origin[ r ];
    out[ 3 ] = 0.0f;
}

// btManifoldPoint(localA, localB, normal, distance) @0x5523E0: the four
// vectors copied whole (all four lanes), the distance, and exactly these
// zeroed: restitution, user data, applied impulse, the lateral-initialized
// byte, the six floats and lifetime at +120..+144, the three row impulses.
// The combined friction (+84) is NOT zeroed - the caller sets it.
inline void contact_point_init( solver_contact_point_t& p, const float* local_a, const float* local_b,
    const float* normal, float distance )
{
    std::memcpy( p.local_point_a, local_a, 16 );
    std::memcpy( p.local_point_b, local_b, 16 );
    std::memcpy( p.normal_world_on_b, normal, 16 );
    p.distance = distance;
    p.combined_restitution = 0.0f;
    p.user_persistent_data = 0;
    p.applied_impulse = 0.0f;
    p.lateral_friction_initialized = 0;
    p.lateral_impulse_1 = p.lateral_impulse_2 = 0.0f;
    p.contact_motion_1 = p.contact_motion_2 = 0.0f;
    p.contact_cfm_1 = p.contact_cfm_2 = 0.0f;
    p.life_time = 0;
    p.constraint_row[ 0 ][ 7 ] = p.constraint_row[ 1 ][ 7 ] = p.constraint_row[ 2 ][ 7 ] = 0.0f;
}

// getCacheEntry @0x5B6B00: the cached point whose localPointA is nearest,
// strictly within the breaking threshold squared; -1 if none.
inline int manifold_cache_entry( const solver_manifold_t& m, const solver_contact_point_t& p )
{
    float best = m.contact_breaking_threshold * m.contact_breaking_threshold;
    int nearest = -1;
    for ( int i = 0; i < m.num_contacts; ++i )
    {
        const float* c = m.points[ i ].local_point_a;
        const float dx = c[ 0 ] - p.local_point_a[ 0 ];
        const float dy = c[ 1 ] - p.local_point_a[ 1 ];
        const float dz = c[ 2 ] - p.local_point_a[ 2 ];
        const float d2 = ( dy * dy + dx * dx ) + dz * dz;
        if ( best > d2 )
        {
            best = d2;
            nearest = i;
        }
    }
    return nearest;
}

// sortCachedPoints @0x5B67A0 (2.79 with the deepest point kept): the
// deepest cached point (strict `<` against the new point's distance, then
// each cached one) is never replaced; for every other index the area
// spanned by the new point and the remaining three, |cross|^2, and the
// largest of their fabs() wins - from -1e18 (0xDD5E0B6B, .rdata 0x6F7908),
// strict `>`. If
// every area is NaN the result stays -1 (see manifold_add_point).
inline int manifold_sort_cached_points( const solver_manifold_t& m, const solver_contact_point_t& p )
{
    float deepest = p.distance;
    int keep = -1;
    for ( int i = 0; i < 4; ++i )
    {
        if ( deepest > m.points[ i ].distance )
        {
            keep = i;
            deepest = m.points[ i ].distance;
        }
    }
    auto area = [ & ]( int a, int b0, int b1 )
    {
        // (pt - cache[a]) x (cache[b1] - cache[b0]), length squared.
        const float* pa = m.points[ a ].local_point_a;
        const float* q0 = m.points[ b0 ].local_point_a;
        const float* q1 = m.points[ b1 ].local_point_a;
        const float ax = p.local_point_a[ 0 ] - pa[ 0 ];
        const float ay = p.local_point_a[ 1 ] - pa[ 1 ];
        const float az = p.local_point_a[ 2 ] - pa[ 2 ];
        const float bx = q1[ 0 ] - q0[ 0 ];
        const float by = q1[ 1 ] - q0[ 1 ];
        const float bz = q1[ 2 ] - q0[ 2 ];
        const float cy = bx * az - bz * ax;
        const float cx = bz * ay - by * az;
        const float cz = by * ax - bx * ay;
        return ( cy * cy + cx * cx ) + cz * cz;
    };
    float res[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if ( keep != 0 )
        res[ 0 ] = area( 1, 2, 3 );
    if ( keep != 1 )
        res[ 1 ] = area( 0, 2, 3 );
    if ( keep != 2 )
        res[ 2 ] = area( 0, 1, 3 );
    if ( keep != 3 )
        res[ 3 ] = area( 0, 1, 2 );
    float largest = -9.99999984e17f;
    int index = -1;
    for ( int i = 0; i < 4; ++i )
    {
        const float a = std::fabs( res[ i ] );
        if ( a > largest )
        {
            index = i;
            largest = a;
        }
    }
    return index;
}

// addManifoldPoint @0x5B6B90: a full manifold (4) replaces the slot
// sortCachedPoints picks, first handing its user data to the destroyed
// callback; otherwise the point is appended. Retail reads the picked slot's
// user data BEFORE clamping a -1 pick to 0 - an OUT-OF-BOUNDS read
// (points[-1]) that only an all-NaN area set reaches; it is an UNKNOWN and
// is not reproduced: the callback is skipped for -1.
inline int manifold_add_point( solver_manifold_t& m, const solver_contact_point_t& p, const contact_callbacks_t& cb )
{
    int index = m.num_contacts;
    if ( index == 4 )
    {
        index = manifold_sort_cached_points( m, p );
        if ( index >= 0 )
        {
            unsigned& data = m.points[ index ].user_persistent_data;
            if ( data && cb.destroyed )
            {
                cb.destroyed( cb.user, data );
                data = 0;
            }
        }
    }
    else
        m.num_contacts = index + 1;
    if ( index < 0 )
        index = 0;
    std::memcpy( &m.points[ index ], &p, sizeof( solver_contact_point_t ) );
    return index;
}

// removeContactPoint @0x5B6ED0: the destroyed callback on the point's user
// data; the last point moves into the gap and the LAST slot's persistent
// fields are zeroed; the count drops.
inline void manifold_remove_point( solver_manifold_t& m, int index, const contact_callbacks_t& cb )
{
    solver_contact_point_t& p = m.points[ index ];
    if ( p.user_persistent_data && cb.destroyed )
    {
        cb.destroyed( cb.user, p.user_persistent_data );
        p.user_persistent_data = 0;
    }
    const int last = m.num_contacts - 1;
    if ( index != last )
    {
        std::memcpy( &p, &m.points[ last ], sizeof( solver_contact_point_t ) );
        solver_contact_point_t& l = m.points[ last ];
        l.user_persistent_data = 0;
        l.constraint_row[ 0 ][ 7 ] = l.constraint_row[ 1 ][ 7 ] = l.constraint_row[ 2 ][ 7 ] = 0.0f;
        l.applied_impulse = 0.0f;
        l.lateral_friction_initialized = 0;
        l.lateral_impulse_1 = 0.0f;
        l.lateral_impulse_2 = 0.0f;
        l.life_time = 0;
    }
    m.num_contacts = last;
}

// refreshContactPoints @0x5B6C10 (trA / trB: rows of four then the
// origin). Pass 1, last to first: world positions from the local points,
// distance = (posA - posB).normal, lifetime++. Pass 2, last to first: drop
// the point unless threshold >= distance (jnb: a NaN distance is DROPPED);
// else drop it if the lateral drift |posB - (posA - n d)|^2 > threshold^2
// (jbe: a NaN drift is KEPT); else the processed callback.
inline void manifold_refresh( solver_manifold_t& m, const float* basis_a, const float* origin_a, const float* basis_b,
    const float* origin_b, const contact_callbacks_t& cb )
{
    for ( int i = m.num_contacts - 1; i >= 0; --i )
    {
        solver_contact_point_t& p = m.points[ i ];
        contact_xform( basis_a, origin_a, p.local_point_a, p.position_world_on_a );
        contact_xform( basis_b, origin_b, p.local_point_b, p.position_world_on_b );
        const float* a = p.position_world_on_a;
        const float* b = p.position_world_on_b;
        const float* n = p.normal_world_on_b;
        p.distance = ( n[ 1 ] * ( a[ 1 ] - b[ 1 ] ) + n[ 0 ] * ( a[ 0 ] - b[ 0 ] ) ) + n[ 2 ] * ( a[ 2 ] - b[ 2 ] );
        ++p.life_time;
    }
    for ( int i = m.num_contacts - 1; i >= 0; --i )
    {
        solver_contact_point_t& p = m.points[ i ];
        const float threshold = m.contact_breaking_threshold;
        const float d = p.distance;
        if ( !( threshold >= d ) )
        {
            manifold_remove_point( m, i, cb );
            continue;
        }
        const float* a = p.position_world_on_a;
        const float* b = p.position_world_on_b;
        const float* n = p.normal_world_on_b;
        const float pz = a[ 2 ] - n[ 2 ] * d;
        const float dx = b[ 0 ] - ( a[ 0 ] - n[ 0 ] * d );
        const float dy = b[ 1 ] - ( a[ 1 ] - n[ 1 ] * d );
        const float dz = b[ 2 ] - pz;
        const float drift = ( dy * dy + dx * dx ) + dz * dz;
        if ( drift > threshold * threshold )
        {
            manifold_remove_point( m, i, cb );
            continue;
        }
        if ( cb.processed )
            cb.processed( cb.user, p, m.body0, m.body1 );
    }
}

// btManifoldResult::addContactPoint @0x5BAA00 (vtable 0x74E700 slot 3).
// Nothing for depth > the breaking threshold (ja: a NaN depth is ADDED).
// pointA = pointInWorld + normal * depth; the local points through the
// object transforms (swapped when the manifold's body0 is the result's
// object1); a new point; the cache: a hit keeps lifetime, user data and the
// three row impulses (2.79 MAINTAIN_PERSISTENCY), written back into the
// applied / lateral impulses; a miss adds. The combined friction f0 * f1
// clamped to [-10, 10] (-10 > f first, then f > 10; a NaN passes), the
// combined restitution r0 * r1, the part / index ids (swapped with the
// objects); then the added callback when either object has flag 8, in the
// manifold's object order.
inline void manifold_result_add_contact_point( manifold_result_t& r, const float* normal, const float* point_in_world,
    float depth, const contact_callbacks_t& cb )
{
    solver_manifold_t& m = *r.manifold;
    if ( depth > m.contact_breaking_threshold )
        return;
    float point_a[ 4 ] = {
        point_in_world[ 0 ] + normal[ 0 ] * depth,
        point_in_world[ 1 ] + normal[ 1 ] * depth,
        point_in_world[ 2 ] + normal[ 2 ] * depth,
        0.0f };
    const bool swapped = m.body0 != r.object0;
    float local_a[ 4 ];
    float local_b[ 4 ];
    if ( swapped )
    {
        contact_inv_xform( r.basis1, r.origin1, point_a, local_a );
        contact_inv_xform( r.basis0, r.origin0, point_in_world, local_b );
    }
    else
    {
        contact_inv_xform( r.basis0, r.origin0, point_a, local_a );
        contact_inv_xform( r.basis1, r.origin1, point_in_world, local_b );
    }
    solver_contact_point_t p;
    std::memset( &p, 0, sizeof( p ) );      // UNKNOWN: retail's stack point is not pre-cleared; every field it uses is written below
    contact_point_init( p, local_a, local_b, normal, depth );
    std::memcpy( p.position_world_on_a, point_a, 16 );
    std::memcpy( p.position_world_on_b, point_in_world, 16 );
    const int cached = manifold_cache_entry( m, p );
    float friction = r.object0->friction * r.object1->friction;
    if ( -10.0f > friction )
        friction = -10.0f;
    else if ( friction > 10.0f )
        friction = 10.0f;
    p.combined_friction = friction;
    p.combined_restitution = r.object0->restitution * r.object1->restitution;
    if ( swapped )
    {
        p.part_id_0 = r.part_id1;
        p.part_id_1 = r.part_id0;
        p.index_0 = r.index1;
        p.index_1 = r.index0;
    }
    else
    {
        p.part_id_0 = r.part_id0;
        p.part_id_1 = r.part_id1;
        p.index_0 = r.index0;
        p.index_1 = r.index1;
    }
    int index;
    if ( cached < 0 )
        index = manifold_add_point( m, p, cb );
    else
    {
        solver_contact_point_t& slot = m.points[ cached ];
        const unsigned data = slot.user_persistent_data;
        const float row0 = slot.constraint_row[ 0 ][ 7 ];
        const float row1 = slot.constraint_row[ 1 ][ 7 ];
        const float row2 = slot.constraint_row[ 2 ][ 7 ];
        const int life = slot.life_time;
        std::memcpy( &slot, &p, sizeof( p ) );
        slot.user_persistent_data = data;
        slot.applied_impulse = row0;
        slot.lateral_impulse_1 = row1;
        slot.lateral_impulse_2 = row2;
        slot.constraint_row[ 0 ][ 7 ] = row0;
        slot.constraint_row[ 1 ][ 7 ] = row1;
        slot.constraint_row[ 2 ][ 7 ] = row2;
        slot.life_time = life;
        index = cached;
    }
    if ( cb.added && ( ( r.object0->collision_flags & 8 ) != 0 || ( r.object1->collision_flags & 8 ) != 0 ) )
    {
        solver_rigid_t* first = swapped ? r.object1 : r.object0;
        solver_rigid_t* second = swapped ? r.object0 : r.object1;
        solver_contact_point_t& cp = m.points[ index ];
        cb.added( cb.user, cp, first, cp.part_id_0, cp.index_0, second, cp.part_id_1, cp.index_1 );
    }
}
