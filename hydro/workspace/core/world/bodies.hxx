#pragma once

#include <workspace/core/boat/meshes.hxx>
#include <workspace/core/boat/parts.hxx>
#include <workspace/core/collision/concave.hxx>
#include <workspace/core/collision/support.hxx>
#include <workspace/core/dynamics/body.hxx>
#include <workspace/core/collision/world.hxx>
#include <workspace/core/entity/rigid.hxx>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

// The level's static collision bodies (world batch 16): VuPropEntity's
// gameInitialize (vt slot 6 0x546910) and VuRigidBodyComponent's
// createRigidBody 0x544100, recreated from the disassembly and checked
// against retail on every VuPropEntity of the Hydroelectric level (oracle
// case prop_rigid_bodies). Shape paths: 6 convex hull (btConvexTriangleMeshShape
// 0x526380) and 7 triangle mesh (btBvhTriangleMeshShape 0x5AEB60 + Vu's
// setOptimizedBvh 0x5AF400 on the asset's VuOptimizedBvh) - the only two the
// level's props use; 1 - 5 (sphere 0x43D700, box 0x547D80, cylinders
// 0x5AE160), the centre-of-mass compound (0x5269C0 / 0x526BF0), a non-unit
// scale on a BVH mesh (0x5AF480) and a mass's inertia (shape vt+0x1C) are NOT
// recreated: such a body is UNKNOWN (refused, never stood in for).

// One built body: the component's shape (+0xF8), the VuRigidBody (+0x100), the
// VuStridingMesh (+0xA0) its shape reads, and the body's collision object in
// the race's world.
struct level_body_t
{
    std::string name;
    std::int32_t type = 0;
    const collision_mesh_asset_t* asset = nullptr;   // +0xC0
    striding_mesh_t mesh{ };                         // +0xA0 (scaling +0xB0 = (1, 1, 1, 0) from the ctor)
    quantized_bvh_t bvh{ };                          // the asset's +0xA8, as the walk reads it
    bvh_mesh_shape_t concave{ };
    convex_shape_t convex{ };
    vu_rigid_body_t body{ };
    collision_object_t collision{ };
    std::uint32_t flags_d8 = 0;                      // component +0xD8 after the build
    bool in_world = false;                           // component +0x104
};

// VuMatrix -> btTransform as 0x410C30 does it (the basis transposed into rows
// with w 0, then the origin, w 0) - the form boat construct.hxx's motion state
// inlines.
inline void bodies_bt_from_vu( const float* m, float* out )
{
    for ( int row = 0; row < 3; ++row )
    {
        for ( int k = 0; k < 3; ++k )
            out[ 4 * row + k ] = m[ 4 * k + row ];
        out[ 4 * row + 3 ] = 0.0f;
    }
    out[ 12 ] = m[ 12 ];
    out[ 13 ] = m[ 13 ];
    out[ 14 ] = m[ 14 ];
    out[ 15 ] = 0.0f;
}

// The quantized BVH the walk reads, over the asset's loaded VuOptimizedBvh.
inline void bodies_bvh_of( const collision_mesh_asset_t& a, quantized_bvh_t& out )
{
    out = quantized_bvh_t{ };
    std::memcpy( out.aabb_min, a.bvh.header + 0, 16 );
    std::memcpy( out.aabb_max, a.bvh.header + 4, 16 );
    std::memcpy( out.quantization, a.bvh.header + 8, 16 );
    out.cur_node_index = static_cast< int >( a.bvh.header[ 13 ] );
    out.use_quantization = a.bvh.use_quantization;
    out.nodes = reinterpret_cast< const bvh_node_t* >( a.bvh.quantized.bytes.data( ) );
    out.traversal_mode = a.bvh.traversal;
}

struct bodies_hooks_t
{
    void* user = nullptr;
    // g_vudynamicsimpl vt+0x54 (slot 21 0x4F88A0): the surface type's id.
    std::uint8_t ( *surface )( void* user, const std::string& name ) = nullptr;
    // g_vudynamicsimpl vt+0x1C: the world's gravity (12 bytes).
    void ( *gravity )( void* user, float* out ) = nullptr;
};

// createRigidBody 0x544100 on a component whose group / mask are set. False:
// a path not recreated (b.name says nothing; `why` says which) - nothing built.
// True with no body (type 0 / no asset): retail builds none either.
// `unique_id` is the global 0x759030 the VuRigidBody ctor takes and bumps.
inline bool bodies_create( level_body_t& b, rigid_component_t& c, const collision_mesh_asset_t* asset, const float* world,
                           const float* scale, std::uint32_t& unique_id, const bodies_hooks_t& hooks, std::string& why, bool& built )
{
    built = false;
    b.type = c.collision_type;
    if ( c.collision_type == 6 || c.collision_type == 7 )
    {
        b.asset = asset;   // +0xC0 = the asset property's +0x4C
        if ( !asset )
            return true;
    }
    c.flags_d8 &= ~4u;
    if ( c.collision_type == 0 )
        return true;
    b.mesh = striding_mesh_t{ };
    b.mesh.scaling[ 0 ] = b.mesh.scaling[ 1 ] = b.mesh.scaling[ 2 ] = 1.0f;
    if ( asset )
        collision_mesh_striding( *asset, b.mesh );
    bool convex = false;
    if ( c.collision_type == 6 )
    {
        shape_init_triangle_mesh_hull( b.convex, &b.mesh );
        convex = true;
    }
    else if ( c.collision_type == 7 )
    {
        if ( bvh_mesh_shape_init( b.concave, &b.mesh ) < 0 )
        {
            why = "the BVH mesh shape's recalcLocalAabb refused";
            return false;
        }
        bodies_bvh_of( *asset, b.bvh );
        const float unit[ 4 ] = { 1.0f, 1.0f, 1.0f, 0.0f };   // 0x6F7B00
        if ( bvh_mesh_set_optimized_bvh( b.concave, b.mesh, &b.bvh, unit ) < 0 )
        {
            why = "setOptimizedBvh's recalcLocalAabb refused";
            return false;
        }
        if ( asset->flag )
            c.flags_d8 |= 4;
    }
    else
    {
        why = "createRigidBody 0x544100: collision type " + std::to_string( c.collision_type ) + " (sphere / box / cylinder) is not recreated";
        return false;
    }
    // The offset against the centre of mass: (dx^2 + dy^2) + dz^2 > 0 (0x6F7570) -> the compound.
    const float dx = c.offset[ 0 ] - c.centre[ 0 ], dy = c.offset[ 1 ] - c.centre[ 1 ], dz = c.offset[ 2 ] - c.centre[ 2 ];
    if ( ( dx * dx + dy * dy ) + dz * dz > 0.0f )
    {
        why = "createRigidBody 0x544100: the offset compound (0x5269C0 / 0x526BF0) is not recreated";
        return false;
    }
    // The transform's scale: each lane `ucomiss` against 1.0 - not equal or unordered -> setLocalScaling (vt+0x14).
    if ( !( scale[ 0 ] == 1.0f && scale[ 1 ] == 1.0f && scale[ 2 ] == 1.0f ) )
    {
        if ( !convex )
        {
            why = "createRigidBody 0x544100: a scaled BVH mesh (setLocalScaling 0x5AF480) is not recreated";
            return false;
        }
        const float s[ 4 ] = { scale[ 0 ], scale[ 1 ], scale[ 2 ], 0.0f };
        shape_set_triangle_mesh_scaling( b.convex, b.mesh, s );
    }
    const float inertia[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if ( c.mass > 0.0f )
    {
        why = "createRigidBody 0x544100: a mass's calculateLocalInertia (shape vt+0x1C) is not recreated";
        return false;
    }
    // 0x4131E0(mass, motion state +0xF0, shape, inertia) then the start transform = 0x410C30(the owner's world).
    boat_4131e0_t cinfo;
    std::uint32_t mass;
    std::memcpy( &mass, &c.mass, 4 );
    // The motion state (+0xF0: 0 for a VuPropEntity - only VuDynamicPropEntity's 0x545A80 sets it) and the
    // shape are pointer words the setup does not read here (setCollisionShape: held by level_body_t).
    boat_4131e0( cinfo, mass, 0, 0, reinterpret_cast< const std::uint8_t* >( inertia ) );
    float bt[ 16 ];
    bodies_bt_from_vu( world, bt );
    std::memcpy( cinfo.image + 0x10, bt, 64 );
    if ( !vu_rigid_body_construct( b.body, cinfo.image, static_cast< short >( c.group ), static_cast< short >( c.mask ), unique_id, nullptr, nullptr ) )
    {
        why = "the VuRigidBody ctor 0x527B80 refused";
        return false;
    }
    body_word( b.body, 0xD8 ) = c.flags_d0;
    body_word( b.body, 0x2D0 ) = c.flags_d8;
    body_word( b.body, 0x2C4 ) = c.word_f4;
    if ( !hooks.surface || !hooks.gravity )
    {
        why = "createRigidBody 0x544100: g_vudynamicsimpl's surface / gravity not wired";
        return false;
    }
    body_byte( b.body, 0x2C8 ) = hooks.surface( hooks.user, c.surface );
    float g[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
    hooks.gravity( hooks.user, g );
    rigid_set_gravity( b.body.body, g );
    const float ld = body_clamp01( c.linear_damping ), ad = body_clamp01( c.angular_damping );
    std::memcpy( &body_word( b.body, 0x1D0 ), &ld, 4 );
    std::memcpy( &body_word( b.body, 0x1D4 ), &ad, 4 );
    std::memcpy( &body_word( b.body, 0x1EC ), &c.linear_sleep, 4 );
    std::memcpy( &body_word( b.body, 0x1F0 ), &c.angular_sleep, 4 );
    if ( !c.initially_active )
        body_word( b.body, 0xE8 ) = 0x41200000u;   // 10.0
    b.flags_d8 = c.flags_d8;
    b.collision = collision_object_t{ };
    b.collision.body = concave_body_t{ b.body.body.basis, convex ? &b.convex : nullptr, convex ? nullptr : &b.concave, &b.body.body, nullptr,
                                       rigid_bits( body_word( b.body, 0xC4 ) ) };
    built = true;
    return true;
}

// VuPropEntity gameInitialize 0x546910: the component's group 1 / mask 0xFFFE
// (+0xD4 / +0xD6; the body's +0x2CA / +0x2CC and its proxy's +4 / +6 when one
// exists - none before the build), createRigidBody, then - Initially Visible
// (+0x78) - the draw component's registration with g 0x7590B8 vt+0x28 (the
// renderer: `draw`, a hook) and, with a body not yet in the world (+0x104),
// g_vudynamicsimpl vt+0x3C addRigidBody (`add`) and +0x104 = 1.
template < typename draw_fn, typename add_fn >
inline bool bodies_prop_game_initialize( level_body_t& b, rigid_component_t& c, std::uint8_t visible, const collision_mesh_asset_t* asset,
                                         const float* world, const float* scale, std::uint32_t& unique_id, const bodies_hooks_t& hooks,
                                         draw_fn draw, add_fn add, std::string& why )
{
    c.group = 1;
    c.mask = 0xFFFE;
    bool built = false;
    if ( !bodies_create( b, c, asset, world, scale, unique_id, hooks, why, built ) )
        return false;
    if ( visible )
    {
        draw( );
        if ( !b.in_world && built )
        {
            if ( !add( b ) )
            {
                why = "addRigidBody (g_vudynamicsimpl vt+0x3C) refused";
                return false;
            }
            b.in_world = true;
        }
    }
    return true;
}
