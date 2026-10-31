#pragma once

#include <workspace/core/trigger/volume.hxx>
#include <workspace/utility/logger.hxx>

#include <cmath>
#include <functional>
#include <string>
#include <vector>

// Every trigger volume a level authors, and the per-frame crossing test over
// them. The set knows nothing about what a crossing MEANS: it reports the
// path and the plug through the callback and the event graph does the rest.
class c_trigger_set
{
public:
    using fire_t = std::function< void( const std::string&, const std::string& ) >;

    void add( trigger_t&& gate ) { m_triggers.push_back( std::move( gate ) ); }
    void clear( )
    {
        m_triggers.clear( );
        m_tracked = false;
        m_driven = 0.0f;
        m_reported = 0.0f;
        m_ticks = 0;
        m_probes = 0;
    }
    std::size_t size( ) const { return m_triggers.size( ); }
    bool empty( ) const { return m_triggers.empty( ); }
    const std::vector< trigger_t >& all( ) const { return m_triggers; }

    // VuTriggerEntity's Activate / Deactivate plugs. Retail adds the trigger
    // to or removes it from the manager's list; a trigger that is not in the
    // list is never tested. Both guard on the current state, so a redundant
    // Activate does nothing.
    // Returns whether a volume of that path exists - MATCHED, not changed. The
    // caller consumes the event either way, exactly as the original does: a
    // redundant Activate is still the trigger's event to handle.
    bool set_active( const std::string& path, bool active )
    {
        bool matched = false;
        for ( trigger_t& gate : m_triggers )
        {
            if ( gate.path != path )
                continue;
            matched = true;
            gate.active = active;
        }
        return matched;
    }

    // Diagnostics. --aim names a plane to report local coordinates for on
    // close approach, --drive turns on the periodic travel report.
    void watch( const char* aim, bool drive )
    {
        m_aim = aim != nullptr ? aim : "";
        m_drive = drive;
    }
    float driven( ) const { return m_driven; }
    float probe_radius( ) const { return m_probe_radius; }

    // Fire the volumes the probe crosses. `at` is the world point under test:
    // retail passes the position of whichever VuBoatEntity the plane's Trigger
    // Type accepts, and that entity is not transliterated, so the caller
    // supplies the point and the test below is unchanged.
    void update( const float* probe, const fire_t& fire )
    {
        if ( probe == nullptr )
            return;
        const float at[ 3 ] = { probe[ 0 ], probe[ 1 ], probe[ 2 ] };
        if ( m_tracked )
        {
            const float dx = at[ 0 ] - m_last[ 0 ];
            const float dy = at[ 1 ] - m_last[ 1 ];
            m_driven += std::sqrt( dx * dx + dy * dy );
        }
        m_last[ 0 ] = at[ 0 ];
        m_last[ 1 ] = at[ 1 ];
        m_tracked = true;

        // Is the probe actually moving, and where is it relative to the
        // planes? Without this a silent "no events" is indistinguishable from
        // a broken test, a stationary probe, and a route that never reaches a
        // volume. Report on a FRAME counter, not on distance travelled:
        // gating the report on the thing being measured made "stopped moving"
        // look identical to "stopped reporting".
        if ( m_drive && ++m_ticks % 600 == 0 )
        {
            m_reported = m_driven;
            float nearest = 1.0e30f;
            const char* which = "";
            for ( const trigger_t& gate : m_triggers )
            {
                const float dx = gate.world[ 12 ] - at[ 0 ];
                const float dy = gate.world[ 13 ] - at[ 1 ];
                const float span = std::sqrt( dx * dx + dy * dy );
                if ( span < nearest )
                {
                    nearest = span;
                    which = gate.path.c_str( );
                }
            }
            LOG_INFO( "drive: %.0f units travelled, at %.0f %.0f, nearest plane %.0f away (%s)",
                m_driven, at[ 0 ], at[ 1 ], nearest, which );
        }

        for ( trigger_t& gate : m_triggers )
        {
            // The Trigger Type vocabulary is registered by
            // VuTriggerEntity_registerTriggerTypes at 0x4d9260, in this order:
            // "Human Boat", "AI Boat", "Boat", "Camera". First registered is
            // the enum's default, so a plane with no Trigger Type property
            // fires for the PLAYER. Only "Human Boat" and "Boat" accept the
            // player; "AI Boat" drives nuisance traffic and "Camera" is for
            // camera volumes.
            if ( !gate.active )
                continue;
            if ( !gate.kind.empty( ) && gate.kind != "Human Boat" && gate.kind != "Boat" )
                continue;

            float local[ 3 ];
            trigger_local( gate, at, local );
            if ( !m_aim.empty( ) && gate.path.find( m_aim ) != std::string::npos
                && std::fabs( local[ 1 ] ) < 3.0f && m_probes < 12 )
            {
                ++m_probes;
                LOG_INFO( "probe %s: local %.3f %.3f %.3f  (bound is |x|,|z| <= 1, sign flip on y)",
                    gate.path.c_str( ), local[ 0 ], local[ 1 ], local[ 2 ] );
                LOG_INFO( "   world probe %.1f %.1f %.1f  plane centre %.1f %.1f %.1f",
                    at[ 0 ], at[ 1 ], at[ 2 ],
                    gate.world[ 12 ], gate.world[ 13 ], gate.world[ 14 ] );
            }

            const float was = gate.side;
            gate.side = trigger_distance( gate, at, m_probe_radius );
            if ( !gate.primed )
            {
                gate.primed = true;
                continue;
            }
            // The real test is VuTriggerPlaneEntity_testCrossings @0x5d1570.
            // Reversed, it does exactly this shape:
            //
            //   d  = -dot(normal, centre)
            //   a  =  dot(normal, previousPosition) + d
            //   b  =  dot(normal, currentPosition)  + d
            //   if ( b * a <= 0 && (a > 0) != (b > 0) )        <- SIGN CHANGE
            //       t = dot(normal, centre - previous) / dot(normal, delta)
            //       hit = previous + delta * t
            //       fabs( dot(hit - centre, unitAxis) ) <= Scale
            //
            // So sign-change-across-the-step is CONFIRMED, and containment was
            // never how it works.
            if ( ( was < 0.0f ) == ( gate.side < 0.0f ) )
                continue;
            // The in-face bound belongs to the PLANE only: the box's distance
            // already accounts for all three axes.
            if ( !gate.box && ( local[ 0 ] < -1.0f || local[ 0 ] > 1.0f
                || local[ 2 ] < -1.0f || local[ 2 ] > 1.0f ) )
                continue;
            // Retail fires exactly ONE of the two plugs.
            // VuTriggerEntity_fireCrossing @0x61cb70 takes a bool and looks up
            // "OnEnter" or "OnExit", never both. The plane passes
            // `previousDistance > 0` and the box passes `currentDistance < 0`;
            // across a sign change those are the same predicate, so one line
            // serves both - a crossing INTO the negative side is an enter, out
            // of it an exit.
            fire( gate.path, gate.side < 0.0f ? "OnEnter" : "OnExit" );
        }
    }

    // Sweep a probe through each volume's centre along its own local Y. Every
    // one must register that crossing, and must reject a sweep displaced
    // sideways past the face. This exercises the world-to-local inverse and
    // the extent test without anything having to drive anywhere.
    //
    // Note what it CANNOT do: the probe offsets are in the same local units
    // the bound is expressed in, so it passes under either Scale convention
    // and proves consistency rather than correctness. The convention was
    // settled from the binary instead - see trigger_distance. The sideways
    // probe sits at 2.0, outside the +-1.0 face either way.
    void test_geometry( ) const
    {
        if ( m_triggers.empty( ) )
            return;
        int crossed = 0;
        int rejected = 0;
        for ( const trigger_t& gate : m_triggers )
        {
            auto probe = [ & ]( float lx, float ly, float* out )
            {
                const float* m = gate.world;
                out[ 0 ] = m[ 0 ] * lx + m[ 4 ] * ly + m[ 12 ];
                out[ 1 ] = m[ 1 ] * lx + m[ 5 ] * ly + m[ 13 ];
                out[ 2 ] = m[ 2 ] * lx + m[ 6 ] * ly + m[ 14 ];
            };
            // A box is solid, so a sweep that ends inside it is the crossing;
            // a plane is a surface, so the sweep has to pass through it.
            const float enters = gate.box ? 0.0f : 0.6f;
            auto fires = [ & ]( float lx )
            {
                float before[ 3 ]{}, after[ 3 ]{}, a[ 3 ]{}, b[ 3 ]{};
                probe( lx, gate.box ? -2.0f : -0.6f, before );
                probe( lx, enters, after );
                trigger_local( gate, before, a );
                trigger_local( gate, after, b );
                if ( gate.box )
                    return trigger_distance( gate, before, m_probe_radius ) > 0.0f
                        && trigger_distance( gate, after, m_probe_radius ) < 0.0f;
                if ( ( a[ 1 ] < 0.0f ) == ( b[ 1 ] < 0.0f ) )
                    return false;
                return !( b[ 0 ] < -1.0f || b[ 0 ] > 1.0f || b[ 2 ] < -1.0f || b[ 2 ] > 1.0f );
            };
            if ( fires( 0.0f ) )
                ++crossed;
            if ( !fires( 2.0f ) )
                ++rejected;
        }
        const int total = static_cast< int >( m_triggers.size( ) );
        if ( crossed == total && rejected == total )
        {
            LOG_SUCCESS( "trigger geometry: %d of %d planes register a centre crossing and reject an off-face sweep",
                crossed, total );
        }
        else
        {
            LOG_ERROR( "trigger geometry: %d of %d crossed, %d of %d rejected off-face - the volume maths is wrong",
                crossed, total, rejected, total );
        }
    }

private:
    std::vector< trigger_t > m_triggers;
    // The boat is a volume, not a point, and retail tests its collision shape
    // against the box. Carried as an explicit radius so the substitution is
    // visible rather than buried in the distance function.
    float m_probe_radius = 0.0f;

    std::string m_aim;
    bool m_drive = false;
    int m_probes = 0;
    int m_ticks = 0;
    float m_driven = 0.0f;
    float m_reported = 0.0f;
    float m_last[ 2 ]{};
    bool m_tracked = false;
};
