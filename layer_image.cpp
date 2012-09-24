#include <math.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_rotozoom.h>
#include <SDL_gfxBlitFunc.h>

#include "osc_graphics.h"
#include "layer_image.h"

#if 0

static inline void
rgba_blit_with_alpha(SDL_Surface *src_surf, SDL_Surface *dst_surf, Uint8 alpha)
{
	SDL_FillRect(dst_surf, NULL,
		     SDL_MapRGBA(dst_surf->format,
		     	     	 0, 0, 0, SDL_ALPHA_TRANSPARENT));
	SDL_gfxBlitRGBA(src_surf, NULL, dst_surf, NULL);
	SDL_gfxMultiplyAlpha(dst_surf, alpha);
}

#else

static inline void
rgba_blit_with_alpha(SDL_Surface *src_surf, SDL_Surface *dst_surf, Uint8 alpha)
{
	Uint8 *src = (Uint8 *)src_surf->pixels;
	Uint8 *dst = (Uint8 *)dst_surf->pixels;
	SDL_PixelFormat *fmt = src_surf->format;

	int inc = fmt->BytesPerPixel;
	int len = src_surf->w * src_surf->h;

	SDL_MAYBE_LOCK(src_surf);
	SDL_MAYBE_LOCK(dst_surf);

	GFX_DUFFS_LOOP4({
		register Uint32 pixel;
		register int a;

		pixel = *(Uint32 *)src;
		a = ((pixel & fmt->Amask) >> fmt->Ashift) << fmt->Aloss;
		a = (a*alpha)/SDL_ALPHA_OPAQUE;
		a = (a << fmt->Aloss) << fmt->Ashift;
		pixel &= ~fmt->Amask;
		pixel |= a;
		*(Uint32 *)dst = pixel;

		src += inc;
		dst += inc;
	}, len)

	SDL_MAYBE_UNLOCK(dst_surf);
	SDL_MAYBE_UNLOCK(src_surf);
}

#endif

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

	if (alpha == SDL_ALPHA_TRANSPARENT) {
		SDL_FillRect(surf_alpha, NULL,
			     SDL_MapRGBA(surf_alpha->format,
			     	     	 0, 0, 0, SDL_ALPHA_TRANSPARENT));
	} else {
		rgba_blit_with_alpha(use_surf, surf_alpha, alpha);
	}
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