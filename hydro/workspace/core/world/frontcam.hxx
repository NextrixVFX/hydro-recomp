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
// handled here - VuFrontEndGameMode's camera rig below does). env = boat/camera.hxx's
// camera_env_t (display size, VuDB "VuDBAsset/Camera/VerticalFOV").

#include <workspace/core/boat/camera.hxx>
#include <workspace/core/boat/events.hxx>
#include <workspace/core/boat/parts.hxx>
#include <workspace/core/world/wave.hxx>

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

// ---- RETAIL'S FRONT-END CAMERA: VuFrontEndGameMode's camera rig ----------------
// VuFrontEndGameMode (enter 0x4BDEB0) news a 0x2C0-byte rig at its +4 (ctor
// 0x4BE1C0), ticks it through 0x4BE1A0 (-> 0x4BE490 with dt) and deletes it
// at its exit 0x4BE050 (dtor 0x4BE110: g 0x758FB8 +0x2C0 = 0, the event map).
// The rig: +0 the event map (0x515800; the four handlers below registered by
// name - "OnSetFrontEndCameraDrawDistance" 0x4BE5D0, "...EyePosition"
// 0x4BE650, "...TargetPosition" 0x4BE710, "...Interpolation" 0x4BE7D0; the
// broadcasts VuSetFrontEndCameraEntity makes), +4 0, byte +8 0, byte +9 set
// once the first Interpolation placed the camera; +0xC THE VuCamera (0x240
// bytes, 0x50BA20) - the ctor publishes it at g 0x758FB8 +0x2C0, which is the
// camera the menus render with; +0x24C the desired eye (16 b), +0x25C the
// desired target, +0x26C the desired far plane; +0x270 / +0x294 the "from" /
// "to" poses (eye 16 b, target 16 b, far: 0x24 b each); +0x2B8 the duration,
// +0x2BC the elapsed time. The rig is modelled as its image (camera inside).
struct front_rig_t
{
    std::uint8_t w[ 0x2C0 ]{};
    std::uint8_t* camera( ) { return w + 0xC; }
    float f( int at ) const
    {
        float v;
        std::memcpy( &v, w + at, 4 );
        return v;
    }
    void put( int at, float v ) { std::memcpy( w + at, &v, 4 ); }
};

// The ctor 0x4BE1C0's camera half: the VuCamera 0x50BA20; desired eye 0, target
// (0, 1, 0), far 500; both poses the same (their +0xC / +0x1C w lanes and the
// from pose's 3 unwritten words stay 0 - retail's new 0x2C0 is NOT zeroed:
// UNKNOWN, never compared); duration / elapsed 0; then the display's aspect, the
// vertical FOV 0x4D2C60, the projection 0x50BBE0 ( fov, aspect, 1.0, 500 ) and
// the lookAt 0x50BDA0 ( eye (-1, -3, 2), target (0, 0, 0.5), up (0, 0, 1) -
// their w lanes are stack words nothing writes: UNKNOWN, here 0 ). The event
// registration (0x5156E0 x 4) and g 0x758FB8 +0x2C0 = the camera are the
// caller's (`rig.camera( )` is that pointer).
inline void front_rig_construct( front_rig_t& r, const camera_env_t& env )
{
    std::memset( r.w, 0, sizeof( r.w ) );
    parts_50ba20( r.camera( ), 0 );
    for ( int base : { 0x24C, 0x270, 0x294 } )
    {
        r.put( base + 0x14, 1.0f );
        r.put( base + 0x20, 500.0f );
    }
    const float aspect = static_cast< float >( env.display_w ) / static_cast< float >( env.display_h );
    const float fov = camera_vertical_fov( env );
    camera_projection( r.camera( ), fov, aspect, 1.0f, 500.0f );
    const float eye[ 4 ] = { -1.0f, -3.0f, 2.0f, 0.0f }, target[ 4 ] = { 0.0f, 0.0f, 0.5f, 0.0f }, up[ 4 ] = { 0.0f, 0.0f, 1.0f, 0.0f };
    camera_view( r.camera( ), eye, target, up );
}

// A VuParams image's first value (the handlers read only it): +0x80 the byte
// size (signed > 0), +0 its type word (2 a float, 6 a vector3 of 16 bytes), the
// value at +4; anything else leaves the default 0.
inline float front_rig_param_float( const std::uint8_t* params )
{
    std::int32_t size, type;
    std::memcpy( &size, params + 0x80, 4 );
    float v = 0.0f;
    if ( size > 0 )
    {
        std::memcpy( &type, params, 4 );
        if ( type == 2 )
            std::memcpy( &v, params + 4, 4 );
    }
    return v;
}

inline void front_rig_param_vector( const std::uint8_t* params, std::uint8_t* out16 )
{
    std::int32_t size, type;
    std::memcpy( &size, params + 0x80, 4 );
    std::memset( out16, 0, 16 );
    if ( size > 0 )
    {
        std::memcpy( &type, params, 4 );
        if ( type == 6 )
            std::memcpy( out16, params + 4, 16 );
    }
}

// The handlers (thiscall rig; const VuParams&; retn 4).
inline void front_rig_draw_distance( front_rig_t& r, const std::uint8_t* params )   // 0x4BE5D0: +0x26C
{
    r.put( 0x26C, front_rig_param_float( params ) );
}

inline void front_rig_eye( front_rig_t& r, const std::uint8_t* params )   // 0x4BE650: +0x24C (16 bytes)
{
    front_rig_param_vector( params, r.w + 0x24C );
}

inline void front_rig_target( front_rig_t& r, const std::uint8_t* params )   // 0x4BE710: +0x25C (16 bytes)
{
    front_rig_param_vector( params, r.w + 0x25C );
}

// The camera from a pose (eye at +0, target +0x10, far +0x20 of `pose`): the
// projection with the camera's own FOV / aspect / near (+0x170 / +0x174 /
// +0x178) and the pose's far, then the lookAt with up (0, 0, 1) (its w lane a
// stack word nothing writes: 0 here, camera_view reads x y z only).
inline void front_rig_place( front_rig_t& r, const std::uint8_t* pose )
{
    float eye[ 4 ], target[ 4 ], far_plane;
    std::memcpy( eye, pose, 16 );
    std::memcpy( target, pose + 0x10, 16 );
    std::memcpy( &far_plane, pose + 0x20, 4 );
    camera_projection( r.camera( ), r.f( 0x17C ), r.f( 0x180 ), r.f( 0x184 ), far_plane );
    const float up[ 4 ] = { 0.0f, 0.0f, 1.0f, 0.0f };
    camera_view( r.camera( ), eye, target, up );
}

// 0x4BE7D0 Interpolation: t = the float. The first time (byte +9 == 0): "to" =
// the desired pose, the camera placed there, +9 = 1. After: nothing when the
// desired eye / target x y z and far all equal "to" (ucomiss: a NaN differs);
// else "from" = the camera's eye / target (its +0 .. +0x1F) and far (+0x17C),
// "to" = the desired pose, duration = t, elapsed = 0, and when !( 0 < t ) and
// t is not NaN (comiss / jb) the camera is placed at "to" at once.
inline void front_rig_interpolation( front_rig_t& r, const std::uint8_t* params )
{
    const float t = front_rig_param_float( params );
    if ( r.w[ 9 ] == 0 )
    {
        std::memcpy( r.w + 0x294, r.w + 0x24C, 0x24 );
        front_rig_place( r, r.w + 0x24C );
        r.w[ 9 ] = 1;
        return;
    }
    const int lanes[ 7 ][ 2 ] = { { 0x24C, 0x294 }, { 0x250, 0x298 }, { 0x254, 0x29C }, { 0x25C, 0x2A4 }, { 0x260, 0x2A8 }, { 0x264, 0x2AC }, { 0x26C, 0x2B4 } };
    bool same = true;
    for ( const auto& l : lanes )
        same = same && r.f( l[ 0 ] ) == r.f( l[ 1 ] );
    if ( same )
        return;
    std::memcpy( r.w + 0x270, r.camera( ), 0x20 );
    std::memcpy( r.w + 0x290, r.w + 0x188, 4 );
    std::memcpy( r.w + 0x294, r.w + 0x24C, 0x24 );
    r.put( 0x2B8, t );
    r.put( 0x2BC, 0.0f );
    if ( _mm_comilt_ss( _mm_setzero_ps( ), _mm_set_ss( t ) ) || t != t )
        return;
    front_rig_place( r, r.w + 0x294 );
}

// 0x4BEC60 (thiscall pose; out quaternion; retn 4): f = normalise( target -
// eye ), r = normalise( f x (0, 0, 1) ) in retail's lanes ( fy - fz 0,
// fz 0 - fx, fx 0 - fy 0 ), u = normalise( r x f ); the rows (r, f, u) with
// identity w lanes and row 3 (0, 0, 0, 1) -> 0x531000 (boat/math.hxx
// quat_from_rows). Each normalise: x 1 / VuMath_sqrt( (y y + x x) + z z ).
inline void front_rig_basis( const std::uint8_t* pose, float* q )
{
    float e[ 3 ], t[ 3 ];
    std::memcpy( e, pose, 12 );
    std::memcpy( t, pose + 0x10, 12 );
    const float dx = t[ 0 ] - e[ 0 ], dy = t[ 1 ] - e[ 1 ], dz = t[ 2 ] - e[ 2 ];
    const float i1 = 1.0f / hull_sqrt( ( dy * dy + dx * dx ) + dz * dz );
    const float fx = i1 * dx, fy = i1 * dy, fz = i1 * dz;
    const float zero = 0.0f;
    const float fz0 = fz * zero;
    const float rx = fy - fz0, ry = fz0 - fx, rz = fx * zero - fy * zero;
    const float i2 = 1.0f / hull_sqrt( ( ry * ry + rx * rx ) + rz * rz );
    const float Rx = i2 * rx, Ry = i2 * ry, Rz = i2 * rz;
    const float ux = Ry * fz - Rz * fy, uy = Rz * fx - Rx * fz, uz = Rx * fy - Ry * fx;
    const float i3 = 1.0f / hull_sqrt( ( uy * uy + ux * ux ) + uz * uz );
    const float m[ 16 ] = { Rx, Ry, Rz, 0.0f, fx, fy, fz, 0.0f, i3 * ux, i3 * uy, i3 * uz, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    quat_from_rows( q, m );
}

// VuQuaternion slerp 0x5314C0 (ecx out, edx a, [esp+4] b, xmm3 s): dot =
// ((b.x a.x + b.y a.y) + a.z b.z) + a.w b.w; 0 > dot: a' = -a, dot = -dot;
// theta = acosf( minss( dot, 1 ) ) (__libm_sse2_acosf: the CRT double acos
// narrowed - SUBSTITUTION, as field.hxx); theta > FLT_EPSILON (0x6F7584, comiss):
// k = 1 / fsin( theta ) (x87, stored as float), wa = fsin( theta - theta s ) k,
// wb = fsin( theta s ) k, out = a' wa + b wb; else out = a (NOT a').
inline void front_rig_slerp( float* out, const float* a, const float* b, float s )
{
    float dot = b[ 0 ] * a[ 0 ] + b[ 1 ] * a[ 1 ];
    dot = dot + a[ 2 ] * b[ 2 ];
    dot = dot + a[ 3 ] * b[ 3 ];
    float p[ 4 ] = { a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ] };
    if ( _mm_comigt_ss( _mm_setzero_ps( ), _mm_set_ss( dot ) ) )
    {
        for ( float& v : p )
            v = -v;
        dot = -dot;
    }
    const float c = _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( dot ), _mm_set_ss( 1.0f ) ) );
    const float theta = static_cast< float >( std::acos( static_cast< double >( c ) ) );
    if ( !_mm_comigt_ss( _mm_set_ss( theta ), _mm_set_ss( 1.1920929e-07f ) ) )
    {
        std::memcpy( out, a, 16 );
        return;
    }
    const float k = 1.0f / wave_x87_sin( theta );
    const float ts = theta * s;
    const float wa = wave_x87_sin( theta - ts ) * k;
    const float wb = wave_x87_sin( ts ) * k;
    out[ 0 ] = p[ 0 ] * wa + b[ 0 ] * wb;
    out[ 1 ] = p[ 1 ] * wa + b[ 1 ] * wb;
    out[ 2 ] = p[ 2 ] * wa + wb * b[ 2 ];
    out[ 3 ] = p[ 3 ] * wa + b[ 3 ] * wb;
}

// VuQuaternion -> VuMatrix 0x531180 (ecx q = x y z w; out; retn 4): with x2 =
// 2x .. : rows ( 1 - (zz + yy), xy - wz, xz + wy, 0 ), ( xy + wz, 1 - (zz + xx),
// yz - wx, 0 ), ( xz - wy, yz + wx, 1 - (yy + xx), 0 ), ( 0, 0, 0, 1 ).
inline void front_rig_matrix( const float* q, float* m )
{
    const float x2 = q[ 0 ] * 2.0f, y2 = q[ 1 ] * 2.0f, z2 = q[ 2 ] * 2.0f;
    const float wx = q[ 3 ] * x2, wy = q[ 3 ] * y2, wz = q[ 3 ] * z2;
    const float xx = q[ 0 ] * x2, yy = q[ 1 ] * y2, zz = q[ 2 ] * z2;
    const float xy = q[ 0 ] * y2, xz = q[ 0 ] * z2, yz = q[ 1 ] * z2;
    const float out[ 16 ] = { 1.0f - ( zz + yy ), xy - wz, xz + wy, 0.0f, xy + wz, 1.0f - ( zz + xx ), yz - wx, 0.0f,
                              xz - wy, yz + wx, 1.0f - ( yy + xx ), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    std::memcpy( m, out, 64 );
}

// 0x4BEA90 (thiscall rig; out pose; retn 4): t = elapsed / duration, s = (t 3)
// t - ((t 2) t) t; q = slerp( basis( from ), basis( to ), s ) -> matrix M; eye =
// from (1 - s) + to s (x y z); target = eye + M row 1 (y and z as row + eye);
// far = from (1 - s) + to s; the eye's and the target's w lanes = basis( to )'s w
// (the stack temporary they are copied through).
inline void front_rig_pose( const front_rig_t& r, std::uint8_t* out )
{
    const float t = r.f( 0x2BC ) / r.f( 0x2B8 );
    const float s = ( t * 3.0f ) * t - ( ( t * 2.0f ) * t ) * t;
    float qa[ 4 ], qb[ 4 ], q[ 4 ], m[ 16 ];
    front_rig_basis( r.w + 0x270, qa );
    front_rig_basis( r.w + 0x294, qb );
    front_rig_slerp( q, qa, qb, s );
    front_rig_matrix( q, m );
    const float u = 1.0f - s;
    float eye[ 4 ], target[ 4 ];
    for ( int i = 0; i < 3; ++i )
        eye[ i ] = r.f( 0x270 + 4 * i ) * u + r.f( 0x294 + 4 * i ) * s;
    eye[ 3 ] = qb[ 3 ];
    target[ 0 ] = eye[ 0 ] + m[ 4 ];
    target[ 1 ] = m[ 5 ] + eye[ 1 ];
    target[ 2 ] = m[ 6 ] + eye[ 2 ];
    target[ 3 ] = qb[ 3 ];
    const float far_plane = r.f( 0x290 ) * u + r.f( 0x2B4 ) * s;
    std::memcpy( out, eye, 16 );
    std::memcpy( out + 0x10, target, 16 );
    std::memcpy( out + 0x20, &far_plane, 4 );
}

// The tick 0x4BE490 (thiscall rig; xmm1 dt): aspect = (float) w / (float) h of
// the display, FOV 0x4D2C60, the projection ( fov, aspect, the camera's near
// +0x178, its far +0x17C ); when duration > elapsed (comiss): elapsed += dt;
// pose = "to" once elapsed >= duration (comiss / jb: a NaN interpolates), else
// 0x4BEA90's; the camera placed at the pose (front_rig_place).
inline void front_rig_tick( front_rig_t& r, float dt, const camera_env_t& env )
{
    const float aspect = static_cast< float >( env.display_w ) / static_cast< float >( env.display_h );
    const float fov = camera_vertical_fov( env );
    camera_projection( r.camera( ), fov, aspect, r.f( 0x184 ), r.f( 0x188 ) );
    const float duration = r.f( 0x2B8 );
    float elapsed = r.f( 0x2BC );
    if ( !_mm_comigt_ss( _mm_set_ss( duration ), _mm_set_ss( elapsed ) ) )
        return;
    elapsed = elapsed + dt;
    r.put( 0x2BC, elapsed );
    std::uint8_t pose[ 0x24 ];
    if ( _mm_comige_ss( _mm_set_ss( elapsed ), _mm_set_ss( duration ) ) && elapsed == elapsed )
        std::memcpy( pose, r.w + 0x294, 0x24 );
    else
        front_rig_pose( r, pose );
    front_rig_place( r, pose );
}
