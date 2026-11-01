#pragma once

#include <workspace/core/ui/expand.hxx>

#include <cstdint>
#include <cstring>
#include <string>

// The config selectors' navigation (VuBaseConfigEntity, ctor 0x4471C0, vtable
// 0x73C4D4 - shared by VuGameConfigEntity 0x73D1DC and VuBoatConfigEntity
// 0x73C69C): the pad input (slot 14 0x448A50), the touch callbacks (the
// VuTouchCallback at entity +0x5C, vtable 0x73C4C0), the per-frame tick (slot
// 13 0x448470), the focus move 0x4496E0 and the choice change 0x449830 -
// transliterated from the disassembly. The touch device and the tick manager
// are platform / engine: their answers (the touch position, the frame's dt)
// are inputs here.

// The entity's fields the navigation reads and writes. Authored ("Screen
// Scale" +0x80, "Position X/Y" +0x88, "Spacing" +0x90, "TouchLeft X/Y/Width/
// Height" +0x278.., "TouchRight ..." +0x288..) and the touch state (+0x2CD
// down, +0x2CE held, +0x2CF up, +0x2D0 the touch point, +0x2D8 the pressed
// arrow: 0 none / 1 left / 2 right, +0x2DC its option). `m_scroll_time` is the
// combo box block's +0x120 (entity +0x1B4; every combo record's +0 points at
// the block, entity +0x94): 0.25 from its ctor 0x4569F0, no property.
struct ui_config_nav_t
{
    float m_scale[ 2 ] = { 100.0f, 100.0f };
    float m_position[ 2 ] = { 0.0f, 0.0f };
    float m_spacing = 0.0f;
    float m_scroll_time = 0.25f;
    float m_left[ 4 ] = { 0.0f, 0.0f, 100.0f, 100.0f };
    float m_right[ 4 ] = { 100.0f, 0.0f, 100.0f, 100.0f };
    std::uint8_t m_down = 0;
    std::uint8_t m_held = 0;
    std::uint8_t m_up = 0;
    float m_touch[ 2 ] = { 0.0f, 0.0f };
    int m_press = 0;
    int m_press_index = 0;
};

// VuUIImpl +76 (0x81A144): the inverse of the crop matrix (setCropMatrix
// 0x626750 -> 0x4157C0, not recreated: an input). The touch callbacks map a
// device point through m0 m1 m4 m5 m12 m13.
inline void ui_touch_map( const float inverse[ 16 ], const float in[ 2 ], float out[ 2 ] )
{
    out[ 0 ] = in[ 0 ] * inverse[ 0 ] + in[ 1 ] * inverse[ 4 ] + inverse[ 12 ];
    out[ 1 ] = in[ 0 ] * inverse[ 1 ] + in[ 1 ] * inverse[ 5 ] + inverse[ 13 ];
}

// VuTouchCallback slot 0 0x4481C0 (thiscall entity + 0x5C, const VuVector2&):
// unless held (+0x2CE), +0x2CD = +0x2CE = 1 and the mapped point -> +0x2D0.
inline void ui_config_touch_down( ui_config_nav_t& n, const float inverse[ 16 ], const float point[ 2 ] )
{
    if ( n.m_held )
        return;
    n.m_down = 1;
    n.m_held = 1;
    ui_touch_map( inverse, point, n.m_touch );
}

// Slot 1 0x4482C0: +0x2CF = 1.
inline void ui_config_touch_up( ui_config_nav_t& n ) { n.m_up = 1; }

// Slot 2 0x4482D0: the device's touch 0 (g 0x759070 slot +0x1C: HOOK, the
// point is the input) mapped -> +0x2D0, always.
inline void ui_config_touch_move( ui_config_nav_t& n, const float inverse[ 16 ], const float point[ 2 ] )
{
    ui_touch_map( inverse, point, n.m_touch );
}

// 0x44BEE0 (thiscall this, k): option k's combo record (+0x2B0 + 52 x asInt(
// ComboBoxID)). Unchecked in retail (k or the id outside): null here.
inline ui_config_combo_t* ui_config_combo_at( ui_config_entity_t& e, int k )
{
    if ( k < 0 || static_cast< std::size_t >( k ) >= e.m_options.size( ) )
        return nullptr;
    const int id = ui_json_as_int( member( *e.m_options[ static_cast< std::size_t >( k ) ], "ComboBoxID" ) );
    if ( id < 0 || static_cast< std::size_t >( id ) >= e.m_combos.size( ) )
        return nullptr;
    return &e.m_combos[ static_cast< std::size_t >( id ) ];
}

// 0x4496E0 (thiscall this, k): with options, focus (+0x2C8) = (k + count) %
// count (UNSIGNED division); when it moved: the old focus's record +40 = 0,
// the new one's +40 = 1, the output plug "OnSelectionChanged" fired (empty
// params), the voice "Global/UI_and_Menu/menu_browse", the info string
// 0x44B9C0. False on a path retail takes outside its data (or a missing plug).
inline bool ui_config_focus( ui_config_entity_t& e, ui_config_env_t& env, int k, const ui_config_class_t& cls, const ui_profile_t& p,
                             const ui_config_hooks_t& h )
{
    const int old = e.m_focus;
    const std::uint32_t count = static_cast< std::uint32_t >( e.m_options.size( ) );
    if ( count )
        e.m_focus = static_cast< int >( ( static_cast< std::uint32_t >( k ) + count ) % count );
    if ( old == e.m_focus )
        return true;
    ui_config_combo_t* was = ui_config_combo_at( e, old );
    if ( !was )
        return false;
    was->m_focused = 0;
    ui_config_combo_t* now = ui_config_combo_at( e, e.m_focus );
    if ( !now )
        return false;
    now->m_focused = 1;
    if ( !h.fire( "OnSelectionChanged", ui_params_t{ } ) )
        return false;
    h.sound( "Global/UI_and_Menu/menu_browse" );
    return ui_config_info( e, *env.m_root, cls, p, h );
}

// 0x449830 (thiscall this, k, step): for option k of the vector (k < count,
// signed - a negative k reads before the vector in retail: false), the
// current choice (0x44BD50) + step, clamped to the last choice then to 0;
// when it moved: root GameConfig [Name] = that choice's Value text (to its
// first NUL), the option vector emptied and the walk 0x44B0D0 over the
// options document (+0x2A0) with the entity's refresh flag as it is, the
// combo state 0x44B780, slot 12 (+48), the info string 0x44B9C0, the output
// plug "OnOptionChanged" (empty params), the voice "Global/UI_and_Menu/
// menu_change".
inline bool ui_config_change( ui_config_entity_t& e, ui_config_env_t& env, const value_t& doc, int k, int step, const ui_config_class_t& cls,
                              const ui_profile_t& p, const ui_config_hooks_t& h )
{
    if ( k >= static_cast< int >( e.m_options.size( ) ) )
        return true;
    if ( k < 0 )
        return false;
    const value_t& option = *e.m_options[ static_cast< std::size_t >( k ) ];
    value_t& root = *env.m_root;
    const int current = ui_config_choice_index( option, root, cls );
    const value_t& choices = member( option, "Choices" );
    const int count = ui_json_size( choices );
    int next = step + current;
    if ( count - 1 < next )
        next = count - 1;
    if ( next < 0 )
        next = 0;
    if ( next == current )
        return true;
    const std::string value = ui_json_text( member( ui_json_element( choices, next ), "Value" ) );
    const std::string name = ui_json_text( member( option, "Name" ) );
    ui_json_set_string( ui_json_child_s( ui_json_child( root, "GameConfig" ), name ), value.c_str( ) );
    e.m_options.clear( );
    ui_config_walk( e, env, doc, e.m_options, cls, p );
    bool ok = ui_config_combo_state( e, root, cls, p );
    ok = ok && cls.changed( );
    ok = ok && ui_config_info( e, root, cls, p, h );
    ok = ok && h.fire( "OnOptionChanged", ui_params_t{ } );
    if ( ok )
        h.sound( "Global/UI_and_Menu/menu_change" );
    return ok && !e.m_fault;
}

// Slot 14 0x448A50 (thiscall this, const VuParams&): its first record read as
// a string (0x505260): "InputUp" -> 0x4496E0(focus - 1), "InputDown" ->
// 0x4496E0(focus + 1), "InputLeft" -> 0x449830(focus, -1), "InputRight" ->
// 0x449830(focus, +1); anything else nothing.
inline bool ui_config_input( ui_config_entity_t& e, ui_config_env_t& env, const value_t& doc, const ui_params_t& params, const ui_config_class_t& cls,
                             const ui_profile_t& p, const ui_config_hooks_t& h )
{
    ui_params_reader_t r( params );
    const char* input = r.read_string( );
    if ( std::strcmp( input, "InputUp" ) == 0 )
        return ui_config_focus( e, env, e.m_focus - 1, cls, p, h );
    if ( std::strcmp( input, "InputDown" ) == 0 )
        return ui_config_focus( e, env, e.m_focus + 1, cls, p, h );
    if ( std::strcmp( input, "InputLeft" ) == 0 )
        return ui_config_change( e, env, doc, e.m_focus, -1, cls, p, h );
    if ( std::strcmp( input, "InputRight" ) == 0 )
        return ui_config_change( e, env, doc, e.m_focus, 1, cls, p, h );
    return true;
}

// An arrow's hit test as 0x448470 makes it (comiss: an unordered compare
// fails): x >= left, width + left >= x, y >= top, height + top >= y.
inline bool ui_config_hit( float x, float y, float left, float top, float width, float height )
{
    return x >= left && width + left >= x && y >= top && height + top >= y;
}

// Slot 13 0x448470 (thiscall this, const VuParams&): the per-frame tick. The
// params' first float record is read into a local nothing reads; dt = the tick
// manager's slot 15 (g 0x759008 +0x3C, x87, stored as float: HOOK -> `dt`).
//   With a new touch (+0x2CD): for each option k (y = Position Y, then
//   y += Spacing per row, accumulated in float), the left arrow
//   ((TouchLeft X + Position X) / scale x, (TouchLeft Y + y) / scale y, width /
//   scale x, height / scale y; each 1 / scale computed once, then multiplied):
//   hit -> 0x4496E0(k), pressed = 1 (+0x2D8), +0x2DC = k, option k's record
//   +41 = 1; else the right arrow (X + Position X, TouchRight Y + y): hit ->
//   0x4496E0(k), pressed = 2, +0x2DC = k, record +42 = 1.
//   Pressed left: the arrow of row +0x2DC at (TouchLeft Y + Position Y) + row x
//   Spacing; on release (+0x2CF) 0x449830(row, -1), pressed = 0, record +41 =
//   0; then a touch point off that arrow: pressed = 0, record +41 = 0. Pressed
//   right: the same with the right arrow, step +1 and +42.
//   Released: +0x2CE = +0x2CF = 0. +0x2CD = 0. Then every option's record
//   moves its shown choice (+48) toward the current one (+44, as float) by dt
//   / the scroll time: up with minss(shown + step, current) when current >
//   shown, else down with maxss(shown - step, current).
inline bool ui_config_tick( ui_config_entity_t& e, ui_config_nav_t& n, ui_config_env_t& env, const value_t& doc, float dt, const ui_config_class_t& cls,
                            const ui_profile_t& p, const ui_config_hooks_t& h )
{
    const float one = 1.0f;  // 0x6F76B8
    if ( n.m_down )
    {
        float y = n.m_position[ 1 ];
        const float x = n.m_position[ 0 ];
        for ( int k = 0; k < static_cast< int >( e.m_options.size( ) ); ++k )
        {
            const float isx = one / n.m_scale[ 0 ];
            const float isy = one / n.m_scale[ 1 ];
            const float lx = isx * ( n.m_left[ 0 ] + x );
            const float ly = isy * ( n.m_left[ 1 ] + y );
            const float lw = isx * n.m_left[ 2 ];
            const float lh = isy * n.m_left[ 3 ];
            const float rx = ( x + n.m_right[ 0 ] ) * isx;
            const float ry = ( n.m_right[ 1 ] + y ) * isy;
            const float rw = n.m_right[ 2 ] * isx;
            const float rh = n.m_right[ 3 ] * isy;
            if ( ui_config_hit( n.m_touch[ 0 ], n.m_touch[ 1 ], lx, ly, lw, lh ) )
            {
                if ( !ui_config_focus( e, env, k, cls, p, h ) )
                    return false;
                n.m_press = 1;
                n.m_press_index = k;
                ui_config_combo_t* c = ui_config_combo_at( e, k );
                if ( !c )
                    return false;
                c->m_b41 = 1;
            }
            else if ( ui_config_hit( n.m_touch[ 0 ], n.m_touch[ 1 ], rx, ry, rw, rh ) )
            {
                if ( !ui_config_focus( e, env, k, cls, p, h ) )
                    return false;
                n.m_press = 2;
                n.m_press_index = k;
                ui_config_combo_t* c = ui_config_combo_at( e, k );
                if ( !c )
                    return false;
                c->m_b42 = 1;
            }
            y = n.m_spacing + y;
        }
    }
    if ( n.m_press == 1 )
    {
        const float isx = one / n.m_scale[ 0 ];
        const float isy = one / n.m_scale[ 1 ];
        const float row = static_cast< float >( n.m_press_index ) * n.m_spacing;
        const float ly = ( ( n.m_left[ 1 ] + n.m_position[ 1 ] ) + row ) * isy;
        const float lw = isx * n.m_left[ 2 ];
        const float lh = isy * n.m_left[ 3 ];
        const float lx = isx * ( n.m_left[ 0 ] + n.m_position[ 0 ] );
        if ( n.m_up )
        {
            if ( !ui_config_change( e, env, doc, n.m_press_index, -1, cls, p, h ) )
                return false;
            n.m_press = 0;
            ui_config_combo_t* c = ui_config_combo_at( e, n.m_press_index );
            if ( !c )
                return false;
            c->m_b41 = 0;
        }
        if ( !ui_config_hit( n.m_touch[ 0 ], n.m_touch[ 1 ], lx, ly, lw, lh ) )
        {
            n.m_press = 0;
            ui_config_combo_t* c = ui_config_combo_at( e, n.m_press_index );
            if ( !c )
                return false;
            c->m_b41 = 0;
        }
    }
    if ( n.m_press == 2 )
    {
        const float isx = one / n.m_scale[ 0 ];
        const float isy = one / n.m_scale[ 1 ];
        const float row = static_cast< float >( n.m_press_index ) * n.m_spacing;
        const float ry = isy * ( ( n.m_right[ 1 ] + n.m_position[ 1 ] ) + row );
        const float rw = isx * n.m_right[ 2 ];
        const float rh = isy * n.m_right[ 3 ];
        const float rx = isx * ( n.m_right[ 0 ] + n.m_position[ 0 ] );
        if ( n.m_up )
        {
            if ( !ui_config_change( e, env, doc, n.m_press_index, 1, cls, p, h ) )
                return false;
            n.m_press = 0;
            ui_config_combo_t* c = ui_config_combo_at( e, n.m_press_index );
            if ( !c )
                return false;
            c->m_b42 = 0;
        }
        if ( !ui_config_hit( n.m_touch[ 0 ], n.m_touch[ 1 ], rx, ry, rw, rh ) )
        {
            n.m_press = 0;
            ui_config_combo_t* c = ui_config_combo_at( e, n.m_press_index );
            if ( !c )
                return false;
            c->m_b42 = 0;
        }
    }
    if ( n.m_up )
    {
        n.m_held = 0;
        n.m_up = 0;
    }
    n.m_down = 0;
    for ( int k = 0; k < static_cast< int >( e.m_options.size( ) ); ++k )
    {
        ui_config_combo_t* c = ui_config_combo_at( e, k );
        if ( !c )
            return false;
        const float step = dt / n.m_scroll_time;
        const float current = static_cast< float >( c->m_index );
        const float shown = c->m_shown;
        if ( current > shown )
        {
            const float up = step + shown;
            c->m_shown = up < current ? up : current;      // minss
        }
        else
        {
            const float down = shown - step;
            c->m_shown = down > current ? down : current;  // maxss
        }
    }
    return !e.m_fault;
}
