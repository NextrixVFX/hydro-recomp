#pragma once

// VuBackground - the front end's 3D backdrop (the Screens/Background project
// plus the front-end camera rig). It is NOT VuFrontEndGameMode: its tick
// delegate's vtable 0x7460F8 is VuMethod1<VuBackground, void, float> (RTTI).
// The game mode manager VuGameModeManagerImpl (vtable 0x74682C, g 0x758FF0;
// ctor 0x4CBAE0) owns one at +0x44: slot 9 0x4CBF10 creates it (new 8 bytes,
// +0 / +4 = 0, ctor 0x4BDEB0) unless one exists; slot 10 0x4CBFA0 releases it
// (0x4BE050, then the 8 bytes freed, +0x44 = 0). The only callers of the two
// slots (tmp/w25_vcalls.py over [0x758FF0] vt+0x24 / +0x28): VuFrontEndGameMode's
// enter 0x4BF630 (0x4BF689) and its exit 0x4BF9B0 (0x4BFA6A) - the UI agent's
// ui/flow.hxx c_front_end::enter / exit, where they are the platform( "mode
// manager", 9 / 10 ) reaches.
//
// The object: +0 the project (a VuProject, its root entity at +0xC; refcount
// +4), +4 the rig (0x2C0 bytes, world/frontcam.hxx front_rig_t, ctor 0x4BE1C0).
//
// ctor 0x4BDEB0 (thiscall object; returns it):
//   rig = new 0x2C0 (+0 = 0 before the ctor 0x4BE1C0; a failed new -> 0) -> +4
//   g 0x759060 0x530BB0( 1 )                       the viewport manager: UNREAD (hook)
//   import 0x64C314( [g 0x759010 +8], 1 )           platform (hook)
//   +0 = 0x4B4360( "Screens/Background" 0x6B67CC )  the project load (VuProject::load)
//   if +0 and !( root +0x14 bit 2 ):
//     g 0x759080 vt+0x18( project )                 the entity repository's add (hook)
//     0x500B10( root )                              VuEntity::gameInitialize
//     0x501260( root, "OnProjectInitialized" 0x6D2D14, an empty VuParams )
//   g 0x758FF4 vt+0x1C                              the track manager's slot 7 (the build)
//   g 0x759008 vt+0x20( {0x7460F8, this, 0x4BE1A0}, "Decision" 0x6B66C4 )   the tick
// tick 0x4BE1A0 (dt): 0x4BE490( rig, dt ) = front_rig_tick.
// release 0x4BE050:
//   g 0x759008 vt+0x28( this )                      every tick of this object unregistered
//   if +0: if root +0x14 bit 2: 0x500B80( root ) + g 0x759080 vt+0x1C( project )
//          (bit 2 = "game initialised": 0x500B10 sets it last, 0x500B75, and
//          returns at once when it is set; 0x500B80 clears it, 0x500BE5)
//          if g 0x759074 +0xC: g 0x759098 vt+0x3C( [g 0x759074 +0xA0], -1 ),
//             g 0x75907C vt+0x30, +0xC = 0, g 0x75907C vt+0xD0   (render: hooks)
//          --project +4 == 0 -> project vt+0( 1 )   (the delete)
//   g 0x758FF4 vt+0x28                              the track manager's slot 10
//   g 0x759060 0x530BB0( 0 ); import 0x64C314( [g 0x759010 +8], 1 )
//   if rig: 0x4BE110( rig ) - g 0x758FB8 +0x2C0 = 0 (the published camera),
//      0x515620 + 0x43CA10 on the rig's event map +0 (its handlers dropped), the
//      map's head and the rig freed.
// The +0 project field is never cleared by the release (the object is freed
// right after by slot 10).

#include <workspace/core/world/frontcam.hxx>

#include <cstdint>
#include <cstring>
#include <memory>

struct background_hooks_t
{
    void* user = nullptr;
    void ( *viewports )( void* user, int on ) = nullptr;                  // g 0x759060 0x530BB0 (unread)
    void ( *platform )( void* user, int on ) = nullptr;                   // import 0x64C314
    bool ( *load_project )( void* user ) = nullptr;                       // 0x4B4360( "Screens/Background" ) -> +0
    std::uint32_t ( *root_flags )( void* user ) = nullptr;                // the project's root entity +0x14
    void ( *repository_add )( void* user ) = nullptr;                     // g 0x759080 vt+0x18
    void ( *repository_remove )( void* user ) = nullptr;                  // vt+0x1C
    void ( *game_initialize )( void* user ) = nullptr;                    // 0x500B10( root )
    void ( *game_release )( void* user ) = nullptr;                       // 0x500B80( root )
    void ( *project_initialized )( void* user ) = nullptr;                // 0x501260( root, "OnProjectInitialized" )
    void ( *render_flush )( void* user ) = nullptr;                       // the g 0x759074 +0xC block (render)
    void ( *project_release )( void* user ) = nullptr;                    // the refcount drop -> delete
    void ( *track_build )( void* user ) = nullptr;                        // g 0x758FF4 vt+0x1C
    void ( *track_clear )( void* user ) = nullptr;                        // g 0x758FF4 vt+0x28
    void ( *tick_register )( void* user, const char* phase ) = nullptr;   // g 0x759008 vt+0x20
    void ( *tick_unregister )( void* user ) = nullptr;                    // vt+0x28
};

struct background_t
{
    bool project = false;                  // +0 non-null
    std::unique_ptr< front_rig_t > rig;    // +4
};

inline void background_construct( background_t& b, const camera_env_t& env, const background_hooks_t& h )
{
    b.rig = std::make_unique< front_rig_t >( );
    front_rig_construct( *b.rig, env );
    if ( h.viewports )
        h.viewports( h.user, 1 );
    if ( h.platform )
        h.platform( h.user, 1 );
    b.project = h.load_project && h.load_project( h.user );
    if ( b.project && ( ( h.root_flags ? h.root_flags( h.user ) : 0u ) >> 2 & 1u ) == 0u )
    {
        if ( h.repository_add )
            h.repository_add( h.user );
        if ( h.game_initialize )
            h.game_initialize( h.user );
        if ( h.project_initialized )
            h.project_initialized( h.user );
    }
    if ( h.track_build )
        h.track_build( h.user );
    if ( h.tick_register )
        h.tick_register( h.user, "Decision" );
}

// The tick 0x4BE1A0.
inline void background_tick( background_t& b, float dt, const camera_env_t& env )
{
    if ( b.rig )
        front_rig_tick( *b.rig, dt, env );
}

inline void background_release( background_t& b, const background_hooks_t& h )
{
    if ( h.tick_unregister )
        h.tick_unregister( h.user );
    if ( b.project )
    {
        if ( ( ( h.root_flags ? h.root_flags( h.user ) : 0u ) >> 2 & 1u ) != 0u )
        {
            if ( h.game_release )
                h.game_release( h.user );
            if ( h.repository_remove )
                h.repository_remove( h.user );
        }
        if ( h.render_flush )
            h.render_flush( h.user );
        if ( h.project_release )
            h.project_release( h.user );
    }
    if ( h.track_clear )
        h.track_clear( h.user );
    if ( h.viewports )
        h.viewports( h.user, 0 );
    if ( h.platform )
        h.platform( h.user, 1 );
    b.rig.reset( );   // 0x4BE110
}

// VuSetFrontEndCameraEntity's "Trigger" handler 0x408F90 (thiscall entity;
// retn 8, answers VuRetVal 0): one VuParams buffer (0x84 bytes on its stack),
// its size word +0x80 zeroed before each add - so the bytes past the size are
// the previous add's (and the first add's tail is uninitialised stack): only
// [0, size) and the size are defined. Modify Eye +0x64 -> addVector3 0x4089E0
// (type 6 + the 16 bytes at +0x68) and "OnSetFrontEndCameraEyePosition";
// Modify Target +0x65 -> +0x78, "...TargetPosition"; Modify Far +0x66 ->
// addFloat 0x405120 (type 2 + +0x88), "...DrawDistance"; then always
// Interpolation Time +0x60, "...Interpolation". Each name FNV-1a hashed inline,
// broadcast through VuEventManager_broadcast 0x505690 (g 0x759028).
struct front_end_trigger_t
{
    float interpolation = 0.0f;      // +0x60
    std::uint8_t modify[ 3 ]{};      // +0x64 eye, +0x65 target, +0x66 far
    float eye[ 4 ]{};                // +0x68 (w lane included: the add copies 16 bytes)
    float target[ 4 ]{};             // +0x78
    float far_plane = 0.0f;          // +0x88
};

template < typename Broadcast >
inline void front_end_trigger( const front_end_trigger_t& e, Broadcast&& broadcast )
{
    std::uint8_t p[ 0x84 ] = { };
    const auto fnv = []( const char* s ) {
        std::uint32_t x = 0x811C9DC5u;
        for ( ; *s; ++s )
            x = ( x ^ static_cast< std::uint8_t >( *s ) ) * 16777619u;
        return x;
    };
    const auto add = [ &p ]( std::uint32_t type, const void* v, std::int32_t bytes ) {
        std::memcpy( p, &type, 4 );
        std::memcpy( p + 4, v, static_cast< std::size_t >( bytes ) );
        const std::int32_t size = 4 + bytes;
        std::memcpy( p + 0x80, &size, 4 );
    };
    if ( e.modify[ 0 ] != 0 )
    {
        add( 6, e.eye, 16 );
        broadcast( fnv( "OnSetFrontEndCameraEyePosition" ), static_cast< const std::uint8_t* >( p ) );
    }
    if ( e.modify[ 1 ] != 0 )
    {
        add( 6, e.target, 16 );
        broadcast( fnv( "OnSetFrontEndCameraTargetPosition" ), static_cast< const std::uint8_t* >( p ) );
    }
    if ( e.modify[ 2 ] != 0 )
    {
        add( 2, &e.far_plane, 4 );
        broadcast( fnv( "OnSetFrontEndCameraDrawDistance" ), static_cast< const std::uint8_t* >( p ) );
    }
    add( 2, &e.interpolation, 4 );
    broadcast( fnv( "OnSetFrontEndCameraInterpolation" ), static_cast< const std::uint8_t* >( p ) );
}

// The rig's event map (registered by the rig ctor 0x4BE1C0, 0x5156E0 x 4): a
// broadcast by its FNV-1a name hash reaches the handler; false = not one of
// the four (the rig ignores it).
inline bool background_event( background_t& b, std::uint32_t hash, const std::uint8_t* params )
{
    if ( !b.rig )
        return false;
    const auto fnv = []( const char* s ) {
        std::uint32_t x = 0x811C9DC5u;
        for ( ; *s; ++s )
            x = ( x ^ static_cast< std::uint8_t >( *s ) ) * 16777619u;
        return x;
    };
    if ( hash == fnv( "OnSetFrontEndCameraDrawDistance" ) )
        front_rig_draw_distance( *b.rig, params );
    else if ( hash == fnv( "OnSetFrontEndCameraEyePosition" ) )
        front_rig_eye( *b.rig, params );
    else if ( hash == fnv( "OnSetFrontEndCameraTargetPosition" ) )
        front_rig_target( *b.rig, params );
    else if ( hash == fnv( "OnSetFrontEndCameraInterpolation" ) )
        front_rig_interpolation( *b.rig, params );
    else
        return false;
    return true;
}
