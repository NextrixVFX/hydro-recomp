#pragma once

// The world area's oracle-backed checks (one file per parallel agent; included by check.hxx after every
// shared check, so anything above is usable here). verify_world_all runs them all.
inline bool verify_world_all( )
{
    bool ok = true;
    return ok;
}
