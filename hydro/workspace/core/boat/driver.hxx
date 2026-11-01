#pragma once

#include <workspace/core/boat/nuisance.hxx>
#include <workspace/core/boat/sections.hxx>
#include <workspace/core/entity/object.hxx>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

// The boat's AI driver, as postLoad 0x41CFF0 builds it for Driver Type "AI"
// (the boat ctor's default): operator new(0xD4), 0x41D2E0 on the block (its
// stack argument unused; ret 4), +0x8C = 0 and +0xBC = 0, then VuAiDriver's
// ctor 0x40B690(boat) - the driver goes to boat +0x220. Recreated as the
// object graph it leaves (entity/object.hxx), in retail's allocation order:
//   0 the driver (0xD4), 1-5 its VuEntity part's map head, transform
//   component and three property nodes (vu_entity), 6 the 0x20-byte
//   _aligned_malloc block of the array at +0x84.
// A ref to the boat is object AI_DRIVER_REF_BOAT.

constexpr int AI_DRIVER_REF_BOAT = -1;

// 0x4D0CC0 (thiscall, ret 4 - its argument unused) at `at`: vtable 0x746CDC,
// +4 = 8, +0x0C = 0, an _aligned_malloc(0x20, 16) array at +8 (memcpy_s of
// nothing into it, the old - null - one freed), capacity +0x10 = 8.
inline void ai_driver_4d0cc0( std::vector< image_object_t >& g, int object, int at )
{
    std::uint8_t* b = g[ object ].image.data( );
    image_put32( b, at, 0x00746CDCu );
    image_put32( b, at + 4, 8 );
    image_put32( b, at + 12, 0 );
    image_put32( b, at + 16, 0 );
    g.push_back( {} );
    g.back( ).image.assign( 0x20, 0 );   // never written: its bytes are the allocator's
    g[ object ].refs.push_back( { at + 8, static_cast< int >( g.size( ) ) - 1, 0 } );
    image_put32( g[ object ].image.data( ), at + 16, 8 );
}

inline std::vector< image_object_t > ai_driver_construct( )
{
    std::vector< image_object_t > g( 1 );
    g[ 0 ].image.assign( 0xD4, 0 );
    std::uint8_t* b = g[ 0 ].image.data( );
    // 0x41D2E0: zeroes +8, +0x0C, +0x34..+0x5C and +0x7C (every one rewritten below).
    for ( int at : { 0x08, 0x0C, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x7C } )
        image_put32( b, at, 0 );
    image_put32( b, 0x8C, 0 );
    image_put32( b, 0xBC, 0 );
    // 0x40B690: the VuEntity base 0x500710(this, 0) - its 0x5C bytes and objects 1-5.
    const std::vector< image_object_t > entity = vu_entity( 0 );
    std::memcpy( b, entity[ 0 ].image.data( ), 0x5C );
    g[ 0 ].refs = entity[ 0 ].refs;
    for ( std::size_t k = 1; k < entity.size( ); ++k )
        g.push_back( entity[ k ] );
    b = g[ 0 ].image.data( );
    g[ 0 ].refs.push_back( { 0x5C, AI_DRIVER_REF_BOAT, 0 } );   // the boat
    image_empty_string( b, 0x60 );
    image_put32( b, 0x78, 0xFFFFFFFFu );
    image_put32( b, 0x7C, 0 );
    image_put32( b, 0x00, 0x00737B5Cu );   // VuAiDriver
    image_put32( b, 0x80, 1 );
    ai_driver_4d0cc0( g, 0, 0x84 );
    b = g[ 0 ].image.data( );
    image_put32( b, 0x84, 0x00746CC8u );
    for ( int at : { 0x98, 0x9C, 0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB8, 0xBC, 0xC0, 0xC4, 0xC8, 0xCC } )
        image_put32( b, at, 0 );
    b[ 0xD0 ] = 0;
    // g_vudevconfig's getParam("DebugDrivers") (vt+0x18): a bool (tag 3)
    // there sets the .data byte 0x758F8A - the dev config is compiled out
    // (slot 6 0x4FDB70 returns the static null container), so never.
    return g;
}
// postLoad 0x41CFF0's other Driver Types (0x404AC0's type index picks the
// human for the local player's boat; then the strings "AI", "UI", "Nuisance",
// "Ghost"). Each is retail's operator new block, a pre-step, postLoad's zero
// stores and the ctor (thiscall, the boat; retn 4), recreated as the object
// graph it leaves (objects in retail's allocation order; AI_DRIVER_REF_BOAT
// for the boat).

// "UI": as "AI" (0x41D2E0, +0x8C / +0xBC = 0, VuAiDriver's ctor 0x40B690),
// then postLoad writes the VuAiUiDriver vtable 0x737CDC. 0xD4.
inline std::vector< image_object_t > ui_driver_construct( )
{
    std::vector< image_object_t > g = ai_driver_construct( );
    image_put32( g[ 0 ].image.data( ), 0x00, 0x00737CDCu );
    return g;
}

// "Nuisance" (0x104): 0x41D280 (0x41D2E0, then +0x8C, +0xBC, +0xE8, +0xEC,
// +0xF0, +0xF4, +0xFC = 0; ret 4) and VuAiNuisanceDriver's ctor 0x40D880:
// 0x40B690, the vtable 0x737C4C, the weak reference at +0xE4 (vtable
// 0x737C44 VuWeakRef<VuBoatEntity>; +0xE8 / +0xEC its links, +0xF0 the boat
// it refers to, all 0) and its slot 0 0x40E4D0 (the release - nothing with
// +0xF0 0), +0xF4 0 (the target's sector), the word +0xF8 = 1 (+0xF8 1, +0xF9
// 0), +0xFC 0 (the audio line's event), +0x100 -1 (the last line). +0xD4 ..
// +0xE3 are never written (the allocator's).
inline std::vector< image_object_t > nuisance_driver_construct( )
{
    std::vector< image_object_t > g = ai_driver_construct( );
    g[ 0 ].image.resize( 0x104, 0 );
    std::uint8_t* b = g[ 0 ].image.data( );
    for ( int at : { 0xE8, 0xEC, 0xF0, 0xF4, 0xFC } )
        image_put32( b, at, 0 );
    image_put32( b, 0x00, 0x00737C4Cu );
    image_put32( b, 0xE4, 0x00737C44u );
    b[ 0xF8 ] = 1;
    b[ 0xF9 ] = 0;
    image_put32( b, 0x100, 0xFFFFFFFFu );
    return g;
}

// VuDriverEntity's part of the human / ghost ctors (0x40ED50 / 0x40EB50, after
// postLoad's 0x41D2E0 and its own zero store): VuEntity 0x500710(this, 0) -
// objects 1-5 - then +0x5C the boat, the empty Name string at +0x60, +0x78 -1,
// +0x7C 0.
inline std::vector< image_object_t > driver_entity_construct( std::size_t size, int zero_at )
{
    std::vector< image_object_t > g( 1 );
    g[ 0 ].image.assign( size, 0 );
    std::uint8_t* b = g[ 0 ].image.data( );
    for ( int at : { 0x08, 0x0C, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x7C } )
        image_put32( b, at, 0 );
    image_put32( b, zero_at, 0 );
    const std::vector< image_object_t > entity = vu_entity( 0 );
    std::memcpy( b, entity[ 0 ].image.data( ), 0x5C );
    g[ 0 ].refs = entity[ 0 ].refs;
    for ( std::size_t k = 1; k < entity.size( ); ++k )
        g.push_back( entity[ k ] );
    b = g[ 0 ].image.data( );
    g[ 0 ].refs.push_back( { 0x5C, AI_DRIVER_REF_BOAT, 0 } );
    image_empty_string( b, 0x60 );
    image_put32( b, 0x78, 0xFFFFFFFFu );
    image_put32( b, 0x7C, 0 );
    return g;
}

// "Ghost" (0x84): 0x41D2E0, +0x80 = 0, VuGhostDriver's ctor 0x40EB50: the
// VuDriverEntity part, the vtable 0x737DDC, +0x80 (the sector) 0.
inline std::vector< image_object_t > ghost_driver_construct( )
{
    std::vector< image_object_t > g = driver_entity_construct( 0x84, 0x80 );
    image_put32( g[ 0 ].image.data( ), 0x00, 0x00737DDCu );
    image_put32( g[ 0 ].image.data( ), 0x80, 0 );
    return g;
}

extern "C" void x87_sincos( float x, float* c, float* s );

// One DB scalar into a float lane, as the human ctor reads it: an int (tag 1)
// through cvtdq2ps, a float (tag 2) as is, an int64 (tag 7) through fild /
// fstp; anything else leaves the lane (false).
inline bool driver_db_float( std::uint8_t* image, int at, const value_t& v )
{
    float lane;
    if ( v.tag == 1 )
        lane = static_cast< float >( v.integer );
    else if ( v.tag == 2 )
        lane = v.real;
    else if ( v.tag == 7 )
        lane = static_cast< float >( v.int64 );
    else
        return false;
    std::memcpy( image + at, &lane, 4 );
    return true;
}

inline std::uint32_t driver_fnv( const char* name )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( ; *name; ++name )
        h = ( h ^ static_cast< unsigned char >( *name ) ) * 0x1000193u;
    return h;
}

// The human's graph: the objects, and the one method its ctor registers
// through 0x5156E0 (on +0xC, the entity's method map, and g_vueventmanager's
// tree): (FNV-1a hash of the name, the delegate object). The map / tree
// nodes and 0x4FBDF0's key-string temporaries retail allocates are not
// objects here.
struct human_driver_graph_t
{
    std::vector< image_object_t > objects;
    std::vector< std::pair< std::uint32_t, int > > methods;
};

// "Human" (0xD0): 0x41D2E0, +0x84 = 0, VuHumanDriver's ctor 0x40ED50. The
// VuDriverEntity part; +0x80 the explosion listener's vtable (0x737F20 then
// VuHumanDriver's 0x737F30), +0 0x737F44; +0x84 / +0x88 0, +0x8C 0, +0x90 10,
// +0x94 30, +0x98 1, +0x9C .. +0xB4 0, the word +0xB8 = 0x100 (+0xB8 0, +0xB9
// 1), +0xBA 0, +0xBC .. +0xCC 0. The delegate VuMethod1<VuHumanDriver, void,
// VuParams const&> (vtable 0x737F10, the driver, 0x40F680, 0) registered as
// "OnStartLightsOut". Then ConstantDB's ([[g_vugameutil 0x758FF8 + 4] +
// 0x58]) VuDBAsset: AccelerometerSteeringExtents [0] / [1] (a list's
// elements, else the null container) -> +0x90 / +0x94,
// AccelerometerCameraDamping -> +0x98, GamePadCameraTilt -> +0x9C; HUD's
// WrongWaySpeedThreshold -> +0xA0 (x 0.44704 when read), WrongWayAngleThreshold
// -> +0xA4 (when read: x87 fcos of it x pi/180 - this machine's FPU, as
// x87_sincos's cos), WrongWayInertiaTime -> +0xA8.
inline human_driver_graph_t human_driver_construct( const value_t& constant_db )
{
    human_driver_graph_t h;
    h.objects = driver_entity_construct( 0xD0, 0x84 );
    std::vector< image_object_t >& g = h.objects;
    std::uint8_t* b = g[ 0 ].image.data( );
    image_put32( b, 0x80, 0x00737F30u );
    image_put32( b, 0x00, 0x00737F44u );
    image_put32( b, 0x84, 0 );
    image_put32( b, 0x88, 0 );
    b[ 0x8C ] = 0;
    image_put32( b, 0x90, 0x41200000u );
    image_put32( b, 0x94, 0x41F00000u );
    image_put32( b, 0x98, 0x3F800000u );
    for ( int at = 0x9C; at <= 0xB4; at += 4 )
        image_put32( b, at, 0 );
    b[ 0xB8 ] = 0;
    b[ 0xB9 ] = 1;
    b[ 0xBA ] = 0;
    for ( int at = 0xBC; at <= 0xCC; at += 4 )
        image_put32( b, at, 0 );
    g.push_back( {} );
    g.back( ).image.assign( 0x10, 0 );
    const int delegate = static_cast< int >( g.size( ) ) - 1;
    image_put32( g.back( ).image.data( ), 0, 0x00737F10u );
    g.back( ).refs.push_back( { 4, 0, 0 } );
    image_put32( g.back( ).image.data( ), 8, 0x0040F680u );
    h.methods.push_back( { driver_fnv( "OnStartLightsOut" ), delegate } );
    image_put32( g[ 0 ].image.data( ), 16, 1 );   // the method map's size (+0xC's _Mysize)
    b = g[ 0 ].image.data( );
    const value_t& db = member( constant_db, "VuDBAsset" );
    const value_t& extents = member( db, "AccelerometerSteeringExtents" );
    static const value_t none;
    const bool list = extents.kind == value_t::kind_t::list;
    driver_db_float( b, 0x90, list && !extents.list.empty( ) ? extents.list[ 0 ] : none );
    driver_db_float( b, 0x94, list && extents.list.size( ) > 1 ? extents.list[ 1 ] : none );
    driver_db_float( b, 0x98, member( db, "AccelerometerCameraDamping" ) );
    driver_db_float( b, 0x9C, member( db, "GamePadCameraTilt" ) );
    const value_t& hud = member( db, "HUD" );
    if ( driver_db_float( b, 0xA0, member( hud, "WrongWaySpeedThreshold" ) ) )
    {
        float v;
        std::memcpy( &v, b + 0xA0, 4 );
        const std::uint32_t k = 0x3EE4E26Du;   // 0x6F7654
        float m;
        std::memcpy( &m, &k, 4 );
        v *= m;
        std::memcpy( b + 0xA0, &v, 4 );
    }
    if ( driver_db_float( b, 0xA4, member( hud, "WrongWayAngleThreshold" ) ) )
    {
        float v, c, s;
        std::memcpy( &v, b + 0xA4, 4 );
        const std::uint32_t k = 0x3C8EFA35u;   // 0x6F75DC
        float m;
        std::memcpy( &m, &k, 4 );
        x87_sincos( v * m, &c, &s );
        std::memcpy( b + 0xA4, &c, 4 );
    }
    driver_db_float( b, 0xA8, member( hud, "WrongWayInertiaTime" ) );
    return h;
}

// The drivers' load (vtable slot 2, postLoad's driver->load(container
// ["Driver"]); thiscall, retn 4). VuDriverEntity's 0x40EAC0 (VuHumanDriver /
// VuGhostDriver): "Name" (tag 4) into the string at +0x60.
inline void driver_entity_load( boat_strings_t& strings, const value_t& container )
{
    section_string( strings, 0x60, member( container, "Name" ) );
}

// VuAiDriver's 0x40B8A0 (also VuAiUiDriver / VuAiNuisanceDriver's slot 2):
// the name, then +0x98 = maxss(half the boat's aabb width in x, in y) from
// the boat (+0x5C) +0x270 / +0x280 (min / max) - (hi - lo) * 0.5 (0x6F765C)
// per lane, x kept only when strictly greater (a NaN or an equal x gives y).
// postLoad calls it BEFORE 0x420A70 writes that aabb: on a fresh boat it
// reads the ctor's zeros.
inline void ai_driver_load( std::uint8_t* driver, boat_strings_t& strings, const value_t& container, const std::uint8_t* boat )
{
    section_string( strings, 0x60, member( container, "Name" ) );
    float lo[ 2 ], hi[ 2 ];
    std::memcpy( lo, boat + 0x270, 8 );
    std::memcpy( hi, boat + 0x280, 8 );
    const float half = 0.5f;
    const float y = ( hi[ 1 ] - lo[ 1 ] ) * half;
    const float x = ( hi[ 0 ] - lo[ 0 ] ) * half;
    const float r = x > y ? x : y;
    std::memcpy( driver + 0x98, &r, 4 );
}
// ---- The drivers' dtors (vtable slot 0, scalar-deleting: thiscall(driver,
// flags), retn 4; eax = this), on retail's words ----
//
// The VuDriverEntity part (0x40EA30 is it whole, for VuDriverEntity /
// VuGhostDriver / VuHumanDriver): +0 = 0x737D4C; the +0x60 string's heap text
// freed when its capacity +0x74 is >= 16, then +0x74 = 15, +0x70 = 0, the byte
// +0x60 = 0; VuEntity's dtor 0x500870. The VuAiDriver part before it (0x40B7E0,
// with no vtable store of its own): _aligned_free(+0x8C) with +0x84 = 0x746CDC
// stored first (the VuRandomTrackPlan's dtor, inlined). VuAiUiDriver 0x40E530
// -> 0x40E560: +0 = 0x737CDC, then the AI part. VuAiNuisanceDriver 0x40D920
// -> 0x40D950: +0 = 0x737C4C; the stop mode 2 when the byte +0xF8 is set
// (else 0); with an FMOD event +0xFC: setCallback(event, 0, 0), with a mode
// stop(event, mode == 2), then +0xFC = 0; the weak reference +0xE4 (vtable
// 0x737C44 stored) released inline (vu_weak_ref_release); then the AI part.
// Then, with flags & 1, operator delete(this). The imports and 0x500870 are
// hooks, in retail's order.
enum class driver_dtor_kind_t
{
    entity,     // 0x40EA30
    ai,         // 0x40B7E0
    ui,         // 0x40E530
    nuisance,   // 0x40D920
};

struct driver_dtor_hooks_t
{
    std::function< void( std::uint32_t ) > free_block;                     // operator delete
    std::function< void( std::uint32_t ) > aligned_free;                   // _aligned_free
    std::function< void( std::uint32_t ) > event_clear_callback;           // FMOD::Event::setCallback(event, 0, 0)
    std::function< void( std::uint32_t, bool ) > event_stop;               // FMOD::Event::stop(event, immediate)
    std::function< void( ) > entity_dtor;                                  // 0x500870
};

inline std::uint32_t driver_destroy( driver_dtor_kind_t kind, const vu_words_t& w, std::uint32_t self, std::uint32_t flags,
                                     const driver_dtor_hooks_t& h )
{
    if ( kind == driver_dtor_kind_t::nuisance )
    {
        w.put( self, 0x737C4Cu );
        const std::uint32_t mode = ( w.get( self + 0xF8 ) & 0xFFu ) ? 2u : 0u;
        if ( const std::uint32_t event = w.get( self + 0xFC ) )
        {
            if ( h.event_clear_callback ) h.event_clear_callback( event );
            if ( mode && h.event_stop ) h.event_stop( event, mode == 2u );
            w.put( self + 0xFC, 0u );
        }
        w.put( self + 0xE4, 0x737C44u );
        vu_weak_ref_release( w, self + 0xE4 );
    }
    else if ( kind == driver_dtor_kind_t::ui )
        w.put( self, 0x737CDCu );
    if ( kind != driver_dtor_kind_t::entity )
    {
        const std::uint32_t block = w.get( self + 0x8C );
        w.put( self + 0x84, 0x746CDCu );
        if ( h.aligned_free ) h.aligned_free( block );
    }
    w.put( self, 0x737D4Cu );
    if ( w.get( self + 0x74 ) >= 0x10u && h.free_block )
        h.free_block( w.get( self + 0x60 ) );
    w.put( self + 0x74, 0xFu );
    w.put( self + 0x70, 0u );
    w.put( self + 0x60, w.get( self + 0x60 ) & 0xFFFFFF00u );
    if ( h.entity_dtor ) h.entity_dtor( );
    if ( ( flags & 1u ) && h.free_block )
        h.free_block( self );
    return self;
}