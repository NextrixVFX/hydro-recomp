#pragma once

#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/flow.hxx>
#include <workspace/core/ui/gauge.hxx>
#include <workspace/core/ui/image.hxx>
#include <workspace/core/ui/select.hxx>

#include <cstdint>
#include <cstring>

// Small Screens leaves: their draws and the runtime state those draws read,
// transliterated from the disassembly.

// --- VuBoatStatEntity (ctor 0x44F5C0, vtable 0x73C808) --------------------
// The ctor: Screen Scale +0x64 = (100, 100), the value +0xB0 = 5, the two asset
// properties "Tick Off Texture" (+0x78; its object at +0xA8) and "Tick On
// Texture" (+0x90; +0xAC), one input plug "Set" (0x44FE10 -> 0x44FCA0).
struct ui_boat_stat_t
{
    std::int32_t m_value = 5;   // +0xB0
};

// The fields the draw reads (the loaded properties).
struct ui_boat_stat_fields_t
{
    float m_screen_scale[ 2 ] = { 100.0f, 100.0f };  // +0x64 "Screen Scale"
    float m_position[ 2 ] = { 0.0f, 0.0f };          // +0x6C "Position X" / "Position Y"
    float m_spacing = 0.0f;                          // +0x74 "Spacing"
    float m_depth = 0.0f;                            // the draw component (+0x60) +0x14 "Depth"
};

// "Set" 0x44FCA0 (stdcall VuRetVal* out, const VuParams&): +0xB0 = the first
// record read as an int (0x4237B0 over { p, p +128 }); VuRetVal 0.
inline std::uint32_t ui_boat_stat_set( ui_boat_stat_t& s, const ui_params_t& p )
{
    std::uint8_t view[ 132 ];
    std::memcpy( view, p.m_bytes, 128 );
    std::memcpy( view + 128, &p.m_size, 4 );
    ui_params_reader_t r( p );
    r.m_at = view;
    s.m_value = ui_params_read_int( r );
    return 0;
}

// 0x44FB60 (the draw component's member; its alpha argument unread): ten
// ticks from Position: tick i uses the On texture while i < +0xB0 (signed),
// else the Off one (each the property's +0x4C asset -> +0x58 - `off` / `on`,
// null = nothing drawn); the texture draw 0x507360 at ((1/sx) x, (1/sy) y,
// (1/sx) width, (1/sy) height) - the texture's own size - uv (0, 0, 1, 1),
// white, depth Depth x 0.005 + 0.5; x = x + Spacing per tick, drawn or not
// (y stays Position Y).
inline void ui_boat_stat_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_boat_stat_fields_t& f, const ui_boat_stat_t& s,
                               const ui_texture_t* off, const ui_texture_t* on )
{
    float x = f.m_position[ 0 ];
    for ( int i = 0; i < 10; ++i )
    {
        const ui_texture_t* t = i < s.m_value ? on : off;
        if ( t )
        {
            const float ix = 1.0f / f.m_screen_scale[ 0 ];
            const float iy = 1.0f / f.m_screen_scale[ 1 ];
            const float rect[ 4 ] = { ix * x, iy * f.m_position[ 1 ], ix * static_cast< float >( t->m_width ), iy * static_cast< float >( t->m_height ) };
            const float uv[ 4 ] = { 0.0f, 0.0f, 1.0f, 1.0f };
            const float depth = f.m_depth * 0.005f + 0.5f;   // 0x6F75C4, 0x6F765C
            ui_texture_draw( sort, env, t->m_handle, 0xFFFFFFFFu, rect, uv, depth );
        }
        x = x + f.m_spacing;
    }
}

// --- VuLoadingProgressBarEntity (ctor 0x462D70, vtable 0x73D724) ----------
// The ctor: "Initially Visible" +0x80 = 1, the progress +0x84 = 0, visible
// +0x88 = 1; the event "OnLoadProgress" (0x463240) and the input plugs "Show"
// (0x462C40) and "Hide" (0x462C60).
struct ui_progress_bar_t
{
    float m_progress = 0.0f;   // +0x84
    std::uint8_t m_visible = 1; // +0x88
};

struct ui_progress_bar_fields_t
{
    float m_screen_scale[ 2 ] = { 1080.0f, 720.0f };  // +0x64 "Screen Scale" (0x44870000, 0x44340000)
    float m_rect[ 4 ] = { };                          // +0x6C "Rect"
    std::uint32_t m_colour = 0xFFFFFFFFu;             // +0x7C "Color"
    std::uint8_t m_initially_visible = 1;             // +0x80 "Initially Visible"
    float m_depth = 0.0f;                             // the draw component (+0x60) +0x14 "Depth"
};

// Slot 6 0x463220: progress = 0, visible = Initially Visible.
inline void ui_progress_bar_initialize( ui_progress_bar_t& b, const ui_progress_bar_fields_t& f )
{
    b.m_progress = 0.0f;
    b.m_visible = f.m_initially_visible;
}

// "Show" 0x462C40 / "Hide" 0x462C60: visible = 1 / 0; VuRetVal 0.
inline std::uint32_t ui_progress_bar_show( ui_progress_bar_t& b )
{
    b.m_visible = 1;
    return 0;
}

inline std::uint32_t ui_progress_bar_hide( ui_progress_bar_t& b )
{
    b.m_visible = 0;
    return 0;
}

// "OnLoadProgress" 0x463240 (thiscall; const VuParams&; ret 4): progress = the
// first record's float when the size (+0x80) is above 0 and its type word
// (memcpy_s of 4 bytes) is 2, else 0.0.
inline void ui_progress_bar_on_load_progress( ui_progress_bar_t& b, const ui_params_t& p )
{
    float value = 0.0f;
    if ( p.m_size > 0 )
    {
        std::uint32_t type = 0xFFFFFFFFu;
        std::memcpy( &type, p.m_bytes, 4 );
        if ( type == 2 )
            std::memcpy( &value, p.m_bytes + 4, 4 );
    }
    b.m_progress = value;
}

// 0x4632E0 (the draw component's member; the alpha argument): nothing unless
// visible; the colour's alpha byte = the low byte of fistp((float)(colour >>
// 24) x alpha); the solid rect 0x507830 at ((1/sx) x, (1/sy) y, (1/sx) x
// (progress x width), (1/sy) height), depth Depth x 0.005 + 0.5.
inline void ui_progress_bar_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_progress_bar_fields_t& f, const ui_progress_bar_t& b,
                                  float alpha )
{
    if ( !b.m_visible )
        return;
    const float width = b.m_progress * f.m_rect[ 2 ];
    const std::int32_t a = ui_fistp32( static_cast< float >( static_cast< std::int32_t >( f.m_colour >> 24 ) ) * alpha );
    const std::uint32_t colour = ( f.m_colour & 0x00FFFFFFu ) | ( static_cast< std::uint32_t >( a & 0xFF ) << 24 );
    const float ix = 1.0f / f.m_screen_scale[ 0 ];
    const float iy = 1.0f / f.m_screen_scale[ 1 ];
    const float rect[ 4 ] = { ix * f.m_rect[ 0 ], iy * f.m_rect[ 1 ], ix * width, iy * f.m_rect[ 3 ] };
    const float depth = f.m_depth * 0.005f + 0.5f;   // 0x6F75C4, 0x6F765C
    ui_solid_draw( sort, env, colour, rect, depth );
}
