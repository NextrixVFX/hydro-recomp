#pragma once

// The race's audio-trigger entities: VuAudioEmitterEntity (vtable 0x756E6C, 0x90 bytes, create 0x5F3560 = 0x5F35E0 pre-zero
// + ctor 0x5F3660) and VuAnimatedPropEntity's "PlayAudioEvent" timed event (the animation-event IF slot 0 0x5ED680).
// FMOD itself is the PLATFORM HOOK: every import retail calls is one hook below, called with retail's arguments in
// retail's order, so the platform audio layer receives exactly the calls the game made:
//   FMOD::EventSystem::getEvent  0x64C348 (on g_vuaudio 0x759010 +8; name, mode, &event) -> FMOD_RESULT (0 = FMOD_OK)
//   FMOD::Event::setCallback     0x64C334 (event, callback 0 or 0x50FBB0, userdata = the VuAudioEvent holder)
//   FMOD::Event::stop            0x64C340 (event, immediate)
//   FMOD::Event::start           0x64C354 (event)
//   FMOD::Event::set3DAttributes 0x64C358 (event, position, velocity, orientation; null = not given)
// Verified against retail: hydro checks/logic.hxx verify_audio_emitter (case audio_emitter).
//
// VuAudioEmitterEntity's members (the ctor 0x5F3660):
//   +0x5C the VuMotionComponentIF (vtable 0x756EA0: slot 0 0x5F3D80), +0x60 VuScriptComponent (order 100), +0x64
//   Vu3dLayoutComponent (its draw 0x5F3C80 = the editor's range spheres: DEV, not recreated), +0x68 VuMotionComponent
//   (0x552CD0 with the IF), the transform component at +0x50.
//   +0x6C "Event Name" (VuAudioEventNameProperty; std::string, change hook 0x5F3C30), +0x84 "Initially Active" (0).
//   +0x88 a VuAudioEvent holder (0x50FBF0's object): +0 the word 1 (+0x88 = 1 "stop when destroyed", +0x89 = 0 "stop at
//   the sound definition's end"), +4 (+0x8C) the FMOD event (0).
//   Plugs: inputs Start (0x5F3B80) / Stop (0x5F3BE0), no outputs. Slot 6 (game initialize) 0x5F3B30; slot 7 = nullsub.
//   The ctor also writes 7 to the transform component's +0xC4 (entity +0x50 -> +0xC4; its meaning unread).

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/boat/state.hxx>
#include <workspace/core/world/wave.hxx>

#include <cstdint>
#include <cstring>
#include <string>

constexpr std::uint32_t g_sound_event_callback = 0x50FBB0u;   // the address retail hands setCallback

struct sound_hooks_t
{
    void* user = nullptr;
    // getEvent (0x64C348): writes *event (whatever FMOD writes, also on an error); answers the FMOD_RESULT. `holder` = the
    // VuAudioEvent whose +4 retail hands as the out pointer, null for a fire-and-forget local (PlayAudioEvent).
    int ( *get_event )( void* user, const char* name, std::uint32_t mode, std::uint32_t* event, const void* holder ) = nullptr;
    void ( *set_callback )( void* user, std::uint32_t event, std::uint32_t callback, const void* holder ) = nullptr;
    void ( *stop )( void* user, std::uint32_t event, bool immediate ) = nullptr;
    void ( *start )( void* user, std::uint32_t event ) = nullptr;
    void ( *attributes )( void* user, std::uint32_t event, const float* position, const float* velocity, const float* orientation ) = nullptr;
    // VuTransformComponent::setWorldTransform 0x5344A0 (thiscall the transform; matrix*, notify) - the motion IF's
    // first call; world/transform.hxx's entity_set_world is its recreation (the world agent's).
    void ( *set_world )( void* user, const float* matrix, bool notify ) = nullptr;
};

// The VuAudioEvent holder (8 bytes: +0 / +1 flags, +4 the FMOD event) as retail lays it.
struct sound_event_t
{
    std::uint8_t image[ 8 ] = { };

    std::uint32_t event( ) const { std::uint32_t v; std::memcpy( &v, image + 4, 4 ); return v; }
    void set_event( std::uint32_t v ) { std::memcpy( image + 4, &v, 4 ); }
};

// 0x50FBF0 (thiscall the holder; name, an argument it never reads; retn 8) = boat/state.hxx boat_audio_event_create
// (the boat agent's recreation, verified there) - reused through its hook shape: its `release` is setCallback( e, 0, 0 )
// + stop( e, false ), its `get_event` getEvent( mode 1 ) and, on FMOD_OK, setCallback( e, 0x50FBB0, holder ).
inline bool sound_event_create( sound_event_t& holder, const std::string& name, const sound_hooks_t& h )
{
    struct bridge_t { const sound_hooks_t* h; const sound_event_t* holder; } b{ &h, &holder };
    boat_engine_audio_hooks_t a;
    a.user = &b;
    a.release = []( void* u, std::uint32_t e )
    {
        const auto* p = static_cast< bridge_t* >( u );
        if ( p->h->set_callback )
            p->h->set_callback( p->h->user, e, 0u, nullptr );
        if ( p->h->stop )
            p->h->stop( p->h->user, e, false );
    };
    a.get_event = []( void* u, int, const std::string& nm, std::uint32_t* e ) -> bool
    {
        const auto* p = static_cast< bridge_t* >( u );
        const int r = p->h->get_event ? p->h->get_event( p->h->user, nm.c_str( ), 1u, e, p->holder ) : -1;
        if ( r == 0 && p->h->set_callback )
            p->h->set_callback( p->h->user, *e, g_sound_event_callback, p->holder );
        return r == 0;
    };
    return boat_audio_event_create( holder.image, 0, name, a );
}

// FMOD's event callback 0x50FBB0 (stdcall event, type, param1, param2, userdata = the holder; retn 0x14; answers 0):
// type 4 (the event finished) zeroes the holder's event; type 2 (a sound definition ended) with the holder's +1 set
// stops the event (not immediate). Nothing else. The platform layer calls this where FMOD would.
inline std::uint32_t sound_event_callback( sound_event_t& holder, std::uint32_t type, const sound_hooks_t& h )
{
    if ( type == 4u )
    {
        holder.set_event( 0u );
        return 0u;
    }
    if ( type == 2u && holder.image[ 1 ] != 0 && h.stop )
        h.stop( h.user, holder.event( ), false );
    return 0u;
}

struct audio_emitter_t
{
    std::string event_name;          // +0x6C
    std::uint8_t initially_active = 0;   // +0x84
    sound_event_t holder{ { 1, 0, 0, 0, 0, 0, 0, 0 } };   // +0x88: the word 1, event 0
};

// The shared body of slot 6, Start and the name hook: 0x50FBF0 on the holder with the name, and when it answers true
// set3DAttributes( event, the transform's world position (entity +0x50 -> +0x80), null, null ) then start( event ).
inline void audio_emitter_play( audio_emitter_t& e, const float* position, const sound_hooks_t& h )
{
    if ( !sound_event_create( e.holder, e.event_name, h ) )
        return;
    if ( h.attributes )
        h.attributes( h.user, e.holder.event( ), position, nullptr, nullptr );
    if ( h.start )
        h.start( h.user, e.holder.event( ) );
}

// Slot 6 0x5F3B30 (game initialize): plays when Initially Active.
inline void audio_emitter_game_init( audio_emitter_t& e, const float* position, const sound_hooks_t& h )
{
    if ( e.initially_active )
        audio_emitter_play( e, position, h );
}

// "Start" 0x5F3B80 (VuRetVal void: the type word 0 written).
inline void audio_emitter_start( audio_emitter_t& e, const float* position, const sound_hooks_t& h )
{
    audio_emitter_play( e, position, h );
}

// "Stop" 0x5F3BE0: a held event gets setCallback( e, 0, 0 ), stop( e, false ), and is dropped.
inline void audio_emitter_stop( audio_emitter_t& e, const sound_hooks_t& h )
{
    const std::uint32_t ev = e.holder.event( );
    if ( !ev )
        return;
    if ( h.set_callback )
        h.set_callback( h.user, ev, 0u, nullptr );
    if ( h.stop )
        h.stop( h.user, ev, false );
    e.holder.set_event( 0u );
}

// The Event Name's change hook 0x5F3C30: replays only while an event is held.
inline void audio_emitter_name_changed( audio_emitter_t& e, const float* position, const sound_hooks_t& h )
{
    if ( e.holder.event( ) )
        audio_emitter_play( e, position, h );
}

// The motion IF's slot 0 0x5F3D80 (this = entity + 0x5C): setWorldTransform( the transform, motion +0x18, true ), then
// with an event held set3DAttributes( event, motion +0x48 (the matrix's translation), motion +0x58 (velocity), null ).
// `motion18` = the motion component's +0x18 words (16 matrix + the velocity at +0x40).
inline void audio_emitter_motion( audio_emitter_t& e, const float* motion18, const sound_hooks_t& h )
{
    if ( h.set_world )
        h.set_world( h.user, motion18, true );
    const std::uint32_t ev = e.holder.event( );
    if ( ev && h.attributes )
        h.attributes( h.user, ev, motion18 + 12, motion18 + 16, nullptr );
}

// The dtor 0x5F3A60's own part (the VuAudioEvent holder's release inlined, then the name freed; the base dtor 0x500870
// follows): a held event gets setCallback( e, 0, 0 ) and, when +0x88 is set, stop( e, immediate = true ); dropped.
inline void audio_emitter_destroy( audio_emitter_t& e, const sound_hooks_t& h )
{
    const bool stop = e.holder.image[ 0 ] != 0;
    const std::uint32_t ev = e.holder.event( );
    if ( ev )
    {
        if ( h.set_callback )
            h.set_callback( h.user, ev, 0u, nullptr );
        if ( stop && h.stop )
            h.stop( h.user, ev, true );
        e.holder.set_event( 0u );
    }
    e.event_name.clear( );
}

// VuAnimatedPropEntity's animation-event IF slot 0 0x5ED680, its "PlayAudioEvent" branch (an exact VuString compare):
// the name = json["EventName"] when a string (tag 4), else "" (0x64D20B) - no empty test, getEvent is asked anyway -
// getEvent( mode 1 ) into a LOCAL (no callback, no holder: fire and forget); on FMOD_OK the point json["Pos"] (vec3
// 0x4FDC60 into zeroed lanes: a lane it cannot read stops the parse, the rest stay 0) through M = fromScale( transform
// +0xB0 ) x transform +0x50 (0x407DB0, 0x4078C0 with the scale matrix as `this`), x' = ((M4 y + M0 x) + M8 z) + M12 and
// likewise, then set3DAttributes( event, point, null, null ) and start( event ). `matrix` = the transform's +0x50 words,
// `scale` its +0xB0.
inline void prop_play_audio_event( const value_t& json, const float* matrix, const float* scale, const sound_hooks_t& h )
{
    const value_t& named = member( json, "EventName" );
    const std::string name = named.tag == 4 ? named.text : std::string( );
    std::uint32_t ev = 0;
    if ( !h.get_event || h.get_event( h.user, name.c_str( ), 1u, &ev, nullptr ) != 0 )
        return;
    float s[ 16 ], m[ 16 ];
    wave_matrix_scale( s, scale );
    hull_multiply( s, m, matrix );
    float p[ 3 ] = { 0.0f, 0.0f, 0.0f };
    vec3( member( json, "Pos" ), p );
    const float out[ 3 ] = { ( ( m[ 4 ] * p[ 1 ] + m[ 0 ] * p[ 0 ] ) + m[ 8 ] * p[ 2 ] ) + m[ 12 ],
                             ( ( m[ 5 ] * p[ 1 ] + m[ 1 ] * p[ 0 ] ) + m[ 9 ] * p[ 2 ] ) + m[ 13 ],
                             ( ( m[ 6 ] * p[ 1 ] + m[ 2 ] * p[ 0 ] ) + m[ 10 ] * p[ 2 ] ) + m[ 14 ] };
    if ( h.attributes )
        h.attributes( h.user, ev, out, nullptr, nullptr );
    if ( h.start )
        h.start( h.user, ev );
}
