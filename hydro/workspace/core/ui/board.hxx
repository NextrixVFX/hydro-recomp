#pragma once

#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/frame.hxx>
#include <workspace/core/ui/fsm.hxx>
#include <workspace/core/ui/results.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

// The mini leaderboard (VuUIMiniLeaderboardEntity, ctor 0x47CC40, vtable
// 0x73F408 - Screens/GameConfig*), transliterated from the disassembly (batch
// 21). The rows come from the platform's leaderboard manager g 0x758FA8 (an
// online service: a HOOK - its read request, status, row count and rows are
// the caller's); the state machine and the draws are 1:1.
//
// Fields (the ctor's defaults): +0x64 Screen Scale (100, 100), +0x6C / +0x70
// Position X / Y, +0x74 RowCount (5), +0x78..+0x84 RowRect X / Y / Width /
// Height, +0x88 RowSpacing, +0x8C NameOffsetX, +0x90 ScoreOffsetX, three VuUIFont
// blocks (0x626900: the font name at +8, the 60-byte draw params at +0x20, the
// asset at +0x60) NameFont +0x94 / ScoreFont +0xF8 / InfoFont +0x16C, +0x15C
// InfoRect, +0x1D0 the info text flags block (InfoHorizontal / Vertical
// Alignment, InfoNo Clip / Single Line / Wordbreak: 0x5546B0), +0x1DC the anchor
// (Horizontal / Vertical Anchor, Ratio), +0x1EC the VuFSM, +0x214 the key it
// last asked for (a std::string).
//
// The VuFSM (built by the ctor): states "Read" (enter 0x47E4C0, tick 0x47E510,
// draw 0x47E560), "Show" (draw 0x47E600), "NotConnected" (draw 0x47EA20);
// transitions Read -> Show "Success", Read -> NotConnected "Failure", Show ->
// Read "Refresh", NotConnected -> Read "Refresh" (so the conditions are Success,
// Failure, Refresh in that order).

inline constexpr std::uint32_t k_ui_mini_board_vtable = 0x73F408u;

struct ui_mini_board_t
{
    float m_scale[ 2 ] = { 100.0f, 100.0f };   // +0x64
    float m_pos[ 2 ] = { 0.0f, 0.0f };         // +0x6C
    std::int32_t m_rows = 5;                   // +0x74 RowCount
    float m_row[ 4 ] = { };                    // +0x78 RowRect
    float m_spacing = 0.0f;                    // +0x88 RowSpacing
    float m_name_x = 0.0f;                     // +0x8C NameOffsetX
    float m_score_x = 0.0f;                    // +0x90 ScoreOffsetX
    float m_info[ 4 ] = { };                   // +0x15C InfoRect
    std::int32_t m_h_align = 0;                // +0x1D0
    std::int32_t m_v_align = 2;                // +0x1D4
    std::uint8_t m_no_clip = 0, m_single_line = 0, m_wordbreak = 0;   // +0x1D8..+0x1DA
    ui_anchor_t m_anchor;                      // +0x1DC
    float m_depth = 0.0f;                      // the draw component's (+0x5C) +0x14 Depth
};

// The fields from OUR loaded entity (its property values, defaults included).
inline ui_mini_board_t ui_mini_board_of( const entity_t& e )
{
    ui_mini_board_t b;
    const auto bits = [ &e ]( const char* name, void* out, std::size_t n )
    {
        if ( const ui_slot_t* v = ui_property_value( e, name ) )
            std::memcpy( out, v->m_bits, n );
    };
    bits( "Screen Scale", b.m_scale, 8 );
    bits( "Position X", &b.m_pos[ 0 ], 4 );
    bits( "Position Y", &b.m_pos[ 1 ], 4 );
    bits( "RowCount", &b.m_rows, 4 );
    const char* const parts[ 4 ] = { "RowRect X", "RowRect Y", "RowRect Width", "RowRect Height" };
    for ( int k = 0; k < 4; ++k )
        bits( parts[ k ], &b.m_row[ k ], 4 );
    bits( "RowSpacing", &b.m_spacing, 4 );
    bits( "NameOffsetX", &b.m_name_x, 4 );
    bits( "ScoreOffsetX", &b.m_score_x, 4 );
    bits( "InfoRect", b.m_info, 16 );
    bits( "InfoHorizontal Alignment", &b.m_h_align, 4 );
    bits( "InfoVertical Alignment", &b.m_v_align, 4 );
    bits( "InfoNo Clip", &b.m_no_clip, 1 );
    bits( "InfoSingle Line", &b.m_single_line, 1 );
    bits( "InfoWordbreak", &b.m_wordbreak, 1 );
    bits( "Horizontal Anchor", &b.m_anchor.m_horizontal, 4 );
    bits( "Vertical Anchor", &b.m_anchor.m_vertical, 4 );
    bits( "Horizontal Ratio", &b.m_anchor.m_h_ratio, 4 );
    bits( "Vertical Ratio", &b.m_anchor.m_v_ratio, 4 );
    b.m_depth = ui_frame_depth( e );
    return b;
}

// 0x47E2B0 (thiscall out std::string): the game-data root (g_vugameutil 0x758FF8
// +0x50) GameConfig.Mode / Track / Course (each a string, else the static ""
// 0x819950); key = Track + Mode (0x47EBF0), then + Course when Mode is exactly
// "Slalom" (a 6-character compare and length 6).
inline std::string ui_mini_board_key( const value_t& root )
{
    const auto text = [ &root ]( const char* name ) -> std::string
    {
        const value_t& v = member( member( root, "GameConfig" ), name );
        return v.tag == 4 ? v.text : std::string( );
    };
    const std::string mode = text( "Mode" );
    const std::string track = text( "Track" );
    const std::string course = text( "Course" );
    std::string key = track + mode;
    if ( mode == "Slalom" )
        key += course;
    return key;
}

// The platform's leaderboard manager g 0x758FA8 (HOOKS: an online service) and the
// engine services the entity calls.
struct ui_mini_board_hooks_t
{
    std::function< void( const std::string&, int, int ) > m_read;   // vt+0x18 (key, 0, RowCount)
    std::function< int( ) > m_status;                               // vt+0x1C: 1 done, 2 failed
    std::function< int( ) > m_count;                                // vt+0x24
    std::function< void( int, std::string&, std::int32_t& ) > m_row;   // vt+0x28 (k, -, -, name, score)
    std::function< void( bool ) > m_tick_register;                  // the tick manager g 0x759008 vt+0x20 / +0x24 "Decision"
    std::function< float( ) > m_delta;                              // its vt+0x3C (the frame's dt)
    const value_t* m_root = nullptr;                                // gameutil +0x50
};

// What a draw needs: the sort, the font environment, VuUIImpl's crop (the anchor
// 0x6282D0), the three fonts (0x626B80's answer + the block's 60 params: name,
// score, info), the 0x5546B0 string-db answer and VuStringDB slot 6 (getString:
// a HOOK - the string tables).
struct ui_mini_board_draw_t
{
    ui_gfx_sort_t* m_sort = nullptr;
    const ui_font_draw_env_t* m_env = nullptr;
    ui_crop_t m_crop;
    ui_results_font_t m_fonts[ 3 ];
    ui_string_db_t m_db;
    std::function< std::string( const std::string& ) > m_string;
};

class c_ui_mini_board
{
public:
    explicit c_ui_mini_board( const ui_mini_board_t& fields, ui_mini_board_hooks_t& hooks ) : m_f( fields ), m_hooks( hooks )
    {
        fsm_state_t& read = m_fsm.add_state( "Read" );
        read.m_enter = [ this ] { read_enter( ); };
        read.m_tick = [ this ]( float ) { read_tick( ); };
        read.m_draw = [ this ] { read_draw( ); };
        read.m_address[ 0 ] = 0x47E4C0u;
        read.m_address[ 2 ] = 0x47E510u;
        read.m_address[ 3 ] = 0x47E560u;
        fsm_state_t& show = m_fsm.add_state( "Show" );
        show.m_draw = [ this ] { show_draw( ); };
        show.m_address[ 3 ] = 0x47E600u;
        fsm_state_t& lost = m_fsm.add_state( "NotConnected" );
        lost.m_draw = [ this ] { lost_draw( ); };
        lost.m_address[ 3 ] = 0x47EA20u;
        m_fsm.add_transition( "Read", "Show", "Success" );
        m_fsm.add_transition( "Read", "NotConnected", "Failure" );
        m_fsm.add_transition( "Show", "Read", "Refresh" );
        m_fsm.add_transition( "NotConnected", "Read", "Refresh" );
    }

    // Slot 6 0x47DAB0 (gameInitialize): "Decision" (0x47E170) registered with the
    // tick manager; current = the first state, then its enter.
    void initialize( )
    {
        if ( m_hooks.m_tick_register )
            m_hooks.m_tick_register( true );
        m_fsm.start( );
    }

    // Slot 7 0x47DB10 (gameRelease): unregistered, then the current state's exit
    // (none of the three has one).
    void release( )
    {
        if ( m_hooks.m_tick_register )
            m_hooks.m_tick_register( false );
        if ( fsm_state_t* s = m_fsm.current( ) )
            if ( s->m_exit )
                s->m_exit( );
    }

    // "Decision" 0x47E170 (the tick): dt = the tick manager's vt+0x3C, evaluate
    // 0x555AD0, the current state's tick (dt), the FSM time += dt, then
    // Refresh's value = (the key now != the key last asked for - std::string
    // compare 0x404AC0 + the lengths).
    void decision( )
    {
        const float dt = m_hooks.m_delta ? m_hooks.m_delta( ) : 0.0f;
        m_fsm.evaluate( );
        if ( fsm_state_t* s = m_fsm.current( ) )
            if ( s->m_tick )
                s->m_tick( dt );
        m_fsm.m_time = m_fsm.m_time + dt;
        const std::string now = ui_mini_board_key( root( ) );
        m_fsm.set_condition( "Refresh", now != m_key );
    }

    // The draw component's member 0x47DB40 (float alpha - never read): the current
    // state's draw (+0x2C), if any.
    void draw( const ui_mini_board_draw_t& d )
    {
        m_draw = &d;
        if ( fsm_state_t* s = m_fsm.current( ) )
            if ( s->m_draw )
                s->m_draw( );
        m_draw = nullptr;
    }

    c_fsm m_fsm;          // +0x1EC
    std::string m_key;    // +0x214

private:
    const value_t& root( ) const
    {
        static const value_t k_none;
        return m_hooks.m_root ? *m_hooks.m_root : k_none;
    }

    // Read's enter 0x47E4C0: Refresh's value = 0, the key (0x47E2B0) into +0x214,
    // the manager's read (vt+0x18: &key, 0, RowCount).
    void read_enter( )
    {
        m_fsm.set_condition( "Refresh", false );
        m_key = ui_mini_board_key( root( ) );
        if ( m_hooks.m_read )
            m_hooks.m_read( m_key, 0, m_f.m_rows );
    }

    // Read's tick 0x47E510 (the dt is not read): the manager's status (vt+0x1C) 1
    // pulses "Success", 2 "Failure" (conditions +0x1D = 1).
    void read_tick( )
    {
        const int status = m_hooks.m_status ? m_hooks.m_status( ) : 0;
        if ( status == 1 )
            m_fsm.pulse_condition( "Success" );
        else if ( status == 2 )
            m_fsm.pulse_condition( "Failure" );
    }

    // 0x47E090 (thiscall text, VuUIFont*, rect*, flags, a 5th never read; ret 0x14):
    // the font (0x626B80; none -> nothing), the rect x (1 / scale) (x, w by the x
    // scale, y, h by the y one), the anchor 0x6282D0 over +0x1DC in place, then the
    // font draw 0x536C10 (the block's params, flags, alpha 1.0, offset 0, depth =
    // Depth x 0.005 + 0.5).
    void text( const ui_results_font_t& font, const float r[ 4 ], std::uint32_t flags, const char* s ) const
    {
        if ( !font.m_font )
            return;
        const float ix = 1.0f / m_f.m_scale[ 0 ];   // 0x6F76B8 = 1.0
        const float iy = 1.0f / m_f.m_scale[ 1 ];
        const float scaled[ 4 ] = { r[ 0 ] * ix, r[ 1 ] * iy, r[ 2 ] * ix, r[ 3 ] * iy };
        float rect[ 4 ];
        ui_anchor_transform( m_f.m_anchor, m_draw->m_crop, scaled, rect );
        const float offset[ 2 ] = { 0.0f, 0.0f };
        const float depth = m_f.m_depth * 0.005f + 0.5f;   // 0x6F75C4, 0x6F765C
        ui_font_draw( *m_draw->m_sort, *m_draw->m_env, font.m_font, s, font.m_params, rect, flags, 1.0f, offset, depth );
    }

    // Read / NotConnected / Show-with-no-rows: the info text - rect (InfoRect x +
    // Position X, InfoRect y + Position Y, its w, h), flags 0x5546B0 on +0x1D0, the
    // string-db text of `id` (slot 6), InfoFont.
    void info( const char* id ) const
    {
        const float rect[ 4 ] = { m_f.m_info[ 0 ] + m_f.m_pos[ 0 ], m_f.m_info[ 1 ] + m_f.m_pos[ 1 ], m_f.m_info[ 2 ], m_f.m_info[ 3 ] };
        const std::uint32_t flags = ui_font_flags( m_f.m_h_align, m_f.m_v_align, m_f.m_no_clip, m_f.m_single_line, m_f.m_wordbreak, m_draw->m_db );
        const std::string s = m_draw->m_string ? m_draw->m_string( id ) : std::string( );
        text( m_draw->m_fonts[ 2 ], rect, flags, s.c_str( ) );
    }

    void read_draw( ) const { info( "Leaderboard_Enumerating" ); }    // 0x47E560
    void lost_draw( ) const { info( "Leaderboard_NotConnected" ); }   // 0x47EA20

    // Show's draw 0x47E600: no rows (vt+0x24 == 0) -> "Leaderboard_NoOneRanked" as
    // the info text. Else x = Position X + RowRect X, y = Position Y + (RowRect
    // Height x 0.5 + RowRect Y); for k while k < the count (re-read each row): the
    // row (vt+0x28: name, score), the name at (NameOffsetX + x, y, 0, 0) - the
    // rect is built from the min / max of the point with itself: x - x, y - y -
    // flags 0x30 with NameFont; the score's text (float)(-score) x 0.001 as
    // results.hxx ui_results_time (the same '-', x 1000, "%d:%02d.%02d") at
    // (ScoreOffsetX + x, y, 0, 0), flags 0x31, ScoreFont; y = RowSpacing + y.
    void show_draw( ) const
    {
        const int count = m_hooks.m_count ? m_hooks.m_count( ) : 0;
        if ( count == 0 )
        {
            info( "Leaderboard_NoOneRanked" );
            return;
        }
        const float x = m_f.m_pos[ 0 ] + m_f.m_row[ 0 ];
        float y = m_f.m_pos[ 1 ] + ( m_f.m_row[ 3 ] * 0.5f + m_f.m_row[ 1 ] );
        for ( int k = 0; k < ( m_hooks.m_count ? m_hooks.m_count( ) : 0 ); ++k )
        {
            std::string name;
            std::int32_t score = 0;
            if ( m_hooks.m_row )
                m_hooks.m_row( k, name, score );
            const float w = x - x;   // maxss( x, x ) - minss( x, x )
            const float h = y - y;
            const float name_rect[ 4 ] = { m_f.m_name_x + x, y, w, h };
            text( m_draw->m_fonts[ 0 ], name_rect, 0x30u, name.c_str( ) );
            const float t = static_cast< float >( static_cast< std::int32_t >( 0u - static_cast< std::uint32_t >( score ) ) ) * 0.001f;   // neg, cvtdq2ps, 0x6F75B0
            const std::string s = ui_results_time( t );
            const float score_rect[ 4 ] = { m_f.m_score_x + x, y, w, h };
            text( m_draw->m_fonts[ 1 ], score_rect, 0x31u, s.c_str( ) );
            y = m_f.m_spacing + y;
        }
    }

    ui_mini_board_t m_f;
    ui_mini_board_hooks_t& m_hooks;
    const ui_mini_board_draw_t* m_draw = nullptr;
};
