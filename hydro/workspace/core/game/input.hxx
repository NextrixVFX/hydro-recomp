#pragma once

#if defined( _WIN32 )
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#  include <Xinput.h>
#  pragma comment( lib, "xinput.lib" )
#endif

struct input_t
{
    float throttle = 0.0f;
    float brake = 0.0f;
    float steer = 0.0f;
    bool boost = false;
};

inline input_t read_input( )
{
    input_t input{};
#if defined( _WIN32 )
    XINPUT_STATE state{};
    if ( XInputGetState( 0, &state ) == ERROR_SUCCESS )
    {
        input.throttle = static_cast< float >( state.Gamepad.bRightTrigger ) / 255.0f;
        input.brake = static_cast< float >( state.Gamepad.bLeftTrigger ) / 255.0f;
        input.steer = static_cast< float >( state.Gamepad.sThumbLX ) / 32768.0f;
        input.boost = ( state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ) != 0;
    }

    if ( GetAsyncKeyState( 'W' ) & 0x8000 )
        input.throttle = 1.0f;
    if ( GetAsyncKeyState( 'S' ) & 0x8000 )
        input.brake = 1.0f;
    if ( GetAsyncKeyState( 'A' ) & 0x8000 )
        input.steer -= 1.0f;
    if ( GetAsyncKeyState( 'D' ) & 0x8000 )
        input.steer += 1.0f;
    if ( input.steer > 1.0f )
        input.steer = 1.0f;
    if ( input.steer < -1.0f )
        input.steer = -1.0f;
    if ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 )
        input.boost = true;
#endif
    return input;
}
