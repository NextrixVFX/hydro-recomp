#pragma once
#include <array>

#include <workspace/core/assets/archive.hxx>
#include <workspace/core/assets/model.hxx>
#include <workspace/core/assets/skin.hxx>
#include <workspace/core/entity/bind.hxx>
#include <workspace/core/entity/schema.hxx>
#include <workspace/core/light/rig.hxx>
#include <workspace/core/trigger/set.hxx>
#include <workspace/core/assets/reader.hxx>
#include <workspace/core/assets/tree.hxx>
#include <workspace/core/game/input.hxx>
#include <workspace/core/world/control.hxx>
#include <workspace/core/world/pfx.hxx>
#include <workspace/core/entity/light.hxx>
#include <workspace/core/entity/node.hxx>
#include <workspace/core/game/types.hxx>
#include <workspace/core/world/water.hxx>
#include <workspace/core/assets/texture.hxx>
#include <workspace/utility/logger.hxx>
#include <workspace/utility/trace.hxx>
#include <workspace/utility/math.hxx>

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#if defined( _WIN32 )
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#endif

// Screens/Background. Static models keep the transforms from their
// VuTransformComponent, and the front-end camera is the one stored on
// VuSetFrontEndCameraEntity.
class c_world
{
public:
    c_world( )
    {
        m_eye[ 0 ] = 0.0f;
        m_eye[ 1 ] = -40.0f;
        m_eye[ 2 ] = 18.0f;
        m_target[ 0 ] = 0.0f;
        m_target[ 1 ] = 0.0f;
        m_target[ 2 ] = 4.0f;
    }

    const float* eye( ) const { return m_eye; }
    const float* target( ) const { return m_target; }
    float far_plane( ) const { return m_far; }

private:
    // A placement captured inside a rotating subtree, with the world matrix
    // it was placed at.
    struct collected_t
    {
        int handle = -1;
        std::array< float, 16 > world{};
        // The entity that owns this placement. A motion entity's ref names ONE
        // node, so the adopt step has to know which handles came from it.
        std::string owner;
    };

    // A VuRotateMotionEntity. Every field and the integration below are
    // transliterated from VuRotateMotionEntity_ctor @0x5d0dc0 and
    // VuRotateMotionEntity_tick @0x5d1090 - see memory/decompile.md.
    struct rotator_t
    {
        std::string path;
        float base[ 16 ]{};
        // ctor defaults: Axis (0,0,1), Speed pi/2 rad/s (a VuAngleProperty,
        // so radians), Local true, angle 0.
        float axis[ 3 ]{ 0.0f, 0.0f, 1.0f };
        float speed = 1.5707964f;
        bool local = true;
        float angle = 0.0f;
        // Each driven placement with its transform expressed RELATIVE to the
        // rotator's base. The retail engine re-derives a child's transform
        // from its parent every frame; capturing the relative matrix once and
        // re-composing gives the same result without an entity hierarchy.
        // FLAGGED as an implementation difference, not a derived constant.
        std::vector< int > placements;
        std::vector< std::array< float, 16 > > relative;
    };

    struct cached_part_t
    {
        int geometry = -1;
        int texture = -1;
        int detail = -1;
        int index_start = 0;
        int index_count = 0;
        int alpha_test = 0;
        bool paint = false;
        float node[ 16 ]{};
        // The authored shading constants, carried from the model's material
        // table through to c_engine::shade(). The cube is kept as a NAME and
        // resolved at place time, not here: the model cache is filled during
        // the entity walk, and a model loaded before the scene's
        // VuSubstituteAssetEntity is visited would resolve Proxy_cube to the
        // placeholder asset - which really exists in the pack, so the wrong
        // answer would succeed silently instead of erroring.
        std::string cube;
        float specular[ 3 ]{ 1.0f, 1.0f, 1.0f };
        // The material's DiffuseColor. The retail pixel shader folds it into
        // the lit term, so it tints every material, not just reflective ones.
        float diffuse[ 3 ]{ 1.0f, 1.0f, 1.0f };
        float specular_amount = 0.0f;
        float environment_amount = 0.0f;
    };

    std::string pack_path( ) const
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

    int texture_for( const std::string& path )
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
    int cube_for( const std::string& path )
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

    // The drawable parts of a geom pattern's model, in the small form c_pfx
    // asks for. A VuPfxGeomPattern draws a static model per particle, and the
    // model cache lives here, so the pfx system gets it through the same kind
    // of callback it already uses for texture ids.
    struct pfx_part_t
    {
        int geometry = -1;
        int texture = -1;
        int detail = -1;
        int index_start = 0;
        int index_count = 0;
        float node[ 16 ]{};
    };

    const std::vector< pfx_part_t >& pfx_parts( const std::string& asset )
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

    const std::vector< cached_part_t >& model_for( const std::string& asset )
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
        if ( !loaded_ok )
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
    void place_model( const std::string& asset, const float* world, int bob = 0, int alpha_override = -1, const float* tint = nullptr, const char* skin = nullptr, int geometry_override = -1, std::vector< int >* out_placements = nullptr )
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
            // While a VuRotateMotionEntity's subtree is being visited, every
            // placement made inside it is collected so the rotation can drive
            // them each frame.
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

    const value_t* field( const value_t* overlay, const value_t* properties, const char* key ) const
    {
        const value_t* value = overlay != nullptr ? find( *overlay, key ) : nullptr;
        if ( value == nullptr && properties != nullptr )
            value = find( *properties, key );
        return value;
    }

    const value_t* template_root( const std::string& name )
    {
        auto existing = m_templates.find( name );
        if ( existing == m_templates.end( ) )
        {
            std::vector< std::uint8_t > bytes;
            value_t parsed{};
            const std::string path = "VuTemplateAsset/" + name;
            if ( m_archive.read( path.c_str( ), bytes ) )
                parsed = c_reader( bytes ).parse( );
            else
                LOG_ERROR( "template %s", path.c_str( ) );
            existing = m_templates.emplace( name, std::move( parsed ) ).first;
        }

        const value_t* project = find( existing->second, "VuProject" );
        return project != nullptr ? find( *project, "RootEntity" ) : nullptr;
    }

    // Every property key a level actually authors, per entity class. This is
    // the input side of the silent-drop audit: what the data offers. What the
    // code consumes is the other side, and the gap between them is the list of
    // authored values this engine currently throws away.
    void record_authored( const std::string& type, const value_t* block )
    {
        if ( block == nullptr || block->kind != value_t::kind_t::object )
            return;
        std::set< std::string >& keys = m_authored_keys[ type ];
        for ( const auto& pair : block->object )
            keys.insert( pair.first );
    }

    // Report it against the retail schema. A key the schema does not declare
    // means the schema is incomplete for that class - the emulator could not
    // finish its constructor, or the class is not in the table at all - and
    // that is worth knowing before anything is built on top of it.
    void audit_authored( ) const
    {
        int classes_known = 0;
        int classes_unknown = 0;
        int keys_declared = 0;
        int keys_undeclared = 0;
        for ( const auto& pair : m_authored_keys )
        {
            const entity_schema_t* schema = entity_schema_find( pair.first.c_str( ) );
            if ( schema == nullptr )
            {
                ++classes_unknown;
                LOG_VERBOSE( "authored: %-34s %2u keys, NO SCHEMA",
                    pair.first.c_str( ), static_cast< unsigned >( pair.second.size( ) ) );
                continue;
            }
            ++classes_known;
            std::string missing;
            for ( const std::string& key : pair.second )
            {
                if ( entity_schema_property( schema, key.c_str( ) ) != nullptr )
                {
                    ++keys_declared;
                    continue;
                }
                ++keys_undeclared;
                missing += ( missing.empty( ) ? "" : ", " ) + key;
            }
            if ( !missing.empty( ) )
            {
                LOG_VERBOSE( "authored: %-34s declares %d, NOT in schema: %s",
                    pair.first.c_str( ), schema->m_count, missing.c_str( ) );
            }
        }
        LOG_INFO( "authored keys: %d classes with a schema, %d without; %d keys declared, %d not",
            classes_known, classes_unknown, keys_declared, keys_undeclared );
    }

    // VuSetFrontEndCameraEntity, read from its constructor @0x408b00 and its
    // one input plug's handler @0x408f90. All seven properties and all four
    // defaults are the constructor's own: Interpolation Time 0x40000000 = 2.0
    // at +0x60, the three Modify booleans FALSE at +0x64/+0x65/+0x66, Eye and
    // Target Position zero at +0x68/+0x78, Far Plane 0x447A0000 = 1000 at
    // +0x88.
    //
    // THE ENTITY DOES NOTHING AT LOAD. @0x408f90 is the whole of its
    // behaviour, and it runs only when the "Trigger" input plug fires.
    //
    // No member carries a literal default: every one of them is filled from
    // the schema by read_front_end_camera, so the constructor's values live in
    // exactly one place and cannot drift out of step with it.
    struct front_end_camera_t
    {
        float interpolation_time = 0.0f;
        bool modify_eye = false;
        bool modify_target = false;
        bool modify_far = false;
        float eye[ 3 ]{};
        float target[ 3 ]{};
        float far_plane = 0.0f;
    };

    // VuSetFrontEndCameraEntity, all seven properties through the schema.
    // Its constructor @0x408b00 builds the nodes in this order and binds them
    // to +0x60, +0x64, +0x65, +0x66, +0x68, +0x78 and +0x88; every default
    // therefore comes from the constructor by way of the schema instead of
    // being written here. None of the seven is a VuAngleProperty or a
    // VuPercentageProperty, so nothing is converted on the way in.
    //
    // The remaining front-end camera path - the screen-to-screen blend - was
    // inferred and is banked in reference/shot.hxx.
    front_end_camera_t read_front_end_camera( const value_t* overlay,
        const value_t* properties ) const
    {
        front_end_camera_t shot{};
        const c_entity_binder bind( "VuSetFrontEndCameraEntity", overlay, properties );
        shot.interpolation_time = bind.real( "Interpolation Time" );
        shot.modify_eye = bind.boolean( "Modify Eye Position" );
        shot.modify_target = bind.boolean( "Modify Target Position" );
        shot.modify_far = bind.boolean( "Modify Far Plane" );
        bind.vector( "Eye Position", shot.eye );
        bind.vector( "Target Position", shot.target );
        shot.far_plane = bind.real( "Far Plane" );
        return shot;
    }

    // A type that starts with '#' is a VuTemplateAsset. The instance transform
    // and properties replace the template root; the template supplies the entity.
    // `trail` is the entity's absolute path, built from the "name" field that
    // sits BESIDE "type" and "data" rather than inside the Properties. The
    // event graph addresses entities by exactly this path - a
    // VuPlayAnimationEntity names its target as, for instance,
    // "Hydroelectric/Terrain/zone06 folder/Props/Helicopter stuff/Helicopter"
    // - so the path has to be carried down the walk to resolve any of it.
    void visit( const value_t& entity, const float* parent, bool use_local = true,
        const value_t* overlay = nullptr, const std::string& trail = std::string( ) )
    {
        const value_t* data = find( entity, "data" );
        const value_t* label = find( entity, "name" );
        const std::string here = label != nullptr && label->kind == value_t::kind_t::text
            && !label->text.empty( )
            ? ( trail.empty( ) ? label->text : trail + "/" + label->text )
            : trail;
        int rotator = -1;
        // A motion entity sits as a CHILD of the thing it drives, and it can
        // arrive through a template expansion, so whether one exists is not
        // knowable until the subtree has been walked. Collect unconditionally
        // and adopt afterwards if a rotator turned up: an empty std::vector
        // allocates nothing, so the entities that never see one cost nothing.
        bool mine = false;
        std::vector< collected_t > collected;
        std::vector< collected_t >* saved_collect = m_collect;
        m_collect = &collected;
        // Which entity any placement made from here belongs to. Restored on
        // EVERY exit path beside m_collect, for the same reason.
        const std::string saved_owner = m_place_owner;
        m_place_owner = here;
        // A TEMPLATE INSTANCE OVERRIDES ITS CHILDREN BY NAME. The instance
        // node's "ChildEntities" is an OBJECT keyed by child name - not the
        // LIST an ordinary entity carries - and each entry holds "Components"
        // and "Properties" DIRECTLY, with no "data" wrapper. The template
        // definition supplies the structure; the instance supplies the
        // placement. Measured on the WarningSiren templates, whose thirteen
        // instances carry the only transform in the whole subtree.
        const value_t* override_node = m_pending_override;
        m_pending_override = nullptr;
        const value_t* over_components = override_node != nullptr
            ? find( *override_node, "Components" ) : nullptr;
        if ( override_node != nullptr )
        {
            ++m_overrides_seen;
            if ( over_components != nullptr && find( *over_components, "VuTransformComponent" ) != nullptr )
                ++m_overrides_placed;
            // Every component key an override carries that is NOT the
            // transform. The transform is merged; anything else here is read
            // from the TEMPLATE's copy, so this census is the measure of what
            // that costs.
            if ( over_components != nullptr )
                for ( const auto& pair : over_components->object )
                    if ( pair.first != "VuTransformComponent" )
                        ++m_override_components[ pair.first ];
        }
        if ( override_node != nullptr && overlay == nullptr )
        {
            const value_t* over_properties = find( *override_node, "Properties" );
            if ( over_properties != nullptr )
                overlay = over_properties;
        }

        const value_t* components = data != nullptr ? find( *data, "Components" ) : nullptr;
        float local[ 16 ]{};
        math_identity( local );
        if ( use_local )
        {
            // The instance's own transform wins over the template's.
            const value_t* transform = over_components != nullptr
                ? find( *over_components, "VuTransformComponent" ) : nullptr;
            if ( transform == nullptr && components != nullptr )
                transform = find( *components, "VuTransformComponent" );
            const value_t* properties = transform != nullptr ? find( *transform, "Properties" ) : nullptr;
            if ( properties != nullptr && properties->kind == value_t::kind_t::object && !properties->object.empty( ) )
                read_transform( *properties, local );
        }
        float world[ 16 ]{};
        math_multiply( world, parent, local );

        // Every entity's placed transform, by path. An attach composes the
        // child onto its PARENT ENTITY, not onto a placement, and the parent
        // is named by a ref that may resolve to an entity this walk has not
        // reached yet - so the matrix has to be recorded as it is computed.
        if ( !here.empty( ) )
        {
            std::array< float, 16 > placed{};
            std::memcpy( placed.data( ), world, sizeof( world ) );
            m_entity_world[ here ] = placed;
        }

        // Resolve this entity's script connections once, the way retail
        // resolves them once at load. See collect_graph and fire().
        if ( !here.empty( ) )
            collect_graph( here, components );

        const value_t* type = find( entity, "type" );
        if ( type != nullptr && type->kind == value_t::kind_t::text && type->text.size( ) > 1 && type->text[ 0 ] == '#' )
        {
            const std::string name = type->text.substr( 1 );
            if ( m_stack.insert( name ).second )
            {
                const value_t* root = template_root( name );
                const value_t* properties = data != nullptr ? find( *data, "Properties" ) : nullptr;
                if ( root != nullptr )
                {
                    float carried[ 16 ];
                    std::memcpy( carried, world, sizeof( carried ) );
                    const float sx = std::sqrt( local[ 0 ] * local[ 0 ] + local[ 1 ] * local[ 1 ] + local[ 2 ] * local[ 2 ] );
                    const float sy = std::sqrt( local[ 4 ] * local[ 4 ] + local[ 5 ] * local[ 5 ] + local[ 6 ] * local[ 6 ] );
                    const float sz = std::sqrt( local[ 8 ] * local[ 8 ] + local[ 9 ] * local[ 9 ] + local[ 10 ] * local[ 10 ] );
                    const value_t* root_type = find( *root, "type" );
                    const bool foliage = root_type != nullptr && root_type->kind == value_t::kind_t::text && root_type->text == "VuFoliageEntity";
                    if ( foliage && sx > 0.95f && sx < 1.05f && sy > 0.95f && sy < 1.05f && sz > 0.95f && sz < 1.05f )
                    {
                        const value_t* root_data = find( *root, "data" );
                        const value_t* root_components = root_data != nullptr ? find( *root_data, "Components" ) : nullptr;
                        const value_t* root_transform = root_components != nullptr ? find( *root_components, "VuTransformComponent" ) : nullptr;
                        const value_t* root_properties = root_transform != nullptr ? find( *root_transform, "Properties" ) : nullptr;
                        if ( root_properties != nullptr )
                        {
                            float tmpl[ 16 ];
                            math_identity( tmpl );
                            read_transform( *root_properties, tmpl );
                            math_multiply( carried, world, tmpl );
                        }
                    }
                    // A template's root must not add a segment of its own, or
                    // every path beneath it gains a level the event graph's
                    // EntityName does not have.
                    //
                    // The instance's per-child overrides ride down with it.
                    // They key the template root's OWN children by name, one
                    // level only, so a nested template instance replaces the
                    // map rather than inheriting it - saved and restored like
                    // the collector above.
                    const value_t* saved_overrides = m_child_overrides;
                    const value_t* instance_children = data != nullptr
                        ? find( *data, "ChildEntities" ) : nullptr;
                    m_child_overrides = instance_children != nullptr
                        && instance_children->kind == value_t::kind_t::object
                        ? instance_children : nullptr;
                    visit( *root, carried, false, properties, here );
                    m_child_overrides = saved_overrides;
                }
                m_stack.erase( name );
                if ( root != nullptr )
                {
                    // This return bypasses the tail below, so the collector
                    // has to be restored here or m_collect is left pointing
                    // at this frame's destroyed local - which crashed the
                    // next place_model with an access violation.
                    m_collect = saved_collect;
                    m_place_owner = saved_owner;
                    if ( saved_collect != nullptr )
                        saved_collect->insert( saved_collect->end( ),
                            collected.begin( ), collected.end( ) );
                    return;
                }
            }
        }

        if ( type != nullptr && type->kind == value_t::kind_t::text && data != nullptr )
        {
            const value_t* properties = find( *data, "Properties" );
            record_authored( type->text, properties );
            record_authored( type->text, overlay );
            // RECORDED, NOT APPLIED. The entity's whole behaviour is its
            // "Trigger" plug handler @0x408f90; the constructor @0x408b00
            // touches no camera and the load walk must not either. What used
            // to stand here read Eye Position, Target Position and Far Plane
            // straight onto the viewer the moment the entity was seen, which
            // both ignored the three Modify booleans that gate them and fired
            // without the trigger that is the only thing that runs the
            // handler.
            if ( type->text == "VuSetFrontEndCameraEntity" && !here.empty( ) )
                m_front_end.emplace( here, read_front_end_camera( overlay, properties ) );
            // VuWaterSurfaceEntity is handled by the water work in its own
            // right - it has 14 authored properties and none of the geometry,
            // height resolution or displacement that read them has been
            // transliterated. The partial reader that used to sit here, and
            // the rectangle-overlap height lookup it fed, are banked in
            // reference/water.hxx.
            if ( type->text == "VuWaterTextureSettingsEntity" && properties != nullptr )
            {
                // VuWaterTextureSettingsEntity_ctor @0x5d2bd0, emulated. Its
                // Gravity default is 0x411CD0E5 = 9.801, a THIRD distinct
                // gravity constant - different from the ocean wave's 9.806 and
                // from the textbook value. "TimeFactor" (default 1.0) is also
                // authored here and is not consumed anywhere yet.
                const c_entity_binder bind( type->text, overlay, properties );
                m_wave_height = bind.real( "HeightFactor" );
                m_wave_scale = bind.real( "NormalTextureScale" );
                m_wave_wind = bind.real( "WindSpeed" );
                m_wave_size = bind.real( "WorldSize" );
                m_ocean.add(
                    // Complexity is not one of this entity's properties; the
                    // detail field is built at a fixed size.
                    6.0f,
                    m_wave_size,
                    0.0f,
                    m_wave_wind,
                    bind.real( "DirectionalPower" ),
                    bind.real( "SuppressionWaveLength" ),
                    m_wave_height,
                    bind.real( "Gravity" ),
                    nullptr,
                    nullptr,
                    1.0f,
                    1.0f,
                    true
                );
            }
            if ( ( type->text == "VuInfiniteOceanWaveEntity" || type->text == "VuRectangularOceanWaveEntity" ) && properties != nullptr )
            {
                const bool rectangular = type->text == "VuRectangularOceanWaveEntity";
                const float center[ 2 ] = { world[ 12 ], world[ 13 ] };
                const float extent[ 2 ] = {
                    std::sqrt( world[ 0 ] * world[ 0 ] + world[ 1 ] * world[ 1 ] + world[ 2 ] * world[ 2 ] ),
                    std::sqrt( world[ 4 ] * world[ 4 ] + world[ 5 ] * world[ 5 ] + world[ 6 ] * world[ 6 ] )
                };
                // Every key, unit and default below comes from the schema -
                // VuInfiniteOceanWaveEntity_ctor @0x5d8380 and
                // VuRectangularOceanWaveEntity_ctor @0x5d8ad0, both emulated.
                // Wave Direction is a VuAngleProperty and the two Decay Ratios
                // are VuPercentageProperty, so the binder converts an authored
                // value and leaves a constructor default alone, which is
                // already stored.
                const c_entity_binder bind( type->text, overlay, properties );
                m_ocean.add(
                    bind.real( "Complexity" ),
                    bind.real( "Bin Siz" ),
                    bind.real( "Wave Direction" ),
                    bind.real( "Wind Speed" ),
                    bind.real( "Directional Power" ),
                    bind.real( "Suppression Wave Length" ),
                    bind.real( "Height Multiplier" ),
                    bind.real( "Gravity" ),
                    rectangular ? center : nullptr,
                    rectangular ? extent : nullptr,
                    bind.real( "Decay Ratio X %" ),
                    bind.real( "Decay Ratio Y %" )
                );
            }
            // VuPointWaveEntity. Recorded, not emitted: retail only creates a
            // wave when the "Emit" INPUT plug fires (VuPointWaveEntity_tick
            // @0x5d4c50 gates on the flag that onEmitPlug @0x5d4b40 sets), so
            // the parameters wait here until the event graph reaches them.
            // Defaults are VuPointWaveEntity_ctor @0x5d43b0.
            if ( type->text == "VuPointWaveEntity" && properties != nullptr && !here.empty( ) )
            {
                c_ocean::ripple_t source{};
                source.x = world[ 12 ];
                source.y = world[ 13 ];
                // All seven from VuPointWaveEntity_ctor @0x5d43b0, emulated.
                const c_entity_binder bind( type->text, overlay, properties );
                source.falloff = bind.real( "Falloff Time" );
                source.magnitude = bind.real( "Magnitude" );
                source.range_start = bind.real( "Range Start" );
                source.range_end = bind.real( "Range End" );
                source.speed = bind.real( "Speed" );
                source.frequency = bind.real( "Frequency" );
                source.foaminess = bind.real( "Foaminess" );
                m_ripple_sources.emplace( here, source );
            }
            if ( type->text == "VuGlobalGfxSettingsEntity" && properties != nullptr )
            {
                // Every key here maps onto a named constant in the retail
                // shaders: Contrast/Tint feed PostProcess/Effect, and the
                // Water * keys feed the water shader's gAmbientColor,
                // gDiffuseColor, gFoamAmbientColor, gFoamDiffuseColor and
                // gFoamTextureSize. See memory/water.md.
                if ( properties->kind == value_t::kind_t::object )
                    for ( const auto& pair : properties->object )
                    {
                        const value_t* red = find( pair.second, "R" );
                        if ( red != nullptr )
                            LOG_VERBOSE( "gfx %-28s = %.0f %.0f %.0f", pair.first.c_str( ),
                                number( red, -1.0f ),
                                number( find( pair.second, "G" ), -1.0f ),
                                number( find( pair.second, "B" ), -1.0f ) );
                        else
                            LOG_VERBOSE( "gfx %-28s = %.4f", pair.first.c_str( ),
                                number( &pair.second, -999.0f ) );
                    }
                // Every fallback below is VuGlobalGfxSettingsEntity's own
                // constructor default, read out of the constructor under
                // emulation (tools/ctor.py) rather than chosen: Fog Start
                // 0x43FA0000 = 500, Fog End 0x447A0000 = 1000, Fog Color
                // 0xFF000000 = black at full alpha. The values that stood here
                // before - 0, 900 and (156,196,215) - were invented.
                // The far plane is authored HERE, not on any camera - neither
                // VuBoatCamera_update nor the front-end camera sets one, which
                // is why a placeholder stood in for it. Constructor default
                // 0x43FA0000 = 500.
                const c_entity_binder bind( type->text, overlay, properties );
                m_authored_far = bind.real( "Camera Far Plane" );
                float fog_color[ 4 ]{};
                bind.color( "Fog Color", fog_color );
                const float red = fog_color[ 0 ];
                const float green = fog_color[ 1 ];
                const float blue = fog_color[ 2 ];
                const float start = bind.real( "Fog Start" );
                const float end = bind.real( "Fog End" );
                LOG_VERBOSE( "gfx settings: fog %.0f..%.0f  colour %.3f %.3f %.3f",
                    start, end, red, green, blue );
                // Hydroelectric authors Fog End 1800 but does NOT author
                // Camera Far Plane, so the far plane falls back to the
                // constructor's 500 - and geometry would be clipped long
                // before the fog it is meant to fade into reaches full
                // density. One of the two comes from somewhere not yet found.
                // Reported rather than reconciled: picking a value here would
                // be an invention.
                if ( end > m_authored_far )
                {
                    LOG_WARNING( "gfx settings: Fog End %.0f is beyond the far plane %.0f - "
                        "the far plane's real source has not been found",
                        end, m_authored_far );
                }
                c_engine::fog( start, end, red, green, blue );
            }
            if ( type->text == "VuDirectionalLightEntity" && properties != nullptr )
            {
                const value_t* transform = components != nullptr ? find( *components, "VuTransformComponent" ) : nullptr;
                const value_t* pose = transform != nullptr ? find( *transform, "Properties" ) : nullptr;
                const value_t* rotation = pose != nullptr ? find( *pose, "Rotation" ) : nullptr;
                float degrees[ 3 ] = {
                    number( find( rotation != nullptr ? *rotation : *properties, "X" ), 0.0f ),
                    number( find( rotation != nullptr ? *rotation : *properties, "Y" ), 0.0f ),
                    number( find( rotation != nullptr ? *rotation : *properties, "Z" ), 0.0f )
                };
                float basis[ 16 ]{};
                const float origin[ 3 ] = {};
                const float scale[ 3 ] = { 1.0f, 1.0f, 1.0f };
                math_transform( basis, origin, degrees, scale );
                const float direction[ 3 ] = { -basis[ 8 ], -basis[ 9 ], -basis[ 10 ] };
                c_engine::sun( direction );
            }
            // VuFoliageEntity drew a hand-built billboard quad from an invented
            // vertex layout. Deleted; build it from the entity's own draw path.
            if ( type->text == "VuStaticPfxEntity" )
            {
                // VuStaticPfxEntity_ctor @0x5EAA40, emulated. Note its
                // "Initially Active" defaults TRUE, where
                // VuRigidBodyComponent's defaults FALSE - a default belongs to
                // its own class and never carries across.
                const c_entity_binder bind( type->text, overlay, properties );
                const std::string effect = bind.text( "Effect Name" );
                if ( !effect.empty( ) )
                {
                    // Pfx Color is a VuColor, which the binder has no typed
                    // accessor for yet, so the four bytes are read here.
                    const value_t* tint = bind.authored( "Pfx Color" );
                    const float color[ 4 ] = {
                        number( tint != nullptr ? find( *tint, "R" ) : nullptr, 255.0f ) / 255.0f,
                        number( tint != nullptr ? find( *tint, "G" ) : nullptr, 255.0f ) / 255.0f,
                        number( tint != nullptr ? find( *tint, "B" ) : nullptr, 255.0f ) / 255.0f,
                        number( tint != nullptr ? find( *tint, "A" ) : nullptr, 255.0f ) / 255.0f
                    };
                    m_pfx.add( m_archive, effect, world, bind.real( "Pfx Scale", 1.0f ),
                        color, bind.boolean( "Initially Active", true ) );
                }
            }
            if ( type->text == "VuSkyBoxEntity" && properties != nullptr )
            {
                const c_entity_binder bind( type->text, overlay, properties );
                const std::string asset = bind.text( "Model Asset" );
                if ( !asset.empty( ) )
                    m_sky = asset;
            }
            // The hull shader's gAmbLightColor. Colours are authored as 0..255
            // bytes throughout the level data.
            if ( type->text == "VuAmbientLightEntity" && properties != nullptr )
            {
                const c_entity_binder bind( type->text, overlay, properties );
                bind.color( "Color", m_rig.ambient( ) );
                m_rig.mark_lit( );
            }
            // gDirLightFrontColor and gDirLightBackColor - the shader's light
            // is TWO-SIDED, and Hydroelectric authors a warm front with a cool
            // back, which a one-sided lambert cannot reproduce.
            //
            // The entity carries NO direction property, so gDirLightWorld has
            // to come from this transform. Which axis it is has not been
            // reversed, so the matrix is kept whole and the choice is made
            // (and flagged) where it is used.
            if ( type->text == "VuDirectionalLightEntity" && properties != nullptr )
            {
                const c_entity_binder bind( type->text, overlay, properties );
                bind.color( "Front Color", m_rig.front( ) );
                bind.color( "Back Color", m_rig.back( ) );
                std::memcpy( m_rig.sun_world( ), world, 16 * sizeof( float ) );
                m_rig.mark_sun( );
            }
            // Every Proxy_* name in a material is a placeholder, and this is
            // what replaces it: Proxy_cube -> Hydro/Outside on Hydroelectric.
            // So the per-track environment map is authored, not chosen here.
            if ( type->text == "VuSubstituteAssetEntity" && properties != nullptr )
            {
                const value_t* from = find( *properties, "Asset Name" );
                const value_t* kind = find( *properties, "Asset Type" );
                const value_t* to = find( *properties, "Subst Asset Name" );
                if ( from != nullptr && from->kind == value_t::kind_t::text
                    && to != nullptr && to->kind == value_t::kind_t::text
                    && kind != nullptr && kind->kind == value_t::kind_t::text )
                {
                    m_rig.add_substitution( kind->text + "/" + from->text, to->text );
                }
            }
            // A prop never names its own clip. The binding lives on an action
            // entity's VuScriptComponent, whose plugs name both ends:
            //
            //   Plugs.PlayAnimation.Connections[].EntityName -> target prop
            //   Plugs.Trigger.Connections[].EntityName       -> what fires it
            //   Properties["Animation Asset"]                -> the clip
            //   Properties["Start Time"]                     -> phase offset
            //
            // Measured on Hydroelectric: 19 of 19 connections resolve to a
            // real entity, 19 of 19 of those carry a
            // Vu3dDrawAnimatedModelComponent, and 19 of 19 clips are in the
            // pack. Start Time is why four identical pump pistons do not pump
            // in step; values of 1 and 15 appear.
            // A trigger plane carries NO volume properties - the volume IS its
            // transform. Measured: Position is the centre, Rotation is in
            // degrees, and Scale is the extent, with Y always 1 because the
            // thing is a PLANE (e.g. 82.14 x 1.00 x 17.05 - a gate 82 wide and
            // 17 tall, one unit thick). An optional "Trigger Type" names what
            // may set it off; "AI Boat" appears on the AI nuisance planes.
            // VuTriggerBoxEntity shares EVERYTHING authored with the plane:
            // its create @0x5d1a30 allocates 0x8C bytes, calls
            // VuTriggerEntity_ctor @0x61c6a0 - the base that declares
            // "Initially Active", "Trigger Type", the Activate/Deactivate
            // INPUT plugs and the OnEnter/OnExit outputs - and only swaps in
            // its own vtable. It has NO constructor and NO properties of its
            // own; the volume is its transform, exactly as for the plane.
            // What differs is the crossing test, below.
            if ( type->text == "VuTriggerPlaneEntity" || type->text == "VuTriggerBoxEntity" )
            {
                trigger_t gate{};
                gate.box = type->text == "VuTriggerBoxEntity";
                gate.path = here;
                // The three row lengths are the authored Scale, which IS the
                // half extent the box test compares against - proved for the
                // plane by VuTriggerPlaneEntity_bounds @0x5d18b0 and again for
                // the box by VuTriggerBoxEntity_bounds @0x5d1dc0, which builds
                // its bounding box from 2.0 * the floats at shape+176.
                for ( int axis = 0; axis < 3; ++axis )
                {
                    const float* row = world + axis * 4;
                    gate.scale[ axis ] = std::sqrt( row[ 0 ] * row[ 0 ]
                        + row[ 1 ] * row[ 1 ] + row[ 2 ] * row[ 2 ] );
                }
                std::memcpy( gate.world, world, sizeof( gate.world ) );
                if ( !model_invert( gate.inverse, world ) )
                    gate.inverse[ 0 ] = 0.0f;
                const value_t* kind = properties != nullptr
                    ? find( *properties, "Trigger Type" ) : nullptr;
                if ( kind != nullptr && kind->kind == value_t::kind_t::text )
                    gate.kind = kind->text;
                // Reversed from VuTriggerEntity_ctor at 0x61c6a0: the base
                // class declares an "Initially Active" bool alongside
                // "Trigger Type", and Activate / Deactivate input plugs that
                // flip it. A plane that starts inactive must not fire.
                gate.active = properties == nullptr
                    || number( find( *properties, "Initially Active" ), 1.0f ) != 0.0f;
                // Its OnEnter / OnExit connections are in m_graph with every
                // other entity's, resolved by collect_graph above.
                m_triggers.add( std::move( gate ) );
            }
            // VuOneShotFilterEntity. Its whole behaviour is three input plug
            // handlers over one byte at this+96, all four plugs named by
            // VuOneShotFilterEntity_ctor @0x5ded70:
            //
            //   "In"      VuOneShotFilter_onInPlug      @0x5df000  input
            //   "Reset"   VuOneShotFilter_onResetPlug   @0x5decc0  input
            //   "SetShot" VuOneShotFilter_onSetShotPlug @0x5dece0  input
            //   "Out"     VuScriptOutputPlug_init                  output
            //
            // The ctor is what settles which is which: the three inputs go
            // through VuScriptInputPlug_init @0x5e28f0, which binds a
            // VuMethod1 handler at plug+96, and "Out" through
            // VuScriptOutputPlug_init. Nothing in the authored data says.
            if ( type->text == "VuOneShotFilterEntity" )
                m_filters.emplace( here, filter_t{} );
            // VuDelayEntity - the commonest node in the graph by a wide
            // margin. VuDelayEntity_ctor @0x5df830 stores 0x3F800000 = 1.0
            // into this+96 ("Delay", a VuBasicProperty<float,1>) and 0 into
            // this+100 ("Random Window"), with the pending byte at +104 and
            // the countdown at +108. Its input plug "In" is named inside the
            // init helper @0x5e2a30 and its output plug "Out" in the ctor
            // body; the countdown runs in a method registered under
            // "OnManualTick", not "Decision".
            if ( type->text == "VuDelayEntity" )
            {
                delay_t wait{};
                if ( properties != nullptr )
                {
                    wait.delay = number( field( overlay, properties, "Delay" ), 1.0f );
                    wait.window = number( field( overlay, properties, "Random Window" ), 0.0f );
                }
                m_delays.emplace( here, wait );
            }
            // VuGenericEventEntity. VuGenericEventEntity_ctor @0x5f00e0: one
            // property, "Event Name" (a VuString at this+92, empty by
            // default), the INPUT plug "Broadcast" (named in the init helper
            // @0x5f0500, handler @0x5f0330) and the OUTPUT plug "OnReceived".
            //
            // IT IS A GLOBAL NAMED BUS, NOT A WIRE, and no connection in the
            // authored data shows that. Broadcast hashes the literal
            // "OnGenericEvent" with FNV-1a (basis 0x811c9dc5, prime
            // 16777619) and posts that engine-wide event carrying its own
            // Event Name string; every instance's "OnGenericEvent" hook
            // @0x5f0420 compares the carried string against its OWN Event
            // Name and fires "OnReceived" on a match. A broadcast therefore
            // reaches every entity sharing the name, the sender included.
            if ( type->text == "VuGenericEventEntity" )
            {
                const value_t* named = properties != nullptr
                    ? field( overlay, properties, "Event Name" ) : nullptr;
                m_events.emplace( here, named != nullptr && named->kind == value_t::kind_t::text
                    ? named->text : std::string( ) );
            }
            // VuOnProjectInitializedEntity. VuOnProjectInitializedEntity_ctor
            // @0x5ef8d0 declares ONE plug, the output "Trigger", and hooks
            // @0x5efa60 under "OnProjectInitialized" - that handler does
            // nothing but fire Trigger. This is the real mechanism behind the
            // props that move from the first frame.
            if ( type->text == "VuOnProjectInitializedEntity" )
                m_initialized.push_back( here );
            // VuIfEntity carries no state of its own - its ctor @0x5ddbe0
            // declares only the Trigger input and the Operand/True/False
            // outputs - so recording that the path exists is the whole thing.
            if ( type->text == "VuIfEntity" )
                m_ifs.insert( here );
            // VuAttachEntity. Schema from VuAttachEntity_ctor @0x5f4fa0:
            // "Attach at Start" (bool), "Relative Position" (vec3 at +100),
            // "Relative Rotation" (vec3 at +116), "Node Name" (VuString at
            // +132), and the two VuScriptRefs "Parent" (+156) and "Child"
            // (+160). The input plugs are "Attach"
            // (VuAttachEntity_doAttach @0x5f5550) and "Detach" (@0x5f5690).
            if ( type->text == "VuAttachEntity" )
            {
                attach_t join{};
                join.path = here;
                if ( properties != nullptr )
                {
                    const c_entity_binder bind( type->text, overlay, properties );
                    join.at_start = bind.boolean( "Attach at Start", false );
                    bind.vector( "Relative Position", join.position );
                    join.node = bind.text( "Node Name" );

                    // "Relative Rotation" IS a VuRotation3dProperty - the
                    // schema says degrees - and is deliberately NOT read
                    // through the binder. join.rotation goes to
                    // math_transform, which converts degrees to radians
                    // itself, so binding it would convert twice and scale
                    // every attach rotation by 57.3. Moving it means moving
                    // math_transform's contract with it, which is a change to
                    // every one of its callers, so both stay as they are.
                    const value_t* turn = bind.authored( "Relative Rotation" );
                    if ( turn != nullptr )
                    {
                        join.rotation[ 0 ] = number( find( *turn, "X" ), 0.0f );
                        join.rotation[ 1 ] = number( find( *turn, "Y" ), 0.0f );
                        join.rotation[ 2 ] = number( find( *turn, "Z" ), 0.0f );
                    }
                }
                m_attaches.push_back( std::move( join ) );
            }
            // VuBooleanEntity's one property is "Value", the byte at +96 that
            // VuBooleanEntity_ctor @0x5e5880 zeroes.
            if ( type->text == "VuBooleanEntity" )
            {
                const c_entity_binder bind( type->text, overlay, properties );
                m_booleans.emplace( here, bind.boolean( "Value", false ) );
            }
            // VuCoronaEntity is loaded from the retail entity tree (load_entity_tree).
            ++m_type_census[ type->text ];
            // The class behind each path, so the graph audit can say which
            // ENTITY TYPES the unhandled events are landing on - the only
            // thing that says which constructor to read next. A '#' name is a
            // template reference, not a class.
            if ( !here.empty( ) && type->text[ 0 ] != '#' )
                m_entity_type[ here ] = type->text;
            // VuRotateMotionEntity. Property names and every default below
            // come from VuRotateMotionEntity_ctor @0x5d0dc0: Axis is a
            // VuBasicProperty<VuVector3,8> defaulting to (0,0,1), Speed is a
            // VuAngleProperty (radians) defaulting to pi/2, Local is a bool
            // defaulting to true.
            if ( type->text == "VuRotateMotionEntity" && properties != nullptr )
            {
                rotator_t motion{};
                motion.path = here;
                std::memcpy( motion.base, world, sizeof( motion.base ) );
                // Read through the schema: the three keys, their units and
                // their defaults all come from VuRotateMotionEntity's own
                // constructor, emulated. Speed is a VuAngleProperty, so an
                // authored 360 becomes radians here and the constructor's
                // default of pi/2 is already in radians and is not converted
                // again.
                const c_entity_binder bind( type->text, overlay, properties );
                motion.axis[ 0 ] = 0.0f;
                motion.axis[ 1 ] = 0.0f;
                motion.axis[ 2 ] = 1.0f;
                bind.vector( "Axis", motion.axis );
                motion.speed = bind.real( "Speed" );
                motion.local = bind.boolean( "Local", true );
                m_rotators.push_back( motion );
                rotator = static_cast< int >( m_rotators.size( ) ) - 1;
                mine = true;
            }
            if ( type->text == "VuPlayAnimationEntity" && properties != nullptr )
            {
                const value_t* clip = find( *properties, "Animation Asset" );
                if ( clip != nullptr && clip->kind == value_t::kind_t::text && !clip->text.empty( ) )
                {
                    play_t play{};
                    play.clip = clip->text;
                    play.path = here;
                    // The second asset ref (+164), packed into the params by
                    // onTriggerPlug only when it is set.
                    const value_t* timed = find( *properties, "Timed Event Asset" );
                    if ( timed != nullptr && timed->kind == value_t::kind_t::text )
                        play.timed = timed->text;
                    play.start = number( find( *properties, "Start Time" ), 0.0f );
                    play.looping = number( find( *properties, "Looping" ), 1.0f ) > 0.5f;
                    play.one_shot = number( find( *properties, "One Shot" ), 0.0f ) > 0.5f;
                    // Defaults from VuPlayAnimationEntity_ctor @0x5f5950: Start
                    // Time 0, Blend Time 0, Time Factor 1.0, Looping 1, One
                    // Shot 0. The `factor <= 0 -> 1` clamp that stood here was
                    // invented - a negative factor plays backward in retail.
                    play.factor = number( find( *properties, "Time Factor" ), 1.0f );
                    const value_t* script = components != nullptr
                        ? find( *components, "VuScriptComponent" ) : nullptr;
                    const value_t* plugs = script != nullptr ? find( *script, "Plugs" ) : nullptr;
                    const value_t* trigger = plugs != nullptr ? find( *plugs, "Trigger" ) : nullptr;
                    const value_t* triggers = trigger != nullptr ? find( *trigger, "Connections" ) : nullptr;
                    if ( triggers != nullptr && triggers->kind == value_t::kind_t::list
                        && !triggers->list.empty( ) )
                    {
                        const value_t* named = find( triggers->list.front( ), "EntityName" );
                        if ( named != nullptr && named->kind == value_t::kind_t::text )
                            play.trigger = named->text;
                    }
                    const value_t* plug = plugs != nullptr ? find( *plugs, "PlayAnimation" ) : nullptr;
                    const value_t* links = plug != nullptr ? find( *plug, "Connections" ) : nullptr;
                    if ( links != nullptr && links->kind == value_t::kind_t::list )
                    {
                        for ( const value_t& link : links->list )
                        {
                            const value_t* named = find( link, "EntityName" );
                            if ( named != nullptr && named->kind == value_t::kind_t::text
                                && !named->text.empty( ) )
                                m_play.emplace( named->text, play );
                        }
                    }
                    // The action entity itself, so a "Trigger" event arriving
                    // from the event graph has something to land on. Its own
                    // targets are not stored here any more: they are the
                    // resolved connections of its "PlayAnimation" OUTPUT plug
                    // in m_graph, which is where retail keeps them too.
                    //
                    // "One Shot" latches the ENTITY, not the clip -
                    // VuPlayAnimationEntity_onTriggerPlug @0x5f5e80 sets the
                    // byte at this+168 on the first trigger and returns early
                    // ever after, so a one-shot action fires once per load.
                    if ( !here.empty( ) )
                    {
                        action_t act{};
                        act.one_shot = play.one_shot;
                        m_actions.emplace( here, act );
                    }
                }
            }
        }

        if ( components != nullptr )
        {
            const value_t* model = find( *components, "Vu3dDrawStaticModelComponent" );
            const value_t* properties = model != nullptr ? find( *model, "Properties" ) : nullptr;
            const value_t* asset = properties != nullptr ? find( *properties, "Model Asset" ) : nullptr;
            if ( asset != nullptr && asset->kind == value_t::kind_t::text && !asset->text.empty( ) )
            {
                float tint[ 3 ] = { 1.0f, 1.0f, 1.0f };
                const value_t* color = find( *properties, "Color" );
                if ( color != nullptr )
                {
                    tint[ 0 ] = number( find( *color, "R" ), 255.0f ) / 255.0f;
                    tint[ 1 ] = number( find( *color, "G" ), 255.0f ) / 255.0f;
                    tint[ 2 ] = number( find( *color, "B" ), 255.0f ) / 255.0f;
                }
                place_model( asset->text, world, 0, -1, tint );
            }

            // Vu3dDrawAnimatedModelComponent was not handled at all, so all
            // 19 instances in Hydroelectric were simply invisible. The
            // component itself names only the model - the clip arrives from a
            // separate VuPlayAnimationEntity, resolved after the walk - so
            // record the instance here and let bind_animations() join them.
            const value_t* animated = find( *components, "Vu3dDrawAnimatedModelComponent" );
            const value_t* animated_props = animated != nullptr ? find( *animated, "Properties" ) : nullptr;
            const value_t* animated_asset = animated_props != nullptr ? find( *animated_props, "Model Asset" ) : nullptr;
            if ( animated_asset != nullptr && animated_asset->kind == value_t::kind_t::text
                && !animated_asset->text.empty( ) )
            {
                animated_t item{};
                item.asset = animated_asset->text;
                item.path = here;
                std::memcpy( item.world, world, sizeof( item.world ) );
                item.tint[ 0 ] = item.tint[ 1 ] = item.tint[ 2 ] = 1.0f;
                const value_t* color = find( *animated_props, "Color" );
                if ( color != nullptr )
                {
                    item.tint[ 0 ] = number( find( *color, "R" ), 255.0f ) / 255.0f;
                    item.tint[ 1 ] = number( find( *color, "G" ), 255.0f ) / 255.0f;
                    item.tint[ 2 ] = number( find( *color, "B" ), 255.0f ) / 255.0f;
                }
                item.distance = number( find( *animated_props, "Draw Distance" ), 0.0f );
                m_animated.push_back( std::move( item ) );
            }
        }

        const value_t* children = data != nullptr ? find( *data, "ChildEntities" ) : nullptr;
        if ( children != nullptr && children->kind == value_t::kind_t::list )
        {
            for ( const value_t& child : children->list )
            {
                // Hand this child its instance override, if the template
                // instance above named one, and take the map out of scope for
                // the child's own subtree - the override map is one level
                // deep, keyed by the template root's direct child names.
                const value_t* child_name = find( child, "name" );
                const value_t* saved_overrides = m_child_overrides;
                m_pending_override = m_child_overrides != nullptr && child_name != nullptr
                    && child_name->kind == value_t::kind_t::text
                    ? find( *m_child_overrides, child_name->text.c_str( ) ) : nullptr;
                m_child_overrides = nullptr;
                visit( child, world, true, nullptr, here );
                m_child_overrides = saved_overrides;
                // A direct child reports a rotator it created exactly once.
                if ( m_pending_rotator >= 0 )
                {
                    rotator = m_pending_rotator;
                    m_pending_rotator = -1;
                }
            }
        }

        // A motion entity is registered under the "Motion" category and sits
        // as a CHILD of the thing it drives - every VuRotateMotionEntity in
        // Hydroelectric is a "RotateMotion1" beneath a "WarningSiren" - and it
        // can arrive through a template expansion, so it is only knowable
        // after the subtree has been walked. Only the DIRECT parent adopts:
        // letting every ancestor adopt made one rotator swallow 1918
        // placements instead of its siren's 2.
        m_collect = saved_collect;
        m_place_owner = saved_owner;
        if ( rotator >= 0 && !mine )
        {
            adopt_rotator( rotator, collected );
        }
        else if ( saved_collect != nullptr )
        {
            // Nothing here drives rotation, so hand the placements up to the
            // enclosing collector - a driven prop may sit several levels deep.
            saved_collect->insert( saved_collect->end( ), collected.begin( ), collected.end( ) );
        }
        // Report upward: this entity's own rotator if it IS the motion entity,
        // otherwise nothing (a child's has already been consumed above).
        m_pending_rotator = mine ? rotator : -1;
    }

    // Record a rotator's driven placements, each as a matrix RELATIVE to the
    // rotator's base, so the per-frame update is base' * relative.
    //
    // WHICH placements is decided by the motion entity's "Entity" REF, not by
    // the subtree it sits in. VuMotionEntity_ctor @0x61a000 declares that ref
    // and VuMotionEntity_onActivate @0x61a4f0 drives exactly what it resolves
    // to. Every rotator on Hydroelectric names ONE child node
    // ("WarningSiren/SirenLight01"), so adopting the whole subtree span the
    // siren's housing as well - the ref is what caught it.
    void adopt_rotator( int rotator, const std::vector< collected_t >& collected )
    {
        rotator_t& motion = m_rotators[ static_cast< std::size_t >( rotator ) ];
        const std::string target = ref_target( motion.path, "Entity" );
        float inverse[ 16 ]{};
        rigid_inverse( inverse, motion.base );
        int taken = 0;
        for ( const collected_t& item : collected )
        {
            // No ref at all: keep the old subtree behaviour rather than
            // silently driving nothing, and say so below.
            if ( !target.empty( ) && !owned_by( item.owner, target ) )
                continue;
            std::array< float, 16 > relative{};
            math_multiply( relative.data( ), inverse, item.world.data( ) );
            motion.placements.push_back( item.handle );
            motion.relative.push_back( relative );
            ++taken;
        }
        LOG_VERBOSE( "rotator %s drives %d of %u collected placements%s",
            motion.path.c_str( ), taken, static_cast< unsigned >( collected.size( ) ),
            target.empty( ) ? " (NO Entity ref - whole subtree)" : "" );
        if ( taken == 0 && !collected.empty( ) )
            LOG_WARNING( "rotator %s drives NOTHING: its ref target %s owns none of the %u placements collected",
                motion.path.c_str( ), target.c_str( ),
                static_cast< unsigned >( collected.size( ) ) );
    }

    // Is this placement's owner the ref target, or something beneath it?
    static bool owned_by( const std::string& owner, const std::string& target )
    {
        if ( owner == target )
            return true;
        return owner.size( ) > target.size( )
            && owner.compare( 0, target.size( ), target ) == 0
            && owner[ target.size( ) ] == '/';
    }

    // Resolve one of an entity's refs to a full entity path.
    //
    // A ref's authored value comes in TWO forms, both measured off the asset:
    // an ABSOLUTE full path (VuAttachEntity's Parent and Child), and a
    // TEMPLATE-RELATIVE one (every VuRotateMotionEntity authors
    // "WarningSiren/SirenLight01" while living at ".../WarningSiren1/
    // RotateMotion1"). In the second form the first segment is the TEMPLATE's
    // name and the rest hangs off the owner's parent; dropping that segment
    // resolved to a real entity for 13 of 13 instances, which is why the rule
    // is this and not a search.
    std::string ref_target( const std::string& owner, const std::string& name ) const
    {
        const auto found = m_refs.find( std::make_pair( owner, name ) );
        if ( found == m_refs.end( ) )
            return std::string( );
        const std::string& authored = found->second;
        if ( m_entity_type.count( authored ) != 0 )
            return authored;
        const std::size_t slash = owner.rfind( '/' );
        const std::size_t inner = authored.find( '/' );
        if ( slash == std::string::npos || inner == std::string::npos )
            return authored;
        const std::string relative = owner.substr( 0, slash ) + authored.substr( inner );
        return m_entity_type.count( relative ) != 0 ? relative : authored;
    }

    // Transliterated from VuRotateMotionEntity_tick @0x5d1090. The angle
    // integration, the wrap constants and the Rodrigues matrix are exactly
    // the shipped arithmetic; the compose order comes from
    // VuMatrix_rotateAxisAngle_pre/_post, which differ ONLY in the argument
    // order given to VuMatrix_multiply.
    void update_rotators( float dt )
    {
        for ( rotator_t& motion : m_rotators )
        {
            if ( motion.placements.empty( ) )
                continue;

            const float x = dt * motion.speed + motion.angle + 3.1415927f;
            const float magnitude = std::fabs( x );
            const float wrapped =
                ( magnitude - static_cast< float >( static_cast< int >( magnitude * 0.15915494f ) ) * 6.2831855f )
                - 3.1415927f;
            motion.angle = x < 0.0f ? -wrapped : wrapped;

            const float c = std::cos( motion.angle );
            const float s = std::sin( motion.angle );
            const float t = 1.0f - c;
            const float ax = motion.axis[ 0 ];
            const float ay = motion.axis[ 1 ];
            const float az = motion.axis[ 2 ];

            float rotation[ 16 ]{};
            rotation[ 0 ] = t * ax * ax + c;
            rotation[ 1 ] = t * ax * ay + s * az;
            rotation[ 2 ] = t * ax * az - s * ay;
            rotation[ 4 ] = t * ax * ay - s * az;
            rotation[ 5 ] = t * ay * ay + c;
            rotation[ 6 ] = t * ay * az + s * ax;
            rotation[ 8 ] = t * ax * az + s * ay;
            rotation[ 9 ] = t * ay * az - s * ax;
            rotation[ 10 ] = t * az * az + c;
            rotation[ 15 ] = 1.0f;

            float posed[ 16 ]{};
            if ( motion.local )
                math_multiply( posed, motion.base, rotation );
            else
                math_multiply( posed, rotation, motion.base );

            for ( std::size_t index = 0; index < motion.placements.size( ); ++index )
            {
                float world[ 16 ]{};
                math_multiply( world, posed, motion.relative[ index ].data( ) );
                c_engine::move( motion.placements[ index ], world );
            }
        }
    }

    // VuOffsetAttachComponent_update @0x5af900, the whole body:
    //
    //   childWorld = relative * parentWorld
    //   childLinear = parentLinear + cross( relative.translation, parentAngular )
    //   childAngular = parentAngular
    //
    // Only the first line has anywhere to land here: nothing in this tree
    // carries a per-entity velocity yet, so the two velocity lines are a named
    // gap rather than a guess. The ANIMATED variant @0x61e010 inserts the
    // parent's node matrix between the two - every attach on this level
    // resolves to the offset component (the census prints "no node"), so that
    // path is still unwritten.
    void update_attaches( )
    {
        for ( const attach_t& join : m_attaches )
        {
            if ( !join.attached )
                continue;
            const std::string parent = ref_target( join.path, "Parent" );
            const std::string child = ref_target( join.path, "Child" );
            const auto seat = m_entity_world.find( parent );
            if ( seat == m_entity_world.end( ) )
                continue;

            // The relative matrix, exactly as VuAttachEntity_doAttach
            // @0x5f5550 builds it: identity, multiplied by
            // VuMatrix_fromEuler("Relative Rotation"), then the three
            // translation slots overwritten with "Relative Position" - which
            // is what math_transform composes, with the rotation in degrees
            // because the property is a VuRotation3dProperty.
            const float scale[ 3 ] = { 1.0f, 1.0f, 1.0f };
            float relative[ 16 ]{};
            math_transform( relative, join.position, join.rotation, scale );
            float placed[ 16 ]{};
            math_multiply( placed, seat->second.data( ), relative );
            if ( m_coronas.owns( child ) )
                m_coronas.place( child, placed );
        }
    }

    // Inverse of a rigid transform: transpose the basis and negate the
    // rotated translation. Exact for the rotation-plus-translation matrices
    // the entity walk produces.
    static void rigid_inverse( float* out, const float* m )
    {
        math_identity( out );
        for ( int row = 0; row < 3; ++row )
            for ( int column = 0; column < 3; ++column )
                out[ column * 4 + row ] = m[ row * 4 + column ];
        for ( int row = 0; row < 3; ++row )
            out[ 12 + row ] = -( out[ row ] * m[ 12 ]
                + out[ 4 + row ] * m[ 13 ] + out[ 8 + row ] * m[ 14 ] );
    }


    // One end of an event-graph edge. Edges are recorded on BOTH ends, so the
    // direction comes from the plug's own meaning (In and Reset are inputs,
    // Out and OnExit are outputs) rather than from where it was stored.
    // Declared up here because collect_plug takes it by reference, and a
    // nested type must be declared before it appears in a parameter list.
    struct edge_t
    {
        std::string entity;
        std::string plug;
    };

    // A VuRetVal - what a plug handler returns. Retail's is a type tag plus a
    // union; the only tag we have seen produced is 3, which
    // VuBoolean_onGet @0x5e57f0 writes along with the bool, and which
    // VuIfEntity_onTriggerPlug @0x5dde60 checks for before it branches. The
    // tag is carried rather than assumed so a later type cannot be silently
    // read as a bool. Declared here because fire() and dispatch() return it.
    struct retval_t
    {
        int type = 0;
        bool value = false;
    };


    // ---- the event graph -------------------------------------------------
    //
    // RETAIL MODEL, and it is small. VuScriptOutputPlug_fire @0x5023a0 is the
    // WHOLE dispatcher in 138 bytes, reached as vtable slot 2 through the
    // thunk @0x503270 after VuScriptComponent_findPlug @0x504770 looks the
    // plug up by name. Connections are resolved ONCE at load - sub_503d70 and
    // sub_503ec0 read Plugs / Connections / RefConnections, sub_502720
    // resolves each EntityName + PlugName pair - and firing then just walks
    // the resolved list calling slot 2 on every entry. this[20] / this[21]
    // are the array bounds.
    //
    // this[9] == 0 is a BROADCAST: call every connection and discard the
    // results. Non-zero calls ONLY THE FIRST and propagates its return value.
    // this[9] is set from the THIRD ARGUMENT of VuScriptOutputPlug_init
    // @0x502080, so it is the plug's declared value type and only a
    // constructor can tell us which plugs have one - see
    // plug_returns_value() below.
    //
    // DIRECTION IS NOT IN THE DATA. The authored file records a connection on
    // BOTH ends, so an input plug carries a mirrored entry pointing back at
    // its source. Which end is which comes from the owning entity's
    // CONSTRUCTOR: an INPUT plug is built by VuScriptInputPlug_init @0x5e28f0
    // (which binds a VuMethod1 handler at plug+96), an OUTPUT plug by
    // VuScriptOutputPlug_init. Every classification below cites the ctor that
    // settled it, and because we only ever fire names those ctors prove to be
    // outputs, the mirrored input-side entries are never walked.
    void collect_graph( const std::string& owner, const value_t* components )
    {
        const value_t* script = components != nullptr
            ? find( *components, "VuScriptComponent" ) : nullptr;
        if ( script == nullptr )
            return;
        // THE REFS. A VuScriptComponent carries more than plugs: every
        // VuMotionEntity declares a VuScriptRef named "Entity"
        // (VuMotionEntity_ctor @0x61a000 stores the literal at ref+12), and
        // VuMotionEntity_onActivate @0x61a4f0 drives whatever that ref
        // resolves to. VuAttachEntity's 30 instances need the same thing.
        // The authored side is read by sub_503d70 / sub_503ec0 as
        // "Refs" / "RefConnections"; this probe prints the real shape once so
        // the parser is written against the data rather than against a guess.
        // THE REFS, measured off the asset rather than guessed: a
        // VuScriptComponent's "Refs" node is a flat map from the ref's NAME to
        // the FULL PATH of the entity it points at, as one text value.
        //
        //   Refs { "Child":  "Hydroelectric/Audio/AudioEmitter_Helicopter",
        //          "Parent": "Hydroelectric/Terrain/zone06 .../Helicopter" }
        //
        // A ref is declared in a constructor as a VuScriptRef carrying that
        // name - VuMotionEntity_ctor @0x61a000 builds one called "Entity" and
        // VuMotionEntity_onActivate @0x61a4f0 drives whatever it resolves to.
        // This is the block that VuAttachEntity's 30 instances need, and the
        // reason the windmills have no propellers.
        const value_t* refs = find( *script, "Refs" );
        if ( refs != nullptr && refs->kind == value_t::kind_t::object )
        {
            for ( const auto& named : refs->object )
            {
                if ( named.second.kind != value_t::kind_t::text || named.second.text.empty( ) )
                    continue;
                m_refs.emplace( std::make_pair( owner, named.first ), named.second.text );
            }
        }
        const value_t* plugs = find( *script, "Plugs" );
        if ( plugs == nullptr || plugs->kind != value_t::kind_t::object )
            return;
        for ( const auto& named : plugs->object )
        {
            const value_t* links = find( named.second, "Connections" );
            if ( links == nullptr || links->kind != value_t::kind_t::list || links->list.empty( ) )
                continue;
            std::vector< edge_t > edges;
            for ( const value_t& link : links->list )
            {
                const value_t* entity = find( link, "EntityName" );
                const value_t* port = find( link, "PlugName" );
                if ( entity == nullptr || entity->kind != value_t::kind_t::text || entity->text.empty( ) )
                    continue;
                edge_t edge{};
                edge.entity = entity->text;
                if ( port != nullptr && port->kind == value_t::kind_t::text )
                    edge.plug = port->text;
                edges.push_back( std::move( edge ) );
            }
            if ( edges.empty( ) )
                continue;
            // An entity reached through a template expansion can be walked
            // more than once; keeping the first resolution stops one plug
            // accumulating the same connection twice and firing twice.
            m_graph.emplace( std::make_pair( owner, named.first ), std::move( edges ) );
        }
    }

    // Which plugs carry a VALUE. this[9] in VuScriptOutputPlug_fire is set
    // from the THIRD ARGUMENT of VuScriptOutputPlug_init @0x502080
    // (`this[9] = a3`), and every ctor read so far passes 0 except two:
    //
    //   VuIfEntity_ctor      @0x5ddbe0   "Operand", 3
    //   VuBooleanEntity_ctor @0x5e5880   "Get",     3
    //
    // and 3 is exactly the tag VuBoolean_onGet returns. So this[9] is the
    // plug's RETURN TYPE, 0 meaning void - which is what selects the
    // dispatcher's two halves: a void plug broadcasts to every connection and
    // discards the results, a typed plug calls ONLY THE FIRST and propagates
    // its return value. Nothing in the authored data says which is which;
    // only the constructor does, so this list grows one ctor at a time.
    static bool plug_returns_value( const std::string& type, const std::string& plug )
    {
        return ( type == "VuIfEntity" && plug == "Operand" )
            || ( type == "VuBooleanEntity" && plug == "Get" );
    }

    // VuScriptOutputPlug_fire @0x5023a0, both halves.
    retval_t fire( const std::string& owner, const std::string& plug, int depth = 0 )
    {
        const auto found = m_graph.find( std::make_pair( owner, plug ) );
        if ( found == m_graph.end( ) || found->second.empty( ) )
            return retval_t{};
        const auto type = m_entity_type.find( owner );
        if ( type != m_entity_type.end( ) && plug_returns_value( type->second, plug ) )
            return dispatch( found->second.front( ).entity, found->second.front( ).plug, depth + 1 );
        // The handler may edit m_graph-adjacent state but never m_graph
        // itself, so iterating the resolved list directly is safe.
        for ( const edge_t& edge : found->second )
            dispatch( edge.entity, edge.plug, depth + 1 );
        return retval_t{};
    }

    // Slot 2 on an INPUT plug: invoke the VuMethod1 its entity's constructor
    // bound to it. Each branch below is one such handler, transliterated.
    // DEVIATION, FLAGGED: the depth guard. Retail has none - VuOneShotFilter's
    // In handler even sets its latch AFTER firing Out, so a cyclic graph
    // recurses forever there too - and nothing in the authored graph is known
    // to be cyclic, so the guard only bounds a bug.
    retval_t dispatch( const std::string& entity, const std::string& plug, int depth = 0 )
    {
        if ( depth > 8 )
            return retval_t{};

        // VuPlayAnimationEntity. INPUT plug "Trigger", built by
        // VuPlayAnimationEntity_initTriggerPlug @0x5f6240 and handled by
        // VuPlayAnimationEntity_onTriggerPlug @0x5f5e80:
        //
        //   if ( !this->done )                       // byte +168
        //   {
        //       if ( this->oneShot ) this->done = 1; // byte +157
        //       if ( animationAsset->loaded )        // *(ref+76)
        //           fire( "PlayAnimation", params );
        //   }
        //   return VuRetVal( 0 );
        //
        // The params it builds carry the asset, Start Time (+144), Blend Time
        // (+148), Time Factor (+152) and Looping (+156): the PROP owns none of
        // that, which is why our play_t is indexed by the target's path and
        // copied onto the prop in bind_animations.
        // VuAttachEntity's two input plugs, @0x5f5550 and @0x5f5690. Attach
        // resolves both refs, builds the relative matrix and hands it to the
        // parent's attach component; Detach calls the component's slot 13.
        // Both return VuRetVal( 0 ). A second Attach on a claimed child is
        // refused by the component, which is what `attached` models.
        for ( attach_t& join : m_attaches )
        {
            if ( join.path != entity )
                continue;
            if ( plug == "Attach" )
            {
                join.attached = true;
                update_attaches( );
                return retval_t{};
            }
            if ( plug == "Detach" )
            {
                join.attached = false;
                return retval_t{};
            }
            count_unhandled( entity, plug );
            return retval_t{};
        }

        // VuCoronaEntity's two input plugs. @0x5e8be0 and @0x5e8c30 register
        // and unregister the corona's draw component with the global
        // Vu3dDrawManager, guarded by the same +240 flag, and both return
        // VuRetVal( 0 ). A redundant Show or Hide does nothing at all.
        if ( m_coronas.owns( entity ) )
        {
            if ( plug == "Show" )
            {
                m_coronas.show( entity );
                return retval_t{};
            }
            if ( plug == "Hide" )
            {
                m_coronas.hide( entity );
                return retval_t{};
            }
            count_unhandled( entity, plug );
            return retval_t{};
        }

        // VuSetFrontEndCameraEntity. "Trigger" is its ONE input plug, named
        // inside the init helper @0x409570 which binds
        // VuMethod1<VuSetFrontEndCameraEntity,VuRetVal,VuParams const&> to the
        // handler @0x408f90. That handler, entire:
        //
        //   if ( this->modifyEyePosition )    { params.add( eye    ); broadcast( "OnSetFrontEndCameraEyePosition",    params ); }
        //   if ( this->modifyTargetPosition ) { params.add( target ); broadcast( "OnSetFrontEndCameraTargetPosition", params ); }
        //   if ( this->modifyFarPlane )       { params.add( far    ); broadcast( "OnSetFrontEndCameraDrawDistance",   params ); }
        //                                       params.add( lerp   ); broadcast( "OnSetFrontEndCameraInterpolation",  params );
        //   return VuRetVal( 0 );
        //
        // Four separate engine-wide broadcasts, each name hashed with FNV-1a
        // (basis 0x811c9dc5, prime 16777619) and each carrying exactly ONE
        // parameter - the param count is re-zeroed before every add, so the
        // buffer is reused but never accumulates. The interpolation broadcast
        // is UNCONDITIONAL: there is no Modify boolean for it.
        //
        // COVERAGE GAP, NAMED: the four subscribers live on the front-end
        // camera, which needs the menus. Nothing in the build hooks
        // "OnSetFrontEndCamera*" yet, so the eye, target and far plane are
        // applied straight to the scene viewer - OUR vantage, not retail's -
        // and Interpolation Time is carried but not acted on, because no
        // interpolation code has been read.
        const auto shot = m_front_end.find( entity );
        if ( shot != m_front_end.end( ) )
        {
            if ( plug == "Trigger" )
            {
                const front_end_camera_t& set = shot->second;
                if ( set.modify_eye )
                    std::memcpy( m_eye, set.eye, sizeof( m_eye ) );
                if ( set.modify_target )
                    std::memcpy( m_target, set.target, sizeof( m_target ) );
                if ( set.modify_far )
                    m_far = set.far_plane;
                if ( set.modify_eye || set.modify_target || set.modify_far )
                    m_authored_camera = true;
                ++m_fired;
                LOG_SUCCESS( "event: %s set the front end camera  eye %s target %s far %s  interpolation %.3f (carried, not applied)",
                    entity.c_str( ),
                    set.modify_eye ? "yes" : "no",
                    set.modify_target ? "yes" : "no",
                    set.modify_far ? "yes" : "no",
                    set.interpolation_time );
                return retval_t{};
            }
            count_unhandled( entity, plug );
            return retval_t{};
        }

        const auto action = m_actions.find( entity );
        if ( action != m_actions.end( ) )
        {
            if ( plug == "Trigger" )
            {
                if ( action->second.done )
                    return retval_t{};
                if ( action->second.one_shot )
                    action->second.done = true;
                fire( entity, "PlayAnimation", depth );
                return retval_t{};
            }
            count_unhandled( entity, plug );
            return retval_t{};
        }

        // The receiving end of a "PlayAnimation" edge: VuAnimatedPropEntity's
        // INPUT plug "PlayAnimation", bound in its ctor @0x5ec300 to the
        // handler @0x5ecb60. That handler builds a NEW VuAnimationControl on
        // every call, so a prop that is already playing restarts - the old
        // `|| item.playing` skip here was invented. It then:
        //
        //   control+28 = Start Time; advance( 0 )          a no-op
        //   control+24 = Time Factor; +20 = Looping; advance( 0 )
        //   Blend Time <= 0 ? clear the skeleton's controls @0x517440
        //                   : this+126 = 1, this+128 = 1/Blend, weight 0
        //   add the control @0x5173d0; +36 = timed events; +40 = the prop
        //   advance( sixth param )                         0 from this sender
        //   fire( "OnAnimStart" )
        //
        // UNKNOWN, not reproduced: Blend Time. Its weight ramp lives in the
        // prop's tick @0x5ed030 and the pose blend in @0x516cf0; with a single
        // control the blend normalises by the control's own weight, so it
        // only shows as the bind pose until the first tick - but the case of
        // a prop with a previous clip still fading has not been read.
        bool started = false;
        for ( animated_t& item : m_animated )
        {
            if ( item.path != entity )
                continue;
            item.playing = true;
            // control.end / control.loop are the clip's own members, so they
            // are read from the clip where it is advanced, in skin_props().
            item.control.looping = item.looping;
            item.control.factor = item.factor;
            item.control.time = item.start;
            const auto timed = m_timed.find( item.timed );
            item.control.events = timed != m_timed.end( ) ? &timed->second : nullptr;
            ++m_fired;
            started = true;
            fire( entity, "OnAnimStart", depth );
            // The distance proves the ordering: a trigger that only fires
            // after the player has travelled cannot have fired at t = 0 on
            // the start grid.
            LOG_SUCCESS( "event: %s . %s fired after %.0f units driven, %s begins its clip",
                entity.c_str( ), plug.c_str( ), m_triggers.driven( ), item.asset.c_str( ) );
        }
        if ( started )
            return retval_t{};

        // VuPointWaveEntity. "Emit" is the INPUT plug, named inside
        // VuPointWaveEntity_initEmitPlug @0x5d4e60; "OnEmit" and "OnExpired"
        // are outputs the entity fires, so only "Emit" may start a ripple.
        const auto ripple = m_ripple_sources.find( entity );
        if ( ripple != m_ripple_sources.end( ) )
        {
            if ( plug == "Emit" )
            {
                const c_ocean::ripple_t& source = ripple->second;
                m_ocean.emit( source.x, source.y, source.falloff, source.magnitude,
                    source.range_start, source.range_end, source.speed, source.frequency,
                    source.foaminess );
                ++m_fired;
                LOG_SUCCESS( "event: %s emitted a point wave at %.0f %.0f  range %.0f speed %.0f",
                    entity.c_str( ), source.x, source.y, source.range_end, source.speed );
                return retval_t{};
            }
            count_unhandled( entity, plug );
            return retval_t{};
        }

        // VuOneShotFilterEntity, all three handlers over the byte at this+96.
        const auto latch = m_filters.find( entity );
        if ( latch != m_filters.end( ) )
        {
            // VuOneShotFilter_onResetPlug @0x5decc0: this[96] = 0.
            if ( plug == "Reset" )
            {
                latch->second.fired = false;
                return retval_t{};
            }
            // VuOneShotFilter_onSetShotPlug @0x5dece0: this[96] = 1. It arms
            // the latch as ALREADY SHOT, so a later "In" does nothing - the
            // opposite of Reset, and nothing in the name says so.
            if ( plug == "SetShot" )
            {
                latch->second.fired = true;
                return retval_t{};
            }
            // VuOneShotFilter_onInPlug @0x5df000: if the byte is clear, fire
            // "Out" and THEN set it. The order is retail's, not a tidy-up.
            if ( plug == "In" )
            {
                if ( latch->second.fired )
                    return retval_t{};
                fire( entity, "Out", depth );
                latch->second.fired = true;
                return retval_t{};
            }
            count_unhandled( entity, plug );
            return retval_t{};
        }

        // VuTriggerEntity's own two INPUT plugs, declared by its ctor
        // @0x61c6a0 through VuScriptInputPlug_create @0x61cd90 and shared by
        // the plane and the box. VuTriggerEntity_onActivate @0x61ccb0 and
        // _onDeactivate @0x61cd20 flip the byte at this+132, which is what
        // puts the trigger into (or takes it out of) the manager's active
        // list; a trigger that is not in the list is never tested. Both guard
        // on the current state, so a redundant Activate does nothing.
        if ( plug == "Activate" || plug == "Deactivate" )
        {
            if ( m_triggers.set_active( entity, plug == "Activate" ) )
                return retval_t{};
        }
        else
        {
            for ( const trigger_t& gate : m_triggers.all( ) )
            {
                if ( gate.path != entity )
                    continue;
                count_unhandled( entity, plug );
                return retval_t{};
            }
        }

        // VuDelayEntity. VuDelayEntity_onInPlug @0x5dfbe0:
        //
        //   if ( !this->pending )               // byte +104
        //   {
        //       this->pending  = 1;
        //       this->remaining = rand01() * randomWindow + delay;  // +108
        //       copy the incoming params into this+112 for the Out
        //   }
        //   return VuRetVal( 0 );
        //
        // A delay already counting down IGNORES a second In - the retrigger
        // is dropped, not restarted, and that is not what the name suggests.
        const auto wait = m_delays.find( entity );
        if ( wait != m_delays.end( ) )
        {
            if ( plug == "In" )
            {
                if ( !wait->second.pending )
                {
                    wait->second.pending = true;
                    wait->second.remaining = m_random.draw( ) * wait->second.window
                        + wait->second.delay;
                }
                return retval_t{};
            }
            count_unhandled( entity, plug );
            return retval_t{};
        }

        // VuGenericEventEntity. The "Broadcast" input handler @0x5f0330 posts
        // the global "OnGenericEvent" carrying this entity's Event Name, and
        // every instance's hook @0x5f0420 fires its own "OnReceived" when the
        // carried name equals its own. Walking the name map IS that
        // comparison; the sender matches itself too, exactly as retail does.
        const auto event = m_events.find( entity );
        if ( event != m_events.end( ) )
        {
            if ( plug == "Broadcast" )
            {
                for ( const auto& pair : m_events )
                {
                    if ( pair.second == event->second )
                        fire( pair.first, "OnReceived", depth );
                }
                return retval_t{};
            }
            count_unhandled( entity, plug );
            return retval_t{};
        }

        // VuIfEntity, ctor @0x5ddbe0. INPUT "Trigger" (named in the init
        // helper @0x5e2670, handler VuIfEntity_onTriggerPlug @0x5dde60),
        // OUTPUTS "Operand" (return type 3), "True" and "False".
        //
        //   ret = fire( "Operand" );
        //   if ( ret.type == 3 ) fire( ret.value ? "True" : "False", params );
        //
        // The TYPE CHECK IS LOAD-BEARING: an Operand that answers with
        // anything other than a bool - including an unconnected Operand -
        // takes neither branch. It does not fall through to False.
        if ( m_ifs.count( entity ) != 0 )
        {
            if ( plug == "Trigger" )
            {
                const retval_t answer = fire( entity, "Operand", depth );
                if ( answer.type == 3 )
                    fire( entity, answer.value ? "True" : "False", depth );
                return retval_t{};
            }
            count_unhandled( entity, plug );
            return retval_t{};
        }

        // VuBooleanEntity, ctor @0x5e5880. One property, "Value", on the byte
        // at +96. INPUTS "Set", "SetTrue" (@0x5e5790), "SetFalse" (@0x5e57b0),
        // "Toggle" (@0x5e57d0), "Get" (@0x5e57f0, return type 3); OUTPUTS
        // "OnChanged", "OnChangedTrue", "OnChangedFalse".
        const auto boolean = m_booleans.find( entity );
        if ( boolean != m_booleans.end( ) )
        {
            // VuBoolean_onGet: returns VuRetVal{ 3, value }. This is the only
            // handler in the engine so far that answers rather than acts.
            if ( plug == "Get" )
            {
                retval_t answer{};
                answer.type = 3;
                answer.value = boolean->second;
                return answer;
            }
            if ( plug == "SetTrue" )
                return set_boolean( entity, true, depth );
            if ( plug == "SetFalse" )
                return set_boolean( entity, false, depth );
            // VuBoolean_onToggle: setValue( value == 0 ).
            if ( plug == "Toggle" )
                return set_boolean( entity, !boolean->second, depth );
            // "Set" takes its value from the incoming VuParams, which our
            // events do not carry. Counted, not guessed - and nothing in
            // Hydroelectric is wired to it.
            count_unhandled( entity, plug );
            return retval_t{};
        }

        // Everything else in the 813-node graph - keyframe motion, audio
        // emitters, pfx emitters, the other wave emitters - is not built yet.
        // Count it and say so once, so the gap stays visible instead of
        // events vanishing silently.
        count_unhandled( entity, plug );
        return retval_t{};
    }

    // VuBooleanEntity_setValue @0x5e5ea0. It fires NOTHING when the value is
    // unchanged - the whole body sits inside `if ( a2 != this[96] )` - and
    // when it does change it fires "OnChanged" first and then exactly one of
    // "OnChangedTrue" / "OnChangedFalse".
    retval_t set_boolean( const std::string& entity, bool value, int depth )
    {
        const auto found = m_booleans.find( entity );
        if ( found == m_booleans.end( ) || found->second == value )
            return retval_t{};
        found->second = value;
        fire( entity, "OnChanged", depth );
        fire( entity, value ? "OnChangedTrue" : "OnChangedFalse", depth );
        return retval_t{};
    }

    // VuDelayEntity's countdown. VuDelayEntity_tick @0x5dfb50 is the method
    // registered under "OnManualTick": it unpacks dt from the params - the
    // first entry, and only when its type tag is 2, zero otherwise - and
    // calls VuDelayEntity_update @0x5dfc40, which is exactly
    //
    //   if ( pending ) { remaining -= dt;
    //                    if ( remaining < 0.0f ) { pending = false; fire( "Out" ); } }
    //
    // STRICTLY less than zero, so a zero-length delay still costs one tick.
    // Firing inside the sweep is retail's own order: a delay restarted by an
    // Out still counts down this frame if it is later in the walk. The walk
    // is ours though - retail's is the tick registration (load) order and
    // ours is alphabetical by path - so two delays wired to each other can
    // resolve a frame apart. FLAGGED, no constant involved.
    void update_delays( float dt )
    {
        for ( auto& pair : m_delays )
        {
            if ( !pair.second.pending )
                continue;
            pair.second.remaining -= dt;
            if ( pair.second.remaining >= 0.0f )
                continue;
            pair.second.pending = false;
            fire( pair.first, "Out" );
        }
    }

    void count_unhandled( const std::string& entity, const std::string& plug )
    {
        if ( ++m_unhandled_events == 1 )
        {
            LOG_WARNING( "event %s . %s has no handler yet; further unhandled events are counted only",
                entity.c_str( ), plug.c_str( ) );
        }
    }

    // VuAnimatedPropEntity's VuAnimationEventIF slot 0 @0x5ed680 (the IDB
    // calls it `..._load_EventName`, which is wrong). The control's walk calls
    // it with each timed event's name and VUJB params, and with AnimDone /
    // AnimLoop and no params when the clip ends or wraps.
    void on_animation_event( const std::string& owner, const std::string& name, const value_t* params )
    {
        // The first test compares min( length, 8 ) bytes against "AnimDone"
        // and then the lengths, so it is an exact match.
        if ( name == "AnimDone" )
        {
            fire( owner, "OnAnimDone" );
            return;
        }
        if ( name == "AnimLoop" )
        {
            fire( owner, "OnAnimLoop" );
            return;
        }
        if ( name == "BroadcastGenericEvent" )
        {
            // json "EventName", then the same global "OnGenericEvent" post as
            // VuGenericEventEntity's Broadcast @0x5f0330 - every instance whose
            // Event Name matches fires OnReceived. Retail posts only when the
            // name is non-empty (v26[4], the string's length).
            const value_t* named = params != nullptr ? find( *params, "EventName" ) : nullptr;
            const std::string carried = named != nullptr && named->kind == value_t::kind_t::text
                ? named->text : std::string( );
            if ( carried.empty( ) )
                return;
            for ( const auto& pair : m_events )
            {
                if ( pair.second == carried )
                    fire( pair.first, "OnReceived" );
            }
            return;
        }
        // UNBUILT, named rather than guessed: PlayAudioEvent (FMOD event at
        // json Pos through the prop's matrix and scale), PlayPfxModelSpace
        // (json PfxName/Pos/Rot), Show and Hide (the prop's vtable slots 12
        // and 13, @0x5ecf90 / @0x5ecfe0, unread). Any other name does nothing
        // in retail either.
        if ( name == "PlayAudioEvent" || name == "PlayPfxModelSpace" || name == "Show" || name == "Hide" )
            count_unhandled( owner, name );
    }

    // Does an event sent to this plug reach the named prop? A side-effect-free
    // mirror of dispatch(), for the wiring check below - the bench has no
    // input, so the player never moves and never crosses a plane, which means
    // a bench run alone can never exercise any of this.
    bool reaches( const std::string& entity, const std::string& plug,
        const std::string& prop, int depth = 0 ) const
    {
        if ( depth > 8 )
            return false;
        if ( entity == prop )
            return true;
        const auto action = m_actions.find( entity );
        if ( action != m_actions.end( ) )
            return plug == "Trigger" && walks( entity, "PlayAnimation", prop, depth );
        const auto latch = m_filters.find( entity );
        if ( latch != m_filters.end( ) )
            return plug == "In" && walks( entity, "Out", prop, depth );
        if ( m_delays.count( entity ) != 0 )
            return plug == "In" && walks( entity, "Out", prop, depth );
        // An If takes one branch or the other, so either reaching the prop
        // makes it reachable.
        if ( m_ifs.count( entity ) != 0 )
            return plug == "Trigger" && ( walks( entity, "True", prop, depth )
                || walks( entity, "False", prop, depth ) );
        if ( m_booleans.count( entity ) != 0 )
        {
            if ( plug != "SetTrue" && plug != "SetFalse" && plug != "Toggle" )
                return false;
            return walks( entity, "OnChanged", prop, depth )
                || walks( entity, "OnChangedTrue", prop, depth )
                || walks( entity, "OnChangedFalse", prop, depth );
        }
        const auto event = m_events.find( entity );
        if ( event != m_events.end( ) && plug == "Broadcast" )
        {
            for ( const auto& pair : m_events )
            {
                if ( pair.second == event->second && walks( pair.first, "OnReceived", prop, depth ) )
                    return true;
            }
        }
        return false;
    }

    bool animated_at( const std::string& path ) const
    {
        for ( const animated_t& item : m_animated )
        {
            if ( item.path == path )
                return true;
        }
        return false;
    }

    // What the resolved graph actually holds, printed once per load. This is
    // the measurement that says which plug names the unbuilt node kinds use -
    // names that must be read, never guessed - and whether anything at all is
    // wired to the entities we do handle. The nine point waves are called out
    // by name because each one needs an "Emit" to arrive from somewhere.
    void audit_graph( ) const
    {
        std::map< std::string, int > waiting;
        for ( const auto& pair : m_graph )
        {
            for ( const edge_t& edge : pair.second )
            {
                const bool known = m_actions.count( edge.entity ) != 0
                    || m_filters.count( edge.entity ) != 0
                    || m_ripple_sources.count( edge.entity ) != 0
                    || m_delays.count( edge.entity ) != 0
                    || m_events.count( edge.entity ) != 0
                    || m_ifs.count( edge.entity ) != 0
                    || m_booleans.count( edge.entity ) != 0
                    || animated_at( edge.entity );
                if ( known )
                    continue;
                const auto type = m_entity_type.find( edge.entity );
                ++waiting[ ( type == m_entity_type.end( ) ? std::string( "?" ) : type->second )
                    + " . " + edge.plug ];
            }
        }
        // Can anything actually reach each point wave? The bench never drives
        // far enough to cross the planes these hang off, so reachability is
        // the only check available without playing the course, and it is the
        // one that exercises the new node kinds end to end.
        int reachable = 0;
        for ( const auto& source : m_ripple_sources )
        {
            std::string via;
            for ( const trigger_t& gate : m_triggers.all( ) )
            {
                if ( walks( gate.path, "OnExit", source.first, 0 )
                    || walks( gate.path, "OnEnter", source.first, 0 ) )
                {
                    via = gate.path;
                    break;
                }
            }
            for ( std::size_t i = 0; via.empty( ) && i < m_initialized.size( ); ++i )
            {
                if ( walks( m_initialized[ i ], "Trigger", source.first, 0 ) )
                    via = m_initialized[ i ];
            }
            if ( !via.empty( ) )
            {
                ++reachable;
                LOG_SUCCESS( "point wave %s is reached from %s",
                    source.first.c_str( ), via.c_str( ) );
                continue;
            }
            // Unreached: walk the chain BACKWARDS and print it, so the one
            // missing node kind is named instead of guessed at. Connections
            // are stored on both ends, so the reverse walk is the same map.
            LOG_WARNING( "point wave %s is reached from nothing built; upstream chain:",
                source.first.c_str( ) );
            std::set< std::pair< std::string, std::string > > seen;
            std::vector< std::pair< std::string, std::string > > front;
            front.emplace_back( source.first, "Emit" );
            for ( int depth = 0; depth < 6 && !front.empty( ); ++depth )
            {
                std::vector< std::pair< std::string, std::string > > next;
                for ( const auto& node : front )
                {
                    for ( const auto& pair : m_graph )
                    {
                        for ( const edge_t& edge : pair.second )
                        {
                            if ( edge.entity != node.first || edge.plug != node.second )
                                continue;
                            if ( !seen.insert( pair.first ).second )
                                continue;
                            const auto type = m_entity_type.find( pair.first.first );
                            LOG_WARNING( "   %*s%s . %s   [%s]", depth * 2, "",
                                pair.first.first.c_str( ), pair.first.second.c_str( ),
                                type == m_entity_type.end( ) ? "?" : type->second.c_str( ) );
                            next.push_back( pair.first );
                        }
                    }
                }
                front.swap( next );
            }
        }
        LOG_INFO( "point waves: %d of %u reachable through the built node kinds",
            reachable, static_cast< unsigned >( m_ripple_sources.size( ) ) );
        // The refs, grouped by the owning class and ref name, so the census
        // says which constructors have to be read to consume them.
        std::map< std::string, int > by_ref;
        for ( const auto& pair : m_refs )
        {
            const auto type = m_entity_type.find( pair.first.first );
            ++by_ref[ ( type == m_entity_type.end( ) ? std::string( "?" ) : type->second )
                + " . " + pair.first.second ];
        }
        for ( const auto& pair : by_ref )
            LOG_INFO( "refs: %4d %s", pair.second, pair.first.c_str( ) );
        // THE ROTATOR AUDIT. update_rotators adopts the placements of the
        // subtree the motion entity SITS IN - an inference made before the
        // Refs block was parsed. VuMotionEntity_onActivate @0x61a4f0 drives
        // the "Entity" ref instead, so the two can now be compared: the
        // inference is sound exactly when the ref names an ANCESTOR of the
        // motion entity's own path.
        int absolute = 0, relative = 0, unresolved = 0, missing = 0;
        for ( const rotator_t& motion : m_rotators )
        {
            const auto ref = m_refs.find( std::make_pair( motion.path, "Entity" ) );
            if ( ref == m_refs.end( ) )
            {
                ++missing;
                LOG_WARNING( "rotator %s has no Entity ref", motion.path.c_str( ) );
                continue;
            }
            const std::string target = ref_target( motion.path, "Entity" );
            if ( m_entity_type.count( target ) == 0 )
            {
                ++unresolved;
                LOG_WARNING( "rotator %s: ref \"%s\" resolves to nothing",
                    motion.path.c_str( ), ref->second.c_str( ) );
            }
            else if ( target == ref->second )
                ++absolute;
            else
                ++relative;
        }
        LOG_INFO( "rotator refs: %u rotators - %d absolute, %d template-relative, %d unresolved, %d without a ref",
            static_cast< unsigned >( m_rotators.size( ) ),
            absolute, relative, unresolved, missing );
        // THE CORONA CENSUS. The two draw branches we could not read out of
        // our own renderer - the screen-space sprite size and the roll's
        // screen-x term - only matter for coronas that author them, so say how
        // many do instead of leaving the substitution unmeasured. A corona
        // with no Texture Name has nothing to draw and is a parse failure, not
        // a feature gap.
        int screen_space = 0;
        int rolling = 0;
        int spinning = 0;
        int untextured = 0;
        int hidden = 0;
        for ( const c_corona_entity::corona_t& light : m_coronas.lights( ) )
        {
            if ( light.screen_space )
                ++screen_space;
            if ( light.rotation_amount != 0.0f )
                ++rolling;
            if ( light.rotation_speed != 0.0f )
                ++spinning;
            if ( light.texture.empty( ) )
                ++untextured;
            if ( !light.visible )
                ++hidden;
        }
        LOG_INFO( "coronas: %d placed - %d screen-space sized, %d roll with screen x, %d sweep a cone, %d start hidden, %d without a Texture Name",
            m_coronas.total( ), screen_space, rolling, spinning, hidden, untextured );
        // Where each one actually sits, with the two numbers that decide
        // whether it is ever on screen. A corona is only PROVEN once one is
        // seen, and the attach children cannot be in the right place until the
        // attach transform lands, so the positions are listed rather than
        // summarised.
        for ( const c_corona_entity::corona_t& light : m_coronas.lights( ) )
            LOG_INFO( "corona %s at %.0f %.0f %.0f - draw %.0f fade %.0f size %.1f cone %.0f deg tex %s",
                light.path.c_str( ), light.world[ 12 ], light.world[ 13 ], light.world[ 14 ],
                light.draw_distance, light.fade_distance, light.size,
                light.cone * 57.29578f, light.texture.c_str( ) );
        // WHAT THE ATTACH TRANSFORM ACTUALLY MOVED, and what it could not.
        // The composition is only as good as the parent matrix it reads, so
        // say how many parents were found, how many children landed somewhere
        // this engine can draw, and - the residual that matters - how many
        // parents are themselves driven by a rotate motion, because
        // m_entity_world holds the AUTHORED transform and a spinning parent
        // would carry its child round with it in retail.
        int joined = 0;
        int no_parent = 0;
        int undrawable = 0;
        int spun_parent = 0;
        for ( const attach_t& join : m_attaches )
        {
            if ( !join.attached )
                continue;
            const std::string parent = ref_target( join.path, "Parent" );
            const std::string child = ref_target( join.path, "Child" );
            if ( m_entity_world.count( parent ) == 0 )
            {
                ++no_parent;
                LOG_WARNING( "attach %s: Parent \"%s\" has no placed transform",
                    join.path.c_str( ), parent.c_str( ) );
                continue;
            }
            for ( const rotator_t& motion : m_rotators )
                if ( ref_target( motion.path, "Entity" ) == parent )
                    ++spun_parent;
            if ( m_coronas.owns( child ) )
            {
                ++joined;
                // Where the seat actually is. A child that lands on the origin
                // means the PARENT subtree carries no transform in the level
                // data, not that the composition is wrong. MEASURED for the
                // thirteen WarningSiren templates: the instance node's data
                // holds only ChildEntities, the template root is a bare
                // VuSceneEntity, and not one entity in the subtree - prop,
                // rotate motion, attach or corona - has a
                // VuTransformComponent. Those sirens have no authored position
                // at all, so something outside the scene file places them.
                const std::array< float, 16 >& seat = m_entity_world.at( parent );
                LOG_INFO( "attach %s: parent \"%s\" at %.0f %.0f %.0f, offset %.1f %.1f %.1f",
                    join.path.c_str( ), parent.c_str( ),
                    seat[ 12 ], seat[ 13 ], seat[ 14 ],
                    join.position[ 0 ], join.position[ 1 ], join.position[ 2 ] );
            }
            else
                ++undrawable;
        }
        LOG_INFO( "attach transform: %d children seated on their parent, %d parents not placed, %d children this engine cannot move yet, %d parents also driven by a rotate motion",
            joined, no_parent, undrawable, spun_parent );
        LOG_INFO( "template overrides: %d children overridden by their instance, %d of them carrying the instance's transform",
            m_overrides_seen, m_overrides_placed );
        for ( const auto& pair : m_override_components )
            LOG_INFO( "  override component %-28s x%d (read from the template, not the instance)",
                pair.first.c_str( ), pair.second );
        // WHAT EACH ATTACH ACTUALLY JOINS. VuAttachEntity's own semantics are
        // read (see memory/audit.md), but whether an attach is VISIBLE here
        // depends on what the Child is: a model's placements can be driven,
        // an audio emitter or a pfx emitter cannot until those subsystems
        // exist. Printing both sides' classes is what decides the scope.
        std::map< std::string, int > joins;
        for ( const attach_t& join : m_attaches )
        {
            const std::string parent = ref_target( join.path, "Parent" );
            const std::string child = ref_target( join.path, "Child" );
            const auto parent_type = m_entity_type.find( parent );
            const auto child_type = m_entity_type.find( child );
            ++joins[ ( child_type == m_entity_type.end( ) ? std::string( "?" ) : child_type->second )
                + " -> " + ( parent_type == m_entity_type.end( ) ? std::string( "?" ) : parent_type->second )
                + ( join.node.empty( ) ? std::string( " (no node)" ) : std::string( " (node)" ) ) ];
        }
        for ( const auto& pair : joins )
            LOG_INFO( "attach: %4d %s", pair.second, pair.first.c_str( ) );
        // Every volume's own outgoing edges. These are the only entry points
        // the player can trip, so what they reach bounds what the graph can
        // ever do without a timed event or a global broadcast.
        for ( const trigger_t& gate : m_triggers.all( ) )
        {
            for ( const char* plug : { "OnEnter", "OnExit" } )
            {
                const auto found = m_graph.find( std::make_pair( gate.path, plug ) );
                if ( found == m_graph.end( ) )
                    continue;
                for ( const edge_t& edge : found->second )
                {
                    const auto type = m_entity_type.find( edge.entity );
                    LOG_INFO( "%s %s . %s -> %s . %s [%s]", gate.box ? "box " : "plane",
                        gate.path.c_str( ), plug, edge.entity.c_str( ), edge.plug.c_str( ),
                        type == m_entity_type.end( ) ? "?" : type->second.c_str( ) );
                }
            }
        }
        for ( const auto& pair : waiting )
            LOG_INFO( "graph: %4d edges land on %s", pair.second, pair.first.c_str( ) );
    }

    // The side-effect-free mirror of fire().
    bool walks( const std::string& owner, const std::string& plug,
        const std::string& prop, int depth ) const
    {
        const auto found = m_graph.find( std::make_pair( owner, plug ) );
        if ( found == m_graph.end( ) )
            return false;
        for ( const edge_t& edge : found->second )
        {
            if ( reaches( edge.entity, edge.plug, prop, depth + 1 ) )
                return true;
        }
        return false;
    }

    // For each prop still waiting on a trigger, confirm some volume actually
    // reaches it through the graph. A prop wired to nothing would sit at its
    // bind pose forever and look merely static rather than broken.
    //
    // This is the wiring half of what used to be one test_triggers; the
    // geometry half moved to c_trigger_set, which can check itself without
    // knowing anything about props or the graph.
    void test_wiring( ) const
    {
        for ( const animated_t& item : m_animated )
        {
            if ( item.playing || item.clip.empty( ) )
                continue;
            bool wired = false;
            std::string via;
            for ( const trigger_t& gate : m_triggers.all( ) )
            {
                if ( !walks( gate.path, "OnExit", item.path, 0 )
                    && !walks( gate.path, "OnEnter", item.path, 0 ) )
                    continue;
                wired = true;
                via = gate.path;
                break;
            }
            // A prop that no plane reaches is not necessarily unwired: most of
            // them hang off the load-time event instead.
            for ( std::size_t i = 0; !wired && i < m_initialized.size( ); ++i )
            {
                if ( !walks( m_initialized[ i ], "Trigger", item.path, 0 ) )
                    continue;
                wired = true;
                via = m_initialized[ i ] + " (project init)";
            }
            if ( wired )
            {
                LOG_SUCCESS( "trigger wiring: %-34s waits on %s",
                    item.asset.c_str( ), via.c_str( ) );
            }
            else
            {
                LOG_WARNING( "trigger wiring: %-34s is reachable from nothing built (%s)",
                    item.asset.c_str( ), item.trigger.c_str( ) );
            }
        }
    }


    // The animated props are already placed, so this only refreshes their
    // vertices. "Start Time" is a phase offset in seconds, which is why four
    // identical pump pistons do not pump in step.
    void skin_props( float dt )
    {
        std::vector< float > palette;
        for ( animated_t& item : m_animated )
        {
            if ( ( item.geometry < 0 && !item.rigid ) || !item.playing )
                continue;
            const auto mesh = m_skinned.find( item.asset );
            const auto bones = m_skeletons.find( item.asset );
            const auto clip = m_clips.find( item.clip );
            if ( mesh == m_skinned.end( ) || bones == m_skeletons.end( ) || clip == m_clips.end( ) )
                continue;

            // VuAnimatedPropEntity's tick @0x5ed030 advances every control
            // (@0x5169e0, verified as control_advance) and then evaluates the
            // pose (@0x516cf0). The listener is the prop's VuAnimationEventIF
            // @0x5ed680: AnimDone and AnimLoop become its outputs.
            //
            // Two things the old hand-written clock did are gone because retail
            // does not do them: a One Shot prop was retired at its last frame
            // (One Shot latches only the VuPlayAnimationEntity, at +168), and
            // Start Time was added as a phase every frame instead of being the
            // control's starting time.
            // VuAnimation's load @0x562250 sets +24 = (frames - 1) / 30 and
            // +28 = frames / 30, multiplying by the float 0.033333335.
            const int frames = clip->second.frames( );
            item.control.end = static_cast< float >( frames - 1 ) * 0.033333335f;
            item.control.loop = static_cast< float >( frames ) * 0.033333335f;
            const std::string owner = item.path;
            control_advance( item.control, dt, true,
                [ this, &owner ]( const std::string& name, const value_t* params )
                {
                    on_animation_event( owner, name, params );
                } );

            // @0x516cf0 picks ONE frame, no interpolation between frames:
            //   ( (int)( frames * time / loop - 0.5 ) + 1 ) % frames
            // so time 0 shows frame 1, not frame 0.
            int frame = 0;
            if ( frames > 0 )
                frame = ( static_cast< int >( static_cast< float >( frames ) * item.control.time
                    / item.control.loop - 0.5f ) + 1 ) % frames;

            // Rigid: fold the single bone's palette matrix into each part's
            // placement. The streamed path computes
            // (world * node) * (palette * vertex); this computes
            // (world * node * palette) * vertex, which is the same transform
            // regrouped, so the picture is identical and the mesh is untouched.
            if ( item.rigid )
            {
                build_palette( palette, bones->second, clip->second, frame, nullptr, 0, 0.0f );
                const std::vector< cached_part_t >& parts = model_for( item.asset );
                for ( std::size_t index = 0;
                    index < item.placements.size( ) && index < parts.size( ); ++index )
                {
                    if ( item.placements[ index ] < 0 )
                        continue;
                    float node[ 16 ]{};
                    float posed[ 16 ]{};
                    math_multiply( node, item.world, parts[ index ].node );
                    math_multiply( posed, node, palette.data( ) );
                    c_engine::move( item.placements[ index ], posed );

                    // The extent test cannot see this path - no vertices are
                    // written - so check the equivalent property on the
                    // matrix. A rigid motion must not change the volume the
                    // mesh occupies, so the determinant of the posed basis has
                    // to match the unposed one. A bad palette scale or a
                    // transposed block shows up here as a ratio far from 1.
                    if ( index != 0 || m_scored.count( item.asset ) != 0 )
                        continue;
                    // Mid-clip here too, for the same reason as the streamed
                    // branch: scoring a rigid prop on its first frame caught
                    // Props/TNT at frame 0 and reported a translation of
                    // 0 0 0, which says nothing about where the clip takes it.
                    const int frames = clip->second.frames( );
                    const int cycle = frames > 0 ? frame % frames : 0;
                    if ( frames > 2 && cycle != frames / 2 )
                        continue;
                    m_scored.insert( item.asset );
                    const float before = determinant( node );
                    const float after = determinant( posed );
                    const float ratio = std::fabs( before ) > 1.0e-6f
                        ? std::fabs( after / before ) : 0.0f;

                    // "A rigid motion preserves volume" is NOT true in
                    // general: Props/TNT/PittsFlyby authors a scale track
                    // running 1.000 to 2.000, so at its half-way frame the
                    // determinant is exactly 8 and the old test called correct
                    // data broken. Compare against the volume the clip's OWN
                    // scale implies instead - that still catches a wrong
                    // composition while accepting an authored scale.
                    const clip_pose_t& pose = clip->second.at( frame, 0 );
                    const model_bone_t& rest = bones->second[ 0 ];
                    const float clip_volume = pose.scale[ 0 ] * pose.scale[ 1 ] * pose.scale[ 2 ];
                    const float bind_volume = rest.scale[ 0 ] * rest.scale[ 1 ] * rest.scale[ 2 ];
                    const float expect = std::fabs( bind_volume ) > 1.0e-6f
                        ? clip_volume / bind_volume : clip_volume;
                    const float error = std::fabs( expect ) > 1.0e-6f
                        ? std::fabs( ratio / expect ) : 0.0f;
                    if ( error > 0.9f && error < 1.1f )
                    {
                        LOG_SUCCESS( "prop %-34s rigid by matrix: determinant x%.3f, clip scale implies x%.3f, translation %.1f %.1f %.1f",
                            item.asset.c_str( ), ratio, expect,
                            posed[ 12 ], posed[ 13 ], posed[ 14 ] );
                    }
                    else
                    {
                        LOG_ERROR( "prop %-34s rigid BROKEN: determinant x%.3f but the clip scale implies x%.3f",
                            item.asset.c_str( ), ratio, expect );
                    }
                }
                continue;
            }
            build_palette( palette, bones->second, clip->second, frame, nullptr, 0, 0.0f );
            const mesh_vertex_t* target = skin_geometry(
                item.geometry, mesh->second, bones->second, palette );

            // The hull's frame-0 round-trip does NOT transfer to props: a prop
            // clip is entitled to start away from its bind pose, and several
            // measurably do. What must hold for any correct pose, whether the
            // clip is a local deformation or a world path, is that it
            // PRESERVES THE MODEL'S SIZE - articulated motion moves a mesh
            // about, it does not inflate it. A mesh flung apart by a bad
            // palette blows the extent up by orders of magnitude, which is
            // exactly how the earlier mis-space showed itself.
            if ( target == nullptr || m_scored.count( item.asset ) != 0 )
                continue;
            // Score MID-CLIP, not on the first streamed frame. A
            // trigger-started prop gets its first frame the instant the event
            // fires, and for a clip whose frame 0 is the bind pose that frame
            // is the bind pose: the rockslide duly reported a perfect extent
            // ratio with 0 of 2002 vertices moved, which proves nothing about
            // motion. Waiting for the half-way frame makes the vertex count
            // meaningful for every prop, however it was started.
            const int cycle = frames > 0 ? frame % frames : 0;
            if ( frames > 2 && cycle != frames / 2 )
                continue;
            m_scored.insert( item.asset );

            float low[ 3 ]{}, high[ 3 ]{}, rest_low[ 3 ]{}, rest_high[ 3 ]{};
            for ( int axis = 0; axis < 3; ++axis )
            {
                low[ axis ] = rest_low[ axis ] = 1.0e30f;
                high[ axis ] = rest_high[ axis ] = -1.0e30f;
            }
            int moved = 0;
            for ( std::size_t v = 0; v < mesh->second.bind.size( ); ++v )
            {
                const mesh_vertex_t& rest = mesh->second.bind[ v ];
                for ( int axis = 0; axis < 3; ++axis )
                {
                    const float posed = target[ v ].position[ axis ];
                    if ( posed < low[ axis ] ) low[ axis ] = posed;
                    if ( posed > high[ axis ] ) high[ axis ] = posed;
                    if ( rest.position[ axis ] < rest_low[ axis ] ) rest_low[ axis ] = rest.position[ axis ];
                    if ( rest.position[ axis ] > rest_high[ axis ] ) rest_high[ axis ] = rest.position[ axis ];
                }
                const float dx = target[ v ].position[ 0 ] - rest.position[ 0 ];
                const float dy = target[ v ].position[ 1 ] - rest.position[ 1 ];
                const float dz = target[ v ].position[ 2 ] - rest.position[ 2 ];
                if ( dx * dx + dy * dy + dz * dz > 1.0e-4f )
                    ++moved;
            }
            float span = 0.0f;
            float rest_span = 0.0f;
            for ( int axis = 0; axis < 3; ++axis )
            {
                const float a = high[ axis ] - low[ axis ];
                const float b = rest_high[ axis ] - rest_low[ axis ];
                span += a * a;
                rest_span += b * b;
            }
            span = std::sqrt( span );
            rest_span = std::sqrt( rest_span );
            const float ratio = rest_span > 0.001f ? span / rest_span : 0.0f;
            // A rigid or articulated pose keeps the diagonal within a small
            // factor. Anything outside this is a broken palette, not a pose.
            if ( ratio > 0.5f && ratio < 3.0f )
            {
                LOG_SUCCESS( "prop %-34s posed: extent %.1f vs bind %.1f (x%.2f), %d of %u vertices moved",
                    item.asset.c_str( ), span, rest_span, ratio, moved,
                    static_cast< unsigned >( mesh->second.bind.size( ) ) );
            }
            else
            {
                LOG_ERROR( "prop %-34s BROKEN: extent %.1f vs bind %.1f (x%.2f) - the palette is wrong",
                    item.asset.c_str( ), span, rest_span, ratio );
            }
        }
    }

    void wipe_scene( )
    {
        for ( int alpha = 0; alpha <= 11; ++alpha )
            c_engine::drop( alpha );
        m_pfx.clear( );
        m_ocean.clear( );
        m_draws = 0;
        m_stack.clear( );
        m_sky.clear( );
        m_animated.clear( );
        m_play.clear( );
        m_scored.clear( );
        m_triggers.clear( );
        m_filters.clear( );
        m_actions.clear( );
        m_delays.clear( );
        m_events.clear( );
        m_initialized.clear( );
        m_ifs.clear( );
        m_attaches.clear( );
        m_refs.clear( );
        m_booleans.clear( );
        m_random = random_t{};
        m_graph.clear( );
        m_ripple_sources.clear( );
        m_unhandled_events = 0;
        m_fired = 0;
        // The light rig and the asset substitutions are per scene, so they
        // must be dropped with it or a second level would inherit the first's.
        m_rotators.clear( );
        m_coronas.clear( );
        m_entity_world.clear( );
        m_overrides_seen = 0;
        m_overrides_placed = 0;
        m_override_components.clear( );
        m_collect = nullptr;
        m_pending_rotator = -1;
        m_rig.clear( );
        m_type_census.clear( );
        m_authored_keys.clear( );
        m_entity_type.clear( );
    }

    // Join the animated prop instances to the clips the event graph points at
    // them. Actions and props sit in unrelated parts of the tree and either
    // may be visited first, so the join has to happen after the whole walk.
    void bind_animations( )
    {
        // --stream-rigid-props forces the single-bone props back through CPU
        // skinning, so the saving from the placement-matrix path is a measured
        // A/B rather than an assertion.
        m_rigid_props = c_engine::option( "stream-rigid-props" ) == nullptr;
        const char* named = c_engine::option( "drive-to" );
        m_triggers.watch( named, c_engine::option( "drive" ) != nullptr || ( named != nullptr && named[ 0 ] != '\0' ) );
        int bound = 0;
        int loaded = 0;
        std::map< std::string, int > by_trigger;
        for ( animated_t& item : m_animated )
        {
            const auto found = m_play.find( item.path );
            if ( found == m_play.end( ) )
                continue;
            ++bound;
            item.clip = "VuAnimationAsset/" + found->second.clip;
            item.trigger = found->second.trigger;
            item.start = found->second.start;
            item.looping = found->second.looping;
            item.factor = found->second.factor;
            if ( !found->second.timed.empty( ) )
            {
                item.timed = "VuTimedEventAsset/" + found->second.timed;
                if ( m_timed.find( item.timed ) == m_timed.end( ) )
                {
                    std::vector< std::uint8_t > bytes;
                    std::vector< timed_event_t > events;
                    if ( m_archive.read( item.timed.c_str( ), bytes ) && timed_events_load( bytes, events ) )
                        m_timed.emplace( item.timed, std::move( events ) );
                    else
                        LOG_WARNING( "timed events %s failed to load", item.timed.c_str( ) );
                }
            }
            ++by_trigger[ item.trigger ];

            if ( m_clips.find( item.clip ) == m_clips.end( ) )
            {
                std::vector< std::uint8_t > bytes;
                c_clip clip{};
                if ( m_archive.read( item.clip.c_str( ), bytes ) && clip.load( bytes ) )
                    m_clips.emplace( item.clip, std::move( clip ) );
                else
                    LOG_WARNING( "prop clip %s failed to parse", item.clip.c_str( ) );
            }
            const auto clip = m_clips.find( item.clip );
            if ( clip == m_clips.end( ) || !clip->second.valid( ) )
                continue;
            ++loaded;
            // Cross-check the two unrelated parsers against each other, the
            // same way the boat clips are checked: the clip's bone count must
            // agree with the skeleton read from the model file.
            model_for( item.asset );
            const auto skeleton = m_skeletons.find( item.asset );
            if ( skeleton != m_skeletons.end( )
                && clip->second.bones( ) != static_cast< int >( skeleton->second.size( ) ) )
            {
                LOG_WARNING( "prop %s clip has %d bones but model %s has %u",
                    item.path.c_str( ), clip->second.bones( ), item.asset.c_str( ),
                    static_cast< unsigned >( skeleton->second.size( ) ) );
            }

            // A CHECK THE STILLS CANNOT MAKE. Run a copy of this prop's control
            // over one pass of its clip at the bench step, offline - nothing is
            // fired - and count what the walk would deliver: every timed event
            // name, and for BroadcastGenericEvent how many VuGenericEventEntity
            // receivers carry the name. An event the clip never reaches, or a
            // broadcast nobody listens for, shows up here at load.
            const auto timed = m_timed.find( item.timed );
            if ( timed != m_timed.end( ) )
            {
                animation_control_t probe{};
                const int frames = clip->second.frames( );
                probe.end = static_cast< float >( frames - 1 ) * 0.033333335f;
                probe.loop = static_cast< float >( frames ) * 0.033333335f;
                // As authored: a looping prop is probed for exactly one loop, a
                // one-shot until its AnimDone. Forcing every probe one-shot
                // misreported the pistons - one starting at 15s, past its
                // 1.933s end, wraps rather than finishing.
                probe.looping = item.looping;
                probe.factor = item.factor;
                probe.time = item.start;
                probe.events = &timed->second;
                const float step_time = 0.016666667f;
                const float rate = std::fabs( item.factor ) * step_time;
                const int limit = !item.looping ? 100000
                    : rate > 0.0f ? static_cast< int >( std::ceil( probe.loop / rate ) ) : 0;
                int fires = 0;
                int heard = 0;
                int deaf = 0;
                bool done = false;
                for ( int step = 0; step < limit && !done; ++step )
                {
                    control_advance( probe, step_time, true,
                        [ & ]( const std::string& name, const value_t* params )
                        {
                            if ( name == "AnimDone" )
                            {
                                done = true;
                                return;
                            }
                            if ( name == "AnimLoop" )
                                return;
                            ++fires;
                            if ( name != "BroadcastGenericEvent" || params == nullptr )
                                return;
                            const value_t* carried = find( *params, "EventName" );
                            int receivers = 0;
                            for ( const auto& pair : m_events )
                            {
                                if ( carried != nullptr && pair.second == carried->text )
                                    ++receivers;
                            }
                            receivers > 0 ? ++heard : ++deaf;
                        } );
                }
                LOG_SUCCESS( "timed events %s on %s: %d fires from %u events over one %s (end %.3fs, start %.3fs), %d broadcasts heard, %d with no receiver",
                    item.timed.c_str( ), item.path.c_str( ), fires,
                    static_cast< unsigned >( timed->second.size( ) ),
                    item.looping ? "loop" : "pass", probe.end, item.start, heard, deaf );
            }
        }
        // Place them. These 19 were invisible before, because the walk only
        // ever handled Vu3dDrawStaticModelComponent.
        //
        // A prop never moves - only its vertices deform - so unlike a boat it
        // does not need re-placing every frame. Each animatable instance gets
        // its own cloned buffer, placed ONCE here, and the frame loop just
        // streams new vertices into it. That keeps every part's real material
        // tag (the tag doubles as the render-state selector AND the per-frame
        // drop group, so there is no spare one to borrow) and costs no
        // placement work per frame.
        // FIRST, decide which clips are even skin deformations.
        //
        // Does a prop clip's frame 0 equal its bind pose? For the boats that
        // was MEASURED (41 of 41 chained, worst 0.0000). For props it would
        // only be an assumption. Measured here, they split in two:
        //
        //   PumpSpin_skinned   2 of 2 bones identity, worst   0.0000
        //   PumpPiston_skinned 0 of 3 bones identity, worst 120.8034
        //   Helicopter_AS-350  0 of 3 bones identity, worst  21.9291
        //   HanggliderA        0 of 1 bones identity, worst 2138.0303
        //   HanggliderB        0 of 1 bones identity, worst 2174.8982
        //
        // A clip that merely started mid-stroke would be off by a few units -
        // the hull's own mismatch was 3.74. Thousands of units is a different
        // kind of thing: those clips hold an ABSOLUTE WORLD TRANSFORM, moving
        // the entity through the level (the hanggliders fly, the helicopter
        // hovers) rather than deforming the mesh. Skinning them against the
        // inverse bind pose would fling the geometry across the map.
        //
        // So only stream the ones that really are deformations. The rest stay
        // at their bind pose until the world-path case is reversed properly -
        // a static prop is honest, a prop flung 2000 units is not.
        std::set< std::string > deformable;
        std::set< std::string > scored;
        for ( const animated_t& item : m_animated )
        {
            if ( item.clip.empty( ) || !scored.insert( item.asset ).second )
                continue;
            const auto bones = m_skeletons.find( item.asset );
            const auto clip = m_clips.find( item.clip );
            if ( bones == m_skeletons.end( ) || clip == m_clips.end( ) || !clip->second.valid( ) )
                continue;
            if ( clip->second.bones( ) != static_cast< int >( bones->second.size( ) ) )
                continue;
            const std::vector< model_bone_t >& bind = bones->second;
            std::vector< float > chained;
            build_palette( chained, bind, clip->second, 0, nullptr, 0, 0.0f );
            // build_palette already folds in inverse(bind), so if frame 0 is
            // the bind pose every matrix is the identity. Scoring the matrices
            // is the same quantity the vertex round-trip measures, obtained
            // before anything is streamed.
            int identity = 0;
            float worst = 0.0f;
            for ( std::size_t b = 0; b < bind.size( ); ++b )
            {
                const float* m = chained.data( ) + b * 16;
                float off = 0.0f;
                for ( int cell = 0; cell < 16; ++cell )
                {
                    const float want = cell % 5 == 0 ? 1.0f : 0.0f;
                    const float delta = std::fabs( m[ cell ] - want );
                    if ( delta > off )
                        off = delta;
                }
                if ( off > worst )
                    worst = off;
                if ( off < 0.001f )
                    ++identity;
            }
            // Frame 0 need NOT equal the bind pose, and demanding it was the
            // wrong test. Printing the three candidate spaces side by side
            // settled what these clips are:
            //
            //   HanggliderA  placement t (0,0,0)       clip f0 (-288, -2138, 55)
            //   Airplane_Pitts placement t (0,0,0)     clip f0 ( 317, -1598, 64)
            //     -> the entity has NO transform of its own; the clip carries
            //        world coordinates and IS the object's world path. Because
            //        the placement is the identity, streaming the clip-posed
            //        vertices puts them at the right world position anyway.
            //
            //   Helicopter   bind t (0, -0.69, 3.85)   clip f0 (22.33, -0.69, 3.85)
            //   PumpPiston   bind t (0, 0, 53.00)      clip f0 (0, 0, 49.80)
            //     -> Y and Z match the bind EXACTLY and only the moving axis
            //        differs, so these are ordinary model-local clips that
            //        merely START AWAY FROM BIND. The helicopter patrols in X,
            //        the piston reciprocates in Z from 49.80 to 61.83.
            //
            // So every case is handled by the same stream path; only the
            // acceptance test had to change, from "frame 0 reproduces bind" to
            // "the pose preserves the model's size" (see skin_props).
            const bool at_bind = identity == static_cast< int >( bind.size( ) );
            const clip_pose_t& first = clip->second.at( 0, 0 );
            const bool origin = std::fabs( item.world[ 12 ] ) < 0.01f
                && std::fabs( item.world[ 13 ] ) < 0.01f
                && std::fabs( item.world[ 14 ] ) < 0.01f;
            deformable.insert( item.asset );
            LOG_INFO( "prop %-34s bones %-3u frame 0 palette %d of %u identity (worst %8.2f) %s",
                item.asset.c_str( ), static_cast< unsigned >( bind.size( ) ),
                identity, static_cast< unsigned >( bind.size( ) ), worst,
                at_bind ? "starts at bind"
                    : ( origin ? "world path, placement at origin" : "starts away from bind" ) );
            ( void )first;
        }

        int clones = 0;
        int rigid = 0;
        int streamed = 0;
        for ( animated_t& item : m_animated )
        {
            const auto mesh = m_skinned.find( item.asset );
            const auto bones = m_skeletons.find( item.asset );
            const auto clip = m_clips.find( item.clip );
            // Nothing starts itself any more. Props that move from the first
            // frame are started below by firing every
            // VuOnProjectInitializedEntity's "Trigger" through the graph,
            // which is how retail starts them; the name-sniffing test that
            // used to live here ("does the trigger's path contain
            // OnGameStart") was a guess at that mechanism and is gone.
            //
            // Prepare EVERY bound prop, whether or not its trigger has fired.
            // A gated prop still needs its clone or placement handles ready
            // at load, or the event would arrive with nowhere to put the pose.
            item.playing = false;
            const bool usable = deformable.count( item.asset ) != 0
                && mesh != m_skinned.end( ) && bones != m_skeletons.end( )
                && clip != m_clips.end( ) && clip->second.valid( )
                && clip->second.bones( ) == static_cast< int >( bones->second.size( ) )
                && mesh->second.skin.size( ) == mesh->second.bind.size( );
            // A single-bone clip is a rigid motion, so the placement matrix
            // can carry it: one matrix write per part per frame instead of
            // transforming 1331 to 6604 vertices and re-uploading. These keep
            // the shared cached geometry - no clone, no streaming, and the
            // result is mathematically identical because the final transform
            // is just regrouped from (world*node)*(palette*v) to
            // (world*node*palette)*v.
            if ( m_rigid_props && usable && bones->second.size( ) == 1 )
            {
                item.rigid = true;
                ++rigid;
                place_model( item.asset, item.world, 0, -1, item.tint, nullptr, -1, &item.placements );
                continue;
            }
            if ( usable )
            {
                item.geometry = c_engine::geometry(
                    mesh->second.bind.data( ), static_cast< int >( mesh->second.bind.size( ) ),
                    mesh->second.indices.data( ), static_cast< int >( mesh->second.indices.size( ) ) );
            }
            if ( item.geometry >= 0 )
            {
                ++clones;
                streamed += static_cast< int >( mesh->second.bind.size( ) );
                place_model( item.asset, item.world, 0, -1, item.tint, nullptr, item.geometry );
            }
            else
            {
                place_model( item.asset, item.world, 0, -1, item.tint );
            }
        }
        LOG_INFO( "animated props: %d streaming %d vertices per frame, %d rigid by placement matrix (0 vertices)",
            clones, streamed, rigid );


        if ( m_animated.empty( ) )
            return;
        if ( bound == static_cast< int >( m_animated.size( ) ) )
        {
            LOG_SUCCESS( "animated props: all %d bound to a clip, %d clips parsed",
                bound, loaded );
        }
        else
        {
            LOG_WARNING( "animated props: %d of %u bound to a clip, %d clips parsed",
                bound, static_cast< unsigned >( m_animated.size( ) ), loaded );
        }
        LOG_INFO( "event graph: %u trigger planes, %u one-shot filters, %u play actions, %u resolved plugs",
            static_cast< unsigned >( m_triggers.size( ) ),
            static_cast< unsigned >( m_filters.size( ) ),
            static_cast< unsigned >( m_actions.size( ) ),
            static_cast< unsigned >( m_graph.size( ) ) );
        // "Attach at Start" is the property VuAttachEntity_ctor @0x5f4fa0
        // declares first; the entity attaches itself at load rather than
        // waiting for its "Attach" input plug. Run the composition once here
        // so the audit below reports the result rather than the authored
        // state, and every frame afterwards in case the parent moves.
        for ( attach_t& join : m_attaches )
            if ( join.at_start )
                join.attached = true;
        update_attaches( );
        audit_graph( );
        m_triggers.test_geometry( );
        test_wiring( );
        for ( const auto& pair : by_trigger )
            LOG_INFO( "  trigger %-62s x%d", pair.first.c_str( ), pair.second );
        // The project is built, every prop has its clone or its placement
        // handles, so the load-time event can go out. Retail raises
        // "OnProjectInitialized" once and every
        // VuOnProjectInitializedEntity's hook @0x5efa60 fires its "Trigger"
        // output in response; the graph does the rest.
        for ( const std::string& path : m_initialized )
            fire( path, "Trigger" );
        LOG_INFO( "project initialized: %u entities fired, %d props playing, %d events delivered to a handler, %d not",
            static_cast< unsigned >( m_initialized.size( ) ), playing( ), m_fired, m_unhandled_events );
    }

    int playing( ) const
    {
        int count = 0;
        for ( const animated_t& item : m_animated )
            count += item.playing ? 1 : 0;
        return count;
    }

    // A VuTemplateAsset's whole document, what factory slot 7 0x555690 loads,
    // or null when the pack has none of that name.
    const value_t* template_document( const std::string& name )
    {
        template_root( name );
        const auto found = m_templates.find( name );
        return found != m_templates.end( ) && found->second.tag == 6 ? &found->second : nullptr;
    }

    // The project as retail loads it (the race game 0x4B4360 -> VuProject::load
    // 0x556390): the registry as start-up leaves it, the asset's AssetData
    // "File" path (0x55FBE0), and the entity tree through every recreated load.
    // The coronas are fed from it: each VuCoronaEntity's transform world and its
    // property targets. A corona below an UNKNOWN entity is never created, and
    // one whose world is unknown is not placed - both counted, neither stood in
    // for.
    void load_entity_tree( const char* asset, const value_t& project )
    {
        std::vector< std::uint8_t > bytes;
        const std::string prefix = "VuProjectAsset/";
        std::string path;
        if ( std::strncmp( asset, prefix.c_str( ), prefix.size( ) ) == 0 && m_archive.read( "Assets/AssetData", bytes ) )
        {
            const value_t database = c_reader( bytes ).parse( );
            const value_t& file = member( member( member( database, "VuProjectAsset" ), asset + prefix.size( ) ), "File" );
            if ( file.tag == 4 )
                path = file.text;
        }
        if ( path.empty( ) )
        {
            LOG_ERROR( "entity tree: %s has no AssetData \"File\" - the project is not loaded the retail way", asset );
            return;
        }
        const entity_registry_t registry = build_entity_registry( m_archive.names( "VuTemplateAsset/" ) );
        entity_world_t world;
        world.m_registry = &registry;
        world.m_ctor_for = entity_ctor_for;
        world.m_template = [ this ]( const std::string& name ) { return template_document( name ); };
        world.m_components_for = entity_components_constructed;
        world.m_component_load = entity_load_component;
        world.m_component_mark = entity_mark_component;
        world.m_state_for = entity_state_for;
        world.m_properties_load = entity_load_properties;
        world.m_properties_mark = entity_mark_properties;
        m_tree = entity_load_project( project, path, world );
        if ( !m_tree )
        {
            LOG_ERROR( "entity tree: VuProject::load failed on %s (%s)", asset, path.c_str( ) );
            return;
        }
        int entities = 0;
        int unknown = 0;
        int unplaced = 0;
        int placed = 0;
        // The event graph addresses an entity by its names below the project
        // root, '/'-joined.
        const auto walk = [ & ]( const auto& self, entity_t& e, const std::string& trail ) -> void
        {
            ++entities;
            unknown += e.m_ctor ? 0 : 1;
            if ( e.m_type == "VuCoronaEntity" && e.m_state )
            {
                const transform_component_t* t = entity_transform( e );
                if ( !t || e.m_world_unknown )
                    ++unplaced;
                else
                {
                    m_coronas.add( corona_from( trail, *t, static_cast< const corona_state_t* >( e.m_state.get( ) )->value ) );
                    ++placed;
                }
            }
            for ( auto& child : e.m_children )
                self( self, *child, trail.empty( ) ? child->m_name : trail + "/" + child->m_name );
        };
        walk( walk, *m_tree, std::string( ) );
        LOG_INFO( "entity tree: %s (%s) - %d entities, %d UNKNOWN (nothing below one is loaded); %d coronas placed, %d with an "
                  "unknown world", asset, path.c_str( ), entities, unknown, placed, unplaced );
    }

    // VuCoronaEntity's members as c_corona_entity reads them. The colour is
    // the VuColor the property load packed (R, G, B, A bytes).
    static c_corona_entity::corona_t corona_from( const std::string& path, const transform_component_t& t, const corona_properties_t& c )
    {
        c_corona_entity::corona_t light{};
        light.path = path;
        std::memcpy( light.world, t.world, sizeof( light.world ) );
        light.visible = c.initially_visible != 0;
        light.texture = c.texture_name;
        light.draw_distance = c.draw_distance;
        light.fade_distance = c.fade_distance;
        light.query_radius = c.query_radius;
        light.cone = c.cone;
        light.penumbra = c.penumbra;
        light.back_light = c.back_light != 0;
        for ( int k = 0; k < 4; ++k )
            light.color[ k ] = static_cast< float >( ( c.texture_color >> ( 8 * k ) ) & 0xFF ) / 255.0f;
        light.screen_space = c.screen_space != 0;
        light.size = c.texture_size;
        light.rotation_offset = c.rotation_offset;
        light.rotation_amount = c.rotation_amount;
        light.rotation_speed = c.rotation_speed;
        std::memcpy( light.axis, c.rotation_axis, sizeof( light.axis ) );
        return light;
    }

    void load_project( const char* asset )
    {
        std::vector< std::uint8_t > project;
        if ( asset == nullptr || !m_archive.read( asset, project ) )
        {
            LOG_ERROR( "project %s", asset != nullptr ? asset : "" );
            return;
        }
        const value_t root = c_reader( project ).parse( );
        const value_t* project_node = find( root, "VuProject" );
        const value_t* entity = project_node != nullptr ? find( *project_node, "RootEntity" ) : nullptr;
        if ( entity == nullptr )
        {
            LOG_ERROR( "project %s has no root", asset );
            return;
        }
        float identity[ 16 ]{};
        math_identity( identity );
        visit( *entity, identity );
        load_entity_tree( asset, *project_node );
        m_rig.report( );
        for ( const auto& pair : m_type_census )
            LOG_VERBOSE( "census %-40s %d", pair.first.c_str( ), pair.second );
        // Hand the authored rig to the renderer. Until this runs the engine
        // keeps the old hardcoded lambert's constants in the rig's form, so a
        // scene with no light entities looks exactly as it did before.
        // --no-rig holds it back on purpose: it is the A/B switch that makes
        // the rig's contribution measurable against the old lambert instead
        // of merely plausible.
        if ( m_rig.lit( ) && c_engine::option( "no-rig" ) == nullptr )
            c_engine::rig( m_rig.ambient( ), m_rig.front( ), m_rig.back( ) );
        bind_animations( );
        audit_authored( );
        // The water surfaces and the sky box are counted but not built: the
        // readers that used to run here were partial and the geometry they fed
        // was invented. The count comes from the census, so the load log still
        // states exactly what the level authors and nothing draws.
        const auto water = m_type_census.find( "VuWaterSurfaceEntity" );
        LOG_SUCCESS( "project %s  draws %d  water surfaces %d (none built)  sky %s", asset, m_draws,
            water != m_type_census.end( ) ? water->second : 0,
            m_sky.empty( ) ? "-" : m_sky.c_str( ) );
    }

    std::string skin_texture( const value_t* skins, const char* name ) const
    {
        if ( skins == nullptr || name == nullptr || name[ 0 ] == '\0' )
            return {};
        const value_t* skin = find( *skins, name );
        const value_t* boat_node = skin != nullptr ? find( *skin, "Boat" ) : nullptr;
        const value_t* textures = boat_node != nullptr ? find( *boat_node, "Textures" ) : nullptr;
        const value_t* diffuse = textures != nullptr ? find( *textures, "DiffuseTexture" ) : nullptr;
        if ( diffuse != nullptr && diffuse->kind == value_t::kind_t::text )
            return diffuse->text;
        return {};
    }

    void load( )
    {
        // Retained only as the pack opener. The front end it used to build -
        // the Screens/Background project, its fifteen screen cameras and the
        // blend between them - is banked in reference/shot.hxx.
        const std::string path = pack_path( );
        if ( path.empty( ) || !m_archive.open( path.c_str( ) ) )
            return;
        m_opened = true;
    }

    // SCAFFOLD - ours, no retail counterpart.
    //
    // A vantage for looking at a loaded scene. Retail has nothing like it:
    // every camera in the game is either VuBoatCamera, which needs a boat, or
    // a VuSetFrontEndCameraEntity authored into a screen project, and a race
    // project authors neither. So this orbits the bounding box of whatever the
    // entity walk actually placed. It is a measuring instrument, not a
    // recreation, and no part of it may be taken as retail behaviour.
    void dev_vantage( float dt )
    {
        if ( m_authored_camera )
            return;
        if ( !m_vantage_ready )
        {
            // The MEAN of the placements, not the centre of their bounding box:
            // a level is a long ribbon with outliers at both ends, and the box
            // centre lands in empty space off the course.
            double sum[ 3 ]{};
            for ( const auto& pair : m_entity_world )
                for ( int axis = 0; axis < 3; ++axis )
                    sum[ axis ] += pair.second[ 12 + axis ];
            const double count = m_entity_world.empty( ) ? 1.0 : static_cast< double >( m_entity_world.size( ) );
            for ( int axis = 0; axis < 3; ++axis )
                m_centre[ axis ] = static_cast< float >( sum[ axis ] / count );

            // Deliberately a close vantage. The orbit must sit well inside the
            // fog's range or the fog - which is 1:1 and correct - washes the
            // whole frame to the fog colour and the viewer shows nothing.
            m_radius = 80.0f;
            if ( const char* option = c_engine::option( "vantage" ) )
            {
                const float parsed = static_cast< float >( std::atof( option ) );
                if ( parsed > 1.0f )
                    m_radius = parsed;
            }
            m_vantage_ready = true;
            LOG_INFO( "dev vantage: %u placements, mean %.0f %.0f %.0f, orbit radius %.0f (--vantage <units>)",
                static_cast< unsigned >( m_entity_world.size( ) ),
                m_centre[ 0 ], m_centre[ 1 ], m_centre[ 2 ], m_radius );
        }

        m_orbit += dt * 0.1f;
        m_eye[ 0 ] = m_centre[ 0 ] + std::cos( m_orbit ) * m_radius;
        m_eye[ 1 ] = m_centre[ 1 ] + std::sin( m_orbit ) * m_radius;
        m_eye[ 2 ] = m_centre[ 2 ] + m_radius * 0.4f;
        std::memcpy( m_target, m_centre, sizeof( m_target ) );
        m_far = m_authored_far;
    }

public:
    bool loaded( ) const
    {
        return m_loaded;
    }

    // The project path is built the way the game modes name their projects -
    // Games/<track><mode> - which is read out of the pack's own asset table,
    // not guessed. Everything the mode is supposed to MEAN (grid, rules,
    // scoring, laps) lives in VuGameModeManagerImpl and is not transliterated,
    // so this loads the scene and nothing else.
    void load_scene( const char* track, const char* mode, const char* course = nullptr )
    {
        if ( !m_opened )
            load( );
        const char* name = track != nullptr && track[ 0 ] != '\0' ? track : "Hydroelectric";
        std::string suffix = "Race";
        if ( mode != nullptr )
        {
            if ( std::strstr( mode, "Slalom" ) != nullptr )
            {
                suffix = "Slalom";
                if ( course != nullptr && std::strncmp( course, "Course", 6 ) == 0 )
                    suffix += course;
                else
                    suffix += "CourseA";
            }
            else if ( std::strstr( mode, "TimeTrial" ) != nullptr )
                suffix = "TimeTrial";
            else if ( std::strstr( mode, "Ghost" ) != nullptr )
                suffix = "Ghost";
        }
        const std::string path = std::string( "VuProjectAsset/Games/" ) + name + suffix;
        wipe_scene( );
        m_vantage_ready = false;
        m_authored_camera = false;
        m_authored_far = 500.0f;
        load_project( path.c_str( ) );
        m_loaded = true;
        LOG_SUCCESS( "scene %s%s", name, suffix.c_str( ) );
    }

    // SCAFFOLD - ours. The per-frame order below is NOT retail's frame; retail
    // runs these off VuTickManager phases that have not been transliterated.
    // Every function it calls is 1:1, but the sequence is ours and proves
    // nothing about ordering.
    void scene_frame( float dt )
    {
        if ( !m_loaded )
            return;

        const double ocean_start = trace::now( );
        m_ocean.advance( dt );
        trace::frame( ).ocean += trace::now( ) - ocean_start;

        dev_vantage( dt );
        // The crossing test wants the position of whatever the plane's Trigger
        // Type accepts. With no boat in the scene the dev vantage stands in,
        // so flying the orbit through a plane still exercises the real test.
        m_triggers.update( m_target, [ this ]( const std::string& path, const std::string& plug ) { fire( path, plug ); } );
        update_delays( dt );
        update_rotators( dt );
        update_attaches( );

        const double props_start = trace::now( );
        skin_props( dt );
        trace::frame( ).props += trace::now( ) - props_start;

        const double pfx_start = trace::now( );
        m_pfx.draw( dt, m_eye, m_target, [ this ]( const std::string& path ) { return texture_for( path ); },
            [ this ]( const std::string& path ) -> const std::vector< pfx_part_t >& { return pfx_parts( path ); } );
        m_coronas.tick( dt );
        m_coronas.draw( m_eye, m_target, [ this ]( const std::string& path ) { return texture_for( path ); } );
        trace::frame( ).pfx += trace::now( ) - pfx_start;
    }

private:
    bool m_opened = false;
    bool m_loaded = false;
    // Set when the walk found a real VuSetFrontEndCameraEntity. A race project
    // does not author one, but a screen project does, and an authored vantage
    // always beats the scaffold orbit.
    bool m_authored_camera = false;
    bool m_vantage_ready = false;
    // VuGlobalGfxSettingsEntity's "Camera Far Plane", or its ctor default.
    float m_authored_far = 500.0f;
    float m_centre[ 3 ]{};
    float m_radius = 64.0f;
    float m_orbit = 0.0f;

    c_archive m_archive{};
    c_pfx m_pfx{};
    c_corona_entity m_coronas{};
    c_ocean m_ocean{};
    std::map< std::string, std::vector< cached_part_t > > m_models;
    std::map< std::string, std::vector< pfx_part_t > > m_pfx_models;
    std::map< std::string, std::vector< model_bone_t > > m_skeletons;
    std::map< std::string, c_clip > m_clips;

    // Bind-pose mesh kept for skinned hulls. The cached geometry is shared
    // between every instance of a model, so skinning writes into a per-rider
    // clone and needs the untouched bind vertices to transform from.
    std::map< std::string, skinned_mesh_t > m_skinned;
    std::vector< rotator_t > m_rotators;
    // Every entity's placed world matrix, by path - what an attach's "Parent"
    // ref resolves onto.
    std::map< std::string, std::array< float, 16 > > m_entity_world;
    std::vector< collected_t >* m_collect = nullptr;
    std::string m_place_owner;
    // The template instance override map in scope for the current child loop,
    // and the single override handed to the next visit().
    const value_t* m_child_overrides = nullptr;
    const value_t* m_pending_override = nullptr;
    int m_overrides_seen = 0;
    int m_overrides_placed = 0;
    std::map< std::string, int > m_override_components;
    int m_pending_rotator = -1;
    std::map< std::string, int > m_type_census;
    std::map< std::string, std::set< std::string > > m_authored_keys;
    std::map< std::string, std::string > m_entity_type;
    std::map< std::string, int > m_textures;
    std::map< std::string, int > m_cubes;
    std::string m_sky;
    float m_eye[ 3 ]{};
    float m_target[ 3 ]{};
    float m_far = 800.0f;

    // VuWaterTextureSettingsEntity_ctor @0x5d2bd0, read in property order:
    // Gravity 9.801, WorldSize 10, WindSpeed 10, DirectionalPower 0,
    // SuppressionWaveLength 0, HeightFactor 1, TimeFactor 1,
    // NormalTextureScale 0.5. All four below were invented and all four are
    // reached - Hydroelectric does not author them, so the load log showed the
    // guessed 12 / 2.0 / 15 going straight into the ocean.
    float m_wave_height = 1.0f;
    float m_wave_scale = 0.5f;
    float m_wave_wind = 10.0f;
    float m_wave_size = 10.0f;
    std::map< std::string, value_t > m_templates;
    // The project's entity tree as retail's VuProject::load builds it.
    std::unique_ptr< entity_t > m_tree;
    // Loaded VuTimedEventAssets, keyed by asset name. A control points into
    // this, so entries are only ever added.
    std::map< std::string, std::vector< timed_event_t > > m_timed;
    std::set< std::string > m_stack;

    // One Vu3dDrawAnimatedModelComponent instance. The clip and phase are
    // filled in by bind_animations() from the event graph, not from the
    // component, which names only the model.
    struct animated_t
    {
        std::string asset;
        std::string path;
        std::string clip;
        std::string trigger;
        // "VuTimedEventAsset/<name>", empty when the driver names none.
        std::string timed;
        float world[ 16 ]{};
        float tint[ 3 ]{ 1.0f, 1.0f, 1.0f };
        float distance = 0.0f;
        float start = 0.0f;
        // Its own streamed clone and its own clock, so four pump pistons
        // sharing one model still animate out of step as authored.
        int geometry = -1;
        // The VuAnimationControl the last PlayAnimation built (@0x5ecb60).
        animation_control_t control{};
        // A single-bone clip is a RIGID motion: every vertex rides one
        // matrix, so the placement matrix can carry it and the mesh never has
        // to be touched. These instances keep the SHARED cached geometry and
        // a placement handle per part instead of a clone.
        bool rigid = false;
        std::vector< int > placements;
        // Props on an OnGameStart trigger begin at load; the rest wait for
        // their event to arrive and sit at the bind pose until it does.
        bool playing = false;
        // Copied from the driving VuPlayAnimationEntity - the params its
        // onTriggerPlug @0x5f5e80 packs. One Shot is not among them.
        bool looping = true;
        float factor = 1.0f;
    };

    // A resolved VuPlayAnimationEntity, keyed by the path of the prop it
    // drives.
    struct play_t
    {
        std::string clip;
        std::string timed;
        std::string trigger;
        std::string path;
        float start = 0.0f;
        // Authored on VuPlayAnimationEntity and previously ignored, which
        // made every clip loop forever at full speed. The "Looping" key is
        // written only when it is FALSE - every entity carrying Looping 0 is
        // a one-shot event (the rockslide, the Pitts flyby, the hangglider
        // swoops) and every entity without the key is a continuous motion
        // (the pump, the spin, the helicopter hover, the glider cruise), so
        // the default is true. Two unrelated groupings agreeing.
        bool looping = true;
        bool one_shot = false;
        float factor = 1.0f;
    };

    // VuOneShotFilterEntity: one byte of state, the flag at this+96.
    struct filter_t
    {
        bool fired = false;
    };

    // VuPlayAnimationEntity, keyed by its own path. "One Shot" is the byte at
    // +157 and `done` the latch at +168 that onTriggerPlug sets.
    struct action_t
    {
        bool one_shot = false;
        bool done = false;
    };

    // VuAttachEntity, keyed by its own path. The refs live in m_refs.
    struct attach_t
    {
        std::string path;
        std::string node;
        float position[ 3 ]{};
        // A VuRotation3dProperty, so the authored numbers are DEGREES per axis
        // - confirmed in VuAttachEntity_ctor @0x5f4fa0, where "Relative
        // Position" is a plain VuBasicProperty<VuVector3> and "Relative
        // Rotation" is the rotation class.
        float rotation[ 3 ]{};
        bool at_start = false;
        // The live half. VuOffsetAttachComponent_attach @0x5af7a0 refuses a
        // child that is already claimed (its motion component's slot at +20),
        // so an attach is either in the list or it is not; "attached" is that
        // claim.
        bool attached = false;
    };

    // VuDelayEntity: the two authored floats and the two runtime fields the
    // constructor zeroes, in the same roles - pending at +104, remaining at
    // +108.
    struct delay_t
    {
        float delay = 1.0f;
        float window = 0.0f;
        bool pending = false;
        float remaining = 0.0f;
    };

    // The engine's shared random source, transliterated from sub_50FD20:
    // Park-Miller 16807 * s - 2147483647 * (s / 127773) behind a 32 entry
    // Bays-Durham shuffle, the draw being (float)last * 2^-31 clamped by
    // fminf against 0x3F7FFFFE. The state is NOT seeded by any initialiser -
    // dword_736810 is statically -1 with a zeroed table and zeroed `last` -
    // so the first draws legitimately return 0.0 until the table fills, and
    // reproducing that is the point of copying it exactly.
    //
    // DEVIATION, FLAGGED: retail has ONE of these, shared with the particle
    // spawner and everything else that draws, so the sequence a delay sees
    // depends on a global draw order we do not reproduce. The generator is
    // identical; the stream is ours.
    struct random_t
    {
        std::int32_t seed = -1;
        std::int32_t last = 0;
        std::int32_t table[ 32 ]{};

        float draw( )
        {
            const std::int32_t k = seed / 127773;
            seed = 16807 * seed - 2147483647 * k;
            if ( seed < 0 )
                seed += 2147483647;
            const std::int32_t slot = last / 0x4000000;
            last = table[ slot & 31 ];
            table[ slot & 31 ] = seed;
            const float value = static_cast< float >( last ) * 4.65661287e-10f;
            return value < 0.99999988f ? value : 0.99999988f;
        }
    };

    std::vector< animated_t > m_animated;
    std::map< std::string, play_t > m_play;
    c_trigger_set m_triggers;
    c_light_rig m_rig;
    std::map< std::string, filter_t > m_filters;
    std::map< std::string, action_t > m_actions;
    std::map< std::string, delay_t > m_delays;
    std::map< std::string, front_end_camera_t > m_front_end;
    // Every VuGenericEventEntity's Event Name, which is the whole bus.
    std::map< std::string, std::string > m_events;
    std::vector< std::string > m_initialized;
    std::set< std::string > m_ifs;
    std::vector< attach_t > m_attaches;
    // (entity, refName) -> the full path the ref points at.
    std::map< std::pair< std::string, std::string >, std::string > m_refs;
    std::map< std::string, bool > m_booleans;
    random_t m_random;
    // The crossing sphere's radius. Retail's comes from
    // VuInstigatorComponent+36 via _register @0x531e10 and is written by boat
    // code at 0x41da90 / 0x421670 that has not been read; see gate_distance.
    float m_probe_radius = 0.0f;
    // Every entity's resolved script connections: (entity, plug) -> the
    // (entity, plug) pairs it is wired to. Built once during the entity walk,
    // walked by fire(). This is retail's resolved connection array.
    std::map< std::pair< std::string, std::string >, std::vector< edge_t > > m_graph;
    std::map< std::string, c_ocean::ripple_t > m_ripple_sources;
    // Events delivered to node kinds that are not built yet. Counted rather
    // than guessed at, so the size of the remaining graph stays visible.
    int m_unhandled_events = 0;
    // Assets already scored by the extent test, so it reports once each.
    std::set< std::string > m_scored;
    // Route single-bone props through the placement matrix instead of CPU
    // skinning. Kept switchable so the saving can be measured rather than
    // asserted: --stream-rigid-props forces them back through the vertex path.
    bool m_rigid_props = true;
    // --drive: let the AI controller steer the player boat round the gate
    // chain, so an unattended bench run exercises anything that needs the
    // player to actually move.
    bool m_drive = false;
    // Distance the player has travelled and how many graph events have fired,
    // so a run can show that a trigger fired AFTER motion rather than at t=0.
    float m_driven = 0.0f;
    int m_fired = 0;
    float m_last[ 2 ]{};
    bool m_tracked = false;
    float m_reported = 0.0f;
    int m_ticks = 0;
    std::string m_aim;
    int m_probes = 0;
    int m_draws = 0;
};
