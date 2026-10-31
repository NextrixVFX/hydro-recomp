#pragma once

#include <workspace/core/ui/button.hxx>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// A screen's UI events: how input reaches the front end's entities.
//
// Every entity's event map (VuEventMap at entity +12: a std::map from the
// FNV-1a hash of an event name to the handler registered under it, 0x5156E0)
// is ui_event_map_t; dispatch 0x5155C0 runs the handler for a hash, if any.
// VuUIScreenEntity (create 0x6299A0, vtable 0x6F7140):
//   ScreenEnter (slot 6 0x629A70) / ScreenExit (slot 7 0x629B00): "OnUIEvent"
//     carrying the string "ScreenEnter" / "ScreenExit" to each DIRECT child
//     (0x5011A0 - the children's maps only; not the screen's own, not deeper);
//   its tick (slot 12 0x629B90) with input allowed: the input manager is asked
//     about ten names in a fixed order and each one down posts (0x629F10)
//     "OnUIEvent" [ its UI name, int 0 ] to the screen's own map and then each
//     direct child; then (input allowed or not) "OnUITick" [ float dt ] to the
//     direct children. The draw half of slot 12 / slot 13 is not here.
// VuUIEventEntity (ctor 0x62E750, handler 0x62E950): on "OnUIEvent" whose
// string equals its "Event" property (+96, std::string ==), fires Trigger
// with the params left after that string.

struct ui_event_map_t
{
    std::vector< std::pair< std::uint32_t, std::function< void( const ui_params_t& ) > > > m_handlers;

    // 0x5156E0: registered under the name's hash (operator[]: a second
    // registration of a name replaces the first).
    void add( const char* name, std::function< void( const ui_params_t& ) > fn )
    {
        const std::uint32_t h = fsm_hash( name );
        for ( auto& e : m_handlers )
            if ( e.first == h )
            {
                e.second = std::move( fn );
                return;
            }
        m_handlers.push_back( { h, std::move( fn ) } );
    }

    // 0x5155C0.
    void handle( std::uint32_t hash, const ui_params_t& params ) const
    {
        for ( const auto& e : m_handlers )
            if ( e.first == hash )
            {
                e.second( params );
                return;
            }
    }
};

// The button's three event handlers as its ctor 0x441170 registers them
// (0x443800: OnUISetFocus, OnUITick, OnUIEvent).
inline void ui_button_events( c_ui_button& b, ui_event_map_t& map )
{
    map.add( "OnUISetFocus", [ &b ]( const ui_params_t& p ) { b.on_set_focus( p ); } );
    map.add( "OnUITick", [ &b ]( const ui_params_t& ) { b.on_tick( ); } );
    map.add( "OnUIEvent", [ &b ]( const ui_params_t& p ) { b.on_event( p ); } );
}

class c_ui_event
{
public:
    explicit c_ui_event( std::string event ) : m_event( std::move( event ) ) { }

    // 0x62E950.
    void on_ui_event( const ui_params_t& params )
    {
        ui_params_reader_t r( params );
        const char* name = r.read_string( );
        if ( m_event != name )
            return;
        ui_params_t rest;
        rest.m_size = r.m_remaining;
        std::memcpy( rest.m_bytes, r.m_at, static_cast< std::size_t >( r.m_remaining ) );
        if ( m_fire )
            m_fire( rest );
    }

    void events( ui_event_map_t& map )
    {
        map.add( "OnUIEvent", [ this ]( const ui_params_t& p ) { on_ui_event( p ); } );
    }

    std::string m_event;                                 // +96 "Event"
    std::function< void( const ui_params_t& ) > m_fire;  // the Trigger output
};

class c_ui_screen
{
public:
    // 0x629A70 / 0x629B00.
    void screen_enter( ) { to_children( "OnUIEvent", ui_string_params( "ScreenEnter" ) ); }
    void screen_exit( ) { to_children( "OnUIEvent", ui_string_params( "ScreenExit" ) ); }

    // 0x629B90's input half.
    void tick( float dt, bool input )
    {
        if ( input )
        {
            static const char* const devices[ 10 ] = { "Select", "Back", "Up", "Down", "Left", "Right", "Up2", "Down2", "Left2", "Right2" };
            static const char* const events[ 10 ] = { "InputSelect", "InputBack", "InputUp", "InputDown", "InputLeft", "InputRight",
                                                      "InputUp", "InputDown", "InputLeft", "InputRight" };
            for ( int k = 0; k < 10; ++k )
                if ( m_input && m_input( 0, devices[ k ] ) )
                    post_input( events[ k ] );
        }
        ui_params_t p;
        std::uint32_t bits;
        std::memcpy( &bits, &dt, 4 );
        ui_params_add( p, 2, bits );  // 0x405120: a float record
        to_children( "OnUITick", p );
    }

    ui_event_map_t m_self;                    // the screen's own map (+12)
    std::vector< const ui_event_map_t* > m_children;  // the direct children's maps, in +0x38 order
    // HOOK: the input manager (g 0x759038 slot 9: (pad, name) -> down).
    std::function< bool( int, const char* ) > m_input;

private:
    static ui_params_t ui_string_params( const char* text )
    {
        ui_params_t p;
        p.add_string( text );
        return p;
    }

    // 0x5011A0.
    void to_children( const char* event, const ui_params_t& p ) const
    {
        const std::uint32_t h = fsm_hash( event );
        for ( const ui_event_map_t* child : m_children )
            child->handle( h, p );
    }

    // 0x629F10.
    void post_input( const char* name )
    {
        ui_params_t p = ui_string_params( name );
        ui_params_add( p, 0, 0 );
        m_self.handle( fsm_hash( "OnUIEvent" ), p );
        to_children( "OnUIEvent", p );
    }
};
