#pragma once

#include <cmath>
#include <cstdint>

// ran1: Park-Miller minimal standard behind a 32 entry Bays-Durham shuffle.
//
// The engine writes the step as 16807*s - 2147483647*(s/127773), which is
// Schrage's trick in one line because 16807*127773 + 2836 is exactly
// 2147483647. Seeded at -1 and warmed for 40 iterations: the first eight draws
// are discarded and the last 32 fill the shuffle table. The seed is fixed, so
// the same spectrum always produces the same field.
//
// Read twice, independently, from two different subsystems: the ocean's
// BuildSpectrum and VuPfxEmitFountain_emit @0x55D030, which inlines the same
// step over the globals at 0x736810 (seed), 0x736814 (last) and 0x736818 (the
// 32 entry table). The static image holds seed = 0xFFFFFFFF = -1 and a zeroed
// table, which is the pre-warm state.
//
// RETAIL HAS EXACTLY ONE OF THESE, as a global. We give each subsystem its own
// instance, so our draws do not interleave the way retail's do. FLAGGED as a
// known structural difference rather than papered over: making them share is
// only correct once the ocean has been confirmed to read the same globals, and
// it would change every sequence, so it is not a change to make on a guess.
//
// SECOND UNVERIFIED POINT, AND IT AFFECTS THE PFX EMITTER. The 40 iteration
// warm-up is read from the OCEAN's spectrum builder. The emitter does NOT warm
// anything: VuPfxEmitFountain_emit @0x55D030 inlines only the step and the
// shuffle, over globals that the static image leaves at seed = -1 with a
// ZEROED table. Emulating the three functions that write 0x736810 (0x40DA90,
// 0x410FF0, 0x430830) filled nothing, so WHERE THE GLOBAL TABLE IS SEEDED IS
// NOT ESTABLISHED.
//
// That matters: from a zeroed table the first draw returns exactly 0.0,
// because the shuffle reads table[last / 0x4000000] before writing it. A
// warm-started generator never does that. So a warm-started c_random is
// CORRECT FOR THE OCEAN and UNPROVEN FOR THE EMITTER. Do not "fix" this by
// picking one - find the writer.
//
// The shuffle divisor is confirmed: retail divides by 0x4000000, and
// 1 + 2147483646/32 is exactly 0x4000000.
class c_random
{
public:
    c_random( )
    {
        m_seed = -1;
        for ( int index = 39; index >= 0; --index )
        {
            m_seed = step( m_seed );
            if ( index < 32 )
                m_table[ index ] = m_seed;
        }
        m_last = m_table[ 0 ];
    }

    float unit( )
    {
        m_seed = step( m_seed );
        const int slot = static_cast< int >( m_last / divisor );
        m_last = m_table[ slot & 31 ];
        m_table[ slot & 31 ] = m_seed;
        const float value = static_cast< float >( m_last ) * ( 1.0f / 2147483647.0f );
        // The emitter applies this as an fminf (sub_403260) before lerping
        // between a property's min and max, so a draw can never reach 1.0.
        return value > 0.99999994f ? 0.99999994f : value;
    }

    // Vu_GaussianDeviate: the polar form, which draws a pair and hands back
    // one, so the cached half must survive to the next call or the sequence
    // walks out of step with the original.
    float gauss( )
    {
        if ( m_have )
        {
            m_have = false;
            return m_spare;
        }
        float u = 0.0f;
        float v = 0.0f;
        float radius = 0.0f;
        do
        {
            u = unit( ) * 2.0f - 1.0f;
            v = unit( ) * 2.0f - 1.0f;
            radius = u * u + v * v;
        }
        while ( radius >= 1.0f || radius == 0.0f );
        const float factor = std::sqrt( -2.0f * std::log( radius ) / radius );
        m_spare = u * factor;
        m_have = true;
        return v * factor;
    }

    // lerp( min, max, unit() ), which is how every emitter property is drawn:
    // (1 - t) * min + max * t, in that order, as the emitter writes it.
    float range( float minimum, float maximum )
    {
        const float t = unit( );
        return ( 1.0f - t ) * minimum + maximum * t;
    }

private:
    static std::int32_t step( std::int32_t seed )
    {
        const std::int32_t k = seed / 127773;
        seed = 16807 * ( seed - k * 127773 ) - 2836 * k;
        if ( seed < 0 )
            seed += 2147483647;
        return seed;
    }

    static const std::int32_t divisor = 1 + 2147483646 / 32;

    std::int32_t m_seed = -1;
    std::int32_t m_last = 0;
    std::int32_t m_table[ 32 ]{};
    float m_spare = 0.0f;
    bool m_have = false;
};
