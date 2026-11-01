#pragma once

#include <workspace/core/entity/node.hxx>
#include <workspace/core/entity/property.hxx>
#include <workspace/core/entity/values.hxx>
#include <workspace/core/world/transform.hxx>
#include <workspace/core/world/tree.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <xmmintrin.h>

// VuWater AS A LEVEL BUILDS IT: the water surfaces and the waves over them,
// made by the water entities' slot 6 (onGameInitialize) in VuEntity::
// gameInitialize 0x500B10's order - an entity's children (+0x38, in order)
// before the entity itself (then +0x14 |= 4). 0x500B10's other steps, the
// entity repository registration (vt+0x20) and each component's vt+0x24, are
// hooks.
//
//   VuWaterSurfaceEntity 0x5DB510: descriptor 0x5DBA90, VuWater_createSurface
//     0x5284C0 -> the surface ctor 0x5359E0 / 0x535C50, the surface-tree add
//     0x529860 (which also tests every wave already held); the draw
//     registration (g_vu3ddrawmanager vt+0x28) and VuWater's listeners
//     (+0x34) are hooks.
//   VuWaterRampEntity 0x5D6F80: the same surface, then its ramp wave through
//     0x528D70 (VuWaterRampWave: base ctor 0x5A6390(1), 0x5A2570, bounds
//     0x5A28F0) and the wave add 0x529A90.
//   the six wave entities whose creation wave.hxx recreates (rectangular /
//     circular ocean, banked turn, whirlpool, directional, bump): their
//     allocator's pool node on VuWater's wave list (+0x30, PUSHED AT THE HEAD)
//     and 0x529A90.
// Any other water class an entity tree holds is counted, not built.
//
// The surface entity's water-shader lookup on its post-load (0x5DB800 ->
// 0x528610) is the render side and not modelled.

// ---------------------------------------------------------------------------
// THE ENTITIES' PROPERTIES. Each water class's targets as words from entity
// +0x60, loaded by the verified VuBasicProperty loads (engine
// entity/property.hxx) with the constructor's defaults (schema.hxx, captured
// from the ctors under emulation; the ramp's three from its ctor 0x5D6D50).
enum class water_prop_t : std::uint8_t
{
    real, angle, percent, int_enum
};

struct water_prop_row_t
{
    const char* name;
    std::uint32_t offset;
    water_prop_t kind;
    std::uint32_t fallback;
    const property_choice_t* choices;
};

// The choice tables, read out of the image: 0x736010 (X Size / Y Size) and
// 0x735FE0 (Complexity).
inline const property_choice_t k_water_sizes[] = { { "1", 1 }, { "2", 2 }, { "4", 4 }, { "8", 8 }, { "16", 16 }, { "32", 32 },
    { "64", 64 }, { "128", 128 }, { "256", 256 }, { "512", 512 }, { "1024", 1024 }, { "2048", 2048 }, { "4096", 4096 },
    { "8192", 8192 }, { nullptr, 0 } };
inline const property_choice_t k_water_complexity[] = { { "4", 4 }, { "5", 5 }, { "6", 6 }, { "7", 7 }, { "8", 8 }, { nullptr, 0 } };

// VuWaterSurfaceEntity's ctor 0x5DA4F0 (the ones its creation reads). Its
// ProceduralReflection (+0xEC, a bool the descriptor copies) and WaterMap
// (+0x8C, through the node at +0x130 whose +0x4C the descriptor copies - an
// asset) reach only surface words nothing here reads: UNKNOWN, not loaded.
inline const water_prop_row_t k_water_surface_props[] = {
    { "X Size", 0x70, water_prop_t::int_enum, 256, k_water_sizes },
    { "Y Size", 0x74, water_prop_t::int_enum, 256, k_water_sizes },
    { "Max Wave Depth", 0x78, water_prop_t::real, 0x40A00000u, nullptr },
    { "Max Wave Height", 0x7C, water_prop_t::real, 0x40A00000u, nullptr },
    { "Reflection Height", 0x80, water_prop_t::real, 0x40A00000u, nullptr },
    { "Reflection Offset", 0x84, water_prop_t::real, 0, nullptr },
    { nullptr, 0, water_prop_t::real, 0, nullptr } };

// VuWaterRampEntity's ctor 0x5D6D50 adds three: Z Size (1.0), Transition
// Ratio % (a VuPercentageProperty, 0.5) and Flow Speed (10.0).
inline const water_prop_row_t k_water_ramp_props[] = {
    { "Z Size", 0x134, water_prop_t::real, 0x3F800000u, nullptr },
    { "Transition Ratio %", 0x138, water_prop_t::percent, 0x3F000000u, nullptr },
    { "Flow Speed", 0x13C, water_prop_t::real, 0x41200000u, nullptr },
    { nullptr, 0, water_prop_t::real, 0, nullptr } };

inline const water_prop_row_t k_water_ocean_props[] = {
    { "Complexity", 0x68, water_prop_t::int_enum, 5, k_water_complexity },
    { "Bin Siz", 0x6C, water_prop_t::real, 0x42C80000u, nullptr },
    { "Wave Direction", 0x70, water_prop_t::angle, 0, nullptr },
    { "Gravity", 0x74, water_prop_t::real, 0x411CE560u, nullptr },
    { "Wind Speed", 0x78, water_prop_t::real, 0x40A00000u, nullptr },
    { "Directional Power", 0x7C, water_prop_t::real, 0x40400000u, nullptr },
    { "Suppression Wave Length", 0x80, water_prop_t::real, 0x3F000000u, nullptr },
    { "Height Multiplier", 0x84, water_prop_t::real, 0x3DCCCCCDu, nullptr },
    { nullptr, 0, water_prop_t::real, 0, nullptr } };

inline const water_prop_row_t k_water_rectangular_props[] = {
    { "Decay Ratio X %", 0x8C, water_prop_t::percent, 0x3F666666u, nullptr },
    { "Decay Ratio Y %", 0x90, water_prop_t::percent, 0x3F666666u, nullptr },
    { nullptr, 0, water_prop_t::real, 0, nullptr } };

inline const water_prop_row_t k_water_circular_props[] = {
    { "Radius", 0x8C, water_prop_t::real, 0x42C80000u, nullptr },
    { "Decay Ratio %", 0x90, water_prop_t::percent, 0x3F666666u, nullptr },
    { nullptr, 0, water_prop_t::real, 0, nullptr } };

inline const water_prop_row_t k_water_banked_props[] = {
    { "Outer Radius", 0x60, water_prop_t::real, 0x41A00000u, nullptr },
    { "Inner Radius", 0x64, water_prop_t::real, 0x41200000u, nullptr },
    { "Height", 0x68, water_prop_t::real, 0x40000000u, nullptr },
    { "Angular Size", 0x6C, water_prop_t::angle, 0x40C90FDBu, nullptr },
    { "Angular Decay Ratio", 0x70, water_prop_t::real, 0x3F000000u, nullptr },
    { nullptr, 0, water_prop_t::real, 0, nullptr } };

inline const water_prop_row_t k_water_whirlpool_props[] = {
    { "Outer Radius", 0x60, water_prop_t::real, 0x41A00000u, nullptr },
    { "Inner Radius", 0x64, water_prop_t::real, 0x40A00000u, nullptr },
    { "Depth", 0x68, water_prop_t::real, 0x41200000u, nullptr },
    { "Angular Speed", 0x6C, water_prop_t::angle, 0, nullptr },
    { "Linear Speed", 0x70, water_prop_t::real, 0, nullptr },
    { "Foaminess", 0x74, water_prop_t::real, 0x3F800000u, nullptr },
    { nullptr, 0, water_prop_t::real, 0, nullptr } };

inline const water_prop_row_t k_water_directional_props[] = {
    { "Max Height", 0x6C, water_prop_t::real, 0x3F800000u, nullptr },
    { "Speed", 0x70, water_prop_t::real, 0x3F800000u, nullptr },
    { "Period", 0x74, water_prop_t::real, 0x3F800000u, nullptr },
    { "Longitudinal Decay Ratio %", 0x78, water_prop_t::percent, 0x3F000000u, nullptr },
    { "Lateral Decay Ratio %", 0x7C, water_prop_t::percent, 0x3F000000u, nullptr },
    { nullptr, 0, water_prop_t::real, 0, nullptr } };

inline const water_prop_row_t k_water_bump_props[] = {
    { "Max Height", 0x60, water_prop_t::real, 0x3F800000u, nullptr },
    { "Lateral Decay Ratio %", 0x64, water_prop_t::percent, 0x3F000000u, nullptr },
    { nullptr, 0, water_prop_t::real, 0, nullptr } };

// The classes built here, in the kind order the oracle case uses.
enum water_kind_t
{
    water_kind_surface, water_kind_ramp, water_kind_rectangular, water_kind_circular, water_kind_banked, water_kind_whirlpool,
    water_kind_directional, water_kind_bump, water_kind_none,
    water_kind_point   // made at run time (VuWater_createPointWave 0x528E10), not by an entity's slot 6
};

inline water_kind_t water_kind_of( const std::string& cls )
{
    static const char* const names[] = { "VuWaterSurfaceEntity", "VuWaterRampEntity", "VuRectangularOceanWaveEntity",
        "VuCircularOceanWaveEntity", "VuBankedTurnWaveEntity", "VuWhirlpoolWaveEntity", "VuDirectionalWaveEntity", "VuBumpWaveEntity" };
    for ( int k = 0; k < water_kind_none; ++k )
        if ( cls == names[ k ] )
            return static_cast< water_kind_t >( k );
    return water_kind_none;
}

// Words +0x60 .. +0x143 (57), the targets each loaded property fills; `known`
// marks the ones loaded.
constexpr int k_water_entity_words = 57;

struct water_entity_props_t
{
    std::uint32_t word[ k_water_entity_words ]{};
    bool known[ k_water_entity_words ]{};
};

struct water_entity_state_t
{
    water_kind_t kind = water_kind_none;
    water_entity_props_t value;
    water_entity_props_t defaults;
};

inline void water_props_rows( water_kind_t kind, const water_prop_row_t* rows[ 3 ] )
{
    rows[ 0 ] = rows[ 1 ] = rows[ 2 ] = nullptr;
    switch ( kind )
    {
    case water_kind_surface: rows[ 0 ] = k_water_surface_props; break;
    case water_kind_ramp: rows[ 0 ] = k_water_surface_props; rows[ 1 ] = k_water_ramp_props; break;
    case water_kind_rectangular: rows[ 0 ] = k_water_ocean_props; rows[ 1 ] = k_water_rectangular_props; break;
    case water_kind_circular: rows[ 0 ] = k_water_ocean_props; rows[ 1 ] = k_water_circular_props; break;
    case water_kind_banked: rows[ 0 ] = k_water_banked_props; break;
    case water_kind_whirlpool: rows[ 0 ] = k_water_whirlpool_props; break;
    case water_kind_directional: rows[ 0 ] = k_water_directional_props; break;
    case water_kind_bump: rows[ 0 ] = k_water_bump_props; break;
    default: break;
    }
}

// The state an entity of `cls` starts with (its ctor's defaults), or null for
// a class not built here.
inline std::shared_ptr< water_entity_state_t > water_entity_state_for( const std::string& cls )
{
    const water_kind_t kind = water_kind_of( cls );
    if ( kind == water_kind_none )
        return nullptr;
    auto s = std::make_shared< water_entity_state_t >( );
    s->kind = kind;
    const water_prop_row_t* rows[ 3 ];
    water_props_rows( kind, rows );
    for ( const water_prop_row_t* table : rows )
        for ( const water_prop_row_t* r = table; r && r->name; ++r )
        {
            const int k = static_cast< int >( r->offset - 0x60 ) / 4;
            s->value.word[ k ] = r->fallback;
            s->value.known[ k ] = true;
        }
    s->defaults = s->value;
    return s;
}

// VuEntity::load's "Properties" step for these classes: each node's load in
// turn (reset to its default, then the value read).
inline void water_entity_load( water_entity_state_t& s, const value_t& properties )
{
    const water_prop_row_t* rows[ 3 ];
    water_props_rows( s.kind, rows );
    for ( const water_prop_row_t* table : rows )
        for ( const water_prop_row_t* r = table; r && r->name; ++r )
        {
            const int k = static_cast< int >( r->offset - 0x60 ) / 4;
            std::uint32_t& word = s.value.word[ k ];
            const std::uint32_t fallback = s.defaults.word[ k ];
            if ( r->kind == water_prop_t::int_enum )
            {
                std::int32_t v;
                int count = 0;
                while ( r->choices[ count ].m_name )
                    ++count;
                property_enum( properties, r->name, v, static_cast< std::int32_t >( fallback ), r->choices, count );
                word = static_cast< std::uint32_t >( v );
                continue;
            }
            float fb;
            std::memcpy( &fb, &fallback, 4 );
            float v;
            if ( r->kind == water_prop_t::percent )
                property_percentage( properties, r->name, v, fb );
            else
                property_float( properties, r->name, v, fb, r->kind == water_prop_t::angle );
            std::memcpy( &word, &v, 4 );
        }
}

// ---------------------------------------------------------------------------
// THE TRANSFORM WORDS the creations read: VuTransformComponent +0x10 .. +0xBF -
// the local (+0x10), the world (+0x50), the rotation (+0x90), the world euler
// (+0xA0), the scale (+0xB0). Each vector's fourth lane is the stack word the
// property parse left (transform.hxx): UNKNOWN, held 0.
constexpr int k_water_transform_words = 44;

// 0x534AA0's world euler WITH a parent (transform.hxx leaves it unknown):
//   x = (float) atan2( (double) world[ 6 ], (double) world[ 10 ] )
//   y = asinf( maxss( -1.0, minss( 1.0, -world[ 2 ] ) ) )
//   z = (float) atan2( (double) world[ 1 ], (double) world[ 0 ] )
// SUBSTITUTION: __libm_sse2_atan2 / __libm_sse2_asinf are the CRT DLL's; these
// are the host CRT's atan2 / asinf (the oracle's emulation measures the same
// host functions, so a check against it cannot see a difference between them).
inline void water_world_euler( const float* world, float* euler )
{
    const float lo = _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( 1.0f ), _mm_set_ss( -world[ 2 ] ) ) );
    const float s = _mm_cvtss_f32( _mm_max_ss( _mm_set_ss( -1.0f ), _mm_set_ss( lo ) ) );
    euler[ 0 ] = static_cast< float >( std::atan2( static_cast< double >( world[ 6 ] ), static_cast< double >( world[ 10 ] ) ) );
    euler[ 1 ] = std::asin( s );
    euler[ 2 ] = static_cast< float >( std::atan2( static_cast< double >( world[ 1 ] ), static_cast< double >( world[ 0 ] ) ) );
}

inline void water_transform_words( const transform_component_t& c, std::uint32_t* out )
{
    std::memset( out, 0, 4 * k_water_transform_words );
    std::memcpy( out, c.local, 64 );
    std::memcpy( out + 16, c.world, 64 );
    std::memcpy( out + 32, c.rotation, 12 );
    float euler[ 3 ];
    if ( c.world_rotation_known )
        std::memcpy( euler, c.world_rotation, 12 );
    else
        water_world_euler( c.world, euler );
    std::memcpy( out + 36, euler, 12 );
    std::memcpy( out + 40, c.scale, 12 );
}

// ---------------------------------------------------------------------------
// An object's words by byte offset with the ones recreated marked.
struct water_words_t
{
    std::uint32_t word[ 80 ]{};
    bool known[ 80 ]{};
    void set( int offset, std::uint32_t v )
    {
        word[ offset / 4 ] = v;
        known[ offset / 4 ] = true;
    }
    void setf( int offset, float v )
    {
        std::uint32_t b;
        std::memcpy( &b, &v, 4 );
        set( offset, b );
    }
    float f( int offset ) const
    {
        float v;
        std::memcpy( &v, &word[ offset / 4 ], 4 );
        return v;
    }
};

// The two inputs every creation reads: entity words from +0x60 and the
// transform component's from +0x10.
struct water_input_t
{
    const std::uint32_t* entity = nullptr;
    const std::uint32_t* transform = nullptr;
    std::uint32_t e( int offset ) const { return entity[ ( offset - 0x60 ) / 4 ]; }
    std::uint32_t t( int offset ) const { return transform[ ( offset - 0x10 ) / 4 ]; }
    wave_source_t source( ) const
    {
        wave_source_t s{};
        s.entity = entity;
        s.transform = transform + ( 0x80 - 0x10 ) / 4;
        return s;
    }
};

// 0x41b2a0 (out, box*, matrix*): the box seeded with +-FLT_MAX, then 0x5175e0
// folds the eight corners (0x5177a0's order: x fastest, then y, then z) each
// through the matrix - ((m0 x + y m4) + m8 z) + m12, likewise y and z - into
// it, VuMath_min / max 0x403260 / 0x403280 with the corner first. The result's
// w lanes are stack words never written: not produced.
inline void water_box_transform( const float* lo, const float* hi, const float* m, float* out_lo, float* out_hi )
{
    for ( int k = 0; k < 3; ++k )
    {
        out_lo[ k ] = 3.4028235e38f;
        out_hi[ k ] = -3.4028235e38f;
    }
    for ( int c = 0; c < 8; ++c )
    {
        const float x = ( c & 1 ) ? hi[ 0 ] : lo[ 0 ];
        const float y = ( c & 2 ) ? hi[ 1 ] : lo[ 1 ];
        const float z = ( c & 4 ) ? hi[ 2 ] : lo[ 2 ];
        const float p[ 3 ] = { ( ( ( m[ 0 ] * x ) + ( y * m[ 4 ] ) ) + ( m[ 8 ] * z ) ) + m[ 12 ],
            ( ( ( m[ 1 ] * x ) + ( m[ 5 ] * y ) ) + ( z * m[ 9 ] ) ) + m[ 13 ],
            ( ( ( x * m[ 2 ] ) + ( y * m[ 6 ] ) ) + ( z * m[ 10 ] ) ) + m[ 14 ] };
        for ( int k = 0; k < 3; ++k )
        {
            out_lo[ k ] = water_min( p[ k ], out_lo[ k ] );
            out_hi[ k ] = water_max( p[ k ], out_hi[ k ] );
        }
    }
}

// VuWaterSurfaceEntity's descriptor 0x5DBA90 (56 bytes, memset first): the
// transform's world translation (+0x80, four words), its world euler z
// (+0xA8), log2 of X / Y Size as (int) of logf( (float) size ) * (1 / (float)
// log( 2.0 )) rounded by fistp (to nearest), Max Wave Depth / Height,
// Reflection Height / Offset, ProceduralReflection's byte (UNKNOWN here), the
// WaterMap node's +0x4C (an asset: UNKNOWN), and 1.
// SUBSTITUTION: logf / log are the host CRT's for __libm_sse2_logf / _log.
inline int water_log2( std::uint32_t size )
{
    const float inv = 1.0f / static_cast< float >( std::log( 2.0 ) );
    return static_cast< int >( std::nearbyint( std::log( static_cast< float >( static_cast< std::int32_t >( size ) ) ) * inv ) );
}

// The surface ctor 0x5359E0 then 0x535C50 on that descriptor - the words, by
// retail offset:
//   +0 vtable 0x74D378, +4 refcount 1, +8 0; +12 .. +67 the descriptor;
//   +68 the owner entity (a pointer: not produced); +72 / +76 1 << the log2s;
//   +80 identity . translation(+12) then rotZ(+28) . that (0x426b40);
//   +144 its inverse (0x4157c0); +208 .. +239 the box (-hx, -hy, -depth) ..
//   (hx, hy, height) through +80 (0x41b2a0); +240 / +244 hx / hy (each size
//   (float) * 0.5); +248 the surface-tree leaf, +252 the wave tree (the tree
//   itself is water_held_surface_t's), +256 = +60.
inline water_words_t water_surface_create( const water_input_t& in )
{
    water_words_t s{};
    std::uint32_t d[ 14 ]{};
    for ( int k = 0; k < 4; ++k )
        d[ k ] = in.t( 0x80 + 4 * k );
    d[ 4 ] = in.t( 0xA8 );
    d[ 5 ] = static_cast< std::uint32_t >( water_log2( in.e( 0x70 ) ) );
    d[ 6 ] = static_cast< std::uint32_t >( water_log2( in.e( 0x74 ) ) );
    d[ 7 ] = in.e( 0x78 );
    d[ 8 ] = in.e( 0x7C );
    d[ 9 ] = in.e( 0x80 );
    d[ 10 ] = in.e( 0x84 );
    d[ 13 ] = 1;
    s.set( 0, 0x74D378u );
    s.set( 4, 1 );
    s.set( 8, 0 );
    for ( int k = 0; k < 14; ++k )
        if ( k != 11 && k != 12 )
            s.set( 12 + 4 * k, d[ k ] );
    s.set( 72, 1u << d[ 5 ] );
    s.set( 76, 1u << d[ 6 ] );
    float m[ 16 ];
    float t[ 16 ];
    float o[ 16 ];
    wave_matrix_identity( m );
    const float at[ 3 ] = { s.f( 12 ), s.f( 16 ), s.f( 20 ) };
    hull_translation( t, at );
    hull_multiply( m, o, t );
    std::memcpy( m, o, 64 );
    wave_matrix_rotation_z( t, s.f( 28 ) );
    hull_multiply( t, o, m );
    std::memcpy( m, o, 64 );
    for ( int k = 0; k < 16; ++k )
        s.setf( 80 + 4 * k, m[ k ] );
    float inv[ 16 ];
    std::memcpy( inv, m, 64 );
    hull_invert( inv );
    for ( int k = 0; k < 16; ++k )
        s.setf( 144 + 4 * k, inv[ k ] );
    const float hx = static_cast< float >( static_cast< std::int32_t >( s.word[ 72 / 4 ] ) ) * 0.5f;
    const float hy = static_cast< float >( static_cast< std::int32_t >( s.word[ 76 / 4 ] ) ) * 0.5f;
    s.setf( 240, hx );
    s.setf( 244, hy );
    const float lo[ 3 ] = { -hx, -hy, -s.f( 40 ) };
    const float hi[ 3 ] = { hx, hy, s.f( 44 ) };
    float blo[ 3 ];
    float bhi[ 3 ];
    water_box_transform( lo, hi, m, blo, bhi );
    for ( int k = 0; k < 3; ++k )
    {
        s.setf( 208 + 4 * k, blo[ k ] );
        s.setf( 224 + 4 * k, bhi[ k ] );
    }
    return s;
}

// VuWaterRampEntity's ramp wave: the descriptor 0x5D6F80 packs (44 bytes,
// memset first) - the world translation, (float) X / Y Size, Z Size, 0, the
// world euler z, Transition Ratio %, Flow Speed - and 0x528D70 builds it:
// VuWaterWave's ctor 0x5A6390 with 1 (+0x44 = 1: tested by its centre), vtable
// 0x75093C, +76 .. +119 zeroed, then 0x5A2570 and 0x5A28F0. +4 is 2 (the
// ctor's 1 and the allocator's hold).
inline water_words_t ramp_wave_create( const water_input_t& in )
{
    water_words_t w{};
    std::uint32_t d[ 11 ]{};
    for ( int k = 0; k < 4; ++k )
        d[ k ] = in.t( 0x80 + 4 * k );
    float x;
    x = static_cast< float >( static_cast< std::int32_t >( in.e( 0x70 ) ) );
    std::memcpy( &d[ 4 ], &x, 4 );
    x = static_cast< float >( static_cast< std::int32_t >( in.e( 0x74 ) ) );
    std::memcpy( &d[ 5 ], &x, 4 );
    d[ 6 ] = in.e( 0x134 );
    d[ 8 ] = in.t( 0xA8 );
    d[ 9 ] = in.e( 0x138 );
    d[ 10 ] = in.e( 0x13C );
    w.set( 0, 0x75093Cu );
    w.set( 4, 2 );
    w.set( 8, 0 );
    w.set( 56, 0 );
    w.set( 60, 0 );
    w.set( 64, 0 );
    w.set( 68, 1 );
    w.set( 72, 0x3F800000u );
    // 0x5A2570: the descriptor to +76 .. +116.
    for ( int k = 0; k < 11; ++k )
        w.set( 76 + 4 * k, d[ k ] );
    // The frame at +120: identity . fromScale( +92 / 2, +96 / 2, +100 / 2 ) .
    // rotZ( +108 ) . translation( +76 ); its inverse at +184.
    float m[ 16 ];
    float t[ 16 ];
    float o[ 16 ];
    wave_matrix_identity( m );
    const float scale[ 3 ] = { w.f( 92 ) * 0.5f, w.f( 96 ) * 0.5f, w.f( 100 ) * 0.5f };
    wave_matrix_scale( t, scale );
    hull_multiply( m, o, t );
    std::memcpy( m, o, 64 );
    wave_matrix_rotation_z( t, w.f( 108 ) );
    hull_multiply( m, o, t );
    std::memcpy( m, o, 64 );
    const float at[ 3 ] = { w.f( 76 ), w.f( 80 ), w.f( 84 ) };
    hull_translation( t, at );
    hull_multiply( m, o, t );
    std::memcpy( m, o, 64 );
    for ( int k = 0; k < 16; ++k )
        w.setf( 120 + 4 * k, m[ k ] );
    float inv[ 16 ];
    std::memcpy( inv, m, 64 );
    hull_invert( inv );
    for ( int k = 0; k < 16; ++k )
        w.setf( 184 + 4 * k, inv[ k ] );
    // The ramp's shape terms: +248 = 1 / ((2 - band) * band) when band (+112)
    // > 0, else 0; +252 = ((band * 2) * that) * (+100 * 0.5); +256 / +260 =
    // inverse +188 / +204 times +252.
    const float band = w.f( 112 );
    float curve = 0.0f;
    if ( _mm_comigt_ss( _mm_set_ss( band ), _mm_setzero_ps( ) ) )
        curve = 1.0f / ( ( 2.0f - band ) * band );
    const float half_z = w.f( 100 ) * 0.5f;
    w.setf( 248, curve );
    const float slope = ( ( band * 2.0f ) * curve ) * half_z;
    w.setf( 252, slope );
    const float sx = w.f( 188 ) * slope;
    const float sy = w.f( 204 ) * slope;
    w.setf( 256, sx );
    w.setf( 260, sy );
    // +264 .. +272: the frame's first row normalised (+276 a stack word: not
    // produced); +280 .. +288 its cross term with ( -sx, -sy ) times Flow Speed
    // (+292 likewise); +296 .. +304 the second row normalised times -Flow
    // Speed (+308 likewise).
    const float len = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( ( m[ 0 ] * m[ 0 ] ) + ( m[ 1 ] * m[ 1 ] ) ) + ( m[ 2 ] * m[ 2 ] ) ) ) );
    const float r = 1.0f / len;
    w.setf( 264, m[ 0 ] * r );
    w.setf( 268, m[ 1 ] * r );
    w.setf( 272, m[ 2 ] * r );
    const float v27 = -w.f( 272 );
    const float v28 = -sx;
    const float v29 = -w.f( 268 );
    const float v30 = -sy;
    const float v31 = -w.f( 264 );
    const float flow = w.f( 116 );
    w.setf( 280, ( ( v30 * v27 ) - v29 ) * flow );
    w.setf( 284, ( v31 - ( v28 * v27 ) ) * flow );
    w.setf( 288, ( ( v28 * v29 ) - ( v30 * v31 ) ) * flow );
    const float r2 = 1.0f / _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( ( w.f( 136 ) * w.f( 136 ) ) + ( w.f( 140 ) * w.f( 140 ) ) ) +
                                                                    ( w.f( 144 ) * w.f( 144 ) ) ) ) );
    const float nflow = -flow;
    w.setf( 296, ( w.f( 136 ) * r2 ) * nflow );
    w.setf( 300, ( w.f( 140 ) * r2 ) * nflow );
    w.setf( 304, ( w.f( 144 ) * r2 ) * nflow );
    // 0x5A28F0: the bounds - the rotated half-extent rectangle's corners (x87
    // fsincos of +108) folded through minss / maxss, z = +84 -+ +100 * 0.5, the
    // centre +44 / +48 and the radius sqrt( hy * hy + hx * hx ). +24 / +40 are
    // the high halves of zero-extended qwords: 0.
    const float c = wave_x87_cos( w.f( 108 ) );
    const float s = wave_x87_sin( w.f( 108 ) );
    const float v1 = w.f( 92 ) * 0.5f;
    const float v2 = w.f( 96 ) * 0.5f;
    const float v3 = w.f( 76 );
    const float v26 = w.f( 80 );
    const float v4 = c * v1;
    const float v24 = v26 - ( ( -s ) * v1 );
    const float v27b = v24 - ( c * v2 );
    const float v5 = ( ( -s ) * v1 ) + v26;
    const float v22 = v3 - ( c * v1 );
    const float v17 = s * v2;
    const float v21 = c * v2;
    const float v6 = v22 - v17;
    const float v18 = v4 + v3;
    const float v7 = v5 - v21;
    const float v8 = ( v4 + v3 ) - v17;
    const float v9 = water_min( v8, water_min( v6, 3.4028235e38f ) );
    const float v19 = v18 + v17;
    const float v10 = water_max( v8, water_max( v6, -3.4028235e38f ) );
    const float max_y = water_max( v24 + v21, water_max( v5 + v21, water_max( v7, water_max( v27b, -3.4028235e38f ) ) ) );
    const float min_y = water_min( v24 + v21, water_min( v5 + v21, water_min( v7, water_min( v27b, 3.4028235e38f ) ) ) );
    const float min_x = water_min( v22 + v17, water_min( v19, v9 ) );
    const float max_x = water_max( v22 + v17, water_max( v19, v10 ) );
    const float hz = w.f( 100 ) * 0.5f;
    const float z = w.f( 84 );
    w.set( 44, w.word[ 76 / 4 ] );
    w.setf( 48, v26 );
    w.setf( 12, min_x );
    w.setf( 16, min_y );
    w.setf( 20, z - hz );
    w.set( 24, 0 );
    w.setf( 28, max_x );
    w.setf( 32, max_y );
    w.setf( 36, z + hz );
    w.set( 40, 0 );
    w.setf( 52, _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( v2 * v2 ) + ( v1 * v1 ) ) ) ) );
    return w;
}

// ---------------------------------------------------------------------------
// VuWater's pool, as its ctor's 0x594B20 on +4 builds it: 1024 eight-byte
// nodes linked in order, the last to null; count 1024, the free head the
// first. Shared by the wave list (+0x30) and every wave's entry list (+0x40).
inline void water_hold_init( water_hold_t& w )
{
    w = water_hold_t{};
    water_tree_init( w.surface_tree );
    w.pool.assign( 1024, water_pool_node_t{} );
    for ( int k = 0; k < 1024; ++k )
        w.pool[ k ].next = k < 1023 ? k + 1 : -1;
    w.pool_free = 0;
    w.pool_count = 1024;
}

// A wave the hold takes: its held fields from its words (+12 .. +40 the box,
// +44 / +48 the centre, +52 the radius, +0x44 bit 0 the centre test).
inline water_held_wave_t water_held_from( const std::uint32_t* word )
{
    water_held_wave_t v{};
    std::memcpy( v.box, word + 3, 32 );
    std::memcpy( v.centre, word + 11, 8 );
    std::memcpy( &v.radius, word + 13, 4 );
    v.point = ( word[ 17 ] & 1u ) != 0;
    return v;
}

// The allocators' common tail (0x528B40 / 0x528C20 / 0x528F40 / 0x528D00 /
// 0x528910 / 0x528E90 / 0x528D70): a pool node holding the wave pushed at the
// HEAD of the wave list, then 0x529A90.
inline int water_hold_wave( water_hold_t& w, const water_held_wave_t& v )
{
    const int wave = static_cast< int >( w.waves.size( ) );
    w.waves.push_back( v );
    const int e = w.pool_free;
    w.pool_free = w.pool[ e ].next;
    --w.pool_count;
    w.pool[ e ].leaf = wave;
    w.pool[ e ].surface = -1;
    w.pool[ e ].next = w.list;
    w.list = e;
    water_hold_add( w, wave );
    return wave;
}

// 0x529860 (thiscall water, surface): the surface's box (+208 / +212 / +224 /
// +228) into the surface tree, its leaf kept (+248), then every wave on the
// list from the head - a centre-tested wave by its box's centre inside the
// surface's box, any other by box overlap - through 0x529C60.
inline int water_hold_surface( water_hold_t& w, const water_words_t& s )
{
    water_held_surface_t h{};
    for ( int k = 0; k < 16; ++k )
        h.rows[ k ] = s.f( 144 + 4 * k );
    h.z_low = s.f( 216 );
    h.z_high = s.f( 232 );
    h.half[ 0 ] = s.f( 240 );
    h.half[ 1 ] = s.f( 244 );
    h.below = s.f( 40 );
    h.above = s.f( 44 );
    water_tree_init( h.waves );
    const int surface = static_cast< int >( w.surfaces.size( ) );
    w.surfaces.push_back( std::move( h ) );
    const float r[ 4 ] = { s.f( 208 ), s.f( 212 ), s.f( 224 ), s.f( 228 ) };
    water_tree_add( w.surface_tree, surface, r );
    for ( int e = w.list; e >= 0; e = w.pool[ e ].next )
    {
        const water_held_wave_t& v = w.waves[ w.pool[ e ].leaf ];
        bool hit;
        if ( v.point )
        {
            const float cx = ( v.box[ 4 ] + v.box[ 0 ] ) * 0.5f;
            const float cy = ( v.box[ 5 ] + v.box[ 1 ] ) * 0.5f;
            hit = water_tree_ge( cx, r[ 0 ] ) && water_tree_ge( cy, r[ 1 ] ) && water_tree_ge( r[ 2 ], cx ) && water_tree_ge( r[ 3 ], cy );
        }
        else
        {
            hit = water_tree_ge( v.box[ 4 ], r[ 0 ] ) && water_tree_ge( r[ 2 ], v.box[ 0 ] ) && water_tree_ge( v.box[ 5 ], r[ 1 ] ) &&
                  water_tree_ge( r[ 3 ], v.box[ 1 ] );
        }
        if ( hit )
            water_hold_test( w, w.pool[ e ].leaf, surface );
    }
    return surface;
}

// What a build leaves: the hold and every surface / wave object's words in
// creation order (a wave entity's from wave.hxx's image), per wave its kind.
struct water_built_t
{
    water_hold_t hold;
    std::vector< water_words_t > surfaces;
    std::vector< water_words_t > waves;
    std::vector< water_kind_t > wave_kind;
    int skipped = 0;   // water entities of a class not built here, or with no state / transform
};

inline void water_built_init( water_built_t& b )
{
    b = water_built_t{};
    water_hold_init( b.hold );
}

// One entity's slot 6.
inline void water_build_entity( water_built_t& b, water_kind_t kind, const water_input_t& in )
{
    auto add_wave = [ & ]( const water_words_t& w, water_kind_t k ) {
        water_hold_wave( b.hold, water_held_from( w.word ) );
        b.waves.push_back( w );
        b.wave_kind.push_back( k );
    };
    auto from_image = [ & ]( const wave_image_t& image ) {
        water_words_t w{};
        for ( int k = 0; k < 64; ++k )
        {
            w.word[ k ] = image.word[ k ];
            w.known[ k ] = image.known[ k ];
        }
        return w;
    };
    switch ( kind )
    {
    case water_kind_surface:
    case water_kind_ramp:
    {
        b.surfaces.push_back( water_surface_create( in ) );
        water_hold_surface( b.hold, b.surfaces.back( ) );
        if ( kind == water_kind_ramp )
            add_wave( ramp_wave_create( in ), kind );
        break;
    }
    case water_kind_rectangular: add_wave( from_image( rectangular_ocean_create( in.source( ) ) ), kind ); break;
    case water_kind_circular: add_wave( from_image( circular_ocean_create( in.source( ) ) ), kind ); break;
    case water_kind_banked: add_wave( from_image( banked_turn_create( in.source( ) ) ), kind ); break;
    case water_kind_whirlpool: add_wave( from_image( whirlpool_create( in.source( ) ) ), kind ); break;
    case water_kind_directional: add_wave( from_image( directional_create( in.source( ) ) ), kind ); break;
    case water_kind_bump: add_wave( from_image( bump_create( in.source( ) ) ), kind ); break;
    default: ++b.skipped; break;
    }
}

// VuWater_createPointWave 0x528E10 (thiscall water; desc*): new 0x94 bytes,
// +8 / +0x38 / +0x3C / +0x40 / +0x8C zeroed, VuWaterPointWave_ctor 0x5A3340,
// +4 incremented (the ctor's 1 and the manager's hold), then the allocators'
// common tail (water_hold_wave). The ctor: VuWaterWave's ctor 0x5A6390 with 0
// (+0x44 = 0: tested by its box, not its centre), vtable 0x750978, +0x4C ..
// +0x77 zeroed then the 44-byte descriptor copied over them (VuPointWaveEntity
// _tick 0x5D4C50's: x, y, z, w, Falloff Time, Magnitude, Range Start, Range
// End, Speed, Frequency, Foaminess), +0x78 (the clock) = 0, +0x8C (the expiry
// callback) = 0, the byte +0x90 (expired) = 0; then 1.0 / +0x6C (+0x7C),
// 1.0 / ( +0x68 - +0x64 ) (+0x80), 1.0 / +0x5C (+0x84), 1.0 / +0x60 (+0x88);
// the box (x - End, y - End, z) .. (End + x, End + y, z), the centre (x, y),
// the radius End. +24 / +40 are the high halves of stack qwords the ctor never
// writes and +0x91 .. +0x93 heap bytes nobody writes: UNKNOWN, not produced.
inline water_words_t point_wave_create( const std::uint32_t* desc )
{
    water_words_t w{};
    w.set( 0, 0x750978u );
    w.set( 4, 2 );
    w.set( 8, 0 );
    w.set( 56, 0 );
    w.set( 60, 0 );
    w.set( 64, 0 );
    w.set( 68, 0 );
    w.set( 72, 0x3F800000u );
    for ( int k = 0; k < 11; ++k )
        w.set( 76 + 4 * k, desc[ k ] );
    w.set( 120, 0 );
    w.set( 140, 0 );
    w.setf( 124, 1.0f / w.f( 108 ) );
    w.setf( 128, 1.0f / ( w.f( 104 ) - w.f( 100 ) ) );
    w.setf( 132, 1.0f / w.f( 92 ) );
    w.setf( 136, 1.0f / w.f( 96 ) );
    const float x = w.f( 76 );
    const float y = w.f( 80 );
    const float r = w.f( 104 );
    w.setf( 12, x - r );
    w.setf( 16, y - r );
    w.set( 20, w.word[ 84 / 4 ] );
    w.setf( 28, r + x );
    w.setf( 32, r + y );
    w.set( 36, w.word[ 84 / 4 ] );
    w.setf( 44, x );
    w.setf( 48, y );
    w.setf( 52, r );
    w.word[ 144 / 4 ] = 0;   // the byte +0x90 only: `known` stays false for the word
    return w;
}

// 0x528E10 on a build: the wave's words, its hold entry and kind; returns its
// index in the hold.
inline int water_create_point_wave( water_built_t& b, const std::uint32_t* desc )
{
    b.waves.push_back( point_wave_create( desc ) );
    b.wave_kind.push_back( water_kind_point );
    return water_hold_wave( b.hold, water_held_from( b.waves.back( ).word ) );
}

// VuPointWaveEntity_tick 0x5D4C50 (thiscall entity; float dt), THE EXPORT the
// entity's Emit path calls: when the pending byte +0x9C is set it is cleared
// and the descriptor at entity +0x70 is the transform component's +0x80 ..
// +0x8F (the world position, four words) followed by the entity's +0x80 ..
// +0x9B - the ctor 0x5D43B0's bindings, in order: Falloff Time, Magnitude,
// Range Start, Range End, Speed, Frequency, Foaminess; then 0x528E10, entity
// +0xA0 = the wave, wave +0x8C = entity + 0x5C (its VuPointWaveCallbackIF:
// the wave's tick fires it once on expiry). The +0x8C pointer is held here as
// a non-zero word with `known` false - the caller owns the expiry hook.
// READ FROM THE DISASSEMBLY, NOT ORACLE-MEASURED as a unit (0x528E10 and the
// ctor it calls are: case point_waves).
inline int water_point_wave_emit( water_built_t& b, const std::uint32_t* position, const std::uint32_t* properties )
{
    std::uint32_t desc[ 11 ];
    for ( int k = 0; k < 4; ++k )
        desc[ k ] = position[ k ];
    for ( int k = 0; k < 7; ++k )
        desc[ 4 + k ] = properties[ k ];
    const int wave = water_create_point_wave( b, desc );
    b.waves.back( ).word[ 140 / 4 ] = 1u;   // entity + 0x5C: non-null
    return wave;
}

// Is `cls` a water class (registry category "Water") this build does not
// make? Counted so the gap is visible.
inline bool water_class_unbuilt( const std::string& cls )
{
    static const char* const others[] = { "VuInfiniteOceanWaveEntity", "VuDirectionalFlowWaveEntity", "VuStaticWakeWaveEntity",
        "VuWakeWaveEntity" };
    for ( const char* o : others )
        if ( cls == o )
            return true;
    return false;
}

// THE EXPORT world.hxx calls once load_entity_tree has built m_tree: every
// water entity's slot 6 in 0x500B10's order over the whole tree. An entity
// whose world is unknown, or that has no transform or no property state, is
// skipped and counted (never stood in for). `transform_of` is game/types.hxx's
// entity_transform (this file is included above it).
using water_transform_of_fn = transform_component_t* ( * )( entity_t& );

inline void water_build_world( entity_t& root, water_built_t& out, water_transform_of_fn transform_of )
{
    water_built_init( out );
    const auto visit = [ & ]( const auto& self, entity_t& e ) -> void {
        for ( auto& child : e.m_children )
            self( self, *child );
        const water_kind_t kind = water_kind_of( e.m_type );
        if ( kind == water_kind_none )
        {
            out.skipped += water_class_unbuilt( e.m_type ) ? 1 : 0;
            return;
        }
        const transform_component_t* t = transform_of( e );
        const water_entity_state_t* s = static_cast< const water_entity_state_t* >( e.m_state.get( ) );
        if ( !t || !s || e.m_world_unknown )
        {
            ++out.skipped;
            return;
        }
        std::uint32_t transform[ k_water_transform_words ];
        water_transform_words( *t, transform );
        water_input_t in{};
        in.entity = s->value.word;
        in.transform = transform;
        water_build_entity( out, kind, in );
    };
    visit( visit, root );
}
