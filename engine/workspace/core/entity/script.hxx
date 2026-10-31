#pragma once

#include <workspace/core/entity/node.hxx>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// The event graph as VuScriptComponent builds it at load. A component's plugs
// and refs are DECLARED by the owning type's constructor (captured from retail:
// hydro game/plugs.hxx); the data only names what each one connects to, and
// the load resolves those names ONCE into pointer lists that firing then walks
// (VuScriptOutputPlug_fire 0x5023A0 - not here).
//
// The component's own Width (+0x48) and Position (+0x4C) - the editor's layout
// of the node, read first by both loads - are not modelled: nothing in the
// graph reads them.

// One type's descriptor chain: its vt+4 getType, then each descriptor's +4
// parent, names '|'-joined (a descriptor's +0 is its name's pointer).
struct entity_chain_row_t
{
    const char* m_class;
    const char* m_chain;
};

// A plug its type's ctor declares on its VuScriptComponent (+0x10 order):
// VuScriptOutputPlug_init 0x502080(name, type, params) - name at +0xC, the
// return type at +0x24, the parameter count at +0x28 and its types at +0x2C;
// an input plug is the same object re-tagged VuScriptInputPlug (vt 0x737138),
// whose vt+4 answers true.
struct script_plug_row_t
{
    const char* m_class;
    const char* m_name;
    int m_input;
    std::uint32_t m_type;
    std::uint32_t m_count;
    std::uint32_t m_params[ 8 ];
};

// A VuScriptRef its type's ctor declares (+0x1C order): its name (+0xC) and
// the type a target must be (+0x10, a descriptor).
struct script_ref_row_t
{
    const char* m_class;
    const char* m_name;
    const char* m_requires;
};

// Where the captured rows live (the game's tables) - handed to every call.
struct script_facts_t
{
    const entity_chain_row_t* m_chains;
    std::size_t m_chain_count;
    const script_plug_row_t* m_plugs;
    std::size_t m_plug_count;
    const script_ref_row_t* m_refs;
    std::size_t m_ref_count;
};

// The other end of a link: the plug's owner and its index in that
// component's +0x10 list.
struct script_link_t
{
    entity_t* m_entity = nullptr;
    int m_plug = -1;
    bool operator==( const script_link_t& o ) const { return m_entity == o.m_entity && m_plug == o.m_plug; }
};

struct script_plug_t
{
    const script_plug_row_t* m_row = nullptr;
    std::vector< script_link_t > m_links;   // +0x50 .. +0x58
};

struct script_ref_t
{
    const script_ref_row_t* m_row = nullptr;
    entity_t* m_target = nullptr;           // +0x18: the target's script component, held as its entity
};

// A ref pointing at this component (its +0x28 list): the ref's owner and index.
struct script_referrer_t
{
    entity_t* m_entity = nullptr;
    int m_ref = -1;
    bool operator==( const script_referrer_t& o ) const { return m_entity == o.m_entity && m_ref == o.m_ref; }
};

struct script_component_t
{
    std::vector< script_plug_t > m_plugs;         // +0x10
    std::vector< script_ref_t > m_refs;           // +0x1C
    std::vector< script_referrer_t > m_referrers; // +0x28
    // A ref whose watcher (+0x20) retail notifies (vt+8) on a RefConnections
    // bind or unbind - the watcher is the owning entity's (a hook): counted.
    int m_ref_notifications = 0;
};

inline constexpr const char* k_script_component_key = "VuScriptComponent";
inline constexpr std::uint32_t k_script_component_vtable = 0x74C3F0u;

// The component state a type's ctor leaves: its declared plugs and refs, no
// link. Null for a type with no captured rows of either kind.
inline std::shared_ptr< script_component_t > script_construct( const std::string& cls, const script_facts_t& facts )
{
    auto sc = std::make_shared< script_component_t >( );
    for ( std::size_t k = 0; k < facts.m_plug_count; ++k )
        if ( cls == facts.m_plugs[ k ].m_class )
            sc->m_plugs.push_back( script_plug_t{ &facts.m_plugs[ k ], { } } );
    for ( std::size_t k = 0; k < facts.m_ref_count; ++k )
        if ( cls == facts.m_refs[ k ].m_class )
            sc->m_refs.push_back( script_ref_t{ &facts.m_refs[ k ], nullptr } );
    return sc;
}

// 0x46E070: the first component whose getType chain holds VuScriptComponent's
// descriptor 0x736800. Only VuScriptComponent itself does (codegen
// plugfacts.py asserts it over every captured type), so: the first component
// keyed "VuScriptComponent". Null when there is none or it is not modelled.
inline script_component_t* script_of( entity_t& e )
{
    for ( entity_component_t& c : e.m_components )
        if ( c.m_vtable == k_script_component_vtable )
            return static_cast< script_component_t* >( c.m_state.get( ) );
    return nullptr;
}

// 0x500F60 findEntity(path), from the topmost ancestor (+0x34 walked to null):
// while the path is not the current entity's name, drop the path's first
// segment (0x5649C0: after the first '/', or the whole path when it has none)
// and step to the child named by the new first segment (0x564940: before the
// first '/', or all of it; findChild 0x500E60). So a path's first segment is
// never compared - it stands for the root, whatever its name - and a path
// without a '/' names a child of the root.
inline entity_t* entity_find_path( entity_t& from, const std::string& path )
{
    entity_t* e = &from;
    while ( e->m_parent )
        e = e->m_parent;
    std::string rest = path;
    while ( e )
    {
        if ( rest == e->m_name )
            return e;
        const std::size_t slash = rest.find( '/' );
        if ( slash != std::string::npos )
            rest = rest.substr( slash + 1 );
        const std::size_t next = rest.find( '/' );
        e = entity_find_child( *e, next != std::string::npos ? rest.substr( 0, next ) : rest );
    }
    return nullptr;
}

// 0x504770 findPlug(name): the first plug in +0x10 order of that exact name.
inline int script_find_plug( const script_component_t& sc, const std::string& name )
{
    for ( std::size_t k = 0; k < sc.m_plugs.size( ); ++k )
        if ( name == sc.m_plugs[ k ].m_row->m_name )
            return static_cast< int >( k );
    return -1;
}

// 0x504870 findRef(name), the same over +0x1C (the ref's name is a char*).
inline int script_find_ref( const script_component_t& sc, const std::string& name )
{
    for ( std::size_t k = 0; k < sc.m_refs.size( ); ++k )
        if ( name == sc.m_refs[ k ].m_row->m_name )
            return static_cast< int >( k );
    return -1;
}

// 0x502260, whether plug a may connect to plug b: one input and one output
// (vt+4 differs); an output with a return type (+0x24 != 0) needs the input's
// to match AND no link yet; the input's parameter count must not exceed the
// output's and its types must equal the output's first ones; and neither may
// already list the other.
inline bool script_plugs_compatible( const script_link_t& a, const script_plug_t& pa, const script_link_t& b, const script_plug_t& pb )
{
    if ( ( pa.m_row->m_input != 0 ) == ( pb.m_row->m_input != 0 ) )
        return false;
    const bool a_in = pa.m_row->m_input != 0;
    const script_plug_t& in = a_in ? pa : pb;
    const script_plug_t& out = a_in ? pb : pa;
    const script_link_t& in_at = a_in ? a : b;
    const script_link_t& out_at = a_in ? b : a;
    if ( out.m_row->m_type != 0 && ( out.m_row->m_type != in.m_row->m_type || !out.m_links.empty( ) ) )
        return false;
    if ( out.m_row->m_count < in.m_row->m_count )
        return false;
    for ( std::uint32_t k = 0; k < in.m_row->m_count; ++k )
        if ( in.m_row->m_params[ k ] != out.m_row->m_params[ k ] )
            return false;
    for ( const script_link_t& l : in.m_links )
        if ( l == out_at )
            return false;
    for ( const script_link_t& l : out.m_links )
        if ( l == in_at )
            return false;
    return true;
}

// A string member's text, or "" when it is not a string (retail's static
// empty VuString 0x819950).
inline const std::string& script_text( const value_t& v, const char* key )
{
    static const std::string empty;
    const value_t& m = member( v, key );
    return m.tag == 4 ? m.text : empty;
}

// 0x502430, one plug's "Connections" (this = owner's plug `index`): for each
// element, EntityName resolved from the owner (0x500F60), its script
// component (0x46E070), PlugName found there (0x504770); when compatible
// (0x502260) BOTH ends get the other pushed onto +0x50 - this plug first.
inline void script_load_connections( entity_t& owner, int index, const value_t& connections )
{
    script_component_t* sc = script_of( owner );
    const int count = connections.tag == 5 ? static_cast< int >( connections.list.size( ) ) : 0;  // 0x4FBC40
    for ( int k = 0; k < count; ++k )
    {
        const value_t& element = connections.list[ k ];
        const std::string& entity_name = script_text( element, "EntityName" );
        const std::string& plug_name = script_text( element, "PlugName" );
        entity_t* target = entity_find_path( owner, entity_name );
        script_component_t* other = target ? script_of( *target ) : nullptr;
        if ( !other )
            continue;
        const int found = script_find_plug( *other, plug_name );
        if ( found < 0 )
            continue;
        const script_link_t here{ &owner, index };
        const script_link_t there{ target, found };
        if ( !script_plugs_compatible( here, sc->m_plugs[ index ], there, other->m_plugs[ found ] ) )
            continue;
        sc->m_plugs[ index ].m_links.push_back( there );
        other->m_plugs[ found ].m_links.push_back( here );
    }
}

inline bool script_chain_holds( const std::string& cls, const char* required, const script_facts_t& facts )
{
    for ( std::size_t k = 0; k < facts.m_chain_count; ++k )
    {
        if ( cls != facts.m_chains[ k ].m_class )
            continue;
        const std::string chain = facts.m_chains[ k ].m_chain;
        std::size_t at = 0;
        while ( at <= chain.size( ) )
        {
            const std::size_t bar = chain.find( '|', at );
            const std::size_t end = bar == std::string::npos ? chain.size( ) : bar;
            if ( chain.compare( at, end - at, required ) == 0 )
                return true;
            if ( bar == std::string::npos )
                break;
            at = bar + 1;
        }
        return false;
    }
    return false;
}

// 0x542750, whether ref `index` of `owner` may point at `target`'s script
// component: not its own component; the target ENTITY's type chain (vt+4)
// holds the ref's required type; not already in the target's +0x28 list.
inline bool script_ref_compatible( entity_t& owner, int index, entity_t& target, const script_facts_t& facts )
{
    if ( &owner == &target )
        return false;
    const script_ref_t& ref = script_of( owner )->m_refs[ index ];
    if ( !script_chain_holds( target.m_type, ref.m_row->m_requires, facts ) )
        return false;
    for ( const script_referrer_t& r : script_of( target )->m_referrers )
        if ( r == script_referrer_t{ &owner, index } )
            return false;
    return true;
}

// 0x542830, ref `index`'s load from "Refs": Refs[its name] as a path (or ""),
// resolved from the topmost ancestor (0x500F60), its script component
// (0x46E070); when compatible (0x542750) +0x18 = it and the ref joins its
// +0x28 list. No watcher call, and a target already held is not released.
inline void script_load_ref( entity_t& owner, int index, const value_t& refs, const script_facts_t& facts )
{
    script_ref_t& ref = script_of( owner )->m_refs[ index ];
    entity_t* target = entity_find_path( owner, script_text( refs, ref.m_row->m_name ) );
    if ( !target || !script_of( *target ) || !script_ref_compatible( owner, index, *target, facts ) )
        return;
    ref.m_target = target;
    script_of( *target )->m_referrers.push_back( script_referrer_t{ &owner, index } );
}

// 0x5427B0, a ref (re)bound to `target` from the target's side: compatible
// (0x542750) or nothing; a held target first lets go (0x504980 removes the
// ref from that component's +0x28 list, +0x18 = 0, watcher vt+8); then +0x18
// = target, the ref joins its +0x28 list, watcher vt+8 again.
inline void script_connect_ref( entity_t& owner, int index, entity_t& target, const script_facts_t& facts )
{
    if ( !script_ref_compatible( owner, index, target, facts ) )
        return;
    script_component_t* sc = script_of( owner );
    script_ref_t& ref = sc->m_refs[ index ];
    const script_referrer_t me{ &owner, index };
    if ( ref.m_target )
    {
        std::vector< script_referrer_t >& old = script_of( *ref.m_target )->m_referrers;
        for ( auto it = old.begin( ); it != old.end( ); ++it )
            if ( *it == me )
            {
                old.erase( it );
                break;
            }
        ref.m_target = nullptr;
        ++sc->m_ref_notifications;
    }
    ref.m_target = &target;
    script_of( target )->m_referrers.push_back( me );
    ++sc->m_ref_notifications;
}

// 0x5049E0, "RefConnections" on the TARGET side: each element's EntityName
// resolved from this component's owner (0x500F60), its script component
// (0x46E070), RefName found there (0x504870), bound to this one (0x542750,
// then 0x5427B0).
inline void script_load_ref_connections( entity_t& owner, const value_t& list, const script_facts_t& facts )
{
    const int count = list.tag == 5 ? static_cast< int >( list.list.size( ) ) : 0;
    for ( int k = 0; k < count; ++k )
    {
        const value_t& element = list.list[ k ];
        entity_t* source = entity_find_path( owner, script_text( element, "EntityName" ) );
        script_component_t* other = source ? script_of( *source ) : nullptr;
        if ( !other )
            continue;
        const int ref = script_find_ref( *other, script_text( element, "RefName" ) );
        if ( ref < 0 || !script_ref_compatible( *source, ref, owner, facts ) )
            continue;
        script_connect_ref( *source, ref, owner, facts );
    }
}

// VuScriptComponent's load: vt+0xC 0x503D70 on a normal load, vt+0x10
// 0x504320 on a template override. Both: (Width, Position - not modelled),
// then each declared plug's Connections from Plugs[its name] (an OBJECT, else
// the null container - std::map find 0x4F22B0), then each ref from Refs; only
// the normal load reads RefConnections.
inline void script_load( entity_t& owner, const value_t& data, bool override, const script_facts_t& facts )
{
    script_component_t* sc = script_of( owner );
    if ( !sc )
        return;
    static const value_t null{};
    const value_t& plugs = member( data, "Plugs" );
    for ( std::size_t k = 0; k < sc->m_plugs.size( ); ++k )
    {
        const value_t* named = plugs.tag == 6 ? find( plugs, sc->m_plugs[ k ].m_row->m_name ) : nullptr;
        script_load_connections( owner, static_cast< int >( k ), member( named ? *named : null, "Connections" ) );
    }
    const value_t& refs = member( data, "Refs" );
    for ( std::size_t k = 0; k < sc->m_refs.size( ); ++k )
        script_load_ref( owner, static_cast< int >( k ), refs, facts );
    if ( !override )
        script_load_ref_connections( owner, member( data, "RefConnections" ), facts );
}
