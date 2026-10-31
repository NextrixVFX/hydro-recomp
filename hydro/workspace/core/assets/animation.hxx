#pragma once

#include <workspace/utility/logger.hxx>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// VuAnimationAsset.
//
//   u32                    bones
//   u32                    frames
//   bones x frames x 48 B  TRS, frame-major
//   33 B                   trailer
//
// Verified across all 94 clips in the pack: len - 8 - bones*frames*48 is
// exactly 33 every time, a single-bucket histogram with no exceptions. Bone
// counts also match the hull bone counts measured independently from the
// model files, so two unrelated sources agree.
//
// The 48-byte record is the same one the skinned model's bind pose uses:
// position float4, quaternion float4 in (x, y, z, w), scale float4, with the
// fourth component of position and scale as padding.
//
// Ordering is frame-major. Measured rather than assumed: stepping the array
// by `bones` entries moves a position 200-800x less than stepping by one, so
// entries `bones` apart are the same bone on successive frames.
//
// Note that some clips carry no positional motion at all and animate purely
// through rotation - Boats/Razorback/Thrust is one - so an all-zero position
// delta is not a parse failure.
struct clip_pose_t
{
    float position[ 3 ]{};
    float rotation[ 4 ]{ 0.0f, 0.0f, 0.0f, 1.0f };
    float scale[ 3 ]{ 1.0f, 1.0f, 1.0f };
};

class c_clip
{
public:
    bool load( const std::vector< std::uint8_t >& blob )
    {
        m_bones = 0;
        m_frames = 0;
        m_poses.clear( );

        if ( blob.size( ) < 16 )
            return false;

        std::uint32_t bones = 0;
        std::uint32_t frames = 0;
        std::memcpy( &bones, blob.data( ), sizeof( bones ) );
        std::memcpy( &frames, blob.data( ) + 4, sizeof( frames ) );
        if ( bones == 0 || frames == 0 || bones > 512 || frames > 4096 )
            return false;

        const std::size_t body = static_cast< std::size_t >( bones ) * frames * 48;
        if ( blob.size( ) != 8 + body + 33 )
            return false;

        m_bones = static_cast< int >( bones );
        m_frames = static_cast< int >( frames );
        m_poses.resize( static_cast< std::size_t >( bones ) * frames );
        for ( std::size_t index = 0; index < m_poses.size( ); ++index )
        {
            const std::uint8_t* record = blob.data( ) + 8 + index * 48;
            clip_pose_t& pose = m_poses[ index ];
            std::memcpy( pose.position, record, sizeof( float ) * 3 );
            std::memcpy( pose.rotation, record + 16, sizeof( float ) * 4 );
            std::memcpy( pose.scale, record + 32, sizeof( float ) * 3 );
        }
        return true;
    }

    bool valid( ) const { return m_bones > 0 && m_frames > 0; }
    int bones( ) const { return m_bones; }
    int frames( ) const { return m_frames; }

    // Frame-major: entries `bones` apart are the same bone on the next frame.
    const clip_pose_t& at( int frame, int bone ) const
    {
        const int wrapped = m_frames > 0 ? ( ( frame % m_frames ) + m_frames ) % m_frames : 0;
        const std::size_t index = static_cast< std::size_t >( wrapped ) * m_bones + bone;
        return m_poses[ index ];
    }

    // TurnLR carries exactly 3 frames on every boat - left, centre, right -
    // and is a blend set driven by steering, not a timeline. A clip this
    // shape must be sampled by input rather than played back.
    bool blend_set( ) const { return m_frames == 3; }

private:
    int m_bones = 0;
    int m_frames = 0;
    std::vector< clip_pose_t > m_poses;
};
