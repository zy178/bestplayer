#pragma once

extern "C"
{
#define	SDL_MAIN_HANDLED
#include "sdl.h"
}

class sdl_render
{
private:
	//audio 
	typedef int (SDLCALL* AudioRenderCallBack) (void* userdata, void** stream);
	static void fillbuffer(void* udata, Uint8* stream, int len);
	SDL_AudioSpec wanted_spec;
	SDL_AudioCallback callback;
	AudioRenderCallBack audiorender_callback = NULL;
	void* userdata = NULL;

	//video
	SDL_Rect rect;
	Uint32 pixformat;
	SDL_Window* win = NULL;
	SDL_Renderer* renderer = NULL;
	SDL_Texture* texture = NULL;

public:
	int init(int sample_rate, int channels, int nb_samples, void* userdata, AudioRenderCallBack callback);
	int init(int width, int height);
	
	int render(Uint8* stream, int len);
	int render(uint8_t** pvideodata, int* linesize, long height, long width);
	sdl_render();
};

