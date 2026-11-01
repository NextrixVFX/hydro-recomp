#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/boat/parts.hxx>
#include <workspace/core/dynamics/body.hxx>
#include <workspace/core/entity/object.hxx>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// The VuBoatEntity construction, as every allocation site does it:
// operator new(0x520), 0x41B710 (zeroes 35 pointer slots - all of which the
// ctor then writes), then the ctor 0x41B850. Recreated as the object graph it
// builds (entity/object.hxx), in retail's ALLOCATION order, so object k here
// is the k-th block retail allocates (the STL container nodes excepted:
// their contents are `boat_graph_t::methods` / `events`).
//
// Pointers into the graph are refs; two kinds point outside it:
// BOAT_REF_ASSET_* (the BoatDB / SkinDB assets the asset factory hands out)
// and BOAT_REF_STL (the entity's method map head). Function pointers, vtables
// and .rdata strings are image addresses, kept as the values retail stores.

constexpr int BOAT_REF_STL = -1;
constexpr int BOAT_REF_ASSET_BOATDB = -100;
constexpr int BOAT_REF_ASSET_SKINDB = -101;

struct boat_graph_t
{
    std::vector< image_object_t > objects;
    // The registrations 0x5156E0 makes, by FNV-1a hash of the name: into the
    // entity's own method map and into g_vueventmanager's map (one delegate
    // per name here) - each as the delegate's object index.
    std::vector< std::pair< std::uint32_t, int > > methods;
    std::vector< std::pair< std::uint32_t, int > > events;
};

inline int boat_add( boat_graph_t& g, std::size_t size )
{
    g.objects.push_back( {} );
    g.objects.back( ).image.assign( size, 0 );
    return static_cast< int >( g.objects.size( ) ) - 1;
}

inline void boat_ref( boat_graph_t& g, int object, int slot, int target, int offset )
{
    g.objects[ object ].refs.push_back( { slot, target, offset } );
}

inline std::uint32_t boat_fnv( const char* name )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( ; *name; ++name )
        h = ( h ^ static_cast< unsigned char >( *name ) ) * 0x1000193u;
    return h;
}

// A VuMethod0 / VuMethod1 delegate (0x10 bytes): vtable, the boat, the
// member function's address, 0.
inline int boat_delegate( boat_graph_t& g, std::uint32_t vtable, std::uint32_t function )
{
    const int k = boat_add( g, 0x10 );
    std::uint8_t* b = g.objects[ k ].image.data( );
    image_put32( b, 0, vtable );
    boat_ref( g, k, 4, 0, 0 );
    image_put32( b, 8, function );
    image_put32( b, 12, 0 );
    return k;
}

// Copy a parts.hxx image into a graph object of the same size.
inline int boat_image( boat_graph_t& g, const std::uint8_t* image, std::size_t size )
{
    const int k = boat_add( g, size );
    std::memcpy( g.objects[ k ].image.data( ), image, size );
    return k;
}

// A 0x5240C0 object and the three blocks it allocates, in its order (+0x38
// and +0x44 aligned 0x140 arrays, then +0x34's 16 bytes).
inline int boat_5240c0_graph( boat_graph_t& g, const boat_5240c0_t& o )
{
    const int k = boat_image( g, o.image, sizeof( o.image ) );
    const int a38 = boat_add( g, 0x140 );
    const int a44 = boat_add( g, 0x140 );
    const int a34 = boat_add( g, 0x10 );
    boat_ref( g, k, 52, a34, 0 );
    boat_ref( g, k, 56, a38, 0 );
    boat_ref( g, k, 68, a44, 0 );
    return k;
}

// VuBoatEntity's btMotionState::getWorldTransform 0x41E370 (vtable 0x7383C4
// slot 1): the transform component's world (+0x50 of the component the
// entity's +0x50 points at) times 0x410AB0's translation by the offset at boat
// +0x290, converted by 0x410C30 to a btTransform: the basis transposed into
// rows with a zero w, then the origin.
struct boat_motion_t
{
    const std::uint8_t* boat;
    const std::uint8_t* transform;
};

inline bool boat_motion_get( void* user, std::uint32_t, float* out )
{
    const boat_motion_t& m = *static_cast< const boat_motion_t* >( user );
    float world[ 16 ], offset[ 16 ], product[ 16 ];
    std::memcpy( world, m.transform + 0x50, 64 );
    vu_identity( offset );
    std::memcpy( offset + 12, m.boat + 0x290, 12 );
    offset[ 15 ] = 1.0f;
    hull_multiply( offset, product, world );
    const int column[ 3 ][ 3 ] = { { 0, 4, 8 }, { 1, 5, 9 }, { 2, 6, 10 } };
    for ( int row = 0; row < 3; ++row )
    {
        for ( int k = 0; k < 3; ++k )
            out[ 4 * row + k ] = product[ column[ row ][ k ] ];
        out[ 4 * row + 3 ] = 0.0f;
    }
    out[ 12 ] = product[ 12 ];
    out[ 13 ] = product[ 13 ];
    out[ 14 ] = product[ 14 ];
    out[ 15 ] = 0.0f;
    return true;
}

// 0x41B850 itself. `constant_db` is ConstantDB's root (the hull reads it),
// `boat_db` BoatDB's (the Beached / Hollywood keys under VuDBAsset/Boat... as
// [[g_vugameutil + 4] + 0x58] - ConstantDB - holds them).
inline boat_graph_t boat_construct( const value_t& constant_db )
{
    boat_graph_t g;
    // VuEntity 0x500710(this, 0): the entity part becomes the boat's first
    // 0x5C bytes; its transform component and property nodes are objects 1-4.
    const std::vector< image_object_t > entity = vu_entity( 0 );
    const int boat = boat_add( g, 0x520 );
    std::uint8_t* b = g.objects[ boat ].image.data( );
    std::memcpy( b, entity[ 0 ].image.data( ), 0x5C );
    for ( int k = 2; k <= 5; ++k )
        g.objects.push_back( entity[ k ] );
    auto remap = []( int object ) { return object == 0 ? 0 : object - 1; };   // vu_entity's 2..5 -> 1..4
    for ( int k = 1; k <= 4; ++k )
        for ( auto& r : g.objects[ k ].refs )
            r.object = remap( r.object );
    boat_ref( g, boat, 12, BOAT_REF_STL, 0 );
    for ( const auto& r : entity[ 0 ].refs )
        if ( r.slot != 12 )
            boat_ref( g, boat, r.slot, remap( r.object ), r.offset );
    b = g.objects[ boat ].image.data( );

    // The three interface vtables, then VuBoatEntity's own.
    image_put32( b, 0, 0x007383D4u );
    image_put32( b, 92, 0x007383C4u );
    image_put32( b, 96, 0x007383B8u );
    image_put32( b, 100, 0x007383B0u );
    auto text = [ & ]( int at, const char* s )
    {
        image_empty_string( b, at );
        parts_string( b, at, s );
    };
    text( 116, "AI" );
    text( 140, "Default" );
    for ( int at : { 164, 196, 220, 244 } )
        image_empty_string( b, at );
    image_put32( b, 268, 0x7F7FFFFFu );
    image_put32( b, 272, 0x7F7FFFFFu );
    image_put32( b, 276, 0xFF404040u );
    image_put32( b, 280, 0xFFFFFFFFu );
    image_empty_string( b, 284 );
    image_put32( b, 308, 0x43FA0000u );
    image_put32( b, 312, 0x42480000u );
    image_put32( b, 316, 0x41B2F5C3u );
    image_put32( b, 320, 0x7F7FFFFFu );
    image_put32( b, 324, 0x3F800000u );
    for ( int at : { 328, 352, 376 } )
        image_empty_string( b, at );
    for ( int at : { 400, 404, 408, 416 } )
        image_put32( b, at, 0 );
    for ( int k = 0; k < 5; ++k )
        image_empty_string( b, 420 + 24 * k );
    image_put32( b, 544, 0 );
    for ( int at : { 592, 596, 600 } )
        image_put32( b, at, 0x3F800000u );
    image_put32( b, 604, 0 );
    image_put32( b, 576, 0x007380C4u );   // VuStridingMesh
    image_put32( b, 608, 0 );
    // +0x270 / +0x280 and +0x278 / +0x288: two qwords each of (0, 0) and of a
    // stack pair (v76[3] = 0, v76[4] - a stack address: indeterminate).
    for ( int at : { 624, 628, 640, 644, 632, 648 } )
        image_put32( b, at, 0 );
    for ( int at : { 656, 660, 664, 688 } )
        image_put32( b, at, 0 );
    b[ 672 ] = 0;
    for ( int at : { 692, 696, 700, 704, 708, 712 } )
        image_put32( b, at, 0x3F800000u );
    // Three VuModelSkins (vtable 0x738068), each with an _aligned_malloc(0x20)
    // array at +4 and a capacity of 8 at +0x0C: objects 5, 6, 7.
    for ( int skin : { 716, 732, 748 } )
    {
        image_put32( b, skin, 0x00738068u );
        image_put32( b, skin + 8, 0 );
        const int array = boat_add( g, 0x20 );
        b = g.objects[ boat ].image.data( );
        boat_ref( g, boat, skin + 4, array, 0 );
        image_put32( b, skin + 12, 8 );
    }
    image_put32( b, 764, 0 );
    image_put32( b, 768, 0 );
    b[ 772 ] = 0;
    b[ 773 ] = 0;
    b[ 774 ] = 0;
    for ( int at = 776; at <= 796; at += 4 )
        image_put32( b, at, 0 );
    image_put32( b, 800, 0x3F800000u );
    for ( int at : { 804, 808, 812, 816, 824, 828, 832 } )
        image_put32( b, at, 0 );
    b[ 820 ] = 0;
    b[ 836 ] = 0;
    parts_static_identity( b, 840 );
    b[ 904 ] = 0;
    b[ 905 ] = 0;
    image_put32( b, 908, 0x41200000u );
    image_put32( b, 912, 0x41200000u );
    for ( int at : { 916, 920, 924, 932, 936, 940, 948, 952, 956, 964, 968, 972 } )
        image_put32( b, at, 0 );
    b[ 980 ] = 0;
    for ( int at : { 984, 988, 992, 1000, 1004, 1008, 1016, 1020, 1024, 1028 } )
        image_put32( b, at, 0 );
    {
        boat_41b400_t o;
        boat_41b400( o );
        std::memcpy( b + 1032, o.image, sizeof( o.image ) );
    }
    for ( int at : { 1120, 1124, 1128, 1132, 1144, 1148, 1152, 1164, 1176, 1188, 1184, 1180 } )
        image_put32( b, at, 0 );
    image_put32( b, 1136, 0x3F800000u );
    image_put32( b, 1140, 0x3F800000u );
    b[ 1156 ] = 0;
    const std::uint16_t one = 1, zero = 0;
    std::memcpy( b + 1160, &one, 2 );
    image_put32( b, 1168, 0xFFFFFFFFu );
    std::memcpy( b + 1172, &zero, 2 );
    {
        boat_41b1b0_t o;
        boat_41b1b0( o );
        std::memcpy( b + 1196, o.image, sizeof( o.image ) );
    }

    // The components, each appended to the entity's list at +0x48 (linked
    // through each component's +0x0C): layout, draw (reflect 0), instigator.
    int previous = 1;   // the transform component heads the list
    auto append = [ & ]( int component )
    {
        boat_ref( g, previous, 12, component, 0 );
        previous = component;
    };
    {
        vu_3d_layout_t o;
        vu_3d_layout( o, 0 );
        const int k = boat_image( g, o.image, sizeof( o.image ) );
        boat_ref( g, k, 8, 0, 0 );
        boat_ref( g, boat, 104, k, 0 );
        append( k );
    }
    {
        vu_3d_draw_t o;
        vu_3d_draw( o, 0, 0 );
        const int k = boat_image( g, o.image, sizeof( o.image ) );
        boat_ref( g, k, 8, 0, 0 );
        const int p0 = boat_image( g, o.properties[ 0 ].image, 0x1C );
        const int p1 = boat_image( g, o.properties[ 1 ].image, 0x1C );
        boat_ref( g, k, 4, p0, 0 );
        boat_ref( g, p0, 16, p1, 0 );
        boat_ref( g, p0, 24, k, o.properties[ 0 ].member );
        boat_ref( g, p1, 24, k, o.properties[ 1 ].member );
        boat_ref( g, boat, 108, k, 0 );
        append( k );
    }
    {
        // VuInstigatorComponent (vtable 0x74D29C, 0x2C bytes), inline.
        const int k = boat_add( g, 0x2C );
        std::uint8_t* c = g.objects[ k ].image.data( );
        image_put32( c, 4, 0 );
        boat_ref( g, k, 8, 0, 0 );
        image_put32( c, 12, 0 );
        image_put32( c, 0, 0x0074D29Cu );
        for ( int at : { 20, 24, 28, 36 } )
            image_put32( c, at, 0 );
        c[ 40 ] = 0;
        boat_ref( g, boat, 112, k, 0 );
        append( k );
    }

    // The delegates: the transform component's +0xC0 (VuMethod0 0x421670, with
    // +0xC4 = 63), the layout's +0x10 (0x41EA30), the draw's +0x10 (0x4077E3).
    {
        const int k = boat_delegate( g, 0x00738388u, 0x00421670u );
        boat_ref( g, 1, 192, k, 0 );
        image_put32( g.objects[ 1 ].image.data( ), 196, 63 );
        boat_ref( g, 8, 16, boat_delegate( g, 0x00738378u, 0x0041EA30u ), 0 );
        boat_ref( g, 9, 16, boat_delegate( g, 0x00738368u, 0x004077E3u ), 0 );
    }

    // The properties, appended to the entity's +0x44 list (linked through +0x10).
    auto property = [ & ]( std::uint32_t name, int target, const char* value, std::uint32_t vtable )
    {
        vu_string_property_t o;
        vu_string_property( o, name, 0, value );
        // The subclass's block is 0x4C: +0x48 is its own (the enum table, the
        // DB asset), written by the caller.
        const int k = boat_add( g, 0x4C );
        std::memcpy( g.objects[ k ].image.data( ), o.image, sizeof( o.image ) );
        image_put32( g.objects[ k ].image.data( ), 72, 0 );
        image_put32( g.objects[ k ].image.data( ), 0, vtable );
        boat_ref( g, k, 68, 0, target );
        return k;
    };
    const int driver = property( 0x0066CDFCu, 116, "AI", 0x0074C394u );   // "Driver Type"
    image_put32( g.objects[ driver ].image.data( ), 72, 0x0073839Cu );
    boat_ref( g, boat, 68, driver, 0 );
    const int boat_prop = property( 0x0066DD1Cu, 140, "Default", 0x0074C4C0u );   // "Boat" (BoatDB)
    boat_ref( g, boat_prop, 72, BOAT_REF_ASSET_BOATDB, 0 );
    boat_ref( g, boat, 188, boat_prop, 0 );
    boat_ref( g, driver, 16, boat_prop, 0 );
    const int skin_prop = property( 0x0066DD24u, 164, "", 0x0074C4C0u );   // "Skin" (SkinDB)
    boat_ref( g, skin_prop, 72, BOAT_REF_ASSET_SKINDB, 0 );
    boat_ref( g, boat, 192, skin_prop, 0 );
    boat_ref( g, boat_prop, 16, skin_prop, 0 );
    boat_ref( g, skin_prop, 12, boat_delegate( g, 0x00738388u, 0x00421490u ), 0 );
    g.objects[ skin_prop ].image[ 9 ] = 1;
    b = g.objects[ boat ].image.data( );

    // The hull's rigid body: btAlignedAlloc(0x300, 16) - the block holds its
    // own unaligned address at +0x0C and the body at +0x10 - then 0x412AA0
    // (pointer slots zeroed), then VuRigidBody 0x527B80(cinfo, boat, 16, -1)
    // over the btRigidBodyConstructionInfo 0x4131E0 built on the stack (mass
    // 0, the boat's motion state at +0x5C, no shape, zero inertia).
    {
        boat_4131e0_t cinfo;
        const std::uint8_t zero16[ 16 ]{};
        boat_4131e0( cinfo, 0, 1, 0, zero16 );   // the motion state: non-null (a pointer word, ref'd below)
        // Then (decompile v78 = v79 = 0; disassembly [esp+0xBC] / [esp+0xC0])
        // the cinfo's sleeping thresholds +0x80 / +0x84 are zeroed.
        image_put32( cinfo.image, 0x80, 0 );
        image_put32( cinfo.image, 0x84, 0 );
        const int k = boat_add( g, 0x313 );
        boat_ref( g, k, 12, k, 0 );
        vu_rigid_body_t body{};
        std::uint32_t unique = 0;   // 0x759030 before any body
        boat_motion_t motion{ g.objects[ boat ].image.data( ), g.objects[ 1 ].image.data( ) };
        vu_rigid_body_construct( body, cinfo.image, 16, -1, unique, boat_motion_get, &motion );
        body_word( body, 0x1F4 ) = 0;   // +500 the motion state: a pointer
        body_word( body, 0 ) = 0x0074CFF0u;   // the VuRigidBody vtable, retail's last store over the btRigidBody one
        std::memcpy( g.objects[ k ].image.data( ) + 16, &body, 0x2D4 );   // the body's 0x2D4 bytes (sizeof is 0x2E0: 16-byte alignment padding)
        boat_ref( g, k, 16 + 500, 0, 92 );
        boat_ref( g, k, 16 + 704, 0, 0 );
        boat_ref( g, k, 16 + 708, 0, 100 );
        boat_ref( g, boat, 540, k, 16 );
    }

    // The boat's parts, each with the boat as its owner.
    {
        boat_409750_t o;
        boat_409750( o, 0 );
        const int k = boat_image( g, o.image, sizeof( o.image ) );
        boat_ref( g, k, 576, 0, 0 );
        boat_ref( g, boat, 548, k, 0 );
    }
    {
        boat_splash_t o;
        boat_splash( o, 0, constant_db );
        const int k = boat_image( g, o.image, sizeof( o.image ) );
        boat_ref( g, k, 0, 0, 0 );
        boat_ref( g, k, 460, boat_5240c0_graph( g, *o.at_1cc ), 0 );
        // BigSplashPfx: 23 characters, so the string moved to the heap - MSVC
        // grows an inline string to (size | 15) = 31, one 0x20 block.
        const int str = boat_add( g, 0x20 );
        std::memcpy( g.objects[ str ].image.data( ), o.pfx.c_str( ), o.pfx.size( ) + 1 );
        boat_ref( g, k, 380, str, 0 );
        image_put32( g.objects[ k ].image.data( ), 396, static_cast< std::uint32_t >( o.pfx.size( ) ) );
        image_put32( g.objects[ k ].image.data( ), 400, static_cast< std::uint32_t >( o.pfx.size( ) | 15 ) );
        boat_ref( g, boat, 552, k, 0 );
    }
    {
        boat_413590_t o;
        boat_413590( o, 0 );
        const int k = boat_image( g, o.image, sizeof( o.image ) );
        boat_ref( g, k, 0, 0, 0 );
        boat_ref( g, k, 48, boat_add( g, 0x20 ), 0 );
        boat_ref( g, boat, 556, k, 0 );
    }
    {
        boat_40fe00_t o;
        boat_40fe00( o, 0 );
        const int k = boat_image( g, o.image, sizeof( o.image ) );
        boat_ref( g, k, 0, 0, 0 );
        boat_ref( g, boat, 560, k, 0 );
    }
    {
        boat_4192e0_t o;
        boat_4192e0( o, 0 );
        const int k = boat_image( g, o.image, sizeof( o.image ) );
        boat_ref( g, k, 48, 0, 0 );
        boat_ref( g, k, 52, boat_5240c0_graph( g, *o.at_34 ), 0 );
        boat_ref( g, boat, 564, k, 0 );
    }
    {
        boat_410d90_t o;
        boat_410d90( o, 0 );
        const int k = boat_image( g, o.image, sizeof( o.image ) );
        boat_ref( g, k, 0, 0, 0 );
        boat_ref( g, k, 156, boat_add( g, 0x20 ), 0 );
        boat_ref( g, boat, 568, k, 0 );
    }
    for ( int pair : { 0, 1 } )
    {
        vu_animated_model_t o;
        vu_animated_model( o );
        const int k = boat_image( g, o.image, sizeof( o.image ) );
        boat_ref( g, k, 8, 0, pair ? 732 : 716 );
        boat_ref( g, boat, pair ? 680 : 676, k, 0 );
    }
    {
        // VuStaticModelInstance (vtable 0x74CDAC, 0x20 bytes), inline.
        const int k = boat_add( g, 0x20 );
        std::uint8_t* s = g.objects[ k ].image.data( );
        image_put32( s, 4, 0 );
        image_put32( s, 0, 0x0074CDACu );
        image_put32( s, 12, 0 );
        image_put32( s, 16, 0 );
        image_put32( s, 20, 0x3F800000u );
        image_put32( s, 24, 0xFFFFFFFFu );
        std::memcpy( s + 28, &zero, 2 );
        boat_ref( g, k, 8, 0, 748 );
        boat_ref( g, boat, 684, k, 0 );
    }

    // BoatDB / ConstantDB's VuDBAsset/Boat: the two Beached thresholds (int /
    // float / int64 lanes) and the four Hollywood vectors (vec3 0x4FDC60), then
    // the linear ones x 0x3EE4E26D (mph -> m/s) and the angular x 0x3C8EFA35.
    // The angular pair is written back through a stack temp whose fourth lane
    // (+0x3C0, +0x3D0) is an unset stack slot: indeterminate.
    b = g.objects[ boat ].image.data( );
    const value_t& db = member( member( constant_db, "VuDBAsset" ), "Boat" );
    parts_db_float( b, 908, member( db, "BeachedSpeedThreshold" ) );
    parts_db_float( b, 912, member( db, "BeachedTimeInAirThreshold" ) );
    vec3( member( db, "HollywoodMinLinVel" ), reinterpret_cast< float* >( b + 916 ) );
    vec3( member( db, "HollywoodMaxLinVel" ), reinterpret_cast< float* >( b + 932 ) );
    vec3( member( db, "HollywoodMinAngVel" ), reinterpret_cast< float* >( b + 948 ) );
    vec3( member( db, "HollywoodMaxAngVel" ), reinterpret_cast< float* >( b + 964 ) );
    auto scale = [ & ]( int at, std::uint32_t bits )
    {
        float k;
        std::memcpy( &k, &bits, 4 );
        for ( int lane = 0; lane < 3; ++lane )
        {
            float v;
            std::memcpy( &v, b + at + 4 * lane, 4 );
            v *= k;
            std::memcpy( b + at + 4 * lane, &v, 4 );
        }
    };
    scale( 916, 0x3EE4E26Du );
    scale( 932, 0x3EE4E26Du );
    scale( 948, 0x3C8EFA35u );
    scale( 964, 0x3C8EFA35u );

    // The two event methods, registered through 0x5156E0 into the entity's map
    // and g_vueventmanager's.
    for ( const auto& event : { std::pair< const char*, std::uint32_t >{ "OnCameraShake", 0x0041E7B0u },
                                std::pair< const char*, std::uint32_t >{ "OnRumble", 0x0041E950u } } )
    {
        const int k = boat_delegate( g, 0x00738358u, event.second );
        g.methods.push_back( { boat_fnv( event.first ), k } );
        g.events.push_back( { boat_fnv( event.first ), k } );
    }
    // The method map's size word (boat +0x10, std::map's _Mysize).
    image_put32( g.objects[ boat ].image.data( ), 16, static_cast< std::uint32_t >( g.methods.size( ) ) );
    std::sort( g.methods.begin( ), g.methods.end( ) );
    std::sort( g.events.begin( ), g.events.end( ) );
    for ( auto& o : g.objects )
        std::sort( o.refs.begin( ), o.refs.end( ), []( const image_ref_t& x, const image_ref_t& y ) { return x.slot < y.slot; } );
    return g;
}
