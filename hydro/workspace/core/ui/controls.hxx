#pragma once

#include <workspace/core/ui/button.hxx>
#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/gauge.hxx>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

// The race HUD's touch controls as draw data, transliterated from the
// disassembly (HUDs/*GameHUD's "#HUD/Controls" template):
// - VuHUDButtonEntity (ctor 0x424A20): +0x64 Screen Scale, +0x6C Image X / Y /
//   Width / Height, +0x7C Touch X / Y / Width / Height, +0xBC Horizontal / +0xC0
//   Vertical Anchor, +0xC4 / +0xC8 the ratios, +0xCC / +0xD0 the Texture /
//   Pressed Texture Asset properties (+0x4C the asset, its +0x58 the texture),
//   +0xE0 the touch state (1 = pressed; written by its touch code - an input
//   here). VuHUDPauseButtonEntity (ctor 0x426120, vtable 0x738AC4): visible
//   slot 15 0x426330, draw slot 16 0x4257A0. VuHUDRecoverButtonEntity (ctor
//   0x425B60, vtable 0x738B24; Boing Rate / Scale Min / Max / Duration +0xE4..
//   +0xF0, +0xF4 the current boing scale - its tick's, an input): visible slot
//   15 0x425F50, draw slot 16 0x425F70.
// - VuHUDOnScreenControlEntity (ctor 0x4292F0; VuHUDOnScreenAxisEntity 0x739020 /
//   VuHUDOnScreenButtonEntity 0x739058 share the draw 0x42A0C0): +0x60 Screen
//   Scale, +0x68 Image rect, +0x78 Touch rect, +0xB8 the anchor block, +0xC8..
//   +0xCB Enable for Tilt / Touch / GamePad / Keyboard, +0xCC / +0xD0 the
//   textures, +0xD4 pressed (its touch code's - an input).
// The globals they read are inputs: g 0x758FB8 (+0x28, the pause button's
// test) and the control-method manager g 0x758FC0 (+0x18: 0 gamepad, 1
// keyboard, 2 tilt, 3 touch).

struct ui_hud_control_t
{
    float m_scale[ 2 ] = { 1080.0f, 720.0f };
    float m_image[ 4 ] = { };
    ui_anchor_t m_anchor;
    std::uint8_t m_enable[ 4 ] = { 1, 1, 1, 1 };   // on-screen controls only: Tilt, Touch, GamePad, Keyboard
    float m_depth = 0.0f;                          // the draw component's Depth
};

// The inputs: the two textures (null = no asset / no texture), the touch
// state, the boing scale and the two globals.
struct ui_hud_control_inputs_t
{
    const ui_texture_t* m_texture = nullptr;
    const ui_texture_t* m_pressed_texture = nullptr;
    std::int32_t m_state = 0;          // +0xE0 (buttons) / the +0xD4 byte (on-screen)
    float m_boing = 1.0f;              // +0xF4 (recover)
    bool m_race_present = false;       // g 0x758FB8 non-null
    std::uint32_t m_race_28 = 0;       // its +0x28
    bool m_method_present = false;     // g 0x758FC0 non-null
    std::int32_t m_method = 0;         // its +0x18
};

// 0x426330: no g 0x758FB8, or its +0x28 non-zero.
inline bool ui_hud_pause_visible( const ui_hud_control_inputs_t& in ) { return !in.m_race_present || in.m_race_28 != 0; }

// 0x425F50: no control-method manager, or a method other than 0 / 1.
inline bool ui_hud_recover_visible( const ui_hud_control_inputs_t& in )
{
    return !in.m_method_present || ( in.m_method != 0 && in.m_method != 1 );
}

// The shared tail: the image rect over the screen scale through the anchor
// transform 0x6282D0, (the recover button's boing), colour 0x??FFFFFF with the
// alpha byte fistp( alpha x 255 ), uv (0, 0, 1, 1), depth Depth x 0.005 + 0.5.
inline void ui_hud_control_tail( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_crop_t& crop, const ui_hud_control_t& c,
                                 const ui_texture_t& texture, float alpha, const float* boing )
{
    const float ix = 1.0f / c.m_scale[ 0 ];
    const float iy = 1.0f / c.m_scale[ 1 ];
    float rect[ 4 ] = { ix * c.m_image[ 0 ], c.m_image[ 1 ] * iy, c.m_image[ 2 ] * ix, c.m_image[ 3 ] * iy };
    ui_anchor_transform( c.m_anchor, crop, rect, rect );
    if ( boing )
    {
        const float s = *boing;
        const float shrink = 1.0f - s;
        const float w = rect[ 2 ], h = rect[ 3 ];
        rect[ 2 ] = w * s;
        rect[ 0 ] = w * 0.5f * shrink + rect[ 0 ];
        rect[ 3 ] = h * s;
        rect[ 1 ] = h * 0.5f * shrink + rect[ 1 ];
    }
    const std::uint32_t a = static_cast< std::uint32_t >( ui_fistp32( alpha * 255.0f ) ) & 0xFFu;   // 0x6F7808
    const float uv[ 4 ] = { 0.0f, 0.0f, 1.0f, 1.0f };
    ui_texture_draw( sort, env, texture.m_handle, ( a << 24 ) | 0x00FFFFFFu, rect, uv, c.m_depth * 0.005f + 0.5f );
}

// 0x4257A0 (pause) / 0x425F70 (recover): visible (slot 15), the pressed texture
// when the state is 1, nothing without a texture.
inline void ui_hud_button_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_crop_t& crop, const ui_hud_control_t& c,
                                const ui_hud_control_inputs_t& in, bool recover, float alpha )
{
    if ( !( recover ? ui_hud_recover_visible( in ) : ui_hud_pause_visible( in ) ) )
        return;
    const ui_texture_t* t = in.m_state == 1 ? in.m_pressed_texture : in.m_texture;
    if ( !t )
        return;
    ui_hud_control_tail( sort, env, crop, c, *t, alpha, recover ? &in.m_boing : nullptr );
}

// 0x42A0C0 (the on-screen axis / button): hidden under a control method its
// Enable flag is off (Tilt 2, Touch 3, GamePad 0, Keyboard 1), else the pressed
// texture when the +0xD4 byte is set.
inline void ui_hud_onscreen_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_crop_t& crop, const ui_hud_control_t& c,
                                  const ui_hud_control_inputs_t& in, float alpha )
{
    if ( in.m_method_present )
    {
        static const std::int32_t methods[ 4 ] = { 2, 3, 0, 1 };
        for ( int k = 0; k < 4; ++k )
            if ( !c.m_enable[ k ] && in.m_method == methods[ k ] )
                return;
    }
    const ui_texture_t* t = ( in.m_state & 0xFF ) ? in.m_pressed_texture : in.m_texture;
    if ( !t )
        return;
    ui_hud_control_tail( sort, env, crop, c, *t, alpha, nullptr );
}

// VuHUDHiddenPackageToastEntity (ctor 0x424360 over the text base 0x627070,
// vtable 0x738BDC; its text getter slot 12 0x4242D0 = +0x118): +0x10C Total Time
// (5.0), +0x110 Fade Time (1.0), +0x114 the time left (0), +0x118 the text
// ("Text"); the base's +0x64 Visible and +0x108 alpha are what its handlers
// drive. Handlers (disassembled): OnPackageFound 0x424600 and OnUITick 0x4248E0.
struct ui_hud_toast_t
{
    float m_total = 5.0f;     // +0x10C
    float m_fade = 1.0f;      // +0x110
    float m_left = 0.0f;      // +0x114
    std::string m_text = "Text";   // +0x118
    bool m_visible = true;    // +0x64
    float m_alpha = 1.0f;     // +0x108
};

// 0x43E030 / 0x407450: the first occurrence of `key` replaced by `with`.
inline void ui_hud_toast_replace( std::string& s, const char* key, const std::string& with )
{
    const std::size_t at = s.find( key );
    if ( at != std::string::npos )
        s.replace( at, std::strlen( key ), with );
}

// 0x424600 (params: two int records - the found count, the track's count; a
// record of another type reads 0): the text = VuStringDB slot 6
// ("HiddenPackage_Toast", a HOOK) with "[FOUND_COUNT]" then "[TRACK_COUNT]"
// replaced by the "%d" texts; time left = Total Time; visible; alpha 0.
inline void ui_hud_toast_found( ui_hud_toast_t& t, const ui_params_t& params, const std::function< std::string( const char* ) >& string_db )
{
    ui_params_reader_t r( params );
    const std::int32_t found = static_cast< std::int32_t >( ui_params_read( r, 0 ) );
    const std::int32_t total = static_cast< std::int32_t >( ui_params_read( r, 0 ) );
    t.m_text = string_db ? string_db( "HiddenPackage_Toast" ) : std::string( );
    char a[ 16 ], b[ 16 ];
    std::snprintf( a, sizeof( a ), "%d", found );
    std::snprintf( b, sizeof( b ), "%d", total );
    ui_hud_toast_replace( t.m_text, "[FOUND_COUNT]", a );
    ui_hud_toast_replace( t.m_text, "[TRACK_COUNT]", b );
    t.m_left = t.m_total;
    t.m_visible = true;
    t.m_alpha = 0.0f;
}

// 0x4248E0 (params: dt - a float record, else 0): while time is left, fade in
// over the first Fade Time (alpha = left / fade while fade > left), out over the
// last (alpha = (total - left) / fade while left > total - fade), else 1; left
// -= dt. Without time left: hidden. (comiss / jbe: unordered takes the later branch.)
inline void ui_hud_toast_tick( ui_hud_toast_t& t, const ui_params_t& params )
{
    float dt = 0.0f;
    ui_params_reader_t r( params );
    std::uint32_t type = 0xFFFFFFFFu;
    if ( r.m_remaining > 0 )
    {
        std::memcpy( &type, r.m_at, 4 );
        if ( type == 2 )
            std::memcpy( &dt, r.m_at + 4, 4 );
    }
    const float left = t.m_left;
    if ( !( left > 0.0f ) )
    {
        t.m_visible = false;
        return;
    }
    const float fade = t.m_fade;
    if ( fade > left )
    {
        t.m_alpha = left / fade;
        t.m_left = left - dt;
        return;
    }
    if ( left > t.m_total - fade )
    {
        t.m_alpha = ( t.m_total - left ) / fade;
        t.m_left = left - dt;
        return;
    }
    t.m_alpha = 1.0f;
    t.m_left = left - dt;
}
