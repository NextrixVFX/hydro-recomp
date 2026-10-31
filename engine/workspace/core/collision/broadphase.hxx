#pragma once

#include <memory>
#include <vector>
#include <xmmintrin.h>

#include <workspace/core/collision/dbvt.hxx>
#include <workspace/core/collision/pairs.hxx>

// btDbvtBroadphase (Bullet 2.79, vt 0x74F848), recreated from retail's
// disassembly (memory/audit.md "Batch 8i"): the ctor 0x56DC10, createProxy
// 0x56DDF0 (+ collideTV 0x56EBD0), destroyProxy 0x56DF40, setAabb 0x56E150
// (+ collideTTpersistentStack 0x56F4D0), calculateOverlappingPairs 0x56E3F0
// = collide 0x56E6A0 + performDeferredPairRemoval 0x56E420 (gated on the
// hashed cache's hasDeferredRemoval - 0 - so it does nothing here).

// btDbvtProxy (64 bytes): the btBroadphaseProxy (+0 client, +4 group, +6
// mask, +8 multiSap 0, +12 uid), +16 / +32 the aabb, +48 the leaf, +52 / +56
// the stage-list links (prev, next), +60 the stage (2 = the fixed set).
struct dbvt_proxy_t : broadphase_proxy_t
{
    float aabb_min[ 4 ];
    float aabb_max[ 4 ];
    dbvt_node_t* leaf = nullptr;
    dbvt_proxy_t* links[ 2 ] = {};
    int stage = 0;
    int index = 0;   // creation order - retail's address, for the check
};

// The members at retail's offsets: +4 / +44 m_sets[2] (dynamic / fixed),
// +84 m_stageRoots[3], +96 m_paircache, +100 m_prediction, +104
// m_stageCurrent, +108 m_fupdates, +112 m_dupdates, +116 m_cupdates, +120
// m_newpairs, +124 m_fixedleft, +128 m_updates_call, +132 m_updates_done,
// +136 m_updates_ratio, +140 m_pid, +144 m_cid, +148 m_gid, +152
// m_releasepaircache, +153 m_deferedcollide, +154 m_needcleanup - the ctor's
// values (CTOR DEFAULTS: what Vu changes after is not read).
struct dbvt_broadphase_t
{
    dbvt_t sets[ 2 ];
    dbvt_proxy_t* stage_roots[ 3 ] = {};
    pair_cache_t cache;
    float prediction = 0.0f;
    int stage_current = 0;
    int fupdates = 1;
    int dupdates = 0;
    int cupdates = 10;
    int newpairs = 1;
    int fixedleft = 0;
    unsigned updates_call = 0;
    unsigned updates_done = 0;
    float updates_ratio = 0.0f;
    int pid = 0;
    int cid = 0;
    int gid = 0;
    bool deferedcollide = false;
    bool needcleanup = true;
    std::vector< std::unique_ptr< dbvt_proxy_t > > proxies;
    int node_counter = 0;
};

// The ctor 0x56DC10 (thiscall paircache; retn 4) with paircache 0: two
// btDbvt (ctor 0x5945E0: root / free 0, m_lkhd -1, leaves 0, opath 0, the
// stack array empty), its own btHashedOverlappingPairCache (76 bytes, ctor
// 0x5B0B10), the fields above.
inline void broadphase_init( dbvt_broadphase_t& bp )
{
    bp = dbvt_broadphase_t{};
    for ( dbvt_t& t : bp.sets )
    {
        t.lkhd = -1;
        t.counter = &bp.node_counter;
    }
    pair_cache_init( bp.cache );
}

// listappend / listremove on a stage list (+52 prev, +56 next).
inline void broadphase_list_append( dbvt_proxy_t* item, dbvt_proxy_t*& list )
{
    item->links[ 0 ] = nullptr;
    item->links[ 1 ] = list;
    if ( list )
        list->links[ 0 ] = item;
    list = item;
}

inline void broadphase_list_remove( dbvt_proxy_t* item, dbvt_proxy_t*& list )
{
    if ( item->links[ 0 ] )
        item->links[ 0 ]->links[ 1 ] = item->links[ 1 ];
    else
        list = item->links[ 1 ];
    if ( item->links[ 1 ] )
        item->links[ 1 ]->links[ 0 ] = item->links[ 0 ];
}

// Intersect as every site inlines it: cmpltps(a.max, b.min) | cmpltps(b.max,
// a.min), lanes 0..2 - none set = an overlap (NaN = an overlap).
inline bool broadphase_intersect( const float* amn, const float* amx, const float* bmn, const float* bmx )
{
    const __m128 m = _mm_or_ps( _mm_cmplt_ps( _mm_loadu_ps( amx ), _mm_loadu_ps( bmn ) ), _mm_cmplt_ps( _mm_loadu_ps( bmx ), _mm_loadu_ps( amn ) ) );
    return ( _mm_movemask_ps( m ) & 7 ) == 0;
}

// btDbvtTreeCollider::Process: addOverlappingPair(a.data, b.data) on the
// cache (slot 1), ++m_newpairs.
inline int broadphase_add_pair( dbvt_broadphase_t& bp, const dbvt_node_t* a, const dbvt_node_t* b )
{
    if ( pair_cache_add( bp.cache, static_cast< broadphase_proxy_t* >( a->data ), static_cast< broadphase_proxy_t* >( b->data ) ) == -1 )
        return -1;
    ++bp.newpairs;
    return 1;
}

// collideTV @0x56EBD0 (ecx = the root NODE, volume, collider; retn 8): a
// stack from the root; a node intersecting the volume - internal: push
// childs[0] then childs[1]; a leaf other than the collider proxy's own:
// Process(leaf, proxy leaf).
inline int broadphase_collide_tv( dbvt_broadphase_t& bp, dbvt_node_t* root, const float* mn, const float* mx, dbvt_proxy_t* proxy )
{
    std::vector< dbvt_node_t* > stack{ root };
    while ( !stack.empty( ) )
    {
        dbvt_node_t* n = stack.back( );
        stack.pop_back( );
        if ( !broadphase_intersect( mn, mx, n->mn, n->mx ) )
            continue;
        if ( !dbvt_is_leaf( n ) )
        {
            stack.push_back( n->childs[ 0 ] );
            stack.push_back( n->childs[ 1 ] );
        }
        else if ( n != proxy->leaf )
        {
            if ( broadphase_add_pair( bp, n, proxy->leaf ) < 0 )
                return -1;
        }
    }
    return 1;
}

// collideTTpersistentStack @0x56F4D0 (thiscall tree, root0, root1, collider;
// retn 0Ch): pairs (a, b) from a stack - a == b internal: (c0, c0), (c1,
// c1), (c0, c1); a != b intersecting: both internal (a0, b0), (a1, b0), (a0,
// b1), (a1, b1); a internal (a0, b), (a1, b); b internal (a, b0), (a, b1);
// two leaves: Process(a, b). The tree's persistent stack array (resized to
// 128, doubled past 124) only holds the pairs.
inline int broadphase_collide_tt( dbvt_broadphase_t& bp, dbvt_node_t* root0, dbvt_node_t* root1 )
{
    if ( !root0 || !root1 )
        return 1;
    std::vector< std::pair< dbvt_node_t*, dbvt_node_t* > > stack{ { root0, root1 } };
    while ( !stack.empty( ) )
    {
        const auto [ a, b ] = stack.back( );
        stack.pop_back( );
        if ( a == b )
        {
            if ( !dbvt_is_leaf( a ) )
            {
                stack.push_back( { a->childs[ 0 ], a->childs[ 0 ] } );
                stack.push_back( { a->childs[ 1 ], a->childs[ 1 ] } );
                stack.push_back( { a->childs[ 0 ], a->childs[ 1 ] } );
            }
        }
        else if ( broadphase_intersect( a->mn, a->mx, b->mn, b->mx ) )
        {
            const bool ai = !dbvt_is_leaf( a ), bi = !dbvt_is_leaf( b );
            if ( ai && bi )
            {
                stack.push_back( { a->childs[ 0 ], b->childs[ 0 ] } );
                stack.push_back( { a->childs[ 1 ], b->childs[ 0 ] } );
                stack.push_back( { a->childs[ 0 ], b->childs[ 1 ] } );
                stack.push_back( { a->childs[ 1 ], b->childs[ 1 ] } );
            }
            else if ( ai )
            {
                stack.push_back( { a->childs[ 0 ], b } );
                stack.push_back( { a->childs[ 1 ], b } );
            }
            else if ( bi )
            {
                stack.push_back( { a, b->childs[ 0 ] } );
                stack.push_back( { a, b->childs[ 1 ] } );
            }
            else if ( broadphase_add_pair( bp, a, b ) < 0 )
                return -1;
        }
    }
    return 1;
}

// createProxy @0x56DDF0 (slot 1; aabbMin*, aabbMax*, shapeType, client,
// group, mask, dispatcher, multiSap; retn 20h): the 64-byte proxy (uid =
// ++m_gid, stage = m_stageCurrent), its leaf in m_sets[0], appended to its
// stage list; without the deferred collide, collideTV of the proxy's aabb
// from m_sets[0]'s root, then m_sets[1]'s (each when there is one).
inline dbvt_proxy_t* broadphase_create_proxy( dbvt_broadphase_t& bp, const float* mn, const float* mx, void* client, int client_type,
    short group, short mask, int& status )
{
    status = 1;
    bp.proxies.push_back( std::make_unique< dbvt_proxy_t >( ) );
    dbvt_proxy_t* p = bp.proxies.back( ).get( );
    p->index = static_cast< int >( bp.proxies.size( ) ) - 1;
    p->client = client;
    p->client_type = client_type;
    p->group = group;
    p->mask = mask;
    std::memcpy( p->aabb_min, mn, 16 );
    std::memcpy( p->aabb_max, mx, 16 );
    p->stage = bp.stage_current;
    p->uid = ++bp.gid;
    p->leaf = dbvt_insert( bp.sets[ 0 ], mn, mx, static_cast< broadphase_proxy_t* >( p ) );
    broadphase_list_append( p, bp.stage_roots[ bp.stage_current ] );
    if ( !bp.deferedcollide )
    {
        if ( bp.sets[ 0 ].root && broadphase_collide_tv( bp, bp.sets[ 0 ].root, mn, mx, p ) < 0 )
            status = -1;
        if ( status > 0 && bp.sets[ 1 ].root && broadphase_collide_tv( bp, bp.sets[ 1 ].root, mn, mx, p ) < 0 )
            status = -1;
    }
    return p;
}

// btDbvt::remove inlined: removeleaf, deletenode(leaf), --m_leaves.
inline void broadphase_tree_remove( dbvt_t& t, dbvt_node_t* leaf )
{
    dbvt_remove_leaf( t, leaf );
    dbvt_delete_node( t, leaf );
    --t.leaves;
}

// destroyProxy @0x56DF40 (slot 2; proxy, dispatcher; retn 8): the leaf out
// of its set (stage 2 -> m_sets[1]), the stage-list unlink, the cache's
// removeOverlappingPairsContainingProxy (vt+12), the proxy freed,
// m_needcleanup = 1.
inline int broadphase_destroy_proxy( dbvt_broadphase_t& bp, dbvt_proxy_t* p )
{
    broadphase_tree_remove( bp.sets[ p->stage == 2 ? 1 : 0 ], p->leaf );
    broadphase_list_remove( p, bp.stage_roots[ p->stage ] );
    const int r = pair_cache_remove_containing( bp.cache, p );
    p->leaf = nullptr;
    bp.needcleanup = true;
    return r;
}

// setAabb @0x56E150 (slot 3; proxy, aabbMin*, aabbMax*, dispatcher; retn
// 10h). The volume is a 4-lane copy of the two vectors. Stage 2: out of
// m_sets[1] (remove), into m_sets[0] (insert), collide. Else ++m_updates_call;
// a leaf volume that does not Intersect the new one -> update (plain),
// ++m_updates_done, collide; else the velocity = m_prediction * ((max - min)
// * 0.5) per lane, negated where `comiss 0, aabbMin - proxy.min; jbe` (0 >
// delta), w 0, and update with velocity (the volume edited in place) - true:
// ++m_updates_done, collide. Then the stage-list move to m_stageCurrent with
// the proxy's aabb = the ARGUMENTS; on collide m_needcleanup = 1 and,
// without the deferred collide, collideTT(m_sets[1].root, leaf) then
// collideTT(m_sets[0].root, leaf).
inline int broadphase_set_aabb( dbvt_broadphase_t& bp, dbvt_proxy_t* p, const float* mn, const float* mx )
{
    float vmn[ 4 ], vmx[ 4 ];
    std::memcpy( vmn, mn, 16 );
    std::memcpy( vmx, mx, 16 );
    bool docollide = false;
    if ( p->stage == 2 )
    {
        broadphase_tree_remove( bp.sets[ 1 ], p->leaf );
        p->leaf = dbvt_insert( bp.sets[ 0 ], vmn, vmx, static_cast< broadphase_proxy_t* >( p ) );
        docollide = true;
    }
    else
    {
        ++bp.updates_call;
        if ( !broadphase_intersect( p->leaf->mn, p->leaf->mx, vmn, vmx ) )
        {
            dbvt_update( bp.sets[ 0 ], p->leaf, vmn, vmx );
            ++bp.updates_done;
            docollide = true;
        }
        else
        {
            float velocity[ 4 ];
            for ( int k = 0; k < 3; ++k )
            {
                const float delta = mn[ k ] - p->aabb_min[ k ];
                velocity[ k ] = bp.prediction * ( ( p->aabb_max[ k ] - p->aabb_min[ k ] ) * 0.5f );
                if ( 0.0f > delta )
                    velocity[ k ] = support_negate( velocity[ k ] );
            }
            velocity[ 3 ] = 0.0f;
            if ( dbvt_update_velocity( bp.sets[ 0 ], p->leaf, vmn, vmx, velocity ) )
            {
                ++bp.updates_done;
                docollide = true;
            }
        }
    }
    broadphase_list_remove( p, bp.stage_roots[ p->stage ] );
    std::memcpy( p->aabb_min, mn, 16 );
    std::memcpy( p->aabb_max, mx, 16 );
    p->stage = bp.stage_current;
    broadphase_list_append( p, bp.stage_roots[ bp.stage_current ] );
    if ( docollide )
    {
        bp.needcleanup = true;
        if ( !bp.deferedcollide )
        {
            if ( broadphase_collide_tt( bp, bp.sets[ 1 ].root, p->leaf ) < 0 )
                return -1;
            if ( broadphase_collide_tt( bp, bp.sets[ 0 ].root, p->leaf ) < 0 )
                return -1;
        }
    }
    return 1;
}

// The `/ 100` the collide inlines: imul by 0x51EB851F, sar 5, + the sign bit
// - a truncating signed divide.
inline int broadphase_percent( int a, int b )
{
    return ( a * b ) / 100;
}

// collide @0x56E6A0 (thiscall dispatcher; retn 4): m_sets[0].optimizeIncre-
// mental(1 + leaves * m_dupdates / 100); with m_fixedleft, m_sets[1]'s with
// 1 + leaves * m_fupdates / 100 and m_fixedleft = max(m_fixedleft - count,
// 0); m_stageCurrent = (+1) % 2; that stage's proxies each moved to the
// fixed list (stage 2), out of m_sets[0] (remove) and into m_sets[1] (a
// createnode + insertleaf from its root, ++leaves) - m_fixedleft = m_sets[1]
// leaves, m_needcleanup = 1 when any moved; the deferred collide is skipped
// (m_deferedcollide 0); with m_needcleanup, over the cache's pair array: ni =
// min(size, max(m_newpairs, size * m_cupdates / 100)), for i < ni the pair at
// (m_cid + i) % size (the LIVE size) whose leaves do not Intersect is removed
// (vt+8) and --ni, --i; m_cid = size > 0 ? (m_cid + ni) % size : 0; then
// ++m_pid, m_newpairs = 1, m_needcleanup = 0, the ratio done / call (0 with
// no call), done >>= 1, call >>= 1.
inline int broadphase_collide( dbvt_broadphase_t& bp )
{
    dbvt_optimize_incremental( bp.sets[ 0 ], 1 + broadphase_percent( bp.sets[ 0 ].leaves, bp.dupdates ) );
    if ( bp.fixedleft )
    {
        const int count = 1 + broadphase_percent( bp.sets[ 1 ].leaves, bp.fupdates );
        dbvt_optimize_incremental( bp.sets[ 1 ], count );
        const int left = bp.fixedleft - count;
        bp.fixedleft = left < 0 ? 0 : left;
    }
    bp.stage_current = ( bp.stage_current + 1 ) % 2;
    dbvt_proxy_t* current = bp.stage_roots[ bp.stage_current ];
    if ( current )
    {
        do
        {
            dbvt_proxy_t* next = current->links[ 1 ];
            broadphase_list_remove( current, bp.stage_roots[ current->stage ] );
            broadphase_list_append( current, bp.stage_roots[ 2 ] );
            broadphase_tree_remove( bp.sets[ 0 ], current->leaf );
            current->leaf = dbvt_insert( bp.sets[ 1 ], current->aabb_min, current->aabb_max, static_cast< broadphase_proxy_t* >( current ) );
            current->stage = 2;
            current = next;
        } while ( current );
        bp.fixedleft = bp.sets[ 1 ].leaves;
        bp.needcleanup = true;
    }
    if ( bp.deferedcollide )
        return -1;   // the deferred collideTT pair: not reached (m_deferedcollide is 0)
    if ( bp.needcleanup )
    {
        aligned_array_t< broadphase_pair_t >& pairs = bp.cache.pairs;
        if ( pairs.size > 0 )
        {
            const int wanted = broadphase_percent( pairs.size, bp.cupdates );
            const int floor_ = bp.newpairs > wanted ? bp.newpairs : wanted;
            int ni = pairs.size >= floor_ ? floor_ : pairs.size;
            for ( int i = 0; i < ni; ++i )
            {
                broadphase_pair_t& p = pairs.data[ ( bp.cid + i ) % pairs.size ];
                const dbvt_node_t* la = static_cast< dbvt_proxy_t* >( p.proxy0 )->leaf;
                const dbvt_node_t* lb = static_cast< dbvt_proxy_t* >( p.proxy1 )->leaf;
                if ( !broadphase_intersect( la->mn, la->mx, lb->mn, lb->mx ) )
                {
                    unsigned internal = 0;
                    if ( pair_cache_remove( bp.cache, p.proxy0, p.proxy1, internal ) < 0 )
                        return -1;
                    --ni;
                    --i;
                }
            }
            bp.cid = pairs.size > 0 ? ( bp.cid + ni ) % pairs.size : 0;
        }
    }
    ++bp.pid;
    bp.newpairs = 1;
    bp.needcleanup = false;
    if ( bp.updates_call )
        bp.updates_ratio = static_cast< float >( bp.updates_done ) / static_cast< float >( bp.updates_call );
    else
        bp.updates_ratio = 0.0f;
    bp.updates_done >>= 1;
    bp.updates_call >>= 1;
    return 1;
}

// calculateOverlappingPairs @0x56E3F0 (slot 7; dispatcher): collide, then
// performDeferredPairRemoval 0x56E420 - only when the cache's
// hasDeferredRemoval (vt+52) is true; the hashed cache's 0x4052D0 returns 0.
inline int broadphase_calculate_overlapping_pairs( dbvt_broadphase_t& bp )
{
    return broadphase_collide( bp );
}
