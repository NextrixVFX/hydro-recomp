#pragma once

#include <workspace/core/boat/ai.hxx>
#include <workspace/core/collision/raytest.hxx>

#include <cstdint>
#include <cstring>
#include <vector>

// The ray test the camera's collision 0x40B090 and the AI's avoidance 0x40C750
// hand their hit lists to (camera_env_t::ray / ai_avoid_hooks_t::ray), on the
// race's collision world: VuDynamicsRayTest 0x50CFB0 builds the ray (from /
// to, w lanes 0) and a RayTestWrapper (vtable 0x74C5B4: closestHitFraction
// 1.0, group 1, mask -1, flags 0) and calls the world's rayTest 0x54F590
// (engine collision/raytest.hxx). The wrapper NEVER writes its +4, so the
// test runs with closest = 1.0 throughout; its needsCollision 0x50CDF0 /
// addSingleResult 0x50CE20 (with the Vu result's slots 0 / 1) are what
// ai_ray / the camera's ray apply to each hit - the hits here are every
// addSingleResult retail's traversal makes, in its order. That is exact on
// every path recreated (convex, compound): there the value addSingleResult
// returns is discarded (the convex path's `fstp st0`, LocalInfoAdder2) and
// needsCollision only skips the object it is asked about. The BVH mesh's
// triangle path (boat batch 16) feeds the returned value back
// (m_hitFraction): the answer below is retail's. A path still not recreated
// (the non-quantized BVH walk, a concave type other than 21) returns -1 and
// sets `unknown`.
struct boat_ray_world_t
{
    collision_world_t* world = nullptr;
    void* user = nullptr;
    // The hit object's VuRigidBody image (what ai_ray / the camera read:
    // +0x10 the basis rows, +0xF4, +0x140 the velocity, +0x2CA, +0x2D0).
    const std::uint8_t* ( *image )( void* user, const collision_object_t& o ) = nullptr;
    std::vector< ai_ray_hit_t > hits;
    bool unknown = false;   // a ray reached a path not recreated (UNKNOWN): its hits are retail's only up to there
    int rays = 0;
};

inline int boat_ray_hits( void* user, const float* from, const float* to, const ai_ray_hit_t** hits )
{
    boat_ray_world_t& r = *static_cast< boat_ray_world_t* >( user );
    r.hits.clear( );
    ++r.rays;
    *hits = nullptr;
    if ( !r.world || !r.image )
    {
        r.unknown = true;
        return 0;
    }
    ray_callback_t cb;
    cb.user = &r;
    cb.needs_collision = []( void*, ray_callback_t&, const broadphase_proxy_t& ) { return true; };
    cb.add_single_result = []( void* u, ray_callback_t&, ray_local_result_t& res, bool normal_in_world ) {
        boat_ray_world_t& w = *static_cast< boat_ray_world_t* >( u );
        ai_ray_hit_t h;
        h.body = w.image( w.user, *res.object );
        h.fraction = res.fraction;
        std::memcpy( h.normal, res.normal, 16 );
        h.world = normal_in_world;
        w.hits.push_back( h );
        // 0x50CE20's answer: the Vu result's slot 1 (0x404B20 in both the
        // camera's 0x737B14 and the AI's 0x736F7C vtables) always returns 1
        // (`mov al, 1`), so the wrapper sets hit and returns `fld [r+0x20]` -
        // the hit's own fraction. The BVH path feeds it back as the triangle
        // callback's hitFraction (per object: an object the consumer's
        // needsCollision would skip only feeds its own, skipped, hits).
        return res.fraction;
    };
    const float f[ 4 ] = { from[ 0 ], from[ 1 ], from[ 2 ], 0.0f };
    const float t[ 4 ] = { to[ 0 ], to[ 1 ], to[ 2 ], 0.0f };
    if ( collision_world_ray_test( *r.world, f, t, cb ) < 0 )
        r.unknown = true;
    *hits = r.hits.data( );
    return static_cast< int >( r.hits.size( ) );
}
