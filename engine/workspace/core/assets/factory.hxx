#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// VuAssetFactoryImpl's core (boat batch 24, the asset loaders): g_vuassetfactoryimpl 0x759004, 0xA8 bytes, vtable
// 0x74BDCC, built by 0x4ECFD0 (new 0xA8, ctors 0x4ED060 / 0x4ED0F0). What is recreated here and checked against
// retail run on a factory retail built itself (oracle case asset_factory):
//   - the type registry: registerType slot 6 0x4ED9E0 and VuAssetFactory_registerAllTypes 0x56B730's 22 types;
//   - the asset key hash 0x4ECB80;
//   - the loaded-asset table +0x54: createAsset slot 7 0x4EDA20 up to its package walk, addRef slot 8 0x4EDE60,
//     release slot 9 0x4EDE70, findAsset slot 22 0x4EF4D0 (hash) / slot 23 0x4EF4B0 (type, name);
//   - the substitutes +0x9C: lookup 0x4F1C90, addSubstitute slot 42 0x4EF8E0, removeSubstitute slot 43 0x4EF9A0.
// createAsset past the cache (the packages +0x48, the AssetData entry, skus / langs, the record's create function,
// the pack read 0x4F08F0, later packages' patches 0x4F1B50, the listeners +0x88) is NOT recreated: it runs through
// asset_factory_hooks_t, the caller's. With no hooks createAsset answers what retail answers with no package loaded
// (0 for every name the table does not hold) - the check runs exactly that.
// SUBSTITUTIONS (flagged): the +0x40 type map is std::map< std::string, ... > (retail's is the same std::map: find
// 0x4F22B0, operator[] 0x4F23C0); the +0x54 table is std::unordered_map< std::uint32_t, vu_asset_t* > (retail's is
// MSVC's _Hash, list + bucket vector, mask 7 / maxidx 8 after construction - its iteration order, which only slots
// 25 / 37 walk, is not modelled); the +0x9C substitutes are a std::vector in insertion order (retail: 28-byte
// { hash, std::string } records, push_back 0x4F27F0, erase-by-shift 0x4F4470).

// 0x4ECB80, fastcall (ecx type, edx name): FNV-1a 32 over the type's bytes, then the name's, no separator.
inline std::uint32_t asset_key_hash( const char* type, const char* name )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( const unsigned char* p = reinterpret_cast< const unsigned char* >( type ); *p != 0; ++p )
        h = ( h ^ *p ) * 16777619u;
    for ( const unsigned char* p = reinterpret_cast< const unsigned char* >( name ); *p != 0; ++p )
        h = ( h ^ *p ) * 16777619u;
    return h;
}

// One registerType call of VuAssetFactory_registerAllTypes 0x56B730, in its order: the type name, the create
// function (record +0xC), the second function (+0x10: the bake - called only by slot 17 0x4EE3B0, which nothing
// calls), the version (+0x14) and the flag byte (+0x18).
struct asset_type_t
{
    const char* name;
    std::uint32_t create;
    std::uint32_t bake;
    std::uint32_t version;
    bool flag;
};

inline constexpr asset_type_t g_asset_types[] = {
    { "VuAnimatedModelAsset", 0x5A81B0u, 0x5A81E0u, 15u, true },
    { "VuAnimationAsset", 0x515E90u, 0x515EB0u, 1u, true },
    { "VuAssetListAsset", 0x5644E0u, 0x55C330u, 1u, true },
    { "VuAudioBankAsset", 0x5CB3F0u, 0x5CB420u, 1u, false },
    { "VuAudioProjectAsset", 0x5CAFD0u, 0x5CAFF0u, 1u, true },
    { "VuAudioStreamAsset", 0x5C3820u, 0x5C3900u, 1u, false },
    { "VuCollisionMeshAsset", 0x518EC0u, 0x518F50u, 7u, true },
    { "VuCompiledShaderAsset", 0x53FEB0u, 0x53FF30u, 2u, true },
    { "VuCubeTextureAsset", 0x5ADF50u, 0x5ADF70u, 13u, true },
    { "VuDBAsset", 0x55C280u, 0x55C330u, 1u, true },
    { "VuFluidsMeshAsset", 0x522720u, 0x522760u, 2u, true },
    { "VuFontAsset", 0x5CACC0u, 0x5CACE0u, 1u, true },
    { "VuPfxAsset", 0x5CAB00u, 0x55C330u, 1u, true },
    { "VuProjectAsset", 0x554ED0u, 0x554EF0u, 1u, true },
    { "VuShaderAsset", 0x5CA9F0u, 0x55C330u, 1u, true },
    { "VuStaticModelAsset", 0x5429F0u, 0x542A10u, 17u, true },
    { "VuStringAsset", 0x5A9800u, 0x55C330u, 1u, true },
    { "VuTemplateAsset", 0x5BB0E0u, 0x55C330u, 1u, true },
    { "VuTextureAsset", 0x539790u, 0x5397B0u, 18u, true },
    { "VuTimedEventAsset", 0x5C8CD0u, 0x5C8D50u, 1u, true },
    { "VuWaterMapAsset", 0x5C7D60u, 0x5C7DA0u, 2u, true },
    { "VuXmlAsset", 0x5C7920u, 0x5C7A70u, 1u, true },
};

// The factory-side words of a VuAsset (the base ctor, inlined e.g. in 0x515DB0): +8 the name, +0x48 the key hash
// (createAsset's miss path writes it), +0x4C the reference count (the ctor's 1). `object` is the caller's asset.
struct vu_asset_t
{
    std::string type;
    std::string name;
    std::uint32_t hash = 0;
    std::int32_t refs = 1;
    void* object = nullptr;
};

// createAsset past the cache, release's two platform calls and the delete: the caller's.
struct asset_factory_hooks_t
{
    void* user = nullptr;
    // The package walk (+0x48 vector, each package's +60 type -> name -> entry; a hit sets the entry's +56 byte).
    bool ( *listed )( void* user, const std::string& type, const std::string& name ) = nullptr;
    // The type record's create function (+0xC), called with no arguments; answers a new asset, refs 1.
    vu_asset_t* ( *create )( void* user, const std::string& type ) = nullptr;
    // 0x4F08F0 (the pack read + the asset's own load) and 0x4F1B50 per later package; retail ignores the answer.
    void ( *load )( void* user, vu_asset_t& asset ) = nullptr;
    // The +0x88 listeners' vt+4(asset).
    void ( *created )( void* user, vu_asset_t& asset ) = nullptr;
    // release's frame sync when g_vugfxsort +0xC is set (VuMetroThread vt+0x3C, D3D11 vt+0x30 / +0xD0): platform.
    void ( *sync )( void* user ) = nullptr;
    // The asset's vt+4(1), the scalar deleting dtor.
    void ( *destroy )( void* user, vu_asset_t* asset ) = nullptr;
};

class c_asset_factory
{
public:
    // The +0x40 map's value: +0xC create, +0x10 bake, +0x14 version, +0x18 flag (its +0 vector of asset names,
    // slot 12's answer, is filled by the package loads - not here).
    struct record_t
    {
        std::uint32_t create = 0;
        std::uint32_t bake = 0;
        std::uint32_t version = 0;
        bool flag = false;
    };

    asset_factory_hooks_t hooks{};

    // registerType, slot 6 0x4ED9E0: push the name onto +0x34 (0x431390, a std::vector< std::string > push_back),
    // then +0x40[ name ] (operator[] 0x4F23C0) gets the four fields - a second registration of a name overwrites
    // the record and lists the name twice.
    void register_type( const char* name, std::uint32_t create, std::uint32_t bake, std::uint32_t version, bool flag )
    {
        m_type_names.emplace_back( name );
        record_t& r = m_types[ name ];
        r.create = create;
        r.bake = bake;
        r.version = version;
        r.flag = flag;
    }

    // VuAssetFactory_registerAllTypes 0x56B730 (called by init slot 5 0x4ED410).
    void register_all_types( )
    {
        for ( const asset_type_t& t : g_asset_types )
            register_type( t.name, t.create, t.bake, t.version, t.flag );
    }

    // getAssetTypes, slot 11 0x4EDF30 (= +0x34).
    const std::vector< std::string >& types( ) const
    {
        return m_type_names;
    }

    const record_t* record( const std::string& type ) const
    {
        auto it = m_types.find( type );
        return it == m_types.end( ) ? nullptr : &it->second;
    }

    // findAsset( hash ), slot 22 0x4EF4D0: the +0x54 table's value, else 0.
    vu_asset_t* find( std::uint32_t hash ) const
    {
        auto it = m_loaded.find( hash );
        return it == m_loaded.end( ) ? nullptr : it->second;
    }

    // findAsset( type, name ), slot 23 0x4EF4B0 (char* type, char* name): slot 22 of the key hash.
    vu_asset_t* find( const char* type, const char* name ) const
    {
        return find( asset_key_hash( type, name ) );
    }

    // addRef, slot 8 0x4EDE60: +0x4C += 1, answers the asset.
    vu_asset_t* add_ref( vu_asset_t* asset )
    {
        ++asset->refs;
        return asset;
    }

    // release, slot 9 0x4EDE70. Null does nothing. Else the frame sync (hook), then +0x4C -= 1; at 0 the table entry
    // of the asset's OWN +0x48 hash is erased when there is one, and the asset is deleted (vt+4(1)) either way.
    void release( vu_asset_t* asset )
    {
        if ( asset == nullptr )
            return;
        if ( hooks.sync != nullptr )
            hooks.sync( hooks.user );
        if ( --asset->refs != 0 )
            return;
        auto it = m_loaded.find( asset->hash );
        if ( it != m_loaded.end( ) )
            m_loaded.erase( it );
        if ( hooks.destroy != nullptr )
            hooks.destroy( hooks.user, asset );
    }

    // 0x4F1C90: the first +0x9C record whose hash is the key hash copies its substitute name out.
    bool substitute( const std::string& type, const std::string& name, std::string& out ) const
    {
        const std::uint32_t h = asset_key_hash( type.c_str( ), name.c_str( ) );
        for ( const auto& s : m_substitutes )
        {
            if ( s.first == h )
            {
                out = s.second;
                return true;
            }
        }
        return false;
    }

    // addSubstitute, slot 42 0x4EF8E0: removeSubstitute( type, name ) (slot 43, through the vtable), then
    // push_back { key hash, substitute }.
    void add_substitute( const std::string& type, const std::string& name, const std::string& with )
    {
        remove_substitute( type, name );
        m_substitutes.emplace_back( asset_key_hash( type.c_str( ), name.c_str( ) ), with );
    }

    // removeSubstitute, slot 43 0x4EF9A0: the first record of the key hash, erased (the tail shifted down).
    void remove_substitute( const std::string& type, const std::string& name )
    {
        const std::uint32_t h = asset_key_hash( type.c_str( ), name.c_str( ) );
        for ( auto it = m_substitutes.begin( ); it != m_substitutes.end( ); ++it )
        {
            if ( it->first == h )
            {
                m_substitutes.erase( it );
                return;
            }
        }
    }

    const std::vector< std::pair< std::uint32_t, std::string > >& substitutes( ) const
    {
        return m_substitutes;
    }

    // createAsset, slot 7 0x4EDA20. A substitute -> createAsset( type, substitute ) (its answer, nothing else). Else
    // the key hash through slot 22: a loaded asset gets +0x4C += 1 and is answered. Else the miss path: the package
    // walk (hooks.listed; none, or no package lists it -> 0), the type's record (none, or its create null -> 0),
    // create, +8 = the name, +0x48 = the hash, the load (its answer ignored, as retail), the table insert (operator[]
    // 0x4F3850 of the hash), the listeners.
    vu_asset_t* create( const std::string& type, const std::string& name )
    {
        std::string with;
        if ( substitute( type, name, with ) )
            return create( type, with );
        const std::uint32_t h = asset_key_hash( type.c_str( ), name.c_str( ) );
        if ( vu_asset_t* hit = find( h ) )
        {
            ++hit->refs;
            return hit;
        }
        if ( hooks.listed == nullptr || !hooks.listed( hooks.user, type, name ) )
            return nullptr;
        const record_t* r = record( type );
        if ( r == nullptr || r->create == 0 || hooks.create == nullptr )
            return nullptr;
        vu_asset_t* asset = hooks.create( hooks.user, type );
        if ( asset == nullptr )
            return nullptr;
        asset->type = type;
        asset->name = name;
        asset->hash = h;
        if ( hooks.load != nullptr )
            hooks.load( hooks.user, *asset );
        m_loaded[ h ] = asset;
        if ( hooks.created != nullptr )
            hooks.created( hooks.user, *asset );
        return asset;
    }

    // Put an asset in the table as createAsset's miss path does (the check seeds the table this way).
    void insert( vu_asset_t* asset )
    {
        m_loaded[ asset->hash ] = asset;
    }

private:
    std::vector< std::string > m_type_names;
    std::map< std::string, record_t > m_types;
    std::unordered_map< std::uint32_t, vu_asset_t* > m_loaded;
    std::vector< std::pair< std::uint32_t, std::string > > m_substitutes;
};
