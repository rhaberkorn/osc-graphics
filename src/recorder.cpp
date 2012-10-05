#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define __STDC_CONSTANT_MACROS
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <SDL.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/error.h>
}

#include "osc_graphics.h"
#include "osc_server.h"

#include "recorder.h"

/*
 * Macros
 */
#define FFMPEG_ERROR(AVERROR, FMT, ...) do {		\
	char buf[255];					\
							\
	av_strerror(AVERROR, buf, sizeof(buf));		\
	ERROR(FMT ": %s", ##__VA_ARGS__, buf);		\
} while (0)

extern "C" {

static int start_handler(const char *path, const char *types,
			 lo_arg **argv, int argc,
			 void *data, void *user_data);
static int stop_handler(const char *path, const char *types,
			lo_arg **argv, int argc,
			void *data, void *user_data);

}

Recorder::Recorder() : Mutex(), ffmpeg(NULL), stream(NULL), sws_context(NULL),
				encodeFrame(NULL), encodeFrameBuffer(NULL)
{
	static bool initialized = false;

	if (!initialized) {
		avcodec_register_all();
		av_register_all();

		initialized = true;
	}
}

void
Recorder::register_methods()
{
	osc_server.add_method("ss", start_handler, this, "/recorder/start");
	osc_server.add_method("", stop_handler, this, "/recorder/stop");
}

static int
start_handler(const char *path, const char *types,
	      lo_arg **argv, int argc, void *data, void *user_data)
{
	Recorder *obj = (Recorder *)user_data;

	obj->start(&argv[0]->s, &argv[1]->s);

	return 0;
}

static enum PixelFormat
get_pix_fmt(SDL_PixelFormat *format)
{
	switch (format->BitsPerPixel) {
	case 8:
		return PIX_FMT_PAL8;
	case 24:
		return PIX_FMT_RGB24;
	case 32:
		return PIX_FMT_RGB32;
	}

	return PIX_FMT_NONE;
}

void
Recorder::start(const char *filename, const char *codecname)
{
	AVCodec *videoCodec;
	int err;

	stop();

	if (!filename || !*filename)
		return;

	lock();

	ffmpeg = avformat_alloc_context();

	ffmpeg->oformat = av_guess_format(NULL, filename, NULL);
	if (!ffmpeg->oformat)
		ffmpeg->oformat = av_guess_format("dvd", NULL, NULL);

	ffmpeg->preload = (int)(0.5 * AV_TIME_BASE);
	ffmpeg->max_delay = (int)(0.7 * AV_TIME_BASE);

	err = url_fopen(&ffmpeg->pb, filename, URL_WRONLY);
	if (err < 0) {
		FFMPEG_ERROR(-err, "url_fopen");
		exit(EXIT_FAILURE);
	}

	stream = av_new_stream(ffmpeg, 0);
	if (!stream) {
		ERROR("Could not open stream!");
		exit(EXIT_FAILURE);
	}

	stream->codec = avcodec_alloc_context();

	avcodec_get_context_defaults2(stream->codec, CODEC_TYPE_VIDEO);

	stream->codec->codec_type = CODEC_TYPE_VIDEO;

	stream->codec->bit_rate = 6000000;

	/* resolution must be a multiple of two */
	stream->codec->width = screen->w;
	stream->codec->height = screen->h;

	/* set time_base */
	stream->codec->time_base.num = 1;
	stream->codec->time_base.den = config_framerate;

	/* emit one intra frame every twelve frames at most */
	stream->codec->gop_size = 12;

	switch (stream->codec->codec_id) {
	case CODEC_ID_MPEG2VIDEO:
		/* set mpeg2 codec parameters */
		stream->codec->max_b_frames = 2;
		break;
	case CODEC_ID_MPEG1VIDEO:
		/* set mpeg1 codec parameters */
		/* needed to avoid using macroblocks in which some coeffs overflow
		   this doesnt happen with normal video, it just happens here as the
		   motion of the chroma plane doesnt match the luma plane */
		stream->codec->mb_decision = 2;
		break;
	default:
		break;
	}

	/* some formats want stream headers to be separate */
	if (ffmpeg->oformat->flags & AVFMT_GLOBALHEADER)
		stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

	/* find the video encoder */
	if (codecname && *codecname)
		videoCodec = avcodec_find_encoder_by_name(codecname);
	else
		videoCodec = avcodec_find_encoder(ffmpeg->oformat->video_codec);
	if (!videoCodec) {
		ERROR("Could not find encoder %s (%d)!",
		      codecname ? : "NULL",
		      ffmpeg->oformat->video_codec);
		exit(EXIT_FAILURE);
	}
	stream->codec->codec_id = videoCodec->id;

	enum PixelFormat screen_fmt = get_pix_fmt(screen->format);

	/* set pixel format */
	stream->codec->pix_fmt = PIX_FMT_YUV420P;
	for (const PixelFormat *fmt = videoCodec->pix_fmts; *fmt != -1; fmt++) {
		if (*fmt == screen_fmt) {
			stream->codec->pix_fmt = screen_fmt;
			break;
		}
	}

	if (stream->codec->pix_fmt != screen_fmt) {
		WARNING("Pixel format conversion necessary!");

		/* NOTE: sizes shouldn't differ */
		sws_context = sws_getContext(screen->w, screen->h, screen_fmt,
					     stream->codec->width,
					     stream->codec->height,
					     stream->codec->pix_fmt,
					     SWS_BILINEAR, NULL, NULL, NULL);
	}

	/* open the codec */
	err = avcodec_open(stream->codec, videoCodec);
	if (err < 0) {
		FFMPEG_ERROR(-err, "avcodec_open");
		exit(EXIT_FAILURE);
	}

	encodeFrame = avcodec_alloc_frame();

	if (sws_context) {
		int size = avpicture_get_size(stream->codec->pix_fmt,
					      stream->codec->width,
					      stream->codec->height) +
			   FF_INPUT_BUFFER_PADDING_SIZE;
		avpicture_fill((AVPicture *)encodeFrame, new uint8_t[size],
			       stream->codec->pix_fmt,
			       stream->codec->width,
			       stream->codec->height);
	}

	encodeFrameBufferSize = stream->codec->width * stream->codec->height * 4 +
				FF_INPUT_BUFFER_PADDING_SIZE;
	encodeFrameBuffer = new uint8_t[encodeFrameBufferSize];

	err = av_set_parameters(ffmpeg, 0);
	if (err < 0) {
		FFMPEG_ERROR(-err, "av_set_parameters");
		exit(EXIT_FAILURE);
	}

	/* try to write a header */
	av_write_header(ffmpeg);

	start_time = SDL_GetTicks();

	unlock();
}

static int
stop_handler(const char *path, const char *types,
	     lo_arg **argv, int argc, void *data, void *user_data)
{
	Recorder *obj = (Recorder *)user_data;

	obj->stop();

	return 0;
}

void
Recorder::stop()
{
	lock();

	if (stream && stream->codec)
		avcodec_flush_buffers(stream->codec);

	if (ffmpeg)
		av_write_trailer(ffmpeg);

	av_free_packet(&pkt);
	if (encodeFrameBuffer) {
		delete encodeFrameBuffer;
		encodeFrameBuffer = NULL;
	}
	if (sws_context) {
		sws_freeContext(sws_context);
		sws_context = NULL;

		if (encodeFrame)
			delete encodeFrame->data[0];
	}
	if (encodeFrame) {
		av_free(encodeFrame);
		encodeFrame = NULL;
	}
	if (stream) {
		if (stream->codec)
			avcodec_close(stream->codec);
		av_free(stream);
		stream = NULL;
	}
	if (ffmpeg) {
		url_fclose(ffmpeg->pb);
		av_free(ffmpeg);
		ffmpeg = NULL;
	}

	unlock();
}

void
Recorder::record(SDL_Surface *surf)
{
	lock();

	if (!ffmpeg) {
		unlock();
		return;
	}

	int64_t pts = (SDL_GetTicks() - start_time)/FRAME_DELAY;
	if (pts <= encodeFrame->pts) {
		unlock();
		return;
	}
	encodeFrame->pts = pts;

	if (sws_context) {
		const uint8_t *const data[] = {(uint8_t *)surf->pixels, NULL};
		const int pitch[] = {surf->pitch, 0};

		sws_scale(sws_context, data, pitch, 0, surf->h,
			  encodeFrame->data, encodeFrame->linesize);
	} else {
		encodeFrame->data[0] = (uint8_t *)surf->pixels;
		encodeFrame->data[1] = NULL;
		encodeFrame->linesize[0] = surf->pitch;
		encodeFrame->linesize[1] = 0;
	}

	int out_size = avcodec_encode_video(stream->codec, encodeFrameBuffer,
					    encodeFrameBufferSize, encodeFrame);

	if (out_size > 0) {
		av_init_packet(&pkt);

		/* set correct stream index for this packet */
		pkt.stream_index = stream->index;
		/* set keyframe flag if needed */
		if (stream->codec->coded_frame->key_frame)
			pkt.flags |= PKT_FLAG_KEY;
		/* write encoded data into packet */
		pkt.data = encodeFrameBuffer;
		/* set the correct size of this packet */
		pkt.size = out_size;
		/* set the correct duration of this packet */
		pkt.duration = AV_TIME_BASE / stream->time_base.den;

		/* if needed info is available, write pts for this packet */
		if (stream->codec->coded_frame->pts != AV_NOPTS_VALUE)
			pkt.pts = av_rescale_q(stream->codec->coded_frame->pts,
					       stream->codec->time_base,
					       stream->time_base);

		av_write_frame(ffmpeg, &pkt);
	}

	unlock();
}

Recorder::~Recorder()
{
	osc_server.del_method("ss", "/recorder/start");
	osc_server.del_method("",   "/recorder/stop");

	stop();
}
