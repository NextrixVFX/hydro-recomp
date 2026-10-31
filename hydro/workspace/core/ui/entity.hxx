#pragma once

#include <workspace/core/game/types.hxx>
#include <workspace/core/ui/schema.hxx>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

// The front end's entities as VuEntity::load leaves them: each type's property
// nodes (schema.hxx, captured from its ctor) loaded by their classes' read
// loads (engine entity/property.hxx, values.hxx). One slot per node holds the
// target and the node's default (+0x14) - the ctor's until a template mark
// copies the loaded value in (every class's vt+0x1C).

struct ui_slot_t
{
    std::uint32_t m_bits[ 4 ] = { };  // a numeric target's raw bits (bool: low byte; int / enum / colour: one word)
    std::string m_text;               // a string node's target
};

// A row's target width in words (a string: 0).
inline int ui_width( ui_kind_t kind )
{
    switch ( kind )
    {
    case ui_kind_t::vector2: return 2;
    case ui_kind_t::vector3:
    case ui_kind_t::rotation: return 3;
    case ui_kind_t::rect: return 4;
    case ui_kind_t::text: return 0;
    default: return 1;
    }
}

struct ui_state_t
{
    const ui_property_row_t* m_rows = nullptr;  // this class's first row in g_ui_property_rows
    int m_count = 0;
    std::vector< ui_slot_t > m_value;
    std::vector< ui_slot_t > m_defaults;
};

inline const ui_property_row_t* ui_rows_for( const std::string& cls, int& count )
{
    count = 0;
    const ui_property_row_t* first = nullptr;
    for ( const ui_property_row_t& row : g_ui_property_rows )
        if ( cls == row.m_class )
        {
            if ( !first )
                first = &row;
            ++count;
        }
    return first;
}

// The state a captured front-end type's ctor leaves: every target at its node's default.
inline std::shared_ptr< ui_state_t > ui_state_construct( const std::string& cls )
{
    int count = 0;
    const ui_property_row_t* rows = ui_rows_for( cls, count );
    if ( !rows )
        return nullptr;
    auto s = std::make_shared< ui_state_t >( );
    s->m_rows = rows;
    s->m_count = count;
    s->m_defaults.resize( count );
    for ( int k = 0; k < count; ++k )
    {
        std::memcpy( s->m_defaults[ k ].m_bits, rows[ k ].m_default, sizeof( rows[ k ].m_default ) );
        s->m_defaults[ k ].m_text = rows[ k ].m_text;
    }
    s->m_value = s->m_defaults;
    return s;
}

// One node's vt+8 load on the entity's "Properties": the class's load (reset to
// the default, read, filter, store if it differs). The post-store vt+0x2C (a
// nop, or VuAssetProperty<VuTextureAsset>'s texture load 0x426840 - the
// rendering layer) and the watcher (vt+8 of +0xC, e.g. a text's Font node) are
// hooks.
inline void ui_load_slot( const ui_property_row_t& row, const value_t& properties, ui_slot_t& value, const ui_slot_t& fallback )
{
    float f[ 4 ];
    float d[ 4 ];
    std::memcpy( d, fallback.m_bits, sizeof( d ) );
    switch ( row.m_kind )
    {
    case ui_kind_t::boolean:
    {
        std::uint8_t b = 0;
        property_bool( properties, row.m_name, b, static_cast< std::uint8_t >( fallback.m_bits[ 0 ] ) );
        value.m_bits[ 0 ] = b;
        return;
    }
    case ui_kind_t::real:
    case ui_kind_t::angle:
        property_float( properties, row.m_name, f[ 0 ], d[ 0 ], row.m_kind == ui_kind_t::angle );
        std::memcpy( value.m_bits, f, 4 );
        return;
    case ui_kind_t::percent:
        property_percentage( properties, row.m_name, f[ 0 ], d[ 0 ] );
        std::memcpy( value.m_bits, f, 4 );
        return;
    case ui_kind_t::integer:
    {
        std::int32_t i = 0;
        property_int( properties, row.m_name, i, static_cast< std::int32_t >( fallback.m_bits[ 0 ] ) );
        value.m_bits[ 0 ] = static_cast< std::uint32_t >( i );
        return;
    }
    case ui_kind_t::enumeration:
    {
        const ui_choices_t& table = g_ui_choices[ row.m_choices ];
        std::int32_t i = 0;
        property_enum( properties, row.m_name, i, static_cast< std::int32_t >( fallback.m_bits[ 0 ] ), table.m_first, table.m_count );
        value.m_bits[ 0 ] = static_cast< std::uint32_t >( i );
        return;
    }
    case ui_kind_t::color:
        property_color( properties, row.m_name, value.m_bits[ 0 ], fallback.m_bits[ 0 ] );
        return;
    case ui_kind_t::vector2:
        property_floats( properties, row.m_name, f, d, 2 );
        std::memcpy( value.m_bits, f, 8 );
        return;
    case ui_kind_t::rect:
        property_floats( properties, row.m_name, f, d, 4 );
        std::memcpy( value.m_bits, f, 16 );
        return;
    case ui_kind_t::vector3:
        property_vector3( properties, row.m_name, f, d );
        std::memcpy( value.m_bits, f, 12 );
        return;
    case ui_kind_t::rotation:
        // VuRotation3dProperty (vtable 0x73D24C): the vector3's load / read with the
        // filter 0x45CB50 (each lane x 0x3C8EFA35) - transform.hxx's verified load.
        transform_property( properties, row.m_name, f, d, true );
        std::memcpy( value.m_bits, f, 12 );
        return;
    case ui_kind_t::text:
        property_string( properties, row.m_name, value.m_text, fallback.m_text );
        return;
    }
}

// The world callbacks for the front end: a captured UI type gets its ui_state_t;
// every other type falls through to types.hxx's own (the corona's).
inline std::shared_ptr< void > ui_entity_state_for( const std::string& cls )
{
    if ( auto s = ui_state_construct( cls ) )
        return s;
    return entity_state_for( cls );
}

inline bool ui_is_state( const entity_t& entity )
{
    int count = 0;
    return entity.m_state && ui_rows_for( entity.m_type, count ) != nullptr;
}

// VuEntity::load's "Properties" step: each node in +0x44 order.
inline void ui_entity_load_properties( entity_t& entity, const value_t& properties )
{
    if ( !ui_is_state( entity ) )
    {
        entity_load_properties( entity, properties );
        return;
    }
    ui_state_t* s = static_cast< ui_state_t* >( entity.m_state.get( ) );
    for ( int k = 0; k < s->m_count; ++k )
        ui_load_slot( s->m_rows[ k ], properties, s->m_value[ k ], s->m_defaults[ k ] );
}

// The template mark's property step: each node's vt+0x1C - default = value.
inline void ui_entity_mark_properties( entity_t& entity )
{
    if ( !ui_is_state( entity ) )
    {
        entity_mark_properties( entity );
        return;
    }
    ui_state_t* s = static_cast< ui_state_t* >( entity.m_state.get( ) );
    s->m_defaults = s->m_value;
}

// The world as the front end's projects load: the registry, the captured ctor
// facts and components (types.hxx), the templates by name, and the UI property
// callbacks above.
inline entity_world_t ui_world( const entity_registry_t& registry, std::function< const value_t*( const std::string& ) > templates )
{
    entity_world_t world;
    world.m_registry = &registry;
    world.m_ctor_for = entity_ctor_for;
    world.m_template = std::move( templates );
    world.m_components_for = entity_components_constructed;
    world.m_component_load = entity_load_component;
    world.m_component_mark = entity_mark_component;
    world.m_state_for = ui_entity_state_for;
    world.m_properties_load = ui_entity_load_properties;
    world.m_properties_mark = ui_entity_mark_properties;
    return world;
}
