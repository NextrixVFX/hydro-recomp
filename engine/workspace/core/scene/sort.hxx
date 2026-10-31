#pragma once

class c_sort
{
public:
    void add( void ( *function )( void* ), void* user );
    void flush( );

private:
    struct command_t
    {
        void ( *function )( void* );
        void* user;
    };

    static constexpr int capacity = 64;
    command_t m_commands[ capacity ]{};
    int m_count{};
};

c_sort& g_sort( );

#include <workspace/utility/logger.hxx>

namespace
{
    c_sort g_sort_instance{};
}

c_sort& g_sort( )
{
    return g_sort_instance;
}

void c_sort::add( void ( *function )( void* ), void* user )
{
    if ( m_count >= capacity )
    {
        LOG_ERROR( "sort queue is full" );
        return;
    }

    m_commands[ m_count ].function = function;
    m_commands[ m_count ].user = user;
    ++m_count;
}

void c_sort::flush( )
{
    int count = m_count;
    m_count = 0;
    for ( int index = 0; index < count; ++index )
        m_commands[ index ].function( m_commands[ index ].user );
}
