#include "audiorender.h"
#include "stdio.h"
void audiorender::fill_audio(void* udata, Uint8* stream, int len)
{
	
}

int audiorender::init(int sample_rate, int channels, int nb_samples,void* userdata, AudioRenderCallBack callback)
{
	
}

void audiorender::fill_buffer(void* udata, Uint8* stream, int len)
{
	void* pdata = NULL;
	int samplecount = 0;
	int sampleall = len;
	SDL_memset(stream, 0, len);

	if (audiorender_callback)
	{
		samplecount = audiorender_callback(userdata, &pdata);
		SDL_MixAudio(stream, (Uint8*)pdata, len, SDL_MIX_MAXVOLUME);
	}
	
	free(pdata);
}
