#pragma once

#include <cstdint>

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
};

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
// 0x5B53E0, 0x54C0C0, 0x56DD30: thiscall; flags; retn 4): the inner one, then
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
