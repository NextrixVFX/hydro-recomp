#pragma once

#include <workspace/core/boat/human.hxx>
#include <workspace/core/boat/rtti.hxx>

#include <cstdint>
#include <cstring>

// VuBoatEntity's event handlers: VuMethod1 delegates the ctor 0x41B850
// registers on the entity (thiscall(boat, VuParams const&), retn 4). The
// boat's +0x220 is its driver, +0x224 its camera (images: the caller's).

// The VuParams read both handlers inline: with the size word (+0x80, signed)
// > 0, the type word at the cursor; type 2 (a float) -> its value at +4, the
// cursor + 8 and the size - 8. Any other type reads nothing and leaves the
// cursor (so every later read sees the same word). 0 when nothing is read.
inline float boat_param_float( const std::uint8_t*& at, std::int32_t& left )
{
    float value = 0.0f;
    std::uint32_t type;
    if ( left > 0 )
    {
        std::memcpy( &type, at, 4 );
        if ( type == 2 )
        {
            std::memcpy( &value, at + 4, 4 );
            at += 8;
            left -= 8;
        }
    }
    return value;
}

// OnCameraShake 0x41E7B0: four floats a, b, c, d. On the camera: when its
// shake timer +0x2E8 is ordered <= 0 (comiss 0, +0x2E8; jae) or a is ordered
// above the amplitude +0x2E4 (comiss a, +0x2E4; jbe skips): +0x2E4 = a,
// +0x2EC = c, +0x2F4 / +0x2F8 = 0, +0x2FC = 1.0, +0x2E8 = c + b, +0x2F0 = d.
inline void boat_on_camera_shake( std::uint8_t* cam, const std::uint8_t* params )
{
    std::int32_t left;
    std::memcpy( &left, params + 0x80, 4 );
    const std::uint8_t* at = params;
    const float a = boat_param_float( at, left );
    const float b = boat_param_float( at, left );
    const float c = boat_param_float( at, left );
    const float d = boat_param_float( at, left );
    if ( human_ge( 0.0f, state_f( cam, 0x2E8 ) ) || human_gt( a, state_f( cam, 0x2E4 ) ) )
    {
        state_put_f( cam, 0x2E4, a );
        state_put_f( cam, 0x2EC, c );
        state_put_f( cam, 0x2F4, 0.0f );
        state_put_f( cam, 0x2F8, 0.0f );
        state_put_f( cam, 0x2FC, 1.0f );
        state_put_f( cam, 0x2E8, c + b );
        state_put_f( cam, 0x2F0, d );
    }
}

// 0x41E2E0 (thiscall boat; the pair in xmm1 / xmm2): only for a driver whose
// vt+4 type chain holds VuHumanDriver's descriptor 0x73623C - +0xC8 =
// maxss(+0xC8, a), +0xCC = maxss(+0xCC, b).
inline void boat_rumble( std::uint8_t* drv, std::uint32_t drv_vtable, float a, float b )
{
    if ( !boat_rtti_is( drv_vtable, 0x73623Cu ) )
        return;
    state_put_f( drv, 0xC8, ai_maxss( state_f( drv, 0xC8 ), a ) );
    state_put_f( drv, 0xCC, ai_maxss( state_f( drv, 0xCC ), b ) );
}

// OnRumble 0x41E950: two floats, then 0x41E2E0 with them.
inline void boat_on_rumble( std::uint8_t* drv, std::uint32_t drv_vtable, const std::uint8_t* params )
{
    std::int32_t left;
    std::memcpy( &left, params + 0x80, 4 );
    const std::uint8_t* at = params;
    const float a = boat_param_float( at, left );
    const float b = boat_param_float( at, left );
    boat_rumble( drv, drv_vtable, a, b );
}
