#pragma once

#include <workspace/utility/logger.hxx>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

extern "C"
{
#include <impl/inflate/puff.h>
}

#ifdef NIL
#  undef NIL
#endif

// PC Base.apf (FPUV, version 3). Header +8 is the directory offset and
// header +12 is the record count. Each record is a NUL-terminated name,
// then offset, uncompressed size, compressed size, type, crc32, and a
// compressed flag. A set flag means the payload is a zlib stream.
class c_archive
{
public:
    bool open( const char* path )
    {
        m_entries.clear( );
        m_path.clear( );
        if ( path == nullptr || path[ 0 ] == '\0' )
            return false;

        std::ifstream file( path, std::ios::binary );
        if ( !file )
        {
            LOG_ERROR( "pack did not open: %s", path );
            return false;
        }

        unsigned char header[ 32 ]{};
        file.read( reinterpret_cast< char* >( header ), sizeof( header ) );
        if ( !file )
            return false;

        const std::uint32_t magic = read_u32( header );
        const std::uint32_t directory = read_u32( header + 8 );
        const std::uint32_t listed = read_u32( header + 12 );
        const std::uint32_t directory_bytes = read_u32( header + 16 );
        if ( magic != 0x56555046u || directory_bytes == 0 || directory_bytes > 16u * 1024u * 1024u )
        {
            LOG_ERROR( "pack header is not a VuPF archive" );
            return false;
        }

        std::vector< unsigned char > table( directory_bytes );
        file.seekg( directory );
        file.read( reinterpret_cast< char* >( table.data( ) ), static_cast< std::streamsize >( table.size( ) ) );
        if ( !file )
        {
            LOG_ERROR( "pack directory did not read" );
            return false;
        }

        std::size_t cursor = 0;
        while ( cursor < table.size( ) )
        {
            std::size_t end = cursor;
            while ( end < table.size( ) && table[ end ] != 0 )
                ++end;
            if ( end == cursor || end >= table.size( ) || end + 1 + 24 > table.size( ) )
                break;

            entry_t entry{};
            entry.name.assign( reinterpret_cast< const char* >( table.data( ) + cursor ), end - cursor );
            cursor = end + 1;
            entry.offset = read_u32( table.data( ) + cursor );
            entry.raw = read_u32( table.data( ) + cursor + 4 );
            entry.compressed = read_u32( table.data( ) + cursor + 8 );
            entry.kind = read_u32( table.data( ) + cursor + 12 );
            entry.crc = read_u32( table.data( ) + cursor + 16 );
            entry.packed = read_u32( table.data( ) + cursor + 20 ) != 0;
            cursor += 24;
            m_entries.push_back( std::move( entry ) );
        }

        if ( m_entries.empty( ) || m_entries.front( ).name != "Assets/AssetData" || m_entries.front( ).offset != 0x44u )
        {
            LOG_ERROR( "pack directory did not start at Assets/AssetData" );
            m_entries.clear( );
            return false;
        }

        if ( m_entries.size( ) != listed )
            LOG_WARNING( "pack listed %u assets and the directory has %u", listed, static_cast< unsigned >( m_entries.size( ) ) );

        m_path = path;
        LOG_VERBOSE( "pack %u assets", static_cast< unsigned >( m_entries.size( ) ) );
        return true;
    }

    bool read( const char* name, std::vector< std::uint8_t >& out ) const
    {
        out.clear( );
        const entry_t* entry = find( name );
        if ( entry == nullptr )
        {
            LOG_ERROR( "asset missing: %s", name != nullptr ? name : "" );
            return false;
        }
        if ( entry->raw == 0 )
            return false;

        const std::uint32_t stored = entry->packed ? entry->compressed : entry->raw;
        if ( stored < 8 )
            return false;

        std::ifstream file( m_path, std::ios::binary );
        if ( !file )
            return false;

        std::vector< unsigned char > compressed( stored );
        file.seekg( entry->offset );
        file.read( reinterpret_cast< char* >( compressed.data( ) ), static_cast< std::streamsize >( compressed.size( ) ) );
        if ( !file )
            return false;

        if ( !entry->packed )
        {
            out = std::move( compressed );
            return true;
        }

        const unsigned header = static_cast< unsigned >( compressed[ 0 ] ) * 256u + compressed[ 1 ];
        if ( ( compressed[ 0 ] & 0x0f ) != 8 || ( header % 31u ) != 0 || ( compressed[ 1 ] & 0x20 ) != 0 )
        {
            LOG_ERROR( "asset %s is not a zlib stream", name );
            return false;
        }

        out.resize( entry->raw );
        unsigned long dest_len = entry->raw;
        unsigned long source_len = stored - 6;
        const int error = puff( out.data( ), &dest_len, compressed.data( ) + 2, &source_len );
        if ( error != 0 || dest_len != entry->raw )
        {
            LOG_ERROR( "asset %s inflate failed (%d)", name, error );
            out.clear( );
            return false;
        }

        LOG_VERBOSE( "asset %s  %u bytes", name, static_cast< unsigned >( dest_len ) );
        return true;
    }

    int count( ) const
    {
        return static_cast< int >( m_entries.size( ) );
    }

    // Every asset named "<prefix><name>", as <name>, in directory order.
    std::vector< std::string > names( const std::string& prefix ) const
    {
        std::vector< std::string > out;
        for ( const entry_t& entry : m_entries )
            if ( entry.name.compare( 0, prefix.size( ), prefix ) == 0 )
                out.push_back( entry.name.substr( prefix.size( ) ) );
        return out;
    }

private:
    struct entry_t
    {
        std::string name;
        std::uint32_t offset = 0;
        std::uint32_t raw = 0;
        std::uint32_t compressed = 0;
        std::uint32_t kind = 0;
        std::uint32_t crc = 0;
        bool packed = false;
    };

    static std::uint32_t read_u32( const unsigned char* bytes )
    {
        return static_cast< std::uint32_t >( bytes[ 0 ] )
            | ( static_cast< std::uint32_t >( bytes[ 1 ] ) << 8 )
            | ( static_cast< std::uint32_t >( bytes[ 2 ] ) << 16 )
            | ( static_cast< std::uint32_t >( bytes[ 3 ] ) << 24 );
    }

    const entry_t* find( const char* name ) const
    {
        if ( name == nullptr )
            return nullptr;
        for ( const entry_t& entry : m_entries )
        {
            if ( entry.name == name )
                return &entry;
        }
        return nullptr;
    }

    std::string m_path;
    std::vector< entry_t > m_entries;
};
