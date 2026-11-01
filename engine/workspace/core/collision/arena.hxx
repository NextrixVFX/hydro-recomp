#pragma once

#include <workspace/core/collision/teardown.hxx>

#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

// More of the race's Bullet rows on RETAIL MEMORY (boat batch 19), the
// bt_mem_t model of teardown.hxx: the shapes' / broadphase's / result
// callbacks' small virtuals, recreated from retail's disassembly. A virtual
// that takes a pointer to the caller's STACK (getAabb's min / max) goes
// through bt_io_t: its stack-free arguments go in, the words it writes come
// back (the pointer values themselves are retail's stack, never compared).

struct bt_io_t
{
    const bt_mem_t* m = nullptr;
    // A virtual on `self` at vtable byte offset `offset` with `count` stack-free
    // arguments; `out_words` words written through its pointer arguments land
    // in `out` (in argument order). Answers eax.
    std::uint32_t ( *vcall_out )( void* user, std::uint32_t self, std::uint32_t offset, const std::uint32_t* args, int count, std::uint32_t* out,
                                  int out_words ) = nullptr;
};

inline float bt_rdf( const bt_mem_t& m, std::uint32_t address )
{
    const std::uint32_t w = m.rd( m.user, address );
    float f;
    std::memcpy( &f, &w, 4 );
    return f;
}

inline void bt_wrf( const bt_mem_t& m, std::uint32_t address, float f )
{
    std::uint32_t w;
    std::memcpy( &w, &f, 4 );
    m.wr( m.user, address, w );
}

inline void bt_copy16( const bt_mem_t& m, std::uint32_t to, std::uint32_t from )
{
    for ( std::uint32_t k = 0; k < 16; k += 4 )
        m.wr( m.user, to + k, m.rd( m.user, from + k ) );
}

// btConvexInternalShape::setLocalScaling 0x552F30 (the sphere / polyhedral /
// triangle rows' slot 5; thiscall; scaling*; retn 4): +0x10..+0x18 = the
// scaling's lanes `andps` 0x6F7CC0 (0x7FFFFFFF: the absolute value, bitwise -
// a NaN keeps its payload), +0x1C = 0.
inline void bt_convex_internal_set_scaling( const bt_mem_t& m, std::uint32_t self, std::uint32_t scaling )
{
    for ( std::uint32_t k = 0; k < 12; k += 4 )
        m.wr( m.user, self + 0x10 + k, m.rd( m.user, scaling + k ) & 0x7FFFFFFFu );
    m.wr( m.user, self + 0x1C, 0 );
}

// btPointCollector::addContactPoint 0x5B8E10 (slot 3; thiscall; normal*,
// point*, depth; retn 0Ch): `comiss distance(+0x30), depth; jbe` - only when
// the kept distance is greater (ordered): +0x34 hasResult = 1, +0x10 the
// normal, +0x20 the point (16 bytes each), +0x30 = depth.
inline void bt_point_collector_add( const bt_mem_t& m, std::uint32_t self, std::uint32_t normal, std::uint32_t point, std::uint32_t depth_bits )
{
    float depth;
    std::memcpy( &depth, &depth_bits, 4 );
    if ( !_mm_comigt_ss( _mm_set_ss( bt_rdf( m, self + 0x30 ) ), _mm_set_ss( depth ) ) )
        return;
    m.wr8( m.user, self + 0x34, 1 );
    bt_copy16( m, self + 0x10, normal );
    bt_copy16( m, self + 0x20, point );
    m.wr( m.user, self + 0x30, depth_bits );
}

// btConvexTriangleCallback's scalar deleting dtor 0x5F6700 (thiscall; flags;
// retn 4): [this] = 0x75752C; the dispatcher (+0x34) vt+0x10
// clearManifold( +0x44 ) then vt+0xC releaseManifold( +0x44 ) (both re-read);
// [this] = btTriangleCallback's 0x74E6BC; operator delete (the CRT's) with
// flags & 1. eax = this.
inline std::uint32_t bt_convex_triangle_callback_delete( const bt_mem_t& m, std::uint32_t self, std::uint32_t flags )
{
    m.wr( m.user, self, 0x75752Cu );
    const std::uint32_t a[ 1 ] = { m.rd( m.user, self + 0x44 ) };
    m.vcall( m.user, m.rd( m.user, self + 0x34 ), 0x10, a, 1 );
    const std::uint32_t b[ 1 ] = { m.rd( m.user, self + 0x44 ) };
    m.vcall( m.user, m.rd( m.user, self + 0x34 ), 0xC, b, 1 );
    m.wr( m.user, self, 0x74E6BCu );
    if ( ( flags & 1u ) != 0 && m.release )
        m.release( m.user, self );
    return self;
}

// btCompoundShape::~btCompoundShape 0x526B40: [this] = 0x74CF54; with the
// dynamic aabb tree (+0x40) its ~btDbvt (0x594620) and, +0x40 re-read, its
// btAlignedFree; the m_children array (+0xC) cleared; [this] = btCollision-
// Shape's 0x74CF98. The scalar deleting dtor 0x526B10 (flags; retn 4): that,
// then with flags & 1 and this non-null the btAlignedFree. eax = this.
inline void bt_compound_shape_destroy( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x74CF54u );
    if ( m.rd( m.user, self + 0x40 ) != 0 )
    {
        bt_dbvt_destroy( m, m.rd( m.user, self + 0x40 ) );
        const std::uint32_t tree = m.rd( m.user, self + 0x40 );
        if ( tree != 0 )
            bt_aligned_free( m, tree );
    }
    bt_array_clear( m, self + 0xC );
    m.wr( m.user, self, 0x74CF98u );
}

inline std::uint32_t bt_compound_shape_delete( const bt_mem_t& m, std::uint32_t self, std::uint32_t flags )
{
    bt_compound_shape_destroy( m, self );
    if ( ( flags & 1u ) != 0 && self != 0 )
        bt_aligned_free( m, self );
    return self;
}

// btBvhTriangleMeshShape's scalar deleting dtor 0x5AEC10 (flags; retn 4):
// [this] = 0x751794; when it owns the bvh (+0x49) the bvh's (+0x40) vt+0( 0 )
// and, +0x40 re-read and non-null, its btAlignedFree; [this] = 0x74CF98;
// with flags & 1 the btAlignedFree of this (no null test). eax = this.
inline std::uint32_t bt_bvh_mesh_shape_delete( const bt_mem_t& m, std::uint32_t self, std::uint32_t flags )
{
    m.wr( m.user, self, 0x751794u );
    if ( m.rd8( m.user, self + 0x49 ) != 0 )
    {
        const std::uint32_t zero = 0;
        m.vcall( m.user, m.rd( m.user, self + 0x40 ), 0, &zero, 1 );
        const std::uint32_t bvh = m.rd( m.user, self + 0x40 );
        if ( bvh != 0 )
            bt_aligned_free( m, bvh );
    }
    m.wr( m.user, self, 0x74CF98u );
    if ( ( flags & 1u ) != 0 )
        bt_aligned_free( m, self );
    return self;
}

// btCollisionWorld::removeCollisionObject 0x54C940 (slot 8; thiscall; object;
// retn 4): with a broadphase handle (+0xC8) the broadphase's (+0x4C) vt+0x24
// getOverlappingPairCache -> its vt+0x24 cleanProxyFromPairs( handle, the
// dispatcher +0x18 ), the broadphase's vt+8 destroyProxy( handle, the
// dispatcher ), +0xC8 = 0; then btAlignedObjectArray::remove 0x50F200 on the
// objects (+4: m_size +8, m_data +0x10): the FIRST match of a linear search
// swapped with the last element (the data pointer re-read), --m_size; no match
// -> nothing.
inline void bt_collision_world_remove_object( const bt_mem_t& m, std::uint32_t self, std::uint32_t object )
{
    const std::uint32_t handle = m.rd( m.user, object + 0xC8 );
    if ( handle != 0 )
    {
        const std::uint32_t cache = m.vcall( m.user, m.rd( m.user, self + 0x4C ), 0x24, nullptr, 0 );
        const std::uint32_t a[ 2 ] = { handle, m.rd( m.user, self + 0x18 ) };
        m.vcall( m.user, cache, 0x24, a, 2 );
        const std::uint32_t b[ 2 ] = { handle, m.rd( m.user, self + 0x18 ) };
        m.vcall( m.user, m.rd( m.user, self + 0x4C ), 8, b, 2 );
        m.wr( m.user, object + 0xC8, 0 );
    }
    const std::int32_t size = static_cast< std::int32_t >( m.rd( m.user, self + 8 ) );
    std::int32_t at = size;
    for ( std::int32_t i = 0; i < size; ++i )
        if ( m.rd( m.user, m.rd( m.user, self + 0x10 ) + 4u * static_cast< std::uint32_t >( i ) ) == object )
        {
            at = i;
            break;
        }
    if ( at >= size )
        return;
    const std::uint32_t data = m.rd( m.user, self + 0x10 );
    const std::uint32_t found = m.rd( m.user, data + 4u * static_cast< std::uint32_t >( at ) );
    m.wr( m.user, data + 4u * static_cast< std::uint32_t >( at ), m.rd( m.user, data + 4u * static_cast< std::uint32_t >( size ) - 4 ) );
    m.wr( m.user, m.rd( m.user, self + 0x10 ) + 4u * static_cast< std::uint32_t >( size ) - 4, found );
    m.wr( m.user, self + 8, m.rd( m.user, self + 8 ) - 1 );
}

// btDbvtBroadphase::getAabb 0x56E000 (slot 4; proxy, aabbMin*, aabbMax*; retn
// 0Ch): the proxy's +0x10 / +0x20 (16 bytes each) out.
inline void bt_dbvt_broadphase_get_aabb( const bt_mem_t& m, std::uint32_t proxy, std::uint32_t mn, std::uint32_t mx )
{
    bt_copy16( m, mn, proxy + 0x10 );
    bt_copy16( m, mx, proxy + 0x20 );
}

// btDbvtBroadphase::getBroadphaseAabb 0x56EA70 (slot 10; aabbMin*, aabbMax*;
// retn 8): both roots (m_sets[0] +4, m_sets[1] +0x2C) -> Merge (`minps` /
// `maxps` with the first root first: a NaN lane answers the second root's);
// one root -> its volume; none -> zeros. 16 bytes each out.
inline void bt_dbvt_broadphase_world_aabb( const bt_mem_t& m, std::uint32_t self, std::uint32_t mn, std::uint32_t mx )
{
    const std::uint32_t r0 = m.rd( m.user, self + 4 ), r1 = m.rd( m.user, self + 0x2C );
    std::uint32_t v[ 4 ][ 4 ] = { };   // r0 min, r0 max, r1 min, r1 max (words)
    for ( std::uint32_t k = 0; k < 4; ++k )
    {
        if ( r0 != 0 )
        {
            v[ 0 ][ k ] = m.rd( m.user, r0 + 4 * k );
            v[ 1 ][ k ] = m.rd( m.user, r0 + 0x10 + 4 * k );
        }
        if ( r1 != 0 )
        {
            v[ 2 ][ k ] = m.rd( m.user, r1 + 4 * k );
            v[ 3 ][ k ] = m.rd( m.user, r1 + 0x10 + 4 * k );
        }
    }
    std::uint32_t lo[ 4 ] = { }, hi[ 4 ] = { };
    if ( r0 != 0 && r1 != 0 )
    {
        const __m128 a0 = _mm_castsi128_ps( _mm_loadu_si128( reinterpret_cast< const __m128i* >( v[ 0 ] ) ) );
        const __m128 a1 = _mm_castsi128_ps( _mm_loadu_si128( reinterpret_cast< const __m128i* >( v[ 1 ] ) ) );
        const __m128 b0 = _mm_castsi128_ps( _mm_loadu_si128( reinterpret_cast< const __m128i* >( v[ 2 ] ) ) );
        const __m128 b1 = _mm_castsi128_ps( _mm_loadu_si128( reinterpret_cast< const __m128i* >( v[ 3 ] ) ) );
        _mm_storeu_si128( reinterpret_cast< __m128i* >( lo ), _mm_castps_si128( _mm_min_ps( a0, b0 ) ) );
        _mm_storeu_si128( reinterpret_cast< __m128i* >( hi ), _mm_castps_si128( _mm_max_ps( a1, b1 ) ) );
    }
    else if ( r0 != 0 || r1 != 0 )
    {
        const int r = r0 != 0 ? 0 : 2;
        std::memcpy( lo, v[ r ], 16 );
        std::memcpy( hi, v[ r + 1 ], 16 );
    }
    for ( std::uint32_t k = 0; k < 4; ++k )
    {
        m.wr( m.user, mn + 4 * k, lo[ k ] );
        m.wr( m.user, mx + 4 * k, hi[ k ] );
    }
}

// btDbvtBroadphase::resetPool 0x56EB20 (slot 11; dispatcher; retn 4): only
// when m_sets[0].m_leaves (+0x10) + m_sets[1].m_leaves (+0x38) is 0: both
// sets cleared (btDbvt::clear 0x5946B0), +0x99 m_deferedcollide = 0 and +0x9A
// m_needcleanup = 1 (one word store), m_stageCurrent 0, m_fixedleft 0,
// m_fupdates 1, m_dupdates 0, m_cupdates 10, m_newpairs 1, m_updates_call /
// _done / _ratio 0, m_gid / m_pid / m_cid 0, m_stageRoots[0..2] 0.
inline void bt_dbvt_broadphase_reset_pool( const bt_mem_t& m, std::uint32_t self )
{
    if ( m.rd( m.user, self + 0x38 ) + m.rd( m.user, self + 0x10 ) != 0 )
        return;
    bt_dbvt_clear( m, self + 4 );
    bt_dbvt_clear( m, self + 0x2C );
    m.wr8( m.user, self + 0x99, 0 );
    m.wr8( m.user, self + 0x9A, 1 );
    const std::uint32_t stores[ ][ 2 ] = { { 0x68, 0 }, { 0x7C, 0 }, { 0x6C, 1 }, { 0x70, 0 }, { 0x74, 10 }, { 0x78, 1 }, { 0x80, 0 }, { 0x84, 0 },
                                           { 0x88, 0 }, { 0x94, 0 }, { 0x8C, 0 }, { 0x90, 0 }, { 0x54, 0 }, { 0x58, 0 }, { 0x5C, 0 } };
    for ( const auto& s : stores )
        m.wr( m.user, self + s[ 0 ], s[ 1 ] );
}

// ClosestConvexResultCallback::addSingleResult 0x605FA0 (slot 2; thiscall;
// convexResult*, normalInWorldSpace; retn 8): +4 m_closestHitFraction = the
// result's +0x30, +0x50 m_hitCollisionObject = its +0; the hit normal (+0x30)
// = the result's +0x10 (16 bytes) in world space, else the object's basis
// (rows +0x10 / +0x20 / +0x30) times it - x = (b01 ny + b00 nx) + b02 nz,
// y = (b11 ny + b10 nx) + b12 nz, z = (b21 ny + nx b20) + b22 nz, w 0; +0x40
// the hit point = the result's +0x20. Answers (ST0) the result's +0x30.
inline std::uint32_t bt_closest_convex_add_single_result( const bt_mem_t& m, std::uint32_t self, std::uint32_t result, std::uint32_t normal_in_world )
{
    m.wr( m.user, self + 4, m.rd( m.user, result + 0x30 ) );
    const std::uint32_t object = m.rd( m.user, result );
    m.wr( m.user, self + 0x50, object );
    if ( ( normal_in_world & 0xFFu ) != 0 )
        bt_copy16( m, self + 0x30, result + 0x10 );
    else
    {
        const float nx = bt_rdf( m, result + 0x10 ), ny = bt_rdf( m, result + 0x14 ), nz = bt_rdf( m, result + 0x18 );
        const auto b = [ & ]( std::uint32_t at ) { return bt_rdf( m, object + at ); };
        const float x = ( b( 0x14 ) * ny + b( 0x10 ) * nx ) + b( 0x18 ) * nz;
        const float y = ( b( 0x24 ) * ny + b( 0x20 ) * nx ) + b( 0x28 ) * nz;
        const float z = ( b( 0x34 ) * ny + nx * b( 0x30 ) ) + b( 0x38 ) * nz;
        bt_wrf( m, self + 0x30, x );
        bt_wrf( m, self + 0x34, y );
        bt_wrf( m, self + 0x38, z );
        m.wr( m.user, self + 0x3C, 0 );
    }
    bt_copy16( m, self + 0x40, result + 0x20 );
    return m.rd( m.user, result + 0x30 );
}

// btClosestNotMeConvexResultCallback::addSingleResult 0x6027A0 (slot 2;
// convexResult*, normalInWorldSpace; retn 8): 1.0 (fld1) when the hit object
// is m_me (+0x60) or has CF_NO_CONTACT_RESPONSE (its +0xD8 bit 2); 1.0 when
// the hit normal (+0x10, used as given) . (m_convexToWorld +0x20 -
// m_convexFromWorld +0x10) = (ny dy + nx dx) + nz dz `comiss`-is >= -m_allowed-
// Penetration (+0x64, sign flipped by xorps); else 0x605FA0's answer.
inline std::uint32_t bt_closest_not_me_add_single_result( const bt_mem_t& m, std::uint32_t self, std::uint32_t result, std::uint32_t normal_in_world )
{
    const std::uint32_t object = m.rd( m.user, result );
    if ( object == m.rd( m.user, self + 0x60 ) )
        return 0x3F800000u;
    if ( ( ( m.rd( m.user, object + 0xD8 ) >> 2 ) & 1u ) != 0 )
        return 0x3F800000u;
    const float dy = bt_rdf( m, self + 0x24 ) - bt_rdf( m, self + 0x14 );
    const float dx = bt_rdf( m, self + 0x20 ) - bt_rdf( m, self + 0x10 );
    const float dz = bt_rdf( m, self + 0x28 ) - bt_rdf( m, self + 0x18 );
    const float dot = ( bt_rdf( m, result + 0x14 ) * dy + bt_rdf( m, result + 0x10 ) * dx ) + bt_rdf( m, result + 0x18 ) * dz;
    const std::uint32_t allowed = m.rd( m.user, self + 0x64 ) ^ 0x80000000u;
    float neg;
    std::memcpy( &neg, &allowed, 4 );
    if ( _mm_comige_ss( _mm_set_ss( dot ), _mm_set_ss( neg ) ) )
        return 0x3F800000u;
    return bt_closest_convex_add_single_result( m, self, result, normal_in_world );
}

// btClosestNotMeConvexResultCallback::needsCollision 0x602830 (slot 1;
// proxy; retn 4): false for m_me's (+0x60) own proxy (client +0), when the
// proxy's group (+4) misses this mask (+0xA) or its mask (+6) this group (+8)
// (16-bit tests); else the dispatcher's (+0x6C) vt+0x18 needsCollision( m_me,
// the client ) != 0.
inline bool bt_closest_not_me_needs_collision( const bt_mem_t& m, std::uint32_t self, std::uint32_t proxy )
{
    const std::uint32_t me = m.rd( m.user, self + 0x60 ), client = m.rd( m.user, proxy );
    if ( client == me )
        return false;
    const std::uint32_t pw = m.rd( m.user, proxy + 4 ), sw = m.rd( m.user, self + 8 );
    if ( ( ( sw >> 16 ) & pw & 0xFFFFu ) == 0 )
        return false;
    if ( ( ( pw >> 16 ) & sw & 0xFFFFu ) == 0 )
        return false;
    const std::uint32_t a[ 2 ] = { me, client };
    return ( m.vcall( m.user, m.rd( m.user, self + 0x6C ), 0x18, a, 2 ) & 0xFFu ) != 0;
}

// btCompoundShape::recalculateLocalAabb 0x527160 (slot 15; thiscall): +0x20
// m_localAabbMin = (1e18, 1e18, 1e18, 0) (0x6F7B10), +0x30 m_localAabbMax =
// (-1e18 x3, 0) (0x6F7C90); per child (count +0x10 and data +0x18 re-read;
// 0x50-byte records, the transform at +0, the shape at +0x40) the shape's
// vt+4 getAabb( transform, min*, max* ) and per lane x / y / z `comiss`:
// min = the child's when min > it, max = the child's when it > max (ordered).
inline void bt_compound_recalculate_local_aabb( const bt_io_t& io, std::uint32_t self )
{
    const bt_mem_t& m = *io.m;
    for ( std::uint32_t k = 0; k < 12; k += 4 )
    {
        m.wr( m.user, self + 0x20 + k, 0x5D5E0B6Bu );
        m.wr( m.user, self + 0x30 + k, 0xDD5E0B6Bu );
    }
    m.wr( m.user, self + 0x2C, 0 );
    m.wr( m.user, self + 0x3C, 0 );
    for ( std::int32_t i = 0; i < static_cast< std::int32_t >( m.rd( m.user, self + 0x10 ) ); ++i )
    {
        const std::uint32_t record = m.rd( m.user, self + 0x18 ) + 0x50u * static_cast< std::uint32_t >( i );
        std::uint32_t out[ 8 ] = { };
        io.vcall_out( m.user, m.rd( m.user, record + 0x40 ), 4, &record, 1, out, 8 );
        for ( std::uint32_t lane = 0; lane < 3; ++lane )
        {
            float lo, hi;
            std::memcpy( &lo, &out[ lane ], 4 );
            std::memcpy( &hi, &out[ 4 + lane ], 4 );
            if ( _mm_comigt_ss( _mm_set_ss( bt_rdf( m, self + 0x20 + 4 * lane ) ), _mm_set_ss( lo ) ) )
                bt_wrf( m, self + 0x20 + 4 * lane, lo );
            if ( _mm_comigt_ss( _mm_set_ss( hi ), _mm_set_ss( bt_rdf( m, self + 0x30 + 4 * lane ) ) ) )
                bt_wrf( m, self + 0x30 + 4 * lane, hi );
        }
    }
}

// btPolyhedralConvexShape::calculateLocalInertia 0x54B4C0 (slot 7 of the
// polyhedral / aabb-caching / convex-triangle-mesh shapes; thiscall; mass,
// inertia*; retn 8): margin = vt+0x28 getMargin (ST0, stored as a float);
// getAabb (vt+4) on an identity transform (0x413280's stores: the basis
// rows (1,0,0,0) (0,1,0,0) (0,0,1,0), origin 0); per lane L = ((max - min) *
// 0.5 (0x6F765C) + margin) * 2.0 (0x6F76F8), squared; k = mass * 0x6F7608
// (0x3DAAAAAA, 1/12); inertia = ((z2 + y2) k, (z2 + x2) k, (y2 + x2) k, 0).
// `margin` answers vt+0x28 (its float bits).
inline void bt_polyhedral_local_inertia( const bt_io_t& io, std::uint32_t self, std::uint32_t mass_bits, std::uint32_t inertia,
                                         std::uint32_t ( *margin )( const bt_mem_t& m, std::uint32_t self ) )
{
    const bt_mem_t& m = *io.m;
    const std::uint32_t mg = margin( m, self );
    float margin_f, mass;
    std::memcpy( &margin_f, &mg, 4 );
    std::memcpy( &mass, &mass_bits, 4 );
    const std::uint32_t identity[ 16 ] = { 0x3F800000u, 0, 0, 0, 0, 0x3F800000u, 0, 0, 0, 0, 0x3F800000u, 0, 0, 0, 0, 0 };
    std::uint32_t out[ 8 ] = { };   // min, max
    io.vcall_out( m.user, self, 4, identity, 16, out, 8 );
    float mn[ 4 ], mx[ 4 ];
    std::memcpy( mn, out, 16 );
    std::memcpy( mx, out + 4, 16 );
    float l[ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        const float h = ( mx[ k ] - mn[ k ] ) * 0.5f + margin_f;
        const float e = h * 2.0f;
        l[ k ] = e * e;
    }
    float scaled;
    const std::uint32_t twelfth = 0x3DAAAAAAu;
    std::memcpy( &scaled, &twelfth, 4 );
    scaled = mass * scaled;
    bt_wrf( m, inertia + 0, ( l[ 2 ] + l[ 1 ] ) * scaled );
    bt_wrf( m, inertia + 4, ( l[ 2 ] + l[ 0 ] ) * scaled );
    bt_wrf( m, inertia + 8, ( l[ 1 ] + l[ 0 ] ) * scaled );
    m.wr( m.user, inertia + 12, 0 );
}

// btTriangleShape (vertices at +0x50 / +0x60 / +0x70, 16 bytes each).
// The max-dot vertex index the support rows inline: d1 > d0 (`comiss`,
// ordered) -> 1 + (d2 > d1); else d2 > d0 ? 2 : 0.
inline std::uint32_t bt_triangle_max_vertex( float d0, float d1, float d2 )
{
    if ( _mm_comigt_ss( _mm_set_ss( d1 ), _mm_set_ss( d0 ) ) )
        return 1u + ( _mm_comigt_ss( _mm_set_ss( d2 ), _mm_set_ss( d1 ) ) ? 1u : 0u );
    return _mm_comigt_ss( _mm_set_ss( d2 ), _mm_set_ss( d0 ) ) ? 2u : 0u;
}

// localGetSupportingVertexWithoutMargin 0x5B7D60 (slot 15; out*, dir*; retn
// 8): the dots (v.y dy + v.x dx) + v.z dz per vertex; out = that vertex's 16
// bytes.
inline void bt_triangle_support( const bt_mem_t& m, std::uint32_t self, std::uint32_t out, std::uint32_t dir )
{
    const float dx = bt_rdf( m, dir ), dy = bt_rdf( m, dir + 4 ), dz = bt_rdf( m, dir + 8 );
    float d[ 3 ];
    for ( std::uint32_t k = 0; k < 3; ++k )
    {
        const std::uint32_t v = self + 0x50 + 0x10 * k;
        d[ k ] = ( bt_rdf( m, v + 4 ) * dy + bt_rdf( m, v ) * dx ) + bt_rdf( m, v + 8 ) * dz;
    }
    bt_copy16( m, out, self + 0x50 + 0x10 * bt_triangle_max_vertex( d[ 0 ], d[ 1 ], d[ 2 ] ) );
}

// batchedUnitVectorGetSupportingVertexWithoutMargin 0x5B7E20 (slot 17;
// dirs*, outs*, count; retn 0Ch): count <= 0 -> nothing; else per direction
// (16-byte strides) the same pick into the next output.
inline void bt_triangle_support_batch( const bt_mem_t& m, std::uint32_t self, std::uint32_t dirs, std::uint32_t outs, std::uint32_t count )
{
    for ( std::int32_t i = 0; i < static_cast< std::int32_t >( count ); ++i )
        bt_triangle_support( m, self, outs + 0x10u * static_cast< std::uint32_t >( i ), dirs + 0x10u * static_cast< std::uint32_t >( i ) );
}

// The forwarder 0x5B7FA0 (slot 27 getPlane; normal*, support*, i; retn 0Ch):
// this one's vt+0x74 getPlaneEquation( i, normal*, support* ); the forwarded
// call answers.
inline std::uint32_t bt_triangle_get_plane( const bt_mem_t& m, std::uint32_t self, const std::uint32_t* args )
{
    const std::uint32_t a[ 3 ] = { args[ 2 ], args[ 0 ], args[ 1 ] };
    return m.vcall( m.user, self, 0x74, a, 3 );
}

// calcNormal as 0x5B7FC0 / 0x5B83D0 inline it: e1 = v1 - v0, e2 = v2 - v0,
// n = (e1y e2z - e1z e2y, e1z e2x - e1x e2z, e1x e2y - e1y e2x, 0) stored,
// then (re-read) times 1.0 (0x6F76B8) / sqrt( (x x + y y) + z z ) - the
// three lanes stored again.
inline void bt_triangle_calc_normal( const bt_mem_t& m, std::uint32_t self, std::uint32_t out )
{
    const auto f = [ & ]( std::uint32_t at ) { return bt_rdf( m, self + at ); };
    const float e1y = f( 0x64 ) - f( 0x54 ), e1z = f( 0x68 ) - f( 0x58 ), e2z = f( 0x78 ) - f( 0x58 );
    const float e1x = f( 0x60 ) - f( 0x50 ), e2y = f( 0x74 ) - f( 0x54 ), e2x = f( 0x70 ) - f( 0x50 );
    bt_wrf( m, out, e1y * e2z - e1z * e2y );
    bt_wrf( m, out + 4, e1z * e2x - e1x * e2z );
    bt_wrf( m, out + 8, e1x * e2y - e1y * e2x );
    m.wr( m.user, out + 12, 0 );
    const float x = bt_rdf( m, out ), y = bt_rdf( m, out + 4 ), z = bt_rdf( m, out + 8 );
    const float r = 1.0f / _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( x * x + y * y ) + z * z ) ) );
    bt_wrf( m, out, x * r );
    bt_wrf( m, out + 4, y * r );
    bt_wrf( m, out + 8, z * r );
}

// getPlaneEquation 0x5B7FC0 (slot 29; i, normal*, support*; retn 0Ch):
// calcNormal into normal, support = v0 (+0x50, 16 bytes).
inline void bt_triangle_plane_equation( const bt_mem_t& m, std::uint32_t self, std::uint32_t normal, std::uint32_t support )
{
    bt_triangle_calc_normal( m, self, normal );
    bt_copy16( m, support, self + 0x50 );
}

// getPreferredPenetrationDirection 0x5B83D0 (slot 20; index, out*; retn 8):
// calcNormal into out; index != 0 -> the three lanes times -1.0 (0x6F78A0).
inline void bt_triangle_penetration_direction( const bt_mem_t& m, std::uint32_t self, std::uint32_t index, std::uint32_t out )
{
    bt_triangle_calc_normal( m, self, out );
    if ( index == 0 )
        return;
    for ( std::uint32_t k = 0; k < 12; k += 4 )
        bt_wrf( m, out + k, bt_rdf( m, out + k ) * -1.0f );
}
// btAlignedObjectArray's ctor inlined (init): m_ownsMemory (+16) = 1, m_data
// (+12) = 0, m_size (+4) = 0, m_capacity (+8) = 0.
inline void bt_array_init( const bt_mem_t& m, std::uint32_t array )
{
    m.wr8( m.user, array + 16, 1 );
    m.wr( m.user, array + 12, 0 );
    m.wr( m.user, array + 4, 0 );
    m.wr( m.user, array + 8, 0 );
}

// The scalar deleting dtor 0x5263F0 of the polyhedral rows (btConvexTriangle-
// MeshShape / btBoxShape / btPolyhedralConvex(AabbCaching)Shape /
// btTriangleShape slot 0; flags; retn 4): [this] = 0x74E074; the +0x40 block
// (m_polyhedron) btAlignedFree'd when set - no destructor call; [this] =
// 0x74CF98; with flags & 1 the btAlignedFree of this. eax = this.
inline std::uint32_t bt_polyhedral_shape_delete( const bt_mem_t& m, std::uint32_t self, std::uint32_t flags )
{
    m.wr( m.user, self, 0x74E074u );
    const std::uint32_t polyhedron = m.rd( m.user, self + 0x40 );
    if ( polyhedron != 0 )
        bt_aligned_free( m, polyhedron );
    m.wr( m.user, self, 0x74CF98u );
    if ( ( flags & 1u ) != 0 )
        bt_aligned_free( m, self );
    return self;
}

// CProfileNode::Reset 0x57B840 (thiscall): per node of the sibling chain
// (+0x1C) from this one: TotalCalls (+4) = 0, TotalTime (+8) = 0, then the
// child (+0x18) reset first (recursively).
inline void bt_profile_node_reset( const bt_mem_t& m, std::uint32_t node )
{
    do
    {
        m.wr( m.user, node + 4, 0 );
        m.wr( m.user, node + 8, 0 );
        const std::uint32_t child = m.rd( m.user, node + 0x18 );
        if ( child != 0 )
            bt_profile_node_reset( m, child );
        node = m.rd( m.user, node + 0x1C );
    } while ( node != 0 );
}

// btCollisionWorld::LocalConvexResult's ctor 0x552570 (thiscall; hitObject,
// localShapeInfo, normal*, point*, fraction; retn 14h): +0 / +4 the two
// words, +0x10 the normal, +0x20 the point (16 bytes each), +0x30 the
// fraction. eax = this.
inline std::uint32_t bt_local_convex_result_construct( const bt_mem_t& m, std::uint32_t self, const std::uint32_t* args )
{
    m.wr( m.user, self, args[ 0 ] );
    m.wr( m.user, self + 4, args[ 1 ] );
    bt_copy16( m, self + 0x10, args[ 2 ] );
    bt_copy16( m, self + 0x20, args[ 3 ] );
    m.wr( m.user, self + 0x30, args[ 4 ] );
    return self;
}

// btSimulationIslandManager's ctor 0x624520: [this] = 0x758330; the union
// find's m_elements (+4), m_islandmanifold (+0x18), m_islandBodies (+0x2C)
// initialised; m_splitIslands (+0x40) = 1. eax = this.
inline std::uint32_t bt_island_manager_construct( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x758330u );
    for ( std::uint32_t at : { 4u, 0x18u, 0x2Cu } )
        bt_array_init( m, self + at );
    m.wr8( m.user, self + 0x40, 1 );
    return self;
}

// btSequentialImpulseConstraintSolver's ctor 0x56FAD0: [this] = 0x74F8EC; the
// six pools (+4 m_tmpSolverBodyPool, +0x18 / +0x2C / +0x40 / +0x54 the
// contact / non-contact / friction / rolling constraint pools, +0x68
// m_orderTmpConstraintPool ... ) initialised; m_btSeed2 (+0x7C) = 0. eax =
// this.
inline std::uint32_t bt_sequential_solver_construct( const bt_mem_t& m, std::uint32_t self )
{
    m.wr( m.user, self, 0x74F8ECu );
    for ( std::uint32_t at : { 4u, 0x18u, 0x2Cu, 0x40u, 0x54u, 0x68u } )
        bt_array_init( m, self + at );
    m.wr( m.user, self + 0x7C, 0 );
    return self;
}

// btMatrix3x3's ctor from nine scalars 0x413340 (thiscall; nine float*; retn
// 24h): the rows (xx, xy, xz, 0) (yx, yy, yz, 0) (zx, zy, zz, 0). eax = this.
inline std::uint32_t bt_matrix3x3_construct( const bt_mem_t& m, std::uint32_t self, const std::uint32_t* args )
{
    for ( std::uint32_t row = 0; row < 3; ++row )
    {
        for ( std::uint32_t k = 0; k < 3; ++k )
            m.wr( m.user, self + 0x10 * row + 4 * k, m.rd( m.user, args[ 3 * row + k ] ) );
        m.wr( m.user, self + 0x10 * row + 12, 0 );
    }
    return self;
}

// btHashMap::findIndex 0x551E90 (thiscall; key*; retn 4): Thomas Wang's
// 32-bit hash of the key's first word (the shifts right are `sar`), masked
// by m_valueArray's capacity (+0x30) - 1; an index at or past m_hashTable's
// size (+4, unsigned) -> -1; else the chain from m_hashTable[index] (+0xC)
// through m_next (+0x20) until -1 or m_keyArray[i] (+0x48, 8-byte keys, the
// first word) equals the key.
inline std::uint32_t bt_hash_map_find_index( const bt_mem_t& m, std::uint32_t self, std::uint32_t key_ptr )
{
    const std::uint32_t key = m.rd( m.user, key_ptr );
    const auto sar = []( std::uint32_t v, int s ) { return static_cast< std::uint32_t >( static_cast< std::int32_t >( v ) >> s ); };
    std::uint32_t h = key + ~( key << 15 );
    h ^= sar( h, 10 );
    h = h * 9u;
    h ^= sar( h, 6 );
    h += ~( h << 11 );
    h ^= sar( h, 16 );
    const std::uint32_t index = h & ( m.rd( m.user, self + 0x30 ) - 1u );
    if ( index >= m.rd( m.user, self + 4 ) )
        return 0xFFFFFFFFu;
    std::uint32_t i = m.rd( m.user, m.rd( m.user, self + 0xC ) + 4u * index );
    while ( i != 0xFFFFFFFFu && key != m.rd( m.user, m.rd( m.user, self + 0x48 ) + 8u * i ) )
        i = m.rd( m.user, m.rd( m.user, self + 0x20 ) + 4u * i );
    return i;
}
// btBoxShape::getHalfExtentsWithMargin 0x548B20 (thiscall; out*; retn 4):
// out = m_implicitShapeDimensions (+0x20, 16 bytes); getMargin (vt+0x28)
// three times (ST0 stored as floats: the first for z, the second y, the
// third x); out.x += the third, out.y += the second, out.z += the first. eax
// = out. `margin` answers vt+0x28.
inline std::uint32_t bt_box_half_extents_with_margin( const bt_mem_t& m, std::uint32_t self, std::uint32_t out,
                                                      std::uint32_t ( *margin )( const bt_mem_t& m, std::uint32_t self ) )
{
    bt_copy16( m, out, self + 0x20 );
    std::uint32_t mb[ 3 ];
    for ( int k = 0; k < 3; ++k )
        mb[ k ] = margin( m, self );
    float mf[ 3 ];
    std::memcpy( mf, mb, 12 );
    bt_wrf( m, out, bt_rdf( m, out ) + mf[ 2 ] );
    bt_wrf( m, out + 4, bt_rdf( m, out + 4 ) + mf[ 1 ] );
    bt_wrf( m, out + 8, bt_rdf( m, out + 8 ) + mf[ 0 ] );
    return out;
}

// btCollisionDispatcher's ctor 0x56D1B0 (thiscall; collisionConfiguration;
// retn 4): [this] = 0x74F7C4; m_dispatcherFlags (+4) = 2; m_manifoldsPtr (+8)
// initialised; the btManifoldResult member's vtable (+0x20) = 0x74E700; the
// configuration at +0x151C; m_nearCallback (+0xD0) = defaultNearCallback
// 0x56D840; +0xD4 = the configuration's vt+8 getCollisionAlgorithmPool, +0xD8
// its vt+4 getPersistentManifoldPool; then for i, j < 36 (MAX_BROADPHASE_
// COLLISION_TYPES) m_doubleDispatch[i][j] (+0xDC, row-major) = its vt+0x10
// getCollisionAlgorithmCreateFunc( i, j ) (the configuration re-read). eax =
// this.
inline std::uint32_t bt_collision_dispatcher_construct( const bt_mem_t& m, std::uint32_t self, std::uint32_t config )
{
    m.wr( m.user, self, 0x74F7C4u );
    m.wr( m.user, self + 4, 2 );
    m.wr8( m.user, self + 0x18, 1 );
    m.wr( m.user, self + 0x14, 0 );
    m.wr( m.user, self + 0xC, 0 );
    m.wr( m.user, self + 0x10, 0 );
    m.wr( m.user, self + 0x20, 0x74E700u );
    m.wr( m.user, self + 0x151C, config );
    m.wr( m.user, self + 0xD0, 0x56D840u );
    m.wr( m.user, self + 0xD4, m.vcall( m.user, config, 8, nullptr, 0 ) );
    m.wr( m.user, self + 0xD8, m.vcall( m.user, config, 4, nullptr, 0 ) );
    std::uint32_t at = self + 0xDC;
    for ( std::uint32_t i = 0; i < 36; ++i )
        for ( std::uint32_t j = 0; j < 36; ++j )
        {
            const std::uint32_t a[ 2 ] = { i, j };
            m.wr( m.user, at, m.vcall( m.user, m.rd( m.user, self + 0x151C ), 0x10, a, 2 ) );
            at += 4;
        }
    return self;
}
// --- Serialization (btSerializer virtuals: vt+0xC allocate( size, count ),
// vt+0x10 finalizeChunk( chunk, structType, code, oldPtr ), vt+0x18
// getUniquePointer, vt+0x1C / +0x20 start / finishSerialization, vt+0x24
// findNameForPointer, vt+0x2C serializeName). Race-reach rows the running
// race never calls (nothing serializes a world in a race).

// The name prologue btCollisionShape::serialize 0x554620 and the convex
// rows inline: name = findNameForPointer( this ); buffer +0 = getUnique-
// Pointer( name ); a non-null answer -> serializeName( name ); +4 =
// m_shapeType (+4).
inline void bt_shape_serialize_name( const bt_mem_t& m, std::uint32_t self, std::uint32_t buffer, std::uint32_t serializer )
{
    const std::uint32_t name = m.vcall( m.user, serializer, 0x24, &self, 1 );
    const std::uint32_t unique = m.vcall( m.user, serializer, 0x18, &name, 1 );
    m.wr( m.user, buffer, unique );
    if ( unique != 0 )
        m.vcall( m.user, serializer, 0x2C, &name, 1 );
    m.wr( m.user, buffer + 4, m.rd( m.user, self + 4 ) );
}

// btCollisionShape::serialize 0x554620 (slot 12; buffer, serializer; retn 8):
// the prologue; answers "btCollisionShapeData" (0x6D2CAC).
inline std::uint32_t bt_shape_serialize( const bt_mem_t& m, std::uint32_t self, std::uint32_t buffer, std::uint32_t serializer )
{
    bt_shape_serialize_name( m, self, buffer, serializer );
    return 0x6D2CACu;
}

// btConvexInternalShape::serialize 0x43D680 (slot 12): the prologue, then
// m_implicitShapeDimensions (+0x20..+0x2C) to +0x1C..+0x28, m_localScaling
// (+0x10..+0x1C) to +0xC..+0x18, m_collisionMargin (+0x30) to +0x2C; answers
// "btConvexInternalShapeData" (0x6F36B8). btCylinderShape's 0x5AEAC0 adds
// m_upAxis (+0x40) to +0x34 and answers "btCylinderShapeData" (0x6F6494).
inline std::uint32_t bt_convex_internal_serialize( const bt_mem_t& m, std::uint32_t self, std::uint32_t buffer, std::uint32_t serializer, bool cylinder )
{
    bt_shape_serialize_name( m, self, buffer, serializer );
    for ( std::uint32_t k = 0; k < 16; k += 4 )
        m.wr( m.user, buffer + 0x1C + k, m.rd( m.user, self + 0x20 + k ) );
    for ( std::uint32_t k = 0; k < 16; k += 4 )
        m.wr( m.user, buffer + 0xC + k, m.rd( m.user, self + 0x10 + k ) );
    m.wr( m.user, buffer + 0x2C, m.rd( m.user, self + 0x30 ) );
    if ( !cylinder )
        return 0x6F36B8u;
    m.wr( m.user, buffer + 0x34, m.rd( m.user, self + 0x40 ) );
    return 0x6F6494u;
}

// One chunk: size = object's vt+`size_slot`, chunk = allocate( size, 1 ),
// structType = object's vt+`serialize_slot`( chunk->m_oldPtr (+8),
// serializer ), finalizeChunk( chunk, structType, code, object ).
inline void bt_serialize_chunk( const bt_mem_t& m, std::uint32_t object, std::uint32_t serializer, std::uint32_t size_slot, std::uint32_t serialize_slot,
                                std::uint32_t code )
{
    const std::uint32_t size = m.vcall( m.user, object, size_slot, nullptr, 0 );
    const std::uint32_t a[ 2 ] = { size, 1 };
    const std::uint32_t chunk = m.vcall( m.user, serializer, 0xC, a, 2 );
    const std::uint32_t b[ 2 ] = { m.rd( m.user, chunk + 8 ), serializer };
    const std::uint32_t type = m.vcall( m.user, object, serialize_slot, b, 2 );
    const std::uint32_t c[ 4 ] = { chunk, type, code, object };
    m.vcall( m.user, serializer, 0x10, c, 4 );
}

// btCollisionShape::serializeSingleShape 0x554670 (slot 13; serializer;
// retn 4): one 'SHAP' (0x50414853) chunk of this (vt+0x2C / vt+0x30).
inline void bt_shape_serialize_single( const bt_mem_t& m, std::uint32_t self, std::uint32_t serializer )
{
    bt_serialize_chunk( m, self, serializer, 0x2C, 0x30, 0x50414853u );
}

// btBvhTriangleMeshShape::serializeSingleBvh 0x5AF5F0 (slot 17) / serialize-
// SingleTriangleInfoMap 0x5AF640 (slot 18) (serializer; retn 4): with the
// bvh (+0x40) a 'QBVH' (0x48564251) chunk of it (vt+0xC / vt+4); with the
// triangle info map (+0x44) a 'TMAP' (0x50414D54) one (vt+4 / vt+8).
inline void bt_bvh_serialize_single_bvh( const bt_mem_t& m, std::uint32_t self, std::uint32_t serializer )
{
    const std::uint32_t bvh = m.rd( m.user, self + 0x40 );
    if ( bvh != 0 )
        bt_serialize_chunk( m, bvh, serializer, 0xC, 4, 0x48564251u );
}

inline void bt_bvh_serialize_single_info_map( const bt_mem_t& m, std::uint32_t self, std::uint32_t serializer )
{
    const std::uint32_t map = m.rd( m.user, self + 0x44 );
    if ( map != 0 )
        bt_serialize_chunk( m, map, serializer, 4, 8, 0x50414D54u );
}

// btDiscreteDynamicsWorld::serializeRigidBodies 0x605C90 (serializer; retn
// 4): each collision object (+8 count, +0x10 data, both re-read) whose
// internal type (+0xF4) has bit 1 -> an 'RBDY' (0x59444252) chunk (vt+0xC /
// vt+0x10); then each constraint (+0xB8 count, +0xC0 data) a 'CONS'
// (0x534E4F43) chunk (vt+0x20 / vt+0x24).
inline void bt_world_serialize_rigid_bodies( const bt_mem_t& m, std::uint32_t self, std::uint32_t serializer )
{
    for ( std::int32_t i = 0; i < static_cast< std::int32_t >( m.rd( m.user, self + 8 ) ); ++i )
    {
        const std::uint32_t object = m.rd( m.user, m.rd( m.user, self + 0x10 ) + 4u * static_cast< std::uint32_t >( i ) );
        if ( ( m.rd8( m.user, object + 0xF4 ) & 2u ) != 0 )
            bt_serialize_chunk( m, object, serializer, 0xC, 0x10, 0x59444252u );
    }
    for ( std::int32_t i = 0; i < static_cast< std::int32_t >( m.rd( m.user, self + 0xB8 ) ); ++i )
        bt_serialize_chunk( m, m.rd( m.user, m.rd( m.user, self + 0xC0 ) + 4u * static_cast< std::uint32_t >( i ) ), serializer, 0x20, 0x24, 0x534E4F43u );
}