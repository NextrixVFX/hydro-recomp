#pragma once

#include <workspace/core/boat/events.hxx>
#include <workspace/core/boat/fluid.hxx>
#include <workspace/core/boat/hull.hxx>
#include <workspace/core/world/pfxsim.hxx>

#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

// VuBoatEntity's pfx controller (the object at boat +0x234; its +0x30 is the
// boat) and what the Build tick 0x41F330's step 5 runs on it: 0x419700 (dt).
//
// The controller keeps seven lists (std::vector: begin / end / capacity) of
// 0x60-byte records - +0x18 a VuPfxSystemInstance, +0x1C a bone index of the
// boat's model instance (< 0: none), +0x20 the record's local matrix - at
// +0x38 +0x44 +0x50 +0x5C +0x68 +0x74 +0x80 (list 0 .. 6 here: named by
// offset, the ctor that fills them is not read), the five "on" bytes +0x8C ..
// +0x90 (lists 2, 3, 4, 5, 6), one more system at +0x94 (0 = none) and an aabb
// +0x98 .. +0xB7 (min x y z w, max x y z w).
//
// Every call onto a system is a Vu virtual of VuPfxSystemInstance (vtable
// 0x74FF8C): slot 0 setPosition (+0), 2 setMatrix (+8), 3 the four words at
// +0x5C (+0xC: the velocity), 5 setColor (+0x14), 7 the aabb (+0x1C), 11 start
// (+0x2C), 12 stop(immediate) (+0x30), 13 tick(dt) (+0x34) - boat_pfx_ops_t.
// The water agent's recreated runtime implements them (world/pfxclass.hxx /
// pfxsim.hxx): boat_pfx_runtime_ops binds them.

struct boat_pfx_record_t
{
    std::uint32_t system = 0;   // +0x18
    std::int32_t bone = -1;     // +0x1C
    float local[ 16 ]{};        // +0x20
};

struct boat_pfx_t
{
    std::vector< boat_pfx_record_t > list[ 7 ];   // +0x38 +0x44 +0x50 +0x5C +0x68 +0x74 +0x80
    std::uint8_t on[ 5 ]{};                       // +0x8C .. +0x90 (lists 2 .. 6)
    std::uint32_t extra = 0;                      // +0x94
    std::uint32_t aabb[ 8 ]{};                    // +0x98 .. +0xB7
    bool aabb_w_stale = false;   // a merge (0x41A790) left +0xA4 / +0xB4 = that function's stale stack: UNKNOWN
};

// What 0x419700 reads through the controller's +0x30 (the boat). `throttle`
// is the driver's (boat +0x220) vt+0x64 answer - retail's FIRST call, made
// before anything is written; `d570` is 0x41D570's answer (boat/tick.hxx
// boat_41D570 - it reads none of what this function or its callees write
// before it runs, so the caller may compute it first).
struct boat_pfx_boat_t
{
    float throttle = 0.0f;
    float d570 = 0.0f;
    const std::uint32_t* velocity = nullptr;   // body (+0x21C) +0x140, four words
    std::uint8_t b32c = 0;                     // boat +0x32C
    std::uint32_t f400 = 0, f404 = 0;          // boat +0x400 / +0x404 (words)
    float f2b4 = 0.0f;                         // boat +0x2B4 (the water colour blend, Build step 2)
    const std::uint8_t* colour = nullptr;      // boat +0x454 .. +0x45B
    const float* offset = nullptr;             // boat +0x49C (3)
    const float* world = nullptr;              // the transform (+0x50) +0x50: 16 floats (+0x80 = row 3)
    const float* bones = nullptr;              // the model instance (+0x2A4) +0x18: 16 floats each
    int bone_count = 0;
};

struct boat_pfx_ops_t
{
    void* user = nullptr;
    void ( *set_position )( void* user, std::uint32_t system, const float* p ) = nullptr;   // slot 0 (reads 3)
    void ( *set_matrix )( void* user, std::uint32_t system, const float* m16 ) = nullptr;   // 2
    void ( *set_velocity )( void* user, std::uint32_t system, const std::uint32_t* v4 ) = nullptr;   // 3
    void ( *set_color )( void* user, std::uint32_t system, const float* rgba ) = nullptr;   // 5
    const std::uint32_t* ( *aabb )( void* user, std::uint32_t system ) = nullptr;         // 7 (8 words)
    void ( *start )( void* user, std::uint32_t system ) = nullptr;                          // 11
    void ( *stop )( void* user, std::uint32_t system, bool immediate ) = nullptr;           // 12
    void ( *tick )( void* user, std::uint32_t system, float dt ) = nullptr;                 // 13
    // 0x41E2E0 (ecx the boat, xmm1 a, xmm2 b) = boat/state.hxx boat_driver_raise( driver, human, a, b ).
    void ( *raise )( void* user, float a, float b ) = nullptr;
};

// 0x41ABE0 (stdcall list): every record's system started (slot 11).
inline void boat_pfx_start_list( const std::vector< boat_pfx_record_t >& l, const boat_pfx_ops_t& o )
{
    for ( const boat_pfx_record_t& r : l )
        o.start( o.user, r.system );
}

// 0x41AC50 (stdcall list, immediate): every record's system stopped (slot 12).
inline void boat_pfx_stop_list( const std::vector< boat_pfx_record_t >& l, bool immediate, const boat_pfx_ops_t& o )
{
    for ( const boat_pfx_record_t& r : l )
        o.stop( o.user, r.system, immediate );
}

// 0x41AB70 (stdcall list, rgba*): every record's slot 5 with the same four floats.
inline void boat_pfx_color_list( const std::vector< boat_pfx_record_t >& l, const float* rgba, const boat_pfx_ops_t& o )
{
    for ( const boat_pfx_record_t& r : l )
        o.set_color( o.user, r.system, rgba );
}

// 0x41A790 (thiscall controller; list; xmm2 dt; retn 4): per record M =
// local (+0x20) [. bone (+0x1C) when >= 0] . the transform's world (+0x50) .
// translation( boat +0x49C ) (VuMatrix_multiply 0x4078C0, the translation's
// row 3 = (x, y, z, 1) written in place), slot 2 (M), slot 13 (dt); then slot
// 7, and unless its min x == FLT_MAX (ucomiss / jnp: NaN merges) slot 7 again
// and the controller's aabb takes minss( own, its ) on +0x98 .. +0xA0 and
// maxss on +0xA8 .. +0xB0. The merge's movq stores carry a stack word nothing
// in the function writes into +0xA4 / +0xB4 (UNKNOWN: aabb_w_stale).
inline void boat_pfx_place_list( boat_pfx_t& c, const std::vector< boat_pfx_record_t >& l, const boat_pfx_boat_t& b, float dt,
                                 const boat_pfx_ops_t& o )
{
    float m[ 16 ], t[ 16 ], x[ 16 ];
    for ( const boat_pfx_record_t& r : l )
    {
        std::memcpy( m, r.local, 64 );
        if ( r.bone >= 0 )
        {
            hull_multiply( m, x, b.bones + 16 * r.bone );
            std::memcpy( m, x, 64 );
        }
        hull_multiply( m, x, b.world );
        std::memcpy( m, x, 64 );
        hull_translation( t, b.offset );
        hull_multiply( m, x, t );
        std::memcpy( m, x, 64 );
        o.set_matrix( o.user, r.system, m );
        o.tick( o.user, r.system, dt );
        float lo;
        std::memcpy( &lo, o.aabb( o.user, r.system ), 4 );
        if ( lo == water_bits( 0x7F7FFFFFu ) )   // ordered equal only (jnp)
            continue;
        const std::uint32_t* a = o.aabb( o.user, r.system );
        float own[ 8 ], its[ 8 ];
        std::memcpy( own, c.aabb, 32 );
        std::memcpy( its, a, 32 );
        for ( int k = 0; k < 3; ++k )
        {
            own[ k ] = fluid_min( own[ k ], its[ k ] );
            own[ 4 + k ] = fluid_max( own[ 4 + k ], its[ 4 + k ] );
        }
        std::memcpy( c.aabb, own, 12 );
        std::memcpy( c.aabb + 4, own + 4, 12 );
        c.aabb_w_stale = true;
    }
}

// 0x419700 (__usercall: ecx the controller, xmm1 dt), in retail's order:
//   - the throttle t (driver vt+0x64, fstp). List 2 (+0x50, on +0x8C): off and
//     t > 0.75 -> on, started, 0x41E2E0( 0.75, 0.75 ); on and 0.25 > t -> off,
//     stopped (not immediate). Every compare is a comiss branch (NaN: neither).
//   - List 3 (+0x5C, +0x8D) on s = (vx vx + vy vy) + vz vz of the body's
//     +0x140: off, t > 0.75 and 17.8816^2 > s (dword 0x736284, its .data
//     value: nothing writes it) -> on, started; on and (0.25 > t or s >
//     35.7632^2 (0x736288)) -> off, stopped.
//   - List 4 (+0x68, +0x8E) follows the byte boat +0x32C: set -> started,
//     0x41E2E0( 1.0, 0.0 ); clear -> stopped IMMEDIATELY.
//   - Lists 5 / 6 (+0x74 / +0x80, +0x8F / +0x90) on boat +0x400 / +0x404: off
//     and > 0.1 -> on, started; on and 0 >= it -> off, stopped.
//   - 0x41AAD0 (thiscall; list +0x44; one ignored stack word; retn 8): each
//     list-1 record's slot 3 with the body's +0x140 .. +0x14F.
//   - The colour (r, g, b) lane k = (1 - v) x (byte(+0x454 + k) x c) +
//     (byte(+0x458 + k) x c) x v, v = boat +0x2B4, c = 0x3B808081 (movzx ->
//     cvtdq2ps -> mulss); 0x41AB70 with (1, 1, 1, maxss( 0x41D570, 0 )) on
//     list 0, (r, g, b, 1) on lists 1 and 3, (1, 1, 1, +0x400) on 5,
//     (1, 1, 1, +0x404) on 6.
//   - The aabb reset: +0x98 .. +0xA0 FLT_MAX, +0xA8 .. +0xB0 -FLT_MAX; +0xA4 and
//     +0xB4 take the stack word last written - the +0x404 the list-6 colour
//     carried.
//   - 0x41A790 on lists 2, 3, 4 (only while +0x8E), 0, 1, 5, 6.
//   - With +0x94: its slot 0 (the transform's +0x80), slot 5 (r, g, b, 1),
//     slot 13 (dt).
// False - nothing done - when a record's bone is outside the model's bones
// (retail would read past the palette: UNKNOWN).
inline bool boat_pfx_tick( boat_pfx_t& c, const boat_pfx_boat_t& b, float dt, const boat_pfx_ops_t& o )
{
    for ( const auto& l : c.list )
        for ( const boat_pfx_record_t& r : l )
            if ( r.bone >= 0 && ( b.bones == nullptr || r.bone >= b.bone_count ) )
                return false;
    auto gt = []( float a, float x ) { return _mm_comigt_ss( _mm_set_ss( a ), _mm_set_ss( x ) ) != 0; };   // ordered a > x
    auto ge = []( float a, float x ) { return _mm_comige_ss( _mm_set_ss( a ), _mm_set_ss( x ) ) != 0; };   // ordered a >= x
    const float hi = water_bits( 0x3F400000u ), lo = water_bits( 0x3E800000u ), tenth = water_bits( 0x3DCCCCCDu );
    const float t = b.throttle;
    if ( c.on[ 0 ] == 0 )
    {
        if ( gt( t, hi ) )
        {
            c.on[ 0 ] = 1;
            boat_pfx_start_list( c.list[ 2 ], o );
            if ( o.raise )
                o.raise( o.user, hi, hi );
        }
    }
    else if ( gt( lo, t ) )
    {
        c.on[ 0 ] = 0;
        boat_pfx_stop_list( c.list[ 2 ], false, o );
    }
    float v[ 4 ];
    std::memcpy( v, b.velocity, 16 );
    auto speed2 = [ & ]( ) { return sse_add( sse_add( sse_mul( v[ 0 ], v[ 0 ] ), sse_mul( v[ 1 ], v[ 1 ] ) ), sse_mul( v[ 2 ], v[ 2 ] ) ); };
    if ( c.on[ 1 ] == 0 )
    {
        if ( gt( t, hi ) )
        {
            const float k = water_bits( 0x418F0D84u );
            if ( gt( k * k, speed2( ) ) )
            {
                c.on[ 1 ] = 1;
                boat_pfx_start_list( c.list[ 3 ], o );
            }
        }
    }
    else
    {
        const float k = water_bits( 0x420F0D84u );
        if ( gt( lo, t ) || gt( speed2( ), k * k ) )
        {
            c.on[ 1 ] = 0;
            boat_pfx_stop_list( c.list[ 3 ], false, o );
        }
    }
    if ( c.on[ 2 ] == 0 )
    {
        if ( b.b32c )
        {
            c.on[ 2 ] = 1;
            boat_pfx_start_list( c.list[ 4 ], o );
            if ( o.raise )
                o.raise( o.user, 1.0f, 0.0f );
        }
    }
    else if ( b.b32c == 0 )
    {
        c.on[ 2 ] = 0;
        boat_pfx_stop_list( c.list[ 4 ], true, o );
    }
    const std::uint32_t words[ 2 ] = { b.f400, b.f404 };
    for ( int k = 0; k < 2; ++k )
    {
        const float f = water_bits( words[ k ] );
        if ( c.on[ 3 + k ] == 0 )
        {
            if ( gt( f, tenth ) )
            {
                c.on[ 3 + k ] = 1;
                boat_pfx_start_list( c.list[ 5 + k ], o );
            }
        }
        else if ( ge( 0.0f, f ) )
        {
            c.on[ 3 + k ] = 0;
            boat_pfx_stop_list( c.list[ 5 + k ], false, o );
        }
    }
    for ( const boat_pfx_record_t& r : c.list[ 1 ] )
        o.set_velocity( o.user, r.system, b.velocity );
    const float cc = water_bits( 0x3B808081u ), u = sse_sub( 1.0f, b.f2b4 );
    float rgb[ 3 ];
    for ( int k = 0; k < 3; ++k )
    {
        const float a = sse_mul( static_cast< float >( static_cast< std::int32_t >( b.colour[ k ] ) ), cc );
        const float z = sse_mul( sse_mul( static_cast< float >( static_cast< std::int32_t >( b.colour[ 4 + k ] ) ), cc ), b.f2b4 );
        rgb[ k ] = sse_add( sse_mul( u, a ), z );
    }
    const float d = fluid_max( b.d570, 0.0f );
    const float white_d[ 4 ] = { 1.0f, 1.0f, 1.0f, d };
    const float tint[ 4 ] = { rgb[ 0 ], rgb[ 1 ], rgb[ 2 ], 1.0f };
    float white_400[ 4 ] = { 1.0f, 1.0f, 1.0f, 0.0f }, white_404[ 4 ] = { 1.0f, 1.0f, 1.0f, 0.0f };
    std::memcpy( white_400 + 3, &b.f400, 4 );
    std::memcpy( white_404 + 3, &b.f404, 4 );
    boat_pfx_color_list( c.list[ 0 ], white_d, o );
    boat_pfx_color_list( c.list[ 1 ], tint, o );
    boat_pfx_color_list( c.list[ 3 ], tint, o );
    boat_pfx_color_list( c.list[ 5 ], white_400, o );
    boat_pfx_color_list( c.list[ 6 ], white_404, o );
    const std::uint32_t reset[ 8 ] = { 0x7F7FFFFFu, 0x7F7FFFFFu, 0x7F7FFFFFu, b.f404, 0xFF7FFFFFu, 0xFF7FFFFFu, 0xFF7FFFFFu, b.f404 };
    std::memcpy( c.aabb, reset, 32 );
    c.aabb_w_stale = false;
    boat_pfx_place_list( c, c.list[ 2 ], b, dt, o );
    boat_pfx_place_list( c, c.list[ 3 ], b, dt, o );
    if ( c.on[ 2 ] )
        boat_pfx_place_list( c, c.list[ 4 ], b, dt, o );
    boat_pfx_place_list( c, c.list[ 0 ], b, dt, o );
    boat_pfx_place_list( c, c.list[ 1 ], b, dt, o );
    boat_pfx_place_list( c, c.list[ 5 ], b, dt, o );
    boat_pfx_place_list( c, c.list[ 6 ], b, dt, o );
    if ( c.extra != 0 )
    {
        o.set_position( o.user, c.extra, b.world + 12 );
        o.set_color( o.user, c.extra, tint );
        o.tick( o.user, c.extra, dt );
    }
    return true;
}

// The binding onto the recreated VuPfx runtime (world/pfxsim.hxx's heap: a
// system is its heap word): slots 0 / 2 / 3 / 5 / 7 as world/pfxclass.hxx's
// setters, 11 / 12 / 13 as pfx_system_start / _stop / _tick (with rt.hooks).
// `raise` stays the caller's (boat_driver_raise on the boat's driver).
inline boat_pfx_ops_t boat_pfx_runtime_ops( pfx_runtime_t& rt )
{
    boat_pfx_ops_t o;
    o.user = &rt;
    o.set_position = []( void* u, std::uint32_t s, const float* p ) {
        std::uint32_t w[ 3 ];
        std::memcpy( w, p, sizeof( w ) );
        pfx_system_set_position( static_cast< pfx_runtime_t* >( u )->heap.find( s ), w );
    };
    o.set_matrix = []( void* u, std::uint32_t s, const float* m ) {
        std::uint32_t w[ 16 ];
        std::memcpy( w, m, sizeof( w ) );
        pfx_system_set_matrix( static_cast< pfx_runtime_t* >( u )->heap.find( s ), w );
    };
    o.set_velocity = []( void* u, std::uint32_t s, const std::uint32_t* v ) {
        pfx_system_set_words5c( static_cast< pfx_runtime_t* >( u )->heap.find( s ), v );
    };
    o.set_color = []( void* u, std::uint32_t s, const float* c ) {
        std::uint32_t w[ 4 ];
        std::memcpy( w, c, sizeof( w ) );
        pfx_system_set_wordsac( static_cast< pfx_runtime_t* >( u )->heap.find( s ), w );
    };
    o.aabb = []( void* u, std::uint32_t s ) -> const std::uint32_t* {
        return static_cast< pfx_runtime_t* >( u )->heap.find( s + pfx_system_aabb_offset( ) );
    };
    o.start = []( void* u, std::uint32_t s ) { pfx_system_start( static_cast< pfx_runtime_t* >( u )->heap, s ); };
    o.stop = []( void* u, std::uint32_t s, bool immediate ) {
        pfx_runtime_t& rt = *static_cast< pfx_runtime_t* >( u );
        pfx_system_stop( rt.heap, rt.resources, s, immediate );
    };
    o.tick = []( void* u, std::uint32_t s, float dt ) {
        pfx_runtime_t& rt = *static_cast< pfx_runtime_t* >( u );
        pfx_system_tick( rt.heap, s, dt, rt.hooks );
    };
    return o;
}
