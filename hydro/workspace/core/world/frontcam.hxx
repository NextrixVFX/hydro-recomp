#pragma once

// VuFrontEndCameraEntity ("Objects" / "FrontEndCamera"; create 0x43A210: new
// 0x2A0, ctor 0x43A290; vtable 0x73A9E4) - THE FRONT END'S CAMERA: its
// transform places a VuCamera (+0x60, 0x240 bytes) and every frame the camera
// is copied into viewport 0 (g_vuviewportmanager 0x759060, +0x28).
//
// The ctor: VuEntity 0x500710( 0 ), the camera's base ctor 0x50BA20 (boat
// parts.hxx parts_50ba20), a Vu3dLayoutComponent 0x50B8D0 at +0x5C (its draw
// delegate +0x10 = 0x43A560, the layout DEBUG DRAW - VuGfxUtil 0x508E30: not
// recreated), and the transform's (+0x50) watcher delegate +0xC0 = VuMethod0
// (0x73A9D4) -> 0x43A4D0 with mask +0xC4 = 0x3F. Slots: 1 getType 0x43A1F0
// (descriptor 0x7363D8), 2 0x43A200 = 0x43A4D0 (after the load), 6 0x43A3D0 =
// the tick 0x43A430 registered with the tick manager (g 0x759008 vt+0x20, the
// phase string 0x687C14), 7 0x43A420 its unregister (vt+0x28) - the
// registration is the caller's (platform tick manager).
//
// WIRING (world agent): a front-end screen project that authors one keeps a
// front_camera_t per entity; construct it, front_camera_look( c, world ) after
// the load and whenever the entity's transform moves (the watcher), then once a
// frame front_camera_tick( c, env, viewport0 ) - its output IS what the menus
// render with. This entity's ctor registers no event handler (the
// "OnSetFrontEndCamera*" events VuSetFrontEndCameraEntity broadcasts are not
// handled here; who handles them is unread). env = boat/camera.hxx's
// camera_env_t (display size, VuDB "VuDBAsset/Camera/VerticalFOV").

#include <workspace/core/boat/camera.hxx>
#include <workspace/core/boat/events.hxx>
#include <workspace/core/boat/parts.hxx>

#include <cstdint>
#include <cstring>

struct front_camera_t
{
    std::uint8_t camera[ 0x240 ]{};   // the entity's +0x60
};

inline void front_camera_construct( front_camera_t& c )
{
    parts_50ba20( c.camera, 0 );
}

// The lookAt 0x43A4D0 (thiscall entity; the transform's watcher and slot 2):
// world = the transform's world matrix (+0x50 -> +0x50, 16 floats): eye =
// row 3 (16 bytes, w too), target = row 1 + eye (x y z, each `row + eye`;
// its w lane is a stack word nothing writes - UNKNOWN, written here as 0),
// up = row 2 (16 bytes); then the camera's 0x50BDA0 (camera_view).
inline void front_camera_look( front_camera_t& c, const float* world )
{
    float eye[ 4 ], up[ 4 ];
    std::memcpy( eye, world + 12, 16 );
    std::memcpy( up, world + 8, 16 );
    const float target[ 4 ] = { sse_add( world[ 4 ], eye[ 0 ] ), sse_add( world[ 5 ], eye[ 1 ] ), sse_add( world[ 6 ], eye[ 2 ] ), 0.0f };
    camera_view( c.camera, eye, target, up );
}

// The tick 0x43A430 (thiscall entity, dt unread; retn 4): aspect = (float) w
// / (float) h of the display (g 0x75907C vt+0x40: a platform hook, here
// env.display_w / _h), the vertical FOV 0x4D2C60 (camera_vertical_fov), the
// projection 0x50BBE0 with near 1.0 and far 500.0 (0x43FA0000), then the
// camera's 0x240 bytes into `viewport_camera` (viewport 0's +0x28).
inline void front_camera_tick( front_camera_t& c, const camera_env_t& env, std::uint8_t* viewport_camera )
{
    const float aspect = static_cast< float >( env.display_w ) / static_cast< float >( env.display_h );
    const float fov = camera_vertical_fov( env );
    camera_projection( c.camera, fov, aspect, 1.0f, 500.0f );
    if ( viewport_camera != nullptr )
        std::memcpy( viewport_camera, c.camera, sizeof( c.camera ) );
}
