#pragma once

#include <cstdint>
#include <cstring>

// The one-instruction leaf virtuals of the race's physics (Bullet) classes -
// and of the classes whose identical bodies the linker folded onto the same
// address (the comment names the first vtable slots that point there; the
// inventory row lists them all). Read from retail's disassembly, each is one
// of: a constant in eax (getShapeType / calculateSerializeBufferSize / a
// count), a name (getName: the text's address and its text), this + k, the
// word or byte at this + k, a byte constant in al, nothing, ST0 = 1.0, or the
// word at this + k = 0, the first argument stored at this + k, or the word at
// this + k plus a constant; `pop` is what its ret pops (stdcall-cleaned
// arguments). al forms leave eax's upper bytes as they were (UNKNOWN to the
// caller), so do nop / one / store_zero for all of eax.
enum class bt_leaf_kind_t
{
    constant,
    text,
    this_plus,
    load,
    load_byte,
    byte,
    nop,
    one,
    store_zero,
    store_arg,   // the first stack argument's word at this + k (mov or movss)
    load_add,    // the word at this + k, plus value
};

struct bt_leaf_t
{
    std::uint32_t address;
    bt_leaf_kind_t kind;
    std::uint32_t k;       // the field offset
    std::uint32_t value;   // the constant / the text's address / the byte
    std::uint32_t pop;     // ret N
    const char* text;
};

inline constexpr bt_leaf_t k_bt_leaves[] = {
    { 0x40EA20u, bt_leaf_kind_t::constant, 0x0u, 0x0u, 0, nullptr },   // VuDriverEntity+23; VuBasicProperty<...>+1; btSphereShape+19 ...
    { 0x5F6570u, bt_leaf_kind_t::nop, 0x0u, 0x0u, 16, nullptr },   // btEmptyAlgorithm+1
    { 0x4049F0u, bt_leaf_kind_t::load, 0x4u, 0x0u, 0, nullptr },   // VuMethod1<...>+1; VuMethod1<...>+1; VuMethod1<...>+1 ...
    { 0x43D610u, bt_leaf_kind_t::this_plus, 0x10u, 0x0u, 0, nullptr },   // btSphereShape+6; btBoxShape+6; btPolyhedralConvexAabbCachingShape+6 ...
    { 0x4F7B60u, bt_leaf_kind_t::load_byte, 0x58u, 0x0u, 0, nullptr },   // VuDynamicsImpl+8
    { 0x4F7C90u, bt_leaf_kind_t::load, 0x38u, 0x0u, 0, nullptr },   // VuDynamicsImpl+28
    { 0x4F88F0u, bt_leaf_kind_t::load_byte, 0xDu, 0x0u, 0, nullptr },   // VuDynamicsImpl+24
    { 0x4F8940u, bt_leaf_kind_t::load, 0x34u, 0x0u, 0, nullptr },   // VuDynamicsImpl+27
    { 0x527B30u, bt_leaf_kind_t::this_plus, 0x50u, 0x0u, 0, nullptr },   // btCompoundShape+6
    { 0x5523D0u, bt_leaf_kind_t::load, 0x50u, 0x0u, 0, nullptr },   // btCollisionWorld+3; VuDynamicsWorldImpl+3; btDiscreteDynamicsWorld+3 ...
    { 0x490F00u, bt_leaf_kind_t::constant, 0x0u, 0x0u, 4, nullptr },   // VuBarracudaMetroGfxComposer+5; V?$allocat::Platform::$0A::Microsoft::Xbox::Leaderboards::P$AAVLeaderboardAttribute::V?$Agile::V?$vector::.?AV?$_Ref_count_obj+3; _Ref_count_obj<...>+3 ...
    { 0x4A9800u, bt_leaf_kind_t::constant, 0x0u, 0x0u, 12, nullptr },   // VuXMLHttpRequest2Callback+3; btCollisionAlgorithmCreateFunc+1; VuMetroHttpRequest2Callback+3
    { 0x515D60u, bt_leaf_kind_t::byte, 0x0u, 0x1u, 4, nullptr },   // VuAnimationAsset+2; VuAnimationAsset+4; VuAsset+2 ...
    { 0x5F6580u, bt_leaf_kind_t::one, 0x0u, 0x0u, 16, nullptr },   // btEmptyAlgorithm+2; btConvexPlaneCollisionAlgorithm+2; btBoxBoxCollisionAlgorithm+2 ...
    { 0x406AF0u, bt_leaf_kind_t::constant, 0x0u, 0x1u, 0, nullptr },   // VuBasicProperty<...>+1; VuAngleProperty+1; VuPercentageProperty+1 ...
    { 0x406B30u, bt_leaf_kind_t::constant, 0x0u, 0x2u, 0, nullptr },   // VuBasicProperty<...>+1; VuDynamicsWorldImpl+27; VuD3d11Gfx+53 ...
    { 0x409210u, bt_leaf_kind_t::constant, 0x0u, 0x8u, 0, nullptr },   // VuBasicProperty<...>+1; VuRotation3dProperty+1; btBoxShape+22
    { 0x413330u, bt_leaf_kind_t::constant, 0x0u, 0x1Cu, 0, nullptr },   // VuStridingMesh+12; btStridingMeshInterface+12; VuSimpleStridingMesh+12
    { 0x432F10u, bt_leaf_kind_t::constant, 0x0u, 0x6u, 0, nullptr },   // VuBasicProperty<...>+1; btBoxShape+19; btBoxShape+26
    { 0x433ED0u, bt_leaf_kind_t::constant, 0x0u, 0xCu, 0, nullptr },   // VuAudioEventNameProperty+1; btCollisionShape+11; btBoxShape+23 ...
    { 0x43D620u, bt_leaf_kind_t::text, 0x0u, 0x6F36B0u, 0, "SPHERE" },   // btSphereShape+8
    { 0x43D670u, bt_leaf_kind_t::constant, 0x0u, 0x34u, 0, nullptr },   // btSphereShape+11; btConvexTriangleMeshShape+11; btBoxShape+11 ...
    { 0x50EF30u, bt_leaf_kind_t::constant, 0x0u, 0x1E0u, 0, nullptr },   // btRigidBody+3; VuRigidBody+3
    { 0x518D50u, bt_leaf_kind_t::constant, 0x0u, 0x7368A8u, 0, nullptr },   // VuCollisionMeshAsset+0
    { 0x51C3E0u, bt_leaf_kind_t::constant, 0x0u, 0x54u, 0, nullptr },   // VuOptimizedBvh+3; btOptimizedBvh+3; btQuantizedBvh+3
    { 0x5269B0u, bt_leaf_kind_t::text, 0x0u, 0x6F6318u, 0, "ConvexTrimesh" },   // btConvexTriangleMeshShape+8
    { 0x527B20u, bt_leaf_kind_t::text, 0x0u, 0x6F6328u, 0, "Compound" },   // btCompoundShape+8
    { 0x543870u, bt_leaf_kind_t::text, 0x0u, 0x6F63A4u, 0, "RigidBody" },   // VuRigidBodyComponent+1
    { 0x547490u, bt_leaf_kind_t::constant, 0x0u, 0xF8u, 0, nullptr },   // btCollisionObject+3; btPairCachingGhostObject+3; btGhostObject+3
    { 0x5489F0u, bt_leaf_kind_t::text, 0x0u, 0x6F63B0u, 0, "Box" },   // btBoxShape+8
    { 0x5AE8D0u, bt_leaf_kind_t::text, 0x0u, 0x6F647Cu, 0, "CylinderZ" },   // btCylinderShapeZ+8
    { 0x5AE940u, bt_leaf_kind_t::text, 0x0u, 0x6F6488u, 0, "CylinderX" },   // btCylinderShapeX+8
    { 0x5AEB40u, bt_leaf_kind_t::constant, 0x0u, 0x3Cu, 0, nullptr },   // btCylinderShapeZ+11; btCylinderShape+11; btCylinderShapeX+11 ...
    { 0x5AEB50u, bt_leaf_kind_t::text, 0x0u, 0x6F64A8u, 0, "CylinderY" },   // btCylinderShape+8
    { 0x5AF6E0u, bt_leaf_kind_t::text, 0x0u, 0x6F64B4u, 0, "BVHTRIANGLEMESH" },   // btBvhTriangleMeshShape+8
    { 0x5B83C0u, bt_leaf_kind_t::text, 0x0u, 0x6F64E0u, 0, "Triangle" },   // btTriangleShape+8
    { 0x60C4B0u, bt_leaf_kind_t::text, 0x0u, 0x6F6E9Cu, 0, "TRIANGLEMESH" },   // btTriangleMeshShape+8
    { 0x56DA10u, bt_leaf_kind_t::load, 0xD8u, 0x0u, 0, nullptr },   // btCollisionDispatcher+11; btCollisionDispatcher+12
    { 0x605C50u, bt_leaf_kind_t::load, 0xACu, 0x0u, 0, nullptr },   // VuDynamicsWorldImpl+23; btDiscreteDynamicsWorld+23
    { 0x605C60u, bt_leaf_kind_t::load, 0xB8u, 0x0u, 0, nullptr },   // VuDynamicsWorldImpl+24; btDiscreteDynamicsWorld+24
    { 0x5749C0u, bt_leaf_kind_t::store_zero, 0x7Cu, 0x0u, 0, nullptr },   // btSequentialImpulseConstraintSolver+4
    { 0x5269A0u, bt_leaf_kind_t::load_add, 0x80u, 0x10u, 0, nullptr },   // btConvexTriangleMeshShape+6
    { 0x5523C0u, bt_leaf_kind_t::store_arg, 0x50u, 0x0u, 4, nullptr },   // btCollisionWorld+2; VuDynamicsWorldImpl+2; btDiscreteDynamicsWorld+2 ...
    { 0x575820u, bt_leaf_kind_t::store_arg, 0x4u, 0x0u, 4, nullptr },   // VuDynamicsDebugDrawerImpl+10
    { 0x5AF6C0u, bt_leaf_kind_t::store_arg, 0xCu, 0x0u, 4, nullptr },   // btBvhTriangleMeshShape+9; btTriangleMeshShape+9
};

inline const bt_leaf_t* bt_leaf_find( std::uint32_t address )
{
    for ( const bt_leaf_t& l : k_bt_leaves )
        if ( l.address == address )
            return &l;
    return nullptr;
}

// One call on `self` (retail's address of this) over its image `object`:
// eax (with `eax_mask` the bits it defines), ST0 when `st0_set`; a store goes
// into the image.
struct bt_leaf_result_t
{
    std::uint32_t eax = 0;
    std::uint32_t eax_mask = 0;
    bool st0_set = false;
    float st0 = 0.0f;
};

inline bt_leaf_result_t bt_leaf_call( const bt_leaf_t& l, std::uint8_t* object, std::uint32_t self, std::uint32_t arg0 = 0 )
{
    bt_leaf_result_t r;
    switch ( l.kind )
    {
    case bt_leaf_kind_t::constant:
    case bt_leaf_kind_t::text:
        r.eax = l.value;
        r.eax_mask = 0xFFFFFFFFu;
        break;
    case bt_leaf_kind_t::this_plus:
        r.eax = self + l.k;
        r.eax_mask = 0xFFFFFFFFu;
        break;
    case bt_leaf_kind_t::load:
        std::memcpy( &r.eax, object + l.k, 4 );
        r.eax_mask = 0xFFFFFFFFu;
        break;
    case bt_leaf_kind_t::load_byte:
        r.eax = object[ l.k ];
        r.eax_mask = 0xFFu;
        break;
    case bt_leaf_kind_t::byte:
        r.eax = l.value;
        r.eax_mask = 0xFFu;
        break;
    case bt_leaf_kind_t::nop:
        break;
    case bt_leaf_kind_t::one:
        r.st0_set = true;
        r.st0 = 1.0f;
        break;
    case bt_leaf_kind_t::store_zero:
        std::memset( object + l.k, 0, 4 );
        break;
    case bt_leaf_kind_t::store_arg:
        std::memcpy( object + l.k, &arg0, 4 );
        break;
    case bt_leaf_kind_t::load_add:
        std::memcpy( &r.eax, object + l.k, 4 );
        r.eax += l.value;
        r.eax_mask = 0xFFFFFFFFu;
        break;
    }
    return r;
}
// The scalar deleting destructors of the race's Bullet classes whose own dtor
// is empty (boat batch 17; tmp/b17/dtors.py finds them by their whole body):
// [this] = the class's vtable; when the flag's bit 0 is set operator delete(
// this ) (the CRT import at 0x64C150 - the allocator: a hook); eax = this; ret 4.
// The comment names the first vtable slots that point there.
struct bt_scalar_dtor_t
{
    std::uint32_t address;
    std::uint32_t vtable;
};

inline constexpr bt_scalar_dtor_t k_bt_scalar_dtors[] = {
    { 0x413300u, 0x7380B4u },   // btMotionState+0
    { 0x4F8F10u, 0x74BFCCu },   // btOverlappingPairCallback+0; btGhostPairCallback+0
    { 0x50CF70u, 0x74C5F8u },   // VuDynamicsRayTest::RayTestWrapper+0; btCollisionWorld::RayResultCallback+0
    { 0x5188F0u, 0x74CB2Cu },   // btInternalTriangleIndexCallback+0; LocalSupportVertexCallback+0
    { 0x527B50u, 0x74CF98u },   // btCollisionShape+0; btTriangleMeshShape+0
    { 0x54BF60u, 0x74E8D0u },   // btGjkEpaPenetrationDepthSolver+0; btConvexPenetrationDepthSolver+0
    { 0x54BFB0u, 0x74E8BCu },   // btConvexCast::CastResult+3
    { 0x54DB60u, 0x74E6BCu },   // btTriangleCallback+0; 1PAV::ABVbtTransform::SAXPBVbtConvexShape::btCollisionWorld::??objec
    { 0x54F300u, 0x74E81Cu },   // 1PAVbtCollisionObject::ABVbtTransform::SAXPBVbtConvexShape::btCollisionWorld::??objectQuer
    { 0x54F550u, 0x74E7CCu },   // btSingleRayCallback+0; btSingleSweepCallback+0
    { 0x5524C0u, 0x74E750u },   // btManifoldResult+0; btDiscreteCollisionDetectorInterface::Result+0
    { 0x56C6D0u, 0x74F6D8u },   // btSphereTriangleCollisionAlgorithm::CreateFunc+0; btBoxBoxCollisionAlgorithm::CreateFunc+0
    { 0x56D170u, 0x74F684u },   // btCollisionConfiguration+0
    { 0x56D2C0u, 0x74F784u },   // btDispatcher+0
    { 0x56D7A0u, 0x74F778u },   // btCollisionPairCallback+0; btOverlapCallback+0
    { 0x56FA90u, 0x74F880u },   // btBroadphaseInterface+0
    { 0x574BC0u, 0x74F938u },   // btConstraintSolver+0
    { 0x575B70u, 0x74FB4Cu },   // btIDebugDraw+0
    { 0x5AF020u, 0x751784u },   // Z::111::ABVbtVector3::QAEXPAVbtTriangleCallback::btBvhTriangleMeshShape::?1??performConvex
    { 0x5B6F90u, 0x751C2Cu },   // btSubsimplexConvexCast+0; btConvexCast+0
    { 0x5B8DE0u, 0x751DB8u },   // btDiscreteCollisionDetectorInterface+0; btGjkPairDetector+0
    { 0x5F6540u, 0x7573CCu },   // btEmptyAlgorithm+0; btCollisionAlgorithm+0
    { 0x5FFF10u, 0x757BCCu },   // btSimulationIslandManager::IslandCallback+0
};

// One call: the object's first word becomes the vtable; elease (operator
// delete) runs when flags & 1. Returns self (eax).
inline std::uint32_t bt_scalar_dtor_run( const bt_scalar_dtor_t& d, std::uint8_t* object, std::uint32_t self, std::uint32_t flags,
                                         void ( *release )( void* user, std::uint32_t object ), void* user )
{
    std::memcpy( object, &d.vtable, 4 );
    if ( ( flags & 1u ) != 0 && release != nullptr )
        release( user, self );
    return self;
}