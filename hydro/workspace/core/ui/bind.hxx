#pragma once

#include <workspace/core/ui/controls.hxx>
#include <workspace/core/ui/hud.hxx>
#include <workspace/core/ui/screen.hxx>
#include <workspace/core/ui/slider.hxx>
#include <workspace/core/ui/ticker.hxx>
#include <workspace/core/ui/wiring.hxx>

#include <array>
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
// - VuInfoUITextEntity (0x73BC5C): the ticker (ticker.hxx: OnSetInfoStringID
//   0x446480, OnClearInfoStringID 0x446450, OnUITick 0x446580 - its delta is the
//   tick manager's, a hook), Screen Scale / Width from OUR loaded properties, the
//   font (0x626B80's answer) an input.
// - VuSetInfoStringUIAction (0x73E818): Trigger -> slot 12 0x471E30
//   (disassembled): a VuParams with the "Info String ID" string record (only
//   while strlen + 1 + 4 <= 0x80) broadcast as "OnSetInfoStringID"; its VuRetVal
//   +0 = 0 (+4 the caller's).
// - the screen actions and VuSetGameModeUIAction (wiring.hxx / action.hxx).
// - the four VuSliderEntity subclasses (slider.hxx): OnUISetFocus 0x46F8F0,
//   OnUITick 0x46F940, OnUIEvent 0x46FCC0; inputs Show / Hide / SetFocus; the
//   settings manager, the touch device and the FMOD categories are hooks.
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
    { 0x756828u, 0xD5127E2Du, 0x5EFA60u },  // VuOnProjectInitializedEntity
    { 0x73DF3Cu, 0x4CF1B9C6u, 0x404AB2u },  // VuUISubtitlesSettingEntity OnUISetFocus (thunk -> slot 14 0x475230)
    { 0x73DF3Cu, 0x5E798927u, 0x4077DEu },  // its OnUITick (thunk -> slot 15 0x475280)
    { 0x73DF3Cu, 0xFE191C9Eu, 0x4077E8u },  // its OnUIEvent (thunk -> slot 16 0x475640)
};

inline constexpr std::uint32_t k_ui_screen_vtable = 0x6F7140u;
inline constexpr std::uint32_t k_ui_container_vtable = 0x6F74F0u;
inline constexpr std::uint32_t k_ui_button_vtable = 0x73B7B0u;
inline constexpr std::uint32_t k_ui_event_vtable = 0x6F7308u;
inline constexpr std::uint32_t k_ui_if_vtable = 0x754CD4u;
inline constexpr std::uint32_t k_ui_info_vtable = 0x73BC5Cu;
inline constexpr std::uint32_t k_ui_set_info_vtable = 0x73E818u;

// One step of the frame's trace, at the points retail's case logs them.
enum class ui_trace_kind_t
{
    fire,       // an output plug's 0x5023A0 entered: owner, plug, params
    input,      // an input plug's 0x404BE0 entered: owner, plug, params, recreated
    broadcast,  // 0x505690 entered: hash, params
    sound,      // FMOD getEvent: the name
    event,      // a registered handler not recreated: owner, its method
    string,     // VuStringDB slot 7 (the ticker's hook): the id
    measure,    // the font measure 0x5373D0 (the ticker's hook): the text, xmm3 bits, the font
    touches,    // the touch device's count (slot 6, the slider's hook)
    touch,      // the touch device's point (slot 7): the index
    category,   // FMOD getCategory (the volume sliders' hook): the name
    hook,       // the HUD's gameInitialize / gameRelease hooks: the label, the entity
    query,      // the input manager's slot 9 (the pause tick's "Pause"): the name, the answer
    button,     // the input manager's vt+0x44 (an on-screen button): the name
    axis,       // the input manager's vt+0x48 (an on-screen axis): the name, the value bits
};

struct ui_trace_t
{
    ui_trace_kind_t m_kind;
    int m_index = -1;          // the owner's pre-order index
    std::string m_name;        // plug / sound name
    std::uint32_t m_word = 0;  // broadcast hash / event method
    ui_params_t m_params;
    bool m_known = true;
    std::uint32_t m_font = 0;  // measure: the font
};

// The race HUD (batch 16, census case hud_census on Race Pre / Game / PostGameHUD).
// Bound only when ui_bind_hooks_t::m_hud is set (the HUD driver, core/ui
// overlay.hxx) - except, since batch 18, the inputs that read no game state:
// the text bases' / images' Show / Hide / SetAlpha / SetStringID and
// VuTriggerForwarderEntity's Trigger run in every binding (case ui_frames runs
// them live in retail on the Screens projects). Types, handlers and inputs
// (all disassembled):
// - the text base (VuUITextBaseEntity ctor 0x627070) of VuHUDElementEntity
//   0x738C64, VuUITextEntity 0x6F7434, VuHUDHiddenPackageToastEntity 0x738BDC:
//   inputs Show 0x446C80 (+0x64 = 1), Hide 0x446FB0 (+0x64 = 0), SetAlpha
//   0x627AA0 (+0x108 = the first record when a float, else 0); VuUITextEntity's
//   SetStringID 0x62F480 (+0x10C = readString 0x505260); VuRetVal +0 = 0.
// - VuUIImageEntity 0x6F7484: Show 0x440B50 / Hide 0x6284C0 (+0x6C), SetAlpha
//   0x629270 (+0xB4, as the text's).
// - the toast: OnPackageFound 0x424600, OnUITick 0x4248E0 (controls.hxx);
//   gameInitialize slot 6 0x4245F0: +0x64 = 0.
// - VuHUDSpeedometerEntity 0x739250: OnUITick (thunk 0x4077E3) -> slot 12
//   0x42B860 (hud.hxx); VuHUDMiniMapEntity 0x738E10: OnUITick -> slot 12
//   0x427B60, which reads the dt record into a local and nothing else.
// - VuOnProjectInitializedEntity 0x756828: OnProjectInitialized 0x5EFA60 fires
//   "Trigger" with an EMPTY VuParams.
// - VuControlMethodEntity 0x7398E4: OnControlMethodChanged 0x42EC80 fires
//   "MethodChanged" with the event's params; input Trigger 0x42EAE0: the
//   control-method manager g 0x758FC0 +0x18 (read without a null test) 0 / 1 / 2
//   / 3 fires IsGamePad / IsKeyboard / IsTilt / IsTouch with the Trigger's
//   params, any other value nothing; VuRetVal +0 = 0.
// - VuTriggerForwarderEntity 0x754B00: Trigger 0x5E0FC0 fires "OnTrigger" with
//   its params (findPlug; nothing when absent); VuRetVal +0 = 0.
// - VuIsCircuitTrack 0x73994C: Get 0x42E1F0 = { 3, track manager g 0x758FF4
//   slot 8 (built) && slot 9 (circuit) } (the byte +4 only).
// - VuStartLightsOutAction 0x737664: Trigger -> slot 12 0x405470: the broadcast
//   "OnStartLightsOut" with an empty VuParams; VuRetVal +0 = 0.
// - the on-screen axis / button 0x739020 / 0x739058: OnUITick 0x429E10
//   (controls.hxx ui_hud_onscreen_tick); the pause / recover buttons 0x738AC4 /
//   0x738B24: OnUITick through the thunk 0x4077E3 -> slot 12 0x4255B0
//   (ui_hud_button_tick: pause slots 13 0x4261B0 / 14 0x4262A0, recover 13
//   0x425DC0 / 14 0x425F10), their touch callbacks 0x425400 / 0x425490 /
//   0x4254B0 (hud_touch - the device's dispatch is the caller's) and the
//   callback list (gameInitialize 0x425390 / gameRelease 0x4253D0 over
//   m_touch_device).
// The world's / the audio layer's inputs (VuMultiDelayEntity In 0x5E0660,
// VuAudioEventEntity Start 0x5F42D0 / Stop 0x5F4310) go to the m_external hook.
struct ui_hud_env_t
{
    ui_hud_world_t m_world;               // g 0x758FB8 / 0x758FE0 (hud.hxx)
    bool m_track_built = false;           // g 0x758FF4 slot 8 0x4D0720
    bool m_track_circuit = false;         // slot 9 0x4D0730
    std::int32_t m_control_method = 0;    // g 0x758FC0 +0x18
    // batch 17 (additive): the pause button's visibility reads the boat
    // manager's +0x28 (0x426330); the recover button's reads the control-method
    // manager's presence (0x425F50); its ticks read the local boat's driver.
    std::uint32_t m_manager_28 = 0;
    bool m_method_present = true;         // g 0x758FC0 != 0
    ui_hud_recover_driver_t m_driver;
};

inline constexpr std::uint32_t k_ui_hud_element_vtable = 0x738C64u;
inline constexpr std::uint32_t k_ui_text_vtable = 0x6F7434u;
inline constexpr std::uint32_t k_ui_hud_toast_vtable = 0x738BDCu;
inline constexpr std::uint32_t k_ui_image_vtable = 0x6F7484u;
inline constexpr std::uint32_t k_ui_hud_speedometer_vtable = 0x739250u;
inline constexpr std::uint32_t k_ui_hud_minimap_vtable = 0x738E10u;
inline constexpr std::uint32_t k_ui_project_initialized_vtable = 0x756828u;
inline constexpr std::uint32_t k_ui_control_method_vtable = 0x7398E4u;
inline constexpr std::uint32_t k_ui_trigger_forwarder_vtable = 0x754B00u;
inline constexpr std::uint32_t k_ui_is_circuit_vtable = 0x73994Cu;
inline constexpr std::uint32_t k_ui_start_lights_vtable = 0x737664u;
inline constexpr std::uint32_t k_ui_hud_pause_vtable = 0x738AC4u;
inline constexpr std::uint32_t k_ui_hud_recover_vtable = 0x738B24u;
inline constexpr std::uint32_t k_ui_onscreen_axis_vtable = 0x739020u;
inline constexpr std::uint32_t k_ui_onscreen_button_vtable = 0x739058u;
inline constexpr std::uint32_t k_ui_multi_delay_vtable = 0x754B68u;

// A text base's / an image's runtime fields the HUD's inputs drive.
struct ui_hud_shown_t
{
    bool m_visible = true;     // +0x64 (image +0x6C) "Visible"
    float m_alpha = 1.0f;      // +0x108 (image +0xB4)
    std::string m_string_id;   // VuUITextEntity +0x10C "String ID"
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
    std::unique_ptr< ui_info_ticker_t > m_ticker;
    ui_slider_hooks_t m_slider_hooks;
    std::unique_ptr< c_ui_slider > m_slider;
    // the race HUD's (m_hud set)
    std::unique_ptr< ui_hud_shown_t > m_shown;              // text base / image
    std::unique_ptr< c_ui_hud_element > m_element;          // VuHUDElementEntity's Type / text
    std::unique_ptr< ui_hud_toast_t > m_toast;
    std::unique_ptr< c_ui_hud_speedometer > m_speedometer;
    std::unique_ptr< ui_hud_button_t > m_hud_button;        // pause / recover
    std::unique_ptr< ui_hud_onscreen_t > m_onscreen;        // on-screen axis / button
};

struct ui_bind_hooks_t
{
    // HOOK: the input manager g 0x759038 slot 9: (pad, name) -> down.
    std::function< bool( int, const char* ) > m_input;
    // HOOK: an input plug not recreated - its VuRetVal (default { 0, 0 }).
    std::function< ui_ret_t( const entity_t&, const char* ) > m_answer;
    value_t* m_root = nullptr;  // gameutil +80
    // The ticker's: VuStringDB slot 7 and the font measure 0x5373D0 (HOOKS, ticker.hxx),
    // the tick manager's delta (g 0x759008 slot 15, a HOOK) and the font 0x626B80
    // answers for an entity (an input: the fonts are the rendering layer's).
    ui_info_hooks_t m_info;
    std::function< float( ) > m_delta;
    std::function< std::uint32_t( const entity_t& ) > m_font;
    // The sliders': HOOKS the touch device g 0x759070 (slot 6 count, slot 7 point),
    // an input the inverse crop matrix (VuUIImpl +76; null = identity), the settings
    // manager g 0x758FE0 (null = none: the getters' defaults).
    std::function< int( ) > m_touch_count;
    std::function< void( int, float[ 2 ] ) > m_touch_point;
    const float* m_inverse = nullptr;
    ui_settings_t* m_settings = nullptr;
    // The race HUD's (null: not bound - see ui_hud_env_t): the game it reads,
    // VuStringDB slot 6 (the toast's template - a HOOK), the inputs owned by the
    // world / the audio layer (multi delay In, audio Start / Stop: entity, plug,
    // params -> VuRetVal; unset = { 0, 0 }) and the broadcasts' other
    // subscribers (the race's: VuRaceGame / the drivers registered theirs before
    // the HUD loaded, so they run first).
    const ui_hud_env_t* m_hud = nullptr;
    std::function< std::string( const char* ) > m_string_db;
    std::function< ui_ret_t( entity_t&, const char*, const ui_params_t& ) > m_external;
    std::function< void( std::uint32_t, const ui_params_t& ) > m_broadcast_out;
    // The race HUD's controls (batch 17): the touch device's callback list
    // (null: no list - the pause tick finds nothing), the crop matrix the touch
    // rects are anchored over (null: ui_crop_t's default), the input manager's
    // vt+0x44 / vt+0x48 (HOOKS), the FMOD event start (HOOK; traced as sound
    // either way) and the recover press's driver +0xB8 = 1 (the world's).
    ui_touch_device_t* m_touch_device = nullptr;
    const ui_crop_t* m_crop = nullptr;
    std::function< void( int, const char* ) > m_input_button;
    std::function< void( int, const char*, float ) > m_input_axis;
    std::function< void( const char* ) > m_sound;
    std::function< void( ) > m_recover;
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
        m_hud_hooks.m_string = m_hooks.m_string_db;
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
        if ( m_hooks.m_external )
            m_wiring.m_answer_params = m_hooks.m_external;
        m_screen.m_input = m_hooks.m_input;
        m_info.m_string = [ this ]( const std::string& id )
        {
            trace( ui_trace_kind_t::string, -1, id, 0u, ui_params_t( ), true );
            return m_hooks.m_info.m_string ? m_hooks.m_info.m_string( id ) : std::string( );
        };
        m_info.m_measure = [ this ]( std::uint32_t font, const std::string& text, float aspect )
        {
            std::uint32_t bits;
            std::memcpy( &bits, &aspect, 4 );
            trace( ui_trace_kind_t::measure, -1, text, bits, ui_params_t( ), true );
            m_trace.back( ).m_font = font;
            return m_hooks.m_info.m_measure ? m_hooks.m_info.m_measure( font, text, aspect ) : 0.0f;
        };
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

    static bool shown_vtable( std::uint32_t vt )
    {
        return vt == k_ui_hud_element_vtable || vt == k_ui_text_vtable || vt == k_ui_hud_toast_vtable || vt == k_ui_image_vtable;
    }

    static void make_shown( ui_bound_t& b )
    {
        b.m_shown = std::make_unique< ui_hud_shown_t >( );
        if ( const ui_slot_t* s = ui_bind_slot( *b.m_entity, "Visible" ) )
            b.m_shown->m_visible = ( s->m_bits[ 0 ] & 0xFFu ) != 0;
        if ( const ui_slot_t* s = ui_bind_slot( *b.m_entity, "String ID" ) )
            b.m_shown->m_string_id = s->m_text;
    }

    // Batch 18: outside the race HUD (no m_hud - the Screens) the text bases' /
    // images' Show / Hide / SetAlpha / SetStringID and VuTriggerForwarderEntity's
    // Trigger 0x5E0FC0 run too (they read no game state); their event maps hold
    // nothing. The kinds that read ui_hud_env_t stay HUD-only.
    bool bind_shown( ui_bound_t& b )
    {
        if ( shown_vtable( b.m_vtable ) && b.m_vtable != k_ui_hud_element_vtable && b.m_vtable != k_ui_hud_toast_vtable )
        {
            make_shown( b );
            return true;
        }
        return false;
    }

    static bool shown_input( std::uint32_t vt, const char* plug )
    {
        if ( shown_vtable( vt ) )
            for ( const char* name : { "Show", "Hide", "SetAlpha" } )
                if ( std::strcmp( plug, name ) == 0 )
                    return true;
        if ( vt == k_ui_text_vtable && std::strcmp( plug, "SetStringID" ) == 0 )
            return true;
        return vt == k_ui_trigger_forwarder_vtable && std::strcmp( plug, "Trigger" ) == 0;
    }

    // The race HUD's types (see ui_hud_env_t); true when `b` is one of them.
    bool bind_hud( ui_bound_t& b )
    {
        entity_t& e = *b.m_entity;
        const std::uint32_t vt = b.m_vtable;
        if ( shown_vtable( vt ) )
            make_shown( b );
        if ( vt == k_ui_hud_element_vtable )
        {
            b.m_element = std::make_unique< c_ui_hud_element >( m_hud_hooks );
            if ( const ui_slot_t* s = ui_bind_slot( e, "Type" ) )
                std::memcpy( &b.m_element->m_type, s->m_bits, 4 );
            return true;
        }
        if ( vt == k_ui_text_vtable || vt == k_ui_image_vtable )
            return true;
        if ( vt == k_ui_hud_toast_vtable )
        {
            b.m_toast = std::make_unique< ui_hud_toast_t >( );
            ui_hud_toast_t& t = *b.m_toast;
            if ( const ui_slot_t* s = ui_bind_slot( e, "Total Time" ) )
                std::memcpy( &t.m_total, s->m_bits, 4 );
            if ( const ui_slot_t* s = ui_bind_slot( e, "Fade Time" ) )
                std::memcpy( &t.m_fade, s->m_bits, 4 );
            ui_bound_t* self = &b;
            // its ctor 0x424360: OnPackageFound 0x424600, OnUITick 0x4248E0 (the base's +0x64 / +0x108 are m_shown's)
            add( b, "OnPackageFound", 0x424600u, [ this, self ]( const ui_params_t& p )
            {
                sync_toast( *self, true );
                ui_hud_toast_found( *self->m_toast, p, [ this ]( const char* key )
                {
                    trace( ui_trace_kind_t::string, -1, key, 0u, ui_params_t( ), true );
                    return m_hooks.m_string_db ? m_hooks.m_string_db( key ) : std::string( );
                } );
                sync_toast( *self, false );
            } );
            add( b, "OnUITick", 0x4248E0u, [ this, self ]( const ui_params_t& p )
            {
                sync_toast( *self, true );
                ui_hud_toast_tick( *self->m_toast, p );
                sync_toast( *self, false );
            } );
            return true;
        }
        if ( vt == k_ui_hud_speedometer_vtable )
        {
            b.m_speedometer = std::make_unique< c_ui_hud_speedometer >( );
            c_ui_hud_speedometer& s = *b.m_speedometer;
            add( b, "OnUITick", 0x4077E3u, [ this, &s ]( const ui_params_t& p ) { s.on_tick( p.m_bytes, p.m_size, m_hooks.m_hud->m_world ); } );
            return true;
        }
        if ( vt == k_ui_hud_minimap_vtable )
        {
            add( b, "OnUITick", 0x4077E3u, []( const ui_params_t& ) { } );   // slot 12 0x427B60: the dt read into a local only
            return true;
        }
        if ( vt == k_ui_project_initialized_vtable )
        {
            add( b, "OnProjectInitialized", 0x5EFA60u, [ this, &e ]( const ui_params_t& ) { fire_named( e, "Trigger", ui_params_t( ) ); } );
            return true;
        }
        if ( vt == k_ui_control_method_vtable )
        {
            add( b, "OnControlMethodChanged", 0x42EC80u, [ this, &e ]( const ui_params_t& p ) { fire_named( e, "MethodChanged", p ); } );
            return true;
        }
        const auto take = [ &e ]( const char* name, void* out, std::size_t bytes )
        {
            if ( const ui_slot_t* s = ui_bind_slot( e, name ) )
                std::memcpy( out, s->m_bits, bytes );
        };
        const auto take_anchor = [ &take ]( ui_anchor_t& a )
        {
            take( "Horizontal Anchor", &a.m_horizontal, 4 );
            take( "Vertical Anchor", &a.m_vertical, 4 );
            take( "Horizontal Ratio", &a.m_h_ratio, 4 );
            take( "Vertical Ratio", &a.m_v_ratio, 4 );
        };
        const char* const touch_names[ 4 ] = { "Touch X", "Touch Y", "Touch Width", "Touch Height" };
        if ( vt == k_ui_hud_pause_vtable || vt == k_ui_hud_recover_vtable )
        {
            b.m_hud_button = std::make_unique< ui_hud_button_t >( );
            ui_hud_button_t& hb = *b.m_hud_button;
            hb.m_recover = vt == k_ui_hud_recover_vtable;
            take( "Screen Scale", hb.m_scale, 8 );
            for ( int k = 0; k < 4; ++k )
                take( touch_names[ k ], &hb.m_touch[ k ], 4 );
            take_anchor( hb.m_anchor );
            if ( hb.m_recover )
            {
                take( "Boing Rate", &hb.m_rate, 4 );
                take( "Boing Scale Min", &hb.m_scale_min, 4 );
                take( "Boing Scale Max", &hb.m_scale_max, 4 );
                take( "Boing Duration", &hb.m_duration, 4 );
            }
            const int index = b.m_index;
            // ctor 0x424A20 registers OnUITick -> the thunk 0x4077E3 = slot 12 0x4255B0
            add( b, "OnUITick", 0x4077E3u, [ this, &hb, index ]( const ui_params_t& p ) { hud_button_tick( hb, index, p ); } );
            return true;
        }
        if ( vt == k_ui_onscreen_axis_vtable || vt == k_ui_onscreen_button_vtable )
        {
            b.m_onscreen = std::make_unique< ui_hud_onscreen_t >( );
            ui_hud_onscreen_t& o = *b.m_onscreen;
            o.m_axis = vt == k_ui_onscreen_axis_vtable;
            take( "Screen Scale", o.m_scale, 8 );
            for ( int k = 0; k < 4; ++k )
                take( touch_names[ k ], &o.m_touch[ k ], 4 );
            take_anchor( o.m_anchor );
            const char* const enables[ 4 ] = { "Enable for Tilt", "Enable for Touch", "Enable for GamePad", "Enable for Keyboard" };
            for ( int k = 0; k < 4; ++k )
                take( enables[ k ], &o.m_enable[ k ], 1 );
            if ( const ui_slot_t* s = ui_bind_slot( e, o.m_axis ? "Axis Name" : "Button Name" ) )
                o.m_name = s->m_text;
            if ( o.m_axis )
                take( "Axis Direction", &o.m_direction, 4 );
            // ctor 0x4292F0 registers OnUITick 0x429E10
            add( b, "OnUITick", 0x429E10u, [ this, &o ]( const ui_params_t& ) { hud_onscreen_tick( o ); } );
            return true;
        }
        return false;
    }

    const ui_crop_t& hud_crop( ) const
    {
        static const ui_crop_t k_default;
        return m_hooks.m_crop ? *m_hooks.m_crop : k_default;
    }

    // slot 15: pause 0x426330 / recover 0x425F50 over the env.
    bool hud_button_visible( const ui_hud_button_t& hb ) const
    {
        const ui_hud_env_t& env = *m_hooks.m_hud;
        ui_hud_control_inputs_t in;
        in.m_race_present = env.m_world.m_manager;
        in.m_race_28 = env.m_manager_28;
        in.m_method_present = env.m_method_present;
        in.m_method = env.m_control_method;
        return hb.m_recover ? ui_hud_recover_visible( in ) : ui_hud_pause_visible( in );
    }

    ui_hud_pause_hooks_t hud_pause_hooks( )
    {
        ui_hud_pause_hooks_t ph;
        ph.m_input = [ this ]( int pad, const char* name )
        {
            const bool down = m_hooks.m_input ? m_hooks.m_input( pad, name ) : false;
            trace( ui_trace_kind_t::query, -1, name, down ? 1u : 0u, ui_params_t( ), true );
            return down;
        };
        ph.m_sound = [ this ]( const char* name )
        {
            trace( ui_trace_kind_t::sound, -1, name, 0u, ui_params_t( ), true );
            if ( m_hooks.m_sound )
                m_hooks.m_sound( name );
        };
        ph.m_broadcast = [ this ]( std::uint32_t hash, const ui_params_t& p ) { broadcast( hash, p ); };
        return ph;
    }

    void hud_button_tick( ui_hud_button_t& hb, int index, const ui_params_t& p )
    {
        ui_hud_button_class_t cls;
        cls.m_visible = [ this, &hb ] { return hud_button_visible( hb ); };
        if ( !hb.m_recover )
        {
            const ui_hud_pause_hooks_t ph = hud_pause_hooks( );
            cls.m_press = [ ph ] { ui_hud_pause_press( ph ); };
            cls.m_tick = [ this, &hb, ph ]( float )
            {
                if ( m_hooks.m_touch_device )
                    ui_hud_pause_tick( *m_hooks.m_touch_device, &hb, ph );
            };
        }
        else
        {
            cls.m_press = [ this, index ]
            {
                ui_hud_recover_press( m_hooks.m_hud->m_driver, [ this, index ]
                {
                    trace( ui_trace_kind_t::hook, index, "recover", 0u, ui_params_t( ), true );   // driver +0xB8 = 1
                    if ( m_hooks.m_recover )
                        m_hooks.m_recover( );
                } );
            };
            cls.m_tick = [ this, &hb ]( float dt ) { ui_hud_recover_tick( hb, m_hooks.m_hud->m_driver, dt ); };
        }
        ui_hud_button_tick( hb, hud_crop( ), p, cls );
    }

    int traced_touch_count( )
    {
        trace( ui_trace_kind_t::touches, -1, std::string( ), 0u, ui_params_t( ), true );
        return m_hooks.m_touch_count ? m_hooks.m_touch_count( ) : 0;
    }

    void traced_touch_point( int index, float out[ 2 ] )
    {
        trace( ui_trace_kind_t::touch, -1, std::string( ), static_cast< std::uint32_t >( index ), ui_params_t( ), true );
        if ( m_hooks.m_touch_point )
            m_hooks.m_touch_point( index, out );
    }

    void hud_onscreen_tick( ui_hud_onscreen_t& o )
    {
        const ui_hud_env_t& env = *m_hooks.m_hud;
        ui_hud_onscreen_hooks_t h;
        h.m_method_present = env.m_method_present;
        h.m_method = env.m_control_method;
        h.m_touch_count = [ this ] { return traced_touch_count( ); };
        h.m_touch_point = [ this ]( int k, float out[ 2 ] ) { traced_touch_point( k, out ); };
        h.m_inverse = m_hooks.m_inverse;
        h.m_button = [ this ]( int pad, const char* name )
        {
            trace( ui_trace_kind_t::button, -1, name, 0u, ui_params_t( ), true );
            if ( m_hooks.m_input_button )
                m_hooks.m_input_button( pad, name );
        };
        h.m_set_axis = [ this ]( int pad, const char* name, float value )
        {
            std::uint32_t bits;
            std::memcpy( &bits, &value, 4 );
            trace( ui_trace_kind_t::axis, -1, name, bits, ui_params_t( ), true );
            if ( m_hooks.m_input_axis )
                m_hooks.m_input_axis( pad, name, value );
        };
        ui_hud_onscreen_tick( o, hud_crop( ), h );
    }

    // The toast's text-base fields live in m_shown (the inputs') and its own struct (the handlers').
    static void sync_toast( ui_bound_t& b, bool in )
    {
        if ( in )
        {
            b.m_toast->m_visible = b.m_shown->m_visible;
            b.m_toast->m_alpha = b.m_shown->m_alpha;
        }
        else
        {
            b.m_shown->m_visible = b.m_toast->m_visible;
            b.m_shown->m_alpha = b.m_toast->m_alpha;
        }
    }

    void bind( ui_bound_t& b )
    {
        entity_t& e = *b.m_entity;
        if ( m_hooks.m_hud && bind_hud( b ) )
            return;
        if ( !m_hooks.m_hud && bind_shown( b ) )
            return;
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
        else if ( b.m_vtable == k_ui_info_vtable )
        {
            b.m_ticker = std::make_unique< ui_info_ticker_t >( ui_info_ticker_of( e ) );
            ui_info_ticker_t& t = *b.m_ticker;
            std::array< float, 2 > scale = { 0.0f, 0.0f };
            float width = 0.0f;
            if ( const ui_slot_t* s = ui_bind_slot( e, "Screen Scale" ) )
                std::memcpy( scale.data( ), s->m_bits, 8 );
            if ( const ui_slot_t* s = ui_bind_slot( e, "Width" ) )
                std::memcpy( &width, s->m_bits, 4 );
            const std::uint32_t font = m_hooks.m_font ? m_hooks.m_font( e ) : 0u;
            // 0x446120 registers OnClearInfoStringID, OnSetInfoStringID, OnUITick.
            add( b, "OnClearInfoStringID", 0x446450u, [ &t ]( const ui_params_t& ) { ui_info_clear( t ); } );
            add( b, "OnSetInfoStringID", 0x446480u, [ this, &t, scale, width, font ]( const ui_params_t& p )
            {
                ui_params_reader_t r( p );
                ui_info_set( t, r.read_string( ), scale.data( ), width, font, m_info );
            } );
            add( b, "OnUITick", 0x446580u, [ this, &t, width ]( const ui_params_t& )
            {
                ui_info_tick( t, width, m_hooks.m_delta ? m_hooks.m_delta( ) : 0.0f );
            } );
        }
        else if ( ui_is_slider_vtable( b.m_vtable ) )
        {
            const ui_slider_t s = ui_bind_slider( e, b.m_vtable );
            ui_slider_hooks_t& h = b.m_slider_hooks;
            h.m_broadcast = [ this ]( std::uint32_t hash, const ui_params_t& p ) { broadcast( hash, p ); };
            h.m_fire = [ this, &e ]( const char* plug, const ui_params_t& p ) { fire_named( e, plug, p ); };
            h.m_connections = [ &e ]( const char* plug ) -> int
            {
                const script_component_t* sc = script_of( e );
                const int k = plug_index( e, plug );
                return sc && k >= 0 ? static_cast< int >( sc->m_plugs[ static_cast< std::size_t >( k ) ].m_links.size( ) ) : 0;
            };
            h.m_sound = [ this ]( const char* name ) { trace( ui_trace_kind_t::sound, -1, name, 0u, ui_params_t( ), true ); };
            h.m_category = [ this ]( const char* name, float ) { trace( ui_trace_kind_t::category, -1, name, 0u, ui_params_t( ), true ); };
            if ( m_hooks.m_touch_count )
                h.m_touch_count = [ this ]
                {
                    trace( ui_trace_kind_t::touches, -1, std::string( ), 0u, ui_params_t( ), true );
                    return m_hooks.m_touch_count( );
                };
            h.m_touch_point = [ this ]( int index, float out[ 2 ] )
            {
                trace( ui_trace_kind_t::touch, -1, std::string( ), static_cast< std::uint32_t >( index ), ui_params_t( ), true );
                if ( m_hooks.m_touch_point )
                    m_hooks.m_touch_point( index, out );
            };
            h.m_inverse = m_hooks.m_inverse;
            h.m_settings = m_hooks.m_settings;
            b.m_slider = std::make_unique< c_ui_slider >( ui_entity_id( ui_long_name( e ) ), s, h );
            c_ui_slider& slider = *b.m_slider;
            // 0x46E300 registers OnUISetFocus, OnUITick, OnUIEvent.
            add( b, "OnUISetFocus", 0x46F8F0u, [ &slider ]( const ui_params_t& p ) { slider.on_set_focus( p ); } );
            add( b, "OnUITick", 0x46F940u, [ &slider ]( const ui_params_t& ) { slider.on_tick( ); } );
            add( b, "OnUIEvent", 0x46FCC0u, [ &slider ]( const ui_params_t& p ) { slider.on_event( p ); } );
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
        if ( m_hooks.m_broadcast_out )
            m_hooks.m_broadcast_out( hash, p );
        for ( auto& b : m_bound )
            b->m_map.handle( hash, p );
    }

    bool bound_input( const entity_t& owner, const char* plug ) const
    {
        const std::uint32_t vt = owner.m_ctor ? owner.m_ctor->m_vtable : 0u;
        if ( shown_input( vt, plug ) && ( m_hooks.m_hud || ( vt != k_ui_hud_element_vtable && vt != k_ui_hud_toast_vtable ) ) )
            return true;
        if ( m_hooks.m_hud )
        {
            if ( ( vt == k_ui_control_method_vtable || vt == k_ui_start_lights_vtable ) && std::strcmp( plug, "Trigger" ) == 0 )
                return true;
            if ( vt == k_ui_is_circuit_vtable && std::strcmp( plug, "Get" ) == 0 )
                return true;
        }
        if ( vt == k_ui_button_vtable )
            for ( const char* name : { "SetFocus", "SetStringID", "Enable", "Disable", "IsEnabled" } )
                if ( std::strcmp( plug, name ) == 0 )
                    return true;
        if ( ui_is_slider_vtable( vt ) )
            for ( const char* name : { "Show", "Hide", "SetFocus" } )
                if ( std::strcmp( plug, name ) == 0 )
                    return true;
        return ( vt == k_ui_if_vtable || vt == k_ui_set_info_vtable ) && std::strcmp( plug, "Trigger" ) == 0;
    }

public:
    // The slider's authored fields from OUR loaded properties (ctor 0x46E300's names).
    static ui_slider_t ui_bind_slider( const entity_t& e, std::uint32_t vtable )
    {
        ui_slider_t s;
        s.m_vtable = vtable;
        const auto real = [ &e ]( const char* name, float& out )
        {
            if ( const ui_slot_t* v = ui_bind_slot( e, name ) )
                std::memcpy( &out, v->m_bits, 4 );
        };
        const auto pair = [ &e ]( const char* name, float out[ 2 ] )
        {
            if ( const ui_slot_t* v = ui_bind_slot( e, name ) )
                std::memcpy( out, v->m_bits, 8 );
        };
        const auto byte = [ &e ]( const char* name, std::uint8_t& out )
        {
            if ( const ui_slot_t* v = ui_bind_slot( e, name ) )
                out = static_cast< std::uint8_t >( v->m_bits[ 0 ] & 0xFFu );
        };
        const auto word = [ &e ]( const char* name, std::int32_t& out )
        {
            if ( const ui_slot_t* v = ui_bind_slot( e, name ) )
                std::memcpy( &out, v->m_bits, 4 );
        };
        std::uint8_t visible = 1;
        byte( "Visible", visible );
        s.m_visible = visible != 0;
        pair( "Screen Scale", s.m_scale );
        const char* const frame[ 4 ] = { "FrameX", "FrameY", "FrameWidth", "FrameHeight" };
        const char* const text[ 4 ] = { "TextX", "TextY", "TextWidth", "TextHeight" };
        const char* const dst[ 4 ] = { "Bar DstX", "Bar DstY", "Bar DstWidth", "Bar DstHeight" };
        const char* const src[ 4 ] = { "Bar SrcX", "Bar SrcY", "Bar SrcWidth", "Bar SrcHeight" };
        for ( int k = 0; k < 4; ++k )
        {
            real( frame[ k ], s.m_frame[ k ] );
            real( text[ k ], s.m_text[ k ] );
            real( dst[ k ], s.m_dst[ k ] );
            real( src[ k ], s.m_src[ k ] );
        }
        word( "Horizontal Alignment", s.m_h_align );
        word( "Vertical Alignment", s.m_v_align );
        byte( "No Clip", s.m_no_clip );
        byte( "Single Line", s.m_single_line );
        byte( "Wordbreak", s.m_wordbreak );
        if ( const ui_slot_t* v = ui_bind_slot( e, "String ID" ) )
            s.m_string_id = v->m_text;
        pair( "Bar Dst Delta", s.m_dst_delta );
        pair( "Bar Src Delta", s.m_src_delta );
        pair( "Bar Src On Delta", s.m_src_on_delta );
        word( "Bar Count", s.m_count );
        return s;
    }

private:
    // The race HUD's inputs (see ui_hud_env_t). VuRetVal +0 = 0 and +4 the
    // caller's (m_known false) unless written.
    ui_ret_t run_hud_input( entity_t& owner, std::uint32_t vt, const char* plug, const ui_params_t& p )
    {
        ui_ret_t out;
        out.m_known = false;
        ui_bound_t& b = *m_bound[ static_cast< std::size_t >( index_of( owner ) ) ];
        if ( b.m_shown && std::strcmp( plug, "Show" ) == 0 )
            b.m_shown->m_visible = true;
        else if ( b.m_shown && std::strcmp( plug, "Hide" ) == 0 )
            b.m_shown->m_visible = false;
        else if ( b.m_shown && std::strcmp( plug, "SetAlpha" ) == 0 )
        {
            // 0x627AA0 / 0x629270: 0 unless the first record is a float
            float a = 0.0f;
            std::uint32_t type = 0xFFFFFFFFu;
            if ( p.m_size > 0 )
            {
                std::memcpy( &type, p.m_bytes, 4 );
                if ( type == 2 )
                    std::memcpy( &a, p.m_bytes + 4, 4 );
            }
            b.m_shown->m_alpha = a;
        }
        else if ( b.m_shown && std::strcmp( plug, "SetStringID" ) == 0 )
        {
            ui_params_reader_t r( p );
            b.m_shown->m_string_id = r.read_string( );
        }
        else if ( vt == k_ui_trigger_forwarder_vtable )
        {
            if ( plug_index( owner, "OnTrigger" ) >= 0 )
                fire_named( owner, "OnTrigger", p );
        }
        else if ( vt == k_ui_control_method_vtable )
        {
            static const char* const k_names[ 4 ] = { "IsGamePad", "IsKeyboard", "IsTilt", "IsTouch" };
            const std::int32_t method = m_hooks.m_hud->m_control_method;
            if ( method >= 0 && method < 4 )
                fire_named( owner, k_names[ method ], p );
        }
        else if ( vt == k_ui_is_circuit_vtable )
        {
            out.m_words[ 0 ] = 3u;
            out.m_words[ 1 ] = m_hooks.m_hud->m_track_built && m_hooks.m_hud->m_track_circuit ? 1u : 0u;   // the byte +4 only
        }
        else if ( vt == k_ui_start_lights_vtable )
            broadcast( fsm_hash( "OnStartLightsOut" ), ui_params_t( ) );
        return out;
    }

    // 0x500B10 (gameInitialize) / 0x500B80 (gameRelease) over the tree. The
    // components' slots 9 / 10 are nullsubs for every component type the race
    // HUDs hold (VuScriptComponent, VuUIDrawComponent, Vu2dLayoutComponent).
    void game_initialize( entity_t& e )
    {
        if ( e.m_flags & 4u )
            return;
        if ( !( e.m_flags & 8u ) )
            trace( ui_trace_kind_t::hook, index_of( e ), "repository_add", 0u, ui_params_t( ), true );
        for ( auto& child : e.m_children )
            game_initialize( *child );
        const std::uint32_t vt = e.m_ctor ? e.m_ctor->m_vtable : 0u;
        if ( vt == k_ui_hud_pause_vtable || vt == k_ui_hud_recover_vtable )
        {
            trace( ui_trace_kind_t::hook, index_of( e ), "touch_add", 0u, ui_params_t( ), true );        // 0x425390 -> 0x53FB90
            ui_bound_t& b = *m_bound[ static_cast< std::size_t >( index_of( e ) ) ];
            if ( m_hooks.m_touch_device && b.m_hud_button )
                ui_touch_add( *m_hooks.m_touch_device, b.m_hud_button.get( ), 0u );
        }
        else if ( vt == k_ui_hud_toast_vtable && m_hooks.m_hud )
            m_bound[ static_cast< std::size_t >( index_of( e ) ) ]->m_shown->m_visible = false;            // 0x4245F0
        else if ( vt == k_ui_multi_delay_vtable )
            trace( ui_trace_kind_t::hook, index_of( e ), "multi_delay_init", 0u, ui_params_t( ), true ); // 0x5E0610 (the world's)
        e.m_flags |= 4u;
    }

    void game_release( entity_t& e )
    {
        if ( !( e.m_flags & 4u ) )
            return;
        for ( auto& child : e.m_children )
            game_release( *child );
        const std::uint32_t vt = e.m_ctor ? e.m_ctor->m_vtable : 0u;
        if ( vt == k_ui_hud_pause_vtable || vt == k_ui_hud_recover_vtable )
        {
            trace( ui_trace_kind_t::hook, index_of( e ), "touch_remove", 0u, ui_params_t( ), true );        // 0x4253D0 -> 0x53FA60
            ui_bound_t& b = *m_bound[ static_cast< std::size_t >( index_of( e ) ) ];
            if ( m_hooks.m_touch_device && b.m_hud_button )
                ui_touch_remove( *m_hooks.m_touch_device, b.m_hud_button.get( ) );
        }
        else if ( vt == k_ui_multi_delay_vtable )
            trace( ui_trace_kind_t::hook, index_of( e ), "multi_delay_release", 0u, ui_params_t( ), true ); // 0x43A420 (the world's)
        if ( !( e.m_flags & 8u ) )
            trace( ui_trace_kind_t::hook, index_of( e ), "repository_remove", 0u, ui_params_t( ), true );
        e.m_flags &= ~4u;
    }

public:
    // The race HUD driver's entry points (core/ui overlay.hxx).
    // gameInitialize 0x500B10 then 0x501260 "OnProjectInitialized" (each
    // entity's own map, pre-order; an empty VuParams).
    void hud_initialize( )
    {
        game_initialize( *m_bound.front( )->m_entity );
        const std::uint32_t h = fsm_hash( "OnProjectInitialized" );
        for ( auto& b : m_bound )
            b->m_map.handle( h, ui_params_t( ) );
    }
    void hud_release( ) { game_release( *m_bound.front( )->m_entity ); }
    // VuHUDEntity slot 12 0x423A20: VuParams { float dt } -> "OnUITick" to the
    // DIRECT children (0x5011A0).
    void hud_tick( float dt )
    {
        ui_params_t p;
        std::uint32_t bits;
        std::memcpy( &bits, &dt, 4 );
        ui_params_add( p, 2, bits );
        const std::uint32_t h = fsm_hash( "OnUITick" );
        for ( auto& child : m_bound.front( )->m_entity->m_children )
            m_bound[ static_cast< std::size_t >( m_index[ child.get( ) ] ) ]->m_map.handle( h, p );
    }
    // VuEventManager_broadcast 0x505690 from outside (the race's events).
    void post( std::uint32_t hash, const ui_params_t& p ) { broadcast( hash, p ); }
    // An output plug fired from outside (the world's VuMultiDelayEntity outputs).
    ui_ret_t fire_plug( entity_t& owner, const char* plug, const ui_params_t& p ) { return fire_named( owner, plug, p ); }
    int bound_index( const entity_t& e ) const { return index_of( e ); }
    // A pause / recover button's touch callback (VuTouchCallback vtable 0x738B80,
    // delivered by the caller - the device's dispatch is not read): kind 0 down
    // 0x425400 (the device point), 1 up 0x425490, 2 move 0x4254B0 (the device's
    // touch 0 through m_touch_point). False when `e` is not such a button.
    bool hud_touch( const entity_t& e, int kind, const float point[ 2 ] )
    {
        const int k = index_of( e );
        if ( k < 0 || !m_bound[ static_cast< std::size_t >( k ) ]->m_hud_button || !m_hooks.m_hud )
            return false;
        ui_hud_button_t& hb = *m_bound[ static_cast< std::size_t >( k ) ]->m_hud_button;
        static const float k_identity[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        const float* inverse = m_hooks.m_inverse ? m_hooks.m_inverse : k_identity;
        if ( kind == 0 )
            ui_hud_button_touch_down( hb, hud_button_visible( hb ), inverse, point );
        else if ( kind == 1 )
            ui_hud_button_touch_up( hb, hud_button_visible( hb ) );
        else
        {
            float device[ 2 ] = { 0.0f, 0.0f };
            traced_touch_point( 0, device );
            ui_hud_button_touch_move( hb, inverse, device );
        }
        return true;
    }

private:
    ui_ret_t run_input( entity_t& owner, const char* plug, const ui_params_t& p )
    {
        ui_ret_t out;
        const std::uint32_t vt = owner.m_ctor ? owner.m_ctor->m_vtable : 0u;
        if ( ( m_hooks.m_hud && vt != k_ui_if_vtable && vt != k_ui_set_info_vtable && !ui_is_slider_vtable( vt ) && vt != k_ui_button_vtable ) ||
             shown_input( vt, plug ) )
            return run_hud_input( owner, vt, plug, p );
        if ( vt == k_ui_if_vtable )
        {
            // 0x5DDE60.
            const ui_ret_t answer = fire_named( owner, "Operand", ui_params_t( ) );
            if ( answer.m_words[ 0 ] == 3u )
                fire_named( owner, ( answer.m_words[ 1 ] & 0xFFu ) != 0 ? "True" : "False", p );
            out.m_known = false;  // +4 left as the caller's
            return out;
        }
        if ( vt == k_ui_set_info_vtable )
        {
            // 0x471E30.
            const ui_slot_t* id = ui_bind_slot( owner, "Info String ID" );
            ui_params_t q;
            q.add_string( id ? id->m_text.c_str( ) : "" );
            broadcast( fsm_hash( "OnSetInfoStringID" ), q );
            out.m_known = false;  // +4 left as the caller's
            return out;
        }
        if ( ui_is_slider_vtable( vt ) )
        {
            // 0x446C80 / 0x446FB0 / 0x46F850: VuRetVal +0 = 0 (+4 the caller's).
            c_ui_slider& slider = *m_bound[ static_cast< std::size_t >( index_of( owner ) ) ]->m_slider;
            if ( std::strcmp( plug, "Show" ) == 0 )
                slider.m_visible = true;
            else if ( std::strcmp( plug, "Hide" ) == 0 )
                slider.m_visible = false;
            else
                slider.set_focus( );
            out.m_known = false;
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
    ui_info_hooks_t m_info;  // m_hooks.m_info with the trace
    ui_hud_hooks_t m_hud_hooks;  // the HUD elements' VuStringDB (m_hooks.m_string_db)
    std::map< const entity_t*, int > m_index;
};
