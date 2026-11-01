#pragma once

#include <workspace/core/assets/scene.hxx>
#include <workspace/core/assets/tree.hxx>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <xmmintrin.h>

// The boat's skin pass, recreated 1:1 from the disassembly / decompilation (world batch 12):
//
//   0x421490 VuBoatEntity's skin pass (thiscall boat): the Skin VuDBEntryProperty's +0xC delegate (0x41B850) and the
//            last step of VuBoatDB_loadAllSections 0x420A70. The property (+0xC0): +0x48 the VuDBAsset (SkinDB) - its
//            +0x58 document's "VuDBAsset" (0x4FBDF0), when that is an object (kind 6), std::map find 0x4F22B0 on
//            the property value's VuString (+0x44) -> the entry (node +0x28), else the static null container
//            0x7593A0 (as for a null asset). Then VuModelSkin build 0x521F20 three times, each on entry["Boat"]:
//            +0x2CC with the animated instance +0x2A4's scene (+0x14), +0x2DC with +0x2A8's (+0x14), +0x2EC with
//            the static instance +0x2AC's (+0x10); then 0x4119F0 on +0x238 (the damage pieces) with the entry.
//   0x4119F0 (thiscall pieces; entry): for each of +0xA0 items (+0x9C) 0x521F20 on item +0x68 with the item's
//            scene +0x58 and entry["Dest"].
//   0x521F20 VuModelSkin build (thiscall skin; scene, json): the skin's materials are released (0x521E40 and the
//            temporary copy's 0x53A710 - the platform material cache's refcounts); with a scene, for every shader of
//            its vector (+0xC .. +0x10) the shader's material (+0x24) is copied - its 0x6C8-byte desc: 16
//            constants of 52 bytes (name +0, type +0x24, value +0x28 .. +0x33; count +0x340), 8 textures of 112
//            bytes from +0x344 (name +0, asset +0x30, 64 bytes; count +0x6C4) - and rewritten from the json:
//            * each texture whose name is a "Textures" key with a string value (kind 4): the asset's 64 bytes
//              zeroed, then strcpy_s( asset, 64, the string );
//            * when "Constants" is an object (kind 6), each constant whose name is one of its keys, by the
//              constant's type +0x24: 0 an int (+0x28) - kind 1 the int, 2 cvttss2si of the float, 7 the int64's
//              low word, else 0; 1 a float - kind 1 cvtdq2ps, 2 the bits, 7 fild / fstp, else 0.0; 2 three floats
//              +0x28 / +0x2C / +0x30 from the value's "X" / "Y" / "Z" by the float rule; other types untouched.
//            The rewritten desc goes to the cache's create 0x53A440( g_vugfxsort, material +0x6D4, +0x6D8, desc )
//            and the result is appended to the skin (+4 data, +8 count, +0xC capacity) in shader order.
//
// HOOKS (the rendering platform layer, not copied): g_vugfxsort's material cache - the create 0x53A440 (its key
// search, VuShaderProgram_resolveConstants, the composer slot), the releases 0x53A710 / 0x521E40 and their D3D11
// calls. What a skin array holds here is the rewritten desc and the program / layout words the create was handed.
// UNKNOWN (returns false, never a stand-in): a texture string of 64 bytes or more - retail's strcpy_s hits the
// invalid-parameter handler.

constexpr std::size_t k_livery_desc_size = 0x6C8;

struct livery_material_t
{
    std::array< std::uint8_t, k_livery_desc_size > desc{};
    std::uint32_t program = 0;   // material +0x6D4
    std::uint32_t layout = 0;    // material +0x6D8
};

// One scene: its shaders' materials (+0x24), in the shader vector's order.
using livery_scene_t = std::vector< livery_material_t >;

// The static null container 0x7593A0 (tag 0).
inline const value_t& livery_null( )
{
    static const value_t null{};
    return null;
}

// 0x4FBDF0: a member of an object (kind 6), else the null container.
inline const value_t& livery_member( const value_t& container, const char* key )
{
    if ( container.tag != 6 )
        return livery_null( );
    const value_t* found = find( container, key );
    return found != nullptr ? *found : livery_null( );
}

inline std::uint32_t livery_u32( const std::uint8_t* p )
{
    std::uint32_t v;
    std::memcpy( &v, p, 4 );
    return v;
}

// The float rule of 0x521F20's float and vec3 branches.
inline std::uint32_t livery_float( const value_t& v )
{
    float f = 0.0f;
    std::uint32_t bits;
    if ( v.tag == 1 )
        f = static_cast< float >( v.integer );   // cvtdq2ps
    else if ( v.tag == 2 )
    {
        std::memcpy( &bits, &v.real, 4 );   // the bits as stored
        return bits;
    }
    else if ( v.tag == 7 )
        f = static_cast< float >( v.int64 );   // fild / fstp: one rounding
    std::memcpy( &bits, &f, 4 );
    return bits;
}

// The int rule of the type-0 branch.
inline std::uint32_t livery_int( const value_t& v )
{
    if ( v.tag == 1 )
        return static_cast< std::uint32_t >( v.integer );
    if ( v.tag == 2 )
        return static_cast< std::uint32_t >( _mm_cvtt_ss2si( _mm_set_ss( v.real ) ) );   // cvttss2si
    if ( v.tag == 7 )
        return static_cast< std::uint32_t >( static_cast< std::uint64_t >( v.int64 ) & 0xFFFFFFFFu );
    return 0;   // 0.0's bits
}

// 0x521F20's rewrite of one material's desc from the json (the "Boat" / "Dest" node).
inline bool livery_rewrite( std::uint8_t* desc, const value_t& json )
{
    const value_t& textures = livery_member( json, "Textures" );
    const value_t& constants = livery_member( json, "Constants" );
    const std::uint32_t texture_count = livery_u32( desc + 0x6C4 );
    for ( std::uint32_t k = 0; k < texture_count; ++k )
    {
        std::uint8_t* record = desc + 0x344 + 112 * static_cast< std::size_t >( k );
        const value_t& value = livery_member( textures, reinterpret_cast< const char* >( record ) );
        if ( value.tag != 4 )
            continue;
        std::memset( record + 0x30, 0, 0x40 );
        const std::size_t length = std::strlen( value.text.c_str( ) );   // strcpy_s stops at the first NUL
        if ( length >= 0x40 )
            return false;   // UNKNOWN: strcpy_s's invalid-parameter handler
        std::memcpy( record + 0x30, value.text.c_str( ), length + 1 );
    }
    const std::uint32_t constant_count = livery_u32( desc + 0x340 );
    for ( std::uint32_t k = 0; k < constant_count && constants.tag == 6; ++k )
    {
        std::uint8_t* record = desc + 52 * static_cast< std::size_t >( k );
        const char* name = reinterpret_cast< const char* >( record );
        const value_t* found = find( constants, name );   // the std::map find on the name's VuString
        if ( found == nullptr )
            continue;
        const std::uint32_t type = livery_u32( record + 0x24 );
        std::uint32_t words[ 3 ];
        if ( type == 0 )
        {
            words[ 0 ] = livery_int( *found );
            std::memcpy( record + 0x28, words, 4 );
        }
        else if ( type == 1 )
        {
            words[ 0 ] = livery_float( *found );
            std::memcpy( record + 0x28, words, 4 );
        }
        else if ( type == 2 )
        {
            words[ 0 ] = livery_float( livery_member( *found, "X" ) );
            words[ 1 ] = livery_float( livery_member( *found, "Y" ) );
            words[ 2 ] = livery_float( livery_member( *found, "Z" ) );
            std::memcpy( record + 0x28, words, 12 );
        }
    }
    return true;
}

// 0x521F20 on one skin: the scene's materials rewritten in order (a null scene leaves the skin empty).
inline bool livery_build( const livery_scene_t* scene, const value_t& json, std::vector< livery_material_t >& skin )
{
    skin.clear( );
    if ( scene == nullptr )
        return true;
    for ( const livery_material_t& material : *scene )
    {
        livery_material_t made = material;
        if ( !livery_rewrite( made.desc.data( ), json ) )
            return false;
        skin.push_back( made );
    }
    return true;
}

// 0x421490's entry lookup: SkinDB's document (null when the property has no asset) -> the entry for `skin`.
inline const value_t& livery_entry( const value_t* document, const std::string& skin )
{
    if ( document == nullptr )
        return livery_null( );
    const value_t& table = livery_member( *document, "VuDBAsset" );
    if ( table.tag != 6 )
        return livery_null( );
    const value_t* entry = find( table, skin.c_str( ) );
    return entry != nullptr ? *entry : livery_null( );
}

// 0x421490 whole: scenes[ 0 .. 2 ] the instances +0x2A4 / +0x2A8 / +0x2AC (null when not loaded), pieces the
// damage items' scenes (0x4119F0, in item order). skins[ 0 .. 2 ] are +0x2CC / +0x2DC / +0x2EC, then the items'.
inline bool livery_pass( const value_t* document, const std::string& skin, const livery_scene_t* const scenes[ 3 ],
                         const std::vector< const livery_scene_t* >& pieces, std::vector< std::vector< livery_material_t > >& skins )
{
    const value_t& entry = livery_entry( document, skin );
    skins.assign( 3 + pieces.size( ), { } );
    const value_t& boat = livery_member( entry, "Boat" );
    for ( int i = 0; i < 3; ++i )
        if ( !livery_build( scenes[ i ], boat, skins[ static_cast< std::size_t >( i ) ] ) )
            return false;
    for ( std::size_t j = 0; j < pieces.size( ); ++j )
        if ( !livery_build( pieces[ j ], livery_member( entry, "Dest" ), skins[ 3 + j ] ) )
            return false;
    return true;
}

// A scene's material desc as 0x5929B0's table reads leave it for the create (scene.hxx's table 0, the program's:
// 0x541290's constants, 0x5414C0's textures) - each record's strings strcpy_s'd into their fields, the words as
// read. Bytes past a string's NUL and past the counts are zero here (retail's desc is a stack copy: they never
// reach a consumer - 0x521F20 reads names up to the NUL, counted records only). A table over 16 constants / 8
// textures or a string too long for its field cannot be a desc: false.
inline bool livery_desc_of( const scene_table_t& table, livery_material_t& out )
{
    out = livery_material_t{ };
    if ( table.constants.size( ) > 16 || table.textures.size( ) > 8 )
        return false;
    const auto put = [ ]( std::uint8_t* at, const std::string& s, std::size_t room )
    {
        if ( s.size( ) >= room )
            return false;
        std::memcpy( at, s.c_str( ), s.size( ) + 1 );
        return true;
    };
    for ( std::size_t k = 0; k < table.constants.size( ); ++k )
    {
        std::uint8_t* record = out.desc.data( ) + 52 * k;
        const scene_constant_t& c = table.constants[ k ];
        if ( !put( record, c.name, 32 ) )
            return false;
        std::memcpy( record + 0x24, &c.type, 4 );
        std::memcpy( record + 0x28, c.value, 12 );
    }
    for ( std::size_t k = 0; k < table.textures.size( ); ++k )
    {
        std::uint8_t* record = out.desc.data( ) + 0x344 + 112 * k;
        const scene_texture_t& t = table.textures[ k ];
        if ( !put( record, t.name, 32 ) || !put( record + 0x20, t.slot, 8 ) || !put( record + 0x30, t.asset, 64 ) )
            return false;
        std::memcpy( record + 0x2C, &t.value, 4 );
    }
    const std::uint32_t nc = static_cast< std::uint32_t >( table.constants.size( ) );
    const std::uint32_t nt = static_cast< std::uint32_t >( table.textures.size( ) );
    std::memcpy( out.desc.data( ) + 0x340, &nc, 4 );
    std::memcpy( out.desc.data( ) + 0x6C4, &nt, 4 );
    return true;
}

// The desc back into a table (the renderer's reading of a skinned material).
inline void livery_table_of( const livery_material_t& m, scene_table_t& table )
{
    table = scene_table_t{ };
    const std::uint8_t* d = m.desc.data( );
    const auto text = [ ]( const std::uint8_t* at, std::size_t room )
    {
        std::size_t n = 0;
        while ( n < room && at[ n ] != 0 )
            ++n;
        return std::string( reinterpret_cast< const char* >( at ), n );
    };
    for ( std::uint32_t k = 0; k < livery_u32( d + 0x340 ) && k < 16; ++k )
    {
        const std::uint8_t* record = d + 52 * static_cast< std::size_t >( k );
        scene_constant_t c;
        c.name = text( record, 32 );
        c.type = livery_u32( record + 0x24 );
        std::memcpy( c.value, record + 0x28, 12 );
        table.constants.push_back( c );
    }
    for ( std::uint32_t k = 0; k < livery_u32( d + 0x6C4 ) && k < 8; ++k )
    {
        const std::uint8_t* record = d + 0x344 + 112 * static_cast< std::size_t >( k );
        scene_texture_t t;
        t.name = text( record, 32 );
        t.slot = text( record + 0x20, 8 );
        t.value = livery_u32( record + 0x2C );
        t.asset = text( record + 0x30, 64 );
        table.textures.push_back( t );
    }
}

// OURS - the renderer's reading of a skinned material (flagged; retail hands the whole desc to its pipeline state):
// what the skin changed for our draw, field by field against the unskinned material, read by load_model_scene's
// rule - the first "map1" texture's asset, the "EnvironmentTexture" asset (the cube), DiffuseColor / SpecularColor
// (type 2) and SpecularAmount / EnvironmentAmount (type 1). A field the skin left alone keeps the model's own.
struct livery_draw_t
{
    bool texture_set = false, cube_set = false, diffuse_set = false, specular_set = false, specular_amount_set = false, environment_amount_set = false;
    std::string texture, cube;
    float diffuse[ 3 ]{}, specular[ 3 ]{};
    float specular_amount = 0.0f, environment_amount = 0.0f;
};

inline livery_draw_t livery_draw_of( const livery_material_t& before, const livery_material_t& after )
{
    scene_table_t a, b;
    livery_table_of( before, a );
    livery_table_of( after, b );
    livery_draw_t d;
    const auto map1 = [ ]( const scene_table_t& t, std::string& out )
    {
        for ( const scene_texture_t& x : t.textures )
            if ( x.slot == "map1" )
            {
                out = x.asset;
                return true;
            }
        return false;
    };
    const auto named = [ ]( const scene_table_t& t, const char* name, std::string& out )
    {
        for ( const scene_texture_t& x : t.textures )
            if ( x.name == name )
                out = x.asset;
        return true;
    };
    std::string ta, tb;
    if ( map1( a, ta ) && map1( b, tb ) && ta != tb )
    {
        d.texture_set = true;
        d.texture = tb;
    }
    std::string ca, cb;
    named( a, "EnvironmentTexture", ca );
    named( b, "EnvironmentTexture", cb );
    if ( ca != cb )
    {
        d.cube_set = true;
        d.cube = cb;
    }
    for ( std::size_t k = 0; k < a.constants.size( ) && k < b.constants.size( ); ++k )
    {
        const scene_constant_t& x = a.constants[ k ];
        const scene_constant_t& y = b.constants[ k ];
        if ( std::memcmp( x.value, y.value, 12 ) == 0 )
            continue;
        float v[ 3 ];
        std::memcpy( v, y.value, 12 );
        if ( y.name == "DiffuseColor" && y.type == 2 )
        {
            d.diffuse_set = true;
            std::memcpy( d.diffuse, v, 12 );
        }
        else if ( y.name == "SpecularColor" && y.type == 2 )
        {
            d.specular_set = true;
            std::memcpy( d.specular, v, 12 );
        }
        else if ( y.name == "SpecularAmount" && y.type == 1 )
        {
            d.specular_amount_set = true;
            d.specular_amount = v[ 0 ];
        }
        else if ( y.name == "EnvironmentAmount" && y.type == 1 )
        {
            d.environment_amount_set = true;
            d.environment_amount = v[ 0 ];
        }
    }
    return d;
}