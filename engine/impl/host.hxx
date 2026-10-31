#pragma once

// Windows is the MSVC build. The same sources are meant to compile later
// for macOS and Linux; keep Win32 out of workspace code.

#if defined( _WIN32 )
#  define HYDRO_HOST_WINDOWS 1
#elif defined( __APPLE__ )
#  define HYDRO_HOST_APPLE 1
#elif defined( __linux__ )
#  define HYDRO_HOST_LINUX 1
#else
#  error unsupported host
#endif

namespace host
{
    inline const char* name( ) noexcept
    {
#if HYDRO_HOST_WINDOWS
        return "windows";
#elif HYDRO_HOST_APPLE
        return "macos";
#else
        return "linux";
#endif
    }
}
