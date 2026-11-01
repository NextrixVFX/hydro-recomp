#pragma once

// VuVoiceOverManagerImpl (vtable 0x745E4C, the global g 0x758FEC; ctor 0x4B77A0,
// dtor 0x4B78F0) - the co-pilot's lines: which line, when, and the queue. The
// CPU side, recreated 1:1; FMOD (fmod_eventMetro.dll getEvent / setCallback /
// start), the CRT's rand, the audio ducking (0x4F94B0 / 0x4F95B0 on g
// 0x759010), the subtitle manager g 0x758FE4 vt+0x1C, the dev-menu bool and the
// tick registration are PLATFORM / other managers' HOOKS (vo_hooks_t).
//   +4    bool  enabled (ctor 1; the dev config "Audio/Voice Over" g 0x759014
//               vt+0x18 binds it - a hook)
//   +6    bool  busy: an event started and not yet finished (the callback)
//   +8    std::map<uint32 FNV-1a of the line's name, entry> (+0xC its size)
//   +0x10 entry* current (the line waiting out its StartDelay)
//   +0x14 std::deque<entry*> the queue (+0x20 offset, +0x24 size)
//   +0x28 float the current line's timer
//   +0x2C VuString the ducking category, +0x44 float its volume, +0x48 the
//               ducking handle
// An entry (the map node's value, node + 0x14): +0 VuString EventName, +0x18
// MaxPlayCount, +0x1C StartDelay, +0x20 std::vector<int> Waveforms (shuffled
// at load), +0x2C the play count, +0x30 the next waveform's index.
// SUBSTITUTION: std::map / std::deque for MSVC's (same order, same FIFO).
// Verified against retail: hydro checks/logic.hxx verify_voice_over (case
// voice_over: retail's own manager driven through load / say / queue / tick /
// callback / reset).

#include <workspace/core/entity/json.hxx>
#include <workspace/core/entity/values.hxx>

#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

struct vo_entry_t
{
    std::string event;               // +0    EventName
    std::int32_t max_plays = 0;      // +0x18 MaxPlayCount (0 = unlimited)
    float start_delay = 0.0f;        // +0x1C StartDelay
    std::vector< std::int32_t > waves;   // +0x20 Waveforms
    std::int32_t plays = 0;          // +0x2C
    std::uint32_t next = 0;          // +0x30
};

struct vo_manager_t
{
    bool enabled = true;                         // +4
    bool busy = false;                           // +6
    std::map< std::uint32_t, vo_entry_t > entries;   // +8
    vo_entry_t* current = nullptr;               // +0x10
    std::deque< vo_entry_t* > queue;             // +0x14
    float timer = 0.0f;                          // +0x28
    std::string duck_category;                   // +0x2C
    float duck_volume = 0.0f;                    // +0x44
    std::uint32_t duck_handle = 0;               // +0x48
};

struct vo_hooks_t
{
    // CRT rand (0x64C1F8): 0 .. 0x7FFF.
    std::function< std::int32_t( ) > rand;
    // FMOD::EventSystem::getEvent( name, mode 1, &event ) on g 0x759010 +8: the FMOD_RESULT (0 = FMOD_OK), the handle.
    std::function< std::int32_t( const std::string& name, std::uint32_t& event ) > get_event;
    // FMOD::Event::setCallback( event, 0x4B8180, userdata = the waveform ) and FMOD::Event::start( event ).
    std::function< void( std::uint32_t event, std::int32_t wave ) > set_callback;
    std::function< void( std::uint32_t event ) > start;
    // g 0x758FE4 vt+0x1C( text ): the subtitle ("Copilot_%03d" of the waveform).
    std::function< void( const char* text ) > subtitle;
    // 0x4F94B0( category, volume, 10.0 ) -> handle / 0x4F95B0( handle ) on g 0x759010.
    std::function< std::uint32_t( const std::string& category, float volume, float time ) > duck;
    std::function< void( std::uint32_t handle ) > unduck;
    // The say gates: the viewport count g 0x759060 +4 and, for a non-null boat, boat +0x46B == 0 and its driver's
    // (boat +0x220) vt+0x30 and vt+0x38 answers (the other managers' state).
    std::function< std::int32_t( ) > viewports;
    std::function< bool( const void* boat ) > boat_ok;
};

// asString 0x4FC4B0's text: a string's whole VuString (0x401DB0 assign), else the static "" 0x819950.
inline std::string vo_text( const value_t& v )
{
    return v.tag == 4 ? v.text : std::string( );
}

// 0x44C780 / the inline loops in 0x4B7970 / 0x4B7F50: FNV-1a 32 over the text
// up to its first NUL.
inline std::uint32_t vo_hash( const char* text )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( ; *text; ++text )
        h = ( h ^ static_cast< std::uint8_t >( *text ) ) * 0x1000193u;
    return h;
}

// 0x4B8F90 = MSVC's random_shuffle( first, last, rng ) with the _Rng_from_urng
// adapter over rand (15 bits a call, mask 0x7FFF - 0x4B7970 measures them):
// for k = 2 .. n, element k - 1 swapped with a draw in [0, k).
inline void vo_shuffle( std::vector< std::int32_t >& v, const vo_hooks_t& h )
{
    const std::uint32_t bits = 15, mask = 0x7FFF;
    std::uint32_t k = 2;
    for ( std::size_t next = 1; next < v.size( ); ++next, ++k )
    {
        std::uint32_t ret, top;
        for ( ;; )
        {
            ret = 0;
            top = 0;
            if ( k - 1 != 0 )
            {
                do
                {
                    ret <<= bits;
                    std::uint32_t r;
                    do
                        r = static_cast< std::uint32_t >( h.rand( ) );
                    while ( r > mask );
                    top <<= bits;
                    ret |= r;
                    top |= mask;
                } while ( top < k - 1 );
            }
            const std::uint32_t limit = top / k, top_rem = top % k;
            const std::uint32_t q = ret / k;
            ret %= k;
            if ( q < limit || top_rem == k - 1 )
                break;
        }
        std::swap( v[ next ], v[ ret ] );
    }
}

// Slot 5 0x4B7970 (init), its data half: the tick 0x4B8000 registered in
// "Decision" (g 0x759008 vt+0x20) and the dev bool are the caller's; `db` =
// the VoiceOverDB asset's document +0x58 ["VuDBAsset"] (0x4AE5A0 creates it,
// g 0x759004 vt+0x24 releases it - the asset manager's). The ducking category
// = db["Ducking"]["Category"]'s text (else ""), its volume asFloat(
// db["Ducking"]["Volume"] ); the map emptied; then per key of db in map order
// (0x4FBE90 / 0x4FC000; the value through std::map find 0x4F22B0): skipped
// unless its "Waveforms" is a non-empty array; entry = map[ FNV-1a( key ) ]
// (find-or-insert 0x4B8210: a new entry's counters 0) - EventName's text,
// MaxPlayCount asInt, StartDelay asFloat, the waveforms resized (0x487960) and
// each asInt, shuffled (0x4B8F90) when not empty, then FMOD getEvent( name, 1 )
// (a preload; its answer unread). Answers 1.
inline bool vo_load( vo_manager_t& m, const value_t& db, const vo_hooks_t& h )
{
    m.duck_category = vo_text( member( member( db, "Ducking" ), "Category" ) );
    m.duck_volume = json_as_float( member( member( db, "Ducking" ), "Volume" ) );
    m.entries.clear( );
    const std::vector< std::string > keys = json_member_keys( db );
    for ( const std::string& key : keys )
    {
        const value_t& line = member( db, key.c_str( ) );
        const value_t& waves = member( line, "Waveforms" );
        if ( waves.tag != 5 || waves.list.empty( ) )
            continue;
        vo_entry_t& e = m.entries[ vo_hash( key.c_str( ) ) ];
        e.event = vo_text( member( line, "EventName" ) );
        std::int32_t max_plays = 0;
        property_int_value( member( line, "MaxPlayCount" ), max_plays );
        e.max_plays = max_plays;
        e.start_delay = json_as_float( member( line, "StartDelay" ) );
        e.waves.resize( waves.list.size( ) );
        for ( std::size_t j = 0; j < waves.list.size( ); ++j )
        {
            std::int32_t w = 0;
            property_int_value( waves.list[ j ], w );
            e.waves[ j ] = w;
        }
        if ( !e.waves.empty( ) )
            vo_shuffle( e.waves, h );
        std::uint32_t event = 0;
        if ( h.get_event )
            h.get_event( e.event, event );
    }
    return true;
}

// Slot 6 0x4B7E40 (reset): the map emptied, the current line dropped, the
// queue popped empty.
inline void vo_reset( vo_manager_t& m )
{
    m.entries.clear( );
    m.current = nullptr;
    m.queue.clear( );
}

// The say gates both slots share: enabled, at most one viewport, and a
// null boat or one boat_ok passes.
inline bool vo_gate( const vo_manager_t& m, const void* boat, const vo_hooks_t& h )
{
    if ( !m.enabled || h.viewports( ) > 1 )
        return false;
    return boat == nullptr || h.boat_ok( boat );
}

// Slot 7 0x4B7EA0 (thiscall boat, const char* line; retn 8): when the gates
// pass and no line is current, the named entry (absent: nothing) becomes
// current with its StartDelay - unless MaxPlayCount is set and reached.
inline void vo_say_now( vo_manager_t& m, const void* boat, const char* line, const vo_hooks_t& h )
{
    if ( !m.enabled || h.viewports( ) > 1 || m.current != nullptr )
        return;
    if ( boat != nullptr && !h.boat_ok( boat ) )
        return;
    auto it = m.entries.find( vo_hash( line ) );
    if ( it == m.entries.end( ) )
        return;
    vo_entry_t& e = it->second;
    if ( e.max_plays == 0 || e.plays < e.max_plays )
    {
        m.current = &e;
        m.timer = e.start_delay;
    }
}

// Slot 8 0x4B7F50 (thiscall boat, const char* line; retn 8): when the gates
// pass, the named entry (absent: nothing) pushed on the queue (0x4805C0).
inline void vo_queue( vo_manager_t& m, const void* boat, const char* line, const vo_hooks_t& h )
{
    if ( !vo_gate( m, boat, h ) )
        return;
    auto it = m.entries.find( vo_hash( line ) );
    if ( it != m.entries.end( ) )
        m.queue.push_back( &it->second );
}

// The tick 0x4B8000 (VuMethod1, "Decision"; dt): a current line counts its
// timer down; at <= 0 (comiss: NaN waits) FMOD getEvent( EventName, 1 ) and on
// FMOD_OK the waveform at `next` goes to setCallback( 0x4B8180, wave ) +
// start, the subtitle "Copilot_%03d" of it, plays += 1, next = (next + 1) mod
// the waveform count (unsigned div); the line is dropped (timer 0) either way.
// With none current, a non-empty queue and not busy: the front popped; it
// becomes current with its StartDelay unless MaxPlayCount is set and reached.
inline void vo_tick( vo_manager_t& m, float dt, const vo_hooks_t& h )
{
    if ( m.current != nullptr )
    {
        m.timer = m.timer - dt;
        if ( 0.0f < m.timer || m.timer != m.timer )
            return;
        std::uint32_t event = 0;
        if ( h.get_event( m.current->event, event ) == 0 )
        {
            vo_entry_t& e = *m.current;
            const std::int32_t wave = e.waves[ e.next ];
            h.set_callback( event, wave );
            h.start( event );
            char text[ 0x100 ];
            std::snprintf( text, sizeof( text ), "Copilot_%03d", wave );   // sprintf_s 0x64C170
            h.subtitle( text );
            e.plays += 1;
            e.next = ( e.next + 1 ) % static_cast< std::uint32_t >( e.waves.size( ) );
        }
        m.current = nullptr;
        m.timer = 0.0f;
        return;
    }
    if ( m.queue.empty( ) || m.busy )
        return;
    vo_entry_t* e = m.queue.front( );
    m.queue.pop_front( );
    if ( e->max_plays == 0 || e->plays < e->max_plays )
    {
        m.current = e;
        m.timer = e->start_delay;
    }
}

// The FMOD event callback 0x4B8180 (stdcall event, type, p1, p2, userdata;
// on g 0x758FEC): type 9 (started) busy, ducking = duck( category, volume,
// 10.0 (0x6F778C) ); type 4 (finished) not busy, unduck( handle ), handle 0;
// type 10 (the sound definition's index) *param2 = userdata (the waveform:
// [ebp+0x14] is param2 - param1 is untouched).
// Answers FMOD_OK (0).
inline std::int32_t vo_event_callback( vo_manager_t& m, std::int32_t type, std::int32_t* param2, std::int32_t userdata, const vo_hooks_t& h )
{
    if ( type == 4 )
    {
        m.busy = false;
        h.unduck( m.duck_handle );
        m.duck_handle = 0;
    }
    else if ( type == 9 )
    {
        m.busy = true;
        m.duck_handle = h.duck( m.duck_category, m.duck_volume, 10.0f );
    }
    else if ( type == 10 )
        *param2 = userdata;
    return 0;
}
