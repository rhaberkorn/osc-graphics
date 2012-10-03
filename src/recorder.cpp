#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include <SDL.h>
#include <SDL/SDL_ffmpeg.h>

#include "osc_graphics.h"
#include "osc_server.h"

#include "recorder.h"

/*
 * Macros
 */
#define SDL_FFMPEGFREE_SAFE(FILE) do {			\
	if (FILE) {					\
		SDL_ffmpegFree(FILE);			\
		FILE = NULL;				\
	}						\
} while (0)

#define SDL_FFMPEG_ERROR(FMT, ...) do {			\
	fprintf(stderr, "%s(%d): " FMT ": %s\n",	\
		__FILE__, __LINE__, ##__VA_ARGS__,	\
		SDL_ffmpegGetError());			\
} while (0)

extern "C" {

static int start_handler(const char *path, const char *types,
		       	 lo_arg **argv, int argc,
		       	 void *data, void *user_data);
static int stop_handler(const char *path, const char *types,
			lo_arg **argv, int argc,
		       	void *data, void *user_data);

}

void
Recorder::register_methods()
{
	osc_server.add_method("s", start_handler, this, "/recorder/start");
	osc_server.add_method("", stop_handler, this, "/recorder/stop");
}

static int
start_handler(const char *path, const char *types,
	      lo_arg **argv, int argc, void *data, void *user_data)
{
	Recorder *obj = (Recorder *)user_data;

	obj->start(&argv[0]->s);

	return 0;
}

void
Recorder::start(const char *filename)
{
	SDL_ffmpegCodec codec = {
		-1,			/* video codec based on file name */
		screen->w, screen->h,
		1, 20 /* FIXME */,	/* framerate */
		6000000, -1, -1,
		-1, 0, -1, -1, -1, -1	/* no audio */
	};

	lock();

	SDL_FFMPEGFREE_SAFE(file);

	file = SDL_ffmpegCreate(filename);
	if (!file) {
		SDL_FFMPEG_ERROR("SDL_ffmpegCreate");
		exit(EXIT_FAILURE);
	}

	SDL_ffmpegAddVideoStream(file, codec);
	SDL_ffmpegSelectVideoStream(file, 0);

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
	SDL_FFMPEGFREE_SAFE(file);
	unlock();
}

void
Recorder::record(SDL_Surface *surf)
{
	lock();

	if (file)
		SDL_ffmpegAddVideoFrame(file, surf);

	unlock();
}

Recorder::~Recorder()
{
	osc_server.del_method("s", "/recorder/start");
	osc_server.del_method("",  "/recorder/stop");

	SDL_FFMPEGFREE_SAFE(file);
	SDL_DestroyMutex(mutex);
}
