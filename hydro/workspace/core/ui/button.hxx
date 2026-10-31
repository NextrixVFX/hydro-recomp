#pragma once

#include <workspace/core/ui/flow.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

// VuBarracudaUIButtonEntity (ctor 0x441170, vtable 0x73B7B0), the front end's
// button: focus, navigation and press. Transliterated from its plug handlers
// (SetFocus 0x4428E0, SetStringID 0x442980, Enable 0x4429E0, Disable 0x442A00,
// IsEnabled 0x442A20) and its event handlers (OnUISetFocus 0x442A40, OnUITick
// 0x442A90, OnUIEvent 0x442D60). Fields (offsets from the entity): +104
// Enabled, +105 Label, +106 Can Have Focus (the ctor's WORD 257 + byte 1),
// +108 String ID, +392 focused, +393 focus wanted, +394 touched, +395 / +396
// the touch bytes, +408 the touch press state.
//
// The VuParams records it reads and writes: type 0 an int (add 0x434E50, read
// 0x4237B0), type 1 an entity id (add 0x440F20, read 0x440FA0), type 4 a
// string (readString 0x505260). A read of the wrong type returns 0 and
// consumes nothing.

inline void ui_params_add( ui_params_t& p, std::uint32_t type, std::uint32_t value )
{
    if ( static_cast< unsigned >( p.m_size + 8 ) > 0x80u )
        return;
    std::memcpy( p.m_bytes + p.m_size, &type, 4 );
    std::memcpy( p.m_bytes + p.m_size + 4, &value, 4 );
    p.m_size += 8;
}

inline std::uint32_t ui_params_read( ui_params_reader_t& r, std::uint32_t type )
{
    std::uint32_t value = 0;
    std::uint32_t have = 0xFFFFFFFFu;
    if ( r.m_remaining <= 0 )
        return 0;
    std::memcpy( &have, r.m_at, 4 );
    if ( have == type )
    {
        r.m_at += 4;
        r.m_remaining -= 4;
        std::memcpy( &value, r.m_at, 4 );
        r.m_at += 4;
        r.m_remaining -= 4;
    }
    return value;
}

// The entity id 0x500D50: FNV-1a of the long name 0x500BF0 - the ancestors'
// names and its own, root first, joined by "/".
inline std::uint32_t ui_entity_id( const std::string& long_name ) { return fsm_hash( long_name.c_str( ) ); }

struct ui_button_hooks_t
{
    // VuEventManager_broadcast 0x505690 (immediate: every subscriber's handler runs inside the call).
    std::function< void( std::uint32_t, const ui_params_t& ) > m_broadcast;
    // An output plug's fire (VuScriptOutputPlug_fire 0x5023A0 on the plug VuScriptComponent_findPlug 0x504770 finds).
    std::function< void( const char*, const ui_params_t& ) > m_fire;
    // The plug's connection count (+0x54 - +0x50) / 4.
    std::function< int( const char* ) > m_connections;
    // FMOD getEvent + start (0x4D2D90's pair).
    std::function< void( const char* ) > m_sound;
};

class c_ui_button
{
public:
    c_ui_button( std::uint32_t id, ui_button_hooks_t& hooks ) : m_id( id ), m_hooks( hooks ) { }

    // 0x4428E0: a button that can have focus broadcasts "OnUISetFocus" with
    // its id (every button's 0x442A40 runs then, this one's included), then
    // wants focus.
    void set_focus( )
    {
        if ( !m_can_focus )
            return;
        ui_params_t p;
        ui_params_add( p, 1, m_id );
        if ( m_hooks.m_broadcast )
            m_hooks.m_broadcast( fsm_hash( "OnUISetFocus" ), p );
        m_want_focus = true;
    }

    // 0x442980: String ID = the string record ("" for anything else).
    void set_string_id( const ui_params_t& params )
    {
        ui_params_reader_t r( params );
        m_string_id = r.read_string( );
    }

    // 0x4429E0 / 0x442A00 (Disable also drops focus: the WORD at +392).
    void enable( ) { m_enabled = true; }

    void disable( )
    {
        m_enabled = false;
        m_focused = false;
        m_want_focus = false;
    }

    // 0x442A20: a VuRetVal { 3 (bool), Enabled }.
    bool is_enabled( ) const { return m_enabled; }

    // 0x442A40: focus is wanted exactly when the id record names this button.
    void on_set_focus( const ui_params_t& params )
    {
        ui_params_reader_t r( params );
        m_want_focus = ui_params_read( r, 1 ) == m_id;
    }

    // 0x442A90: an enabled button takes the wanted focus (and fires OnFocus
    // when it gains it), then the touch test and the touch bytes' reset.
    // NOT RECREATED: the touch hit test - its rect goes through 0x6282D0
    // (unread) and +394 / +396 / +400 / +404 are written only by the VuTouch
    // callback (vtable 0x73B79C, unread). With those never set it is never
    // entered; reaching it sets m_touch_unknown.
    void on_tick( )
    {
        if ( !m_enabled )
            return;
        if ( m_want_focus != m_focused )
        {
            m_focused = m_want_focus;
            if ( m_focused && m_hooks.m_fire )
                m_hooks.m_fire( "OnFocus", ui_params_t( ) );
        }
        if ( m_touched || m_press == 1 )
            m_touch_unknown = true;
        if ( m_touch_release )
        {
            m_touch_press = false;
            m_touch_release = false;
        }
        m_touched = false;
    }

    // 0x442D60: a focused button maps InputUp / Down / Left / Right to its
    // NavUp / Down / Left / Right output and InputSelect to Pressed, each
    // fired with one int record (the int after the input's name, 0 if there
    // is none); a Nav output with a connection also plays menu_browse.
    void on_event( const ui_params_t& params )
    {
        ui_params_reader_t r( params );
        const char* input = r.read_string( );
        if ( !m_focused )
            return;
        static const char* const inputs[ 4 ] = { "InputUp", "InputDown", "InputLeft", "InputRight" };
        static const char* const plugs[ 4 ] = { "NavUp", "NavDown", "NavLeft", "NavRight" };
        for ( int k = 0; k < 4; ++k )
            if ( std::strcmp( input, inputs[ k ] ) == 0 )
            {
                fire_with_int( plugs[ k ], r );
                if ( m_hooks.m_connections && m_hooks.m_connections( plugs[ k ] ) && m_hooks.m_sound )
                    m_hooks.m_sound( "Global/UI_and_Menu/menu_browse" );
                return;
            }
        if ( std::strcmp( input, "InputSelect" ) == 0 )
            fire_with_int( "Pressed", r );
    }

    std::uint32_t m_id;
    ui_button_hooks_t& m_hooks;
    bool m_enabled = true;      // +104
    bool m_label = true;        // +105
    bool m_can_focus = true;    // +106
    std::string m_string_id;    // +108
    bool m_focused = false;     // +392
    bool m_want_focus = false;  // +393
    bool m_touched = false;     // +394
    bool m_touch_press = false;    // +395
    bool m_touch_release = false;  // +396
    std::uint32_t m_press = 0;     // +408
    bool m_touch_unknown = false;

private:
    void fire_with_int( const char* plug, ui_params_reader_t& r )
    {
        ui_params_t out;
        ui_params_add( out, 0, ui_params_read( r, 0 ) );
        if ( m_hooks.m_fire )
            m_hooks.m_fire( plug, out );
    }
};
