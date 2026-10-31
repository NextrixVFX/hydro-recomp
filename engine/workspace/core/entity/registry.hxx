#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// VuEntityRegistry (g_vuentityregistry 0x759088): every entity type a project
// can name, as retail builds it at start-up. 0x5565A0 creates it (ctor
// 0x556620: a vector of 0x4C-byte entries plus a std::map class -> index);
// init 0x556790 runs 0x5685B0 (the engine's types, below) and 0x556AC0 (one
// type per VuTemplateAsset); the game adds its own through 0x4D4180.
//
// A create fn is kept as its retail address: it is a hook until that type's
// constructor is recreated, and a lookup that lands on one is an UNKNOWN
// entity, never a stand-in.

struct entity_type_t
{
    std::string m_class;     // +0x00
    std::string m_category;  // +0x18
    std::string m_display;   // +0x30
    std::uint32_t m_create;  // +0x48
};

struct entity_type_row_t
{
    const char* m_class;
    const char* m_category;
    const char* m_display;
    std::uint32_t m_create;
};

// 0x5567D0, registerType's sort order: an entry with a category before one
// without; then by category; equal categories by class. Both compares are
// std::string's operator< (0x4562C0).
inline bool entity_type_before( const entity_type_t& a, const entity_type_t& b )
{
    if ( !a.m_category.empty( ) && b.m_category.empty( ) )
        return true;
    if ( a.m_category.empty( ) && !b.m_category.empty( ) )
        return false;
    if ( a.m_category != b.m_category )
        return a.m_category < b.m_category;
    return a.m_class < b.m_class;
}

// The template instantiator every "#<name>" type creates through.
constexpr std::uint32_t k_template_create = 0x556E70u;

struct entity_registry_t
{
    std::vector< entity_type_t > m_types;
    std::map< std::string, int > m_index;

    // registerType 0x556880: push the entry (0x557030), sort the whole vector
    // (0x557300 / 0x5567D0), then rebuild the class -> index map from it
    // (0x501700 per entry, so a repeated class keeps its last position).
    // SUBSTITUTION: 0x557300 is MSVC's std::sort (introsort, library code);
    // std::sort with the same order gives the same vector whenever no two
    // entries compare equal - the case asserts the classes are unique.
    void register_type( const std::string& cls, const std::string& category, const std::string& display,
        std::uint32_t create )
    {
        m_types.push_back( { cls, category, display, create } );
        std::sort( m_types.begin( ), m_types.end( ), entity_type_before );
        for ( int index = 0; index < static_cast< int >( m_types.size( ) ); ++index )
            m_index[ m_types[ index ].m_class ] = index;
    }

    void register_rows( const entity_type_row_t* rows, std::size_t count )
    {
        for ( std::size_t row = 0; row < count; ++row )
            register_type( rows[ row ].m_class, rows[ row ].m_category, rows[ row ].m_display, rows[ row ].m_create );
    }

    // 0x556AC0: one type per VuTemplateAsset name, in the order the asset
    // factory's getAssetNames (slot 12 0x4EDF40) lists them. Class "#" + name;
    // the directory (0x564A50: before the LAST '/', else "") becomes
    // "Templates/<dir>", the file (0x564AE0: after it, else the whole name)
    // the display name; a class already registered is skipped.
    void register_templates( const std::vector< std::string >& names )
    {
        for ( const std::string& name : names )
        {
            const std::string cls = "#" + name;
            const std::size_t slash = name.rfind( '/' );
            std::string directory = slash == std::string::npos ? std::string( ) : name.substr( 0, slash );
            if ( !directory.empty( ) )
                directory = "/" + directory;
            const std::string display = slash == std::string::npos ? name : name.substr( slash + 1 );
            if ( m_index.find( cls ) == m_index.end( ) )
                register_type( cls, "Templates" + directory, display, k_template_create );
        }
    }

    // 0x501300's lookup: the create fn for a type name, or 0 when the type is
    // not registered (retail then skips the child).
    std::uint32_t create_for( const std::string& cls ) const
    {
        const auto found = m_index.find( cls );
        return found == m_index.end( ) ? 0u : m_types[ found->second ].m_create;
    }
};

// VuEntityRegistry_registerAllTypes 0x5685B0: its 85 registerType calls in
// retail's order (captured by hydro-tools census/regcalls.py, printed by
// codegen/registry.py engine).
inline const entity_type_row_t g_engine_entity_types[] = {
    { "VuPlayAnimationEntity", "Actions", "PlayAnimation", 0x5F5850u },
    { "VuAttachEntity", "Actions", "Attach", 0x5F4EA0u },
    { "VuSubstituteAssetEntity", "Actions", "SubstituteAsset", 0x5F47A0u },
    { "VuSwapSkyboxEntity", "Actions", "SwapSkybox", 0x5F4400u },
    { "VuAudioEventEntity", "Audio", "AudioEvent", 0x5F3EC0u },
    { "VuAudioEmitterEntity", "Audio", "AudioEmitter", 0x5F3560u },
    { "VuAudioReverbEntity", "Audio", "AudioReverb", 0x5F2B30u },
    { "VuAudioAmbientReverbEntity", "Audio", "AudioAmbientReverb", 0x5F2490u },
    { "VuAudioDspEntity", "Audio", "AudioDsp", 0x5F0DF0u },
    { "VuAudioDuckingEntity", "Audio", "AudioDucking", 0x5F0820u },
    { "VuSceneEntity", "Containers", "Scene", 0x5F0790u },
    { "VuGroupEntity", "Containers", "Group", 0x5F05B0u },
    { "VuOnProjectInitializedEntity", "Events", "OnProjectInitialized", 0x5EF850u },
    { "VuOnKeyboardEventEntity", "Events", "OnKeyboardEvent", 0x5EFB30u },
    { "VuGenericEventEntity", "Events", "GenericEvent", 0x5F0000u },
    { "VuAmbientLightEntity", "Lights", "AmbientLight", 0x5EF510u },
    { "VuDirectionalLightEntity", "Lights", "DirectionalLight", 0x5EEA10u },
    { "VuDynamicLightEntity", "Lights", "DynamicLight", 0x5EDE60u },
    { "VuPropEntity", "Objects", "Prop", 0x546440u },
    { "VuAnimatedPropEntity", "Objects", "AnimatedProp", 0x5EC1F0u },
    { "VuDynamicPropEntity", "Objects", "DynamicProp", 0x5452E0u },
    { "VuSkyBoxEntity", "Objects", "SkyBox", 0x5EB790u },
    { "VuStaticPfxEntity", "Objects", "StaticPfx", 0x5EA940u },
    { "VuOneShotPfxEntity", "Objects", "OneShotPfx", 0x5EA030u },
    { "VuCoronaEntity", "Objects", "Corona", 0x5E8C80u },
    { "VuCoronaOccluderEntity", "Objects", "CoronaOccluder", 0x5E8890u },
    { "VuDirectionalCoronaEntity", "Objects", "CoronaDirectional", 0x5E7530u },
    { "VuFoliageEntity", "Objects", "Foliage", 0x5E68A0u },
    { "VuLensWaterEmitterEntity", "Objects", "LensWaterEmitter", 0x5E64C0u },
    { "VuExplosionEntity", "Objects", "Explosion", 0x5E60D0u },
    { "VuBooleanEntity", "Logic", "Boolean", 0x5E5810u },
    { "VuStringEntity", "Logic", "String", 0x5E51D0u },
    { "VuCounterEntity", "Logic", "Counter", 0x5E4B90u },
    { "VuIfEntity", "Logic", "If", 0x5DDB70u },
    { "VuCompareBooleansEntity", "Logic", "CompareBooleans", 0x5DE060u },
    { "VuCompareIntegersEntity", "Logic", "CompareIntegers", 0x5DE490u },
    { "VuCompareStringsEntity", "Logic", "CompareStrings", 0x5DE950u },
    { "VuPersistentBooleanEntity", "Logic", "PersistentBoolean", 0x5E3F60u },
    { "VuPersistentIntegerEntity", "Logic", "PersistentInteger", 0x5E43C0u },
    { "VuPersistentStringEntity", "Logic", "PersistentString", 0x5E46B0u },
    { "VuSetBooleanEntity", "Logic", "SetBoolean", 0x5E3240u },
    { "VuSetIntegerEntity", "Logic", "SetInteger", 0x5E35C0u },
    { "VuSetStringEntity", "Logic", "SetString", 0x5E39D0u },
    { "VuOneShotFilterEntity", "Logic", "OneShotFilter", 0x5DED00u },
    { "VuSequencerEntity", "Logic", "Sequencer", 0x5DF0E0u },
    { "VuDelayEntity", "Logic", "Delay", 0x5DF7B0u },
    { "VuMultiDelayEntity", "Logic", "MultiDelay", 0x5DFD00u },
    { "VuTriggerArrayEntity", "Logic", "TriggerArray", 0x5E0750u },
    { "VuTriggerForwarderEntity", "Logic", "TriggerForwarder", 0x5E0DB0u },
    { "VuRandomTriggerEntity", "Logic", "RandomTrigger", 0x5E1060u },
    { "VuPlatformFilterEntity", "Logic", "PlatformFilter", 0x5E18A0u },
    { "VuBuildFilterEntity", "Logic", "BuildFilter", 0x5E1D90u },
    { "VuSwitchEntity", "Logic", "Switch", 0x5E2140u },
    { "VuIntegerEntity", "Math", "Integer", 0x5DD6A0u },
    { "VuAddIntegerConstantEntity", "Math", "AddIntegerConstant", 0x5DC8A0u },
    { "VuAddIntegersEntity", "Math", "AddIntegers", 0x5DCC40u },
    { "VuSubtractIntegersEntity", "Math", "SubtractIntegers", 0x5DD070u },
    { "VuWaterSurfaceEntity", "Water", "WaterSurface", 0x5DA460u },
    { "VuDirectionalWaveEntity", "Water", "DrectionalWave", 0x5D96F0u },
    { "VuInfiniteOceanWaveEntity", "Water", "InfiniteOceanWave", 0x5D81C0u },
    { "VuRectangularOceanWaveEntity", "Water", "RectangularOceanWave", 0x5D8260u },
    { "VuCircularOceanWaveEntity", "Water", "CircularOceanWave", 0x5D82F0u },
    { "VuWhirlpoolWaveEntity", "Water", "WhirlpoolWave", 0x5D7930u },
    { "VuWaterRampEntity", "Water", "WaterRamp", 0x5D6C30u },
    { "VuDirectionalFlowWaveEntity", "Water", "DirectionalFlowWave", 0x5D6390u },
    { "VuStaticWakeWaveEntity", "Water", "StaticWakeWave", 0x5D58A0u },
    { "VuWakeWaveEntity", "Water", "WakeWave", 0x5D4F10u },
    { "VuPointWaveEntity", "Water", "PointWave", 0x5D42B0u },
    { "VuBumpWaveEntity", "Water", "BumpWave", 0x5D3B50u },
    { "VuBankedTurnWaveEntity", "Water", "BankedTurnWave", 0x5D3180u },
    { "VuWaterTextureSettingsEntity", "Settings", "WaterTextureSettings", 0x5D2B50u },
    { "VuGlobalGfxSettingsEntity", "Settings", "GlobalGfxSettings", 0x5D29B0u },
    { "VuEllipticalGfxSettingsEntity", "Settings", "EllipticalGfxSettings", 0x5D2500u },
    { "VuRectangularGfxSettingsEntity", "Settings", "RectangularGfxSettings", 0x5D1F30u },
    { "VuTriggerBoxEntity", "Triggers", "TriggerBox", 0x5D1A30u },
    { "VuTriggerPlaneEntity", "Triggers", "TriggerPlane", 0x5D1410u },
    { "VuRotateMotionEntity", "Motion", "RotateMotion", 0x5D0D40u },
    { "VuKeyframeMotionEntity", "Motion", "KeyframeMotion", 0x5CE2D0u },
    { "VuKeyframeEntity", "Motion", "Keyframe", 0x5CE090u },
    { "VuTransformEntity", "Motion", "Transform", 0x5CDE30u },
    { "VuControlEntity", "Control", "Control", 0x5CCA50u },
    { "VuControlKeyframeEntity", "Control", "ControlKeyframe", 0x5CC710u },
    { "VuPfxProjectEntity", "Resource", "PfxProject", 0x5CB960u },
    { "VuAudioBankEntity", "Resource", "AudioBank", 0x5CBAE0u },
    { "VuAudioProjectEntity", "Resource", "AudioProject", 0x5CBC60u },
};