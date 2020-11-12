#include "sdl_render.h"
#include "sdl_render.h"


void sdl_render::fillbuffer(void* udata, Uint8* stream, int len)
{
	((sdl_render*)udata)->render(stream, len);
}

int sdl_render::init(int sample_rate, int channels, int nb_samples, void* userdata, AudioRenderCallBack callback)
{
	this->audiorender_callback = callback;

	//用于存放调用方的this指针
	this->userdata = userdata;

	SDL_AudioSpec wanted_spec;
	wanted_spec.freq = sample_rate;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.channels = channels;
	wanted_spec.silence = 0;
	//每一帧包含的sample数量，例如aac 1024或2048
	wanted_spec.samples = nb_samples;
	wanted_spec.callback = fillbuffer;
	wanted_spec.userdata = this;

	if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
		return -1;
	}

	SDL_PauseAudio(0);
	return 0;
}

int sdl_render::render(Uint8* stream, int len)
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
	return 0;
}

int sdl_render::init(int width, int height)
{
	int ret = -1;
	win = SDL_CreateWindow("Media Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_VULKAN| SDL_WINDOW_INPUT_FOCUS| SDL_WINDOW_SHOWN);

	
	if (!win) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window by SDL");
		return ret;
	}

	//创建渲染器
	renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Renderer by SDL");
		return ret;
	}

	pixformat = SDL_PIXELFORMAT_IYUV;//YUV格式
	// 创建纹理
	texture = SDL_CreateTexture(renderer,
		pixformat,
		SDL_TEXTUREACCESS_STREAMING,
		width,
		height);

	return 0;
}

int sdl_render::render(uint8_t** pvideodata, int* linesize, long height, long width)
{
	
	SDL_PumpEvents();
	SDL_UpdateYUVTexture(texture, NULL,
		pvideodata[0], linesize[0],
		pvideodata[1], linesize[1],
		pvideodata[2], linesize[2]);

	// Set Size of Window
	rect.x = 0;
	rect.y = 0;
	rect.w = width;
	rect.h = height;
	SDL_Rect dst;
	dst.x = 0;
	dst.y = 0;
	dst.w = width;
	dst.h = height;
	//展示
	SDL_RenderClear(renderer);
	SDL_RenderCopyEx(renderer, texture, &rect, &dst,0, NULL, SDL_FLIP_NONE);
	SDL_RenderPresent(renderer);
	return 0;
}



sdl_render::sdl_render()
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO |SDL_INIT_TIMER);
}
