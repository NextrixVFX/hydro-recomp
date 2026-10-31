#pragma once

#include <workspace/core/assets/tree.hxx>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// VUJB, the binary document format every asset in the pack is written in.
// Big-endian, self-describing, one type tag per node. parse( ) is the
// document read @0x55B560 and node( ) the node read @0x55B770.
class c_reader
{
public:
    explicit c_reader( const std::vector< std::uint8_t >& blob )
    {
        const std::size_t start = find_magic( blob );
        m_bytes = blob.data( ) + start;
        m_size = blob.size( ) - start;
    }

    // @0x55B560: a u32 magic 'VUJB' and a u32 version 1, then one node. Any
    // read error clears the root (0x4FBB10), so a document either parses whole
    // or comes back null.
    value_t parse( )
    {
        std::uint32_t magic = 0;
        std::uint32_t version = 0;
        if ( !u32( &magic ) )
            return {};
        if ( !u32( &version ) )
            return {};
        if ( magic != 0x56554A42u || version != 1 )
            return {};
        value_t root{};
        if ( !node( root ) )
            return {};
        return root;
    }

private:
    // @0x55B770. Every failure is 0x55B6E0, which logs and returns false.
    bool node( value_t& value )
    {
        std::uint32_t type = 0;
        if ( !u32( &type ) )
            return false;
        value.tag = type;
        if ( type == 0 )
            return true;
        if ( type == 1 )
        {
            std::uint32_t bits = 0;
            if ( !u32( &bits ) )
                return false;
            value.kind = value_t::kind_t::integer;
            value.integer = static_cast< int >( bits );
            return true;
        }
        if ( type == 2 )
        {
            std::uint32_t bits = 0;
            if ( !u32( &bits ) )
                return false;
            value.kind = value_t::kind_t::real;
            std::memcpy( &value.real, &bits, sizeof( value.real ) );
            return true;
        }
        // Retail keeps the byte under its own bool tag; the readers that tell a
        // bool from an int are the property loads, not read yet, so it stays in
        // the integer kind every caller reads it through today.
        if ( type == 3 )
        {
            if ( m_cursor >= m_size )
                return false;
            value.kind = value_t::kind_t::integer;
            value.integer = m_bytes[ m_cursor ];
            ++m_cursor;
            return true;
        }
        if ( type == 4 )
        {
            if ( !string( ) )
                return false;
            value.kind = value_t::kind_t::text;
            value.text = m_scratch;
            return true;
        }
        if ( type == 5 )
        {
            std::uint32_t count = 0;
            if ( !u32( &count ) )
                return false;
            // A signed compare (jle): a negative count passes it and retail then
            // resizes the vector to it - not recreated, so it fails here.
            if ( static_cast< std::int32_t >( count ) > 0x10000 || static_cast< std::int32_t >( count ) < 0 )
                return false;
            value.kind = value_t::kind_t::list;
            value.list.resize( count );
            for ( std::uint32_t index = 0; index < count; ++index )
            {
                if ( !node( value.list[ index ] ) )
                    return false;
            }
            return true;
        }
        if ( type == 6 )
        {
            std::uint32_t count = 0;
            if ( !u32( &count ) )
                return false;
            value.kind = value_t::kind_t::object;
            for ( std::int32_t index = 0; index < static_cast< std::int32_t >( count ); ++index )
            {
                if ( !string( ) )
                    return false;
                // The key is built through strlen (0x401BD0), then std::map
                // operator[] (0x4FC970): a repeated key reads into the entry
                // already there, and the read allocates a fresh payload for
                // it, so nothing of the earlier value survives.
                const std::string key( m_scratch.c_str( ) );
                value_t* slot = nullptr;
                for ( auto& field : value.object )
                {
                    if ( field.first == key )
                        slot = &field.second;
                }
                if ( slot == nullptr )
                {
                    value.object.emplace_back( key, value_t{} );
                    slot = &value.object.back( ).second;
                }
                *slot = value_t{};
                if ( !node( *slot ) )
                    return false;
            }
            return true;
        }
        if ( type == 7 )
        {
            if ( m_cursor + 8 > m_size )
                return false;
            std::uint64_t bits = 0;
            for ( int index = 0; index < 8; ++index )
                bits = ( bits << 8 ) | m_bytes[ m_cursor + index ];
            m_cursor += 8;
            value.kind = value_t::kind_t::int64;
            value.int64 = static_cast< std::int64_t >( bits );
            return true;
        }
        return false;
    }

    // @0x55BA90: a u32 length, then that many bytes into the reader's own
    // buffer. A length past the end returns true without reading anything, so
    // the buffer still holds the previous string. Before any string has been
    // read retail assigns from an empty buffer with a length of -1 and throws
    // std::length_error (0x401CE9, measured in the oracle) - not recreated, so
    // that one case fails the document. The compare is signed
    // (jg); a negative length passes it into a resize retail cannot satisfy -
    // not recreated either.
    bool string( )
    {
        std::uint32_t length = 0;
        if ( !u32( &length ) )
            return false;
        if ( static_cast< std::int32_t >( length ) < 0 )
            return false;
        if ( static_cast< std::int32_t >( length ) <= static_cast< std::int64_t >( m_size - m_cursor ) )
        {
            m_scratch.assign( reinterpret_cast< const char* >( m_bytes + m_cursor ), length );
            m_cursor += length;
            m_buffered = true;
            return true;
        }
        return m_buffered;
    }

    // The magic is not always at offset zero - some assets carry a wrapper
    // ahead of it - so the document starts at the first VUJB in the blob.
    static std::size_t find_magic( const std::vector< std::uint8_t >& blob )
    {
        const char magic[ 4 ] = { 'V', 'U', 'J', 'B' };
        for ( std::size_t index = 0; index + 4 < blob.size( ); ++index )
        {
            if ( std::memcmp( blob.data( ) + index, magic, 4 ) == 0 )
                return index;
        }
        return 0;
    }

    // 0x55BCA0 / 0x55BCF0: fewer than four bytes left is a read error.
    bool u32( std::uint32_t* value )
    {
        if ( m_size - m_cursor < 4 )
            return false;
        *value = ( static_cast< std::uint32_t >( m_bytes[ m_cursor ] ) << 24 )
            | ( static_cast< std::uint32_t >( m_bytes[ m_cursor + 1 ] ) << 16 )
            | ( static_cast< std::uint32_t >( m_bytes[ m_cursor + 2 ] ) << 8 )
            | static_cast< std::uint32_t >( m_bytes[ m_cursor + 3 ] );
        m_cursor += 4;
        return true;
    }

    const std::uint8_t* m_bytes = nullptr;
    std::size_t m_size = 0;
    std::size_t m_cursor = 0;
    std::string m_scratch;
    bool m_buffered = false;
};
