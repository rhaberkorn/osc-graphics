#ifndef __LAYER_IMAGE_H
#define __LAYER_IMAGE_H

#include <stdlib.h>
#include <stdio.h>

#include <SDL.h>

#include "osc_graphics.h"
#include "layer.h"

class LayerImage : public Layer {
	SDL_Surface	*surf_alpha;	/* with per-surface alpha */
	SDL_Surface	*surf_scaled;	/* scaled image */
	SDL_Surface	*surf;		/* original image */

	SDL_Rect	geov;
	float		alphav;

public:
	LayerImage(const char *name,
		   SDL_Rect geo = (SDL_Rect){0, 0, 0, 0},
		   float opacity = 1.,
		   const char *file = NULL);

	static CtorInfo ctor_info;
	static Layer *
	ctor_osc(const char *name, SDL_Rect geo, float opacity, lo_arg **argv)
	{
		return new LayerImage(name, geo, opacity, &argv[0]->s);
	}

	~LayerImage();

	void frame(SDL_Surface *target);

private:
	void geo(SDL_Rect geo);
	void alpha(float opacity);

	void file(const char *file = NULL);
	OSCServer::MethodHandlerId *file_osc_id;
	static void
	file_osc(LayerImage *obj, lo_arg **argv)
	{
		obj->file(&argv[0]->s);
	}
};

/*
 * Macros
 */
#define SDL_IMAGE_ERROR(FMT, ...) do {					\
	fprintf(stderr, "%s(%d): " FMT ": %s\n",			\
		__FILE__, __LINE__, ##__VA_ARGS__, IMG_GetError());	\
} while (0)

#endif
