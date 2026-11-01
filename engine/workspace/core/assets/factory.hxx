#pragma once

#include <workspace/core/assets/package.hxx>

#include <cstdint>
#include <map>
#include <memory>
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
// createAsset past the cache is recreated since boat batch 25 (package.hxx + below, oracle case asset_package): the
// packages +0x48 (loadPackage 0x4EFA50 -> 0x4F0090), the AssetData entry and its used byte, skus 0x4F1CF0 / langs
// 0x55FC50, the pack read + version check 0x4F08F0, later packages' patches 0x4F1B50, newly loaded packages' patches
// of loaded assets 0x4F05D0, the per-type name vectors 0x4F0710. The caller's (asset_factory_hooks_t): the record's
// create function, the asset's own load (vt+0x14) and patch (vt+8), the listeners, the platform file system (where
// <name>.apf lives), the frame sync, the delete. NOT recreated (`unknown` is set, the answer is false / 0): the loose
// Assets loader 0x4EFBB0 (no <name>.apf), 0x4F08F0's path for an asset its pack does not list (0x4F0EF0 / 0x4F1330 /
// the bake: the dev tree's raw files). With no package loaded createAsset answers 0 for every name the table does not
// hold, as retail does.
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
    // +0x20 the language (0x55FC50), +0x38 the AssetData info (0x55FC50's copy of 0x4F1CF0's pick), +0x50 the pack
    // record's hash once the load succeeded (0x4F08F0).
    std::string lang;
    value_t info;
    std::uint32_t data_hash = 0;
};

// The record's create, the asset's own virtuals, the listeners and the platform calls: the caller's.
struct asset_factory_hooks_t
{
    void* user = nullptr;
    // NO LONGER CALLED (boat batch 25: the package walk is recreated). Kept for source compatibility.
    bool ( *listed )( void* user, const std::string& type, const std::string& name ) = nullptr;
    // The type record's create function (+0xC), called with no arguments; answers a new asset, refs 1.
    vu_asset_t* ( *create )( void* user, const std::string& type ) = nullptr;
    // NO LONGER CALLED (batch 25): load_data is the asset's load now.
    void ( *load )( void* user, vu_asset_t& asset ) = nullptr;
    // The +0x88 listeners' vt+4(asset).
    void ( *created )( void* user, vu_asset_t& asset ) = nullptr;
    // release's / createAsset's frame sync 0x53A400 when g_vugfxsort +0xC is set (VuMetroThread vt+0x3C, D3D11
    // vt+0x30 / +0xD0): platform.
    void ( *sync )( void* user ) = nullptr;
    // The asset's vt+4(1), the scalar deleting dtor.
    void ( *destroy )( void* user, vu_asset_t* asset ) = nullptr;
    // The asset's vt+0x14 load( { data, size } ) on the pack record's bytes: its answer.
    bool ( *load_data )( void* user, vu_asset_t& asset, const std::vector< std::uint8_t >& data ) = nullptr;
    // The asset's vt+8 patch( loaded by a later package ): its answer.
    bool ( *patch )( void* user, vu_asset_t& asset, vu_asset_t& from ) = nullptr;
    // The +0x88 listeners' vt+0( package name ), after a package load succeeded.
    void ( *package_loaded )( void* user, const std::string& name ) = nullptr;
    // The platform file system (g_vumetrofile vt+0x28 exists, then the open the pack makes): the path to open for
    // a file name such as "Base.apf", or "" when there is none.
    std::string ( *pack_path )( void* user, const std::string& file ) = nullptr;
    // The self-test's header variants: the 68 bytes served for the pack's header, or null for the file's own.
    const unsigned char* pack_header = nullptr;
};

class c_asset_factory
{
public:
    // The +0x40 map's value: +0 the asset names every package lists for the type (slot 12's answer, filled by
    // 0x4F0710), +0xC create, +0x10 bake, +0x14 version, +0x18 flag.
    struct record_t
    {
        std::vector< std::string > names;
        std::uint32_t create = 0;
        std::uint32_t bake = 0;
        std::uint32_t version = 0;
        bool flag = false;
    };

    // A path past what is recreated was reached (the loose loader 0x4EFBB0, 0x4F08F0's unlisted-asset path, a pack
    // path archive.hxx flags): the answer given was false / 0, retail's is not known.
    bool unknown = false;

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
    // the key hash through slot 22: a loaded asset gets +0x4C += 1 and is answered. Else the miss path: the first
    // package (+0x48 order) listing type / name (none -> 0) has the entry's used byte set; the type's record (none,
    // or its create null -> 0, the byte stays set); the frame sync 0x53A400; the sku pick 0x4F1CF0( "Metro", +4 sku,
    // entry ); create; +8 = the name, +0x48 = the hash; the language pick 0x55FC50( info, +0x1C language ); the load
    // 0x4F08F0 (its answer ignored: a failed load is cached too); each LATER package listing it patches it (0x4F1B50;
    // their used bytes are not set); the table insert (operator[] 0x4F3850 of the hash); the listeners.
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
        for ( std::size_t k = 0; k < m_packages.size( ); ++k )
        {
            asset_entry_t* e = m_packages[ k ]->entry( type, name );
            if ( e == nullptr )
                continue;
            e->used = true;
            const record_t* r = record( type );
            if ( r == nullptr || r->create == 0 )
                return nullptr;
            if ( hooks.sync != nullptr )
                hooks.sync( hooks.user );
            const value_t& info = asset_pick_sku( "Metro", m_sku, e->data );
            if ( hooks.create == nullptr )
            {
                unknown = true;
                return nullptr;
            }
            vu_asset_t* asset = hooks.create( hooks.user, type );
            if ( asset == nullptr )
                return nullptr;
            asset->type = type;
            asset->name = name;
            asset->hash = h;
            asset_pick_language( asset->info, asset->lang, info, m_lang );
            load_from( *r, *m_packages[ k ], *asset );
            for ( std::size_t j = k + 1; j < m_packages.size( ); ++j )
            {
                if ( asset_entry_t* later = m_packages[ j ]->entry( type, name ) )
                    patch_from( *r, *m_packages[ j ], *later, *asset );
            }
            m_loaded[ h ] = asset;
            if ( hooks.created != nullptr )
                hooks.created( hooks.user, *asset );
            return asset;
        }
        return nullptr;
    }

    // +4 the sku (0x4ED410: the init argument, then the "Base" package's +0x18), +0x1C the language (0x4ED410: the
    // platform's g_vumetrosys vt+0x48, or VuDevConfig "Language").
    void set_sku( const std::string& sku )
    {
        m_sku = sku;
    }

    void set_language( const std::string& language )
    {
        m_lang = language;
    }

    const std::string& sku( ) const
    {
        return m_sku;
    }

    // 0x4F0470: the loaded package of the name, or null.
    asset_package_t* package( const std::string& name ) const
    {
        for ( const auto& p : m_packages )
        {
            if ( p->name == name )
                return p.get( );
        }
        return nullptr;
    }

    std::size_t package_count( ) const
    {
        return m_packages.size( );
    }

    const asset_package_t& package_at( std::size_t index ) const
    {
        return *m_packages[ index ];
    }

    const std::map< std::string, record_t >& records( ) const
    {
        return m_types;
    }

    // loadPackage 0x4EFA50. Already loaded (0x4F0470) -> true. (VuDevConfig "IgnoreAssetDBs": the dev config is the
    // null container in retail - batch 24 - so that early true is never taken.) <name>.apf through the platform file
    // system -> 0x4F0090; none -> the loose Assets loader 0x4EFBB0, NOT recreated (unknown, false). A load that
    // succeeds is announced to the +0x88 listeners (vt+0, the name).
    bool load_package( const std::string& name )
    {
        if ( package( name ) != nullptr )
            return true;
        const std::string path = hooks.pack_path != nullptr ? hooks.pack_path( hooks.user, name + ".apf" ) : std::string( );
        bool ok = false;
        if ( path.empty( ) )
            unknown = true;
        else
            ok = load_package_file( name, path );
        if ( ok && hooks.package_loaded != nullptr )
            hooks.package_loaded( hooks.user, name );
        return ok;
    }

    // 0x4ED410's package part: loadPackage( "Base" ) (false -> false), then +4 = that package's sku.
    bool load_base_package( )
    {
        if ( !load_package( "Base" ) )
            return false;
        m_sku = package( "Base" )->sku;
        return true;
    }

    // Put an asset in the table as createAsset's miss path does (the check seeds the table this way).
    void insert( vu_asset_t* asset )
    {
        m_loaded[ asset->hash ] = asset;
    }

private:
    // 0x4F0090 ( name ): a new package (0x4ECC90: words -1, -1, 0); its pack opened (0x567760; false -> the package
    // deleted, false); +0x18 = the pack's sku, +0x30 / +0x34 = its two words; Assets/AssetData read (0x5679F0): 1 ->
    // the document (0x55B560 on the bytes as read; a failed read -> false) and per root member, in key order, 0x4F1DC0
    // (false -> false); 2 -> false; 0 (not listed) -> the package stays, empty. Then, when assets are loaded (+0x58,
    // the table's size), 0x4F05D0 patches them from the new package; the package goes on +0x48; 0x4F0710 adds its
    // names to the records.
    bool load_package_file( const std::string& name, const std::string& path )
    {
        auto p = std::make_unique< asset_package_t >( );
        p->name = name;
        if ( !p->pack.open_pack( path.c_str( ), hooks.pack_header ) )
        {
            unknown = unknown || p->pack.pack_unknown( );
            return false;
        }
        p->sku = p->pack.pack_sku( );
        p->words[ 0 ] = p->pack.pack_word( 0 );
        p->words[ 1 ] = p->pack.pack_word( 1 );
        std::vector< std::uint8_t > blob;
        std::uint32_t version = 0, hash = 0;
        const int read = p->pack.read_asset( "Assets", "AssetData", "", version, hash, blob );
        unknown = unknown || p->pack.pack_unknown( );
        if ( read == 2 )
            return false;
        if ( read == 1 )
        {
            value_t root;
            if ( !c_reader::read_at_start( blob, root ) )
                return false;
            for ( const auto* member : asset_sorted_members( root ) )
            {
                if ( !asset_package_load_type( *p, member->first, member->second ) )
                    return false;
            }
        }
        if ( !m_loaded.empty( ) )
            patch_loaded( *p );
        m_packages.push_back( std::move( p ) );
        add_names( *m_packages.back( ) );
        return true;
    }

    // 0x4F0710: for each registered type the package lists, its names pushed onto the record's +0 vector, then
    // sorted and made unique (0x4F38D0 introsort, 0x4F3A00 unique, the tail erased). SUBSTITUTION: std::sort /
    // std::unique - the same result (distinct strings, total order; unique keeps one of each).
    void add_names( const asset_package_t& p )
    {
        for ( auto& [ type, r ] : m_types )
        {
            auto t = p.assets.find( type );
            if ( t == p.assets.end( ) )
                continue;
            for ( const auto& n : t->second )
                r.names.push_back( n.first );
            std::sort( r.names.begin( ), r.names.end( ) );
            r.names.erase( std::unique( r.names.begin( ), r.names.end( ) ), r.names.end( ) );
        }
    }

    // 0x4F05D0: each type / name of the new package that is loaded (slot 23) is patched from it (0x4F1B50).
    void patch_loaded( asset_package_t& p )
    {
        for ( auto& [ type, names ] : p.assets )
        {
            const record_t* r = record( type );
            for ( auto& [ n, e ] : names )
            {
                vu_asset_t* asset = find( type.c_str( ), n.c_str( ) );
                if ( asset == nullptr )
                    continue;
                if ( r == nullptr )
                {
                    // 0x4F1B50 would call through a null record: not modelled.
                    unknown = true;
                    continue;
                }
                patch_from( *r, p, e, *asset );
            }
        }
    }

    // 0x4F1B50 ( record, package, entry, asset ): a second asset from the record's create, with the asset's name and
    // +0x48 hash, the sku pick ( "Metro", +4, entry ) and the language pick ( +0x1C ), loaded from this package
    // (0x4F08F0); a failed load deletes it, false. Else the asset's vt+8 patch( it ), then it is deleted either way;
    // the patch's answer.
    bool patch_from( const record_t& r, asset_package_t& p, asset_entry_t& e, vu_asset_t& asset )
    {
        if ( r.create == 0 || hooks.create == nullptr )
        {
            unknown = true;
            return false;
        }
        const value_t& info = asset_pick_sku( "Metro", m_sku, e.data );
        vu_asset_t* from = hooks.create( hooks.user, asset.type );
        if ( from == nullptr )
            return false;
        from->type = asset.type;
        from->name = asset.name;
        from->hash = asset.hash;
        asset_pick_language( from->info, from->lang, info, m_lang );
        bool ok = load_from( r, p, *from );
        if ( ok )
            ok = hooks.patch != nullptr && hooks.patch( hooks.user, asset, *from );
        if ( hooks.destroy != nullptr )
            hooks.destroy( hooks.user, from );
        return ok;
    }

    // 0x4F08F0 ( record, package, asset ): the pack read of "<type>/<name><lang>" (0x5679F0). 1: the record's version
    // must equal the pack record's, then the asset's vt+0x14 load( data ); true -> +0x50 = the pack hash, true.
    // Otherwise false. 2 -> false. 0 (the pack does not list it): the raw-file / bake path, NOT recreated.
    bool load_from( const record_t& r, asset_package_t& p, vu_asset_t& asset )
    {
        std::vector< std::uint8_t > data;
        std::uint32_t version = 0, hash = 0;
        const int read = p.pack.read_asset( asset.type, asset.name, asset.lang, version, hash, data );
        unknown = unknown || p.pack.pack_unknown( );
        if ( read == 0 )
        {
            unknown = true;
            return false;
        }
        if ( read != 1 || version != r.version )
            return false;
        if ( hooks.load_data == nullptr || !hooks.load_data( hooks.user, asset, data ) )
            return false;
        asset.data_hash = hash;
        return true;
    }

    std::string m_sku;
    std::string m_lang;
    std::vector< std::unique_ptr< asset_package_t > > m_packages;
    std::vector< std::string > m_type_names;
    std::map< std::string, record_t > m_types;
    std::unordered_map< std::uint32_t, vu_asset_t* > m_loaded;
    std::vector< std::pair< std::uint32_t, std::string > > m_substitutes;
};
