#pragma once

#include <workspace/core/ui/combo.hxx>
#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/frame.hxx>
#include <workspace/core/ui/fsm.hxx>
#include <workspace/core/ui/image.hxx>
#include <workspace/core/ui/results.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

// The full leaderboard (VuUILeaderboardEntity, ctor 0x477FD0, vtable 0x73F2C8 -
// Screens/Leaderboard), transliterated from the disassembly (batch 22). Like the
// mini board (board.hxx) its rows come from the platform's leaderboard manager g
// 0x758FA8 (an online service: a HOOK - the row count and the rows are the
// caller's); the draws are 1:1.
//
// Fields (the ctor's defaults): +0x68 Screen Scale (100, 100), +0x70 / +0x74
// Position X / Y, +0x78 ComboBoxSpacing, +0x7C the combo style block (ctor
// 0x4569F0, properties 0x456BD0 - combo.hxx ui_combo_block_t), +0x25C..+0x278
// TouchLeft / TouchRight, +0x27C RowCount (10), +0x280..+0x28C RowRect, +0x290
// RowSpacing, +0x294 / +0x298 / +0x29C Rank / Name / ScoreOffsetX, the VuUIFont
// blocks (0x626900: +0x20 the 60-byte params, +0x60 the asset) RankFont +0x2A0,
// NameFont +0x304, ScoreFont +0x368, InfoFont +0x3DC, ButtonFont +0x44C, +0x3CC
// InfoRect, +0x440 the info text flags block (0x5546B0), +0x4B0 ButtonTouch, +0x4C0
// ButtonImage, +0x4D0 ButtonText, +0x4E0 ButtonSpacing, the asset properties
// ButtonImage +0x4E4 / ButtonSelectedImage +0x500 / ButtonPressedImage +0x51C (28
// bytes, +0x18 the asset), the two combo records +0x538 (52 bytes: "Option_Track",
// "Option_Mode"), +0x5A0 the focus (0 / 1 the combos, 2 / 3 the buttons), +0x5A8 the
// VuFSM, +0x5DC the touch press (1 / 2 a combo's left / right arrow, 3 a button).
//
// The VuFSM (built by the ctor): "Read" (enter 0x47BC90, exit 0x47BE70, tick
// 0x47BEA0, draw 0x47BEF0), "Show" (draw 0x47BF90), "NotConnected" (draw 0x47C720);
// Read -> Show "Success", Read -> NotConnected "Failure", Show / NotConnected ->
// Read "Refresh" (ctor 0x4799B4..0x479A87). The state draws (batch 22) and the
// per-frame half (batch 23: c_ui_leaderboard below - slot 6 0x479C50, slot 7
// 0x479CF0, "Decision" 0x47B640, Read's enter / exit / tick, the combo change
// 0x47A1B0, the page 0x47A280, the touch callbacks) are recreated here.

inline constexpr std::uint32_t k_ui_leaderboard_vtable = 0x73F2C8u;

struct ui_leaderboard_t
{
    float m_scale[ 2 ] = { 100.0f, 100.0f };   // +0x68
    float m_pos[ 2 ] = { 0.0f, 0.0f };         // +0x70
    float m_combo_spacing = 0.0f;              // +0x78 ComboBoxSpacing
    float m_left[ 4 ] = { };                   // +0x25C TouchLeft X / Y / Width / Height
    float m_right[ 4 ] = { };                  // +0x26C TouchRight
    std::int32_t m_rows = 10;                  // +0x27C RowCount
    float m_row[ 4 ] = { };                    // +0x280 RowRect
    float m_spacing = 0.0f;                    // +0x290 RowSpacing
    float m_rank_x = 0.0f;                     // +0x294 RankOffsetX
    float m_name_x = 0.0f;                     // +0x298 NameOffsetX
    float m_score_x = 0.0f;                    // +0x29C ScoreOffsetX
    float m_info[ 4 ] = { };                   // +0x3CC InfoRect
    std::int32_t m_h_align = 0;                // +0x440
    std::int32_t m_v_align = 2;                // +0x444
    std::uint8_t m_no_clip = 0, m_single_line = 0, m_wordbreak = 0;   // +0x448..+0x44A
    float m_button_touch[ 4 ] = { };           // +0x4B0 ButtonTouch
    float m_button_image[ 4 ] = { };           // +0x4C0 ButtonImage
    float m_button_text[ 4 ] = { };            // +0x4D0 ButtonText
    float m_button_spacing = 0.0f;             // +0x4E0 ButtonSpacing
    float m_depth = 0.0f;                      // the draw component's (+0x60) +0x14 Depth
};

// The fields from OUR loaded entity (its property values, defaults included).
inline ui_leaderboard_t ui_leaderboard_of( const entity_t& e )
{
    ui_leaderboard_t b;
    const auto bits = [ &e ]( const char* name, void* out, std::size_t n )
    {
        if ( const ui_slot_t* v = ui_property_value( e, name ) )
            std::memcpy( out, v->m_bits, n );
    };
    const auto rect = [ &bits ]( const char* stem, float out[ 4 ] )
    {
        const char* const parts[ 4 ] = { " X", " Y", " Width", " Height" };
        for ( int k = 0; k < 4; ++k )
            bits( ( std::string( stem ) + parts[ k ] ).c_str( ), &out[ k ], 4 );
    };
    bits( "Screen Scale", b.m_scale, 8 );
    bits( "Position X", &b.m_pos[ 0 ], 4 );
    bits( "Position Y", &b.m_pos[ 1 ], 4 );
    bits( "ComboBoxSpacing", &b.m_combo_spacing, 4 );
    rect( "TouchLeft", b.m_left );
    rect( "TouchRight", b.m_right );
    bits( "RowCount", &b.m_rows, 4 );
    rect( "RowRect", b.m_row );
    bits( "RowSpacing", &b.m_spacing, 4 );
    bits( "RankOffsetX", &b.m_rank_x, 4 );
    bits( "NameOffsetX", &b.m_name_x, 4 );
    bits( "ScoreOffsetX", &b.m_score_x, 4 );
    bits( "InfoRect", b.m_info, 16 );
    bits( "InfoHorizontal Alignment", &b.m_h_align, 4 );
    bits( "InfoVertical Alignment", &b.m_v_align, 4 );
    bits( "InfoNo Clip", &b.m_no_clip, 1 );
    bits( "InfoSingle Line", &b.m_single_line, 1 );
    bits( "InfoWordbreak", &b.m_wordbreak, 1 );
    rect( "ButtonTouch", b.m_button_touch );
    rect( "ButtonImage", b.m_button_image );
    rect( "ButtonText", b.m_button_text );
    bits( "ButtonSpacing", &b.m_button_spacing, 4 );
    b.m_depth = ui_frame_depth( e );
    return b;
}

// The ctor's two combo records (0x477FD0, after the FSM's draw component): record 0
// "Option_Track" over the block +0x7C, one entry per OptionDB (gameutil +0x34 asset
// +0x58) ["VuDBAsset"]["Track"] - its first element when an array with one or more,
// else null - ["Choices"] (0x4FBC40 count, re-read per pass; the element when an
// array and in range, else null): ["Value"]'s text (tag 4, else ""), then
// ["VuDBAsset"]["References"] - when a map, its entry of that text (0x4F22B0; absent
// -> null), else null - ["StringID"]'s text (0x457F50: locked 0). Record 1
// "Option_Mode": "Leaderboard_Mode_Race" (0x457F50) then SlalomC / SlalomB / SlalomA /
// TimeTrial / Ghost (0x457830 = the same push, locked 0).
inline void ui_leaderboard_combos( const value_t* option_db, ui_config_combo_t out[ 2 ] )
{
    static const value_t k_none;
    const value_t& db = option_db ? member( *option_db, "VuDBAsset" ) : k_none;
    const value_t& refs = member( db, "References" );
    const value_t& track = member( db, "Track" );
    const value_t& first = track.tag == 5 && !track.list.empty( ) ? track.list[ 0 ] : k_none;
    const value_t& choices = member( first, "Choices" );
    out[ 0 ] = ui_config_combo_t( );
    out[ 0 ].m_string_id = "Option_Track";
    for ( int k = 0; k < ui_json_size( choices ); ++k )
    {
        const value_t& c = choices.tag == 5 && k < static_cast< int >( choices.list.size( ) ) ? choices.list[ static_cast< std::size_t >( k ) ] : k_none;
        const value_t& v = member( c, "Value" );
        const std::string value = v.tag == 4 ? v.text : std::string( );
        const value_t& ref = refs.tag == 6 ? member( refs, value.c_str( ) ) : k_none;
        const value_t& id = member( ref, "StringID" );
        out[ 0 ].m_choice_ids.push_back( id.tag == 4 ? id.text : std::string( ) );
        out[ 0 ].m_locked.push_back( 0 );
    }
    out[ 1 ] = ui_config_combo_t( );
    out[ 1 ].m_string_id = "Option_Mode";
    for ( const char* id : { "Leaderboard_Mode_Race", "Leaderboard_Mode_SlalomC", "Leaderboard_Mode_SlalomB", "Leaderboard_Mode_SlalomA",
                             "Leaderboard_Mode_TimeTrial", "Leaderboard_Mode_Ghost" } )
    {
        out[ 1 ].m_choice_ids.push_back( id );
        out[ 1 ].m_locked.push_back( 0 );
    }
}

// What the draw reaches outside the entity: the sort and the font environment, the
// combo box inputs (the block's fonts / textures, VuStringDB slot 7), the five
// VuUIFonts (0x626B80's answer + their params: Rank, Name, Score, Info, Button), the
// three button images' textures (the property's +0x18 asset -> +0x4C -> +0x58; null =
// none: the solid rect instead), VuStringDB slot 6 (getString: a HOOK), the 0x5546B0
// string-db answer, and the leaderboard manager g 0x758FA8 (HOOKS: vt+0x24 count,
// vt+0x28 row (k, rank*, -, name, score*)).
struct ui_leaderboard_draw_t
{
    ui_gfx_sort_t* m_sort = nullptr;
    const ui_font_draw_env_t* m_env = nullptr;
    ui_combo_inputs_t m_combo;
    ui_results_font_t m_fonts[ 5 ];
    const ui_texture_t* m_buttons[ 3 ] = { };
    ui_string_db_t m_db;
    std::function< std::string( const std::string& ) > m_string;
    std::function< int( ) > m_count;
    std::function< void( int, std::int32_t&, std::string&, std::int32_t& ) > m_row;
};

enum ui_leaderboard_font_t
{
    ui_lb_rank = 0,   // +0x2A0
    ui_lb_name,       // +0x304
    ui_lb_score,      // +0x368
    ui_lb_info,       // +0x3DC
    ui_lb_button      // +0x44C
};

// The entity's runtime state the draw reads.
struct ui_leaderboard_state_t
{
    ui_config_combo_t m_combos[ 2 ];   // +0x538
    std::int32_t m_focus = 0;          // +0x5A0
    std::int32_t m_press = 0;          // +0x5DC
    int m_state = -1;                  // the VuFSM's current state (0 Read, 1 Show, 2 NotConnected; -1 none)
};

// 0x47B570 (thiscall text, VuUIFont*, rect*, flags, float depth offset; ret 0x14): the
// font (0x626B80; none -> nothing), the rect x (1 / scale), the font draw 0x536C10
// (the block's params, flags, alpha 1.0, offset 0, depth = Depth x 0.005 + 0.5 + the
// offset). No anchor transform (unlike the mini board's 0x47E090).
inline bool ui_leaderboard_text( const ui_leaderboard_t& f, const ui_leaderboard_draw_t& d, const ui_results_font_t& font, const float r[ 4 ],
                                 std::uint32_t flags, const char* s, float raise )
{
    if ( !font.m_font )
        return true;
    const float ix = 1.0f / f.m_scale[ 0 ];   // 0x6F76B8 = 1.0
    const float iy = 1.0f / f.m_scale[ 1 ];
    const float rect[ 4 ] = { r[ 0 ] * ix, r[ 1 ] * iy, r[ 2 ] * ix, r[ 3 ] * iy };
    const float offset[ 2 ] = { 0.0f, 0.0f };
    const float depth = f.m_depth * 0.005f + 0.5f + raise;   // 0x6F75C4, 0x6F765C
    return ui_font_draw( *d.m_sort, *d.m_env, font.m_font, s, font.m_params, rect, flags, 1.0f, offset, depth );
}

// Read 0x47BEF0 / NotConnected 0x47C720 / Show-with-no-rows: the info text - rect
// (InfoRect x + Position X, InfoRect y + Position Y, its w, h), flags 0x5546B0 on
// +0x440, the string-db text of `id` (slot 6), InfoFont, depth offset 0.
inline bool ui_leaderboard_info( const ui_leaderboard_t& f, const ui_leaderboard_draw_t& d, const char* id )
{
    const float rect[ 4 ] = { f.m_info[ 0 ] + f.m_pos[ 0 ], f.m_info[ 1 ] + f.m_pos[ 1 ], f.m_info[ 2 ], f.m_info[ 3 ] };
    const std::uint32_t flags = ui_font_flags( f.m_h_align, f.m_v_align, f.m_no_clip, f.m_single_line, f.m_wordbreak, d.m_db );
    const std::string s = d.m_string ? d.m_string( id ) : std::string( );
    return ui_leaderboard_text( f, d, d.m_fonts[ ui_lb_info ], rect, flags, s.c_str( ), 0.0f );
}

// Show's draw 0x47BF90: no rows (vt+0x24 == 0) -> "Leaderboard_NoOneRanked" as the
// info text. Else x = Position X + RowRect X, y = Position Y + (RowRect Height x 0.5
// + RowRect Y); for k while k < the count (re-read each row): the row (vt+0x28: rank,
// name, score); the rect is the min / max of the point with itself (x, y, x - x, y -
// y); the rank as "%d" (sprintf_s, 256 bytes) at RankOffsetX + x with RankFont, the
// name at NameOffsetX + x with NameFont, the score's (float)(-score) x 0.001 as
// results.hxx ui_results_time at ScoreOffsetX + x with ScoreFont (flags 0x30, 0x30,
// 0x31; each through the font draw directly - the inline 0x626B80 - at Depth x 0.005
// + 0.5); y = RowSpacing + y.
inline bool ui_leaderboard_show( const ui_leaderboard_t& f, const ui_leaderboard_draw_t& d )
{
    const int count = d.m_count ? d.m_count( ) : 0;
    if ( count == 0 )
        return ui_leaderboard_info( f, d, "Leaderboard_NoOneRanked" );
    bool known = true;
    const float x = f.m_pos[ 0 ] + f.m_row[ 0 ];
    float y = f.m_pos[ 1 ] + ( f.m_row[ 3 ] * 0.5f + f.m_row[ 1 ] );
    for ( int k = 0; k < ( d.m_count ? d.m_count( ) : 0 ); ++k )
    {
        std::int32_t rank = 0, score = 0;
        std::string name;
        if ( d.m_row )
            d.m_row( k, rank, name, score );
        const float w = x - x;   // maxss( x, x ) - minss( x, x )
        const float h = y - y;
        const float rank_rect[ 4 ] = { f.m_rank_x + x, y, w, h };
        known = ui_leaderboard_text( f, d, d.m_fonts[ ui_lb_rank ], rank_rect, 0x30u, ui_results_itoa( rank ).c_str( ), 0.0f ) && known;
        const float name_rect[ 4 ] = { f.m_name_x + x, y, w, h };
        known = ui_leaderboard_text( f, d, d.m_fonts[ ui_lb_name ], name_rect, 0x30u, name.c_str( ), 0.0f ) && known;
        const float t = static_cast< float >( static_cast< std::int32_t >( 0u - static_cast< std::uint32_t >( score ) ) ) * 0.001f;   // neg, cvtdq2ps, 0x6F75B0
        const float score_rect[ 4 ] = { f.m_score_x + x, y, w, h };
        known = ui_leaderboard_text( f, d, d.m_fonts[ ui_lb_score ], score_rect, 0x31u, ui_results_time( t ).c_str( ), 0.0f ) && known;
        y = f.m_spacing + y;
    }
    return known;
}

// The draw component's member 0x47A360 (float alpha - never read; ret 4):
//  1. from Position, the two combo records (focused byte +0x28 := focus == k, written
//     into the entity) through 0x4578D0 at the Screen Scale and Depth x 0.005 + 0.5;
//     y = ComboBoxSpacing + y.
//  2. the two buttons at ButtonImage + Position (w, h ButtonImage's): the image is
//     ButtonImage, or for the focused button (focus - 2 == k) ButtonSelectedImage, or
//     with press 3 ButtonPressedImage; its texture through 0x507360 (white, uv 0 0 1
//     1), else the solid rect (0x507830's record inline, white); y = ButtonSpacing + y.
//  3. the buttons' texts "Leaderboard_PageUp" / "Leaderboard_PageDown" (.data
//     0x73F2A8, VuStringDB slot 6) at ButtonText + Position with ButtonFont (the inline
//     0x626B80), flags 0x30, at Depth x 0.005 + 0.5 - 1.788e-7 (0x6F7588); y =
//     ButtonSpacing + y.
//  4. the FSM's current state's draw (+0x2C), if any.
// false: a combo draw read a choice outside its entries (combo.hxx).
inline bool ui_leaderboard_draw( const ui_leaderboard_t& f, const ui_combo_block_t& block, ui_leaderboard_state_t& s, const ui_leaderboard_draw_t& d )
{
    bool known = true;
    const float depth = f.m_depth * 0.005f + 0.5f;   // 0x6F75C4, 0x6F765C
    float pos[ 2 ] = { f.m_pos[ 0 ], f.m_pos[ 1 ] };
    for ( int k = 0; k < 2; ++k )
    {
        s.m_combos[ k ].m_focused = s.m_focus == k ? 1 : 0;
        known = ui_combo_draw( *d.m_sort, *d.m_env, block, s.m_combos[ k ], pos, f.m_scale, depth, d.m_combo ) && known;
        pos[ 1 ] = f.m_combo_spacing + pos[ 1 ];
    }
    const float ix = 1.0f / f.m_scale[ 0 ];   // 0x6F76B8
    const float iy = 1.0f / f.m_scale[ 1 ];
    {
        const float x = f.m_button_image[ 0 ] + f.m_pos[ 0 ];
        float y = f.m_button_image[ 1 ] + f.m_pos[ 1 ];
        for ( int k = 0; k < 2; ++k )
        {
            int which = 0;
            if ( s.m_focus - 2 == k )
                which = s.m_press == 3 ? 2 : 1;
            const float rect[ 4 ] = { ix * x, iy * y, ix * f.m_button_image[ 2 ], iy * f.m_button_image[ 3 ] };
            if ( const ui_texture_t* t = d.m_buttons[ which ] )
            {
                const float uv[ 4 ] = { 0.0f, 0.0f, 1.0f, 1.0f };
                ui_texture_draw( *d.m_sort, *d.m_env, t->m_handle, 0xFFFFFFFFu, rect, uv, depth );
            }
            else
                ui_solid_draw( *d.m_sort, *d.m_env, 0xFFFFFFFFu, rect, depth );
            y = f.m_button_spacing + y;
        }
    }
    {
        const float x = f.m_button_text[ 0 ] + f.m_pos[ 0 ];
        float y = f.m_button_text[ 1 ] + f.m_pos[ 1 ];
        for ( const char* id : { "Leaderboard_PageUp", "Leaderboard_PageDown" } )
        {
            const std::string text = d.m_string ? d.m_string( id ) : std::string( );
            const ui_results_font_t& font = d.m_fonts[ ui_lb_button ];
            if ( font.m_font )
            {
                const float rect[ 4 ] = { ix * x, iy * y, ix * f.m_button_text[ 2 ], iy * f.m_button_text[ 3 ] };
                const float offset[ 2 ] = { 0.0f, 0.0f };
                known = ui_font_draw( *d.m_sort, *d.m_env, font.m_font, text.c_str( ), font.m_params, rect, 0x30u, 1.0f, offset,
                                      depth - 1.7881393e-7f ) && known;   // 0x6F7588
            }
            y = f.m_button_spacing + y;
        }
    }
    switch ( s.m_state )
    {
    case 0:
        known = ui_leaderboard_info( f, d, "Leaderboard_Enumerating" ) && known;   // 0x47BEF0
        break;
    case 1:
        known = ui_leaderboard_show( f, d ) && known;   // 0x47BF90
        break;
    case 2:
        known = ui_leaderboard_info( f, d, "Leaderboard_NotConnected" ) && known;   // 0x47C720
        break;
    default:
        break;
    }
    return known;
}

// The Vu2dLayoutComponent's draw 0x47A910 (bool; ret 4), disassembled (tools
// tmp/b23/d_47A910.txt). When the bool is set (the layout view), all at the Screen
// Scale and Depth x 0.005 + 0.5:
//  1. from Position (y + ComboBoxSpacing per record) each combo record's block: the
//     ValueText rect at (x + ValueTextX, ValueTextY + y) as a rect draw 0x507A00 in
//     the value params' own colour (block +0xDC), then LeftArrowTexture (+0x1C8 ->
//     +0x4C -> +0x58) at (LeftArrowPositionX + x, LeftArrowPositionY + y) at its own
//     size (0x507360, white, uv 0 0 1 1; nothing without the texture);
//  2. TouchLeft, TouchRight and InfoRect at Position as white rect draws;
//  3. the two ButtonTouch rects (y + ButtonSpacing) - 0x507A00 inlined (the same
//     84-byte record and command) - white.
// Then always the RowCount sample rows at Show's layout (ui_leaderboard_show: the
// rect is the point with itself; flags 0x30 / 0x30 / 0x31; the fonts the inline
// 0x626B80) with the literal texts "Rank" / "Name" / "Score", and then the draw
// member 0x47A360 (alpha 1.0).
inline bool ui_leaderboard_layout_draw( const ui_leaderboard_t& f, const ui_combo_block_t& block, ui_leaderboard_state_t& s,
                                        const ui_leaderboard_draw_t& d, bool layout )
{
    bool known = true;
    if ( layout )
    {
        const float x = f.m_pos[ 0 ];
        float y = f.m_pos[ 1 ];
        std::uint32_t own;
        std::memcpy( &own, block.m_value_params + 12, 4 );
        for ( int k = 0; k < 2; ++k )
        {
            const float ix = 1.0f / f.m_scale[ 0 ];   // 0x6F76B8
            const float iy = 1.0f / f.m_scale[ 1 ];
            const float depth = f.m_depth * 0.005f + 0.5f;   // 0x6F75C4, 0x6F765C
            const float rect[ 4 ] = { ix * ( x + block.m_value_rect[ 0 ] ), iy * ( block.m_value_rect[ 1 ] + y ), ix * block.m_value_rect[ 2 ],
                                      iy * block.m_value_rect[ 3 ] };
            ui_rect_draw( *d.m_sort, *d.m_env, own, rect, depth );
            const float at[ 2 ] = { block.m_left[ 0 ] + x, block.m_left[ 1 ] + y };
            ui_combo_texture_draw( *d.m_sort, *d.m_env, d.m_combo.m_texture ? d.m_combo.m_texture( ui_combo_left ) : nullptr, at, f.m_scale,
                                   0xFFFFFFFFu, depth );
            y = f.m_combo_spacing + y;
        }
        const auto white = [ & ]( const float r[ 4 ], float px, float py )
        {
            const float ix = 1.0f / f.m_scale[ 0 ];
            const float iy = 1.0f / f.m_scale[ 1 ];
            const float rect[ 4 ] = { ix * ( r[ 0 ] + px ), iy * ( r[ 1 ] + py ), ix * r[ 2 ], iy * r[ 3 ] };
            ui_rect_draw( *d.m_sort, *d.m_env, 0xFFFFFFFFu, rect, f.m_depth * 0.005f + 0.5f );
        };
        white( f.m_left, f.m_pos[ 0 ], f.m_pos[ 1 ] );
        white( f.m_right, f.m_pos[ 0 ], f.m_pos[ 1 ] );
        white( f.m_info, f.m_pos[ 0 ], f.m_pos[ 1 ] );
        {
            const float bx = f.m_button_touch[ 0 ] + f.m_pos[ 0 ];
            float by = f.m_button_touch[ 1 ] + f.m_pos[ 1 ];
            for ( int k = 0; k < 2; ++k )
            {
                const float ix = 1.0f / f.m_scale[ 0 ];
                const float iy = 1.0f / f.m_scale[ 1 ];
                const float rect[ 4 ] = { ix * bx, iy * by, ix * f.m_button_touch[ 2 ], iy * f.m_button_touch[ 3 ] };
                ui_rect_draw( *d.m_sort, *d.m_env, 0xFFFFFFFFu, rect, f.m_depth * 0.005f + 0.5f );
                by = by + f.m_button_spacing;
            }
        }
    }
    {
        const float x = f.m_pos[ 0 ] + f.m_row[ 0 ];
        float y = f.m_pos[ 1 ] + ( f.m_row[ 3 ] * 0.5f + f.m_row[ 1 ] );
        for ( std::int32_t k = 0; k < f.m_rows; ++k )
        {
            const float w = x - x;   // maxss( x, x ) - minss( x, x )
            const float h = y - y;
            const float rank_rect[ 4 ] = { f.m_rank_x + x, y, w, h };
            known = ui_leaderboard_text( f, d, d.m_fonts[ ui_lb_rank ], rank_rect, 0x30u, "Rank", 0.0f ) && known;
            const float name_rect[ 4 ] = { f.m_name_x + x, y, w, h };
            known = ui_leaderboard_text( f, d, d.m_fonts[ ui_lb_name ], name_rect, 0x30u, "Name", 0.0f ) && known;
            const float score_rect[ 4 ] = { f.m_score_x + x, y, w, h };
            known = ui_leaderboard_text( f, d, d.m_fonts[ ui_lb_score ], score_rect, 0x31u, "Score", 0.0f ) && known;
            y = f.m_spacing + y;
        }
    }
    return ui_leaderboard_draw( f, block, s, d ) && known;
}

// The platform's leaderboard manager g 0x758FA8 (HOOKS: an online service) and the
// engine services the per-frame half reaches.
struct ui_leaderboard_hooks_t
{
    std::function< void( const std::string&, int, int ) > m_read;   // vt+0x18 (key, first row, RowCount)
    std::function< int( ) > m_status;                               // vt+0x1C: 1 done, 2 failed
    std::function< int( ) > m_total;                                // vt+0x20: the board's entry count
    std::function< void( bool ) > m_tick_register;                  // the tick manager g 0x759008 vt+0x20 / +0x24 "Decision"
    std::function< float( ) > m_delta;                              // its vt+0x3C (the frame's dt)
    std::function< void( bool ) > m_touch;                          // the touch device g 0x759070: 0x53FB90 add {+0x5C, 0} / 0x53FA60 remove
    std::function< void( const char* ) > m_sound;                   // FMOD getEvent + start (g 0x759010 +8, a HOOK)
    const value_t* m_option_db = nullptr;                           // gameutil g 0x758FF8 +0x34 -> +0x58 (OptionDB)
};

// VuUILeaderboardEntity's per-frame half (batch 23), transliterated from the
// disassembly. The ctor's records come from ui_leaderboard_combos over the hooks'
// OptionDB; the FSM is the ctor's (current = Read from addState - its enter runs
// at slot 6). Every member returns false where retail would fault or read outside
// what it owns (a combo index < 0, RowCount 0 in an idiv, a mode index outside
// the six names) - the path is then UNKNOWN.
class c_ui_leaderboard
{
public:
    c_ui_leaderboard( const ui_leaderboard_t& fields, ui_leaderboard_hooks_t& hooks ) : m_f( fields ), m_hooks( hooks )
    {
        ui_leaderboard_combos( hooks.m_option_db, m_s.m_combos );
        fsm_state_t& read = m_fsm.add_state( "Read" );
        read.m_enter = [ this ] { m_known = read_enter( ) && m_known; };
        read.m_exit = [ this ] { read_exit( ); };
        read.m_tick = [ this ]( float ) { read_tick( ); };
        read.m_address[ 0 ] = 0x47BC90u;
        read.m_address[ 1 ] = 0x47BE70u;
        read.m_address[ 2 ] = 0x47BEA0u;
        read.m_address[ 3 ] = 0x47BEF0u;
        m_fsm.add_state( "Show" ).m_address[ 3 ] = 0x47BF90u;
        m_fsm.add_state( "NotConnected" ).m_address[ 3 ] = 0x47C720u;
        m_fsm.add_transition( "Read", "Show", "Success" );
        m_fsm.add_transition( "Read", "NotConnected", "Failure" );
        m_fsm.add_transition( "Show", "Read", "Refresh" );
        m_fsm.add_transition( "NotConnected", "Read", "Refresh" );
    }

    // Slot 6 0x479C50 (gameInitialize): "Decision" (0x47B640, a 16-byte VuMethod
    // delegate vtable 0x73F274) registered with the tick manager (vt+0x20); the touch
    // callback (+0x5C) registered through 0x53FB90 with priority 0; current = the
    // first state (+0x5B4 [0]), then its enter.
    bool initialize( )
    {
        m_known = true;
        if ( m_hooks.m_tick_register )
            m_hooks.m_tick_register( true );
        if ( m_hooks.m_touch )
            m_hooks.m_touch( true );
        m_fsm.start( );
        return m_known;
    }

    // Slot 7 0x479CF0 (gameRelease): "Decision" unregistered (vt+0x24), the touch
    // callback removed (0x53FA60), then the current state's exit (+0x24).
    void release( )
    {
        if ( m_hooks.m_tick_register )
            m_hooks.m_tick_register( false );
        if ( m_hooks.m_touch )
            m_hooks.m_touch( false );
        if ( fsm_state_t* s = m_fsm.current( ) )
            if ( s->m_exit )
                s->m_exit( );
    }

    // "Decision" 0x47B640 (its float argument is overwritten by the tick manager's
    // dt, vt+0x3C). With the touch down byte (+0x5D0): per combo k (y from Position Y,
    // += ComboBoxSpacing) the left / right arrow (TouchLeft / TouchRight over Position,
    // x (1 / Screen Scale); inclusive, comiss - unordered fails) -> press 1 / 2, the
    // touched entry +0x5E0 = k, the record's +0x29 / +0x2A = 1 (a later k wins); per
    // button k (ButtonTouch, += ButtonSpacing) -> press 3, entry k, focus k + 2. Then
    // the press over the entry's rect rebuilt from k (float k x the spacing - not the
    // running sum): released (+0x5D2) -> 0x47A1B0(k, -1 / +1) or 0x47A280(focus) and
    // the press dropped; the point outside drops it too. Released: held / up cleared;
    // down cleared; each record's shown choice +0x30 toward its current +0x2C by dt /
    // the block's +0x120 (0.25, ctor 0x4569F0; minss / maxss); FSM evaluate 0x555AD0;
    // the current state's tick (dt); the FSM time += dt.
    bool decision( )
    {
        m_known = true;
        const float dt = m_hooks.m_delta ? m_hooks.m_delta( ) : 0.0f;
        const auto inside = [ this ]( float x, float y, float w, float h )
        {
            return m_point[ 0 ] >= x && w + x >= m_point[ 0 ] && m_point[ 1 ] >= y && h + y >= m_point[ 1 ];
        };
        if ( m_down )
        {
            const float x = m_f.m_pos[ 0 ];
            float y = m_f.m_pos[ 1 ];
            for ( int k = 0; k < 2; ++k )
            {
                const float ix = 1.0f / m_f.m_scale[ 0 ];   // 0x6F76B8
                const float iy = 1.0f / m_f.m_scale[ 1 ];
                const float lx = ix * ( x + m_f.m_left[ 0 ] ), lw = ix * m_f.m_left[ 2 ];
                const float ly = iy * ( y + m_f.m_left[ 1 ] ), lh = iy * m_f.m_left[ 3 ];
                const float rx = ix * ( m_f.m_right[ 0 ] + x ), rw = ix * m_f.m_right[ 2 ];
                const float ry = iy * ( m_f.m_right[ 1 ] + y ), rh = iy * m_f.m_right[ 3 ];
                if ( inside( lx, ly, lw, lh ) )
                {
                    m_s.m_press = 1;
                    m_entry = k;
                    m_s.m_combos[ k ].m_b41 = 1;
                }
                else if ( inside( rx, ry, rw, rh ) )
                {
                    m_s.m_press = 2;
                    m_entry = k;
                    m_s.m_combos[ k ].m_b42 = 1;
                }
                y = m_f.m_combo_spacing + y;
            }
            y = m_f.m_pos[ 1 ];
            for ( int k = 0; k < 2; ++k )
            {
                const float ix = 1.0f / m_f.m_scale[ 0 ];
                const float iy = 1.0f / m_f.m_scale[ 1 ];
                const float bx = ix * ( x + m_f.m_button_touch[ 0 ] ), bw = ix * m_f.m_button_touch[ 2 ];
                const float by = iy * ( m_f.m_button_touch[ 1 ] + y ), bh = iy * m_f.m_button_touch[ 3 ];
                if ( inside( bx, by, bw, bh ) )
                {
                    m_s.m_press = 3;
                    m_entry = k;
                    m_s.m_focus = k + 2;
                }
                y = m_f.m_button_spacing + y;
            }
        }
        for ( int side = 1; side <= 2; ++side )
        {
            if ( m_s.m_press != side )
                continue;
            const int e = m_entry;
            const float* r = side == 1 ? m_f.m_left : m_f.m_right;
            const float ix = 1.0f / m_f.m_scale[ 0 ];
            const float iy = 1.0f / m_f.m_scale[ 1 ];
            const float sx = side == 1 ? m_f.m_pos[ 0 ] + r[ 0 ] : r[ 0 ] + m_f.m_pos[ 0 ];
            const float sy = ( r[ 1 ] + m_f.m_pos[ 1 ] ) + static_cast< float >( e ) * m_f.m_combo_spacing;
            const float x = ix * sx, w = ix * r[ 2 ];
            const float y = iy * sy, h = iy * r[ 3 ];
            const auto clear = [ this, side ]
            {
                m_s.m_press = 0;
                if ( m_entry < 0 || m_entry > 1 )
                    return false;   // retail writes the byte outside the two records
                ( side == 1 ? m_s.m_combos[ m_entry ].m_b41 : m_s.m_combos[ m_entry ].m_b42 ) = 0;
                return true;
            };
            if ( m_up )
            {
                m_known = change( e, side == 1 ? -1 : 1 ) && m_known;
                m_known = clear( ) && m_known;
            }
            if ( !inside( x, y, w, h ) )
                m_known = clear( ) && m_known;
        }
        if ( m_s.m_press == 3 )
        {
            const float sy = ( m_f.m_button_touch[ 1 ] + m_f.m_pos[ 1 ] ) + static_cast< float >( m_entry ) * m_f.m_button_spacing;
            const float sx = m_f.m_pos[ 0 ] + m_f.m_button_touch[ 0 ];
            const float iy = 1.0f / m_f.m_scale[ 1 ];
            const float ix = 1.0f / m_f.m_scale[ 0 ];
            const float x = ix * sx, w = ix * m_f.m_button_touch[ 2 ];
            const float y = iy * sy, h = iy * m_f.m_button_touch[ 3 ];
            if ( m_up )
            {
                m_known = page( m_s.m_focus ) && m_known;
                m_s.m_press = 0;
            }
            if ( !inside( x, y, w, h ) )
                m_s.m_press = 0;
        }
        if ( m_up )
        {
            m_held = 0;
            m_up = 0;
        }
        m_down = 0;
        for ( ui_config_combo_t& c : m_s.m_combos )
        {
            const float target = static_cast< float >( c.m_index );   // cvtdq2ps
            const float step = dt / 0.25f;                           // the block's +0x120
            if ( target > c.m_shown )
            {
                const float sum = step + c.m_shown;
                c.m_shown = sum < target ? sum : target;   // minss
            }
            else
            {
                const float diff = c.m_shown - step;
                c.m_shown = diff > target ? diff : target;   // maxss
            }
        }
        m_fsm.evaluate( );
        if ( fsm_state_t* s = m_fsm.current( ) )
            if ( s->m_tick )
                s->m_tick( dt );
        m_fsm.m_time = m_fsm.m_time + dt;
        return m_known;
    }

    // 0x47A1B0 (thiscall k, step; ret 8): nothing unless k < 2 (signed). focus = k; the
    // record's new choice = min(current + step, entries - 1), then 0 if negative; when
    // it moved: current = it, menu_change, the first row +0x5A4 = 0, "Refresh" = 1.
    bool change( int k, int step )
    {
        if ( k >= 2 )
            return true;
        if ( k < 0 )
            return false;   // retail indexes before the records
        m_s.m_focus = k;
        ui_config_combo_t& c = m_s.m_combos[ k ];
        const int last = static_cast< int >( c.m_choice_ids.size( ) ) - 1;
        int n = static_cast< int >( static_cast< std::uint32_t >( step ) + static_cast< std::uint32_t >( c.m_index ) );
        if ( last < n )
            n = last;
        if ( n < 0 )
            n = 0;
        if ( n == c.m_index )
            return true;
        c.m_index = n;
        if ( m_hooks.m_sound )
            m_hooks.m_sound( "Global/UI_and_Menu/menu_change" );
        m_first = 0;
        m_fsm.set_condition( "Refresh", true );
        return true;
    }

    // 0x47A280 (thiscall focus; ret 4): nothing for focus < 2 or while the current
    // state's name is exactly "Read". page = first / RowCount; focus 2 -> first -=
    // RowCount, 3 -> += RowCount; then clamped to [0, the board's count (vt+0x20) - 1]
    // (min first, then 0 if negative); when the page moved, "Refresh" = 1.
    bool page( int focus )
    {
        if ( focus < 2 )
            return true;
        const fsm_state_t* s = m_fsm.current( );
        if ( !s )
            return false;
        if ( s->m_name == "Read" )
            return true;
        const int rows = m_f.m_rows;
        if ( rows == 0 || ( rows == -1 && m_first == INT32_MIN ) )
            return false;   // idiv fault
        const int page0 = m_first / rows;
        if ( focus == 2 )
            m_first = static_cast< int >( static_cast< std::uint32_t >( m_first ) - static_cast< std::uint32_t >( rows ) );
        else if ( focus == 3 )
            m_first = static_cast< int >( static_cast< std::uint32_t >( rows ) + static_cast< std::uint32_t >( m_first ) );
        m_first = clamp_first( m_first );
        if ( rows == -1 && m_first == INT32_MIN )
            return false;
        if ( m_first / rows != page0 )
            m_fsm.set_condition( "Refresh", true );
        return true;
    }

    // The touch callbacks (vtable 0x73F2B4, this = entity + 0x5C). Slot 0 0x479D30
    // (down, const VuVector2&): unless held (+0x5D1), down = held = 1 and the point
    // through VuUIImpl's inverse crop matrix 0x81A144 (m0 m1 m4 m5 m12 m13 - an input)
    // -> +0x5D4.
    void touch_down( const float inverse[ 16 ], const float point[ 2 ] )
    {
        if ( m_held )
            return;
        m_down = 1;
        m_held = 1;
        m_point[ 0 ] = point[ 0 ] * inverse[ 0 ] + point[ 1 ] * inverse[ 4 ] + inverse[ 12 ];
        m_point[ 1 ] = point[ 0 ] * inverse[ 1 ] + point[ 1 ] * inverse[ 5 ] + inverse[ 13 ];
    }

    // Slot 1 0x479E30 (up): +0x5D2 = 1.
    void touch_up( ) { m_up = 1; }

    // Slot 2 0x479E40 (move): the device's touch 0 (g 0x759070 vt+0x1C (0, out) - a
    // HOOK: `point`) mapped -> +0x5D4, always (retail's operand order).
    void touch_move( const float inverse[ 16 ], const float point[ 2 ] )
    {
        const float x = inverse[ 4 ] * point[ 1 ] + inverse[ 0 ] * point[ 0 ] + inverse[ 12 ];
        const float y = inverse[ 5 ] * point[ 1 ] + inverse[ 1 ] * point[ 0 ] + inverse[ 13 ];
        m_point[ 0 ] = x;
        m_point[ 1 ] = y;
    }

    // The draw component's member 0x47A360 over this runtime state.
    bool draw( const ui_combo_block_t& block, const ui_leaderboard_draw_t& d )
    {
        m_s.m_state = m_fsm.m_current;
        return ui_leaderboard_draw( m_f, block, m_s, d );
    }

    ui_leaderboard_state_t m_s;            // +0x538 the records, +0x5A0 focus, +0x5DC press
    c_fsm m_fsm;                           // +0x5A8
    std::int32_t m_first = 0;              // +0x5A4 the first row asked for
    std::int32_t m_entry = 0;              // +0x5E0 the touched record / button
    std::uint8_t m_down = 0, m_held = 0, m_up = 0;   // +0x5D0 / +0x5D1 / +0x5D2
    float m_point[ 2 ] = { 0.0f, 0.0f };   // +0x5D4

private:
    // Read's exit 0x47BE70 / the clamp in 0x47A280: min(first, count - 1), then 0 if negative.
    int clamp_first( int first ) const
    {
        const int count = m_hooks.m_total ? m_hooks.m_total( ) : 0;
        const int last = static_cast< int >( static_cast< std::uint32_t >( count ) - 1u );
        if ( last < first )
            first = last;
        if ( first < 0 )
            first = 0;
        return first;
    }

    // Read's enter 0x47BC90: "Refresh" = 0; the key = OptionDB ["VuDBAsset"]["Track"]
    // (its first element when an array with one or more, else null) ["Choices"] (the
    // element at record 0's current +0x564 when an array and in range, else null)
    // ["Value"]'s text (tag 4, else ""), + the mode name .data 0x73F290 [record 1's
    // current +0x598] (Race / SlalomCourseC / SlalomCourseB / SlalomCourseA / TimeTrial
    // / Ghost; 0x430290 append); the manager's read (vt+0x18: &key, (first / RowCount)
    // x RowCount, RowCount).
    bool read_enter( )
    {
        static const value_t k_none;
        static const char* const k_modes[ 6 ] = { "Race", "SlalomCourseC", "SlalomCourseB", "SlalomCourseA", "TimeTrial", "Ghost" };
        m_fsm.set_condition( "Refresh", false );
        const int track_index = m_s.m_combos[ 0 ].m_index;
        const int mode_index = m_s.m_combos[ 1 ].m_index;
        const value_t& db = m_hooks.m_option_db ? member( *m_hooks.m_option_db, "VuDBAsset" ) : k_none;
        const value_t& track = member( db, "Track" );
        const value_t& first = track.tag == 5 && !track.list.empty( ) ? track.list[ 0 ] : k_none;
        const value_t& choices = member( first, "Choices" );
        const value_t& choice = choices.tag == 5 && track_index >= 0 && track_index < static_cast< int >( choices.list.size( ) )
                                    ? choices.list[ static_cast< std::size_t >( track_index ) ]
                                    : k_none;
        const value_t& v = member( choice, "Value" );
        std::string key = v.tag == 4 ? v.text : std::string( );
        if ( mode_index < 0 || mode_index > 5 )
            return false;   // retail reads past the name table
        key += k_modes[ mode_index ];
        const int rows = m_f.m_rows;
        if ( rows == 0 || ( rows == -1 && m_first == INT32_MIN ) )
            return false;   // idiv fault
        const int start = static_cast< int >( static_cast< std::uint32_t >( m_first / rows ) * static_cast< std::uint32_t >( rows ) );
        if ( m_hooks.m_read )
            m_hooks.m_read( key, start, rows );
        return true;
    }

    // Read's exit 0x47BE70: first = clamped to the board's count (vt+0x20).
    void read_exit( ) { m_first = clamp_first( m_first ); }

    // Read's tick 0x47BEA0 (the dt is not read): the manager's status (vt+0x1C) 1
    // pulses "Success", 2 "Failure" (conditions +0x1D = 1).
    void read_tick( )
    {
        const int status = m_hooks.m_status ? m_hooks.m_status( ) : 0;
        if ( status == 1 )
            m_fsm.pulse_condition( "Success" );
        else if ( status == 2 )
            m_fsm.pulse_condition( "Failure" );
    }

    ui_leaderboard_t m_f;
    ui_leaderboard_hooks_t& m_hooks;
    bool m_known = true;
};