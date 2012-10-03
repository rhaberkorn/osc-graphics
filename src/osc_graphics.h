#ifndef __OSC_GRAPHICS_H
#define __OSC_GRAPHICS_H

#include <stdio.h>

#include <SDL.h>
#include <SDL_thread.h>

class Mutex {
	SDL_mutex *mutex;

public:
	Mutex() : mutex(SDL_CreateMutex()) {}
	virtual ~Mutex()
	{
		SDL_DestroyMutex(mutex);
	}

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
};

#include "osc_server.h"
#include "layer.h"

/*
 * Macros
 */
#define NARRAY(ARRAY) \
	(sizeof(ARRAY) / sizeof((ARRAY)[0]))

#define SDL_MAYBE_LOCK(SURFACE) do {		\
	if (SDL_MUSTLOCK(SURFACE))		\
		SDL_LockSurface(SURFACE);	\
} while (0)

#define SDL_MAYBE_UNLOCK(SURFACE) do {		\
	if (SDL_MUSTLOCK(SURFACE))		\
		SDL_UnlockSurface(SURFACE);	\
} while (0)

#define SDL_FREESURFACE_SAFE(SURFACE) do {	\
	if (SURFACE) {				\
		SDL_FreeSurface(SURFACE);	\
		SURFACE = NULL;			\
	}					\
} while (0)

#define SDL_ERROR(FMT, ...) do {					\
	fprintf(stderr, "%s(%d): " FMT ": %s\n",			\
		__FILE__, __LINE__, ##__VA_ARGS__, SDL_GetError());	\
} while (0)
	
/*
 * Declarations
 */
extern SDL_Surface *screen;

extern int config_dump_osc;
extern int config_framerate;

#define FRAME_DELAY \
	(1000/config_framerate) /* frame delay in ms */

void rgba_blit_with_alpha(SDL_Surface *src_surf, SDL_Surface *dst_surf,
			  Uint8 alpha = SDL_ALPHA_TRANSPARENT);

#endif
