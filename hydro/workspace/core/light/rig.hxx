#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/utility/logger.hxx>

#include <map>
#include <string>

// The authored scene light rig, from VuAmbientLightEntity and
// VuDirectionalLightEntity, plus the asset substitutions a level declares
// through VuSubstituteAssetEntity - which live here because they are the other
// thing the walk resolves once and the renderer then reads.
//
// Defaults are deliberately neutral so an unlit scene looks obviously unlit
// rather than plausibly wrong.
class c_light_rig
{
public:
    void clear( )
    {
        *this = c_light_rig{};
    }

    // Level colours are authored as 0..255 byte triples.
    static void read_color( const value_t* node, float* out )
    {
        if ( node == nullptr )
            return;
        out[ 0 ] = number( find( *node, "R" ), 0.0f ) / 255.0f;
        out[ 1 ] = number( find( *node, "G" ), 0.0f ) / 255.0f;
        out[ 2 ] = number( find( *node, "B" ), 0.0f ) / 255.0f;
    }

    float* ambient( ) { return m_ambient; }
    float* front( ) { return m_light_front; }
    float* back( ) { return m_light_back; }
    float* sun_world( ) { return m_light_world; }
    const float* ambient( ) const { return m_ambient; }
    const float* front( ) const { return m_light_front; }
    const float* back( ) const { return m_light_back; }

    void mark_lit( ) { m_lit = true; }
    void mark_sun( ) { m_sun = true; }
    bool lit( ) const { return m_lit; }

    void add_substitution( const std::string& from, const std::string& to )
    {
        m_substitute[ from ] = to;
    }

    // Resolve a placeholder asset through VuSubstituteAssetEntity.
    // "Proxy_cube" becomes "Hydro/Outside" on Hydroelectric; anything with no
    // substitution is returned unchanged.
    std::string substitute( const char* kind, const std::string& name ) const
    {
        const auto found = m_substitute.find( std::string( kind ) + "/" + name );
        return found != m_substitute.end( ) ? found->second : name;
    }

    // Report the rig once the walk is done, against the values reversed from
    // the level: ambient (79,79,85), front (255,215,157), back (93,91,115).
    // The light's axis is NOT reversed, so print all three and let the numbers
    // say which one points like a sun rather than assuming a convention.
    void report( ) const
    {
        if ( !m_lit && !m_sun )
        {
            LOG_WARNING( "no light entities found - the scene has no authored rig" );
            return;
        }
        LOG_SUCCESS( "light rig: ambient %.3f %.3f %.3f  front %.3f %.3f %.3f  back %.3f %.3f %.3f",
            m_ambient[ 0 ], m_ambient[ 1 ], m_ambient[ 2 ],
            m_light_front[ 0 ], m_light_front[ 1 ], m_light_front[ 2 ],
            m_light_back[ 0 ], m_light_back[ 1 ], m_light_back[ 2 ] );
        LOG_INFO( "sun axes: X %.3f %.3f %.3f | Y %.3f %.3f %.3f | Z %.3f %.3f %.3f",
            m_light_world[ 0 ], m_light_world[ 1 ], m_light_world[ 2 ],
            m_light_world[ 4 ], m_light_world[ 5 ], m_light_world[ 6 ],
            m_light_world[ 8 ], m_light_world[ 9 ], m_light_world[ 10 ] );
        for ( const auto& pair : m_substitute )
            LOG_INFO( "substitute %s -> %s", pair.first.c_str( ), pair.second.c_str( ) );
    }

private:
    float m_ambient[ 3 ]{ 0.0f, 0.0f, 0.0f };
    float m_light_front[ 3 ]{ 0.0f, 0.0f, 0.0f };
    float m_light_back[ 3 ]{ 0.0f, 0.0f, 0.0f };
    float m_light_world[ 16 ]{};
    bool m_lit = false;
    bool m_sun = false;
    // "VuCubeTextureAsset/Proxy_cube" -> "Hydro/Outside".
    std::map< std::string, std::string > m_substitute;
};
