#pragma once

#include <workspace/core/ui/draw.hxx>
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

// Batch 25: the fields the touch hit test of 0x442A90 reads (ctor 0x441170's
// defaults): +0x88 Screen Scale (100, 100), +0x90 X / +0x94 Y (0, 0), +0x110
// TouchX / +0x114 TouchY (0, 0), +0x118 TouchWidth (20) / +0x11C TouchHeight
// (10), +0x168 the anchor block (Horizontal / Vertical Anchor 0, the ratios 1).
struct ui_button_touch_t
{
    float m_scale[ 2 ] = { 100.0f, 100.0f };
    float m_xy[ 2 ] = { 0.0f, 0.0f };
    float m_touch[ 4 ] = { 0.0f, 0.0f, 20.0f, 10.0f };
    ui_anchor_t m_anchor;
};

// The VuTouchCallback (vtable 0x73B79C, at entity +0x5C) maps a device point
// through VuUIImpl +0x4C (0x81A144, the inverse crop matrix - an input):
// x = m4 y + m0 x + m12, y = m1 x + m5 y + m13 (that operand order).
inline void ui_button_touch_map( const float inverse[ 16 ], const float in[ 2 ], float out[ 2 ] )
{
    out[ 0 ] = inverse[ 4 ] * in[ 1 ] + inverse[ 0 ] * in[ 0 ] + inverse[ 12 ];
    out[ 1 ] = inverse[ 1 ] * in[ 0 ] + inverse[ 5 ] * in[ 1 ] + inverse[ 13 ];
}

class c_ui_button
{
public:
    c_ui_button( std::uint32_t id, ui_button_hooks_t& hooks ) : m_id( id ), m_hooks( hooks ) { }

    // The touch callbacks (vtable 0x73B79C; thiscall entity + 0x5C):
    // slot 0 0x442740 (const VuVector2&): Enabled and not +395 -> +394 = +395 =
    // 1 (one WORD store) and the mapped point -> +0x190; slot 1 0x4427D0:
    // Enabled -> +396 = 1; slot 2 0x4427E0: the device's touch 0 (g 0x759070
    // vt+0x1C - the caller's point) mapped -> +0x190, always (VuUIImpl's guard
    // 0x81A0F0 runs first: no effect on the values).
    void touch_down( const float inverse[ 16 ], const float point[ 2 ] )
    {
        if ( !m_enabled || m_touch_press )
            return;
        m_touched = true;
        m_touch_press = true;
        ui_button_touch_map( inverse, point, m_touch_point );
    }

    void touch_up( )
    {
        if ( m_enabled )
            m_touch_release = true;
    }

    void touch_move( const float inverse[ 16 ], const float point[ 2 ] ) { ui_button_touch_map( inverse, point, m_touch_point ); }

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
    // when it gains it), then the touch test (batch 25, disassembled): the rect
    // ( (TouchX + X) x 1 / scale x, (TouchY + Y) x 1 / scale y, TouchWidth x
    // 1 / scale x, TouchHeight x 1 / scale y ) through the anchor 0x6282D0
    // (+0x168, over VuUIImpl's crop - m_crop, null: the identity) in place;
    // touched (+394) with the point (+0x190) inside - px >= x, w + x >= px,
    // py >= y, h + y >= py, comiss (a NaN fails) - SetFocus 0x4428E0 and press
    // (+0x198) = 1. With press 1: a release (+396) fires "Pressed" with one int
    // record 0 and press = 0; then the point outside the rect (the same test,
    // re-read) -> press = 0. A release clears +395 / +396 (one WORD); +394 = 0.
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
        const float ix = 1.0f / m_touch_fields.m_scale[ 0 ];   // 0x6F76B8 = 1.0
        const float iy = 1.0f / m_touch_fields.m_scale[ 1 ];
        float r[ 4 ] = { ix * ( m_touch_fields.m_touch[ 0 ] + m_touch_fields.m_xy[ 0 ] ), iy * ( m_touch_fields.m_touch[ 1 ] + m_touch_fields.m_xy[ 1 ] ),
                         ix * m_touch_fields.m_touch[ 2 ], iy * m_touch_fields.m_touch[ 3 ] };
        static const ui_crop_t k_identity;
        ui_anchor_transform( m_touch_fields.m_anchor, m_crop ? *m_crop : k_identity, r, r );
        const auto inside = [ this, &r ]
        {
            const float px = m_touch_point[ 0 ], py = m_touch_point[ 1 ];
            return px >= r[ 0 ] && r[ 2 ] + r[ 0 ] >= px && py >= r[ 1 ] && r[ 3 ] + r[ 1 ] >= py;
        };
        if ( m_touched && inside( ) )
        {
            set_focus( );
            m_press = 1;
        }
        if ( m_press == 1 )
        {
            if ( m_touch_release )
            {
                ui_params_t p;
                ui_params_add( p, 0, 0u );   // 0x434E50
                if ( m_hooks.m_fire )
                    m_hooks.m_fire( "Pressed", p );
                m_press = 0;
            }
            if ( !inside( ) )
                m_press = 0;
        }
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
    bool m_touch_unknown = false;  // (batch 25: the touch half is recreated - never set)
    // batch 25: the hit test's fields (the owner fills them from the loaded
    // properties), the touch point +0x190 / +0x194, VuUIImpl's crop (null: identity).
    ui_button_touch_t m_touch_fields;
    float m_touch_point[ 2 ] = { 0.0f, 0.0f };
    const ui_crop_t* m_crop = nullptr;

private:
    void fire_with_int( const char* plug, ui_params_reader_t& r )
    {
        ui_params_t out;
        ui_params_add( out, 0, ui_params_read( r, 0 ) );
        if ( m_hooks.m_fire )
            m_hooks.m_fire( plug, out );
    }
};
