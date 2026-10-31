#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/boat/math.hxx>
#include <workspace/core/entity/object.hxx>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

// Five pieces of the VuBoatEntity ctor 0x41B850, recreated as what each one
// leaves in memory. Nothing reads these members yet, so they are kept as the
// byte image retail's ctor writes, at retail's offsets; a member gets a name
// when the code that reads it is recreated. Heap pointers are not part of
// the image (their slots stay 0): each is an owned member instead.

// The byte-image helpers (image_put32, image_zero, image_empty_string) are
// engine/workspace/core/entity/object.hxx's.


// 0x5240C0 on a fresh 0x104-byte block. +0x34 is a new 16-byte block (its
// contents never written); +0x38 / +0x44 are _aligned_malloc(0x140, 16)
// arrays (memcpy_s of 0 bytes into each, so unwritten), each with a capacity
// of 8 two words later.
struct boat_5240c0_t
{
    std::uint8_t image[ 0x104 ]{};
    std::unique_ptr< std::uint8_t[ ] > at_34;
    std::unique_ptr< std::uint8_t[ ] > at_38;
    std::unique_ptr< std::uint8_t[ ] > at_44;
};

inline std::unique_ptr< boat_5240c0_t > boat_5240c0( )
{
    auto o = std::make_unique< boat_5240c0_t >( );
    std::uint8_t* b = o->image;
    image_zero( b, 0, 3 );
    image_put32( b, 20, 0x447A0000u );
    image_put32( b, 24, 0x447A0000u );
    image_put32( b, 28, 0x3F800000u );
    image_zero( b, 8, 10 );
    image_put32( b, 48, 0 );
    image_zero( b, 14, 16 );
    o->at_38.reset( new std::uint8_t[ 0x140 ] );
    image_put32( b, 64, 8 );
    image_zero( b, 17, 19 );
    o->at_44.reset( new std::uint8_t[ 0x140 ] );
    image_put32( b, 76, 8 );
    b[ 80 ] = 0;
    image_zero( b, 21, 22 );
    image_put32( b, 92, 0x7F7FFFFFu );
    image_zero( b, 24, 26 );
    image_zero( b, 28, 30 );
    image_zero( b, 32, 34 );
    image_zero( b, 36, 38 );
    image_zero( b, 40, 42 );
    image_zero( b, 44, 46 );
    image_zero( b, 48, 50 );
    image_zero( b, 52, 54 );
    image_zero( b, 56, 57 );
    image_put32( b, 232, 0x3F800000u );
    image_zero( b, 60, 62 );
    image_put32( b, 256, 0xFF7FFFFFu );
    o->at_34.reset( new std::uint8_t[ 16 ] );
    return o;
}

// The callers' allocation of a 0x5240C0 object: operator new(0x104), then
// +0, +0x34, +0x38, +0x44 zeroed before the ctor runs (all of which it
// rewrites).
inline std::unique_ptr< boat_5240c0_t > boat_new_5240c0( )
{
    return boat_5240c0( );
}

// 0x41B400: 22 constants.
struct boat_41b400_t
{
    std::uint8_t image[ 0x58 ]{};
};

inline void boat_41b400( boat_41b400_t& o )
{
    static const std::uint32_t words[ 22 ] = {
        0x43FA0000u, 0xFF000000u, 0x7F7FFFFFu, 0x7F7FFFFFu, 0xFF000000u, 0xFF000000u, 0xFFFFFFFFu, 0x00000000u,
        0x3F800000u, 0x3F800000u, 0xC0423C2Du, 0xC05A503Cu, 0xFF808080u, 0xFFFFFFFFu, 0x41600000u, 0xFF808080u,
        0xFFFFFFFFu, 0x3F000000u, 0x3F000000u, 0xFFC0C0C0u, 0xFFFFFFFFu, 0xFFFFFFFFu };
    for ( int word = 0; word < 22; ++word )
        image_put32( o.image, 4 * word, words[ word ] );
}

// 0x41CA50 (thiscall, one unused argument): words 12..34 and 37 zeroed.
struct boat_41ca50_t
{
    std::uint8_t image[ 0xA0 ]{};
};

inline void boat_41ca50( boat_41ca50_t& o )
{
    image_zero( o.image, 12, 34 );
    image_put32( o.image, 148, 0 );
}

// 0x4192E0 (thiscall, the owner at +0x30): two empty strings (+0, +0x18), an
// empty box (+0x98 FLT_MAX x3, +0xA8 -FLT_MAX x3) and a 0x5240C0 object at
// +0x34. Each box half is stored as a qword pair whose upper dword is a stack
// local retail never sets (+0xA4, +0xB4) - indeterminate, not modelled.
struct boat_4192e0_t
{
    std::uint8_t image[ 0xB8 ]{};
    std::unique_ptr< boat_5240c0_t > at_34;
};

inline void boat_4192e0( boat_4192e0_t& o, std::uint32_t owner )
{
    std::uint8_t* b = o.image;
    image_empty_string( b, 0 );
    image_empty_string( b, 24 );
    image_put32( b, 48, owner );
    image_zero( b, 14, 35 );
    b[ 144 ] = 0;
    image_put32( b, 148, 0 );
    image_put32( b, 152, 0x7F7FFFFFu );
    image_put32( b, 156, 0x7F7FFFFFu );
    image_put32( b, 160, 0x7F7FFFFFu );
    image_put32( b, 168, 0xFF7FFFFFu );
    image_put32( b, 172, 0xFF7FFFFFu );
    image_put32( b, 176, 0xFF7FFFFFu );
    o.at_34 = boat_new_5240c0( );
}

// The 8-byte element 0x40D810 constructs inside the ctors' eh-vector arrays:
// a u16 1 at +0 (bytes +2, +3 left), a zero dword at +4.
inline void parts_handle( std::uint8_t* image, int at )
{
    const std::uint16_t one = 1;
    std::memcpy( image + at, &one, 2 );
    image_put32( image, at + 4, 0 );
}

// 0x40FE00 (thiscall, the owner at +0): three empty strings (+0x14, +0x2C,
// +0x44), -1 / 0 / 1 x4 / 0 / 0, then the arrays +0x7C (3 empty strings) and
// +0xC4 (3 x 0x40D810).
struct boat_40fe00_t
{
    std::uint8_t image[ 0xDC ]{};
};

inline void boat_40fe00( boat_40fe00_t& o, std::uint32_t owner )
{
    std::uint8_t* b = o.image;
    image_put32( b, 0, owner );
    image_zero( b, 1, 4 );
    image_empty_string( b, 20 );
    image_empty_string( b, 44 );
    image_empty_string( b, 68 );
    image_put32( b, 92, 0xBF800000u );
    image_put32( b, 96, 0 );
    for ( int word = 25; word <= 28; ++word )
        image_put32( b, 4 * word, 0x3F800000u );
    image_zero( b, 29, 30 );
    for ( int k = 0; k < 3; ++k )
        image_empty_string( b, 124 + 24 * k );
    for ( int k = 0; k < 3; ++k )
        parts_handle( b, 196 + 8 * k );
}

// 0x413590 (thiscall, the owner at +0): defaults, an _aligned_malloc(0x20,
// 16) array at +0x30 (capacity 8 at +0x38), a zero u16 at +0x48, the arrays
// +0x4C (5 empty strings) and +0xC4 (5 x 0x40D810), then six zero dwords.
struct boat_413590_t
{
    std::uint8_t image[ 0x104 ]{};
    std::unique_ptr< std::uint8_t[ ] > at_30;
};

inline void boat_413590( boat_413590_t& o, std::uint32_t owner )
{
    std::uint8_t* b = o.image;
    image_put32( b, 0, owner );
    image_put32( b, 4, 0x3F800000u );
    image_put32( b, 8, 0 );
    image_put32( b, 12, 0x3F800000u );
    image_zero( b, 4, 9 );
    image_put32( b, 40, 0x3F99999Au );
    image_put32( b, 44, 0x3F99999Au );
    image_zero( b, 12, 14 );
    o.at_30.reset( new std::uint8_t[ 0x20 ] );
    image_put32( b, 56, 8 );
    image_zero( b, 15, 17 );
    const std::uint16_t zero = 0;
    std::memcpy( b + 72, &zero, 2 );
    for ( int k = 0; k < 5; ++k )
        image_empty_string( b, 76 + 24 * k );
    for ( int k = 0; k < 5; ++k )
        parts_handle( b, 196 + 8 * k );
    image_zero( b, 59, 64 );
}

// 0x413280: 16 words - 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 and four zeros.
inline void parts_413280( std::uint8_t* image, int at )
{
    for ( int word = 0; word < 16; ++word )
        image_put32( image, at + 4 * word, ( word == 0 || word == 5 || word == 10 ) ? 0x3F800000u : 0u );
}

// 0x57CB10: 23 words; +0x0C, +0x1C, +0x2C, +0x3C left.
inline void parts_57cb10( std::uint8_t* image, int at )
{
    static const struct { int word; std::uint32_t value; } writes[] = {
        { 0, 0 }, { 1, 0 }, { 2, 0 }, { 4, 0 }, { 5, 0x3F800000u }, { 6, 0 }, { 8, 0 }, { 9, 0 },
        { 10, 0x3F800000u }, { 12, 0x3F800000u }, { 13, 0 }, { 14, 0 }, { 16, 0x3F800000u }, { 17, 0x40000000u },
        { 18, 0x3F800000u }, { 19, 0x3F800000u }, { 20, 0x40000000u }, { 21, 0xC0800000u }, { 22, 0xC0800000u } };
    for ( const auto& w : writes )
        image_put32( image, at + 4 * w.word, w.value );
}

// 0x50BA20 (a base ctor): zeroes, 0x57CB10 at +0x184, four identities
// (0x410A40) at +0x30 / +0x70 / +0xB0 / +0xF0, then at +0x130 the matrix
// rows (1,0,0,0) (0,0,-1,0) (0,1,0,0) (0,0,0,1) built on the stack.
inline void parts_50ba20( std::uint8_t* image, int at )
{
    for ( int word : { 0, 1, 2, 4, 5, 6, 8, 9, 10, 92, 94, 95, 96 } )
        image_put32( image, at + 4 * word, 0 );
    image_put32( image, at + 4 * 93, 0x3F800000u );
    parts_57cb10( image, at + 4 * 97 );
    for ( int matrix : { 12, 28, 44, 60 } )
    {
        float m[ 16 ];
        vu_identity( m );
        std::memcpy( image + at + 4 * matrix, m, 64 );
    }
    static const std::uint32_t basis[ 16 ] = {
        0x3F800000u, 0, 0, 0, 0, 0, 0xBF800000u, 0, 0, 0x3F800000u, 0, 0, 0, 0, 0, 0x3F800000u };
    for ( int word = 0; word < 16; ++word )
        image_put32( image, at + 4 * ( 76 + word ), basis[ word ] );
}

// 0x51E4A0, VuBreakableModelInstance (vtable 0x74CD9C): 0x74 bytes.
inline void parts_51e4a0( std::uint8_t* image, int at )
{
    static const struct { int word; std::uint32_t value; } writes[] = {
        { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 0 }, { 5, 0x3F800000u }, { 6, 0xFFFFFFFFu }, { 0, 0x0074CD9Cu },
        { 8, 0xC0A00000u }, { 9, 0xC0A00000u }, { 10, 0x40A00000u }, { 12, 0x40A00000u }, { 13, 0x40A00000u },
        { 14, 0x41200000u }, { 16, 0xC0490FDBu }, { 17, 0xC0490FDBu }, { 18, 0xC0490FDBu }, { 20, 0x40490FDBu },
        { 21, 0x40490FDBu }, { 22, 0x40490FDBu }, { 24, 0 }, { 25, 0 }, { 26, 0x411CE560u }, { 27, 0 },
        { 28, 0 } };
    for ( const auto& w : writes )
        image_put32( image, at + 4 * w.word, w.value );
    const std::uint16_t zero = 0;
    std::memcpy( image + at + 28, &zero, 2 );
}

// 0x409750 (thiscall, the owner at +0x240): 0x50BA20, then its own
// defaults from +0x240.
struct boat_409750_t
{
    std::uint8_t image[ 0x308 ]{};
};

inline void boat_409750( boat_409750_t& o, std::uint32_t owner )
{
    std::uint8_t* b = o.image;
    parts_50ba20( b, 0 );
    static const struct { int at; std::uint32_t value; } writes[] = {
        { 576, 0 }, { 580, 0 }, { 584, 0xBF800000u }, { 588, 0 }, { 596, 0x3FC00000u }, { 600, 0x40900000u },
        { 604, 0xC1200000u }, { 612, 0x40800000u }, { 616, 0x40400000u }, { 620, 0x41200000u }, { 624, 0 },
        { 628, 0xC0000000u }, { 632, 0x40000000u }, { 640, 0 }, { 644, 0x40400000u }, { 648, 0x3F000000u },
        { 656, 0 }, { 660, 0x40000000u }, { 664, 0x40000000u }, { 672, 0 }, { 676, 0 }, { 680, 0 }, { 684, 0 },
        { 692, 0 }, { 696, 0 }, { 700, 0 }, { 712, 0 }, { 716, 0 }, { 732, 0 }, { 728, 0 }, { 724, 0 },
        { 740, 0 }, { 744, 0 }, { 748, 0 }, { 752, 0x3F800000u }, { 756, 0 }, { 760, 0 }, { 764, 0x3F800000u },
        { 772, 0 } };
    for ( const auto& w : writes )
        image_put32( b, w.at, w.value );
    image_put32( b, 576, owner );
    b[ 708 ] = 1;
    b[ 720 ] = 0;
}

// 0x410D90 (thiscall, the owner at +0): a VuBreakableModelInstance at +4,
// an empty string at +0x78, 2.0 at +0x90, an _aligned_malloc(0x20, 16) array
// at +0x9C (capacity 8 at +0xA4), an empty box at +0xA8 / +0xB8 whose w
// lanes (+0xB4, +0xC4) are an unset stack dword - indeterminate - then 64
// bytes from .data 0x736244 over +0x24..+0x63 (the instance's own +0x20..)
// and +0x64 / +0x68 = 0, +0x6C = 0x419CE560.
struct boat_410d90_t
{
    std::uint8_t image[ 0xD0 ]{};
    std::unique_ptr< std::uint8_t[ ] > at_9c;
};

inline void boat_410d90( boat_410d90_t& o, std::uint32_t owner )
{
    std::uint8_t* b = o.image;
    image_put32( b, 0, owner );
    parts_51e4a0( b, 4 );
    image_put32( b, 140, 15 );
    image_put32( b, 136, 0 );
    b[ 120 ] = 0;
    image_put32( b, 144, 0x40000000u );
    b[ 148 ] = 0;
    image_zero( b, 38, 41 );
    o.at_9c.reset( new std::uint8_t[ 0x20 ] );
    image_put32( b, 164, 8 );
    image_put32( b, 168, 0x7F7FFFFFu );
    image_put32( b, 172, 0x7F7FFFFFu );
    image_put32( b, 176, 0x7F7FFFFFu );
    image_put32( b, 184, 0xFF7FFFFFu );
    image_put32( b, 188, 0xFF7FFFFFu );
    image_put32( b, 192, 0xFF7FFFFFu );
    image_put32( b, 200, 0 );
    b[ 204 ] = 0;
    static const std::uint32_t block[ 16 ] = {
        0xC0A00000u, 0xC0A00000u, 0x41200000u, 0, 0x40A00000u, 0x40A00000u, 0x41A00000u, 0,
        0xC0C90FDBu, 0xC0C90FDBu, 0x40C90FDBu, 0, 0x40C90FDBu, 0x40C90FDBu, 0x40C90FDBu, 0 };
    for ( int word = 0; word < 16; ++word )
        image_put32( b, 36 + 4 * word, block[ word ] );
    image_put32( b, 100, 0 );
    image_put32( b, 104, 0 );
    image_put32( b, 108, 0x419CE560u );
}

// 0x4131E0 (__userpurge: this in ECX, a float in XMM1 -> +0, then three
// stack arguments: +4, +0x50, and a pointer to 16 bytes copied to +0x60;
// retn 0Ch), then 0x413280 at +0x10.
struct boat_4131e0_t
{
    std::uint8_t image[ 0xA0 ]{};
};

inline void boat_4131e0( boat_4131e0_t& o, std::uint32_t xmm1, std::uint32_t a3, std::uint32_t a4, const std::uint8_t* a5 )
{
    std::uint8_t* b = o.image;
    image_put32( b, 4, a3 );
    image_put32( b, 80, a4 );
    image_put32( b, 0, xmm1 );
    std::memcpy( b + 96, a5, 16 );
    image_put32( b, 112, 0 );
    image_put32( b, 116, 0 );
    image_put32( b, 120, 0x3F000000u );
    image_put32( b, 124, 0 );
    image_put32( b, 128, 0x3F4CCCCDu );
    image_put32( b, 132, 0x3F800000u );
    b[ 136 ] = 0;
    image_put32( b, 140, 0x3BA3D70Au );
    image_put32( b, 144, 0x3C23D70Au );
    image_put32( b, 148, 0x3C23D70Au );
    image_put32( b, 152, 0x3C23D70Au );
    parts_413280( b, 16 );
}

// The engine's static identity 0x7593B0, as its CRT initializer 0x64B520
// writes it.
inline void parts_static_identity( std::uint8_t* image, int at )
{
    for ( int word = 0; word < 16; ++word )
        image_put32( image, at + 4 * word, ( word % 5 == 0 ) ? 0x3F800000u : 0u );
}

// 0x41B1B0: 12 words (+0x0C = FLT_MAX), the static identity at +0x30, +0x70 = 0.
struct boat_41b1b0_t
{
    std::uint8_t image[ 0x74 ]{};
};

inline void boat_41b1b0( boat_41b1b0_t& o )
{
    image_zero( o.image, 0, 11 );
    image_put32( o.image, 12, 0x7F7FFFFFu );
    parts_static_identity( o.image, 48 );
    image_put32( o.image, 112, 0 );
}

// 0x50B8D0, Vu3dLayoutComponent (vtable 0x74C550; owner at +8): two vectors
// (-1, -1, -1) at +0x18 and (1, 1, 1) at +0x28, each stored as two qwords
// whose last dword (+0x24, +0x34) is an unset stack local - indeterminate.
struct vu_3d_layout_t
{
    std::uint8_t image[ 0x38 ]{};
};

inline void vu_3d_layout( vu_3d_layout_t& o, std::uint32_t owner )
{
    std::uint8_t* b = o.image;
    image_put32( b, 4, 0 );
    image_put32( b, 8, owner );
    image_put32( b, 12, 0 );
    image_put32( b, 0, 0x0074C550u );
    image_zero( b, 4, 5 );
    image_put32( b, 24, 0xBF800000u );
    image_put32( b, 28, 0xBF800000u );
    image_put32( b, 32, 0xBF800000u );
    image_put32( b, 40, 0x3F800000u );
    image_put32( b, 44, 0x3F800000u );
    image_put32( b, 48, 0x3F800000u );
}

// A VuBasicProperty<bool, 2> node (vtable 0x737450, 0x1C bytes): its name
// (an .rdata address), a u16 1 at +8, +0x0C = 0, the next node at +0x10, the
// default and current value at +0x14 / +0x15, and the bound member at +0x18.
struct vu_bool_property_t
{
    std::uint8_t image[ 0x1C ]{};
    int member = -1;
};

// 0x531A40, Vu3dDrawComponent (vtable 0x74D244; owner at +8, `reflect` at
// +0x1C, 1 at +0x1D): its property list at +4 - "Reflect" (0x6CDEA0) bound
// to +0x1C, then "Shadow" (0x6CDEBC) bound to +0x1D - and two qword pairs
// whose last dwords (+0x30, +0x40) are unset stack locals - indeterminate.
struct vu_3d_draw_t
{
    std::uint8_t image[ 0x44 ]{};
    vu_bool_property_t properties[ 2 ];
};

inline void vu_3d_draw( vu_3d_draw_t& o, std::uint32_t owner, std::uint8_t reflect )
{
    std::uint8_t* b = o.image;
    image_put32( b, 4, 0 );
    image_put32( b, 8, owner );
    image_put32( b, 12, 0 );
    image_put32( b, 0, 0x0074D244u );
    image_zero( b, 4, 6 );
    b[ 28 ] = reflect;
    b[ 29 ] = 1;
    b[ 30 ] = 0;
    image_put32( b, 32, 0 );
    image_zero( b, 9, 11 );
    image_zero( b, 13, 15 );
    const std::uint32_t names[ 2 ] = { 0x006CDEA0u, 0x006CDEBCu };
    for ( int k = 0; k < 2; ++k )
    {
        std::uint8_t* p = o.properties[ k ].image;
        image_put32( p, 0, 0x00737450u );
        image_put32( p, 4, names[ k ] );
        const std::uint16_t one = 1;
        std::memcpy( p + 8, &one, 2 );
        image_put32( p, 12, 0 );
        image_put32( p, 16, 0 );
        p[ 20 ] = b[ 28 + k ];
        p[ 21 ] = b[ 28 + k ];
        o.properties[ k ].member = 28 + k;
    }
}

// 0x532080, VuAnimatedModelInstance (vtable 0x74D2F8): the static block
// 0x7593F0 at +0x2C (its initializer 0x64B690 fills it from stack locals,
// two of them unset: +0x38 and +0x48 here are indeterminate), the static
// identity at +0x50, +0x90 = -1, a zero u16 at +0x94.
struct vu_animated_model_t
{
    std::uint8_t image[ 0x98 ]{};
};

inline void vu_animated_model( vu_animated_model_t& o )
{
    std::uint8_t* b = o.image;
    image_put32( b, 4, 0 );
    image_put32( b, 8, 0 );
    image_put32( b, 0, 0x0074D2F8u );
    image_zero( b, 3, 6 );
    image_put32( b, 36, 0 );
    image_put32( b, 40, 1 );
    for ( int word = 0; word < 8; ++word )
        image_put32( b, 44 + 4 * word, 0 );
    image_put32( b, 76, 0 );
    parts_static_identity( b, 80 );
    image_put32( b, 144, 0xFFFFFFFFu );
    const std::uint16_t zero = 0;
    std::memcpy( b + 148, &zero, 2 );
    image_zero( b, 7, 8 );
}

// An MSVC x86 std::string after assign (0x401DB0) of `text`: up to 15
// characters inline (the text and its NUL, size at +16, capacity 15 at +20);
// longer ones move to the heap - not part of the image (`heap` = true), kept
// by the caller as a std::string.
inline bool parts_string( std::uint8_t* image, int at, const std::string& text )
{
    if ( text.size( ) > 15 )
        return true;
    std::memcpy( image + at, text.data( ), text.size( ) );
    image[ at + text.size( ) ] = 0;
    image_put32( image, at + 16, static_cast< std::uint32_t >( text.size( ) ) );
    image_put32( image, at + 20, 15 );
    return false;
}

// 0x406B70, VuBasicProperty<std::string, 3> (vtable 0x7374B8; 0x48 bytes):
// the name at +4, a u16 1 at +8, +0x0C / +0x10 = 0, then the target's value
// copied into the default (+0x14) and the current value (+0x2C), and the
// target itself at +0x44.
struct vu_string_property_t
{
    std::uint8_t image[ 0x48 ]{};
    std::string fallback;
    std::string current;
    bool heap = false;
};

inline void vu_string_property( vu_string_property_t& o, std::uint32_t name, std::uint32_t target,
    const std::string& value )
{
    std::uint8_t* b = o.image;
    image_put32( b, 4, name );
    const std::uint16_t one = 1;
    std::memcpy( b + 8, &one, 2 );
    image_put32( b, 12, 0 );
    image_put32( b, 16, 0 );
    image_put32( b, 0, 0x007374B8u );
    image_empty_string( b, 20 );
    o.heap = parts_string( b, 20, value );
    o.fallback = value;
    image_empty_string( b, 44 );
    parts_string( b, 44, value );
    o.current = value;
    image_put32( b, 68, target );
}

// A ConstantDB float read the way 0x415D60 inlines it: tag 1 (int -> float),
// 2 (the bits), 7 (int64 -> float); any other tag leaves the default.
inline void parts_db_float( std::uint8_t* image, int at, const value_t& value )
{
    float lane;
    if ( value.tag == 1 )
        lane = static_cast< float >( value.integer );
    else if ( value.tag == 2 )
        lane = value.real;
    else if ( value.tag == 7 )
        lane = static_cast< float >( value.int64 );
    else
        return;
    std::memcpy( image + at, &lane, 4 );
}

// 0x415D60 (thiscall, the owner at +0): the big splash. Its defaults, two
// eh-vector-constructed arrays (+0xD4: 4 empty strings; +0x134: 4 x 8 bytes
// from 0x40D810 = a u16 1 and a zero dword), a 0x5240C0 object at +0x1CC
// with its +0x100 then zeroed, and the ConstantDB reads - every one through
// [[g_vugameutil_0 + 4] + 0x58]["VuDBAsset"]["Boat"] - with "BigSplashPfx"
// (a string only when tagged 4) into the std::string at +0x17C, kept here
// as `pfx`.
struct boat_splash_t
{
    std::uint8_t image[ 0x1F0 ]{};
    std::string pfx;
    std::unique_ptr< boat_5240c0_t > at_1cc;
};

inline void boat_splash( boat_splash_t& o, std::uint32_t owner, const value_t& constant_db )
{
    std::uint8_t* b = o.image;
    image_put32( b, 0, owner );
    image_put32( b, 24, 15 );
    image_put32( b, 20, 0 );
    b[ 4 ] = 0;
    image_zero( b, 7, 9 );
    image_zero( b, 11, 13 );
    image_zero( b, 15, 17 );
    image_put32( b, 76, 0x447A0000u );
    image_put32( b, 80, 0x447A0000u );
    image_put32( b, 84, 0x3F9D70A4u );
    image_put32( b, 88, 0x3F800000u );
    image_put32( b, 92, 0 );
    for ( int word = 24; word <= 27; ++word )
        image_put32( b, 4 * word, 0x3F800000u );
    image_zero( b, 28, 37 );
    image_put32( b, 152, 0x3F800000u );
    image_zero( b, 39, 41 );
    image_zero( b, 43, 45 );
    image_zero( b, 47, 49 );
    image_zero( b, 51, 52 );
    for ( int k = 0; k < 4; ++k )
        image_empty_string( b, 212 + 24 * k );
    for ( int k = 0; k < 4; ++k )
    {
        const std::uint16_t one = 1;
        std::memcpy( b + 308 + 8 * k, &one, 2 );
        image_put32( b, 308 + 8 * k + 4, 0 );
    }
    image_zero( b, 85, 87 );
    image_put32( b, 352, 0x43B40000u );
    image_put32( b, 356, 0 );
    image_put32( b, 360, 0x7F7FFFFFu );
    image_zero( b, 91, 92 );
    b[ 372 ] = 0;
    image_put32( b, 376, 0 );
    o.pfx.clear( );
    image_zero( b, 101, 108 );
    image_zero( b, 110, 115 );
    image_put32( b, 464, 0x3E4CCCCDu );
    image_put32( b, 468, 0x3F4CCCCDu );
    image_put32( b, 472, 0x3F800000u );
    image_zero( b, 119, 122 );
    b[ 492 ] = 0;
    o.at_1cc = boat_new_5240c0( );
    image_put32( o.at_1cc->image, 256, 0 );

    const value_t& boat = member( member( constant_db, "VuDBAsset" ), "Boat" );
    const value_t& pfx = member( boat, "BigSplashPfx" );
    if ( pfx.tag == 4 )
        o.pfx = pfx.text;
    parts_db_float( b, 404, member( boat, "BigSplashLensSplashAmount" ) );
    parts_db_float( b, 408, member( boat, "BigSplashCameraShakeMagnitude" ) );
    parts_db_float( b, 412, member( boat, "BigSplashCameraShakeDuration" ) );
    parts_db_float( b, 416, member( boat, "BigSplashCameraShakeFalloffTime" ) );
    parts_db_float( b, 420, member( boat, "BigSplashCameraShakeFrequency" ) );
    parts_db_float( b, 444, member( boat, "BigSplashBoatOffsetInitialVelMin" ) );
    parts_db_float( b, 448, member( boat, "BigSplashBoatOffsetInitialVelMax" ) );
    parts_db_float( b, 452, member( boat, "BigSplashBoatOffsetSpringCoeff" ) );
    parts_db_float( b, 456, member( boat, "BigSplashBoatOffsetDampingCoeff" ) );
}
