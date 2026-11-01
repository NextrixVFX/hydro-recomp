#pragma once

#include <workspace/core/boat/hull.hxx>

#include <cstdint>
#include <cstring>

// VuBoatDamageController::VuPiece (the loader 0x412270 installs both
// vtables): a btMotionState at +0 (vtable 0x738088: 0 = the scalar-deleting
// dtor 0x412A70 -> 0x412B00, 1 = getWorldTransform 0x412CA0, 2 =
// setWorldTransform 0x412D90) and the fluid step interface at +4 (vtable
// 0x73807C: 0x410B30, 0x412E30, 0x6F9724). +8 is the piece's world matrix (Vu
// rows), +0x1B0 the offset of its body's origin in it. On the piece's image.

// getWorldTransform 0x412CA0 (thiscall(piece, btTransform* out), retn 4):
// translation(+0x1B0) (0x410AB0) . +8 (VuMatrix_multiply 0x4078C0), as a
// btTransform (0x410C30: the basis transposed into rows with w 0, the origin
// row 3 with w 0).
inline void piece_motion_get( const std::uint8_t* piece, float* out )
{
    float world[ 16 ], offset[ 16 ], product[ 16 ];
    std::memcpy( world, piece + 8, 64 );
    float v[ 3 ];
    std::memcpy( v, piece + 0x1B0, 12 );
    hull_translation( offset, v );
    hull_multiply( offset, product, world );
    const int column[ 3 ][ 3 ] = { { 0, 4, 8 }, { 1, 5, 9 }, { 2, 6, 10 } };
    for ( int row = 0; row < 3; ++row )
    {
        for ( int k = 0; k < 3; ++k )
            out[ 4 * row + k ] = product[ column[ row ][ k ] ];
        out[ 4 * row + 3 ] = 0.0f;
    }
    out[ 12 ] = product[ 12 ];
    out[ 13 ] = product[ 13 ];
    out[ 14 ] = product[ 14 ];
    out[ 15 ] = 0.0f;
}

// setWorldTransform 0x412D90 (thiscall(piece, btTransform const*), retn 4):
// +8 = 0x410B40(in), then +8 = translation(-(+0x1B0)) . +8.
inline void piece_motion_set( std::uint8_t* piece, const float* bt )
{
    float m[ 16 ], t[ 16 ], product[ 16 ];
    hull_vu_from_bt( m, bt );
    std::memcpy( piece + 8, m, 64 );
    float v[ 3 ];
    std::memcpy( v, piece + 0x1B0, 12 );
    v[ 0 ] = -v[ 0 ];
    v[ 1 ] = -v[ 1 ];
    v[ 2 ] = -v[ 2 ];
    hull_translation( t, v );
    hull_multiply( t, product, m );
    std::memcpy( piece + 8, product, 64 );
}
