#pragma once
#include "baserender.h"

class audiorender
{
private:
	typedef int (SDLCALL* AudioRenderCallBack) (void* userdata, void** stream);

public:
	
	static void fill_audio(void* udata, Uint8* stream, int len);
	int init(int sample_rate,int channels, int nb_samples, void* userdata, AudioRenderCallBack callback);
	void fill_buffer(void* udata, Uint8* stream, int len);
private:

	void* userdata;
	SDL_AudioSpec wanted_spec;
	SDL_AudioCallback callback;
	AudioRenderCallBack audiorender_callback = NULL;
};

