#ifndef __RECORDER_H
#define __RECORDER_H

#include <SDL.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include "osc_graphics.h"

class Recorder : Mutex {
	AVFormatContext	*ffmpeg;
	AVStream 	*stream;
	SwsContext	*sws_context;
	AVFrame		*encodeFrame;
	int		encodeFrameBufferSize;
	uint8_t		*encodeFrameBuffer;
	AVPacket	pkt;

	Uint32 start_time;

public:
	Recorder();
	~Recorder();

	void register_methods();

	void start(const char *filename, const char *codecname = NULL);
	void stop();

	void record(SDL_Surface *surf);
};

#endif
