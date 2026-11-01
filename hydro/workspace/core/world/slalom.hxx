#pragma once

// VuSlalomGateEntity (vtable 0x73B3E8, +0x5C 0x73B3D8; create 0x43F250 = new 0xEC, 0x43F2D0, ctor 0x43F390) and the
// course system of VuSlalomGame (vtable 0x73FA6C, ctor 0x48BEC0) that walks its gates - transliterated from the
// disassembly (world batch 29).
//
// The gate's ctor: "Diameter" +0x6C (VuFloatProperty 0x737484, 20.0 = 0x41A00000), "Model Asset" +0x70 /
// "Reflection Model Asset" +0x88 (VuAssetNameProperty 0x74D690 over 0x406B70: "Props/Racing/RingC" /
// "Props/Racing/RingC_Reflection"; their change callback 0x43FE60 - the model instances +0xA8 / +0xC8, the
// renderer's), "Allow Recovery" +0xA0 (VuBoolProperty 0x737450, 1); a Vu3dLayoutComponent +0x60 (its aabb -0.5 ..
// 0.5 in x / z), a VuScriptComponent +0x64 whose VuScriptRef "Next" (type 0x736430 = VuSlalomGateEntity) is +0xA4,
// a VuMotionComponent +0x68, the transform's watcher 0x43FE60; +0xE8 the previous gate (0 - VuSlalomGame's slot 5
// links it). Draw / motion (0x43FE60, 0x43FF90) are the renderer's - not here.

#include <workspace/core/entity/node.hxx>
#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <xmmintrin.h>

struct slalom_gate_props_t
{
    float diameter = 20.0f;                                  // +0x6C
    std::string model = "Props/Racing/RingC";                // +0x70
    std::string reflection = "Props/Racing/RingC_Reflection";   // +0x88
    std::uint8_t allow_recovery = 1;                         // +0xA0
};

struct slalom_gate_state_t
{
    slalom_gate_props_t value;
    slalom_gate_props_t defaults;
};

inline std::shared_ptr< void > slalom_gate_state_for( const std::string& cls )
{
    return cls == "VuSlalomGateEntity" ? std::make_shared< slalom_gate_state_t >( ) : nullptr;
}

inline slalom_gate_state_t* slalom_gate_state_of( const entity_t& e )
{
    return e.m_type == "VuSlalomGateEntity" && e.m_state ? static_cast< slalom_gate_state_t* >( e.m_state.get( ) ) : nullptr;
}

// The four nodes' vt+8 loads in the ctor's order (case slalom_gate_properties).
inline void slalom_gate_load_properties( entity_t& e, const value_t& p )
{
    slalom_gate_state_t* s = slalom_gate_state_of( e );
    if ( !s )
        return;
    property_float( p, "Diameter", s->value.diameter, s->defaults.diameter, false );
    property_string( p, "Model Asset", s->value.model, s->defaults.model );
    property_string( p, "Reflection Model Asset", s->value.reflection, s->defaults.reflection );
    property_bool( p, "Allow Recovery", s->value.allow_recovery, s->defaults.allow_recovery );
}

inline void slalom_gate_mark_properties( entity_t& e )
{
    if ( slalom_gate_state_t* s = slalom_gate_state_of( e ) )
        s->defaults = s->value;
}

// ---- VuSlalomGame's course system (ctor 0x48BEC0; world batch 29, case slalom_gates) -----------------------------
//
// The game's words (0x770 image): +0x720 the course (a VuString), +0x738 the first gate, +0x73C the gate to pass
// next, +0x740 the gate a recovery restarts after (the last passed one that allows it), +0x744 the
// Vu3dDrawComponent (0x531A40; draw manager g 0x7590B8 vt+0x28 add / +0x2C remove, its +0x1E the added flag), +0x748
// GateMissPenalty, +0x74C BoostAddAmount, +0x750 the split times (a std::vector<float>).
// The human = the boat manager g 0x758FB8 +0x24 list [0] everywhere (no count test). Its image fields: +0x190
// SlalomBounds.Pos / +0x1A0 .Radius (boat sections.hxx), +0x310 the boost energy, +0x344 / +0x348 the recovery
// point (respawn.hxx: +0x344 set = no track placement, +0x348 the 4x4 world), +0x4B0 the lap time, +0x4B4 the race
// time, +0x4D0 the gates missed, +0x4DC its world last frame.
//
// The gate links: a VuScriptRef's target is its connection (+0x18) +8 when its type chain (+0x10) reaches 0x736430
// (always: the ref's own required type) - here `next`, null when unconnected.
struct slalom_gate_t
{
    const float* world = nullptr;   // its transform's world (+0x50 -> +0x50; row 1 at [4..6], the translation [12..14])
    float diameter = 20.0f;         // +0x6C
    std::uint8_t allow_recovery = 1;   // +0xA0
    slalom_gate_t* next = nullptr;  // the "Next" ref's target (+0xA4)
    slalom_gate_t* prev = nullptr;  // +0xE8
};

struct slalom_course_t
{
    slalom_gate_t* first = nullptr;     // +0x738
    slalom_gate_t* current = nullptr;   // +0x73C
    slalom_gate_t* recover = nullptr;   // +0x740
    float penalty = 0.0f;               // +0x748
    float boost_add = 0.0f;             // +0x74C
};

// The platform calls the course makes: FMOD (g_vuaudio 0x759010 +8 getEvent( name, 1, &event ), on FMOD_OK
// set3DAttributes( event, &position, 0, 0 ) and start - imports 0x64C348 / 0x64C358 / 0x64C354: a hook) and the
// event manager's broadcast 0x505690 (FNV-1a of the name, an empty VuParams).
struct slalom_hooks_t
{
    void* user = nullptr;
    void ( *sound )( void* user, const char* event, const float* position3 ) = nullptr;
    void ( *broadcast )( void* user, const char* name ) = nullptr;
};

inline float slalom_f( const std::uint8_t* b, int at ) { float v; std::memcpy( &v, b + at, 4 ); return v; }
inline void slalom_put_f( std::uint8_t* b, int at, float v ) { std::memcpy( b + at, &v, 4 ); }

// The game's first gate, 0x48E610 (thiscall game, entity; retn 4) on the project root (+0xC): the entity itself when
// its type chain reaches VuSlalomGateEntity; else, unless it is a template instance (+0x4C set), its children (+0x38
// in order) depth first; else null.
inline const entity_t* slalom_first_gate( const entity_t& e )
{
    if ( e.m_type == "VuSlalomGateEntity" )
        return &e;
    if ( !e.m_template.empty( ) )
        return nullptr;
    for ( const auto& c : e.m_children )
        if ( const entity_t* g = slalom_first_gate( *c ) )
            return g;
    return nullptr;
}

// Slot 5 0x48C660's course part (thiscall game; the rest - the state machine's start, states[0]'s enter, and the draw
// component's add when its +0x1E is clear - is the caller's): the course +0x720 = gameutil 0x4D32F0's (the GameData
// Course - the caller's); `slalom` = ConstantDB (gameutil +4's +0x58) ["VuDBAsset"]["Slalom"] (0x4FBDF0); its
// [the human's +0x160] when a map (0x4F22B0; else the static null) -> "GateMissPenalty" -> +0x748, "BoostAddAmount" ->
// +0x74C (tag 1 cvtdq2ps, 2 the float, 7 fild / fstp, else 0); every gate from +0x738 links its next's +0xE8 to itself;
// the human's +0x344 = 1 and +0x348 = its transform's world (16 words).
inline void slalom_course_start( slalom_course_t& c, const value_t& slalom, const std::string& boat_class, std::uint8_t* boat, const float* world )
{
    static const value_t null{ };
    const value_t* row = slalom.tag == 6 ? find( slalom, boat_class.c_str( ) ) : nullptr;
    const value_t& r = row ? *row : null;
    const auto number = []( const value_t& v ) {
        if ( v.tag == 1 )
            return static_cast< float >( static_cast< std::int32_t >( v.integer ) );
        if ( v.tag == 2 )
            return v.real;
        if ( v.tag == 7 )
            return static_cast< float >( v.int64 );
        return 0.0f;
    };
    c.penalty = number( member( r, "GateMissPenalty" ) );
    c.boost_add = number( member( r, "BoostAddAmount" ) );
    for ( slalom_gate_t* g = c.first; g; g = g->next )
        if ( g->next )
            g->next->prev = g;
    boat[ 0x344 ] = 1;
    std::memcpy( boat + 0x348, world, 64 );
}

// 0x48EBB0 (thiscall game, gate; retn 4): +0x73C = the gate's next; +0x740 = the gate, then back along +0xE8 while it
// does not allow recovery (+0xA0 clear); when one is found, the human's +0x344 = 1 and +0x348 = its world.
inline void slalom_advance( slalom_course_t& c, slalom_gate_t* gate, std::uint8_t* boat )
{
    c.current = gate->next;
    c.recover = gate;
    while ( c.recover && !c.recover->allow_recovery )
        c.recover = c.recover->prev;
    if ( c.recover )
    {
        boat[ 0x344 ] = 1;
        std::memcpy( boat + 0x348, c.recover->world, 64 );
    }
}

// The gate test 0x43FC70 (thiscall gate, const VuVector3* a, const VuVector3* b, xmm3 the radius; retn 8): with the
// gate's world row 1 (n) and translation (t), da = (ny (ay - ty) + nx (ax - tx)) + nz (az - tz), db the same for b;
// 0 unless da db <= 0 and da < 0 (comiss: unordered -> 0). Then the crossing p = a + (b - a) |da| / (|db| + |da|) (as
// 1 / sum times each scaled component), d = sqrt( ((py - ty)^2 + (px - tx)^2) + (pz - tz)^2 ) - r: 2 when
// diameter x 0.5 <= d (or unordered), else 1.
inline int slalom_gate_test( const slalom_gate_t& g, const float* a, const float* b, float r )
{
    const float* m = g.world;
    const auto add = []( float x, float y ) { return _mm_cvtss_f32( _mm_add_ss( _mm_set_ss( x ), _mm_set_ss( y ) ) ); };
    const auto sub = []( float x, float y ) { return _mm_cvtss_f32( _mm_sub_ss( _mm_set_ss( x ), _mm_set_ss( y ) ) ); };
    const auto mul = []( float x, float y ) { return _mm_cvtss_f32( _mm_mul_ss( _mm_set_ss( x ), _mm_set_ss( y ) ) ); };
    const float da = add( add( mul( m[ 5 ], sub( a[ 1 ], m[ 13 ] ) ), mul( m[ 4 ], sub( a[ 0 ], m[ 12 ] ) ) ), mul( m[ 6 ], sub( a[ 2 ], m[ 14 ] ) ) );
    const float db = add( add( mul( m[ 5 ], sub( b[ 1 ], m[ 13 ] ) ), mul( m[ 4 ], sub( b[ 0 ], m[ 12 ] ) ) ), mul( m[ 6 ], sub( b[ 2 ], m[ 14 ] ) ) );
    if ( !_mm_comige_ss( _mm_set_ss( 0.0f ), _mm_set_ss( mul( db, da ) ) ) )
        return 0;
    if ( !_mm_comigt_ss( _mm_set_ss( 0.0f ), _mm_set_ss( da ) ) )
        return 0;
    const auto abs = []( float x ) { std::uint32_t w; std::memcpy( &w, &x, 4 ); w &= 0x7FFFFFFFu; std::memcpy( &x, &w, 4 ); return x; };
    const float fa = abs( da );
    const float dx = mul( sub( b[ 0 ], a[ 0 ] ), fa ), dy = mul( sub( b[ 1 ], a[ 1 ] ), fa ), dz = mul( sub( b[ 2 ], a[ 2 ] ), fa );
    const float s = _mm_cvtss_f32( _mm_div_ss( _mm_set_ss( 1.0f ), _mm_set_ss( add( abs( db ), fa ) ) ) );
    const float ex = sub( add( mul( s, dx ), a[ 0 ] ), m[ 12 ] );
    const float ey = sub( add( mul( s, dy ), a[ 1 ] ), m[ 13 ] );
    const float ez = sub( add( mul( s, dz ), a[ 2 ] ), m[ 14 ] );
    const float sum = add( add( mul( ey, ey ), mul( ex, ex ) ), mul( ez, ez ) );
    const float d = sub( _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( sum ) ) ), r );
    return _mm_comigt_ss( _mm_set_ss( mul( g.diameter, 0.5f ) ), _mm_set_ss( d ) ) ? 1 : 2;
}

// A miss 0x48E910 (thiscall game, gate; retn 4): 0x48EBB0; the human's +0x4D0 += 1, +0x4B0 = +0x4B0 + penalty, +0x4B4
// = penalty + +0x4B4; FMOD "Global/slalom/slalom_miss" at the gate's translation; +0x310 = 0; "OnSlalomGateMissed".
inline void slalom_miss( slalom_course_t& c, slalom_gate_t* gate, std::uint8_t* boat, const slalom_hooks_t& h )
{
    slalom_advance( c, gate, boat );
    std::int32_t missed;
    std::memcpy( &missed, boat + 0x4D0, 4 );
    ++missed;
    std::memcpy( boat + 0x4D0, &missed, 4 );
    slalom_put_f( boat, 0x4B0, _mm_cvtss_f32( _mm_add_ss( _mm_set_ss( slalom_f( boat, 0x4B0 ) ), _mm_set_ss( c.penalty ) ) ) );
    slalom_put_f( boat, 0x4B4, _mm_cvtss_f32( _mm_add_ss( _mm_set_ss( c.penalty ), _mm_set_ss( slalom_f( boat, 0x4B4 ) ) ) ) );
    if ( h.sound )
        h.sound( h.user, "Global/slalom/slalom_miss", gate->world + 12 );
    slalom_put_f( boat, 0x310, 0.0f );
    if ( h.broadcast )
        h.broadcast( h.user, "OnSlalomGateMissed" );
}

// A pass 0x48EA40 (thiscall game, gate; retn 4): 0x48EBB0; FMOD "Global/slalom/slalom_passthrough" at the gate's
// translation; +0x310 = maxss( 0, minss( 16.0, BoostAddAmount + +0x310 ) ) (0x6F7798); "OnSlalomGateHit".
inline void slalom_pass( slalom_course_t& c, slalom_gate_t* gate, std::uint8_t* boat, const slalom_hooks_t& h )
{
    slalom_advance( c, gate, boat );
    if ( h.sound )
        h.sound( h.user, "Global/slalom/slalom_passthrough", gate->world + 12 );
    const __m128 v = _mm_add_ss( _mm_set_ss( c.boost_add ), _mm_set_ss( slalom_f( boat, 0x310 ) ) );
    const __m128 lo = _mm_min_ss( _mm_set_ss( 16.0f ), v );
    slalom_put_f( boat, 0x310, _mm_cvtss_f32( _mm_max_ss( _mm_setzero_ps( ), lo ) ) );
    if ( h.broadcast )
        h.broadcast( h.user, "OnSlalomGateHit" );
}

// The gates tick 0x48E6E0 (thiscall game), from slot 7 0x48C8C0 after the state tick and +0x71C += dt: with a gate
// +0x73C, the human's SlalomBounds.Pos (+0x190) through its world last frame (+0x4DC, rows at +0x4DC / +0x4EC /
// +0x4FC, translation +0x50C) = a and through its transform's world = b (each component ((y r1 + x r0) + z r2) + t,
// retail's order per row), the test with the radius +0x1A0: 1 a pass, 2 a miss, 0 a miss only when b is on the gate's
// front side (n . (b - t) >= 0, ordered).
inline int slalom_gates_tick( slalom_course_t& c, std::uint8_t* boat, const float* world, const slalom_hooks_t& h )
{
    slalom_gate_t* g = c.current;
    if ( !g )
        return -1;
    const auto add = []( float x, float y ) { return _mm_cvtss_f32( _mm_add_ss( _mm_set_ss( x ), _mm_set_ss( y ) ) ); };
    const auto mul = []( float x, float y ) { return _mm_cvtss_f32( _mm_mul_ss( _mm_set_ss( x ), _mm_set_ss( y ) ) ); };
    const float x = slalom_f( boat, 0x190 ), y = slalom_f( boat, 0x194 ), z = slalom_f( boat, 0x198 );
    const auto p = [ & ]( int at ) { return slalom_f( boat, at ); };
    float a[ 3 ], b[ 3 ];
    a[ 0 ] = add( add( add( mul( p( 0x4EC ), y ), mul( x, p( 0x4DC ) ) ), mul( p( 0x4FC ), z ) ), p( 0x50C ) );
    a[ 1 ] = add( add( add( mul( p( 0x4E0 ), x ), mul( p( 0x4F0 ), y ) ), mul( p( 0x500 ), z ) ), p( 0x510 ) );
    a[ 2 ] = add( add( add( mul( p( 0x4E4 ), x ), mul( p( 0x4F4 ), y ) ), mul( p( 0x504 ), z ) ), p( 0x514 ) );
    b[ 0 ] = add( add( add( mul( world[ 4 ], y ), mul( world[ 0 ], x ) ), mul( world[ 8 ], z ) ), world[ 12 ] );
    b[ 1 ] = add( add( add( mul( world[ 1 ], x ), mul( world[ 5 ], y ) ), mul( world[ 9 ], z ) ), world[ 13 ] );
    b[ 2 ] = add( add( add( mul( world[ 2 ], x ), mul( world[ 6 ], y ) ), mul( world[ 10 ], z ) ), world[ 14 ] );
    const int r = slalom_gate_test( *g, a, b, slalom_f( boat, 0x1A0 ) );
    if ( r == 1 )
        slalom_pass( c, g, boat, h );
    else if ( r == 2 )
        slalom_miss( c, g, boat, h );
    else
    {
        const float* m = g->world;
        const float sub0 = b[ 0 ] - m[ 12 ], sub1 = b[ 1 ] - m[ 13 ], sub2 = b[ 2 ] - m[ 14 ];
        const float d = add( add( mul( m[ 5 ], sub1 ), mul( m[ 4 ], sub0 ) ), mul( m[ 6 ], sub2 ) );
        if ( _mm_comige_ss( _mm_set_ss( d ), _mm_set_ss( 0.0f ) ) )
            slalom_miss( c, g, boat, h );
    }
    return r;
}

// OnBoatRecovered 0x48DB70 (thiscall game, const VuParams&; retn 4): the params' entity (0x505370 -> the repository g
// 0x759080 vt+0x28); unless its type chain reaches VuBoatEntity (0x73628C) nothing. Its +0x4DC = its transform's world;
// with a +0x740, +0x73C = that gate's next; its +0x310 = BoostAddAmount.
inline void slalom_recovered( slalom_course_t& c, std::uint8_t* boat, const float* world )
{
    std::memcpy( boat + 0x4DC, world, 64 );
    if ( c.recover )
        c.current = c.recover->next;
    slalom_put_f( boat, 0x310, c.boost_add );
}