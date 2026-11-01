#pragma once

#include <cstdint>

// VuRTTI for the boat / race classes. Every entity class's vtable slot 1 is
// a 6-byte getType (mov eax, descriptor; ret) returning a static VuRTTI
// descriptor (.data, statically initialised: +0 the name, +4 the parent's
// descriptor, 0 at VuEntity's). The chain is what 0x41B4B0 (thiscall, the
// object; the wanted descriptor; retn 4: vt+4, then +4 until it is found or
// 0) and its inline copies (DriverDisable 0x430020, the power-up contact
// 0x43C8C0) answer "is a" with. The tables are read from the image (the
// boat_rtti case runs every getter and 0x41B4B0 over every pair).

struct boat_rtti_t
{
    std::uint32_t descriptor;
    std::uint32_t parent;
    const char* name;
};

inline constexpr boat_rtti_t g_boat_rtti[] = {
    { 0x7367F8u, 0x000000u, "VuEntity" },
    { 0x73622Cu, 0x7367F8u, "VuDriverEntity" },
    { 0x736214u, 0x73622Cu, "VuAiDriver" },
    { 0x73621Cu, 0x736214u, "VuAiNuisanceDriver" },
    { 0x736224u, 0x736214u, "VuAiUiDriver" },
    { 0x736234u, 0x73622Cu, "VuGhostDriver" },
    { 0x73623Cu, 0x73622Cu, "VuHumanDriver" },
    { 0x73628Cu, 0x7367F8u, "VuBoatEntity" },
    { 0x736294u, 0x73628Cu, "VuGhostBoatEntity" },
    { 0x736370u, 0x7367F8u, "VuAiNuisanceDriverStartEntity" },
    { 0x736378u, 0x7367F8u, "VuAIUIDriverStartEntity" },
    { 0x736380u, 0x7367F8u, "VuAiWaypointEntity" },
    { 0x7363E0u, 0x7367F8u, "VuGridPositionEntity" },
    { 0x7363F0u, 0x7367F8u, "VuPowerUpEntity" },
    { 0x7363F8u, 0x7363F0u, "VuPowerUpBoostEntity" },
    { 0x736400u, 0x7363F0u, "VuPowerUpHiddenPackageEntity" },
    { 0x736408u, 0x7363F0u, "VuPowerUpTriggerEntity" },
};

// The main vtables, their slot 1 (getType) and the descriptor it returns.
struct boat_rtti_class_t
{
    std::uint32_t vtable;
    std::uint32_t get_type;
    std::uint32_t descriptor;
};

inline constexpr boat_rtti_class_t g_boat_rtti_classes[] = {
    { 0x737D4Cu, 0x40EA10u, 0x73622Cu },   // VuDriverEntity
    { 0x737B5Cu, 0x40B5E0u, 0x736214u },   // VuAiDriver
    { 0x737C4Cu, 0x40D870u, 0x73621Cu },   // VuAiNuisanceDriver
    { 0x737CDCu, 0x40E520u, 0x736224u },   // VuAiUiDriver
    { 0x737DDCu, 0x40EB30u, 0x736234u },   // VuGhostDriver
    { 0x737F44u, 0x40ED20u, 0x73623Cu },   // VuHumanDriver
    { 0x7383D4u, 0x41B4A0u, 0x73628Cu },   // VuBoatEntity
    { 0x738474u, 0x422300u, 0x736294u },   // VuGhostBoatEntity
    { 0x739B94u, 0x42F920u, 0x736370u },   // VuAiNuisanceDriverStartEntity
    { 0x739BF0u, 0x430510u, 0x736378u },   // VuAIUIDriverStartEntity
    { 0x739D44u, 0x431670u, 0x736380u },   // VuAiWaypointEntity
    { 0x73AA3Cu, 0x43A5E0u, 0x7363E0u },   // VuGridPositionEntity
    { 0x73AE54u, 0x43B130u, 0x7363F0u },   // VuPowerUpEntity
    { 0x73AED4u, 0x43D740u, 0x7363F8u },   // VuPowerUpBoostEntity
    { 0x73AF5Cu, 0x43DBF0u, 0x736400u },   // VuPowerUpHiddenPackageEntity
    { 0x73AFE4u, 0x43E0D0u, 0x736408u },   // VuPowerUpTriggerEntity
};

// A vtable's getType answer; 0 for a vtable not in the table (UNKNOWN here).
inline std::uint32_t boat_rtti_of( std::uint32_t vtable )
{
    for ( const boat_rtti_class_t& c : g_boat_rtti_classes )
        if ( c.vtable == vtable )
            return c.descriptor;
    return 0;
}

inline std::uint32_t boat_rtti_parent( std::uint32_t descriptor )
{
    for ( const boat_rtti_t& r : g_boat_rtti )
        if ( r.descriptor == descriptor )
            return r.parent;
    return 0;
}

// 0x41B4B0 on an object of `vtable`: false when vt+4 answers 0, else the
// chain walked to `wanted` (true) or to its end (false). A vtable outside
// the table answers false (its chain is not known here).
inline bool boat_rtti_is( std::uint32_t vtable, std::uint32_t wanted )
{
    for ( std::uint32_t d = boat_rtti_of( vtable ); d; d = boat_rtti_parent( d ) )
        if ( d == wanted )
            return true;
    return false;
}
