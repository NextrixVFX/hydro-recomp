#pragma once

#include <workspace/core/assets/scene.hxx>
#include <workspace/core/ui/compose.hxx>
#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/gauge.hxx>
#include <workspace/core/ui/image.hxx>

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

// The message box's draws as data: VuMessageBoxScreenEntity (factory 0x4643B0,
// vtable 0x73D8E4; Screens/MessageBox). Its VuUIDrawComponent's delegate calls
// the member 0x4675A0 (the screen-level draw, census ui_draw_targets); its
// Vu2dLayoutComponent draws through 0x467710. Both reach 0x467990, the box
// itself. All three are transliterated below from the disassembly (tools
// tmp/b25/d_4675A0.txt, d_467990.txt, d_4681A0.txt, d_467710.txt).
//
// The five text blocks (Heading, Body, BodyImage, ButtonA, ButtonB) are 0x9C
// bytes apart from +0x78. Each block's enabled byte (+0x80: +0xF8, ... +0x2CC
// ButtonA, +0x368 ButtonB) and text (+0x84) are NOT properties - the box's
// runtime state, like +0x528 / +0x52C / +0x530 / +0x570 / +0x578 / +0x588,
// whose writers are not read here: they are INPUTS.

// One text block (this + 0x78 + k x 0x9C).
struct ui_message_block_t
{
    float m_rect[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };  // +0x00 X, Y, Width, Height
    std::uint8_t m_params[ 60 ] = { };               // +0x30 the VuUIFont's (+0x10) draw params (its +0x20)
    std::int32_t m_h_align = 0;                      // +0x74 (the flags block 0x5546B0 reads)
    std::int32_t m_v_align = 2;                      // +0x78
    std::uint8_t m_no_clip = 0;                      // +0x7C
    std::uint8_t m_single_line = 0;                  // +0x7D
    std::uint8_t m_wordbreak = 0;                    // +0x7E
    std::uint8_t m_enabled = 0;                      // +0x80 runtime (INPUT)
    std::string m_text;                              // +0x84 runtime std::string (INPUT)
    ui_text_font_t m_font;                           // the VuUIFont's asset +0x70 -> +0x58, else VuUIImpl's default 0x81A18C -> +0x58
};

struct ui_message_box_t
{
    float m_screen_scale[ 2 ] = { 100.0f, 100.0f };  // +0x60 "Screen Scale"
    float m_image[ 4 ] = { };                        // +0x68 Image X, Y, Width, Height
    ui_message_block_t m_blocks[ 5 ];                // +0x78 Heading, Body, BodyImage, ButtonA, ButtonB
    float m_button_rect[ 2 ][ 4 ] = { };             // +0x384 / +0x3E0 ButtonA / ButtonB Texture X, Y, Width, Height
    std::uint8_t m_button_flip[ 2 ] = { };           // +0x3DC / +0x438 ButtonA / ButtonB Texture FlipX
    std::uint32_t m_selected_colour = 0xFFFFFFFFu;   // +0x439 "Selected Text Color" (unaligned)
    float m_spinner_rect[ 4 ] = { };                 // +0x440 Animated Texture X, Y, Width, Height
    std::uint32_t m_spinner_colour = 0xFFFFFFFFu;    // +0x468 "Animated Texture Color"
    float m_touch[ 2 ][ 4 ] = { };                   // +0x470 / +0x480 ButtonA / ButtonB Touch X, Y, Width, Height
    std::uint8_t m_image_flip = 0;                   // +0x528 runtime (INPUT)
    float m_progress[ 2 ] = { 1.0f, 1.0f };          // +0x52C / +0x530 runtime (INPUT)
    float m_rotation = 0.0f;                         // +0x578 runtime: the spinner's angle (INPUT)
    std::int32_t m_button = 0;                       // +0x588 runtime: 1 / 2 ButtonA selected / pressed, 3 / 4 ButtonB (INPUT)
    float m_depth = 0.0f;                            // the draw component (+0x4AC) +0x14 "Depth"
};

// The textures each draw reaches (opaque handles; 0 = no asset, or an asset
// whose +0x58 is null - both draw nothing).
struct ui_message_box_inputs_t
{
    std::uint32_t m_buttons[ 6 ] = { };   // +0x490..+0x4A4: the asset properties' +0x4C asset +0x58 (A: normal, +0x588 == 1, == 2; B: normal, == 3, == 4)
    std::uint32_t m_spinner = 0;          // +0x4A8's (Animated Texture Asset)
    std::uint32_t m_image = 0;            // +0x570 (a texture asset, its +0x58)
};

// 0x4681A0 (thiscall: rect*, the asset, flip byte, alpha; ret 0x10): one
// button texture at its rect over the screen scale (no anchor), the uv flipped
// when FlipX, white with the alpha byte fistp( alpha x 255 ), depth 0.005 x
// Depth + 0.5 + 1.788e-7, under the matrix stack's top.
inline void ui_message_button_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_message_box_t& b, const float rect[ 4 ],
                                    std::uint32_t texture, std::uint8_t flip, float alpha )
{
    if ( !texture )
        return;
    const float one = ui_f32( 0x3F800000u );   // 0x6F76B8
    const float ix = one / b.m_screen_scale[ 0 ];
    const float iy = one / b.m_screen_scale[ 1 ];
    const float dst[ 4 ] = { rect[ 0 ] * ix, rect[ 1 ] * iy, rect[ 2 ] * ix, rect[ 3 ] * iy };
    const float uv[ 4 ] = { flip ? one : 0.0f, 0.0f, flip ? ui_f32( 0xBF800000u ) : one, one };
    const std::uint32_t a = static_cast< std::uint32_t >( ui_fistp32( alpha * ui_f32( 0x437F0000u ) ) ) & 0xFFu;   // 0x6F7808: 255
    const float depth = b.m_depth * ui_f32( 0x3BA3D70Au ) + ui_f32( 0x3F000000u ) + ui_f32( 0x34400000u );   // 0x6F75C4, 0x6F765C, 0x6F7588
    ui_texture_draw( sort, env, texture, 0x00FFFFFFu | ( a << 24 ), dst, uv, depth );   // 0x507360
}

// 0x467990 (thiscall; the alpha in xmm1): the box. env.m_matrix is the matrix
// stack's top on entry. `aspect` is VuUIImpl +0x90 (0x81A188). false: a path
// not recreated (the font draw's immediate path - these flags never set it).
inline bool ui_message_box_contents_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_string_db_t& db, float aspect,
                                          const ui_message_box_t& b, const ui_message_box_inputs_t& in, float alpha )
{
    const float one = ui_f32( 0x3F800000u );    // 0x6F76B8
    const float half = ui_f32( 0x3F000000u );   // 0x6F765C
    const float scale = ui_f32( 0x3BA3D70Au );  // 0x6F75C4: 0.005
    const float nudge = ui_f32( 0x34400000u );  // 0x6F7588: 1.788e-7
    bool known = true;
    for ( int k = 0; k < 5; ++k )
    {
        const ui_message_block_t& t = b.m_blocks[ k ];
        if ( !t.m_enabled )
            continue;
        const std::uint32_t font = t.m_font.m_asset ? t.m_font.m_asset_font : t.m_font.m_default ? t.m_font.m_default_font : 0u;
        if ( !font )
            continue;
        std::uint8_t params[ 60 ];
        std::memcpy( params, t.m_params, 60 );
        // ButtonA's / ButtonB's colour (params +12) = Selected Text Color while it is selected or pressed
        if ( ( k == 3 && ( b.m_button == 1 || b.m_button == 2 ) ) || ( k == 4 && ( b.m_button == 3 || b.m_button == 4 ) ) )
            std::memcpy( params + 12, &b.m_selected_colour, 4 );
        const float ix = one / b.m_screen_scale[ 0 ];
        const float iy = one / b.m_screen_scale[ 1 ];
        const float rect[ 4 ] = { t.m_rect[ 0 ] * ix, t.m_rect[ 1 ] * iy, t.m_rect[ 2 ] * ix, t.m_rect[ 3 ] * iy };
        const float offset[ 2 ] = { 0.0f, 0.0f };
        const float depth = b.m_depth * scale + half;
        const std::uint32_t flags = ui_font_flags( t.m_h_align, t.m_v_align, t.m_no_clip, t.m_single_line, t.m_wordbreak, db );   // 0x5546B0 (block +0x74)
        known = ui_font_draw( sort, env, font, t.m_text.c_str( ), params, rect, flags, alpha, offset, depth ) && known;   // 0x536C10
    }
    // the button textures: ButtonA while its block is enabled, then ButtonB
    if ( b.m_blocks[ 3 ].m_enabled )
    {
        const std::uint32_t t = b.m_button == 1 ? in.m_buttons[ 1 ] : b.m_button == 2 ? in.m_buttons[ 2 ] : in.m_buttons[ 0 ];
        ui_message_button_draw( sort, env, b, b.m_button_rect[ 0 ], t, b.m_button_flip[ 0 ], alpha );
    }
    if ( b.m_blocks[ 4 ].m_enabled )
    {
        const std::uint32_t t = b.m_button == 3 ? in.m_buttons[ 4 ] : b.m_button == 4 ? in.m_buttons[ 5 ] : in.m_buttons[ 3 ];
        ui_message_button_draw( sort, env, b, b.m_button_rect[ 1 ], t, b.m_button_flip[ 1 ], alpha );
    }
    // the spinner, only with neither button: rotated about its centre with the aspect undone around it (image.hxx's chain)
    if ( !b.m_blocks[ 3 ].m_enabled && !b.m_blocks[ 4 ].m_enabled && in.m_spinner )
    {
        const float ix = one / b.m_screen_scale[ 0 ];
        const float iy = one / b.m_screen_scale[ 1 ];
        const float x = b.m_spinner_rect[ 0 ] * ix;
        const float y = b.m_spinner_rect[ 1 ] * iy;
        const float w = b.m_spinner_rect[ 2 ] * ix;
        const float h = b.m_spinner_rect[ 3 ] * iy;
        const float cx = w * half + x;
        const float cy = h * half + y;
        float m[ 16 ], t[ 16 ];
        const float identity[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };   // 0x7593B0
        std::memcpy( m, identity, 64 );
        ui_matrix_translation( t, ui_f32( ui_bits( cx ) ^ 0x80000000u ), ui_f32( ui_bits( cy ) ^ 0x80000000u ), ui_f32( 0x80000000u ) );   // 0x410AB0, xorps 0x6F7CD0
        scene_multiply( m, m, t );   // VuMatrix_multiply 0x4078C0 (this = m)
        ui_matrix_scale( t, aspect, one, one );   // VuMatrix_fromScale 0x407DB0
        scene_multiply( m, m, t );
        ui_matrix_rotation_z( t, b.m_rotation );   // 0x40B5F0
        scene_multiply( m, m, t );
        ui_matrix_scale( t, one / aspect, one, one );
        scene_multiply( m, m, t );
        ui_matrix_translation( t, cx, cy, 0.0f );
        scene_multiply( m, m, t );
        ui_font_draw_env_t pushed = env;   // 0x50B4B0: m . top, popped inline after the draw
        scene_multiply( m, pushed.m_matrix, env.m_matrix );
        const float a = static_cast< float >( static_cast< std::int32_t >( b.m_spinner_colour >> 24 ) ) * alpha;
        const std::uint32_t colour = ( b.m_spinner_colour & 0x00FFFFFFu ) | ( static_cast< std::uint32_t >( ui_fistp32( a ) & 0xFF ) << 24 );
        const float rect[ 4 ] = { x, y, w, h };
        const float uv[ 4 ] = { 0.0f, 0.0f, one, one };
        const float depth = b.m_depth * scale + half + nudge;
        ui_texture_draw( sort, pushed, in.m_spinner, colour, rect, uv, depth );   // 0x507360
    }
    // +0x570's image over Image X..Height, cut to the +0x52C / +0x530 fractions about its centre
    if ( in.m_image )
    {
        const std::uint32_t a = static_cast< std::uint32_t >( ui_fistp32( alpha * ui_f32( 0x437F0000u ) ) ) & 0xFFu;   // 0x6F7808: 255
        const float ix = one / b.m_screen_scale[ 0 ];
        const float iy = one / b.m_screen_scale[ 1 ];
        const float w = b.m_image[ 2 ] * ix;
        const float x = b.m_image[ 0 ] * ix;
        const float y = b.m_image[ 1 ] * iy;
        const float h = b.m_image[ 3 ] * iy;
        const float p = b.m_progress[ 0 ];
        const float q = b.m_progress[ 1 ];
        const float rect[ 4 ] = { ( one - p ) * ( w * half ) + x, ( one - q ) * ( h * half ) + y, w * p, h * q };
        const float uv[ 4 ] = { b.m_image_flip ? one : 0.0f, 0.0f, b.m_image_flip ? ui_f32( 0xBF800000u ) : one, one };
        const float depth = b.m_depth * scale + half + nudge;
        ui_texture_draw( sort, env, in.m_image, 0x00FFFFFFu | ( a << 24 ), rect, uv, depth );   // 0x507360
    }
    return known;
}

// 0x4675A0 (thiscall; the alpha on the stack; ret 4): the screen-level draw -
// ui_screen_draw's shape with the layer bits fixed at 5 (0x28000000): the key
// bits, the crop matrix (0x81A104) and VuUIImpl +0x8C pushed, the box
// (0x467990) BEFORE the child walk 0x628070, then both popped (floats first).
inline std::uint32_t ui_message_box_key_hi( std::uint32_t hi )
{
    hi &= 0x7FFFFFFu;
    hi |= 0x28000000u;
    hi &= 0xFCFFFFFFu;
    hi &= 0xFF7FFFFFu;
    hi &= 0xFFC3FFFFu;
    hi |= 0x280000u;
    return hi;
}

inline bool ui_message_box_draw( ui_gfx_sort_t& sort, ui_gfx_stacks_t& stacks, const ui_impl_t& impl, const ui_font_draw_env_t& env,
                                 const ui_string_db_t& db, const entity_t& e, const ui_message_box_t& b, const ui_message_box_inputs_t& in,
                                 float alpha, const std::function< void( const entity_t&, int, float ) >& draw )
{
    sort.m_key_hi = ui_message_box_key_hi( sort.m_key_hi );
    std::array< float, 16 > m;
    std::memcpy( m.data( ), impl.m_crop.m_matrix, sizeof( impl.m_crop.m_matrix ) );
    stacks.m_matrices.push_back( m );
    stacks.m_floats.push_back( impl.m_float );
    ui_font_draw_env_t top = env;
    std::memcpy( top.m_matrix, m.data( ), 64 );
    top.m_top = impl.m_float;
    const bool known = ui_message_box_contents_draw( sort, top, db, impl.m_aspect, b, in, alpha );
    ui_draw_children( e, alpha, draw );
    if ( !stacks.m_floats.empty( ) )
        stacks.m_floats.pop_back( );
    if ( !stacks.m_matrices.empty( ) )
        stacks.m_matrices.pop_back( );
    return known;
}

// 0x467710 (thiscall; byte `layout`; ret 4): the Vu2dLayoutComponent's draw.
// With `layout`: white rects (0x507A00) for the image rect, each of the five
// text blocks in its own font colour (params +12), and both touch rects in
// 0xFF80FFFF - all over the screen scale at depth 0.005 x Depth + 0.5, under
// the matrix stack's top (nothing pushed). Always then the box with alpha 1.0.
inline bool ui_message_box_layout_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_string_db_t& db, float aspect,
                                        const ui_message_box_t& b, const ui_message_box_inputs_t& in, std::uint8_t layout )
{
    const float one = ui_f32( 0x3F800000u );   // 0x6F76B8
    if ( layout )
    {
        const float depth = b.m_depth * ui_f32( 0x3BA3D70Au ) + ui_f32( 0x3F000000u );
        {
            const float ix = one / b.m_screen_scale[ 0 ];
            const float iy = one / b.m_screen_scale[ 1 ];
            const float r[ 4 ] = { b.m_image[ 0 ] * ix, b.m_image[ 1 ] * iy, b.m_image[ 2 ] * ix, b.m_image[ 3 ] * iy };
            ui_rect_draw( sort, env, 0xFFFFFFFFu, r, depth );
        }
        for ( int k = 0; k < 5; ++k )
        {
            const ui_message_block_t& t = b.m_blocks[ k ];
            const float ix = one / b.m_screen_scale[ 0 ];
            const float iy = one / b.m_screen_scale[ 1 ];
            const float r[ 4 ] = { t.m_rect[ 0 ] * ix, t.m_rect[ 1 ] * iy, ix * t.m_rect[ 2 ], t.m_rect[ 3 ] * iy };   // mulss xmm2, [W]: the scale first
            std::uint32_t colour;
            std::memcpy( &colour, t.m_params + 12, 4 );
            ui_rect_draw( sort, env, colour, r, depth );
        }
        for ( int k = 0; k < 2; ++k )
        {
            const float ix = one / b.m_screen_scale[ 0 ];
            const float iy = one / b.m_screen_scale[ 1 ];
            const float r[ 4 ] = { ix * b.m_touch[ k ][ 0 ], b.m_touch[ k ][ 1 ] * iy, b.m_touch[ k ][ 2 ] * ix, b.m_touch[ k ][ 3 ] * iy };
            ui_rect_draw( sort, env, 0xFF80FFFFu, r, depth );
        }
    }
    return ui_message_box_contents_draw( sort, env, db, aspect, b, in, one );
}
