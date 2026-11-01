#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/entity/registry.hxx>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// The entity tree as VuEntity::load builds it from a project's data. Only the
// members the loader itself reads or writes are named; everything a type's
// constructor puts in its object is outside this file.
//
// A type is KNOWN when its construction facts were captured from retail
// (entity_ctor_t: what its create fn leaves - vtable, the base ctor's flags,
// no ctor-made children, slot 11 = VuEntity::load); its node then loads its
// children exactly as retail does. Any other type is an UNKNOWN node: created
// (retail would create it) and named, but its own load is a hook and nothing
// below it is read. Properties, Components and the post-load calls (vt+8 /
// vt+0xC) are hooks for every type until the classes behind them are recreated.

struct entity_ctor_t
{
    const char* m_class;
    std::uint32_t m_vtable;  // +0x00 after create
    std::uint32_t m_flags;   // +0x14: VuEntity ctor 0x500710's argument (0x50076D)
};

// One component a type's ctor attaches (the +0x48 list, next at +0xC), as
// captured from retail: its getType (vt+0) key - the data's "Components" key -
// and its vtable.
struct entity_component_row_t
{
    const char* m_class;
    const char* m_key;
    std::uint32_t m_vtable;
};

// A component of one entity. What it holds is up to the world's component
// callback (a recreated component keeps its state in m_state); an unrecreated
// one is a hook and holds nothing.
struct entity_component_t
{
    const char* m_key = nullptr;
    std::uint32_t m_vtable = 0;
    std::shared_ptr< void > m_state;
};

struct entity_t
{
    std::string m_type;                                 // the data's "type" it was created from
    std::uint32_t m_create = 0;                         // the registry's create fn (a hook unless recreated)
    const entity_ctor_t* m_ctor = nullptr;              // null: UNKNOWN
    std::uint32_t m_flags = 0;                          // +0x14
    std::string m_name;                                 // +0x18
    entity_t* m_parent = nullptr;                       // +0x34
    std::vector< std::unique_ptr< entity_t > > m_children;  // +0x38
    std::string m_template;                             // +0x4C: the VuTemplateAsset an instance came from
    std::vector< entity_component_t > m_components;     // +0x48, in list order
    // The entity's own properties (its +0x44 nodes' targets and defaults), for
    // a type whose properties are recreated; null otherwise (a hook).
    std::shared_ptr< void > m_state;
    // Set when an ancestor's world changed after this entity's own transform
    // was computed: retail's 0x534C00(1) child walk would then update it, and
    // that walk is not recreated - so this entity's world is unknown.
    bool m_world_unknown = false;
};

struct entity_world_t
{
    const entity_registry_t* m_registry = nullptr;
    const entity_ctor_t* ( *m_ctor_for )( const std::string& cls ) = nullptr;
    // A VuTemplateAsset's document by name (what factory slot 7 0x555690 would load), or null when the
    // asset does not exist.
    std::function< const value_t*( const std::string& name ) > m_template;
    // The components a known type's ctor attaches, in +0x48 order.
    std::function< std::vector< entity_component_t >( const std::string& cls ) > m_components_for;
    // One component's load: its property nodes' vt+8 loads on `properties`,
    // then its post-load (vt+0xC, or vt+0x10 on a template override). Null, or
    // a component it does not know: a hook.
    std::function< void( entity_t& owner, entity_component_t& component, const value_t& properties, bool override ) >
        m_component_load;
    // The component's own post-load after its property loads (vt+0xC, or vt+0x10 on a template override), on its
    // whole data (components[its key]) - what VuScriptComponent's loads read. Null or unknown: a hook.
    std::function< void( entity_t& owner, entity_component_t& component, const value_t& data, bool override ) > m_component_post;
    // The template mark's component step (0x501FE0: each property node's
    // vt+0x1C, then the component's vt+0x20). Null or unknown: a hook.
    std::function< void( entity_t& owner, entity_component_t& component ) > m_component_mark;
    // The entity's own properties: its state as its ctor leaves it, the "Properties" load (each node's vt+8;
    // on a normal load and on a template override alike), and the mark's property step (each node's vt+0x1C).
    std::function< std::shared_ptr< void >( const std::string& cls ) > m_state_for;
    std::function< void( entity_t& entity, const value_t& properties ) > m_properties_load;
    std::function< void( entity_t& entity ) > m_properties_mark;
    // The entity's own vt+8 postLoad (a normal load, 0x5009D0's last call) / vt+0xC (a template override, 0x5015D0's), on
    // the entity's whole data. Null or a class it does not know: a hook.
    std::function< void( entity_t& entity, const value_t& data, bool override ) > m_post_load;
};

// 0x500690, the child order (std::sort's comparator in 0x501810): a child with
// +0x14 bit 0 set before one without; otherwise by name, std::string's
// operator< (0x404AC0 memcmp of the shorter length, then the lengths).
inline bool entity_child_before( const entity_t* a, const entity_t* b )
{
    const bool fa = ( a->m_flags & 1 ) != 0;
    const bool fb = ( b->m_flags & 1 ) != 0;
    if ( fa && !fb )
        return true;
    if ( !fa && fb )
        return false;
    return a->m_name < b->m_name;
}

// findChild 0x500E60: the first child in +0x38 order whose name is `name`.
inline entity_t* entity_find_child( entity_t& entity, const std::string& name )
{
    for ( auto& child : entity.m_children )
        if ( child->m_name == name )
            return child.get( );
    return nullptr;
}

// 0x501810 with 0x500690. SUBSTITUTION: std::sort for MSVC's (library code);
// an UNKNOWN child has no known flags, so a list holding one cannot be ordered
// and is left as created - the caller's tree reports it.
inline bool entity_sort_children( entity_t& entity )
{
    for ( const auto& child : entity.m_children )
        if ( !child->m_ctor )
            return false;
    std::sort( entity.m_children.begin( ), entity.m_children.end( ),
        []( const std::unique_ptr< entity_t >& a, const std::unique_ptr< entity_t >& b )
        { return entity_child_before( a.get( ), b.get( ) ); } );
    return true;
}

inline void entity_load( entity_t& entity, const value_t* data, const entity_world_t& world );

// 0x501F70 (a normal load) / 0x502010 (a template override), over the +0x48
// list in order: each component's data is components[its getType key], its
// property nodes load from that data's "Properties", then its post-load
// (vt+0xC / vt+0x10) on that data.
inline void entity_load_components( entity_t& entity, const value_t& components, const entity_world_t& world, bool override )
{
    for ( entity_component_t& component : entity.m_components )
    {
        const value_t& data = member( components, component.m_key );
        if ( world.m_component_load )
            world.m_component_load( entity, component, member( data, "Properties" ), override );
        if ( world.m_component_post )
            world.m_component_post( entity, component, data, override );
    }
}

// 0x5016A0, run on a fresh template instance, over the whole subtree (the
// instance too): an entity whose +0x4C is empty - every descendant, not the
// instance - gets +0x14 |= 2; then each of its property nodes' vt+0x1C (its
// default becomes the current value - a hook for the entity's own properties
// until they are modelled per entity) and 0x501FE0 over its components.
inline void entity_mark_template( entity_t& entity, const entity_world_t& world )
{
    if ( entity.m_template.empty( ) )
        entity.m_flags |= 2;
    if ( world.m_properties_mark )
        world.m_properties_mark( entity );
    for ( entity_component_t& component : entity.m_components )
        if ( world.m_component_mark )
            world.m_component_mark( entity, component );
    for ( auto& child : entity.m_children )
        entity_mark_template( *child, world );
}

// The registry's create fn for `type`, as far as it is recreated: a captured
// type (its facts), the template instantiator 0x556E70, or an UNKNOWN node.
// Null where retail's create returns null (a template whose asset or root type
// is missing) and where the type is not registered.
inline std::unique_ptr< entity_t > entity_instantiate_template( const std::string& type, const entity_world_t& world );

inline std::unique_ptr< entity_t > entity_create( const std::string& type, const entity_world_t& world )
{
    const std::uint32_t create = world.m_registry->create_for( type );
    if ( !create )
        return nullptr;
    if ( create == k_template_create )
        return entity_instantiate_template( type, world );
    {
        auto entity = std::make_unique< entity_t >( );
        entity->m_type = type;
        entity->m_create = create;
        entity->m_ctor = world.m_ctor_for( type );
        entity->m_flags = entity->m_ctor ? entity->m_ctor->m_flags : 0;
        if ( entity->m_ctor && world.m_components_for )
            entity->m_components = world.m_components_for( type );
        if ( entity->m_ctor && world.m_state_for )
            entity->m_state = world.m_state_for( type );
        return entity;
    }
}

// The template instantiator 0x556E70 itself, for a type "#<name>": the asset is
// the name after the '#' (0x555690); its root type is VuProject.RootEntity.type
// when that is a string, else "" (0x556520).
inline std::unique_ptr< entity_t > entity_instantiate_template( const std::string& type, const entity_world_t& world )
{
    const std::string name = type.substr( 1 );
    const value_t* asset = world.m_template ? world.m_template( name ) : nullptr;
    if ( !asset )
        return nullptr;
    const value_t& root = member( member( *asset, "VuProject" ), "RootEntity" );
    const value_t& root_type = member( root, "type" );
    auto entity = entity_create( root_type.tag == 4 ? root_type.text : std::string( ), world );
    if ( !entity )
        return nullptr;
    // The root type's own vt+0x2C load on the template's data, while +0x4C is still empty.
    entity_load( *entity, &member( root, "data" ), world );
    entity->m_template = name;
    entity_mark_template( *entity, world );
    return entity;
}

// loadChildEntities 0x501300: create every element of `array` that has a
// string "type" and a string "name" (the name -> "data" map keeps the LAST
// element of a name, std::map operator[] 0x501700), unless a child of that
// name already exists (0x500E60) or the create returns null; new children get
// their name, the parent link and a place in the sorted list. Then every
// child - including any that existed before - is loaded from its entry in the
// map (vt+0x2C).
inline void entity_load_children( entity_t& entity, const value_t& array, const entity_world_t& world )
{
    std::map< std::string, const value_t* > data_by_name;
    const int count = array.tag == 5 ? static_cast< int >( array.list.size( ) ) : 0;  // 0x4FBC40
    for ( int index = 0; index < count; ++index )
    {
        const value_t& element = array.list[ index ];
        const value_t& type = member( element, "type" );
        if ( type.tag != 4 )
            continue;
        const value_t& name = member( element, "name" );
        if ( name.tag != 4 )
            continue;
        data_by_name[ name.text ] = &member( element, "data" );
        if ( entity_find_child( entity, name.text ) )
            continue;
        auto child = entity_create( type.text, world );
        if ( !child )
            continue;
        child->m_name = name.text;
        child->m_parent = &entity;
        entity.m_children.push_back( std::move( child ) );
        // parent vt+0x24 (the child-added hook) runs here - not recreated.
        entity_sort_children( entity );
    }
    for ( auto& child : entity.m_children )
    {
        const auto found = data_by_name.find( child->m_name );
        // A child absent from the data gets operator[]'s default (0) and retail
        // passes a null container pointer - not reached by any recreated path.
        entity_load( *child, found == data_by_name.end( ) ? nullptr : found->second, world );
    }
    entity_sort_children( entity );
}

// 0x5015D0, a template instance's load (and, recursively, its descendants'):
// nothing is created. Each existing child takes its override from
// data.ChildEntities when that is an OBJECT keyed by child name (else the null
// container) - recursing into 0x5015D0 itself, whatever the child's type.
inline void entity_load_override( entity_t& entity, const value_t& data, const entity_world_t& world )
{
    static const value_t null{};
    const value_t& overrides = member( data, "ChildEntities" );
    for ( auto& child : entity.m_children )
    {
        const value_t* found = overrides.tag == 6 ? find( overrides, child->m_name ) : nullptr;
        entity_load_override( *child, found ? *found : null, world );
    }
    if ( world.m_properties_load )
        world.m_properties_load( entity, member( data, "Properties" ) );
    entity_load_components( entity, member( data, "Components" ), world, true );
    // The entity's vt+0xC follows.
    if ( world.m_post_load )
        world.m_post_load( entity, data, true );
}

// 0x55AFE0: a project root's name from its file path - the path normalised by
// 0x55ADB0 ('\' -> '/', then one '/' of every "//" removed until none is
// left), cut after its LAST '/', then before its LAST '.'.
inline std::string entity_project_name( const std::string& path )
{
    std::string name = path;
    for ( char& c : name )
        if ( c == '\\' )
            c = '/';
    // The "//" collapse cannot change a base name (the last '/' survives it):
    // verify_load_project does not - cannot - observe this loop.
    for ( std::size_t at = name.find( "//" ); at != std::string::npos; at = name.find( "//" ) )
        name.erase( at, 1 );
    const std::size_t slash = name.rfind( '/' );
    if ( slash != std::string::npos )
        name = name.substr( slash + 1 );
    const std::size_t dot = name.rfind( '.' );
    if ( dot != std::string::npos )
        name = name.substr( 0, dot );
    return name;
}

// VuProject::load 0x556390(data = the document's "VuProject", path = the asset's
// AssetData "File"): the root's type is RootEntity.type when that is a string
// and not empty, else "VuSceneEntity"; 0x5561C0 creates it through the registry
// (null -> the load fails: null here) and names it after the path; it is then
// loaded from RootEntity.data (vt+0x2C). 0x500A60 follows - a hook.
inline std::unique_ptr< entity_t > entity_load_project( const value_t& data, const std::string& path, const entity_world_t& world )
{
    const value_t& root = member( data, "RootEntity" );
    const value_t& type = member( root, "type" );
    std::string cls = type.tag == 4 ? type.text : std::string( );
    if ( cls.empty( ) )
        cls = "VuSceneEntity";
    std::unique_ptr< entity_t > entity = entity_create( cls, world );
    if ( !entity )
        return nullptr;
    entity->m_name = entity_project_name( path );
    entity_load( *entity, &member( root, "data" ), world );
    return entity;
}

// VuEntity::load 0x5009D0, every known type's slot 11: an entity a template
// made (+0x4C set) takes the override path 0x5015D0; otherwise ChildEntities
// is the step recreated. An UNKNOWN entity's own load is a hook: nothing below
// it is read.
inline void entity_load( entity_t& entity, const value_t* data, const entity_world_t& world )
{
    if ( !entity.m_ctor || !data )
        return;
    if ( !entity.m_template.empty( ) )
    {
        entity_load_override( entity, *data, world );
        return;
    }
    entity_load_children( entity, member( *data, "ChildEntities" ), world );
    if ( world.m_properties_load )
        world.m_properties_load( entity, member( *data, "Properties" ) );
    entity_load_components( entity, member( *data, "Components" ), world, false );
    // The entity's vt+8 postLoad follows.
    if ( world.m_post_load )
        world.m_post_load( entity, *data, false );
}
