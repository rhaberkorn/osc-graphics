#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_rotozoom.h>

#include "osc_graphics.h"
#include "layer_image.h"

Layer::CtorInfo LayerImage::ctor_info = {"image", "s" /* file */};

/*
 * Macros
 */
#define SDL_IMAGE_ERROR(FMT, ...) do {					\
	fprintf(stderr, "%s(%d): " FMT ": %s\n",			\
		__FILE__, __LINE__, ##__VA_ARGS__, IMG_GetError());	\
} while (0)

LayerImage::LayerImage(const char *name, SDL_Rect geo, float opacity,
		       const char *file) :
		      Layer(name),
		      surf_alpha(NULL), surf_scaled(NULL), surf(NULL)
{
	file_osc_id = register_method("file", "s",
				      (OSCServer::MethodHandlerCb)file_osc);

	LayerImage::alpha(opacity);
	LayerImage::geo(geo);
	LayerImage::file(file);
}

void
LayerImage::geo(SDL_Rect geo)
{
	if (!geo.x && !geo.y && !geo.w && !geo.h)
		geov = (SDL_Rect){0, 0, screen->w, screen->h};
	else
		geov = geo;

	if (!surf)
		return;

	if (surf_scaled &&
	    surf_scaled->w == geov.w && surf_scaled->h == geov.h)
		return;

	SDL_FREESURFACE_SAFE(surf_alpha);
	SDL_FREESURFACE_SAFE(surf_scaled);

	if (surf->w != geov.w || surf->h != geov.h) {
		surf_scaled = zoomSurface(surf,
					  (double)geov.w/surf->w,
					  (double)geov.h/surf->h,
					  SMOOTHING_ON);
	}

	alpha(alphav);
}

void
LayerImage::alpha(float opacity)
{
	SDL_Surface *use_surf = surf_scaled ? : surf;
	Uint8 alpha = (Uint8)ceilf(opacity*SDL_ALPHA_OPAQUE);

	alphav = opacity;

	if (!use_surf)
		return;

	if (!use_surf->format->Amask) {
		if (alpha == SDL_ALPHA_OPAQUE)
			SDL_SetAlpha(use_surf, 0, 0);
		else
			SDL_SetAlpha(use_surf, SDL_SRCALPHA | SDL_RLEACCEL, alpha);

		return;
	}

	if (alpha == SDL_ALPHA_OPAQUE) {
		SDL_FREESURFACE_SAFE(surf_alpha);
		return;
	}

	if (!surf_alpha) {
		surf_alpha = SDL_CreateRGBSurface(use_surf->flags,
						  use_surf->w, use_surf->h,
						  use_surf->format->BitsPerPixel,
						  use_surf->format->Rmask,
						  use_surf->format->Gmask,
						  use_surf->format->Bmask,
						  use_surf->format->Amask);
	}

	rgba_blit_with_alpha(use_surf, surf_alpha, alpha);
}

void
LayerImage::file(const char *file)
{
	SDL_FREESURFACE_SAFE(surf_alpha);
	SDL_FREESURFACE_SAFE(surf_scaled);
	SDL_FREESURFACE_SAFE(surf);

	if (!file || !*file)
		return;

	surf = IMG_Load(file);
	if (!surf) {
		SDL_IMAGE_ERROR("IMG_Load");
		exit(EXIT_FAILURE);
	}

	geo(geov);
}

void
LayerImage::frame(SDL_Surface *target)
{
	if (surf)
		SDL_BlitSurface(surf_alpha ? : surf_scaled ? : surf, NULL,
				target, &geov);
}

LayerImage::~LayerImage()
{
	unregister_method(file_osc_id);

	SDL_FREESURFACE_SAFE(surf_alpha);
	SDL_FREESURFACE_SAFE(surf_scaled);
	SDL_FREESURFACE_SAFE(surf);
}
