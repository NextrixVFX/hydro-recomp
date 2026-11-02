#pragma once

#include <workspace/core/world/dbrt.hxx>
#include <workspace/core/world/gfxsort.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// VULIGHTMANAGER'S CPU SIDE AND THE DYNAMIC LIGHT (render layer, platform independent), recreated 1:1 from the disassembly (water batch 30).
//   VuDynamicLight (vtable 0x751ECC, ctor 0x5C2F30, update 0x5C30D0), at VuDynamicLightEntity +0x70: +4 position, +0x14 direction, +0x24 / +0x28
//   the colours, +0x2C factor, +0x30 / +0x34 the falloff range, +0x38 cone, +0x3C penumbra, +0x40 draw distance, +0x44 reflecting, +0x45 on, +0x48
//   the tree leaf, +0x4C the aabb, +0x6C the 0x50-byte record the shaders' light lists copy.
//   VuLightManager (0x104, g 0x759094, ctor 0x562890): +4 sixteen lists of records, +0xC4 the list set being filled, +0xC8 the one being drawn
//   (swapped by VuGfxSort's flip), +0xCC the ambient colour (a word, then +0xD0.. ), +0xE0 .., +0xFC the tree, +0x100 a dev byte.
// The dev draw 0x562FD0 is not recreated. The lights' properties load through engine entity/dynamic.hxx.

constexpr float k_gfx_byte = 1.0f / 255.0f;        // 0x6F75BC, 0x3B808081
constexpr float k_gfx_light_epsilon = 1.1920929e-07f;  // 0x6F7584, 0x34000000

struct gfx_light_record_t
{
    float m_words[ 20 ] = { };
};

struct gfx_light_t
{
    float m_position[ 3 ] = { };                       // +4
    float m_direction[ 3 ] = { 0.0f, 0.0f, -1.0f };    // +0x14
    std::uint32_t m_diffuse = 0xFFFFFFFFu;             // +0x24
    std::uint32_t m_specular = 0xFFFFFFFFu;            // +0x28
    float m_factor = 1.0f;                             // +0x2C
    float m_range_a = 30.0f, m_range_b = 50.0f;        // +0x30 / +0x34
    float m_cone = 1.5707964f;                         // +0x38, bits 0x3FC90FDB
    float m_penumbra = 0.0f;                           // +0x3C
    float m_draw_distance = 3.4028235e+38f;            // +0x40
    std::uint8_t m_reflecting = 0;                     // +0x44
    float m_aabb[ 8 ] = { };                           // +0x4C
    gfx_light_record_t m_record;                       // +0x6C
    dbrt_node_t* m_leaf = nullptr;                     // +0x48; non-null = on (+0x45)
};

// x87 fcos, here through double (flagged: the 80-bit result can round differently in the last float bit).
inline float gfx_fcos( float x )
{
    return static_cast< float >( std::cos( static_cast< double >( x ) ) );
}

struct gfx_light_manager_t
{
    // 0x5EDD90: the light into the tree.
    void light_on( gfx_light_t& l )
    {
        if ( !l.m_leaf )
            l.m_leaf = m_tree.add( l.m_aabb, &l );
    }

    // 0x5EDDF0.
    void light_off( gfx_light_t& l )
    {
        if ( !l.m_leaf )
            return;
        m_tree.erase( l.m_leaf );
        l.m_leaf = nullptr;
    }

    // 0x5C30D0: the aabb from the second range (the leaf moved when on), then the record.
    void update( gfx_light_t& l )
    {
        const float r = l.m_range_b;
        for ( int i = 0; i < 3; ++i )
        {
            l.m_aabb[ i ] = l.m_position[ i ] - r;
            l.m_aabb[ 4 + i ] = l.m_position[ i ] + r;
        }
        if ( l.m_leaf )
            m_tree.move( l.m_leaf, l.m_aabb );
        const float half = l.m_cone * 0.5f, outer = l.m_penumbra + half;
        float* w = l.m_record.m_words;
        std::memcpy( w, l.m_position, 12 );
        w[ 3 ] = 1.0f;
        std::memcpy( w + 4, l.m_direction, 12 );
        w[ 7 ] = 0.0f;
        for ( int i = 0; i < 4; ++i )
        {
            w[ 8 + i ] = static_cast< float >( ( l.m_diffuse >> ( 8 * i ) ) & 0xFFu ) * k_gfx_byte * l.m_factor;
            w[ 12 + i ] = static_cast< float >( ( l.m_specular >> ( 8 * i ) ) & 0xFFu ) * k_gfx_byte * l.m_factor;
        }
        w[ 16 ] = l.m_range_a < l.m_range_b ? l.m_range_a : l.m_range_b;
        w[ 17 ] = ( l.m_range_a > l.m_range_b ? l.m_range_a : l.m_range_b ) + k_gfx_light_epsilon;
        w[ 18 ] = gfx_fcos( half < outer ? half : outer ) + k_gfx_light_epsilon;
        w[ 19 ] = gfx_fcos( half > outer ? half : outer );
    }

    static int list_index( std::uint32_t set, std::uint32_t key_hi )
    {
        return static_cast< int >( ( set * 4 + ( ( key_hi >> 24 ) & 3u ) ) * 2 + ( ( key_hi >> 23 ) & 1u ) );
    }

    // 0x562B10 (VuGfxDrawParams: +0 the eye, +0x10 the camera with six planes at +0x1E0, +0x18 the reflecting flag, +0x1C a seventh plane when set):
    // the list of the sort's current key bits emptied, then the walk 0x5630F0 / 0x563490 copies each light's record that has the flag and
    // whose squared eye distance is below its squared draw distance. Same walk as dbrt_walk, its own copy in retail.
    void submit( const float eye[ 3 ], const float planes[ 28 ], std::uint8_t reflecting, std::uint32_t key_hi )
    {
        std::vector< gfx_light_record_t >& list = m_lists[ list_index( m_fill, key_hi ) ];
        list.clear( );
        dbrt_walk( m_tree.m_root, planes, reflecting ? 7 : 6, [ & ]( void* data )
        {
            const gfx_light_t& l = *static_cast< const gfx_light_t* >( data );
            if ( ( l.m_reflecting & reflecting ) != reflecting )
                return;
            const float dx = l.m_position[ 0 ] - eye[ 0 ], dy = l.m_position[ 1 ] - eye[ 1 ], dz = l.m_position[ 2 ] - eye[ 2 ];
            if ( l.m_draw_distance * l.m_draw_distance > dx * dx + dy * dy + dz * dz )
                list.push_back( l.m_record );
        } );
    }

    // 0x562C40 (matrix, aabb, out; the shader's light block: five 16-byte fields by four lights, the count at +0x140): the drawn command's list in
    // order, a light taken when the box-space distance below is under the record's word 17; at most four.
    int gather( const float* m, const float* aabb, std::uint32_t key_hi, float* out ) const
    {
        int count = 0;
        const std::vector< gfx_light_record_t >& list = m_lists[ list_index( m_render, key_hi ) ];
        const float cx = ( aabb[ 4 ] + aabb[ 0 ] ) * 0.5f, cy = ( aabb[ 5 ] + aabb[ 1 ] ) * 0.5f, cz = ( aabb[ 6 ] + aabb[ 2 ] ) * 0.5f;
        const float wx = m[ 0 ] * cx + m[ 4 ] * cy + m[ 8 ] * cz + m[ 12 ];
        const float wy = m[ 5 ] * cy + m[ 1 ] * cx + m[ 9 ] * cz + m[ 13 ];
        const float wz = m[ 6 ] * cy + m[ 2 ] * cx + m[ 10 ] * cz + m[ 14 ];
        const float ex = ( aabb[ 4 ] - aabb[ 0 ] ) * 0.5f, ey = ( aabb[ 5 ] - aabb[ 1 ] ) * 0.5f, ez = ( aabb[ 6 ] - aabb[ 2 ] ) * 0.5f;
        for ( const gfx_light_record_t& r : list )
        {
            const float* w = r.m_words;
            const float dx = w[ 0 ] - wx, dy = w[ 1 ] - wy, dz = w[ 2 ] - wz;
            const float a = std::fabs( m[ 1 ] * dy + m[ 0 ] * dx + m[ 2 ] * dz ) - ex;
            const float b = std::fabs( m[ 4 ] * dx + m[ 5 ] * dy + m[ 6 ] * dz ) - ey;
            const float c = std::fabs( m[ 9 ] * dy + m[ 8 ] * dx + dz * m[ 10 ] ) - ez;
            const float ab = a < b ? a : b;
            const float d = ab < c ? ab : c;
            if ( w[ 17 ] > d )
            {
                for ( int field = 0; field < 5; ++field )
                    std::memcpy( out + 16 * field + 4 * count, w + 4 * field, 16 );
                if ( ++count == 4 )
                    break;
            }
        }
        return count;
    }

    dbrt_tree_t m_tree;                                 // +0xFC
    std::vector< gfx_light_record_t > m_lists[ 16 ];    // +4
    std::uint32_t m_fill = 0, m_render = 1;             // +0xC4 / +0xC8
    std::uint32_t m_ambient = 0xFF333333u;              // +0xCC
    float m_ambient_b = 0.0f, m_ambient_c = 0.0f;       // +0xD0 / +0xD4
    float m_d8 = 500.0f;                                // +0xD8, bits 0x43FA0000
    float m_dir_light[ 3 ] = { 0.0f, 0.0f, -1.0f };     // +0xE0
    std::uint32_t m_front = 0xFFCCCCCCu, m_back = 0xFF404040u, m_specular = 0xFFFFFFFFu;   // +0xF0 / +0xF4 / +0xF8
};
