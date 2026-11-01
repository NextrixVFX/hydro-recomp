#pragma once

#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/gauge.hxx>
#include <workspace/core/ui/select.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// The combo box draw as data - one row of a config selector (VuGameConfigEntity
// / VuBoatConfigEntity, through the base's slot 16 0x448F40 -> 0x448F50) or a
// settings combo box (0x475B10). Transliterated from the disassembly of
// 0x4578D0 (thiscall on the 52-byte combo record; pos*, screen scale*, depth
// in xmm3; ret 8) and its helpers 0x457E00 (a texture at its own size) and
// 0x457EB0 (the value text's colour).
//
// The record (ui_config_combo_t): +0 the style block below, +4 the label's
// string id, +0x1C the choice entries (28 bytes: the string id, +0x18 the
// locked byte), +0x28 focused, +0x29 / +0x2A the left / right arrow pressed
// bytes, +0x30 the shown choice (float).
//
// The style block (ctor 0x4569F0) lives at VuBaseConfigEntity +0x94 (ctor
// 0x4471C0) and VuUIComboBoxEntity-style entities +0x78 (ctor 0x474590); its
// fields are that entity's properties (names below, the block offset first).
struct ui_combo_block_t
{
    float m_label_rect[ 4 ] = { };             // +0x08 HeadingTextX / Y / Width / Height
    std::uint8_t m_label_params[ 60 ] = { };   // +0x38 HeadingFont's VuFontDrawParams (0x626CF0 over FontDB)
    std::uint32_t m_label_selected = 0;        // +0x7C HeadingSelectedTextColor
    float m_value_offset[ 2 ] = { };           // +0x98 ValueTexturePositionX / Y
    float m_value_rect[ 4 ] = { };             // +0xA0 ValueTextX / Y / Width / Height
    std::uint8_t m_value_params[ 60 ] = { };   // +0xD0 ValueFont's params (+0xDC = params +12, its colour)
    std::uint32_t m_value_selected = 0;        // +0x114 ValueSelectedTextColor
    std::uint32_t m_value_locked = 0;          // +0x118 ValueLockedTextColor
    std::uint32_t m_value_locked_selected = 0; // +0x11C ValueLockedSelectedTextColor
    float m_left[ 2 ] = { };                   // +0x1B4 LeftArrowPositionX / Y
    float m_right[ 2 ] = { };                  // +0x1BC RightArrowPositionX / Y
};

// The block's seven asset properties (+0x1C4.. : each its +0x4C asset -> +0x58
// the texture), in order.
enum ui_combo_texture_t
{
    ui_combo_value = 0,          // +0x1C4 ValueTexture
    ui_combo_left,               // +0x1C8 LeftArrowTexture
    ui_combo_right,              // +0x1CC RightArrowTexture
    ui_combo_selected_left,      // +0x1D0 SelectedLeftArrowTexture
    ui_combo_selected_right,     // +0x1D4 SelectedRightArrowTexture
    ui_combo_pressed_left,       // +0x1D8 PressedLeftArrowTexture
    ui_combo_pressed_right       // +0x1DC PressedRightArrowTexture
};

// What the draw reaches outside the record and the block - each an input:
// the two fonts (0x626B80 on block +0x18 / +0xB0: the asset's, else VuUIImpl's
// default; 0 = none), VuStringDB (g 0x759078) slot 7 on a string id, and the
// textures (the asset layer's; null = no asset or no texture - both draw nothing).
struct ui_combo_inputs_t
{
    std::uint32_t m_label_font = 0;
    std::uint32_t m_value_font = 0;
    std::function< std::string( const std::string& id ) > m_string;
    std::function< const ui_texture_t*( int which ) > m_texture;
};

// The block from a loaded entity's properties (OUR load). `params` fills the
// two font params (the caller: ui_font_params over FontDB - bytes 41..59 are
// UNKNOWN, see draw.hxx).
inline ui_combo_block_t ui_combo_block_of( const entity_t& e )
{
    ui_combo_block_t b;
    const auto take = [ & ]( const char* name, void* out, std::size_t bytes )
    {
        if ( const ui_slot_t* s = ui_property_value( e, name ) )
            std::memcpy( out, s->m_bits, bytes );
    };
    take( "HeadingTextX", &b.m_label_rect[ 0 ], 4 );
    take( "HeadingTextY", &b.m_label_rect[ 1 ], 4 );
    take( "HeadingTextWidth", &b.m_label_rect[ 2 ], 4 );
    take( "HeadingTextHeight", &b.m_label_rect[ 3 ], 4 );
    take( "HeadingSelectedTextColor", &b.m_label_selected, 4 );
    take( "ValueTexturePositionX", &b.m_value_offset[ 0 ], 4 );
    take( "ValueTexturePositionY", &b.m_value_offset[ 1 ], 4 );
    take( "ValueTextX", &b.m_value_rect[ 0 ], 4 );
    take( "ValueTextY", &b.m_value_rect[ 1 ], 4 );
    take( "ValueTextWidth", &b.m_value_rect[ 2 ], 4 );
    take( "ValueTextHeight", &b.m_value_rect[ 3 ], 4 );
    take( "ValueSelectedTextColor", &b.m_value_selected, 4 );
    take( "ValueLockedTextColor", &b.m_value_locked, 4 );
    take( "ValueLockedSelectedTextColor", &b.m_value_locked_selected, 4 );
    take( "LeftArrowPositionX", &b.m_left[ 0 ], 4 );
    take( "LeftArrowPositionY", &b.m_left[ 1 ], 4 );
    take( "RightArrowPositionX", &b.m_right[ 0 ], 4 );
    take( "RightArrowPositionY", &b.m_right[ 1 ], 4 );
    return b;
}

// 0x457E00 (thiscall on the asset; pos*, screen scale*, colour*; depth in
// xmm3; ret 0xC): nothing without the asset's texture (+0x58); else the texture
// draw 0x507360 with rect ((1/sx) x, (1/sy) y, (1/sx) width, (1/sy) height) - the
// texture's own +0xC / +0x10 ints - and uv (0, 0, 1, 1).
inline void ui_combo_texture_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_texture_t* texture, const float pos[ 2 ],
                                   const float scale[ 2 ], std::uint32_t colour, float depth )
{
    if ( !texture )
        return;
    const float ix = 1.0f / scale[ 0 ];
    const float iy = 1.0f / scale[ 1 ];
    const float rect[ 4 ] = { ix * pos[ 0 ], iy * pos[ 1 ], ix * static_cast< float >( texture->m_width ), iy * static_cast< float >( texture->m_height ) };
    const float uv[ 4 ] = { 0.0f, 0.0f, 1.0f, 1.0f };
    ui_texture_draw( sort, env, texture->m_handle, colour, rect, uv, depth );
}

// 0x457EB0 (thiscall on the record; out*, index; ret 8): with entries, the
// index clamped to [0, count - 1] (cmovl then cmovs); a locked entry ->
// focused ? +0x11C : +0x118; otherwise (or with no entries) focused ? +0x114 :
// +0xDC (the value params' own colour).
inline std::uint32_t ui_combo_value_colour( const ui_combo_block_t& b, const ui_config_combo_t& c, int index )
{
    std::uint32_t own;
    std::memcpy( &own, b.m_value_params + 12, 4 );
    const int count = static_cast< int >( c.m_choice_ids.size( ) );
    if ( count )
    {
        int j = index;
        if ( count - 1 < j )
            j = count - 1;
        if ( j < 0 )
            j = 0;
        const bool locked = static_cast< std::size_t >( j ) < c.m_locked.size( ) && c.m_locked[ static_cast< std::size_t >( j ) ];
        if ( locked )
            return c.m_focused ? b.m_value_locked_selected : b.m_value_locked;
    }
    return c.m_focused ? b.m_value_selected : own;
}

// 0x4578D0. false: retail reads a choice entry outside the vector (the shown
// index - fistp of +0x30 - is not checked against the count).
inline bool ui_combo_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_combo_block_t& b, const ui_config_combo_t& c,
                           const float pos[ 2 ], const float scale[ 2 ], float depth, const ui_combo_inputs_t& in )
{
    const auto texture = [ & ]( int which ) { return in.m_texture ? in.m_texture( which ) : nullptr; };
    const auto lookup = [ & ]( const std::string& id ) { return in.m_string ? in.m_string( id ) : std::string( ); };
    bool known = true;
    const float raised = depth + 0.1f;   // 0x6F7610
    // The label: the font (block +0x18), its params with the selected colour
    // when focused, the rect over the scale, offset 0, flags 0x70, alpha 1.
    if ( in.m_label_font )
    {
        const std::string text = lookup( c.m_string_id );
        std::uint8_t params[ 60 ];
        std::memcpy( params, b.m_label_params, 60 );
        if ( c.m_focused )
            std::memcpy( params + 12, &b.m_label_selected, 4 );
        const float ix = 1.0f / scale[ 0 ];
        const float iy = 1.0f / scale[ 1 ];
        const float rect[ 4 ] = { ix * ( pos[ 0 ] + b.m_label_rect[ 0 ] ), iy * ( pos[ 1 ] + b.m_label_rect[ 1 ] ), ix * b.m_label_rect[ 2 ],
                                  iy * b.m_label_rect[ 3 ] };
        const float offset[ 2 ] = { 0.0f, 0.0f };
        known = ui_font_draw( sort, env, in.m_label_font, text.c_str( ), params, rect, 0x70u, 1.0f, offset, depth ) && known;
    }
    // The value texture at pos + the block's offset, white, depth + 0.1.
    {
        const float at[ 2 ] = { pos[ 0 ] + b.m_value_offset[ 0 ], b.m_value_offset[ 1 ] + pos[ 1 ] };
        ui_combo_texture_draw( sort, env, texture( ui_combo_value ), at, scale, 0xFFFFFFFFu, raised );
    }
    // The shown choice's text ("Choice" without entries), sliding by (index -
    // shown) x 2 x ValueTextWidth over the scale.
    const int index = ui_fistp32( c.m_shown );
    const int count = static_cast< int >( c.m_choice_ids.size( ) );
    std::string value = "Choice";   // 0x69370C
    if ( count )
    {
        if ( index < 0 || index >= count )
            return false;
        value = lookup( c.m_choice_ids[ static_cast< std::size_t >( index ) ] );
    }
    if ( in.m_value_font )
    {
        const float ix = 1.0f / scale[ 0 ];
        const float iy = 1.0f / scale[ 1 ];
        const float offset[ 2 ] = { ( static_cast< float >( index ) - c.m_shown ) * 2.0f * b.m_value_rect[ 2 ] * ix, 0.0f };   // 0x6F76F8
        std::uint8_t params[ 60 ];
        std::memcpy( params, b.m_value_params, 60 );
        const std::uint32_t colour = ui_combo_value_colour( b, c, index );
        std::memcpy( params + 12, &colour, 4 );
        const float rect[ 4 ] = { ix * ( pos[ 0 ] + b.m_value_rect[ 0 ] ), iy * ( b.m_value_rect[ 1 ] + pos[ 1 ] ), ix * b.m_value_rect[ 2 ],
                                  iy * b.m_value_rect[ 3 ] };
        known = ui_font_draw( sort, env, in.m_value_font, value.c_str( ), params, rect, 0x52u, 1.0f, offset, depth ) && known;
    }
    // The arrows: left while index > 0, right while index < count - 1; the
    // selected texture when focused, the pressed one over both when +0x29 / +0x2A.
    if ( index > 0 )
    {
        int which = c.m_focused ? ui_combo_selected_left : ui_combo_left;
        if ( c.m_b41 )
            which = ui_combo_pressed_left;
        const float at[ 2 ] = { pos[ 0 ] + b.m_left[ 0 ], b.m_left[ 1 ] + pos[ 1 ] };
        ui_combo_texture_draw( sort, env, texture( which ), at, scale, 0xFFFFFFFFu, depth );
    }
    if ( index < count - 1 )
    {
        int which = c.m_focused ? ui_combo_selected_right : ui_combo_right;
        if ( c.m_b42 )
            which = ui_combo_pressed_right;
        const float at[ 2 ] = { pos[ 0 ] + b.m_right[ 0 ], b.m_right[ 1 ] + pos[ 1 ] };
        ui_combo_texture_draw( sort, env, texture( which ), at, scale, 0xFFFFFFFFu, depth );
    }
    return known;
}

// VuBaseConfigEntity's draw: slot 16 0x448F40 (its alpha argument unread) ->
// 0x448F50: from Position (+0x88 / +0x8C), for each option of the vector
// (+0x2BC) its combo record +0x2B0 + 52 x asInt(ComboBoxID) (0x4FBDF0 member;
// unchecked in retail: false here when outside) drawn at the position with the
// Screen Scale (+0x80) and the depth Depth x 0.005 + 0.5 (recomputed per row);
// then y = Spacing (+0x90) + y.
inline bool ui_config_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_config_entity_t& e, const ui_combo_block_t& b,
                            const float scale[ 2 ], const float position[ 2 ], float spacing, float depth_property, const ui_combo_inputs_t& in )
{
    float pos[ 2 ] = { position[ 0 ], position[ 1 ] };
    bool known = true;
    for ( const value_t* option : e.m_options )
    {
        const float depth = depth_property * 0.005f + 0.5f;   // 0x6F75C4, 0x6F765C
        const int id = ui_json_as_int( member( *option, "ComboBoxID" ) );
        if ( id < 0 || static_cast< std::size_t >( id ) >= e.m_combos.size( ) )
            return false;
        known = ui_combo_draw( sort, env, b, e.m_combos[ static_cast< std::size_t >( id ) ], pos, scale, depth, in ) && known;
        pos[ 1 ] = spacing + pos[ 1 ];
    }
    return known;
}

// VuBaseConfigEntity slot 15 0x448B90 (the Vu2dLayoutComponent's draw; bool;
// ret 4), disassembled. When set: per option row (ComboBoxID's record, unchecked
// in retail: false here when outside) the block's ValueText rect at the row
// position as a rect draw 0x507A00 in the value params' own colour (+0xDC), and
// the block's LeftArrowTexture (+0x1C8) at its own size at the row position +
// LeftArrowPosition (white, 0x507360); then the TouchLeft (+0x278) and
// TouchRight (+0x288) rects at Position as white rect draws - all over the
// Screen Scale at Depth x 0.005 + 0.5. Then always 0x448F50 (ui_config_draw).
inline bool ui_config_layout_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_config_entity_t& e, const ui_combo_block_t& b,
                                   const float scale[ 2 ], const float position[ 2 ], float spacing, float depth_property,
                                   const float touch_left[ 4 ], const float touch_right[ 4 ], const ui_combo_inputs_t& in, bool layout )
{
    if ( layout )
    {
        float y = position[ 1 ];
        std::uint32_t own;
        std::memcpy( &own, b.m_value_params + 12, 4 );
        for ( const value_t* option : e.m_options )
        {
            const float depth = depth_property * 0.005f + 0.5f;   // 0x6F75C4, 0x6F765C
            const int id = ui_json_as_int( member( *option, "ComboBoxID" ) );
            if ( id < 0 || static_cast< std::size_t >( id ) >= e.m_combos.size( ) )
                return false;
            const float ix = 1.0f / scale[ 0 ];
            const float iy = 1.0f / scale[ 1 ];
            const float rect[ 4 ] = { ix * ( b.m_value_rect[ 0 ] + position[ 0 ] ), iy * ( b.m_value_rect[ 1 ] + y ), ix * b.m_value_rect[ 2 ],
                                      iy * b.m_value_rect[ 3 ] };
            ui_rect_draw( sort, env, own, rect, depth );
            const float at[ 2 ] = { b.m_left[ 0 ] + position[ 0 ], b.m_left[ 1 ] + y };
            ui_combo_texture_draw( sort, env, in.m_texture ? in.m_texture( ui_combo_left ) : nullptr, at, scale, 0xFFFFFFFFu, depth );
            y = spacing + y;
        }
        const float depth = depth_property * 0.005f + 0.5f;
        const float ix = 1.0f / scale[ 0 ];
        const float iy = 1.0f / scale[ 1 ];
        const float left[ 4 ] = { ix * ( position[ 0 ] + touch_left[ 0 ] ), iy * ( touch_left[ 1 ] + position[ 1 ] ), ix * touch_left[ 2 ],
                                  iy * touch_left[ 3 ] };
        ui_rect_draw( sort, env, 0xFFFFFFFFu, left, depth );
        const float right[ 4 ] = { ix * ( touch_right[ 0 ] + position[ 0 ] ), iy * ( touch_right[ 1 ] + position[ 1 ] ), ix * touch_right[ 2 ],
                                   iy * touch_right[ 3 ] };
        ui_rect_draw( sort, env, 0xFFFFFFFFu, right, depth );
    }
    return ui_config_draw( sort, env, e, b, scale, position, spacing, depth_property, in );
}
