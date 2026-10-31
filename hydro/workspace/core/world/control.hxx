#pragma once

#include <workspace/core/assets/reader.hxx>
#include <workspace/core/assets/tree.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// VuAnimationControl's clock and its timed-event walk.
//
// One control is one clip playing on one skeleton. VuAnimatedPropEntity's
// PlayAnimation handler @0x5ecb60 builds it (0x2C bytes, ctor @0x5161f0) and
// the skeleton advances every control it holds once a frame (@0x5169e0, an
// inlined copy of the advance below). The timed-event asset hangs off the
// control at +36 and the listener - the prop's VuAnimationEventIF - at +40.
//
// Transliterated from the DISASSEMBLY of the advance @0x516300, because
// Hex-Rays dropped the float arguments at every call: dt arrives in XMM1, and
// both walks take their two bounds in XMM1 and XMM2. The pseudocode of the
// inlined copy @0x5169e0, which does show the arguments, agrees with that
// reading, and so does retail itself run under emulation
// (tools/oracle.py case animation_advance).

// One VuTimedEventAsset record (48 bytes in retail, loaded @0x5c95b0): the
// time at +0, the name at +4, the parsed VUJB document at +32.
struct timed_event_t
{
    float time = 0.0f;
    std::string name;
    value_t params;
};

// VuTimedEventAsset's load @0x5c95b0. The wrapper is LITTLE-endian - checked
// against the bytes of all nine assets in the pack, every one consumed to its
// last byte:
//
//   u32 count
//   count x { f32 time, NUL-terminated name, u32 size, size bytes VUJB }
//
// and each blob goes through the VUJB document parse @0x55b560, which unlike
// the engine reader does NOT scan for the magic: 'VUJB' must sit at offset 0
// and the big-endian version after it must be 1, or the load fails.
inline bool timed_events_load( const std::vector< std::uint8_t >& bytes, std::vector< timed_event_t >& out )
{
    std::size_t cursor = 0;
    auto take = [ & ]( void* to, std::size_t count ) -> bool
    {
        if ( cursor + count > bytes.size( ) )
            return false;
        std::memcpy( to, bytes.data( ) + cursor, count );
        cursor += count;
        return true;
    };

    std::uint32_t count = 0;
    if ( !take( &count, 4 ) )
        return false;
    out.clear( );
    for ( std::uint32_t index = 0; index < count; ++index )
    {
        timed_event_t event{};
        if ( !take( &event.time, 4 ) )
            return false;
        while ( cursor < bytes.size( ) && bytes[ cursor ] != 0 )
            event.name.push_back( static_cast< char >( bytes[ cursor++ ] ) );
        if ( cursor >= bytes.size( ) )
            return false;
        ++cursor;
        std::uint32_t size = 0;
        if ( !take( &size, 4 ) || cursor + size > bytes.size( ) || size < 8 )
            return false;
        const std::uint8_t* doc = bytes.data( ) + cursor;
        const std::uint32_t version = ( static_cast< std::uint32_t >( doc[ 4 ] ) << 24 )
            | ( static_cast< std::uint32_t >( doc[ 5 ] ) << 16 )
            | ( static_cast< std::uint32_t >( doc[ 6 ] ) << 8 ) | doc[ 7 ];
        if ( std::memcmp( doc, "VUJB", 4 ) != 0 || version != 1 )
            return false;
        // The magic is at offset 0, so the engine reader's magic scan lands
        // exactly where retail starts and the two parse the same bytes.
        const std::vector< std::uint8_t > blob( doc, doc + size );
        c_reader reader( blob );
        event.params = reader.parse( );
        cursor += size;
        out.push_back( std::move( event ) );
    }
    return true;
}

struct animation_control_t
{
    // The clip's +24 and +28. Two different members used in two different
    // places - +24 is the clamp and the AnimDone test, +28 the loop length -
    // so they are not merged even where a clip gives them the same value.
    float end = 0.0f;
    float loop = 0.0f;
    // control +20, +24, +28.
    bool looping = true;
    float factor = 1.0f;
    float time = 0.0f;
    // control +36. Null when the PlayAnimation carried no timed-event asset.
    const std::vector< timed_event_t >* events = nullptr;
};

// 0x516570: ascending, fires every event with from <= t < to.
// UNKNOWN, not reproduced: retail re-reads the event count after every fire,
// so a listener that changed the asset mid-walk would change the walk. No
// listener read so far does that.
template < typename fire_t >
inline void control_walk_forward( const animation_control_t& control, float from, float to, fire_t&& fire )
{
    const std::vector< timed_event_t >& events = *control.events;
    for ( std::size_t index = 0; index < events.size( ); ++index )
    {
        const float t = events[ index ].time;
        if ( t >= from && to > t )
            fire( events[ index ].name, &events[ index ].params );
    }
}

// 0x516610: descending, fires every event with to <= t < from.
template < typename fire_t >
inline void control_walk_backward( const animation_control_t& control, float from, float to, fire_t&& fire )
{
    const std::vector< timed_event_t >& events = *control.events;
    for ( std::size_t index = events.size( ); index-- > 0; )
    {
        const float t = events[ index ].time;
        if ( from > t && t >= to )
            fire( events[ index ].name, &events[ index ].params );
    }
}

// 0x516300. `fire( name, params )` stands in for the call through the
// listener's slot 0 (VuAnimatedPropEntity's is @0x5ed680). AnimDone and
// AnimLoop pass retail's shared container dword_7593A0, which arrives here as
// a null params pointer. `listening` is control +40 being non-null: without a
// listener the clock still moves and nothing fires.
template < typename fire_t >
inline void control_advance( animation_control_t& control, float dt, bool listening, fire_t&& fire )
{
    const float old = control.time;
    control.time = control.factor * dt + old;
    if ( control.looping )
    {
        while ( control.time >= control.loop )
            control.time = control.time - control.loop;
        while ( 0.0f > control.time )
            control.time = control.time + control.loop;
    }
    else
    {
        control.time = std::fmaxf( 0.0f, std::fminf( control.end, control.time ) );
    }

    // ucomiss: only an exact, ordered equality returns early.
    if ( old == control.time || !listening )
        return;

    const bool has = control.events != nullptr && !control.events->empty( );
    if ( dt > 0.0f )
    {
        if ( control.time > old )
        {
            if ( has )
                control_walk_forward( control, old, control.time, fire );
            if ( control.time == control.end )
                fire( std::string( "AnimDone" ), nullptr );
        }
        else
        {
            // Reached with a negative Time Factor as well as on a wrap: retail
            // branches on the sign of dt, not of the step.
            if ( has )
            {
                control_walk_forward( control, old, control.loop, fire );
                control_walk_forward( control, 0.0f, control.time, fire );
            }
            fire( std::string( "AnimLoop" ), nullptr );
        }
    }
    else if ( 0.0f > dt )
    {
        if ( old > control.time )
        {
            if ( has )
                control_walk_backward( control, old, control.time, fire );
            if ( control.time == 0.0f )
                fire( std::string( "AnimDone" ), nullptr );
        }
        else
        {
            if ( has )
            {
                control_walk_backward( control, old, 0.0f, fire );
                control_walk_backward( control, control.loop, control.time, fire );
            }
            fire( std::string( "AnimLoop" ), nullptr );
        }
    }
}
