#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/entity/registry.hxx>

#include <algorithm>
#include <cstdint>
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
// below it is read. Properties, Components and postLoad (vt+8) are hooks for
// every type until the classes behind them are recreated.

struct entity_ctor_t
{
    const char* m_class;
    std::uint32_t m_vtable;  // +0x00 after create
    std::uint32_t m_flags;   // +0x14: VuEntity ctor 0x500710's argument (0x50076D)
};

struct entity_t
{
    std::string m_type;                                 // the data's "type" it was created from
    std::uint32_t m_create = 0;                         // the registry's create fn (a hook)
    const entity_ctor_t* m_ctor = nullptr;              // null: UNKNOWN
    std::string m_name;                                 // +0x18
    entity_t* m_parent = nullptr;                       // +0x34
    std::vector< std::unique_ptr< entity_t > > m_children;  // +0x38
};

struct entity_world_t
{
    const entity_registry_t* m_registry = nullptr;
    const entity_ctor_t* ( *m_ctor_for )( const std::string& cls ) = nullptr;
};

// 0x500690, the child order (std::sort's comparator in 0x501810): a child with
// +0x14 bit 0 set before one without; otherwise by name, std::string's
// operator< (0x404AC0 memcmp of the shorter length, then the lengths).
inline bool entity_child_before( const entity_t* a, const entity_t* b )
{
    const bool fa = ( a->m_ctor->m_flags & 1 ) != 0;
    const bool fb = ( b->m_ctor->m_flags & 1 ) != 0;
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

// loadChildEntities 0x501300: create every element of `array` that has a
// string "type" and a string "name" (the name -> "data" map keeps the LAST
// element of a name, std::map operator[] 0x501700), unless a child of that
// name already exists (0x500E60) or the type is not registered (the create
// is skipped); new children get their name, the parent link and a place in
// the sorted list. Then every child - including any that existed before - is
// loaded from its entry in the map (vt+0x2C).
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
        const std::uint32_t create = world.m_registry->create_for( type.text );
        if ( !create )
            continue;
        auto child = std::make_unique< entity_t >( );
        child->m_type = type.text;
        child->m_create = create;
        child->m_ctor = world.m_ctor_for( type.text );
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

// VuEntity::load 0x5009D0, every known type's slot 11. The template path
// (+0x4C != 0 -> 0x5015D0) is only taken by an entity 0x556E70 instantiated,
// which is UNKNOWN here; ChildEntities is the step recreated. An UNKNOWN entity's
// own load is a hook: nothing below it is read.
inline void entity_load( entity_t& entity, const value_t* data, const entity_world_t& world )
{
    if ( !entity.m_ctor || !data )
        return;
    entity_load_children( entity, member( *data, "ChildEntities" ), world );
    // "Properties" (each property's vt+8), "Components" (0x501F70) and the
    // entity's vt+8 postLoad follow - hooks until those classes are recreated.
}
