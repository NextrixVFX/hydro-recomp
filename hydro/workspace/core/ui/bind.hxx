#pragma once

#include <workspace/core/ui/screen.hxx>
#include <workspace/core/ui/wiring.hxx>

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// The per-frame update of a loaded screen: OUR loaded entity tree (the scripted
// world, wiring.hxx - plugs and links from the load) bound to the runtime
// objects that retail's ctors build, so c_ui_screen's enter / tick / exit drive
// them through the same event maps and the same graph.
//
// Every handler a ctor registers through VuEventMap 0x5156E0 goes BOTH into the
// entity's own map (+0xC, a std::map by hash; the dispatch 0x5155C0) and into
// the event manager g 0x759028 (+4: hash -> subscribers, in registration order);
// VuEventManager_broadcast 0x505690 runs every subscriber of the hash inside the
// call. So a broadcast here reaches every bound map holding the hash, in
// pre-order (the load creates a parent before its children).
//
// Recreated (bound) types, their handlers and input plugs:
// - VuUIScreenEntity (vtable 0x6F7140): ScreenEnter 0x629A70 / ScreenExit
//   0x629B00 / tick 0x629B90 (c_ui_screen); it registers no handler of its own.
// - VuUIContainerEntity (ctor 0x62F880, vtable 0x6F74F0): "OnUITick" 0x62F7A0
//   and "OnUIEvent" 0x62F7C0, each 0x5011A0 of the same name and params to its
//   own DIRECT children (disassembled).
// - VuBarracudaUIButtonEntity (0x73B7B0): c_ui_button (OnUISetFocus 0x442A40,
//   OnUITick 0x442A90, OnUIEvent 0x442D60; inputs SetFocus / SetStringID /
//   Enable / Disable / IsEnabled), its fields from OUR loaded Enabled / Label /
//   Can Have Focus / String ID, its id = FNV-1a of the long name (0x500D50).
// - VuUIEventEntity (0x6F7308): c_ui_event (OnUIEvent 0x62E950 -> Trigger).
// - VuIfEntity (0x754CD4): input Trigger 0x5DDE60 (disassembled): fire the
//   "Operand" output with an EMPTY VuParams; when its VuRetVal type (+0) is 3,
//   fire "True" when the value's LOW BYTE (+4) is non-zero else "False", both
//   with the Trigger's own params; the handler writes only +0 = 0 of its
//   VuRetVal (+4 is the caller's: UNKNOWN - m_known false).
// - the screen actions and VuSetGameModeUIAction (wiring.hxx / action.hxx).
// Anything else: an input plug reached is UNKNOWN (counted; its VuRetVal is
// the m_answer hook's - e.g. VuLicenseEntity's IsFullVersion, a platform
// service); a registered handler not recreated (k_ui_event_facts) is entered
// as UNKNOWN (counted, traced, nothing runs).

// The event-map rows of the types whose handlers are NOT recreated here, as
// retail's ctors registered them on the loaded Screens projects (case
// ui_frames' census of every entity's VuEventMap: vtable, hash, method).
struct ui_event_fact_t
{
    std::uint32_t m_vtable;
    std::uint32_t m_hash;
    std::uint32_t m_method;
};

inline constexpr ui_event_fact_t k_ui_event_facts[] = {
    { 0x738614u, 0x407F5DB0u, 0x4232B0u },  // VuSysNotifyEntity
    { 0x73BC5Cu, 0x120E6E48u, 0x446480u },  // VuInfoUITextEntity OnSetInfoStringID (ticker.hxx - not bound yet)
    { 0x73BC5Cu, 0x3683B89Du, 0x446450u },  // VuInfoUITextEntity OnClearInfoStringID
    { 0x73BC5Cu, 0x5E798927u, 0x446580u },  // VuInfoUITextEntity OnUITick
    { 0x756828u, 0xD5127E2Du, 0x5EFA60u },  // VuOnProjectInitializedEntity
};

inline constexpr std::uint32_t k_ui_screen_vtable = 0x6F7140u;
inline constexpr std::uint32_t k_ui_container_vtable = 0x6F74F0u;
inline constexpr std::uint32_t k_ui_button_vtable = 0x73B7B0u;
inline constexpr std::uint32_t k_ui_event_vtable = 0x6F7308u;
inline constexpr std::uint32_t k_ui_if_vtable = 0x754CD4u;

// One step of the frame's trace, at the points retail's case logs them.
enum class ui_trace_kind_t
{
    fire,       // an output plug's 0x5023A0 entered: owner, plug, params
    input,      // an input plug's 0x404BE0 entered: owner, plug, params, recreated
    broadcast,  // 0x505690 entered: hash, params
    sound,      // FMOD getEvent: the name
    event,      // a registered handler not recreated: owner, its method
};

struct ui_trace_t
{
    ui_trace_kind_t m_kind;
    int m_index = -1;          // the owner's pre-order index
    std::string m_name;        // plug / sound name
    std::uint32_t m_word = 0;  // broadcast hash / event method
    ui_params_t m_params;
    bool m_known = true;
};

struct ui_bound_t
{
    entity_t* m_entity = nullptr;
    int m_index = 0;
    std::uint32_t m_vtable = 0;
    ui_event_map_t m_map;
    std::vector< std::pair< std::uint32_t, std::uint32_t > > m_methods;  // (hash, retail method) as registered
    ui_button_hooks_t m_button_hooks;
    std::unique_ptr< c_ui_button > m_button;
    std::unique_ptr< c_ui_event > m_event;
};

struct ui_bind_hooks_t
{
    // HOOK: the input manager g 0x759038 slot 9: (pad, name) -> down.
    std::function< bool( int, const char* ) > m_input;
    // HOOK: an input plug not recreated - its VuRetVal (default { 0, 0 }).
    std::function< ui_ret_t( const entity_t&, const char* ) > m_answer;
    value_t* m_root = nullptr;  // gameutil +80
};

// An entity's own property slot (its loaded ui_state_t), or null.
inline const ui_slot_t* ui_bind_slot( const entity_t& e, const char* name )
{
    if ( !ui_is_state( e ) )
        return nullptr;
    const ui_state_t* s = static_cast< const ui_state_t* >( e.m_state.get( ) );
    for ( int k = 0; k < s->m_count; ++k )
        if ( std::strcmp( s->m_rows[ k ]->m_name, name ) == 0 )
            return &s->m_value[ static_cast< std::size_t >( k ) ];
    return nullptr;
}

// 0x500BF0: the ancestors' names and its own, root first, joined by "/".
inline std::string ui_long_name( const entity_t& e )
{
    return e.m_parent ? ui_long_name( *e.m_parent ) + "/" + e.m_name : e.m_name;
}

class c_ui_binding
{
public:
    c_ui_binding( entity_t& root, ui_bind_hooks_t hooks ) : m_hooks( std::move( hooks ) )
    {
        const auto walk = [ this ]( const auto& self, entity_t& e ) -> void
        {
            auto b = std::make_unique< ui_bound_t >( );
            b->m_entity = &e;
            b->m_index = static_cast< int >( m_bound.size( ) );
            b->m_vtable = e.m_ctor ? e.m_ctor->m_vtable : 0u;
            m_index[ &e ] = b->m_index;
            m_bound.push_back( std::move( b ) );
            for ( auto& child : e.m_children )
                self( self, *child );
        };
        walk( walk, root );
        for ( auto& b : m_bound )
            bind( *b );
        m_wiring.m_root = m_hooks.m_root;
        m_wiring.m_broadcast = [ this ]( std::uint32_t hash, const ui_params_t& p ) { broadcast( hash, p ); };
        m_wiring.m_on_fire = [ this ]( const entity_t& owner, int plug, const ui_params_t& p )
        {
            trace( ui_trace_kind_t::fire, index_of( owner ), plug_name( owner, plug ), 0u, p, true );
        };
        m_wiring.m_on_input = [ this ]( const entity_t& owner, const char* plug, const ui_params_t& p, bool known )
        {
            trace( ui_trace_kind_t::input, index_of( owner ), plug, 0u, p, known );
        };
        m_wiring.m_bound_known = [ this ]( const entity_t& owner, const char* plug ) { return bound_input( owner, plug ); };
        m_wiring.m_bound_run = [ this ]( entity_t& owner, const char* plug, const ui_params_t& p ) { return run_input( owner, plug, p ); };
        m_wiring.m_answer = m_hooks.m_answer;
        m_screen.m_input = m_hooks.m_input;
        for ( auto& child : root.m_children )
            m_screen.m_children.push_back( &m_bound[ static_cast< std::size_t >( m_index[ child.get( ) ] ) ]->m_map );
    }

    void enter( ) { m_screen.screen_enter( ); }
    void exit( ) { m_screen.screen_exit( ); }
    void tick( float dt, bool input ) { m_screen.tick( dt, input ); }

    std::vector< std::unique_ptr< ui_bound_t > > m_bound;  // pre-order
    std::vector< ui_trace_t > m_trace;
    int m_unknown_events = 0;  // registered handlers entered that are not recreated
    c_ui_wiring m_wiring;
    c_ui_screen m_screen;

private:
    void trace( ui_trace_kind_t kind, int index, std::string name, std::uint32_t word, const ui_params_t& p, bool known )
    {
        ui_trace_t t;
        t.m_kind = kind;
        t.m_index = index;
        t.m_name = std::move( name );
        t.m_word = word;
        t.m_params = p;
        t.m_known = known;
        m_trace.push_back( std::move( t ) );
    }

    int index_of( const entity_t& e ) const
    {
        const auto found = m_index.find( &e );
        return found == m_index.end( ) ? -1 : found->second;
    }

    static std::string plug_name( const entity_t& owner, int plug )
    {
        const script_component_t* sc = script_of( const_cast< entity_t& >( owner ) );
        if ( !sc || plug < 0 || static_cast< std::size_t >( plug ) >= sc->m_plugs.size( ) )
            return "?";
        return sc->m_plugs[ static_cast< std::size_t >( plug ) ].m_row->m_name;
    }

    static int plug_index( entity_t& owner, const char* name )
    {
        const script_component_t* sc = script_of( owner );
        for ( std::size_t k = 0; sc && k < sc->m_plugs.size( ); ++k )
            if ( std::strcmp( sc->m_plugs[ k ].m_row->m_name, name ) == 0 )
                return static_cast< int >( k );
        return -1;
    }

    // VuScriptComponent findPlug 0x504770 + the plug's slot 2.
    ui_ret_t fire_named( entity_t& owner, const char* plug, const ui_params_t& p )
    {
        return m_wiring.fire( owner, plug_index( owner, plug ), p );
    }

    void add( ui_bound_t& b, const char* event, std::uint32_t method, std::function< void( const ui_params_t& ) > fn )
    {
        b.m_map.add( event, std::move( fn ) );
        b.m_methods.push_back( { fsm_hash( event ), method } );
    }

    void bind( ui_bound_t& b )
    {
        entity_t& e = *b.m_entity;
        if ( b.m_vtable == k_ui_container_vtable )
        {
            // 0x62F880 registers OnUITick (0x62F7A0) then OnUIEvent (0x62F7C0).
            for ( const char* event : { "OnUITick", "OnUIEvent" } )
                add( b, event, std::strcmp( event, "OnUITick" ) == 0 ? 0x62F7A0u : 0x62F7C0u, [ this, &e, event ]( const ui_params_t& p )
                {
                    const std::uint32_t h = fsm_hash( event );
                    for ( auto& child : e.m_children )
                        m_bound[ static_cast< std::size_t >( m_index[ child.get( ) ] ) ]->m_map.handle( h, p );
                } );
        }
        else if ( b.m_vtable == k_ui_button_vtable )
        {
            b.m_button_hooks.m_broadcast = [ this ]( std::uint32_t hash, const ui_params_t& p ) { broadcast( hash, p ); };
            b.m_button_hooks.m_fire = [ this, &e ]( const char* plug, const ui_params_t& p ) { fire_named( e, plug, p ); };
            b.m_button_hooks.m_connections = [ &e ]( const char* plug ) -> int
            {
                const script_component_t* sc = script_of( e );
                const int k = plug_index( e, plug );
                return sc && k >= 0 ? static_cast< int >( sc->m_plugs[ static_cast< std::size_t >( k ) ].m_links.size( ) ) : 0;
            };
            b.m_button_hooks.m_sound = [ this ]( const char* name ) { trace( ui_trace_kind_t::sound, -1, name, 0u, ui_params_t( ), true ); };
            b.m_button = std::make_unique< c_ui_button >( ui_entity_id( ui_long_name( e ) ), b.m_button_hooks );
            c_ui_button& button = *b.m_button;
            if ( const ui_slot_t* s = ui_bind_slot( e, "Enabled" ) )
                button.m_enabled = ( s->m_bits[ 0 ] & 0xFFu ) != 0;
            if ( const ui_slot_t* s = ui_bind_slot( e, "Label" ) )
                button.m_label = ( s->m_bits[ 0 ] & 0xFFu ) != 0;
            if ( const ui_slot_t* s = ui_bind_slot( e, "Can Have Focus" ) )
                button.m_can_focus = ( s->m_bits[ 0 ] & 0xFFu ) != 0;
            if ( const ui_slot_t* s = ui_bind_slot( e, "String ID" ) )
                button.m_string_id = s->m_text;
            // 0x443800: OnUISetFocus, OnUITick, OnUIEvent.
            add( b, "OnUISetFocus", 0x442A40u, [ &button ]( const ui_params_t& p ) { button.on_set_focus( p ); } );
            add( b, "OnUITick", 0x442A90u, [ &button ]( const ui_params_t& ) { button.on_tick( ); } );
            add( b, "OnUIEvent", 0x442D60u, [ &button ]( const ui_params_t& p ) { button.on_event( p ); } );
        }
        else if ( b.m_vtable == k_ui_event_vtable )
        {
            const ui_slot_t* s = ui_bind_slot( e, "Event" );
            b.m_event = std::make_unique< c_ui_event >( s ? s->m_text : std::string( ) );
            c_ui_event& ev = *b.m_event;
            ev.m_fire = [ this, &e ]( const ui_params_t& p ) { fire_named( e, "Trigger", p ); };
            add( b, "OnUIEvent", 0x62E950u, [ &ev ]( const ui_params_t& p ) { ev.on_ui_event( p ); } );
        }
        else
        {
            for ( const ui_event_fact_t& f : k_ui_event_facts )
                if ( f.m_vtable == b.m_vtable )
                {
                    const int index = b.m_index;
                    const std::uint32_t method = f.m_method;
                    b.m_methods.push_back( { f.m_hash, f.m_method } );
                    b.m_map.m_handlers.push_back( { f.m_hash, [ this, index, method ]( const ui_params_t& p )
                    {
                        ++m_unknown_events;
                        trace( ui_trace_kind_t::event, index, std::string( ), method, p, false );
                    } } );
                }
        }
    }

    // 0x505690 over the bound maps (pre-order = registration order).
    void broadcast( std::uint32_t hash, const ui_params_t& p )
    {
        trace( ui_trace_kind_t::broadcast, -1, std::string( ), hash, p, true );
        for ( auto& b : m_bound )
            b->m_map.handle( hash, p );
    }

    bool bound_input( const entity_t& owner, const char* plug ) const
    {
        const std::uint32_t vt = owner.m_ctor ? owner.m_ctor->m_vtable : 0u;
        if ( vt == k_ui_button_vtable )
            for ( const char* name : { "SetFocus", "SetStringID", "Enable", "Disable", "IsEnabled" } )
                if ( std::strcmp( plug, name ) == 0 )
                    return true;
        return vt == k_ui_if_vtable && std::strcmp( plug, "Trigger" ) == 0;
    }

    ui_ret_t run_input( entity_t& owner, const char* plug, const ui_params_t& p )
    {
        ui_ret_t out;
        const std::uint32_t vt = owner.m_ctor ? owner.m_ctor->m_vtable : 0u;
        if ( vt == k_ui_if_vtable )
        {
            // 0x5DDE60.
            const ui_ret_t answer = fire_named( owner, "Operand", ui_params_t( ) );
            if ( answer.m_words[ 0 ] == 3u )
                fire_named( owner, ( answer.m_words[ 1 ] & 0xFFu ) != 0 ? "True" : "False", p );
            out.m_known = false;  // +4 left as the caller's
            return out;
        }
        c_ui_button* button = m_bound[ static_cast< std::size_t >( index_of( owner ) ) ]->m_button.get( );
        // 0x4428E0 / 0x442980 / 0x4429E0 / 0x442A00: VuRetVal { 0, 0 }; 0x442A20: { 3, Enabled }.
        if ( std::strcmp( plug, "SetFocus" ) == 0 )
            button->set_focus( );
        else if ( std::strcmp( plug, "SetStringID" ) == 0 )
            button->set_string_id( p );
        else if ( std::strcmp( plug, "Enable" ) == 0 )
            button->enable( );
        else if ( std::strcmp( plug, "Disable" ) == 0 )
            button->disable( );
        else
        {
            out.m_words[ 0 ] = 3u;
            out.m_words[ 1 ] = button->is_enabled( ) ? 1u : 0u;
        }
        return out;
    }

    ui_bind_hooks_t m_hooks;
    std::map< const entity_t*, int > m_index;
};
