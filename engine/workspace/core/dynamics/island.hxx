#pragma once

#include <workspace/core/dynamics/setup.hxx>

#include <vector>

// Bullet 2.79's simulation islands and the world's island solve, recreated
// from retail: btUnionFind, btSimulationIslandManager (vtable 0x758330) and
// btDiscreteDynamicsWorld::solveConstraints @0x601B60 with its
// InplaceSolverIslandCallback (vtable 0x757A88). memory/audit.md "Batch 7".
//
// Retail wraps several of these in CProfileSamples ("solveConstraints",
// "processIslands", "islandUnionFindAndQuickSort",
// "calculateSimulationIslands"); timing only, NOT recreated.
//
// Typed constraints are carried as their two body pointers (btTypedConstraint
// +24 rbA, +28 rbB - all the island code reads of them). Their solver stage is
// not recreated, so solveGroup still fails for a group that holds any.

struct island_constraint_t
{
    solver_rigid_t* body_a;     // +24
    solver_rigid_t* body_b;     // +28
};

// One broadphase pair as findUnions reads it: each proxy's client object.
struct island_pair_t
{
    solver_rigid_t* object0;
    solver_rigid_t* object1;
};

// btUnionFind element, 8 bytes: m_id, m_sz.
struct union_element_t
{
    int id;
    int sz;
};

// btSimulationIslandManager: +4 the union-find's elements, +28 the island
// manifolds, +48 the island bodies, byte +64 m_splitIslands.
struct island_manager_t
{
    std::vector< union_element_t > elements;
    std::vector< solver_manifold_t* > island_manifolds;
    std::vector< solver_rigid_t* > island_bodies;
    bool split_islands = true;
};

// btAlignedObjectArray::quickSortInternal as retail inlines it (0x605D90,
// 0x625110, 0x625FC0): the pivot is the middle element, copied once; the
// second recursion is a loop. Not stable - the order of equal keys is part
// of the result.
template < class T, class Key >
inline void island_quick_sort( T* d, int lo, int hi, Key key )
{
    for ( ;; )
    {
        int i = lo;
        int j = hi;
        const T x = d[ ( lo + hi ) / 2 ];
        do
        {
            while ( key( d[ i ] ) < key( x ) )
                ++i;
            while ( key( x ) < key( d[ j ] ) )
                --j;
            if ( i <= j )
            {
                const T t = d[ i ];
                d[ i ] = d[ j ];
                d[ j ] = t;
                ++i;
                --j;
            }
        } while ( i <= j );
        if ( lo < j )
            island_quick_sort( d, lo, j, key );
        if ( i >= hi )
            return;
        lo = i;
    }
}

// btUnionFind::find @0x5FFEC0: path halving.
inline int union_find( std::vector< union_element_t >& e, int x )
{
    while ( x != e[ x ].id )
    {
        e[ x ].id = e[ e[ x ].id ].id;
        x = e[ x ].id;
    }
    return x;
}

// btUnionFind::unite, inlined at 0x6246C0 and 0x602580: both finds first (p
// then q), then the root of p joins q's.
inline void union_unite( std::vector< union_element_t >& e, int p, int q )
{
    const int i = union_find( e, p );
    const int j = union_find( e, q );
    if ( i == j )
        return;
    e[ i ].id = j;
    e[ j ].sz += e[ i ].sz;
}

// The island id of a manifold (0x625110, 0x624D20) and of a constraint
// (0x605D90, 0x601E90): the first body's tag, or the second's if negative.
inline int island_manifold_id( const solver_manifold_t* m )
{
    return m->body0->island_tag >= 0 ? m->body0->island_tag : m->body1->island_tag;
}

inline int island_constraint_id( const island_constraint_t* c )
{
    return c->body_a->island_tag >= 0 ? c->body_a->island_tag : c->body_b->island_tag;
}

// btCollisionObject::setActivationState: no change from 4 or 5.
inline void island_set_activation( solver_rigid_t& o, int state )
{
    if ( o.activation_state != 4 && o.activation_state != 5 )
        o.activation_state = state;
}

// btSimulationIslandManager::updateActivationState @0x624770 (vtable slot
// 1): initUnionFind inlined - every non-static, non-kinematic object takes
// the next tag; every object's companion id is -1 and hit fraction 1 - then
// the union-find is reset (0x625E50, each element {i, 1}) and findUnions
// (0x6246C0) unites the pairs whose objects both merge islands (flags & 7
// == 0).
inline void island_update_activation_state( island_manager_t& m, solver_rigid_t* const* objects, int object_count,
    const island_pair_t* pairs, int pair_count )
{
    int index = 0;
    for ( int i = 0; i < object_count; ++i )
    {
        solver_rigid_t& o = *objects[ i ];
        if ( ( o.collision_flags & 3 ) == 0 )
            o.island_tag = index++;
        o.companion_id = -1;
        o.hit_fraction = 1.0f;
    }
    m.elements.resize( static_cast< size_t >( index ) );
    for ( int j = 0; j < index; ++j )
    {
        m.elements[ j ].id = j;
        m.elements[ j ].sz = 1;
    }
    for ( int i = 0; i < pair_count; ++i )
    {
        solver_rigid_t* a = pairs[ i ].object0;
        solver_rigid_t* b = pairs[ i ].object1;
        if ( a && ( a->collision_flags & 7 ) == 0 && b && ( b->collision_flags & 7 ) == 0 )
            union_unite( m.elements, a->island_tag, b->island_tag );
    }
}

// btSimulationIslandManager::storeIslandActivationState @0x624800 (slot 2).
inline void island_store_activation_state( island_manager_t& m, solver_rigid_t* const* objects, int object_count )
{
    int index = 0;
    for ( int i = 0; i < object_count; ++i )
    {
        solver_rigid_t& o = *objects[ i ];
        if ( ( o.collision_flags & 3 ) != 0 )
        {
            o.island_tag = -1;
            o.companion_id = -2;
        }
        else
        {
            o.island_tag = union_find( m.elements, index );
            m.elements[ index ].sz = i;
            o.companion_id = -1;
            ++index;
        }
    }
}

// btDiscreteDynamicsWorld::calculateSimulationIslands @0x602580. Retail 2.79
// does NOT test the constraint's enabled flag (2.82 does).
inline void island_calculate( island_manager_t& m, solver_rigid_t* const* objects, int object_count,
    const island_pair_t* pairs, int pair_count, const island_constraint_t* const* constraints, int constraint_count )
{
    island_update_activation_state( m, objects, object_count, pairs, pair_count );
    for ( int i = 0; i < constraint_count; ++i )
    {
        solver_rigid_t* a = constraints[ i ]->body_a;
        solver_rigid_t* b = constraints[ i ]->body_b;
        if ( a && ( a->collision_flags & 3 ) == 0 && b && ( b->collision_flags & 3 ) == 0 )
        {
            const bool a_active = a->activation_state != 2 && a->activation_state != 5;
            const bool b_active = b->activation_state != 2 && b->activation_state != 5;
            if ( a_active || b_active )
                union_unite( m.elements, a->island_tag, b->island_tag );
        }
    }
    island_store_activation_state( m, objects, object_count );
}

// btCollisionDispatcher::needsResponse @0x56D6C0 (dispatcher slot 6).
inline bool island_needs_response( const solver_rigid_t& a, const solver_rigid_t& b )
{
    if ( ( a.collision_flags & 4 ) != 0 || ( b.collision_flags & 4 ) != 0 )
        return false;
    return ( a.collision_flags & 3 ) == 0 || ( b.collision_flags & 3 ) == 0;
}

// btSimulationIslandManager::buildIslands @0x6248D0. `manifolds` is the
// dispatcher's manifold list in index order (getManifoldByIndexInternal).
// The kinematic wake is 2.79's: no hasContactResponse test. activate() is
// inlined - only an object neither static nor kinematic is woken.
inline void island_build( island_manager_t& m, solver_rigid_t* const* objects,
    solver_manifold_t* const* manifolds, int manifold_count )
{
    m.island_manifolds.clear();

    // sortIslands @0x625F40: every element points at its root, then the
    // elements are sorted by id.
    const int count = static_cast< int >( m.elements.size() );
    for ( int i = 0; i < count; ++i )
        m.elements[ i ].id = union_find( m.elements, i );
    if ( count > 1 )
        island_quick_sort( m.elements.data(), 0, count - 1, []( const union_element_t& e ) { return e.id; } );

    for ( int start = 0; start < count; )
    {
        const int island_id = m.elements[ start ].id;
        int end = start + 1;
        while ( end < count && m.elements[ end ].id == island_id )
            ++end;

        bool all_sleeping = true;
        for ( int idx = start; idx < end; ++idx )
        {
            const solver_rigid_t& o = *objects[ m.elements[ idx ].sz ];
            if ( o.island_tag == island_id )
            {
                if ( o.activation_state == 1 )
                    all_sleeping = false;
                if ( o.activation_state == 4 )
                    all_sleeping = false;
            }
        }
        for ( int idx = start; idx < end; ++idx )
        {
            solver_rigid_t& o = *objects[ m.elements[ idx ].sz ];
            if ( o.island_tag != island_id )
                continue;
            if ( all_sleeping )
                island_set_activation( o, 2 );
            else if ( o.activation_state == 2 )
            {
                o.activation_state = 3;
                o.deactivation_time = 0.0f;
            }
        }
        start = end;
    }

    for ( int i = 0; i < manifold_count; ++i )
    {
        solver_manifold_t* manifold = manifolds[ i ];
        solver_rigid_t* a = manifold->body0;
        solver_rigid_t* b = manifold->body1;
        if ( !( ( a && a->activation_state != 2 ) || ( b && b->activation_state != 2 ) ) )
            continue;
        if ( ( a->collision_flags & 2 ) != 0 && a->activation_state != 2 && ( b->collision_flags & 3 ) == 0 )
        {
            island_set_activation( *b, 1 );
            b->deactivation_time = 0.0f;
        }
        if ( ( b->collision_flags & 2 ) != 0 && b->activation_state != 2 && ( a->collision_flags & 3 ) == 0 )
        {
            island_set_activation( *a, 1 );
            a->deactivation_time = 0.0f;
        }
        if ( m.split_islands && island_needs_response( *a, *b ) )
            m.island_manifolds.push_back( manifold );
    }
}

// InplaceSolverIslandCallback: +4 info, +8 solver, +12 the island-sorted
// constraints (null when there are none), +16 their count, then the three
// batch arrays (bodies +36, manifolds +56, constraints +76). The debug
// drawer, stack allocator and dispatcher it forwards to solveGroup are not
// read by the solver pieces recreated so far.
struct island_callback_t
{
    const solver_info_t* info = nullptr;
    solver_t* solver = nullptr;
    const island_constraint_t* const* sorted_constraints = nullptr;
    int constraint_count = 0;
    std::vector< solver_rigid_t* > bodies;
    std::vector< solver_manifold_t* > manifolds;
    std::vector< const island_constraint_t* > constraints;
    bool solved = true;     // false once a group held typed constraints (not recreated)
    // The solver's slot 2 (solveGroup). Null calls solver_solve_group; the
    // self-test sets it to record the calls, as the oracle's fake solver does.
    bool ( *solve )( void* user, solver_rigid_t* const* bodies, int body_count, solver_manifold_t* const* manifolds,
        int manifold_count, const island_constraint_t* const* constraints, int constraint_count ) = nullptr;
    void* solve_user = nullptr;
};

inline void island_solve_group( island_callback_t& cb, solver_rigid_t* const* bodies, int body_count,
    solver_manifold_t* const* manifolds, int manifold_count, const island_constraint_t* const* constraints,
    int constraint_count )
{
    const bool ok = cb.solve
        ? cb.solve( cb.solve_user, bodies, body_count, manifolds, manifold_count, constraints, constraint_count )
        : solver_solve_group( *cb.solver, bodies, body_count, manifolds, manifold_count, constraint_count, *cb.info );
    if ( !ok )
        cb.solved = false;
}

// processConstraints @0x602220: one solveGroup over the batch if it holds
// any manifold or constraint, then all three arrays are emptied.
inline void island_process_constraints( island_callback_t& cb )
{
    const int manifold_count = static_cast< int >( cb.manifolds.size() );
    const int constraint_count = static_cast< int >( cb.constraints.size() );
    if ( manifold_count + constraint_count > 0 )
        island_solve_group( cb, cb.bodies.empty() ? nullptr : cb.bodies.data(), static_cast< int >( cb.bodies.size() ),
            manifold_count != 0 ? cb.manifolds.data() : nullptr, manifold_count,
            constraint_count != 0 ? cb.constraints.data() : nullptr, constraint_count );
    cb.bodies.clear();
    cb.manifolds.clear();
    cb.constraints.clear();
}

// processIsland @0x601E90 (callback slot 1).
inline void island_process( island_callback_t& cb, solver_rigid_t* const* bodies, int body_count,
    solver_manifold_t* const* manifolds, int manifold_count, int island_id )
{
    if ( island_id < 0 )
    {
        if ( manifold_count + cb.constraint_count != 0 )
            island_solve_group( cb, bodies, body_count, manifolds, manifold_count, cb.sorted_constraints, cb.constraint_count );
        return;
    }

    // The first constraint of this island, then every one with its id from
    // there to the END of the list - the count does not stop at a mismatch.
    const island_constraint_t* const* start = nullptr;
    int i = 0;
    for ( ; i < cb.constraint_count; ++i )
    {
        if ( island_constraint_id( cb.sorted_constraints[ i ] ) == island_id )
        {
            start = cb.sorted_constraints + i;
            break;
        }
    }
    int constraint_count = 0;
    for ( ; i < cb.constraint_count; ++i )
        if ( island_constraint_id( cb.sorted_constraints[ i ] ) == island_id )
            ++constraint_count;

    if ( cb.info->minimum_solver_batch_size > 1 )
    {
        for ( int b = 0; b < body_count; ++b )
            cb.bodies.push_back( bodies[ b ] );
        for ( int k = 0; k < manifold_count; ++k )
            cb.manifolds.push_back( manifolds[ k ] );
        for ( int k = 0; k < constraint_count; ++k )
            cb.constraints.push_back( start[ k ] );
        if ( static_cast< int >( cb.manifolds.size() + cb.constraints.size() ) > cb.info->minimum_solver_batch_size )
            island_process_constraints( cb );
        return;
    }
    if ( manifold_count + constraint_count != 0 )
        island_solve_group( cb, bodies, body_count, manifolds, manifold_count, start, constraint_count );
}

// btSimulationIslandManager::buildAndProcessIslands @0x624D20. `process` is
// the callback's processIsland (slot 1): (bodies, count, manifolds, count,
// island id).
template < class Process >
inline void island_build_and_process( island_manager_t& m, solver_rigid_t* const* objects, int object_count,
    solver_manifold_t* const* manifolds, int manifold_count, Process&& process )
{
    island_build( m, objects, manifolds, manifold_count );

    if ( !m.split_islands )
    {
        process( objects, object_count, manifolds, manifold_count, -1 );
        return;
    }

    const int island_manifold_count = static_cast< int >( m.island_manifolds.size() );
    if ( island_manifold_count > 1 )
        island_quick_sort( m.island_manifolds.data(), 0, island_manifold_count - 1, island_manifold_id );

    const int count = static_cast< int >( m.elements.size() );
    int start_manifold = 0;
    int end_manifold = 1;
    for ( int start = 0; start < count; )
    {
        const int island_id = m.elements[ start ].id;
        bool sleeping = true;
        int end = start;
        for ( ; end < count && m.elements[ end ].id == island_id; ++end )
        {
            solver_rigid_t* o = objects[ m.elements[ end ].sz ];
            m.island_bodies.push_back( o );
            if ( o->activation_state != 2 && o->activation_state != 5 )
                sleeping = false;
        }

        solver_manifold_t* const* first = nullptr;
        int island_manifolds = 0;
        if ( start_manifold < island_manifold_count
            && island_manifold_id( m.island_manifolds[ start_manifold ] ) == island_id )
        {
            first = m.island_manifolds.data() + start_manifold;
            end_manifold = start_manifold + 1;
            while ( end_manifold < island_manifold_count && island_manifold_id( m.island_manifolds[ end_manifold ] ) == island_id )
                ++end_manifold;
            island_manifolds = end_manifold - start_manifold;
        }

        if ( !sleeping )
            process( m.island_bodies.data(), static_cast< int >( m.island_bodies.size() ), first, island_manifolds, island_id );
        if ( island_manifolds != 0 )
            start_manifold = end_manifold;
        m.island_bodies.clear();
        start = end;
    }
}

// btDiscreteDynamicsWorld::solveConstraints @0x601B60: the constraints are
// sorted by island id (0x605D90), the islands built and processed, the last
// batch flushed. The solver's prepareSolve (slot 1, 0x410B30) and allSolved
// (slot 3, 0x4FB9A0) are empty in retail (`retn 8` / `retn 0Ch`). Returns
// false if a group held typed constraints, whose solve is not recreated.
inline bool island_solve_constraints( solver_t& solver, const solver_info_t& info, island_manager_t& m,
    solver_rigid_t* const* objects, int object_count, solver_manifold_t* const* manifolds, int manifold_count,
    const island_constraint_t* const* constraints, int constraint_count,
    decltype( island_callback_t::solve ) solve = nullptr, void* solve_user = nullptr )
{
    std::vector< const island_constraint_t* > sorted( constraints, constraints + constraint_count );
    if ( constraint_count > 1 )
        island_quick_sort( sorted.data(), 0, constraint_count - 1, island_constraint_id );

    island_callback_t cb;
    cb.info = &info;
    cb.solver = &solver;
    cb.solve = solve;
    cb.solve_user = solve_user;
    cb.sorted_constraints = constraint_count != 0 ? sorted.data() : nullptr;
    cb.constraint_count = constraint_count;

    island_build_and_process( m, objects, object_count, manifolds, manifold_count,
        [ &cb ]( solver_rigid_t* const* bodies, int body_count, solver_manifold_t* const* island_manifolds,
            int island_manifold_count, int island_id )
        { island_process( cb, bodies, body_count, island_manifolds, island_manifold_count, island_id ); } );
    island_process_constraints( cb );
    return cb.solved;
}
