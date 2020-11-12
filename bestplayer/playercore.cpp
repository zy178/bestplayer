#include "playercore.h"
#include "util.h"
#include<chrono>
_ERR_CODE playercore::open(char* name)
{
	pformat_context = avformat_alloc_context();
	if (!pformat_context)
		return _ERR_CODE_OPEN;

	if (avformat_open_input(&pformat_context, name, NULL, NULL) != 0)
		return _ERR_CODE_OPEN;

	if (avformat_find_stream_info(pformat_context, NULL) != 0)
		return _ERR_CODE_OPEN;

	//find video and audio track index
	index_video = 0;
	index_audio = 0;

	result_audio = 0;
	result_video = 0;

	for (int i = 0; i < pformat_context->nb_streams; i++)
	{
		if (pformat_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			index_video = i;
			pstream_video = pformat_context->streams[index_video];
			pcodec_context_video = pstream_video->codec;

			pcodec_video = avcodec_find_decoder(pcodec_context_video->codec_id);
			if (pcodec_video)
				result_video = avcodec_open2(pcodec_context_video, pcodec_video, NULL);

			continue;
		}
		if (pformat_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			index_audio = i;
			pstream_audio = pformat_context->streams[index_audio];
			pcodec_context_audio = pstream_audio->codec;
			pcodec_audio = avcodec_find_decoder(pcodec_context_audio->codec_id);
			if (pcodec_audio)
				result_audio = avcodec_open2(pcodec_context_audio, pcodec_audio, NULL);

			continue;
		}
	}

	//初始化渲染器
	if (!render_sdl)
	{
		render_sdl = new sdl_render();
	}

	if ((index_video == -1 && index_audio == -1) || (pcodec_video == NULL && pcodec_audio == NULL) || result_audio < 0 && result_video < 0)
		return _ERR_CODE_OPEN;

	//设定视频音频packet缓冲区的最低长度和最大长度
	pqueue_packet_video.setminsize(PKG_SIZE_VIDEO_MIN);
	pqueue_packet_audio.setminsize(PKG_SIZE_AUDIO_MIN);

	pqueue_packet_video.setmaxsize(PKG_SIZE_VIDEO_MAX);
	pqueue_packet_audio.setmaxsize(PKG_SIZE_AUDIO_MAX);

	pqueue_frame_video.setminsize(FRAME_SIZE_VIDEO_MIN);
	pqueue_frame_audio.setminsize(FRAME_SIZE_AUDIO_MIN);

	pqueue_frame_video.setmaxsize(FRAME_SIZE_VIDEO_MAX);
	pqueue_frame_audio.setmaxsize(FRAME_SIZE_AUDIO_MAX);

	//队列临界区初始化
	pthread_mutex_init(&pthread_mutex_decode_video, NULL);
	pthread_mutex_init(&pthread_mutex_decode_audio, NULL);
	pthread_mutex_init(&pthread_mutex_clock, NULL);

	//启动视频线程
	pthread_create(&pthread_video_process_packet, NULL, thread_process_video_packet_static, this);

	//启动音频线程
	pthread_create(&pthread_audio_process_packet, NULL, thread_process_audio_packet_static, this);

	//启动视频解码线程
	pthread_create(&pthread_video_process_frame, NULL, thread_process_video_frame_static, this);

	//启动音频解码线程
	pthread_create(&pthread_audio_process_frame, NULL, thread_process_audio_frame_static, this);

	//启动音频解码线程
	pthread_create(&pthread_video_render, NULL, thread_process_video_render_static, this);


	//启动主线程
	pthread_create(&pthread_main, NULL, thread_main_static, this);

	init_audio_render();

	//等待线程结束
	pthread_join(pthread_audio_render, NULL);
	pthread_join(pthread_video_render, NULL);
	pthread_join(pthread_video_process_packet, NULL);
	pthread_join(pthread_audio_process_packet, NULL);
	pthread_join(pthread_video_process_frame, NULL);
	pthread_join(pthread_audio_process_frame, NULL);
	pthread_join(pthread_main, NULL);

	return _ERR_CODE();
}

void* playercore::thread_main_static(void* arg)
{
	playercore* p = (playercore*)arg;
	p->thread_main(nullptr);
	return nullptr;
}

_ERR_CODE playercore::close()
{
	return _ERR_CODE();
}

_ERR_CODE playercore::play()
{
	return _ERR_CODE();
}

_ERR_CODE playercore::stop()
{
	return _ERR_CODE();
}

_ERR_CODE playercore::pause()
{
	return _ERR_CODE();
}

_ERR_CODE playercore::seek(long long mill)
{
	return _ERR_CODE();
}

void* playercore::thread_process_video_render_static(void* arg)
{
	playercore* p = (playercore*)arg;
	p->thread_process_video_render(arg);
	return nullptr;
}

double playercore::compute_video_pts(AVFrame* srcFrame, double pts)
{
	double frame_delay;

	if (pts != 0)
		video_clock = pts; // Get pts,then set video clock to it
	else
		pts = video_clock; // Don't get pts,set it to video clock

	frame_delay = av_q2d(pstream_video->time_base);
	frame_delay += srcFrame->repeat_pict * (frame_delay * 0.5);

	video_clock += frame_delay;

	return pts;
}

void playercore::init_audio_render()
{
	AVFrame* pframe = (AVFrame*)pqueue_frame_audio.get(0);
	if (pqueue_frame_audio.getstate() == state::FLUSHED && pqueue_frame_audio.getlength() == 0)
		return;

	//重采样
	long samplesize = audio_resample(&render_data, pcodec_context_audio, pframe, AVSampleFormat::AV_SAMPLE_FMT_S16, 2, pcodec_context_audio->sample_rate);

	render_sdl->init(pcodec_context_audio->sample_rate, 2, pframe->nb_samples, this, playercore::audiorender_cb_static);
}

int playercore::audio_resample(byte** out, AVCodecContext* audio_dec_ctx, AVFrame* pAudioDecodeFrame, int out_sample_fmt, int out_channels, int out_sample_rate)
{
	int ret = 0;
	if (!init_swr)
	{
		//获取swr对象
		swr_ctx = swr_alloc_set_opts(swr_ctx, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, out_sample_rate, audio_dec_ctx->channel_layout, audio_dec_ctx->sample_fmt, audio_dec_ctx->sample_rate, 0, nullptr);
		
		//init swr
		if ((ret = swr_init(swr_ctx)) < 0) 
			return ret;

		//计算目标样本数量
		dst_nb_samples = av_rescale_rnd(pAudioDecodeFrame->nb_samples, out_sample_rate, audio_dec_ctx->sample_rate, AV_ROUND_UP);

		ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, 2,
			dst_nb_samples, AV_SAMPLE_FMT_S16, 0);

		init_swr = true;
	}
	
	ret = swr_convert(swr_ctx, dst_data, dst_nb_samples,
		(const uint8_t**)pAudioDecodeFrame->data, pAudioDecodeFrame->nb_samples);

	if (ret <= 0)
	{
		printf("swr_convert error \n");
		return -1;
	}

	resampled_data_size = av_samples_get_buffer_size(&dst_linesize, 2,
		ret, (AVSampleFormat)out_sample_fmt, 1);

	if (resampled_data_size <= 0)
	{
		printf("av_samples_get_buffer_size error \n");
		return -1;
	}
	
	if (*out == NULL)
	{
		*out = (byte*)av_malloc(resampled_data_size);
	}

	memset(*out, 0, resampled_data_size);
	//将值返回去
	memcpy(*out, dst_data[0], resampled_data_size);
	av_samples_copy(out,dst_data,0,0, dst_nb_samples,2, AV_SAMPLE_FMT_S16);

	return resampled_data_size;
}
/*
int playercore::audio_resample(AVCodecContext* audio_dec_ctx, AVFrame* pAudioDecodeFrame,
	int out_sample_fmt, int out_channels, int out_sample_rate, byte** out_buf)
{

	return audio_resample(out_buf,audio_dec_ctx, pAudioDecodeFrame, out_sample_fmt, out_channels, out_sample_rate);

	uint8_t* output = NULL;
	//////////////////////////////////////////////////////////////////////////
	
	int data_size = 0;
	int ret = 0;
	int64_t src_ch_layout = AV_CH_LAYOUT_STEREO; //初始化这样根据不同文件做调整
	int64_t dst_ch_layout = AV_CH_LAYOUT_STEREO; //这里设定ok
	int dst_nb_channels = 0;
	int dst_linesize = 0;
	int src_nb_samples = 0;
	int dst_nb_samples = 0;
	int max_dst_nb_samples = 0;
	uint8_t** dst_data = NULL;
	int resampled_data_size = 0;

	//重新采样
	if (swr_ctx)
	{
		swr_free(&swr_ctx);
	}
	swr_ctx = swr_alloc();
	if (!swr_ctx)
	{
		printf("swr_alloc error \n");
		return -1;
	}

	src_ch_layout = (audio_dec_ctx->channel_layout &&
		audio_dec_ctx->channels ==
		av_get_channel_layout_nb_channels(audio_dec_ctx->channel_layout)) ?
		audio_dec_ctx->channel_layout :
		av_get_default_channel_layout(audio_dec_ctx->channels);

	if (out_channels == 1)
	{
		dst_ch_layout = AV_CH_LAYOUT_MONO;
	}
	else if (out_channels == 2)
	{
		dst_ch_layout = AV_CH_LAYOUT_STEREO;
	}
	else
	{
		//可扩展
	}

	if (src_ch_layout <= 0)
	{
		printf("src_ch_layout error \n");
		return -1;
	}

	src_nb_samples = pAudioDecodeFrame->nb_samples;
	if (src_nb_samples <= 0)
	{
		printf("src_nb_samples error \n");
		return -1;
	}

	av_opt_set_int(swr_ctx, "in_channel_layout", src_ch_layout, 0);
	av_opt_set_int(swr_ctx, "in_sample_rate", audio_dec_ctx->sample_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", audio_dec_ctx->sample_fmt, 0);

	av_opt_set_int(swr_ctx, "out_channel_layout", dst_ch_layout, 0);
	av_opt_set_int(swr_ctx, "out_sample_rate", out_sample_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", (AVSampleFormat)out_sample_fmt, 0);
	swr_init(swr_ctx);

	max_dst_nb_samples = dst_nb_samples =
		av_rescale_rnd(src_nb_samples, out_sample_rate, audio_dec_ctx->sample_rate, AV_ROUND_UP);
	if (max_dst_nb_samples <= 0)
	{
		printf("av_rescale_rnd error \n");
		return -1;
	}

	dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
	ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
		dst_nb_samples, (AVSampleFormat)out_sample_fmt, 0);
	if (ret < 0)
	{
		printf("av_samples_alloc_array_and_samples error \n");
		return -1;
	}


	dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, audio_dec_ctx->sample_rate) +
		src_nb_samples, out_sample_rate, audio_dec_ctx->sample_rate, AV_ROUND_UP);
	if (dst_nb_samples <= 0)
	{
		printf("av_rescale_rnd error \n");
		return -1;
	}
	if (dst_nb_samples > max_dst_nb_samples)
	{
		av_free(dst_data[0]);
		ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
			dst_nb_samples, (AVSampleFormat)out_sample_fmt, 1);
		max_dst_nb_samples = dst_nb_samples;
	}

	data_size = av_samples_get_buffer_size(NULL, audio_dec_ctx->channels,
		pAudioDecodeFrame->nb_samples,
		audio_dec_ctx->sample_fmt, 1);
	if (data_size <= 0)
	{
		printf("av_samples_get_buffer_size error \n");
		return -1;
	}
	resampled_data_size = data_size;

	if (swr_ctx)
	{
		ret = swr_convert(swr_ctx, dst_data, dst_nb_samples,
			(const uint8_t**)pAudioDecodeFrame->data, pAudioDecodeFrame->nb_samples);
		if (ret <= 0)
		{
			printf("swr_convert error \n");
			return -1;
		}

		resampled_data_size = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
			ret, (AVSampleFormat)out_sample_fmt, 1);
		if (resampled_data_size <= 0)
		{
			printf("av_samples_get_buffer_size error \n");
			return -1;
		}
	}
	else
	{
		printf("swr_ctx null error \n");
		return -1;
	}

	*out_buf = (byte*)malloc(resampled_data_size);
	memset(*out_buf, 0, resampled_data_size);
	//将值返回去
	memcpy(*out_buf, dst_data[0], resampled_data_size);

	if (dst_data)
	{
		av_freep(&dst_data[0]);
	}
	av_freep(&dst_data);
	dst_data = NULL;

	if (swr_ctx)
	{
		swr_free(&swr_ctx);
	}
	return resampled_data_size;
}
*/

void* playercore::thread_process_video_packet_static(void* arg)
{
	playercore* p = (playercore*)arg;
	p->thread_process_video_packet(arg);
	return nullptr;
}

void* playercore::thread_process_audio_packet_static(void* arg)
{
	playercore* p = (playercore*)arg;
	p->thread_process_audio_packet(arg);
	return nullptr;
}


void* playercore::thread_process_video_frame_static(void* arg)
{
	playercore* p = (playercore*)arg;
	p->thread_process_video_frame(arg);
	return nullptr;
}

void* playercore::thread_process_audio_frame_static(void* arg)
{
	playercore* p = (playercore*)arg;
	p->thread_process_audio_frame(arg);
	return nullptr;
}

int playercore::decode_video(AVPacket* packet)
{
	return 0;
}

int playercore::decode_audio(AVPacket* packet)
{
	return -1;
}

_ERR_CODE playercore::register_callback_video(callbackc_videodecode callback)
{
	m_callback_video = callback;
	return _ERR_CODE_NONE;
}

_ERR_CODE playercore::register_callback_audio(callbackc_videodecode callback)
{
	m_callback_audio = callback;
	return _ERR_CODE_NONE;
}

_ERR_CODE playercore::init()
{
	av_register_all();

	return _ERR_CODE();
}

_ERR_CODE playercore::uninit()
{
	return _ERR_CODE();
}



void playercore::thread_main(void* arg)
{
	while (true)
	{
		//分配一个packet
		ppacket = av_packet_alloc();

		//读取帧数据
		//读取不到
		if (av_read_frame(pformat_context, ppacket) < 0)
		{
			pqueue_packet_video.flush();
			pqueue_packet_audio.flush();
			av_packet_free(&ppacket);
#ifdef _DEBUG
			printf("decode ended ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
#endif
			break;
		}
		else
		{
			//将帧数据分别压入音频和视频队列
			if (ppacket->stream_index == index_video)
			{
				//压入视频packet，如果发现队列满则等待，直到压入队列为止
				int pushi = pqueue_packet_video.push(ppacket);
				int count = pqueue_packet_video.getlength();
				long long dts = ppacket->dts;
				long long pts = ppacket->pts;

#ifdef _DEBUG
				printf("video packet push count: %d dts : %lld  pts : %lld \n", count, ppacket->dts, ppacket->pts);
#endif
			}

			else if (ppacket->stream_index == index_audio)
			{
				int pushi = pqueue_packet_audio.push(ppacket);
				int count = pqueue_packet_audio.getlength();
				long long dts = ppacket->dts;
				long long pts = ppacket->pts;

#ifdef _DEBUG
				printf("audio packet push count: %d dts : %lld  pts : %lld \n", count, ppacket->dts, ppacket->pts);
#endif
			}
		}
		sleepcore(1);
	}
}

void playercore::thread_process_video_packet(void* arg)
{
	while (true)
	{
		//从队列取出视频Packet
		AVPacket* ppacket_video = (AVPacket*)pqueue_packet_video.dequeue();

		if (pqueue_packet_video.getstate() == state::FLUSHED && pqueue_packet_video.getlength() == 0)
		{

			//向解码器发送最后一帧null，弹出缓存的帧
			pthread_mutex_lock(&pthread_mutex_decode_video);
			avcodec_flush_buffers(pcodec_context_video);
			pthread_mutex_unlock(&pthread_mutex_decode_video);
			break;
		}

		int retrycount = 0;
		while (true)
		{
			//将视频Packet送入解码器解码，解码成功则送入下一帧Packet，解码不成功则循环，直到成功
			pthread_mutex_lock(&pthread_mutex_decode_video);
			int ret = avcodec_send_packet(pcodec_context_video, (AVPacket*)ppacket_video);
			pthread_mutex_unlock(&pthread_mutex_decode_video);
			//队列并没有满，但是重试3次都没有解出视频帧，直接跳帧
			if ((ret == 0) || (++retrycount == DECODE_RETRY && !pqueue_frame_video.isfull()))
			{
				//释放video packet占用的资源
				av_packet_unref(ppacket_video);
				break;
			}
			
			sleepcore(1);
		}
		sleepcore(1);
	}
	printf("thread_video_packet_exit++++++++++++++++++++++++++++++++\n");
}

void playercore::thread_process_audio_packet(void* arg)
{
	while (true)
	{
		AVPacket* ppacket_audio = (AVPacket*)pqueue_packet_audio.dequeue();
		int count = pqueue_packet_audio.getlength();

		if (pqueue_packet_audio.getstate() == state::FLUSHED && pqueue_packet_audio.getlength() == 0)
		{
			//flush，输出最后几帧
			pthread_mutex_lock(&pthread_mutex_decode_audio);
			avcodec_flush_buffers(pcodec_context_audio);
			pthread_mutex_unlock(&pthread_mutex_decode_audio);
			break;
		}

		int retrycount = 0;
		while (true)
		{
			//将视频Packet送入解码器解码，解码成功则送入下一帧Packet，解码不成功则循环，直到成功
			pthread_mutex_lock(&pthread_mutex_decode_audio);
			int ret = avcodec_send_packet(pcodec_context_audio, ppacket_audio);
			pthread_mutex_unlock(&pthread_mutex_decode_audio);

			if ((ret == 0) || (++retrycount == DECODE_RETRY && !pqueue_frame_audio.isfull()))
			{
#if _DEBUG
				printf("audio decoder send ------------------------------------------------------------------------------ \n");
#endif
				//释放audio packet占用的资源
				av_packet_unref(ppacket_audio);
				break;
			}
			sleepcore(1);
		}
		sleepcore(1);
	}
	printf("thread_audio_packet_exit++++++++++++++++++++++++++++++++\n");
}

void playercore::thread_process_video_frame(void* arg)
{
	int decodecount = 0;
	while (true)
	{
		int ret = -1;
		//从解码器接收视频帧
		if (!pqueue_frame_video.isfull())
		{
			//分配frame空间
			pframe_video = av_frame_alloc();

			pthread_mutex_lock(&pthread_mutex_decode_video);
			ret = avcodec_receive_frame(pcodec_context_video, pframe_video);
			pthread_mutex_unlock(&pthread_mutex_decode_video);

			//将接收到的视频帧压入队列
			if (ret == 0)
			{
#ifdef _DEBUG
				printf("video decoder return %d------------------------------------------------------------------------------ \n", ret);
#endif

				int pushi = pqueue_frame_video.push(pframe_video);
				int count = pqueue_frame_video.getlength();
				long long dts = pframe_video->pkt_dts;
				long long pts = pframe_video->pkt_pts;

#ifdef _DEBUG
				printf("video frame push count type: %s count: %d pts : %lld dts: %lld  \n", pframe_video->pict_type == AVPictureType::AV_PICTURE_TYPE_B ? "B" : pframe_video->pict_type == AVPictureType::AV_PICTURE_TYPE_P ? "P" : "I", count, pts, dts);
#endif
			}
			else
			{
				av_frame_free(&pframe_video);
				if (pqueue_packet_video.getstate() == state::FLUSHED && pqueue_packet_video.getlength() == 0 && ++decodecount == 10)
				{
					break;
				}
			}
		}
		sleepcore(1);
	}
	pqueue_frame_video.flush();
	printf("thread_video_frame_exit++++++++++++++++++++++++++++++++\n");
	
}


void playercore::thread_process_audio_frame(void* arg)
{
	int decodecount = 0;
	while (true)
	{

		int ret = -1;
		//从解码器接收视频帧
		
		if (!pqueue_frame_audio.isfull())
		{
			//分配frame空间
			pframe_audio = av_frame_alloc();
			pthread_mutex_lock(&pthread_mutex_decode_audio);
			ret = avcodec_receive_frame(pcodec_context_audio, pframe_audio);
			pthread_mutex_unlock(&pthread_mutex_decode_audio);

			//将接收到的视频帧压入队列
			if (ret == 0)
			{
#ifdef _DEBUG
				printf("audio decoder return ------------------------------------------------------------------------------ \n");
#endif
				int pushi = pqueue_frame_audio.push(pframe_audio);
				int count = pqueue_frame_audio.getlength();
				long long dts = pframe_audio->pkt_dts;
				long long pts = pframe_audio->pkt_pts;
#ifdef _DEBUG
				printf("audio frame push count: %d pts : %lld dts: %lld  \n", count, pts, dts);
#endif
			}
			else
			{
				av_frame_free(&pframe_audio);
				if (pqueue_packet_audio.getstate() == state::FLUSHED && pqueue_packet_audio.getlength() == 0 && ++decodecount == 10)
				{
					break;
				}
			}
		}
		sleepcore(1);
	}

	pqueue_frame_audio.flush();
	printf("thread_audio_frame_exit++++++++++++++++++++++++++++++++\n");
}

void playercore::thread_process_video_render(void* arg)
{
	render_sdl->init(pcodec_context_video->width, pcodec_context_video->height);

	//获取视频旋转角度
	AVDictionaryEntry* tag = NULL;
	int angle = 0;
	tag = av_dict_get(pstream_video->metadata, "rotate", tag, 0);
	if (tag)
	{
		angle = atoi(tag->value);
		angle %= 360;
	}
	//prev_pts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	prev_video_pts = 0;
	
	while (true)
	{
		if (audio_pts > 0)
		{
			AVFrame* pframe = (AVFrame*)pqueue_frame_video.dequeue();
			double video_pts = av_q2d(pstream_video->time_base) * pframe->pts;

			//获取不到pts的时候，调用此方法获取
			//videopts = compute_video_pts(pframe, videopts);

			//计算实际每一帧的渲染时间
			double video_duration = 1000 * (video_pts - prev_video_pts);
			prev_video_pts = video_pts;

			//计算视频pts与音频pts的时间差
			double video_audio_diff = 1000 * (video_pts - audio_pts);

			if (pqueue_frame_video.getstate() == state::FLUSHED && pqueue_frame_video.getlength() == 0)
				break;

			double delaytime = 0;

			video_duration += video_audio_diff;

			if(video_duration > 0)
			sleepcore(video_duration);

			render_sdl->render(pframe->data, pframe->linesize, pframe->height, pframe->width);
			
			printf("Video Render working Video PTS: %lf  Audio PTS: %lf  diff: %lf  Video Sleep Time: %lf  ms\n", video_pts, audio_pts, video_audio_diff, video_duration);
			av_frame_free(&pframe);
		}
		else
		{
			sleepcore(1);
		}
	}
}

int playercore::audiorender_cb_static(void* userdata, void** stream)
{
	return ((playercore*)userdata)->audiorender_cb(stream);
}

int playercore::audiorender_cb(void** stream)
{
	//更新音频时间戳
	AVFrame* pframe = (AVFrame*)pqueue_frame_audio.dequeue();
	
	pthread_mutex_lock(&pthread_mutex_clock);
	audio_pts = av_q2d(pstream_audio->time_base) * pframe->pts;
	pthread_mutex_unlock(&pthread_mutex_clock);

	//printf("set audio pts : %lf \n", audio_pts);
	if (pqueue_frame_audio.getstate() == state::FLUSHED && pqueue_frame_audio.getlength() == 0)
		return 0;
	//重采样
	long samplesize = audio_resample(&render_data, pcodec_context_audio, pframe, AVSampleFormat::AV_SAMPLE_FMT_S16, 2, pcodec_context_audio->sample_rate);
	
	*stream = malloc(samplesize);
	memcpy(*stream, render_data, samplesize);

	av_frame_free(&pframe);

	return samplesize;
}
