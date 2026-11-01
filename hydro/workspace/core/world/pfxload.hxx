#pragma once

#include <workspace/core/world/pfxsim.hxx>
#include <workspace/core/entity/actions.hxx>
#include <workspace/core/entity/property.hxx>
#include <workspace/core/entity/values.hxx>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

// THE EFFECT-ASSET LOAD: VuPfxAsset -> VuPfxImpl's project -> the node tree on
// pfx_runtime_t's heap, as retail builds it (verified: case pfx_load, the
// race's Hydroelectric and Generic documents, 1232 nodes).
//
// VuPfxAsset load slot 5 0x5CAB30: 0x5C1270 reads a u32 size and the VUJB
// document after it (0x55B560 into asset +0x58), then g_vupfximpl slot 7
// addProject 0x51D6C0( asset name, document ): refused when the name is in the
// projects map already (0x51DB40); a VuPfxGroup 0x51D7E0 (empty name) loads the
// document through VuPfxNode slot 2 0x563D40 and goes in the map by name.
// Slot 8 0x51D870 reloads a project in place (the old group's refcount
// released), slot 9 0x51D9C0 removes one.
//
// VuPfxNode slot 2 0x563D40 (every class's): each property of the node's +0x14
// list (next +0x10) runs its load (vt+8) on doc["Properties"]; the ChildNodes
// loader 0x563EE0 on doc["ChildNodes"]; then slot 4 (a nullsub but for the geom
// pattern's 0x58FE60). 0x563EE0, per key of the ChildNodes map in its order
// (0x4FBE90 count, 0x4FC000 the i-th key): "BaseType" picks the kind - "group"
// a VuPfxGroup (0x30 bytes), "system" a VuPfxSystem 0x58A710 (0x34), "pattern"
// the pattern registry (g_vupfximpl +4 -> +0, keyed by "Type", entry +0x40 the
// create), "process" 0x564DC0 (the registry's generic map +8 by "Type" first,
// then the parent's pattern entry +0x44 by "Type"; the parent's type name from
// its slot 1); anything else, or an unregistered type, is skipped. The child
// gets its key as name (+0x18), loads (slot 2) and goes into the parent's +0xC
// map (0x501700) by name. The registry is VuPfxImpl's init 0x51C840 (0x564B70 /
// 0x564CB0, sixteen types) plus the game's three boat processes (0x4D8F79..).
//
// A node here: +0 its node vtable, +4 the refcount 1, +8 0, +0x10 its child
// count, its params from +0x30 at retail's offsets. NOT MODELLED (the words are
// pointers): +0xC the children map (rt.children, the names in g's maps), +0x14
// the property list, +0x18 the name (g's names). std::string members (texture /
// model names) live in g's strings by (node, offset). SUBSTITUTION: std::map for
// MSVC's tree (the same order: std::string's < is the VuString compare
// 0x404AC0).
//
// The property classes are entity/property.hxx's and values.hxx's verified
// loads (target reset to the ctor's default, read, filter, store if it
// differs): float 0x737484, angle 0x73A490, percentage 0x73B2DC, int 0x73A218,
// static int enum 0x74C354 (+0x20 the choices), bool 0x737450 (one byte),
// VuVector3 0x737A84, VuRotation3dProperty 0x73D24C (actions.hxx), VuColor
// 0x739DA8, VuAssetNameProperty 0x74D690 (the string load 0x406C20 / 0x406CD0)
// and VuAssetProperty<VuTextureAsset> 0x7389D4 (the same string load; when the
// value is stored its vt+0x2C 0x426840 releases and loads the texture through
// the asset factory: HOOK, `texture`). The geom pattern's slot 4 0x58FE60:
// VuStaticModelInstance (+0x5C) setModelAsset 0x520080( "Model Asset" ) - HOOK,
// `model` - then +0x70 = "Rejection Scale Modifier" (+0x54).
// UNKNOWN: a stored VuVector3 / VuRotation3d target's FOURTH lane. The value
// read 0x409330 copies 16 bytes (two movq) from its stack temporaries, whose w
// lane nothing writes - measured: the key string's length left by 0x4FBDF0
// (0x13 "Min Linear Velocity", 0xC "Min Position"), "Min " text, heap
// pointers. Left at the ctor's 0 here and never compared.

enum pfx_prop_kind_t
{
    pfx_prop_float,
    pfx_prop_angle,
    pfx_prop_percent,
    pfx_prop_int,
    pfx_prop_choice,
    pfx_prop_flag,
    pfx_prop_vec3,
    pfx_prop_rot3,
    pfx_prop_color,
    pfx_prop_name,
    pfx_prop_texture
};

struct pfx_choices_t
{
    const property_choice_t* choices;
    int count;
};

// One property node as its ctor builds it: the name, the class, the target's
// offset in the node, the default (+0x14: a word, a byte, three lanes; the
// strings' is ""), the enum's table (+0x20).
struct pfx_prop_t
{
    const char* name;
    pfx_prop_kind_t kind;
    std::uint32_t offset;
    std::uint32_t fallback[ 3 ];
    const pfx_choices_t* choices;
};

// The enum tables (8-byte {name, value} entries, null-ended) at 0x735BBC
// "Space", 0x735C74 / 0x735C54 the quad pattern's Blend Mode / Water
// Interaction, 0x735C3C / 0x735C24 / 0x735C04 the trail's Trail Type / Blend
// Mode / Water Interaction.
inline const property_choice_t g_pfx_space_c[] = { { "World", 0 }, { "Object", 1 } };
inline const property_choice_t g_pfx_blend_c[] = { { "Additive", 0 }, { "Modulate", 1 } };
inline const property_choice_t g_pfx_water_c[] = { { "Above", 0 }, { "Below", 1 }, { "UI", 2 } };
inline const property_choice_t g_pfx_trail_type_c[] = { { "2D Trail", 0 }, { "3D Ribbon", 1 } };
inline const pfx_choices_t g_pfx_space[ 1 ] = { { g_pfx_space_c, 2 } };
inline const pfx_choices_t g_pfx_quad_blend[ 1 ] = { { g_pfx_blend_c, 2 } };
inline const pfx_choices_t g_pfx_quad_water[ 1 ] = { { g_pfx_water_c, 3 } };
inline const pfx_choices_t g_pfx_trail_type[ 1 ] = { { g_pfx_trail_type_c, 2 } };
inline const pfx_choices_t g_pfx_trail_blend[ 1 ] = { { g_pfx_blend_c, 2 } };
inline const pfx_choices_t g_pfx_trail_water[ 1 ] = { { g_pfx_water_c, 3 } };

// THE PROPERTY LISTS, in each ctor's order (the pattern base 0x58B030 adds Max
// Particle Count / Space / Start Delay first; the process base 0x4CD890 none;
// VuPfxEmitDirectionalQuadFountain is the quad fountain's ctor 0x55DAE0 with
// its own vtable, VuPfxEmitBoatThrustQuadFountain the same ctor plus "Test
// Drive"). Read by running each create under emulation and walking +0x14.
// Quad (node vtable 0x7505AC, 0xA0 bytes; ctor 0x5914F0)
inline const pfx_prop_t g_pfx_props_Quad[] = {
    { "Max Particle Count", pfx_prop_int, 0x30u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Space", pfx_prop_choice, 0x34u, { 0x0u, 0x0u, 0x0u }, g_pfx_space },
    { "Start Delay", pfx_prop_float, 0x38u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Texture Asset", pfx_prop_texture, 0x3Cu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Blend Mode", pfx_prop_choice, 0x6Cu, { 0x0u, 0x0u, 0x0u }, g_pfx_quad_blend },
    { "Water Interaction", pfx_prop_choice, 0x70u, { 0x0u, 0x0u, 0x0u }, g_pfx_quad_water },
    { "Clip Threshold", pfx_prop_float, 0x74u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Near Fade Min", pfx_prop_float, 0x78u, { 0x40000000u, 0x0u, 0x0u }, nullptr },
    { "Near Fade Max", pfx_prop_float, 0x7Cu, { 0x40800000u, 0x0u, 0x0u }, nullptr },
    { "Tile Texture Asset", pfx_prop_texture, 0x54u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Tile Scroll Speed U", pfx_prop_float, 0x80u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Tile Scroll Speed V", pfx_prop_float, 0x84u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Tile Scroll Loop Time", pfx_prop_float, 0x88u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Tile Scale", pfx_prop_float, 0x8Cu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Stretch", pfx_prop_float, 0x90u, { 0x7F7FFFFFu, 0x0u, 0x0u }, nullptr },
    { "Fog Enabled", pfx_prop_flag, 0x94u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// EmitQuadFountain (node vtable 0x74EF2C, 0xC8 bytes; ctor 0x55DAE0)
inline const pfx_prop_t g_pfx_props_EmitQuadFountain[] = {
    { "Spawn Per Second", pfx_prop_float, 0x30u, { 0x41200000u, 0x0u, 0x0u }, nullptr },
    { "Max Spawn Count", pfx_prop_int, 0x34u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Lifespan", pfx_prop_float, 0x38u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Lifespan", pfx_prop_float, 0x3Cu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Color", pfx_prop_color, 0x40u, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
    { "Max Color", pfx_prop_color, 0x44u, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
    { "Min Scale", pfx_prop_float, 0x48u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Scale", pfx_prop_float, 0x4Cu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Position", pfx_prop_vec3, 0x70u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Position", pfx_prop_vec3, 0x80u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Linear Velocity", pfx_prop_vec3, 0x50u, { 0x0u, 0x0u, 0x40A00000u }, nullptr },
    { "Max Linear Velocity", pfx_prop_vec3, 0x60u, { 0x0u, 0x0u, 0x40A00000u }, nullptr },
    { "Spawn At Water Surface", pfx_prop_flag, 0x90u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Spawn Distance", pfx_prop_float, 0x94u, { 0x43FA0000u, 0x0u, 0x0u }, nullptr },
    { "Min Rotation", pfx_prop_angle, 0x98u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Rotation", pfx_prop_angle, 0x9Cu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Angular Velocity", pfx_prop_angle, 0xA0u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Angular Velocity", pfx_prop_angle, 0xA4u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min World Scale Z", pfx_prop_float, 0xA8u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max World Scale Z", pfx_prop_float, 0xACu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Directional Stretch", pfx_prop_float, 0xB0u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Directional Stretch", pfx_prop_float, 0xB4u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Tile Offset U", pfx_prop_float, 0xB8u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Tile Offset U", pfx_prop_float, 0xBCu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Tile Offset V", pfx_prop_float, 0xC0u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Tile Offset V", pfx_prop_float, 0xC4u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// EmitDirectional (node vtable 0x74EEC4, 0xC8 bytes; ctor 0x55DAE0)
inline const pfx_prop_t g_pfx_props_EmitDirectional[] = {
    { "Spawn Per Second", pfx_prop_float, 0x30u, { 0x41200000u, 0x0u, 0x0u }, nullptr },
    { "Max Spawn Count", pfx_prop_int, 0x34u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Lifespan", pfx_prop_float, 0x38u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Lifespan", pfx_prop_float, 0x3Cu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Color", pfx_prop_color, 0x40u, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
    { "Max Color", pfx_prop_color, 0x44u, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
    { "Min Scale", pfx_prop_float, 0x48u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Scale", pfx_prop_float, 0x4Cu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Position", pfx_prop_vec3, 0x70u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Position", pfx_prop_vec3, 0x80u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Linear Velocity", pfx_prop_vec3, 0x50u, { 0x0u, 0x0u, 0x40A00000u }, nullptr },
    { "Max Linear Velocity", pfx_prop_vec3, 0x60u, { 0x0u, 0x0u, 0x40A00000u }, nullptr },
    { "Spawn At Water Surface", pfx_prop_flag, 0x90u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Spawn Distance", pfx_prop_float, 0x94u, { 0x43FA0000u, 0x0u, 0x0u }, nullptr },
    { "Min Rotation", pfx_prop_angle, 0x98u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Rotation", pfx_prop_angle, 0x9Cu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Angular Velocity", pfx_prop_angle, 0xA0u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Angular Velocity", pfx_prop_angle, 0xA4u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min World Scale Z", pfx_prop_float, 0xA8u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max World Scale Z", pfx_prop_float, 0xACu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Directional Stretch", pfx_prop_float, 0xB0u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Directional Stretch", pfx_prop_float, 0xB4u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Tile Offset U", pfx_prop_float, 0xB8u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Tile Offset U", pfx_prop_float, 0xBCu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Tile Offset V", pfx_prop_float, 0xC0u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Tile Offset V", pfx_prop_float, 0xC4u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// TickAlpha (node vtable 0x750534, 0x38 bytes; ctor 0x591210)
inline const pfx_prop_t g_pfx_props_TickAlpha[] = {
    { "Start Delay", pfx_prop_float, 0x30u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Rate", pfx_prop_float, 0x34u, { 0xBF800000u, 0x0u, 0x0u }, nullptr },
};
// TickAlphaInOut (node vtable 0x7504C8, 0x40 bytes; ctor 0x590EB0)
inline const pfx_prop_t g_pfx_props_TickAlphaInOut[] = {
    { "Fade In Duration", pfx_prop_float, 0x30u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Fade In Rate", pfx_prop_float, 0x34u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Fade Out Start Time", pfx_prop_float, 0x38u, { 0x40000000u, 0x0u, 0x0u }, nullptr },
    { "Fade Out Rate", pfx_prop_float, 0x3Cu, { 0xBF800000u, 0x0u, 0x0u }, nullptr },
};
// TickScale (node vtable 0x750448, 0x38 bytes; ctor 0x590C50)
inline const pfx_prop_t g_pfx_props_TickScale[] = {
    { "Start Delay", pfx_prop_float, 0x30u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Rate", pfx_prop_percent, 0x34u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// TickWorldScaleZ (node vtable 0x7503C4, 0x38 bytes; ctor 0x5909F0)
inline const pfx_prop_t g_pfx_props_TickWorldScaleZ[] = {
    { "Start Delay", pfx_prop_float, 0x30u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Rate", pfx_prop_percent, 0x34u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// Geom (node vtable 0x750328, 0x7C bytes; ctor 0x58FB20)
inline const pfx_prop_t g_pfx_props_Geom[] = {
    { "Max Particle Count", pfx_prop_int, 0x30u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Space", pfx_prop_choice, 0x34u, { 0x0u, 0x0u, 0x0u }, g_pfx_space },
    { "Start Delay", pfx_prop_float, 0x38u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Model Asset", pfx_prop_name, 0x3Cu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Rejection Scale Modifier", pfx_prop_float, 0x54u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Dynamic Lighting", pfx_prop_flag, 0x58u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// EmitGeomFountain (node vtable 0x74EEF8, 0xD8 bytes; ctor 0x55E310)
inline const pfx_prop_t g_pfx_props_EmitGeomFountain[] = {
    { "Spawn Per Second", pfx_prop_float, 0x30u, { 0x41200000u, 0x0u, 0x0u }, nullptr },
    { "Max Spawn Count", pfx_prop_int, 0x34u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Lifespan", pfx_prop_float, 0x38u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Lifespan", pfx_prop_float, 0x3Cu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Color", pfx_prop_color, 0x40u, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
    { "Max Color", pfx_prop_color, 0x44u, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
    { "Min Scale", pfx_prop_float, 0x48u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Scale", pfx_prop_float, 0x4Cu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Position", pfx_prop_vec3, 0x70u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Position", pfx_prop_vec3, 0x80u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Linear Velocity", pfx_prop_vec3, 0x50u, { 0x0u, 0x0u, 0x40A00000u }, nullptr },
    { "Max Linear Velocity", pfx_prop_vec3, 0x60u, { 0x0u, 0x0u, 0x40A00000u }, nullptr },
    { "Spawn At Water Surface", pfx_prop_flag, 0x90u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Spawn Distance", pfx_prop_float, 0x94u, { 0x43FA0000u, 0x0u, 0x0u }, nullptr },
    { "Min Rotation", pfx_prop_rot3, 0x98u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Rotation", pfx_prop_rot3, 0xA8u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Angular Velocity", pfx_prop_rot3, 0xB8u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Angular Velocity", pfx_prop_rot3, 0xC8u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// Trail (node vtable 0x750294, 0x90 bytes; ctor 0x58E9F0)
inline const pfx_prop_t g_pfx_props_Trail[] = {
    { "Max Particle Count", pfx_prop_int, 0x30u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Space", pfx_prop_choice, 0x34u, { 0x0u, 0x0u, 0x0u }, g_pfx_space },
    { "Start Delay", pfx_prop_float, 0x38u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Trail Type", pfx_prop_choice, 0x3Cu, { 0x0u, 0x0u, 0x0u }, g_pfx_trail_type },
    { "Lifespan", pfx_prop_float, 0x40u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Fade In Time", pfx_prop_float, 0x44u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Fade Out Start Time", pfx_prop_float, 0x48u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Color", pfx_prop_color, 0x4Cu, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
    { "Width", pfx_prop_float, 0x50u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Linear Velocity", pfx_prop_vec3, 0x54u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Spawn Distance", pfx_prop_float, 0x64u, { 0x43FA0000u, 0x0u, 0x0u }, nullptr },
    { "Texture Asset", pfx_prop_texture, 0x68u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Tex Coord Rate", pfx_prop_float, 0x80u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Blend Mode", pfx_prop_choice, 0x84u, { 0x0u, 0x0u, 0x0u }, g_pfx_trail_blend },
    { "Water Interaction", pfx_prop_choice, 0x88u, { 0x0u, 0x0u, 0x0u }, g_pfx_trail_water },
};
// TickLinearAcceleration (node vtable 0x75021C, 0x44 bytes; ctor 0x58E5E0)
inline const pfx_prop_t g_pfx_props_TickLinearAcceleration[] = {
    { "Accel X", pfx_prop_float, 0x30u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Accel Y", pfx_prop_float, 0x34u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Accel Z", pfx_prop_float, 0x38u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Start Delay", pfx_prop_float, 0x40u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// TickDampenVelocity (node vtable 0x75018C, 0x38 bytes; ctor 0x58E360)
inline const pfx_prop_t g_pfx_props_TickDampenVelocity[] = {
    { "Amount", pfx_prop_float, 0x30u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Start Delay", pfx_prop_float, 0x34u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// SpringConstraint (node vtable 0x750104, 0x4C bytes; ctor 0x58DEB0)
inline const pfx_prop_t g_pfx_props_SpringConstraint[] = {
    { "Spring Coeff", pfx_prop_float, 0x30u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Damping Coeff", pfx_prop_float, 0x34u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Start Delay", pfx_prop_float, 0x38u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Target X", pfx_prop_float, 0x3Cu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Target Y", pfx_prop_float, 0x40u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Target Z", pfx_prop_float, 0x44u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// EmitBoatHull (node vtable 0x746B6C, 0x74 bytes; ctor 0x4CDF60)
inline const pfx_prop_t g_pfx_props_EmitBoatHull[] = {
    { "Min Lifespan", pfx_prop_float, 0x30u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Lifespan", pfx_prop_float, 0x34u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Scale", pfx_prop_float, 0x38u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Scale", pfx_prop_float, 0x3Cu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Rotation", pfx_prop_angle, 0x40u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Rotation", pfx_prop_angle, 0x44u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Angular Velocity", pfx_prop_angle, 0x48u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Angular Velocity", pfx_prop_angle, 0x4Cu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Spawn Boat Speed", pfx_prop_float, 0x50u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Spawn Boat Speed", pfx_prop_float, 0x54u, { 0x42200000u, 0x0u, 0x0u }, nullptr },
    { "Max Spawn Per Second", pfx_prop_float, 0x58u, { 0x42C80000u, 0x0u, 0x0u }, nullptr },
    { "Diffuse Color", pfx_prop_color, 0x5Cu, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
    { "Lateral Velocity Multiplier", pfx_prop_float, 0x60u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Vertical Velocity Multiplier", pfx_prop_float, 0x64u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Directional Stretch", pfx_prop_float, 0x68u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Directional Stretch", pfx_prop_float, 0x6Cu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Spawn Distance", pfx_prop_float, 0x70u, { 0x43480000u, 0x0u, 0x0u }, nullptr },
};
// EmitBoatThrust (node vtable 0x746AB0, 0xCC bytes; ctor 0x4CD4D0)
inline const pfx_prop_t g_pfx_props_EmitBoatThrust[] = {
    { "Spawn Per Second", pfx_prop_float, 0x30u, { 0x41200000u, 0x0u, 0x0u }, nullptr },
    { "Max Spawn Count", pfx_prop_int, 0x34u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Lifespan", pfx_prop_float, 0x38u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Lifespan", pfx_prop_float, 0x3Cu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Color", pfx_prop_color, 0x40u, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
    { "Max Color", pfx_prop_color, 0x44u, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
    { "Min Scale", pfx_prop_float, 0x48u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max Scale", pfx_prop_float, 0x4Cu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Position", pfx_prop_vec3, 0x70u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Position", pfx_prop_vec3, 0x80u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Linear Velocity", pfx_prop_vec3, 0x50u, { 0x0u, 0x0u, 0x40A00000u }, nullptr },
    { "Max Linear Velocity", pfx_prop_vec3, 0x60u, { 0x0u, 0x0u, 0x40A00000u }, nullptr },
    { "Spawn At Water Surface", pfx_prop_flag, 0x90u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Spawn Distance", pfx_prop_float, 0x94u, { 0x43FA0000u, 0x0u, 0x0u }, nullptr },
    { "Min Rotation", pfx_prop_angle, 0x98u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Rotation", pfx_prop_angle, 0x9Cu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Angular Velocity", pfx_prop_angle, 0xA0u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Angular Velocity", pfx_prop_angle, 0xA4u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min World Scale Z", pfx_prop_float, 0xA8u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Max World Scale Z", pfx_prop_float, 0xACu, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Min Directional Stretch", pfx_prop_float, 0xB0u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Directional Stretch", pfx_prop_float, 0xB4u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Tile Offset U", pfx_prop_float, 0xB8u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Tile Offset U", pfx_prop_float, 0xBCu, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Min Tile Offset V", pfx_prop_float, 0xC0u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Max Tile Offset V", pfx_prop_float, 0xC4u, { 0x0u, 0x0u, 0x0u }, nullptr },
    { "Test Drive", pfx_prop_int, 0xC8u, { 0xFFFFFFFFu, 0x0u, 0x0u }, nullptr },
};
// BoatSpring (node vtable 0x746A8C, 0x3C bytes; ctor 0x4CD6D0)
inline const pfx_prop_t g_pfx_props_BoatSpring[] = {
    { "Spring Coeff", pfx_prop_float, 0x30u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Damping Coeff", pfx_prop_float, 0x34u, { 0x3F800000u, 0x0u, 0x0u }, nullptr },
    { "Start Delay", pfx_prop_float, 0x38u, { 0x0u, 0x0u, 0x0u }, nullptr },
};
// System (node vtable 0x74FFD4, 0x34 bytes; ctor 0x58A710)
inline const pfx_prop_t g_pfx_props_System[] = {
    { "Duration", pfx_prop_float, 0x30u, { 0x0u, 0x0u, 0x0u }, nullptr },
};

// One node class: its registry type name, node vtable, size, create, the
// properties, and the words its ctor writes besides them (the geom pattern's
// VuStaticModelInstance: +0x5C vtable 0x74CDAC, +0x70 1.0, +0x74 -1).
struct pfx_node_type_t
{
    const char* type;
    const char* pattern;   // the registry entry: "" generic process, else its pattern; null: a pattern or system / group
    std::uint32_t vtable;
    std::uint32_t size;
    std::uint32_t create;
    const pfx_prop_t* props;
    int prop_count;
};

#define PFX_PROPS( n ) g_pfx_props_##n, static_cast< int >( sizeof( g_pfx_props_##n ) / sizeof( g_pfx_props_##n[ 0 ] ) )
inline const pfx_node_type_t g_pfx_node_types[] = {
    { "VuPfxSystem", nullptr, 0x74FFD4u, 0x34u, 0x58A710u, PFX_PROPS( System ) },
    { "VuPfxGroup", nullptr, 0x74CD14u, 0x30u, 0x51D7E0u, nullptr, 0 },
    // 0x51C840's patterns (0x564B70) and processes (0x564CB0), in its order
    { "VuPfxQuadPattern", nullptr, 0x7505ACu, 0xA0u, 0x5913D0u, PFX_PROPS( Quad ) },
    { "VuPfxEmitQuadFountain", "VuPfxQuadPattern", 0x74EF2Cu, 0xC8u, 0x55DA10u, PFX_PROPS( EmitQuadFountain ) },
    { "VuPfxEmitDirectionalQuadFountain", "VuPfxQuadPattern", 0x74EEC4u, 0xC8u, 0x55E8E0u, PFX_PROPS( EmitDirectional ) },
    { "VuPfxTickAlpha", "VuPfxQuadPattern", 0x750534u, 0x38u, 0x591170u, PFX_PROPS( TickAlpha ) },
    { "VuPfxTickAlphaInOut", "VuPfxQuadPattern", 0x7504C8u, 0x40u, 0x590E10u, PFX_PROPS( TickAlphaInOut ) },
    { "VuPfxTickScale", "VuPfxQuadPattern", 0x750448u, 0x38u, 0x590BB0u, PFX_PROPS( TickScale ) },
    { "VuPfxTickWorldScaleZ", "VuPfxQuadPattern", 0x7503C4u, 0x38u, 0x590950u, PFX_PROPS( TickWorldScaleZ ) },
    { "VuPfxGeomPattern", nullptr, 0x750328u, 0x7Cu, 0x58FA00u, PFX_PROPS( Geom ) },
    { "VuPfxEmitGeomFountain", "VuPfxGeomPattern", 0x74EEF8u, 0xD8u, 0x55E250u, PFX_PROPS( EmitGeomFountain ) },
    { "VuPfxTickAlpha", "VuPfxGeomPattern", 0x750534u, 0x38u, 0x591170u, PFX_PROPS( TickAlpha ) },
    { "VuPfxTickAlphaInOut", "VuPfxGeomPattern", 0x7504C8u, 0x40u, 0x590E10u, PFX_PROPS( TickAlphaInOut ) },
    { "VuPfxTickScale", "VuPfxGeomPattern", 0x750448u, 0x38u, 0x590BB0u, PFX_PROPS( TickScale ) },
    { "VuPfxTrailPattern", nullptr, 0x750294u, 0x90u, 0x58E900u, PFX_PROPS( Trail ) },
    { "VuPfxTickLinearAcceleration", "", 0x75021Cu, 0x44u, 0x58E540u, PFX_PROPS( TickLinearAcceleration ) },
    { "VuPfxTickDampenVelocity", "", 0x75018Cu, 0x38u, 0x58E2B0u, PFX_PROPS( TickDampenVelocity ) },
    { "VuPfxSpringConstraint", "", 0x750104u, 0x4Cu, 0x58DDF0u, PFX_PROPS( SpringConstraint ) },
    // the game's (0x4D8F79..)
    { "VuPfxEmitBoatHull", "VuPfxQuadPattern", 0x746B6Cu, 0x74u, 0x4CDEA0u, PFX_PROPS( EmitBoatHull ) },
    { "VuPfxEmitBoatThrustQuadFountain", "VuPfxQuadPattern", 0x746AB0u, 0xCCu, 0x4CD400u, PFX_PROPS( EmitBoatThrust ) },
    { "VuPfxBoatSpringConstraint", "", 0x746A8Cu, 0x3Cu, 0x4CD620u, PFX_PROPS( BoatSpring ) },
};
#undef PFX_PROPS

inline const pfx_node_type_t* pfx_node_type_of( std::uint32_t vtable )
{
    for ( const pfx_node_type_t& t : g_pfx_node_types )
        if ( t.vtable == vtable )
            return &t;
    return nullptr;
}

// The registry lookups: a pattern by Type (0x563EE0's map +0), a process by
// 0x564DC0 (generic first, then the parent's pattern's).
inline const pfx_node_type_t* pfx_pattern_type( const std::string& type )
{
    for ( const pfx_node_type_t& t : g_pfx_node_types )
        if ( t.pattern == nullptr && t.vtable != 0x74FFD4u && t.vtable != 0x74CD14u && type == t.type )
            return &t;
    return nullptr;
}

inline const pfx_node_type_t* pfx_process_type( const std::string& parent, const std::string& type )
{
    for ( const pfx_node_type_t& t : g_pfx_node_types )
        if ( t.pattern != nullptr && t.pattern[ 0 ] == '\0' && type == t.type )
            return &t;
    for ( const pfx_node_type_t& t : g_pfx_node_types )
        if ( t.pattern != nullptr && t.pattern[ 0 ] != '\0' && parent == t.pattern && type == t.type )
            return &t;
    return nullptr;
}

// What a load asks of its caller: the texture / model loads (the asset
// factory's - platform).
struct pfx_load_hooks_t
{
    void* user = nullptr;
    void ( *texture )( void* user, std::uint32_t node, std::uint32_t offset, const std::string& name ) = nullptr;
    void ( *model )( void* user, std::uint32_t node, const std::string& name ) = nullptr;
};

// The loaded library: VuPfxImpl's projects map (+0x14) by name, every node's
// name and children by name, the std::string members.
struct pfx_library_t
{
    std::map< std::string, std::uint32_t > projects;
    std::map< std::uint32_t, std::string > names;
    std::map< std::uint32_t, std::map< std::string, std::uint32_t > > kids;
    std::map< std::pair< std::uint32_t, std::uint32_t >, std::string > strings;
};

// A node of `t` as its create leaves it (+4 = 1, the ctor words, every
// property's default).
inline std::uint32_t pfx_node_create( pfx_runtime_t& rt, pfx_library_t& lib, const pfx_node_type_t& t )
{
    const std::uint32_t a = pfx_runtime_node( rt, t.vtable, { { 4u, 1u } }, {} );
    if ( t.vtable == 0x750328u )
    {
        rt.heap[ a + 0x5C ] = 0x74CDACu;
        rt.heap[ a + 0x70 ] = 0x3F800000u;
        rt.heap[ a + 0x74 ] = 0xFFFFFFFFu;
    }
    for ( int k = 0; k < t.prop_count; ++k )
    {
        const pfx_prop_t& p = t.props[ k ];
        if ( p.kind == pfx_prop_name || p.kind == pfx_prop_texture )
            lib.strings[ { a, p.offset } ] = std::string( );
        else if ( p.kind == pfx_prop_flag )
            rt.heap[ a + p.offset ] = ( rt.heap[ a + p.offset ] & 0xFFFFFF00u ) | p.fallback[ 0 ];
        else
            for ( int l = 0; l < ( p.kind == pfx_prop_vec3 || p.kind == pfx_prop_rot3 ? 3 : 1 ); ++l )
                rt.heap[ a + p.offset + 4u * l ] = p.fallback[ l ];
    }
    lib.names[ a ] = std::string( );
    lib.kids[ a ];
    return a;
}

// Every property's load (vt+8) on `properties`, in list order.
inline void pfx_node_properties( pfx_runtime_t& rt, pfx_library_t& lib, std::uint32_t a, const pfx_node_type_t& t,
    const value_t& properties, const pfx_load_hooks_t& k )
{
    for ( int i = 0; i < t.prop_count; ++i )
    {
        const pfx_prop_t& p = t.props[ i ];
        std::uint32_t* w = rt.heap.find( a + p.offset );
        float f, d;
        std::memcpy( &d, &p.fallback[ 0 ], 4 );
        switch ( p.kind )
        {
        case pfx_prop_float:
        case pfx_prop_angle:
            property_float( properties, p.name, f, d, p.kind == pfx_prop_angle );
            std::memcpy( w, &f, 4 );
            break;
        case pfx_prop_percent:
            property_percentage( properties, p.name, f, d );
            std::memcpy( w, &f, 4 );
            break;
        case pfx_prop_int:
        {
            std::int32_t v;
            property_int( properties, p.name, v, static_cast< std::int32_t >( p.fallback[ 0 ] ) );
            *w = static_cast< std::uint32_t >( v );
            break;
        }
        case pfx_prop_choice:
        {
            std::int32_t v;
            property_enum( properties, p.name, v, static_cast< std::int32_t >( p.fallback[ 0 ] ), p.choices->choices, p.choices->count );
            *w = static_cast< std::uint32_t >( v );
            break;
        }
        case pfx_prop_flag:
        {
            std::uint8_t v;
            property_bool( properties, p.name, v, static_cast< std::uint8_t >( p.fallback[ 0 ] ) );
            *w = ( *w & 0xFFFFFF00u ) | v;
            break;
        }
        case pfx_prop_vec3:
        case pfx_prop_rot3:
        {
            float v[ 3 ], fb[ 3 ];
            std::memcpy( fb, p.fallback, 12 );
            if ( p.kind == pfx_prop_vec3 )
                property_vector3( properties, p.name, v, fb );
            else
                property_rotation3( properties, p.name, v, fb );
            std::memcpy( w, v, 4 );
            std::memcpy( rt.heap.find( a + p.offset + 4 ), v + 1, 4 );
            std::memcpy( rt.heap.find( a + p.offset + 8 ), v + 2, 4 );
            break;
        }
        case pfx_prop_color:
            property_color( properties, p.name, *w, p.fallback[ 0 ] );
            break;
        case pfx_prop_name:
        case pfx_prop_texture:
        {
            std::string& s = lib.strings[ { a, p.offset } ];
            const std::string before = std::string( );   // the load's reset to the default ""
            property_string( properties, p.name, s, before );
            if ( p.kind == pfx_prop_texture && s != before && k.texture != nullptr )
                k.texture( k.user, a, p.offset, s );
            break;
        }
        }
    }
}

inline void pfx_node_load( pfx_runtime_t& rt, pfx_library_t& lib, std::uint32_t a, const value_t& doc, const pfx_load_hooks_t& k );

// 0x563EE0 on doc["ChildNodes"] of node `a`.
inline void pfx_child_nodes( pfx_runtime_t& rt, pfx_library_t& lib, std::uint32_t a, const value_t& children, const pfx_load_hooks_t& k )
{
    if ( children.tag != 6 )
        return;   // 0x4FBE90 counts only a map's members
    std::map< std::string, const value_t* > order;
    for ( const auto& field : children.object )
        order.emplace( field.first, &field.second );
    const pfx_class_t* parent = pfx_node_class( rt.heap, a );
    for ( const auto& c : order )
    {
        const value_t& doc = *c.second;
        const std::string& base = member( doc, "BaseType" ).tag == 4 ? member( doc, "BaseType" ).text : std::string( );
        const std::string& type = member( doc, "Type" ).tag == 4 ? member( doc, "Type" ).text : std::string( );
        const pfx_node_type_t* t = nullptr;
        if ( base == "group" )
            t = pfx_node_type_of( 0x74CD14u );
        else if ( base == "system" )
            t = pfx_node_type_of( 0x74FFD4u );
        else if ( base == "pattern" )
            t = pfx_pattern_type( type );
        else if ( base == "process" )
            t = pfx_process_type( parent != nullptr ? parent->name : "", type );
        if ( t == nullptr )
            continue;
        const std::uint32_t n = pfx_node_create( rt, lib, *t );
        lib.names[ n ] = c.first;
        pfx_node_load( rt, lib, n, doc, k );
        lib.kids[ a ][ c.first ] = n;
        rt.heap[ a + 0x10 ] = static_cast< std::uint32_t >( lib.kids[ a ].size( ) );
        std::vector< std::uint32_t >& list = rt.children[ a ];
        list.clear( );
        for ( const auto& e : lib.kids[ a ] )
            list.push_back( e.second );
    }
}

// VuPfxNode slot 2 0x563D40, then slot 4.
inline void pfx_node_load( pfx_runtime_t& rt, pfx_library_t& lib, std::uint32_t a, const value_t& doc, const pfx_load_hooks_t& k )
{
    const pfx_node_type_t* t = pfx_node_type_of( rt.heap[ a ] );
    if ( t == nullptr )
        return;
    pfx_node_properties( rt, lib, a, *t, member( doc, "Properties" ), k );
    pfx_child_nodes( rt, lib, a, member( doc, "ChildNodes" ), k );
    if ( t->vtable == 0x750328u )
    {
        if ( k.model != nullptr )
            k.model( k.user, a, lib.strings[ { a, 0x3Cu } ] );
        rt.heap[ a + 0x70 ] = rt.heap[ a + 0x54 ];
    }
}

// addProject 0x51D6C0: 0 when the name is taken, else the project's group.
inline std::uint32_t pfx_add_project( pfx_runtime_t& rt, pfx_library_t& lib, const std::string& name, const value_t& doc,
    const pfx_load_hooks_t& k = pfx_load_hooks_t( ) )
{
    if ( lib.projects.count( name ) != 0 )
        return 0u;
    const std::uint32_t g = pfx_node_create( rt, lib, *pfx_node_type_of( 0x74CD14u ) );
    pfx_node_load( rt, lib, g, doc, k );
    lib.projects[ name ] = g;
    return g;
}

// VuPfxAsset's load slot 5 0x5CAB30 on the asset's bytes (c_archive::read's
// blob): the u32 size, the VUJB document (c_reader = 0x55B560), addProject.
inline std::uint32_t pfx_load_asset( pfx_runtime_t& rt, pfx_library_t& lib, const std::string& name,
    const std::vector< std::uint8_t >& blob, const pfx_load_hooks_t& k = pfx_load_hooks_t( ) )
{
    if ( blob.size( ) < 4 )
        return 0u;
    std::uint32_t size;
    std::memcpy( &size, blob.data( ), 4 );
    if ( size > blob.size( ) - 4 )
        return 0u;
    const std::vector< std::uint8_t > doc( blob.begin( ) + 4, blob.begin( ) + 4 + size );
    c_reader reader( doc );
    const value_t root = reader.parse( );
    if ( root.tag == 0 && size != 0 )
        return 0u;   // the read failed (0x5C1270 false): no project
    return pfx_add_project( rt, lib, name, root, k );
}

// The path lookup 0x51DBD0: strcpy_s into 0x100 bytes, strtok_s on "/" (empty
// tokens skipped); the first token in the projects map, each next one in the
// node's children; 0 on a miss or with no token. A path of 0x100 bytes or more
// is strcpy_s' invalid-parameter path in retail: UNKNOWN here (0).
inline std::uint32_t pfx_find( const pfx_library_t& lib, const std::string& path )
{
    if ( path.size( ) >= 0x100u )
        return 0u;
    std::vector< std::string > tokens;
    std::string cur;
    for ( char ch : path.substr( 0, path.find( '\0' ) ) )
    {
        if ( ch == '/' )
        {
            if ( !cur.empty( ) )
                tokens.push_back( cur );
            cur.clear( );
        }
        else
            cur += ch;
    }
    if ( !cur.empty( ) )
        tokens.push_back( cur );
    if ( tokens.empty( ) )
        return 0u;
    auto p = lib.projects.find( tokens[ 0 ] );
    if ( p == lib.projects.end( ) )
        return 0u;
    std::uint32_t n = p->second;
    for ( std::size_t i = 1; i < tokens.size( ); ++i )
    {
        auto kids = lib.kids.find( n );
        if ( kids == lib.kids.end( ) )
            return 0u;
        auto c = kids->second.find( tokens[ i ] );
        if ( c == kids->second.end( ) )
            return 0u;
        n = c->second;
    }
    return n;
}

// createSystemInstance 0x51DA70 by path: the lookup, then pfxsim.hxx's create.
inline std::uint32_t pfx_runtime_create_path( pfx_runtime_t& rt, const pfx_library_t& lib, const std::string& path )
{
    return pfx_system_instance_create( rt.heap, rt.resources, pfx_find( lib, path ), rt.hooks );
}