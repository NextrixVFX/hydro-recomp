#pragma once

// VuCollisionManager's game side (g 0x758FBC; ctor 0x4ACDF0, vtables 0x745434
// and the contact-listener interface 0x74542C at +4 - its small slots are
// engine dynamics/wrapper.hxx's). The dynamics world tells the listener of
// every new contact; the listener keeps the ones a boat is in as impact
// records (+0x48 the array, +0x4C its count, +0x50 its capacity), skipping a
// weak one whose body pair / part pair is already in the scrape records
// (+0x54 / +0x58 / +0x5C). The tick 0x4AD970 plays the impacts (0x4ADA10) and
// the scrapes (0x4ADC00) and empties the arrays.

#include <workspace/core/boat/events.hxx>
#include <workspace/core/boat/meshes.hxx>
#include <workspace/core/boat/surface.hxx>
#include <workspace/core/world/pfxmgr.hxx>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <xmmintrin.h>

// The engine's VuArray<T> (data, count, capacity; _aligned_malloc 16). Its
// reserve, here for the 0x1C-byte records, is 0x4AE490 (thiscall(array, n),
// retn 4): when n > the capacity, a new block of n records, the count's
// records copied (memcpy_s), the old freed, capacity = n. `data` holds
// `capacity` records; the ones past `count` are unwritten.
template < typename T >
struct vu_array_t
{
    std::vector< T > data;
    std::int32_t count = 0;
    std::int32_t capacity = 0;
};

template < typename T >
inline void vu_array_reserve( vu_array_t< T >& a, std::int32_t n )
{
    if ( n <= a.capacity )
        return;
    std::vector< T > grown( static_cast< std::size_t >( n ) );
    for ( std::int32_t i = 0; i < a.count; ++i )
        grown[ static_cast< std::size_t >( i ) ] = a.data[ static_cast< std::size_t >( i ) ];
    a.data.swap( grown );
    a.capacity = n;
}

// An impact record (+0x48's element, 0x1C bytes).
struct collision_impact_t
{
    std::int32_t part_a;   // +0x00 the contact's +0x2C byte (zero-extended)
    std::int32_t part_b;   // +0x04 its +0x2D byte
    float point[ 4 ];      // +0x08 .. +0x17 the contact's +0xC .. +0x1B (16 bytes, the w lane too)
    float strength;        // +0x18 0x4AD340's answer
};

// A scrape record (+0x54's element, 0x1C bytes): the listener compares only the
// first four words; the rest is the scrape tick's (0x4ADC00 below).
struct collision_scrape_t
{
    const void* body_a;     // +0x00
    const void* body_b;     // +0x04
    std::int32_t part_a;    // +0x08
    std::int32_t part_b;    // +0x0C
    std::uint8_t alive;     // +0x10 cleared every tick, 1 once a contact point touched it
    std::uint8_t pad[ 3 ];  // +0x11 .. +0x13 never written
    std::uint32_t event;    // +0x14 the FMOD event (getEvent's out word)
    std::uint32_t handle;   // +0x18 the VuPfxManager handle (0 = none)
};

struct collision_manager_t
{
    vu_array_t< collision_impact_t > impacts;   // +0x48
    vu_array_t< collision_scrape_t > scrapes;   // +0x54
    // +8 / +0xC the two FMOD event groups 0x4AD970 fetches once (0 = not yet).
    std::uint32_t group_impact = 0, group_scrape = 0;
};

// The contact-added listener 0x4AD540 (interface slot 0; thiscall this =
// manager + 4; the contact*; retn 4). Contact: +0 body A, +4 body B, +0xC the
// point (16 bytes), +0x1C the normal, +0x2C / +0x2D the two part bytes. A
// body is a boat's when its +0x2CA short & 0x10; its owner entity is +0x2C0
// (`owner_a` / `owner_b`: those images, read only then) and the owner's
// +0x467 byte must be set for one of them - else nothing. s = 0x4AD340( A, B,
// point, normal ) (boat/events.hxx body_contact_strength); nothing when s ==
// 0 (ucomiss / lahf / test ah 0x44 / jnp: a NaN goes on). When 1 > s
// (`comiss; jbe`): nothing when a scrape record has (A, B, part A, part B).
// Then the record is appended: count + 1 > capacity -> reserve( max( count +
// 1, capacity + capacity / 2 ) ) (cdq / sar: toward zero).
inline void collision_contact_added( collision_manager_t& m, const std::uint8_t* a, const std::uint8_t* b, const std::uint8_t* owner_a,
                                     const std::uint8_t* owner_b, const float* point, const float* normal, std::uint8_t part_a, std::uint8_t part_b )
{
    std::uint16_t ga, gb;
    std::memcpy( &ga, a + 0x2CA, 2 );
    std::memcpy( &gb, b + 0x2CA, 2 );
    if ( ( ( ga | gb ) & 0x10u ) == 0u )
        return;
    std::uint8_t boat = 0;
    if ( ga & 0x10u )
        boat = owner_a[ 0x467 ];
    if ( gb & 0x10u )
        boat |= owner_b[ 0x467 ];
    if ( boat == 0 )
        return;
    const float s = body_contact_strength( a, b, point, normal );
    if ( s == 0.0f )
        return;
    if ( 1.0f > s )
        for ( std::int32_t i = 0; i < m.scrapes.count; ++i )
        {
            const collision_scrape_t& r = m.scrapes.data[ static_cast< std::size_t >( i ) ];
            if ( r.body_a == a && r.body_b == b && r.part_a == part_a && r.part_b == part_b )
                return;
        }
    const std::int32_t n = m.impacts.count + 1;
    if ( n > m.impacts.capacity )
    {
        const std::int32_t c = m.impacts.capacity;
        const std::int32_t grow = c + c / 2;
        vu_array_reserve( m.impacts, n > grow ? n : grow );
    }
    m.impacts.count = n;
    collision_impact_t& r = m.impacts.data[ static_cast< std::size_t >( n - 1 ) ];
    r.part_a = part_a;
    r.part_b = part_b;
    std::memcpy( r.point, point, 16 );
    r.strength = s;
}

// The surface-pair tables init 0x4AD110 loads from "SurfaceTableDB" (0x4AD690
// below): +0x14 the surface type count (g_vudynamicsimpl vt+0x4C), then four
// VuArray<char[64]> (data, count, capacity) - +0x18 "ImpactSfx", +0x24
// "ScrapeSfx", +0x30 "ImpactPfx" (prefix "Generic/Impact/"), +0x3C
// "ScrapePfx" ("Generic/Scrape/"); cell (a, b) = entry a x types + b.
using collision_name_t = std::array< char, 64 >;

struct collision_tables_t
{
    std::int32_t types = 0;
    vu_array_t< collision_name_t > impact_sfx, scrape_sfx, impact_pfx, scrape_pfx;
};

// VuArray<char[64]>::resize 0x4AE420 (thiscall(array, n), retn 4): when n >
// the capacity, capacity = max( n, capacity + capacity / 2 ) (a new block, the
// count's entries copied, the old freed); count = n.
inline void collision_names_resize( vu_array_t< collision_name_t >& a, std::int32_t n )
{
    if ( n > a.capacity )
    {
        const std::int32_t c = a.capacity;
        const std::int32_t grow = c + c / 2;
        vu_array_reserve( a, n > grow ? n : grow );
    }
    a.count = n;
}

// One table's load 0x4AD690 (thiscall manager; key, table, prefix; retn 0xC)
// on the asset's document (+0x58): resize to types x types, memset the count's
// cells to 0; t = document["VuDBAsset"][key] (0x4FBDF0). An element read
// (0x4FBC10) is the array's i-th for an array (tag 5) and 0 <= i < size, else
// the null container; a name is a string's (tag 4) text, else "". Row ids:
// for i < count( t ) (0x4FBC40: an array's size, else 0): ids_r[ i ] = the
// surface id (g_vudynamicsimpl vt+0x54 = boat/surface.hxx surface_id, a
// BYTE) of t[ i ][ 0 ] - the first element only when t[ i ] is an array with
// one or more; column ids: for j < count( t[ 0 ] ): ids_c[ j ] = the id of
// t[ 0 ][ j ]. Then for 1 <= r < count( t ), 1 <= c < count( t[ r ] ):
// strcpy_s( buf, 256, prefix ), strcat_s( buf, 256, t[ r ][ c ] ),
// strncpy_s( cell( ids_r[ r ], ids_c[ c ] ), 64, buf, 63 ) - at most 63
// characters and the NUL; the bytes after it keep what the cell held. NOT
// MODELLED: a prefix + name of 256 or more (strcat_s's invalid-parameter
// path) and more than 256 rows / columns (the two 256-byte id arrays on the
// stack overrun) - SurfaceTableDB has 16 x 16.
inline void collision_table_load( collision_tables_t& t, vu_array_t< collision_name_t >& table, const value_t& document, const char* key,
                                  const char* prefix, const surface_table_t& surfaces )
{
    collision_names_resize( table, t.types * t.types );
    for ( std::int32_t i = 0; i < table.count; ++i )
        table.data[ static_cast< std::size_t >( i ) ].fill( 0 );
    static const value_t null{};
    auto count = []( const value_t& v ) { return v.tag == 5 ? static_cast< std::int32_t >( v.list.size( ) ) : 0; };
    auto element = []( const value_t& v, std::int32_t i ) -> const value_t& {
        return v.tag == 5 && i >= 0 && i < static_cast< std::int32_t >( v.list.size( ) ) ? v.list[ static_cast< std::size_t >( i ) ] : null;
    };
    auto name = []( const value_t& v ) { return v.tag == 4 ? v.text.c_str( ) : ""; };
    const value_t& rows = member( member( document, "VuDBAsset" ), key );
    std::uint8_t ids_r[ 256 ] = {}, ids_c[ 256 ] = {};
    for ( std::int32_t i = 0; i < count( rows ) && i < 256; ++i )
    {
        const value_t& row = element( rows, i );
        const value_t& first = row.tag == 5 && !row.list.empty( ) ? row.list[ 0 ] : null;
        ids_r[ i ] = surface_id( surfaces, name( first ) );
    }
    const value_t& head = element( rows, 0 );
    for ( std::int32_t j = 0; j < count( head ) && j < 256; ++j )
        ids_c[ j ] = surface_id( surfaces, name( element( head, j ) ) );
    for ( std::int32_t r = 1; r < count( rows ) && r < 256; ++r )
    {
        const value_t& row = element( rows, r );
        for ( std::int32_t c = 1; c < count( row ) && c < 256; ++c )
        {
            const std::string buf = std::string( prefix ) + name( element( row, c ) );
            collision_name_t& cell = table.data[ static_cast< std::size_t >( ids_c[ c ] + t.types * ids_r[ r ] ) ];
            const std::size_t n = buf.size( ) < 63 ? buf.size( ) : 63;
            std::memcpy( cell.data( ), buf.data( ), n );
            cell[ n ] = '\0';
        }
    }
}

// The ctor 0x4ACDF0 (create 0x4ACD30: new 0x60, +8 .. +0x54 zeroed first):
// vtables 0x745434 / the listener's 0x74542C, +8 .. +0x14 zero, the four
// tables with capacity 8 (count 0), the impacts and the scrapes with capacity
// 8 then reserved to 32; then g 0x759020 vt+0x18 ( "CollisionManager", {50,
// 10, 40, 80} ) - the dev stats page registration (a dev hook, not made).
inline void collision_manager_construct( collision_manager_t& m, collision_tables_t& t )
{
    m = collision_manager_t( );
    t = collision_tables_t( );
    for ( vu_array_t< collision_name_t >* a : { &t.impact_sfx, &t.scrape_sfx, &t.impact_pfx, &t.scrape_pfx } )
        vu_array_reserve( *a, 8 );
    vu_array_reserve( m.impacts, 8 );
    vu_array_reserve( m.scrapes, 8 );
    vu_array_reserve( m.impacts, 32 );
    vu_array_reserve( m.scrapes, 32 );
}

// The init 0x4AD110 (thiscall manager -> true). HOOKS: g_vudynamicsimpl slot
// 13 (vt+0x34) addContactListener( manager + 4 ), the asset manager's load of
// "SurfaceTableDB" (0x4AE5A0 on g 0x759004; +0x10 = the asset, whose +0x58
// document `asset` answers), the tick manager's (g 0x759008 vt+0x20)
// registration of the tick 0x4AD970 in the "Decision" phase. +0x14 =
// g_vudynamicsimpl vt+0x4C = surface_count( surfaces ); the four loads in
// the order ImpactSfx, ScrapeSfx, ImpactPfx, ScrapePfx.
struct collision_init_hooks_t
{
    void* user = nullptr;
    void ( *add_listener )( void* user ) = nullptr;
    const value_t* ( *asset )( void* user, const char* name ) = nullptr;
    void ( *register_tick )( void* user, const char* phase ) = nullptr;
};

inline bool collision_manager_init( collision_tables_t& t, const surface_table_t& surfaces, const collision_init_hooks_t& h )
{
    if ( h.add_listener != nullptr )
        h.add_listener( h.user );
    t.types = surface_count( surfaces );
    static const value_t none{};
    const value_t* doc = h.asset != nullptr ? h.asset( h.user, "SurfaceTableDB" ) : nullptr;
    const value_t& d = doc != nullptr ? *doc : none;
    collision_table_load( t, t.impact_sfx, d, "ImpactSfx", "", surfaces );
    collision_table_load( t, t.scrape_sfx, d, "ScrapeSfx", "", surfaces );
    collision_table_load( t, t.impact_pfx, d, "ImpactPfx", "Generic/Impact/", surfaces );
    collision_table_load( t, t.scrape_pfx, d, "ScrapePfx", "Generic/Scrape/", surfaces );
    if ( h.register_tick != nullptr )
        h.register_tick( h.user, "Decision" );
    return true;
}

// The impact tick's outside world. AUDIO (platform: FMOD's event API, hooks):
// +8 is the "Global/Collisions/Collisions_Player" event group (0x4AD970 fetches
// it once through EventSystem::getGroup); `get_event` = its vt+0x20
// getEvent( name, 1, &event ) - false = a non-zero FMOD_RESULT; then
// set3DAttributes( event, &position, 0, 0 ), getParameter( event, "intensity",
// &p ) (false = failed) -> setValue( p, strength ), start( event ). THE PFX:
// g_vupfxmanager (pfxmgr.hxx: the manager, its ops) and the system instance's
// slots 10 (Duration, the answer discarded), 0 (position), 11 (start) - by
// default pfxsim.hxx's on `heap` (null hook = that default).
struct collision_impact_hooks_t
{
    void* user = nullptr;
    bool ( *get_event )( void* user, const char* name, std::uint32_t& event ) = nullptr;
    void ( *set_position )( void* user, std::uint32_t event, const float* xyz ) = nullptr;
    bool ( *get_parameter )( void* user, std::uint32_t event, const char* name, std::uint32_t& parameter ) = nullptr;
    void ( *set_value )( void* user, std::uint32_t parameter, float value ) = nullptr;
    void ( *start )( void* user, std::uint32_t event ) = nullptr;
    void ( *pfx_duration )( void* user, std::uint32_t instance ) = nullptr;
    void ( *pfx_position )( void* user, std::uint32_t instance, const float* xyz ) = nullptr;
    void ( *pfx_start )( void* user, std::uint32_t instance ) = nullptr;
    void ( *pfx_walk )( void* user, std::uint32_t instance ) = nullptr;   // slot 16 (0x401DA0: the instance itself) - told only
};

// The impact tick 0x4ADA10 (thiscall manager): per impact record, cell = part
// A x types + part B: the sound (ImpactSfx[cell]) at the record's point with
// "intensity" = the strength; then createEffect 0x52BCD0( ImpactPfx[cell] ) -
// on an entry: a new handle (the serial bump, 0x43C5E0's inline), the
// instance's slot 10, and when the handle is not 0 and still resolves (the
// lookup inline): slot 16, every process of a pattern whose node is a
// VuPfxEmitFountain (descriptor 0x7369BC up the chain) gets +0x1C (its count
// scale) = the strength, then slot 0 with the record's point, slot 11. The
// effect is fire-and-forget: the manager's Final tick releases it when its
// state reaches 0. Last: the count = 0.
inline void collision_impact_tick( collision_manager_t& m, const collision_tables_t& t, pfx_manager_t& pm, const pfx_manager_ops_t& ops,
                                   pfx_heap_t& heap, const collision_impact_hooks_t& k )
{
    for ( std::int32_t i = 0; i < m.impacts.count; ++i )
    {
        const collision_impact_t& r = m.impacts.data[ static_cast< std::size_t >( i ) ];
        const std::size_t cell = static_cast< std::size_t >( r.part_a * t.types + r.part_b );
        std::uint32_t event = 0;
        if ( k.get_event != nullptr && k.get_event( k.user, t.impact_sfx.data[ cell ].data( ), event ) )
        {
            if ( k.set_position != nullptr )
                k.set_position( k.user, event, r.point );
            std::uint32_t parameter = 0;
            if ( k.get_parameter != nullptr && k.get_parameter( k.user, event, "intensity", parameter ) && k.set_value != nullptr )
                k.set_value( k.user, parameter, r.strength );
            if ( k.start != nullptr )
                k.start( k.user, event );
        }
        const int e = pfx_manager_create( pm, t.impact_pfx.data[ cell ].data( ), ops );
        if ( e < 0 )
            continue;
        const std::uint32_t handle = pfx_manager_handle( pm, e );
        const std::uint32_t created = pm.entries[ static_cast< std::size_t >( e ) ].instance;
        if ( k.pfx_duration != nullptr )
            k.pfx_duration( k.user, created );
        else
            ( void )pfx_instance_duration( heap, created );
        if ( handle == 0u )
            continue;
        const int f = pfx_manager_lookup( pm, handle );
        if ( f < 0 )
            continue;
        const std::uint32_t inst = pm.entries[ static_cast< std::size_t >( f ) ].instance;
        if ( k.pfx_walk != nullptr )
            k.pfx_walk( k.user, inst );
        std::uint32_t bits;
        std::memcpy( &bits, &r.strength, 4 );
        pfx_system_bind( heap, inst, "VuPfxEmitFountain", 0x1C, bits );
        if ( k.pfx_position != nullptr )
            k.pfx_position( k.user, inst, r.point );
        else
            pfx_system_set_position( heap.find( inst ), reinterpret_cast< const std::uint32_t* >( r.point ) );
        if ( k.pfx_start != nullptr )
            k.pfx_start( k.user, inst );
        else
            pfx_instance_start( heap, inst );
    }
    m.impacts.count = 0;
}

// One persistent manifold of the dynamics world as the scrape tick reads it
// (btPersistentManifold: +0x10 the point cache, +0x490 / +0x494 the bodies,
// +0x498 the cached point count). The bodies are btCollisionObject images
// (+0x40 .. +0x48 the origin, +0xF4 the internal type, +0x140 .. the linear
// velocity, +0x150 .. the angular, byte +0x2C8 the surface id, short +0x2CA
// the group, +0x2C0 the owner entity - `owner0` / `owner1` its image, read
// only for a boat body: byte +0x467); `mesh0` / `mesh1` = the shape's (+0xD4)
// +8, a collision mesh or none. `points`: `contacts` btManifoldPoint images of
// 0x120 bytes (+0x20 / +0x30 the world points on B / A, +0x64 / +0x68 the
// triangle indices index0 / index1).
struct collision_manifold_t
{
    const std::uint8_t* body0 = nullptr;
    const std::uint8_t* body1 = nullptr;
    const std::uint8_t* owner0 = nullptr;
    const std::uint8_t* owner1 = nullptr;
    const collision_mesh_asset_t* mesh0 = nullptr;
    const collision_mesh_asset_t* mesh1 = nullptr;
    std::int32_t contacts = 0;
    const std::uint8_t* points = nullptr;
};

// The scrape tick's outside world. AUDIO (platform: FMOD's event API, hooks):
// `group` is the manager's +0xC "Global/Scrapes/Scrapes_Player" group;
// get_event = its vt+0x20 getEvent( name, 1, &record +0x14 ) - the answer is
// NOT tested: the hook writes the record's event word (it holds the slot's
// previous bytes on entry); set3DAttributes( event, &position, 0, 0 ),
// getParameter( event, "speed", &p ) (false = a non-zero FMOD_RESULT) ->
// setValue( p, speed ), start( event ), stop( event, immediate )
// (Event::stop - called with a 0 event too). THE PFX: g_vupfxmanager (the
// manager, its ops) and the system instance's slots 16 (walk: told only), 0
// (position), 11 (start), 12 (stop) - pfxsim.hxx's on `heap` when null.
struct collision_scrape_hooks_t
{
    void* user = nullptr;
    void ( *get_event )( void* user, std::uint32_t group, const char* name, std::uint32_t& event ) = nullptr;
    void ( *set_position )( void* user, std::uint32_t event, const float* xyz ) = nullptr;
    bool ( *get_parameter )( void* user, std::uint32_t event, const char* name, std::uint32_t& parameter ) = nullptr;
    void ( *set_value )( void* user, std::uint32_t parameter, float value ) = nullptr;
    void ( *start )( void* user, std::uint32_t event ) = nullptr;
    void ( *stop )( void* user, std::uint32_t event, bool immediate ) = nullptr;
    void ( *pfx_walk )( void* user, std::uint32_t instance ) = nullptr;
    void ( *pfx_position )( void* user, std::uint32_t instance, const float* xyz ) = nullptr;
    void ( *pfx_start )( void* user, std::uint32_t instance ) = nullptr;
    void ( *pfx_stop )( void* user, std::uint32_t instance, bool immediate ) = nullptr;
};

// A body's part for one contact point: its +0x2C8 surface byte, or - with a
// collision mesh - the surface id (byte +0x28) of material ( the triangle's
// u32 +0x70 [ index ] low byte & 0x1F ) in the mesh's +0x7C table (0x2C-byte
// entries). A read past the asset's arrays is UNKNOWN in retail (the index is
// Bullet's): counted in `unread`, the part left at the body's byte.
inline std::int32_t collision_scrape_part( const std::uint8_t* body, const collision_mesh_asset_t* mesh, std::int32_t index, int& unread )
{
    std::int32_t part = body[ 0x2C8 ];
    if ( mesh == nullptr )
        return part;
    const std::size_t at = static_cast< std::size_t >( static_cast< std::uint32_t >( index ) ) * 4u;
    if ( index < 0 || at >= mesh->triangles.bytes.size( ) )
    {
        ++unread;
        return part;
    }
    const std::size_t mat = static_cast< std::size_t >( mesh->triangles.bytes[ at ] & 0x1Fu ) * 0x2Cu + 0x28u;
    if ( mat >= mesh->materials.bytes.size( ) )
    {
        ++unread;
        return part;
    }
    return mesh->materials.bytes[ mat ];
}

// The scrape tick 0x4ADC00 (thiscall manager). Every record's +0x10 = 0. Then
// per manifold (g_vudynamicsimpl vt+0x6C -> +0x18 the dispatcher: vt+0x20
// count, vt+0x24 the i-th) with a nonzero point count, both bodies rigid
// (+0xF4 == 2), ( A +0x2CA | B +0x2CA ) & 0x10 and a boat owner's +0x467 set
// (as the listener), per contact point: the parts (collision_scrape_part with
// index0 / index1); the record with (A, B, part A, part B) - or a new one
// appended (growth max( n, cap + cap / 2 ), memcpy of the count's records):
// +0x10 = 0, the event from ScrapeSfx[ cell ], the handle from createEffect
// ( ScrapePfx[ cell ] ) inlined (0x52BCD0 + the serial bump; 0 when no free
// entry or no instance) - `fresh`. Speed = min( sqrt( |vA - vB|^2 ) x 0.02,
// 1.0 ) (minss: a NaN gives 1.0), v = linear + angular x ( point - origin ) at
// A's / B's world point; position = ( point on B + point on A ) x 0.5. With an
// event: set3DAttributes, "speed", start when fresh. When the handle resolves:
// slot 16, every VuPfxEmitFountain-derived process +0x20 (its rate scale) =
// speed, slot 0 position, slot 11 when fresh. Then +0x10 = 1. Last, every
// record still at +0x10 == 0: stop( event, false ), the handle's slot 12
// stop( false ), removal 0x4AE4F0 (swap with the last, count - 1; the swapped
// record is tested next). Returns the unknown mesh reads (0 = none).
inline int collision_scrape_tick( collision_manager_t& m, const collision_tables_t& t, const collision_manifold_t* manifolds, std::int32_t count,
                                  pfx_manager_t& pm, const pfx_manager_ops_t& ops, pfx_heap_t& heap, std::uint32_t resources,
                                  const collision_scrape_hooks_t& k )
{
    int unread = 0;
    for ( std::int32_t i = 0; i < m.scrapes.count; ++i )
        m.scrapes.data[ static_cast< std::size_t >( i ) ].alive = 0;
    auto f = []( const std::uint8_t* p, int at ) {
        float v;
        std::memcpy( &v, p + at, 4 );
        return v;
    };
    auto stop_pfx = [ & ]( std::uint32_t handle, bool immediate ) {
        const int e = pfx_manager_lookup( pm, handle );
        if ( e < 0 )
            return;
        const std::uint32_t inst = pm.entries[ static_cast< std::size_t >( e ) ].instance;
        if ( k.pfx_stop != nullptr )
            k.pfx_stop( k.user, inst, immediate );
        else
            pfx_instance_stop( heap, resources, inst, immediate );
    };
    for ( std::int32_t mi = 0; mi < count; ++mi )
    {
        const collision_manifold_t& mf = manifolds[ mi ];
        if ( mf.contacts == 0 )
            continue;
        const std::uint8_t* a = mf.body0;
        const std::uint8_t* b = mf.body1;
        std::int32_t ta, tb;
        std::memcpy( &ta, a + 0xF4, 4 );
        std::memcpy( &tb, b + 0xF4, 4 );
        if ( ta != 2 || tb != 2 )
            continue;
        std::uint16_t ga, gb;
        std::memcpy( &ga, a + 0x2CA, 2 );
        std::memcpy( &gb, b + 0x2CA, 2 );
        if ( ( ( ga | gb ) & 0x10u ) == 0u )
            continue;
        std::uint8_t boat = 0;
        if ( ga & 0x10u )
            boat = mf.owner0[ 0x467 ];
        if ( gb & 0x10u )
            boat |= mf.owner1[ 0x467 ];
        if ( boat == 0 || mf.contacts <= 0 )
            continue;
        for ( std::int32_t c = 0; c < mf.contacts; ++c )
        {
            const std::uint8_t* p = mf.points + 0x120 * static_cast< std::size_t >( c );
            std::int32_t index0, index1;
            std::memcpy( &index0, p + 0x64, 4 );
            std::memcpy( &index1, p + 0x68, 4 );
            const std::int32_t part_a = collision_scrape_part( a, mf.mesh0, index0, unread );
            const std::int32_t part_b = collision_scrape_part( b, mf.mesh1, index1, unread );
            std::int32_t at = 0;
            while ( at < m.scrapes.count )
            {
                const collision_scrape_t& r = m.scrapes.data[ static_cast< std::size_t >( at ) ];
                if ( r.body_a == a && r.body_b == b && r.part_a == part_a && r.part_b == part_b )
                    break;
                ++at;
            }
            bool fresh = false;
            if ( at == m.scrapes.count )
            {
                const std::int32_t n = m.scrapes.count + 1;
                if ( n > m.scrapes.capacity )
                {
                    const std::int32_t cap = m.scrapes.capacity;
                    const std::int32_t grow = cap + cap / 2;
                    vu_array_reserve( m.scrapes, n > grow ? n : grow );
                }
                m.scrapes.count = n;
                collision_scrape_t& r = m.scrapes.data[ static_cast< std::size_t >( n - 1 ) ];
                r.body_a = a;
                r.body_b = b;
                r.part_b = part_b;
                r.part_a = part_a;
                r.alive = 0;
                const std::size_t cell = static_cast< std::size_t >( t.types * part_a + part_b );
                if ( k.get_event != nullptr )
                    k.get_event( k.user, m.group_scrape, t.scrape_sfx.data[ cell ].data( ), r.event );
                const int e = pfx_manager_create( pm, t.scrape_pfx.data[ cell ].data( ), ops );
                r.handle = e < 0 ? 0u : pfx_manager_handle( pm, e );
                fresh = true;
            }
            collision_scrape_t& r = m.scrapes.data[ static_cast< std::size_t >( at ) ];
            // A's point velocity: linear + angular x ( point on A - origin ).
            const float ax = f( p, 0x30 ) - f( a, 0x40 ), ay = f( p, 0x34 ) - f( a, 0x44 ), az = f( p, 0x38 ) - f( a, 0x48 );
            const float wax = f( a, 0x150 ), way = f( a, 0x154 ), waz = f( a, 0x158 );
            const float vax = f( a, 0x140 ) + ( way * az - waz * ay );
            const float vay = f( a, 0x144 ) + ( waz * ax - wax * az );
            const float vaz = f( a, 0x148 ) + ( wax * ay - way * ax );
            // B's at the point on B.
            const float bx = f( p, 0x20 ) - f( b, 0x40 ), by = f( p, 0x24 ) - f( b, 0x44 ), bz = f( p, 0x28 ) - f( b, 0x48 );
            const float wbx = f( b, 0x150 ), wby = f( b, 0x154 ), wbz = f( b, 0x158 );
            const float vbx = f( b, 0x140 ) + ( bz * wby - by * wbz );
            const float vby = f( b, 0x144 ) + ( bx * wbz - bz * wbx );
            const float vbz = f( b, 0x148 ) + ( by * wbx - bx * wby );
            const float position[ 3 ] = { ( f( p, 0x20 ) + f( p, 0x30 ) ) * 0.5f, ( f( p, 0x24 ) + f( p, 0x34 ) ) * 0.5f,
                                          ( f( p, 0x28 ) + f( p, 0x38 ) ) * 0.5f };
            const float dx = vax - vbx, dy = vay - vby, dz = vaz - vbz;
            const float raw = _mm_cvtss_f32( _mm_sqrt_ss( _mm_set_ss( ( dy * dy + dx * dx ) + dz * dz ) ) ) * 0.02f;
            const float speed = _mm_cvtss_f32( _mm_min_ss( _mm_set_ss( raw ), _mm_set_ss( 1.0f ) ) );
            if ( r.event != 0u )
            {
                if ( k.set_position != nullptr )
                    k.set_position( k.user, r.event, position );
                std::uint32_t parameter = 0;
                if ( k.get_parameter != nullptr && k.get_parameter( k.user, r.event, "speed", parameter ) && k.set_value != nullptr )
                    k.set_value( k.user, parameter, speed );
                if ( fresh && k.start != nullptr )
                    k.start( k.user, r.event );
            }
            const int e = pfx_manager_lookup( pm, r.handle );
            if ( e >= 0 )
            {
                const std::uint32_t inst = pm.entries[ static_cast< std::size_t >( e ) ].instance;
                if ( k.pfx_walk != nullptr )
                    k.pfx_walk( k.user, inst );
                std::uint32_t bits;
                std::memcpy( &bits, &speed, 4 );
                pfx_system_bind( heap, inst, "VuPfxEmitFountain", 0x20, bits );
                if ( k.pfx_position != nullptr )
                    k.pfx_position( k.user, inst, position );
                else
                    pfx_system_set_position( heap.find( inst ), reinterpret_cast< const std::uint32_t* >( position ) );
                if ( fresh )
                {
                    if ( k.pfx_start != nullptr )
                        k.pfx_start( k.user, inst );
                    else
                        pfx_instance_start( heap, inst );
                }
            }
            r.alive = 1;
        }
    }
    for ( std::int32_t i = 0; i < m.scrapes.count; ++i )
    {
        collision_scrape_t& r = m.scrapes.data[ static_cast< std::size_t >( i ) ];
        if ( r.alive != 0 )
            continue;
        if ( k.stop != nullptr )
            k.stop( k.user, r.event, false );
        stop_pfx( r.handle, false );
        // 0x4AE4F0: swap with the last record, then count - 1 (its resize
        // never grows here).
        const std::size_t last = static_cast< std::size_t >( m.scrapes.count - 1 );
        if ( static_cast< std::size_t >( i ) + 1u < static_cast< std::size_t >( m.scrapes.count ) )
            std::swap( m.scrapes.data[ static_cast< std::size_t >( i ) ], m.scrapes.data[ last ] );
        --m.scrapes.count;
        --i;
    }
    return unread;
}

// The reset 0x4AD2B0 (thiscall manager; called by 0x482D50): the impact count
// = 0; per scrape record stop( event, true ) and the handle's slot 12
// stop( true ); the scrape count = 0 (the records' bytes stay).
inline void collision_manager_reset( collision_manager_t& m, pfx_manager_t& pm, pfx_heap_t& heap, std::uint32_t resources,
                                     const collision_scrape_hooks_t& k )
{
    m.impacts.count = 0;
    for ( std::int32_t i = 0; i < m.scrapes.count; ++i )
    {
        const collision_scrape_t& r = m.scrapes.data[ static_cast< std::size_t >( i ) ];
        if ( k.stop != nullptr )
            k.stop( k.user, r.event, true );
        const int e = pfx_manager_lookup( pm, r.handle );
        if ( e < 0 )
            continue;
        const std::uint32_t inst = pm.entries[ static_cast< std::size_t >( e ) ].instance;
        if ( k.pfx_stop != nullptr )
            k.pfx_stop( k.user, inst, true );
        else
            pfx_instance_stop( heap, resources, inst, true );
    }
    m.scrapes.count = 0;
}

// The tick 0x4AD970 (thiscall manager, dt unread; retn 4; registered in the
// "Decision" phase by the init): +8 == 0 -> EventSystem::getGroup(
// "Global/Collisions/Collisions_Player", true, &+8 ), +0xC == 0 -> getGroup(
// "Global/Scrapes/Scrapes_Player", true, &+0xC ) (the answers not tested:
// `get_group` writes the word); the impact tick; the scrape tick; then the dev
// stats page (g 0x759020 vt+0x20: when its current page is "CollisionManager",
// clear + "Manifolds: %d\n" with the SCRAPE count) - `dev_page`, a dev hook.
// Returns the scrape tick's unknown reads.
struct collision_tick_hooks_t
{
    void* user = nullptr;
    void ( *get_group )( void* user, const char* name, std::uint32_t& group ) = nullptr;
    void ( *dev_page )( void* user, std::int32_t scrapes ) = nullptr;
};

inline int collision_manager_tick( collision_manager_t& m, const collision_tables_t& t, const collision_manifold_t* manifolds, std::int32_t count,
                                   pfx_manager_t& pm, const pfx_manager_ops_t& ops, pfx_heap_t& heap, std::uint32_t resources,
                                   const collision_impact_hooks_t& impact, const collision_scrape_hooks_t& scrape, const collision_tick_hooks_t& h )
{
    if ( m.group_impact == 0u && h.get_group != nullptr )
        h.get_group( h.user, "Global/Collisions/Collisions_Player", m.group_impact );
    if ( m.group_scrape == 0u && h.get_group != nullptr )
        h.get_group( h.user, "Global/Scrapes/Scrapes_Player", m.group_scrape );
    collision_impact_tick( m, t, pm, ops, heap, impact );
    const int unread = collision_scrape_tick( m, t, manifolds, count, pm, ops, heap, resources, scrape );
    if ( h.dev_page != nullptr )
        h.dev_page( h.user, m.scrapes.count );
    return unread;
}
