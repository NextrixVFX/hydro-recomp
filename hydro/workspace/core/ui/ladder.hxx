#pragma once

#include <workspace/core/ui/combo.hxx>
#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/frame.hxx>
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
// Read "Refresh". The state draws are recreated here; the state machine's enter /
// exit / tick, slot 6 0x479C50, slot 7 0x479CF0 and "Decision" 0x47B640 are NOT yet.

inline constexpr std::uint32_t k_ui_leaderboard_vtable = 0x73F2C8u;

struct ui_leaderboard_t
{
    float m_scale[ 2 ] = { 100.0f, 100.0f };   // +0x68
    float m_pos[ 2 ] = { 0.0f, 0.0f };         // +0x70
    float m_combo_spacing = 0.0f;              // +0x78 ComboBoxSpacing
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
