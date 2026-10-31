#pragma once

struct screen_t
{
    const unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
};

inline screen_t& g_screen( )
{
    static screen_t screen{};
    return screen;
}
