#pragma once

#include <cstdint>
#include <initializer_list>

// The race's Bullet destructors that tear down btAlignedObjectArrays and
// btDbvt trees, recreated from retail's disassembly (boat batch 18). They work
// on RETAIL MEMORY - 32-bit addresses through bt_mem_t - because what they
// touch is the game's own object images (the vtable words, the arrays' data
// pointers, the tree nodes), not this tree's typed objects. The allocator
// (btAlignedFree's function pointer at 0x7358AC, operator delete = the CRT
// import 0x64C150) and every virtual a destructor calls on another object
// are hooks. gNumAlignedFree (0x759050) is the counter retail bumps before
// each btAlignedFree.
//
// btAlignedObjectArray<T> (Bullet 2.79, 20 bytes): +0 the allocator (a byte),
// +4 m_size, +8 m_capacity, +12 m_data, +16 m_ownsMemory (a byte).

struct bt_mem_t
{
    void* user = nullptr;
    std::uint32_t ( *rd )( void* user, std::uint32_t address ) = nullptr;
    std::uint8_t ( *rd8 )( void* user, std::uint32_t address ) = nullptr;
    void ( *wr )( void* user, std::uint32_t address, std::uint32_t value ) = nullptr;
    void ( *wr8 )( void* user, std::uint32_t address, std::uint8_t value ) = nullptr;
    void ( *aligned_free )( void* user, std::uint32_t pointer ) = nullptr;   // [0x7358AC]
    void ( *release )( void* user, std::uint32_t object ) = nullptr;         // operator delete (import 0x64C150)
    // A virtual call on another object: `self` its address, `offset` the vtable
    // slot's byte offset, the stack arguments; answers eax.
    std::uint32_t ( *vcall )( void* user, std::uint32_t self, std::uint32_t offset, const std::uint32_t* args, int count ) = nullptr;
    std::uint32_t* num_aligned_free = nullptr;                               // gNumAlignedFree 0x759050
    // (boat batch 21) btAlignedAlloc's function pointer [0x7358A4] (cdecl size,
    // alignment; answers the block) and gNumAlignedAlloc 0x759054.
    std::uint32_t ( *aligned_alloc )( void* user, std::uint32_t size, std::uint32_t alignment ) = nullptr;
    std::uint32_t* num_aligned_alloc = nullptr;
};

// btAlignedAlloc inlined: ++gNumAlignedAlloc, then the alloc function (a
// hook: the caller must set it).
inline std::uint32_t bt_aligned_alloc( const bt_mem_t& m, std::uint32_t size, std::uint32_t alignment )
{
    if ( m.num_aligned_alloc )
        ++*m.num_aligned_alloc;
    return m.aligned_alloc( m.user, size, alignment );
}

// btAlignedFree inlined: ++gNumAlignedFree, then the free function.
inline void bt_aligned_free( const bt_mem_t& m, std::uint32_t pointer )
{
    if ( m.num_aligned_free )
        ++*m.num_aligned_free;
    if ( m.aligned_free )
        m.aligned_free( m.user, pointer );
}

// ~btAlignedObjectArray / clear() inlined (every destructor below repeats it):
// m_data set -> freed only when m_ownsMemory (then m_data = 0); then
// m_ownsMemory = 1, m_data = 0, m_size = 0, m_capacity = 0.
inline void bt_array_clear( const bt_mem_t& m, std::uint32_t array )
{
    const std::uint32_t data = m.rd( m.user, array + 12 );
    if ( data != 0 )
    {
        if ( m.rd8( m.user, array + 16 ) != 0 )
            bt_aligned_free( m, data );
        m.wr( m.user, array + 12, 0 );
    }
    m.wr8( m.user, array + 16, 1 );
    m.wr( m.user, array + 12, 0 );
    m.wr( m.user, array + 4, 0 );
    m.wr( m.user, array + 8, 0 );
}

// The scalar deleting destructors that call an inner destructor (0x56D290,
// 0x5B53E0, 0x54C0C0, 0x56DD30, 0x56FB80, 0x624580, 0x6000F0, 0x56CCD0: thiscall;
// flags; retn 4): the inner one, then
// operator delete( this ) when flags & 1; eax = this.
inline std::uint32_t bt_deleting( const bt_mem_t& m, std::uint32_t self, std::uint32_t flags, void ( *inner )( const bt_mem_t&, std::uint32_t ) )
{
    inner( m, self );
    if ( ( flags & 1u ) != 0 && m.release )
        m.release( m.user, self );
    return self;
}

// btCollisionDispatcher::~btCollisionDispatcher 0x56D2F0: [this] = 0x74F7C4;
// the member btManifoldResult's base vtable (+0x20 = 0x74E750, its empty dtor
// inlined); m_manifoldsPtr (+8) cleared; [this] = btDispatcher's 0x74F784.
inline void bt_dispatcher_destroy( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x74F7C4u );
    m.wr( m.user, self + 0x20, 0x74E750u );
    bt_array_clear( m, self + 8 );
    m.wr( m.user, self, 0x74F784u );
}

// 0x54B830: ~btAlignedObjectArray<btFace> (ecx the array): each face (0x24
// bytes from m_data, m_size of them in order) has its m_indices array at +0
// cleared; then the array itself.
inline void bt_face_array_clear( const bt_mem_t& m, std::uint32_t array )
{
    const std::int32_t n = static_cast< std::int32_t >( m.rd( m.user, array + 4 ) );
    for ( std::int32_t i = 0; i < n; ++i )   // m_data re-read each pass, the count not
        bt_array_clear( m, m.rd( m.user, array + 12 ) + 0x24u * static_cast< std::uint32_t >( i ) );
    bt_array_clear( m, array );
}

// btConvexPolyhedron::~btConvexPolyhedron 0x5B5410: [this] = 0x751BD4;
// m_uniqueEdges (+0x38) cleared, m_faces (+0x24) through 0x54B830, then
// m_vertices (+0x10).
inline void bt_convex_polyhedron_destroy( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x751BD4u );
    bt_array_clear( m, self + 0x38 );
    bt_face_array_clear( m, self + 0x24 );
    bt_array_clear( m, self + 0x10 );
}

// btCollisionWorld::~btCollisionWorld 0x54C0F0: [this] = 0x74E880; per
// collision object (m_collisionObjects +4: m_size +8, m_data +0x10, in order)
// with a broadphase handle (+0xC8): the broadphase's (+0x4C) vt+0x24
// (getOverlappingPairCache) -> its vt+0x24 cleanProxyFromPairs( handle, the
// dispatcher +0x18 ), the broadphase's vt+8 destroyProxy( handle, the
// dispatcher ), +0xC8 = 0 (the count re-read each pass); then the array.
inline void bt_collision_world_destroy( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x74E880u );
    for ( std::int32_t i = 0; i < static_cast< std::int32_t >( m.rd( m.user, self + 8 ) ); ++i )
    {
        const std::uint32_t object = m.rd( m.user, m.rd( m.user, self + 0x10 ) + 4u * static_cast< std::uint32_t >( i ) );
        const std::uint32_t handle = m.rd( m.user, object + 0xC8 );
        if ( handle == 0 )
            continue;
        const std::uint32_t broadphase = m.rd( m.user, self + 0x4C );
        const std::uint32_t cache = m.vcall( m.user, broadphase, 0x24, nullptr, 0 );
        const std::uint32_t a[ 2 ] = { handle, m.rd( m.user, self + 0x18 ) };
        m.vcall( m.user, cache, 0x24, a, 2 );
        const std::uint32_t b[ 2 ] = { handle, m.rd( m.user, self + 0x18 ) };
        m.vcall( m.user, m.rd( m.user, self + 0x4C ), 8, b, 2 );
        m.wr( m.user, object + 0xC8, 0 );
    }
    bt_array_clear( m, self + 4 );
}

// btDbvt deletenode inlined: the cached node (+4 m_free) freed, this one
// cached.
inline void bt_dbvt_delete_node( const bt_mem_t& m, std::uint32_t tree, std::uint32_t node )
{
    const std::uint32_t cached = m.rd( m.user, tree + 4 );
    if ( cached != 0 )
        bt_aligned_free( m, cached );
    m.wr( m.user, tree + 4, node );
}

// recursedeletenode 0x594290 (ecx tree, edx node): an internal node (+0x28
// childs[1] set) recurses into childs[0] (+0x24) then childs[1]; the root (+0)
// is cleared when it is this node; then deletenode.
inline void bt_dbvt_recurse_delete( const bt_mem_t& m, std::uint32_t tree, std::uint32_t node )
{
    if ( m.rd( m.user, node + 0x28 ) != 0 )
    {
        bt_dbvt_recurse_delete( m, tree, m.rd( m.user, node + 0x24 ) );
        bt_dbvt_recurse_delete( m, tree, m.rd( m.user, node + 0x28 ) );
    }
    if ( node == m.rd( m.user, tree ) )
        m.wr( m.user, tree, 0 );
    bt_dbvt_delete_node( m, tree, node );
}

// btDbvt::clear 0x5946B0: a root goes through recursedeletenode (inlined at
// the top); m_free freed when set and +4 = 0; m_lkhd (+8) = -1; m_stkStack
// (+0x14) cleared; m_opath (+0x10) = 0.
inline void bt_dbvt_clear( const bt_mem_t& m, std::uint32_t tree )
{
    const std::uint32_t root = m.rd( m.user, tree );
    if ( root != 0 )
        bt_dbvt_recurse_delete( m, tree, root );
    const std::uint32_t cached = m.rd( m.user, tree + 4 );
    if ( cached != 0 )
        bt_aligned_free( m, cached );
    m.wr( m.user, tree + 4, 0 );
    m.wr( m.user, tree + 8, 0xFFFFFFFFu );
    bt_array_clear( m, tree + 0x14 );
    m.wr( m.user, tree + 0x10, 0 );
}

// btDbvt::~btDbvt 0x594620: clear, then m_stkStack's destructor (cleared again).
inline void bt_dbvt_destroy( const bt_mem_t& m, std::uint32_t tree )
{
    bt_dbvt_clear( m, tree );
    bt_array_clear( m, tree + 0x14 );
}

// btDbvtBroadphase::~btDbvtBroadphase 0x56DD60: [this] = 0x74F848; with
// m_releasepaircache (+0x98) the pair cache (+0x60)'s vt+0( 0 ) (its
// destructor: a hook) and then, when +0x60 is still set, btAlignedFree of it;
// the two m_sets (+4, 0x28 each) destroyed LAST TO FIRST (the CRT's eh vector
// destructor iterator 0x630EF0 with 0x594620); [this] = 0x74F880.
inline void bt_dbvt_broadphase_destroy( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x74F848u );
    if ( m.rd8( m.user, self + 0x98 ) != 0 )
    {
        const std::uint32_t zero = 0;
        m.vcall( m.user, m.rd( m.user, self + 0x60 ), 0, &zero, 1 );
        const std::uint32_t cache = m.rd( m.user, self + 0x60 );
        if ( cache != 0 )
            bt_aligned_free( m, cache );
    }
    for ( int k = 1; k >= 0; --k )
        bt_dbvt_destroy( m, self + 4 + 0x28u * static_cast< std::uint32_t >( k ) );
    m.wr( m.user, self, 0x74F880u );
}

// btSequentialImpulseConstraintSolver::~btSequentialImpulseConstraintSolver
// 0x56FBB0: [this] = 0x74F8EC; the six arrays from the last member to the
// first - +0x68, +0x54, +0x40, +0x2C, +0x18, +4; [this] = btConstraintSolver's
// 0x74F938.
inline void bt_sequential_solver_destroy( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x74F8ECu );
    for ( std::uint32_t at : { 0x68u, 0x54u, 0x40u, 0x2Cu, 0x18u, 4u } )
        bt_array_clear( m, self + at );
    m.wr( m.user, self, 0x74F938u );
}

// btSimulationIslandManager::~btSimulationIslandManager 0x6245B0: [this] =
// 0x758330; m_islandmanifold (+0x2C), m_islandBodies (+0x18), then the
// btUnionFind's m_elements (+4) cleared. No base vtable store follows.
inline void bt_island_manager_destroy( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x758330u );
    bt_array_clear( m, self + 0x2C );
    bt_array_clear( m, self + 0x18 );
    bt_array_clear( m, self + 4 );
}

// A member Bullet owns and destroys in place: its vt+0( 0 ) (the scalar
// deleting dtor without the delete: a hook), then btAlignedFree when the
// pointer is still set - NO null test before the virtual call, as retail.
inline void bt_owned_destroy( const bt_mem_t& m, std::uint32_t self, std::uint32_t at )
{
    const std::uint32_t zero = 0;
    m.vcall( m.user, m.rd( m.user, self + at ), 0, &zero, 1 );
    const std::uint32_t p = m.rd( m.user, self + at );
    if ( p != 0 )
        bt_aligned_free( m, p );
}

// btDiscreteDynamicsWorld::~btDiscreteDynamicsWorld 0x600120: [this] =
// 0x757A94; m_ownsIslandManager (+0xF4) -> the island manager (+0xB0)
// destroyed in place; m_ownsConstraintSolver (+0xF5) -> the solver (+0xAC);
// the arrays +0xF8, +0xC8, +0xB4; [this] = btDynamicsWorld's 0x757B44; then
// btCollisionWorld's 0x54C0F0.
inline void bt_dynamics_world_destroy( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x757A94u );
    if ( m.rd8( m.user, self + 0xF4 ) != 0 )
        bt_owned_destroy( m, self, 0xB0 );
    if ( m.rd8( m.user, self + 0xF5 ) != 0 )
        bt_owned_destroy( m, self, 0xAC );
    bt_array_clear( m, self + 0xF8 );
    bt_array_clear( m, self + 0xC8 );
    bt_array_clear( m, self + 0xB4 );
    m.wr( m.user, self, 0x757B44u );
    bt_collision_world_destroy( m, self );
}

// btStackAlloc::destroy inlined: with usedsize (+8) 0, data (+0) freed unless
// ischild (+0x10, a byte) or null, then data = 0, usedsize = 0.
inline void bt_stack_alloc_destroy( const bt_mem_t& m, std::uint32_t alloc )
{
    if ( m.rd( m.user, alloc + 8 ) != 0 )
        return;
    if ( m.rd8( m.user, alloc + 0x10 ) == 0 )
    {
        const std::uint32_t data = m.rd( m.user, alloc );
        if ( data != 0 )
            bt_aligned_free( m, data );
    }
    m.wr( m.user, alloc, 0 );
    m.wr( m.user, alloc + 8, 0 );
}

// btDefaultCollisionConfiguration::~btDefaultCollisionConfiguration 0x56CD00:
// [this] = 0x74F668; m_ownsStackAllocator (+0xC): the stack allocator (+8)
// destroy()ed twice (its destroy, then its dtor's), then freed when set;
// m_ownsCollisionAlgorithmPool (+0x1C) / m_ownsPersistentManifoldPool (+0x14):
// the pool (+0x18 / +0x10)'s block (+0x10) freed when set, then the pool;
// the create functions destroyed in place in retail's order +0x28, +0x2C,
// +0x30, +0x34, +0x38, +0x3C, +0x40, +0x48, +0x4C, +0x44, +0x54, +0x50; the
// simplex solver (+0x20) freed when set (no dtor call); the penetration
// depth solver (+0x24) destroyed in place; [this] = 0x74F684.
inline void bt_collision_configuration_destroy( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x74F668u );
    if ( m.rd8( m.user, self + 0xC ) != 0 )
    {
        bt_stack_alloc_destroy( m, m.rd( m.user, self + 8 ) );
        bt_stack_alloc_destroy( m, m.rd( m.user, self + 8 ) );
        const std::uint32_t p = m.rd( m.user, self + 8 );
        if ( p != 0 )
            bt_aligned_free( m, p );
    }
    for ( std::uint32_t own : { 0x1Cu, 0x14u } )
    {
        if ( m.rd8( m.user, self + own ) == 0 )
            continue;
        const std::uint32_t block = m.rd( m.user, m.rd( m.user, self + own - 4 ) + 0x10 );
        if ( block != 0 )
            bt_aligned_free( m, block );
        const std::uint32_t pool = m.rd( m.user, self + own - 4 );
        if ( pool != 0 )
            bt_aligned_free( m, pool );
    }
    for ( std::uint32_t at : { 0x28u, 0x2Cu, 0x30u, 0x34u, 0x38u, 0x3Cu, 0x40u, 0x48u, 0x4Cu, 0x44u, 0x54u, 0x50u } )
        bt_owned_destroy( m, self, at );
    const std::uint32_t simplex = m.rd( m.user, self + 0x20 );
    if ( simplex != 0 )
        bt_aligned_free( m, simplex );
    bt_owned_destroy( m, self, 0x24 );
    m.wr( m.user, self, 0x74F684u );
}

// btQuantizedBvh::~btQuantizedBvh 0x586470: [this] = 0x74FED4; the five
// arrays from the last member - +0xA0, +0x88, +0x74, +0x60, +0x4C.
inline void bt_quantized_bvh_destroy( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x74FED4u );
    for ( std::uint32_t at : { 0xA0u, 0x88u, 0x74u, 0x60u, 0x4Cu } )
        bt_array_clear( m, self + at );
}

// The scalar deleting dtors of the classes with Bullet's aligned operator
// delete (BT_DECLARE_ALIGNED_ALLOCATOR: btAlignedFree, not the CRT's):
// 0x586320 (btQuantizedBvh: 0x586470, then with flags & 1 and this non-null
// the free); 0x43D5E0 (btCollisionShape's vtable 0x74CF98 for the
// sphere / cylinder rows) and 0x5471A0 (btCollisionObject 0x74DEBC): the base
// vtable stored, then with flags & 1 the free (no null test). eax = this.
inline std::uint32_t bt_quantized_bvh_delete( const bt_mem_t& m, std::uint32_t self, std::uint32_t flags )
{
    bt_quantized_bvh_destroy( m, self );
    if ( ( flags & 1u ) != 0 && self != 0 )
        bt_aligned_free( m, self );
    return self;
}

inline std::uint32_t bt_aligned_base_delete( const bt_mem_t& m, std::uint32_t self, std::uint32_t flags, std::uint32_t vtable )
{
    m.wr( m.user, self, vtable );
    if ( ( flags & 1u ) != 0 )
        bt_aligned_free( m, self );
    return self;
}

// btDynamicsWorld's scalar deleting dtor 0x6060A0: [this] = 0x757B44,
// btCollisionWorld's 0x54C0F0, operator delete (the CRT import) with
// flags & 1; eax = this.
inline std::uint32_t bt_dynamics_world_base_delete( const bt_mem_t& m, std::uint32_t self, std::uint32_t flags )
{
    m.wr( m.user, self, 0x757B44u );
    bt_collision_world_destroy( m, self );
    if ( ( flags & 1u ) != 0 && m.release )
        m.release( m.user, self );
    return self;
}

// btEmptyAlgorithm::CreateFunc::CreateCollisionAlgorithm 0x56C270 (thiscall;
// ci, body0, body1; retn 0Ch): ci.m_dispatcher1's (+0) vt+0x34
// allocateCollisionAlgorithm( 8 ); a block -> [block] = btCollisionAlgorithm's
// 0x7573CC, +4 = the dispatcher, [block] = btEmptyAlgorithm's 0x757394.
// Answers the block (0 when none).
inline std::uint32_t bt_empty_algorithm_create( const bt_mem_t& m, std::uint32_t ci )
{
    const std::uint32_t eight = 8;
    const std::uint32_t block = m.vcall( m.user, m.rd( m.user, ci ), 0x34, &eight, 1 );
    if ( block == 0 )
        return 0;
    m.wr( m.user, block, 0x7573CCu );
    m.wr( m.user, block + 4, m.rd( m.user, ci ) );
    m.wr( m.user, block, 0x757394u );
    return block;
}

// ClosestConvexResultCallback's needsCollision 0x552540 (ConvexResultCallback
// slot 1; thiscall; proxy; retn 4): the proxy's group (+4, u16) against this
// one's mask (+0xA), then this one's group (+8) against the proxy's mask (+6).
inline bool bt_convex_result_needs_collision( const bt_mem_t& m, std::uint32_t self, std::uint32_t proxy )
{
    const std::uint32_t pw = m.rd( m.user, proxy + 4 ), sw = m.rd( m.user, self + 8 );
    if ( ( ( pw & 0xFFFFu ) & ( sw >> 16 ) ) == 0 )
        return false;
    return ( ( pw >> 16 ) & ( sw & 0xFFFFu ) ) != 0;
}

// calculateLocalInertia of the triangle / mesh shapes 0x5B80E0 (slot 7;
// thiscall; mass, inertia*; retn 8): the four words of *inertia = 0.
inline void bt_zero_inertia( const bt_mem_t& m, std::uint32_t inertia )
{
    for ( std::uint32_t k = 0; k < 16; k += 4 )
        m.wr( m.user, inertia + k, 0 );
}

// btBvhTriangleMeshShape slot 16 0x5AF690 (thiscall; out, direction; retn 8):
// this one's vt+0x3C( out, direction ); answers out.
inline std::uint32_t bt_support_forward( const bt_mem_t& m, std::uint32_t self, std::uint32_t out, std::uint32_t direction )
{
    const std::uint32_t a[ 2 ] = { out, direction };
    m.vcall( m.user, self, 0x3C, a, 2 );
    return out;
}

// The tail-jump forwarders: 0x43D5B0 (btTriangleShape slot 1: jmp vt+0x48 on
// this, the caller's arguments untouched) and 0x54FE50 (DebugDrawcallback's
// adjustor thunk: this - 4, then jmp vt+4). The forwarded call answers.
inline std::uint32_t bt_forward_43D5B0( const bt_mem_t& m, std::uint32_t self, const std::uint32_t* args, int count )
{
    return m.vcall( m.user, self, 0x48, args, count );
}

inline std::uint32_t bt_forward_54FE50( const bt_mem_t& m, std::uint32_t self, const std::uint32_t* args, int count )
{
    return m.vcall( m.user, self - 4, 4, args, count );
}