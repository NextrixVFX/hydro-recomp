#pragma once

// The game-start "Trigger" handlers of the race's settings entities (Hydroelectric's "Actions/Global settings" and
// "Lights" folders fire them once from their VuOnGameStart / OnProjectInitialized nodes). Each is a VuScriptInputPlug
// handler (thiscall entity; VuRetVal*, const VuParams&; retn 8) answering the empty VuRetVal (type word 0).
// Verified against retail: hydro checks/logic.hxx verify_settings_triggers (case settings_triggers).
//   VuAmbientLightEntity "Trigger" 0x5EF780 (plug helper 0x5EF7A0, VuMethod1 0x756584).
//   VuDirectionalLightEntity "Trigger" 0x5EEEC0 = its "Use" notify 0x5EEEE0 (plug helper 0x5EF460).
//   VuSubstituteAssetEntity "Trigger" 0x5F4BD0 (plug helper 0x5F4DF0, VuMethod1 0x7570B4).
//   VuWaterTextureSettingsEntity "Trigger" 0x5D2B00 = world/watchers.hxx water_texture_settings_apply (verified there).
// The light manager g_vulightmanager 0x759094 is the renderer's (its consumer of +0xCC .. +0xF8 is not recreated): the
// handlers write its words, given here as `light_manager_t` - the platform's light setup reads them.

#include <cstdint>
#include <cstring>

// g_vulightmanager's words +0xCC .. +0xFB.
struct light_manager_t
{
    std::uint32_t ambient = 0;              // +0xCC: the ambient colour (VuColor)
    std::uint32_t position[ 4 ] = { };      // +0xD0: the directional light's position (its transform +0x80 .. +0x8C)
    std::uint32_t direction[ 4 ] = { };     // +0xE0: -(transform +0x70 .. +0x78) (the matrix's row 2 negated); +0xEC UNKNOWN
    std::uint32_t front = 0, back = 0, specular = 0;   // +0xF0 / +0xF4 / +0xF8
};

// VuAmbientLightEntity "Trigger" 0x5EF780: +0xCC = Color (+0x60).
inline std::uint32_t ambient_light_trigger( std::uint32_t color, light_manager_t& m )
{
    m.ambient = color;
    return 0u;
}

// VuDirectionalLightEntity "Trigger" 0x5EEEC0 -> 0x5EEEE0 ("Use"'s notify): +0xD0 = the transform's +0x80 .. +0x8C
// (two movq), +0xE0 = (-+0x70, -+0x74, -+0x78) (xorps of the sign bit) and the fourth lane a stack word never written
// (UNKNOWN: left as it was here), +0xF0 / +0xF4 / +0xF8 = Front / Back / Specular Color (+0x6C / +0x70 / +0x74).
// `transform` = the transform component's +0x50 words (16; +0x70 = [8], +0x80 = [12]).
inline std::uint32_t directional_light_trigger( const std::uint32_t* transform, std::uint32_t front, std::uint32_t back,
                                                std::uint32_t specular, light_manager_t& m )
{
    std::memcpy( m.position, transform + 12, 16 );
    for ( int k = 0; k < 3; ++k )
        m.direction[ k ] = transform[ 8 + k ] ^ 0x80000000u;
    m.front = front;
    m.back = back;
    m.specular = specular;
    return 0u;
}

// VuSubstituteAssetEntity "Trigger" 0x5F4BD0: the "Asset Name" property (+0xA8) and the "Subst Asset Name" property
// (+0xAC) each hold their resolved asset at +0x64 (the asset manager's - a hook); when both exist and differ, the
// asset's vt+0x10 (substitute) is called with the other. `substitute` = that asset slot (the asset manager's).
inline std::uint32_t substitute_asset_trigger( const void* asset, const void* subst, void* user,
                                               void ( *substitute )( void* user, const void* asset, const void* with ) )
{
    if ( asset && subst && asset != subst && substitute )
        substitute( user, asset, subst );
    return 0u;
}
