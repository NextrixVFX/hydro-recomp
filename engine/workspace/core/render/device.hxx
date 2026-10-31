#pragma once

#include <impl/host.hxx>

#if HYDRO_HOST_WINDOWS
#  include <d3d11.h>
#endif

class c_device
{
public:
    c_device( ) = default;
    ~c_device( );

    bool create( );

#if HYDRO_HOST_WINDOWS
    ID3D11Device* device( ) const { return m_device; }
    ID3D11DeviceContext* context( ) const { return m_context; }
#endif

private:
#if HYDRO_HOST_WINDOWS
    ID3D11Device*        m_device{};
    ID3D11DeviceContext* m_context{};
#endif
};

c_device& g_device( );

#include <workspace/utility/logger.hxx>

#if HYDRO_HOST_WINDOWS
#  pragma comment( lib, "d3d11.lib" )
namespace
{
    c_device* g_device_instance = nullptr;
}
#endif

c_device& g_device( )
{
#if HYDRO_HOST_WINDOWS
    return *g_device_instance;
#else
    static c_device missing{};
    return missing;
#endif
}

c_device::~c_device( )
{
#if HYDRO_HOST_WINDOWS
    if ( g_device_instance == this )
        g_device_instance = nullptr;
    if ( m_context )
        m_context->Release( );
    if ( m_device )
        m_device->Release( );
#endif
}

bool c_device::create( )
{
#if HYDRO_HOST_WINDOWS
    UINT flags = 0;
#if defined( _DEBUG )
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL level{};
    HRESULT result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &m_device,
        &level,
        &m_context
    );

    if ( FAILED( result ) && ( flags & D3D11_CREATE_DEVICE_DEBUG ) )
    {
        result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &m_device,
            &level,
            &m_context
        );
    }

    if ( FAILED( result ) )
    {
        LOG_ERROR( "D3D11CreateDevice failed: 0x%08x", static_cast< unsigned >( result ) );
        return false;
    }

    g_device_instance = this;
    LOG_SUCCESS( "device feature level 0x%x", static_cast< unsigned >( level ) );
    return true;
#else
    LOG_ERROR( "device create is unimplemented on %s", host::name( ) );
    return false;
#endif
}
