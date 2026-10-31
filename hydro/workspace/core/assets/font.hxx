#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// VuFontAsset written by the game's font baker. Three metrics, a glyph
// count, then 44-byte glyphs (code, source rect, destination rect,
// advance, image index). The atlas is the 8-bit distance field that
// follows the width and height.
class c_font
{
public:
    bool load( const std::uint8_t* data, std::size_t size )
    {
        m_glyphs.clear( );
        m_atlas.clear( );
        m_width = 0;
        m_height = 0;
        if ( data == nullptr || size < 16 )
            return false;

        m_ascender = read_f32( data );
        m_descender = read_f32( data + 4 );
        const std::uint32_t count = read_u32( data + 12 );
        const std::size_t table_end = 16 + static_cast< std::size_t >( count ) * 44;
        if ( count == 0 || count > 4096 || table_end + 8 > size )
            return false;

        m_glyphs.reserve( count );
        for ( std::uint32_t index = 0; index < count; ++index )
        {
            const std::uint8_t* record = data + 16 + static_cast< std::size_t >( index ) * 44;
            glyph_t glyph{};
            glyph.code = read_u32( record );
            glyph.src_l = read_f32( record + 4 );
            glyph.src_r = read_f32( record + 8 );
            glyph.src_t = read_f32( record + 12 );
            glyph.src_b = read_f32( record + 16 );
            glyph.dst_l = read_f32( record + 20 );
            glyph.dst_r = read_f32( record + 24 );
            glyph.dst_t = read_f32( record + 28 );
            glyph.dst_b = read_f32( record + 32 );
            glyph.advance = read_f32( record + 36 );
            m_glyphs.push_back( glyph );
        }

        m_width = static_cast< int >( read_u32( data + table_end ) );
        m_height = static_cast< int >( read_u32( data + table_end + 4 ) );
        const std::size_t pixels = static_cast< std::size_t >( m_width ) * static_cast< std::size_t >( m_height );
        if ( m_width <= 0 || m_height <= 0 || table_end + 8 + pixels > size )
            return false;

        m_atlas.assign( data + table_end + 8, data + table_end + 8 + pixels );
        return true;
    }

    float ascender( ) const
    {
        return m_ascender;
    }

    float descender( ) const
    {
        return m_descender;
    }

    float measure( const std::string& text, float size ) const
    {
        float pen = 0.0f;
        for ( unsigned char character : text )
        {
            const glyph_t* glyph = find( character );
            pen += glyph != nullptr ? glyph->advance * size : size * 0.33f;
        }
        return pen;
    }

    void draw( std::uint8_t* pixels, int canvas_w, int canvas_h, const std::string& text, float pen, float baseline, float size, int red = 255, int green = 255, int blue = 255 ) const
    {
        if ( pixels == nullptr || m_atlas.empty( ) || size <= 0.0f )
            return;

        for ( unsigned char character : text )
        {
            const glyph_t* glyph = find( character );
            if ( glyph == nullptr )
            {
                pen += size * 0.33f;
                continue;
            }

            int x0 = static_cast< int >( pen + glyph->dst_l * size );
            int x1 = static_cast< int >( pen + glyph->dst_r * size );
            int y0 = static_cast< int >( baseline + glyph->dst_t * size );
            int y1 = static_cast< int >( baseline + glyph->dst_b * size );
            if ( x1 < x0 )
            {
                const int swap = x0;
                x0 = x1;
                x1 = swap;
            }
            if ( y1 < y0 )
            {
                const int swap = y0;
                y0 = y1;
                y1 = swap;
            }

            const int span_x = x1 - x0;
            const int span_y = y1 - y0;
            for ( int y = y0; y < y1; ++y )
            {
                if ( y < 0 || y >= canvas_h || span_y <= 0 )
                    continue;
                const float v = glyph->src_t + ( glyph->src_b - glyph->src_t ) * ( ( y - y0 ) + 0.5f ) / static_cast< float >( span_y );
                const int source_y = clamp_index( static_cast< int >( v * static_cast< float >( m_height ) ), m_height );
                for ( int x = x0; x < x1; ++x )
                {
                    if ( x < 0 || x >= canvas_w || span_x <= 0 )
                        continue;
                    const float u = glyph->src_l + ( glyph->src_r - glyph->src_l ) * ( ( x - x0 ) + 0.5f ) / static_cast< float >( span_x );
                    const int source_x = clamp_index( static_cast< int >( u * static_cast< float >( m_width ) ), m_width );
                    const int coverage = m_atlas[ static_cast< std::size_t >( source_y ) * m_width + source_x ];
                    const int alpha = coverage < 100 ? 0 : ( coverage - 100 ) * 3;
                    if ( alpha <= 0 )
                        continue;
                    std::uint8_t* dest = pixels + ( static_cast< std::size_t >( y ) * canvas_w + x ) * 4;
                    const int blend = alpha > 255 ? 255 : alpha;
                    dest[ 0 ] = static_cast< std::uint8_t >( ( red * blend + dest[ 0 ] * ( 255 - blend ) ) / 255 );
                    dest[ 1 ] = static_cast< std::uint8_t >( ( green * blend + dest[ 1 ] * ( 255 - blend ) ) / 255 );
                    dest[ 2 ] = static_cast< std::uint8_t >( ( blue * blend + dest[ 2 ] * ( 255 - blend ) ) / 255 );
                    dest[ 3 ] = 255;
                }
            }
            pen += glyph->advance * size;
        }
    }

private:
    struct glyph_t
    {
        std::uint32_t code = 0;
        float src_l = 0.0f;
        float src_r = 0.0f;
        float src_t = 0.0f;
        float src_b = 0.0f;
        float dst_l = 0.0f;
        float dst_r = 0.0f;
        float dst_t = 0.0f;
        float dst_b = 0.0f;
        float advance = 0.0f;
    };

    const glyph_t* find( unsigned char character ) const
    {
        for ( const glyph_t& glyph : m_glyphs )
        {
            if ( glyph.code == character )
                return &glyph;
        }
        return nullptr;
    }

    static int clamp_index( int value, int limit )
    {
        if ( value < 0 )
            return 0;
        if ( value >= limit )
            return limit - 1;
        return value;
    }

    static std::uint32_t read_u32( const std::uint8_t* data )
    {
        return static_cast< std::uint32_t >( data[ 0 ] )
            | ( static_cast< std::uint32_t >( data[ 1 ] ) << 8 )
            | ( static_cast< std::uint32_t >( data[ 2 ] ) << 16 )
            | ( static_cast< std::uint32_t >( data[ 3 ] ) << 24 );
    }

    static float read_f32( const std::uint8_t* data )
    {
        float value = 0.0f;
        std::memcpy( &value, data, sizeof( value ) );
        return value;
    }

    std::vector< glyph_t > m_glyphs;
    std::vector< std::uint8_t > m_atlas;
    int m_width = 0;
    int m_height = 0;
    float m_ascender = 0.8f;
    float m_descender = -0.2f;
};
