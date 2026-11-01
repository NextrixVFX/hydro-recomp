#pragma once

#include <workspace/core/boat/math.hxx>
#include <workspace/core/boat/hull.hxx>
#include <workspace/core/world/pfx.hxx>
#include <workspace/core/world/surface.hxx>

#include <cstddef>
#include <cstdint>
#include <cstring>

// THE VuPfx NODE CLASSES' OWN SLOTS - the constant getters and the placement
// creates every node class carries (read from each vtable, census/vtref.py
// "VuPfx"; memory/particles.md maps the per-frame slot 1 of the instances).
// A node class's vtable:
//   slot 1  getType: a static VuRTTI descriptor (+0 the name, +4 the parent;
//           statically initialised .data, no CRT initializer);
//   slot 6  the instance's size (VuPfxProcess / VuPfxPattern: _purecall);
//   slot 7  the instance's placement create (thiscall node; void* memory;
//           retn 4): nothing for null memory, else the instance's words;
//   slot 8  (patterns) the particle's size; slot 9 (patterns) the particle's
//           placement init.
// An instance's slot 0 is its reset (VuPfxSystemInstance's start 0x58AC90 runs
// it on every pattern, a pattern on every child): pfx_*_reset below.
// Words are the instance's / particle's dwords by offset / 4; a create writes
// only the words named.

// 0x58B240 VuPfxPatternInstance's ctor (the patterns' slot 7 calls it first):
// vtable 0x750068, +0x14 .. +0x28 0 (the child list, the particle list, the
// count), the AABB min +0x2C .. +0x34 and max +0x3C .. +0x44 0 - its pad words
// +0x38 / +0x48 are stack words the ctor never sets (UNKNOWN: not written).
inline void pfx_pattern_instance_ctor( std::uint32_t* w )
{
    w[ 0 ] = 0x750068u;
    for ( int o = 0x14; o <= 0x28; o += 4 )
        w[ o / 4 ] = 0u;
    for ( int o : { 0x2C, 0x30, 0x34, 0x3C, 0x40, 0x44 } )
        w[ o / 4 ] = 0u;
}

// 0x58E8A0 the particle's base init (the patterns' slot 9 calls it first):
// position +8 .. +0x10 and velocity +0x18 .. +0x20 0, the colour +0x28 ..
// +0x34 and scale +0x38 1.0, age +0x3C and life +0x40 0.
inline void pfx_particle_init( std::uint32_t* w )
{
    for ( int o : { 0x08, 0x0C, 0x10, 0x18, 0x1C, 0x20, 0x3C, 0x40 } )
        w[ o / 4 ] = 0u;
    for ( int o = 0x28; o <= 0x38; o += 4 )
        w[ o / 4 ] = 0x3F800000u;
}

// Slot 7 of the tick processes that keep no state of their own (the dampen /
// linear acceleration / world scale z / scale / alpha in-out / alpha ticks:
// 0x58E340, 0x58E5C0, 0x5909D0, 0x590C30, 0x590E90, 0x5911F0): the instance's
// vtable only.
inline void pfx_create_vtable_only( std::uint32_t* w, std::uint32_t vtable )
{
    w[ 0 ] = vtable;
}

// Slot 7 of the fountains (0x55DAA0 quad, 0x55E2D0 geom, 0x55E970 directional
// quad; 0x4CD490 the boat thrust's adds +0x28 = 0): alive +0x14 and the
// accumulator +0x18 0, the count / rate / alpha scales +0x1C .. +0x24 1.0, then
// the vtable (world/pfx.hxx pfx_fountain_node_t's defaults).
inline void pfx_create_fountain( std::uint32_t* w, std::uint32_t vtable, bool boat_thrust )
{
    w[ 0x14 / 4 ] = 0u;
    w[ 0x18 / 4 ] = 0u;
    w[ 0x1C / 4 ] = 0x3F800000u;
    w[ 0x20 / 4 ] = 0x3F800000u;
    w[ 0x24 / 4 ] = 0x3F800000u;
    w[ 0 ] = vtable;
    if ( boat_thrust )
        w[ 0x28 / 4 ] = 0u;   // the boat
}

// The four create kinds that remain, each by its address.
inline void pfx_create_class( std::uint32_t address, std::uint32_t* w )
{
    switch ( address )
    {
    case 0x4CD6B0:   // VuPfxBoatSpringConstraint: vtable, +0x14 the boat 0
        w[ 0 ] = 0x746AD4u;
        w[ 0x14 / 4 ] = 0u;
        break;
    case 0x58DE80:   // VuPfxSpringConstraint: vtable, the offset +0x14 .. +0x1C 0
        w[ 0 ] = 0x750128u;
        w[ 0x14 / 4 ] = w[ 0x18 / 4 ] = w[ 0x1C / 4 ] = 0u;
        break;
    case 0x4CDF30:   // VuPfxEmitBoatHull: vtable, +0x14 the boat and +0x1C the accumulator 0 (+0x18 the fluids untouched)
        w[ 0 ] = 0x746B90u;
        w[ 0x14 / 4 ] = 0u;
        w[ 0x1C / 4 ] = 0u;
        break;
    case 0x58E9A0:   // VuPfxTrailPattern: the base ctor, vtable, +0x4C .. +0x54 0
        pfx_pattern_instance_ctor( w );
        w[ 0 ] = 0x7502C0u;
        w[ 0x4C / 4 ] = w[ 0x50 / 4 ] = w[ 0x54 / 4 ] = 0u;
        break;
    case 0x58FAA0:   // VuPfxGeomPattern
        pfx_pattern_instance_ctor( w );
        w[ 0 ] = 0x750354u;
        break;
    case 0x591470:   // VuPfxQuadPattern
        pfx_pattern_instance_ctor( w );
        w[ 0 ] = 0x7505D8u;
        break;
    case 0x55DAA0: pfx_create_fountain( w, 0x746AE0u, false ); break;
    case 0x55E2D0: pfx_create_fountain( w, 0x74EF1Cu, false ); break;
    case 0x55E970: pfx_create_fountain( w, 0x74EEE8u, false ); break;
    case 0x4CD490: pfx_create_fountain( w, 0x746AF0u, true ); break;
    case 0x58E340: pfx_create_vtable_only( w, 0x7501B0u ); break;
    case 0x58E5C0: pfx_create_vtable_only( w, 0x750240u ); break;
    case 0x5909D0: pfx_create_vtable_only( w, 0x7503E8u ); break;
    case 0x590C30: pfx_create_vtable_only( w, 0x75043Cu ); break;
    case 0x590E90: pfx_create_vtable_only( w, 0x7504BCu ); break;
    case 0x5911F0: pfx_create_vtable_only( w, 0x750558u ); break;
    default: break;
    }
}

// The patterns' slot 9 (thiscall pattern; particle*; retn 4): the base init,
// then the pattern's own words.
inline void pfx_particle_class( std::uint32_t address, std::uint32_t* w )
{
    pfx_particle_init( w );
    switch ( address )
    {
    case 0x58E9D0:   // VuPfxTrailPattern
        w[ 0x54 / 4 ] = 0u;
        break;
    case 0x58FAD0:   // VuPfxGeomPattern: the angles +0x44 .. +0x4C and angular velocity +0x54 .. +0x5C 0
        for ( int o : { 0x44, 0x48, 0x4C, 0x54, 0x58, 0x5C } )
            w[ o / 4 ] = 0u;
        break;
    case 0x5914A0:   // VuPfxQuadPattern: +0x44 rotation, +0x48 spin 0, +0x4C 1.0, +0x50 .. +0x58 0
        w[ 0x44 / 4 ] = 0u;
        w[ 0x48 / 4 ] = 0u;
        w[ 0x4C / 4 ] = 0x3F800000u;
        w[ 0x50 / 4 ] = w[ 0x54 / 4 ] = w[ 0x58 / 4 ] = 0u;
        break;
    default: break;
    }
}

// The instances' slot 0 (reset):
//   0x55D020 the fountains' (quad, directional quad, geom, boat thrust): alive
//            +0x14 and the accumulator +0x18 0;
//   0x4CE750 the boat hull's: the accumulator +0x1C 0;
//   0x58FE80 the quad / geom patterns': slot 0 on every child node instance
//            (the +0x14 list, next at +4: Vu virtuals - `child( user, k )`);
//   0x58F080 the trail pattern's: the same walk, then +0x4C .. +0x54 0.
inline void pfx_fountain_reset( std::uint32_t* w )
{
    w[ 0x14 / 4 ] = 0u;
    w[ 0x18 / 4 ] = 0u;
}

inline void pfx_hull_reset( std::uint32_t* w )
{
    w[ 0x1C / 4 ] = 0u;
}

inline void pfx_pattern_reset( std::uint32_t* w, bool trail, int children, void ( *child )( void* user, int k ), void* user )
{
    for ( int k = 0; k < children; ++k )
        child( user, k );
    if ( trail )
        w[ 0x4C / 4 ] = w[ 0x50 / 4 ] = w[ 0x54 / 4 ] = 0u;
}

// One node class: its vtable, slot 1 (the address and the descriptor it
// returns, the descriptor chain's names), slots 6 / 7 / 8 / 9 (0 when the slot
// is _purecall 0x6310B4 or past the vtable's end) and the instance's slot 0.
struct pfx_class_t
{
    const char* name;
    std::uint32_t vtable;
    std::uint32_t get_type;
    std::uint32_t descriptor;
    const char* chain;
    std::uint32_t size_slot;
    std::uint32_t instance_size;
    std::uint32_t create;
    std::uint32_t particle_slot;
    std::uint32_t particle_size;
    std::uint32_t particle;
    std::uint32_t reset;   // the instance vtable's slot 0 (0: a nullsub or none)
};

inline const pfx_class_t g_pfx_classes[] = {
    { "VuPfxNode", 0x746A70u, 0x4CD380u, 0x7369CCu, "VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxProcess", 0x746B00u, 0x4CD390u, 0x7369D4u, "VuPfxProcess|VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxGroup", 0x74CD14u, 0x51C400u, 0x736A08u, "VuPfxGroup|VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxSystem", 0x74FFD4u, 0x58A610u, 0x736A10u, "VuPfxSystem|VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxPattern", 0x750088u, 0x58AFE0u, 0x736A18u, "VuPfxPattern|VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxEmitFountain", 0x74EF50u, 0x55C8A0u, 0x7369BCu, "VuPfxEmitFountain|VuPfxProcess|VuPfxNode", 0, 0, 0, 0, 0, 0, 0 },
    { "VuPfxEmitQuadFountain", 0x74EF2Cu, 0x55C8B0u, 0x7369B4u, "VuPfxEmitQuadFountain|VuPfxEmitFountain|VuPfxProcess|VuPfxNode",
        0x55DA90u, 0x28u, 0x55DAA0u, 0, 0, 0, 0x55D020u },
    { "VuPfxEmitGeomFountain", 0x74EEF8u, 0x55C8C0u, 0x7369A4u, "VuPfxEmitGeomFountain|VuPfxEmitFountain|VuPfxProcess|VuPfxNode",
        0x55DA90u, 0x28u, 0x55E2D0u, 0, 0, 0, 0x55D020u },
    { "VuPfxEmitDirectionalQuadFountain", 0x74EEC4u, 0x55C8D0u, 0x7369ACu,
        "VuPfxEmitDirectionalQuadFountain|VuPfxEmitQuadFountain|VuPfxEmitFountain|VuPfxProcess|VuPfxNode", 0x55DA90u, 0x28u, 0x55E970u, 0,
        0, 0, 0x55D020u },
    { "VuPfxEmitBoatThrustQuadFountain", 0x746AB0u, 0x4CD3E0u, 0x7367D0u,
        "VuPfxEmitBoatThrustQuadFountain|VuPfxEmitQuadFountain|VuPfxEmitFountain|VuPfxProcess|VuPfxNode", 0x4CD480u, 0x2Cu, 0x4CD490u, 0,
        0, 0, 0x55D020u },
    { "VuPfxEmitBoatHull", 0x746B6Cu, 0x4CDE90u, 0x7367D8u, "VuPfxEmitBoatHull|VuPfxProcess|VuPfxNode", 0x4CDF20u, 0x20u, 0x4CDF30u, 0, 0, 0,
        0x4CE750u },
    { "VuPfxBoatSpringConstraint", 0x746A8Cu, 0x4CD3F0u, 0x7367C8u, "VuPfxBoatSpringConstraint|VuPfxProcess|VuPfxNode", 0x4CD6A0u,
        0x18u, 0x4CD6B0u, 0, 0, 0, 0 },
    { "VuPfxSpringConstraint", 0x750104u, 0x58DDE0u, 0x736A20u, "VuPfxSpringConstraint|VuPfxProcess|VuPfxNode", 0x58DE70u, 0x24u,
        0x58DE80u, 0, 0, 0, 0 },
    { "VuPfxTickDampenVelocity", 0x75018Cu, 0x58E2A0u, 0x736A28u, "VuPfxTickDampenVelocity|VuPfxProcess|VuPfxNode", 0x58E330u, 0x14u,
        0x58E340u, 0, 0, 0, 0 },
    { "VuPfxTickLinearAcceleration", 0x75021Cu, 0x58E530u, 0x736A30u, "VuPfxTickLinearAcceleration|VuPfxProcess|VuPfxNode", 0x58E330u,
        0x14u, 0x58E5C0u, 0, 0, 0, 0 },
    { "VuPfxTickWorldScaleZ", 0x7503C4u, 0x590940u, 0x736A48u, "VuPfxTickWorldScaleZ|VuPfxProcess|VuPfxNode", 0x58E330u, 0x14u,
        0x5909D0u, 0, 0, 0, 0 },
    { "VuPfxTickScale", 0x750448u, 0x590BA0u, 0x736A50u, "VuPfxTickScale|VuPfxProcess|VuPfxNode", 0x58E330u, 0x14u, 0x590C30u, 0, 0, 0, 0 },
    { "VuPfxTickAlphaInOut", 0x7504C8u, 0x590E00u, 0x736A58u, "VuPfxTickAlphaInOut|VuPfxProcess|VuPfxNode", 0x58E330u, 0x14u, 0x590E90u,
        0, 0, 0, 0 },
    { "VuPfxTickAlpha", 0x750534u, 0x591160u, 0x736A60u, "VuPfxTickAlpha|VuPfxProcess|VuPfxNode", 0x58E330u, 0x14u, 0x5911F0u, 0, 0, 0, 0 },
    { "VuPfxTrailPattern", 0x750294u, 0x58E890u, 0x736A38u, "VuPfxTrailPattern|VuPfxPattern|VuPfxNode", 0x58E990u, 0x58u, 0x58E9A0u,
        0x58E990u, 0x58u, 0x58E9D0u, 0x58F080u },
    { "VuPfxGeomPattern", 0x750328u, 0x58F9F0u, 0x736A40u, "VuPfxGeomPattern|VuPfxPattern|VuPfxNode", 0x58FA90u, 0x4Cu, 0x58FAA0u,
        0x58FAC0u, 0x64u, 0x58FAD0u, 0x58FE80u },
    { "VuPfxQuadPattern", 0x7505ACu, 0x5913C0u, 0x736A68u, "VuPfxQuadPattern|VuPfxPattern|VuPfxNode", 0x58FA90u, 0x4Cu, 0x591470u,
        0x591490u, 0x5Cu, 0x5914A0u, 0x58FE80u },
};

// VuPfxSystemInstance (vtable 0x74FF8C, 17 slots) on its words: +0x0C the
// VuPfxSystem's params (+0x30 Duration), +0x10 the pattern instance list (next
// at +4), +0x1C the matrix (16 floats; its translation +0x4C .. +0x54), +0x5C
// four words (slot 3), +0x6C the rotation (4), +0x7C the state, +0x80 the AABB,
// +0xA0 the live particle count, +0xA4 the clock, +0xA8 the instance scale,
// +0xAC four words (slot 5). Slots 11 / 12 / 13 (start / stop / tick) are
// world/pfx.hxx's; these are the rest.
inline void pfx_system_set_position( std::uint32_t* w, const std::uint32_t* p )   // slot 0 0x58ABF0
{
    w[ 0x4C / 4 ] = p[ 0 ];
    w[ 0x50 / 4 ] = p[ 1 ];
    w[ 0x54 / 4 ] = p[ 2 ];
}

// Slot 1 0x58AC10: the rotation (4 words) to +0x6C, the matrix rebuilt by
// VuMatrix_fromEuler 0x435640 (boat/math.hxx vu_matrix_from_euler) from it,
// the old translation +0x4C .. +0x54 put back (+0x58 the rebuilt matrix's).
inline void pfx_system_set_rotation( std::uint32_t* w, const std::uint32_t* r )
{
    std::uint32_t keep[ 3 ] = { w[ 0x4C / 4 ], w[ 0x50 / 4 ], w[ 0x54 / 4 ] };
    for ( int k = 0; k < 4; ++k )
        w[ 0x6C / 4 + k ] = r[ k ];
    float e[ 4 ], m[ 16 ];
    std::memcpy( e, w + 0x6C / 4, 16 );
    vu_matrix_from_euler( m, e );
    std::memcpy( w + 0x1C / 4, m, 64 );
    w[ 0x4C / 4 ] = keep[ 0 ];
    w[ 0x50 / 4 ] = keep[ 1 ];
    w[ 0x54 / 4 ] = keep[ 2 ];
}

// Slot 2 0x58AB00: the matrix (16 words) to +0x1C, the rotation +0x6C .. +0x74
// its euler - the same three as 0x534AA0's (world/surface.hxx
// water_world_euler, with its SUBSTITUTION of the CRT's atan2 / asinf) -, +0x78
// a stack word retail never writes (esp+0x1C): UNKNOWN, not written here.
inline void pfx_system_set_matrix( std::uint32_t* w, const std::uint32_t* m )
{
    std::memcpy( w + 0x1C / 4, m, 64 );
    float f[ 16 ], e[ 3 ];
    std::memcpy( f, m, 64 );
    water_world_euler( f, e );
    std::memcpy( w + 0x6C / 4, e, 12 );
}

inline void pfx_system_set_words5c( std::uint32_t* w, const std::uint32_t* v )   // slot 3 0x58A650: 4 words to +0x5C
{
    std::memcpy( w + 0x5C / 4, v, 16 );
}

inline void pfx_system_set_scale( std::uint32_t* w, std::uint32_t s )   // slot 4 0x58A670: +0xA8
{
    w[ 0xA8 / 4 ] = s;
}

inline void pfx_system_set_wordsac( std::uint32_t* w, const std::uint32_t* v )   // slot 5 0x58A690: 4 words to +0xAC
{
    std::memcpy( w + 0xAC / 4, v, 16 );
}

// The getters: slot 6 0x58A6C0 the state +0x7C; slot 7 0x58A6D0 the AABB's
// address (this + 0x80); slot 8 0x58A6E0 the count +0xA0; slot 9 0x58A6F0 the
// clock +0xA4 (fld); slot 10 0x58A700 the system's Duration (params +0x30,
// fld); slot 16 0x401DA0 this itself.
inline std::uint32_t pfx_system_state( const std::uint32_t* w ) { return w[ 0x7C / 4 ]; }
inline std::uint32_t pfx_system_aabb_offset( ) { return 0x80u; }
inline std::uint32_t pfx_system_count( const std::uint32_t* w ) { return w[ 0xA0 / 4 ]; }
inline std::uint32_t pfx_system_clock( const std::uint32_t* w ) { return w[ 0xA4 / 4 ]; }
inline std::uint32_t pfx_system_duration( const std::uint32_t* params ) { return params[ 0x30 / 4 ]; }

// Slots 14 / 15 0x58AF60 / 0x58AFA0 (thiscall; one argument; retn 4): when the
// byte g_vupfximpl (0x75904C) +0x1C is set, every pattern instance's slot 2
// (14) or slot 3 (15) with the argument - Vu virtuals: `pass( user, k )`.
inline void pfx_system_draw( bool enabled, int patterns, void ( *pass )( void* user, int k ), void* user )
{
    if ( !enabled )
        return;
    for ( int k = 0; k < patterns; ++k )
        pass( user, k );
}

// THE FOUNTAIN INSTANCES' SLOT 2 - the particle init VuPfxEmitFountain_emit
// 0x55D030 runs on every particle it fills (pfx_fountain_hooks_t::init). Each
// draw is the global VuRand's VuRandom_draw 0x50FD20 (world/pfx.hxx pfx_rand_t:
// the same Park-Miller / Bays-Durham body the emit inlines), each pair blended
// as (1 - r) * params[ lo ] + params[ hi ] * r. `params` is the node's +0x10.
inline float pfx_init_blend( pfx_rand_t& rand, const std::uint8_t* params, int lo, int hi )
{
    float a, b;
    std::memcpy( &a, params + lo, 4 );
    std::memcpy( &b, params + hi, 4 );
    const float r = rand.next( );
    return ( ( 1.0f - r ) * a ) + ( b * r );
}

// 0x55E100 VuPfxEmitQuadFountainInstance (and the boat thrust fountain's) slot 2:
// six draws in order, +0x44 .. +0x58 from the pairs (+0x98, +0x9C) .. (+0xC0, +0xC4).
inline void pfx_quad_fountain_init( pfx_rand_t& rand, const std::uint8_t* params, pfx_quad_particle_t& p )
{
    for ( int k = 0; k < 6; ++k )
        p.set( 0x44 + 4 * k, pfx_init_blend( rand, params, 0x98 + 8 * k, 0x9C + 8 * k ) );
}

// 0x55E9B0 VuPfxEmitDirectionalQuadFountainInstance's slot 2: 0x55E100 first;
// then, the system instance s = node +0xC -> +0xC and its velocity v = s +0x5C,
// dv = particle velocity (+0x18) - v BEFORE the matrix is fetched (the pattern's
// slot 5: `matrix`), dp = particle position (+0x08) - the matrix translation
// (+0x30 .. +0x38); velocity = v + (dp * (1 / |dp|)) * |dv| with |dv| =
// sqrt((dy*dy + dx*dx) + dz*dz) and |dp| likewise (VuMath_sqrt 0x40B5A0 =
// sqrtss). The fourth word +0x24 takes a stack word retail never writes (the
// movq pair at 0x55EB06): UNKNOWN, not written here.
inline void pfx_directional_fountain_init( pfx_rand_t& rand, const std::uint8_t* params, const float* system_velocity,
    const float* matrix, pfx_quad_particle_t& p )
{
    pfx_quad_fountain_init( rand, params, p );
    const float dvx = p.f( 0x18 ) - system_velocity[ 0 ];
    const float dvy = p.f( 0x1C ) - system_velocity[ 1 ];
    const float dvz = p.f( 0x20 ) - system_velocity[ 2 ];
    const float dpx = p.f( 0x08 ) - matrix[ 12 ];
    const float dpy = p.f( 0x0C ) - matrix[ 13 ];
    const float dpz = p.f( 0x10 ) - matrix[ 14 ];
    const float speed = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( ( dvy * dvy ) + ( dvx * dvx ) ) + ( dvz * dvz ) ) ) );
    const float d = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( ( dpy * dpy ) + ( dpx * dpx ) ) + ( dpz * dpz ) ) ) );
    const float inv = 1.0f / d;
    p.set( 0x18, ( ( inv * dpx ) * speed ) + system_velocity[ 0 ] );
    p.set( 0x1C, system_velocity[ 1 ] + ( ( inv * dpy ) * speed ) );
    p.set( 0x20, system_velocity[ 2 ] + ( ( inv * dpz ) * speed ) );
}

// 0x55E640 VuPfxEmitGeomFountainInstance's slot 2: the angles +0x44 .. +0x4C
// from (+0x98, +0xA8), (+0x9C, +0xAC), (+0xA0, +0xB0), the angular velocity
// +0x54 .. +0x5C from (+0xB8, +0xC8), (+0xBC, +0xCC), (+0xC0, +0xD0); then the
// pattern's slot 5 matrix m, M = identity (0x410A40) x fromEuler( angles )
// (0x435640) x m (VuMatrix_multiply 0x4078C0 twice: boat/hull.hxx
// hull_multiply), and the angles become M's euler - 0x534AA0's three (world/
// surface.hxx water_world_euler, its CRT atan2 / asinf SUBSTITUTION). +0x50
// takes a stack word retail never writes (the movq at 0x55E8D0): UNKNOWN, not
// written here.
inline void pfx_geom_fountain_init( pfx_rand_t& rand, const std::uint8_t* params, const float* matrix, pfx_quad_particle_t& p )
{
    p.set( 0x44, pfx_init_blend( rand, params, 0x98, 0xA8 ) );
    p.set( 0x48, pfx_init_blend( rand, params, 0x9C, 0xAC ) );
    p.set( 0x4C, pfx_init_blend( rand, params, 0xA0, 0xB0 ) );
    p.set( 0x54, pfx_init_blend( rand, params, 0xB8, 0xC8 ) );
    p.set( 0x58, pfx_init_blend( rand, params, 0xBC, 0xCC ) );
    p.set( 0x5C, pfx_init_blend( rand, params, 0xC0, 0xD0 ) );
    static const float identity[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    float angles[ 4 ] = { p.f( 0x44 ), p.f( 0x48 ), p.f( 0x4C ), p.f( 0x50 ) };
    float e[ 16 ], a[ 16 ], b[ 16 ], euler[ 3 ];
    vu_matrix_from_euler( e, angles );
    hull_multiply( identity, a, e );
    hull_multiply( a, b, matrix );
    water_world_euler( b, euler );
    p.set( 0x44, euler[ 0 ] );
    p.set( 0x48, euler[ 1 ] );
    p.set( 0x4C, euler[ 2 ] );
}

// THE PATTERN INSTANCES' DRAW SLOTS, CPU side. VuPfxSystemInstance's slots 14 /
// 15 run every pattern's slot 2 / 3 with the pass argument (pfx_system_draw).
//   quad 0x5920B0 / trail 0x58F9C0 (slot 2; slot 3 nullsub 0x4021A0): when the
//     pattern's count +0x28 is not 0, g_vupfximpl (0x75904C) +0xC (quad) / +0x10
//     (trail) renderer's 0x58BD60 / 0x58D030 ( argument +0x10, pattern ) - the
//     renderers build GPU batches: a hook (`pfx_pattern_submits`).
//   geom 0x5903A0 (slot 2) / 0x5906A0 (slot 3): per particle, the list +0x20 in
//     order (next at +0), a model matrix - pfx_geom_draw_matrix - and for slot 2
//     its colour; the model instance at params +0x5C draws it (0x520130 / per
//     mesh 0x520670: hooks).
inline bool pfx_pattern_submits( std::uint32_t count )
{
    return count != 0u;
}

// The model matrix of one geom particle q: D = params Space (+0x34) == 1 ? the
// system matrix (+0x1C) : the static identity 0x7593B0; T = 0x410AB0's
// translation of q's position (+0x08); E = fromEuler( q's angles +0x44 );
// S = diag( s, s, s, 1 ), s = q's scale (+0x38) x the instance scale (+0xA8);
// matrix = S x ( E x ( T x D ) ) - three VuMatrix_multiply 0x4078C0, in that
// order (boat/hull.hxx hull_multiply, a = the left).
inline void pfx_geom_draw_matrix( const pfx_quad_particle_t& q, const float* draw_matrix, float instance_scale, float* out )
{
    const float position[ 3 ] = { q.f( 0x08 ), q.f( 0x0C ), q.f( 0x10 ) };
    const float angles[ 3 ] = { q.f( 0x44 ), q.f( 0x48 ), q.f( 0x4C ) };
    float t[ 16 ], a[ 16 ], e[ 16 ], b[ 16 ];
    hull_translation( t, position );
    hull_multiply( t, a, draw_matrix );
    vu_matrix_from_euler( e, angles );
    hull_multiply( e, b, a );
    const float s = q.f( 0x38 ) * instance_scale;
    const float scale[ 16 ] = { s, 0, 0, 0, 0, s, 0, 0, 0, 0, s, 0, 0, 0, 0, 1 };
    hull_multiply( scale, out, b );
}

// Slot 2's colour: per channel c the system tint (+0xAC .. +0xB8) x the
// particle's colour (+0x28 .. +0x34) x 255.0 (0x6F7808), fld / fistp (round to
// nearest even; out of range -> 0x80000000) and its low byte; r g b a from the
// low byte up. Slot 2 stores it at params +0x74 and params +0x58's byte at
// +0x78 (the model instance's colour / flag) before each draw.
inline std::uint32_t pfx_geom_draw_colour( const pfx_quad_particle_t& q, const float* tint )
{
    std::uint32_t c = 0u;
    for ( int k = 0; k < 4; ++k )
    {
        const float v = ( tint[ k ] * q.f( 0x28 + 4 * k ) ) * 255.0f;
        const std::int32_t i = _mm_cvtsd_si32( _mm_set_sd( static_cast< double >( v ) ) );
        c |= ( static_cast< std::uint32_t >( i ) & 0xFFu ) << ( 8 * k );
    }
    return c;
}

inline const pfx_class_t* pfx_class_find( const char* name )
{
    for ( const pfx_class_t& c : g_pfx_classes )
    {
        const char* a = c.name;
        const char* b = name;
        while ( *a && *a == *b )
            ++a, ++b;
        if ( *a == 0 && *b == 0 )
            return &c;
    }
    return nullptr;
}
