#pragma once

#include <impl/engine.hxx>
#include <impl/host.hxx>
#include <workspace/utility/logger.hxx>
#include <workspace/utility/trace.hxx>
#include <workspace/core/render/device.hxx>
#include <workspace/core/render/swap.hxx>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Unattended run harness. Presents with no sync interval so the frame times
// are the renderer's own cost rather than the display cadence, grabs the
// backbuffer at scheduled moments, and fails the process when the frame rate
// floor is broken. Without --bench none of this is live and run() behaves as
// it always did.
//
//   hydro.exe --bench --seconds 20 --fps-floor 240 --shots 2,5,10,15
//             --race Hydroelectric --out <dir> --warmup 2
class c_bench
{
public:
    void configure( )
    {
        m_active = c_engine::option( "bench" ) != nullptr;
        if ( !m_active )
            return;

        m_seconds = number( "seconds", 20.0f );
        m_warmup = number( "warmup", 2.0f );
        m_floor = number( "fps-floor", 240.0f );

        const char* out = c_engine::option( "out" );
        m_out = out != nullptr ? out : "bench";
        make_directory( m_out );

        const char* shots = c_engine::option( "shots" );
        if ( shots == nullptr )
            shots = "3,8,15";
        parse_shots( shots );

        // --still <n,n,...> captures at exact FRAME INDICES instead of at
        // elapsed seconds, and --frames <n> ends the run on a frame count
        // rather than a stopwatch. Paired with --fixed-step this makes a
        // capture reproducible, which a time-triggered shot can never be:
        // the frame it lands on depends on how fast the machine happened to
        // be running.
        if ( const char* still = c_engine::option( "still" ) )
        {
            m_still = true;
            parse_frames( still );
        }
        m_limit = static_cast< long >( number( "frames", 0.0f ) );

        LOG_INFO( "bench %.1fs warmup %.1fs floor %.0f fps shots %u out %s",
            m_seconds, m_warmup, m_floor,
            static_cast< unsigned >( m_shots.size( ) ), m_out.c_str( ) );
        if ( m_still )
            LOG_INFO( "bench still frames %u limit %ld",
                static_cast< unsigned >( m_frames_at.size( ) ), m_limit );
    }

    bool active( ) const { return m_active; }

    // Called once per frame after present. Returns false when the run is over.
    bool step( )
    {
        if ( !m_active )
            return true;

        ++m_frame;
        const double time = trace::now( );
        if ( m_last == 0.0 )
        {
            m_last = time;
            m_start = time;
            return true;
        }
        const double delta = time - m_last;
        m_last = time;

        const float elapsed = static_cast< float >( ( time - m_start ) / 1000.0 );

        // Warmup covers first-frame shader compiles, asset streaming and the
        // driver settling; those frames are real but they are not what the
        // floor is about.
        // A capture stalls the pipeline on CopyResource and Map and costs
        // several milliseconds. That cost lands in the following frame's
        // delta, so it is dropped rather than reported as a renderer hitch.
        const bool tainted = m_tainted;
        m_tainted = false;

        if ( elapsed >= m_warmup && delta > 0.0 && !tainted )
        {
            m_frames.push_back( delta );
            if ( delta > m_worst )
            {
                m_worst = delta;
                m_worst_at = elapsed;
                m_worst_parts = trace::previous( );
            }
            // Keep the breakdown for every frame that misses budget. One
            // isolated stall with no subsystem accounting for it is the
            // scheduler; a repeated one with the same culprit is ours.
            if ( delta > 1000.0 / static_cast< double >( m_floor ) && m_slow.size( ) < 16 )
            {
                slow_t entry{};
                entry.ms = delta;
                entry.at = elapsed;
                entry.parts = trace::previous( );
                m_slow.push_back( entry );
            }
        }

        if ( m_still )
        {
            while ( m_next < m_frames_at.size( ) && m_frame >= m_frames_at[ m_next ] )
            {
                char name[ 256 ]{};
                std::snprintf( name, sizeof( name ), "%s/still_%02u_%ldf.bmp",
                    m_out.c_str( ), static_cast< unsigned >( m_next ), m_frames_at[ m_next ] );
                capture( name );
                m_tainted = true;
                ++m_next;
            }
        }
        else
        {
            while ( m_next < m_shots.size( ) && elapsed >= m_shots[ m_next ] )
            {
                char name[ 256 ]{};
                std::snprintf( name, sizeof( name ), "%s/shot_%02u_%.0fs.bmp",
                    m_out.c_str( ), static_cast< unsigned >( m_next ), m_shots[ m_next ] );
                capture( name );
                m_tainted = true;
                ++m_next;
            }
        }

        if ( m_limit > 0 )
            return m_frame < m_limit;
        return elapsed < m_seconds;
    }

    // Writes the report and returns the process exit code.
    int report( )
    {
        if ( !m_active )
            return 0;
        if ( m_frames.empty( ) )
        {
            // A --still run can be shorter than the warmup, so it legitimately
            // has no timing samples. That is not a failure - it is a capture
            // run, not a measurement run. Any other empty run still fails.
            if ( m_still )
            {
                LOG_INFO( "bench still %ld frames captured %u - no timing collected",
                    m_frame, static_cast< unsigned >( m_next ) );
                return 0;
            }
            LOG_ERROR( "bench collected no frames" );
            return 1;
        }

        std::vector< double > sorted = m_frames;
        std::sort( sorted.begin( ), sorted.end( ) );

        double total = 0.0;
        int under = 0;
        const double budget = 1000.0 / static_cast< double >( m_floor );
        for ( double value : m_frames )
        {
            total += value;
            if ( value > budget )
                ++under;
        }

        const std::size_t count = sorted.size( );
        const double average = total / static_cast< double >( count );
        const double best = sorted.front( );
        const double worst = sorted.back( );
        const double median = sorted[ count / 2 ];
        const double p99 = sorted[ static_cast< std::size_t >( static_cast< double >( count ) * 0.99 ) ];
        const double p999 = sorted[ static_cast< std::size_t >( static_cast< double >( count ) * 0.999 ) ];

        LOG_INFO( "bench frames %u over %.1fs", static_cast< unsigned >( count ), m_seconds - m_warmup );
        LOG_INFO( "bench ms   avg %.3f  median %.3f  p99 %.3f  p99.9 %.3f  best %.3f  worst %.3f",
            average, median, p99, p999, best, worst );
        LOG_INFO( "bench fps  avg %.0f  median %.0f  p99 %.0f  p99.9 %.0f  peak %.0f  low %.0f",
            1000.0 / average, 1000.0 / median, 1000.0 / p99, 1000.0 / p999,
            1000.0 / best, 1000.0 / worst );
        LOG_INFO( "bench worst frame %.3f ms at t=%.2fs", m_worst, m_worst_at );
        LOG_INFO( "bench worst parts  ocean %.3f mesh %.3f upload %.3f shade %.3f pfx %.3f props %.3f ensure %.3f draw %.3f present %.3f gpu %.3f",
            m_worst_parts.ocean, m_worst_parts.mesh, m_worst_parts.upload, m_worst_parts.shade,
            m_worst_parts.pfx, m_worst_parts.props,
            m_worst_parts.ensure, m_worst_parts.draw, m_worst_parts.present,
            m_worst_parts.gpu );
        const double accounted =
            m_worst_parts.ocean + m_worst_parts.mesh + m_worst_parts.upload + m_worst_parts.shade +
            m_worst_parts.pfx + m_worst_parts.props + m_worst_parts.ensure +
            m_worst_parts.draw + m_worst_parts.present;
        LOG_INFO( "bench worst accounted %.3f of %.3f ms (%.0f%%) - the remainder is outside our timers",
            accounted, m_worst, m_worst > 0.0 ? 100.0 * accounted / m_worst : 0.0 );
        for ( const slow_t& entry : m_slow )
        {
            const double parts =
                entry.parts.ocean + entry.parts.mesh + entry.parts.upload + entry.parts.shade +
                entry.parts.pfx + entry.parts.ensure + entry.parts.draw + entry.parts.present;
            LOG_INFO( "bench slow t=%.2fs %.3f ms  accounted %.3f  ocean %.3f mesh %.3f upload %.3f pfx %.3f draw %.3f present %.3f draws %d tex %d",
                entry.at, entry.ms, parts, entry.parts.ocean, entry.parts.mesh, entry.parts.upload,
                entry.parts.pfx, entry.parts.draw, entry.parts.present,
                entry.parts.draws, entry.parts.textures );
        }

        const double percent = 100.0 * static_cast< double >( under ) / static_cast< double >( count );
        char path[ 512 ]{};
        std::snprintf( path, sizeof( path ), "%s/bench.txt", m_out.c_str( ) );
        FILE* file = nullptr;
        if ( fopen_s( &file, path, "w" ) == 0 && file != nullptr )
        {
            std::fprintf( file, "frames %u\n", static_cast< unsigned >( count ) );
            std::fprintf( file, "floor_fps %.0f\n", m_floor );
            std::fprintf( file, "budget_ms %.4f\n", budget );
            std::fprintf( file, "avg_ms %.4f\nmedian_ms %.4f\np99_ms %.4f\np999_ms %.4f\n",
                average, median, p99, p999 );
            std::fprintf( file, "best_ms %.4f\nworst_ms %.4f\nworst_at_s %.2f\n", best, worst, m_worst_at );
            std::fprintf( file, "avg_fps %.1f\nlow_fps %.1f\n", 1000.0 / average, 1000.0 / worst );
            std::fprintf( file, "frames_under_floor %d\nunder_floor_percent %.3f\n", under, percent );
            std::fprintf( file, "worst_parts ocean %.4f mesh %.4f upload %.4f shade %.4f pfx %.4f props %.4f ensure %.4f draw %.4f present %.4f gpu %.4f\n",
                m_worst_parts.ocean, m_worst_parts.mesh, m_worst_parts.upload, m_worst_parts.shade,
                m_worst_parts.pfx, m_worst_parts.props, m_worst_parts.ensure,
                m_worst_parts.draw, m_worst_parts.present, m_worst_parts.gpu );
            std::fprintf( file, "props_ms %.4f\n", m_worst_parts.props );
            for ( const slow_t& entry : m_slow )
            {
                const double parts =
                    entry.parts.ocean + entry.parts.mesh + entry.parts.upload + entry.parts.shade +
                    entry.parts.pfx + entry.parts.ensure + entry.parts.draw + entry.parts.present;
                std::fprintf( file, "slow at %.2f ms %.4f accounted %.4f ocean %.4f mesh %.4f upload %.4f pfx %.4f draw %.4f present %.4f draws %d tex %d\n",
                    entry.at, entry.ms, parts, entry.parts.ocean, entry.parts.mesh, entry.parts.upload,
                    entry.parts.pfx, entry.parts.draw, entry.parts.present,
                    entry.parts.draws, entry.parts.textures );
            }
            std::fclose( file );
        }

        if ( under > 0 )
        {
            LOG_ERROR( "bench FAIL  %d of %u frames (%.2f%%) exceeded the %.3f ms budget for %.0f fps",
                under, static_cast< unsigned >( count ), percent, budget, m_floor );
            return 1;
        }

        LOG_SUCCESS( "bench PASS  every frame inside %.3f ms (%.0f fps floor)", budget, m_floor );
        return 0;
    }

    // Backbuffer grab. Staging copy, then a 32bpp bottom-up BMP, which needs
    // no encoder and opens anywhere on Windows.
    static bool capture( const char* path )
    {
#if HYDRO_HOST_WINDOWS
        IDXGISwapChain1* chain = g_swap( ).chain( );
        ID3D11Device* device = g_device( ).device( );
        ID3D11DeviceContext* context = g_device( ).context( );
        if ( chain == nullptr || device == nullptr || context == nullptr )
            return false;

        ID3D11Texture2D* back = nullptr;
        if ( FAILED( chain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast< void** >( &back ) ) ) )
            return false;

        D3D11_TEXTURE2D_DESC description{};
        back->GetDesc( &description );
        D3D11_TEXTURE2D_DESC staging = description;
        staging.Usage = D3D11_USAGE_STAGING;
        staging.BindFlags = 0;
        staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        staging.MiscFlags = 0;

        ID3D11Texture2D* copy = nullptr;
        if ( FAILED( device->CreateTexture2D( &staging, nullptr, &copy ) ) )
        {
            back->Release( );
            return false;
        }
        context->CopyResource( copy, back );
        back->Release( );

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if ( FAILED( context->Map( copy, 0, D3D11_MAP_READ, 0, &mapped ) ) )
        {
            copy->Release( );
            return false;
        }

        const int width = static_cast< int >( description.Width );
        const int height = static_cast< int >( description.Height );
        const int stride = width * 4;

        std::vector< unsigned char > rows( static_cast< std::size_t >( stride ) * height );
        for ( int y = 0; y < height; ++y )
        {
            const unsigned char* source =
                static_cast< const unsigned char* >( mapped.pData ) + static_cast< std::size_t >( y ) * mapped.RowPitch;
            // BMP rows run bottom-up, and the backbuffer is RGBA while BMP
            // wants BGRA.
            unsigned char* target = rows.data( ) + static_cast< std::size_t >( height - 1 - y ) * stride;
            for ( int x = 0; x < width; ++x )
            {
                target[ x * 4 + 0 ] = source[ x * 4 + 2 ];
                target[ x * 4 + 1 ] = source[ x * 4 + 1 ];
                target[ x * 4 + 2 ] = source[ x * 4 + 0 ];
                target[ x * 4 + 3 ] = 255;
            }
        }
        context->Unmap( copy, 0 );
        copy->Release( );

        FILE* file = nullptr;
        if ( fopen_s( &file, path, "wb" ) != 0 || file == nullptr )
        {
            LOG_ERROR( "capture could not open %s", path );
            return false;
        }

        const unsigned pixels = static_cast< unsigned >( stride ) * static_cast< unsigned >( height );
        const unsigned offset = 14u + 40u;
        unsigned char header[ 54 ]{};
        header[ 0 ] = 'B';
        header[ 1 ] = 'M';
        put32( header + 2, offset + pixels );
        put32( header + 10, offset );
        put32( header + 14, 40 );
        put32( header + 18, static_cast< unsigned >( width ) );
        put32( header + 22, static_cast< unsigned >( height ) );
        header[ 26 ] = 1;
        header[ 28 ] = 32;
        put32( header + 34, pixels );
        std::fwrite( header, 1, sizeof( header ), file );
        std::fwrite( rows.data( ), 1, rows.size( ), file );
        std::fclose( file );
        LOG_SUCCESS( "capture %s  %dx%d", path, width, height );
        return true;
#else
        ( void )path;
        return false;
#endif
    }

private:
    static void put32( unsigned char* target, unsigned value )
    {
        target[ 0 ] = static_cast< unsigned char >( value & 0xFF );
        target[ 1 ] = static_cast< unsigned char >( ( value >> 8 ) & 0xFF );
        target[ 2 ] = static_cast< unsigned char >( ( value >> 16 ) & 0xFF );
        target[ 3 ] = static_cast< unsigned char >( ( value >> 24 ) & 0xFF );
    }

    static float number( const char* name, float fallback )
    {
        const char* text = c_engine::option( name );
        if ( text == nullptr || text[ 0 ] == '\0' )
            return fallback;
        return static_cast< float >( std::atof( text ) );
    }

    static void make_directory( const std::string& path )
    {
#if HYDRO_HOST_WINDOWS
        CreateDirectoryA( path.c_str( ), nullptr );
#else
        ( void )path;
#endif
    }

    void parse_frames( const char* text )
    {
        std::string item;
        for ( const char* cursor = text;; ++cursor )
        {
            if ( *cursor == ',' || *cursor == '\0' )
            {
                if ( !item.empty( ) )
                    m_frames_at.push_back( std::atol( item.c_str( ) ) );
                item.clear( );
                if ( *cursor == '\0' )
                    break;
                continue;
            }
            item.push_back( *cursor );
        }
        std::sort( m_frames_at.begin( ), m_frames_at.end( ) );
    }

    void parse_shots( const char* text )
    {
        std::string item;
        for ( const char* cursor = text;; ++cursor )
        {
            if ( *cursor == ',' || *cursor == '\0' )
            {
                if ( !item.empty( ) )
                    m_shots.push_back( static_cast< float >( std::atof( item.c_str( ) ) ) );
                item.clear( );
                if ( *cursor == '\0' )
                    break;
                continue;
            }
            item.push_back( *cursor );
        }
        std::sort( m_shots.begin( ), m_shots.end( ) );
    }

    bool m_active = false;
    float m_seconds = 20.0f;
    float m_warmup = 2.0f;
    float m_floor = 240.0f;
    std::string m_out = "bench";
    std::vector< float > m_shots;
    std::vector< long > m_frames_at;
    bool m_still = false;
    long m_frame = 0;
    long m_limit = 0;
    std::vector< double > m_frames;
    std::size_t m_next = 0;
    double m_last = 0.0;
    double m_start = 0.0;
    double m_worst = 0.0;
    float m_worst_at = 0.0f;
    bool m_tainted = false;

    struct slow_t
    {
        double ms = 0.0;
        float at = 0.0f;
        trace::frame_t parts{};
    };
    trace::frame_t m_worst_parts{};
    std::vector< slow_t > m_slow;
};

inline c_bench& g_bench( )
{
    static c_bench instance{};
    return instance;
}
