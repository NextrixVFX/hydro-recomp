#pragma once

#include <impl/engine.hxx>

class c_draw_manager
{
public:
    void add( c_draw& draw );
    void tick( );
    int count( ) const { return m_count; }

private:
    static constexpr int capacity = 32;
    c_draw* m_handlers[ capacity ]{};
    int m_count{};
};

c_draw_manager& g_draw( );

#include <workspace/utility/logger.hxx>

namespace
{
    c_draw_manager g_draw_manager{};
}

c_draw_manager& g_draw( )
{
    return g_draw_manager;
}

void c_draw_manager::add( c_draw& draw )
{
    if ( m_count >= capacity )
    {
        LOG_ERROR( "draw manager is full" );
        return;
    }

    m_handlers[ m_count++ ] = &draw;
}

void c_draw_manager::tick( )
{
    for ( int index = 0; index < m_count; ++index )
        m_handlers[ index ]->tick( );
}
