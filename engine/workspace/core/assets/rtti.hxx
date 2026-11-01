#pragma once

#include <cstdint>

// VuRTTI of the asset classes (boat batch 23, the asset loaders). Each asset
// class's main vtable slot 0 is a 6-byte getType (mov eax, descriptor; ret)
// answering a static VuRTTI descriptor (.data, statically initialised: +0 the
// name, +4 the parent's descriptor, 0 at VuAsset's 0x7369C4). The tables are
// read from the image and checked by running every getter (oracle case
// asset_rtti). Other areas hold four asset getters already: VuCollisionMesh
// Asset 0x518D50 (collision/leaves.hxx), VuPfxAsset 0x5CAA20 (pfxclass.hxx),
// VuFluidsMeshAsset 0x5226C0 / VuWaterMapAsset 0x5C7CF0 (water surface.hxx).

struct asset_rtti_t
{
    std::uint32_t descriptor;
    std::uint32_t parent;
    const char* name;
};

inline constexpr asset_rtti_t g_asset_rtti[] = {
    { 0x7369C4u, 0x000000u, "VuAsset" },
    { 0x736B20u, 0x7369C4u, "VuGenericDataAsset" },
    { 0x736E40u, 0x7369C4u, "VuGenericAsset" },
    { 0x7368A0u, 0x7369C4u, "VuAnimationAsset" },
    { 0x73699Cu, 0x736B20u, "VuDBAsset" },
    { 0x7369DCu, 0x736B20u, "VuAssetListAsset" },
    { 0x736978u, 0x7369C4u, "VuProjectAsset" },
    { 0x736AD8u, 0x7369C4u, "VuAnimatedModelAsset" },
    { 0x736B68u, 0x7369C4u, "VuAudioBankAsset" },
    { 0x736B60u, 0x7369C4u, "VuAudioProjectAsset" },
    { 0x736B28u, 0x736E40u, "VuAudioStreamAsset" },
    { 0x736938u, 0x7369C4u, "VuCompiledShaderAsset" },
    { 0x736B00u, 0x7369C4u, "VuCubeTextureAsset" },
    { 0x736B58u, 0x7369C4u, "VuFontAsset" },
    { 0x736B48u, 0x736B20u, "VuShaderAsset" },
    { 0x736940u, 0x7369C4u, "VuStaticModelAsset" },
    { 0x736AF8u, 0x736B20u, "VuStringAsset" },
    { 0x736B18u, 0x736B20u, "VuTemplateAsset" },
    { 0x736930u, 0x7369C4u, "VuTextureAsset" },
    { 0x736B40u, 0x7369C4u, "VuTimedEventAsset" },
    { 0x736B30u, 0x7369C4u, "VuXmlAsset" },
    // VuSubstituteAssetEntity is an entity: its parent is VuEntity's 0x7367F8 (boat rtti.hxx), whose parent is 0.
    { 0x736E00u, 0x7367F8u, "VuSubstituteAssetEntity" },
    { 0x7367F8u, 0x000000u, "VuEntity" },
};

// The main vtables, the getType slot (0; the entity's 1), its address and the
// descriptor it answers.
struct asset_rtti_class_t
{
    std::uint32_t vtable;
    std::uint32_t slot;
    std::uint32_t get_type;
    std::uint32_t descriptor;
};

inline constexpr asset_rtti_class_t g_asset_rtti_classes[] = {
    { 0x74CA08u, 0, 0x515CF0u, 0x7369C4u },   // VuAsset
    { 0x74EDCCu, 0, 0x55C1A0u, 0x736B20u },   // VuGenericDataAsset
    { 0x74C9E8u, 0, 0x515DA0u, 0x7368A0u },   // VuAnimationAsset
    { 0x74EDECu, 0, 0x55C220u, 0x73699Cu },   // VuDBAsset
    { 0x74F194u, 0, 0x5644D0u, 0x7369DCu },   // VuAssetListAsset
    { 0x74EB64u, 0, 0x554E00u, 0x736978u },   // VuProjectAsset
    { 0x750C7Cu, 0, 0x5A8090u, 0x736AD8u },   // VuAnimatedModelAsset
    { 0x752610u, 0, 0x5CB350u, 0x736B68u },   // VuAudioBankAsset
    { 0x7525D0u, 0, 0x5CAF40u, 0x736B60u },   // VuAudioProjectAsset
    { 0x751F18u, 0, 0x5C3810u, 0x736B28u },   // VuAudioStreamAsset
    { 0x74D72Cu, 0, 0x53FE20u, 0x736938u },   // VuCompiledShaderAsset
    { 0x7513ACu, 0, 0x5ADE60u, 0x736B00u },   // VuCubeTextureAsset
    { 0x75258Cu, 0, 0x5CABC0u, 0x736B58u },   // VuFontAsset
    { 0x751F38u, 0, 0x5C37A0u, 0x736E40u },   // VuGenericAsset
    { 0x752514u, 0, 0x5CA9E0u, 0x736B48u },   // VuShaderAsset
    { 0x74D918u, 0, 0x542900u, 0x736940u },   // VuStaticModelAsset
    { 0x750D38u, 0, 0x5A97F0u, 0x736AF8u },   // VuStringAsset
    { 0x751DF0u, 0, 0x5BB0D0u, 0x736B18u },   // VuTemplateAsset
    { 0x74D550u, 0, 0x539690u, 0x736930u },   // VuTextureAsset
    { 0x7524D8u, 0, 0x5C8BA0u, 0x736B40u },   // VuTimedEventAsset
    { 0x752458u, 0, 0x5C7840u, 0x736B30u },   // VuXmlAsset
    { 0x7570C4u, 1, 0x5F4790u, 0x736E00u },   // VuSubstituteAssetEntity
};

inline const asset_rtti_t* asset_rtti_find( std::uint32_t descriptor )
{
    for ( const asset_rtti_t& r : g_asset_rtti )
        if ( r.descriptor == descriptor )
            return &r;
    return nullptr;
}
