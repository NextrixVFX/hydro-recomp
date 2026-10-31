#pragma once

#include <workspace/core/dynamics/body.hxx>
#include <workspace/utility/golden.hxx>
#include <workspace/utility/logger.hxx>

#include <cstring>

// The boat's checks against retail (memory/audit.md "Batch 9"..): first the
// VuRigidBody ctor.

struct body_ctor_hook_t
{
    const unsigned* transform;
};

inline bool body_ctor_motion( void* user, uint32_t, float* out )
{
    std::memcpy( out, static_cast< body_ctor_hook_t* >( user )->transform, 64 );
    return true;
}

// The VuRigidBody ctor 0x527B80 against retail: every byte of the 0x2D4-byte
// object, except the bytes retail never writes (g_golden_body_unwritten) and
// the pointer words (+0 the vtable, +204 / +212 the shape, +500 the motion
// state, +704 the owner), and the unique-id counter after.
inline bool verify_body_ctor( )
{
    int failed = 0, statics = 0, with_motion = 0;
    for ( std::size_t index = 0; index < g_golden_body_ctor_count; ++index )
    {
        const golden_body_ctor_t& g = g_golden_body_ctor[ index ];
        unsigned char cinfo[ 160 ];
        std::memcpy( cinfo, g.m_cinfo, 160 );
        static vu_rigid_body_t b;
        uint32_t uid = g.m_uid;
        body_ctor_hook_t hook{ g.m_chosen };
        const char* what = nullptr;
        int bad = -1;
        if ( !vu_rigid_body_construct( b, cinfo, static_cast< short >( g.m_group ), static_cast< short >( g.m_mask ), uid, body_ctor_motion,
                 &hook ) )
            what = "a path not recreated was reached";
        else if ( uid != g.m_uid_after )
            what = "the unique-id counter";
        const unsigned char* ours = reinterpret_cast< const unsigned char* >( &b );
        const unsigned char* retail = reinterpret_cast< const unsigned char* >( g.m_words );
        for ( int k = 0; k < 0x2D4 && !what; ++k )
        {
            const int w = k & ~3;
            if ( g_golden_body_unwritten[ k ] || w == 0 || w == 0xCC || w == 0xD4 || w == 0x1F4 || w == 0x2C0 )
                continue;
            if ( ours[ k ] != retail[ k ] )
            {
                what = "a byte of the object";
                bad = k;
            }
        }
        statics += ( b.body.collision_flags & 1 ) ? 1 : 0;
        with_motion += g.m_ms;
        if ( what )
        {
            ++failed;
            unsigned o = 0, r = 0;
            if ( bad >= 0 )
            {
                std::memcpy( &o, ours + ( bad & ~3 ), 4 );
                std::memcpy( &r, retail + ( bad & ~3 ), 4 );
            }
            LOG_ERROR( "body ctor vector %d: %s (byte +0x%X: ours 0x%08X, retail 0x%08X)", static_cast< int >( index ), what, bad, o, r );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the VuRigidBody ctor (btCollisionObject ctor, setupRigidBody, setMassProps, updateInertiaTensor) matches retail "
                     "over %d bodies, every written byte exact (%d static, %d through a motion state)",
            static_cast< int >( g_golden_body_ctor_count ), statics, with_motion );
    return failed == 0;
}
