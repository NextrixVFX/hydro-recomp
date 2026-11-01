#pragma once

#include <workspace/core/boat/fleet.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>

// The power-ups (VuPowerUpEntity, ctor 0x43B180, vtable 0x73AE54; with the
// VuRigidBodyContactCallback part at +0x5C (vtable 0x73AE4C) and the
// VuMotionComponentIF part at +0x60 (0x73AE3C)). VuPowerUpBoostEntity (ctor
// 0x43D860, vtable 0x73AED4) adds +0x138 "Randomize for Multiplayer" (1),
// +0x13C the boost energy (0; slot 14 0x43DA90 reads "BoostEnergy" from the
// PowerUpDB entry) and +0x140 the multiplayer byte (0; slot 13 0x43D960 sets
// it, with the "MPBoost" entry's pfx / audio, when +0x138 and the boat
// manager's human count +0x28 > 1). The images here are retail's layout.

inline float powerup_f( const std::uint8_t* p, int at )
{
    float v;
    std::memcpy( &v, p + at, 4 );
    return v;
}

// fistp (x87, round to nearest) of a float: the integer indefinite
// 0x80000000 for a NaN or a value outside int32.
inline std::int32_t powerup_fistp( float x )
{
    const double r = std::nearbyint( static_cast< double >( x ) );
    if ( !( r >= -2147483648.0 && r <= 2147483647.0 ) )
        return static_cast< std::int32_t >( 0x80000000u );
    return static_cast< std::int32_t >( r );
}

// cvttss2si: truncation, 0x80000000 for a NaN or out of range.
inline std::int32_t powerup_cvtt( float x )
{
    if ( !( x >= -2147483648.0f && x < 2147483648.0f ) )
        return static_cast< std::int32_t >( 0x80000000u );
    return static_cast< std::int32_t >( x );
}

// VuPowerUpBoostEntity slot 15 0x43DB40 (thiscall boat; retn 4): what a
// picked-up boost gives the boat. e = +0x13C; with the multiplayer byte
// +0x140: b = boat +0x474, n = fistp((1 - b) + b * 7) (the subtraction and
// the product in float, then added), the global VuRandom 0x736810's draw r,
// k = n - 1 + cvttss2si(r * 3) (32-bit wraparound), k = min(k, 8) then
// max(k, 1) (cmovg / cmovl), e = (float)k * 2. Then boat +0x314 = e, boat
// +0x318 = 0; returns true.
inline bool powerup_boost_apply( const std::uint8_t* powerup, std::uint8_t* boat, fleet_random_t& random )
{
    float e = powerup_f( powerup, 0x13C );
    if ( powerup[ 0x140 ] )
    {
        const float b = powerup_f( boat, 0x474 );
        const std::int32_t n = powerup_fistp( ( 1.0f - b ) + b * 7.0f );
        const float r = random.draw( );
        std::int32_t k = static_cast< std::int32_t >( static_cast< std::uint32_t >( n ) - 1u + static_cast< std::uint32_t >( powerup_cvtt( r * 3.0f ) ) );
        if ( k > 8 )
            k = 8;
        if ( k < 1 )
            k = 1;
        e = static_cast< float >( k ) * 2.0f;
    }
    std::memcpy( boat + 0x314, &e, 4 );
    std::memset( boat + 0x318, 0, 4 );
    return true;
}
