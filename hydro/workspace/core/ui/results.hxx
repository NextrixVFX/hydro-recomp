#pragma once

#include <workspace/core/ui/action.hxx>
#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/frame.hxx>
#include <workspace/core/ui/hud.hxx>
#include <workspace/core/ui/image.hxx>
#include <workspace/core/ui/leave.hxx>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// The race results screen (Screens/RaceResults, Screens/RaceResultsPoints - the
// PostGame screens), transliterated from the disassembly (batch 20).
//
// --- VuRaceResultsTableEntity (ctor 0x4687C0, vtable 0x73DA38) -------------
// The ctor: Screen Scale +0x64 = (100, 100), Position +0x6C, Text Highlight Color
// +0x74 = 0xFF00C8FF, six column blocks - a rect (x, y, w, h) then a VuUIFont
// (0x626900; its properties under the column's "...Font" name 0x628100) - Place
// +0x78 / font +0x88, Name +0xEC / +0xFC, Boat +0x160 / +0x170, BestLap +0x1D4 /
// +0x1E4, TotalTime +0x248 / +0x258, PointsEarned +0x2BC / +0x2CC; the byte +0x330
// (IsChampionship) = 0; the row block +0x334 (0x443BE0: Width 800, VerticalSpacing
// 24, HeaderColor +0x33C, Inactive 0xFF808080, Active 0xFFC0C0C0, Player
// 0xFFFFFFFF, the active row count +0x34C = 0, then a VuArray<int> +0x350 size /
// +0x354 capacity / +0x358 data / +0x35C owned, reserved to 16 and filled with 16 x
// -1); the Vu2dLayoutComponent (0x46BB90: member 0x469BD0, bool) and the
// VuUIDrawComponent (0x46BC20: member 0x46A500, float - +0x5C) and two VuRetVal
// input plugs "IsCircuit" 0x469B70 / "IsShowingPointsEarned" 0x469BB0.
//
// THE RACE DATA it reads (g_vugameutil 0x758FF8 +0x50, the game-data root):
//   GameData.IsChampionship (bool, tag 3) - slot 6 only;
//   GameResults.CircuitRace (bool; anything else = true);
//   GameResults.Table (array, tag 5) of rows, each:
//     TierEntry (bool) - a tier row: TierName (string), TierTime (number) only;
//     DriverName (string), Boat (string: a VuStringDB key), IsFinished (bool),
//     LapsCompleted (int: 1 int / 2 truncated / 7 low dword), BestLapTime,
//     TotalTime (numbers: 1 int / 2 float / 7 int64, anything else 0),
//     IsLocal, IsHuman (bools), PadIndex (int, as LapsCompleted).
// The rows are drawn in the table's order: row k's place is k + 1.

enum ui_results_column_t : int { ui_results_place, ui_results_name, ui_results_boat, ui_results_best, ui_results_total, ui_results_points };

struct ui_results_table_t
{
    float m_scale[ 2 ] = { 100.0f, 100.0f };      // +0x64 Screen Scale
    float m_pos[ 2 ] = { 0.0f, 0.0f };            // +0x6C Position X / Y
    std::uint32_t m_highlight = 0xFF00C8FFu;      // +0x74 Text Highlight Color
    float m_rect[ 6 ][ 4 ] = { };                 // +0x78 / +0xEC / +0x160 / +0x1D4 / +0x248 / +0x2BC (the columns' X, Y, Width, Height)
    float m_width = 800.0f;                       // +0x334 Width
    float m_spacing = 24.0f;                      // +0x338 VerticalSpacing (also each row block's height)
    std::uint32_t m_header = 0u;                  // +0x33C HeaderColor
    std::uint32_t m_inactive = 0xFF808080u;       // +0x340 InactiveColor
    std::uint32_t m_active = 0xFFC0C0C0u;         // +0x344 ActiveColor
    std::uint32_t m_player = 0xFFFFFFFFu;         // +0x348 PlayerColor
    float m_depth = 0.0f;                         // the draw component's (+0x5C) +0x14 Depth
    // runtime
    std::uint8_t m_championship = 0;              // +0x330
    std::int32_t m_active_rows = 0;               // +0x34C
    std::int32_t m_size = 16;                     // +0x350 (the pad-index array)
    std::int32_t m_capacity = 16;                 // +0x354
    std::vector< std::int32_t > m_pads = std::vector< std::int32_t >( 16, -1 );   // +0x358 (capacity entries; [0, size) meaningful)
};

// The column fonts' property names (each a VuUIFont's "Font" under the column name, 0x628100).
constexpr const char* k_ui_results_font_names[ 6 ] = { "PlaceFont", "NameFont", "BoatFont", "BestLapFont", "TotalTimeFont", "PointsEarnedFont" };

// The table's fields from OUR loaded entity (the property values, defaults included)
// and its draw component's Depth; the runtime state as the ctor leaves it.
inline ui_results_table_t ui_results_table_of( const entity_t& e )
{
    ui_results_table_t t;
    const auto bits = [ &e ]( const char* name, void* out, std::size_t n )
    {
        if ( const ui_slot_t* v = ui_property_value( e, name ) )
            std::memcpy( out, v->m_bits, n );
    };
    bits( "Screen Scale", t.m_scale, 8 );
    bits( "Position X", &t.m_pos[ 0 ], 4 );
    bits( "Position Y", &t.m_pos[ 1 ], 4 );
    bits( "Text Highlight Color", &t.m_highlight, 4 );
    const char* const columns[ 6 ] = { "PlaceText", "NameRect", "BoatRect", "BestLapRect", "TotalTimeRect", "PointsEarnedRect" };
    const char* const parts[ 4 ] = { "X", "Y", "Width", "Height" };
    for ( int c = 0; c < 6; ++c )
        for ( int k = 0; k < 4; ++k )
            bits( ( std::string( columns[ c ] ) + parts[ k ] ).c_str( ), &t.m_rect[ c ][ k ], 4 );
    bits( "Width", &t.m_width, 4 );
    bits( "VerticalSpacing", &t.m_spacing, 4 );
    bits( "HeaderColor", &t.m_header, 4 );
    bits( "InactiveColor", &t.m_inactive, 4 );
    bits( "ActiveColor", &t.m_active, 4 );
    bits( "PlayerColor", &t.m_player, 4 );
    t.m_depth = ui_frame_depth( e );
    return t;
}

// One column's font: 0x626B80's answer on the VuUIFont (its +0x60 asset's +88, else
// VuUIImpl's default 0x81A18C +88; 0 = none: that column is not drawn) and the
// VuUIFont's 60-byte draw params (+0x20).
struct ui_results_font_t
{
    std::uint32_t m_font = 0;
    std::uint8_t m_params[ 60 ] = { };
};

struct ui_results_inputs_t
{
    ui_results_font_t m_fonts[ 6 ];
    // VuStringDB g 0x759078 slot 6 (getString) - world/strings.hxx string_db_get in the game.
    std::function< std::string( const std::string& ) > m_string;
};

// The points per place, .rdata 0x6F48C0 (15 ints); a place past them earns 0.
constexpr std::int32_t k_ui_results_points[ 15 ] = { 25, 20, 16, 13, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };

// A number field as 0x46A500 reads it: 1 an int (cvtdq2ps), 2 the float, 7 the int64
// (fild / fstp - SUBSTITUTION: static_cast< float > of the int64; the same for every
// int64 that a float holds exactly), anything else 0.
inline float ui_results_float( const value_t& v )
{
    if ( v.tag == 1 )
        return static_cast< float >( v.integer );
    if ( v.tag == 2 )
        return v.real;
    if ( v.tag == 7 )
        return static_cast< float >( v.int64 );
    return 0.0f;
}

// An int field: 1 the int, 2 cvttss2si, 7 the low dword; `known` false otherwise.
inline std::int32_t ui_results_int( const value_t& v, bool& known )
{
    known = true;
    if ( v.tag == 1 )
        return v.integer;
    if ( v.tag == 2 )
        return ui_cvtt( v.real );
    if ( v.tag == 7 )
        return static_cast< std::int32_t >( static_cast< std::uint64_t >( v.int64 ) & 0xFFFFFFFFu );
    known = false;
    return 0;
}

inline bool ui_results_bool( const value_t& v ) { return v.tag == 3 && ( v.integer & 0xFF ) != 0; }

// A string field (tag 4) else the static "" (0x64D20B).
inline const char* ui_results_text( const value_t& v ) { return v.tag == 4 ? v.text.c_str( ) : ""; }

// The time text: t < 0 (comiss: NaN is not) -> '-' and -t (xorps); c = cvttss2si( t x
// 1000 ) (0x6F7828); "%d:%02d.%02d" of c / 10 / 100 / 60, c / 10 / 100 % 60, c / 10 %
// 100 (signed truncating division) through _snprintf_s into a 256-byte buffer.
inline std::string ui_results_time( float t )
{
    std::string out;
    if ( 0.0f > t )
    {
        t = -t;
        out = "-";
    }
    const std::int32_t c = ui_cvtt( t * 1000.0f );
    const std::int32_t cs = c / 10;
    const std::int32_t s = cs / 100;
    char b[ 64 ];
    std::snprintf( b, sizeof b, "%d:%02d.%02d", s / 60, s % 60, cs % 100 );
    return out + b;
}

inline std::string ui_results_itoa( std::int32_t v )   // _itoa_s( v, buf, 0x100, 10 )
{
    char b[ 16 ];
    std::snprintf( b, sizeof b, "%d", v );
    return b;
}

inline float ui_results_depth( const ui_results_table_t& t ) { return t.m_depth * 0.005f + 0.5f; }   // 0x6F75C4, 0x6F765C

// The direct column draws of 0x46A500 / 0x469BD0 (the font draw 0x536C10 with the
// column's own params): rect (1/sx x x, 1/sy x y, 1/sx x w, 1/sy x h), flags 0x70,
// alpha 1, offset 0, the depth above.
inline void ui_results_plain_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_results_table_t& t, const ui_results_font_t& f,
                                   const char* text, float x, float y, float w, float h )
{
    if ( !f.m_font )
        return;
    const float ix = 1.0f / t.m_scale[ 0 ];   // 0x6F76B8 = 1.0
    const float iy = 1.0f / t.m_scale[ 1 ];
    const float rect[ 4 ] = { ix * x, iy * y, ix * w, iy * h };
    const float offset[ 2 ] = { 0.0f, 0.0f };
    ui_font_draw( sort, env, f.m_font, text, f.m_params, rect, 0x70u, 1.0f, offset, ui_results_depth( t ) );
}

// 0x46BAA0 (thiscall rect*, VuUIFont*, text, bool highlight; ret 0x10): the font
// (0x626B80; none -> nothing), the params with the colour (+12) replaced by Text
// Highlight Color when highlighted, rect (x / sx, y / sy, w / sx, h / sy as x x 1/sx),
// flags 0x70, alpha 1, offset 0, the depth.
inline void ui_results_text_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_results_table_t& t, const ui_results_font_t& f,
                                  const float r[ 4 ], const char* text, bool highlight )
{
    if ( !f.m_font )
        return;
    std::uint8_t params[ 60 ];
    std::memcpy( params, f.m_params, 60 );
    if ( highlight )
        std::memcpy( params + 12, &t.m_highlight, 4 );
    const float ix = 1.0f / t.m_scale[ 0 ];
    const float iy = 1.0f / t.m_scale[ 1 ];
    const float rect[ 4 ] = { r[ 0 ] * ix, r[ 1 ] * iy, r[ 2 ] * ix, r[ 3 ] * iy };
    const float offset[ 2 ] = { 0.0f, 0.0f };
    ui_font_draw( sort, env, f.m_font, text, params, rect, 0x70u, 1.0f, offset, ui_results_depth( t ) );
}

// 0x443C90 (thiscall on the row block +0x334; position*, scale*; the depth in xmm3;
// ret 8): nothing while the array is empty; else the header block (HeaderColor) at
// the position (Width x VerticalSpacing over the scale) through 0x507830 at depth +
// 0.002 (0x6F75B8), then one block per array entry below it, each VerticalSpacing
// lower: Inactive, Active for k < +0x34C, Player when the entry >= 0.
inline void ui_results_rows_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_results_table_t& t, float depth )
{
    if ( t.m_size == 0 )
        return;
    const float d = depth + 0.002f;
    const float ix = 1.0f / t.m_scale[ 0 ];
    const float iy = 1.0f / t.m_scale[ 1 ];
    const float head[ 4 ] = { ix * t.m_pos[ 0 ], iy * t.m_pos[ 1 ], ix * t.m_width, iy * t.m_spacing };
    ui_solid_draw( sort, env, t.m_header, head, d );
    float y = t.m_spacing + t.m_pos[ 1 ];
    for ( std::int32_t k = 0; k < t.m_size; ++k )
    {
        std::uint32_t colour = t.m_inactive;
        if ( k < t.m_active_rows )
            colour = t.m_active;
        if ( t.m_pads[ static_cast< std::size_t >( k ) ] >= 0 )
            colour = t.m_player;
        const float rect[ 4 ] = { ix * t.m_pos[ 0 ], iy * y, ix * t.m_width, iy * t.m_spacing };
        ui_solid_draw( sort, env, colour, rect, d );
        y = t.m_spacing + y;
    }
}

// The VuArray<int> grow 0x404A40 (reserve: a capacity below n becomes exactly n,
// the first `size` entries copied).
inline void ui_results_reserve( ui_results_table_t& t, std::int32_t n )
{
    if ( t.m_capacity >= n )
        return;
    t.m_pads.resize( static_cast< std::size_t >( n ), 0 );
    t.m_capacity = n;
}

// Slot 6 0x469B10 (onGameInitialize): the data = root "GameData" when an object (tag
// 6), else the dev config g 0x75901C slot 6 ("ActionGameData" - `dev`, null when
// there is no dev config: the static null 0x7593A0); its IsChampionship bool true ->
// +0x330 = 1 (never cleared here).
inline void ui_results_initialize( ui_results_table_t& t, const value_t& root, const value_t* dev )
{
    static const value_t null{};
    const value_t& game = member( root, "GameData" );
    const value_t& data = game.tag == 6 ? game : dev ? *dev : null;
    if ( ui_results_bool( member( data, "IsChampionship" ) ) )
        t.m_championship = 1;
}

// "IsCircuit" 0x469B70: the track manager g_vutrackmanagerimpl's vt+0x20 (built?)
// and then vt+0x24 (circuit?) - both inputs (the world's track manager).
inline bool ui_results_is_circuit( bool built, bool circuit ) { return built && circuit; }

// "IsShowingPointsEarned" 0x469BB0: +0x330.
inline bool ui_results_points_shown( const ui_results_table_t& t ) { return t.m_championship != 0; }

// The draw 0x46A500 (the VuUIDrawComponent's member; the float alpha is never read;
// ret 4). The game-data root's GameResults: the best lap = minss over every row's
// BestLapTime from FLT_MAX (0x6F7864); then per row (y from Position Y, + VerticalSpacing
// per row):
//  - TierEntry true: the place (k + 1, Place font + rect), TierName (Name), TierTime's
//    time text (TotalTime column) - all plain;
//  - else the place, DriverName, VuStringDB slot 6 of Boat (Boat column) - plain; then
//    only when IsFinished: on a circuit with LapsCompleted (an int) > 0 the BestLapTime
//    (a number; else 0, FLT_MAX -> 0) time text, highlighted when equal to the best
//    (ucomiss: NaN never); then TotalTime's time text when LapsCompleted >= (circuit ?
//    3 : 1), else VuStringDB slot 6 "RaceResults_006" - highlighted on the first row;
//    then, when +0x330, the points (k < 15 ? 0x6F48C0[k] : 0 when the laps reach, else
//    0) in the PointsEarned column, highlighted on the first row.
// Then the pad array: resized to the table's count (grown entries -1), +0x34C = that
// count, entry k = PadIndex when IsLocal && IsHuman else -1 (DriverName is looked up
// and dropped; a function-local static string is initialised - not observable). Last
// the row blocks 0x443C90 at the depth.
inline void ui_results_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, ui_results_table_t& t, const ui_results_inputs_t& in,
                             const value_t& root )
{
    static const value_t null{};
    const auto lookup = [ & ]( const std::string& id ) { return in.m_string ? in.m_string( id ) : std::string( ); };
    const value_t& results = member( root, "GameResults" );
    const value_t& table = member( results, "Table" );
    bool circuit = true;
    const value_t& c = member( results, "CircuitRace" );
    if ( c.tag == 3 )
        circuit = ( c.integer & 0xFF ) != 0;
    const std::int32_t count = table.tag == 5 ? static_cast< std::int32_t >( table.list.size( ) ) : 0;   // 0x4FBC40
    const auto row_of = [ & ]( std::int32_t k ) -> const value_t& { return table.tag == 5 && k >= 0 && k < count ? table.list[ static_cast< std::size_t >( k ) ] : null; };
    float best = 3.4028235e38f;
    for ( std::int32_t k = 0; k < count; ++k )
    {
        const float v = ui_results_float( member( row_of( k ), "BestLapTime" ) );
        best = best < v ? best : v;   // minss best, v: the second operand on NaN / equal
    }
    const ui_results_font_t* f = in.m_fonts;
    const float x0 = t.m_pos[ 0 ];
    float y = t.m_pos[ 1 ];
    for ( std::int32_t k = 0; k < count; ++k )
    {
        const value_t& row = row_of( k );
        const std::string place = ui_results_itoa( k + 1 );
        const float* pr = t.m_rect[ ui_results_place ];
        ui_results_plain_draw( sort, env, t, f[ ui_results_place ], place.c_str( ), pr[ 0 ] + x0, pr[ 1 ] + y, pr[ 2 ], pr[ 3 ] );
        const float* nr = t.m_rect[ ui_results_name ];
        const float* tr = t.m_rect[ ui_results_total ];
        if ( ui_results_bool( member( row, "TierEntry" ) ) )
        {
            ui_results_plain_draw( sort, env, t, f[ ui_results_name ], ui_results_text( member( row, "TierName" ) ), x0 + nr[ 0 ], nr[ 1 ] + y, nr[ 2 ], nr[ 3 ] );
            const std::string time = ui_results_time( ui_results_float( member( row, "TierTime" ) ) );
            ui_results_plain_draw( sort, env, t, f[ ui_results_total ], time.c_str( ), tr[ 0 ] + x0, tr[ 1 ] + y, tr[ 2 ], tr[ 3 ] );
            y = t.m_spacing + y;
            continue;
        }
        ui_results_plain_draw( sort, env, t, f[ ui_results_name ], ui_results_text( member( row, "DriverName" ) ), x0 + nr[ 0 ], nr[ 1 ] + y, nr[ 2 ], nr[ 3 ] );
        const std::string boat = lookup( ui_results_text( member( row, "Boat" ) ) );
        const float* br = t.m_rect[ ui_results_boat ];
        ui_results_plain_draw( sort, env, t, f[ ui_results_boat ], boat.c_str( ), br[ 0 ] + x0, br[ 1 ] + y, br[ 2 ], br[ 3 ] );
        if ( ui_results_bool( member( row, "IsFinished" ) ) )
        {
            bool known = false;
            if ( circuit && ui_results_int( member( row, "LapsCompleted" ), known ) > 0 && known )
            {
                const value_t& b = member( row, "BestLapTime" );
                float lap = 0.0f;
                if ( b.tag == 1 || b.tag == 2 || b.tag == 7 )
                {
                    lap = ui_results_float( b );
                    if ( lap == 3.4028235e38f )   // ucomiss / lahf / test ah 0x44 / jp: equal (ordered) -> 0
                        lap = 0.0f;
                }
                const std::string text = ui_results_time( lap );
                const float* r = t.m_rect[ ui_results_best ];
                const float rect[ 4 ] = { r[ 0 ] + x0, r[ 1 ] + y, r[ 2 ], r[ 3 ] };
                ui_results_text_draw( sort, env, t, f[ ui_results_best ], rect, text.c_str( ), lap == best );
            }
            const std::int32_t laps = ui_results_int( member( row, "LapsCompleted" ), known );
            const std::int32_t need = circuit ? 3 : 1;
            const float rect[ 4 ] = { tr[ 0 ] + x0, tr[ 1 ] + y, tr[ 2 ], tr[ 3 ] };
            if ( laps >= need )
            {
                const std::string text = ui_results_time( ui_results_float( member( row, "TotalTime" ) ) );
                ui_results_text_draw( sort, env, t, f[ ui_results_total ], rect, text.c_str( ), k == 0 );
            }
            else
            {
                const std::string text = lookup( "RaceResults_006" );
                ui_results_text_draw( sort, env, t, f[ ui_results_total ], rect, text.c_str( ), k == 0 );
            }
            if ( t.m_championship )
            {
                const std::int32_t points = ui_results_int( member( row, "LapsCompleted" ), known ) >= need ? ( k >= 15 ? 0 : k_ui_results_points[ k ] ) : 0;
                const std::string text = ui_results_itoa( points );
                const float* r = t.m_rect[ ui_results_points ];
                const float pr4[ 4 ] = { x0 + r[ 0 ], r[ 1 ] + y, r[ 2 ], r[ 3 ] };
                ui_results_text_draw( sort, env, t, f[ ui_results_points ], pr4, text.c_str( ), k == 0 );
            }
        }
        y = t.m_spacing + y;
    }
    if ( count >= t.m_size )
    {
        if ( count > t.m_size )
            ui_results_reserve( t, count );
        for ( std::int32_t k = t.m_size; k < count; ++k )
            t.m_pads[ static_cast< std::size_t >( k ) ] = -1;
    }
    t.m_size = count;
    t.m_active_rows = count;
    for ( std::int32_t k = 0; k < count; ++k )
    {
        const value_t& row = row_of( k );
        std::int32_t pad = -1;
        if ( ui_results_bool( member( row, "IsLocal" ) ) && ui_results_bool( member( row, "IsHuman" ) ) )
        {
            bool known = false;
            pad = ui_results_int( member( row, "PadIndex" ), known );
        }
        t.m_pads[ static_cast< std::size_t >( k ) ] = pad;
    }
    ui_results_rows_draw( sort, env, t, ui_results_depth( t ) );
}

// The layout draw 0x469BD0 (the Vu2dLayoutComponent's member; the bool is never
// read; ret 4): twelve sample rows from Position Y, + VerticalSpacing each: the place
// k + 1, "DRIVER NAME", "BOAT NAME" (plain), "9:99:99" in the BestLap rect and in the
// TotalTime rect - BOTH with the TotalTime font (+0x258) - highlighted on the first,
// then 12 - k in the PointsEarned column (plain). Then the array is 12 entries (grown
// to capacity 12 by an aligned_malloc of 0x30 when below; entries from the old size
// -1; a larger array is cut to 12), +0x34C = 12, entry 0 = 0; the row blocks.
inline void ui_results_layout_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, ui_results_table_t& t, const ui_results_inputs_t& in )
{
    const ui_results_font_t* f = in.m_fonts;
    const float x0 = t.m_pos[ 0 ];
    float y = t.m_pos[ 1 ];
    for ( std::int32_t k = 0; k < 12; ++k )
    {
        const std::string place = ui_results_itoa( k + 1 );
        const float* pr = t.m_rect[ ui_results_place ];
        ui_results_plain_draw( sort, env, t, f[ ui_results_place ], place.c_str( ), x0 + pr[ 0 ], pr[ 1 ] + y, pr[ 2 ], pr[ 3 ] );
        const float* nr = t.m_rect[ ui_results_name ];
        ui_results_plain_draw( sort, env, t, f[ ui_results_name ], "DRIVER NAME", nr[ 0 ] + x0, y + nr[ 1 ], nr[ 2 ], nr[ 3 ] );
        const float* br = t.m_rect[ ui_results_boat ];
        ui_results_plain_draw( sort, env, t, f[ ui_results_boat ], "BOAT NAME", br[ 0 ] + x0, br[ 1 ] + y, br[ 2 ], br[ 3 ] );
        const float* lr = t.m_rect[ ui_results_best ];
        const float best[ 4 ] = { x0 + lr[ 0 ], lr[ 1 ] + y, lr[ 2 ], lr[ 3 ] };
        ui_results_text_draw( sort, env, t, f[ ui_results_total ], best, "9:99:99", k == 0 );
        const float* tr = t.m_rect[ ui_results_total ];
        const float total[ 4 ] = { x0 + tr[ 0 ], tr[ 1 ] + y, tr[ 2 ], tr[ 3 ] };
        ui_results_text_draw( sort, env, t, f[ ui_results_total ], total, "9:99:99", k == 0 );
        const std::string points = ui_results_itoa( 12 - k );
        const float* qr = t.m_rect[ ui_results_points ];
        ui_results_plain_draw( sort, env, t, f[ ui_results_points ], points.c_str( ), qr[ 0 ] + x0, qr[ 1 ] + y, qr[ 2 ], qr[ 3 ] );
        y = t.m_spacing + y;
    }
    if ( t.m_size < 12 )
    {
        if ( t.m_capacity < 12 )
        {
            t.m_pads.resize( 12, 0 );
            t.m_capacity = 12;
        }
        for ( std::int32_t k = t.m_size; k < 12; ++k )
            t.m_pads[ static_cast< std::size_t >( k ) ] = -1;
    }
    t.m_size = 12;
    t.m_active_rows = 12;
    t.m_pads[ 0 ] = 0;
    ui_results_rows_draw( sort, env, t, ui_results_depth( t ) );
}

// --- VuUserNextUIAction (vtable 0x73EA10) ----------------------------------
// Slot 12 0x470E50 (stdcall VuRetVal* out, params; ret 8): broadcast FNV-1a
// "OnUserNext" with empty VuParams (VuEventManager_broadcast 0x505690 - a HOOK),
// VuRetVal +0 = 0 (its type word only). The trigger's params are never read.
inline std::uint32_t ui_user_next( const std::function< void( std::uint32_t, const ui_params_t& ) >& broadcast )
{
    if ( broadcast )
        broadcast( fsm_hash( "OnUserNext" ), ui_params_t{ } );
    return 0u;
}
