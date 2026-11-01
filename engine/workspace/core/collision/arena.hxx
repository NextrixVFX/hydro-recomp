#pragma once

#include <workspace/core/collision/teardown.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>
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
    // Words and _mm_loadu_si128, not a float[ 4 ] filled per lane from m.rd and read by _mm_loadu_ps: MSVC 14.51 /O2's
    // SSA optimizer keeps only the LAST call's result for all four lanes of that shape (boat batch 20, tools
    // tmp/b20/miscompile: defined code, correct at /O1 and with /d2SSAOptimizer-).
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
inline std::uint32_t bt_hash_wang( std::uint32_t key )
{
    const auto sar = []( std::uint32_t v, int s ) { return static_cast< std::uint32_t >( static_cast< std::int32_t >( v ) >> s ); };
    std::uint32_t h = key + ~( key << 15 );
    h ^= sar( h, 10 );
    h = h * 9u;
    h ^= sar( h, 6 );
    h += ~( h << 11 );
    h ^= sar( h, 16 );
    return h;
}

// findIndex on the key's word (0x551E90's body; 0x5516E0 inlines it).
inline std::uint32_t bt_hash_map_find( const bt_mem_t& m, std::uint32_t self, std::uint32_t key )
{
    const std::uint32_t index = bt_hash_wang( key ) & ( m.rd( m.user, self + 0x30 ) - 1u );
    if ( index >= m.rd( m.user, self + 4 ) )
        return 0xFFFFFFFFu;
    std::uint32_t i = m.rd( m.user, m.rd( m.user, self + 0xC ) + 4u * index );
    while ( i != 0xFFFFFFFFu && key != m.rd( m.user, m.rd( m.user, self + 0x48 ) + 8u * i ) )
        i = m.rd( m.user, m.rd( m.user, self + 0x20 ) + 4u * i );
    return i;
}

inline std::uint32_t bt_hash_map_find_index( const bt_mem_t& m, std::uint32_t self, std::uint32_t key_ptr )
{
    return bt_hash_map_find( m, self, m.rd( m.user, key_ptr ) );
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
// --- btHashMap<btHashPtr, btCollisionShape*> (boat batch 21; 0x50 bytes, Bullet
// 2.79): four btAlignedObjectArrays - +0x00 m_hashTable (int), +0x14 m_next
// (int), +0x28 m_valueArray (pointers), +0x3C m_keyArray (8-byte btHashPtr: the
// pointer, then the union's second word). The hash is bt_hash_wang masked by
// m_valueArray's capacity (+0x30) - 1.

// btAlignedObjectArray::reserve inlined (the map's arrays): a block of count x
// `stride` bytes (btAlignedAlloc( size, 16 ); none for 0), the m_size elements
// copied (each only to a non-null destination - placement new), the old block
// freed when owned (then m_data = 0), m_ownsMemory = 1, m_data, m_capacity.
inline void bt_array_reserve( const bt_mem_t& m, std::uint32_t array, std::uint32_t count, std::uint32_t stride )
{
    const std::uint32_t block = count != 0 ? bt_aligned_alloc( m, count * stride, 16 ) : 0u;
    const std::int32_t size = static_cast< std::int32_t >( m.rd( m.user, array + 4 ) );
    for ( std::int32_t i = 0; i < size; ++i )
    {
        const std::uint32_t to = block + stride * static_cast< std::uint32_t >( i );
        if ( to == 0 )
            continue;
        const std::uint32_t from = m.rd( m.user, array + 12 ) + stride * static_cast< std::uint32_t >( i );
        for ( std::uint32_t k = 0; k < stride; k += 4 )
            m.wr( m.user, to + k, m.rd( m.user, from + k ) );
    }
    const std::uint32_t data = m.rd( m.user, array + 12 );
    if ( data != 0 )
    {
        if ( m.rd8( m.user, array + 16 ) != 0 )
            bt_aligned_free( m, data );
        m.wr( m.user, array + 12, 0 );
    }
    m.wr8( m.user, array + 16, 1 );
    m.wr( m.user, array + 12, block );
    m.wr( m.user, array + 8, count );
}

// push_back inlined: at size == capacity (both read first) a reserve of
// size ? 2 x size : 1 when the capacity is (signed) below it; the element
// (`words` words read from `from`) to data + stride x size when that is
// non-null; ++size.
inline void bt_array_push( const bt_mem_t& m, std::uint32_t array, std::uint32_t stride, std::uint32_t from )
{
    const std::int32_t size = static_cast< std::int32_t >( m.rd( m.user, array + 4 ) );
    const std::int32_t capacity = static_cast< std::int32_t >( m.rd( m.user, array + 8 ) );
    if ( size == capacity )
    {
        const std::int32_t grown = size != 0 ? size * 2 : 1;
        if ( capacity < grown )
            bt_array_reserve( m, array, static_cast< std::uint32_t >( grown ), stride );
    }
    const std::uint32_t to = m.rd( m.user, array + 12 ) + stride * m.rd( m.user, array + 4 );
    if ( to != 0 )
        for ( std::uint32_t k = 0; k < stride; k += 4 )
            m.wr( m.user, to + k, m.rd( m.user, from + k ) );
    m.wr( m.user, array + 4, m.rd( m.user, array + 4 ) + 1u );
}

// resize( n ) of an int array inlined (growTables): n (signed) below m_size
// -> only m_size = n; above it a reserve( n ) when the capacity is below n,
// then the new elements [m_size, n) = 0 (non-null destinations); m_size = n.
inline void bt_int_array_resize( const bt_mem_t& m, std::uint32_t array, std::int32_t n )
{
    const std::int32_t size = static_cast< std::int32_t >( m.rd( m.user, array + 4 ) );
    if ( n >= size )
    {
        if ( n > size && static_cast< std::int32_t >( m.rd( m.user, array + 8 ) ) < n )
            bt_array_reserve( m, array, static_cast< std::uint32_t >( n ), 4 );
        for ( std::int32_t i = size; i < n; ++i )
        {
            const std::uint32_t to = m.rd( m.user, array + 12 ) + 4u * static_cast< std::uint32_t >( i );
            if ( to != 0 )
                m.wr( m.user, to, 0 );
        }
    }
    m.wr( m.user, array + 4, static_cast< std::uint32_t >( n ) );
}

// btHashMap::growTables 0x551C70 (thiscall; key* - unused; retn 4): newSize =
// m_valueArray's capacity, curHashtableSize = m_hashTable's size; only when
// cur < newSize: both int arrays resized to newSize, every m_hashTable /
// m_next entry -1, then for i < cur (the OLD hashtable size, as retail) key i
// relinked: m_next[i] = m_hashTable[h], m_hashTable[h] = i.
inline void bt_hash_map_grow_tables( const bt_mem_t& m, std::uint32_t self )
{
    const std::int32_t n = static_cast< std::int32_t >( m.rd( m.user, self + 0x30 ) );
    const std::int32_t cur = static_cast< std::int32_t >( m.rd( m.user, self + 4 ) );
    if ( cur >= n )
        return;
    bt_int_array_resize( m, self, n );
    bt_int_array_resize( m, self + 0x14, n );
    for ( std::int32_t i = 0; i < n; ++i )
        m.wr( m.user, m.rd( m.user, self + 0xC ) + 4u * static_cast< std::uint32_t >( i ), 0xFFFFFFFFu );
    for ( std::int32_t i = 0; i < n; ++i )
        m.wr( m.user, m.rd( m.user, self + 0x20 ) + 4u * static_cast< std::uint32_t >( i ), 0xFFFFFFFFu );
    for ( std::int32_t i = 0; i < cur; ++i )
    {
        const std::uint32_t key = m.rd( m.user, m.rd( m.user, self + 0x48 ) + 8u * static_cast< std::uint32_t >( i ) );
        const std::uint32_t h = bt_hash_wang( key ) & ( m.rd( m.user, self + 0x30 ) - 1u );
        m.wr( m.user, m.rd( m.user, self + 0x20 ) + 4u * static_cast< std::uint32_t >( i ), m.rd( m.user, m.rd( m.user, self + 0xC ) + 4u * h ) );
        m.wr( m.user, m.rd( m.user, self + 0xC ) + 4u * h, static_cast< std::uint32_t >( i ) );
    }
}

// btHashMap::insert 0x551A30 (thiscall; key*, value*; retn 8): the hash under
// the capacity at entry kept; a key found (findIndex 0x551E90) -> its value
// replaced; else value and key pushed (m_valueArray, then the 8-byte key),
// growTables + the hash again when the value array's capacity grew (signed),
// and the new index linked at the head of its bucket. The key / value are
// re-read through their pointers where retail re-reads them.
inline void bt_hash_map_insert( const bt_mem_t& m, std::uint32_t self, std::uint32_t key_ptr, std::uint32_t value_ptr )
{
    const std::uint32_t old_capacity = m.rd( m.user, self + 0x30 );
    std::uint32_t h = bt_hash_wang( m.rd( m.user, key_ptr ) ) & ( old_capacity - 1u );
    const std::uint32_t index = bt_hash_map_find_index( m, self, key_ptr );
    if ( index != 0xFFFFFFFFu )
    {
        m.wr( m.user, m.rd( m.user, self + 0x34 ) + 4u * index, m.rd( m.user, value_ptr ) );
        return;
    }
    const std::uint32_t count = m.rd( m.user, self + 0x2C );
    bt_array_push( m, self + 0x28, 4, value_ptr );
    bt_array_push( m, self + 0x3C, 8, key_ptr );
    if ( static_cast< std::int32_t >( old_capacity ) < static_cast< std::int32_t >( m.rd( m.user, self + 0x30 ) ) )
    {
        bt_hash_map_grow_tables( m, self );
        h = bt_hash_wang( m.rd( m.user, key_ptr ) ) & ( m.rd( m.user, self + 0x30 ) - 1u );
    }
    m.wr( m.user, m.rd( m.user, self + 0x20 ) + 4u * count, m.rd( m.user, m.rd( m.user, self + 0xC ) + 4u * h ) );
    m.wr( m.user, m.rd( m.user, self + 0xC ) + 4u * h, count );
}

// btHashMap's dtor 0x5518B0: the four arrays cleared, keys, values, next,
// hashtable (bt_array_clear each).
inline void bt_hash_map_destroy( const bt_mem_t& m, std::uint32_t self )
{
    bt_array_clear( m, self + 0x3C );
    bt_array_clear( m, self + 0x28 );
    bt_array_clear( m, self + 0x14 );
    bt_array_clear( m, self );
}

// btCollisionWorld::serializeCollisionObjects 0x5516E0 (serializer; retn 4):
// each collision object (+8 count, +0x10 data, re-read) whose internal type
// (+0xF4, a dword) is 1 -> its vt+0x14 serializeSingleObject( serializer );
// then a local btHashMap of the shapes already done: each object's shape
// (+0xCC) not found (findIndex inlined: found = index != -1 and &values[index]
// non-null) -> insert( shape, shape ) and the shape's vt+0x34
// serializeSingleShape( serializer ); the map's dtor. `frame` = 0x5C bytes
// standing in for retail's stack frame: the map at +0, the key at +0x50 (its
// second word is never written - the caller's stale word, copied into the key
// array as retail copies its stack), the value at +0x58.
inline void bt_collision_world_serialize_objects( const bt_mem_t& m, std::uint32_t self, std::uint32_t serializer, std::uint32_t frame )
{
    for ( std::int32_t i = 0; i < static_cast< std::int32_t >( m.rd( m.user, self + 8 ) ); ++i )
    {
        const std::uint32_t object = m.rd( m.user, m.rd( m.user, self + 0x10 ) + 4u * static_cast< std::uint32_t >( i ) );
        if ( m.rd( m.user, object + 0xF4 ) == 1u )
            m.vcall( m.user, object, 0x14, &serializer, 1 );
    }
    const std::uint32_t map = frame, key = frame + 0x50, value = frame + 0x58;
    for ( std::uint32_t a = 0; a < 0x50; a += 0x14 )
    {
        m.wr8( m.user, map + a + 16, 1 );
        m.wr( m.user, map + a + 12, 0 );
        m.wr( m.user, map + a + 4, 0 );
        m.wr( m.user, map + a + 8, 0 );
    }
    for ( std::int32_t i = 0; i < static_cast< std::int32_t >( m.rd( m.user, self + 8 ) ); ++i )
    {
        const std::uint32_t object = m.rd( m.user, m.rd( m.user, self + 0x10 ) + 4u * static_cast< std::uint32_t >( i ) );
        const std::uint32_t shape = m.rd( m.user, object + 0xCC );
        m.wr( m.user, value, shape );
        const std::uint32_t index = bt_hash_map_find( m, map, shape );
        if ( index != 0xFFFFFFFFu && m.rd( m.user, map + 0x34 ) + 4u * index != 0 )
            continue;
        m.wr( m.user, key, shape );
        bt_hash_map_insert( m, map, key, value );
        m.vcall( m.user, shape, 0x34, &serializer, 1 );
    }
    bt_hash_map_destroy( m, map );
}

// btCollisionWorld::serialize 0x551A00 (slot 10; serializer; retn 4): the
// serializer's vt+0x1C startSerialization, serializeCollisionObjects, vt+0x20
// finishSerialization. btDiscreteDynamicsWorld::serialize 0x605D60 (slot 10)
// adds serializeRigidBodies 0x605C90 before the objects.
inline void bt_collision_world_serialize( const bt_mem_t& m, std::uint32_t self, std::uint32_t serializer, std::uint32_t frame, bool dynamics )
{
    m.vcall( m.user, serializer, 0x1C, nullptr, 0 );
    if ( dynamics )
        bt_world_serialize_rigid_bodies( m, self, serializer );
    bt_collision_world_serialize_objects( m, self, serializer, frame );
    m.vcall( m.user, serializer, 0x20, nullptr, 0 );
}
// btDbvt::collideTV 0x56F220 (static; ecx = root - the callers test it, it
// does not; volume*, policy*; retn 8) with btDbvtBroadphase's
// BroadphaseAabbTester policy (one word, the callback: Process(leaf) = its
// vt+4 process( leaf->data (+0x24) )): the volume copied (min +0, max +0x10,
// four lanes each), a btAlignedObjectArray stack reserved to 64 (btAlignedAlloc
// 0x100), the root pushed; until empty: pop; the node (min +0, max +0x10)
// intersects when no lane 0..2 has volume.max < node.min or node.max <
// volume.min (cmpltps: a NaN lane intersects); an internal node (childs[1]
// +0x28 non-zero) pushes childs[0] (+0x24) then childs[1], a leaf calls the
// policy. A push at size == capacity grows to 2 x size (1 at 0): a new
// block, the size entries copied, the old block freed (owned: no test). The
// stack's block freed at the end.
inline void bt_dbvt_collide_tv( const bt_mem_t& m, std::uint32_t root, std::uint32_t volume, std::uint32_t policy )
{
    float v[ 8 ];
    for ( int k = 0; k < 8; ++k )
        v[ k ] = bt_rdf( m, volume + 4u * static_cast< std::uint32_t >( k ) );
    std::uint32_t data = bt_aligned_alloc( m, 0x100, 16 );
    std::int32_t capacity = 0x40;
    if ( data != 0 )
        m.wr( m.user, data, root );
    std::int32_t size = 1;
    const auto push = [ & ]( std::uint32_t node ) {
        if ( size == capacity )
        {
            const std::int32_t grown = size != 0 ? size * 2 : 1;
            if ( capacity < grown )
            {
                const std::uint32_t block = grown != 0 ? bt_aligned_alloc( m, 4u * static_cast< std::uint32_t >( grown ), 16 ) : 0u;
                for ( std::int32_t i = 0; i < size; ++i )
                    if ( block + 4u * static_cast< std::uint32_t >( i ) != 0 )
                        m.wr( m.user, block + 4u * static_cast< std::uint32_t >( i ), m.rd( m.user, data + 4u * static_cast< std::uint32_t >( i ) ) );
                if ( data != 0 )
                    bt_aligned_free( m, data );
                data = block;
                capacity = grown;
            }
        }
        if ( data + 4u * static_cast< std::uint32_t >( size ) != 0 )
            m.wr( m.user, data + 4u * static_cast< std::uint32_t >( size ), node );
        ++size;
    };
    do
    {
        const std::uint32_t n = m.rd( m.user, data + 4u * static_cast< std::uint32_t >( size - 1 ) );
        --size;
        bool apart = false;
        for ( std::uint32_t k = 0; k < 3; ++k )
            apart = apart || v[ 4 + k ] < bt_rdf( m, n + 4 * k ) || bt_rdf( m, n + 0x10 + 4 * k ) < v[ k ];
        if ( apart )
            continue;
        if ( m.rd( m.user, n + 0x28 ) != 0 )
        {
            push( m.rd( m.user, n + 0x24 ) );
            push( m.rd( m.user, n + 0x28 ) );
        }
        else
        {
            const std::uint32_t leaf = m.rd( m.user, n + 0x24 );
            m.vcall( m.user, m.rd( m.user, policy ), 4, &leaf, 1 );
        }
    } while ( size > 0 );
    if ( data != 0 )
        bt_aligned_free( m, data );
}

// btDbvtBroadphase::aabbTest 0x56E0D0 (slot 6; aabbMin*, aabbMax*, callback;
// retn 0Ch): the policy (the callback) and the volume from the two points
// (four lanes each) on its stack, then collideTV over m_sets[0] (root +4)
// and m_sets[1] (root +0x2C) where the root is set. `frame` = 0x24 bytes
// standing in for that stack: the policy word at +0, the volume at +4.
inline void bt_dbvt_broadphase_aabb_test( const bt_mem_t& m, std::uint32_t self, std::uint32_t aabb_min, std::uint32_t aabb_max, std::uint32_t callback,
                                          std::uint32_t frame )
{
    const std::uint32_t volume = frame + 4;
    m.wr( m.user, frame, callback );
    for ( std::uint32_t k = 0; k < 16; k += 4 )
    {
        m.wr( m.user, volume + k, m.rd( m.user, aabb_min + k ) );
        m.wr( m.user, volume + 0x10 + k, m.rd( m.user, aabb_max + k ) );
    }
    if ( const std::uint32_t root = m.rd( m.user, self + 4 ) )
        bt_dbvt_collide_tv( m, root, volume, frame );
    if ( const std::uint32_t root = m.rd( m.user, self + 0x2C ) )
        bt_dbvt_collide_tv( m, root, volume, frame );
}

// btPolyhedralConvexShape::localGetSupportingVertexWithoutMargin 0x54B230
// (slot; out*, dir*; retn 8; eax = out): out = 0; maxDot = -9.99999984e17
// (0xDD5E0B6B); the direction's length squared (y y + x x) + z z under
// 9.99999975e-05 (ordered) -> (1, 0, 0), else the direction x 1 / sqrt; then
// for each vertex (getNumVertices vt+0x58, re-asked each step; getVertex
// vt+0x64( i, v* ) through io) the dot (v.y d.y + v.x d.x) + v.z d.z above
// maxDot (ordered) -> out = v (four lanes), maxDot = dot.
inline std::uint32_t bt_polyhedral_local_support( const bt_io_t& io, std::uint32_t self, std::uint32_t out, std::uint32_t dir )
{
    const bt_mem_t& m = *io.m;
    for ( std::uint32_t k = 0; k < 16; k += 4 )
        m.wr( m.user, out + k, 0 );
    float max_dot;
    const std::uint32_t lowest = 0xDD5E0B6Bu;
    std::memcpy( &max_dot, &lowest, 4 );
    const float x = bt_rdf( m, dir ), y = bt_rdf( m, dir + 4 ), z = bt_rdf( m, dir + 8 );
    const float len2 = ( y * y + x * x ) + z * z;
    float dx = 1.0f, dy = 0.0f, dz = 0.0f;
    if ( !( 9.99999975e-05f > len2 ) )
    {
        const float r = 1.0f / std::sqrt( len2 );
        dx = x * r;
        dy = y * r;
        dz = z * r;
    }
    for ( std::int32_t i = 0; i < static_cast< std::int32_t >( m.vcall( m.user, self, 0x58, nullptr, 0 ) ); ++i )
    {
        const std::uint32_t a = static_cast< std::uint32_t >( i );
        std::uint32_t v[ 4 ] = { };
        io.vcall_out( m.user, self, 0x64, &a, 1, v, 4 );
        float f[ 3 ];
        std::memcpy( f, v, 12 );
        const float dot = ( f[ 1 ] * dy + f[ 0 ] * dx ) + f[ 2 ] * dz;
        if ( dot > max_dot )
        {
            for ( std::uint32_t k = 0; k < 4; ++k )
                m.wr( m.user, out + 4 * k, v[ k ] );
            max_dot = dot;
        }
    }
    return out;
}

// btPolyhedralConvexShape::batchedUnitVectorGetSupportingVertexWithoutMargin
// 0x54B3C0 (dirs*, outs*, n; retn 0Ch): each out's w = -9.99999984e17; then
// per direction j, per vertex (vt+0x58 re-asked, vt+0x64 through io) the dot
// (d.x v.x + d.y v.y) + d.z v.z above out[ j ].w (ordered) -> out[ j ] = v
// (four lanes), then its w = the dot.
inline void bt_polyhedral_batched_support( const bt_io_t& io, std::uint32_t self, std::uint32_t dirs, std::uint32_t outs, std::int32_t n )
{
    const bt_mem_t& m = *io.m;
    for ( std::int32_t j = 0; j < n; ++j )
        m.wr( m.user, outs + 16u * static_cast< std::uint32_t >( j ) + 0xC, 0xDD5E0B6Bu );
    for ( std::int32_t j = 0; j < n; ++j )
    {
        const std::uint32_t d = dirs + 16u * static_cast< std::uint32_t >( j ), o = outs + 16u * static_cast< std::uint32_t >( j );
        for ( std::int32_t i = 0; i < static_cast< std::int32_t >( m.vcall( m.user, self, 0x58, nullptr, 0 ) ); ++i )
        {
            const std::uint32_t a = static_cast< std::uint32_t >( i );
            std::uint32_t v[ 4 ] = { };
            io.vcall_out( m.user, self, 0x64, &a, 1, v, 4 );
            float f[ 3 ];
            std::memcpy( f, v, 12 );
            const float dot = ( bt_rdf( m, d ) * f[ 0 ] + bt_rdf( m, d + 4 ) * f[ 1 ] ) + bt_rdf( m, d + 8 ) * f[ 2 ];
            if ( dot > bt_rdf( m, o + 0xC ) )
            {
                for ( std::uint32_t k = 0; k < 4; ++k )
                    m.wr( m.user, o + 4 * k, v[ k ] );
                bt_wrf( m, o + 0xC, dot );
            }
        }
    }
}

// btTriangleShape::isInside 0x5B8110 (slot 28; point*, tolerance; retn 8; al):
// the normal (e1 x e2 of the vertices +0x50 / +0x60 / +0x70) normalised (1 /
// sqrt), the point's plane distance within [-tol, tol] (both ordered: a NaN
// is outside), then for each edge (getEdge vt+0x60( i, pa*, pb* ) through io)
// its inward normal (edge x normal, normalised) - the point's distance below
// -tol (ordered) -> false. Products / sums in retail's grouping.
inline bool bt_triangle_is_inside( const bt_io_t& io, std::uint32_t self, std::uint32_t point, std::uint32_t tolerance_bits )
{
    const bt_mem_t& m = *io.m;
    const auto v = [ & ]( std::uint32_t k, std::uint32_t lane ) { return bt_rdf( m, self + 0x50 + 0x10 * k + 4 * lane ); };
    const float e1y = v( 1, 1 ) - v( 0, 1 ), e1z = v( 1, 2 ) - v( 0, 2 ), e2z = v( 2, 2 ) - v( 0, 2 );
    const float e2y = v( 2, 1 ) - v( 0, 1 ), e1x = v( 1, 0 ) - v( 0, 0 ), e2x = v( 2, 0 ) - v( 0, 0 );
    const float nx = e1y * e2z - e1z * e2y, ny = e1z * e2x - e1x * e2z, nz = e1x * e2y - e1y * e2x;
    const float r = 1.0f / std::sqrt( ( ny * ny + nx * nx ) + nz * nz );
    const float ux = nx * r, uy = ny * r, uz = nz * r;
    const float px = bt_rdf( m, point ), py = bt_rdf( m, point + 4 ), pz = bt_rdf( m, point + 8 );
    const float dist = ( ( uy * py + px * ux ) + uz * pz ) - ( ( v( 0, 1 ) * uy + v( 0, 0 ) * ux ) + v( 0, 2 ) * uz );
    float tol, neg;
    const std::uint32_t neg_bits = tolerance_bits ^ 0x80000000u;
    std::memcpy( &tol, &tolerance_bits, 4 );
    std::memcpy( &neg, &neg_bits, 4 );
    if ( !( dist >= neg ) || !( tol >= dist ) )
        return false;
    for ( std::uint32_t i = 0; i < 3; ++i )
    {
        std::uint32_t w[ 8 ] = { };
        io.vcall_out( m.user, self, 0x60, &i, 1, w, 8 );
        float pa[ 3 ], pb[ 3 ];
        std::memcpy( pa, w, 12 );
        std::memcpy( pb, w + 4, 12 );
        const float edz = pb[ 2 ] - pa[ 2 ], edy = pb[ 1 ] - pa[ 1 ], edx = pb[ 0 ] - pa[ 0 ];
        const float enx = edy * uz - edz * uy, eny = edz * ux - edx * uz, enz = edx * uy - edy * ux;
        const float s = 1.0f / std::sqrt( ( eny * eny + enx * enx ) + enz * enz );
        const float qx = enx * s, qy = eny * s, qz = enz * s;
        const float edist = ( ( qy * py + px * qx ) + qz * pz ) - ( ( qy * pa[ 1 ] + qx * pa[ 0 ] ) + qz * pa[ 2 ] );
        if ( neg > edist )
            return false;
    }
    return true;
}

// btConvexPlaneCollisionAlgorithm::CreateFunc::CreateCollisionAlgorithm
// 0x56C5B0 (ci*, body0, body1; retn 0Ch): mem = the dispatcher's (ci +0)
// vt+0x34 allocateCollisionAlgorithm( 0x1C ); null -> 0; else the ctor
// inline: [mem] = 0x7573CC then 0x7575BC, +4 the dispatcher, +8 m_ownManifold
// 0, +0xC m_manifoldPtr 0, +0x10 m_isSwapped (this +4 non-zero -> 1), +0x14 /
// +0x18 = this +8 / +0xC (the perturbation iterations / threshold); the
// bodies in that order (swapped: body1, body0) to needsCollision vt+0x14 -
// al set -> m_manifoldPtr = getNewManifold vt+8 (same order) on mem +4, own
// = 1. eax = mem.
inline std::uint32_t bt_convex_plane_create( const bt_mem_t& m, std::uint32_t self, std::uint32_t ci, std::uint32_t body0, std::uint32_t body1 )
{
    const std::uint32_t size = 0x1C;
    const std::uint32_t mem = m.vcall( m.user, m.rd( m.user, ci ), 0x34, &size, 1 );
    const bool swapped = m.rd8( m.user, self + 4 ) != 0;
    if ( mem == 0 )
        return 0;
    const std::uint32_t threshold = m.rd( m.user, self + 0xC ), iterations = m.rd( m.user, self + 8 );
    const std::uint32_t dispatcher = m.rd( m.user, ci );
    m.wr( m.user, mem, 0x7573CCu );
    m.wr( m.user, mem + 4, dispatcher );
    m.wr( m.user, mem + 0x14, iterations );
    m.wr( m.user, mem, 0x7575BCu );
    m.wr8( m.user, mem + 8, 0 );
    m.wr( m.user, mem + 0xC, 0 );
    m.wr8( m.user, mem + 0x10, swapped ? 1 : 0 );
    m.wr( m.user, mem + 0x18, threshold );
    const std::uint32_t bodies[ 2 ] = { swapped ? body1 : body0, swapped ? body0 : body1 };
    if ( ( m.vcall( m.user, dispatcher, 0x14, bodies, 2 ) & 0xFFu ) != 0 )
    {
        m.wr( m.user, mem + 0xC, m.vcall( m.user, m.rd( m.user, mem + 4 ), 8, bodies, 2 ) );
        m.wr8( m.user, mem + 8, 1 );
    }
    return mem;
}
// The transform products of btConvexShape's getAabbSlow / project (a
// btTransform: basis rows +0 / +0x10 / +0x20, origin +0x30): every sum in
// retail's grouping (the x and y terms first, then z, then the origin).
// dir * basis (column k):
inline float bt_basis_column_dot( const bt_mem_t& m, std::uint32_t trans, std::uint32_t k, const float* v )
{
    return ( bt_rdf( m, trans + 4 * k ) * v[ 0 ] + bt_rdf( m, trans + 0x10 + 4 * k ) * v[ 1 ] ) + bt_rdf( m, trans + 0x20 + 4 * k ) * v[ 2 ];
}

// trans( s ) row r:
inline float bt_transform_row( const bt_mem_t& m, std::uint32_t trans, std::uint32_t r, const float* s )
{
    const std::uint32_t row = trans + 0x10 * r;
    return ( ( bt_rdf( m, row ) * s[ 0 ] + bt_rdf( m, row + 4 ) * s[ 1 ] ) + bt_rdf( m, row + 8 ) * s[ 2 ] ) + bt_rdf( m, trans + 0x30 + 4 * r );
}

// localGetSupportingVertex vt+0x38( out*, dir* ) through io: the direction's
// four words go as the call's arguments (the w lane 0), the vertex comes back
// through out (retail reads it from out, or through the returned out).
inline void bt_convex_support_call( const bt_io_t& io, std::uint32_t self, const float* dir, float* out )
{
    std::uint32_t d[ 4 ] = { 0, 0, 0, 0 };
    std::memcpy( d, dir, 12 );
    std::uint32_t o[ 4 ] = { };
    io.vcall_out( io.m->user, self, 0x38, d, 4, o, 4 );
    std::memcpy( out, o, 12 );
}

// btConvexShape::getAabbSlow 0x552F90 (trans*, min*, max*; retn 0Ch): margin
// = getMargin (vt+0x28, `margin` answers it) once; per axis i: the unit
// vector e_i (the others 0) through the basis, its supporting vertex
// transformed, max[ i ] = that + margin; then -e_i the same, min[ i ] = that
// - margin.
inline void bt_convex_aabb_slow( const bt_io_t& io, std::uint32_t self, std::uint32_t trans, std::uint32_t mn, std::uint32_t mx,
                                 std::uint32_t ( *margin )( const bt_mem_t& m, std::uint32_t self ) )
{
    const bt_mem_t& m = *io.m;
    const std::uint32_t mb = margin( m, self );
    float mf;
    std::memcpy( &mf, &mb, 4 );
    for ( std::uint32_t i = 0; i < 3; ++i )
    {
        for ( int sign = 0; sign < 2; ++sign )
        {
            float vec[ 3 ] = { 0.0f, 0.0f, 0.0f };
            vec[ i ] = sign == 0 ? 1.0f : -1.0f;
            const float dir[ 3 ] = { bt_basis_column_dot( m, trans, 0, vec ), bt_basis_column_dot( m, trans, 1, vec ),
                                     bt_basis_column_dot( m, trans, 2, vec ) };
            float s[ 3 ];
            bt_convex_support_call( io, self, dir, s );
            const float t = bt_transform_row( m, trans, i, s );
            if ( sign == 0 )
                bt_wrf( m, mx + 4 * i, t + mf );
            else
                bt_wrf( m, mn + 4 * i, t - mf );
        }
    }
}

// btConvexShape::project 0x553610 (trans*, dir*, min*, max*; retn 10h): the
// local axis dir * basis; the supporting vertices along it and along its
// negation (sign bits flipped), each transformed; min = vertex1 . dir, max =
// vertex2 . dir ((y, x first) then z); min > max (ordered) -> swapped.
inline void bt_convex_project( const bt_io_t& io, std::uint32_t self, std::uint32_t trans, std::uint32_t dir, std::uint32_t mn, std::uint32_t mx )
{
    const bt_mem_t& m = *io.m;
    const float d[ 3 ] = { bt_rdf( m, dir ), bt_rdf( m, dir + 4 ), bt_rdf( m, dir + 8 ) };
    const float axis[ 3 ] = { bt_basis_column_dot( m, trans, 0, d ), bt_basis_column_dot( m, trans, 1, d ), bt_basis_column_dot( m, trans, 2, d ) };
    float s[ 3 ];
    bt_convex_support_call( io, self, axis, s );
    const float v1[ 3 ] = { bt_transform_row( m, trans, 0, s ), bt_transform_row( m, trans, 1, s ), bt_transform_row( m, trans, 2, s ) };
    float neg[ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        std::uint32_t w;
        std::memcpy( &w, &axis[ k ], 4 );
        w ^= 0x80000000u;
        std::memcpy( &neg[ k ], &w, 4 );
    }
    bt_convex_support_call( io, self, neg, s );
    const float v2[ 3 ] = { bt_transform_row( m, trans, 0, s ), bt_transform_row( m, trans, 1, s ), bt_transform_row( m, trans, 2, s ) };
    const float lo = ( bt_rdf( m, dir + 4 ) * v1[ 1 ] + bt_rdf( m, dir ) * v1[ 0 ] ) + bt_rdf( m, dir + 8 ) * v1[ 2 ];
    bt_wrf( m, mn, lo );
    const float hi = ( bt_rdf( m, dir + 4 ) * v2[ 1 ] + bt_rdf( m, dir ) * v2[ 0 ] ) + bt_rdf( m, dir + 8 ) * v2[ 2 ];
    bt_wrf( m, mx, hi );
    const float kept = bt_rdf( m, mn );
    if ( kept > hi )
    {
        bt_wrf( m, mn, hi );
        bt_wrf( m, mx, kept );
    }
}

// --- btDbvt on retail memory (boat batch 22): the twin of dbvt.hxx's typed
// insertleaf / removeleaf / update (same functions, read again from the
// disassembly) for the compound shape's own tree (+0x40). Nodes 0x30 bytes:
// +0 min, +0x10 max (movaps: four lanes), +0x20 parent, +0x24 childs[ 0 ] /
// data, +0x28 childs[ 1 ] (0 = a leaf). The tree: +0 root, +4 m_free, +8
// m_lkhd. Four-lane loads go through a word array and _mm_loadu_si128 (BRIEF:
// the float[4]-per-lane fill + _mm_loadu_ps shape miscompiles).
inline __m128 bt_ld128( const bt_mem_t& m, std::uint32_t a )
{
    std::uint32_t w[ 4 ];
    for ( std::uint32_t k = 0; k < 4; ++k )
        w[ k ] = m.rd( m.user, a + 4 * k );
    return _mm_castsi128_ps( _mm_loadu_si128( reinterpret_cast< const __m128i* >( w ) ) );
}

inline void bt_st128( const bt_mem_t& m, std::uint32_t a, __m128 v )
{
    std::uint32_t w[ 4 ];
    _mm_storeu_si128( reinterpret_cast< __m128i* >( w ), _mm_castps_si128( v ) );
    for ( std::uint32_t k = 0; k < 4; ++k )
        m.wr( m.user, a + 4 * k, w[ k ] );
}

// A node's volume = minps( a.min, b.min ) / maxps( a.max, b.max ) (retail's
// operand order: a NaN lane takes b's).
inline void bt_dbvt_merge( const bt_mem_t& m, std::uint32_t a, std::uint32_t b, std::uint32_t to )
{
    const __m128 mn = _mm_min_ps( bt_ld128( m, a ), bt_ld128( m, b ) );
    const __m128 mx = _mm_max_ps( bt_ld128( m, a + 0x10 ), bt_ld128( m, b + 0x10 ) );
    bt_st128( m, to, mn );
    bt_st128( m, to + 0x10, mx );
}

// Proximity as insertleaf 0x5942E0 inlines it: |(c.max + c.min) - lsum| (andps
// 0x6F7CC0) over four lanes, then (lane 2 + lane 0) + (lane 3 + lane 1).
inline __m128 bt_dbvt_proximity( const bt_mem_t& m, std::uint32_t c, __m128 lsum )
{
    const __m128 mask = _mm_castsi128_ps( _mm_set1_epi32( 0x7FFFFFFF ) );
    const __m128 d = _mm_and_ps( _mm_sub_ps( _mm_add_ps( bt_ld128( m, c + 0x10 ), bt_ld128( m, c ) ), lsum ), mask );
    const __m128 h = _mm_add_ps( _mm_movehl_ps( d, d ), d );
    return _mm_add_ss( h, _mm_shuffle_ps( h, h, 1 ) );
}

// insertleaf @0x5942E0 (ecx tree, edx start, stack leaf; caller pops): an
// empty tree takes the leaf as root (parent 0); else from `start` while
// internal to childs[ cmpless( d1, d0 ) & 1 ]; the new node is m_free (taken)
// or btAlignedAlloc( 0x30, 16 ); its parent = the sibling's, childs 0, volume
// merge( leaf, sibling ); it takes the sibling's place (childs[ 0 ] =
// sibling, childs[ 1 ] = leaf); walking up, an ancestor that does not Contain
// the node below (`comiss; jb` x6: min >= / max >= ordered) is re-merged from
// its childs, until one does or the root is passed.
inline void bt_dbvt_insert_leaf( const bt_mem_t& m, std::uint32_t tree, std::uint32_t start, std::uint32_t leaf )
{
    if ( m.rd( m.user, tree ) == 0 )
    {
        m.wr( m.user, tree, leaf );
        m.wr( m.user, leaf + 0x20, 0 );
        return;
    }
    std::uint32_t sibling = start;
    if ( m.rd( m.user, sibling + 0x28 ) != 0 )
    {
        const __m128 lsum = _mm_add_ps( bt_ld128( m, leaf + 0x10 ), bt_ld128( m, leaf ) );
        do
        {
            const __m128 d0 = bt_dbvt_proximity( m, m.rd( m.user, sibling + 0x24 ), lsum );
            const __m128 d1 = bt_dbvt_proximity( m, m.rd( m.user, sibling + 0x28 ), lsum );
            const std::uint32_t pick = static_cast< std::uint32_t >( _mm_cvtsi128_si32( _mm_castps_si128( _mm_cmple_ss( d1, d0 ) ) ) ) & 1u;
            sibling = m.rd( m.user, sibling + 0x24 + 4 * pick );
        } while ( m.rd( m.user, sibling + 0x28 ) != 0 );
    }
    std::uint32_t node = m.rd( m.user, tree + 4 );
    const std::uint32_t prev = m.rd( m.user, sibling + 0x20 );
    if ( node != 0 )
        m.wr( m.user, tree + 4, 0 );
    else
        node = bt_aligned_alloc( m, 0x30, 0x10 );
    m.wr( m.user, node + 0x20, prev );
    m.wr( m.user, node + 0x24, 0 );
    m.wr( m.user, node + 0x28, 0 );
    bt_dbvt_merge( m, leaf, sibling, node );
    if ( prev == 0 )
    {
        m.wr( m.user, node + 0x24, sibling );
        m.wr( m.user, sibling + 0x20, node );
        m.wr( m.user, node + 0x28, leaf );
        m.wr( m.user, leaf + 0x20, node );
        m.wr( m.user, tree, node );
        return;
    }
    const std::uint32_t at = m.rd( m.user, m.rd( m.user, sibling + 0x20 ) + 0x28 ) == sibling ? 1u : 0u;
    m.wr( m.user, prev + 0x24 + 4 * at, node );
    m.wr( m.user, node + 0x24, sibling );
    m.wr( m.user, sibling + 0x20, node );
    m.wr( m.user, node + 0x28, leaf );
    m.wr( m.user, leaf + 0x20, node );
    for ( std::uint32_t up = prev; up != 0; up = m.rd( m.user, up + 0x20 ) )
    {
        if ( bt_rdf( m, node ) >= bt_rdf( m, up ) && bt_rdf( m, node + 4 ) >= bt_rdf( m, up + 4 ) && bt_rdf( m, node + 8 ) >= bt_rdf( m, up + 8 ) &&
             bt_rdf( m, up + 0x10 ) >= bt_rdf( m, node + 0x10 ) && bt_rdf( m, up + 0x14 ) >= bt_rdf( m, node + 0x14 ) &&
             bt_rdf( m, up + 0x18 ) >= bt_rdf( m, node + 0x18 ) )
            return;
        bt_dbvt_merge( m, m.rd( m.user, up + 0x24 ), m.rd( m.user, up + 0x28 ), up );
        node = up;
    }
}

// removeleaf @0x594480 (ecx tree, edx leaf): the root leaf empties the tree
// (answers 0); else the parent's other child takes the parent's place (as the
// root, parent 0, when the parent was the root - answers the root), the
// parent goes through deletenode; the ancestors are re-merged until one whose
// min / max lanes x y z come out ordered-equal to before (`ucomiss; lahf; test
// ah, 44h; jp`: NaN = changed) - answered - else the root.
inline std::uint32_t bt_dbvt_remove_leaf( const bt_mem_t& m, std::uint32_t tree, std::uint32_t leaf )
{
    if ( leaf == m.rd( m.user, tree ) )
    {
        m.wr( m.user, tree, 0 );
        return 0;
    }
    const std::uint32_t parent = m.rd( m.user, leaf + 0x20 );
    const std::uint32_t sibling = m.rd( m.user, parent + ( m.rd( m.user, parent + 0x28 ) == leaf ? 0x24u : 0x28u ) );
    std::uint32_t prev = m.rd( m.user, parent + 0x20 );
    if ( prev == 0 )
    {
        m.wr( m.user, tree, sibling );
        m.wr( m.user, sibling + 0x20, 0 );
        bt_dbvt_delete_node( m, tree, parent );
        return m.rd( m.user, tree );
    }
    m.wr( m.user, prev + 0x24 + ( m.rd( m.user, prev + 0x28 ) == parent ? 4u : 0u ), sibling );
    m.wr( m.user, sibling + 0x20, prev );
    bt_dbvt_delete_node( m, tree, parent );
    do
    {
        float old[ 8 ];
        for ( std::uint32_t k = 0; k < 8; ++k )
            old[ k ] = bt_rdf( m, prev + 4 * k );
        bt_dbvt_merge( m, m.rd( m.user, prev + 0x24 ), m.rd( m.user, prev + 0x28 ), prev );
        if ( old[ 0 ] == bt_rdf( m, prev ) && old[ 1 ] == bt_rdf( m, prev + 4 ) && old[ 2 ] == bt_rdf( m, prev + 8 ) && old[ 4 ] == bt_rdf( m, prev + 0x10 ) &&
             old[ 5 ] == bt_rdf( m, prev + 0x14 ) && old[ 6 ] == bt_rdf( m, prev + 0x18 ) )
            return prev;
        prev = m.rd( m.user, prev + 0x20 );
    } while ( prev != 0 );
    return m.rd( m.user, tree );
}

// btDbvt::update @0x594960 (thiscall leaf, volume*; retn 8): removeleaf; with
// a result, m_lkhd < 0 (`js`) restarts from the root, else the result walked
// up at most m_lkhd parents; the volume's 32 bytes into the leaf; insertleaf
// from there. `volume` = the eight words.
inline void bt_dbvt_update( const bt_mem_t& m, std::uint32_t tree, std::uint32_t leaf, const std::uint32_t* volume )
{
    std::uint32_t start = bt_dbvt_remove_leaf( m, tree, leaf );
    if ( start != 0 )
    {
        const std::int32_t lkhd = static_cast< std::int32_t >( m.rd( m.user, tree + 8 ) );
        if ( lkhd < 0 )
            start = m.rd( m.user, tree );
        else
            for ( std::int32_t i = 0; i < lkhd; ++i )
            {
                const std::uint32_t up = m.rd( m.user, start + 0x20 );
                if ( up == 0 )
                    break;
                start = up;
            }
    }
    for ( std::uint32_t k = 0; k < 8; ++k )
        m.wr( m.user, leaf + 4 * k, volume[ k ] );
    bt_dbvt_insert_leaf( m, tree, start, leaf );
}

// btCompoundShape::updateChildTransform @0x526F40 (thiscall index,
// transform*, a third word never read; retn 0Ch - no local-aabb recalc in
// this build): the 64 bytes into the child record (data +0x18, 0x50-byte
// records); with the tree (+0x40) the child shape's (+0x40) getAabb vt+4(
// transform, min*, max* ) - through io: the transform's 16 words as its
// arguments, min / max back - and btDbvt::update( the record's leaf +0x4C,
// min:max ).
inline void bt_compound_update_child_transform( const bt_io_t& io, std::uint32_t self, std::uint32_t index, const std::uint32_t* transform )
{
    const bt_mem_t& m = *io.m;
    const std::uint32_t off = 0x50u * index;
    const std::uint32_t record = m.rd( m.user, self + 0x18 ) + off;
    for ( std::uint32_t k = 0; k < 16; ++k )
        m.wr( m.user, record + 4 * k, transform[ k ] );
    if ( m.rd( m.user, self + 0x40 ) == 0 )
        return;
    std::uint32_t volume[ 8 ] = { };
    io.vcall_out( m.user, m.rd( m.user, m.rd( m.user, self + 0x18 ) + off + 0x40 ), 4, transform, 16, volume, 8 );
    bt_dbvt_update( m, m.rd( m.user, self + 0x40 ), m.rd( m.user, off + m.rd( m.user, self + 0x18 ) + 0x4C ), volume );
}

// btCompoundShape::setLocalScaling @0x5276D0 (slot 5; thiscall scaling*; retn
// 4): per child (count +0x10 re-read): the child transform copied; the child
// shape's getLocalScaling vt+0x18 (answers a pointer: its 16 bytes read);
// child scale = ((s.x * c.x) / ls.x, (s.y * c.y) / ls.y, (s.z * c.z) / ls.z,
// 0) with ls = m_localScaling +0x50 -> the child's setLocalScaling vt+0x14
// (its four words as the arguments); the copy's origin = (s.x * o.x, o.y *
// s.y, o.z * s.z, 0); updateChildTransform( i, copy ). Then m_localScaling =
// the scaling's 16 bytes and recalculateLocalAabb (vt+0x3C, virtual).
inline void bt_compound_set_local_scaling( const bt_io_t& io, std::uint32_t self, std::uint32_t scaling )
{
    const bt_mem_t& m = *io.m;
    for ( std::int32_t i = 0; i < static_cast< std::int32_t >( m.rd( m.user, self + 0x10 ) ); ++i )
    {
        const std::uint32_t off = 0x50u * static_cast< std::uint32_t >( i );
        std::uint32_t t[ 16 ];
        const std::uint32_t record = m.rd( m.user, self + 0x18 ) + off;
        for ( std::uint32_t k = 0; k < 16; ++k )
            t[ k ] = m.rd( m.user, record + 4 * k );
        const std::uint32_t p = m.vcall( m.user, m.rd( m.user, record + 0x40 ), 0x18, nullptr, 0 );
        const float c[ 3 ] = { bt_rdf( m, p ), bt_rdf( m, p + 4 ), bt_rdf( m, p + 8 ) };
        const float s[ 3 ] = { bt_rdf( m, scaling ), bt_rdf( m, scaling + 4 ), bt_rdf( m, scaling + 8 ) };
        const float v[ 3 ] = { ( s[ 0 ] * c[ 0 ] ) / bt_rdf( m, self + 0x50 ), ( s[ 1 ] * c[ 1 ] ) / bt_rdf( m, self + 0x54 ),
                               ( s[ 2 ] * c[ 2 ] ) / bt_rdf( m, self + 0x58 ) };
        std::uint32_t vw[ 4 ] = { 0, 0, 0, 0 };
        std::memcpy( vw, v, 12 );
        m.vcall( m.user, m.rd( m.user, m.rd( m.user, self + 0x18 ) + off + 0x40 ), 0x14, vw, 4 );
        float o[ 3 ];
        std::memcpy( o, t + 12, 12 );
        const float no[ 3 ] = { bt_rdf( m, scaling ) * o[ 0 ], o[ 1 ] * bt_rdf( m, scaling + 4 ), o[ 2 ] * bt_rdf( m, scaling + 8 ) };
        std::memcpy( t + 12, no, 12 );
        t[ 15 ] = 0;
        bt_compound_update_child_transform( io, self, static_cast< std::uint32_t >( i ), t );
    }
    bt_copy16( m, self + 0x50, scaling );
    m.vcall( m.user, self, 0x3C, nullptr, 0 );
}

// btCompoundShape::serialize @0x5278B0 (slot 12; buffer, serializer; retn 8):
// the name prologue; buffer +0x14 = m_collisionMargin (+0x48), +0x10 = the
// child count (+0x10), +0xC = 0; with children: chunk = allocate( 0x4C,
// count ) (vt+0xC), buffer +0xC = getUniquePointer( chunk->m_oldPtr ) (vt+0x18);
// per child (buffer +0x10 re-read) into the 0x4C-byte btCompoundShapeChildData:
// +0x48 the margin, +0x40 getUniquePointer( shape ), and when findPointer(
// shape ) (vt+0x14) is 0 the shape's own 'SHAP' chunk (its vt+0x2C size,
// allocate( size, 1 ), its vt+0x30( chunk->m_oldPtr, serializer ), finalizeChunk(
// chunk, type, 'SHAP', shape )); +0x44 the child type; +0 the 64-byte
// transform. Then finalizeChunk( chunk, "btCompoundShapeChildData" 0x6CDCD8,
// 'ARAY' 0x59415241, chunk->m_oldPtr ). Answers "btCompoundShapeData"
// (0x6CDCF4).
inline std::uint32_t bt_compound_serialize( const bt_mem_t& m, std::uint32_t self, std::uint32_t buffer, std::uint32_t serializer )
{
    bt_shape_serialize_name( m, self, buffer, serializer );
    m.wr( m.user, buffer + 0x14, m.rd( m.user, self + 0x48 ) );
    const std::uint32_t n = m.rd( m.user, self + 0x10 );
    m.wr( m.user, buffer + 0x10, n );
    m.wr( m.user, buffer + 0xC, 0 );
    if ( n == 0 )
        return 0x6CDCF4u;
    const std::uint32_t a[ 2 ] = { 0x4C, n };
    const std::uint32_t chunk = m.vcall( m.user, serializer, 0xC, a, 2 );
    const std::uint32_t data = m.rd( m.user, chunk + 8 );
    m.wr( m.user, buffer + 0xC, m.vcall( m.user, serializer, 0x18, &data, 1 ) );
    for ( std::int32_t i = 0; i < static_cast< std::int32_t >( m.rd( m.user, buffer + 0x10 ) ); ++i )
    {
        const std::uint32_t off = 0x50u * static_cast< std::uint32_t >( i );
        const std::uint32_t to = data + 0x4Cu * static_cast< std::uint32_t >( i );
        m.wr( m.user, to + 0x48, m.rd( m.user, m.rd( m.user, self + 0x18 ) + off + 0x48 ) );
        const std::uint32_t shape = m.rd( m.user, m.rd( m.user, self + 0x18 ) + off + 0x40 );
        m.wr( m.user, to + 0x40, m.vcall( m.user, serializer, 0x18, &shape, 1 ) );
        const std::uint32_t again = m.rd( m.user, m.rd( m.user, self + 0x18 ) + off + 0x40 );
        if ( m.vcall( m.user, serializer, 0x14, &again, 1 ) == 0 )
        {
            const std::uint32_t child = m.rd( m.user, m.rd( m.user, self + 0x18 ) + off + 0x40 );
            const std::uint32_t b[ 2 ] = { m.vcall( m.user, child, 0x2C, nullptr, 0 ), 1 };
            const std::uint32_t sub = m.vcall( m.user, serializer, 0xC, b, 2 );
            const std::uint32_t c[ 2 ] = { m.rd( m.user, sub + 8 ), serializer };
            const std::uint32_t type = m.vcall( m.user, m.rd( m.user, m.rd( m.user, self + 0x18 ) + off + 0x40 ), 0x30, c, 2 );
            const std::uint32_t d[ 4 ] = { sub, type, 0x50414853u, m.rd( m.user, m.rd( m.user, self + 0x18 ) + off + 0x40 ) };
            m.vcall( m.user, serializer, 0x10, d, 4 );
        }
        m.wr( m.user, to + 0x44, m.rd( m.user, m.rd( m.user, self + 0x18 ) + off + 0x44 ) );
        const std::uint32_t record = m.rd( m.user, self + 0x18 ) + off;
        for ( std::uint32_t k = 0; k < 16; ++k )
            m.wr( m.user, to + 4 * k, m.rd( m.user, record + 4 * k ) );
    }
    const std::uint32_t e[ 4 ] = { chunk, 0x6CDCD8u, 0x59415241u, m.rd( m.user, chunk + 8 ) };
    m.vcall( m.user, serializer, 0x10, e, 4 );
    return 0x6CDCF4u;
}