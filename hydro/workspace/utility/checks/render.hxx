#pragma once

#include <workspace/core/world/skeleton.hxx>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// The render layer's CPU side against retail (water batch 30; tools oracle/cases/render.py, emit/render.py).

// The findNodeByName probes the case makes (cases/render.py anim_find_names): every bone name, then "", "NoSuchBone",
// bone 0's name + "x" and (when longer than one character) bone 0's name less its last character.
inline std::vector< std::string > anim_find_names( const skel_skeleton_t& s )
{
    std::vector< std::string > out;
    for ( int i = 0; i < s.count; ++i )
        out.emplace_back( s.names.data( ) + 32 * i );
    const std::string first = out.empty( ) ? std::string( ) : out[ 0 ];
    out.emplace_back( "" );
    out.emplace_back( "NoSuchBone" );
    out.push_back( first + "x" );
    if ( first.size( ) > 1 )
        out.push_back( first.substr( 0, first.size( ) - 1 ) );
    return out;
}

inline std::uint32_t anim_fnv( const void* data, std::size_t bytes )
{
    const std::uint8_t* p = static_cast< const std::uint8_t* >( data );
    std::uint32_t h = 0x811C9DC5u;
    for ( std::size_t i = 0; i < bytes; ++i )
        h = ( h ^ p[ i ] ) * 0x01000193u;
    return h;
}

struct anim_cursor_t
{
    const unsigned* w = nullptr;
    std::size_t at = 0;
    std::size_t size = 0;
    int compared = 0;
    int failed = 0;
    const char* where = "";
    unsigned take( ) { return at < size ? w[ at++ ] : 0u; }
    void same( const char* what, int index, std::uint32_t mine )
    {
        const std::uint32_t r = take( );
        ++compared;
        if ( r != mine )
        {
            if ( failed < 8 )
                LOG_ERROR( "anim_model %s: %s[%d] 0x%08X, retail 0x%08X", where, what, index, mine, r );
            ++failed;
        }
    }
    void floats( const char* what, const float* f, int count )
    {
        for ( int k = 0; k < count; ++k )
        {
            std::uint32_t b;
            std::memcpy( &b, f + k, 4 );
            same( what, k, b );
        }
    }
    // A pose array, 12 words per bone; `skip_w`: the position / scale w lanes (UNKNOWN) are skipped from bone `from`.
    void poses( const char* what, const skel_xform_t* x, int count, bool skip_w, int from )
    {
        for ( int b = 0; b < count; ++b )
        {
            const float* f = &x[ b ].pos[ 0 ];
            for ( int k = 0; k < 12; ++k )
            {
                if ( skip_w && b >= from && ( k == 3 || k == 11 ) )
                {
                    take( );
                    continue;
                }
                std::uint32_t v;
                std::memcpy( &v, f + k, 4 );
                same( what, 12 * b + k, v );
            }
        }
    }
    void box( const char* what, const float* aabb, bool w_lanes )   // x y z lanes; the w lanes only when known
    {
        for ( int k = 0; k < 8; ++k )
        {
            if ( !w_lanes && ( k == 3 || k == 7 ) )
            {
                take( );
                continue;
            }
            std::uint32_t v;
            std::memcpy( &v, aabb + k, 4 );
            same( what, k, v );
        }
    }
};

inline float anim_float( unsigned w )
{
    float f;
    std::memcpy( &f, &w, 4 );
    return f;
}

// Case anim_model: the shipped animated models and clips through world/skeleton.hxx against retail's own code -
// VuSkeleton 0x52F140 / 0x52F4C0 / 0x57FE80, findNodeByName 0x52F3C0, VuAnimation 0x562250, VuAnimationControl
// 0x5161F0, VuAnimatedSkeleton 0x516750 / 0x5173D0 / 0x5169E0 / 0x516CF0 (0x5802D0, 0x580AC0), VuAnimatedModelInstance
// 0x532360 / 0x532720 (0x57FAF0) / 0x532990. The asset bytes are read from Base.apf here, as the case read them.
inline bool verify_anim_model( )
{
    c_archive archive;
    const std::string pack = world_check_pack( );
    if ( pack.empty( ) || !archive.open( pack.c_str( ) ) )
    {
        LOG_ERROR( "anim_model: Base.apf did not open" );
        return false;
    }
    anim_cursor_t g;
    g.w = g_golden_anm_words;
    g.size = g_golden_anm_size;
    const unsigned models = g.take( );
    int steps_total = 0, bones_total = 0, events_total = 0;
    for ( unsigned mi = 0; mi < models; ++mi )
    {
        const unsigned n = g.take( );
        const unsigned skel_off = g.take( ), reader_end = g.take( );
        const char* model = g_golden_anm_names[ g.take( ) ];
        g.where = model;
        std::vector< std::uint8_t > blob;
        skel_model_asset_t asset;
        if ( !archive.read( ( std::string( "VuAnimatedModelAsset/" ) + model ).c_str( ), blob )
            || !skel_model_asset_load( blob.data( ), blob.size( ), asset ) )
        {
            LOG_ERROR( "anim_model: %s did not load", model );
            return false;
        }
        const skel_skeleton_t& s = asset.skeleton;
        // The bone count, where our scene read ends (= where retail's skeleton read starts) and where the skeleton
        // read ends.
        g.compared += 3;
        if ( static_cast< unsigned >( s.count ) != n || asset.skeleton_offset != skel_off || asset.reader_end != reader_end )
        {
            LOG_ERROR( "anim_model %s: bones %d / offset %zu / end %zu, retail %u / %u / %u", model, s.count,
                asset.skeleton_offset, asset.reader_end, n, skel_off, reader_end );
            return false;
        }
        const int count = s.count;
        bones_total += count;
        for ( int k = 0; k < 8 * count; ++k )
        {
            std::uint32_t v;
            std::memcpy( &v, s.names.data( ) + 4 * k, 4 );
            g.same( "names", k, v );
        }
        for ( int k = 0; k < count; ++k )
            g.same( "parents", k, static_cast< std::uint32_t >( s.parents[ static_cast< std::size_t >( k ) ] ) );
        g.poses( "model pose", s.model.data( ), count, false, 0 );
        g.poses( "local pose", s.local.data( ), count, true, 1 );
        g.floats( "inverse", s.inverse.data( ), 16 * count );
        g.floats( "skeleton aabb", s.aabb, 8 );
        // findNodeByName
        const std::vector< std::string > probes = anim_find_names( s );
        const unsigned finds = g.take( );
        if ( finds != probes.size( ) )
        {
            LOG_ERROR( "anim_model %s: %u probes, ours %zu", model, finds, probes.size( ) );
            return false;
        }
        for ( std::size_t k = 0; k < probes.size( ); ++k )
            g.same( "findNodeByName", static_cast< int >( k ), static_cast< std::uint32_t >( skel_find_node( s, probes[ k ].c_str( ) ) ) );
        // The instances.
        float scene_aabb[ 8 ];
        for ( int k = 0; k < 8; ++k )
            scene_aabb[ k ] = anim_float( g.take( ) );
        skel_instance_t A, B;
        skel_instance_init( A );
        skel_instance_init( B );
        skel_instance_set_model( A, s, scene_aabb );
        skel_instance_set_model( B, s, scene_aabb );
        g.floats( "setModel pad", &A.pad, 1 );
        g.floats( "setModel aabb", A.aabb, 8 );
        g.floats( "setModel matrices", A.matrices.data( ), 16 * count );
        g.floats( "setModel palette 0", A.palette[ 0 ].data( ), 16 * count );
        g.floats( "setModel palette 1", A.palette[ 1 ].data( ), 16 * count );
        skel_instance_set_pose( A, nullptr );
        g.floats( "setPose(null) matrices", A.matrices.data( ), 16 * count );
        g.floats( "setPose(null) palette", A.palette[ 0 ].data( ), 16 * count );
        g.floats( "setPose(null) root", A.root, 16 );
        // The animated skeleton and its controls.
        skel_animated_t animated;
        skel_animated_init( animated, s );
        g.poses( "animated init local", animated.local.data( ), count, true, 0 );
        g.box( "animated init aabb", animated.aabb, false );
        const unsigned clips = g.take( );
        std::vector< skel_animation_t > anims( clips );
        std::vector< skel_control_t > controls( clips );
        std::vector< std::vector< timed_event_t > > events( clips );
        for ( unsigned k = 0; k < clips; ++k )
        {
            const char* clip = g_golden_anm_names[ g.take( ) ];
            std::vector< std::uint8_t > cb;
            std::size_t end = 0;
            if ( !archive.read( ( std::string( "VuAnimationAsset/" ) + clip ).c_str( ), cb )
                || !skel_animation_load( cb.data( ), cb.size( ), anims[ k ], &end ) )
            {
                LOG_ERROR( "anim_model: %s did not load", clip );
                return false;
            }
            const skel_animation_t& a = anims[ k ];
            g.same( "clip bones", static_cast< int >( k ), static_cast< std::uint32_t >( a.bones ) );
            g.same( "clip frames", static_cast< int >( k ), static_cast< std::uint32_t >( a.frames ) );
            g.floats( "clip end", &a.end, 1 );
            g.floats( "clip loop", &a.loop, 1 );
            g.same( "clip additive", static_cast< int >( k ), a.additive );
            g.same( "clip reader end", static_cast< int >( k ), static_cast< std::uint32_t >( end ) );
            g.floats( "clip aabb", a.aabb, 8 );
            g.same( "clip data", static_cast< int >( k ), anim_fnv( a.data.data( ), 48u * a.data.size( ) ) );
            skel_control_t& c = controls[ k ];
            c.animation = &anims[ k ];
            c.looping = g.take( ) != 0;
            c.factor = anim_float( g.take( ) );
            c.time = anim_float( g.take( ) );
            c.weight = anim_float( g.take( ) );
            const unsigned ne = g.take( );
            for ( unsigned j = 0; j < ne; ++j )
            {
                timed_event_t e{ };
                e.time = anim_float( g.take( ) );
                e.name = "E" + std::to_string( j );
                events[ k ].push_back( e );
            }
            c.events = ne ? &events[ k ] : nullptr;
            c.listener = true;
            skel_animated_add( animated, &c );
            events_total += static_cast< int >( ne );
        }
        const unsigned steps = g.take( );
        for ( unsigned st = 0; st < steps; ++st )
        {
            const float dt = anim_float( g.take( ) );
            std::vector< std::pair< int, int > > fired;
            skel_animated_advance( animated, dt, [ & ]( int k, const std::string& name, const value_t* )
                {
                    const int code = name == "AnimDone" ? 0 : name == "AnimLoop" ? 1 : 2 + std::atoi( name.c_str( ) + 1 );
                    fired.emplace_back( k, code );
                } );
            for ( unsigned k = 0; k < clips; ++k )
                g.floats( "control time", &controls[ k ].time, 1 );
            const unsigned nf = g.take( );
            if ( nf != fired.size( ) )
            {
                LOG_ERROR( "anim_model %s step %u: %zu events fired, retail %u", model, st, fired.size( ), nf );
                ++g.failed;
                return false;
            }
            for ( unsigned k = 0; k < nf; ++k )
            {
                g.same( "fired control", static_cast< int >( k ), static_cast< std::uint32_t >( fired[ k ].first ) );
                g.same( "fired name", static_cast< int >( k ), static_cast< std::uint32_t >( fired[ k ].second ) );
            }
            if ( !skel_animated_build( animated ) || !skel_instance_set_pose( A, &animated ) )
            {
                LOG_ERROR( "anim_model %s step %u: a path read UNKNOWN memory", model, st );
                return false;
            }
            skel_instance_copy( B, A );
            g.poses( "built local", animated.local.data( ), count, true, 0 );
            g.box( "built aabb", animated.aabb, false );
            g.same( "A current", 0, static_cast< std::uint32_t >( A.current ) );
            g.same( "B current", 0, static_cast< std::uint32_t >( B.current ) );
            g.floats( "A matrices", A.matrices.data( ), 16 * count );
            g.floats( "A palette", A.palette[ A.current ].data( ), 16 * count );
            g.box( "A aabb", A.aabb, false );
            g.floats( "A root", A.root, 16 );
            g.box( "B aabb", B.aabb, false );
            g.floats( "B root", B.root, 16 );
            g.same( "B palette", 0, anim_fnv( B.palette[ B.current ].data( ), 64u * count ) );
            g.same( "B matrices", 0, anim_fnv( B.matrices.data( ), 64u * count ) );
            A.current = A.current == 0 ? 1 : 0;   // the Build tick's flips
            B.current = B.current == 0 ? 1 : 0;
            ++steps_total;
        }
    }
    if ( g.failed == 0 && g.at == g.size )
    {
        LOG_SUCCESS( "anim_model matches retail's animated model CPU side (VuSkeleton 0x52F140 / 0x52F4C0 / 0x57FE80, findNodeByName "
                     "0x52F3C0, VuAnimation 0x562250, VuAnimatedSkeleton 0x516750 / 0x5173D0 / advance 0x5169E0 / build 0x516CF0 "
                     "(0x5802D0, 0x580AC0), VuAnimatedModelInstance 0x532360 / setPose 0x532720 (0x57FAF0) / copy 0x532990) on %u "
                     "shipped models (%d bones), %d steps, %d timed events: %d words exact",
            models, bones_total, steps_total, events_total, g.compared );
        return true;
    }
    LOG_ERROR( "anim_model FAILED %d of %d words (cursor %zu of %zu)", g.failed, g.compared, g.at, g.size );
    return false;
}

inline bool verify_render_all( )
{
    bool ok = true;
    ok = verify_anim_model( ) && ok;
    return ok;
}
