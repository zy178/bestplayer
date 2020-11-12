#pragma once
#include "baserender.h"
class videorender
{
private:
    SDL_Rect rect;
    Uint32 pixformat;
    SDL_Window* win = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_Texture* texture = NULL;
    //默认窗口大小
    int w_width = 0;
    int w_height = 0;
public:
    int init(int width,int height);
    int render(uint8_t** pdata, int* linesize, long height, long width);

};