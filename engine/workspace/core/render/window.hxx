#pragma once

#include <impl/host.hxx>

#if HYDRO_HOST_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#endif

class c_window
{
public:
    c_window( ) = default;
    ~c_window( );

    // `background` keeps the window off the user's primary display and out of
    // the foreground: it is placed on a secondary monitor when one exists,
    // shown without activation, and kept out of the taskbar and alt-tab. A
    // headless bench run must never steal focus from whatever the user is
    // doing.
    bool create( int width, int height, const char* title, bool background = false );
    bool pump( );

    int width( ) const { return m_width; }
    int height( ) const { return m_height; }

#if HYDRO_HOST_WINDOWS
    HWND handle( ) const { return m_hwnd; }
#endif

private:
#if HYDRO_HOST_WINDOWS
    static LRESULT CALLBACK procedure( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam );
    LRESULT on_message( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam );

    HWND m_hwnd{};
#endif
    int  m_width{};
    int  m_height{};
    bool m_open{};
};

#include <workspace/utility/logger.hxx>

c_window::~c_window( )
{
#if HYDRO_HOST_WINDOWS
    if ( m_hwnd )
        DestroyWindow( m_hwnd );
#endif
}

#if HYDRO_HOST_WINDOWS
namespace
{
    struct monitor_pick_t
    {
        RECT secondary{};
        RECT primary{};
        bool found_secondary = false;
    };

    inline BOOL CALLBACK pick_monitor( HMONITOR monitor, HDC, LPRECT, LPARAM user )
    {
        MONITORINFO info{};
        info.cbSize = sizeof( info );
        if ( !GetMonitorInfoW( monitor, &info ) )
            return TRUE;

        auto* pick = reinterpret_cast< monitor_pick_t* >( user );
        if ( info.dwFlags & MONITORINFOF_PRIMARY )
            pick->primary = info.rcWork;
        else if ( !pick->found_secondary )
        {
            pick->secondary = info.rcWork;
            pick->found_secondary = true;
        }
        return TRUE;
    }
}
#endif

bool c_window::create( int width, int height, const char* title, bool background )
{
#if HYDRO_HOST_WINDOWS
    wchar_t wide_title[ 128 ]{};
    if ( !title || !title[ 0 ] )
        title = "engine";
    MultiByteToWideChar( CP_UTF8, 0, title, -1, wide_title, 128 );

    HINSTANCE instance = GetModuleHandleW( nullptr );

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof( window_class );
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = &c_window::procedure;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW( nullptr, IDC_ARROW );
    window_class.lpszClassName = L"hydro_window";

    if ( !RegisterClassExW( &window_class ) && GetLastError( ) != ERROR_CLASS_ALREADY_EXISTS )
    {
        LOG_ERROR_WIN32( "RegisterClassExW" );
        return false;
    }

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT bounds{ 0, 0, width, height };
    AdjustWindowRect( &bounds, style, FALSE );

    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    DWORD extended = 0;
    if ( background )
    {
        // WS_EX_NOACTIVATE stops the window taking focus on creation or click;
        // WS_EX_TOOLWINDOW keeps it out of the taskbar and the alt-tab list.
        extended = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;

        monitor_pick_t pick{};
        EnumDisplayMonitors( nullptr, nullptr, &pick_monitor, reinterpret_cast< LPARAM >( &pick ) );
        const RECT& area = pick.found_secondary ? pick.secondary : pick.primary;
        x = area.left;
        y = area.top;
        // With no second display, tuck it into the bottom-right corner rather
        // than landing in the middle of whatever the user is looking at.
        if ( !pick.found_secondary )
        {
            x = area.right - ( bounds.right - bounds.left );
            y = area.bottom - ( bounds.bottom - bounds.top );
        }
    }

    m_hwnd = CreateWindowExW(
        extended,
        window_class.lpszClassName,
        wide_title,
        style,
        x,
        y,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        nullptr,
        nullptr,
        instance,
        this
    );

    if ( !m_hwnd )
    {
        LOG_ERROR_WIN32( "CreateWindowExW" );
        return false;
    }

    m_width = width;
    m_height = height;
    m_open = true;
    // SW_SHOWNOACTIVATE leaves the caller's foreground window alone. Pairing it
    // with SWP_NOACTIVATE and HWND_BOTTOM keeps a bench run from ever surfacing
    // over whatever is in front.
    ShowWindow( m_hwnd, background ? SW_SHOWNOACTIVATE : SW_SHOW );
    if ( background )
        SetWindowPos( m_hwnd, HWND_BOTTOM, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
    UpdateWindow( m_hwnd );
    LOG_SUCCESS( "window %dx%d%s", m_width, m_height, background ? " (background)" : "" );
    return true;
#else
    ( void )width;
    ( void )height;
    ( void )title;
    ( void )background;
    LOG_ERROR( "window create is unimplemented on %s", host::name( ) );
    return false;
#endif
}

bool c_window::pump( )
{
#if HYDRO_HOST_WINDOWS
    MSG message{};
    while ( PeekMessageW( &message, nullptr, 0, 0, PM_REMOVE ) )
    {
        if ( message.message == WM_QUIT )
            m_open = false;

        TranslateMessage( &message );
        DispatchMessageW( &message );
    }
#endif
    return m_open;
}

#if HYDRO_HOST_WINDOWS
LRESULT CALLBACK c_window::procedure( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam )
{
    if ( message == WM_NCCREATE )
    {
        auto* created = reinterpret_cast< CREATESTRUCTW* >( lparam );
        SetWindowLongPtrW( hwnd, GWLP_USERDATA, reinterpret_cast< LONG_PTR >( created->lpCreateParams ) );
    }

    auto* window = reinterpret_cast< c_window* >( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );
    if ( window )
        return window->on_message( hwnd, message, wparam, lparam );

    return DefWindowProcW( hwnd, message, wparam, lparam );
}

LRESULT c_window::on_message( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam )
{
    if ( message == WM_DESTROY )
    {
        m_open = false;
        m_hwnd = nullptr;
        PostQuitMessage( 0 );
        return 0;
    }

    if ( message == WM_ERASEBKGND )
        return 1;

    return DefWindowProcW( hwnd, message, wparam, lparam );
}
#endif
