#pragma once

// VuCollisionManager's game side (g 0x758FBC; ctor 0x4ACDF0, vtables 0x745434
// and the contact-listener interface 0x74542C at +4 - its small slots are
// engine dynamics/wrapper.hxx's). The dynamics world tells the listener of
// every new contact; the listener keeps the ones a boat is in as impact
// records (+0x48 the array, +0x4C its count, +0x50 its capacity), skipping a
// weak one whose body pair / part pair is already in the scrape records
// (+0x54 / +0x58 / +0x5C). The tick 0x4AD970 plays the impacts (0x4ADA10) and
// the scrapes (0x4ADC00) and empties the arrays.

#include <workspace/core/boat/events.hxx>

#include <cstdint>
#include <cstring>
#include <vector>

// The engine's VuArray<T> (data, count, capacity; _aligned_malloc 16). Its
// reserve, here for the 0x1C-byte records, is 0x4AE490 (thiscall(array, n),
// retn 4): when n > the capacity, a new block of n records, the count's
// records copied (memcpy_s), the old freed, capacity = n. `data` holds
// `capacity` records; the ones past `count` are unwritten.
template < typename T >
struct vu_array_t
{
    std::vector< T > data;
    std::int32_t count = 0;
    std::int32_t capacity = 0;
};

template < typename T >
inline void vu_array_reserve( vu_array_t< T >& a, std::int32_t n )
{
    if ( n <= a.capacity )
        return;
    std::vector< T > grown( static_cast< std::size_t >( n ) );
    for ( std::int32_t i = 0; i < a.count; ++i )
        grown[ static_cast< std::size_t >( i ) ] = a.data[ static_cast< std::size_t >( i ) ];
    a.data.swap( grown );
    a.capacity = n;
}

// An impact record (+0x48's element, 0x1C bytes).
struct collision_impact_t
{
    std::int32_t part_a;   // +0x00 the contact's +0x2C byte (zero-extended)
    std::int32_t part_b;   // +0x04 its +0x2D byte
    float point[ 4 ];      // +0x08 .. +0x17 the contact's +0xC .. +0x1B (16 bytes, the w lane too)
    float strength;        // +0x18 0x4AD340's answer
};

// A scrape record (+0x54's element, 0x1C bytes): the listener compares only the
// first four words; the rest is the scrape tick's (0x4ADC00, unread).
struct collision_scrape_t
{
    const void* body_a;     // +0x00
    const void* body_b;     // +0x04
    std::int32_t part_a;    // +0x08
    std::int32_t part_b;    // +0x0C
    std::uint32_t rest[ 3 ];
};

struct collision_manager_t
{
    vu_array_t< collision_impact_t > impacts;   // +0x48
    vu_array_t< collision_scrape_t > scrapes;   // +0x54
};

// The contact-added listener 0x4AD540 (interface slot 0; thiscall this =
// manager + 4; the contact*; retn 4). Contact: +0 body A, +4 body B, +0xC the
// point (16 bytes), +0x1C the normal, +0x2C / +0x2D the two part bytes. A
// body is a boat's when its +0x2CA short & 0x10; its owner entity is +0x2C0
// (`owner_a` / `owner_b`: those images, read only then) and the owner's
// +0x467 byte must be set for one of them - else nothing. s = 0x4AD340( A, B,
// point, normal ) (boat/events.hxx body_contact_strength); nothing when s ==
// 0 (ucomiss / lahf / test ah 0x44 / jnp: a NaN goes on). When 1 > s
// (`comiss; jbe`): nothing when a scrape record has (A, B, part A, part B).
// Then the record is appended: count + 1 > capacity -> reserve( max( count +
// 1, capacity + capacity / 2 ) ) (cdq / sar: toward zero).
inline void collision_contact_added( collision_manager_t& m, const std::uint8_t* a, const std::uint8_t* b, const std::uint8_t* owner_a,
                                     const std::uint8_t* owner_b, const float* point, const float* normal, std::uint8_t part_a, std::uint8_t part_b )
{
    std::uint16_t ga, gb;
    std::memcpy( &ga, a + 0x2CA, 2 );
    std::memcpy( &gb, b + 0x2CA, 2 );
    if ( ( ( ga | gb ) & 0x10u ) == 0u )
        return;
    std::uint8_t boat = 0;
    if ( ga & 0x10u )
        boat = owner_a[ 0x467 ];
    if ( gb & 0x10u )
        boat |= owner_b[ 0x467 ];
    if ( boat == 0 )
        return;
    const float s = body_contact_strength( a, b, point, normal );
    if ( s == 0.0f )
        return;
    if ( 1.0f > s )
        for ( std::int32_t i = 0; i < m.scrapes.count; ++i )
        {
            const collision_scrape_t& r = m.scrapes.data[ static_cast< std::size_t >( i ) ];
            if ( r.body_a == a && r.body_b == b && r.part_a == part_a && r.part_b == part_b )
                return;
        }
    const std::int32_t n = m.impacts.count + 1;
    if ( n > m.impacts.capacity )
    {
        const std::int32_t c = m.impacts.capacity;
        const std::int32_t grow = c + c / 2;
        vu_array_reserve( m.impacts, n > grow ? n : grow );
    }
    m.impacts.count = n;
    collision_impact_t& r = m.impacts.data[ static_cast< std::size_t >( n - 1 ) ];
    r.part_a = part_a;
    r.part_b = part_b;
    std::memcpy( r.point, point, 16 );
    r.strength = s;
}
