#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/ui/config.hxx>
#include <workspace/core/ui/fsm.hxx>

#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <string>
#include <vector>

// The front end's screen flow: VuFrontEndGameMode (ctor 0x4BEEA0, vtable
// 0x746204), transliterated. It owns the screen names and the screen stack,
// answers the UI actions' broadcasts (VuPushScreenUIAction 0x4718C0 posts
// "OnPushScreen" + the screen name, VuPopScreenUIAction 0x4719C0 "OnPopScreen",
// VuSetScreenUIAction 0x471600 "OnSetScreen"), and walks a VuFSM:
//   Begin -(FadeManagerInactive)-> Run -(NextScreenSet)-> Transition -("")-> Run
//   Run -(NextGameModeSet)-> End -(FadeManagerInactive)-> Exit
//   Run -(SnapViewActive)-> SnapView -(SnapViewInactive)-> Run
// The mode's fields (offsets from the mode): +52 the next game mode, +76 the
// next screen, +100 the current screen, +124 the stack (std::deque of
// std::string, back = top), +144 the current screen's project, +152 the
// settings depth, +156 TransitionDuration (ConstantDB VuDBAsset/UI), +164 /
// +168 the title voice-over and its "preload pending" byte.

// A VuParams (0x84 bytes): 128 bytes of records then the byte count at +128.
// A string record is the u32 type 4 then the text and its NUL.
struct ui_params_t
{
    std::uint8_t m_bytes[ 128 ] = { };
    int m_size = 0;

    void add_string( const char* text )
    {
        const std::uint32_t type = 4;
        const std::size_t length = std::strlen( text ) + 1;
        if ( static_cast< std::size_t >( m_size ) + 4 + length > 128 )
            return;  // VuParams' add drops a record that does not fit (the actions' 0x80 test)
        std::memcpy( m_bytes + m_size, &type, 4 );
        std::memcpy( m_bytes + m_size + 4, text, length );
        m_size += static_cast< int >( 4 + length );
    }
};

// VuParams' reader { cursor, remaining }; readString 0x505260: with bytes left
// and a type-4 record, steps over the type and the text (NUL included) and
// returns the text; otherwise "" and nothing is consumed.
struct ui_params_reader_t
{
    const std::uint8_t* m_at;
    int m_remaining;

    explicit ui_params_reader_t( const ui_params_t& p ) : m_at( p.m_bytes ), m_remaining( p.m_size ) { }

    const char* read_string( )
    {
        std::uint32_t type = 0xFFFFFFFFu;
        if ( m_remaining <= 0 )
            return "";
        std::memcpy( &type, m_at, 4 );
        if ( type != 4 )
            return "";
        m_at += 4;
        m_remaining -= 4;
        const char* text = reinterpret_cast< const char* >( m_at );
        const int length = static_cast< int >( std::strlen( text ) ) + 1;
        m_remaining -= length;
        m_at += length;
        return text;
    }
};

// The engine the mode reaches. Everything here is a singleton retail reads
// through a global; the platform layer and the platform services are HOOKS.
struct front_end_engine_t
{
    // g_vufademanager 0x758FC4: +4 the fade duration, +8 the fade value, +12
    // its rate, +16 a frame-skip count (ui_fade_t below, with its tick).
    struct ui_fade_t
    {
        float m_duration = 0.0f;
        float m_value = 0.0f;
        float m_rate = 0.0f;
        std::uint32_t m_skip = 0;

        // The create 0x4AED60: duration 1.0, the rest 0 (the ConstantDB's
        // FadeManagerDefaultDuration replaces +4 at init 0x4AEDB0).
        static ui_fade_t created( )
        {
            ui_fade_t f;
            f.m_duration = 1.0f;
            return f;
        }

        // The tick 0x4AEE50 (phase "Final"; its own dt argument is unused):
        // a pending skip is spent first; else, with a rate that is not 0
        // (ucomiss: NaN counts as not equal), value += rate x the tick
        // manager's delta (g 0x759008 slot 15 - `delta` here); past 1.0
        // "OnFadeOutComplete" is broadcast and value = 1, below 0
        // "OnFadeInComplete" and value = 0, the rate 0 either way. `broadcast`
        // is VuEventManager's by-name broadcast 0x505650 (empty VuParams).
        void tick( float delta, const std::function< void( const char* ) >& broadcast )
        {
            if ( m_skip != 0 )
            {
                --m_skip;
                return;
            }
            if ( m_rate == 0.0f )
                return;
            m_value = m_rate * delta + m_value;
            if ( m_value > 1.0f )
            {
                if ( broadcast )
                    broadcast( "OnFadeOutComplete" );
                m_value = 1.0f;
                m_rate = 0.0f;
            }
            else if ( 0.0f > m_value )
            {
                if ( broadcast )
                    broadcast( "OnFadeInComplete" );
                m_value = 0.0f;
                m_rate = 0.0f;
            }
        }

        // The draw 0x4AEF70 runs only while value > 0: a full-screen quad of
        // colour (0, 0, 0, value) - the quad itself is the rendering layer.
        bool draws( ) const { return m_value > 0.0f; }
    } m_fade;
    // g_vugameutil 0x758FF8: +100 the last screen operation ("set" / "push" /
    // "pop"), +124 the screen it named; +68 the music channel and +72 its
    // volume (End's tick); +97 a byte that blocks the settings command.
    std::string m_operation;
    std::string m_screen;
    bool m_music_channel = false;
    float m_music_volume = 0.0f;
    bool m_settings_blocked = false;
    // g_vumessageboxmanager 0x758FD4 +52: a message box is up.
    bool m_message_box = false;
    // g_vucontrolmethodmanager 0x758FC0 +24: the control method (Run's tick
    // passes "< 2" to the screen as its input flag).
    std::uint32_t m_control_method = 0;

    // HOOKS. D3D11 (g_vumetrod3d11gfx 0x75907C slot 16): the display size.
    std::function< void( int&, int& ) > m_display;
    // FMOD (getEvent [0x64C348], then Event::start on success).
    std::function< void( const char* ) > m_sound;
    // FMOD Channel::setVolume on the music channel (End's tick 0x4BAB10).
    std::function< void( float ) > m_set_music_volume;
    // The project loader 0x4B4360 (VuProject by asset name; 0 = none) and,
    // for a loaded project, what 0x4C0550 / 0x4C0690 do with it: entity
    // repository add / remove (g 0x759080 slots 6 / 7), VuEntity init 0x500B10
    // / uninit 0x500B80, the "OnProjectInitialized" broadcast, the render-
    // thread sync, the release. The project's root is a VuUIScreenEntity when
    // it is a screen: m_screen_tick is its slot 12 (0x629B90: dt, input flag),
    // m_screen_draw its slot 13 (0x629D60: alpha), m_screen_layer its +92.
    std::function< int( const std::string& ) > m_load_project;
    std::function< void( int ) > m_init_project;
    std::function< void( int ) > m_release_project;
    std::function< bool( int ) > m_is_screen;
    std::function< void( int, float, bool ) > m_screen_tick;
    std::function< void( int, float ) > m_screen_draw;
    std::function< void( int, int ) > m_screen_layer;
    // Platform services (analytics 0x758F9C slot 6; the corrupt-save message
    // box Begin's enter raises; the title voice-over's ActionGameData preload
    // 0x4D2DE0; SnapView's tick-manager pause 0x759008 slots 11 / 12 and its
    // audio pause) - re-made for the new platform, not 1:1.
    std::function< void( const char*, const std::string& ) > m_analytics;
    std::function< void( ) > m_save_check;
    std::function< void( ) > m_preload_action_game;
    std::function< void( bool ) > m_snap_pause;
    // The enter's other platform reaches, by retail address / role (0x530BB0 on g 0x759060 - UNREAD, arg 1 on
    // enter; the audio pause g_vuaudio +20 slot 14 (1 then 0); the game mode manager g 0x758FF0 slot 9; the
    // profile and cloud save managers 0x5547F0 / 0x4ABD60).
    std::function< void( const char*, int ) > m_platform;
    // The title voice-over 0x50FBF0 (FMOD; true = an event was made, then started) and the byte 0x758FCD that
    // says it already played this run.
    std::function< bool( const char* ) > m_title_voice;
    bool m_title_played = false;
};

// core/world action.hxx (included at the end of this file: it includes this one for the fade type).
inline void action_end_enter( front_end_engine_t::ui_fade_t& f );
inline void action_end_tick( const front_end_engine_t::ui_fade_t& f, bool channel, float volume, const std::function< void( float ) >& set_volume );

class c_front_end
{
public:
    // The ctor 0x4BEEA0: the eight event handlers (registered on the mode's
    // event map 0x5156E0, in this order), TransitionDuration from the
    // ConstantDB, the six states with their delegates, then the transitions.
    // `constant_db` is ConstantDB's root ([[g_vugameutil + 4] + 0x58]): the
    // ctor reads VuDBAsset / UI / TransitionDuration through 0x4FBDF0 - tag 1
    // an int converted, 2 the float, 7 an int64 converted, anything else 0.
    c_front_end( front_end_engine_t& engine, const value_t& constant_db ) : m_engine( engine )
    {
        const value_t& d = member( member( member( constant_db, "VuDBAsset" ), "UI" ), "TransitionDuration" );
        m_transition_duration = d.tag == 1 ? static_cast< float >( d.integer )
                              : d.tag == 2 ? d.real
                              : d.tag == 7 ? static_cast< float >( d.int64 )
                                           : 0.0f;
        m_events = {
            { "OnStartActionGame", 0x4BFDE0u, [ this ]( const ui_params_t& p ) { on_start_action_game( p ); } },
            { "OnSetScreen", 0x4BFE20u, [ this ]( const ui_params_t& p ) { on_set_screen( p ); } },
            { "OnSetScreenStack", 0x4BFF70u, [ this ]( const ui_params_t& p ) { on_set_screen_stack( p ); } },
            { "OnPushScreen", 0x4C0130u, [ this ]( const ui_params_t& p ) { on_push_screen( p ); } },
            { "OnPopScreen", 0x4C02A0u, [ this ]( const ui_params_t& p ) { on_pop_screen( p ); } },
            { "OnReloadScreen", 0x4C0420u, [ this ]( const ui_params_t& p ) { on_reload_screen( p ); } },
            { "OnShutdownApp", 0x4BFE00u, [ this ]( const ui_params_t& p ) { on_shutdown_app( p ); } },
            { "OnSettingsCommand", 0x4C0440u, [ this ]( const ui_params_t& p ) { on_settings_command( p ); } },
        };
        fsm_state_t& begin = m_fsm.add_state( "Begin" );
        begin.m_enter = [ this ] { begin_enter( ); };
        begin.m_draw = [ this ] { draw_screen( ); };
        begin.m_address[ 0 ] = 0x4C0780u;
        begin.m_address[ 3 ] = 0x4C0730u;
        fsm_state_t& end = m_fsm.add_state( "End" );
        end.m_enter = [ this ] { end_enter( ); };
        end.m_tick = [ this ]( float dt ) { end_tick( dt ); };
        end.m_draw = [ this ] { draw_screen( ); };
        end.m_address[ 0 ] = 0x4BAAD0u;
        end.m_address[ 2 ] = 0x4BAB10u;
        end.m_address[ 3 ] = 0x4C0730u;
        fsm_state_t& run = m_fsm.add_state( "Run" );
        run.m_tick = [ this ]( float dt ) { run_tick( dt ); };
        run.m_draw = [ this ] { draw_screen( ); };
        run.m_address[ 2 ] = 0x4C08B0u;
        run.m_address[ 3 ] = 0x4C0730u;
        m_fsm.add_state( "Exit" );
        fsm_state_t& transition = m_fsm.add_state( "Transition" );
        transition.m_enter = [ this ] { transition_enter( ); };
        transition.m_draw = [ this ] { draw_screen( ); };
        transition.m_address[ 0 ] = 0x4C0520u;
        transition.m_address[ 3 ] = 0x4C0730u;
        fsm_state_t& snap = m_fsm.add_state( "SnapView" );
        snap.m_enter = [ this ] { snap_enter( ); };
        snap.m_exit = [ this ] { snap_exit( ); };
        snap.m_draw = [ this ] { snap_draw( ); };
        snap.m_address[ 0 ] = 0x4C0930u;
        snap.m_address[ 1 ] = 0x4C09A0u;
        snap.m_address[ 3 ] = 0x4C09E0u;
        m_fsm.add_transition( "Begin", "Run", "FadeManagerInactive" );
        m_fsm.add_transition( "Run", "End", "NextGameModeSet" );
        m_fsm.add_transition( "Run", "Transition", "NextScreenSet" );
        m_fsm.add_transition( "Run", "SnapView", "SnapViewActive" );
        m_fsm.add_transition( "Transition", "Run", "" );
        m_fsm.add_transition( "SnapView", "Run", "SnapViewInactive" );
        m_fsm.add_transition( "End", "Exit", "FadeManagerInactive" );
    }

    struct event_t
    {
        const char* m_name;
        std::uint32_t m_address;  // the retail handler
        std::function< void( const ui_params_t& ) > m_handler;
    };

    // A broadcast reaching the mode's event map: the handler registered under
    // the name (the map is keyed by the name's FNV-1a hash). Unknown: ignored.
    void handle( const char* name, const ui_params_t& params ) { handle( fsm_hash( name ), params ); }

    // The same by the hash a broadcast carries (VuEventManager_broadcast 0x505690).
    void handle( std::uint32_t hash, const ui_params_t& params )
    {
        for ( event_t& e : m_events )
            if ( fsm_hash( e.m_name ) == hash )
            {
                e.m_handler( params );
                return;
            }
    }

    // slot 4 0x4BFC10 (dt): the conditions, evaluate, the state's tick, the
    // time, the title voice-over preload, then the next game mode's name when
    // the machine is in "Exit" (else "").
    const std::string& tick( float dt )
    {
        m_fsm.set_condition( "FadeManagerInactive", !( m_engine.m_fade.m_rate != 0.0f ) );
        m_fsm.set_condition( "NextGameModeSet", !m_mode.empty( ) );
        m_fsm.set_condition( "NextScreenSet", !m_next.empty( ) );
        int width = 0;
        int height = 0;
        if ( m_engine.m_display )
            m_engine.m_display( width, height );
        const float ratio = static_cast< float >( width ) / static_cast< float >( height );
        const bool snapped = ratio < 1.0f;  // comiss 1.0, ratio; seta (a NaN ratio: not snapped)
        m_fsm.set_condition( "SnapViewActive", snapped );
        m_fsm.set_condition( "SnapViewInactive", !snapped );  // sete on the same byte
        m_fsm.evaluate( );
        if ( m_fsm.current( )->m_tick )
            m_fsm.current( )->m_tick( dt );
        m_fsm.m_time = m_fsm.m_time + dt;
        if ( m_preload_pending && !m_title_voice )
        {
            m_preload_pending = false;
            if ( m_engine.m_preload_action_game )
                m_engine.m_preload_action_game( );
        }
        static const std::string none;
        return m_fsm.current( )->m_name == "Exit" ? m_mode : none;
    }

    // slot 5 0x4BFDD0: the current state's draw.
    void draw( )
    {
        if ( m_fsm.current( )->m_draw )
            m_fsm.current( )->m_draw( );
    }

    // The FSM start at the end of the enter 0x4BF630.
    void start( ) { m_fsm.start( ); }

    // slot 3 0x4BF9B0, the mode's exit: the current state's exit delegate (FSM
    // +0 -> state +0x24, its slot 2); gameutil's audio stop / release 0x4D3100
    // (FMOD channel and sound, the asset factory's release - a HOOK); unload
    // 0x4C0690; the SnapMode project (+148) released the same way, inlined;
    // the game mode manager's slot 10 (g 0x758FF0) and 0x530BB0( 0 ) on g
    // 0x759060 (UNREAD) - hooks; then the data step ui_game_config over
    // gameutil +80 (`root`) with the next game mode (+52).
    ui_game_config_t exit( value_t& root, const ui_game_env_t& env )
    {
        if ( m_fsm.current( ) && m_fsm.current( )->m_exit )
            m_fsm.current( )->m_exit( );
        platform( "0x4D3100", 0 );
        unload( );
        if ( m_snap_project )
        {
            if ( m_engine.m_release_project )
                m_engine.m_release_project( m_snap_project );
            m_snap_project = 0;
        }
        platform( "mode manager", 10 );
        platform( "0x530BB0", 0 );
        return ui_game_config( m_mode, root, env );
    }

    // slot 2 0x4BF630, the mode's enter. `game_data` is gameutil +80. The
    // first screen: "Screens/Start" when InitialScreen (a string member, else
    // "") is empty; otherwise "Screens/Main", with InitialScreen pushed on top
    // of it when GameResults exists (any non-null tag). The top is loaded
    // (0x4C0550) and popped. Then the title voice-over the first time (its
    // preload byte +168 set either way; later entries preload ActionGameData
    // at once), the FSM start, the profile / cloud managers.
    void enter( const value_t& game_data )
    {
        platform( "0x530BB0", 1 );
        platform( "audio pause", 1 );
        platform( "mode manager", 9 );
        m_snap_project = m_engine.m_load_project ? m_engine.m_load_project( "Screens/SnapMode" ) : 0;
        if ( m_snap_project && m_engine.m_init_project )
            m_engine.m_init_project( m_snap_project );
        const value_t& initial = member( game_data, "InitialScreen" );
        const std::string first = initial.tag == 4 ? initial.text : std::string( );
        if ( first.empty( ) )
            m_stack.push_back( "Screens/Start" );
        else
        {
            m_stack.push_back( "Screens/Main" );
            if ( member( game_data, "GameResults" ).tag != 0 )
                m_stack.push_back( first );
        }
        load( m_stack.back( ) );
        m_stack.pop_back( );
        if ( m_engine.m_title_played )
        {
            if ( m_engine.m_preload_action_game )
                m_engine.m_preload_action_game( );
        }
        else
        {
            m_engine.m_title_played = true;
            m_preload_pending = true;
            m_title_voice = m_engine.m_title_voice && m_engine.m_title_voice( "Global/Voice/vo_title_HydroHurricane" );
        }
        m_fsm.start( );
        platform( "profile", 0 );
        platform( "cloud", 0 );
        platform( "audio pause", 0 );
    }

    front_end_engine_t& m_engine;
    c_fsm m_fsm;
    std::vector< event_t > m_events;
    std::string m_mode;     // +52
    std::string m_next;     // +76
    std::string m_current;  // +100
    std::deque< std::string > m_stack;  // +124
    int m_project = 0;      // +144 (0 = none)
    int m_snap_project = 0; // +148 (Screens/SnapMode)
    int m_depth = 0;        // +152
    float m_transition_duration = 0.0f;  // +156
    bool m_title_voice = false;      // +164 (the voice-over event is live)
    bool m_preload_pending = false;  // +168

private:
    void platform( const char* what, int arg )
    {
        if ( m_engine.m_platform )
            m_engine.m_platform( what, arg );
    }

    void play( const char* name )
    {
        if ( m_engine.m_sound )
            m_engine.m_sound( name );
    }

    // 0x4BFE20: next = the name; gameutil op "set" / screen = next; screen_next.
    void on_set_screen( const ui_params_t& params )
    {
        ui_params_reader_t r( params );
        m_next = r.read_string( );
        m_engine.m_operation = "set";
        m_engine.m_screen = m_next;
        play( "Global/UI_and_Menu/screen_next" );
    }

    // 0x4BFF70: the stack emptied (pop_back until empty); next = the first
    // name; then for every further string record: next is pushed and becomes
    // that record's text. The loop reads the type word straight off the cursor
    // (no readString), twice per record, and stops at the end of the bytes or
    // the first record that is not a string.
    void on_set_screen_stack( const ui_params_t& params )
    {
        while ( !m_stack.empty( ) )
            m_stack.pop_back( );
        ui_params_reader_t r( params );
        m_next = r.read_string( );
        const std::uint8_t* at = r.m_at;
        int remaining = r.m_remaining;
        for ( ;; )
        {
            std::uint32_t type = 0xFFFFFFFFu;
            if ( remaining <= 0 )
                break;
            std::memcpy( &type, at, 4 );
            if ( type != 4 )
                break;
            m_stack.push_back( m_next );
            type = 0xFFFFFFFFu;
            std::memcpy( &type, at, 4 );
            const char* text = "";
            if ( type == 4 )
            {
                text = reinterpret_cast< const char* >( at + 4 );
                const int length = static_cast< int >( std::strlen( text ) );
                at += length + 5;
                remaining = remaining - 4 - ( length + 1 );
            }
            m_next = text;
        }
    }

    // 0x4C0130: next = the name; the CURRENT screen is pushed; op "push";
    // screen_next; a non-zero settings depth counts the push.
    void on_push_screen( const ui_params_t& params )
    {
        ui_params_reader_t r( params );
        m_next = r.read_string( );
        m_stack.push_back( m_current );
        m_engine.m_operation = "push";
        m_engine.m_screen = m_next;
        play( "Global/UI_and_Menu/screen_next" );
        if ( m_depth )
            ++m_depth;
    }

    // 0x4C02A0: with a stack: next = its top, popped; op "pop"; screen_back;
    // a non-zero settings depth counts the pop. An empty stack: nothing.
    void on_pop_screen( const ui_params_t& )
    {
        if ( m_stack.empty( ) )
            return;
        m_next = m_stack.back( );
        m_stack.pop_back( );
        m_engine.m_operation = "pop";
        m_engine.m_screen = m_next;
        play( "Global/UI_and_Menu/screen_back" );
        if ( m_depth )
            --m_depth;
    }

    // 0x4C0420: next = current.
    void on_reload_screen( const ui_params_t& ) { m_next = m_current; }

    // 0x4BFDE0 / 0x4BFE00: the next game mode.
    void on_start_action_game( const ui_params_t& ) { m_mode = "Action"; }
    void on_shutdown_app( const ui_params_t& ) { m_mode = "Shutdown"; }

    // 0x4C0440: with no message box and the gameutil byte clear - outside the
    // settings (depth 0) push the screen; inside, unwind to depth 1 (one
    // pop_back per level, when the stack has one) and SET it. Depth is 1 after.
    void on_settings_command( const ui_params_t& params )
    {
        if ( m_engine.m_message_box || m_engine.m_settings_blocked )
            return;
        if ( m_depth )
        {
            while ( m_depth >= 2 )
            {
                if ( !m_stack.empty( ) )
                    m_stack.pop_back( );
                --m_depth;
            }
            on_set_screen( params );
        }
        else
            on_push_screen( params );
        m_depth = 1;
    }

    // Begin enter 0x4C0780: a fade in (duration not > 0: value 0, rate 0; else
    // value 1, rate -1 / duration), then the corrupt-save message box (a
    // platform service).
    void begin_enter( )
    {
        if ( !( m_engine.m_fade.m_duration > 0.0f ) )  // comiss + jbe: a NaN duration takes this branch
        {
            m_engine.m_fade.m_value = 0.0f;
            m_engine.m_fade.m_rate = 0.0f;
        }
        else
        {
            m_engine.m_fade.m_value = 1.0f;
            m_engine.m_fade.m_rate = -1.0f / m_engine.m_fade.m_duration;
        }
        if ( m_engine.m_save_check )
            m_engine.m_save_check( );
    }

    // End enter 0x4BAAD0 / tick 0x4BAB10: VuActionGameMode's member functions
    // (the front end binds the same two) - ONE recreation, core/world action.hxx
    // action_end_enter / action_end_tick (declared above, included at the end).
    void end_enter( ) { action_end_enter( m_engine.m_fade ); }
    void end_tick( float ) { action_end_tick( m_engine.m_fade, m_engine.m_music_channel, m_engine.m_music_volume, m_engine.m_set_music_volume ); }

    bool screen_loaded( ) const { return m_project && m_engine.m_is_screen && m_engine.m_is_screen( m_project ); }

    // Run tick 0x4C08B0: a screen ticks (dt, control method < 2) unless a
    // message box is up.
    void run_tick( float dt )
    {
        if ( screen_loaded( ) && !m_engine.m_message_box && m_engine.m_screen_tick )
            m_engine.m_screen_tick( m_project, dt, m_engine.m_control_method < 2u );
    }

    // The draws 0x4C0730: a screen draws at alpha 1.
    void draw_screen( )
    {
        if ( screen_loaded( ) && m_engine.m_screen_draw )
            m_engine.m_screen_draw( m_project, 1.0f );
    }

    // Transition enter 0x4C0520: unload the current screen, load next, next = "".
    void transition_enter( )
    {
        unload( );
        load( m_next );
        m_next.clear( );
    }

    // 0x4C0690: current = ""; the project's uninit / removal / release.
    void unload( )
    {
        m_current.clear( );
        if ( m_project )
        {
            if ( m_engine.m_release_project )
                m_engine.m_release_project( m_project );
            m_project = 0;
        }
    }

    // 0x4C0550: current = the name; its project loaded and (unless its root
    // is already initialised) initialised; the "MenuShown" analytics event.
    void load( const std::string& name )
    {
        m_current = name;
        m_project = m_engine.m_load_project ? m_engine.m_load_project( name ) : 0;
        if ( m_project && m_engine.m_init_project )
            m_engine.m_init_project( m_project );
        if ( m_engine.m_analytics )
            m_engine.m_analytics( "MenuShown", name );
    }

    // SnapView 0x4C0930 / 0x4C09A0: the tick manager and audio paused while
    // the view is snapped (platform); its draw 0x4C09E0 sets the screen's +92
    // to 5 then draws it at alpha 1.
    void snap_enter( )
    {
        if ( m_engine.m_snap_pause )
            m_engine.m_snap_pause( true );
    }

    void snap_exit( )
    {
        if ( m_engine.m_snap_pause )
            m_engine.m_snap_pause( false );
    }

    void snap_draw( )
    {
        if ( !screen_loaded( ) )
            return;
        if ( m_engine.m_screen_layer )
            m_engine.m_screen_layer( m_project, 5 );
        if ( m_engine.m_screen_draw )
            m_engine.m_screen_draw( m_project, 1.0f );
    }
};

#include <workspace/core/world/action.hxx>
