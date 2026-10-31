#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

// Retail objects recreated as what their constructors leave in memory: a
// byte image at retail's offsets, plus every pointer between the objects of
// one graph as (slot, object, offset). Nothing reads these members yet, so
// they are kept as images; a member gets a name when the code that reads it
// is recreated. An address is the allocator's, never the game's, so a
// pointer is recorded by what it points at, not by its value.

inline void image_put32( std::uint8_t* image, int at, std::uint32_t value )
{
    std::memcpy( image + at, &value, 4 );
}

inline void image_zero( std::uint8_t* image, int first_word, int last_word )
{
    for ( int word = first_word; word <= last_word; ++word )
        image_put32( image, 4 * word, 0 );
}

// An MSVC x86 std::string's empty state as its ctor (0x407860) writes it:
// capacity 15 at +20, size 0 at +16, a NUL first byte.
inline void image_empty_string( std::uint8_t* image, int at )
{
    image_put32( image, at + 20, 15 );
    image_put32( image, at + 16, 0 );
    image[ at ] = 0;
}

struct image_ref_t
{
    int slot;
    int object;
    int offset;
    bool operator==( const image_ref_t& other ) const
    {
        return slot == other.slot && object == other.object && offset == other.offset;
    }
};

struct image_object_t
{
    std::vector< std::uint8_t > image;
    std::vector< image_ref_t > refs;
};

// A VuBasicProperty<VuVector3, 8> node as 0x534680 builds one (0x38 bytes):
// the name (an .rdata address) at +4, a u16 1 at +8, +0x0C / +0x10 = 0 (the
// next node, linked after), the target's 16 bytes copied into the default
// (+0x14) and the current value (+0x24), the target at +0x34. Rotation's
// node is then re-tagged VuRotation3dProperty (vtable 0x73D24C).
inline image_object_t vu_vector3_property( std::uint32_t vtable, std::uint32_t name, const image_object_t& owner,
    int owner_index, int target )
{
    image_object_t p;
    p.image.assign( 0x38, 0 );
    std::uint8_t* b = p.image.data( );
    image_put32( b, 4, name );
    const std::uint16_t one = 1;
    std::memcpy( b + 8, &one, 2 );
    image_put32( b, 12, 0 );
    image_put32( b, 16, 0 );
    image_put32( b, 0, vtable );
    std::memcpy( b + 20, owner.image.data( ) + target, 16 );
    std::memcpy( b + 36, owner.image.data( ) + target, 16 );
    p.refs.push_back( { 52, owner_index, target } );
    return p;
}

// The graph the VuEntity ctor 0x500710(this, a2) builds, in this order:
//   0 the VuEntity (vtable 0x74C234, 0x5C bytes): a refcount 1 at +4, an
//     empty std::map at +0x0C (its size at +0x10), a2 at +0x14, an empty
//     string at +0x18, the FNV-1a offset basis 0x811C9DC5 at +0x30 (the hash
//     of the empty name), the component list at +0x48 and +0x50;
//   1 the map's head node (0x515800, 0x18 bytes): left / parent / right all
//     the head itself, color and isnil at +0x0C = 0x0101;
//   2 its VuTransformComponent (0x534180 on operator new(0xC8), the entity
//     as owner): vtable 0x74D324, the static identity 0x7593B0 at +0x10 and
//     +0x50, rotation / world euler 0, scale 1, +0xC0 = 0, +0xC4 = -1;
//   3-5 the component's property nodes Position (+0x40), Rotation (+0x90),
//     Scale (+0xB0), listed at +4, linked through +0x10.
inline std::vector< image_object_t > vu_entity( std::uint32_t a2 )
{
    std::vector< image_object_t > graph( 3 );
    graph[ 0 ].image.assign( 0x5C, 0 );
    graph[ 1 ].image.assign( 0x18, 0 );
    graph[ 2 ].image.assign( 0xC8, 0 );

    std::uint8_t* e = graph[ 0 ].image.data( );
    image_put32( e, 4, 1 );
    image_put32( e, 8, 0 );
    image_put32( e, 0, 0x0074C234u );
    image_put32( e, 16, 0 );
    graph[ 0 ].refs.push_back( { 12, 1, 0 } );
    image_put32( e, 20, a2 );
    image_empty_string( e, 24 );
    image_put32( e, 48, 0x811C9DC5u );
    image_zero( e, 13, 19 );
    image_zero( e, 21, 22 );

    std::uint8_t* h = graph[ 1 ].image.data( );
    for ( int slot : { 0, 4, 8 } )
        graph[ 1 ].refs.push_back( { slot, 1, 0 } );
    const std::uint16_t nil = 0x0101;
    std::memcpy( h + 12, &nil, 2 );

    std::uint8_t* c = graph[ 2 ].image.data( );
    image_put32( c, 4, 0 );
    graph[ 2 ].refs.push_back( { 8, 0, 0 } );
    image_put32( c, 12, 0 );
    image_put32( c, 0, 0x0074D324u );
    for ( int at : { 16, 80 } )
    {
        for ( int word = 0; word < 16; ++word )
            image_put32( c, at + 4 * word, ( word % 5 == 0 ) ? 0x3F800000u : 0u );
    }
    image_zero( c, 36, 38 );
    image_zero( c, 40, 42 );
    for ( int word = 44; word <= 46; ++word )
        image_put32( c, 4 * word, 0x3F800000u );
    image_put32( c, 192, 0 );
    image_put32( c, 196, 0xFFFFFFFFu );

    graph.push_back( vu_vector3_property( 0x00737A84u, 0x006CDEDCu, graph[ 2 ], 2, 64 ) );
    graph.push_back( vu_vector3_property( 0x0073D24Cu, 0x006CDF48u, graph[ 2 ], 2, 144 ) );
    graph.push_back( vu_vector3_property( 0x00737A84u, 0x006CDF6Cu, graph[ 2 ], 2, 176 ) );
    graph[ 2 ].refs.insert( graph[ 2 ].refs.begin( ), { 4, 3, 0 } );
    graph[ 3 ].refs.insert( graph[ 3 ].refs.begin( ), { 16, 4, 0 } );
    graph[ 4 ].refs.insert( graph[ 4 ].refs.begin( ), { 16, 5, 0 } );

    // The component list: the entity's +0x48 (empty, so the component becomes
    // its head) and +0x50.
    graph[ 0 ].refs.push_back( { 72, 2, 0 } );
    graph[ 0 ].refs.push_back( { 80, 2, 0 } );
    return graph;
}
