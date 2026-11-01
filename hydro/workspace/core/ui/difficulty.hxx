#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/ui/config.hxx>
#include <workspace/core/ui/dock.hxx>
#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/gauge.hxx>
#include <workspace/core/world/race.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

// The two "difficulty" leaves of the BoatConfig* screens (one class shown as
// C / B / A): VuDifficultyImageEntity (vtable 0x73CE3C) and VuDifficultyText-
// Entity (0x73CE74). Their VuUIDrawComponent delegate's member 0x4077E3 is
// `jmp [vt+0x30]` - slot 12, the draw (thiscall, the alpha argument unread;
// ret 4): 0x45AF40 / 0x45AA30. Both ask the class picker 0x459D70 first. All
// transliterated from the disassembly (tools tmp\dis_459d70.txt, dis_45af40.txt,
// dis_45aa30.txt, dis_4d37b0.txt).

// What the picker reads outside the entity (g_vugameutil 0x758FF8 and two
// managers). The documents are the whole VUJB trees.
struct ui_difficulty_env_t
{
    const value_t* m_root = nullptr;      // gameutil +0x50 (null: the null container)
    // g_vudevconfig 0x75901C's getParam("ActionGameData") (slot 6) when GameData
    // is not an object; null = no dev config (the null container 0x7593A0).
    const value_t* m_dev = nullptr;
    const value_t* m_boat_db = nullptr;   // gameutil +8's document (BoatDB)
    const value_t* m_champ_db = nullptr;  // gameutil +0x14's document (ChampDB)
    // HOOK: the progress manager 0x758FD8 slot 27 (+0x6C) - the current
    // championship round's container (a save / profile service). Null: none.
    std::function< const value_t&( ) > m_round;
};

// 0x4D37B0 (thiscall gameutil): the human boat's class. GameData (as 0x4D3260:
// gameutil +0x50 ["GameData"] when an object, else the dev config's
// "ActionGameData", else null) ["Boats"]; for i < its count (0x4FBC40, read
// again each pass) the element (an array's, in range; else null) ["Properties"],
// whose "Driver Type" text (tag 4, else "") equal to "Human" (0x404AC0 over
// min(size, 5) bytes, and size 5) names the boat: BoatDB ["VuDBAsset"] find by
// its "Boat" text (0x4F22B0 - the whole string), ["Class"]'s text. No human: "".
inline std::string ui_human_boat_class( const ui_difficulty_env_t& env )
{
    static const value_t null{ };
    const value_t& root = env.m_root ? *env.m_root : null;
    const value_t& boats = member( race_action_game_data( root, env.m_dev ), "Boats" );
    for ( int i = 0; boats.tag == 5 ? i < static_cast< int >( boats.list.size( ) ) : false; ++i )
    {
        const value_t& props = member( boats.list[ static_cast< std::size_t >( i ) ], "Properties" );
        if ( ui_json_text( member( props, "Driver Type" ) ) != "Human" )
            continue;
        const std::string& boat = ui_json_text( member( props, "Boat" ) );
        return ui_json_text( member( ui_db_entry( env.m_boat_db, boat ), "Class" ) );
    }
    return std::string( );
}

// 0x459D70 (thiscall entity; this +0x68 "Loading Screen"): the class -> 0 for
// "C", 1 "B", 2 "A" (each 0x404AC0 over min(size, 1) bytes and size 1), else -1.
//   Loading Screen: GameType (0x4D31D0), Course (0x4D32F0), the human boat's
//   class (0x4D37B0) - all three read first; then GameData ["IsChampionship"]
//   a bool (tag 3) and true -> the round (progress slot 27) ["Name"]'s text
//   finds ChampDB's entry, its ["Class"]; else GameType "Slalom" (VuString_
//   equals 0x404990) -> Course "CourseC" / "CourseB" / "CourseA" -> "C" / "B" /
//   "A" (anything else: none); else the human boat's class.
//   Otherwise GameConfig's Mode / Course / Championship / Boat texts: Mode
//   "Championship" -> ChampDB [Championship] ["Class"]; "Slalom" -> the Course
//   mapping as above; else BoatDB ["VuDBAsset"] [Boat] (0x4FBDA0) ["Class"].
inline int ui_difficulty_class( bool loading_screen, const ui_difficulty_env_t& env )
{
    static const value_t null{ };
    const value_t& root = env.m_root ? *env.m_root : null;
    const auto course_class = []( const std::string& course ) -> std::string
    {
        if ( course == "CourseC" )
            return "C";
        if ( course == "CourseB" )
            return "B";
        if ( course == "CourseA" )
            return "A";
        return std::string( );
    };
    std::string cls;
    if ( loading_screen )
    {
        const std::string type = race_game_data_text( root, env.m_dev, "GameType" );
        const std::string course = race_game_data_text( root, env.m_dev, "Course" );
        const std::string boat_class = ui_human_boat_class( env );
        const value_t& champ = member( race_action_game_data( root, env.m_dev ), "IsChampionship" );
        if ( champ.tag == 3 && ( champ.integer & 0xFF ) != 0 )
        {
            const value_t& round = env.m_round ? env.m_round( ) : null;
            const std::string& name = ui_json_text( member( round, "Name" ) );
            cls = ui_json_text( member( ui_db_entry( env.m_champ_db, name ), "Class" ) );
        }
        else if ( type == "Slalom" )
            cls = course_class( course );
        else
            cls = boat_class;
    }
    else
    {
        const value_t& config = member( root, "GameConfig" );
        const std::string& mode = ui_json_text( member( config, "Mode" ) );
        const std::string& course = ui_json_text( member( config, "Course" ) );
        const std::string& championship = ui_json_text( member( config, "Championship" ) );
        const std::string& boat = ui_json_text( member( config, "Boat" ) );
        if ( mode == "Championship" )
            cls = ui_json_text( member( ui_db_entry( env.m_champ_db, championship ), "Class" ) );
        else if ( mode == "Slalom" )
            cls = course_class( course );
        else
            cls = ui_json_text( member( ui_db_entry( env.m_boat_db, boat ), "Class" ) );
    }
    if ( cls == "C" )
        return 0;
    if ( cls == "B" )
        return 1;
    if ( cls == "A" )
        return 2;
    return -1;
}

// VuDifficultyImageEntity's fields (the loaded properties; ctor's defaults).
struct ui_difficulty_image_t
{
    float m_screen_scale[ 2 ] = { 100.0f, 100.0f };  // +0x60 "Screen Scale"
    std::uint8_t m_loading_screen = 0;               // +0x68 "Loading Screen"
    float m_position[ 2 ] = { 0.0f, 0.0f };          // +0x6C "Position X" / "Position Y"
    float m_depth = 0.0f;                            // the draw component (+0x5C) +0x14 "Depth"
};

// 0x45AF40: k = the picker; k < 0 -> nothing. The k-th texture property's
// object (+0xBC + 4k: Texture Asset C / B / A) +0x4C asset, its +0x58 texture
// (`texture`: null either way -> nothing); the texture draw 0x507360 at ((1/sx)
// x, (1/sy) y, (1/sx) width, (1/sy) height) - the texture's own size (+0xC /
// +0x10, cvtdq2ps) - uv (0, 0, 1, 1), white, depth Depth x 0.005 + 0.5.
inline void ui_difficulty_image_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_difficulty_image_t& f, int k,
                                      const ui_texture_t* texture )
{
    if ( k < 0 || !texture )
        return;
    const float ix = 1.0f / f.m_screen_scale[ 0 ];   // 0x6F76B8 (1.0)
    const float iy = 1.0f / f.m_screen_scale[ 1 ];
    const float rect[ 4 ] = { ix * f.m_position[ 0 ], iy * f.m_position[ 1 ], ix * static_cast< float >( texture->m_width ),
                              iy * static_cast< float >( texture->m_height ) };
    const float uv[ 4 ] = { 0.0f, 0.0f, 1.0f, 1.0f };
    const float depth = f.m_depth * 0.005f + 0.5f;   // 0x6F75C4, 0x6F765C
    ui_texture_draw( sort, env, texture->m_handle, 0xFFFFFFFFu, rect, uv, depth );
}

// VuDifficultyTextEntity's fields (the loaded properties; ctor's defaults).
struct ui_difficulty_text_t
{
    float m_screen_scale[ 2 ] = { 100.0f, 100.0f };  // +0x60 "Screen Scale"
    std::uint8_t m_loading_screen = 0;               // +0x68 "Loading Screen"
    float m_rect[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };  // +0x6C X, Y, Width, Height
    std::uint8_t m_params[ 3 ][ 60 ] = { };          // +0x9C + 100k: Font C / B / A's VuUIFont params (+0x20 of +0x7C + 100k)
    std::int32_t m_h_align = 0;                      // +0x1A8
    std::int32_t m_v_align = 2;                      // +0x1AC
    std::uint8_t m_no_clip = 0;                      // +0x1B0
    std::uint8_t m_single_line = 0;                  // +0x1B1
    std::uint8_t m_wordbreak = 0;                    // +0x1B2
    std::string m_string_id[ 3 ];                    // +0x1B4 + 24k: C / B / A String ID
    float m_depth = 0.0f;                            // the draw component (+0x5C) +0x14 "Depth"
};

// 0x45AA30: k = the picker; k < 0 -> nothing. The font 0x626B80 on the k-th
// VuUIFont (+0x7C + 100k: `font` - its asset's, else VuUIImpl's default; none
// -> nothing, before anything else). Then VuStringDB (g 0x759078) slot 7 on the
// k-th String ID (`text`: the caller's answer), the rect (X, Y, Width, Height)
// over the screen scale (no anchor transform), the k-th params (60 bytes copied
// by rep movsd), the flags 0x5546B0 on +0x1A8, alpha 1.0 (the argument is
// unread), offset (0, 0), depth Depth x 0.005 + 0.5 -> the font draw 0x536C10.
inline bool ui_difficulty_text_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_string_db_t& db, const ui_difficulty_text_t& t,
                                     int k, const ui_text_font_t& font, const std::function< std::string( const std::string& id ) >& text )
{
    if ( k < 0 || k > 2 )
        return true;
    const std::uint32_t f = font.m_asset ? font.m_asset_font : font.m_default ? font.m_default_font : 0u;
    if ( !f )
        return true;
    const float ix = 1.0f / t.m_screen_scale[ 0 ];   // 0x6F76B8 (1.0)
    const float iy = 1.0f / t.m_screen_scale[ 1 ];
    const float depth = t.m_depth * 0.005f + 0.5f;   // 0x6F75C4, 0x6F765C
    const float rect[ 4 ] = { t.m_rect[ 0 ] * ix, t.m_rect[ 1 ] * iy, t.m_rect[ 2 ] * ix, t.m_rect[ 3 ] * iy };
    const std::string s = text ? text( t.m_string_id[ k ] ) : std::string( );
    const float offset[ 2 ] = { 0.0f, 0.0f };
    const std::uint32_t flags = ui_font_flags( t.m_h_align, t.m_v_align, t.m_no_clip, t.m_single_line, t.m_wordbreak, db );
    return ui_font_draw( sort, env, f, s.c_str( ), t.m_params[ k ], rect, flags, 1.0f, offset, depth );
}
