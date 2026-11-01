#pragma once

// MSVC 11's std::sort (VS2012 <algorithm>, the library the retail binary links), as retail's instantiation for
// VuEntity's child list runs it (std::sort( children, 0x500690 ) in 0x501300 -> _Sort 0x501810). Read from the
// disassembly of every piece and matched to the VC11 source shape:
//   0x501810  _Sort( first, last, ideal = count, pred ): while count > 32 (_ISORT_MAX) and ideal > 0, partition;
//             ideal = ideal / 2 + ( ideal / 2 ) / 2 (signed, cdq / sar); recurse into the SMALLER side ((mid.first -
//             first) < (last - mid.second): the left, else the right), loop on the other. Then count > 32: make_heap
//             (only when count >= 2) + sort_heap; else count > 1: insertion sort.
//   0x501900  _Unguarded_partition: mid = first + count / 2, _Median( first, mid, last - 1 ), the equal run grown
//             both ways from mid, then the three-way scan with its iter_swaps (order below kept exactly).
//   0x501AF0  _Median: more than 40 elements (last - first, last = the final element) -> Tukey's ninther with step
//             ( n + 1 ) / 8 (n = last - first), else _Med3( first, mid, last ). _Med3 inlined.
//   0x501CC0  _Make_heap (its _Adjust_heap inlined), 0x501DA0 _Sort_heap (_Pop_heap -> 0x501EB0 _Adjust_heap with
//             the _Push_heap tail), 0x501E00 _Insertion_sort (memmove 0x64C144 = std::move_backward here).
// Every pred call keeps retail's argument order (cdecl: the LAST pushed word is the first argument). The sort is
// not stable: equal keys come out in the order this exact algorithm leaves them.

#include <cstdint>
#include <utility>

template < typename T, typename Pred >
inline void vc_sort_med3( T* a, T* b, T* c, Pred& pred )
{
    if ( pred( *b, *a ) )
        std::swap( *b, *a );
    if ( pred( *c, *b ) )
    {
        std::swap( *c, *b );
        if ( pred( *b, *a ) )
            std::swap( *b, *a );
    }
}

// 0x501AF0 ( first, mid, last ) - `last` is the final element (the caller's last - 1).
template < typename T, typename Pred >
inline void vc_sort_median( T* first, T* mid, T* last, Pred& pred )
{
    const std::int32_t n = static_cast< std::int32_t >( last - first );
    if ( n > 40 )
    {
        const std::int32_t step = ( n + 1 ) / 8;
        const std::int32_t two = 2 * step;
        vc_sort_med3( first, first + step, first + two, pred );
        vc_sort_med3( mid - step, mid, mid + step, pred );
        vc_sort_med3( last - two, last - step, last, pred );
        vc_sort_med3( first + step, mid, last - step, pred );
    }
    else
        vc_sort_med3( first, mid, last, pred );
}

// 0x501900: answers ( pfirst, plast ), the run of elements equal to the pivot.
template < typename T, typename Pred >
inline std::pair< T*, T* > vc_sort_partition( T* first, T* last, Pred& pred )
{
    T* mid = first + static_cast< std::int32_t >( last - first ) / 2;
    vc_sort_median( first, mid, last - 1, pred );
    T* pfirst = mid;
    T* plast = pfirst + 1;
    while ( first < pfirst && !pred( *( pfirst - 1 ), *pfirst ) && !pred( *pfirst, *( pfirst - 1 ) ) )
        --pfirst;
    while ( plast < last && !pred( *plast, *pfirst ) && !pred( *pfirst, *plast ) )
        ++plast;
    T* gfirst = plast;
    T* glast = pfirst;
    for ( ;; )
    {
        for ( ; gfirst < last; ++gfirst )
        {
            if ( pred( *pfirst, *gfirst ) )
                continue;
            if ( pred( *gfirst, *pfirst ) )
                break;
            std::swap( *plast, *gfirst );
            ++plast;
        }
        for ( ; first < glast; --glast )
        {
            if ( pred( *( glast - 1 ), *pfirst ) )
                continue;
            if ( pred( *pfirst, *( glast - 1 ) ) )
                break;
            --pfirst;
            std::swap( *pfirst, *( glast - 1 ) );
        }
        if ( glast == first && gfirst == last )
            return { pfirst, plast };
        if ( glast == first )
        {
            if ( plast != gfirst )
                std::swap( *pfirst, *plast );
            ++plast;
            std::swap( *pfirst, *gfirst );
            ++pfirst;
            ++gfirst;
        }
        else if ( gfirst == last )
        {
            --glast;
            --pfirst;
            if ( glast != pfirst )
                std::swap( *glast, *pfirst );
            --plast;
            std::swap( *pfirst, *plast );
        }
        else
        {
            --glast;
            std::swap( *gfirst, *glast );
            ++gfirst;
        }
    }
}

// 0x501EB0 _Adjust_heap( first, hole, bottom, val ) with _Push_heap's tail (top = the starting hole).
template < typename T, typename Pred >
inline void vc_sort_adjust_heap( T* first, std::int32_t hole, std::int32_t bottom, T&& val, Pred& pred )
{
    const std::int32_t top = hole;
    std::int32_t idx = 2 * hole + 2;
    for ( ; idx < bottom; idx = 2 * idx + 2 )
    {
        if ( pred( first[ idx ], first[ idx - 1 ] ) )
            --idx;
        first[ hole ] = std::move( first[ idx ] );
        hole = idx;
    }
    if ( idx == bottom )
    {
        first[ hole ] = std::move( first[ bottom - 1 ] );
        hole = bottom - 1;
    }
    for ( idx = ( hole - 1 ) / 2; top < hole && pred( first[ idx ], val ); idx = ( hole - 1 ) / 2 )
    {
        first[ hole ] = std::move( first[ idx ] );
        hole = idx;
    }
    first[ hole ] = std::move( val );
}

// 0x501CC0 _Make_heap: every hole from count / 2 - 1 down to 0.
template < typename T, typename Pred >
inline void vc_sort_make_heap( T* first, T* last, Pred& pred )
{
    const std::int32_t bottom = static_cast< std::int32_t >( last - first );
    for ( std::int32_t hole = bottom / 2; 0 < hole; )
    {
        --hole;
        T val = std::move( first[ hole ] );
        vc_sort_adjust_heap( first, hole, bottom, std::move( val ), pred );
    }
}

// 0x501DA0 _Sort_heap: pop while more than one element is left.
template < typename T, typename Pred >
inline void vc_sort_sort_heap( T* first, T* last, Pred& pred )
{
    for ( ; 1 < last - first; --last )
    {
        T val = std::move( *( last - 1 ) );
        *( last - 1 ) = std::move( *first );
        vc_sort_adjust_heap( first, 0, static_cast< std::int32_t >( ( last - 1 ) - first ), std::move( val ), pred );
    }
}

// 0x501E00 _Insertion_sort.
template < typename T, typename Pred >
inline void vc_sort_insertion( T* first, T* last, Pred& pred )
{
    if ( first == last )
        return;
    for ( T* next = first; ++next != last; )
    {
        T val = std::move( *next );
        if ( pred( val, *first ) )
        {
            for ( T* p = next; p != first; --p )
                *p = std::move( *( p - 1 ) );
            *first = std::move( val );
        }
        else
        {
            T* hole = next;
            for ( T* prev = hole - 1; pred( val, *prev ); --prev )
            {
                *hole = std::move( *prev );
                hole = prev;
            }
            *hole = std::move( val );
        }
    }
}

// 0x501810 _Sort.
template < typename T, typename Pred >
inline void vc_sort_range( T* first, T* last, std::int32_t ideal, Pred& pred )
{
    std::int32_t count;
    for ( ; 32 < ( count = static_cast< std::int32_t >( last - first ) ) && 0 < ideal; )
    {
        const std::pair< T*, T* > mid = vc_sort_partition( first, last, pred );
        ideal = ideal / 2;
        ideal += ideal / 2;
        if ( mid.first - first < last - mid.second )
        {
            vc_sort_range( first, mid.first, ideal, pred );
            first = mid.second;
        }
        else
        {
            vc_sort_range( mid.second, last, ideal, pred );
            last = mid.first;
        }
    }
    if ( 32 < count )
    {
        if ( 2 <= last - first )
            vc_sort_make_heap( first, last, pred );
        vc_sort_sort_heap( first, last, pred );
    }
    else if ( 1 < count )
        vc_sort_insertion( first, last, pred );
}

// std::sort( first, last, pred ) as VC11 builds it: _Sort( first, last, last - first, pred ).
template < typename T, typename Pred >
inline void vc_sort( T* first, T* last, Pred pred )
{
    vc_sort_range( first, last, static_cast< std::int32_t >( last - first ), pred );
}
