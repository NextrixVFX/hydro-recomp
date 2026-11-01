#pragma once

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/ui/fsm.hxx>

#include <cstdint>
#include <cstring>
#include <vector>

// VuRaceGame slot 15 0x48B180 (vt+0x3C; thiscall (boat, checkpoint), ret 8):
// what a boat crossing a checkpoint does to the split / result display. The
// race rules' checkpoint pass (core/world rules.hxx race_rules_checkpoints,
// the world agent's) calls it through its `crossed` hook. Transliterated from
// the disassembly:
//   - the boat's +0x220 object's slot 12 (vt+0x30: the driver's "is human" -
//     an unread Vu virtual, an INPUT here) false, or the lap +0x4AC < 1: nothing.
//   - the checkpoint's byte +0x8A (the finish line) and the lap == the game's
//     +0x4C (the lap count): +0x6DC = race time (+0x4B4) - +0x6C4 (the personal
//     best's time) and the +0x67C machine's "ShowResult" pulsed (0x555C60, the
//     condition's +0x1D = 1). Nothing else.
//   - else the split: k = the +0x72C vector's size (std::vector<float>); when
//     the +0x6C8 container is an array (tag 5) and 0 <= k < its size, element k,
//     else the null container 0x7593A0; its float (0x4FC590: int 1 cvtdq2ps,
//     float 2, int64 7 fild / fstp, anything else false) -> +0x6D8 = race time
//     - it and "ShowSplit" pulsed; then (either way) the race time pushed onto
//     +0x72C (0x440990).
// The fields it touches (offsets into VuRaceGame):
struct ui_race_split_t
{
    std::int32_t m_laps = 0;                  // +0x4C
    float m_best_time = 0.0f;                 // +0x6C4 (the personal best: the game's vt+0x38 fills it - saves, a platform hook)
    value_t m_best_splits;                    // +0x6C8 (its split times; the same call)
    float m_split_delta = 0.0f;               // +0x6D8 (the HUD's)
    float m_result_delta = 0.0f;              // +0x6DC
    std::vector< float > m_splits;            // +0x72C this race's split times
};

// The boat's side (an input: the boat side's fields).
struct ui_race_split_boat_t
{
    bool m_human = false;                     // +0x220 slot 12 (HOOK answer)
    std::int32_t m_lap = 0;                   // +0x4AC
    float m_race_time = 0.0f;                 // +0x4B4
};

// 0x4FC590 VuJsonContainer::getValue( float& ).
inline bool ui_json_float( const value_t& v, float& out )
{
    if ( v.tag == 1 )
    {
        out = static_cast< float >( v.integer );   // cvtdq2ps
        return true;
    }
    if ( v.tag == 2 )
    {
        std::memcpy( &out, &v.real, 4 );
        return true;
    }
    if ( v.tag == 7 )
    {
        out = static_cast< float >( v.int64 );     // fild (exact) + fstp (one rounding, to nearest)
        return true;
    }
    return false;
}

// false: a path not recreated (the machine has no such condition - retail
// writes before its condition vector).
inline bool ui_race_checkpoint( ui_race_split_t& g, c_fsm& split_fsm, const ui_race_split_boat_t& boat, bool finish_line )
{
    if ( !boat.m_human || boat.m_lap < 1 )
        return true;
    if ( finish_line && boat.m_lap == g.m_laps )
    {
        g.m_result_delta = boat.m_race_time - g.m_best_time;
        return split_fsm.pulse_condition( "ShowResult" );   // 0x69F7A8
    }
    bool ok = true;
    const std::int32_t k = static_cast< std::int32_t >( g.m_splits.size( ) );
    static const value_t k_null{};   // 0x7593A0
    const value_t* split = &k_null;
    if ( g.m_best_splits.tag == 5 && k >= 0 && k < static_cast< std::int32_t >( g.m_best_splits.list.size( ) ) )
        split = &g.m_best_splits.list[ static_cast< std::size_t >( k ) ];
    float best = 0.0f;
    if ( ui_json_float( *split, best ) )
    {
        g.m_split_delta = boat.m_race_time - best;
        ok = split_fsm.pulse_condition( "ShowSplit" );   // 0x69F7C0
    }
    g.m_splits.push_back( boat.m_race_time );
    return ok;
}
