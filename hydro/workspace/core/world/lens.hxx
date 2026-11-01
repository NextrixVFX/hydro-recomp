#pragma once

// VuLensWaterEmitterEntity (vtable 0x75557C, 0x6C bytes: create 0x5E64C0) - a
// point that puts water drops on the camera lens while the camera is near it.
// Properties (engine entity/schema.hxx): Radius +0x60 (25), Drops Per Second
// +0x64 (64). Its emitter interface sits at +0x5C (slot 0 0x5E6780); the
// entity's slots 6 / 7 (0x5E6720 / 0x5E6750, adjustor-thunk null tests: a null
// entity hands 0) add / remove that interface with the lens-water manager
// g 0x759048 (VuLensWaterManagerImpl vt+0x38 / vt+0x3C - render side: hooks).
// Verified against retail: hydro checks/logic.hxx verify_lens_water (case
// lens_water).

#include <cstdint>
#include <cstring>

struct lens_water_hooks_t
{
    void* user = nullptr;
    // g 0x759048 vt+0x38 (add) / vt+0x3C (remove) with the emitter interface (entity +0x5C).
    void ( *add )( void* user, const void* emitter ) = nullptr;
    void ( *remove )( void* user, const void* emitter ) = nullptr;
};

// Slot 6 0x5E6720 (game initialize).
inline void lens_water_emitter_start( const void* emitter, const lens_water_hooks_t& h )
{
    if ( h.add )
        h.add( h.user, emitter );
}

// Slot 7 0x5E6750 (game release).
inline void lens_water_emitter_stop( const void* emitter, const lens_water_hooks_t& h )
{
    if ( h.remove )
        h.remove( h.user, emitter );
}

// The interface's slot 0 0x5E6780 (point = the camera): the entity's world
// position (its transform component, entity +0x50 -> +0x80 .. +0x88) against
// the point; Drops Per Second (+0x64) when Radius^2 >= the squared distance
// (comiss / jb: unordered answers 0), else 0 (fldz). The sum is (dy^2 + dx^2)
// + dz^2 in that order.
inline float lens_water_emitter_rate( const float* position, float radius, float drops, const float* point )
{
    const float dx = position[ 0 ] - point[ 0 ];
    const float dy = position[ 1 ] - point[ 1 ];
    const float dz = position[ 2 ] - point[ 2 ];
    const float d = ( dy * dy + dx * dx ) + dz * dz;
    return radius * radius >= d ? drops : 0.0f;
}
