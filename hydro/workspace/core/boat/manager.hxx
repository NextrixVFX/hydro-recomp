#pragma once

#include <cstdint>
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
    std::uint8_t b2b9 = 0;       // +0x2B9
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
