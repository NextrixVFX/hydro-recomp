#pragma once

#include <workspace/core/ui/compose.hxx>

#include <array>
#include <cstdint>
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
// What a leaf draw then emits: VuUITextBaseEntity-drawn entities (text) ->
// ui_text_draw (draw.hxx: the font record + its 32-byte sort command, checked
// on 940 real entities); VuUIImageEntity (slot 13 0x629300), the buttons'
// own draws, the HUD gauges' (gauge.hxx / map.hxx are recreated for the HUD
// path only) and every other type: NOT recreated here - `m_kind` says which.
//
// How the world agent draws it: walk m_draws in order; for a `text` leaf build
// ui_text_base_t from the entity's loaded properties (the fields
// verify_ui_text_draw takes: Screen Scale, X / Y / Width / Height, the
// alignments, No Clip / Single Line / Wordbreak, the anchor, the draw
// component's Depth) and call ui_text_draw into one ui_gfx_sort_t with an env
// whose m_matrix / m_top are this leaf's; the sort's commands (sorted by their
// 64-bit key, as VuGfxSort submits) are the GPU work - the submission (the
// record's callback 0x5375E0: glyph layout + the device) stays a hook. The
// text string (VuStringDB) and the font's texture are hooks too.

enum class ui_frame_kind_t
{
    text,      // slot 14 0x627C60 family (VuUITextEntity and the text base's types)
    image,     // VuUIImageEntity: 0x629300 NOT recreated
    other      // any other UI-drawn type: its draw NOT recreated
};

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
};

struct ui_frame_t
{
    std::vector< ui_frame_draw_t > m_draws;
    int m_containers = 0;  // container draws walked through
    int m_unrecreated = 0; // leaves whose draw is not recreated (image / other)
};

// The leaf kinds by loaded type name (the text base's drawn types are the ones
// the text_draw case found with slot 14 == 0x627C60).
inline ui_frame_kind_t ui_frame_kind_of( const entity_t& e )
{
    static const char* const texts[ ] = { "VuUITextEntity", "VuUITextBaseEntity" };
    for ( const char* t : texts )
        if ( e.m_type == t )
            return ui_frame_kind_t::text;
    if ( e.m_type == "VuUIImageEntity" )
        return ui_frame_kind_t::image;
    return ui_frame_kind_t::other;
}

// `sort` holds the key words before the frame (VuGfxSort +0x70 / +0x74);
// `layer` = the screen's +0x5C (4 from its ctor).
inline ui_frame_t ui_screen_frame( const entity_t& screen, const ui_impl_t& impl, ui_gfx_sort_t& sort, float alpha, std::uint32_t layer = 4 )
{
    ui_frame_t frame;
    ui_gfx_stacks_t stacks;
    int depth = 0;
    std::function< void( const entity_t&, int, float ) > draw = [ & ]( const entity_t& e, int component, float a )
    {
        if ( e.m_type == "VuUIContainerEntity" )
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
        d.m_kind = ui_frame_kind_of( e );
        if ( d.m_kind != ui_frame_kind_t::text )
            ++frame.m_unrecreated;
        frame.m_draws.push_back( d );
    };
    ui_screen_draw( sort, stacks, impl, screen, layer, alpha, draw );
    return frame;
}
