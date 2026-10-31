#pragma once

class c_light
{
public:
    void direction( float* out ) const;
    void aim( const float* direction );
    // The authored three-colour rig: VuAmbientLightEntity plus the Front and
    // Back colours of VuDirectionalLightEntity. Passing nullptr for any of
    // them leaves that colour alone.
    void rig( const float* ambient, const float* front, const float* back );
    const float* ambient( ) const { return m_ambient; }
    const float* front( ) const { return m_front; }
    const float* back( ) const { return m_back; }

private:
    float m_direction[ 3 ]{ 0.25f, 0.55f, 0.80f };
    // These defaults are not authored values - they are the constants of the
    // old hardcoded one-sided lambert, shade = 0.45 + 0.55 * ndotl, written
    // in the rig's form so that a scene which never calls rig() renders
    // exactly as it did before. A level that loads its light entities
    // overwrites all three.
    float m_ambient[ 3 ]{ 0.45f, 0.45f, 0.45f };
    float m_front[ 3 ]{ 0.55f, 0.55f, 0.55f };
    float m_back[ 3 ]{ 0.0f, 0.0f, 0.0f };
};

c_light& g_light( );

#include <workspace/utility/math.hxx>

namespace
{
    c_light g_light_instance{};
}

c_light& g_light( )
{
    return g_light_instance;
}

void c_light::direction( float* out ) const
{
    out[ 0 ] = m_direction[ 0 ];
    out[ 1 ] = m_direction[ 1 ];
    out[ 2 ] = m_direction[ 2 ];
    math_normalize( out );
}

void c_light::aim( const float* direction )
{
    if ( direction == nullptr )
        return;
    m_direction[ 0 ] = direction[ 0 ];
    m_direction[ 1 ] = direction[ 1 ];
    m_direction[ 2 ] = direction[ 2 ];
}

void c_light::rig( const float* ambient, const float* front, const float* back )
{
    for ( int index = 0; index < 3; ++index )
    {
        if ( ambient != nullptr )
            m_ambient[ index ] = ambient[ index ];
        if ( front != nullptr )
            m_front[ index ] = front[ index ];
        if ( back != nullptr )
            m_back[ index ] = back[ index ];
    }
}
