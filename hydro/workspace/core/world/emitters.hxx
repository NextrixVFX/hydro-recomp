#pragma once

// The world's state for VuAudioEmitterEntity (the water agent's world/sound.hxx audio_emitter_t - verified against retail by
// case audio_emitter): its two property nodes from the ctor 0x5F3660 - "Event Name" +0x6C (VuAudioEventNameProperty, vtable
// 0x73A18C: its load slot 2 is 0x406C20, property_string's contract - the same node class the breakable's Sfx Name uses,
// verified by case breakable_properties) and "Initially Active" +0x84 (bool, 0).

#include <workspace/core/entity/property.hxx>
#include <workspace/core/world/keymotion.hxx>
#include <workspace/core/world/sound.hxx>

#include <memory>
#include <string>

struct audio_emitter_state_t
{
    audio_emitter_t value;
    std::string name_default;
    std::uint8_t active_default = 0;
};

// VuKeyframeMotionEntity (world/keymotion.hxx key_motion_t, the water agent's - case key_motion): VuMotionEntity's ctor
// 0x61A000 nodes "Initially Active" / "One Shot" and the ctor 0x5CE400's "Loop" / "Ease In/Out" / "Affect Position" /
// "Affect Rotation" are VuBasicProperty<bool> (0x737450: property_bool); "Curve Type" is a VuStaticIntEnumProperty
// (0x74C354, its value read 0x503600 UNREAD) - an entity that authors it is UNKNOWN (`curve_authored`, not run).
// VuKeyframeEntity (ctor 0x5CE110): "Time" +0x60, VuBasicProperty<float> (0x737484: property_float).
struct key_motion_state_t
{
    key_motion_t value;
    std::uint8_t d_active = 0, d_one_shot = 0, d_loop = 0, d_ease = 1, d_position = 1, d_rotation = 1;
    bool curve_authored = false;
};

struct keyframe_state_t
{
    float time = 0.0f, time_default = 0.0f;
};

// VuSubstituteAssetEntity (create 0x5F47A0, ctor 0x5F48A0 over 0x500710(0), vtable 0x7570C4): "Asset Type" +0x60 - the
// ctor's one property (0x4C bytes, vtable 0x7570F8: the string node, its load slot 2 0x406C20 = property_string's
// contract; +0x48 the asset factory's vt+0x2C type list); +0x78 / +0x90 the two names. Its postLoad (slot 2 0x5F4B70)
// first runs 0x5F4C10 - the +0xA8 / +0xAC properties unlinked from the list and deleted, +0x78 / +0x90 = "", two new
// asset-name properties (0x53FC70: 0x68 bytes, vtable 0x74D6CC, the type = +0x60's text, "Asset Name" over +0x78 and
// "Subst Asset Name" over +0x90, +0x64 the asset 0) appended - then each one's vt+8 (0x406C20) on data["Properties"]
// (0x4FBDF0). Their notify (slot 11 0x53FDD0): a held asset released (factory vt+0x24), then when the factory's vt+0x28
// (doesAssetExist(type, name)) answers true +0x64 = its vt+0x1C (createAsset(type, name)). The postLoad runs once per
// load with the data being loaded: ours resets the two names on every property load and reads them from that load's node.
struct substitute_asset_state_t
{
    std::string type, type_default;   // +0x60 "Asset Type"
    std::string name, subst;          // +0x78 "Asset Name" / +0x90 "Subst Asset Name" (0x5F4C10 resets them: no default)
    bool post_loaded = false;
};

inline std::shared_ptr< void > audio_emitter_state_for( const std::string& cls )
{
    if ( cls == "VuSubstituteAssetEntity" )
        return std::make_shared< substitute_asset_state_t >( );
    if ( cls == "VuKeyframeMotionEntity" )
        return std::make_shared< key_motion_state_t >( );
    if ( cls == "VuKeyframeEntity" )
        return std::make_shared< keyframe_state_t >( );
    return cls == "VuAudioEmitterEntity" ? std::make_shared< audio_emitter_state_t >( ) : nullptr;
}

inline void key_motion_load_properties( entity_t& e, const value_t& p )
{
    if ( e.m_type == "VuKeyframeEntity" && e.m_state )
    {
        keyframe_state_t& s = *static_cast< keyframe_state_t* >( e.m_state.get( ) );
        property_float( p, "Time", s.time, s.time_default, false );
        return;
    }
    if ( e.m_type != "VuKeyframeMotionEntity" || !e.m_state )
        return;
    key_motion_state_t& s = *static_cast< key_motion_state_t* >( e.m_state.get( ) );
    key_motion_t& v = s.value;
    property_bool( p, "Initially Active", v.initially_active, s.d_active );
    property_bool( p, "One Shot", v.one_shot, s.d_one_shot );
    property_bool( p, "Loop", v.loop, s.d_loop );
    property_bool( p, "Ease In/Out", v.ease, s.d_ease );
    property_bool( p, "Affect Position", v.affect_position, s.d_position );
    property_bool( p, "Affect Rotation", v.affect_rotation, s.d_rotation );
    s.curve_authored = s.curve_authored || find( p, "Curve Type" ) != nullptr;
}

inline void key_motion_mark_properties( entity_t& e )
{
    if ( e.m_type == "VuKeyframeEntity" && e.m_state )
    {
        keyframe_state_t& s = *static_cast< keyframe_state_t* >( e.m_state.get( ) );
        s.time_default = s.time;
    }
    if ( e.m_type == "VuKeyframeMotionEntity" && e.m_state )
    {
        key_motion_state_t& s = *static_cast< key_motion_state_t* >( e.m_state.get( ) );
        const key_motion_t& v = s.value;
        s.d_active = v.initially_active;
        s.d_one_shot = v.one_shot;
        s.d_loop = v.loop;
        s.d_ease = v.ease;
        s.d_position = v.affect_position;
        s.d_rotation = v.affect_rotation;
    }
}

inline void audio_emitter_load_properties( entity_t& e, const value_t& p )
{
    if ( e.m_type == "VuSubstituteAssetEntity" && e.m_state )
    {
        substitute_asset_state_t& s = *static_cast< substitute_asset_state_t* >( e.m_state.get( ) );
        property_string( p, "Asset Type", s.type, s.type_default );
        if ( !s.post_loaded )
        {
            // VuEntity::load 0x5009D0's postLoad (slot 2): the names reset, then read from this load's node.
            s.post_loaded = true;
            s.name.clear( );
            s.subst.clear( );
        }
        // A template override (0x5015D0) loads every listed property (the two made by the postLoad above included) through
        // its vt+8 and calls vt+0xC (nullsub 0x4021A0), not the postLoad: an absent key keeps the value.
        const std::string name = s.name, subst = s.subst;
        property_string( p, "Asset Name", s.name, name );
        property_string( p, "Subst Asset Name", s.subst, subst );
        return;
    }
    if ( e.m_type != "VuAudioEmitterEntity" || !e.m_state )
        return;
    audio_emitter_state_t& s = *static_cast< audio_emitter_state_t* >( e.m_state.get( ) );
    property_string( p, "Event Name", s.value.event_name, s.name_default );
    property_bool( p, "Initially Active", s.value.initially_active, s.active_default );
}

inline void audio_emitter_mark_properties( entity_t& e )
{
    if ( e.m_type == "VuSubstituteAssetEntity" && e.m_state )
        static_cast< substitute_asset_state_t* >( e.m_state.get( ) )->type_default = static_cast< substitute_asset_state_t* >( e.m_state.get( ) )->type;
    if ( e.m_type == "VuAudioEmitterEntity" && e.m_state )
    {
        audio_emitter_state_t& s = *static_cast< audio_emitter_state_t* >( e.m_state.get( ) );
        s.name_default = s.value.event_name;
        s.active_default = s.value.initially_active;
    }
}
