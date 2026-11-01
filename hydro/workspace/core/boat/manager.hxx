#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// VuBoatManager (g_vu_boat_manager 0x758FB8; ctor 0x4AAAA0, vtable 0x74533C
// with the keyboard / dev-menu callback vtables 0x745330 / 0x745328 at +4 /
// +8). Every boat registers itself in gameInitialize 0x41D350 (0x4AAF60) and
// leaves in gameRelease 0x41D470 (0x4AB040). The ctor's lists are VuArray<
// VuBoatEntity* > {data, size, capacity}, each first given an 8-entry block
// (aligned_malloc 0x20):
//   +0x0C every boat          +0x18 the racers (every driver but a nuisance)
//   +0x24 the humans          +0x30 the AI drivers      +0x3C the ghosts
// then +0x54 byte 1, +0x58 the viewport boat (an index into +0x0C) 0, +0x5C
// the debug-camera byte 0, +0x60 the debug camera (0x55A4A0), +0x2B8 "Dev/Draw
// HUD" 1, +0x2B9 0, +0x2BC / +0x2C0 0 (the dev-menu registrations - "Dev/Draw
// HUD", "DebugCamera/Save" / "Load" - are the dev layer: hooks).

// The four driver questions 0x4AAF60 asks of the boat's driver (boat +0x220):
// vt+0x30, vt+0x34, vt+0x40, vt+0x44. In every driver vtable they are the
// constant answers 0x404BD0 (mov al, 1) and 0x4052D0 (xor al, al):
//   class (vtable)                +0x30  +0x34  +0x40  +0x44
//   VuHumanDriver      0x737F44    1      0      0      0
//   VuAiDriver         0x737B5C    0      1      0      0
//   VuAiUiDriver       0x737CDC    0      1      0      0
//   VuAiNuisanceDriver 0x737C4C    0      1      1      0
//   VuGhostDriver      0x737DDC    0      0      0      1
// (VuDriverEntity 0x737D4C has _purecall at +0x30 / +0x34: never a boat's.)
struct boat_driver_kind_t
{
    bool human = false;    // vt+0x30
    bool ai = false;       // vt+0x34
    bool nuisance = false; // vt+0x40 (kept out of the racers)
    bool ghost = false;    // vt+0x44
};

// The answers for a driver's vtable; false for a vtable that is not one of
// the five (UNKNOWN - no answers are made up).
inline bool boat_driver_kind( std::uint32_t vtable, boat_driver_kind_t& kind )
{
    switch ( vtable )
    {
    case 0x737F44: kind = { true, false, false, false }; return true;
    case 0x737B5C: kind = { false, true, false, false }; return true;
    case 0x737CDC: kind = { false, true, false, false }; return true;
    case 0x737C4C: kind = { false, true, true, false }; return true;
    case 0x737DDC: kind = { false, false, false, true }; return true;
    default: return false;
    }
}

// One VuArray< VuBoatEntity* >: the entries plus the capacity retail's growth
// keeps (the ctor's 8).
struct boat_manager_list_t
{
    std::vector< const void* > boats;
    std::int32_t capacity = 8;
};

struct boat_manager_t
{
    boat_manager_list_t all;     // +0x0C
    boat_manager_list_t racers;  // +0x18
    boat_manager_list_t humans;  // +0x24
    boat_manager_list_t ais;     // +0x30
    boat_manager_list_t ghosts;  // +0x3C
    std::uint8_t b54 = 1;        // +0x54
    std::int32_t viewport = 0;   // +0x58
    std::uint8_t debug_camera = 0; // +0x5C
    std::uint8_t draw_hud = 1;   // +0x2B8 ("Dev/Draw HUD")
    std::uint8_t b2b9 = 0;       // +0x2B9 (set: the lens scale stays 0.25)
    // +0x2C0: a camera image (0x240 bytes) that overrides the boats' while
    // set. Only the ctor / create 0x4AA980 write it (0) in what was read; who
    // sets it is NOT ESTABLISHED.
    const std::uint8_t* override_camera = nullptr;
};

// VuArray resize 0x415690 (thiscall n; retn 4): n > capacity -> the capacity
// becomes max(n, capacity + capacity / 2) (cdq / sub / sar: the halving rounds
// toward zero), a new block, the old entries copied; then size = n.
inline void boat_manager_resize( boat_manager_list_t& l, std::int32_t n )
{
    if ( n > l.capacity )
    {
        std::int32_t grown = l.capacity + l.capacity / 2;
        if ( n > grown )
            grown = n;
        if ( grown > l.capacity )
            l.capacity = grown;
    }
    l.boats.resize( static_cast< std::size_t >( n ), nullptr );
}

inline void boat_manager_push( boat_manager_list_t& l, const void* boat )
{
    boat_manager_resize( l, static_cast< std::int32_t >( l.boats.size( ) ) + 1 );
    l.boats.back( ) = boat;
}

// 0x4AB7D0 (thiscall &boat; retn 4): the FIRST entry equal to the boat is
// erased (memmove of the rest down one), then the resize to size - 1 (which
// never grows). No match: nothing.
inline void boat_manager_erase( boat_manager_list_t& l, const void* boat )
{
    for ( std::size_t i = 0; i < l.boats.size( ); ++i )
        if ( l.boats[ i ] == boat )
        {
            l.boats.erase( l.boats.begin( ) + static_cast< std::ptrdiff_t >( i ) );
            return;
        }
}

// 0x4AAF60 (thiscall boat; retn 4), from gameInitialize 0x41D350. With no
// human yet (+0x28 == 0) the viewport index becomes the boat's place in +0x0C
// (its count before the push) - so the first human, or the last boat added
// before one, is the one the viewport follows.
inline void boat_manager_add( boat_manager_t& m, const void* boat, const boat_driver_kind_t& driver )
{
    if ( m.humans.boats.empty( ) )
        m.viewport = static_cast< std::int32_t >( m.all.boats.size( ) );
    if ( !driver.nuisance )
    {
        boat_manager_push( m.racers, boat );
        if ( driver.human )
            boat_manager_push( m.humans, boat );
        else if ( driver.ai )
            boat_manager_push( m.ais, boat );
        else if ( driver.ghost )
            boat_manager_push( m.ghosts, boat );
    }
    boat_manager_push( m.all, boat );
}

// 0x4AB040 (thiscall boat; retn 4), from gameRelease 0x41D470: 0x4AB7D0 on
// all five lists, then viewport = min(viewport, count - 1) (cmovl), and 0 when
// that is negative (cmovs).
inline void boat_manager_remove( boat_manager_t& m, const void* boat )
{
    boat_manager_erase( m.all, boat );
    boat_manager_erase( m.racers, boat );
    boat_manager_erase( m.humans, boat );
    boat_manager_erase( m.ais, boat );
    boat_manager_erase( m.ghosts, boat );
    std::int32_t v = m.viewport;
    const std::int32_t last = static_cast< std::int32_t >( m.all.boats.size( ) ) - 1;
    if ( last < v )
        v = last;
    if ( v < 0 )
        v = 0;
    m.viewport = v;
}

// ---- The "Viewports" tick 0x4AB0A0 ---------------------------------------
//
// VuMethod1<VuBoatManager, void, float> registered by slot 5 0x4AAD70 in the
// tick manager's "Viewports" phase (dt unused). The entries of the lists are
// the boats' IMAGES here: the tick reads boat +0x3F8 and +0x3FC; boat +0x224
// (its camera image's pointer) is the boat_camera hook's, +0x467 the local's.
//   count (+0x10) != 0:
//     override (+0x2C0) set -> its 0x240 bytes into the viewport (g
//       0x759060 +0x28); the viewport boat stays none;
//     else the debug-camera byte (+0x5C) -> the dev path (0x41B400, 0x52F690
//       on dword_759430, the display size g 0x7590?? vt+0x40, VerticalFOV,
//       0x50BBE0 with 0.5, tick manager vt+0x3C, 0x55A500) then the debug
//       camera +0x60's bytes in - a HOOK (the dev keyboard's camera);
//     else +0x58 >= count (signed) -> none; else the entry: null -> none;
//       else its camera (*(boat +0x224), 0x240 bytes) in, viewport +0x278 =
//       0.25 (when +0x2B9) or boat +0x3F8 * 0.25, then the lens-water manager
//       g 0x759048 vt+0x2C (thiscall) with boat +0x3FC.
//   every boat of +0x0C: +0x467 = (boat == the viewport boat) (sete).
//   FMOD set3DListenerAttributes(0, vp +0x188, +0x48, +0x168, +0x178) (the
//   audio device: a hook); the trigger entity's (+0x2BC) transform component
//   (+0x50) position +0x80..+0x88 = vp +0x188..+0x190, 0x534830 then
//   0x534C00(0) on it (the entity framework: hooks); then count != 0 ->
//   0x531F50 on the trigger's +0x5C (register), else the trigger manager g
//   0x759064's 0x5362A0(+0x5C) when its +0x28 byte is set, then +0x28 = 0.
// A negative +0x58 reads before the list's block in retail: UNKNOWN (false;
// nothing is called). 0x4AB040 / 0x4AAF60 never leave it negative.
struct boat_viewport_hooks_t
{
    std::function< void( const std::uint8_t* camera ) > viewport_camera; // the override's 0x240 bytes into viewport +0x28
    std::function< void( const void* boat ) > boat_camera;              // *(boat +0x224)'s 0x240 bytes into viewport +0x28
    std::function< void( ) > debug_camera;                              // the +0x5C dev path, its copy included
    std::function< void( float ) > lens_scale;                          // viewport +0x278
    std::function< void( float ) > lens_water;                          // g 0x759048 vt+0x2C
    std::function< void( const void* boat, std::uint8_t local ) > local; // boat +0x467
    std::function< void( ) > listener;                                  // FMOD + the trigger's position + 0x534830 / 0x534C00(0)
    std::function< void( bool registered ) > trigger;                   // 0x531F50 (true) / the 0x5362A0 branch (false)
};

// The source of the viewport camera the tick chose: 0 none, 1 the override,
// 2 the debug camera, 3 the boat `boat`'s camera.
struct boat_viewport_t
{
    int source = 0;
    const void* boat = nullptr;
};

inline float boat_manager_f( const void* boat, int at )
{
    float v;
    std::memcpy( &v, static_cast< const std::uint8_t* >( boat ) + at, 4 );
    return v;
}

inline bool boat_manager_viewport_tick( const boat_manager_t& m, boat_viewport_t& out, const boat_viewport_hooks_t& hooks )
{
    out = {};
    const std::int32_t count = static_cast< std::int32_t >( m.all.boats.size( ) );
    if ( count != 0 )
    {
        if ( m.override_camera )
        {
            if ( hooks.viewport_camera )
                hooks.viewport_camera( m.override_camera );
            out.source = 1;
        }
        else if ( m.debug_camera )
        {
            if ( hooks.debug_camera )
                hooks.debug_camera( );
            out.source = 2;
        }
        else if ( m.viewport < 0 )
            return false;
        else if ( m.viewport < count && m.all.boats[ static_cast< std::size_t >( m.viewport ) ] )
        {
            const void* boat = m.all.boats[ static_cast< std::size_t >( m.viewport ) ];
            if ( hooks.boat_camera )
                hooks.boat_camera( boat );
            float scale = 0.25f;
            if ( !m.b2b9 )
                scale = boat_manager_f( boat, 0x3F8 ) * 0.25f;
            if ( hooks.lens_scale )
                hooks.lens_scale( scale );
            if ( hooks.lens_water )
                hooks.lens_water( boat_manager_f( boat, 0x3FC ) );
            out.source = 3;
            out.boat = boat;
        }
    }
    for ( const void* boat : m.all.boats )
        if ( hooks.local )
            hooks.local( boat, boat == out.boat ? 1 : 0 );
    if ( hooks.listener )
        hooks.listener( );
    if ( hooks.trigger )
        hooks.trigger( count != 0 );
    return true;
}

// ---- Slots 5 / 3, the draw (boat batch 11) --------------------------------
//
// The draw 0x4AB300 (VuMethod0<VuBoatManager, void>, registered with the draw
// manager by slot 5): with boats (+0x10, the +0x0C list's size), no debug
// camera (+0x5C) and Draw HUD (+0x2B8): g_vugfxsort 0x759074's +0x74 word
// rewritten (boat_manager_sort_bits; its +0x70 read and stored back
// unchanged), then - unless the viewport boat (the +0x0C list's [+0x58])
// has its +0x32F byte - that boat's driver (+0x220) vt+0x58 with
// g_vuviewportmanager 0x759060 +0x18 (the driver's own draw: a hook).
inline std::uint32_t boat_manager_sort_bits( std::uint32_t w )
{
    w &= 0x07FFFFFFu;
    w |= 0x18000000u;
    w &= 0xFCFFFFFFu;
    w &= 0xFF7FFFFFu;
    w &= 0xFFC3FFFFu;
    w |= 0x00240000u;
    return w;
}

inline void boat_manager_draw( const boat_manager_t& m, std::uint32_t& sort_word, const std::function< void( const void* boat ) >& driver_draw )
{
    if ( m.all.boats.empty( ) || m.debug_camera || !m.draw_hud )
        return;
    sort_word = boat_manager_sort_bits( sort_word );
    if ( m.viewport < 0 || static_cast< std::size_t >( m.viewport ) >= m.all.boats.size( ) )
        return;   // retail reads past its list: UNKNOWN
    const void* boat = m.all.boats[ static_cast< std::size_t >( m.viewport ) ];
    if ( static_cast< const std::uint8_t* >( boat )[ 0x32F ] )
        return;
    if ( driver_draw )
        driver_draw( boat );
}

// What slots 5 / 3 call - the tick / draw managers, the entity framework,
// the system-component registry: all hooks, in retail's order.
struct boat_manager_game_hooks_t
{
    std::function< void( ) > register_tick;      // g 0x759008 vt+0x20(VuMethod1 -> 0x4AB0A0, "Viewports")
    std::function< void( ) > register_draw;      // g 0x759024 vt+0x1C(VuMethod0 -> 0x4AB300)
    std::function< void( ) > create_trigger;     // operator new(0x60), 0x405550, 0x4AB6C0: VuActiveCameraTriggerEntity -> +0x2BC
    std::function< void( const std::string& ) > name_trigger;  // its +0x18 (0x401DB0)
    std::function< void( ) > load_trigger;       // its vt+0x2C(the null container 0x7593A0)
    std::function< void( std::uint32_t ) > hash_trigger;       // 0x500A60(0x811C9DC5)
    std::function< void( ) > add_trigger;        // 0x500B10
    std::function< void( ) > remove_trigger;     // 0x500B80
    std::function< void( ) > delete_trigger;     // its vt+0(1), when its +4 count drops to 0
    std::function< void( ) > unregister_component; // g 0x75906C 0x539020(this + 4)
    std::function< void( ) > unregister_draw;    // g 0x759024 vt+0x20(this)
    std::function< void( ) > unregister_tick;    // g 0x759008 vt+0x28(this)
};

// Slot 5 0x4AAD70 (gameInitialize): returns 1.
inline bool boat_manager_game_initialize( const boat_manager_game_hooks_t& h )
{
    if ( h.register_tick ) h.register_tick( );
    if ( h.register_draw ) h.register_draw( );
    if ( h.create_trigger ) h.create_trigger( );
    if ( h.name_trigger ) h.name_trigger( "ActiveCameraTrigger" );
    if ( h.load_trigger ) h.load_trigger( );
    if ( h.hash_trigger ) h.hash_trigger( 0x811C9DC5u );
    if ( h.add_trigger ) h.add_trigger( );
    return true;
}

// Slot 3 0x4AAF10 (gameRelease). references = the trigger's +4 count (dec;
// deleted when it reaches 0).
inline void boat_manager_game_release( std::int32_t& references, const boat_manager_game_hooks_t& h )
{
    if ( h.remove_trigger ) h.remove_trigger( );
    if ( --references == 0 && h.delete_trigger )
        h.delete_trigger( );
    if ( h.unregister_component ) h.unregister_component( );
    if ( h.unregister_draw ) h.unregister_draw( );
    if ( h.unregister_tick ) h.unregister_tick( );
}

// The dtor 0x4AACE0 -> 0x4AAD10: aligned_free of the five lists' blocks
// (+0x0C .. +0x3C) and - when +0x54 owns it - the +0x48 array's; the
// VuSystemComponent vtable back. boat_manager_list_t's std::vector owns its
// block here (SUBSTITUTION for the VuArray's aligned block): nothing to call.
