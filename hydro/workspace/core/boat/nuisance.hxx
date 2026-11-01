#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/entity/node.hxx>
#include <workspace/core/entity/property.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

// VuAiNuisanceDriverStartEntity (create 0x42F930 = operator new(0x84) +
// 0x42F9B0 + ctor 0x42FA20; vtable 0x739B94): what puts the nuisance boats in
// a race. The ctor (VuEntity ctor 0x500710(0)): +0x64 "Boat" - a
// VuDBEntryProperty (0x506A70, vtable 0x74C4C0, BoatDB; its load slot 2 is
// 0x406C20, the string load property_string recreates) defaulting to
// "Police"; the word +0x80 = 1 (+0x80 = 1, +0x81 = 0 - no property, and no
// other slot writes them but the plugs below); a VuScriptComponent (+0x5C,
// 0x150) with the input plugs "DriverTarget" -> 0x42FF90 and "DriverDisable"
// -> 0x430020 (0x4300C0: VuMethod1<..., VuRetVal, VuParams const&>); a
// Vu3dLayoutComponent (+0x60, 0x50B8D0); and its child +0x7C - a VuBoatEntity
// made by operator new(0x520) + 0x41B710 + ctor 0x41B850 (not via the
// registry). Slots 2-5 / 8-10 are nops, 11 is VuEntity::load 0x5009D0.
// The dtor 0x42FD20 drops the child's +4 count (its vt+0(1) at 0), frees
// +0x64 and runs VuEntity's 0x500870 - the owner's job here (no function).
//
// WHO SETS THE NUISANCE DRIVER'S +0xF0: the DriverTarget plug. Its weak
// reference (driver +0xE4, vtable 0x737C44) is linked to the entity the
// event's VuParams name (0x42F8E0, the plug's only caller), once - only while
// it is unset - and DriverDisable releases it for good (+0x81).

struct nuisance_start_t
{
    std::string boat = "Police";           // +0x64
    std::string boat_default = "Police";   // the property's reset target (a template's mark)
    std::uint8_t active = 1;               // +0x80
    std::uint8_t disabled = 0;             // +0x81
};

inline constexpr const char* k_nuisance_start_class = "VuAiNuisanceDriverStartEntity";

// For the world's m_state_for / m_properties_load / m_properties_mark.
inline std::shared_ptr< void > nuisance_start_state_for( const std::string& cls )
{
    return cls == k_nuisance_start_class ? std::make_shared< nuisance_start_t >( ) : nullptr;
}

inline void nuisance_start_load_properties( entity_t& e, const value_t& properties )
{
    if ( e.m_type != k_nuisance_start_class || !e.m_state )
        return;
    nuisance_start_t* s = static_cast< nuisance_start_t* >( e.m_state.get( ) );
    property_string( properties, "Boat", s->boat, s->boat_default );
}

inline void nuisance_start_mark_properties( entity_t& e )
{
    if ( e.m_type == k_nuisance_start_class && e.m_state )
    {
        nuisance_start_t* s = static_cast< nuisance_start_t* >( e.m_state.get( ) );
        s->boat_default = s->boat;
    }
}

// The document slot 6 builds (0x4FC090 makes each member, 0x4FC640 sets a
// string): {"Properties": {"Boat": +0x64, "Driver Type": "Nuisance"},
// "Driver": {"Name": name + "Driver"}} (0x430160 = std::string + const char*
// with edx = the entity's +0x18 name). A VuJsonContainer member set is a
// std::map: the members here are in its order.
inline value_t nuisance_start_text( const std::string& s )
{
    value_t v;
    v.kind = value_t::kind_t::text;
    v.tag = 4;
    v.text = s;
    return v;
}

inline value_t nuisance_start_document( const std::string& name, const nuisance_start_t& s )
{
    value_t driver, properties, doc;
    driver.kind = properties.kind = doc.kind = value_t::kind_t::object;
    driver.tag = properties.tag = doc.tag = 6;
    driver.object.emplace_back( "Name", nuisance_start_text( name + "Driver" ) );
    properties.object.emplace_back( "Boat", nuisance_start_text( s.boat ) );
    properties.object.emplace_back( "Driver Type", nuisance_start_text( "Nuisance" ) );
    doc.object.emplace_back( "Driver", std::move( driver ) );
    doc.object.emplace_back( "Properties", std::move( properties ) );
    return doc;
}

// What slots 6 / 7 call on the child - the entity framework: hooks, in
// retail's order.
struct nuisance_start_hooks_t
{
    std::function< void( const std::string& ) > name_child;   // child +0x18 (0x401DB0)
    std::function< void( const value_t& ) > load_child;       // child vt+0x2C (VuEntity::load 0x5009D0)
    std::function< void( std::uint32_t ) > hash_child;        // 0x500A60(seed)
    std::function< void( bool ) > place_child;                // 0x5344A0(this +0x50's +0x50 world, notify) on the child's +0x50
    std::function< void( ) > add_child;                       // 0x500B10 (gameInitialize)
    std::function< void( ) > remove_child;                    // 0x500B80 (gameRelease)
};

// Slot 6 0x42FDC0 (gameInitialize): the document, the child named name +
// "Boat" and loaded from it (so its driver is a VuAiNuisanceDriver named
// name + "Driver"), hashed from 0x811C9DC5, put at this entity's world
// matrix (notify 1), initialised; the document freed (0x4FBB10).
inline void nuisance_start_game_initialize( const std::string& name, const nuisance_start_t& s, const nuisance_start_hooks_t& h )
{
    const value_t doc = nuisance_start_document( name, s );
    if ( h.name_child ) h.name_child( name + "Boat" );
    if ( h.load_child ) h.load_child( doc );
    if ( h.hash_child ) h.hash_child( 0x811C9DC5u );
    if ( h.place_child ) h.place_child( true );
    if ( h.add_child ) h.add_child( );
}

// Slot 7 0x42FF80: 0x500B80 on the child (a tail jump).
inline void nuisance_start_game_release( const nuisance_start_hooks_t& h )
{
    if ( h.remove_child ) h.remove_child( );
}

// 0x505370 (thiscall on a {cursor, remaining} pair the plugs build from the
// VuParams: data at +0, size at +0x80): with remaining > 0, the type word;
// type 11 (an entity) -> the next word (the entity's +0x30 hash). 0 for any
// other type or an empty VuParams.
inline std::uint32_t vu_params_entity( const std::uint8_t* params )
{
    std::int32_t size;
    std::memcpy( &size, params + 0x80, 4 );
    if ( size <= 0 )
        return 0;
    std::uint32_t type, value;
    std::memcpy( &type, params, 4 );
    if ( type != 11 )
        return 0;
    std::memcpy( &value, params + 4, 4 );
    return value;
}

// Retail's 32-bit words, for the weak references' intrusive lists (the
// objects are the caller's: a reference is +4 prev / +8 next / +0xC target,
// an entity's list head is its +8).
struct vu_words_t
{
    std::function< std::uint32_t( std::uint32_t ) > get;
    std::function< void( std::uint32_t, std::uint32_t ) > put;
};

// VuWeakRef's release 0x40E4D0 (vtable 0x737C44 slot 0).
inline void vu_weak_ref_release( const vu_words_t& w, std::uint32_t ref )
{
    const std::uint32_t target = w.get( ref + 0xC );
    if ( !target )
        return;
    if ( w.get( target + 8 ) == ref )
        w.put( target + 8, w.get( ref + 8 ) );
    if ( const std::uint32_t prev = w.get( ref + 4 ) )
        w.put( prev + 8, w.get( ref + 8 ) );
    if ( const std::uint32_t next = w.get( ref + 8 ) )
        w.put( next + 4, w.get( ref + 4 ) );
    w.put( ref + 4, 0 );
    w.put( ref + 8, 0 );
    w.put( ref + 0xC, 0 );
}

// 0x42F8E0 (thiscall, the nuisance driver; the entity; retn 4): its +0xE4
// reference released (vt+0), then - for an entity - +0xF0 = it, pushed at
// the head of the entity's +8 list.
inline void nuisance_driver_set_target( const vu_words_t& w, std::uint32_t driver, std::uint32_t entity )
{
    const std::uint32_t ref = driver + 0xE4;
    vu_weak_ref_release( w, ref );
    if ( !entity )
        return;
    w.put( ref + 0xC, entity );
    if ( const std::uint32_t head = w.get( entity + 8 ) )
        w.put( head + 4, ref );
    w.put( ref + 8, w.get( entity + 8 ) );
    w.put( entity + 8, ref );
}

// 0x41B4B0(off_73621C) / 0x430020's inline walk: the driver's vt+4 type
// chain (+4 the parent) holds VuAiNuisanceDriver's. No class derives from
// VuAiNuisanceDriver (RTTI), so that is its own vtable 0x737C4C - the
// chain walk itself is not recreated (the case runs every driver vtable).
inline bool nuisance_driver_is( std::uint32_t vtable )
{
    return vtable == 0x737C4Cu;
}

// The plugs (thiscall(this, VuRetVal* out, VuParams const&), retn 8; *out =
// 0). `child` = +0x7C (0: none), `find` = the entity repository g 0x759080's
// vt+0x28 (a hash -> the entity, 0 when none); the child's driver is its
// +0x220. DriverTarget 0x42FF90: unless +0x81, with +0x80 and a child: the
// entity the params name; when there is one and the child's driver is a
// nuisance driver whose +0xF0 is unset - 0x42F8E0.
inline std::uint32_t nuisance_start_driver_target( const nuisance_start_t& s, std::uint32_t child, const std::uint8_t* params,
                                                   const std::function< std::uint32_t( std::uint32_t ) >& find, const vu_words_t& w )
{
    if ( s.disabled || !s.active || !child )
        return 0;
    const std::uint32_t entity = find( vu_params_entity( params ) );
    if ( !entity )
        return 0;
    const std::uint32_t driver = w.get( child + 0x220 );
    if ( nuisance_driver_is( w.get( driver ) ) && w.get( driver + 0xF0 ) == 0 )
        nuisance_driver_set_target( w, driver, entity );
    return 0;
}

// DriverDisable 0x430020: with +0x80 and a child, when the entity the params
// name IS the child and its driver is a nuisance driver: the reference
// released (+0xE4 vt+0) and +0x81 = 1.
inline std::uint32_t nuisance_start_driver_disable( nuisance_start_t& s, std::uint32_t child, const std::uint8_t* params,
                                                    const std::function< std::uint32_t( std::uint32_t ) >& find, const vu_words_t& w )
{
    if ( !s.active || !child )
        return 0;
    if ( find( vu_params_entity( params ) ) != child )
        return 0;
    const std::uint32_t driver = w.get( child + 0x220 );
    if ( !nuisance_driver_is( w.get( driver ) ) )
        return 0;
    vu_weak_ref_release( w, driver + 0xE4 );
    s.disabled = 1;
    return 0;
}
