#pragma once

#include <workspace/core/ui/compose.hxx>
#include <workspace/core/ui/image.hxx>
#include <workspace/core/ui/leave.hxx>

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// THE EXPORT: one frame of a loaded screen's draws as data. ui_screen_frame
// runs the screen's draw (VuUIScreenEntity slot 13 0x629D60, ui_screen_draw)
// and, under it, every container's draw (VuUIContainerEntity 0x62F7E0,
// ui_container_draw) recursively - the walk retail makes - and returns each
// LEAF draw retail would run there, in order, with exactly what that draw sees:
// the entity, its UI draw component's index, the alpha, the matrix-stack top
// (the crop matrix), the float-stack top (VuUIImpl +0x8C) and the sort key
// words at that moment. Both walks are checked against retail (verify_ui_screen_
// draw, verify_ui_container_draw; verify_ui_screen_frame re-checks the screen
// level through this path).
//
// What a leaf draw is: retail's VuUIDrawComponent +0x10 delegate calls a member
// pointer on the entity; k_ui_draw_targets below is that pointer per entity
// vtable as the census case ui_draw_targets read it from retail's own loaded
// objects (tools tmp/frametable.py prints the rows). Three families are
// recreated and, given `inputs`, the frame EMITS their real draw records into
// `sort` (m_first / m_commands index sort.m_commands[ sort.m_index ]):
//   text   - member 0x627040 (the text base ctor 0x627070 -> 0x627E30, delegate
//            0x6F703C): if Visible (+0x64), slot 14 0x627C60( slot 12's text,
//            alpha ) = ui_text_draw; slot 12 (the text getter: VuStringDB etc.)
//            is per type and a HOOK input.
//   image  - member 0x4077DE = slot 15 0x4401C0 -> slot 13 0x629300 (image.hxx).
//   button - member 0x443280 (image.hxx ui_button_draw).
// Everything else (VuInfoUITextEntity's own slot 14 0x446670, the config
// selectors' 0x448F40, the combo boxes, the boat stats, ...) is `other`: NOT
// recreated; m_target names the retail function.

enum class ui_frame_kind_t
{
    text,       // 0x627040 -> slot 14 0x627C60 (VuUITextEntity and the text base's types)
    image,      // slot 15 0x4401C0 -> slot 13 0x629300
    other,      // any other UI-drawn type: its draw NOT recreated
    button,     // 0x443280
    container,  // 0x62F7E0: walked into, never a leaf
    info        // VuInfoUITextEntity: 0x627040 -> its slot 14 0x446670 (draw.hxx ui_info_text_draw; needs inputs->m_info)
};

struct ui_draw_target_t
{
    std::uint32_t m_vtable;   // the entity's vtable
    std::uint32_t m_method;   // the delegate's member pointer (+8)
    std::uint32_t m_target;   // the function it reaches (a thunk resolved through the vtable; text: slot 14)
    ui_frame_kind_t m_kind;
    std::uint32_t m_text;     // text: slot 12, the text getter (a HOOK); 0 otherwise
};

// From the census case ui_draw_targets on the 67 front-end / HUD projects (every
// loaded entity's FIRST VuUIDrawComponent's delegate: vtable, slot 2 0x404A00 /
// 0x422000, member pointer).
inline const ui_draw_target_t k_ui_draw_targets[ ] = {
    { 0x006F72C4u, 0x0062E120u, 0x0062E120u, ui_frame_kind_t::other, 0x00000000u },   // Screens/Credits (3 loaded)
    { 0x006F737Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/Cheats (1 loaded)
    { 0x006F7434u, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00446DE0u },   // HUDs/GhostGameHUD (253 loaded)
    { 0x006F7484u, 0x004077DEu, 0x004401C0u, ui_frame_kind_t::image, 0x00000000u },   // HUDs/GhostGameHUD (311 loaded)
    { 0x006F74F0u, 0x0062F7E0u, 0x0062F7E0u, ui_frame_kind_t::container, 0x00000000u },   // HUDs/GhostGameHUD (339 loaded)
    { 0x00738AC4u, 0x004077E8u, 0x004257A0u, ui_frame_kind_t::other, 0x00000000u },   // HUDs/GhostGameHUD (8 loaded)
    { 0x00738B24u, 0x004077E8u, 0x00425F70u, ui_frame_kind_t::other, 0x00000000u },   // HUDs/GhostGameHUD (8 loaded)
    { 0x00738BDCu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x004242D0u },   // HUDs/GhostGameHUD (8 loaded)
    { 0x00738C64u, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00423F40u },   // HUDs/GhostGameHUD (40 loaded)
    { 0x00738E10u, 0x004077EDu, 0x00427BD0u, ui_frame_kind_t::other, 0x00000000u },   // HUDs/GhostGameHUD (8 loaded)
    { 0x00739020u, 0x0042A0C0u, 0x0042A0C0u, ui_frame_kind_t::other, 0x00000000u },   // HUDs/GhostGameHUD (32 loaded)
    { 0x00739058u, 0x0042A0C0u, 0x0042A0C0u, ui_frame_kind_t::other, 0x00000000u },   // HUDs/GhostGameHUD (24 loaded)
    { 0x00739250u, 0x0042BA00u, 0x0042BA00u, ui_frame_kind_t::other, 0x00000000u },   // HUDs/GhostGameHUD (8 loaded)
    { 0x0073B4FCu, 0x004077DEu, 0x004401C0u, ui_frame_kind_t::image, 0x00000000u },   // Screens/LoadingChamp (5 loaded)
    { 0x0073B5DCu, 0x004077DEu, 0x004401C0u, ui_frame_kind_t::image, 0x00000000u },   // Screens/Award (2 loaded)
    { 0x0073B7B0u, 0x00443280u, 0x00443280u, ui_frame_kind_t::button, 0x00000000u },   // Screens/Achievements (104 loaded)
    { 0x0073B86Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/Award (2 loaded)
    { 0x0073BC5Cu, 0x00627040u, 0x00446670u, ui_frame_kind_t::info, 0x00446860u },   // Screens/Achievements (39 loaded)
    { 0x0073BC9Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00445F00u },   // Screens/Achievements (39 loaded)
    { 0x0073BCDCu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00445DE0u },   // Screens/Achievements (39 loaded)
    { 0x0073BD1Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00445C50u },   // Screens/Achievements (12 loaded)
    { 0x0073BD5Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x004458F0u },   // Screens/Achievements (12 loaded)
    { 0x0073BD9Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00445680u },   // Screens/AchievementNotification (1 loaded)
    { 0x0073BDDCu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/BoatConfig (10 loaded)
    { 0x0073BE1Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00445340u },   // Screens/Cheats (1 loaded)
    { 0x0073BE9Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/LoadingChamp (15 loaded)
    { 0x0073BEDCu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00444720u },   // Screens/LoadingChamp (15 loaded)
    { 0x0073BF1Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/LoadingChamp (5 loaded)
    { 0x0073BF9Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/LoadingChampSlalom (2 loaded)
    { 0x0073BFDCu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/LoadingChamp (5 loaded)
    { 0x0073C0BCu, 0x004077DEu, 0x004401C0u, ui_frame_kind_t::image, 0x00000000u },   // Screens/Award (2 loaded)
    { 0x0073C18Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00446DE0u },   // Screens/Award (2 loaded)
    { 0x0073C25Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00446DE0u },   // Screens/Award (2 loaded)
    { 0x0073C32Cu, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00446DE0u },   // Screens/Award (2 loaded)
    { 0x0073C69Cu, 0x004077E8u, 0x00448F40u, ui_frame_kind_t::other, 0x00000000u },   // Screens/BoatConfig (5 loaded)
    { 0x0073C808u, 0x0044FB60u, 0x0044FB60u, ui_frame_kind_t::other, 0x00000000u },   // Screens/BoatConfig (20 loaded)
    { 0x0073C998u, 0x00451CA0u, 0x00451CA0u, ui_frame_kind_t::other, 0x00000000u },   // Screens/Championship (1 loaded)
    { 0x0073CC34u, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/SinglePlayer (1 loaded)
    { 0x0073CC74u, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/SinglePlayer (1 loaded)
    { 0x0073CCB4u, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/SinglePlayer (1 loaded)
    { 0x0073CCF4u, 0x00627040u, 0x00627C60u, ui_frame_kind_t::text, 0x00443920u },   // Screens/SinglePlayer (1 loaded)
    { 0x0073CE3Cu, 0x004077E3u, 0x0045AF40u, ui_frame_kind_t::other, 0x00000000u },   // Screens/BoatConfig (11 loaded)
    { 0x0073CE74u, 0x004077E3u, 0x0045AA30u, ui_frame_kind_t::other, 0x00000000u },   // Screens/BoatConfig (11 loaded)
    { 0x0073D1DCu, 0x004077E8u, 0x00448F40u, ui_frame_kind_t::other, 0x00000000u },   // Screens/GameConfigChampionship (5 loaded)
    { 0x0073D45Cu, 0x0045FB90u, 0x0045FB90u, ui_frame_kind_t::other, 0x00000000u },   // Screens/ControlsGamepad (2 loaded)
    { 0x0073D4F4u, 0x0045FB90u, 0x0045FB90u, ui_frame_kind_t::other, 0x00000000u },   // Screens/ControlsGamepad (14 loaded)
    { 0x0073D724u, 0x004632E0u, 0x004632E0u, ui_frame_kind_t::other, 0x00000000u },   // Screens/LoadingChamp (7 loaded)
    { 0x0073D8E4u, 0x004675A0u, 0x004675A0u, ui_frame_kind_t::other, 0x00000000u },   // Screens/MessageBox (1 loaded)
    { 0x0073DA38u, 0x0046A500u, 0x0046A500u, ui_frame_kind_t::other, 0x00000000u },   // Screens/RaceResults (2 loaded)
    { 0x0073DE6Cu, 0x004390BEu, 0x00475B10u, ui_frame_kind_t::other, 0x00000000u },   // Screens/SettingsGraphics (1 loaded)
    { 0x0073DED4u, 0x004390BEu, 0x00475B10u, ui_frame_kind_t::other, 0x00000000u },   // Screens/Settings (2 loaded)
    { 0x0073DF3Cu, 0x004390BEu, 0x00475B10u, ui_frame_kind_t::other, 0x00000000u },   // Screens/Settings (2 loaded)
    { 0x0073DFA4u, 0x004390BEu, 0x00475B10u, ui_frame_kind_t::other, 0x00000000u },   // Screens/Settings (2 loaded)
    { 0x0073DFF4u, 0x004700E0u, 0x004700E0u, ui_frame_kind_t::other, 0x00000000u },   // Screens/SettingsGraphics (1 loaded)
    { 0x0073E030u, 0x004700E0u, 0x004700E0u, ui_frame_kind_t::other, 0x00000000u },   // Screens/SettingsAudio (1 loaded)
    { 0x0073E06Cu, 0x004700E0u, 0x004700E0u, ui_frame_kind_t::other, 0x00000000u },   // Screens/SettingsAudio (1 loaded)
    { 0x0073E0BCu, 0x004390BEu, 0x00475B10u, ui_frame_kind_t::other, 0x00000000u },   // Screens/ControlsGamepad (1 loaded)
    { 0x0073E10Cu, 0x004700E0u, 0x004700E0u, ui_frame_kind_t::other, 0x00000000u },   // Screens/ControlsTiltTouch (1 loaded)
    { 0x0073E15Cu, 0x004390BEu, 0x00475B10u, ui_frame_kind_t::other, 0x00000000u },   // Screens/ControlsTiltTouch (1 loaded)
    { 0x0073F110u, 0x00477240u, 0x00477240u, ui_frame_kind_t::other, 0x00000000u },   // HUDs/GhostPreGameHUD (1 loaded)
    { 0x0073F2C8u, 0x0047A360u, 0x0047A360u, ui_frame_kind_t::other, 0x00000000u },   // Screens/Leaderboard (1 loaded)
    { 0x0073F408u, 0x0047DB40u, 0x0047DB40u, ui_frame_kind_t::other, 0x00000000u },   // Screens/GameConfigGhost (4 loaded)
};

inline const ui_draw_target_t* ui_draw_target_of( const entity_t& e )
{
    if ( !e.m_ctor )
        return nullptr;
    for ( const ui_draw_target_t& t : k_ui_draw_targets )
        if ( t.m_vtable == e.m_ctor->m_vtable )
            return &t;
    return nullptr;
}

struct ui_frame_draw_t
{
    const entity_t* m_entity = nullptr;
    int m_component = 0;
    float m_alpha = 1.0f;
    std::array< float, 16 > m_matrix{ };
    float m_top = 1.0f;
    std::uint32_t m_key_lo = 0;
    std::uint32_t m_key_hi = 0;
    int m_depth = 0;  // container nesting under the screen (0 = a direct child)
    ui_frame_kind_t m_kind = ui_frame_kind_t::other;
    std::uint32_t m_target = 0;   // the retail draw this leaf runs (0: a type the census did not see)
    // With inputs: the commands this leaf emitted into the sort (sort.m_commands[ sort.m_index ]).
    int m_first = 0;
    int m_commands = 0;
    bool m_emitted = false;       // a recreated draw ran (text / image / button)
};

struct ui_frame_t
{
    std::vector< ui_frame_draw_t > m_draws;
    int m_containers = 0;  // container draws walked through
    int m_unrecreated = 0; // leaves whose draw is not recreated (other)
    int m_unknown = 0;     // emitted draws that reached a path not recreated (the font draw's immediate path)
};

// The leaf kinds by the loaded entity's vtable (the census table).
inline ui_frame_kind_t ui_frame_kind_of( const entity_t& e )
{
    const ui_draw_target_t* t = ui_draw_target_of( e );
    return t ? t->m_kind : ui_frame_kind_t::other;
}

// What the recreated leaf draws need beyond the loaded properties - each a HOOK
// for the rendering / string layers. Null callbacks answer "none".
struct ui_frame_inputs_t
{
    ui_font_draw_env_t m_env;            // the materials (the matrix / float tops are each leaf's)
    ui_string_db_t m_db;                 // 0x5546B0's "right-ea-left" answer
    const value_t* m_fontdb = nullptr;   // FontDB's document (VuUIFont's reload 0x626CF0 over it)
    // A text leaf's slot 12 / a button's VuStringDB slot 7 on its String ID.
    std::function< std::string( const entity_t& ) > m_text;
    // 0x626B80: the font's asset font (+0x58), else VuUIImpl's default.
    std::function< ui_text_font_t( const entity_t& ) > m_font;
    // An image's slot 14 (which 0); a button's selected / unselected / pressed (which 0 / 1 / 2). Null: none.
    std::function< const ui_texture_t*( const entity_t&, int which ) > m_texture;
    // A button's runtime +0x188 (focused) / +0x198 (press) - c_ui_button's; default 0 / 0.
    std::function< void( const entity_t&, std::uint8_t& focused, std::uint32_t& press ) > m_button;
    // UNKNOWN: the font params' bytes 41..59 (retail's 0x626CF0 copies them from stack it never wrote); this
    // may fill them per entity (else they stay 0).
    std::function< void( const entity_t&, std::uint8_t params[ 60 ] ) > m_params_tail;
    // VuInfoUITextEntity's ticker state +0x12C..+0x13C (its tick is NOT recreated). Null: its leaves stay unrecreated.
    std::function< ui_info_scroll_t( const entity_t& ) > m_info;
};

// A loaded entity's own / its first UI draw component's property bits.
inline bool ui_frame_take( const entity_t& e, const char* name, void* out, std::size_t bytes )
{
    const ui_slot_t* s = ui_property_value( e, name );
    if ( !s )
        return false;
    std::memcpy( out, s->m_bits, bytes );
    return true;
}

inline float ui_frame_depth( const entity_t& e )
{
    float d = 0.0f;
    for ( const entity_component_t& c : e.m_components )
        if ( c.m_vtable == k_ui_draw_component_vtable )
        {
            if ( c.m_state )
            {
                const ui_state_t* s = static_cast< const ui_state_t* >( c.m_state.get( ) );
                for ( int k = 0; k < s->m_count; ++k )
                    if ( std::strcmp( s->m_rows[ k ]->m_name, "Depth" ) == 0 )
                        std::memcpy( &d, s->m_value[ k ].m_bits, 4 );
            }
            break;
        }
    return d;
}

inline void ui_frame_params( const entity_t& e, const ui_frame_inputs_t& in, std::uint8_t params[ 60 ] )
{
    std::memset( params, 0, 60 );
    if ( in.m_params_tail )
        in.m_params_tail( e, params );
    const ui_slot_t* font = ui_property_value( e, "Font" );
    std::string asset;
    if ( in.m_fontdb )
        ui_font_params( *in.m_fontdb, font ? font->m_text : std::string( ), params, asset );
}

// One recreated leaf's draw from the loaded properties (the field lists the
// draw checks verify against retail's bytes).
inline bool ui_frame_emit( ui_gfx_sort_t& sort, const ui_impl_t& impl, const ui_frame_draw_t& d, const ui_frame_inputs_t& in )
{
    const entity_t& e = *d.m_entity;
    ui_font_draw_env_t env = in.m_env;
    std::memcpy( env.m_matrix, d.m_matrix.data( ), 64 );
    env.m_top = d.m_top;
    if ( d.m_kind == ui_frame_kind_t::text || d.m_kind == ui_frame_kind_t::info )
    {
        std::uint8_t visible = 1;
        ui_frame_take( e, "Visible", &visible, 1 );   // 0x627040: +0x64
        if ( !visible )
            return true;
        ui_text_base_t t;
        ui_frame_take( e, "Screen Scale", t.m_screen_scale, 8 );
        ui_frame_take( e, "X", &t.m_rect[ 0 ], 4 );
        ui_frame_take( e, "Y", &t.m_rect[ 1 ], 4 );
        ui_frame_take( e, "Width", &t.m_rect[ 2 ], 4 );
        ui_frame_take( e, "Height", &t.m_rect[ 3 ], 4 );
        ui_frame_take( e, "Horizontal Alignment", &t.m_h_align, 4 );
        ui_frame_take( e, "Vertical Alignment", &t.m_v_align, 4 );
        ui_frame_take( e, "No Clip", &t.m_no_clip, 1 );
        ui_frame_take( e, "Single Line", &t.m_single_line, 1 );
        ui_frame_take( e, "Wordbreak", &t.m_wordbreak, 1 );
        ui_frame_take( e, "Horizontal Anchor", &t.m_anchor.m_horizontal, 4 );
        ui_frame_take( e, "Horizontal Ratio", &t.m_anchor.m_h_ratio, 4 );
        ui_frame_take( e, "Vertical Anchor", &t.m_anchor.m_vertical, 4 );
        ui_frame_take( e, "Vertical Ratio", &t.m_anchor.m_v_ratio, 4 );
        t.m_depth = ui_frame_depth( e );
        ui_frame_params( e, in, t.m_params );
        const ui_text_font_t font = in.m_font ? in.m_font( e ) : ui_text_font_t( );
        const std::string text = in.m_text ? in.m_text( e ) : std::string( );
        if ( d.m_kind == ui_frame_kind_t::info )
            return ui_info_text_draw( sort, env, impl.m_crop, in.m_db, t, font, text, in.m_info( e ), d.m_alpha );
        return ui_text_draw( sort, env, impl.m_crop, in.m_db, t, font, text, d.m_alpha );
    }
    if ( d.m_kind == ui_frame_kind_t::image )
    {
        ui_image_t im;
        ui_frame_take( e, "Visible", &im.m_visible, 1 );
        ui_frame_take( e, "Color", &im.m_colour, 4 );
        ui_frame_take( e, "Screen Scale", im.m_screen_scale, 8 );
        ui_frame_take( e, "DstX", &im.m_dst[ 0 ], 4 );
        ui_frame_take( e, "DstY", &im.m_dst[ 1 ], 4 );
        ui_frame_take( e, "DstWidth", &im.m_dst[ 2 ], 4 );
        ui_frame_take( e, "DstHeight", &im.m_dst[ 3 ], 4 );
        ui_frame_take( e, "SrcX", &im.m_src[ 0 ], 4 );
        ui_frame_take( e, "SrcY", &im.m_src[ 1 ], 4 );
        ui_frame_take( e, "SrcWidth", &im.m_src[ 2 ], 4 );
        ui_frame_take( e, "SrcHeight", &im.m_src[ 3 ], 4 );
        ui_frame_take( e, "Rotation", &im.m_rotation, 4 );
        ui_frame_take( e, "FlipX", &im.m_flip_x, 1 );
        ui_frame_take( e, "FlipY", &im.m_flip_y, 1 );
        ui_frame_take( e, "Absolute Coords", &im.m_absolute, 1 );
        ui_frame_take( e, "Horizontal Anchor", &im.m_anchor.m_horizontal, 4 );
        ui_frame_take( e, "Horizontal Ratio", &im.m_anchor.m_h_ratio, 4 );
        ui_frame_take( e, "Vertical Anchor", &im.m_anchor.m_vertical, 4 );
        ui_frame_take( e, "Vertical Ratio", &im.m_anchor.m_v_ratio, 4 );
        im.m_depth = ui_frame_depth( e );
        const ui_texture_t* texture = in.m_texture ? in.m_texture( e, 0 ) : nullptr;
        ui_image_component_draw( sort, env, impl.m_crop, impl.m_aspect, im, texture, d.m_alpha );
        return true;
    }
    if ( d.m_kind == ui_frame_kind_t::button )
    {
        ui_button_draw_t b;
        ui_frame_take( e, "Enabled", &b.m_enabled, 1 );
        ui_frame_take( e, "Label", &b.m_label, 1 );
        ui_frame_take( e, "Selected Text Color", &b.m_selected_colour, 4 );
        ui_frame_take( e, "Screen Scale", b.m_screen_scale, 8 );
        ui_frame_take( e, "X", &b.m_rect[ 0 ], 4 );
        ui_frame_take( e, "Y", &b.m_rect[ 1 ], 4 );
        ui_frame_take( e, "Width", &b.m_rect[ 2 ], 4 );
        ui_frame_take( e, "Height", &b.m_rect[ 3 ], 4 );
        ui_frame_take( e, "Horizontal Alignment", &b.m_h_align, 4 );
        ui_frame_take( e, "Vertical Alignment", &b.m_v_align, 4 );
        ui_frame_take( e, "No Clip", &b.m_no_clip, 1 );
        ui_frame_take( e, "Single Line", &b.m_single_line, 1 );
        ui_frame_take( e, "Wordbreak", &b.m_wordbreak, 1 );
        ui_frame_take( e, "Horizontal Anchor", &b.m_anchor.m_horizontal, 4 );
        ui_frame_take( e, "Horizontal Ratio", &b.m_anchor.m_h_ratio, 4 );
        ui_frame_take( e, "Vertical Anchor", &b.m_anchor.m_vertical, 4 );
        ui_frame_take( e, "Vertical Ratio", &b.m_anchor.m_v_ratio, 4 );
        b.m_depth = ui_frame_depth( e );
        ui_frame_params( e, in, b.m_params );
        if ( in.m_button )
            in.m_button( e, b.m_focused, b.m_press );
        ui_button_inputs_t bi;
        const ui_text_font_t font = in.m_font ? in.m_font( e ) : ui_text_font_t( );
        bi.m_font = font.m_asset ? font.m_asset_font : font.m_default ? font.m_default_font : 0u;
        bi.m_text = in.m_text ? in.m_text( e ) : std::string( );
        if ( in.m_texture )
        {
            bi.m_selected = in.m_texture( e, 0 );
            bi.m_unselected = in.m_texture( e, 1 );
            bi.m_pressed = in.m_texture( e, 2 );
        }
        return ui_button_draw( sort, env, impl.m_crop, in.m_db, b, bi, d.m_alpha );
    }
    return true;
}

// `sort` holds the key words before the frame (VuGfxSort +0x70 / +0x74);
// `layer` = the screen's +0x5C (4 from its ctor). With `inputs`, the text /
// image / button leaves emit their records into `sort` in walk order.
inline ui_frame_t ui_screen_frame( const entity_t& screen, const ui_impl_t& impl, ui_gfx_sort_t& sort, float alpha, std::uint32_t layer = 4,
                                   const ui_frame_inputs_t* inputs = nullptr )
{
    ui_frame_t frame;
    ui_gfx_stacks_t stacks;
    int depth = 0;
    std::function< void( const entity_t&, int, float ) > draw = [ & ]( const entity_t& e, int component, float a )
    {
        const ui_draw_target_t* target = ui_draw_target_of( e );
        if ( target && target->m_kind == ui_frame_kind_t::container )
        {
            ++frame.m_containers;
            ++depth;
            ui_container_draw( e, a, draw );
            --depth;
            return;
        }
        ui_frame_draw_t d;
        d.m_entity = &e;
        d.m_component = component;
        d.m_alpha = a;
        if ( !stacks.m_matrices.empty( ) )
            d.m_matrix = stacks.m_matrices.back( );
        if ( !stacks.m_floats.empty( ) )
            d.m_top = stacks.m_floats.back( );
        d.m_key_lo = sort.m_key_lo;
        d.m_key_hi = sort.m_key_hi;
        d.m_depth = depth;
        d.m_kind = target ? target->m_kind : ui_frame_kind_t::other;
        d.m_target = target ? target->m_target : 0u;
        if ( d.m_kind == ui_frame_kind_t::other || ( d.m_kind == ui_frame_kind_t::info && !( inputs && inputs->m_info ) ) )
            ++frame.m_unrecreated;
        else if ( inputs )
        {
            const std::size_t before = sort.m_commands[ sort.m_index ].size( );
            if ( !ui_frame_emit( sort, impl, d, *inputs ) )
                ++frame.m_unknown;
            d.m_first = static_cast< int >( before );
            d.m_commands = static_cast< int >( sort.m_commands[ sort.m_index ].size( ) - before );
            d.m_emitted = true;
        }
        frame.m_draws.push_back( d );
    };
    ui_screen_draw( sort, stacks, impl, screen, layer, alpha, draw );
    return frame;
}