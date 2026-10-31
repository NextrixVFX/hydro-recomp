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

    // pack_path: models.hxx
    std::string pack_path( ) const;

    // texture_for: models.hxx
    int texture_for( const std::string& path );

    // cube_for: models.hxx
    int cube_for( const std::string& path );

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

    // pfx_parts: models.hxx
    const std::vector< pfx_part_t >& pfx_parts( const std::string& asset );

    // model_for: models.hxx
    const std::vector< cached_part_t >& model_for( const std::string& asset );

    // place_model: models.hxx
    void place_model( const std::string& asset, const float* world, int bob = 0, int alpha_override = -1, const float* tint = nullptr, const char* skin = nullptr, int geometry_override = -1, std::vector< int >* out_placements = nullptr );

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
    // The members are VuSetFrontEndCameraEntity's loaded targets (engine
    // entity/actions.hxx, from the retail tree).
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

    // The remaining front-end camera path - the screen-to-screen blend - was
    // inferred and is banked in reference/shot.hxx.

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

        // The script connections and refs come from the retail tree
        // (load_entity_tree; engine entity/script.hxx).

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
                    // map rather than inheriting it - saved and restored here.
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
                    return;
            }
        }

        if ( type != nullptr && type->kind == value_t::kind_t::text && data != nullptr )
        {
            const value_t* properties = find( *data, "Properties" );
            record_authored( type->text, properties );
            record_authored( type->text, overlay );
            // VuSetFrontEndCameraEntity is built from the retail entity tree (load_entity_tree).
            // VuWaterSurfaceEntity is handled by the water work in its own
            // right - it has 14 authored properties and none of the geometry,
            // height resolution or displacement that read them has been
            // transliterated. The partial reader that used to sit here, and
            // the rectangle-overlap height lookup it fed, are banked in
            // reference/water.hxx.
            // VuWaterTextureSettingsEntity feeds the ocean from the retail entity
            // tree (load_entity_tree).
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
            // VuGlobalGfxSettingsEntity feeds the fog and the far plane from the
            // retail entity tree (load_entity_tree).
            // VuFoliageEntity drew a hand-built billboard quad from an invented
            // vertex layout. Deleted; build it from the entity's own draw path.
            // VuStaticPfxEntity is built from the retail entity tree (load_entity_tree).
            // VuSkyBoxEntity is read from the retail entity tree (load_entity_tree).
            // VuAmbientLightEntity / VuDirectionalLightEntity feed the rig from the
            // retail entity tree (load_entity_tree).
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
            // VuTriggerPlaneEntity / VuTriggerBoxEntity, VuOneShotFilterEntity, VuDelayEntity,
            // VuGenericEventEntity, VuOnProjectInitializedEntity, VuIfEntity, VuAttachEntity,
            // VuBooleanEntity, VuCoronaEntity, VuRotateMotionEntity and VuPlayAnimationEntity
            // (its properties AND its plugs' links), the per-class census and the class
            // behind each path come from the retail entity tree (load_entity_tree; engine
            // entity/actions.hxx, entity/script.hxx).
        }

        // The model-draw components (Vu3dDrawStaticModelComponent, Vu3dDrawAnimatedModelComponent) are placed
        // from the retail entity tree (load_entity_tree).

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
            }
        }

    }

    // adopt_rotator: motion.hxx
    void adopt_rotator( int rotator, const std::vector< collected_t >& collected );

    // adopt_rotators: motion.hxx
    void adopt_rotators( std::vector< collected_t > pool );

    // owned_by: motion.hxx
    static bool owned_by( const std::string& owner, const std::string& target );

    // ref_target: motion.hxx
    std::string ref_target( const std::string& owner, const std::string& name ) const;

    // update_rotators: motion.hxx
    void update_rotators( float dt );

    // update_attaches: motion.hxx
    void update_attaches( );

    // rigid_inverse: motion.hxx
    static void rigid_inverse( float* out, const float* m );


    // One end of an event-graph link: the other plug's owner (its path) and name.
    // Every plug's links are held, inputs' as well as outputs' - as retail's +0x50
    // lists are - and only outputs are fired; the ctor-declared direction is in
    // the tree (engine entity/script.hxx).
    // Declared up here because fire() and the graph maps use it.
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


    // plug_returns_value: graph.hxx
    static bool plug_returns_value( const std::string& type, const std::string& plug );

    // fire: graph.hxx
    retval_t fire( const std::string& owner, const std::string& plug, int depth = 0 );

    // dispatch: graph.hxx
    retval_t dispatch( const std::string& entity, const std::string& plug, int depth = 0 );

    // set_boolean: graph.hxx
    retval_t set_boolean( const std::string& entity, bool value, int depth );

    // update_delays: graph.hxx
    void update_delays( float dt );

    // count_unhandled: graph.hxx
    void count_unhandled( const std::string& entity, const std::string& plug );

    // on_animation_event: graph.hxx
    void on_animation_event( const std::string& owner, const std::string& name, const value_t* params );

    // reaches: graph.hxx
    bool reaches( const std::string& entity, const std::string& plug,
        const std::string& prop, int depth = 0 ) const;

    // animated_at: graph.hxx
    bool animated_at( const std::string& path ) const;

    // audit_graph: graph.hxx
    void audit_graph( ) const;

    // walks: graph.hxx
    bool walks( const std::string& owner, const std::string& plug,
        const std::string& prop, int depth ) const;

    // test_wiring: graph.hxx
    void test_wiring( ) const;


    // skin_props: animate.hxx
    void skin_props( float dt );

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
        m_seat_world.clear( );
        m_overrides_seen = 0;
        m_overrides_placed = 0;
        m_override_components.clear( );
        m_collect = nullptr;
        m_adopters.clear( );
        m_rig.clear( );
        m_type_census.clear( );
        m_authored_keys.clear( );
        m_entity_type.clear( );
    }

    // bind_animations: animate.hxx
    void bind_animations( );

    // playing: animate.hxx
    int playing( ) const;

    // template_document: entities.hxx
    const value_t* template_document( const std::string& name );

    // load_entity_tree: entities.hxx
    void load_entity_tree( const char* asset, const value_t& project );

    // corona_from: entities.hxx
    static c_corona_entity::corona_t corona_from( const std::string& path, const transform_component_t& t, const corona_properties_t& c );

    // load_project: entities.hxx
    void load_project( const char* asset );

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
    // Every tree entity's transform world (scale kept out), by path - what an
    // attach's "Parent" ref seats its child on (VuOffsetAttachComponent_update
    // 0x5af900 composes onto the parent's transform world). m_entity_world above
    // (the old walk's, scale baked) now only feeds the dev vantage scaffold.
    std::map< std::string, std::array< float, 16 > > m_seat_world;
    std::vector< collected_t >* m_collect = nullptr;
    std::string m_place_owner;
    // (rotator, the entity that adopts it - the motion entity's direct
    // parent), in visit()'s post-order; consumed by adopt_rotators.
    std::vector< std::pair< int, std::string > > m_adopters;
    // The template instance override map in scope for the current child loop,
    // and the single override handed to the next visit().
    const value_t* m_child_overrides = nullptr;
    const value_t* m_pending_override = nullptr;
    int m_overrides_seen = 0;
    int m_overrides_placed = 0;
    std::map< std::string, int > m_override_components;
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
        // RADIANS: the loaded target - the level authors degrees and the
        // property's filter 0x45CB50 converts (VuAttachEntity_ctor 0x5f4fa0
        // re-tags "Relative Rotation" to that class, 0x73D24C).
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
    // Every plug's resolved links: (entity, plug) -> its +0x50 list in link order,
    // each end by path - from the retail tree (load_entity_tree), walked by fire().
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

#include <workspace/core/world/graph.hxx>
#include <workspace/core/world/motion.hxx>
#include <workspace/core/world/animate.hxx>
#include <workspace/core/world/entities.hxx>
#include <workspace/core/world/models.hxx>
