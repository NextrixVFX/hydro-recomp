#include <impl/includes.hxx>

int main( int argc, char** argv )
{
    c_engine::arguments( argc, argv );
    // --verbose turns on LOG_VERBOSE. Nothing set this before, so every
    // verbose diagnostic in the tree - the entity census, the authored gfx
    // settings dump, the rotator and attach reports - was unreachable.
    g_verbose = c_engine::option( "verbose" ) != nullptr;
    // --selftest checks our maths against vectors captured by running the
    // retail functions, then exits. No window, no device, no assets.
    //
    // Both halves run before either verdict is used: short-circuiting would
    // hide a game-side failure behind an engine-side one, and a self-test that
    // reports less than it measured is worse than none.
    if ( c_engine::option( "selftest" ) != nullptr )
    {
#if HYDRO_GOLDEN
        const bool engine_ok = verify_all( );
        const bool game_ok = verify_game( );
        return engine_ok && game_ok ? 0 : 1;
#else
        LOG_ERROR( "--selftest needs the oracle's golden vectors (hydro-tools/oracle/golden.py); this build has none" );
        return 2;
#endif
    }
    c_engine::guard( );
    c_game game{};
    c_engine::attach( game );
    return c_engine::run( "Hydro" );
}
