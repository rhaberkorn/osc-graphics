#ifndef __OSC_GRAPHICS_H
#define __OSC_GRAPHICS_H

#include <stdio.h>

#include <SDL.h>

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

#endif