#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// The UI's text draws as data: VuUITextBaseEntity's draw (slot 14 0x627C60)
// through the font draw 0x536C10 into VuGfxSort's command memory (0x426B80)
// and its command list (0x429010), transliterated. Nothing here touches a
// device: a record is the bytes retail leaves in the sort's memory and a
// command the 32 bytes it appends; the command's callback 0x5375E0 (the draw
// itself, at submit time) is the GPU side and is NOT recreated.
//
// Handles: retail stores pointers (the VuFont, the font draw's shader set);
// here they are opaque 32-bit values the caller hands in, stored where retail
// stores the pointer.

// VuUIImpl (the singleton 0x81A0F8, made by 0x626190 on first use) +12: the
// crop matrix. 0x626190 copies the identity 0x7593B0 there; setCropMatrix
// 0x626750 copies its argument there and to +76, then inverts +76 (0x4157C0 -
// read by nothing in this chain, NOT recreated). +0x94 (0x81A18C) is the
// default font asset ("Dev", 0x626210).
struct ui_crop_t
{
    float m_matrix[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
};

// 0x6282D0 (this = the text base's anchor block +0xF0: +0 horizontal anchor,
// +4 vertical, +8 / +12 the ratios; src, dst): dst = src, then per anchor
// (none 0 / left 1 / right 2 / left-right 3; none 0 / top 4 / bottom 5 /
// top-bottom 6) over the crop's scale (m0, m5) and offset (m12, m13).
struct ui_anchor_t
{
    std::int32_t m_horizontal = 0;  // +0xF0
    std::int32_t m_vertical = 0;    // +0xF4
    float m_h_ratio = 1.0f;         // +0xF8
    float m_v_ratio = 1.0f;         // +0xFC
};

inline void ui_anchor_transform( const ui_anchor_t& a, const ui_crop_t& crop, const float src[ 4 ], float dst[ 4 ] )
{
    const float sx = crop.m_matrix[ 0 ];   // 0x81A104
    const float sy = crop.m_matrix[ 5 ];   // 0x81A118
    const float tx = crop.m_matrix[ 12 ];  // 0x81A134
    const float ty = crop.m_matrix[ 13 ];  // 0x81A138
    float r[ 4 ] = { src[ 0 ], src[ 1 ], src[ 2 ], src[ 3 ] };
    if ( a.m_horizontal == 1 )
        r[ 0 ] = r[ 0 ] - a.m_h_ratio * tx / sx;
    if ( a.m_horizontal == 2 )
        r[ 0 ] = ( 1.0f - tx - sx ) * a.m_h_ratio / sx + r[ 0 ];
    if ( a.m_horizontal == 3 )
    {
        r[ 0 ] = r[ 0 ] - a.m_h_ratio * tx / sx;
        r[ 2 ] = ( 1.0f - sx ) * a.m_h_ratio / sx + r[ 2 ];
    }
    if ( a.m_vertical == 4 )
        r[ 1 ] = r[ 1 ] - a.m_v_ratio * ty / sy;
    if ( a.m_vertical == 5 )
        r[ 1 ] = ( 1.0f - ty - sy ) * a.m_v_ratio / sy + r[ 1 ];
    if ( a.m_vertical == 6 )
    {
        r[ 1 ] = r[ 1 ] - a.m_v_ratio * ty / sy;
        r[ 3 ] = ( 1.0f - sy ) * a.m_v_ratio / sy + r[ 3 ];
    }
    std::memcpy( dst, r, sizeof( r ) );
}

// 0x5546B0 (this = the text base's +0xE4: +0 horizontal alignment, +4
// vertical, +8 No Clip, +9 Single Line, +10 Wordbreak): the font flags.
// "right-ea-left" (6) asks VuStringDB (g 0x759078) slot 12; no StringDB, or a
// false answer, is right (1), a true one left (0).
struct ui_string_db_t
{
    bool m_present = false;
    bool m_slot12 = false;   // HOOK: the StringDB's own answer (its tables are not loaded)
};

inline std::uint32_t ui_font_flags( std::int32_t h_align, std::int32_t v_align, std::uint8_t no_clip, std::uint8_t single_line,
                                    std::uint8_t wordbreak, const ui_string_db_t& db )
{
    std::uint32_t f = 0;
    if ( h_align == 4 )
        f = 2;
    else if ( h_align == 1 || ( h_align == 6 && ( !db.m_present || !db.m_slot12 ) ) )
        f = 1;
    if ( v_align == 4 )
        f |= 0x10;
    else if ( v_align == 3 )
        f |= 4;
    else if ( v_align == 5 )
        f |= 8;
    if ( no_clip )
        f |= 0x20;
    if ( single_line )
        f |= 0x40;
    if ( wordbreak )
        f |= 0x80;
    return f;
}

// x87 fistp under the default control word (nearest, ties to even) into an
// int32; NaN / out of range -> 0x80000000.
inline std::int32_t ui_fistp32( float x )
{
    const double d = x;
    if ( !( d >= -2147483648.5 && d < 2147483647.5 ) )
        return static_cast< std::int32_t >( 0x80000000u );
    return static_cast< std::int32_t >( std::nearbyint( d ) );
}

// fistp qword with the rounding control forced to truncate (0x429010's
// fnstcw / or 0xC00 / fldcw); NaN / out of range -> 0x8000000000000000.
inline std::int64_t ui_fistp64_trunc( float x )
{
    const double d = x;
    if ( !( d >= -9223372036854775808.0 && d < 9223372036854775808.0 ) )
        return static_cast< std::int64_t >( 0x8000000000000000ull );
    return static_cast< std::int64_t >( d );
}

// VuGfxSort (g 0x759074): +0x64 the current buffer; per buffer i a command
// memory (+0x34 + 12i: base, used, capacity) and a command vector (+0x4C + 12i:
// begin, count, capacity; 32-byte commands); +0x30 the last allocation's
// offset; +0x70 / +0x74 the key bits the current screen / layer set (an input
// here: their setters are not this chain's).
struct ui_gfx_command_t
{
    std::uint32_t m_words[ 8 ] = { };   // +0 / +4 the key, +8 the callback, +0xC the data offset, +0x10 the material,
                                        // +0x14 the 3rd arg, +0x18 the type (a word store; the rest untouched)
};

struct ui_gfx_sort_t
{
    struct buffer_t
    {
        std::vector< std::uint8_t > m_memory;
        std::int32_t m_used = 0;
        std::int32_t m_capacity = 0;
    };

    // 0x426B80 (size, a 2nd arg it never reads): the next 16-aligned offset
    // into the current buffer, stored at +0x30; past the capacity it grows to
    // max(capacity + capacity / 2, the new end) (signed compares; _aligned_malloc
    // + memcpy of the old capacity - here a resize. NOT exercised by the oracle).
    std::uint8_t* allocate( std::int32_t size )
    {
        buffer_t& b = m_buffers[ m_index ];
        const std::int32_t at = ( b.m_used + 15 ) & ~15;
        m_offset = at;
        const std::int32_t end = size + at;
        if ( end > b.m_capacity )
        {
            std::int32_t grow = b.m_capacity / 2 + b.m_capacity;
            if ( end > grow )
                grow = end;
            if ( grow > b.m_capacity )
            {
                b.m_memory.resize( static_cast< std::size_t >( grow ) );
                b.m_capacity = grow;
            }
        }
        b.m_used = end;
        return b.m_memory.data( ) + m_offset;
    }

    // 0x429010 (type, material, a3, callback, depth): the 64-bit key - type,
    // the depth as 24 bits of (1 - depth) x 16777215 (truncated) with bit 24
    // set, the material's +0x6C8, the sort's +0x70 / +0x74, a3's +0xC - then one
    // more command (0x428DC0 resize) filled from it.
    void submit( std::uint32_t type, std::uint32_t material, std::uint32_t material_key, std::uint32_t a3, std::uint32_t a3_key,
                 std::uint32_t callback, float depth )
    {
        const float scaled = ( 1.0f - depth ) * 16777215.0f;  // 0x6F784C
        const std::uint32_t bits = static_cast< std::uint32_t >( ui_fistp64_trunc( scaled ) );
        std::uint64_t key = ( static_cast< std::uint64_t >( type ) << 25 ) | ( bits & 0xFFFFFFu ) | 0x1000000u;
        key = ( key << 10 ) | material_key;
        key = ( key << 11 ) | ( static_cast< std::uint64_t >( m_key_hi ) << 32 ) | m_key_lo;
        if ( a3 )
            key |= a3_key;
        std::vector< ui_gfx_command_t >& list = m_commands[ m_index ];
        list.resize( list.size( ) + 1 );
        ui_gfx_command_t& c = list.back( );
        c.m_words[ 0 ] = static_cast< std::uint32_t >( key );
        c.m_words[ 1 ] = static_cast< std::uint32_t >( key >> 32 );
        c.m_words[ 2 ] = callback;
        c.m_words[ 3 ] = static_cast< std::uint32_t >( m_offset );
        c.m_words[ 4 ] = material;
        c.m_words[ 5 ] = a3;
        c.m_words[ 6 ] = ( c.m_words[ 6 ] & 0xFFFF0000u ) | ( type & 0xFFFFu );
    }

    buffer_t m_buffers[ 2 ];
    std::vector< ui_gfx_command_t > m_commands[ 2 ];
    std::int32_t m_index = 0;     // +0x64
    std::int32_t m_offset = 0;    // +0x30
    std::uint32_t m_key_lo = 0;   // +0x70
    std::uint32_t m_key_hi = 0;   // +0x74
};

// What the font draw reads of VuGfxUtil (g 0x75902C): the matrix stack's top
// (+0x20, a std::deque of matrices: 0x5072C0) - m0, m5, m12, m13 are read -
// the float stack's top (+0x34: 0x507310, stored in the record), and the font
// draw at +0x14 whose +4 table (24-byte rows) holds a shader set per outline
// (row +4: the material handed to the sort, its +0x6C8 the key bits).
struct ui_font_draw_env_t
{
    float m_matrix[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    float m_top = 1.0f;
    std::uint32_t m_material[ 2 ] = { };
    std::uint32_t m_material_key[ 2 ] = { };
    // The texture draw's: **(VuGfxUtil +4) +0x14 (the material handed to the sort) and its +0x6C8.
    std::uint32_t m_texture_material = 0;
    std::uint32_t m_texture_material_key = 0;
    // The rect draw's: **(VuGfxUtil +4) +8 and its +0x6C8.
    std::uint32_t m_rect_material = 0;
    std::uint32_t m_rect_material_key = 0;
};

constexpr std::uint32_t k_ui_texture_callback = 0x507430u;  // the texture record's draw at submit (GPU side)
constexpr int k_ui_texture_record = 104;

// 0x507360 (VuGfxUtil; depth in xmm1; texture, colour*, rect A*, rect B*, a
// 5th arg never read): the 104-byte record - +0 the texture, +4 the matrix
// stack's top (all 64 bytes: 0x4BC130 / 0x4BC2E0, the deque's back), +0x44 the
// colour, +0x48 rect B, +0x58 rect A (the callback 0x507430 reads both as x, y,
// w, h) - then the command (type 17, the material, 0, 0x507430, depth). No
// test of the texture or the rects.
inline void ui_texture_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, std::uint32_t texture, std::uint32_t colour,
                             const float a[ 4 ], const float b[ 4 ], float depth )
{
    std::uint8_t* r = sort.allocate( k_ui_texture_record );
    std::memcpy( r + 0, &texture, 4 );
    std::memcpy( r + 4, env.m_matrix, 64 );
    std::memcpy( r + 0x44, &colour, 4 );
    std::memcpy( r + 0x48, b, 16 );
    std::memcpy( r + 0x58, a, 16 );
    sort.submit( 17, env.m_texture_material, env.m_texture_material_key, 0, 0, k_ui_texture_callback, depth );
}

constexpr std::uint32_t k_ui_rect_callback = 0x507AB0u;  // the rect record's draw at submit (GPU side)
constexpr int k_ui_rect_record = 84;

// 0x507A00 (VuGfxUtil; depth in xmm1; colour*, rect*; ret 8): the 84-byte
// record - +0 the matrix stack's top (64 bytes, 0x4BC130 / 0x4BC2E0), +0x40 the
// colour, +0x44 the rect (x, y, w, h) - then the command (type 17, the
// material ***(VuGfxUtil +4) +8, 0, 0x507AB0, depth). No test of anything.
inline void ui_rect_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, std::uint32_t colour, const float rect[ 4 ], float depth )
{
    std::uint8_t* r = sort.allocate( k_ui_rect_record );
    std::memcpy( r + 0, env.m_matrix, 64 );
    std::memcpy( r + 0x40, &colour, 4 );
    std::memcpy( r + 0x44, rect, 16 );
    sort.submit( 17, env.m_rect_material, env.m_rect_material_key, 0, 0, k_ui_rect_callback, depth );
}

constexpr std::uint32_t k_ui_font_callback = 0x5375E0u;   // the record's draw at submit (GPU side)
constexpr int k_ui_font_record = 121;                      // the record's fixed bytes (+120 the text, NUL-terminated)

// 0x536C10 (the font draw; font, text, params - the 60-byte VuFontDrawParams
// - rect, flags, alpha scale, offset; the depth in xmm1). Nothing without a
// font, a size above 0 (NaN passes), or a non-empty text. Flags bit 0x1000 is
// the immediate path (the device's own draw through 0x537600): NOT recreated,
// returns false; the text base never sets it (0x5546B0 stops at 0x80).
inline bool ui_font_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, std::uint32_t font, const char* text,
                          const std::uint8_t params[ 60 ], const float rect[ 4 ], std::uint32_t flags, float alpha, const float offset[ 2 ],
                          float depth )
{
    float p[ 15 ];
    std::memcpy( p, params, 60 );
    if ( !font || 0.0f >= p[ 0 ] || !text || !*text )
        return true;
    const float* m = env.m_matrix;
    const float out_rect[ 4 ] = { rect[ 0 ] * m[ 0 ] + m[ 12 ], rect[ 1 ] * m[ 5 ] + m[ 13 ], rect[ 2 ] * m[ 0 ], rect[ 3 ] * m[ 5 ] };
    const float out_offset[ 2 ] = { offset[ 0 ] * m[ 0 ], offset[ 1 ] * m[ 5 ] };
    float clip[ 4 ] = { 0.0f, 0.0f, 1.0f, 1.0f };
    if ( params[ 40 ] )
    {
        const float x = p[ 11 ] * m[ 0 ] + m[ 12 ];
        const float w = p[ 13 ] * m[ 0 ];
        const float y = p[ 12 ] * m[ 5 ] + m[ 13 ];
        const float h = p[ 14 ] * m[ 5 ];
        // maxss 0, v / minss 1, v: the second operand on NaN (and on equal)
        const float x0 = 0.0f > x ? 0.0f : x;
        const float y0 = 0.0f > y ? 0.0f : y;
        const float right = x + w;
        const float bottom = y + h;
        clip[ 0 ] = x0;
        clip[ 1 ] = y0;
        clip[ 2 ] = ( 1.0f < right ? 1.0f : right ) - x0;
        clip[ 3 ] = ( 1.0f < bottom ? 1.0f : bottom ) - y0;
    }
    const std::uint32_t outline = p[ 4 ] > 0.0f ? 1u : 0u;  // comiss / seta: NaN -> 0
    if ( flags & 0x1000 )
        return false;
    const std::size_t length = std::strlen( text );
    std::uint8_t* r = sort.allocate( static_cast< std::int32_t >( length ) + k_ui_font_record );
    std::memcpy( r + 0, &font, 4 );
    std::memcpy( r + 4, &outline, 4 );
    std::memcpy( r + 8, params, 60 );
    std::memcpy( r + 68, out_rect, 16 );
    std::memcpy( r + 84, &flags, 4 );
    std::memcpy( r + 88, &env.m_top, 4 );
    std::memcpy( r + 92, &depth, 4 );
    std::memcpy( r + 96, out_offset, 8 );
    std::memcpy( r + 104, clip, 16 );
    std::memcpy( r + 120, text, length + 1 );   // strcpy_s( +120, length + 1, text )
    // the colour's and the outline colour's alpha bytes (+23, +35) x the alpha: cvtdq2ps, mulss, fistp
    r[ 23 ] = static_cast< std::uint8_t >( ui_fistp32( static_cast< float >( r[ 23 ] ) * alpha ) );
    r[ 35 ] = static_cast< std::uint8_t >( ui_fistp32( static_cast< float >( r[ 35 ] ) * alpha ) );
    sort.submit( 17, env.m_material[ outline ], env.m_material_key[ outline ], 0, 0, k_ui_font_callback, depth );
    return true;
}

// The VuUITextBaseEntity fields the draw reads (ctor 0x627070).
struct ui_text_base_t
{
    float m_screen_scale[ 2 ] = { 1080.0f, 720.0f };  // +0x68 "Screen Scale"
    float m_rect[ 4 ] = { 0.0f, 0.0f, 100.0f, 100.0f }; // +0x70 X, Y, Width, Height
    std::uint8_t m_params[ 60 ] = { };                 // +0xA0 the VuUIFont's draw params (+0x20 of +0x80)
    std::int32_t m_h_align = 0;                        // +0xE4
    std::int32_t m_v_align = 2;                        // +0xE8
    std::uint8_t m_no_clip = 0;                        // +0xEC
    std::uint8_t m_single_line = 0;                    // +0xED
    std::uint8_t m_wordbreak = 0;                      // +0xEE
    ui_anchor_t m_anchor;                              // +0xF0
    float m_offset[ 2 ] = { 0.0f, 0.0f };              // +0x100
    float m_alpha = 1.0f;                              // +0x108
    float m_depth = 0.0f;                              // the draw component (+0x60) +0x14 "Depth"
};

// The font 0x626B80 answers: the VuUIFont's asset (+0xE0) +88, else VuUIImpl's
// default font asset (0x81A18C) +88, else none.
struct ui_text_font_t
{
    bool m_asset = false;
    std::uint32_t m_asset_font = 0;
    bool m_default = false;
    std::uint32_t m_default_font = 0;
};

// 0x627C60 (slot 14; its 1st arg unread, the 2nd an alpha factor): the text
// (slot 12) in the rect over the screen scale, through the anchor transform,
// the offset over the screen scale, the depth 0.005 x Depth + 0.5, the alpha x
// the factor, the font flags -> the font draw. false: a path not recreated.
inline bool ui_text_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_crop_t& crop, const ui_string_db_t& db,
                          const ui_text_base_t& t, const ui_text_font_t& font, const std::string& text, float factor )
{
    const std::uint32_t f = font.m_asset ? font.m_asset_font : font.m_default ? font.m_default_font : 0u;
    if ( !f || text.empty( ) )
        return true;
    const float ix = 1.0f / t.m_screen_scale[ 0 ];
    const float iy = 1.0f / t.m_screen_scale[ 1 ];
    float rect[ 4 ] = { ix * t.m_rect[ 0 ], t.m_rect[ 1 ] * iy, t.m_rect[ 2 ] * ix, t.m_rect[ 3 ] * iy };
    ui_anchor_transform( t.m_anchor, crop, rect, rect );
    const float offset[ 2 ] = { t.m_offset[ 0 ] / t.m_screen_scale[ 0 ], t.m_offset[ 1 ] / t.m_screen_scale[ 1 ] };
    const float depth = t.m_depth * 0.005f + 0.5f;  // 0x6F75C4, 0x6F765C
    const float alpha = t.m_alpha * factor;
    const std::uint32_t flags = ui_font_flags( t.m_h_align, t.m_v_align, t.m_no_clip, t.m_single_line, t.m_wordbreak, db );
    return ui_font_draw( sort, env, f, text.c_str( ), t.m_params, rect, flags, alpha, offset, depth );
}
