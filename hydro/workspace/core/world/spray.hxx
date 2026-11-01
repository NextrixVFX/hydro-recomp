#pragma once

#include <workspace/core/world/pfx.hxx>
#include <workspace/core/boat/tick.hxx>

#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

// THE BOAT'S OWN PARTICLE EMITTERS - the two VuPfx emit nodes whose slot 1 is
// not the shared VuPfxEmitFountain_emit 0x55D030 (memory/particles.md):
//   VuPfxEmitBoatThrustQuadFountainInstance 0x4CD5B0 (here);
//   VuPfxEmitBoatHullInstance 0x4CE760 - NOT recreated yet (HANDOFF.md).

// VuPfxEmitBoatThrustQuadFountainInstance's emit 0x4CD5B0 (thiscall node;
// float dt; retn 4), the part before the fountain: with a boat at node +0x28,
// the rate scale (+0x20) = maxss( 0x41D570( boat ), 0 ) - 0 for a negative or
// NaN answer - and then, when the node's params +0xC8 (a signed index) is >= 0
// and the byte of record [index] (8-byte records at boat +0x22C -> +0xF8) is
// 0, +0x20 = 0. Without a boat +0x20 is left as it is. `records` is that +0xF8
// array (null only when the boat is).
inline void pfx_boat_thrust_rate( pfx_fountain_node_t& node, const std::uint8_t* params, boat_view_t* boat, const std::uint8_t* records )
{
    if ( !boat )
        return;
    node.rate_scale = _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( boat_41D570( *boat ) ), _mm_setzero_ps( ) ) );
    std::int32_t index;
    std::memcpy( &index, params + 0xC8, 4 );
    if ( index >= 0 && records[ static_cast< std::size_t >( index ) * 8 ] == 0 )
        node.rate_scale = 0.0f;
}

// 0x4CD5B0 whole: the rate, then VuPfxEmitFountain_emit 0x55D030 with dt.
inline void pfx_boat_thrust_emit( pfx_fountain_node_t& node, const std::uint8_t* params, boat_view_t* boat, const std::uint8_t* records,
    pfx_rand_t& rand, const float* cameras, int camera_count, const float* system_position, const float* system_velocity, float dt,
    const pfx_fountain_hooks_t& hooks )
{
    pfx_boat_thrust_rate( node, params, boat, records );
    pfx_fountain_emit( node, params, rand, cameras, camera_count, system_position, system_velocity, dt, hooks );
}
