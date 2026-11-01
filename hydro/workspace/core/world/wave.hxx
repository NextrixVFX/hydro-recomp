#pragma once

#include <workspace/core/boat/hull.hxx>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>
#include <vector>

// The VuWater*Wave family: the thirteen wave kinds that displace a water
// surface. The ocean three live in water.hxx because they carry a whole
// spectrum field; the rest are per-vertex kernels, and this is where they go.
//
// THE INTERFACE, from VuWaterWave's vftable @0x750a10. It has six slots and
// slots 2 and 3 are `_purecall`, so those two are what every concrete wave
// must implement, while slots 4 and 5 default to nullsubs and are optional
// hooks:
//
//   slot 2  tick( float dt )     this->clock( +116 ) += dt;
//                               return this->mode( +4 ) != 1;
//   slot 3  the vertex kernel.
//
// SLOT 3 IS __stdcall WITH ONE POINTER ARGUMENT AND ECX IS NEVER READ -
// disassembled, not assumed. ECX arrives from slot 3's own caller, which is
// why the kernels it tail-calls are __thiscall while slot 3 itself is not.
//
// THE VERTEX CONTEXT, read out of the kernel at 0x5a1ad0:
//
//   ctx[  0 ]  vertex count
//   ctx[ 20 ]  base pointer to the vertex array
//   ctx[ 21 ]  stride IN BYTES
//   per vertex: +0 and +4 are x and y, READ;
//               +16 is a vec3 displacement, ACCUMULATED;
//               +32 is a scalar height, ACCUMULATED.
//
// EVERY KERNEL ADDS AND NEVER ASSIGNS. That is how several waves compose onto
// one surface, and it is why the oracle can read a single vertex back and get
// exactly one wave's contribution.

// VuWaterWhirlpoolWave's vertex kernel @0x5a1ad0 - the variant its slot 3
// @0x5a16a0 dispatches to when both ctx+96 and ctx+100 are zero. The other
// three (0x5a1d20, 0x5a1f80, 0x5a2160) are not transliterated yet.
//
// The wave's members, at the offsets the kernel reads them from. Retail holds
// these in a VuWaterWhirlpoolWave, which is NOT an entity and therefore has no
// schema entry.
//
// THE AUTHORED NAMES ARE NOW RECOVERED, through four reads: the entity's
// constructor @0x5d79b0 for the property-to-member bindings, its wave creation
// @0x5d7d90 for the 40-byte descriptor it packs, the allocator @0x528d00 for
// the object size, and the wave's own constructor @0x5a1470, which copies that
// descriptor FLAT - five movq pairs landing it at wave +76 onward.
//
//   Outer Radius   entity +96   -> desc +16 -> wave +92
//   Inner Radius   entity +100  -> desc +20 -> wave +96
//   Depth          entity +104  -> desc +24 -> wave +100
//   Angular Speed  entity +108  -> desc +28 -> wave +104
//   Linear Speed   entity +112  -> desc +32 -> wave +108
//   Foaminess      entity +116  -> desc +36 -> wave +112, which THIS kernel
//                                              never reads; the +96 kernel
//                                              @0x5a1f80 does
//
// THE ENTITY DECLARES "Inner Radius" BEFORE "Outer Radius" BUT BINDS THEM THE
// OTHER WAY ROUND: Inner goes to `v3 = this + 25` and Outer to
// `v2 = this + 24`. Declaration order would have had them swapped, which would
// have made the annulus test `inner > r > outer` - satisfiable only with the
// radii inverted. The bindings were read rather than inferred from the order,
// and they confirm the radius naming below, which had been taken from the
// kernel's own use of the two slots.
struct whirlpool_t
{
    float centre_x = 0.0f;      // wave[ 19 ], byte +76
    float centre_y = 0.0f;      // wave[ 20 ], byte +80
    float outer_radius = 0.0f;  // wave[ 23 ], byte +92
    float inner_radius = 0.0f;  // wave[ 24 ], byte +96
    float depth = 0.0f;         // wave[ 25 ], byte +100
    float angular_speed = 0.0f; // wave[ 26 ], byte +104
    float linear_speed = 0.0f;  // wave[ 27 ], byte +108
    float foaminess = 0.0f;     // wave[ 28 ], byte +112 (the +96 kernel only)
};

// One vertex, transliterated statement for statement. The operand order is
// retail's and is kept deliberately: the radius sum adds dy*dy BEFORE dx*dx,
// and the normalising sum adds the two in-plane terms before the slope term.
//
// 1.1920929e-7f is retail's own epsilon, added to the radius before it is
// divided by. Hex-Rays prints a float constant with enough digits to
// round-trip, so this is the shipped value and not a reconstruction of it; the
// static_assert below pins the bits.
inline void whirlpool_vertex( const whirlpool_t& wave, float x, float y,
    float* displacement, float& height )
{
    const float dx = x - wave.centre_x;
    const float dy = y - wave.centre_y;
    const float outer = wave.outer_radius;
    const float radius2 = ( dy * dy ) + ( dx * dx );
    if ( ( outer * outer ) <= radius2 )
        return;

    float rise = -wave.depth;
    if ( radius2 > ( wave.inner_radius * wave.inner_radius ) )
    {
        const float radius = std::sqrt( radius2 ) + 1.1920929e-7f;
        const float span = 1.0f / ( outer - wave.inner_radius );
        const float across = std::sqrt( ( radius - wave.inner_radius ) * span );
        const float swirl = wave.angular_speed;
        rise = ( wave.depth * across ) + rise;
        const float nx = ( 1.0f / radius ) * dx;
        const float ny = -( ( 1.0f / radius ) * dy );
        // The two in-plane swirl terms are CROSSED - ny drives x and nx drives
        // y - which is the ninety degree turn that makes it a swirl rather
        // than a push. The z term is written in retail as a multiply by zero,
        // so it is kept that way: it carries swirl's sign as -0.0.
        const float swirl_x = ny * swirl;
        const float swirl_y = swirl * nx;
        const float swirl_z = swirl * 0.0f;
        const float slope = -( ( ( span * 0.5f ) / across ) * wave.depth );
        const float scale = 1.0f / std::sqrt(
            ( ( -nx ) * ( -nx ) ) + ( ny * ny ) + ( slope * slope ) );
        // Authored as "Linear Speed". It scales the normalised in-plane term
        // here, which is not what that name suggests; the name is retail's and
        // the arithmetic is retail's, and where they disagree the arithmetic
        // is what gets transliterated.
        const float strength = wave.linear_speed;
        displacement[ 0 ] += ( ( ( scale * ( -nx ) ) * strength ) + swirl_x )
            * ( 1.0f - across );
        displacement[ 1 ] += ( ( ( scale * ny ) * strength ) + swirl_y )
            * ( 1.0f - across );
        displacement[ 2 ] += ( ( ( scale * slope ) * strength ) + swirl_z )
            * ( 1.0f - across );
    }
    height += rise;
}

static_assert( sizeof( float ) == 4, "the epsilon's bit pattern assumes a 32 bit float" );

// Fold an angle into [-pi, pi], transliterated from the sequence the
// directional kernel uses three times:
//
//     t = |phase|
//     w = ( t - (float)( (int)( t * 0.15915494 ) * 6.2831855 ) ) - 3.1415927
//     if ( phase < 0 ) w = -w
//
// 0.15915494 is 1/(2*pi) and 6.2831855 is 2*pi. THE TRUNCATION TO int AND BACK
// IS RETAIL'S and is not an artefact of the decompiler: it is how the quotient
// is floored, and replacing it with fmod would change the result for a large
// enough phase.
inline float wave_fold( float phase )
{
    const float magnitude = std::fabs( phase );
    const float folded = ( magnitude - ( static_cast< float >(
        static_cast< int >( magnitude * 0.15915494f ) ) * 6.2831855f ) ) - 3.1415927f;
    return phase < 0.0f ? -folded : folded;
}

// The edge falloff, used once per axis. Inside the threshold it is exactly
// 1.0; beyond it, a raised cosine over the remaining distance to 1.0.
inline float wave_edge( float value, float edge )
{
    if ( value <= edge )
        return 1.0f;
    const float folded = wave_fold(
        ( ( ( value - edge ) / ( 1.0f - edge ) ) * 3.1415927f ) + 3.1415927f );
    return ( std::cos( folded ) + 1.0f ) * 0.5f;
}

// VuWaterDirectionalWave's base kernel @0x59b110 - the both-flags-clear
// variant. Its members, at the offsets the kernel reads:
//
//   +104  amplitude          +116  edge along the SECOND axis
//   +108  wave number        +120  edge along the FIRST axis
//   +112  phase coefficient  +124  the wave's own clock, advanced by its
//                                  slot 2 tick @0x59aad0
//   +192  a 4x4 matrix; only m[0], m[1], m[4], m[5], m[12] and m[13] are
//         read, which is the row-vector transform of (x, y) into the wave's
//         own space. +192+0 is m[0], +208 is m[4], +240 is m[12].
//
// NAMED UNKNOWN, as for every wave in this family: which authored property
// feeds each member is NOT recovered. The names describe what the arithmetic
// does with each slot.
struct directional_t
{
    float amplitude = 0.0f;
    float wavenumber = 0.0f;
    float phase_scale = 0.0f;
    float edge_b = 0.0f;
    float edge_a = 0.0f;
    float clock = 0.0f;
    float transform[ 16 ]{};
};

// One vertex. The footprint is the UNIT SQUARE in the wave's own space: a
// vertex whose larger absolute coordinate reaches 1.0 is skipped entirely.
//
// This kernel writes the HEIGHT and only index 2 of the displacement - unlike
// the whirlpool, which writes all three. Both are left as "displacement"
// because what that vector means to the surface has not been read.
inline void directional_vertex( const directional_t& wave, float x, float y,
    float* displacement, float& height )
{
    const float a = ( wave.transform[ 4 ] * y ) + ( wave.transform[ 0 ] * x )
        + wave.transform[ 12 ];
    const float b = ( wave.transform[ 5 ] * y ) + ( wave.transform[ 1 ] * x )
        + wave.transform[ 13 ];
    const float abs_a = std::fabs( a );
    const float abs_b = std::fabs( b );
    if ( fmaxf( abs_a, abs_b ) >= 1.0f )
        return;

    const float phase = wave_fold( ( ( ( ( wave.phase_scale * b )
        - ( wave.clock * wave.wavenumber ) ) * 2.0f ) * 3.1415927f ) + 3.1415927f );
    // MEASURED, AND THE ANSWER IS "IT DOES NOT MATTER HERE": retail's x86 CRT
    // `cos` and `sin` take a double, and both this form and an explicit
    // promote-to-double-then-narrow form match the oracle at 0.000e+00 on
    // every sample. So which one retail performed is NOT established by these
    // vectors, and the simpler form is kept rather than a cast that implies
    // knowledge the samples do not supply.
    const float cosine = std::cos( phase );
    const float sine = std::sin( phase );

    const float swing = wave.amplitude * sine;
    const float fade_a = wave_edge( abs_a, wave.edge_a );
    const float swept = fade_a * swing;
    const float fade_b = wave_edge( abs_b, wave.edge_b );
    height += fade_b * swept;
    displacement[ 2 ] += ( ( ( ( wave.wavenumber * -2.0f ) * 3.1415927f )
        * ( wave.amplitude * cosine ) ) * fade_a ) * fade_b;
}

// VuWaterDirectionalFlowWave's base kernel @0x59c7c0. Its members:
//
//   +104, +108, +112  a constant flow vector, added to all three displacement
//                     components
//   +120  the edge along the SECOND axis
//   +124  the edge along the FIRST axis
//   +192  the same transform the directional wave uses
//
// TWO THINGS DIFFER FROM THE DIRECTIONAL AND BOTH WOULD HAVE BEEN WRONG IF
// CARRIED ACROSS:
//
//   1. THE FALLOFF IS LINEAR, not the raised cosine wave_edge computes:
//      (value - 1.0) / (edge - 1.0), which is 1 at the edge and 0 at 1.0.
//      The two axes MULTIPLY their fades rather than being applied in
//      sequence to different terms.
//   2. +124 IS AN EDGE HERE AND THE CLOCK ON THE DIRECTIONAL. The slot 2
//      tick these waves share @0x59c380 has no clock at all - it is just
//      `return mode( +4 ) != 1` - and this kernel reads no time, so the flow
//      is static.
//
// It writes all three displacement components and NO height, where the
// directional writes the height and only index 2.
struct flow_t
{
    float flow[ 3 ]{};
    float edge_b = 0.0f;
    float edge_a = 0.0f;
    float transform[ 16 ]{};
};

inline void flow_vertex( const flow_t& wave, float x, float y,
    float* displacement, float& height )
{
    ( void )height;
    const float a = ( y * wave.transform[ 4 ] ) + ( x * wave.transform[ 0 ] )
        + wave.transform[ 12 ];
    const float b = ( y * wave.transform[ 5 ] ) + ( x * wave.transform[ 1 ] )
        + wave.transform[ 13 ];
    const float abs_a = std::fabs( a );
    const float abs_b = std::fabs( b );
    if ( fmaxf( abs_a, abs_b ) >= 1.0f )
        return;

    float fade = 1.0f;
    if ( abs_a > wave.edge_a )
        fade = ( abs_a - 1.0f ) / ( wave.edge_a - 1.0f );
    if ( abs_b > wave.edge_b )
        fade = fade * ( ( abs_b - 1.0f ) / ( wave.edge_b - 1.0f ) );
    displacement[ 0 ] += wave.flow[ 0 ] * fade;
    displacement[ 1 ] += wave.flow[ 1 ] * fade;
    displacement[ 2 ] += wave.flow[ 2 ] * fade;
}

// VuWaterRampWave's base kernel @0x5a2c10.
//
// THE RAMP HAS NO 4x4 MATRIX, and believing it did cost a failing gate. The
// kernel reads exactly THREE floats to build its one local coordinate - at
// +188, +204 and +236 - and the +188/+204/+236 spacing only LOOKS like the
// m[0]/m[4]/m[12] of a matrix at +188. It is not one: a 4x4 at +188 would run
// to +251 and so would cover +248, which this same kernel reads as the
// quadratic's coefficient. Writing sixteen floats at +188 in the oracle case
// overwrote +248 with a matrix's 1.0 and the check failed on exactly the five
// blend-branch vectors. Read only what the code reads.
//
// IT ALSO HAS NO SPATIAL REJECTION. The other three skip a vertex outside
// their footprint; this one touches every vertex it is handed, with a
// quadratic blend near the ends and no clamp - a coordinate well past 1.0
// extrapolates, and that is retail's behaviour, not a defect to fix.
//
//   +100  the height scale used in the blend region
//   +112  the width of that blend region, measured inward from 1.0
//   +188, +204, +236  the x coefficient, y coefficient and offset
//   +248  the quadratic's coefficient
//   +252  the plateau's height slope
//   +280..+288  the flow vector applied on the plateau
//   +296..+304  the flow vector blended toward at the ends
struct ramp_t
{
    float blend_height = 0.0f;
    float band = 0.0f;
    float curve = 0.0f;
    float slope = 0.0f;
    float flow_inner[ 3 ]{};
    float flow_outer[ 3 ]{};
    float along_x = 0.0f;
    float along_y = 0.0f;
    float along_offset = 0.0f;
    // +256 / +260: the plateau's in-plane slope (inverse +188 / +204 times
    // +252), read only by the +96 kernel @0x5a2fc0
    float grad_x = 0.0f;
    float grad_y = 0.0f;
};

inline void ramp_vertex( const ramp_t& wave, float x, float y,
    float* displacement, float& height )
{
    const float along = ( wave.along_y * y ) + ( wave.along_x * x )
        + wave.along_offset;
    const float distance = std::fabs( along );
    if ( distance <= ( 1.0f - wave.band ) )
    {
        height += wave.slope * along;
        displacement[ 0 ] += wave.flow_inner[ 0 ];
        displacement[ 1 ] += wave.flow_inner[ 1 ];
        displacement[ 2 ] = wave.flow_inner[ 2 ] + displacement[ 2 ];
        return;
    }

    // The two halves are NOT a sign flip of one expression: each builds its
    // own square term and the constant 1.0 lands on a different side.
    float shaped = 0.0f;
    if ( along >= 0.0f )
        shaped = 1.0f - ( ( ( 1.0f - along ) * ( 1.0f - along ) ) * wave.curve );
    else
        shaped = ( ( ( along + 1.0f ) * ( along + 1.0f ) ) * wave.curve ) - 1.0f;
    height += ( wave.blend_height * 0.5f ) * shaped;
    const float across = ( 1.0f - distance ) / wave.band;
    for ( int axis = 0; axis < 3; ++axis )
    {
        displacement[ axis ] += ( wave.flow_outer[ axis ] * ( 1.0f - across ) )
            + ( wave.flow_inner[ axis ] * across );
    }
}

// VuWaterBumpWave's base kernel @0x5a4140.
//
// THE MATRIX IS AT +176 HERE. It reads both axes, rejects outside the unit
// square like the directional and flow waves, and writes ONLY THE HEIGHT -
// no displacement component at all.
//
// One axis drives a raised cosine of amplitude +104; the other is faded by
// wave_edge against +108. This is the first kernel that genuinely does reuse
// wave_edge, which is only knowable by reading it.
struct bump_t
{
    float amplitude = 0.0f;
    float edge_a = 0.0f;
    float transform[ 16 ]{};
};

inline void bump_vertex( const bump_t& wave, float x, float y,
    float* displacement, float& height )
{
    ( void )displacement;
    const float a = ( wave.transform[ 4 ] * y ) + ( wave.transform[ 0 ] * x )
        + wave.transform[ 12 ];
    const float b = ( y * wave.transform[ 5 ] ) + ( x * wave.transform[ 1 ] )
        + wave.transform[ 13 ];
    const float abs_a = std::fabs( a );
    if ( fmaxf( abs_a, std::fabs( b ) ) >= 1.0f )
        return;

    const float folded = wave_fold( ( b * 3.1415927f ) + 3.1415927f );
    const float lift = ( std::cos( folded ) + 1.0f ) * ( wave.amplitude * 0.5f );
    const float fade = wave_edge( abs_a, wave.edge_a );
    height += fade * lift;
}

// VuWaterPointWave's base kernel - the both-flags-clear branch INLINED inside
// slot 3 @0x5a3510. The other waves tail-call four separate kernels; this one
// jumps to labels inside itself, so the branch had to be located in the
// pseudocode rather than decompiled on its own.
//
// An EXPANDING RING. The wavefront's arrival time at a radius is
// (r - inner) * rate, and the vertex contributes only while that arrival lies
// between the clock minus tail_width and the clock itself:
//
//   scaled = max( r - inner, 0 ) * rate      when the front reached here
//   if ( clock > scaled && tail_width + scaled > clock )   inside the ring
//
// THE CLOCK AT +120 AND THE RATE AT +124 ARE THE SAME TWO MEMBERS THE TICK
// @0x5a34b0 READS, where the lifetime is ( rate * outer ) + tail_width - the
// wave dies once the ring has crossed its outer radius. The tick and the
// kernel agreeing on those offsets is two independent readings of the same
// fields, which is why they can be trusted.
//
//   +76, +80  centre
//   +92   the ring's width in clock units
//   +96   amplitude
//   +100  inner radius        +104  outer radius
//   +112  the phase scale
//   +120  the clock           +124  the front's rate
//   +128  an amplitude scale  +132  a slope, used negated in the displacement
struct point_wave_t
{
    float centre_x = 0.0f;
    float centre_y = 0.0f;
    float tail_width = 0.0f;
    float amplitude = 0.0f;
    float inner_radius = 0.0f;
    float outer_radius = 0.0f;
    float phase_scale = 0.0f;
    float clock = 0.0f;
    float rate = 0.0f;
    float amplitude_scale = 0.0f;
    float slope = 0.0f;
    // read only by the +96 kernel @0x5a39b0: +108 Speed, +116 Foaminess,
    // +136 = 1 / Magnitude (the ctor's reciprocal of +96)
    float speed = 0.0f;
    float foaminess = 0.0f;
    float inv_amplitude = 0.0f;
};

inline void point_wave_vertex( const point_wave_t& wave, float x, float y,
    float* displacement, float& height )
{
    // The y term is squared and added FIRST, which is retail's order.
    const float radius = std::sqrt(
        ( ( y - wave.centre_y ) * ( y - wave.centre_y ) )
        + ( ( x - wave.centre_x ) * ( x - wave.centre_x ) ) );
    if ( wave.outer_radius <= radius )
        return;

    const float span = fmaxf( radius - wave.inner_radius, 0.0f );
    const float scaled = wave.rate * span;
    if ( wave.clock <= scaled )
        return;

    const float width = wave.outer_radius - wave.inner_radius;
    const float edge = wave.tail_width + scaled;
    const float amount = ( ( width - span ) * wave.amplitude ) * wave.amplitude_scale;
    if ( edge <= wave.clock || amount <= 0.0f )
        return;

    const float phase = ( wave.clock - scaled ) * wave.phase_scale;
    const float cosine = std::cos( phase );
    const float sine = std::sin( phase );
    const float tail = ( ( edge - wave.clock ) * wave.slope ) * amount;
    height += sine * tail;
    displacement[ 2 ] += ( ( ( -wave.slope * amount ) * sine )
        + ( ( cosine * tail ) * wave.phase_scale ) );
}

// VuWaterBankedTurnWave's base kernel @0x5a55c0. The first wave in this
// family with an ANGULAR extent as well as a radial band: a vertex must lie
// inside the annulus AND within an angle of the direction at +116/+120.
//
// HEX-RAYS RENDERS THE acosf CALL AS `_libm_sse2_acosf()` WITH NO ARGUMENT,
// which is a hole in the pseudocode rather than a nullary function - these
// take theirs in xmm0. The disassembly at 0x5a5669 supplies it, along with
// two things the pseudocode could not: the clamp constants at 0x6f76b8 and
// 0x6f78a0 are exactly 1.0 and -1.0, read as bits, and the stack slots pair
// dx with +116 and dy with +120, from their stores at 0x5a5600. It is the
// FLOAT acosf.
//
// THIS READING WAS BRIEFLY ABANDONED BECAUSE THE ORACLE DISAGREED WITH IT.
// The disagreement was the emulator's: every __libm_sse2_* entry point in the
// image is a bare `jmp ds:__imp___libm_sse2_x`, so the real code is in a DLL
// and the harness stubbed it to return zero. acosf returned 0, every angle
// looked like zero, and the gate silently never fired. tools/ctor.py now
// implements the family, and with a real arccosine the sweep shows +112,
// +116, +120 and +124 all live and an extent of 0.1 radian correctly
// rejecting - exactly what this reading predicted.
//
//   +76, +80    centre
//   +96         outer radius      +100  inner radius
//   +104        amplitude
//   +112        the edge threshold, against the NORMALISED angle
//   +116, +120  the direction the angle is measured from
//   +124        the angular extent, in radians
struct banked_turn_t
{
    float centre_x = 0.0f;
    float centre_y = 0.0f;
    float outer_radius = 0.0f;
    float inner_radius = 0.0f;
    float amplitude = 0.0f;
    float edge = 0.0f;
    float direction_x = 0.0f;
    float direction_y = 0.0f;
    float extent = 0.0f;
};

inline void banked_turn_vertex( const banked_turn_t& wave, float x, float y,
    float* displacement, float& height )
{
    ( void )displacement;
    const float dx = x - wave.centre_x;
    const float dy = y - wave.centre_y;
    const float radius2 = ( dy * dy ) + ( dx * dx );
    if ( !( ( wave.outer_radius * wave.outer_radius ) > radius2
        && radius2 > ( wave.inner_radius * wave.inner_radius ) ) )
    {
        return;
    }

    const float radius = std::sqrt( radius2 );
    const float dot = ( ( wave.direction_y * dy ) + ( wave.direction_x * dx ) )
        / radius;
    const float angle = std::acos( fmaxf( -1.0f, fminf( 1.0f, dot ) ) );
    if ( wave.extent <= angle )
        return;

    // This cosine is NOT folded, unlike the one inside wave_edge: the annulus
    // test already bounded the radius, so the ratio cannot leave [0, 1] and
    // retail takes it straight.
    const float band = std::cos(
        ( ( ( radius - wave.inner_radius )
            / ( wave.outer_radius - wave.inner_radius ) ) * 6.2831855f )
        - 3.1415927f );
    const float across = angle / wave.extent;
    const float lift = ( ( band + 1.0f ) * 0.5f ) * wave.amplitude;
    height += wave_edge( across, wave.edge ) * lift;
}

// THE RECTANGULAR AND CIRCULAR OCEAN WAVES' BASE KERNELS, @0x59ed30 and
// @0x59fed0 (slot 3 @0x59da30 / @0x59dec0, both flags clear), and their
// ctx+100 variants @0x59f2b0 / @0x5a03f0, which are the same body behind the
// family's per-vertex tag test.
//
// They read a DOUBLE-BUFFERED field of bicubic bezier patches, 16 floats per
// cell, built elsewhere (VuWaterOceanWave_BuildBezierPatches @0x59d6c0, not
// recreated here). Buffer k sits at wave +176 + 16k: its time as a double at
// +0 and its patch array at +12. The members, as the kernels read them:
//
//   +108  shift (log2 of the field size)     +116  mask (size - 1)
//   +120  the field scale                    +128 / +132  the rotation's
//                                                         sine / cosine
//   +136  the current time, a double         +164 / +168  current / previous
//                                                         buffer index
//   +208 / +212  the centre
//   rectangular: +224 / +228 the sizes, +232 / +236 the two decays
//   circular:    +224 the radius, +228 the decay
//
// Each vertex: rotate (dx, dy) by the field's angle and scale it; the cell is
// nearbyint(v - 0.5) on each axis, and v minus that integer is the patch
// parameter. THE ROUNDING IS AN x87 `fld; fsub 0.5; fistp` - round to nearest
// EVEN on the exact difference - so an exact tie lands on the even cell and
// the parameter can come out as exactly 1.0; floor or truncation would not.
// The footprint test and its fade use the UNROTATED offset on the rectangle
// and the rotated one's length on the circle. The height takes the current
// field times the fade; displacement[ 2 ] takes the difference against the
// previous field divided by the two buffers' time gap - a vertical velocity -
// computed in DOUBLE (cvtps2pd, subsd, divsd, cvtpd2ps).
struct ocean_patch_t
{
    int shift = 0;
    int mask = 0;
    float scale = 0.0f;
    float sine = 0.0f;
    float cosine = 1.0f;
    double now = 0.0;
    int current = 0;
    int previous = 0;
    double time[ 2 ]{};
    const float* patches[ 2 ]{};
    float centre_x = 0.0f;
    float centre_y = 0.0f;
    float shape[ 4 ]{};   // rect: size x, size y, decay x, decay y; circle: radius, decay
};

// One patch, in retail's term order: the control points are NOT visited in
// index order (0, 11, 10, 3 / 4, 12, 15, 9 / 5, 13, 14, 8 / 1, 6, 7, 2), the
// first row is summed before its u weight is applied and every later term is
// weighted on its own. Retail evaluates it twice per vertex with identical
// weights, once per buffer.
inline float ocean_patch_eval( const float* p, const float* bu, const float* bw )
{
    float t = p[ 11 ] * bw[ 1 ];
    t = t + p[ 0 ] * bw[ 0 ];
    t = t + p[ 10 ] * bw[ 2 ];
    t = t + p[ 3 ] * bw[ 3 ];
    t = t * bu[ 0 ];
    t = t + ( p[ 4 ] * bw[ 0 ] ) * bu[ 1 ];
    t = t + ( p[ 12 ] * bw[ 1 ] ) * bu[ 1 ];
    t = t + ( p[ 15 ] * bw[ 2 ] ) * bu[ 1 ];
    t = t + ( p[ 9 ] * bw[ 3 ] ) * bu[ 1 ];
    t = t + ( p[ 5 ] * bw[ 0 ] ) * bu[ 2 ];
    t = t + ( p[ 13 ] * bw[ 1 ] ) * bu[ 2 ];
    t = t + ( p[ 14 ] * bw[ 2 ] ) * bu[ 2 ];
    t = t + ( p[ 8 ] * bw[ 3 ] ) * bu[ 2 ];
    t = t + ( p[ 1 ] * bw[ 0 ] ) * bu[ 3 ];
    t = t + ( p[ 6 ] * bw[ 1 ] ) * bu[ 3 ];
    t = t + ( p[ 7 ] * bw[ 2 ] ) * bu[ 3 ];
    t = t + ( p[ 2 ] * bw[ 3 ] ) * bu[ 3 ];
    return t;
}

// The cubic bernstein weights retail builds: (a*a)*a, ((3t)*a)*a, ((3t)*t)*a,
// (t*t)*t with a = 1 - t.
inline void ocean_bernstein( float t, float* b )
{
    const float a = 1.0f - t;
    const float t3 = t * 3.0f;
    b[ 0 ] = ( a * a ) * a;
    b[ 1 ] = ( t3 * a ) * a;
    b[ 2 ] = ( t3 * t ) * a;
    b[ 3 ] = ( t * t ) * t;
}

// fld m32; fsub 0.5 (m32); fistp m32: the difference is exact in the x87's
// extended precision and so is it in a double, and cvtsd2si rounds to nearest
// even under the default MXCSR as fistp does under the default control word -
// including the 0x80000000 "indefinite" on overflow.
inline int ocean_cell( float v )
{
    return _mm_cvtsd_si32( _mm_set_sd( static_cast< double >( v ) - 0.5 ) );
}

// comiss a, b; ja: a > b, false when unordered.
inline bool ocean_gt( float a, float b )
{
    return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( b ) ) != 0;
}

inline void ocean_patch_vertex( const ocean_patch_t& wave, bool circle, float x, float y,
    float* displacement, float& height )
{
    const float dy = y - wave.centre_y;
    const float dx = x - wave.centre_x;
    const float a = ( wave.cosine * dx ) - ( wave.sine * dy );
    const float b = ( wave.sine * dx ) + ( wave.cosine * dy );
    const float va = a * wave.scale;
    const float vb = b * wave.scale;
    const int ia = ocean_cell( va );
    const int ib = ocean_cell( vb );
    const float u = va - static_cast< float >( ia );
    const float w = vb - static_cast< float >( ib );
    const int cell = ( ( ib & wave.mask ) << wave.shift ) + ( ia & wave.mask );

    float fade = 1.0f;
    float fade_b = 1.0f;
    if ( !circle )
    {
        const float ua = std::fabs( ( 2.0f / wave.shape[ 0 ] ) * dx );
        if ( !ocean_gt( 1.0f, ua ) )
            return;
        const float ub = std::fabs( ( 2.0f / wave.shape[ 1 ] ) * dy );
        if ( !ocean_gt( 1.0f, ub ) )
            return;
        if ( ocean_gt( ua, wave.shape[ 2 ] ) )
            fade = ( ua - 1.0f ) / ( wave.shape[ 2 ] - 1.0f );
        if ( ocean_gt( ub, wave.shape[ 3 ] ) )
            fade_b = ( ub - 1.0f ) / ( wave.shape[ 3 ] - 1.0f );
    }
    else
    {
        const float r = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( b * b ) + ( a * a ) ) ) ) / wave.shape[ 0 ];
        if ( !ocean_gt( 1.0f, r ) )
            return;
        if ( ocean_gt( r, wave.shape[ 1 ] ) )
            fade = ( r - 1.0f ) / ( wave.shape[ 1 ] - 1.0f );
    }

    float bu[ 4 ];
    float bw[ 4 ];
    ocean_bernstein( u, bu );
    ocean_bernstein( w, bw );
    const float* now = wave.patches[ wave.current ] + 16 * cell;
    const float* before = wave.patches[ wave.previous ] + 16 * cell;
    float lift = ocean_patch_eval( now, bu, bw ) * fade;
    float last = ocean_patch_eval( before, bu, bw ) * fade;
    if ( !circle )
    {
        lift = lift * fade_b;
        last = last * fade_b;
    }
    height = height + lift;
    const double rate = static_cast< double >( lift - last ) / ( wave.now - wave.time[ wave.previous ] );
    displacement[ 2 ] = static_cast< float >( rate ) + displacement[ 2 ];
}

// VuWaterInfiniteOceanWave's base kernel @0x59e070 (slot 3 @0x59d900, both
// flags clear; the ctx+100 variant @0x59e520 is the tag-tested sibling - the
// caller's test). The rectangular body WITHOUT the centre and WITHOUT any
// footprint or fade: (x, y) rotated as read (x*c - y*s, s*x + y*c), scaled,
// the same cells, bernstein weights and patch term order (both buffers),
// height += the current field, displacement[ 2 ] += (now - previous) / the
// buffers' time gap in double. Read from the disassembly.
inline void ocean_infinite_vertex( const ocean_patch_t& wave, float x, float y, float* displacement, float& height )
{
    const float a = ( x * wave.cosine ) - ( y * wave.sine );
    const float b = ( wave.sine * x ) + ( y * wave.cosine );
    const float va = a * wave.scale;
    const float vb = b * wave.scale;
    const int ia = ocean_cell( va );
    const int ib = ocean_cell( vb );
    const float u = va - static_cast< float >( ia );
    const float w = vb - static_cast< float >( ib );
    const int cell = ( ( ib & wave.mask ) << wave.shift ) + ( ia & wave.mask );
    float bu[ 4 ];
    float bw[ 4 ];
    ocean_bernstein( u, bu );
    ocean_bernstein( w, bw );
    const float lift = ocean_patch_eval( wave.patches[ wave.current ] + 16 * cell, bu, bw );
    const float last = ocean_patch_eval( wave.patches[ wave.previous ] + 16 * cell, bu, bw );
    height = height + lift;
    const double rate = static_cast< double >( lift - last ) / ( wave.now - wave.time[ wave.previous ] );
    displacement[ 2 ] = static_cast< float >( rate ) + displacement[ 2 ];
}

// VuWaterWakeWave's base kernel @0x52ca60 (slot 3 @0x52c260, both flags
// clear) and its ctx+100 variant @0x52ce20. A wake wave is the stretch of a
// boat's wake between TWO SEGMENT RECORDS, each 13 floats, copied in by the
// constructor @0x52beb0 from the same 0x34-byte source into +88 and +140 (the
// second is overwritten as the wake moves on). Each record, named for what
// this kernel does with the slot (the authoring side is the boat's, not read):
//
//   +0 / +4  a point on the cross-section    +16 / +20  its forward direction
//   +24  the life                            +28  the fade time
//   +32  the amplitude                       +36  the radius
//   +40  the speed                           +44  the frequency
//   +48  the age, advanced by the shared tick @0x52c210
//
// and on the wave itself +76 / +80 the inner / outer radius ratios and +84
// the height scale. A vertex counts only BETWEEN the two cross-sections (its
// signed distances to them have opposite signs); every record field is then
// blended at t = dB / (dB - dA), the vertex must lie inside the blended
// radius, and the ring that has had time to travel out to it contributes a
// sine. HEIGHT ONLY.
//
// THE SINE IS x87 `fsincos` on the folded phase, stored to a float. The
// oracle measures it under Unicorn, whose fsincos is a host double sine; the
// form below - the CRT double sine narrowed - matches those vectors. Real
// hardware's fsincos is NOT measured here, and its float rounding could
// differ in a rare last bit.
struct wake_record_t
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
    float direction_x = 0.0f;
    float direction_y = 0.0f;
    float life = 0.0f;
    float fade_time = 0.0f;
    float amplitude = 0.0f;
    float radius = 0.0f;
    float speed = 0.0f;
    float frequency = 0.0f;
    float age = 0.0f;
};
static_assert( sizeof( wake_record_t ) == 0x34, "the constructor's 0x34-byte copy" );

struct wake_wave_t
{
    float inner_ratio = 0.0f;   // +76
    float outer_ratio = 0.0f;   // +80
    float scale = 0.0f;         // +84
    wake_record_t a{};          // +88
    wake_record_t b{};          // +140
};

inline void wake_vertex( const wake_wave_t& wave, float x, float y, float* displacement, float& height )
{
    ( void )displacement;
    const wake_record_t& a = wave.a;
    const wake_record_t& b = wave.b;
    const float da = ( a.direction_x * ( x - a.x ) ) + ( a.direction_y * ( y - a.y ) );
    const float db = ( b.direction_y * ( y - b.y ) ) + ( b.direction_x * ( x - b.x ) );
    if ( !ocean_gt( 0.0f, db * da ) )
        return;
    const float t = db / ( db - da );
    const float s = 1.0f - t;
    const float px = ( a.x * t ) + ( b.x * s );
    const float py = ( a.y * t ) + ( b.y * s );
    const float radius = ( b.radius * s ) + ( a.radius * t );
    const float ey = y - py;
    const float ex = x - px;
    const float d2 = ( ey * ey ) + ( ex * ex );
    if ( !ocean_gt( radius * radius, d2 ) )
        return;
    const float age = ( b.age * s ) + ( a.age * t );
    const float speed = ( b.speed * s ) + ( a.speed * t );
    const float d = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( d2 ) ) );
    // maxss( 0, q ): q when 0 is not greater, so a NaN q passes through.
    const float travelled = _mm_cvtss_f32( _mm_max_ss( _mm_setzero_ps( ),
        _mm_set_ss( d - ( wave.inner_ratio * radius ) ) ) );
    const float local = age - ( travelled / speed );
    if ( !ocean_gt( local, 0.0f ) )
        return;
    const float life = ( b.life * s ) + ( a.life * t );
    if ( !ocean_gt( life, age ) )
        return;
    float amplitude = ( b.amplitude * s ) + ( a.amplitude * t );
    const float k = d / radius;
    const float frequency = ( b.frequency * s ) + ( a.frequency * t );
    const float fade_time = ( b.fade_time * s ) + ( a.fade_time * t );
    if ( ocean_gt( wave.inner_ratio, k ) )
        amplitude = ( k / wave.inner_ratio ) * amplitude;
    if ( ocean_gt( k, wave.outer_ratio ) )
        amplitude = ( ( 1.0f - k ) / ( 1.0f - wave.outer_ratio ) ) * amplitude;
    const float ratio = ( life - age ) / fade_time;
    const float phase = ( frequency * local ) + 3.1415927f;
    // minss( ratio, 1 ): 1 when ratio is not less, so a NaN ratio gives 1.
    const float weight = _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( ratio ), _mm_set_ss( 1.0f ) ) ) * amplitude;
    const float sine = static_cast< float >( std::sin( static_cast< double >( wave_fold( phase ) ) ) );
    height = ( ( sine * weight ) * wave.scale ) + height;
}

// The x87 fold + fsincos the wake's +96 kernel runs on its phase (0x52D5B7 ..
// 0x52D610): |p| - (int)(|p| / 2pi) * 2pi - pi, negated when p < 0, then
// fsincos stored as floats - wave_x87_sin / cos (see their note).
inline void wake_fold_sincos( float phase, float& sine, float& cosine )
{
    const float a = std::fabs( phase );
    float f = ( a - static_cast< float >( static_cast< int >( a * 0.15915494f ) ) * 6.2831855f ) - 3.1415927f;
    if ( ocean_gt( 0.0f, phase ) )
        f = -f;
    // wave_x87_sin / cos's form (defined further down): the host double, narrowed
    sine = static_cast< float >( std::sin( static_cast< double >( f ) ) );
    cosine = static_cast< float >( std::cos( static_cast< double >( f ) ) );
}

// VuWaterWakeWave's +96 kernel @0x52d1d0 (slot 3 0x52c260 with ctx+0x60 set;
// its tag variant @0x52dcc0 is this body behind the tag test - MEASURED by
// case wake_kernels96, not read from an instruction diff), on the renderer's
// records (+0 x, +4 y, +8 height, +0xC / +0x10 the in-plane gradient, +0x14
// foam). The base kernel's blend of the two segment records at t, then: the
// height takes sin * min( ratio, 1 ) * amplitude - WITHOUT the base's +0x54
// scale; the gradient is the x / y derivative of every blended term through
// dt/dx = ( B.dir * D - ( B.dir - A.dir ) * dB ) / D^2 (D = dB - dA), the
// radius r clamped below by FLT_EPSILON, the maxss clamp's zero slope, the
// minss clamp's zero slope past ratio 1; the foam takes min( ratio, 1 ) times
// a factor that starts at 0.25, becomes ( 1 - k ) + k * 0.25 inside the inner
// ratio (k = rn / inner) and is scaled by the outer fade. Transliterated term
// for term, operand order kept; where the arithmetic is not a derivative of
// what the height uses (the amplitude terms reuse the final weight, the outer
// fade's slope has no minus) it is retail's, kept.
inline void wake_vertex96( const wake_wave_t& wave, float x, float y, float& height, float* flow, float& foam )
{
    const wake_record_t& A = wave.a;
    const wake_record_t& B = wave.b;
    const float dA = ( ( y - A.y ) * A.direction_y ) + ( ( x - A.x ) * A.direction_x );
    const float dB = ( B.direction_x * ( x - B.x ) ) + ( B.direction_y * ( y - B.y ) );
    if ( !ocean_gt( 0.0f, dB * dA ) )
        return;
    const float D = dB - dA;
    const float t = dB / D;
    const float s = 1.0f - t;
    const float py = ( A.y * t ) + ( s * B.y );
    const float px = ( A.x * t ) + ( B.x * s );
    const float rad = ( B.radius * s ) + ( A.radius * t );
    const float ey = y - py;
    const float ex = x - px;
    const float d2 = ( ey * ey ) + ( ex * ex );
    const float R2 = rad * rad;
    if ( !ocean_gt( R2, d2 ) )
        return;
    const float age = ( B.age * s ) + ( A.age * t );
    const float spd = ( s * B.speed ) + ( t * A.speed );
    const float r = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( d2 ) ) );
    const float rq = r - ( wave.inner_ratio * rad );
    const float m = _mm_cvtss_f32( _mm_max_ss( _mm_setzero_ps( ), _mm_set_ss( rq ) ) );
    const float local = age - ( m / spd );
    if ( !ocean_gt( local, 0.0f ) )
        return;
    const float life = ( B.life * s ) + ( A.life * t );
    if ( !ocean_gt( life, age ) )
        return;
    float amp = ( B.amplitude * s ) + ( A.amplitude * t );
    const float freq = ( B.frequency * s ) + ( A.frequency * t );
    float ff = 0.25f;   // the foam factor
    float k1 = 1.0f;
    float k2 = 1.0f;
    const float fade = ( B.fade_time * s ) + ( A.fade_time * t );
    const float rn = r / rad;
    if ( ocean_gt( wave.inner_ratio, rn ) )
    {
        k1 = rn / wave.inner_ratio;
        amp = k1 * amp;
        ff = ( 1.0f - k1 ) + ( k1 * 0.25f );
    }
    if ( ocean_gt( rn, wave.outer_ratio ) )
    {
        k2 = ( 1.0f - rn ) / ( 1.0f - wave.outer_ratio );
        amp = k2 * amp;
        ff = k2 * ff;
    }
    const float left = life - age;
    const float ratio = left / fade;
    const float w = _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( ratio ), _mm_set_ss( 1.0f ) ) );
    const float W = w * amp;
    float sine;
    float cosine;
    wake_fold_sincos( ( freq * local ) + 3.1415927f, sine, cosine );
    height = ( sine * W ) + height;

    // the derivatives of t and s
    const float inv_d2 = 1.0f / ( D * D );
    const float dtx = ( ( B.direction_x * D ) - ( ( B.direction_x - A.direction_x ) * dB ) ) * inv_d2;
    const float dsx = -dtx;
    const float dty = ( ( B.direction_y * D ) - ( ( B.direction_y - A.direction_y ) * dB ) ) * inv_d2;
    const float dsy = -dty;
    // of the blended point and radius
    const float dpx_dy = ( A.x * dty ) + ( B.x * dsy );
    const float dpy_dx = ( A.y * dtx ) + ( B.y * dsx );
    const float dpy_dy = ( A.y * dty ) + ( B.y * dsy );
    const float dpx_dx = ( A.x * dtx ) + ( B.x * dsx );
    const float drad_dx = ( dtx * A.radius ) + ( dsx * B.radius );
    const float drad_dy = ( dty * A.radius ) + ( dsy * B.radius );
    const float dex_dx = 1.0f - dpx_dx;
    const float dex_dy = -dpx_dy;
    const float dey_dy = 1.0f - dpy_dy;
    const float dey_dx = -dpy_dx;
    const float dage_dx = ( dtx * A.age ) + ( dsx * B.age );
    const float dage_dy = ( dty * A.age ) + ( dsy * B.age );
    const float dspd_dx = ( dtx * A.speed ) + ( dsx * B.speed );
    const float dspd_dy = ( dty * A.speed ) + ( dsy * B.speed );
    // of r ( FLT_EPSILON below: maxss( r, 1.1920929e-7 ) )
    const float rr = _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( r ), _mm_set_ss( 1.1920929e-7f ) ) );
    const float ex2 = ex * 2.0f;
    const float inv2r = 1.0f / ( rr * 2.0f );
    const float ey2 = ey * 2.0f;
    const float dr_dx = ( ( ey2 * dey_dx ) + ( ex2 * dex_dx ) ) * inv2r;
    const float dr_dy = ( ( ey2 * dey_dy ) + ( ex2 * dex_dy ) ) * inv2r;
    float dm_dx = dr_dx - ( drad_dx * wave.inner_ratio );
    float dm_dy = dr_dy - ( drad_dy * wave.inner_ratio );
    if ( ocean_gt( 0.0f, rq ) )
    {
        dm_dx = 0.0f;
        dm_dy = 0.0f;
    }
    // of local = age - m / speed
    const float inv_s2 = 1.0f / ( spd * spd );
    const float dq_dx = ( ( dm_dx * spd ) - ( m * dspd_dx ) ) * inv_s2;
    const float dq_dy = ( ( dm_dy * spd ) - ( m * dspd_dy ) ) * inv_s2;
    const float dlocal_dx = dage_dx - dq_dx;
    const float dlocal_dy = dage_dy - dq_dy;
    const float dlife_dx = ( dtx * A.life ) + ( dsx * B.life );
    const float dlife_dy = ( dty * A.life ) + ( dsy * B.life );
    const float dfreq_dx = ( A.frequency * dtx ) + ( B.frequency * dsx );
    const float dfreq_dy = ( A.frequency * dty ) + ( B.frequency * dsy );
    const float dfade_dy = ( A.fade_time * dty ) + ( B.fade_time * dsy );
    const float dfade_dx = ( A.fade_time * dtx ) + ( B.fade_time * dsx );
    // of rn = r / rad
    const float inv_r2 = 1.0f / R2;
    const float drn_dx = ( ( dr_dx * rad ) - ( rr * drad_dx ) ) * inv_r2;
    const float drn_dy = ( ( dr_dy * rad ) - ( rr * drad_dy ) ) * inv_r2;
    // of the amplitude (accumulators from zero)
    float da_dx = 0.0f;
    float da_dy = 0.0f;
    if ( ocean_gt( wave.inner_ratio, rn ) )
    {
        const float inv = 1.0f / wave.inner_ratio;
        const float z = k1 * da_dx;
        da_dx = ( ( drn_dx * inv ) * W ) + z;
        da_dy = ( ( drn_dy * inv ) * W ) + z;
    }
    if ( ocean_gt( rn, wave.outer_ratio ) )
    {
        const float inv = 1.0f / ( 1.0f - wave.outer_ratio );
        const float ax = da_dx * k2;
        const float ay = da_dy * k2;
        da_dx = ( ( drn_dx * inv ) * W ) + ax;
        da_dy = ( ( drn_dy * inv ) * W ) + ay;
    }
    // of w = min( left / fade, 1 )
    const float inv_f2 = 1.0f / ( fade * fade );
    float dw_dx = ( ( ( dlife_dx - dage_dx ) * fade ) - ( left * dfade_dx ) ) * inv_f2;
    float dw_dy = ( ( ( dlife_dy - dage_dy ) * fade ) - ( left * dfade_dy ) ) * inv_f2;
    if ( ocean_gt( ratio, 1.0f ) )
    {
        dw_dx = 0.0f;
        dw_dy = 0.0f;
    }
    const float cw = cosine * W;
    const float gx = ( ( w * da_dx ) + ( dw_dx * W ) ) * sine;
    const float phx = ( dfreq_dx * local ) + ( dlocal_dx * freq );
    flow[ 0 ] = ( gx + ( phx * cw ) ) + flow[ 0 ];
    const float gy = ( ( w * da_dy ) + ( dw_dy * W ) ) * sine;
    const float phy = ( dfreq_dy * local ) + ( dlocal_dy * freq );
    flow[ 1 ] = ( gy + ( phy * cw ) ) + flow[ 1 ];
    foam = ( w * ff ) + foam;
}

// VuWaterFlatWakeWave's +96 kernel @0x52e7d0 (slot 3 0x52ca30 with ctx+0x60
// set; tag variant @0x52eae0 the same body behind the tag test - instruction
// diff read with census/vdiff.py: register / slot allocation and one commuted
// addss): the wake's blend and gates, then FOAM ONLY - min( ratio, 1 ) times the
// same foam factor as the wake's +96 kernel.
inline void flat_wake_vertex96( const wake_wave_t& wave, float x, float y, float& foam )
{
    const wake_record_t& A = wave.a;
    const wake_record_t& B = wave.b;
    const float dA = ( A.direction_y * ( y - A.y ) ) + ( A.direction_x * ( x - A.x ) );
    const float dB = ( ( x - B.x ) * B.direction_x ) + ( B.direction_y * ( y - B.y ) );
    if ( !ocean_gt( 0.0f, dB * dA ) )
        return;
    const float t = dB / ( dB - dA );
    const float s = 1.0f - t;
    const float px = ( A.x * t ) + ( B.x * s );
    const float py = ( A.y * t ) + ( B.y * s );
    const float rad = ( B.radius * s ) + ( A.radius * t );
    const float ex = x - px;
    const float ey = y - py;
    const float d2 = ( ey * ey ) + ( ex * ex );
    if ( !ocean_gt( rad * rad, d2 ) )
        return;
    const float age = ( B.age * s ) + ( A.age * t );
    const float spd = ( B.speed * s ) + ( A.speed * t );
    const float r = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( d2 ) ) );
    const float m = _mm_cvtss_f32( _mm_max_ss( _mm_setzero_ps( ), _mm_set_ss( r - ( wave.inner_ratio * rad ) ) ) );
    if ( !ocean_gt( age - ( m / spd ), 0.0f ) )
        return;
    const float life = ( B.life * s ) + ( A.life * t );
    if ( !ocean_gt( life, age ) )
        return;
    const float rn = r / rad;
    const float fade = ( B.fade_time * s ) + ( A.fade_time * t );
    float ff = 0.25f;
    if ( ocean_gt( wave.inner_ratio, rn ) )
    {
        const float k = rn / wave.inner_ratio;
        ff = ( 1.0f - k ) + ( k * 0.25f );
    }
    if ( ocean_gt( rn, wave.outer_ratio ) )
        ff = ( ( 1.0f - rn ) / ( 1.0f - wave.outer_ratio ) ) * ff;
    const float w = _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( ( life - age ) / fade ), _mm_set_ss( 1.0f ) ) );
    foam = ( w * ff ) + foam;
}

// The tick both wake waves share @0x52c210 (thiscall, float dt; al): both
// records age by dt, and the wave stays alive unless its refcount (+4) is 1 AND
// both records are past their life.
inline bool wake_tick( wake_wave_t& wave, int refcount, float dt )
{
    wave.a.age = dt + wave.a.age;
    wave.b.age = dt + wave.b.age;
    if ( refcount != 1 )
        return true;
    return !( ocean_gt( wave.a.age, wave.a.life ) && ocean_gt( wave.b.age, wave.b.life ) );
}

// ---------------------------------------------------------------------------
// THE WAVE ENTITIES' WAVE CREATION: each entity's slot 6 packs a descriptor
// from its loaded properties and its transform component (entity +0x50), and
// its allocator on g_vuwater runs operator new, the wave's constructor, a pool
// node and the tree insert 0x529a90. Recreated here is the descriptor and the
// constructor - the members a wave object holds once built - as an IMAGE of
// the object's words, so it can be compared with retail's word for word. The
// pool and the tree are VuWater's, not this.
//
// Two words of every wave's bounds, +24 and +40, are written from STACK
// TEMPORARIES the constructors never initialise (the high halves of the movq
// pairs that store +20 and +36): their value is UNKNOWN and nothing here
// reproduces it.
//
// The ocean constructor's spectrum, field and patch arrays (BuildSpectrum
// @0x59d220 onward) are not part of the image: c_ocean in water.hxx carries
// its own, and that path is not yet a transliteration.
struct wave_image_t
{
    std::uint32_t word[ 64 ]{};
    bool known[ 64 ]{};

    void set( int offset, std::uint32_t value )
    {
        word[ offset / 4 ] = value;
        known[ offset / 4 ] = true;
    }
    void setf( int offset, float value )
    {
        std::uint32_t bits;
        std::memcpy( &bits, &value, 4 );
        set( offset, bits );
    }
    float f( int offset ) const
    {
        float value;
        std::memcpy( &value, &word[ offset / 4 ], 4 );
        return value;
    }
};

// What a constructor reads: the entity's words and its transform component's,
// by byte offset.
struct wave_source_t
{
    const std::uint32_t* entity = nullptr;      // entity words from +0x60
    const std::uint32_t* transform = nullptr;   // component words from +0x80
    std::uint32_t e( int offset ) const { return entity[ ( offset - 0x60 ) / 4 ]; }
    std::uint32_t t( int offset ) const { return transform[ ( offset - 0x80 ) / 4 ]; }
};

inline float wave_bits( std::uint32_t u )
{
    float value;
    std::memcpy( &value, &u, 4 );
    return value;
}

// VuWaterWave's constructor @0x5a6390 (called with 0): +56 / +60 / +64 / +68
// zero, +72 = 1.0, +12 .. +52 its defaults (every subclass but the infinite
// ocean overwrites them).
inline void wave_base( wave_image_t& w )
{
    // the box (-FLT_MAX x3, +24 a stack word: UNKNOWN; FLT_MAX x3, +40 a stack
    // word: UNKNOWN), centre 0 / 0, radius FLT_MAX - what VuWaterInfiniteOcean
    // Wave keeps (it has no bounds of its own)
    w.set( 12, 0xFF7FFFFFu );
    w.set( 16, 0xFF7FFFFFu );
    w.set( 20, 0xFF7FFFFFu );
    w.set( 28, 0x7F7FFFFFu );
    w.set( 32, 0x7F7FFFFFu );
    w.set( 36, 0x7F7FFFFFu );
    w.set( 44, 0 );
    w.set( 48, 0 );
    w.set( 52, 0x7F7FFFFFu );
    w.set( 56, 0 );
    w.set( 60, 0 );
    w.set( 64, 0 );
    w.set( 68, 0 );
    w.set( 72, 0x3F800000u );
}

// The x87 fsin / fcos / fsincos the constructors run on a float: measured
// under Unicorn, whose x87 trig is a host double function - see wake_vertex.
inline float wave_x87_sin( float a ) { return static_cast< float >( std::sin( static_cast< double >( a ) ) ); }
inline float wave_x87_cos( float a ) { return static_cast< float >( std::cos( static_cast< double >( a ) ) ); }

// The ocean waves' shared constructor @0x59cb10 and the scalar half of
// VuWaterOceanWave_Update @0x59ccb0 on descriptor words d[ 0 .. 7 ] (the
// entity's +0x68 .. +0x87: Complexity, Bin Siz, Wave Direction, Gravity, Wind
// Speed, Directional Power, Suppression Wave Length, Height Multiplier).
inline void ocean_construct( wave_image_t& w, const std::uint32_t* d )
{
    wave_base( w );
    int shift = 0;
    int size = 1;
    int mask = 0;
    if ( d[ 0 ] != 0 )   // the complexity differs from the ctor's zero
    {
        shift = static_cast< int >( d[ 0 ] );
        size = 1 << shift;
        mask = size - 1;
    }
    for ( int k = 0; k < 8; ++k )
        w.set( 76 + 4 * k, d[ k ] );
    w.set( 108, static_cast< std::uint32_t >( shift ) );
    w.set( 112, static_cast< std::uint32_t >( size ) );
    w.set( 116, static_cast< std::uint32_t >( mask ) );
    w.setf( 120, static_cast< float >( size ) / wave_bits( d[ 1 ] ) );
    const float angle = wave_bits( d[ 2 ] ) - 3.1415927f;
    w.setf( 124, angle );
    w.setf( 128, wave_x87_sin( angle ) );
    w.setf( 132, wave_x87_cos( angle ) );
    // +136 the time (ctor 0.0); buffer 0's time = it; buffer 1's = its own
    // zero minus 1.0; current 0, previous 1.
    w.set( 136, 0 );
    w.set( 140, 0 );
    w.set( 164, 0 );
    w.set( 168, 1 );
    w.set( 176, 0 );
    w.set( 180, 0 );
    w.set( 192, 0 );
    w.set( 196, 0xBFF00000u );
}

// VuRectangularOceanWaveEntity_onEnable @0x5d8ce0: buildRegionDesc @0x5d8e20,
// then VuWater_createWave @0x528b40 -> VuWaterRectangularOceanWave_ctor
// @0x59d940, whose bounds are @0x59dbf0.
inline wave_image_t rectangular_ocean_create( const wave_source_t& s )
{
    std::uint32_t d[ 16 ];
    for ( int k = 0; k < 8; ++k )
        d[ k ] = s.e( 0x68 + 4 * k );
    for ( int k = 0; k < 4; ++k )
        d[ 8 + k ] = s.t( 0x80 + 4 * k );
    d[ 12 ] = s.t( 0xB0 );
    d[ 13 ] = s.t( 0xB4 );
    d[ 14 ] = s.e( 0x8C );   // Decay Ratio X %
    d[ 15 ] = s.e( 0x90 );   // Decay Ratio Y %
    wave_image_t w{};
    ocean_construct( w, d );
    for ( int k = 0; k < 8; ++k )
        w.set( 208 + 4 * k, d[ 8 + k ] );
    const float half_y = w.f( 228 ) * 0.5f;
    const float half_x = w.f( 224 ) * 0.5f;
    const float y = w.f( 212 );
    const float x = w.f( 208 );
    w.setf( 12, x - half_x );
    w.setf( 16, y - half_y );
    w.set( 20, w.word[ 216 / 4 ] );
    w.setf( 28, half_x + x );
    w.setf( 32, half_y + y );
    w.set( 36, w.word[ 216 / 4 ] );
    w.setf( 44, x );
    w.setf( 48, y );
    w.setf( 52, _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( half_y * half_y ) + ( half_x * half_x ) ) ) ) );
    return w;
}

// The circular one's slot 6 @0x5d9340: its descriptor @0x5d9440 (Radius and
// Decay Ratio % in place of the scale and the two decays), then @0x528c20 ->
// the ctor @0x59dcd0 with its bounds inline.
inline wave_image_t circular_ocean_create( const wave_source_t& s )
{
    std::uint32_t d[ 14 ];
    for ( int k = 0; k < 8; ++k )
        d[ k ] = s.e( 0x68 + 4 * k );
    for ( int k = 0; k < 4; ++k )
        d[ 8 + k ] = s.t( 0x80 + 4 * k );
    d[ 12 ] = s.e( 0x8C );   // Radius
    d[ 13 ] = s.e( 0x90 );   // Decay Ratio %
    wave_image_t w{};
    ocean_construct( w, d );
    for ( int k = 0; k < 6; ++k )
        w.set( 208 + 4 * k, d[ 8 + k ] );
    const float x = w.f( 208 );
    const float y = w.f( 212 );
    const float r = w.f( 224 );
    w.setf( 12, x - r );
    w.setf( 16, y - r );
    w.set( 20, w.word[ 216 / 4 ] );
    w.setf( 28, r + x );
    w.setf( 32, r + y );
    w.set( 36, w.word[ 216 / 4 ] );
    w.setf( 44, x );
    w.setf( 48, y );
    w.setf( 52, r );
    return w;
}

// VuBankedTurnWaveEntity's slot 6 @0x5d35f0: the 40-byte descriptor is the
// position (16 bytes from the component's +0x80), the component's +0xA8, then
// the entity's +0x60 .. +0x73 (Outer Radius, Inner Radius, Height, Angular
// Size, Angular Decay Ratio); @0x528f40 -> the ctor @0x5a4d80, which copies it
// flat to +76 and derives the kernel's direction (-fsin, fcos of +92) and its
// extent (+108 * 0.5).
inline wave_image_t banked_turn_create( const wave_source_t& s )
{
    std::uint32_t d[ 10 ];
    for ( int k = 0; k < 4; ++k )
        d[ k ] = s.t( 0x80 + 4 * k );
    d[ 4 ] = s.t( 0xA8 );
    for ( int k = 0; k < 5; ++k )
        d[ 5 + k ] = s.e( 0x60 + 4 * k );
    wave_image_t w{};
    wave_base( w );
    for ( int k = 0; k < 10; ++k )
        w.set( 76 + 4 * k, d[ k ] );
    const float angle = w.f( 92 );
    w.setf( 116, -wave_x87_sin( angle ) );
    w.setf( 120, wave_x87_cos( angle ) );
    w.setf( 124, w.f( 108 ) * 0.5f );
    const float x = w.f( 76 );
    const float y = w.f( 80 );
    const float outer = w.f( 96 );
    w.setf( 12, x - outer );
    w.setf( 16, y - outer );
    w.set( 20, w.word[ 84 / 4 ] );
    w.setf( 28, x + outer );
    w.setf( 32, y + outer );
    w.setf( 36, w.f( 84 ) + w.f( 104 ) );
    w.setf( 44, x );
    w.setf( 48, y );
    w.setf( 52, outer );
    return w;
}

// VuWhirlpoolWaveEntity's slot 6 @0x5d7d90: the position, then the entity's
// +0x60 .. +0x77 (whirlpool_t's note above names each); @0x528d00 -> the ctor
// @0x5a1470, flat to +76; +116 stays the ctor's zero. Its bounds reach DOWN by
// the depth and not up.
inline wave_image_t whirlpool_create( const wave_source_t& s )
{
    wave_image_t w{};
    wave_base( w );
    for ( int k = 0; k < 4; ++k )
        w.set( 76 + 4 * k, s.t( 0x80 + 4 * k ) );
    for ( int k = 0; k < 6; ++k )
        w.set( 92 + 4 * k, s.e( 0x60 + 4 * k ) );
    w.set( 116, 0 );
    const float x = w.f( 76 );
    const float y = w.f( 80 );
    const float outer = w.f( 92 );
    w.setf( 12, x - outer );
    w.setf( 16, y - outer );
    w.setf( 20, w.f( 84 ) - w.f( 100 ) );
    w.setf( 28, x + outer );
    w.setf( 32, y + outer );
    w.set( 36, w.word[ 84 / 4 ] );
    w.setf( 44, x );
    w.setf( 48, y );
    w.setf( 52, outer );
    return w;
}

// ---------------------------------------------------------------------------
// THE DIRECTIONAL AND BUMP WAVES' CREATION. Their constructors build the
// wave's local frame as a matrix and store its inverse - the transform their
// kernels read (+192 on the directional, +176 on the bump). The matrix
// helpers, by address:
//
//   VuMatrix_identity   @0x410a40   (ecx out)
//   VuMatrix_fromScale  @0x407db0   (ecx out, edx vec3*): diag(x, y, z, 1)
//   0x40b5f0            (ecx out, xmm1 angle): a rotation about z from x87
//                        fsincos - m[0] = m[5] = cos, m[1] = sin, m[4] = -sin
//   0x410ab0            (ecx out, edx vec3*): hull_translation
//   VuMatrix_multiply   @0x4078c0   hull_multiply (out = A . B)
//   0x4157c0            hull_invert, in place
//
// the last three already verified on the boat side (boat/hull.hxx).
inline void wave_matrix_identity( float* m )   // @0x410a40
{
    const float i[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    std::memcpy( m, i, 64 );
}

inline void wave_matrix_scale( float* m, const float* v )   // @0x407db0
{
    const float s[ 16 ] = { v[ 0 ], 0, 0, 0, 0, v[ 1 ], 0, 0, 0, 0, v[ 2 ], 0, 0, 0, 0, 1 };
    std::memcpy( m, s, 64 );
}

inline void wave_matrix_rotation_z( float* m, float angle )   // @0x40b5f0
{
    const float s = wave_x87_sin( angle );
    const float c = wave_x87_cos( angle );
    const float r[ 16 ] = { c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    std::memcpy( m, r, 64 );
}

// The frame both constructors build (@0x59acd0 and @0x5a4020, the same
// sequence): identity, times the scale (+96 * 0.5, +100 * 0.5, z), times the
// rotation by +92, times the translation by +76; copied to `inverse` and
// inverted in place.
inline void wave_frame( wave_image_t& w, int matrix, int inverse, float z_scale )
{
    float m[ 16 ];
    float t[ 16 ];
    float o[ 16 ];
    wave_matrix_identity( m );
    const float scale[ 3 ] = { w.f( 96 ) * 0.5f, w.f( 100 ) * 0.5f, z_scale };
    wave_matrix_scale( t, scale );
    hull_multiply( m, o, t );
    std::memcpy( m, o, 64 );
    wave_matrix_rotation_z( t, w.f( 92 ) );
    hull_multiply( m, o, t );
    std::memcpy( m, o, 64 );
    const float at[ 3 ] = { w.f( 76 ), w.f( 80 ), w.f( 84 ) };
    hull_translation( t, at );
    hull_multiply( m, o, t );
    std::memcpy( m, o, 64 );
    for ( int k = 0; k < 16; ++k )
        w.setf( matrix + 4 * k, m[ k ] );
    hull_invert( m );
    for ( int k = 0; k < 16; ++k )
        w.setf( inverse + 4 * k, m[ k ] );
}

// 0x59ae00, the bounds both share: the four corners of the rotated
// half-extent rectangle, folded through minss / maxss in retail's order
// (seeded with +-FLT_MAX), z from +84 -+ +104, and the radius
// sqrt( hy*hy + hx*hx ). The fold's +24 / +40 come from a stack slot this
// function never writes for them: UNKNOWN, not reproduced.
inline void wave_rect_bounds( wave_image_t& w )
{
    const float c = wave_x87_cos( w.f( 92 ) );
    const float s = wave_x87_sin( w.f( 92 ) );
    const float hx = w.f( 96 ) * 0.5f;
    const float hy = w.f( 100 ) * 0.5f;
    const float x = w.f( 76 );
    const float y = w.f( 80 );
    const float a = ( -s ) * hx;
    const float b = s * hy;
    const float cc = c * hy;
    const float d = c * hx;
    const float y1 = y - a;
    const float x1 = x - d;
    const float y2 = a + y;
    const float x2 = d + x;
    auto mn = []( float p, float q ) { return _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( p ), _mm_set_ss( q ) ) ); };
    auto mx = []( float p, float q ) { return _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( p ), _mm_set_ss( q ) ) ); };
    const float big = 3.4028235e38f;
    float min_y = mn( y1 - cc, big );
    float min_x = mn( x1 - b, big );
    float max_y = mx( y1 - cc, -big );
    float max_x = mx( x1 - b, -big );
    min_y = mn( y2 - cc, min_y );
    min_x = mn( x2 - b, min_x );
    max_y = mx( y2 - cc, max_y );
    max_x = mx( x2 - b, max_x );
    min_y = mn( y2 + cc, min_y );
    min_x = mn( x2 + b, min_x );
    max_y = mx( y2 + cc, max_y );
    max_x = mx( x2 + b, max_x );
    min_y = mn( y1 + cc, min_y );
    min_x = mn( x1 + b, min_x );
    max_y = mx( y1 + cc, max_y );
    max_x = mx( x1 + b, max_x );
    w.setf( 12, min_x );
    w.setf( 16, min_y );
    w.setf( 20, w.f( 84 ) - w.f( 104 ) );
    w.setf( 28, max_x );
    w.setf( 32, max_y );
    w.setf( 36, w.f( 104 ) + w.f( 84 ) );
    w.setf( 44, x );
    w.setf( 48, y );
    w.setf( 52, _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( hy * hy ) + ( hx * hx ) ) ) ) );
}

// VuDirectionalWaveEntity's slot 6 @0x5d9d40: the 48-byte descriptor
// @0x5d9eb0 - the position, the component's +0xA8 angle and +0xB0 / +0xB4
// scale, Max Height, Speed / Period, (scale y * 0.5) / Period and the two
// decay ratios - then the allocator @0x528910 (zeroes +76 .. +124), the ctor
// @0x59acd0 (flat copy to +76, the frame at +128, its inverse at +192) and
// the bounds 0x59ae00.
inline wave_image_t directional_create( const wave_source_t& s )
{
    wave_image_t w{};
    wave_base( w );
    for ( int k = 0; k < 4; ++k )
        w.set( 76 + 4 * k, s.t( 0x80 + 4 * k ) );
    w.set( 92, s.t( 0xA8 ) );
    w.set( 96, s.t( 0xB0 ) );
    w.set( 100, s.t( 0xB4 ) );
    w.set( 104, s.e( 0x6C ) );
    w.setf( 108, wave_bits( s.e( 0x70 ) ) / wave_bits( s.e( 0x74 ) ) );
    w.setf( 112, ( w.f( 100 ) * 0.5f ) / wave_bits( s.e( 0x74 ) ) );
    w.set( 116, s.e( 0x78 ) );
    w.set( 120, s.e( 0x7C ) );
    w.set( 124, 0 );
    wave_frame( w, 128, 192, 1.0f );
    wave_rect_bounds( w );
    return w;
}

// VuBumpWaveEntity's slot 6 @0x5d3e70: the 36-byte descriptor - the
// position, the angle and scale as above, Max Height and Lateral Decay Ratio
// % - then @0x528e90 and the ctor @0x5a4020 (flat copy to +76 .. +111, the
// frame at +112 with the height as its z scale, the inverse at +176) and the
// same bounds.
inline wave_image_t bump_create( const wave_source_t& s )
{
    wave_image_t w{};
    wave_base( w );
    for ( int k = 0; k < 4; ++k )
        w.set( 76 + 4 * k, s.t( 0x80 + 4 * k ) );
    w.set( 92, s.t( 0xA8 ) );
    w.set( 96, s.t( 0xB0 ) );
    w.set( 100, s.t( 0xB4 ) );
    w.set( 104, s.e( 0x60 ) );
    w.set( 108, s.e( 0x64 ) );
    wave_frame( w, 112, 176, w.f( 104 ) );
    wave_rect_bounds( w );
    return w;
}

// ---------------------------------------------------------------------------
// THE WAVES' SLOT 2 TICKS (thiscall, float dt; al = still alive).
//
// +4 IS A REFCOUNT, NOT A MODE: VuWaterWave's ctor @0x5a6390 sets it to 1,
// every allocator adds one for its owner, the water tick @0x529e30 drops the
// manager's and the entity's onDisable drops its own. So `+4 != 1` - which
// the family's notes elsewhere call "mode != 1" - reads "someone besides the
// water manager still holds this wave". The wake's tick above takes it the
// same way.
inline bool whirlpool_tick( float& clock, int refcount, float dt )   // @0x5a1680, clock +0x74
{
    clock = dt + clock;
    return refcount != 1;
}

inline bool directional_tick( float& clock, int refcount, float dt )   // @0x59aad0, clock +0x7c
{
    clock = dt + clock;
    return refcount != 1;
}

// @0x59c380, shared by the banked turn, bump, directional flow and ramp
// waves: no clock at all.
inline bool static_wave_tick( int refcount )
{
    return refcount != 1;
}

// @0x5a34b0: the point wave's lifetime. Past (rate * outer) + tail width it
// fires its callback (+0x8c, the entity's VuPointWaveCallbackIF - its slot 0
// is the entity's OnExpired path) once, guarded by +0x90 and by the callback
// being set, and from then on lives only while referenced. +4 is read AFTER
// the callback (0x5a34f0), which matters: VuPointWaveEntity's OnExpired
// @0x5d4b60 drops the entity's reference there - hence `const int&`.
struct point_wave_life_t
{
    float tail_width = 0.0f;   // +0x5c
    float outer_radius = 0.0f; // +0x68
    float elapsed = 0.0f;      // +0x78
    float rate = 0.0f;         // +0x7c
    bool has_callback = false; // +0x8c != 0
    bool expired = false;      // +0x90
};

inline bool point_wave_tick( point_wave_life_t& wave, const int& refcount, float dt, void ( *expire )( void* ), void* user )
{
    wave.elapsed = dt + wave.elapsed;
    const float limit = ( wave.rate * wave.outer_radius ) + wave.tail_width;
    if ( !ocean_gt( wave.elapsed, limit ) )
        return true;
    if ( !wave.expired && wave.has_callback )
        expire( user );
    wave.expired = true;
    return refcount != 1;
}

// @0x59cc00, the three ocean waves' tick: NOT a fixed-step accumulator - it
// advances the double time by dt, flips BOTH buffer indices ((i + 1) % 2,
// retail's signed-remainder idiom), rebuilds the field into the new current
// buffer (EvolveField @0x59d4d0, VuWaterMap_BuildBezierPatches @0x608bb0,
// 0x59d640, BuildBezierPatches @0x59d6c0 - the hook) and stamps that buffer's
// time, every tick. A wave held only by the manager returns false at once.
struct ocean_clock_t
{
    double time = 0.0;          // +0x88
    int current = 0;            // +0xa4
    int previous = 1;           // +0xa8
    double buffer_time[ 2 ]{};  // +0xb0, +0xc0
};

inline bool ocean_tick( ocean_clock_t& wave, int refcount, float dt, void ( *rebuild )( void* ), void* user )
{
    if ( refcount == 1 )
        return false;
    wave.time = static_cast< double >( dt ) + wave.time;
    wave.current = ( wave.current + 1 ) % 2;
    wave.previous = ( wave.previous + 1 ) % 2;
    rebuild( user );
    wave.buffer_time[ wave.current ] = wave.time;
    return true;
}

// ---------------------------------------------------------------------------
// VuWater's "Water" tick @0x529e30 (thiscall, float dt), registered with the
// tick manager by VuWater's init @0x528200. The wave list is a singly linked
// list of pool nodes {wave, next} at water +0x30; the pool's free list is at
// +0x10 with its count at +8. Per node, in list order, with the next node
// taken BEFORE the call:
//
//   alive = wave->slot2( wave +0x48 * dt )      the wave's own time scale
//   if ( !alive )
//       unlink the FIRST node holding this wave (not necessarily the one
//       being visited - a wave listed twice loses its earlier node), push
//       that node on the free list, remove the wave from the trees
//       (0x529b90), and drop its refcount (+4), destroying it through slot 0
//       with 1 once it reaches zero
//
// then the renderer's kick 0x595610 on +0x1C. Nodes and waves are held by
// index (retail's pointers; -1 for null). The wave's tick, the tree removal,
// the destructor and the renderer are Vu virtuals / the render thread: hooks.
struct water_list_node_t
{
    int wave = -1;
    int next = -1;
};

struct water_list_t
{
    std::vector< water_list_node_t > nodes;
    int used = -1;        // +0x30
    int free = -1;        // +0x10
    int free_count = 0;   // +0x08
};

struct water_tick_hooks_t
{
    void* user = nullptr;
    bool ( *tick )( void* user, int wave, float dt ) = nullptr;
    void ( *remove )( void* user, int wave ) = nullptr;
    void ( *destroy )( void* user, int wave, int flag ) = nullptr;
    void ( *render )( void* user ) = nullptr;
};

inline void water_tick( water_list_t& list, const float* time_scale, int* refcount, float dt,
    const water_tick_hooks_t& hooks )
{
    for ( int at = list.used; at >= 0; )
    {
        const int next = list.nodes[ at ].next;
        const int wave = list.nodes[ at ].wave;
        if ( !hooks.tick( hooks.user, wave, time_scale[ wave ] * dt ) )
        {
            int prev = -1;
            int found = list.used;
            while ( found >= 0 && list.nodes[ found ].wave != wave )
            {
                prev = found;
                found = list.nodes[ found ].next;
            }
            if ( found >= 0 )
            {
                if ( prev >= 0 )
                    list.nodes[ prev ].next = list.nodes[ found ].next;
                else
                    list.used = list.nodes[ found ].next;
                list.nodes[ found ].next = list.free;
                ++list.free_count;
                list.free = found;
                hooks.remove( hooks.user, wave );
                if ( --refcount[ wave ] == 0 )
                    hooks.destroy( hooks.user, wave, 1 );
            }
        }
        at = next;
    }
    hooks.render( hooks.user );
}

// The walk retail performs around that body: count vertices from the context's
// base pointer, advancing by a stride given in BYTES.
inline void whirlpool_apply( const whirlpool_t& wave, unsigned char* vertices,
    std::size_t count, std::size_t stride )
{
    for ( std::size_t index = 0; index < count; ++index )
    {
        unsigned char* vertex = vertices + index * stride;
        float position[ 2 ]{};
        std::memcpy( position, vertex, sizeof( position ) );
        float displacement[ 3 ]{};
        std::memcpy( displacement, vertex + 16, sizeof( displacement ) );
        float height = 0.0f;
        std::memcpy( &height, vertex + 32, sizeof( height ) );
        whirlpool_vertex( wave, position[ 0 ], position[ 1 ], displacement, height );
        std::memcpy( vertex + 16, displacement, sizeof( displacement ) );
        std::memcpy( vertex + 32, &height, sizeof( height ) );
    }
}

// ---------------------------------------------------------------------------
// THE +96 KERNELS: what each wave's slot 3 runs when ctx+0x60 is set - which
// the water renderer's vertex pass 0x597D70 does (ctx+0x60 = 1, ctx+0x64 = 0,
// a 32-byte stride), never the physics query (VuFluidSolver_prepare passes
// 0). They are SEPARATE BODIES, not the base kernels behind a test: they
// write a different record -
//
//   +0 x   +4 y   +8 the height (on entry to 0x529F70's +96 branch, the z)
//   +0xC / +0x10 the in-plane displacement (the height's gradient)
//   +0x14 foam   +0x18 1.0 / +0x1C 0 (0x529ED0 / 0x529F70 store them)
//
// and read members the base kernels do not (Foaminess on the whirlpool and
// the point wave, the ramp's +256 / +260). Each tag variant (ctx+0x64 set) is
// the same body behind `tags[ i ] != ctx+0x5C -> skip` (census/vdiff.py on
// each pair; the oracle runs both). Every one is transliterated from its
// disassembly in retail's operand order.

// VuWaterWhirlpoolWave @0x5a1f80 (tag @0x5a2160).
inline void whirlpool_vertex96( const whirlpool_t& wave, float x, float y, float& height, float* flow, float& foam )
{
    const float dx = x - wave.centre_x;
    const float dy = y - wave.centre_y;
    const float outer = wave.outer_radius;
    const float radius2 = ( dy * dy ) + ( dx * dx );
    if ( !( ( outer * outer ) > radius2 ) )
        return;
    const float rise_inner = -wave.depth;
    if ( !( radius2 > ( wave.inner_radius * wave.inner_radius ) ) )
    {
        foam = wave.foaminess + foam;
        height = height + rise_inner;
        return;
    }
    const float radius = std::sqrt( radius2 ) + 1.1920929e-7f;
    const float span = 1.0f / ( outer - wave.inner_radius );
    const float t = ( radius - wave.inner_radius ) * span;
    const float across = std::sqrt( t );
    const float inv_r = 1.0f / radius;
    const float rise = rise_inner + ( wave.depth * across );
    const float inv_across = 1.0f / across;
    const float gy = ( ( ( ( inv_r * dy ) * span ) * 0.5f ) * inv_across ) * wave.depth;
    const float gx = ( ( ( ( inv_r * dx ) * span ) * 0.5f ) * inv_across ) * wave.depth;
    flow[ 1 ] = flow[ 1 ] + gy;
    flow[ 0 ] = gx + flow[ 0 ];
    foam = ( ( 1.0f - t ) * wave.foaminess ) + foam;
    height = height + rise;
}

// VuWaterRampWave @0x5a2fc0 (tag @0x5a3160). No spatial rejection, as the
// base. Its tests are the base's with the branches the other way round (so a
// NaN coordinate lands on the plateau here): |a| > 1 - band blends, and the
// blend's a < 0 half is the first.
inline void ramp_vertex96( const ramp_t& wave, float x, float y, float& height, float* flow )
{
    const float along = ( ( y * wave.along_y ) + ( x * wave.along_x ) ) + wave.along_offset;
    if ( !( std::fabs( along ) > ( 1.0f - wave.band ) ) )
    {
        height = ( wave.slope * along ) + height;
        flow[ 0 ] = wave.grad_x + flow[ 0 ];
        flow[ 1 ] = wave.grad_y + flow[ 1 ];
        return;
    }
    float shaped;
    float grad;
    if ( 0.0f > along )
    {
        const float u = along + 1.0f;
        shaped = ( ( u * u ) * wave.curve ) - 1.0f;
        grad = ( u * 2.0f ) * wave.curve;
    }
    else
    {
        const float u = 1.0f - along;
        shaped = 1.0f - ( ( u * u ) * wave.curve );
        grad = ( u * 2.0f ) * wave.curve;
    }
    const float half = wave.blend_height * 0.5f;
    height = ( half * shaped ) + height;
    flow[ 0 ] = ( half * ( grad * wave.along_x ) ) + flow[ 0 ];
    flow[ 1 ] = flow[ 1 ] + ( half * ( grad * wave.along_y ) );
}

// VuWaterPointWave @0x5a39b0 (tag @0x5a3ca0): the base's ring gates, the
// height term sin * tail, an in-plane gradient in place of the base's z
// displacement, and a foam term outside the ring test (reached by every
// vertex past the first two gates). The x87 fsincos as wave_x87_* (host
// double, as under the oracle).
inline void point_wave_vertex96( const point_wave_t& wave, float x, float y, float& height, float* flow, float& foam )
{
    const float dx = x - wave.centre_x;
    const float dy = y - wave.centre_y;
    const float radius = std::sqrt( ( dy * dy ) + ( dx * dx ) );
    if ( !( wave.outer_radius > radius ) )
        return;
    const float from_inner = radius - wave.inner_radius;
    const float span = from_inner > 0.0f ? from_inner : 0.0f;   // maxss( d, 0 )
    const float scaled = wave.rate * span;
    if ( !( wave.clock > scaled ) )
        return;
    const float edge = scaled + wave.tail_width;
    const float amount = ( ( ( wave.outer_radius - wave.inner_radius ) - span ) * wave.amplitude ) * wave.amplitude_scale;
    if ( edge > wave.clock && amount > 0.0f )
    {
        const float phase = ( wave.clock - scaled ) * wave.phase_scale;
        const float k = ( edge - wave.clock ) * wave.slope;
        const float cosine = wave_x87_cos( phase );
        const float sine = wave_x87_sin( phase );
        const float tail = k * amount;
        const float inv_r = 1.0f / radius;
        height = ( sine * tail ) + height;
        const float rel = radius - wave.inner_radius;
        float nx = 0.0f;
        float ny = inv_r * dy;
        if ( rel >= 0.0f )
            nx = inv_r * dx;
        if ( !( rel >= 0.0f ) )
            ny = 0.0f;
        const float rnx = wave.rate * nx;
        const float rny = wave.rate * ny;
        const float ct = cosine * tail;
        const float gy = ( ( ( -( ( wave.amplitude * ny ) * wave.amplitude_scale ) * k ) + ( ( wave.slope * rny ) * amount ) ) * sine ) +
                         ( -( wave.phase_scale * rny ) * ct );
        const float gx = ( ( ( -( ( wave.amplitude * nx ) * wave.amplitude_scale ) * k ) + ( ( wave.slope * rnx ) * amount ) ) * sine ) +
                         ( -( wave.phase_scale * rnx ) * ct );
        flow[ 1 ] = flow[ 1 ] + gy;
        flow[ 0 ] = gx + flow[ 0 ];
    }
    const float t = ( ( wave.clock - wave.tail_width ) * wave.speed ) * wave.amplitude_scale;
    const float f0 = 1.0f - t;
    const float f = f0 < 1.0f ? f0 : 1.0f;   // minss( 1 - t, 1 )
    foam = ( ( ( wave.inv_amplitude * amount ) * wave.foaminess ) * f ) + foam;
}

// VuWaterBumpWave @0x5a4610 (tag @0x5a49a0): the base's raised cosine across
// b times the edge fade along a, written as height plus its in-plane gradient.
// The fade's derivative term is scaled by fade * lift (retail reuses that
// stack slot), the cosine's by the fade alone; a's row is negated when a < 0
// (or NaN). Both x87 fsincos.
inline void bump_vertex96( const bump_t& wave, float x, float y, float& height, float* flow )
{
    const float* m = wave.transform;
    const float a = ( ( m[ 4 ] * y ) + ( m[ 0 ] * x ) ) + m[ 12 ];
    const float b = ( ( m[ 5 ] * y ) + ( m[ 1 ] * x ) ) + m[ 13 ];
    const float abs_a = std::fabs( a );
    const float abs_b = std::fabs( b );
    const float big = abs_a > abs_b ? abs_a : abs_b;   // maxss( |a|, |b| )
    if ( !( 1.0f > big ) )
        return;
    const float f1 = wave_fold( ( b * 3.1415927f ) + 3.1415927f );
    const float cos1 = wave_x87_cos( f1 );
    const float sin1 = wave_x87_sin( f1 );
    const float lift = ( cos1 + 1.0f ) * ( wave.amplitude * 0.5f );
    float fade = 1.0f;
    float sin2 = 0.0f;   // read only when set (the same test guards both)
    if ( abs_a > wave.edge_a )
    {
        const float f2 = wave_fold( ( ( ( abs_a - wave.edge_a ) / ( 1.0f - wave.edge_a ) ) * 3.1415927f ) + 3.1415927f );
        fade = ( wave_x87_cos( f2 ) + 1.0f ) * 0.5f;
        sin2 = wave_x87_sin( f2 );
    }
    const float fl = fade * lift;
    height = height + fl;
    float m0 = m[ 0 ];
    float m4 = m[ 4 ];
    if ( !( a >= 0.0f ) )
    {
        m0 = -m0;
        m4 = -m4;
    }
    const float s = ( wave.amplitude * -0.5f ) * sin1;
    const float gby = ( m[ 5 ] * 3.1415927f ) * s;
    const float gbx = ( m[ 1 ] * 3.1415927f ) * s;
    float gax = 0.0f;
    float gay = 0.0f;
    if ( abs_a > wave.edge_a )
    {
        const float inv = 1.0f / ( 1.0f - wave.edge_a );
        const float s2 = sin2 * -0.5f;
        gax = ( ( inv * m0 ) * 3.1415927f ) * s2;
        gay = ( ( inv * m4 ) * 3.1415927f ) * s2;
    }
    flow[ 0 ] = ( ( gax * fl ) + ( gbx * fade ) ) + flow[ 0 ];
    flow[ 1 ] = ( ( gay * fl ) + ( gby * fade ) ) + flow[ 1 ];
}

// VuWaterBankedTurnWave @0x5a5ab0 (tag @0x5a5f10): the base's annulus, angle
// and edge fade, with differences the disassembly shows - the direction dot
// is MULTIPLIED by 1 / r (the base divides), the clamp is minss / maxss
// (a NaN dot passes through), the band's cosine comes from an x87 fsincos -
// and the height's in-plane gradient: the band term scaled by the fade, the
// fade's term by fade * lift, through d(acos) = -1 / sqrt( 1 - c * c ).
inline void banked_turn_vertex96( const banked_turn_t& wave, float x, float y, float& height, float* flow )
{
    const float dx = x - wave.centre_x;
    const float dy = y - wave.centre_y;
    const float outer = wave.outer_radius;
    const float radius2 = ( dy * dy ) + ( dx * dx );
    if ( !( ( outer * outer ) > radius2 ) || !( radius2 > ( wave.inner_radius * wave.inner_radius ) ) )
        return;
    const float radius = std::sqrt( radius2 );
    const float inv_r = 1.0f / radius;
    const float dot = ( ( wave.direction_y * dy ) + ( wave.direction_x * dx ) ) * inv_r;
    const float c1 = 1.0f < dot ? 1.0f : dot;   // minss( 1, dot )
    const float c = -1.0f > c1 ? -1.0f : c1;    // maxss( -1, . )
    const float angle = std::acos( c );
    if ( !( wave.extent > angle ) )
        return;
    const float phase = ( ( ( radius - wave.inner_radius ) / ( outer - wave.inner_radius ) ) * 6.2831855f ) - 3.1415927f;
    const float cos_b = wave_x87_cos( phase );
    const float sin_b = wave_x87_sin( phase );
    const float across = angle / wave.extent;
    const float lift = ( ( cos_b + 1.0f ) * 0.5f ) * wave.amplitude;
    float fade = 1.0f;
    float sin_e = 0.0f;   // read only when set (the same test guards both)
    if ( across > wave.edge )
    {
        const float f = wave_fold( ( ( ( across - wave.edge ) / ( 1.0f - wave.edge ) ) * 3.1415927f ) + 3.1415927f );
        sin_e = wave_x87_sin( f );
        fade = ( wave_x87_cos( f ) + 1.0f ) * 0.5f;
    }
    const float fl = fade * lift;
    height = height + fl;
    const float ny = inv_r * dy;
    const float nx = inv_r * dx;
    const float dacos = -1.0f / std::sqrt( 1.0f - ( c * c ) );
    const float inv_band = 1.0f / ( outer - wave.inner_radius );
    const float sb = sin_b * -0.5f;
    const float glx = ( ( ( inv_band * nx ) * 6.2831855f ) * sb ) * wave.amplitude;
    const float gly = ( ( ( inv_band * ny ) * 6.2831855f ) * sb ) * wave.amplitude;
    float gex = 0.0f;
    float gey = 0.0f;
    if ( across > wave.edge )
    {
        const float inv_r2 = 1.0f / radius2;
        const float inv_ext = 1.0f / wave.extent;
        const float inv_e = 1.0f / ( 1.0f - wave.edge );
        const float se = sin_e * -0.5f;
        gex = ( ( ( ( ( ( ( wave.direction_x * radius ) - ( c * nx ) ) * inv_r2 ) * dacos ) * inv_ext ) * inv_e ) * 3.1415927f ) * se;
        gey = ( ( ( ( ( ( ( wave.direction_y * radius ) - ( c * ny ) ) * inv_r2 ) * dacos ) * inv_ext ) * inv_e ) * 3.1415927f ) * se;
    }
    flow[ 0 ] = ( ( gex * fl ) + ( glx * fade ) ) + flow[ 0 ];
    flow[ 1 ] = ( ( gey * fl ) + ( gly * fade ) ) + flow[ 1 ];
}

// VuWaterDirectionalWave @0x59b860 (tag @0x59bde0): the base's travelling
// sine across b, faded along a and b, as height plus its in-plane gradient.
// Each fade's derivative term is scaled by the WHOLE height term h (retail
// reuses that stack slot), the phase term by fade_a, and the a-terms plus the
// phase term by fade_b; a row is negated when its coordinate is < 0 (or NaN).
// Three x87 fsincos.
inline void directional_vertex96( const directional_t& wave, float x, float y, float& height, float* flow )
{
    const float* m = wave.transform;
    const float a = ( ( m[ 4 ] * y ) + ( m[ 0 ] * x ) ) + m[ 12 ];
    const float b = ( ( m[ 5 ] * y ) + ( m[ 1 ] * x ) ) + m[ 13 ];
    const float abs_a = std::fabs( a );
    const float abs_b = std::fabs( b );
    const float big = abs_a > abs_b ? abs_a : abs_b;   // maxss( |a|, |b| )
    if ( !( 1.0f > big ) )
        return;
    const float fp = wave_fold( ( ( ( ( wave.phase_scale * b ) - ( wave.clock * wave.wavenumber ) ) * 2.0f ) * 3.1415927f ) + 3.1415927f );
    const float cos_p = wave_x87_cos( fp );
    const float sin_p = wave_x87_sin( fp );
    const float w = wave.amplitude * sin_p;
    float fade_a = 1.0f;
    float sin_ea = 0.0f;   // read only when set (the same test guards both)
    if ( abs_a > wave.edge_a )
    {
        const float f = wave_fold( ( ( ( abs_a - wave.edge_a ) / ( 1.0f - wave.edge_a ) ) * 3.1415927f ) + 3.1415927f );
        sin_ea = wave_x87_sin( f );
        fade_a = ( wave_x87_cos( f ) + 1.0f ) * 0.5f;
    }
    const float faw = fade_a * w;
    float fade_b = 1.0f;
    float sin_eb = 0.0f;
    if ( abs_b > wave.edge_b )
    {
        const float f = wave_fold( ( ( ( abs_b - wave.edge_b ) / ( 1.0f - wave.edge_b ) ) * 3.1415927f ) + 3.1415927f );
        sin_eb = wave_x87_sin( f );
        fade_b = ( wave_x87_cos( f ) + 1.0f ) * 0.5f;
    }
    const float h = fade_b * faw;
    height = height + h;
    float sm0 = m[ 0 ];
    float sm4 = m[ 4 ];
    if ( !( a >= 0.0f ) )
    {
        sm0 = -sm0;
        sm4 = -sm4;
    }
    const float sm1 = b >= 0.0f ? m[ 1 ] : -m[ 1 ];
    const float sm5 = b >= 0.0f ? m[ 5 ] : -m[ 5 ];
    const float ac = wave.amplitude * cos_p;
    const float gpx = ( ( ( wave.phase_scale * m[ 1 ] ) * 2.0f ) * 3.1415927f ) * ac;
    const float gpy = ( ( ( wave.phase_scale * m[ 5 ] ) * 2.0f ) * 3.1415927f ) * ac;
    float gax = 0.0f;
    float gay = 0.0f;
    if ( abs_a > wave.edge_a )
    {
        const float inv = 1.0f / ( 1.0f - wave.edge_a );
        const float s = sin_ea * -0.5f;
        gax = ( ( inv * sm0 ) * 3.1415927f ) * s;
        gay = ( ( inv * sm4 ) * 3.1415927f ) * s;
    }
    const float x1 = ( gax * h ) + ( gpx * fade_a );
    const float y1 = ( gay * h ) + ( gpy * fade_a );
    float gbx = 0.0f;
    float gby = 0.0f;
    if ( abs_b > wave.edge_b )
    {
        const float inv = 1.0f / ( 1.0f - wave.edge_b );
        const float s = sin_eb * -0.5f;
        gbx = ( ( inv * sm1 ) * 3.1415927f ) * s;
        gby = ( ( inv * sm5 ) * 3.1415927f ) * s;
    }
    flow[ 0 ] = ( ( x1 * fade_b ) + ( gbx * h ) ) + flow[ 0 ];
    flow[ 1 ] = ( ( y1 * fade_b ) + ( gby * h ) ) + flow[ 1 ];
}

// The ocean patch evaluator 0x5a0f70 (usercall patch @ecx, u @xmm1, w @xmm2;
// float* du, float* dw; retn 8; the value in xmm0) the ocean +96 kernels call:
// the bicubic value and both partials. The value's terms and order are
// ocean_patch_eval's (its first sum commuted, the same bits); the partials use
// retail's derivative weights - -3a*a, (1 - 3t)(3a), (2 - 3t)(3t), 3t*t.
inline float ocean_patch_eval_d( const float* p, float u, float w, float& du, float& dw )
{
    const float a = 1.0f - u;
    const float bw = 1.0f - w;
    const float a3 = ( a * a ) * a;
    const float b3 = ( bw * bw ) * bw;
    const float u3 = u * 3.0f;
    const float uu1 = ( u3 * a ) * a;
    const float u3u = u3 * u;
    const float uu2 = u3u * a;
    const float uu3 = ( u * u ) * u;
    const float w3 = w * 3.0f;
    const float w3w = w3 * w;
    const float ww2 = w3w * bw;
    const float ww1 = ( w3 * bw ) * bw;
    const float ww3 = ( w * w ) * w;
    const float da0 = ( a * -3.0f ) * a;
    const float du1 = ( 1.0f - u3 ) * ( a * 3.0f );
    const float du2 = ( 2.0f - u3 ) * u3;
    const float r0 = ( ( ( p[ 0 ] * b3 ) + ( p[ 11 ] * ww1 ) ) + ( p[ 10 ] * ww2 ) ) + ( p[ 3 ] * ww3 );
    float d = ( ( p[ 4 ] * b3 ) * du1 ) + ( r0 * da0 );
    d = d + ( p[ 12 ] * ww1 ) * du1;
    d = d + ( p[ 15 ] * ww2 ) * du1;
    d = d + ( p[ 9 ] * ww3 ) * du1;
    d = d + ( p[ 5 ] * b3 ) * du2;
    d = d + ( p[ 13 ] * ww1 ) * du2;
    d = d + ( p[ 14 ] * ww2 ) * du2;
    d = d + ( p[ 8 ] * ww3 ) * du2;
    d = d + ( p[ 1 ] * b3 ) * u3u;
    d = d + ( p[ 6 ] * ww1 ) * u3u;
    d = d + ( p[ 7 ] * ww2 ) * u3u;
    d = d + ( p[ 2 ] * ww3 ) * u3u;
    du = d;
    const float db0 = ( bw * -3.0f ) * bw;
    const float dw1 = ( 1.0f - w3 ) * ( bw * 3.0f );
    const float dw2 = ( 2.0f - w3 ) * w3;
    float e = ( ( ( ( p[ 0 ] * db0 ) + ( p[ 11 ] * dw1 ) ) + ( p[ 10 ] * dw2 ) ) + ( p[ 3 ] * w3w ) ) * a3;
    e = e + ( p[ 4 ] * db0 ) * uu1;
    e = e + ( p[ 12 ] * dw1 ) * uu1;
    e = e + ( p[ 15 ] * dw2 ) * uu1;
    e = e + ( p[ 9 ] * w3w ) * uu1;
    e = e + ( p[ 5 ] * db0 ) * uu2;
    e = e + ( p[ 13 ] * dw1 ) * uu2;
    e = e + ( p[ 14 ] * dw2 ) * uu2;
    e = e + ( p[ 8 ] * w3w ) * uu2;
    e = e + ( p[ 1 ] * db0 ) * uu3;
    e = e + ( p[ 6 ] * dw1 ) * uu3;
    e = e + ( p[ 7 ] * dw2 ) * uu3;
    e = e + ( p[ 2 ] * w3w ) * uu3;
    dw = e;
    const float bu[ 4 ] = { a3, uu1, uu2, uu3 };
    const float bv[ 4 ] = { b3, ww1, ww2, ww3 };
    return ocean_patch_eval( p, bu, bv );
}

// VuWaterRectangularOceanWave_accumulate @0x59f840 (tag @0x59fb80) and the
// circular @0x5a0920 (tag @0x5a0c40): the base kernels' footprint and fades on
// ONE buffer (+164's), the patch's value as height, and its partials turned
// back through the rotation as the in-plane gradient; past a decay edge the
// gradient is faded and gains the fade's own derivative times the height term.
// No time derivative (the base's z rate). The circle divides by its radius as
// a multiply by 1 / +224 (the base divides).
inline void ocean_patch_vertex96( const ocean_patch_t& wave, bool circle, float x, float y, float& height, float* flow )
{
    const float dx = x - wave.centre_x;
    const float dy = y - wave.centre_y;
    const float a = ( wave.cosine * dx ) - ( wave.sine * dy );
    const float b = ( wave.sine * dx ) + ( wave.cosine * dy );
    const float va = a * wave.scale;
    const float vb = b * wave.scale;
    const int ia = ocean_cell( va );
    const int ib = ocean_cell( vb );
    const float u = va - static_cast< float >( ia );
    const float w = vb - static_cast< float >( ib );
    const int cell = ( ( ib & wave.mask ) << wave.shift ) + ( ia & wave.mask );
    const float* patch = wave.patches[ wave.current ] + 16 * cell;
    if ( !circle )
    {
        const float ka = 2.0f / wave.shape[ 0 ];
        const float sa = ka * dx;
        const float ua = std::fabs( sa );
        if ( !ocean_gt( 1.0f, ua ) )
            return;
        const float kb = 2.0f / wave.shape[ 1 ];
        const float sb = kb * dy;
        const float ub = std::fabs( sb );
        if ( !ocean_gt( 1.0f, ub ) )
            return;
        float fa = 1.0f;
        if ( ocean_gt( ua, wave.shape[ 2 ] ) )
            fa = ( ua - 1.0f ) / ( wave.shape[ 2 ] - 1.0f );
        float fb = 1.0f;
        if ( ocean_gt( ub, wave.shape[ 3 ] ) )
            fb = ( ub - 1.0f ) / ( wave.shape[ 3 ] - 1.0f );
        float du;
        float dw;
        const float value = ocean_patch_eval_d( patch, u, w, du, dw );
        const float sdu = wave.scale * du;
        const float sdw = wave.scale * dw;
        float gx = ( wave.sine * sdw ) + ( wave.cosine * sdu );
        float gy = ( wave.cosine * sdw ) - ( wave.sine * sdu );
        const float h = ( value * fa ) * fb;
        if ( ocean_gt( ua, wave.shape[ 2 ] ) )
        {
            const float k = ocean_gt( 0.0f, sa ) ? -ka : ka;
            gx = ( gx * fa ) + ( ( k / ( wave.shape[ 2 ] - 1.0f ) ) * h );
        }
        if ( ocean_gt( ub, wave.shape[ 3 ] ) )
        {
            const float k = ocean_gt( 0.0f, sb ) ? -kb : kb;
            gy = ( gy * fb ) + ( ( k / ( wave.shape[ 3 ] - 1.0f ) ) * h );
        }
        flow[ 0 ] = flow[ 0 ] + gx;
        flow[ 1 ] = flow[ 1 ] + gy;
        height = height + h;
        return;
    }
    const float rr = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( b * b ) + ( a * a ) ) ) );
    const float inv_r = 1.0f / wave.shape[ 0 ];
    const float rn = inv_r * rr;
    if ( !ocean_gt( 1.0f, rn ) )
        return;
    float fade = 1.0f;
    if ( ocean_gt( rn, wave.shape[ 1 ] ) )
        fade = ( rn - 1.0f ) / ( wave.shape[ 1 ] - 1.0f );
    float du;
    float dw;
    const float value = ocean_patch_eval_d( patch, u, w, du, dw );
    const float sdu = wave.scale * du;
    const float sdw = wave.scale * dw;
    float gx = ( wave.sine * sdw ) + ( wave.cosine * sdu );
    float gy = ( wave.cosine * sdw ) - ( wave.sine * sdu );
    const float h = value * fade;
    if ( ocean_gt( rn, wave.shape[ 1 ] ) )
    {
        const float a2 = a * 2.0f;
        const float b2 = b * 2.0f;
        const float inv_rr2 = 1.0f / ( rr * 2.0f );
        const float inv_d = 1.0f / ( wave.shape[ 1 ] - 1.0f );
        gx = ( gx * fade ) + ( ( ( ( ( ( b2 * wave.sine ) + ( a2 * wave.cosine ) ) * inv_rr2 ) * inv_r ) * inv_d ) * h );
        gy = ( gy * fade ) + ( ( ( ( ( ( -wave.sine * a2 ) + ( b2 * wave.cosine ) ) * inv_rr2 ) * inv_r ) * inv_d ) * h );
    }
    flow[ 0 ] = flow[ 0 ] + gx;
    flow[ 1 ] = flow[ 1 ] + gy;
    height = h + height;
}

// VuWaterInfiniteOceanWave_SampleHeight @0x59e9e0 (slot 3 @0x59d900 with
// ctx+0x60 set; SampleHeight2 @0x59eb80 = the same body behind the ctx+100 tag
// test, read instruction for instruction: only the stack slot of du moves).
// The rectangular +96 body WITHOUT the centre, the footprint or any fade: the
// rotation as the base infinite kernel reads it, ONE buffer (+164's) through
// the evaluator 0x5a0f70, the value added to +8, its partials turned back
// through the rotation added to +0xC / +0x10. Foam (+0x14) untouched.
inline void ocean_infinite_vertex96( const ocean_patch_t& wave, float x, float y, float& height, float* flow )
{
    const float a = ( x * wave.cosine ) - ( y * wave.sine );
    const float b = ( wave.sine * x ) + ( y * wave.cosine );
    const float va = a * wave.scale;
    const float vb = b * wave.scale;
    const int ia = ocean_cell( va );
    const int ib = ocean_cell( vb );
    const float u = va - static_cast< float >( ia );
    const float w = vb - static_cast< float >( ib );
    const int cell = ( ( ib & wave.mask ) << wave.shift ) + ( ia & wave.mask );
    float du;
    float dw;
    const float value = ocean_patch_eval_d( wave.patches[ wave.current ] + 16 * cell, u, w, du, dw );
    const float sdu = wave.scale * du;
    const float sdw = wave.scale * dw;
    const float gx = ( wave.sine * sdw ) + ( wave.cosine * sdu );
    const float gy = ( wave.cosine * sdw ) - ( wave.sine * sdu );
    flow[ 1 ] = flow[ 1 ] + gy;
    flow[ 0 ] = gx + flow[ 0 ];
    height = height + value;
}