#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>

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
//                                              never reads, so it is not a
//                                              member here
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

// The tick both wake waves share @0x52c210 (thiscall, float dt; al): both
// records age by dt, and the wave stays alive unless its mode (+4) is 1 AND
// both records are past their life.
inline bool wake_tick( wake_wave_t& wave, int mode, float dt )
{
    wave.a.age = dt + wave.a.age;
    wave.b.age = dt + wave.b.age;
    if ( mode != 1 )
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
// zero, +72 = 1.0. Its +12 .. +52 defaults are overwritten by every subclass.
inline void wave_base( wave_image_t& w )
{
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
