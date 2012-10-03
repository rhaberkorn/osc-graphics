#ifndef __RECORDER_H
#define __RECORDER_H

#include <SDL.h>
#include <SDL/SDL_ffmpeg.h>

#include "osc_graphics.h"

class Recorder : Mutex {
	SDL_ffmpegFile *file;
	Uint32 start_time;

public:
	Recorder() : Mutex(), file(NULL) {}
	~Recorder();

	void register_methods();

	void start(const char *filename);
	void stop();

	void record(SDL_Surface *surf);
};

#endif
