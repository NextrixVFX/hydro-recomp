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

inline std::shared_ptr< void > audio_emitter_state_for( const std::string& cls )
{
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
    if ( e.m_type != "VuAudioEmitterEntity" || !e.m_state )
        return;
    audio_emitter_state_t& s = *static_cast< audio_emitter_state_t* >( e.m_state.get( ) );
    property_string( p, "Event Name", s.value.event_name, s.name_default );
    property_bool( p, "Initially Active", s.value.initially_active, s.active_default );
}

inline void audio_emitter_mark_properties( entity_t& e )
{
    if ( e.m_type == "VuAudioEmitterEntity" && e.m_state )
    {
        audio_emitter_state_t& s = *static_cast< audio_emitter_state_t* >( e.m_state.get( ) );
        s.name_default = s.value.event_name;
        s.active_default = s.value.initially_active;
    }
}
