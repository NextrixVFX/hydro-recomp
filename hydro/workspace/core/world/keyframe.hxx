#pragma once

// VuControlEntity (vtable 0x752A28, 0x98 bytes: create 0x5CCA50 = pre-zero
// 0x5CCAD0 + ctor 0x5CCBA0) and its VuControlKeyframeEntity children (vtable
// 0x7528AC, 0x6C bytes: create 0x5CC710, ctor 0x5CC780) - the engine's keyframed
// driver of a VuControlComponent's channels. Area51 / Stadium author it on their
// wave generators (a VuDirectionalWaveEntity: world/watchers.hxx
// water_directional_control is the component's handler that then runs).
// Recreated from the disassembly and verified against retail (hydro
// checks/logic.hxx verify_control_entity, case control_entity: retail's own
// code on registry-made entities, real Area51 / Stadium data and hand-built
// documents).
//
// The ctor 0x5CCBA0: six VuBoolProperty (0x737450) "Initially Active" +0x64,
// "One Shot" +0x65, "Network Synchronized" +0x66, "Loop" +0x67, "Ease In/Out"
// +0x68 (default 1), "Real Time" +0x69; the input method "OnManualTick"
// (0x5CD710, registered on +0xC); a VuScriptComponent (+0x5C) with the input
// plugs "Activate" 0x5CD7A0 / "Deactivate" 0x5CD890 and the outputs
// "OnActivated", "OnDeactivated", "OnDone"; the VuScriptRef "Entity" (+0x60,
// vtable 0x74D8EC, type 0x7367F8) whose change delegate is 0x5CD990. +0x6C the
// target's VuControlComponent, +0x70 active, +0x71 the one-shot latch, +0x74 /
// +0x78 / +0x7C a VuArray of 8-byte (time, keyframe) pairs (capacity 8), +0x80
// the duration, +0x84 the clock, +0x88 a byte, +0x8C 0, +0x90 / +0x94 the
// ChildEntities containers (slots 2 / 3).
//
// The component (VuControlComponent, vtable 0x757EA0): +0x10 its interface
// (VuControlComponentIF: slot 0 "changed", 1 "begin", 2 "end"), +0x14 the
// controlling entity (one at a time), +0x18 .. +0x20 a std::vector of 12-byte
// records {char* name, +4 initial, +8 value}.
//
// SUBSTITUTION: the keyframe sort is the import qsort (MSVCR110.dll, 0x64C178).
// control_qsort is the VC11 CRT's qsort algorithm transliterated from its
// published source, not from this binary; only distinct times were measured.
// Hooks (platform / engine, not recreated): the tick manager g 0x759008 (vt+0x20
// register "Motion", vt+0x28 unregister, vt+0x3C the real-time step), the
// output plugs' fire, the ref resolution (which component the ref's entity has).

#include <workspace/core/assets/tree.hxx>
#include <workspace/core/entity/property.hxx>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// VuControlComponent: the part the control entity reads and writes.
struct control_component_t
{
    std::vector< std::string > names;          // record +0 (char*)
    std::vector< std::uint32_t > records;      // 3 words per record: +0 (name, unused here), +4 initial, +8 value
    const void* controller = nullptr;          // +0x14
    void* user = nullptr;
    // The interface +0x10: slot 0 after every write of the values, slots 1 / 2 at activation / deactivation.
    void ( *changed )( void* user, control_component_t& c ) = nullptr;
    void ( *begin )( void* user, control_component_t& c ) = nullptr;
    void ( *end )( void* user, control_component_t& c ) = nullptr;
    int count( ) const { return static_cast< int >( records.size( ) / 3u ); }
};

// One VuBasicProperty<float,1> (vtable 0x737484, 0x20 bytes) a keyframe made in 0x5CC820.
struct control_property_t
{
    std::string name;               // +4
    std::uint32_t fallback = 0;     // +0x14 (the default)
    std::uint32_t current = 0;      // +0x18 (the value at the last load)
};

// VuControlKeyframeEntity: ctor 0x5CC780 (+0x5C = 0, +0x60 a VuArray<float> of capacity 8, size 0, no property).
struct control_keyframe_t
{
    std::uint32_t time = 0;                       // +0x5C ("Time")
    std::vector< std::uint32_t > values;          // +0x60 data, +0x64 size
    std::int32_t capacity = 8;                    // +0x68
    std::vector< control_property_t > properties; // +0x44 (Time first, then one per record)
};

struct control_child_t
{
    bool keyframe = false;        // its type chain holds 0x736B88
    std::string name;             // +0x18 (the template lookup key)
    control_keyframe_t frame;
};

struct control_entity_t
{
    std::uint8_t initially_active = 0, one_shot = 0, network_sync = 0, loop = 0, ease = 1, real_time = 0;   // +0x64 .. +0x69
    control_component_t* target = nullptr;     // +0x6C
    std::uint8_t active = 0;                   // +0x70
    std::uint8_t done = 0;                     // +0x71
    std::vector< std::pair< std::uint32_t, int > > keys;   // +0x74 (time bits, child index), +0x78 the count
    std::int32_t capacity = 8;                 // +0x7C
    std::uint32_t duration = 0;                // +0x80
    std::uint32_t time = 0;                    // +0x84
    const value_t* children_data = nullptr;    // +0x90 (slot 2)
    const value_t* template_data = nullptr;    // +0x94 (slot 3)
    std::vector< control_child_t > children;   // +0x38, in order
};

struct control_hooks_t
{
    void* user = nullptr;
    // An output plug fired with an empty VuParams ("OnActivated" / "OnDeactivated" / "OnDone").
    void ( *fire )( void* user, control_entity_t& e, const char* plug ) = nullptr;
    // The tick manager g 0x759008: vt+0x20 ( {0x7529F8, e, 0x5CD9F0}, "Motion" ) / vt+0x28 ( e ).
    void ( *tick_register )( void* user, control_entity_t& e, bool on ) = nullptr;
    // vt+0x3C: the real-time step (an x87 return, stored as a float).
    float ( *real_dt )( void* user ) = nullptr;
    int* unrecreated = nullptr;
};

inline float control_f( std::uint32_t w )
{
    float f;
    std::memcpy( &f, &w, 4 );
    return f;
}

inline std::uint32_t control_w( float f )
{
    std::uint32_t w;
    std::memcpy( &w, &f, 4 );
    return w;
}

inline void control_unrecreated( const control_hooks_t& h )
{
    if ( h.unrecreated )
        ++*h.unrecreated;
}

// The keyframe compare 0x5CCB60 (comiss): -1 when b > a, else a > b.
inline int control_compare( std::uint32_t a, std::uint32_t b )
{
    const float x = control_f( a ), y = control_f( b );
    if ( y > x )
        return -1;
    return x > y ? 1 : 0;
}

// SUBSTITUTION (see the head): the VC11 CRT qsort (CUTOFF 8, shortsort by the
// maximum, median of three, the explicit stack) on the 8-byte pairs.
inline void control_qsort( std::vector< std::pair< std::uint32_t, int > >& a, int num )
{
    if ( num < 2 )
        return;
    auto cmp = [ & ]( int i, int j ) { return control_compare( a[ i ].first, a[ j ].first ); };
    auto swp = [ & ]( int i, int j ) { if ( i != j ) std::swap( a[ i ], a[ j ] ); };
    int lostk[ 32 ], histk[ 32 ];
    int stkptr = 0;
    int lo = 0, hi = num - 1;
    for ( ;; )
    {
        const int size = hi - lo + 1;
        if ( size <= 8 )
        {
            int h2 = hi;
            while ( h2 > lo )
            {
                int max = lo;
                for ( int p = lo + 1; p <= h2; ++p )
                    if ( cmp( p, max ) > 0 )
                        max = p;
                swp( max, h2 );
                --h2;
            }
        }
        else
        {
            int mid = lo + size / 2;
            if ( cmp( lo, mid ) > 0 ) swp( lo, mid );
            if ( cmp( lo, hi ) > 0 ) swp( lo, hi );
            if ( cmp( mid, hi ) > 0 ) swp( mid, hi );
            int loguy = lo, higuy = hi;
            for ( ;; )
            {
                if ( mid > loguy )
                    do { ++loguy; } while ( loguy < mid && cmp( loguy, mid ) <= 0 );
                if ( mid <= loguy )
                    do { ++loguy; } while ( loguy <= hi && cmp( loguy, mid ) <= 0 );
                do { --higuy; } while ( higuy > mid && cmp( higuy, mid ) > 0 );
                if ( higuy < loguy )
                    break;
                swp( loguy, higuy );
                if ( mid == higuy )
                    mid = loguy;
            }
            ++higuy;
            if ( mid < higuy )
                do { --higuy; } while ( higuy > mid && cmp( higuy, mid ) == 0 );
            if ( mid >= higuy )
                do { --higuy; } while ( higuy > lo && cmp( higuy, mid ) == 0 );
            if ( higuy - lo >= hi - loguy )
            {
                if ( lo < higuy ) { lostk[ stkptr ] = lo; histk[ stkptr ] = higuy; ++stkptr; }
                if ( loguy < hi ) { lo = loguy; continue; }
            }
            else
            {
                if ( loguy < hi ) { lostk[ stkptr ] = loguy; histk[ stkptr ] = hi; ++stkptr; }
                if ( lo < higuy ) { hi = higuy; continue; }
            }
        }
        --stkptr;
        if ( stkptr < 0 )
            return;
        lo = lostk[ stkptr ];
        hi = histk[ stkptr ];
    }
}

// VuArray growth (0x415690 resize / 0x5CD490's push): past the capacity it
// becomes max( n, cap + cap / 2 ) (cdq / sar: signed halving).
inline std::int32_t control_grow( std::int32_t n, std::int32_t cap )
{
    if ( n <= cap )
        return cap;
    const std::int32_t c = cap + cap / 2;
    return n > c ? n : c;
}

// 0x5CC820 (thiscall on the keyframe; the target entity or 0): the property list
// destroyed, size 0, time 0, then the "Time" property (default = +0x5C = 0); with
// the entity's VuControlComponent (0x5CCA10) and n records: resize( n ), value i
// = record i +4 and a float property named record i +0 over it (default = it).
inline void control_keyframe_bind( control_keyframe_t& k, const control_component_t* c )
{
    k.properties.clear( );
    k.values.clear( );
    k.time = 0;
    k.properties.push_back( { "Time", 0u, 0u } );
    if ( !c )
        return;
    const int n = c->count( );
    if ( n == 0 )
        return;
    k.capacity = control_grow( n, k.capacity );
    k.values.assign( static_cast< std::size_t >( n ), 0u );
    for ( int i = 0; i < n; ++i )
    {
        k.values[ i ] = c->records[ 3 * i + 1 ];
        k.properties.push_back( { c->names[ i ], k.values[ i ], k.values[ i ] } );
    }
}

// The keyframe's property list loaded from `props` in list order: each is the
// float load 0x406FE0 (target = default, then the read 0x407060; +0x18 = target).
inline void control_keyframe_load( control_keyframe_t& k, const value_t& props )
{
    for ( std::size_t p = 0; p < k.properties.size( ); ++p )
    {
        control_property_t& q = k.properties[ p ];
        std::uint32_t& target = p == 0 ? k.time : k.values[ p - 1 ];
        float t = control_f( target );
        property_float( props, q.name.c_str( ), t, control_f( q.fallback ), false );
        target = control_w( t );
        q.current = target;
    }
}

// Slots 2 / 3 0x5CD2C0 / 0x5CD2E0: data["ChildEntities"] (0x4FBDF0) to +0x90 / +0x94.
inline void control_load( control_entity_t& e, const value_t& data )
{
    e.children_data = &member( data, "ChildEntities" );
}

inline void control_load_template( control_entity_t& e, const value_t& data )
{
    e.template_data = &member( data, "ChildEntities" );
}

// Slot 4 0x5CD300: per child i that is a keyframe - bind it to the ref's entity
// (component = that entity's VuControlComponent or null), load its properties
// from +0x90's element i (when +0x90 is an array and i is inside it) ["data"]
// ["Properties"]; then when `templated` (0x5012E0: an ancestor up the +0x34 chain
// has a +0x4C) every property's default = its value (0x406B00) and a second load
// from +0x94 [ child name ] ["Properties"] (when +0x94 is an object; else null).
inline void control_post_load( control_entity_t& e, const control_component_t* c, bool templated )
{
    static const value_t null{ };
    for ( std::size_t i = 0; i < e.children.size( ); ++i )
    {
        control_child_t& ch = e.children[ i ];
        if ( !ch.keyframe )
            continue;
        control_keyframe_bind( ch.frame, c );
        const value_t* d = e.children_data;
        const value_t& elem = d && d->tag == 5 && i < d->list.size( ) ? d->list[ i ] : null;
        control_keyframe_load( ch.frame, member( member( elem, "data" ), "Properties" ) );
        if ( !templated )
            continue;
        for ( std::size_t p = 0; p < ch.frame.properties.size( ); ++p )
            ch.frame.properties[ p ].fallback = p == 0 ? ch.frame.time : ch.frame.values[ p - 1 ];
        const value_t* t = e.template_data;
        const value_t* found = t && t->tag == 6 ? find( *t, ch.name ) : nullptr;
        control_keyframe_load( ch.frame, member( found ? *found : null, "Properties" ) );
    }
}

// 0x5CD990 (the ref's change delegate): every keyframe child rebound to the ref's entity.
inline void control_rebind( control_entity_t& e, const control_component_t* c )
{
    for ( control_child_t& ch : e.children )
        if ( ch.keyframe )
            control_keyframe_bind( ch.frame, c );
}

// Slot 9 0x5CD6B0 (a child): a keyframe is bound to the ref's entity.
inline void control_child_added( control_entity_t& e, int child, const control_component_t* c )
{
    control_child_t& ch = e.children[ static_cast< std::size_t >( child ) ];
    if ( ch.keyframe )
        control_keyframe_bind( ch.frame, c );
}

// 0x5CDBF0: record j +8 = keyframe k's value j, then the interface's slot 0.
// A keyframe bound with fewer values than the component now has records: retail
// reads past its array - UNKNOWN, counted, nothing written.
inline void control_apply( control_entity_t& e, int k, const control_hooks_t& h )
{
    control_component_t& c = *e.target;
    const control_keyframe_t& f = e.children[ static_cast< std::size_t >( e.keys[ k ].second ) ].frame;
    if ( static_cast< int >( f.values.size( ) ) < c.count( ) )
    {
        control_unrecreated( h );
        return;
    }
    for ( int j = 0; j < c.count( ); ++j )
        c.records[ 3 * j + 2 ] = f.values[ j ];
    if ( c.changed )
        c.changed( c.user, c );
}

// 0x5CDC80 (x in xmm1): k = the last of keys 0 .. n-2 with x >= its time (comiss
// / cmovae; none - esi is an uninitialised local: UNKNOWN, counted, nothing
// written); f = (x - t_k) / (t_k+1 - t_k); value = v_k x (1 - f) + v_k+1 x f.
inline bool control_interpolate( control_entity_t& e, float x, const control_hooks_t& h )
{
    const int n = static_cast< int >( e.keys.size( ) );
    int k = -1;
    for ( int i = 0; i < n - 1; ++i )
        if ( x >= control_f( e.keys[ i ].first ) )
            k = i;
    if ( k < 0 )
    {
        control_unrecreated( h );
        return false;
    }
    const float tk = control_f( e.keys[ k ].first );
    const float f = ( x - tk ) / ( control_f( e.keys[ k + 1 ].first ) - tk );
    const float w = 1.0f - f;
    control_component_t& c = *e.target;
    const control_keyframe_t& a = e.children[ static_cast< std::size_t >( e.keys[ k ].second ) ].frame;
    const control_keyframe_t& b = e.children[ static_cast< std::size_t >( e.keys[ k + 1 ].second ) ].frame;
    if ( static_cast< int >( a.values.size( ) ) < c.count( ) || static_cast< int >( b.values.size( ) ) < c.count( ) )
    {
        control_unrecreated( h );
        return false;
    }
    for ( int j = 0; j < c.count( ); ++j )
    {
        const float p = control_f( a.values[ j ] ) * w;
        const float q = control_f( b.values[ j ] ) * f;
        c.records[ 3 * j + 2 ] = control_w( p + q );
    }
    if ( c.changed )
        c.changed( c.user, c );
    return true;
}

// "Activate" 0x5CD7A0: with a target, the latch clear and no controller: the
// controller = this, the interface's slot 1, active; One Shot sets the latch;
// the clock 0; "OnActivated". The VuRetVal is void.
inline void control_activate( control_entity_t& e, const control_hooks_t& h )
{
    control_component_t* c = e.target;
    if ( !c || e.done || c->controller )
        return;
    c->controller = &e;
    if ( c->begin )
        c->begin( c->user, *c );
    e.active = 1;
    if ( e.one_shot )
        e.done = 1;
    e.time = 0;
    if ( h.fire )
        h.fire( h.user, e, "OnActivated" );
}

// "Deactivate" 0x5CD890: when active - inactive, "OnDeactivated", the
// controller 0, the interface's slot 2 (+0x6C is not tested: null UNKNOWN).
inline void control_deactivate( control_entity_t& e, const control_hooks_t& h )
{
    if ( !e.active )
        return;
    e.active = 0;
    if ( h.fire )
        h.fire( h.user, e, "OnDeactivated" );
    control_component_t* c = e.target;
    if ( !c )
    {
        control_unrecreated( h );
        return;
    }
    c->controller = nullptr;
    if ( c->end )
        c->end( c->user, *c );
}

// The "Motion" tick 0x5CD9F0 (dt on the stack).
inline void control_tick( control_entity_t& e, float dt, const control_hooks_t& h )
{
    if ( !e.active )
        return;
    if ( e.real_time )
    {
        if ( !h.real_dt )
        {
            control_unrecreated( h );
            return;
        }
        dt = h.real_dt( h.user );
    }
    const int n = static_cast< int >( e.keys.size( ) );
    if ( n == 0 )
    {
        control_deactivate( e, h );
        return;
    }
    float eps;
    const std::uint32_t eb = 0x34000000u;   // 0x6F7584 FLT_EPSILON
    std::memcpy( &eps, &eb, 4 );
    const float dur = control_f( e.duration );
    if ( n == 1 || eps > dur )
    {
        control_apply( e, 0, h );
        control_deactivate( e, h );
        return;
    }
    float t = control_f( e.time );
    if ( t >= dur )
    {
        if ( !e.loop )
        {
            control_apply( e, n - 1, h );
            control_deactivate( e, h );
            if ( h.fire )
                h.fire( h.user, e, "OnDone" );
            return;
        }
        // t / dur, minus fistp( that - 0.5 ) (x87: round to nearest even; out of range = 0x80000000), times dur.
        const float q = t / dur;
        const double r = std::nearbyint( static_cast< double >( q ) - 0.5 );
        const std::int32_t i = r >= -2147483648.0 && r < 2147483648.0 ? static_cast< std::int32_t >( r ) : INT32_MIN;
        e.time = control_w( dur * ( q - static_cast< float >( i ) ) );
    }
    float x = control_f( e.time );
    if ( e.ease )
    {
        const float u = x / dur;
        const float a = u * 3.0f * u;
        const float b = u * 2.0f * u * u;
        x = ( a - b ) * dur;
    }
    control_interpolate( e, x, h );
    e.time = control_w( control_f( e.time ) + dt );
}

// "OnManualTick" 0x5CD710 (a VuParams: size +0x80, a type-2 first record's float
// at +4): the tick with that float, else with 0.
inline void control_manual_tick( control_entity_t& e, const std::uint8_t* params, std::int32_t size, const control_hooks_t& h )
{
    std::uint32_t v = 0, type = 0xFFFFFFFFu;
    if ( size > 0 )
    {
        std::memcpy( &type, params, 4 );
        if ( type == 2u )
            std::memcpy( &v, params + 4, 4 );
    }
    control_tick( e, control_f( v ), h );
}

// Slot 6 0x5CD490 (game initialize): each keyframe child's (time, child) pushed
// (growth as VuArray's), qsort'ed (0x5CCB60), rebased to the first, the duration
// = the last; "Motion" registered; with a linked ref +0x6C = its entity's
// VuControlComponent (or 0; unlinked: unchanged); Initially Active -> Activate.
inline void control_game_init( control_entity_t& e, bool linked, control_component_t* c, const control_hooks_t& h )
{
    for ( std::size_t i = 0; i < e.children.size( ); ++i )
    {
        if ( !e.children[ i ].keyframe )
            continue;
        const std::int32_t n = static_cast< std::int32_t >( e.keys.size( ) ) + 1;
        e.capacity = control_grow( n, e.capacity );
        e.keys.push_back( { e.children[ i ].frame.time, static_cast< int >( i ) } );
    }
    const int n = static_cast< int >( e.keys.size( ) );
    if ( n )
    {
        control_qsort( e.keys, n );
        const float t0 = control_f( e.keys[ 0 ].first );
        for ( int i = 0; i < n; ++i )
            e.keys[ i ].first = control_w( control_f( e.keys[ i ].first ) - t0 );
        e.duration = e.keys[ n - 1 ].first;
    }
    if ( h.tick_register )
        h.tick_register( h.user, e, true );
    if ( linked )
        e.target = c;
    if ( e.initially_active )
        control_activate( e, h );
}

// Slot 7 0x5CD660 (game release): Deactivate, "Motion" unregistered, the count 0.
inline void control_game_release( control_entity_t& e, const control_hooks_t& h )
{
    control_deactivate( e, h );
    if ( h.tick_register )
        h.tick_register( h.user, e, false );
    e.keys.clear( );
}

// Slot 8 0x5CD6A0: the one-shot latch cleared.
inline void control_reset( control_entity_t& e )
{
    e.done = 0;
}
