#pragma once

#include <workspace/core/ui/slider.hxx>

#include <cstdint>
#include <cstring>
#include <functional>

// VuUIComboBoxEntity (ctor 0x474590; the settings combos - VuUISubtitlesSetting
// Entity ctor 0x46D710, vtable 0x73DF3C, is the one the Screens tick) - its
// per-frame half, transliterated from the disassembly. Fields (offsets from the
// entity, the ctor's defaults): +0x68 Screen Scale (100, 100), +0x70 / +0x74
// Position X / Y, +0x78 the style block (ctor 0x4569F0: its +0x120 = 0.25 the
// scroll time, no property; +0x278 points at it), +0x258..+0x264 TouchLeft X /
// Y / Width / Height (0, 0, 100, 100), +0x268..+0x274 TouchRight (100, 0, 100,
// 100), +0x27C Heading String ID, +0x294 the choices (28-byte entries, filled by
// the subclass's slot 6 - an input here), +0x2A0 the focus the last tick saw,
// +0x2A1 / +0x2A2 the left / right arrow pressed, +0x2A4 the current choice,
// +0x2A8 the shown choice (float), +0x2AC focused, +0x2AD / +0x2AE / +0x2AF the
// touch down / held / up (the touch callbacks' - their dispatch is the device's),
// +0x2B0 the touch point, +0x2B8 the pressed arrow (0 none / 1 left / 2 right).
// All zero in the ctor.
//
// Handlers (ctor 0x474F1C..: VuMethod1 delegate vtable 0x73EE24 through 0x475E50):
// OnUISetFocus -> thunk 0x404AB2 = slot 14 0x475230, OnUITick -> 0x4077DE = slot
// 15 0x475280, OnUIEvent -> 0x4077E8 = slot 16 0x475640. Input SetFocus (0x475DB0)
// -> thunk 0x4077ED = slot 13 0x475190. Outputs NavUp, NavDown, SelectionChanged.
// The subclass's slot 12 applies the current choice: VuUISubtitlesSettingEntity
// 0x46D9B0 = VuSettingsManager g 0x758FE0 +0x5C (byte) = (+0x2A4 != 0), read
// without a null test.

inline constexpr std::uint32_t k_ui_subtitles_vtable = 0x73DF3Cu;

struct ui_combo_box_hooks_t
{
    std::function< void( std::uint32_t, const ui_params_t& ) > m_broadcast;  // 0x505690
    std::function< void( const char*, const ui_params_t& ) > m_fire;         // findPlug 0x504770 + slot 2
    std::function< int( const char* ) > m_connections;                       // the plug's (+0x54 - +0x50) / 4
    std::function< void( const char* ) > m_sound;                            // FMOD getEvent + start (HOOK)
    std::function< float( ) > m_delta;                                       // the tick manager g 0x759008 slot 15 (HOOK)
    std::function< void( std::uint32_t vtable, int current ) > m_apply;      // the subclass's slot 12
};

class c_ui_combo_box
{
public:
    c_ui_combo_box( std::uint32_t id, std::uint32_t vtable, ui_combo_box_hooks_t& hooks ) : m_id( id ), m_vtable( vtable ), m_hooks( hooks ) { }

    // Slot 14 0x475230: focused = the id record (0x440FA0) names this entity (0x500D50).
    void on_set_focus( const ui_params_t& params )
    {
        ui_params_reader_t r( params );
        m_focused = ui_params_read( r, 1 ) == m_id;
    }

    // Slot 13 0x475190 (input SetFocus): broadcast "OnUISetFocus" with the id
    // record (0x440F20; every subscriber's handler runs inside), then focused.
    // Its VuRetVal: +0 = 0.
    void set_focus( )
    {
        ui_params_t p;
        ui_params_add( p, 1, m_id );
        if ( m_hooks.m_broadcast )
            m_hooks.m_broadcast( fsm_hash( "OnUISetFocus" ), p );
        m_focused = true;
    }

    // Slot 15 0x475280 (the params' float record is read into a local and
    // never used; dt = the tick manager's). The arrow rects in UI space: (1 /
    // scale) x (position + rect) for x / y, (1 / scale) x the size; a touch down
    // inside one (inclusive, comiss: unordered fails) calls SetFocus (slot 13)
    // and presses it; a press released steps the choice (0x475B40 -1 / +1),
    // and the press is dropped once the point leaves its rect. Then the up /
    // held bytes clear, the focus is latched, the down byte clears, and the
    // shown choice moves toward the current one by dt / the scroll time
    // (minss / maxss: the target wins an unordered compare).
    void on_tick( )
    {
        const float dt = m_hooks.m_delta ? m_hooks.m_delta( ) : 0.0f;
        const float ix = 1.0f / m_scale[ 0 ];   // 0x6F76B8
        const float iy = 1.0f / m_scale[ 1 ];
        const float lx = ix * ( m_position[ 0 ] + m_left[ 0 ] );
        const float ly = iy * ( m_left[ 1 ] + m_position[ 1 ] );
        const float lw = ix * m_left[ 2 ];
        const float lh = iy * m_left[ 3 ];
        const float rx = ix * ( m_position[ 0 ] + m_right[ 0 ] );
        const float ry = iy * ( m_right[ 1 ] + m_position[ 1 ] );
        const float rw = ix * m_right[ 2 ];
        const float rh = iy * m_right[ 3 ];
        const auto inside = [ this ]( float x, float y, float w, float h )
        {
            return m_touch[ 0 ] >= x && w + x >= m_touch[ 0 ] && m_touch[ 1 ] >= y && h + y >= m_touch[ 1 ];
        };
        if ( m_down )
        {
            if ( inside( lx, ly, lw, lh ) )
            {
                set_focus( );
                m_press = 1;
                m_left_pressed = 1;
            }
            else if ( inside( rx, ry, rw, rh ) )
            {
                set_focus( );
                m_press = 2;
                m_right_pressed = 1;
            }
        }
        if ( m_press == 1 )
        {
            if ( m_up )
            {
                change( -1 );
                m_press = 0;
                m_left_pressed = 0;
            }
            if ( !inside( lx, ly, lw, lh ) )
            {
                m_press = 0;
                m_left_pressed = 0;
            }
        }
        if ( m_press == 2 )
        {
            if ( m_up )
            {
                change( 1 );
                m_press = 0;
                m_right_pressed = 0;
            }
            if ( !inside( rx, ry, rw, rh ) )
            {
                m_press = 0;
                m_right_pressed = 0;
            }
        }
        if ( m_up )
        {
            m_held = 0;
            m_up = 0;
        }
        m_down = 0;
        m_latched = m_focused;
        const float step = dt / m_scroll_time;
        const float target = static_cast< float >( m_current );   // cvtdq2ps
        if ( target > m_shown )
        {
            const float sum = m_shown + step;
            m_shown = sum < target ? sum : target;
        }
        else
        {
            const float diff = m_shown - step;
            m_shown = diff > target ? diff : target;
        }
    }

    // Slot 16 0x475640: the input string read first; nothing unless the focus
    // the last tick saw (+0x2A0). InputUp / InputDown fire NavUp / NavDown with
    // one int record (0x4237B0 / 0x434E50) and, when that plug has a connection,
    // play menu_browse (inline for Up, 0x4D2D90 for Down - the same FMOD calls);
    // InputLeft / InputRight -> 0x475B40(-1 / +1).
    void on_event( const ui_params_t& params )
    {
        ui_params_reader_t r( params );
        const char* input = r.read_string( );
        if ( !m_latched )
            return;
        if ( std::strcmp( input, "InputUp" ) == 0 || std::strcmp( input, "InputDown" ) == 0 )
        {
            const char* plug = input[ 5 ] == 'U' ? "NavUp" : "NavDown";
            ui_params_t out;
            ui_params_add( out, 0, ui_params_read( r, 0 ) );
            if ( m_hooks.m_fire )
                m_hooks.m_fire( plug, out );
            if ( m_hooks.m_connections && m_hooks.m_connections( plug ) && m_hooks.m_sound )
                m_hooks.m_sound( "Global/UI_and_Menu/menu_browse" );
        }
        else if ( std::strcmp( input, "InputLeft" ) == 0 )
            change( -1 );
        else if ( std::strcmp( input, "InputRight" ) == 0 )
            change( 1 );
    }

    // 0x475B40 (step): the new choice = min(current + step, count - 1), then 0 if
    // negative (signed); unchanged -> nothing. Else current = it, slot 12, fire
    // "SelectionChanged" with an empty VuParams, play menu_change.
    void change( int step )
    {
        int k = m_current + step;
        const int last = m_choices - 1;
        if ( last < k )
            k = last;
        if ( k < 0 )
            k = 0;
        if ( k == m_current )
            return;
        m_current = k;
        if ( m_hooks.m_apply )
            m_hooks.m_apply( m_vtable, m_current );
        if ( m_hooks.m_fire )
            m_hooks.m_fire( "SelectionChanged", ui_params_t( ) );
        if ( m_hooks.m_sound )
            m_hooks.m_sound( "Global/UI_and_Menu/menu_change" );
    }

    std::uint32_t m_id;
    std::uint32_t m_vtable;
    ui_combo_box_hooks_t& m_hooks;
    // authored (OUR loaded properties)
    float m_scale[ 2 ] = { 100.0f, 100.0f };            // +0x68
    float m_position[ 2 ] = { 0.0f, 0.0f };             // +0x70
    float m_left[ 4 ] = { 0.0f, 0.0f, 100.0f, 100.0f };  // +0x258
    float m_right[ 4 ] = { 100.0f, 0.0f, 100.0f, 100.0f };  // +0x268
    float m_scroll_time = 0.25f;                         // block +0x120 (0x3E800000)
    // runtime
    int m_choices = 0;               // (+0x298 - +0x294) / 28
    int m_current = 0;               // +0x2A4
    float m_shown = 0.0f;            // +0x2A8
    std::uint8_t m_latched = 0;      // +0x2A0
    std::uint8_t m_left_pressed = 0;   // +0x2A1
    std::uint8_t m_right_pressed = 0;  // +0x2A2
    std::uint8_t m_focused = 0;      // +0x2AC
    std::uint8_t m_down = 0;         // +0x2AD
    std::uint8_t m_held = 0;         // +0x2AE
    std::uint8_t m_up = 0;           // +0x2AF
    float m_touch[ 2 ] = { 0.0f, 0.0f };  // +0x2B0
    int m_press = 0;                 // +0x2B8
};
