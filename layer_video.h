#ifndef __HAVE_LAYER_VIDEO_H
#define __HAVE_LAYER_VIDEO_H

#include <SDL.h>
#include <SDL_thread.h>

#include <vlc/vlc.h>

#include "osc_graphics.h"
#include "layer.h"

class LayerVideo : public Layer {
	libvlc_instance_t *vlcinst;
	libvlc_media_player_t *mp;

	SDL_Surface *surf;
	SDL_mutex *mutex;

	SDL_Rect geov;
	float alphav;

	float ratev;
	bool pausedv;

public:
	LayerVideo(const char *name,
		   SDL_Rect geo = (SDL_Rect){0, 0, 0, 0},
		   float opacity = 1.,
		   const char *url = NULL);
	~LayerVideo();

	inline void *
	lock_surf()
	{
		SDL_LockMutex(mutex);
		SDL_MAYBE_LOCK(surf);
		return surf->pixels;
	}
	inline void
	unlock_surf()
	{
		SDL_MAYBE_UNLOCK(surf);
		SDL_UnlockMutex(mutex);
	}

	void geo(SDL_Rect geo);
	void alpha(float opacity);

	void url(const char *url = NULL);
	void rate(float rate);
	void position(float position);
	void paused(bool paused);

	void frame(SDL_Surface *target);
};

#endif
