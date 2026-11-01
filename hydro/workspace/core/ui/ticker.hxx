#pragma once

#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/leave.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

// VuInfoUITextEntity's ticker - the runtime state its draw (ui_info_text_draw,
// slot 14 0x446670) reads at +0x12C..+0x13C. The ctor 0x446120 (after the text
// base 0x627070) stores InfoScrollDelay +0x10C = 4.0 and InfoScrollSpeed +0x110
// = 50.0 (both real properties), the id VuString +0x114 (size +0x124) empty,
// +0x12C..+0x13C = 0, the text VuString +0x140 empty, and registers three
// event handlers (VuEventMap 0x5156E0, delegate vtable 0x73BBF8):
//   OnClearInfoStringID 0x446450, OnSetInfoStringID 0x446480, OnUITick 0x446580.
// Its slot 12 (the text getter) is 0x446860. All four transliterated from the
// disassembly below.
struct ui_info_ticker_t
{
    float m_delay = 4.0f;     // +0x10C InfoScrollDelay (0x40800000)
    float m_speed = 50.0f;    // +0x110 InfoScrollSpeed (0x42480000)
    std::string m_id;         // +0x114 (VuString; +0x124 its size)
    ui_info_scroll_t m_scroll; // +0x12C extent, +0x130 gap, +0x138 x, +0x13C y
    float m_time = 0.0f;      // +0x134
    std::string m_text;       // +0x140 (slot 12's result)
};

// What the handlers reach outside the entity. Each a HOOK / input:
//   m_string  - VuStringDB (g 0x759078) slot 7 (+0x1C) on the id: the string layer;
//   m_measure - 0x5373D0 (thiscall font, edx text, [esp+4] the params +0xA0, xmm3 =
//               scale x / scale y; the width in xmm0): VuFont's text measure over
//               its glyph tables - NOT recreated (the fonts are not loaded by the
//               recreation): an input.
struct ui_info_hooks_t
{
    std::function< std::string( const std::string& id ) > m_string;
    std::function< float( std::uint32_t font, const std::string& text, float aspect ) > m_measure;
};

// The ctor's state with the loaded properties (InfoScrollDelay / InfoScrollSpeed).
inline ui_info_ticker_t ui_info_ticker_of( const entity_t& e )
{
    ui_info_ticker_t t;
    if ( const ui_slot_t* s = ui_property_value( e, "InfoScrollDelay" ) )
        std::memcpy( &t.m_delay, s->m_bits, 4 );
    if ( const ui_slot_t* s = ui_property_value( e, "InfoScrollSpeed" ) )
        std::memcpy( &t.m_speed, s->m_bits, 4 );
    return t;
}

// 0x446450 OnClearInfoStringID: the id's size = 0 and its first byte 0 (+0x12C.. kept).
inline void ui_info_clear( ui_info_ticker_t& t )
{
    t.m_id.clear( );
}

// 0x446480 OnSetInfoStringID( params ): the string read 0x505260 (`id`); the same
// id (0x404990: same size, same bytes) does nothing; else the id is assigned
// (strlen - VuString_assign 0x401C70) and, with a non-empty id and a font
// (0x626B80 on +0x80: the asset's, else VuUIImpl's default - `font`, 0 = none),
// the text = VuStringDB slot 7 on the id, w = 0x5373D0( font, text, params,
// scale x / scale y ), extent = scale x * w, gap = Width x 0.25 (0x6F762C) +
// extent, time = 0, x = 0 (+0x13C kept).
inline void ui_info_set( ui_info_ticker_t& t, const char* id, const float screen_scale[ 2 ], float width, std::uint32_t font,
                         const ui_info_hooks_t& hooks )
{
    if ( t.m_id == id )
        return;
    t.m_id = id;
    if ( t.m_id.empty( ) || !font )
        return;
    const std::string text = hooks.m_string ? hooks.m_string( t.m_id ) : std::string( );
    const float aspect = screen_scale[ 0 ] / screen_scale[ 1 ];
    const float w = hooks.m_measure ? hooks.m_measure( font, text, aspect ) : 0.0f;
    const float extent = screen_scale[ 0 ] * w;
    t.m_scroll.m_extent = extent;
    t.m_time = 0.0f;
    t.m_scroll.m_x = 0.0f;
    t.m_scroll.m_gap = width * 0.25f + extent;
}

// 0x446580 OnUITick( params ): its params are read (a type-2 float) and then
// overwritten - the delta is the tick manager's (g 0x759008 slot 15, x87): `delta`.
// Only while extent is ordered above Width (+0x78): time += delta; past the delay
// (ordered), x -= speed x delta, and when -gap is ordered above x, x = gap + x.
inline void ui_info_tick( ui_info_ticker_t& t, float width, float delta )
{
    if ( !( t.m_scroll.m_extent > width ) )
        return;
    t.m_time = t.m_time + delta;
    if ( !( t.m_time > t.m_delay ) )
        return;
    const float x = t.m_scroll.m_x - t.m_speed * delta;
    t.m_scroll.m_x = x;
    if ( -t.m_scroll.m_gap > x )
        t.m_scroll.m_x = t.m_scroll.m_gap + x;
}

// 0x446860 slot 12: an empty id -> the text "" (assign of the empty literal
// 0x64C873); else the text = VuStringDB slot 7 on the id (0x401DB0 assign unless
// it is the same object). Returns the text (+0x140).
inline const std::string& ui_info_text( ui_info_ticker_t& t, const ui_info_hooks_t& hooks )
{
    if ( t.m_id.empty( ) )
        t.m_text.clear( );
    else
        t.m_text = hooks.m_string ? hooks.m_string( t.m_id ) : std::string( );
    return t.m_text;
}
