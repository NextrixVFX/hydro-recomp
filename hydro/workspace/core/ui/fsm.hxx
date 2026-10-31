#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// VuFSM, the engine's finite state machine (the game modes own one; the front
// end's is at VuFrontEndGameMode +12). Transliterated from retail:
//   addState 0x555850 (state ctor 0x555CC0), addTransition 0x555920
//   (condition ctor 0x555E30, condition push 0x555EB0), evaluate 0x555AD0,
//   the first true transition 0x555BC0, state / condition lookups by FNV-1a
//   name hash 0x555C00 / 0x555C60.
// Layout it keeps (offsets from the FSM): +0 current state, +4 previous, +8
// next, +0xC the state vector, +0x18 the condition vector (32 bytes each),
// +0x24 the time in the current state (the owner adds dt; a transition zeroes
// it).

// FNV-1a over a C string, as every lookup above computes it (basis 0x811C9DC5,
// prime 0x1000193).
inline std::uint32_t fsm_hash( const char* name )
{
    std::uint32_t h = 0x811C9DC5u;
    for ( ; *name; ++name )
        h = ( h ^ static_cast< unsigned char >( *name ) ) * 0x1000193u;
    return h;
}

// A condition (0x20 bytes): +0 name, +24 hash, +28 value, +29 a second byte
// evaluate reads with it (as one WORD) and clears after every evaluate - a
// one-evaluate pulse. Nothing read so far sets +29.
struct fsm_condition_t
{
    std::string m_name;
    std::uint32_t m_hash = 0;
    std::uint8_t m_value = 0;
    std::uint8_t m_pulse = 0;
};

// A state (0x3C bytes): +0 name, +24 a flag (evaluate chains through states
// that carry it in one call; nothing read so far sets it), +28 hash, +32 enter
// / +36 exit / +40 tick (float) / +44 draw VuMethod delegates, +48 the
// transitions { condition index, target state index } in insertion order.
// m_address_* cite the retail member function each delegate binds (0 = none),
// for the checks.
struct fsm_state_t
{
    std::string m_name;
    bool m_flag = false;
    std::uint32_t m_hash = 0;
    std::function< void( ) > m_enter, m_exit, m_draw;
    std::function< void( float ) > m_tick;
    std::uint32_t m_address[ 4 ] = { };  // enter, exit, tick, draw
    std::vector< std::pair< int, int > > m_transitions;
};

class c_fsm
{
public:
    // addState 0x555850: a new state (0x555CC0: name, flag 0, FNV-1a hash) is
    // pushed; the first one becomes current - WITHOUT its enter (the owner
    // calls it when it starts the machine).
    fsm_state_t& add_state( const char* name )
    {
        auto s = std::make_unique< fsm_state_t >( );
        s->m_name = name;
        s->m_hash = fsm_hash( name );
        m_states.push_back( std::move( s ) );
        if ( m_current < 0 )
            m_current = static_cast< int >( m_states.size( ) ) - 1;
        return *m_states.back( );
    }

    // 0x555C00: the first state whose hash matches, else -1.
    int state_index( const char* name ) const
    {
        const std::uint32_t h = fsm_hash( name );
        for ( std::size_t k = 0; k < m_states.size( ); ++k )
            if ( m_states[ k ]->m_hash == h )
                return static_cast< int >( k );
        return -1;
    }

    // 0x555C60: the first condition whose hash matches, else -1.
    int condition_index( const char* name ) const
    {
        const std::uint32_t h = fsm_hash( name );
        for ( std::size_t k = 0; k < m_conditions.size( ); ++k )
            if ( m_conditions[ k ].m_hash == h )
                return static_cast< int >( k );
        return -1;
    }

    // addTransition 0x555920(from, to, condition): an unknown condition is
    // appended (0x555E30: value and pulse 0, then value 1 when the hash IS the
    // empty string's - so "" is always true; compared by hash, as retail
    // does). An unknown FROM state adds the transition to EVERY state.
    void add_transition( const char* from, const char* to, const char* condition )
    {
        const int f = state_index( from );
        const int t = state_index( to );
        int c = condition_index( condition );
        if ( c == -1 )
        {
            c = static_cast< int >( m_conditions.size( ) );
            fsm_condition_t made;
            made.m_name = condition;
            made.m_hash = fsm_hash( condition );
            if ( made.m_hash == 0x811C9DC5u )
                made.m_value = 1;
            m_conditions.push_back( std::move( made ) );
        }
        if ( f == -1 )
        {
            for ( auto& s : m_states )
                s->m_transitions.push_back( { c, t } );
        }
        else
            m_states[ f ]->m_transitions.push_back( { c, t } );
    }

    // The owner's condition write (VuFrontEndGameMode's tick 0x4BFC10 stores
    // the byte at conditions[index] +28). Retail does not test the index: a
    // name with no condition would write before the vector. Every caller here
    // names a condition its ctor added.
    void set_condition( const char* name, bool value )
    {
        m_conditions[ static_cast< std::size_t >( condition_index( name ) ) ].m_value = value ? 1 : 0;
    }

    // 0x555AD0: the current state's first transition whose condition is set
    // (value or pulse); while its target carries the +24 flag, move there and
    // look again from the new state (0x555BC0); then every pulse is cleared;
    // then a remaining target is moved to. A move: previous = current, next =
    // target, current's exit, current = target, its enter, time = 0.
    void evaluate( )
    {
        int i = first_true( );
        for ( ; i >= 0; i = first_true( ) )
        {
            if ( !m_states[ static_cast< std::size_t >( i ) ]->m_flag )
                break;
            move( i );
        }
        for ( auto& c : m_conditions )
            c.m_pulse = 0;
        if ( i >= 0 )
            move( i );
    }

    // What VuFrontEndGameMode's InitialScreen load 0x4BF630 ends with: current
    // = the first state, then its enter (the time is NOT reset).
    void start( )
    {
        m_current = m_states.empty( ) ? -1 : 0;
        if ( m_current >= 0 && m_states[ 0 ]->m_enter )
            m_states[ 0 ]->m_enter( );
    }

    fsm_state_t* current( ) const { return m_current < 0 ? nullptr : m_states[ static_cast< std::size_t >( m_current ) ].get( ); }

    std::vector< std::unique_ptr< fsm_state_t > > m_states;
    std::vector< fsm_condition_t > m_conditions;
    int m_current = -1;
    int m_previous = -1;
    int m_next = -1;
    float m_time = 0.0f;

private:
    // 0x555BC0 (and the same scan at the top of 0x555AD0).
    int first_true( ) const
    {
        const fsm_state_t* s = current( );
        for ( const auto& t : s->m_transitions )
        {
            const fsm_condition_t& c = m_conditions[ static_cast< std::size_t >( t.first ) ];
            if ( c.m_value | c.m_pulse )
                return t.second;
        }
        return -1;
    }

    void move( int target )
    {
        m_previous = m_current;
        m_next = target;
        if ( current( )->m_exit )
            current( )->m_exit( );
        m_current = target;
        if ( current( )->m_enter )
            current( )->m_enter( );
        m_time = 0.0f;
    }
};
