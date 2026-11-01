#pragma once

#include <workspace/core/world/pfxclass.hxx>

#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// THE VuPfx RUNTIME ON RETAIL'S OWN MEMORY SHAPE. Retail keeps every system,
// pattern and process instance and every particle in four fixed pools owned
// by VuPfxResources (g_vupfximpl 0x75904C +8), linked by raw pointers. This
// file recreates that machine word for word over a 32-bit address space
// (pfx_heap_t: regions of words at retail-sized addresses, 0 = null), so a
// state can be compared with retail's memory directly.
//
// VuPfxResources (0x64 bytes, ctor 0x58B5B0):
//   +0x00 .. +0x0C the four blocks (systems, patterns, processes, particles);
//   +0x10 / +0x14 / +0x18 the free systems (first, last, count; links +4 next,
//   +8 previous); +0x1C .. +0x24 the patterns', +0x28 .. +0x30 the processes'
//   (the same links); +0x34 / +0x38 / +0x3C the free particles (links +0 next,
//   +4 previous); +0x40 .. +0x5C four (count, size) pairs - the ctor's 512 /
//   188, 1024 / 108, 4096 / 52, 16384 / 100, VuPfxImpl's init 0x51C840 passes
//   the same; +0x60 particles allocated since the last VuPfxImpl tick.
// A system instance: +0x0C its VuPfxSystem node, +0x10 .. +0x18 its patterns
// (first, last, count). A pattern instance: +0x0C its system instance, +0x10
// its node, +0x14 .. +0x1C its processes, +0x20 .. +0x28 its particles (first,
// last, count; links +0 / +4). A process instance: +0x0C its pattern, +0x10
// its node. A node: +0x0C its children (an MSVC std::map by name - here the
// caller's `children` list in the map's order), its params from +0x30.
struct pfx_heap_t
{
    struct region_t
    {
        std::uint32_t base = 0;
        std::vector< std::uint32_t > words;
    };
    std::vector< region_t > regions;

    std::uint32_t* find( std::uint32_t a )
    {
        for ( region_t& r : regions )
            if ( a >= r.base && a - r.base < 4u * r.words.size( ) )
                return &r.words[ ( a - r.base ) / 4u ];
        return nullptr;
    }
    std::uint32_t& operator[]( std::uint32_t a )
    {
        static std::uint32_t sink = 0;
        std::uint32_t* p = find( a );
        return p != nullptr ? *p : ( sink = 0, sink );
    }
    float f( std::uint32_t a )
    {
        float v;
        std::memcpy( &v, &( *this )[ a ], 4 );
        return v;
    }
    void set( std::uint32_t a, float v ) { std::memcpy( &( *this )[ a ], &v, 4 ); }
    void add( std::uint32_t base, std::uint32_t bytes )
    {
        region_t r;
        r.base = base;
        r.words.assign( ( bytes + 3u ) / 4u, 0u );
        regions.push_back( std::move( r ) );
    }
};

// What the runtime asks of its caller: the children of a node in map order,
// a pattern instance's tick (its vtable slot 1 - quad 0x591C30, geom 0x58FEA0,
// trail 0x58F0C0), with `user`.
struct pfx_sim_hooks_t
{
    void* user = nullptr;
    const std::vector< std::uint32_t >* ( *children )( void* user, std::uint32_t node ) = nullptr;
    void ( *pattern_tick )( void* user, pfx_heap_t& h, std::uint32_t pattern, float dt ) = nullptr;
};

// The node class a node's vtable names (g_pfx_classes), or null.
inline const pfx_class_t* pfx_node_class( pfx_heap_t& h, std::uint32_t node )
{
    const std::uint32_t vt = h[ node ];
    for ( const pfx_class_t& c : g_pfx_classes )
        if ( c.vtable == vt )
            return &c;
    return nullptr;
}

// The two list shapes. `list` addresses the (first, last, count) triple.
// Instances: next +4, previous +8; append at the last end (0x58B950 et al.),
// pop from the last end (0x58B880 et al.).
inline void pfx_list_append( pfx_heap_t& h, std::uint32_t list, std::uint32_t b )
{
    h[ b + 4 ] = 0u;
    const std::uint32_t last = h[ list + 4 ];
    if ( last != 0u )
    {
        h[ b + 8 ] = last;
        h[ last + 4 ] = b;
    }
    else
    {
        h[ b + 8 ] = 0u;
        h[ list ] = b;
    }
    ++h[ list + 8 ];
    h[ list + 4 ] = b;
}

inline std::uint32_t pfx_list_pop( pfx_heap_t& h, std::uint32_t list )
{
    const std::uint32_t b = h[ list + 4 ];
    if ( b == 0u )
        return 0u;
    const std::uint32_t prev = h[ b + 8 ];
    const std::uint32_t next = h[ b + 4 ];
    if ( b == h[ list ] )
        h[ list ] = next;
    h[ list + 4 ] = prev;
    if ( next != 0u )
        h[ next + 8 ] = prev;
    if ( prev != 0u )
        h[ prev + 4 ] = next;
    h[ b + 4 ] = 0u;
    h[ b + 8 ] = 0u;
    --h[ list + 8 ];
    return b;
}

// Particles: next +0, previous +4. The free pool pops and pushes at its +0x38
// end (0x58B8E0 / the retirements); a pattern's list takes new particles at
// its first end (0x58B570).
inline std::uint32_t pfx_particle_pop( pfx_heap_t& h, std::uint32_t r )
{
    const std::uint32_t p = h[ r + 0x38 ];
    if ( p == 0u )
        return 0u;
    const std::uint32_t next = h[ p ];
    const std::uint32_t prev = h[ p + 4 ];
    if ( p == h[ r + 0x34 ] )
        h[ r + 0x34 ] = next;
    h[ r + 0x38 ] = prev;
    if ( next != 0u )
        h[ next + 4 ] = prev;
    if ( prev != 0u )
        h[ prev ] = next;
    h[ p ] = 0u;
    h[ p + 4 ] = 0u;
    --h[ r + 0x3C ];
    return p;
}

inline void pfx_particle_free( pfx_heap_t& h, std::uint32_t r, std::uint32_t p )   // the push of 0x58B3F0 / 0x58ACC0
{
    h[ p ] = 0u;
    const std::uint32_t end = h[ r + 0x38 ];
    if ( end != 0u )
    {
        h[ p + 4 ] = end;
        h[ end ] = p;
    }
    else
    {
        h[ p + 4 ] = 0u;
        h[ r + 0x34 ] = p;
    }
    ++h[ r + 0x3C ];
    h[ r + 0x38 ] = p;
}

// The particle list's own unlink from its +4 (last) end, as 0x58B3F0 / 0x58ACC0
// empty a pattern: first +0x20, last +0x24, count +0x28.
inline std::uint32_t pfx_particle_unlink_last( pfx_heap_t& h, std::uint32_t pattern )
{
    const std::uint32_t p = h[ pattern + 0x24 ];
    if ( p == 0u )
        return 0u;
    const std::uint32_t next = h[ p ];
    const std::uint32_t prev = h[ p + 4 ];
    if ( p == h[ pattern + 0x20 ] )
        h[ pattern + 0x20 ] = next;
    h[ pattern + 0x24 ] = prev;
    if ( next != 0u )
        h[ next + 4 ] = prev;
    if ( prev != 0u )
        h[ prev ] = next;
    h[ p ] = 0u;
    h[ p + 4 ] = 0u;
    --h[ pattern + 0x28 ];
    return p;
}

// 0x58B5B0 VuPfxResources' ctor.
inline void pfx_resources_ctor( pfx_heap_t& h, std::uint32_t r )
{
    for ( int k = 0; k < 16; ++k )
        h[ r + 4 * k ] = 0u;
    const std::uint32_t d[ 8 ] = { 512u, 188u, 1024u, 108u, 4096u, 52u, 0x4000u, 100u };
    for ( int k = 0; k < 8; ++k )
        h[ r + 0x40 + 4 * k ] = d[ k ];
    h[ r + 0x60 ] = 0u;
}

// 0x58B670 VuPfxResources' init (VuPfxImpl slot 6 0x51D6B0): the (count, size)
// pairs to +0x40, the old blocks deleted and four new ones made by 0x58B9A0 -
// which first rounds every size up to a multiple of 16 in place, then calls
// operator new (the platform: `blocks` are their addresses, each count x the
// rounded size bytes, regions of `h`) -, each zeroed and every block appended to its free
// list in address order (particles: pushed at the +0x38 end).
inline void pfx_resources_init( pfx_heap_t& h, std::uint32_t r, const std::uint32_t* pairs, const std::uint32_t* blocks )
{
    for ( int k = 0; k < 8; ++k )
        h[ r + 0x40 + 4 * k ] = pairs[ k ];
    for ( int k = 0; k < 4; ++k )
    {
        h[ r + 0x44 + 8 * k ] = ( h[ r + 0x44 + 8 * k ] + 15u ) & 0xFFFFFFF0u;
        h[ r + 4 * k ] = blocks[ k ];
    }
    for ( int pool = 0; pool < 3; ++pool )
    {
        const std::uint32_t list = r + 0x10 + 0xC * pool;
        h[ list ] = h[ list + 4 ] = h[ list + 8 ] = 0u;
        const std::uint32_t count = h[ r + 0x40 + 8 * pool ], size = h[ r + 0x44 + 8 * pool ];
        for ( std::uint32_t o = 0; o < count * size; o += 4 )
            h[ blocks[ pool ] + o ] = 0u;
        for ( std::uint32_t i = 0; i < count; ++i )
            pfx_list_append( h, list, blocks[ pool ] + i * size );
    }
    h[ r + 0x34 ] = h[ r + 0x38 ] = h[ r + 0x3C ] = 0u;
    const std::uint32_t count = h[ r + 0x58 ], size = h[ r + 0x5C ];
    for ( std::uint32_t o = 0; o < count * size; o += 4 )
        h[ blocks[ 3 ] + o ] = 0u;
    for ( std::uint32_t i = 0; i < count; ++i )
        pfx_particle_free( h, r, blocks[ 3 ] + i * size );
}

// 0x58A830 VuPfxSystemInstance's ctor: the vtable 0x74FF8C, the pattern list 0,
// the matrix the static identity 0x7593B0, +0x5C .. +0x64, +0x6C .. +0x74 and
// +0x7C 0, the AABB +0x80 .. +0x98 0 but its pads +0x8C / +0x9C (stack words
// retail never writes: UNKNOWN, not written), +0xA0 / +0xA4 0, the scale +0xA8
// and the tint +0xAC .. +0xB8 1.0.
inline void pfx_system_ctor( pfx_heap_t& h, std::uint32_t s )
{
    h[ s ] = 0x74FF8Cu;
    h[ s + 0x10 ] = h[ s + 0x14 ] = h[ s + 0x18 ] = 0u;
    static const float identity[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    for ( int k = 0; k < 16; ++k )
        h.set( s + 0x1C + 4 * k, identity[ k ] );
    for ( std::uint32_t o : { 0x5Cu, 0x60u, 0x64u, 0x6Cu, 0x70u, 0x74u, 0x7Cu, 0x80u, 0x84u, 0x88u, 0x90u, 0x94u, 0x98u, 0xA0u, 0xA4u } )
        h[ s + o ] = 0u;
    for ( std::uint32_t o = 0xA8u; o <= 0xB8u; o += 4u )
        h[ s + o ] = 0x3F800000u;
}

// 0x58B3F0 a pattern instance's release: every process back to the free
// processes (popped from its last end, appended), then every particle back to
// the free particles.
inline void pfx_pattern_release( pfx_heap_t& h, std::uint32_t r, std::uint32_t pattern )
{
    for ( std::uint32_t b; ( b = pfx_list_pop( h, pattern + 0x14 ) ) != 0u; )
        pfx_list_append( h, r + 0x28, b );
    for ( std::uint32_t p; ( p = pfx_particle_unlink_last( h, pattern ) ) != 0u; )
        pfx_particle_free( h, r, p );
}

// 0x58B2D0 a pattern instance's processes: per child node in map order, fail
// when its instance size (slot 6) exceeds the process block size (+0x54) or no
// block is free; else the node's create (slot 7) on a popped block, +0x10 the
// node, +0x0C the pattern, appended to the pattern's list.
inline bool pfx_pattern_create( pfx_heap_t& h, std::uint32_t r, std::uint32_t pattern, const pfx_sim_hooks_t& k )
{
    const std::vector< std::uint32_t >* kids = k.children( k.user, h[ pattern + 0x10 ] );
    for ( std::size_t i = 0; kids != nullptr && i < kids->size( ); ++i )
    {
        const std::uint32_t node = ( *kids )[ i ];
        const pfx_class_t* c = pfx_node_class( h, node );
        if ( c == nullptr || c->instance_size > h[ r + 0x54 ] )
            return false;
        const std::uint32_t b = pfx_list_pop( h, r + 0x28 );
        if ( b == 0u || h.find( b ) == nullptr )   // the second: an address outside the heap (never, on retail's own state)
            return false;
        pfx_create_class( c->create, h.find( b ) );
        h[ b + 0x10 ] = node;
        h[ b + 0x0C ] = pattern;
        pfx_list_append( h, pattern + 0x14, b );
    }
    return true;
}

// 0x58A940 a system instance's patterns: the same walk over the system node's
// children with the pattern pool (+0x1C, block size +0x4C); a pattern whose
// processes fail is released (0x58B3F0), its block appended to the free
// patterns, and the whole create answers false.
inline bool pfx_system_create( pfx_heap_t& h, std::uint32_t r, std::uint32_t s, const pfx_sim_hooks_t& k )
{
    const std::vector< std::uint32_t >* kids = k.children( k.user, h[ s + 0x0C ] );
    for ( std::size_t i = 0; kids != nullptr && i < kids->size( ); ++i )
    {
        const std::uint32_t node = ( *kids )[ i ];
        const pfx_class_t* c = pfx_node_class( h, node );
        if ( c == nullptr || c->instance_size > h[ r + 0x4C ] )
            return false;
        const std::uint32_t b = pfx_list_pop( h, r + 0x1C );
        if ( b == 0u || h.find( b ) == nullptr )
            return false;
        pfx_create_class( c->create, h.find( b ) );
        h[ b + 0x10 ] = node;
        h[ b + 0x0C ] = s;
        if ( !pfx_pattern_create( h, r, b, k ) )
        {
            pfx_pattern_release( h, r, b );
            pfx_list_append( h, r + 0x1C, b );
            return false;
        }
        pfx_list_append( h, s + 0x10, b );
    }
    return true;
}

// 0x58AA70 a system instance's patterns released (popped from the last end,
// 0x58B3F0 each, appended to the free patterns).
inline void pfx_system_release_patterns( pfx_heap_t& h, std::uint32_t r, std::uint32_t s )
{
    for ( std::uint32_t b; ( b = pfx_list_pop( h, s + 0x10 ) ) != 0u; )
    {
        pfx_pattern_release( h, r, b );
        pfx_list_append( h, r + 0x1C, b );
    }
}

// 0x58B880 a system instance from the free list (0 when none): popped, 0x58A830,
// +0x0C the node.
inline std::uint32_t pfx_system_alloc( pfx_heap_t& h, std::uint32_t r, std::uint32_t node )
{
    const std::uint32_t s = pfx_list_pop( h, r + 0x10 );
    if ( s == 0u )
        return 0u;
    pfx_system_ctor( h, s );
    h[ s + 0x0C ] = node;
    return s;
}

// 0x51DAE0 VuPfxImpl's releaseSystemInstance: 0x58AA70, then the instance back
// to the free systems with +0x0C = 0 (0x58B950's body, inlined).
inline void pfx_system_release( pfx_heap_t& h, std::uint32_t r, std::uint32_t s )
{
    pfx_system_release_patterns( h, r, s );
    pfx_list_append( h, r + 0x10, s );
    h[ s + 0x0C ] = 0u;
}

// 0x51DA70 VuPfxImpl's createSystemInstance after its path lookup 0x51DBD0 (a
// std::map walk by name - the caller's: `node` is what it found, 0 for none):
// 0 unless the node's getType chain holds VuPfxSystem (descriptor 0x736A10);
// then 0x58B880 and 0x58A940, a failed create released (0x58AA70 + 0x58B950).
inline std::uint32_t pfx_system_instance_create( pfx_heap_t& h, std::uint32_t r, std::uint32_t node, const pfx_sim_hooks_t& k )
{
    const pfx_class_t* c = node != 0u ? pfx_node_class( h, node ) : nullptr;
    if ( c == nullptr || std::strstr( c->chain, "VuPfxSystem" ) == nullptr )
        return 0u;
    const std::uint32_t s = pfx_system_alloc( h, r, node );
    if ( s == 0u )
        return 0u;
    if ( pfx_system_create( h, r, s, k ) )
        return s;
    pfx_system_release( h, r, s );
    return 0u;
}

// An instance's slot 0 (reset), by its node's class (pfxclass.hxx): the
// fountains' 0x55D020, the hull's 0x4CE750, the patterns' 0x58FE80 (slot 0 on
// every process, first to last) and the trail's 0x58F080 (the same, then
// +0x4C .. +0x54 0); the rest are nullsubs.
inline void pfx_instance_reset( pfx_heap_t& h, std::uint32_t b )
{
    const pfx_class_t* c = pfx_node_class( h, h[ b + 0x10 ] );
    if ( c == nullptr )
        return;
    switch ( c->reset )
    {
    case 0x55D020u: if ( h.find( b ) != nullptr ) pfx_fountain_reset( h.find( b ) ); break;
    case 0x4CE750u: if ( h.find( b ) != nullptr ) pfx_hull_reset( h.find( b ) ); break;
    case 0x58FE80u:
    case 0x58F080u:
        for ( std::uint32_t p = h[ b + 0x14 ]; p != 0u; p = h[ p + 4 ] )
            pfx_instance_reset( h, p );
        if ( c->reset == 0x58F080u )
            h[ b + 0x4C ] = h[ b + 0x50 ] = h[ b + 0x54 ] = 0u;
        break;
    default: break;
    }
}

// Slot 11 0x58AC90 start: state +0x7C = 1, the clock +0xA4 = 0, slot 0 on every
// pattern.
inline void pfx_system_start( pfx_heap_t& h, std::uint32_t s )
{
    h[ s + 0x7C ] = 1u;
    h[ s + 0xA4 ] = 0u;
    for ( std::uint32_t p = h[ s + 0x10 ]; p != 0u; p = h[ p + 4 ] )
        pfx_instance_reset( h, p );
}

// Slot 12 0x58ACC0 stop( immediate ): immediate -> state 0 and every pattern's
// particles freed (from each list's last end); else state 2.
inline void pfx_system_stop( pfx_heap_t& h, std::uint32_t r, std::uint32_t s, bool immediate )
{
    if ( !immediate )
    {
        h[ s + 0x7C ] = 2u;
        return;
    }
    h[ s + 0x7C ] = 0u;
    for ( std::uint32_t p = h[ s + 0x10 ]; p != 0u; p = h[ p + 4 ] )
        for ( std::uint32_t q; ( q = pfx_particle_unlink_last( h, p ) ) != 0u; )
            pfx_particle_free( h, r, q );
}

// Slot 13 0x58AD70 tick( dt ): the AABB reset (min +0x80 .. +0x88 FLT_MAX, max
// +0x90 .. +0x98 -FLT_MAX; pads +0x8C / +0x9C UNKNOWN), the count +0xA0 = 0;
// unless idle: clock += dt, then per pattern its tick (slot 1 - the hook) and,
// when its count +0x28 is not 0, min = minss( system, pattern +0x2C .. ), max =
// maxss( system, pattern +0x3C .. ), count += its count; state 2 with no
// particles -> 0; Duration (node +0x30) > 0 and state 1 and clock >= Duration
// -> 2 (comiss: NaN never).
inline void pfx_system_tick( pfx_heap_t& h, std::uint32_t s, float dt, const pfx_sim_hooks_t& k )
{
    for ( std::uint32_t o : { 0x80u, 0x84u, 0x88u } )
        h[ s + o ] = 0x7F7FFFFFu;
    for ( std::uint32_t o : { 0x90u, 0x94u, 0x98u } )
        h[ s + o ] = 0xFF7FFFFFu;
    h[ s + 0xA0 ] = 0u;
    if ( h[ s + 0x7C ] == 0u )
        return;
    h.set( s + 0xA4, h.f( s + 0xA4 ) + dt );
    for ( std::uint32_t p = h[ s + 0x10 ]; p != 0u; p = h[ p + 4 ] )
    {
        k.pattern_tick( k.user, h, p, dt );
        if ( h[ p + 0x28 ] == 0u )
            continue;
        for ( std::uint32_t c = 0; c < 3; ++c )
        {
            h.set( s + 0x80 + 4 * c, _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( h.f( s + 0x80 + 4 * c ) ), _mm_set_ss( h.f( p + 0x2C + 4 * c ) ) ) ) );
            h.set( s + 0x90 + 4 * c, _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( h.f( s + 0x90 + 4 * c ) ), _mm_set_ss( h.f( p + 0x3C + 4 * c ) ) ) ) );
        }
        h[ s + 0xA0 ] += h[ p + 0x28 ];
    }
    if ( h[ s + 0x7C ] == 2u && h[ s + 0xA0 ] == 0u )
        h[ s + 0x7C ] = 0u;
    const float duration = h.f( h[ s + 0x0C ] + 0x30 );
    if ( _mm_comigt_ss( _mm_set_ss( duration ), _mm_setzero_ps( ) ) && h[ s + 0x7C ] == 1u &&
         _mm_comige_ss( _mm_set_ss( h.f( s + 0xA4 ) ), _mm_set_ss( duration ) ) )
        h[ s + 0x7C ] = 2u;
}

// The pattern instance's slot 4 0x58B4E0 allocateParticle: 0 unless its system
// is emitting (state 1) and under the node's Max Particle Count (+0x30, 0 = no
// limit) by the pattern's count; then 0x58B8E0 - 0 when the node's particle
// size (slot 8) exceeds the particle block size (+0x5C) or none is free, else
// popped, the node's slot 9 init on it, +0x60 counted - and 0x58B570 puts it
// at the first end of the pattern's list.
inline std::uint32_t pfx_particle_allocate( pfx_heap_t& h, std::uint32_t r, std::uint32_t pattern )
{
    if ( h[ h[ pattern + 0x0C ] + 0x7C ] != 1u )
        return 0u;
    const std::uint32_t node = h[ pattern + 0x10 ];
    const std::uint32_t max = h[ node + 0x30 ];
    if ( max != 0u && static_cast< std::int32_t >( h[ pattern + 0x28 ] ) >= static_cast< std::int32_t >( max ) )
        return 0u;
    const pfx_class_t* c = pfx_node_class( h, node );
    if ( c == nullptr || c->particle_size > h[ r + 0x5C ] )
        return 0u;
    const std::uint32_t p = pfx_particle_pop( h, r );
    if ( p == 0u || h.find( p ) == nullptr )
        return 0u;
    pfx_particle_class( c->particle, h.find( p ) );
    ++h[ r + 0x60 ];
    const std::uint32_t first = h[ pattern + 0x20 ];
    h[ p + 4 ] = 0u;
    if ( first != 0u )
    {
        h[ p ] = first;
        h[ first + 4 ] = p;
    }
    else
    {
        h[ p ] = 0u;
        h[ pattern + 0x24 ] = p;
    }
    ++h[ pattern + 0x28 ];
    h[ pattern + 0x20 ] = p;
    return p;
}

// The pattern's slots 5 / 6 0x58B530 / 0x58B550: the spawn and draw matrices -
// node Space (+0x34) == 1: identity 0x7593B0 / the system matrix; else the
// other way round. Returned as an address; 0 stands for the static identity.
inline std::uint32_t pfx_pattern_spawn_matrix( pfx_heap_t& h, std::uint32_t pattern )
{
    return h[ h[ pattern + 0x10 ] + 0x34 ] == 1u ? 0u : h[ pattern + 0x0C ] + 0x1C;
}

inline std::uint32_t pfx_pattern_draw_matrix( pfx_heap_t& h, std::uint32_t pattern )
{
    return h[ h[ pattern + 0x10 ] + 0x34 ] == 1u ? h[ pattern + 0x0C ] + 0x1C : 0u;
}

// VuPfxImpl's tick 0x51DD80 ("Final" phase, registered by 0x51E390): the dev
// stats (g_vudevstat) and the debug draw (byte +0x1D) are hooks; the
// resources' +0x60 = 0.
inline void pfx_impl_tick( pfx_heap_t& h, std::uint32_t r )
{
    h[ r + 0x60 ] = 0u;
}
