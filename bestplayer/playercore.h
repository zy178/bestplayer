#pragma once

#define HAVE_STRUCT_TIMESPEC
#include "pthread.h"

#include "sdl_render.h"

#ifdef _WIN32
	//Windows platform
	extern "C"
	{
		#include <libavformat/avformat.h>
		#include <libavcodec/avcodec.h>
		#include <libavutil/channel_layout.h>
		#include <libavutil/common.h>
		#include <libavutil/imgutils.h>
		#include <libswscale/swscale.h>
		#include <libavutil/imgutils.h>
		#include <libavutil/opt.h>
		#include <libavutil/mathematics.h>
		#include <libavutil/samplefmt.h>
		#include <libswresample/swresample.h>
		
	};

	#include <windows.h>


#else
	#include <unistd.h>
	//Linux platform
	#ifdef __cplusplus
		extern "C"
		{
	#endif

	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/imgutils.h>

	#ifdef __cplusplus
		};
	#endif

#endif


#include "pqueue.h"

#ifdef _DEBUG
		static int framecount = 0;
#endif
typedef const int _ERR_CODE;
_ERR_CODE _ERR_CODE_NONE = 0;
_ERR_CODE _ERR_CODE_OPEN = 1;

typedef void (*callbackc_videodecode)(double* pDate);
typedef void (*callbackc_audiodecode)(double* pDate);


#define DECODE_RETRY 300

#define PKG_SIZE_AUDIO_MIN 1
#define PKG_SIZE_VIDEO_MIN 1

#define PKG_SIZE_AUDIO_MAX 120
#define PKG_SIZE_VIDEO_MAX 10

#define FRAME_SIZE_AUDIO_MIN 1
#define FRAME_SIZE_VIDEO_MIN 1

#define FRAME_SIZE_AUDIO_MAX 120
#define FRAME_SIZE_VIDEO_MAX 10

#define SYNC_THRESHOLD 10


class playercore
{

private:
	AVFormatContext* pformat_context = NULL;
	AVCodecContext* pcodec_context_video = NULL;
	AVCodecContext* pcodec_context_audio = NULL;
	AVStream* pstream_video = NULL;
	AVStream* pstream_audio = NULL;
	AVCodec* pcodec_video = NULL;
	AVCodec* pcodec_audio = NULL;

	AVFrame* pframe_video = NULL;
	AVFrame* pframe_audio = NULL;

	AVPacket* ppacket = NULL;
	AVPacket* ppacket_audio = NULL;

	int index_video, index_audio;
	int result_audio, result_video;

	int got_videoframe = 0;
	
	SwrContext* swr_ctx = NULL;
	int dst_nb_samples = 0;
	int dst_linesize = 0;
	int resampled_data_size = 0;
	bool init_swr = false;
	uint8_t** dst_data = NULL;
	byte* render_data = nullptr;

	unsigned char* pbuffer_video;
	pthread_t pthread_main;
	pthread_t pthread_video_process_packet;
	pthread_t pthread_audio_process_packet;
	pthread_t pthread_video_process_frame;
	pthread_t pthread_audio_process_frame;

	pthread_t pthread_audio_render,pthread_video_render;

	pthread_mutex_t pthread_mutex_decode_audio;
	pthread_mutex_t pthread_mutex_decode_video;
	pthread_mutex_t pthread_mutex_clock;

	pqueue pqueue_packet_video, pqueue_frame_video;
	pqueue pqueue_packet_audio, pqueue_frame_audio;

	double video_clock = -1;
	double audio_clock = -1;
	double audio_pts = -1;

	double prev_video_pts = 0;
	double diff = 0;
private:
	callbackc_videodecode m_callback_video, m_callback_audio;

	static void* thread_process_video_render_static(void* arg);
	void thread_process_video_render(void* arg);

	void init_audio_render();


	static void* thread_process_video_packet_static(void* arg);
	void thread_process_video_packet(void* arg);

	static void* thread_process_audio_packet_static(void* arg);
	void thread_process_audio_packet(void* arg);

	static void* thread_process_video_frame_static(void* arg);
	void thread_process_video_frame(void* arg);

	static void* thread_process_audio_frame_static(void* arg);
	void thread_process_audio_frame(void* arg);

	static void* thread_main_static(void* arg);
	void thread_main(void* arg);

	static int audiorender_cb_static(void* userdata, void** stream);
	int audiorender_cb(void** stream);

	int decode_video(AVPacket* packet);
	int decode_audio(AVPacket* packet);

	int audio_resample(byte** out, AVCodecContext* audio_dec_ctx, AVFrame* pAudioDecodeFrame, int out_sample_fmt, int out_channels, int out_sample_rate);

	double compute_video_pts(AVFrame* srcFrame, double pts);
	sdl_render* render_sdl = nullptr;
public:
	_ERR_CODE register_callback_video(callbackc_videodecode callback);
	_ERR_CODE register_callback_audio(callbackc_videodecode callback);

public:
	_ERR_CODE init();
	_ERR_CODE uninit();

	_ERR_CODE open(char* name);
	_ERR_CODE close();

public:
	_ERR_CODE play();
	_ERR_CODE stop();
	_ERR_CODE pause();
	_ERR_CODE seek(long long mill);
};

