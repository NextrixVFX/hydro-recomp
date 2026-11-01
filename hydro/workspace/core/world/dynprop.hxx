#pragma once

#include <workspace/core/boat/hull.hxx>
#include <workspace/core/entity/node.hxx>
#include <workspace/core/entity/property.hxx>
#include <workspace/core/entity/rigid.hxx>
#include <workspace/core/entity/values.hxx>
#include <workspace/core/game/types.hxx>
#include <workspace/core/world/bodies.hxx>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <xmmintrin.h>

// The level's dynamic game props (world batch 23): VuDynamicGamePropEntity (create
// 0x436740, ctor 0x4367E0) and VuCheckpointBuoyEntity (create 0x434EE0, ctor
// 0x434F80 over 0x4367E0), both over VuDynamicPropEntity's ctor 0x545360. Their
// property sets, the postLoad's component step (0x5459F0), the motion state
// (vt 0x73A260 / 0x73A580 slot 1 0x545C00) and gameInitialize (vt slot 6
// 0x437580 -> VuDynamicPropEntity's 0x545A80 -> createRigidBody 0x544100 ->
// the show vt+0x30 0x4381B0 -> 0x545EC0), checked against retail by the case
// dynamic_prop_bodies. VuBreakableDynamicGamePropEntity (its own ctor, not in the
// captured schema) and VuFloatingMineEntity (slot 6 0x438CF0) are NOT
// recreated: UNKNOWN.

// VuDynamicPropEntity's three nodes (ctor 0x545360, in its +0x44 order):
//   "Initially Visible" +0x74 bool (vt 0x737450)  default 1
//   "Mass"              +0x78 float (vt 0x737484) default 100 (0x42C80000)
//   "Center of Mass"    +0x7C vector3 (vt 0x737A84) default 0 (the w lane +0x88
//                       is never written by the ctor - UNKNOWN, not modelled)
// (the Mass / Center of Mass watcher 0x5460E0 has notify 0: a load never fires
// it; postLoad does its work instead.) Then VuDynamicGamePropEntity's nodes
// (0x4367E0; the schema's captured rows, offsets +0xB8 .. +0x142): floats vt
// 0x737484, vector3 0x737A84, bools 0x737450, "Splash Pfx Name" +0xD8 a
// VuStringProperty (0x406B70, vt 0x7374B8) and "Sfx Name" +0x110 (vt 0x73A18C) -
// both load through 0x406C20, property_string's contract. The buoy adds
// "CheckPoint Index" +0x184 (VuBasicProperty<int> vt 0x73A218, default 0).
struct dynamic_prop_t
{
    std::uint8_t visible = 1;            // +0x74
    float mass = 100.0f;                 // +0x78
    float centre[ 3 ] = { 0, 0, 0 };     // +0x7C
    float fluids_radius = 0.0f;          // +0xB8
    float fluids_density = 0.5f;         // +0xBC
    float fluids_drag = 0.0f;            // +0xC0 (0x3EF0A3D7)
    float fluids_lift = 0.0f;            // +0xC4 (0x3E4CCCCD)
    float hydro_centre[ 3 ] = { 0, 0, 0 };   // +0xC8
    std::string splash_pfx;              // +0xD8
    float splash_speed = 10.0f;          // +0xF0
    float linear_damping = 0.5f;         // +0xF4
    float angular_damping = 0.5f;        // +0xF8
    std::uint8_t linear_anchor = 0;      // +0xFC
    std::uint8_t angular_anchor = 0;     // +0xFD
    float anchor_spring = 1.0f;          // +0x100
    float anchor_damping = 1.0f;         // +0x104
    std::uint8_t hard_hit = 0;           // +0x108
    float threshold_mph = 25.0f;         // +0x10C
    std::string sfx;                     // +0x110
    float shake_magnitude = 0.0f, shake_duration = 0.0f, shake_falloff = 0.0f, shake_frequency = 0.0f;   // +0x128 .. +0x134
    float rumble_low = 0.0f, rumble_high = 0.0f;   // +0x138 / +0x13C
    std::uint8_t throw_ai = 0;           // +0x140
    std::uint8_t initially_active = 1;   // +0x141
    std::uint8_t collide_static = 1;     // +0x142
    std::int32_t checkpoint_index = 0;   // +0x184 (the buoy only)
};

inline void dynamic_prop_construct( dynamic_prop_t& p )
{
    p = dynamic_prop_t{ };
    p.fluids_drag = support_bits( 0x3EF0A3D7u );
    p.fluids_lift = support_bits( 0x3E4CCCCDu );
}

struct dynamic_prop_state_t
{
    dynamic_prop_t value;
    dynamic_prop_t defaults;
    bool buoy = false;
};

inline bool dynamic_prop_class( const std::string& cls )
{
    return cls == "VuDynamicGamePropEntity" || cls == "VuCheckpointBuoyEntity";
}

inline std::shared_ptr< void > dynamic_prop_state_for( const std::string& cls )
{
    if ( !dynamic_prop_class( cls ) )
        return nullptr;
    auto s = std::make_shared< dynamic_prop_state_t >( );
    dynamic_prop_construct( s->value );
    dynamic_prop_construct( s->defaults );
    s->buoy = cls == "VuCheckpointBuoyEntity";
    return s;
}

// The loads in node order, each reset to its default first.
inline void dynamic_prop_load_properties( entity_t& e, const value_t& p )
{
    if ( !dynamic_prop_class( e.m_type ) || !e.m_state )
        return;
    dynamic_prop_state_t& s = *static_cast< dynamic_prop_state_t* >( e.m_state.get( ) );
    dynamic_prop_t& v = s.value;
    const dynamic_prop_t& d = s.defaults;
    property_bool( p, "Initially Visible", v.visible, d.visible );
    property_float( p, "Mass", v.mass, d.mass, false );
    property_vector3( p, "Center of Mass", v.centre, d.centre );
    property_float( p, "Fluids Radius", v.fluids_radius, d.fluids_radius, false );
    property_float( p, "Fluids Density", v.fluids_density, d.fluids_density, false );
    property_float( p, "Fluids Drag Coeff", v.fluids_drag, d.fluids_drag, false );
    property_float( p, "Fluids Lift Coeff", v.fluids_lift, d.fluids_lift, false );
    property_vector3( p, "Hydrodynamic Center", v.hydro_centre, d.hydro_centre );
    property_string( p, "Splash Pfx Name", v.splash_pfx, d.splash_pfx );
    property_float( p, "Splash Speed", v.splash_speed, d.splash_speed, false );
    property_float( p, "Linear Damping Coeff", v.linear_damping, d.linear_damping, false );
    property_float( p, "Angular Damping Coeff", v.angular_damping, d.angular_damping, false );
    property_bool( p, "Linear Anchor", v.linear_anchor, d.linear_anchor );
    property_bool( p, "Angular Anchor", v.angular_anchor, d.angular_anchor );
    property_float( p, "Anchor Spring Coeff", v.anchor_spring, d.anchor_spring, false );
    property_float( p, "Anchor Damping Coeff", v.anchor_damping, d.anchor_damping, false );
    property_bool( p, "Hard Hit", v.hard_hit, d.hard_hit );
    property_float( p, "Threshold Speed (MPH)", v.threshold_mph, d.threshold_mph, false );
    property_string( p, "Sfx Name", v.sfx, d.sfx );
    property_float( p, "Cam Shake Magnitude", v.shake_magnitude, d.shake_magnitude, false );
    property_float( p, "Cam Shake Duration", v.shake_duration, d.shake_duration, false );
    property_float( p, "Cam Shake Falloff Time", v.shake_falloff, d.shake_falloff, false );
    property_float( p, "Cam Shake Frequency", v.shake_frequency, d.shake_frequency, false );
    property_float( p, "Rumble Low Freq", v.rumble_low, d.rumble_low, false );
    property_float( p, "Rumble High Freq", v.rumble_high, d.rumble_high, false );
    property_bool( p, "Throw Ai Driver", v.throw_ai, d.throw_ai );
    property_bool( p, "Initially Active", v.initially_active, d.initially_active );
    property_bool( p, "Collide With Static Props", v.collide_static, d.collide_static );
    if ( s.buoy )
        property_int( p, "CheckPoint Index", v.checkpoint_index, d.checkpoint_index );
}

// The template mark's property step (each node's vt+0x1C): default = value.
inline void dynamic_prop_mark_properties( entity_t& e )
{
    if ( dynamic_prop_class( e.m_type ) && e.m_state )
    {
        dynamic_prop_state_t& s = *static_cast< dynamic_prop_state_t* >( e.m_state.get( ) );
        s.defaults = s.value;
    }
}

// 0x544050 (thiscall component; xmm1 the mass): +0xEC = maxss(mass, 0) (a NaN
// mass gives 0 - maxss answers the second operand), +0xD0 bit 0 = that == 0
// (ucomiss / lahf / 0x44: equal only), and with a body (+0x100 - none before
// gameInitialize) the inertia / setMassProps 0x50E040 - not reached here.
inline bool dynamic_prop_set_mass( rigid_component_t& c, float mass, bool has_body )
{
    const __m128 m = _mm_max_ss( _mm_set_ss( mass ), _mm_setzero_ps( ) );
    c.mass = _mm_cvtss_f32( m );
    if ( _mm_ucomieq_ss( m, _mm_setzero_ps( ) ) )
        c.flags_d0 |= 1u;
    else
        c.flags_d0 &= ~1u;
    return !has_body;   // a body's re-mass is not recreated
}

// VuDynamicPropEntity's postLoad 0x5459F0, its component step: 0x544050(Mass),
// the centre of mass (+0x7C, 16 bytes) into the component's +0xDC (its w lane
// UNKNOWN - not modelled), 0x545010 only with a body (none at load). (Its first
// call 0x545FF0 is the draw matrix - world/models.hxx's transform_draw_matrix
// path - and its +0x90 .. +0xAC copies of the transform's +0x40 / +0x90 words
// are read by nothing recreated.)
inline void dynamic_prop_post_load( const dynamic_prop_t& p, rigid_component_t& c )
{
    dynamic_prop_set_mass( c, p.mass, false );
    std::memcpy( c.centre, p.centre, 12 );
}

// The motion state's getWorldTransform 0x545C00 (this = entity +0x5C; retn 4):
// the transform component's world (+0x50 of the entity's +0x50) under
// 0x410AB0's translation by the component's centre of mass (+0xDC) -
// VuMatrix_multiply 0x4078C0(translation; out, world) - converted by 0x410C30
// to a btTransform (the basis transposed into rows, w 0, then the origin).
struct dynamic_prop_motion_t
{
    const float* world = nullptr;     // transform +0x50
    const float* centre = nullptr;    // component +0xDC
};

inline bool dynamic_prop_motion_get( void* user, std::uint32_t, float* out )
{
    const dynamic_prop_motion_t& m = *static_cast< const dynamic_prop_motion_t* >( user );
    float offset[ 16 ], product[ 16 ];
    vu_identity( offset );
    std::memcpy( offset + 12, m.centre, 12 );
    offset[ 15 ] = 1.0f;
    hull_multiply( offset, product, m.world );
    bodies_bt_from_vu( product, out );
    return true;
}

// What gameInitialize registers / hands out, for the caller to act on.
struct dynamic_prop_init_t
{
    bool decision_tick = false;   // g 0x759008 vt+0x20 (0x4077E8 -> vt+0x40 slot 16 0x438260), "Decision" (0x685E74)
    bool build_tick = false;      // the same with 0x4077DE (-> vt+0x3C slot 15 0x546130), "Build" (0x6CEC6C)
    bool contact_callback = false;   // body +0x2C4 = entity +0xB4 (vt 0x73A24C / 0x73A56C slot 0)
    bool step_callback = false;   // g_vudynamicsimpl vt+0x2C(entity +0xB0: slot 1 0x437730)
    bool drawn = false;           // g 0x7590B8 vt+0x28(draw component +0x6C)
    bool added = false;           // g_vudynamicsimpl vt+0x3C addRigidBody
    std::uint8_t shown = 0;       // entity +0x8C
};

// VuDynamicGamePropEntity gameInitialize 0x437580 on the entity's state, its
// rigid component and transform:
//  - Hard Hit (+0x108) or a Splash Pfx Name (+0xD8's size +0xE8 != 0): component
//    +0xF4 = entity +0xB4 (the contact callback; the body's +0x2C4 when one
//    exists - none yet), +0xD8 |= 1, the "Decision" tick.
//  - 0x545A80: 0x544050's mass step (inlined; no body), +0xF0 = entity +0x5C
//    (the motion state), group 2 (+0xD4) / mask 0xFFFF (+0xD6), createRigidBody,
//    Initially Visible -> the show vt+0x30 0x4381B0 (0x545EC0: +0x8C = 1, the
//    draw add unless the draw component's +0x1E, addRigidBody unless +0x104 or no
//    body; then Fluids Radius > 0 (comiss) or an anchor -> the step callback;
//    +0x144 = the world's translation and 0x531000 on +0x154 - read by the
//    Decision tick, not modelled here), then the "Build" tick.
//  - Initially Active (+0x141) 0 -> body +0xE4 (activation) = 2; Collide With
//    Static Props (+0x142) 0 -> mask &= 0xFFFE (+0xD6, the body's +0x2CC; the
//    broadphase proxy's +6 when the body has one - the caller's world).
//  - body +0x1D0 / +0x1D4 = Linear / Angular Damping Coeff clamped to [0, 1]
//    (0x6F76B8; comiss / cmovbe as body_clamp01).
// The body is dereferenced unconditionally from there (retail has one by then).
inline bool dynamic_prop_game_initialize( level_body_t& b, rigid_component_t& c, const dynamic_prop_t& p, const transform_component_t& t,
                                          const collision_mesh_asset_t* asset, std::uint32_t& unique_id, const bodies_hooks_t& hooks,
                                          dynamic_prop_motion_t& motion, dynamic_prop_init_t& out, std::string& why )
{
    out = dynamic_prop_init_t{ };
    if ( p.hard_hit || !p.splash_pfx.empty( ) )
    {
        c.word_f4 = 1;   // = entity +0xB4: a pointer word, the marker 1 here
        c.flags_d8 |= 1u;
        out.contact_callback = true;
        out.decision_tick = true;
    }
    dynamic_prop_set_mass( c, p.mass, false );
    c.group = 2;
    c.mask = 0xFFFF;
    motion.world = t.world;
    motion.centre = c.centre;
    bodies_motion_t m;
    m.get = dynamic_prop_motion_get;
    m.user = &motion;
    bool built = false;
    if ( !bodies_create( b, c, asset, t.world, t.scale, unique_id, hooks, why, built, &m ) )
        return false;
    if ( !built )
    {
        why = "gameInitialize 0x437580: no body built - retail dereferences +0x100 regardless (UNKNOWN)";
        return false;
    }
    if ( p.visible )
    {
        out.shown = 1;
        out.drawn = true;
        if ( !b.in_world )
        {
            out.added = true;
            b.in_world = true;
        }
        if ( _mm_comigt_ss( _mm_set_ss( p.fluids_radius ), _mm_setzero_ps( ) ) || p.linear_anchor || p.angular_anchor )
            out.step_callback = true;
    }
    out.build_tick = true;
    if ( !p.initially_active )
        body_word( b.body, 0xE4 ) = 2;
    if ( !p.collide_static )
    {
        c.mask = static_cast< std::uint16_t >( c.mask & 0xFFFEu );
        std::memcpy( reinterpret_cast< std::uint8_t* >( &b.body ) + 0x2CC, &c.mask, 2 );
    }
    const float ld = body_clamp01( p.linear_damping ), ad = body_clamp01( p.angular_damping );
    std::memcpy( &body_word( b.body, 0x1D0 ), &ld, 4 );
    std::memcpy( &body_word( b.body, 0x1D4 ), &ad, 4 );
    return true;
}
