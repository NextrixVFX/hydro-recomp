#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/entity/schema.hxx>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// Reading an entity's authored properties against the schema, in one place
// instead of a hand-written line per key.
//
// THE CONVERSION RULE, AND IT IS NOT SYMMETRIC. A value authored in a level is
// in AUTHORED units - degrees, percent - and retail converts it on the way in.
// A default in the schema was read out of the constructor, so it is already in
// STORED units: VuCoronaEntity's Cone Angle default is 6.2831855 radians, not
// 360 degrees. So an authored value is converted and a default is not. Getting
// that backwards would scale every default by 57.3 or by 100.
//
// An absent key and a key authored at its default are NOT the same thing, and
// this never conflates them: `authored()` answers by pointer.
class c_entity_binder
{
public:
    c_entity_binder( const std::string& class_name, const value_t* overlay,
        const value_t* properties )
        : m_schema( entity_schema_find( class_name.c_str( ) ) )
        , m_overlay( overlay )
        , m_properties( properties )
    {
    }

    // False when no constructor for this class has been emulated yet. The
    // caller must not silently treat that as "no properties".
    bool known( ) const { return m_schema != nullptr; }
    const entity_schema_t* schema( ) const { return m_schema; }

    // The authored node for a key: the instance overlay wins over the
    // template's own block, which is how a template child override works.
    const value_t* authored( const char* key ) const
    {
        if ( m_overlay != nullptr )
        {
            if ( const value_t* value = find( *m_overlay, key ) )
                return value;
        }
        if ( m_properties != nullptr )
            return find( *m_properties, key );
        return nullptr;
    }

    float real( const char* key, float fallback = 0.0f ) const
    {
        const entity_property_t* declared = entity_schema_property( m_schema, key );
        const value_t* value = authored( key );
        if ( value != nullptr )
            return convert( number( value, fallback ), declared );
        if ( declared != nullptr && declared->m_verified )
            return entity_schema_default_real( *declared );
        return fallback;
    }

    bool boolean( const char* key, bool fallback = false ) const
    {
        const value_t* value = authored( key );
        if ( value != nullptr )
            return number( value, fallback ? 1.0f : 0.0f ) != 0.0f;
        const entity_property_t* declared = entity_schema_property( m_schema, key );
        if ( declared != nullptr && declared->m_verified )
            return ( declared->m_default[ 0 ] & 0xFF ) != 0;
        return fallback;
    }

    std::string text( const char* key ) const
    {
        const value_t* value = authored( key );
        if ( value != nullptr && value->kind == value_t::kind_t::text )
            return value->text;
        const entity_property_t* declared = entity_schema_property( m_schema, key );
        if ( declared != nullptr && declared->m_verified && declared->m_default_text != nullptr )
            return declared->m_default_text;
        return std::string( );
    }

    // X/Y/Z off a sub-object. A rotation is three angles, so the unit applies
    // per component exactly as it does to a scalar.
    void vector( const char* key, float* out ) const
    {
        const entity_property_t* declared = entity_schema_property( m_schema, key );
        const value_t* value = authored( key );
        if ( value != nullptr )
        {
            static const char* axes[ 3 ] = { "X", "Y", "Z" };
            for ( int index = 0; index < 3; ++index )
                out[ index ] = convert( number( find( *value, axes[ index ] ), out[ index ] ), declared );
            return;
        }
        if ( declared != nullptr && declared->m_verified && declared->m_width == 16 )
        {
            for ( int index = 0; index < 3; ++index )
                std::memcpy( &out[ index ], &declared->m_default[ index ], sizeof( float ) );
        }
    }

    // A VuColor, as four floats in 0..1. Levels author colours as 0..255 byte
    // triples throughout, and the constructor stores them packed: the default
    // 0xFF000000 is black at full alpha, so R is the LOW byte and A the high
    // one - deduced from VuGlobalGfxSettingsEntity, whose Clear Color and Fog
    // Color default black while Tint defaults 0xFFFFFFFF white.
    void color( const char* key, float* out ) const
    {
        if ( const value_t* value = authored( key ) )
        {
            static const char* channels[ 4 ] = { "R", "G", "B", "A" };
            for ( int index = 0; index < 4; ++index )
                out[ index ] = number( find( *value, channels[ index ] ), out[ index ] * 255.0f ) / 255.0f;
            return;
        }
        const entity_property_t* declared = entity_schema_property( m_schema, key );
        if ( declared != nullptr && declared->m_verified && declared->m_type == prop_type_t::color )
        {
            const std::uint32_t packed = declared->m_default[ 0 ];
            for ( int index = 0; index < 4; ++index )
                out[ index ] = static_cast< float >( ( packed >> ( index * 8 ) ) & 0xFF ) / 255.0f;
        }
    }

    // Keys the level authored that this class's schema does not declare. A
    // non-empty list means the schema is incomplete for the class, and
    // anything built on it would be reading a guess.
    std::vector< std::string > undeclared( ) const
    {
        std::vector< std::string > out;
        if ( m_schema == nullptr )
            return out;
        for ( const value_t* block : { m_properties, m_overlay } )
        {
            if ( block == nullptr || block->kind != value_t::kind_t::object )
                continue;
            for ( const auto& pair : block->object )
            {
                if ( entity_schema_property( m_schema, pair.first.c_str( ) ) != nullptr )
                    continue;
                if ( std::find( out.begin( ), out.end( ), pair.first ) == out.end( ) )
                    out.push_back( pair.first );
            }
        }
        return out;
    }

private:
    // Authored -> stored. 0.017453292 is retail's own degrees-to-radians
    // constant and 0.0099999998 its percent-to-fraction one; both are read out
    // of VuAngleProperty and VuPercentageProperty rather than written as
    // 1/57.3 and 1/100.
    static float convert( float authored, const entity_property_t* declared )
    {
        if ( declared == nullptr )
            return authored;
        if ( declared->m_unit == prop_unit_t::degrees )
            return authored * 0.017453292f;
        if ( declared->m_unit == prop_unit_t::percent )
            return authored * 0.0099999998f;
        return authored;
    }

    const entity_schema_t* m_schema = nullptr;
    const value_t* m_overlay = nullptr;
    const value_t* m_properties = nullptr;
};
