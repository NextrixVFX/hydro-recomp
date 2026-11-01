#pragma once

#include <workspace/core/assets/tree.hxx>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// The surface-type table: VuDynamicsContactManagerImpl (ctor 0x574EC0, vtable
// 0x74FA94, 0x20 bytes), built by VuDynamicsImpl's init 0x4F8050 into
// g_vudynamicsimpl +0x38. Its image: +0x04 / +0x08 a std::list (head
// 0x4031B0, unused by the build), +0x0C / +0x10 a std::map<std::string,
// uint8> (head 0x487F60) - name -> surface id -, +0x14..+0x1C a vector of
// 8-byte surface types (friction, colour). The ctor also installs the
// contact-added callback 0x574E90 at 0x7590D8 (the dynamics': not here).
//
// Its slots: 0 0x575440 the type count; 1 0x575450 the n-th name of the map
// (in order; 0 out of range); 2 0x5754D0 the id of a name (0x4F22B0 find;
// 0 when absent) - what 0x4F88A0 (VuDynamicsImpl: +0x38's slot 2, a tail
// jump) answers, and so what the collision mesh load 0x51AF20 writes into
// every material record's byte +40; 3 0x574E70 the type of an id
// (+0x14 + 8 * id, unchecked).
struct surface_type_t
{
    float friction = 0.0f;              // +0 (a new element's 0: 0x5757F0)
    std::uint32_t color = 0xFFFFFFFFu;  // +4 R | G << 8 | B << 16 | A << 24 (new: 0xFFFFFFFF)
};

struct surface_table_t
{
    // The map +0x0C in its order (std::string <: bytes unsigned, then the
    // shorter first) - the same order the build numbers them in.
    std::vector< std::pair< std::string, std::uint8_t > > ids;
    std::vector< surface_type_t > types;
};

// cvttss2si: truncation, 0x80000000 for a NaN or out of range.
inline std::int32_t surface_cvtt( float x )
{
    if ( !( x >= -2147483648.0f && x < 2147483648.0f ) )
        return static_cast< std::int32_t >( 0x80000000u );
    return static_cast< std::int32_t >( x );
}

// One channel of the colour parse 0x4FDEC0: an int (1) or an int64 (7: its
// low dword) as it is, a float (2) truncated; anything else false.
inline bool surface_channel( const value_t& v, std::int32_t& out )
{
    if ( v.tag == 1 )
        out = v.integer;
    else if ( v.tag == 7 )
        out = static_cast< std::int32_t >( static_cast< std::uint32_t >( static_cast< std::uint64_t >( v.int64 ) ) );
    else if ( v.tag == 2 )
    {
        float f;
        std::memcpy( &f, &v.real, 4 );
        out = surface_cvtt( f );
    }
    else
        return false;
    return true;
}

// The colour parse 0x4FDEC0 (fastcall container, out): R, G, B each a
// channel or false at once (nothing written); A a channel, else 0xFF. The
// four low bytes stored together.
inline bool surface_color( const value_t& c, std::uint32_t& out )
{
    std::int32_t r, g, b, a = 0xFF;
    if ( !surface_channel( member( c, "R" ), r ) || !surface_channel( member( c, "G" ), g ) || !surface_channel( member( c, "B" ), b ) )
        return false;
    surface_channel( member( c, "A" ), a );
    out = ( static_cast< std::uint32_t >( r ) & 0xFFu ) | ( ( static_cast< std::uint32_t >( g ) & 0xFFu ) << 8 ) |
          ( ( static_cast< std::uint32_t >( b ) & 0xFFu ) << 16 ) | ( static_cast< std::uint32_t >( a ) << 24 );
    return true;
}

// The ctor 0x574EC0's build. surface_db = the VuDBAsset/SurfaceDB asset's
// root (+0x58), or null when the asset factory (g 0x759004) has no such
// asset (0x575700: factory vt+0x28("VuDBAsset", "SurfaceDB") false - then
// nothing is read). With it: c = root["VuDBAsset"]; the vector resized to
// c's member count (0 unless an object); for i over the members in the
// map's order (0x4FC000: the i-th key): map[key] = (uint8)i (operator[]
// 0x575570), its "Friction" an int (1, converted) / a float's bits (2) / an
// int64 (7, converted) into types[i].friction, its "Color" through
// 0x4FDEC0 into types[i].color; the asset released (factory vt+0x24).
// Finally the vector resized to max(1, its size).
// Retail's container is a std::map, so its keys are unique; a document with
// a key twice is refused here (false, nothing built): what retail's reader
// keeps of a duplicate is not read (UNKNOWN).
inline bool surface_table_build( surface_table_t& t, const value_t* surface_db )
{
    t.ids.clear( );
    t.types.clear( );
    if ( surface_db )
    {
        const value_t& c = member( *surface_db, "VuDBAsset" );
        if ( c.tag == 6 )
        {
            std::vector< const std::pair< std::string, value_t >* > order;
            for ( const auto& field : c.object )
                order.push_back( &field );
            std::vector< const std::pair< std::string, value_t >* > sorted = order;
            for ( std::size_t i = 1; i < sorted.size( ); ++i )
                for ( std::size_t j = i; j > 0 && sorted[ j ]->first < sorted[ j - 1 ]->first; --j )
                    std::swap( sorted[ j ], sorted[ j - 1 ] );
            for ( std::size_t i = 1; i < sorted.size( ); ++i )
                if ( sorted[ i ]->first == sorted[ i - 1 ]->first )
                    return false;
            t.types.resize( sorted.size( ) );
            for ( std::size_t i = 0; i < sorted.size( ); ++i )
            {
                t.ids.emplace_back( sorted[ i ]->first, static_cast< std::uint8_t >( i ) );
                const value_t& entry = sorted[ i ]->second;
                const value_t& f = member( entry, "Friction" );
                if ( f.tag == 1 )
                    t.types[ i ].friction = static_cast< float >( f.integer );
                else if ( f.tag == 2 )
                    std::memcpy( &t.types[ i ].friction, &f.real, 4 );
                else if ( f.tag == 7 )
                    t.types[ i ].friction = static_cast< float >( f.int64 );
                surface_color( member( entry, "Color" ), t.types[ i ].color );
            }
        }
    }
    if ( t.types.size( ) < 1 )
        t.types.resize( 1 );
    return true;
}

// Slot 2 0x5754D0 (thiscall name; retn 4): the name up to its NUL (strlen
// into a std::string) found in the map -> its id, else 0.
inline std::uint8_t surface_id( const surface_table_t& t, const char* name )
{
    const std::string key( name );
    for ( const auto& e : t.ids )
        if ( e.first == key )
            return e.second;
    return 0;
}

// Slot 0 0x575440: the number of types.
inline std::int32_t surface_count( const surface_table_t& t )
{
    return static_cast< std::int32_t >( t.types.size( ) );
}

// Slot 1 0x575450 (thiscall n; retn 4): the n-th name in the map's order,
// null when n is outside it.
inline const std::string* surface_name( const surface_table_t& t, std::int32_t n )
{
    if ( n < 0 || static_cast< std::size_t >( n ) >= t.ids.size( ) )
        return nullptr;
    return &t.ids[ static_cast< std::size_t >( n ) ].first;
}

// Slot 3 0x574E70 (thiscall id): the type of an id (unchecked in retail;
// null here past the vector).
inline const surface_type_t* surface_type( const surface_table_t& t, std::uint8_t id )
{
    return id < t.types.size( ) ? &t.types[ id ] : nullptr;
}

// The `surface` hook boat_load_hooks_t / boat_mesh_library_t take (0x4F88A0
// on g_vudynamicsimpl): user = the surface_table_t.
inline std::uint8_t surface_hook( void* user, const std::string& name )
{
    return user ? surface_id( *static_cast< const surface_table_t* >( user ), name.c_str( ) ) : 0;
}
