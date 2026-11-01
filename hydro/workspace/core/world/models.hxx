#pragma once

// c_world's models - member definitions moved out of world.hxx (one class, split by concern).

inline std::string c_world::pack_path( ) const
{
#if defined( _WIN32 )
    char module_path[ MAX_PATH ]{};
    GetModuleFileNameA( nullptr, module_path, MAX_PATH );
    std::string directory = module_path;
    const auto slash = directory.find_last_of( "\\/" );
    if ( slash != std::string::npos )
        directory.resize( slash );
    for ( int step = 0; step < 8; ++step )
    {
        const std::string hydro = directory + "\\HydroThunderUWP\\Base.apf";
        const std::string local = directory + "\\Base.apf";
        if ( GetFileAttributesA( hydro.c_str( ) ) != INVALID_FILE_ATTRIBUTES )
            return hydro;
        if ( GetFileAttributesA( local.c_str( ) ) != INVALID_FILE_ATTRIBUTES )
            return local;
        const auto parent = directory.find_last_of( "\\/" );
        if ( parent == std::string::npos )
            break;
        directory.resize( parent );
    }
#endif
    return {};
}

inline int c_world::texture_for( const std::string& path )
{
    if ( path.empty( ) )
        return -1;
    const auto existing = m_textures.find( path );
    if ( existing != m_textures.end( ) )
        return existing->second;

    std::vector< std::uint8_t > bytes;
    image_t image{};
    const std::string name = "VuTextureAsset/" + path;
    if ( !m_archive.read( name.c_str( ), bytes ) || !decode_texture( bytes, image ) )
    {
        LOG_ERROR( "texture %s", name.c_str( ) );
        m_textures.emplace( path, -1 );
        return -1;
    }
    const int id = c_engine::texture( image.rgba.data( ), image.width, image.height );
    m_textures.emplace( path, id );
    return id;
}

// Resolve a material's declared cube to an engine handle. The name in the
// table is usually a placeholder - the hull asks for Proxy_cube - and the
// level's VuSubstituteAssetEntity is what points it at the real sky cube,
// Hydro/Outside on Hydroelectric. That is why 64 materials can all declare
// a cube and still share one reflection: it is a scene-wide asset.
inline int c_world::cube_for( const std::string& path )
{
    if ( path.empty( ) )
        return -1;
    // Keyed on the RESOLVED name, not the authored one. The cache outlives
    // a scene the way m_textures does, but m_substitute does not - so
    // caching "Proxy_cube" would hand the next level the previous level's
    // sky.
    const std::string resolved = m_rig.substitute( "VuCubeTextureAsset", path );
    const auto existing = m_cubes.find( resolved );
    if ( existing != m_cubes.end( ) )
        return existing->second;

    std::vector< std::uint8_t > bytes;
    cube_t decoded{};
    const std::string name = "VuCubeTextureAsset/" + resolved;
    if ( !m_archive.read( name.c_str( ), bytes ) || !decode_cube( bytes, decoded ) )
    {
        LOG_ERROR( "cube %s", name.c_str( ) );
        m_cubes.emplace( resolved, -1 );
        return -1;
    }
    const unsigned char* faces[ 6 ]{};
    for ( int index = 0; index < 6; ++index )
        faces[ index ] = decoded.rgba[ index ].data( );
    const int id = c_engine::cube( faces, decoded.face );
    LOG_SUCCESS( "cube %s -> %s  %d per face  id %d", path.c_str( ), resolved.c_str( ),
        decoded.face, id );
    m_cubes.emplace( resolved, id );
    return id;
}

inline const std::vector< c_world::pfx_part_t >& c_world::pfx_parts( const std::string& asset )
{
    const auto existing = m_pfx_models.find( asset );
    if ( existing != m_pfx_models.end( ) )
        return existing->second;
    std::vector< pfx_part_t > parts;
    for ( const cached_part_t& part : model_for( asset ) )
    {
        pfx_part_t piece{};
        piece.geometry = part.geometry;
        piece.texture = part.texture;
        piece.detail = part.detail;
        piece.index_start = part.index_start;
        piece.index_count = part.index_count;
        std::memcpy( piece.node, part.node, sizeof( piece.node ) );
        parts.push_back( piece );
    }
    LOG_INFO( "pfx geometry %s: %u parts", asset.c_str( ),
        static_cast< unsigned >( parts.size( ) ) );
    return m_pfx_models.emplace( asset, std::move( parts ) ).first->second;
}

inline const std::vector< c_world::cached_part_t >& c_world::model_for( const std::string& asset )
{
    const auto existing = m_models.find( asset );
    if ( existing != m_models.end( ) )
        return existing->second;

    std::vector< cached_part_t > cached;
    std::vector< std::uint8_t > bytes;
    model_t loaded{};
    const std::string static_name = "VuStaticModelAsset/" + asset;
    const std::string animated_name = "VuAnimatedModelAsset/" + asset;
    bool loaded_ok = m_archive.read( static_name.c_str( ), bytes ) && load_model( bytes, loaded );
    // --scene-compare (diagnostic, the evidence for the switch to load_model_scene): the heuristic load_model and the
    // retail scene load side by side, per part. Measured on Hydroelectric (2026-10-31): geometry, textures, cubes,
    // constants and node matrices identical on 74 of 77 models; the detail / alpha differences are load_model taking
    // the NEXT shader's name (it reads the text after table 2's "gAlphaTestEnabled"), predicted 14 / 2 / 1 / 7 / 1 /
    // 66 / 1 / 7 by tools tmp\blendproof.py and measured the same; z09a (41 of 259 parts) and effects_7 / _11 (its
    // invented duplicate filter) differ in their part lists.
    if ( c_engine::option( "scene-compare" ) != nullptr && loaded_ok )
    {
        model_t scened{};
        const bool sok = load_model_scene( bytes, scened );
        int geo = 0, tex = 0, det = 0, cube = 0, con = 0, alpha = 0, node = 0;
        const std::size_t n = scened.parts.size( ) < loaded.parts.size( ) ? scened.parts.size( ) : loaded.parts.size( );
        for ( std::size_t k = 0; k < n; ++k )
        {
            const model_part_t& a = loaded.parts[ k ];
            const model_part_t& b = scened.parts[ k ];
            geo += a.buffer != b.buffer || a.index_start != b.index_start || a.index_count != b.index_count;
            tex += a.texture != b.texture;
            det += a.detail != b.detail;
            cube += a.cube != b.cube;
            con += std::memcmp( a.diffuse, b.diffuse, 12 ) != 0 || std::memcmp( a.specular, b.specular, 12 ) != 0
                || a.specular_amount != b.specular_amount || a.environment_amount != b.environment_amount;
            alpha += a.alpha_test != b.alpha_test;
            node += std::memcmp( a.node, b.node, 64 ) != 0;
            if ( a.texture != b.texture || a.alpha_test != b.alpha_test || std::memcmp( a.diffuse, b.diffuse, 12 ) != 0 )
                LOG_INFO( "scene-compare %s part %d: tex '%s' / '%s' alpha %d / %d diffuse %.3f %.3f %.3f / %.3f %.3f %.3f", asset.c_str( ), static_cast< int >( k ),
                    a.texture.c_str( ), b.texture.c_str( ), a.alpha_test, b.alpha_test, a.diffuse[ 0 ], a.diffuse[ 1 ], a.diffuse[ 2 ], b.diffuse[ 0 ], b.diffuse[ 1 ], b.diffuse[ 2 ] );
        }
        LOG_INFO( "scene-compare %s ok %d parts %d / %d buffers %d / %d: geo %d tex %d detail %d cube %d constants %d alpha %d node %d", asset.c_str( ), sok ? 1 : 0,
            static_cast< int >( loaded.parts.size( ) ), static_cast< int >( scened.parts.size( ) ), static_cast< int >( loaded.buffers.size( ) ),
            static_cast< int >( scened.buffers.size( ) ), geo, tex, det, cube, con, alpha, node );
    }    if ( !loaded_ok )
    {
        bytes.clear( );
        loaded = {};
        loaded_ok = m_archive.read( animated_name.c_str( ), bytes ) && load_model( bytes, loaded );
    }
    if ( !loaded_ok )
    {
        LOG_ERROR( "model %s", asset.c_str( ) );
        return m_models.emplace( asset, std::move( cached ) ).first->second;
    }

    std::vector< int > geometries( loaded.buffers.size( ), -1 );
    for ( std::size_t index = 0; index < loaded.buffers.size( ); ++index )
    {
        const model_buffer_t& buffer = loaded.buffers[ index ];
        geometries[ index ] = c_engine::geometry(
            buffer.vertices.data( ),
            static_cast< int >( buffer.vertices.size( ) ),
            buffer.indices.data( ),
            static_cast< int >( buffer.indices.size( ) )
        );
    }
    for ( const model_part_t& part : loaded.parts )
    {
        cached_part_t item{};
        item.geometry = geometries[ static_cast< std::size_t >( part.buffer ) ];
        item.texture = texture_for( part.texture );
        item.detail = part.detail.empty( ) ? -1 : texture_for( part.detail );
        item.index_start = part.index_start;
        item.index_count = part.index_count;
        item.alpha_test = part.alpha_test;
        item.paint = part.texture.find( "Proxy" ) != std::string::npos || part.texture.find( "Diffuse" ) != std::string::npos;
        std::memcpy( item.node, part.node, sizeof( item.node ) );
        item.specular[ 0 ] = part.specular[ 0 ];
        item.specular[ 1 ] = part.specular[ 1 ];
        item.specular[ 2 ] = part.specular[ 2 ];
        item.diffuse[ 0 ] = part.diffuse[ 0 ];
        item.diffuse[ 1 ] = part.diffuse[ 1 ];
        item.diffuse[ 2 ] = part.diffuse[ 2 ];
        item.specular_amount = part.specular_amount;
        item.environment_amount = part.environment_amount;
        item.cube = part.cube;
        // The substring rule is a guess, but it holds for the hulls:
        // a boat is a SINGLE part whose material is literally named
        // Boat_proxy/Proxy_diffuse, so the paint flag fires correctly.
        cached.push_back( item );
    }
    // Report the authored shading constants once per model. These are the
    // numbers the hull pass must use; they come from the asset's material
    // table, never from values typed into the shader, or the next boat
    // with different ones would be silently wrong.
    for ( const model_part_t& part : loaded.parts )
    {
        if ( part.environment_amount <= 0.0f && part.specular_amount <= 0.0f )
            continue;
        LOG_SUCCESS( "material %-30s diffuse %.2f %.2f %.2f  specular %.2f %.2f %.2f x%.2f  env %.2f cube %s",
            part.texture.c_str( ),
            part.diffuse[ 0 ], part.diffuse[ 1 ], part.diffuse[ 2 ],
            part.specular[ 0 ], part.specular[ 1 ], part.specular[ 2 ],
            part.specular_amount, part.environment_amount,
            part.cube.empty( ) ? "-" : part.cube.c_str( ) );
    }

    // Keep the skeleton: the clip loader checks its bone count against
    // this, and the skinned draw path will need the bind pose.
    if ( !loaded.bones.empty( ) )
        m_skeletons.emplace( asset, loaded.bones );

    // Keep the bind mesh for anything carrying a per-vertex binding.
    for ( std::size_t index = 0; index < loaded.buffers.size( ); ++index )
    {
        const model_buffer_t& buffer = loaded.buffers[ index ];
        if ( buffer.skin.empty( ) || buffer.skin.size( ) != buffer.vertices.size( ) )
            continue;
        skinned_mesh_t mesh{};
        mesh.bind = buffer.vertices;
        mesh.skin = buffer.skin;
        mesh.indices = buffer.indices;
        for ( const cached_part_t& part : cached )
        {
            mesh.texture = part.texture;
            mesh.detail = part.detail;
            mesh.alpha_test = part.alpha_test;
            break;
        }
        m_skinned.emplace( asset, std::move( mesh ) );
        break;
    }
    LOG_SUCCESS( "model %s  %u parts", asset.c_str( ), static_cast< unsigned >( cached.size( ) ) );
    return m_models.emplace( asset, std::move( cached ) ).first->second;
}

// `geometry_override` swaps in a per-instance streamed clone while keeping
// every part's own texture, material tag, detail map and node matrix. The
// animated props need exactly that: each prop model turned out to have a
// single distinct geometry buffer even when it carries two parts, so one
// clone backs all of a prop's parts and the placement can be made once at
// load and then fed new vertices every frame.
inline void c_world::place_model( const std::string& asset, const float* world, int bob, int alpha_override, const float* tint, const char* skin, int geometry_override, std::vector< int >* out_placements)
{
    const float white[ 3 ] = { 1.0f, 1.0f, 1.0f };
    const float* color = tint != nullptr ? tint : white;
    const int skin_id = skin != nullptr && skin[ 0 ] != '\0' ? texture_for( skin ) : -1;
    // The hull is a single part whose material is the placeholder
    // Boat_proxy/Proxy_diffuse, replaced here by the skin from SkinDB.
    // Measured working: Vector and TidalBlade both resolve a real
    // texture id, so skin selection and decode are not the cause of the
    // boats looking wrong.
    for ( const cached_part_t& part : model_for( asset ) )
    {
        int alpha = bob != 0 ? 4 : part.alpha_test;
        if ( alpha_override >= 0 )
            alpha = alpha_override;
        const int texture = skin_id >= 0 && part.paint ? skin_id : part.texture;
        float placed_node[ 16 ]{};
        math_multiply( placed_node, world, part.node );
        const int handle = c_engine::place( geometry_override >= 0 ? geometry_override : part.geometry,
            texture, part.index_start, part.index_count, placed_node, color, alpha, nullptr, part.detail );
        // While the entity tree's props are placed, every placement is
        // collected with its owner so a rotator can drive the ones it owns
        // each frame (adopt_rotators).
        if ( m_collect != nullptr && handle >= 0 )
        {
            collected_t taken{};
            taken.handle = handle;
            taken.owner = m_place_owner;
            std::memcpy( taken.world.data( ), placed_node, sizeof( placed_node ) );
            m_collect->push_back( std::move( taken ) );
        }
        // --no-reflect is the A/B switch for the cube term. DiffuseColor
        // tints every lit material, so a part whose diffuse is not white
        // needs its constants even with no cube and no specular.
        const bool tinted = part.diffuse[ 0 ] < 0.999f
            || part.diffuse[ 1 ] < 0.999f || part.diffuse[ 2 ] < 0.999f;
        if ( ( part.environment_amount > 0.0f || part.specular_amount > 0.0f || tinted )
            && c_engine::option( "no-reflect" ) == nullptr )
        {
            const int cube = part.environment_amount > 0.0f ? cube_for( part.cube ) : -1;
            c_engine::shade( handle, cube, part.specular, part.specular_amount,
                part.environment_amount, part.diffuse );
            // Where the reflective geometry actually is. Translation is
            // at 12/13/14 because the matrices are column-major - reading
            // 3/7/11 printed 0,0,0 for everything and nearly sent the
            // investigation the wrong way.
            if ( part.environment_amount > 0.0f )
                LOG_VERBOSE( "reflective %-34s at %.1f %.1f %.1f  tris %d  env %.2f  alpha %d  cube %d",
                    asset.c_str( ), placed_node[ 12 ], placed_node[ 13 ], placed_node[ 14 ],
                    part.index_count / 3, part.environment_amount, alpha, cube );
        }
        if ( out_placements != nullptr )
            out_placements->push_back( handle );
        ++m_draws;
    }
}
