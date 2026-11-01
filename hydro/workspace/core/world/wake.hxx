#pragma once

// THE RACE'S WAKE, water side: what a boat's hull writes into the water every
// frame. The hull's wake update 0x417D80 (thiscall hull) builds a 13-word
// segment record through 0x417F80 (boat/anim.hxx boat_wake_params - the boat's
// half) and then, on VuWater (g 0x759058):
//
//   flag = hull +0x1EC (a flat wake: VuWaterFlatWakeWave; else VuWaterWakeWave)
//   slots: wake +0x1DC current / +0x1E0 previous, flat +0x1E4 / +0x1E8
//   no current wave: current = create( hull +0x1D0 descriptor, record ) and
//     return (0x5287D0 wake / 0x528850 flat)
//   else: the record over the current wave's +0x58 (its MOVING end), its
//     bounds again 0x52C720, out of the trees 0x529B90 and back in 0x529A90;
//     then when 1.0 / ( water +0x40 x 4.0 ) > the wave's +0xBC (its FIXED
//     end's age: comiss, ja) return; else a new wave from the same record,
//     the previous wave's reference dropped (slot 0 with 1 at zero), previous
//     = current, current = the new one; the plain wake then also writes the
//     solver (hull +0x1CC) +0x30 = 2, +0x34 -> [0] current, [1] previous - the
//     caller's (boat side).
//
// water +0x40 is 1.0 from VuWater's ctor 0x528050 (water_built_t::wake_rate;
// no other writer read).

#include <workspace/core/world/surface.hxx>

#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

// One wave's records as wave.hxx's wake_wave_t (+0x4C .. +0xBF).
inline wake_wave_t water_wake_of( const water_words_t& w )
{
    wake_wave_t v{};
    std::memcpy( &v.inner_ratio, &w.word[ 0x4C / 4 ], 4 );
    std::memcpy( &v.outer_ratio, &w.word[ 0x50 / 4 ], 4 );
    std::memcpy( &v.scale, &w.word[ 0x54 / 4 ], 4 );
    std::memcpy( &v.a, &w.word[ 0x58 / 4 ], sizeof( v.a ) );
    std::memcpy( &v.b, &w.word[ 0x8C / 4 ], sizeof( v.b ) );
    return v;
}

// 0x52C720 (thiscall wave): the box of the four points record +0x58 and record
// +0x8C put either side of their direction by their radius - (x - dy r, y + dx
// r) and (x + dy r, y - dx r), dx negated first as retail does - grown from an
// empty box (FLT_MAX / -FLT_MAX, minss / maxss in retail's operand order), z
// from the two records' z; then the centre and half the diagonal. +0x18 and
// +0x28 are the high halves of stack qwords nobody writes: UNKNOWN (not set).
inline void water_wake_bounds( water_words_t& w )
{
    auto mn = []( float a, float b ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); };
    auto mx = []( float a, float b ) { return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) ); };
    const float big = wave_bits( 0x7F7FFFFFu );
    const float ndxa = -w.f( 0x68 );
    const float ra = w.f( 0x7C );
    const float p1x = w.f( 0x58 ) - ( w.f( 0x6C ) * ra );
    const float p1y = w.f( 0x5C ) - ( ndxa * ra );
    float minx = mn( p1x, big );
    float miny = mn( p1y, big );
    float maxy = mx( p1y, -big );
    float maxx = mx( p1x, -big );
    const float p2x = ( w.f( 0x6C ) * ra ) + w.f( 0x58 );
    const float p2y = ( ndxa * ra ) + w.f( 0x5C );
    miny = mn( p2y, miny );
    minx = mn( p2x, minx );
    maxy = mx( p2y, maxy );
    maxx = mx( p2x, maxx );
    const float ndxb = -w.f( 0x9C );
    const float rb = w.f( 0xB0 );
    const float p3y = w.f( 0x90 ) - ( ndxb * rb );
    const float p3x = w.f( 0x8C ) - ( w.f( 0xA0 ) * rb );
    miny = mn( p3y, miny );
    minx = mn( p3x, minx );
    maxy = mx( p3y, maxy );
    maxx = mx( p3x, maxx );
    const float p4y = ( ndxb * rb ) + w.f( 0x90 );
    const float p4x = ( w.f( 0xA0 ) * rb ) + w.f( 0x8C );
    miny = mn( p4y, miny );
    minx = mn( p4x, minx );
    maxy = mx( p4y, maxy );
    maxx = mx( p4x, maxx );
    w.setf( 0x0C, minx );
    w.setf( 0x10, miny );
    w.setf( 0x14, mn( w.f( 0x60 ), w.f( 0x94 ) ) );
    w.setf( 0x1C, maxx );
    w.setf( 0x20, maxy );
    w.setf( 0x24, mx( w.f( 0x60 ), w.f( 0x94 ) ) );
    w.setf( 0x2C, ( maxx + minx ) * 0.5f );
    w.setf( 0x30, ( maxy + miny ) * 0.5f );
    const float dy = miny - maxy;
    const float dx = minx - maxx;
    w.setf( 0x34, _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( dy * dy ) + ( dx * dx ) ) ) ) * 0.5f );
}

// The held copy the trees and the wake lookup read (box, centre, radius, the
// two records) from the wave's words. The type chains (measured, case
// water_wake): VuWaterWakeWave 0x7368B8 -> VuWaterWave, VuWaterFlatWakeWave
// 0x7368C0 -> VuWaterWave - the flat wake is NOT a VuWaterWakeWave, so the
// lookup 0x5296C0 passes it over.
inline void water_wake_hold_sync( water_built_t& b, int wave )
{
    const water_words_t& w = b.waves[ static_cast< std::size_t >( wave ) ];
    water_held_wave_t& h = b.hold.waves[ static_cast< std::size_t >( wave ) ];
    const int entries = h.entries;
    h = water_held_from( w.word );
    h.entries = entries;
    h.wake = b.wave_kind[ static_cast< std::size_t >( wave ) ] == water_kind_wake;
    h.wake_data = water_wake_of( w );
}

// VuWater_createWakeWave 0x5287D0 / the flat 0x528850 (thiscall water; desc*,
// record*; retn 8): new 0xC0, +8 / +0x38 / +0x3C / +0x40 zeroed, VuWaterWave's
// ctor 0x5A6390 with 0, the vtable (0x74D148 / 0x74D164), +0x4C .. +0x54 the
// descriptor's 3 words, the record into +0x58 AND +0x8C, 0x52C720 (the wake
// ctor 0x52BEB0; the flat one inlines the same body), +4 incremented, then the
// allocators' common tail. Returns the wave's index.
inline int water_create_wake( water_built_t& b, const std::uint32_t* desc, const std::uint32_t* record, bool flat )
{
    wave_image_t base{};
    wave_base( base );
    water_words_t w{};
    for ( int k = 0; k < 64; ++k )
        if ( base.known[ k ] )
            w.set( 4 * k, base.word[ k ] );
    w.set( 0, flat ? 0x74D164u : 0x74D148u );
    w.set( 4, 2 );
    w.set( 8, 0 );
    for ( int k = 0; k < 3; ++k )
        w.set( 0x4C + 4 * k, desc[ k ] );
    for ( int k = 0; k < 13; ++k )
    {
        w.set( 0x58 + 4 * k, record[ k ] );
        w.set( 0x8C + 4 * k, record[ k ] );
    }
    water_wake_bounds( w );
    water_held_wave_t h = water_held_from( w.word );
    h.wake = !flat;
    h.wake_data = water_wake_of( w );
    // A destroyed wake's index is taken again (b.wake_free: our bookkeeping;
    // the list node, the trees and the refcount are exactly as for a new one).
    while ( !b.wake_free.empty( ) )
    {
        const int at = b.wake_free.back( );
        b.wake_free.pop_back( );
        if ( at < 0 || static_cast< std::size_t >( at ) >= b.waves.size( ) || at >= static_cast< int >( b.wave_gone.size( ) ) || !b.wave_gone[ static_cast< std::size_t >( at ) ] )
            continue;
        b.waves[ static_cast< std::size_t >( at ) ] = w;
        b.wave_kind[ static_cast< std::size_t >( at ) ] = flat ? water_kind_flat_wake : water_kind_wake;
        b.wave_gone[ static_cast< std::size_t >( at ) ] = 0;
        return water_hold_wave_at( b.hold, at, h );
    }
    b.waves.push_back( w );
    b.wave_kind.push_back( flat ? water_kind_flat_wake : water_kind_wake );
    return water_hold_wave( b.hold, h );
}

// A hull's two wake slots (-1 = null): +0x1DC / +0x1E0, or +0x1E4 / +0x1E8.
struct water_wake_slots_t
{
    int current = -1;
    int previous = -1;
};

// THE EXPORT: 0x417D80 after 0x417F80, on the built water. `desc` = hull
// +0x1D0 (3 words), `record` = 0x417F80's 13 words, `flat` = hull +0x1EC,
// `slots` the matching pair. Returns 0 (moved, or no current wave: the first
// one made), 1 when a new segment was made - the plain wake's caller then
// writes solver +0x30 = 2, +0x34 [0] / [1] = the two waves.
inline int water_wake_update( water_built_t& b, const std::uint32_t* desc, const std::uint32_t* record, bool flat, water_wake_slots_t& slots )
{
    if ( slots.current < 0 )
    {
        slots.current = water_create_wake( b, desc, record, flat );
        return 0;
    }
    water_words_t& w = b.waves[ static_cast< std::size_t >( slots.current ) ];
    for ( int k = 0; k < 13; ++k )
        w.set( 0x58 + 4 * k, record[ k ] );
    water_wake_bounds( w );
    water_hold_remove( b.hold, slots.current );
    water_wake_hold_sync( b, slots.current );
    water_hold_add( b.hold, slots.current );
    const float limit = 1.0f / ( b.wake_rate * 4.0f );
    if ( _mm_comigt_ss( _mm_set_ss( limit ), _mm_set_ss( w.f( 0xBC ) ) ) )
        return 0;
    const int made = water_create_wake( b, desc, record, flat );
    if ( slots.previous >= 0 )
    {
        water_words_t& p = b.waves[ static_cast< std::size_t >( slots.previous ) ];
        if ( --p.word[ 1 ] == 0u )
            water_built_destroy( b, slots.previous );
    }
    slots.previous = slots.current;
    slots.current = made;
    return flat ? 0 : 1;
}

// Live (not destroyed) wake / flat wake waves - what the race trace should count
// (b.waves also keeps every destroyed record unless wake_reuse is on).
inline int water_live_wakes( const water_built_t& b )
{
    int n = 0;
    for ( std::size_t k = 0; k < b.wave_kind.size( ); ++k )
        n += ( b.wave_kind[ k ] == water_kind_wake || b.wave_kind[ k ] == water_kind_flat_wake ) && !( k < b.wave_gone.size( ) && b.wave_gone[ k ] ) ? 1 : 0;
    return n;
}
