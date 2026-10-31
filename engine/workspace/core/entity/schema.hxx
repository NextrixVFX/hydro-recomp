#pragma once

// GENERATED - do not edit by hand. Regenerate with tools/schema.py.
//
// Every property a Vu class declares, with the member offset retail binds
// it to. THE OFFSETS ARE RETAIL'S, carried so a property can be matched
// against the disassembly - never to index one of our own objects.
//
// m_verified marks an entry read out of the CONSTRUCTOR UNDER EMULATION:
// its type, its unit and its default are observed, not inferred. An entry
// without it came from the static dump, which gives a reliable name and
// offset but attributes type and unit one property late - so those fields
// are left empty rather than filled in with something plausible.
//
// A unit is a conversion retail applies between the AUTHORED value and the
// STORED one: degrees -> radians (x0.017453292), percent -> fraction
// (x0.01). Both confirmed from VuBasicProperty's set/get hooks.

#include <cstdint>
#include <cstring>

enum class prop_type_t : std::uint8_t
{
    unknown, boolean, integer, real, vector2, vector3, color, rect, text, enumeration
};

enum class prop_unit_t : std::uint8_t
{
    none, degrees, percent
};

struct entity_property_t
{
    const char* m_name;
    std::uint32_t m_retail_offset;
    prop_type_t m_type;
    prop_unit_t m_unit;
    // Bytes of the default that are meaningful: 1 bool, 4 scalar, 16 vector.
    std::uint8_t m_width;
    bool m_verified;
    // The default exactly as the constructor left it, as raw bits.
    std::uint32_t m_default[ 4 ];
    // For a text or asset-reference property, its default string.
    const char* m_default_text;
};

struct entity_schema_t
{
    const char* m_name;
    const entity_property_t* m_properties;
    int m_count;
    // False when the constructor failed part-way under emulation.
    // The properties listed are still observed; what is unknown is
    // whether MORE follow the point where it stopped.
    bool m_complete;
};

static const entity_property_t g_schema_Vu3dDrawAnimatedModelComponent[] = {
    { "Reflect", 0x1C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Shadow", 0x1D, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Model Asset", 0x44, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0x5C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Draw Distance", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x7F7FFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Dynamic Lighting", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_Vu3dDrawBreakableModelComponent[] = {
    { "Reflect", 0x1C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Shadow", 0x1D, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Model Asset", 0x44, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Draw Distance", 0x5C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x7F7FFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rejection Scale Modifier", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Dynamic Lighting", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fade Delay", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fade Time", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Piece Lin Vel", 0x90, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0xC0A00000, 0xC0A00000, 0x40A00000, 0x00000000 }, "" },
    { "Max Piece Lin Vel", 0xA0, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x40A00000, 0x40A00000, 0x41200000, 0x00000000 }, "" },
    { "Min Piece Ang Vel", 0xB0, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0xC0490FDB, 0xC0490FDB, 0xC0490FDB, 0x00000000 }, "" },
    { "Max Piece Ang Vel", 0xC0, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x00000000 }, "" },
    { "Min Velocity Damping", 0xD0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Velocity Damping", 0xD4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gravity", 0xD8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x411CE560, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0xEC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_Vu3dDrawComponent[] = {
    { "Reflect", 0x1C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000090, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Shadow", 0x1D, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_Vu3dDrawStaticModelComponent[] = {
    { "Reflect", 0x1C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Shadow", 0x1D, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Model Asset", 0x44, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "LOD 1 Model Asset", 0x5C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "LOD 2 Model Asset", 0x74, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflection Model Asset", 0x8C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0xA4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Draw Distance", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x7F7FFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "LOD 0 Draw Distance", 0xAC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x7F7FFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "LOD 1 Draw Distance", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x7F7FFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rejection Scale Modifier", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Dynamic Lighting", 0xB8, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAddBoostEntity[] = {
    { "Amount", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAddIntegerConstantEntity[] = {
    { "C", 0x60, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAiNuisanceDriverStartEntity[] = {
    { "Boat", 0x64, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "Police" },
};
static const entity_property_t g_schema_VuAiWaypointEntity[] = {
    { "Speed Hint", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start", 0x88, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "CheckPoint", 0x89, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Finish", 0x8A, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Allow Recovery", 0x8B, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Show Wrong Way", 0x8C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "CheckPoint Width", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "CheckPoint Height", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Branch A Probability", 0xA0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Branch B Probability", 0xA4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAmbientLightEntity[] = {
    { "Color", 0x60, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF333333, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAngleProperty[] = {
    { "Spawn Per Second", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Spawn Count", 0x34, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Lifespan", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Lifespan", 0x3C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Color", 0x40, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Color", 0x44, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Scale", 0x48, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Scale", 0x4C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Linear Velocity", 0x50, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Max Linear Velocity", 0x60, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Min Position", 0x70, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Position", 0x80, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn At Water Surface", 0x90, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn Distance", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Rotation", 0x98, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Rotation", 0x9C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Angular Velocity", 0xA0, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Angular Velocity", 0xA4, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min World Scale Z", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max World Scale Z", 0xAC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Directional Stretch", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Directional Stretch", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Tile Offset U", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Tile Offset U", 0xBC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Tile Offset V", 0xC0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Tile Offset V", 0xC4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAnimatedAttachComponent[] = {
    { "Initially Visible", 0x7C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAnimatedPropEntity[] = {
    { "Initially Visible", 0x7C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAnimationEventIF[] = {
    { "Initially Visible", 0x7C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAssetNameProperty[] = {
    { "Reflect", 0x1C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Shadow", 0x1D, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Model Asset", 0x44, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Draw Distance", 0x5C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x7F7FFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rejection Scale Modifier", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Dynamic Lighting", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fade Delay", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fade Time", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Piece Lin Vel", 0x90, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0xC0A00000, 0xC0A00000, 0x40A00000, 0x00000000 }, "" },
    { "Max Piece Lin Vel", 0xA0, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x40A00000, 0x40A00000, 0x41200000, 0x00000000 }, "" },
    { "Min Piece Ang Vel", 0xB0, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0xC0490FDB, 0xC0490FDB, 0xC0490FDB, 0x00000000 }, "" },
    { "Max Piece Ang Vel", 0xC0, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x40490FDB, 0x40490FDB, 0x40490FDB, 0x00000000 }, "" },
    { "Min Velocity Damping", 0xD0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Velocity Damping", 0xD4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gravity", 0xD8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x411CE560, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0xEC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAttachEntity[] = {
    { "Attach at Start", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Relative Position", 0x64, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Relative Rotation", 0x74, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Node Name", 0x84, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAudioBankEntity[] = {
    { "Audio Bank", 0x5C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAudioDspEntity[] = {
    { "Initially Active", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Type", 0x64, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Category", 0x7C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAudioDuckingEntity[] = {
    { "Initially Active", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Category", 0x64, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Volume (DB)", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0xC0C00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Duration", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAudioEmitterEntity[] = {
    { "Event Name", 0x6C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Initially Active", 0x84, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAudioEventEntity[] = {
    { "Event Name", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Stop When Destroyed", 0x78, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAudioEventNameProperty[] = {
    { "Heading String ID", 0x64, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "String ID", 0x7C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Text A", 0x94, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Text B", 0xAC, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture", 0xC4, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Flip X", 0xDC, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Scale X", 0xE0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Scale Y", 0xE4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Sfx A", 0xE8, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Sfx B", 0x100, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pause Game", 0x118, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAudioProjectEntity[] = {
    { "Audio Project", 0x5C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAudioReverbEntity[] = {
    { "Reverb Name", 0x64, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Initially Active", 0x7C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Distance", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42480000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Distance", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuAudioReverbNameProperty[] = {
    { "Reverb Name", 0x64, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Initially Active", 0x7C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Distance", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42480000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Distance", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuBankedTurnWaveEntity[] = {
    { "Outer Radius", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Radius", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Height", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Angular Size", 0x6C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x40C90FDB, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Angular Decay Ratio", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuBarracudaTeamScoreUITextEntity[] = {
    { "Team Index", 0x10C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuBarracudaTrackUIImageEntity[] = {
    { "Visible", 0x6C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0x6D, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstY", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstWidth", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstHeight", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcX", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcY", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcWidth", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcHeight", 0x98, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation", 0x9C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "FlipX", 0xA0, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "FlipY", 0xA1, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Absolute Coords", 0xA2, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Anchor", 0xA4, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Anchor", 0xA8, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Ratio", 0xAC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Ratio", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuBarracudaTuningUITextEntity[] = {
    { "Place", 0x10C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuBarracudaUnlockUITextEntity[] = {
    { "Place", 0x124, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuBaseOceanWaveEntity[] = {
    { "Complexity", 0x68, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000005, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Bin Siz", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Wave Direction", 0x70, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gravity", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x411CE560, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Wind Speed", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Directional Power", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40400000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Suppression Wave Length", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Height Multiplier", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3DCCCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuBoatEntity[] = {
    { "Driver Type", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "AI" },
    { "Boat", 0x8C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "Default" },
    { "Skin", 0xA4, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuBoatStatEntity[] = {
    { "Position X", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Position Y", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spacing", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Tick Off Texture", 0x78, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Tick On Texture", 0xF4, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuBooleanEntity[] = {
    { "Value", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuBumpWaveEntity[] = {
    { "Max Height", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Lateral Decay Ratio %", 0x64, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuCheatEntity[] = {
    { "Cheat", 0x5C, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000008, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuCheckpointBuoyEntity[] = {
    { "Fluids Radius", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fluids Density", 0xBC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fluids Drag Coeff", 0xC0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3EF0A3D7, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fluids Lift Coeff", 0xC4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3E4CCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Hydrodynamic Center", 0xC8, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Splash Pfx Name", 0xD8, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Splash Speed", 0xF0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Linear Damping Coeff", 0xF4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Angular Damping Coeff", 0xF8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Linear Anchor", 0xFC, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Angular Anchor", 0xFD, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Anchor Spring Coeff", 0x100, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Anchor Damping Coeff", 0x104, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Hard Hit", 0x108, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Threshold Speed (MPH)", 0x10C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Sfx Name", 0x110, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Magnitude", 0x128, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Duration", 0x12C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Falloff Time", 0x130, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Frequency", 0x134, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rumble Low Freq", 0x138, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rumble High Freq", 0x13C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Throw Ai Driver", 0x140, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Initially Active", 0x141, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Collide With Static Props", 0x142, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "CheckPoint Index", 0x184, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuCircularOceanWaveEntity[] = {
    { "Complexity", 0x68, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000005, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Bin Siz", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Wave Direction", 0x70, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gravity", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x411CE560, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Wind Speed", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Directional Power", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40400000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Suppression Wave Length", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Height Multiplier", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3DCCCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Radius", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Decay Ratio %", 0x90, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F666666, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuConstStringEnumProperty[] = {
    { "Initially Active", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Trigger Type", 0x68, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuControlComponent[] = {
    { "Visible", 0x6C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0x6D, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstY", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstWidth", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstHeight", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcX", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcY", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcWidth", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcHeight", 0x98, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation", 0x9C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "FlipX", 0xA0, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "FlipY", 0xA1, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Absolute Coords", 0xA2, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Anchor", 0xA4, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Anchor", 0xA8, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Ratio", 0xAC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Ratio", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuControlComponentIF[] = {
    { "Visible", 0x6C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0x6D, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstX", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstY", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstWidth", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstHeight", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcX", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcY", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcWidth", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcHeight", 0x98, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation", 0x9C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "FlipX", 0xA0, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "FlipY", 0xA1, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Absolute Coords", 0xA2, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Anchor", 0xA4, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Anchor", 0xA8, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Ratio", 0xAC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Ratio", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuCoronaEntity[] = {
    { "Initially Visible", 0x70, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Name", 0x74, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation Speed", 0x8C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation Axis", 0x90, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x3F800000, 0x00000000 }, "" },
    { "Draw Distance", 0xA0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x7F7FFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fade Distance", 0xA4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Query Radius", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cone Angle", 0xAC, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x40C90FDB, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Penumbra Angle", 0xB0, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Enable Back Light", 0xB4, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Color", 0xB5, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Size Screen Space", 0xB9, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Size", 0xBC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation Offset", 0xC0, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation Amount", 0xC4, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x40490FDB, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuCounterEntity[] = {
    { "Initial Count", 0x60, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Target Count", 0x64, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDevJumpPointEntity[] = {
    { "Key", 0x64, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Shift", 0x68, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Alt", 0x69, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDifficultyBaseEntity[] = {
    { "Loading Screen", 0x68, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDifficultyImageEntity[] = {
    { "Loading Screen", 0x68, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Position X", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Position Y", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset C", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset B", 0xF8, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDifficultyTextEntity[] = {
    { "Loading Screen", 0x68, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDirectionalCoronaEntity[] = {
    { "Initially Visible", 0x70, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Name", 0x74, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Color", 0x8C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Size", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation Offset", 0x94, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation Amount", 0x98, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x40490FDB, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDirectionalFlowWaveEntity[] = {
    { "Flow Velocity", 0x60, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x40A00000, 0x00000000, 0x00000000 }, "" },
    { "Longitudinal Decay Ratio %", 0x70, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Lateral Decay Ratio %", 0x74, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDirectionalLightEntity[] = {
    { "Front Color", 0x6C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFCCCCCC, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Back Color", 0x70, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF404040, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Specular Color", 0x74, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDirectionalWaveEntity[] = {
    { "Max Height", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Speed", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Period", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Longitudinal Decay Ratio %", 0x78, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Lateral Decay Ratio %", 0x7C, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDynamicGamePropEntity[] = {
    { "Fluids Radius", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fluids Density", 0xBC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fluids Drag Coeff", 0xC0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3EF0A3D7, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fluids Lift Coeff", 0xC4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3E4CCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Hydrodynamic Center", 0xC8, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Splash Pfx Name", 0xD8, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Splash Speed", 0xF0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Linear Damping Coeff", 0xF4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Angular Damping Coeff", 0xF8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Linear Anchor", 0xFC, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Angular Anchor", 0xFD, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Anchor Spring Coeff", 0x100, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Anchor Damping Coeff", 0x104, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Hard Hit", 0x108, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Threshold Speed (MPH)", 0x10C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Sfx Name", 0x110, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Magnitude", 0x128, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Duration", 0x12C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Falloff Time", 0x130, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Frequency", 0x134, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rumble Low Freq", 0x138, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rumble High Freq", 0x13C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Throw Ai Driver", 0x140, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Initially Active", 0x141, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Collide With Static Props", 0x142, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDynamicLightEntity[] = {
    { "Initially On", 0x6C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Diffuse Color", 0x94, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Specular Color", 0x98, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Factor", 0x9C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Falloff Range Min", 0xA0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41F00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Falloff Range Max", 0xA4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42480000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cone Angle", 0xA8, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x3FC90FDB, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Penumbra Angle", 0xAC, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Draw Distance", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x7F7FFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflecting", 0xB4, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuDynamicsStepCallback[] = {
    { "Fluids Radius", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fluids Density", 0xBC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fluids Drag Coeff", 0xC0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3EF0A3D7, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fluids Lift Coeff", 0xC4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3E4CCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Hydrodynamic Center", 0xC8, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Splash Pfx Name", 0xD8, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Splash Speed", 0xF0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Linear Damping Coeff", 0xF4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Angular Damping Coeff", 0xF8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Linear Anchor", 0xFC, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Angular Anchor", 0xFD, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Anchor Spring Coeff", 0x100, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Anchor Damping Coeff", 0x104, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Hard Hit", 0x108, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Threshold Speed (MPH)", 0x10C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Sfx Name", 0x110, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Magnitude", 0x128, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Duration", 0x12C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Falloff Time", 0x130, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Cam Shake Frequency", 0x134, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rumble Low Freq", 0x138, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rumble High Freq", 0x13C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Throw Ai Driver", 0x140, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Initially Active", 0x141, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Collide With Static Props", 0x142, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuEllipticalGfxSettingsEntity[] = {
    { "Always Active", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Up Time", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Duration", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Down Time", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Camera Far Plane", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Clear Color", 0x74, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Start", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog End", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x447A0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Color", 0x80, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Contrast", 0x84, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Tint", 0x88, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Min", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Max", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Curve", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Ambient Color", 0x98, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC0423C2D, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Diffuse Color", 0x9C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC05A503C, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Ambient Color", 0xA0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Diffuse Color", 0xA4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Texture Size", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41600000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Ambient Color", 0xAC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Diffuse Color", 0xB0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Value", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Intensity", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Ambient Color", 0xBC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFC0C0C0, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Diffuse Color", 0xC0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Foliage Tint", 0xC4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Radius %", 0xD0, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuExplosionEntity[] = {
    { "Explosion Outer Radius", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42480000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Explosion Inner Radius", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Explosion Cam Shake Magnitude", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3E800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Explosion Cam Shake Duration", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Explosion Cam Shake Falloff Time", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Explosion Cam Shake Frequency", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42480000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Explosion Rumble Low Freq Mag", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Explosion Rumble High Freq Mag", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Explosion Lens Splash Amount", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Explosion Pfx System Name", 0x98, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Explosion Audio Event Name", 0xB0, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuFoliageEntity[] = {
    { "Draw Distance", 0x0, prop_type_t::real, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x64, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuForceAffectorEntity[] = {
    { "Initially Active", 0x68, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Acceleration", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C41EB8, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Speed", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuGenericEventEntity[] = {
    { "Event Name", 0x5C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuGfxSettingsEntity[] = {
    { "Always Active", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Up Time", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Duration", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Down Time", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Camera Far Plane", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Clear Color", 0x74, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Start", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog End", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x447A0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Color", 0x80, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Contrast", 0x84, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Tint", 0x88, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Min", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Max", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Curve", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Ambient Color", 0x98, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC0423C2D, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Diffuse Color", 0x9C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC05A503C, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Ambient Color", 0xA0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Diffuse Color", 0xA4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Texture Size", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41600000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Ambient Color", 0xAC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Diffuse Color", 0xB0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Value", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Intensity", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Ambient Color", 0xBC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFC0C0C0, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Diffuse Color", 0xC0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Foliage Tint", 0xC4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuGhostBoatEntity[] = {
    { "Driver Type", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "AI" },
    { "Boat", 0x8C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "Default" },
    { "Skin", 0xA4, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuGlobalGfxSettingsEntity[] = {
    { "Always Active", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Up Time", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Duration", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Down Time", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Camera Far Plane", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Clear Color", 0x74, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Start", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog End", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x447A0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Color", 0x80, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Contrast", 0x84, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Tint", 0x88, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Min", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Max", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Curve", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Ambient Color", 0x98, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC0423C2D, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Diffuse Color", 0x9C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC05A503C, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Ambient Color", 0xA0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Diffuse Color", 0xA4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Texture Size", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41600000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Ambient Color", 0xAC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Diffuse Color", 0xB0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Value", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Intensity", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Ambient Color", 0xBC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFC0C0C0, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Diffuse Color", 0xC0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Foliage Tint", 0xC4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuGroupEntity[] = {
    { "Collapsed", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuHUDButtonEntity[] = {
    { "Image X", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Y", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Width", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Height", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch X", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Y", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Width", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Height", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x8C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuHUDElementEntity[] = {
    { "Type", 0x10C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuHUDOnScreenAxisEntity[] = {
    { "Image X", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Y", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Width", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Height", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch X", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Y", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Width", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Height", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x88, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuHUDOnScreenButtonEntity[] = {
    { "Image X", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Y", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Width", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Height", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch X", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Y", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Width", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Height", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x88, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuHUDOnScreenControlEntity[] = {
    { "Image X", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Y", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Width", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Height", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch X", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Y", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Width", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Height", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x88, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuHUDRecoverButtonEntity[] = {
    { "Image X", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Y", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Width", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Height", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch X", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Y", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Width", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Height", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x8C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuInfiniteOceanWaveEntity[] = {
    { "Complexity", 0x68, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000005, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Bin Siz", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Wave Direction", 0x70, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gravity", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x411CE560, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Wind Speed", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Directional Power", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40400000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Suppression Wave Length", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Height Multiplier", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3DCCCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuInputAxisEnumProperty[] = {
    { "Image X", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Y", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Width", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Height", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch X", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Y", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Width", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Height", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x88, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuInputButtonEnumProperty[] = {
    { "Image X", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Y", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Width", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Height", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch X", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Y", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Width", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Height", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x88, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuInstigatorComponent[] = {
    { "Driver Type", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "AI" },
    { "Boat", 0x8C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "Default" },
    { "Skin", 0xA4, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuIntegerEntity[] = {
    { "Value", 0x60, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuIsAchievementUnlockedEntity[] = {
    { "Achievement", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuIsEventTypeEntity[] = {
    { "Type", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "AttractEvent" },
};
static const entity_property_t g_schema_VuIsSkuEntity[] = {
    { "SKU", 0x60, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuKeyframeEntity[] = {
    { "Time", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuKeyframeMotionEntity[] = {
    { "Initially Active", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "One Shot", 0x65, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Curve Type", 0x70, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Loop", 0x74, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ease In/Out", 0x75, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Affect Position", 0x76, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Affect Rotation", 0x77, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuLensWaterEmitterEntity[] = {
    { "Radius", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Drops Per Second", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuLoadingProgressBarEntity[] = {
    { "Rect", 0x6C, prop_type_t::rect, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0x7C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Initially Visible", 0x80, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuMessageBoxEntity[] = {
    { "Heading String ID", 0x64, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "String ID", 0x7C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Text A", 0x94, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Text B", 0xAC, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture", 0xC4, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Flip X", 0xDC, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Scale X", 0xE0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Scale Y", 0xE4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Sfx A", 0xE8, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Sfx B", 0x100, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pause Game", 0x118, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuMessageBoxScreenEntity[] = {
    { "Image Y", 0x6C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Width", 0x70, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image X", 0x68, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Height", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Heading Y", 0x7C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Heading Width", 0x80, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Heading Height", 0x84, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Heading Horizontal Alignment", 0xEC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Heading Vertical Alignment", 0xF0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Heading No Clip", 0xF4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Heading Single Line", 0xF5, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Heading Wordbreak", 0xF6, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Body X", 0x114, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Body Y", 0x118, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Body Width", 0x11C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Body Height", 0x120, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Body Horizontal Alignment", 0x188, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Body Vertical Alignment", 0x18C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Body No Clip", 0x190, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Body Single Line", 0x191, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Body Wordbreak", 0x192, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BodyImage X", 0x1B0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BodyImage Y", 0x1B4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BodyImage Width", 0x1B8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BodyImage Height", 0x1BC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BodyImage Horizontal Alignment", 0x224, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BodyImage Vertical Alignment", 0x228, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BodyImage No Clip", 0x22C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BodyImage Single Line", 0x22D, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BodyImage Wordbreak", 0x22E, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA X", 0x24C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Y", 0x250, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Width", 0x254, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Height", 0x258, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Horizontal Alignment", 0x2C0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Vertical Alignment", 0x2C4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA No Clip", 0x2C8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Single Line", 0x2C9, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Wordbreak", 0x2CA, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB X", 0x2E8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Y", 0x2EC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Width", 0x2F0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Height", 0x2F4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Horizontal Alignment", 0x35C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Vertical Alignment", 0x360, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB No Clip", 0x364, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Single Line", 0x365, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Heading X", 0x78, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Wordbreak", 0x366, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Texture Y", 0x388, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Texture Width", 0x38C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Texture X", 0x384, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Texture FlipX", 0x3DC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Texture Height", 0x390, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Texture Y", 0x3E4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Texture Width", 0x3E8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Texture X", 0x3E0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Texture FlipX", 0x438, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Selected Text Color", 0x439, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Texture Height", 0x3EC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Animated Texture Y", 0x444, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Animated Texture Width", 0x448, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Animated Texture X", 0x440, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Animated Texture Color", 0x468, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Animated Rotation Speed", 0x46C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Animated Texture Height", 0x44C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Touch Y", 0x474, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Touch Width", 0x478, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Touch X", 0x470, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonA Touch Height", 0x47C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Touch Y", 0x484, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Touch Width", 0x488, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Touch X", 0x480, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonB Touch Height", 0x48C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuModelSkin[] = {
    { "Driver Type", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "AI" },
    { "Boat", 0x8C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "Default" },
    { "Skin", 0xA4, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuMotionComponentIF[] = {
    { "Emission Rate", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Emission Speed", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Emission Speed", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Magnitude", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3E800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Falloff Time", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DecayTime", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Speed", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Frequency", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range Start Ratio %", 0x8C, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3E4CCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range End Ratio %", 0x90, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F4CCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuMotionEntity[] = {
    { "Initially Active", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "One Shot", 0x65, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuMultiDelayEntity[] = {
    { "Delay 1", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Delay 2", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Delay 3", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Delay 4", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Delay 5", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Delay 6", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Delay 7", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Delay 8", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuNotifyProperty[] = {
    { "Always Active", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Up Time", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Duration", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Down Time", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Camera Far Plane", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Clear Color", 0x74, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Start", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog End", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x447A0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Color", 0x80, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Contrast", 0x84, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Tint", 0x88, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Min", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Max", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Curve", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Ambient Color", 0x98, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC0423C2D, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Diffuse Color", 0x9C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC05A503C, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Ambient Color", 0xA0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Diffuse Color", 0xA4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Texture Size", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41600000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Ambient Color", 0xAC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Diffuse Color", 0xB0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Value", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Intensity", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Ambient Color", 0xBC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFC0C0C0, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Diffuse Color", 0xC0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Foliage Tint", 0xC4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuOffsetAttachComponent[] = {
    { "Initially Visible", 0x78, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Mass", 0x78, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Center of Mass", 0x7C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuOnKeyboardEventEntity[] = {
    { "Key", 0x64, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Shift", 0x68, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Alt", 0x69, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Dev Only", 0x6A, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuOnTouchSpecialEntity[] = {
    { "Type", 0x64, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuOneShotPfxEntity[] = {
    { "Effect Name", 0x68, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Scale", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Color", 0x84, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Enable Reflection", 0x88, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPercentageProperty[] = {
    { "Always Active", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Up Time", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Duration", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Down Time", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Camera Far Plane", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Clear Color", 0x74, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Start", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog End", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x447A0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Color", 0x80, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Contrast", 0x84, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Tint", 0x88, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Min", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Max", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Curve", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Ambient Color", 0x98, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC0423C2D, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Diffuse Color", 0x9C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC05A503C, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Ambient Color", 0xA0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Diffuse Color", 0xA4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Texture Size", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41600000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Ambient Color", 0xAC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Diffuse Color", 0xB0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Value", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Intensity", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Ambient Color", 0xBC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFC0C0C0, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Diffuse Color", 0xC0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Foliage Tint", 0xC4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Radius X %", 0xD0, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Radius Y %", 0xD4, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPersistentBaseEntity[] = {
    { "Save", 0x5C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Name", 0x60, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPersistentBooleanEntity[] = {
    { "Save", 0x5C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Name", 0x60, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPersistentIntegerEntity[] = {
    { "Save", 0x5C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Name", 0x60, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPersistentStringEntity[] = {
    { "Save", 0x5C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Name", 0x60, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxBoatSpringConstraint[] = {
    { "Spring Coeff", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Damping Coeff", 0x34, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start Delay", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxEmitBoatHull[] = {
    { "Min Lifespan", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Lifespan", 0x34, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Scale", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Scale", 0x3C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Rotation", 0x40, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Rotation", 0x44, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Angular Velocity", 0x48, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Angular Velocity", 0x4C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Spawn Boat Speed", 0x50, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Spawn Boat Speed", 0x54, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Spawn Per Second", 0x58, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Diffuse Color", 0x5C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Lateral Velocity Multiplier", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Velocity Multiplier", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Directional Stretch", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Directional Stretch", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn Distance", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43480000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxEmitBoatThrustQuadFountain[] = {
    { "Spawn Per Second", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Spawn Count", 0x34, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Lifespan", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Lifespan", 0x3C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Color", 0x40, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Color", 0x44, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Scale", 0x48, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Scale", 0x4C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Linear Velocity", 0x50, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Max Linear Velocity", 0x60, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Min Position", 0x70, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Position", 0x80, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn At Water Surface", 0x90, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn Distance", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Rotation", 0x98, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Rotation", 0x9C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Angular Velocity", 0xA0, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Angular Velocity", 0xA4, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min World Scale Z", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max World Scale Z", 0xAC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Directional Stretch", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Directional Stretch", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Tile Offset U", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Tile Offset U", 0xBC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Tile Offset V", 0xC0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Tile Offset V", 0xC4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Test Drive", 0xC8, prop_type_t::integer, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxEmitFountain[] = {
    { "Spawn Per Second", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Spawn Count", 0x34, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Lifespan", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Lifespan", 0x3C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Color", 0x40, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Color", 0x44, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Scale", 0x48, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Scale", 0x4C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Linear Velocity", 0x50, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Max Linear Velocity", 0x60, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Min Position", 0x70, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Position", 0x80, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn At Water Surface", 0x90, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn Distance", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxEmitGeomFountain[] = {
    { "Spawn Per Second", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Spawn Count", 0x34, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Lifespan", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Lifespan", 0x3C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Color", 0x40, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Color", 0x44, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Scale", 0x48, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Scale", 0x4C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Linear Velocity", 0x50, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Max Linear Velocity", 0x60, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Min Position", 0x70, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Position", 0x80, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn At Water Surface", 0x90, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn Distance", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Rotation", 0x98, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Rotation", 0xA8, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Angular Velocity", 0xB8, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Angular Velocity", 0xC8, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxEmitQuadFountain[] = {
    { "Spawn Per Second", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Spawn Count", 0x34, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Lifespan", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Lifespan", 0x3C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Color", 0x40, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Color", 0x44, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Scale", 0x48, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Scale", 0x4C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Linear Velocity", 0x50, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Max Linear Velocity", 0x60, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Min Position", 0x70, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Position", 0x80, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn At Water Surface", 0x90, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn Distance", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Rotation", 0x98, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Rotation", 0x9C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Angular Velocity", 0xA0, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Angular Velocity", 0xA4, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min World Scale Z", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max World Scale Z", 0xAC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Directional Stretch", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Directional Stretch", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Tile Offset U", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Tile Offset U", 0xBC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Tile Offset V", 0xC0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Tile Offset V", 0xC4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxGeomPattern[] = {
    { "Max Particle Count", 0x30, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Space", 0x34, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start Delay", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Model Asset", 0x3C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rejection Scale Modifier", 0x54, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Dynamic Lighting", 0x58, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxNode[] = {
    { "Max Particle Count", 0x30, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Space", 0x34, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start Delay", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxPattern[] = {
    { "Max Particle Count", 0x30, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Space", 0x34, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start Delay", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxProjectEntity[] = {
    { "Pfx Asset", 0x5C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxQuadPattern[] = {
    { "Max Particle Count", 0x30, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Space", 0x34, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start Delay", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x3C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Blend Mode", 0x6C, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Tile Texture Asset", 0x90, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Interaction", 0xAC, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Clip Threshold", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Near Fade Min", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Near Fade Max", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxSpringConstraint[] = {
    { "Spring Coeff", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Damping Coeff", 0x34, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start Delay", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Target X", 0x3C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Target Y", 0x40, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Target Z", 0x44, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxSystem[] = {
    { "Duration", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxTickAlpha[] = {
    { "Start Delay", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rate", 0x34, prop_type_t::real, prop_unit_t::none, 4, true, { 0xBF800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxTickAlphaInOut[] = {
    { "Fade In Duration", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fade In Rate", 0x34, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fade Out Start Time", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fade Out Rate", 0x3C, prop_type_t::real, prop_unit_t::none, 4, true, { 0xBF800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxTickDampenVelocity[] = {
    { "Amount", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start Delay", 0x34, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxTickLinearAcceleration[] = {
    { "Accel X", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Accel Y", 0x34, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Accel Z", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start Delay", 0x40, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxTickScale[] = {
    { "Start Delay", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rate", 0x34, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxTickWorldScaleZ[] = {
    { "Start Delay", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rate", 0x34, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPfxTrailPattern[] = {
    { "Max Particle Count", 0x30, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Space", 0x34, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start Delay", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Trail Type", 0x3C, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Lifespan", 0x40, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fade In Time", 0x44, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fade Out Start Time", 0x48, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0x4C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Width", 0x50, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Linear Velocity", 0x54, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn Distance", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x68, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPlayAnimationEntity[] = {
    { "Animation Asset", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Timed Event Asset", 0xD8, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPointWaveCallbackIF[] = {
    { "Falloff Time", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Magnitude", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range Start", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range End", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Speed", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Frequency", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Foaminess", 0x98, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPointWaveEntity[] = {
    { "Falloff Time", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Magnitude", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range Start", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range End", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Speed", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Frequency", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Foaminess", 0x98, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPowerUpBoostEntity[] = {
    { "Initially Enabled", 0x70, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Type", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflect", 0x8C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Handle Wave Height", 0x8D, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Height Above Water", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Voice Over Entry", 0x94, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPowerUpEntity[] = {
    { "Initially Enabled", 0x70, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Type", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflect", 0x8C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Handle Wave Height", 0x8D, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPowerUpTriggerEntity[] = {
    { "Initially Enabled", 0x70, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Type", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflect", 0x8C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Handle Wave Height", 0x8D, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Height Above Water", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Voice Over Entry", 0x94, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuPushScreenUIAction[] = {
    { "Screen", 0x5C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuRaceResultsTableEntity[] = {
    { "Position X", 0x6C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Position Y", 0x70, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Text Highlight Color", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "PlaceTextX", 0x78, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "PlaceTextY", 0x7C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "PlaceTextWidth", 0x80, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "PlaceTextHeight", 0x84, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "NameRectX", 0xEC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "NameRectY", 0xF0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "NameRectWidth", 0xF4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "NameRectHeight", 0xF8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BoatRectX", 0x160, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BoatRectY", 0x164, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BoatRectWidth", 0x168, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BoatRectHeight", 0x16C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BestLapRectX", 0x1D4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BestLapRectY", 0x1D8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BestLapRectWidth", 0x1DC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "BestLapRectHeight", 0x1E0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TotalTimeRectX", 0x248, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TotalTimeRectY", 0x24C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TotalTimeRectWidth", 0x250, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TotalTimeRectHeight", 0x254, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "PointsEarnedRectX", 0x2BC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "PointsEarnedRectY", 0x2C0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "PointsEarnedRectWidth", 0x2C4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "PointsEarnedRectHeight", 0x2C8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Width", 0x334, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "VerticalSpacing", 0x338, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "HeaderColor", 0x33C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InactiveColor", 0x340, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ActiveColor", 0x344, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "PlayerColor", 0x348, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuRandomTriggerEntity[] = {
    { "Weight 1", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Weight 2", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Weight 3", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Weight 4", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Weight 5", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Weight 6", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Weight 7", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Weight 8", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuRectangularGfxSettingsEntity[] = {
    { "Always Active", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Up Time", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Duration", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Ramp Down Time", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Camera Far Plane", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Clear Color", 0x74, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Start", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog End", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x447A0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Fog Color", 0x80, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Contrast", 0x84, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Tint", 0x88, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Min", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Max", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gamma Curve", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Ambient Color", 0x98, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC0423C2D, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Diffuse Color", 0x9C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xC05A503C, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Ambient Color", 0xA0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Diffuse Color", 0xA4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Foam Texture Size", 0xA8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41600000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Ambient Color", 0xAC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF808080, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Decal Diffuse Color", 0xB0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Value", 0xB4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Water Depth Foam Intensity", 0xB8, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Ambient Color", 0xBC, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFC0C0C0, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Diffuse Color", 0xC0, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Foliage Tint", 0xC4, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Radius X %", 0xD0, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Radius Y %", 0xD4, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuRectangularOceanWaveEntity[] = {
    { "Complexity", 0x68, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000005, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Bin Siz", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Wave Direction", 0x70, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Gravity", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x411CE560, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Wind Speed", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Directional Power", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40400000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Suppression Wave Length", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Height Multiplier", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3DCCCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Decay Ratio X %", 0x8C, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F666666, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Decay Ratio Y %", 0x90, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F666666, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuRigidBodyComponent[] = {
    { "Collision Type", 0x10, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Collision Mesh Asset", 0x14, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuRigidBodyContactCallback[] = {
    { "Initially Enabled", 0x70, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Type", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflect", 0x8C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Handle Wave Height", 0x8D, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Height Above Water", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Voice Over Entry", 0x94, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuRotateMotionEntity[] = {
    { "Initially Active", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "One Shot", 0x65, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Axis", 0x70, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x3F800000, 0x00000000 }, "" },
    { "Speed", 0x80, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x3FC90FDB, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Local", 0x84, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuRotation3dProperty[] = {
    { "Spawn Per Second", 0x30, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Spawn Count", 0x34, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Lifespan", 0x38, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Lifespan", 0x3C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Color", 0x40, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Color", 0x44, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Scale", 0x48, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Scale", 0x4C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Linear Velocity", 0x50, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Max Linear Velocity", 0x60, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x40A00000, 0x00000000 }, "" },
    { "Min Position", 0x70, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Position", 0x80, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn At Water Surface", 0x90, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Spawn Distance", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x43FA0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Rotation", 0x98, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Rotation", 0xA8, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Angular Velocity", 0xB8, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Angular Velocity", 0xC8, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSayVoiceOverBoxEntity[] = {
    { "Entry", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSayVoiceOverEllipsoidEntity[] = {
    { "Entry", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSayVoiceOverEntity[] = {
    { "Entry", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuScreenFadeBoxEntity[] = {
    { "Color", 0x5C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Ratio X %", 0x64, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Ratio Y %", 0x68, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Ratio Z %", 0x6C, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuScreenFadeEllipsoidEntity[] = {
    { "Color", 0x5C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Ratio %", 0x64, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuScreenFadeEntity[] = {
    { "Color", 0x5C, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFF000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuScriptComponent[] = {
    { "Enable", 0x40, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x000000A0, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuScriptInputPlug[] = {
    { "Cheat", 0x5C, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000008, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuScriptOutputPlug[] = {
    { "Set Branch A", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Set Branch B", 0x61, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Branch A Prob.", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Branch B Prob.", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuScriptRef[] = {
    { "Speed Hint", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Start", 0x88, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "CheckPoint", 0x89, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Finish", 0x8A, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Allow Recovery", 0x8B, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Show Wrong Way", 0x8C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "CheckPoint Width", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "CheckPoint Height", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Branch A Probability", 0xA0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Branch B Probability", 0xA4, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSentientUpsellResponded[] = {
    { "Result", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSetBooleanEntity[] = {
    { "Value", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSetDefaultSettingsEntity[] = {
    { "Audio", 0x5C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "GamePadControls", 0x5D, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "KeyboardControls", 0x5E, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchTiltControls", 0x5F, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Graphics", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSetFrontEndCameraEntity[] = {
    { "Interpolation Time", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Modify Eye Position", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Modify Target Position", 0x65, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Modify Far Plane", 0x66, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Eye Position", 0x68, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Target Position", 0x78, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Far Plane", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x447A0000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSetGameModeUIAction[] = {
    { "Game Mode", 0x5C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSetInfoStringUIAction[] = {
    { "Info String ID", 0x5C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSetIntegerEntity[] = {
    { "Value", 0x60, prop_type_t::integer, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSetMenuPresenceUIAction[] = {
    { "Profiles", 0x5C, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Presence", 0x60, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "Main" },
};
static const entity_property_t g_schema_VuSetScreenUIAction[] = {
    { "Screen", 0x5C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSetStringEntity[] = {
    { "Value", 0x60, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSetTrackBranchProbabilities[] = {
    { "Set Branch A", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Set Branch B", 0x61, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Branch A Prob.", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Branch B Prob.", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuShowWebPageUIAction[] = {
    { "URL", 0x5C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSkyBoxEntity[] = {
    { "Model Asset", 0x64, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSlalomGateEntity[] = {
    { "Diameter", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Model Asset", 0x70, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflection Model Asset", 0x88, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Allow Recovery", 0xA0, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuStartActionGameUIAction[] = {
    { "Type", 0x5C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "AttractEvent" },
};
static const entity_property_t g_schema_VuStaticIntEnumProperty[] = {
    { "Image X", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Y", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Width", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Image Height", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch X", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Y", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Width", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Touch Height", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x88, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuStaticModelInstance[] = {
    { "Diameter", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Model Asset", 0x70, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflection Model Asset", 0x88, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Allow Recovery", 0xA0, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuStaticPfxEntity[] = {
    { "Effect Name", 0x70, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Initially Active", 0x88, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Scale", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Pfx Color", 0x90, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuStaticStringEnumProperty[] = {
    { "Driver Type", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "AI" },
    { "Boat", 0x8C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "Default" },
    { "Skin", 0xA4, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuStaticWakeWaveEntity[] = {
    { "Range Start Ratio %", 0x60, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3E4CCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range End Ratio %", 0x64, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F4CCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Falloff Time", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Decay Time", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Magnitude", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Speed", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Frequency", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuStridingMesh[] = {
    { "Driver Type", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "AI" },
    { "Boat", 0x8C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "Default" },
    { "Skin", 0xA4, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuStringEntity[] = {
    { "Value", 0x60, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSubstituteAssetEntity[] = {
    { "Asset Type", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSwapSkyboxEntity[] = {
    { "Model Asset", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSwitchEntity[] = {
    { "Initial State", 0x60, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuSysNotifyEntity[] = {
    { "Notification", 0x60, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuTransformComponent[] = {
    { "Position", 0x40, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation", 0x90, prop_type_t::vector3, prop_unit_t::degrees, 16, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Scale", 0xB0, prop_type_t::vector3, prop_unit_t::none, 16, true, { 0x3F800000, 0x3F800000, 0x3F800000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuTriggerEntity[] = {
    { "Initially Active", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Trigger Type", 0x68, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuTriggerPlaneEntity[] = {
    { "Initially Active", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Trigger Type", 0x68, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUIDrawComponent[] = {
    { "Depth", 0x14, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUIEventEntity[] = {
    { "Event", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUIEventEnumProperty[] = {
    { "Event", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUIGhostLeaderboardEntity[] = {
    { "Position X", 0x6C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Position Y", 0x70, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowRect Y", 0x78, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowRect Width", 0x7C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowRect X", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowSpacing", 0x84, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "NameOffsetX", 0x88, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowRect Height", 0x80, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ScoreOffsetX", 0x8C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoRect", 0x158, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoVertical Alignment", 0x1D0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoNo Clip", 0x1D4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoSingle Line", 0x1D5, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoHorizontal Alignment", 0x1CC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoWordbreak", 0x1D6, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Ratio", 0x1E0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Anchor", 0x1DC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Anchor", 0x1D8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Ratio", 0x1E4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUIImageBaseEntity[] = {
    { "Visible", 0x6C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0x6D, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstX", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstY", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstWidth", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstHeight", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcX", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcY", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcWidth", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcHeight", 0x98, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation", 0x9C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "FlipX", 0xA0, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "FlipY", 0xA1, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Absolute Coords", 0xA2, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Anchor", 0xA4, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Anchor", 0xA8, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Ratio", 0xAC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Ratio", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUIImageEntity[] = {
    { "Visible", 0x6C, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0x6D, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstY", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstWidth", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstHeight", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcX", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcY", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcWidth", 0x94, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcHeight", 0x98, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation", 0x9C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "FlipX", 0xA0, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "FlipY", 0xA1, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Absolute Coords", 0xA2, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Anchor", 0xA4, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Anchor", 0xA8, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Ratio", 0xAC, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Ratio", 0xB0, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0xB8, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUILeaderboardEntity[] = {
    { "Position X", 0x70, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ComboBoxSpacing", 0x78, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Position Y", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchLeft Y", 0x260, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchLeft Width", 0x264, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchLeft X", 0x25C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchLeft Height", 0x268, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchRight Y", 0x270, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchRight Width", 0x274, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchRight X", 0x26C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowCount", 0x27C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchRight Height", 0x278, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowRect Y", 0x284, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowRect Width", 0x288, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowRect X", 0x280, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowSpacing", 0x290, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RankOffsetX", 0x294, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "NameOffsetX", 0x298, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ScoreOffsetX", 0x29C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "RowRect Height", 0x28C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoVertical Alignment", 0x444, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoNo Clip", 0x448, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoSingle Line", 0x449, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoHorizontal Alignment", 0x440, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoWordbreak", 0x44A, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonTouch Y", 0x4B4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonTouch Width", 0x4B8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonTouch X", 0x4B0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonTouch Height", 0x4BC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonImage Y", 0x4C4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonImage Width", 0x4C8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonImage X", 0x4C0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonImage Height", 0x4CC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonText Y", 0x4D4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonText Width", 0x4D8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonText X", 0x4D0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "InfoRect", 0x3CC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonSpacing", 0x4E0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "ButtonText Height", 0x4DC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUIPageLayoutEntity[] = {
    { "Visible", 0x68, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "X", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Y", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Width", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Height", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42C80000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Anchor", 0x84, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Anchor", 0x88, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Ratio", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Ratio", 0x90, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Page Layout", 0x94, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Scroll at Start", 0xAC, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUIProgressBarEntity[] = {
    { "Visible", 0x60, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Color", 0x61, prop_type_t::color, prop_unit_t::none, 4, true, { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstX", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstY", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x42200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstWidth", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DstHeight", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcX", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcY", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcWidth", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SrcHeight", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Rotation", 0x90, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Texture Asset", 0x94, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Mask Texture Asset", 0x10C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUITextBaseEntity[] = {
    { "Visible", 0x74, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Width", 0x78, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Screen Scale", 0x70, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Height", 0x7C, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Alignment", 0xE8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "No Clip", 0xEC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Single Line", 0xED, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Alignment", 0xE4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Wordbreak", 0xEE, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Ratio", 0xF8, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Anchor", 0xF4, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Anchor", 0xF0, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Ratio", 0xFC, prop_type_t::unknown, prop_unit_t::none, 0, false, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUITouchButtonEntity[] = {
    { "Enabled", 0x64, prop_type_t::boolean, prop_unit_t::none, 1, true, { 0x00000001, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchX", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchY", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchWidth", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TouchHeight", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Anchor", 0x80, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Anchor", 0x84, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Horizontal Ratio", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Vertical Ratio", 0x8C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuUnlockAchievementEntity[] = {
    { "Achievement", 0x60, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Task", 0x78, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuWakeWaveEntity[] = {
    { "Emission Rate", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Min Emission Speed", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Emission Speed", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Magnitude", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3E800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Falloff Time", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DecayTime", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Speed", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Frequency", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range Start Ratio %", 0x8C, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3E4CCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Range End Ratio %", 0x90, prop_type_t::real, prop_unit_t::percent, 4, true, { 0x3F4CCCCD, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuWaterRampEntity[] = {
    { "X Size", 0x70, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000100, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Y Size", 0x74, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000100, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Wave Depth", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Wave Height", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflection Height", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflection Offset", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Draw Distance", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x7F7FFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "WaterMap", 0x8C, prop_type_t::text, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuWaterSurfaceEntity[] = {
    { "X Size", 0x70, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000100, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Y Size", 0x74, prop_type_t::enumeration, prop_unit_t::none, 4, true, { 0x00000100, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Wave Depth", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Max Wave Height", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflection Height", 0x80, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Reflection Offset", 0x84, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Draw Distance", 0x88, prop_type_t::real, prop_unit_t::none, 4, true, { 0x7F7FFFFF, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "WaterMap", 0x8C, prop_type_t::unknown, prop_unit_t::none, 0, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuWaterTextureSettingsEntity[] = {
    { "Gravity", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x411CD0E5, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "WorldSize", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "WindSpeed", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "DirectionalPower", 0x6C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "SuppressionWaveLength", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "HeightFactor", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "TimeFactor", 0x78, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "NormalTextureScale", 0x7C, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};
static const entity_property_t g_schema_VuWhirlpoolWaveEntity[] = {
    { "Outer Radius", 0x60, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Inner Radius", 0x64, prop_type_t::real, prop_unit_t::none, 4, true, { 0x40A00000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Depth", 0x68, prop_type_t::real, prop_unit_t::none, 4, true, { 0x41200000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Angular Speed", 0x6C, prop_type_t::real, prop_unit_t::degrees, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Linear Speed", 0x70, prop_type_t::real, prop_unit_t::none, 4, true, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, "" },
    { "Foaminess", 0x74, prop_type_t::real, prop_unit_t::none, 4, true, { 0x3F800000, 0x00000000, 0x00000000, 0x00000000 }, "" },
};

static const entity_schema_t g_entity_schemas[] = {
    { "Vu3dDrawAnimatedModelComponent", g_schema_Vu3dDrawAnimatedModelComponent, 6, true },
    { "Vu3dDrawBreakableModelComponent", g_schema_Vu3dDrawBreakableModelComponent, 16, true },
    { "Vu3dDrawComponent", g_schema_Vu3dDrawComponent, 2, true },
    { "Vu3dDrawStaticModelComponent", g_schema_Vu3dDrawStaticModelComponent, 12, true },
    { "VuAddBoostEntity", g_schema_VuAddBoostEntity, 1, true },
    { "VuAddIntegerConstantEntity", g_schema_VuAddIntegerConstantEntity, 1, true },
    { "VuAiNuisanceDriverStartEntity", g_schema_VuAiNuisanceDriverStartEntity, 1, true },
    { "VuAiWaypointEntity", g_schema_VuAiWaypointEntity, 10, true },
    { "VuAmbientLightEntity", g_schema_VuAmbientLightEntity, 1, true },
    { "VuAngleProperty", g_schema_VuAngleProperty, 26, true },
    { "VuAnimatedAttachComponent", g_schema_VuAnimatedAttachComponent, 1, true },
    { "VuAnimatedPropEntity", g_schema_VuAnimatedPropEntity, 1, true },
    { "VuAnimationEventIF", g_schema_VuAnimationEventIF, 1, true },
    { "VuAssetNameProperty", g_schema_VuAssetNameProperty, 16, true },
    { "VuAttachEntity", g_schema_VuAttachEntity, 4, true },
    { "VuAudioBankEntity", g_schema_VuAudioBankEntity, 1, true },
    { "VuAudioDspEntity", g_schema_VuAudioDspEntity, 3, true },
    { "VuAudioDuckingEntity", g_schema_VuAudioDuckingEntity, 4, true },
    { "VuAudioEmitterEntity", g_schema_VuAudioEmitterEntity, 2, true },
    { "VuAudioEventEntity", g_schema_VuAudioEventEntity, 2, true },
    { "VuAudioEventNameProperty", g_schema_VuAudioEventNameProperty, 11, true },
    { "VuAudioProjectEntity", g_schema_VuAudioProjectEntity, 1, true },
    { "VuAudioReverbEntity", g_schema_VuAudioReverbEntity, 4, true },
    { "VuAudioReverbNameProperty", g_schema_VuAudioReverbNameProperty, 4, true },
    { "VuBankedTurnWaveEntity", g_schema_VuBankedTurnWaveEntity, 5, true },
    { "VuBarracudaTeamScoreUITextEntity", g_schema_VuBarracudaTeamScoreUITextEntity, 1, true },
    { "VuBarracudaTrackUIImageEntity", g_schema_VuBarracudaTrackUIImageEntity, 17, true },
    { "VuBarracudaTuningUITextEntity", g_schema_VuBarracudaTuningUITextEntity, 1, true },
    { "VuBarracudaUnlockUITextEntity", g_schema_VuBarracudaUnlockUITextEntity, 1, true },
    { "VuBaseOceanWaveEntity", g_schema_VuBaseOceanWaveEntity, 8, true },
    { "VuBoatEntity", g_schema_VuBoatEntity, 3, true },
    { "VuBoatStatEntity", g_schema_VuBoatStatEntity, 5, true },
    { "VuBooleanEntity", g_schema_VuBooleanEntity, 1, true },
    { "VuBumpWaveEntity", g_schema_VuBumpWaveEntity, 2, true },
    { "VuCheatEntity", g_schema_VuCheatEntity, 1, true },
    { "VuCheckpointBuoyEntity", g_schema_VuCheckpointBuoyEntity, 26, true },
    { "VuCircularOceanWaveEntity", g_schema_VuCircularOceanWaveEntity, 10, true },
    { "VuConstStringEnumProperty", g_schema_VuConstStringEnumProperty, 2, true },
    { "VuControlComponent", g_schema_VuControlComponent, 17, true },
    { "VuControlComponentIF", g_schema_VuControlComponentIF, 18, true },
    { "VuCoronaEntity", g_schema_VuCoronaEntity, 15, true },
    { "VuCounterEntity", g_schema_VuCounterEntity, 2, true },
    { "VuDevJumpPointEntity", g_schema_VuDevJumpPointEntity, 3, true },
    { "VuDifficultyBaseEntity", g_schema_VuDifficultyBaseEntity, 1, true },
    { "VuDifficultyImageEntity", g_schema_VuDifficultyImageEntity, 5, true },
    { "VuDifficultyTextEntity", g_schema_VuDifficultyTextEntity, 1, true },
    { "VuDirectionalCoronaEntity", g_schema_VuDirectionalCoronaEntity, 6, true },
    { "VuDirectionalFlowWaveEntity", g_schema_VuDirectionalFlowWaveEntity, 3, true },
    { "VuDirectionalLightEntity", g_schema_VuDirectionalLightEntity, 3, true },
    { "VuDirectionalWaveEntity", g_schema_VuDirectionalWaveEntity, 5, true },
    { "VuDynamicGamePropEntity", g_schema_VuDynamicGamePropEntity, 25, true },
    { "VuDynamicLightEntity", g_schema_VuDynamicLightEntity, 10, true },
    { "VuDynamicsStepCallback", g_schema_VuDynamicsStepCallback, 25, true },
    { "VuEllipticalGfxSettingsEntity", g_schema_VuEllipticalGfxSettingsEntity, 27, true },
    { "VuExplosionEntity", g_schema_VuExplosionEntity, 11, true },
    { "VuFoliageEntity", g_schema_VuFoliageEntity, 2, true },
    { "VuForceAffectorEntity", g_schema_VuForceAffectorEntity, 3, true },
    { "VuGenericEventEntity", g_schema_VuGenericEventEntity, 1, false },
    { "VuGfxSettingsEntity", g_schema_VuGfxSettingsEntity, 26, true },
    { "VuGhostBoatEntity", g_schema_VuGhostBoatEntity, 3, true },
    { "VuGlobalGfxSettingsEntity", g_schema_VuGlobalGfxSettingsEntity, 26, true },
    { "VuGroupEntity", g_schema_VuGroupEntity, 1, true },
    { "VuHUDButtonEntity", g_schema_VuHUDButtonEntity, 9, true },
    { "VuHUDElementEntity", g_schema_VuHUDElementEntity, 1, true },
    { "VuHUDOnScreenAxisEntity", g_schema_VuHUDOnScreenAxisEntity, 9, true },
    { "VuHUDOnScreenButtonEntity", g_schema_VuHUDOnScreenButtonEntity, 9, true },
    { "VuHUDOnScreenControlEntity", g_schema_VuHUDOnScreenControlEntity, 9, true },
    { "VuHUDRecoverButtonEntity", g_schema_VuHUDRecoverButtonEntity, 9, true },
    { "VuInfiniteOceanWaveEntity", g_schema_VuInfiniteOceanWaveEntity, 8, true },
    { "VuInputAxisEnumProperty", g_schema_VuInputAxisEnumProperty, 9, true },
    { "VuInputButtonEnumProperty", g_schema_VuInputButtonEnumProperty, 9, true },
    { "VuInstigatorComponent", g_schema_VuInstigatorComponent, 3, true },
    { "VuIntegerEntity", g_schema_VuIntegerEntity, 1, true },
    { "VuIsAchievementUnlockedEntity", g_schema_VuIsAchievementUnlockedEntity, 1, true },
    { "VuIsEventTypeEntity", g_schema_VuIsEventTypeEntity, 1, true },
    { "VuIsSkuEntity", g_schema_VuIsSkuEntity, 1, true },
    { "VuKeyframeEntity", g_schema_VuKeyframeEntity, 1, true },
    { "VuKeyframeMotionEntity", g_schema_VuKeyframeMotionEntity, 7, true },
    { "VuLensWaterEmitterEntity", g_schema_VuLensWaterEmitterEntity, 2, true },
    { "VuLoadingProgressBarEntity", g_schema_VuLoadingProgressBarEntity, 3, true },
    { "VuMessageBoxEntity", g_schema_VuMessageBoxEntity, 11, true },
    { "VuMessageBoxScreenEntity", g_schema_VuMessageBoxScreenEntity, 74, true },
    { "VuModelSkin", g_schema_VuModelSkin, 3, true },
    { "VuMotionComponentIF", g_schema_VuMotionComponentIF, 11, true },
    { "VuMotionEntity", g_schema_VuMotionEntity, 2, true },
    { "VuMultiDelayEntity", g_schema_VuMultiDelayEntity, 8, true },
    { "VuNotifyProperty", g_schema_VuNotifyProperty, 26, true },
    { "VuOffsetAttachComponent", g_schema_VuOffsetAttachComponent, 3, true },
    { "VuOnKeyboardEventEntity", g_schema_VuOnKeyboardEventEntity, 4, true },
    { "VuOnTouchSpecialEntity", g_schema_VuOnTouchSpecialEntity, 1, true },
    { "VuOneShotPfxEntity", g_schema_VuOneShotPfxEntity, 4, true },
    { "VuPercentageProperty", g_schema_VuPercentageProperty, 28, true },
    { "VuPersistentBaseEntity", g_schema_VuPersistentBaseEntity, 2, true },
    { "VuPersistentBooleanEntity", g_schema_VuPersistentBooleanEntity, 2, true },
    { "VuPersistentIntegerEntity", g_schema_VuPersistentIntegerEntity, 2, true },
    { "VuPersistentStringEntity", g_schema_VuPersistentStringEntity, 2, true },
    { "VuPfxBoatSpringConstraint", g_schema_VuPfxBoatSpringConstraint, 3, true },
    { "VuPfxEmitBoatHull", g_schema_VuPfxEmitBoatHull, 17, true },
    { "VuPfxEmitBoatThrustQuadFountain", g_schema_VuPfxEmitBoatThrustQuadFountain, 27, true },
    { "VuPfxEmitFountain", g_schema_VuPfxEmitFountain, 14, true },
    { "VuPfxEmitGeomFountain", g_schema_VuPfxEmitGeomFountain, 18, true },
    { "VuPfxEmitQuadFountain", g_schema_VuPfxEmitQuadFountain, 26, true },
    { "VuPfxGeomPattern", g_schema_VuPfxGeomPattern, 6, true },
    { "VuPfxNode", g_schema_VuPfxNode, 3, true },
    { "VuPfxPattern", g_schema_VuPfxPattern, 3, true },
    { "VuPfxProjectEntity", g_schema_VuPfxProjectEntity, 1, true },
    { "VuPfxQuadPattern", g_schema_VuPfxQuadPattern, 10, true },
    { "VuPfxSpringConstraint", g_schema_VuPfxSpringConstraint, 6, true },
    { "VuPfxSystem", g_schema_VuPfxSystem, 1, true },
    { "VuPfxTickAlpha", g_schema_VuPfxTickAlpha, 2, true },
    { "VuPfxTickAlphaInOut", g_schema_VuPfxTickAlphaInOut, 4, true },
    { "VuPfxTickDampenVelocity", g_schema_VuPfxTickDampenVelocity, 2, true },
    { "VuPfxTickLinearAcceleration", g_schema_VuPfxTickLinearAcceleration, 4, true },
    { "VuPfxTickScale", g_schema_VuPfxTickScale, 2, true },
    { "VuPfxTickWorldScaleZ", g_schema_VuPfxTickWorldScaleZ, 2, true },
    { "VuPfxTrailPattern", g_schema_VuPfxTrailPattern, 12, true },
    { "VuPlayAnimationEntity", g_schema_VuPlayAnimationEntity, 2, true },
    { "VuPointWaveCallbackIF", g_schema_VuPointWaveCallbackIF, 7, true },
    { "VuPointWaveEntity", g_schema_VuPointWaveEntity, 7, true },
    { "VuPowerUpBoostEntity", g_schema_VuPowerUpBoostEntity, 6, false },
    { "VuPowerUpEntity", g_schema_VuPowerUpEntity, 4, true },
    { "VuPowerUpTriggerEntity", g_schema_VuPowerUpTriggerEntity, 6, false },
    { "VuPushScreenUIAction", g_schema_VuPushScreenUIAction, 1, true },
    { "VuRaceResultsTableEntity", g_schema_VuRaceResultsTableEntity, 33, true },
    { "VuRandomTriggerEntity", g_schema_VuRandomTriggerEntity, 8, true },
    { "VuRectangularGfxSettingsEntity", g_schema_VuRectangularGfxSettingsEntity, 28, true },
    { "VuRectangularOceanWaveEntity", g_schema_VuRectangularOceanWaveEntity, 10, true },
    { "VuRigidBodyComponent", g_schema_VuRigidBodyComponent, 2, true },
    { "VuRigidBodyContactCallback", g_schema_VuRigidBodyContactCallback, 6, true },
    { "VuRotateMotionEntity", g_schema_VuRotateMotionEntity, 5, true },
    { "VuRotation3dProperty", g_schema_VuRotation3dProperty, 18, true },
    { "VuSayVoiceOverBoxEntity", g_schema_VuSayVoiceOverBoxEntity, 1, true },
    { "VuSayVoiceOverEllipsoidEntity", g_schema_VuSayVoiceOverEllipsoidEntity, 1, true },
    { "VuSayVoiceOverEntity", g_schema_VuSayVoiceOverEntity, 1, true },
    { "VuScreenFadeBoxEntity", g_schema_VuScreenFadeBoxEntity, 4, true },
    { "VuScreenFadeEllipsoidEntity", g_schema_VuScreenFadeEllipsoidEntity, 2, true },
    { "VuScreenFadeEntity", g_schema_VuScreenFadeEntity, 1, true },
    { "VuScriptComponent", g_schema_VuScriptComponent, 1, true },
    { "VuScriptInputPlug", g_schema_VuScriptInputPlug, 1, true },
    { "VuScriptOutputPlug", g_schema_VuScriptOutputPlug, 4, true },
    { "VuScriptRef", g_schema_VuScriptRef, 10, true },
    { "VuSentientUpsellResponded", g_schema_VuSentientUpsellResponded, 1, true },
    { "VuSetBooleanEntity", g_schema_VuSetBooleanEntity, 1, true },
    { "VuSetDefaultSettingsEntity", g_schema_VuSetDefaultSettingsEntity, 5, true },
    { "VuSetFrontEndCameraEntity", g_schema_VuSetFrontEndCameraEntity, 7, true },
    { "VuSetGameModeUIAction", g_schema_VuSetGameModeUIAction, 1, true },
    { "VuSetInfoStringUIAction", g_schema_VuSetInfoStringUIAction, 1, true },
    { "VuSetIntegerEntity", g_schema_VuSetIntegerEntity, 1, true },
    { "VuSetMenuPresenceUIAction", g_schema_VuSetMenuPresenceUIAction, 2, true },
    { "VuSetScreenUIAction", g_schema_VuSetScreenUIAction, 1, true },
    { "VuSetStringEntity", g_schema_VuSetStringEntity, 1, true },
    { "VuSetTrackBranchProbabilities", g_schema_VuSetTrackBranchProbabilities, 4, true },
    { "VuShowWebPageUIAction", g_schema_VuShowWebPageUIAction, 1, true },
    { "VuSkyBoxEntity", g_schema_VuSkyBoxEntity, 1, false },
    { "VuSlalomGateEntity", g_schema_VuSlalomGateEntity, 4, true },
    { "VuStartActionGameUIAction", g_schema_VuStartActionGameUIAction, 1, true },
    { "VuStaticIntEnumProperty", g_schema_VuStaticIntEnumProperty, 9, true },
    { "VuStaticModelInstance", g_schema_VuStaticModelInstance, 4, true },
    { "VuStaticPfxEntity", g_schema_VuStaticPfxEntity, 4, true },
    { "VuStaticStringEnumProperty", g_schema_VuStaticStringEnumProperty, 3, true },
    { "VuStaticWakeWaveEntity", g_schema_VuStaticWakeWaveEntity, 7, true },
    { "VuStridingMesh", g_schema_VuStridingMesh, 3, true },
    { "VuStringEntity", g_schema_VuStringEntity, 1, true },
    { "VuSubstituteAssetEntity", g_schema_VuSubstituteAssetEntity, 1, true },
    { "VuSwapSkyboxEntity", g_schema_VuSwapSkyboxEntity, 1, true },
    { "VuSwitchEntity", g_schema_VuSwitchEntity, 1, true },
    { "VuSysNotifyEntity", g_schema_VuSysNotifyEntity, 1, false },
    { "VuTransformComponent", g_schema_VuTransformComponent, 3, true },
    { "VuTriggerEntity", g_schema_VuTriggerEntity, 2, true },
    { "VuTriggerPlaneEntity", g_schema_VuTriggerPlaneEntity, 2, true },
    { "VuUIDrawComponent", g_schema_VuUIDrawComponent, 1, true },
    { "VuUIEventEntity", g_schema_VuUIEventEntity, 1, false },
    { "VuUIEventEnumProperty", g_schema_VuUIEventEnumProperty, 1, true },
    { "VuUIGhostLeaderboardEntity", g_schema_VuUIGhostLeaderboardEntity, 19, true },
    { "VuUIImageBaseEntity", g_schema_VuUIImageBaseEntity, 18, true },
    { "VuUIImageEntity", g_schema_VuUIImageEntity, 18, true },
    { "VuUILeaderboardEntity", g_schema_VuUILeaderboardEntity, 39, true },
    { "VuUIPageLayoutEntity", g_schema_VuUIPageLayoutEntity, 11, true },
    { "VuUIProgressBarEntity", g_schema_VuUIProgressBarEntity, 13, true },
    { "VuUITextBaseEntity", g_schema_VuUITextBaseEntity, 13, true },
    { "VuUITouchButtonEntity", g_schema_VuUITouchButtonEntity, 9, false },
    { "VuUnlockAchievementEntity", g_schema_VuUnlockAchievementEntity, 2, true },
    { "VuWakeWaveEntity", g_schema_VuWakeWaveEntity, 11, true },
    { "VuWaterRampEntity", g_schema_VuWaterRampEntity, 8, false },
    { "VuWaterSurfaceEntity", g_schema_VuWaterSurfaceEntity, 8, true },
    { "VuWaterTextureSettingsEntity", g_schema_VuWaterTextureSettingsEntity, 8, true },
    { "VuWhirlpoolWaveEntity", g_schema_VuWhirlpoolWaveEntity, 6, true },
};

static const int g_entity_schema_count = 187;

// Linear search: the table is read once per entity during the load walk,
// not per frame, and building a map would cost more than the walk saves.
inline const entity_schema_t* entity_schema_find( const char* class_name )
{
    if ( class_name == nullptr )
        return nullptr;
    for ( int index = 0; index < g_entity_schema_count; ++index )
    {
        if ( std::strcmp( g_entity_schemas[ index ].m_name, class_name ) == 0 )
            return &g_entity_schemas[ index ];
    }
    return nullptr;
}

inline const entity_property_t* entity_schema_property(
    const entity_schema_t* schema, const char* key )
{
    if ( schema == nullptr || key == nullptr )
        return nullptr;
    for ( int index = 0; index < schema->m_count; ++index )
    {
        if ( std::strcmp( schema->m_properties[ index ].m_name, key ) == 0 )
            return &schema->m_properties[ index ];
    }
    return nullptr;
}

// The default as retail left it. Reinterpreted, never recomputed.
inline float entity_schema_default_real( const entity_property_t& property )
{
    float value = 0.0f;
    std::memcpy( &value, &property.m_default[ 0 ], sizeof( value ) );
    return value;
}
