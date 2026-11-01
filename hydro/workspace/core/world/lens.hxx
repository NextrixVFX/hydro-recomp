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
#include <xmmintrin.h>

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

// VuLensWaterManagerImpl's drop draw 0x515060 (thiscall manager, count, drops;
// retn 8), reached from the render command 0x5144D0 -> 0x514D70 that slot 13
// 0x5143F0 queues with a copy of the manager's drop array (+0x74, 36-byte
// drops, count +0x2474). THE CPU HALF: 6 vertices of 16 bytes per drop into the
// static 0x799898 (x, y, u, v), drawn by g 0x75907C vt+0xC4( 3, 0, 6 count,
// 4 count, manager +0x2478 (the index buffer), 0x799898, 0x10 ). The GPU half is
// the render layer (hooks, not recreated): the state calls vt+0xAC / +0x94 /
// +0x98 / +0xA0 before and after, the shader +0x30 -> +0x58 bound (vt+0x84),
// the two textures (vt+0xB4: manager +0x14 -> slot 3 into +0x34, +0x44 into
// +0x38), the shader constants (lens_water_draw_scales below) and vt+0x88.
//
// Drop words read: +0 / +4 the position (x, y), +8 / +0xC the previous one,
// +0x18 the size. The streak runs along d = position - previous (normalised by
// 1 / sqrt(dy^2 + dx^2), the length taken as 1 when length - FLT_EPSILON is not
// >= 0, unordered included); h = size x 0.5. Vertices 0 / 1 sit at the previous
// position offset by -h n_y / +h n_y across, 2 / 3 at the position likewise,
// 4 / 5 at the position pushed on by (h n_x, h n_y). The "+ n x 0" terms are
// retail's (x 0 kept: -0 / NaN propagate). u, v: (0, .5) (1, .5) (0, .5)
// (1, .5) (0, 1) (1, 1).
struct lens_water_vertex_t
{
    float x, y, u, v;
};

inline void lens_water_drop_fill( int count, const float* drops, lens_water_vertex_t* out )
{
    for ( int i = 0; i < count; ++i, drops += 9, out += 6 )
    {
        const float* r = drops;
        const float dy = r[ 1 ] - r[ 3 ];
        const float dx = r[ 0 ] - r[ 2 ];
        const float h = r[ 6 ] * 0.5f;                                       // 0x6F765C
        float len = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( dy * dy + dx * dx ) ) );   // VuMath_sqrt 0x40B5A0
        if ( !( len - 1.1920929e-07f >= 0.0f ) )                             // 0x6F7584, comiss / jae
            len = 1.0f;
        const float inv = 1.0f / len;
        const float ny = inv * dy;
        const float nx = inv * dx;
        const float nh = -h;                                                 // xorps 0x6F7CD0
        const float ny0 = ny * 0.0f;
        const float nx0 = nx * 0.0f;
        const float mnx = -nx;
        const float a = nh * ny;                                             // xmm4
        const float b = nh * mnx;                                            // xmm5
        const float c = mnx * h;                                             // xmm6
        const float e = ny * h;                                              // [ebp+0xC]
        const float f = nx * h;                                              // xmm7
        out[ 0 ] = { ( r[ 2 ] + a ) + nx0, ( b + r[ 3 ] ) + ny0, 0.0f, 0.5f };
        out[ 1 ] = { ( r[ 2 ] + e ) + nx0, ( c + r[ 3 ] ) + ny0, 1.0f, 0.5f };
        out[ 2 ] = { ( r[ 0 ] + a ) + nx0, ( r[ 1 ] + b ) + ny0, 0.0f, 0.5f };
        out[ 3 ] = { ( r[ 0 ] + e ) + nx0, ( r[ 1 ] + c ) + ny0, 1.0f, 0.5f };
        out[ 4 ] = { f + ( r[ 0 ] + a ), ( r[ 1 ] + b ) + e, 0.0f, 1.0f };
        out[ 5 ] = { f + ( r[ 0 ] + e ), ( r[ 1 ] + c ) + e, 1.0f, 1.0f };
    }
}

// 0x515060's two shader constants (CPU arithmetic of the GPU half): the shader's
// vt+0x10( manager +0x3C, 1 / manager +0x10 ) and, when manager +0x40 is not 0,
// vt+0x20( +0x40, { 1 / (float) +8, 1 / (float) +0xC } ) (cvtdq2ps of the two
// ints - the target's width / height).
inline void lens_water_draw_scales( float aspect, std::int32_t width, std::int32_t height, float* out3 )
{
    out3[ 0 ] = 1.0f / aspect;
    out3[ 1 ] = 1.0f / static_cast< float >( width );
    out3[ 2 ] = 1.0f / static_cast< float >( height );
}
