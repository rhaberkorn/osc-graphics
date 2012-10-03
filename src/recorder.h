#ifndef __RECORDER_H
#define __RECORDER_H

#include <SDL.h>
#include <SDL_thread.h>
#include <SDL/SDL_ffmpeg.h>

#include "osc_graphics.h"

class Recorder {
	SDL_ffmpegFile *file;

	SDL_mutex *mutex;

	inline void
	lock()
	{
		SDL_LockMutex(mutex);
	}
	inline void
	unlock()
	{
		SDL_UnlockMutex(mutex);
	}

public:
	Recorder() : file(NULL), mutex(SDL_CreateMutex()) {}
	~Recorder();

	void register_methods();

	void start(const char *filename);
	void stop();

	void record(SDL_Surface *surf);
};

#endif
