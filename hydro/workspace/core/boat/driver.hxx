#pragma once

#include <workspace/core/entity/object.hxx>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

// The boat's AI driver, as postLoad 0x41CFF0 builds it for Driver Type "AI"
// (the boat ctor's default): operator new(0xD4), 0x41D2E0 on the block (its
// stack argument unused; ret 4), +0x8C = 0 and +0xBC = 0, then VuAiDriver's
// ctor 0x40B690(boat) - the driver goes to boat +0x220. Recreated as the
// object graph it leaves (entity/object.hxx), in retail's allocation order:
//   0 the driver (0xD4), 1-5 its VuEntity part's map head, transform
//   component and three property nodes (vu_entity), 6 the 0x20-byte
//   _aligned_malloc block of the array at +0x84.
// A ref to the boat is object AI_DRIVER_REF_BOAT.

constexpr int AI_DRIVER_REF_BOAT = -1;

// 0x4D0CC0 (thiscall, ret 4 - its argument unused) at `at`: vtable 0x746CDC,
// +4 = 8, +0x0C = 0, an _aligned_malloc(0x20, 16) array at +8 (memcpy_s of
// nothing into it, the old - null - one freed), capacity +0x10 = 8.
inline void ai_driver_4d0cc0( std::vector< image_object_t >& g, int object, int at )
{
    std::uint8_t* b = g[ object ].image.data( );
    image_put32( b, at, 0x00746CDCu );
    image_put32( b, at + 4, 8 );
    image_put32( b, at + 12, 0 );
    image_put32( b, at + 16, 0 );
    g.push_back( {} );
    g.back( ).image.assign( 0x20, 0 );   // never written: its bytes are the allocator's
    g[ object ].refs.push_back( { at + 8, static_cast< int >( g.size( ) ) - 1, 0 } );
    image_put32( g[ object ].image.data( ), at + 16, 8 );
}

inline std::vector< image_object_t > ai_driver_construct( )
{
    std::vector< image_object_t > g( 1 );
    g[ 0 ].image.assign( 0xD4, 0 );
    std::uint8_t* b = g[ 0 ].image.data( );
    // 0x41D2E0: zeroes +8, +0x0C, +0x34..+0x5C and +0x7C (every one rewritten below).
    for ( int at : { 0x08, 0x0C, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x7C } )
        image_put32( b, at, 0 );
    image_put32( b, 0x8C, 0 );
    image_put32( b, 0xBC, 0 );
    // 0x40B690: the VuEntity base 0x500710(this, 0) - its 0x5C bytes and objects 1-5.
    const std::vector< image_object_t > entity = vu_entity( 0 );
    std::memcpy( b, entity[ 0 ].image.data( ), 0x5C );
    g[ 0 ].refs = entity[ 0 ].refs;
    for ( std::size_t k = 1; k < entity.size( ); ++k )
        g.push_back( entity[ k ] );
    b = g[ 0 ].image.data( );
    g[ 0 ].refs.push_back( { 0x5C, AI_DRIVER_REF_BOAT, 0 } );   // the boat
    image_empty_string( b, 0x60 );
    image_put32( b, 0x78, 0xFFFFFFFFu );
    image_put32( b, 0x7C, 0 );
    image_put32( b, 0x00, 0x00737B5Cu );   // VuAiDriver
    image_put32( b, 0x80, 1 );
    ai_driver_4d0cc0( g, 0, 0x84 );
    b = g[ 0 ].image.data( );
    image_put32( b, 0x84, 0x00746CC8u );
    for ( int at : { 0x98, 0x9C, 0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB8, 0xBC, 0xC0, 0xC4, 0xC8, 0xCC } )
        image_put32( b, at, 0 );
    b[ 0xD0 ] = 0;
    // g_vudevconfig's getParam("DebugDrivers") (vt+0x18): a bool (tag 3)
    // there sets the .data byte 0x758F8A - the dev config is compiled out
    // (slot 6 0x4FDB70 returns the static null container), so never.
    return g;
}