#pragma once

#include <workspace/core/assets/scene.hxx>
#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/gauge.hxx>

#include <cstdint>
#include <cstring>
#include <string>

// The UI image's draw as data: VuUIImageEntity (ctor 0x62F610 over the image
// base 0x6284E0, vtable 0x6F7484). The base's VuUIDrawComponent (0x629850, at
// +0x64; delegate vtable 0x6F70BC, slot 2 0x422000) calls the member pointer
// 0x4077DE = jmp [vt+0x3C]: slot 15 0x4401C0 = "if Visible (+0x6C): slot 13
// 0x629300( alpha )". 0x629300 is transliterated below from the disassembly;
// its texture comes from slot 14 (VuUIImageEntity's 0x62F770: the "Texture
// Asset" property +0xD0 -> its asset +0x4C -> +0x58, null without one - the
// asset itself is the rendering layer's, so the texture here is an INPUT).

// What 0x629300 reads (the base ctor's property block; the offsets are the
// captured schema's).
struct ui_image_t
{
    std::uint8_t m_visible = 1;                        // +0x6C "Visible" (read by slot 15)
    std::uint32_t m_colour = 0xFFFFFFFFu;              // +0x6D "Color" (unaligned)
    float m_screen_scale[ 2 ] = { 1080.0f, 720.0f };   // +0x74 "Screen Scale"
    float m_dst[ 4 ] = { 40.0f, 40.0f, 20.0f, 20.0f }; // +0x7C DstX, DstY, DstWidth, DstHeight
    float m_src[ 4 ] = { 0.0f, 0.0f, 1.0f, 1.0f };     // +0x8C SrcX, SrcY, SrcWidth, SrcHeight
    float m_rotation = 0.0f;                           // +0x9C "Rotation" (VuAngleProperty: radians)
    std::uint8_t m_flip_x = 0;                         // +0xA0 "FlipX"
    std::uint8_t m_flip_y = 0;                         // +0xA1 "FlipY"
    std::uint8_t m_absolute = 0;                       // +0xA2 "Absolute Coords"
    ui_anchor_t m_anchor;                              // +0xA4 / +0xA8 the anchors, +0xAC / +0xB0 the ratios
    float m_alpha = 1.0f;                              // +0xB4 (the ctor's 1.0; not a property)
    float m_depth = 0.0f;                              // the draw component (+0x64) +0x14 "Depth"
};

inline std::uint32_t ui_bits( float f )
{
    std::uint32_t b;
    std::memcpy( &b, &f, 4 );
    return b;
}

// 0x507830 (VuGfxUtil; depth in xmm1; colour*, rect*; ret 8): byte for byte
// 0x507A00's 84-byte record and command (the material ***(util+4)+8) with the
// callback 0x5078E0 (the GPU side, NOT recreated).
constexpr std::uint32_t k_ui_solid_callback = 0x5078E0u;

inline void ui_solid_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, std::uint32_t colour, const float rect[ 4 ], float depth )
{
    std::uint8_t* r = sort.allocate( k_ui_rect_record );
    std::memcpy( r + 0, env.m_matrix, 64 );
    std::memcpy( r + 0x40, &colour, 4 );
    std::memcpy( r + 0x44, rect, 16 );
    sort.submit( 17, env.m_rect_material, env.m_rect_material_key, 0, 0, k_ui_solid_callback, depth );
}

// 0x629300 (thiscall; the alpha on the stack; ret 4). `aspect` is VuUIImpl
// +0x90 (0x81A188: 0x626190 sets 16 / 9 = 0x3FE38E39); env.m_matrix is the
// matrix stack's top on entry (VuGfxUtil +0x20). `texture` null = slot 14's
// null answer.
inline void ui_image_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_crop_t& crop, float aspect, const ui_image_t& im,
                           const ui_texture_t* texture, float alpha )
{
    const float one = ui_f32( 0x3F800000u );    // 0x6F76B8
    const float half = ui_f32( 0x3F000000u );   // 0x6F765C
    float w = im.m_dst[ 2 ];
    float h = im.m_dst[ 3 ];
    if ( texture )
    {
        // ucomiss / lahf / test ah, 0x44 / jp: only an ordered equal takes the texture's size
        if ( w == 0.0f )
            w = static_cast< float >( texture->m_width );
        if ( h == 0.0f )
            h = static_cast< float >( texture->m_height );
    }
    const float ix = one / im.m_screen_scale[ 0 ];
    const float iy = one / im.m_screen_scale[ 1 ];
    float rect[ 4 ] = { ix * im.m_dst[ 0 ], iy * im.m_dst[ 1 ], ix * w, iy * h };
    if ( !im.m_absolute )
        ui_anchor_transform( im.m_anchor, crop, rect, rect );   // 0x6282D0 (this +0xA4)

    // the rotation about the rect's centre, the aspect undone around it
    const float cx = rect[ 2 ] * half + rect[ 0 ];
    const float cy = rect[ 3 ] * half + rect[ 1 ];
    float m[ 16 ], t[ 16 ];
    const float identity[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };   // 0x7593B0
    std::memcpy( m, identity, 64 );
    ui_matrix_translation( t, ui_f32( ui_bits( cx ) ^ 0x80000000u ), ui_f32( ui_bits( cy ) ^ 0x80000000u ), ui_f32( 0x80000000u ) );   // xorps 0x6F7CD0
    scene_multiply( m, m, t );                         // VuMatrix_multiply 0x4078C0 (this = m)
    ui_matrix_scale( t, aspect, one, one );            // VuMatrix_fromScale 0x407DB0
    scene_multiply( m, m, t );
    ui_matrix_rotation_z( t, im.m_rotation );          // 0x40B5F0
    scene_multiply( m, m, t );
    ui_matrix_scale( t, one / aspect, one, one );
    scene_multiply( m, m, t );
    ui_matrix_translation( t, cx, cy, 0.0f );          // 0x410AB0
    scene_multiply( m, m, t );

    // the uv rect with the flips (0x6F78A0 = -1)
    float uv[ 4 ] = { im.m_src[ 0 ], im.m_src[ 1 ], im.m_src[ 2 ], im.m_src[ 3 ] };
    const float minus = ui_f32( 0xBF800000u );
    if ( im.m_flip_x )
    {
        uv[ 0 ] = uv[ 0 ] + uv[ 2 ];
        uv[ 2 ] = uv[ 2 ] * minus;
    }
    if ( im.m_flip_y )
    {
        uv[ 1 ] = uv[ 1 ] + uv[ 3 ];
        uv[ 3 ] = uv[ 3 ] * minus;
    }

    // the colour's alpha byte: fistp( (colour >> 24) x alpha x +0xB4 ), its low byte
    const float a = static_cast< float >( static_cast< std::int32_t >( im.m_colour >> 24 ) ) * alpha * im.m_alpha;
    const std::uint32_t colour = ( im.m_colour & 0x00FFFFFFu ) | ( static_cast< std::uint32_t >( ui_fistp32( a ) & 0xFF ) << 24 );

    // pushed (0x50B4B0): the identity when Absolute Coords, else m . top
    ui_font_draw_env_t pushed = env;
    if ( im.m_absolute )
        std::memcpy( pushed.m_matrix, identity, 64 );
    else
        scene_multiply( m, pushed.m_matrix, env.m_matrix );
    const float depth = im.m_depth * ui_f32( 0x3BA3D70Au ) + half;   // 0x6F75C4: 0.005
    if ( texture )
        ui_texture_draw( sort, pushed, texture->m_handle, colour, rect, uv, depth );
    else
        ui_solid_draw( sort, pushed, colour, rect, depth );
    // popped inline (size--, offset 0 at 0)
}

// Slot 15 0x4401C0: the draw component's target.
inline void ui_image_component_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_crop_t& crop, float aspect, const ui_image_t& im,
                                     const ui_texture_t* texture, float alpha )
{
    if ( im.m_visible )
        ui_image_draw( sort, env, crop, aspect, im, texture, alpha );
}

// The button's draw as data: VuBarracudaUIButtonEntity (ctor 0x441170). Its
// VuUIDrawComponent (0x4436C0, at +0x64; delegate vtable 0x73B77C, slot 2
// 0x422000) calls the member pointer 0x443280 directly (pushed by the ctor at
// 0x441402) - transliterated below from the disassembly. The label's text is
// VuStringDB (g 0x759078) slot 7 on "String ID" (+0x6C): the string tables are
// not loaded, so the text is an INPUT (a hook); so are the font (VuUIFont at
// +0xA0: 0x626B80 = its asset +0x100 -> +0x58, else VuUIImpl's default) and the
// three textures (the asset properties at +0x17C / +0x180 / +0x184 -> +0x4C
// asset -> +0x58).
struct ui_button_draw_t
{
    std::uint8_t m_enabled = 1;                         // +0x68 "Enabled": nothing drawn without it
    std::uint8_t m_label = 1;                           // +0x69 "Label"
    std::uint32_t m_selected_colour = 0xFF00FFFFu;      // +0x84 "Selected Text Color"
    float m_screen_scale[ 2 ] = { 100.0f, 100.0f };     // +0x88 "Screen Scale"
    float m_rect[ 4 ] = { 0.0f, 0.0f, 20.0f, 10.0f };   // +0x90 X, Y, Width, Height
    std::uint8_t m_params[ 60 ] = { };                  // +0xC0 the VuUIFont's draw params (+0x20 of +0xA0)
    std::int32_t m_h_align = 0;                         // +0x104
    std::int32_t m_v_align = 2;                         // +0x108
    std::uint8_t m_no_clip = 0;                         // +0x10C
    std::uint8_t m_single_line = 0;                     // +0x10D
    std::uint8_t m_wordbreak = 0;                       // +0x10E
    ui_anchor_t m_anchor;                               // +0x168 / +0x16C the anchors, +0x170 / +0x174 the ratios
    std::uint8_t m_focused = 0;                         // +0x188 (c_ui_button::m_focused)
    std::uint32_t m_press = 0;                          // +0x198 (c_ui_button::m_press)
    float m_depth = 0.0f;                               // the draw component (+0x64) +0x14 "Depth"
};

struct ui_button_inputs_t
{
    std::uint32_t m_font = 0;                           // 0x626B80's answer (0: none)
    std::string m_text;                                 // StringDB slot 7 on String ID (HOOK)
    const ui_texture_t* m_selected = nullptr;           // +0x17C's texture
    const ui_texture_t* m_unselected = nullptr;         // +0x180's
    const ui_texture_t* m_pressed = nullptr;            // +0x184's
};

// 0x443280 (thiscall; the alpha on the stack; ret 4). env.m_matrix is the
// matrix stack's top (nothing is pushed here). false: a path not recreated
// (the font draw's immediate path - the button's flags never set it).
inline bool ui_button_draw( ui_gfx_sort_t& sort, const ui_font_draw_env_t& env, const ui_crop_t& crop, const ui_string_db_t& db,
                            const ui_button_draw_t& b, const ui_button_inputs_t& in, float alpha )
{
    if ( !b.m_enabled )
        return true;
    const float one = ui_f32( 0x3F800000u );    // 0x6F76B8
    const float half = ui_f32( 0x3F000000u );   // 0x6F765C
    const float ix = one / b.m_screen_scale[ 0 ];
    const float iy = one / b.m_screen_scale[ 1 ];
    const float rect[ 4 ] = { ix * b.m_rect[ 0 ], b.m_rect[ 1 ] * iy, b.m_rect[ 2 ] * ix, b.m_rect[ 3 ] * iy };
    float depth = b.m_depth * ui_f32( 0x3BA3D70Au ) + half;   // 0x6F75C4: 0.005
    bool known = true;
    if ( b.m_label )
    {
        if ( in.m_font )
        {
            std::uint8_t params[ 60 ];
            std::memcpy( params, b.m_params, 60 );
            std::uint32_t colour;
            std::memcpy( &colour, params + 12, 4 );
            if ( b.m_focused )
                colour = b.m_selected_colour;
            std::memcpy( params + 12, &colour, 4 );
            params[ 15 ] = static_cast< std::uint8_t >( ui_fistp32( static_cast< float >( static_cast< std::int32_t >( colour >> 24 ) ) * alpha ) );
            float text_rect[ 4 ] = { rect[ 0 ], rect[ 1 ], rect[ 2 ], rect[ 3 ] };
            ui_anchor_transform( b.m_anchor, crop, text_rect, text_rect );   // 0x6282D0 (this +0x168)
            const float offset[ 2 ] = { 0.0f, 0.0f };
            const float text_depth = b.m_depth * ui_f32( 0x3BA3D70Au ) + half;
            const std::uint32_t flags = ui_font_flags( b.m_h_align, b.m_v_align, b.m_no_clip, b.m_single_line, b.m_wordbreak, db );   // 0x5546B0 (+0x104)
            known = ui_font_draw( sort, env, in.m_font, in.m_text.c_str( ), params, text_rect, flags, one, offset, text_depth );   // 0x536C10
        }
        depth = depth + ui_f32( 0x3DCCCCCDu );   // 0x6F7610: 0.1
    }
    const ui_texture_t* t = b.m_focused ? in.m_selected : in.m_unselected;
    if ( b.m_press == 1 )
        t = in.m_pressed;
    if ( !t )
        return known;
    float place[ 4 ] = { rect[ 0 ], rect[ 1 ], static_cast< float >( t->m_width ) / b.m_screen_scale[ 0 ],
                         static_cast< float >( t->m_height ) / b.m_screen_scale[ 1 ] };
    ui_anchor_transform( b.m_anchor, crop, place, place );
    const float unit[ 4 ] = { 0.0f, 0.0f, one, one };
    ui_texture_draw( sort, env, t->m_handle, 0xFFFFFFFFu, place, unit, depth );   // 0x507360
    return known;
}