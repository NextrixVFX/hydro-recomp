#pragma once

// The world's state for VuAudioEmitterEntity (the water agent's world/sound.hxx audio_emitter_t - verified against retail by
// case audio_emitter): its two property nodes from the ctor 0x5F3660 - "Event Name" +0x6C (VuAudioEventNameProperty, vtable
// 0x73A18C: its load slot 2 is 0x406C20, property_string's contract - the same node class the breakable's Sfx Name uses,
// verified by case breakable_properties) and "Initially Active" +0x84 (bool, 0).

#include <workspace/core/entity/property.hxx>
#include <workspace/core/world/sound.hxx>

#include <memory>
#include <string>

struct audio_emitter_state_t
{
    audio_emitter_t value;
    std::string name_default;
    std::uint8_t active_default = 0;
};

inline std::shared_ptr< void > audio_emitter_state_for( const std::string& cls )
{
    return cls == "VuAudioEmitterEntity" ? std::make_shared< audio_emitter_state_t >( ) : nullptr;
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
