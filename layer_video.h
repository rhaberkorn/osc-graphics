#ifndef __LAYER_VIDEO_H
#define __LAYER_VIDEO_H

#include <SDL.h>
#include <SDL_thread.h>

#include <lo/lo.h>

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

	static CtorInfo ctor_info;
	static Layer *
	ctor_osc(const char *name, SDL_Rect geo, float opacity, lo_arg **argv)
	{
		return new LayerVideo(name, geo, opacity, &argv[0]->s);
	}

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

	void frame(SDL_Surface *target);

private:
	void geo(SDL_Rect geo);
	void alpha(float opacity);

	void url(const char *url = NULL);
	OSCServer::MethodHandlerId *url_osc_id;
	static void
	url_osc(LayerVideo *obj, lo_arg **argv)
	{
		obj->url(&argv[0]->s);
	}
	void rate(float rate);
	OSCServer::MethodHandlerId *rate_osc_id;
	static void
	rate_osc(LayerVideo *obj, lo_arg **argv)
	{
		obj->rate(argv[0]->f);
	}
	void position(float position);
	OSCServer::MethodHandlerId *position_osc_id;
	static void
	position_osc(LayerVideo *obj, lo_arg **argv)
	{
		obj->position(argv[0]->f);
	}
	void paused(bool paused);
	OSCServer::MethodHandlerId *paused_osc_id;
	static void
	paused_osc(LayerVideo *obj, lo_arg **argv)
	{
		obj->paused(argv[0]->i);
	}
};

#endif
