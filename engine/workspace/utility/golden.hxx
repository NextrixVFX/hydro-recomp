#pragma once

// The oracle's golden vectors - retail's own output, captured by running the
// shipped functions under emulation (hydro-tools/oracle) - are NOT part of
// this repository: they embed real asset bytes (ConstantDB, level Properties,
// collision BVHs) from the user's own install. hydro-tools/oracle/golden.py
// generates them into hydro-tools/oracle/generated/, which the projects put on
// the include path (HydroGoldenDir). Without them the build still succeeds and
// --selftest reports the oracle checks as skipped.
#if __has_include( <golden_data.hxx> )
#include <golden_data.hxx>
#define HYDRO_GOLDEN 1
#else
#define HYDRO_GOLDEN 0
#endif
