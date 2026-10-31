#pragma once

#include <memory>
#include <vector>

#include <workspace/core/collision/compound.hxx>

// The collision dispatch layer of performDiscreteCollisionDetection 0x54C710
// (Bullet 2.79), recreated from retail's disassembly (memory/audit.md "Batch
// 8h"): dispatchAllCollisionPairs 0x56D7D0 -> the pair cache's
// processAllOverlappingPairs 0x5B1670 -> btCollisionPairCallback
// processOverlap 0x56D780 -> the nearCallback 0x56D840 (needsCollision
// 0x56D710, findAlgorithm 0x56D660 through the matrix 0x56D000, the
// btManifoldResult ctor 0x5BA930, the algorithm's processCollision).

// A collision object as the dispatch layer reads it: the body processCollision
// sees, +228 the activation state, +216 the collision flags (needsResponse;
// & 1 static for updateSingleAabb), +264 the checkCollideWith flag (set ->
// the object's vt+0, a Vu override), +80 the interpolation world transform,
// +244 the internal type (2 = rigid body), +200 the broadphase proxy.
struct collision_object_t
{
    concave_body_t body;
    int activation_state = 1;
    int collision_flags = 0;
    bool check_collide = false;
    const float* interpolation_transform = nullptr;
    int internal_type = 2;
    unsigned proxy = 0;
    bool proxied = false;   // +200 non-null: addCollisionObject made a proxy
};

// The world members updateSingleAabb reads: +44 the continuous flag
// (dispatchInfo +16), +84 forceUpdateAllAabbs (updateAabbs), +76 the
// broadphase - its setAabb (vt+12: proxy, min, max, dispatcher +24) is a
// hook until the btDbvtBroadphase is recreated (-1 = unknown) - and +80 the
// debug drawer, whose reportErrorWarning (vt+32) is a hook (null = no
// drawer).
struct aabb_world_t
{
    bool use_continuous = false;
    bool force_update_all = false;
    void* user = nullptr;
    int ( *set_aabb )( void* user, unsigned proxy, const float* aabb_min, const float* aabb_max ) = nullptr;
    void ( *report )( void* user, const char* text ) = nullptr;
};

// byte 0x74E878 (.data 1): the overflow report is made once per process.
inline bool g_aabb_report_pending = true;

// updateSingleAabb @0x54C370 (thiscall world; object; retn 4), from the
// disassembly ("bad sp" decompile): the shape's getAabb (vt+4) on the world
// transform (+16); lanes 0..2 min -= / max += gContactBreakingThreshold
// 0x735C8C, lane 3 as written; with world+44 set and the internal type 2, the
// same box from the interpolation transform (+80), lanes 0..2 widened, merged
// over ALL FOUR lanes (`comiss cur, new; jbe` -> min takes new when cur > new;
// `comiss new, cur; jbe` -> max takes new when new > cur; NaN keeps cur).
// Then the static flag (+216 & 1), else `comiss 1e12 (0x5368D4A5), d; ja`
// with d = ((dy^2 + dx^2) + dz^2) -> setAabb; otherwise the activation state
// becomes 5 unless 4 / 5, and the first time (byte 0x74E878, cleared) with a
// drawer the four reportErrorWarning strings.
inline int update_single_aabb( aabb_world_t& w, collision_object_t& o )
{
    float mn[ 4 ], mx[ 4 ];
    if ( !body_get_aabb( o.body, o.body.transform, mn, mx ) )
        return -1;
    const float g = contact_breaking_global( );
    for ( int k = 0; k < 3; ++k )
    {
        mn[ k ] = mn[ k ] - g;
        mx[ k ] = mx[ k ] + g;
    }
    if ( w.use_continuous && o.internal_type == 2 )
    {
        float mn2[ 4 ], mx2[ 4 ];
        if ( !o.interpolation_transform || !body_get_aabb( o.body, o.interpolation_transform, mn2, mx2 ) )
            return -1;
        for ( int k = 0; k < 3; ++k )
        {
            mn2[ k ] = mn2[ k ] - g;
            mx2[ k ] = mx2[ k ] + g;
        }
        for ( int k = 0; k < 4; ++k )
            if ( mn[ k ] > mn2[ k ] )
                mn[ k ] = mn2[ k ];
        for ( int k = 0; k < 4; ++k )
            if ( mx2[ k ] > mx[ k ] )
                mx[ k ] = mx2[ k ];
    }
    bool set = ( o.collision_flags & 1 ) != 0;
    if ( !set )
    {
        const float dy = mx[ 1 ] - mn[ 1 ];
        const float dx = mx[ 0 ] - mn[ 0 ];
        const float dz = mx[ 2 ] - mn[ 2 ];
        const float d = ( dy * dy + dx * dx ) + dz * dz;
        set = support_bits( 0x5368D4A5u ) > d;
    }
    if ( set )
        return w.set_aabb ? w.set_aabb( w.user, o.proxy, mn, mx ) : -1;
    if ( o.activation_state != 4 && o.activation_state != 5 )
        o.activation_state = 5;
    if ( g_aabb_report_pending && w.report )
    {
        g_aabb_report_pending = false;
        w.report( w.user, "Overflow in AABB, object removed from simulation" );
        w.report( w.user, "If you can reproduce this, please email bugs@continuousphysics.com\n" );
        w.report( w.user, "Please include above information, your Platform, version of OS.\n" );
        w.report( w.user, "Thanks.\n" );
    }
    return 1;
}

// updateAabbs @0x54C5F0 (world vt+4; profiled "updateAabbs"): every object in
// array order (+8 count / +16 data) when forceUpdateAllAabbs (+84) or its
// activation state is neither 2 nor 5.
inline int update_aabbs( aabb_world_t& w, std::vector< collision_object_t* >& objects )
{
    for ( collision_object_t* o : objects )
        if ( w.force_update_all || ( o->activation_state != 2 && o->activation_state != 5 ) )
            if ( update_single_aabb( w, *o ) < 0 )
                return -1;
    return 1;
}

// The top-level algorithm a pair holds (+8): ConvexConvex / ConvexConcave
// (the swapped form in leaf.concave.swapped) or Compound (swapped in
// compound.swapped).
struct pair_algorithm_t
{
    int kind = 0;   // 1 ConvexConvex, 2 ConvexConcave, 3 Compound, 4 SphereSphere, 5 BoxBox
    child_algorithm_t leaf;
    compound_algorithm_t compound;
};

// btBroadphasePair (16 bytes): {proxy0, proxy1, algorithm, internal}; a
// proxy's +0 is its client object. The pair's algorithm slot holds a raw
// pointer into the store below (the dispatcher's allocate / free).
struct overlapping_pair_t
{
    collision_object_t* object0 = nullptr;
    collision_object_t* object1 = nullptr;
    pair_algorithm_t* algorithm = nullptr;
};

// The algorithms allocateCollisionAlgorithm 0x56D980 hands out and
// freeCollisionAlgorithm 0x56D9C0 takes back (a pool block or the heap -
// bookkeeping only).
struct pair_algorithm_store_t
{
    std::vector< std::unique_ptr< pair_algorithm_t > > owned;
};

// btDispatcherInfo: +8 the dispatch function (1 = discrete), +12 the time of
// impact the continuous branch mins into.
struct dispatch_info_t
{
    int dispatch_func = 1;
    float time_of_impact = 1.0f;
};

// needsCollision @0x56D710 (dispatcher vt+20, stdcall body0, body1; retn 8):
// false when BOTH activation states are 2 or 5; else the object's vt+0 when
// +264 is set (`test al, al; cmove` -> its bool); else true. The override is
// Vu's: -1.
inline int dispatcher_needs_collision( const collision_object_t& a, const collision_object_t& b )
{
    const int s0 = a.activation_state;
    if ( s0 == 2 || s0 == 5 )
    {
        const int s1 = b.activation_state;
        if ( s1 == 2 || s1 == 5 )
            return 0;
    }
    if ( a.check_collide )
        return -1;   // checkCollideWithOverride (vt+0): a Vu hook, not recreated
    return 1;
}

// needsResponse @0x56D6C0 (vt+24, stdcall body0, body1): false when either
// has flag 4 (no contact response); true when either is neither static nor
// kinematic (flags & 3 == 0); else false.
inline bool dispatcher_needs_response( const collision_object_t& a, const collision_object_t& b )
{
    const int f0 = a.collision_flags;
    if ( f0 & 4 )
        return false;
    const int f1 = b.collision_flags;
    if ( f1 & 4 )
        return false;
    return ( f0 & 3 ) == 0 || ( f1 & 3 ) == 0;
}

// findAlgorithm @0x56D660 with ci.manifold = 0 (the nearCallback's call) for
// the matrix 0x56D000, tested in its own order: (8, 8) SphereSphere, (8, 1) /
// (1, 8) SphereTriangle, (0, 0) BoxBox, convex vs 28 / 28 vs convex the
// plane - not recreated, -1; both < 20 ConvexConvex (CreateFunc 0x5FBCD0:
// ownManifold 0, manifold NULL); convex vs 21..29 ConvexConcave (0x56C2B0:
// getNewManifold(body0, body1) + clearManifold) and 21..29 vs convex the
// swapped one (0x56C370: isSwapped 1, the callback's convex = body1,
// getNewManifold(body1, body0)); body0 31 Compound (0x56C430), else body1 31
// the swapped Compound (0x56C4F0: +28 = 1, +40 = body1's shape revision);
// else the Empty CreateFunc - not read, -1. Concave types other than 21 have
// no shape here: -1.
inline int dispatcher_find_pair_algorithm( collision_dispatcher_t& d, const concave_body_t& body0, const concave_body_t& body1,
    std::unique_ptr< pair_algorithm_t >& out )
{
    const int t0 = body_shape_type( body0 );
    const int t1 = body_shape_type( body1 );
    if ( ( t0 == 8 && t1 == 1 ) || ( t0 == 1 && t1 == 8 ) )
        return -1;   // a top-level SphereTriangle: no triangle objects in content
    if ( ( t0 < 20 && t1 == 28 ) || ( t1 < 20 && t0 == 28 ) )
        return -1;   // the plane algorithms: no plane shape in the binary
    auto a = std::make_unique< pair_algorithm_t >( );
    if ( t0 == 8 && t1 == 8 )
    {
        a->kind = 4;   // SphereSphere: the CreateFunc 0x56D0D0
        if ( dispatcher_make_sphere_sphere( d, body0, body1, a->leaf ) < 0 )
            return -1;
    }
    else if ( t0 == 0 && t1 == 0 )
    {
        a->kind = 5;   // BoxBox: the CreateFunc 0x56C700
        if ( dispatcher_make_box_box( d, body0, body1, a->leaf ) < 0 )
            return -1;
    }
    else if ( t0 < 20 && t1 < 20 )
    {
        a->kind = 1;
        a->leaf.kind = 1;
        a->leaf.convex.simplex = d.simplex;
        a->leaf.convex.pd_solver = d.pd_solver;
        a->leaf.convex.own_manifold = false;
        a->leaf.convex.manifold = nullptr;
        a->leaf.convex.perturbation_iterations = d.perturbation_iterations;
        a->leaf.convex.minimum_points_threshold = d.minimum_points_threshold;
    }
    else if ( ( t0 < 20 && t1 >= 21 && t1 <= 29 ) || ( t1 < 20 && t0 >= 21 && t0 <= 29 ) )
    {
        const bool swapped = t0 >= 21;
        if ( ( swapped ? t0 : t1 ) != 21 )
            return -1;   // a concave shape other than the BVH mesh
        a->kind = 2;
        a->leaf.kind = 2;
        a->leaf.concave.swapped = swapped;
        a->leaf.concave.simplex = d.simplex;
        a->leaf.concave.pd_solver = d.pd_solver;
        a->leaf.concave.perturbation_iterations = d.perturbation_iterations;
        a->leaf.concave.minimum_points_threshold = d.minimum_points_threshold;
        a->leaf.concave.manifold = swapped ? dispatcher_get_new_manifold( d, body1, body0 ) : dispatcher_get_new_manifold( d, body0, body1 );
        if ( !a->leaf.concave.manifold )
            return -1;
        dispatcher_clear_manifold( d, *a->leaf.concave.manifold );
    }
    else if ( t0 == 31 || t1 == 31 )
    {
        const bool swapped = t0 != 31;
        const concave_body_t& c = swapped ? body1 : body0;
        if ( !c.compound )
            return -1;
        a->kind = 3;
        compound_create( a->compound, *c.compound, swapped, nullptr );
    }
    else
        return -1;   // the Empty CreateFunc: not read
    out = std::move( a );
    return 1;
}

inline int pair_process_collision( collision_dispatcher_t& d, pair_algorithm_t& a, const concave_body_t& body0, const concave_body_t& body1,
    manifold_result_t& result )
{
    if ( a.kind == 3 )
        return compound_process_collision( d, a.compound, body0, body1, result, nullptr, nullptr );
    return child_process_collision( d, a.leaf, body0, body1, result, nullptr, nullptr );
}

// The nearCallback @0x56D840 (cdecl pair, dispatcher, dispatchInfo): the
// pair's two client objects; needsCollision (vt+20); a NULL pair algorithm
// is made by findAlgorithm (vt+4, manifold 0) and stored - a NULL result
// skips; the stack btManifoldResult (ctor 0x5BA930: vtable, +4 = 0, +16..+79
// body0's world transform, +80..+143 body1's, +144 / +148 the objects; the
// caller zeroes +4, +144 and +148 before; +152..+167, the ids, are NOT
// written - UNKNOWN stack until setShapeIdentifiers, zero here); then info+8
// == 1 -> processCollision(body0, body1, info, &result), else the time of
// impact (vt+8, x87) and `comiss toi(info+12), t; jbe` -> min - the
// continuous branch is not recreated: -1.
inline int dispatch_near( collision_dispatcher_t& d, pair_algorithm_store_t& store, collision_object_t& o0, collision_object_t& o1,
    pair_algorithm_t*& slot, dispatch_info_t& info )
{
    const int need = dispatcher_needs_collision( o0, o1 );
    if ( need < 0 )
        return -1;
    if ( need == 0 )
        return 1;
    if ( !slot )
    {
        std::unique_ptr< pair_algorithm_t > made;
        if ( dispatcher_find_pair_algorithm( d, o0.body, o1.body, made ) < 0 )
            return -1;
        slot = made.get( );
        store.owned.push_back( std::move( made ) );
    }
    manifold_result_t r{};
    std::memcpy( r.basis0, o0.body.transform, 48 );
    std::memcpy( r.origin0, o0.body.transform + 12, 16 );
    std::memcpy( r.basis1, o1.body.transform, 48 );
    std::memcpy( r.origin1, o1.body.transform + 12, 16 );
    r.object0 = o0.body.object;
    r.object1 = o1.body.object;
    if ( info.dispatch_func != 1 )
        return -1;   // calculateTimeOfImpact (vt+8): not recreated
    return pair_process_collision( d, *slot, o0.body, o1.body, r );
}

inline int dispatch_near_callback( collision_dispatcher_t& d, pair_algorithm_store_t& store, overlapping_pair_t& pair, dispatch_info_t& info )
{
    return dispatch_near( d, store, *pair.object0, *pair.object1, pair.algorithm, info );
}

// dispatchAllCollisionPairs @0x56D7D0 (thiscall dispatcher; pairCache,
// dispatchInfo, dispatcher; retn 0Ch): a stack btCollisionPairCallback
// {vt 0x74F76C, +4 info, +8 dispatcher} handed to the pair cache's
// processAllOverlappingPairs (vt+44) @0x5B1670: for each pair in array order
// processOverlap 0x56D780 calls the dispatcher's +0xD0 nearCallback(pair,
// dispatcher, info) and returns false, so the cache's removal branch
// (removeOverlappingPair vt+8, --gOverlappingPairs, the index not advanced)
// is never taken from here.
inline int dispatch_all_collision_pairs( collision_dispatcher_t& d, pair_algorithm_store_t& store, std::vector< overlapping_pair_t >& pairs,
    dispatch_info_t& info )
{
    for ( std::size_t i = 0; i < pairs.size( ); ++i )
        if ( dispatch_near_callback( d, store, pairs[ i ], info ) < 0 )
            return -1;
    return 1;
}

// A pair's algorithm torn down as cleanOverlappingPair 0x5B0DA0 does it: the
// dtor (vt+0(0)) - Compound 0x5F7A30 -> 0x5F7C10: removeChildAlgorithms
// 0x5F7BD0 (each child in index order: its dtor + freeCollisionAlgorithm)
// and the child array freed; ConvexConcave 0x5F65A0: clearManifold (vt+16)
// then releaseManifold (vt+12) of its callback's manifold; ConvexConvex
// 0x5FBD50: releaseManifold only when it owns one - then
// freeCollisionAlgorithm 0x56D9C0 (bookkeeping).
inline void dispatch_destroy_pair_algorithm( collision_dispatcher_t& d, pair_algorithm_store_t& store, pair_algorithm_t* a )
{
    if ( a->kind == 3 )
    {
        for ( auto& child : a->compound.children )
            dispatcher_destroy_algorithm( d, child );
    }
    else if ( a->kind == 1 )
    {
        if ( a->leaf.convex.own_manifold && a->leaf.convex.manifold )
            dispatcher_release_manifold( d, a->leaf.convex.manifold );
    }
    else if ( a->kind == 5 )
    {
        if ( a->leaf.box.own_manifold && a->leaf.box.manifold )   // the dtor 0x5FB670
            dispatcher_release_manifold( d, a->leaf.box.manifold );
    }
    else if ( a->kind == 4 )
    {
        if ( a->leaf.sphere.own_manifold && a->leaf.sphere.manifold )   // the dtor 0x5FEC00
            dispatcher_release_manifold( d, a->leaf.sphere.manifold );
    }
    else if ( a->leaf.concave.manifold )
    {
        dispatcher_clear_manifold( d, *a->leaf.concave.manifold );
        dispatcher_release_manifold( d, a->leaf.concave.manifold );
    }
    for ( auto it = store.owned.begin( ); it != store.owned.end( ); ++it )
        if ( it->get( ) == a )
        {
            store.owned.erase( it );
            break;
        }
}
