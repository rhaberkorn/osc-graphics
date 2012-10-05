#ifndef __RECORDER_H
#define __RECORDER_H

#include <stdint.h>

#include <SDL.h>

#include "osc_graphics.h"

class Recorder : Mutex {
	struct AVFormatContext	*ffmpeg;
	struct AVStream 	*stream;
	struct SwsContext	*sws_context;
	struct AVFrame		*encodeFrame;
	int			encodeFrameBufferSize;
	uint8_t			*encodeFrameBuffer;
	struct AVPacket		*pkt;

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
