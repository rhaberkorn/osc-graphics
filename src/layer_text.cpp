#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef __WIN32__
#include <windows.h>
#endif

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_rotozoom.h>

#include "osc_graphics.h"
#include "layer_text.h"

Layer::CtorInfo LayerText::ctor_info = {
	"text",
	COLOR_TYPES "ss"	/* r, g, b, text, font file */
};

/*
 * Macros
 */
#define TTF_CLOSEFONT_SAFE(FONT) do {	\
	if (FONT) {			\
		TTF_CloseFont(FONT);	\
		FONT = NULL;		\
	}				\
} while (0)

LayerText::LayerText(const char *name, SDL_Rect geo, float opacity,
		     SDL_Color color, const char *text, const char *file)
		    : Layer(name), ttf_font(NULL), surf_alpha(NULL), surf(NULL),
		      textv(NULL), filev(NULL)
{
	color_osc_id = register_method("color", COLOR_TYPES,
				       (OSCServer::MethodHandlerCb)color_osc);
	text_osc_id = register_method("test", "s",
				      (OSCServer::MethodHandlerCb)text_osc);
	font_osc_id = register_method("font", "s",
				      (OSCServer::MethodHandlerCb)font_osc);
	style_osc_id = register_method("style", "s",
				       (OSCServer::MethodHandlerCb)style_osc);

	if (TTF_Init()) {
		SDL_ERROR("TTF_Init");
		exit(EXIT_FAILURE);
	}

	LayerText::geo(geo);
	LayerText::alpha(opacity);
	LayerText::color(color);
	LayerText::text(text);
	LayerText::font(file);
}

void
LayerText::geo(SDL_Rect geo)
{
	int style = TTF_STYLE_NORMAL;

	geov = geo;
	if (!geov.h)
		geov.h = screen->h;

	if (!filev)
		return;

	if (surf && (!geov.w || geov.w == surf->w) && geov.h == surf->h)
		return;

	if (ttf_font) {
		style = TTF_GetFontStyle(ttf_font);
		TTF_CloseFont(ttf_font);
	}
	ttf_font = TTF_OpenFont(filev, geov.h);

	LayerText::style(style);
}

void
LayerText::alpha(float opacity)
{
	Uint8 alpha = (Uint8)ceilf(opacity*SDL_ALPHA_OPAQUE);

	alphav = opacity;

	if (!surf)
		return;

	if (alpha == SDL_ALPHA_OPAQUE) {
		SDL_FREESURFACE_SAFE(surf_alpha);
		return;
	}

	if (!surf_alpha) {
		surf_alpha = SDL_CreateRGBSurface(surf->flags,
						  surf->w, surf->h,
						  surf->format->BitsPerPixel,
						  surf->format->Rmask,
						  surf->format->Gmask,
						  surf->format->Bmask,
						  surf->format->Amask);
	}

	rgba_blit_with_alpha(surf, surf_alpha, alpha);
}

void
LayerText::color(SDL_Color color)
{
	colorv = color;

	if (!ttf_font)
		return;

	SDL_FREESURFACE_SAFE(surf_alpha);
	SDL_FREESURFACE_SAFE(surf);

	surf = TTF_RenderText_Blended(ttf_font, textv, colorv);

	if (geov.w && surf->w != geov.w) {
		SDL_Surface *new_surf;

		new_surf = zoomSurface(surf, (double)geov.w/surf->w, 1.0,
				       SMOOTHING_ON);
		SDL_FreeSurface(surf);
		surf = new_surf;
	}

	alpha(alphav);
}

#ifdef __WIN32__

void
LayerText::font(const char *file)
{
	free(filev);

	if (PathIsRelative(file)) {
		/* relative path */
		const char *systemroot = getenv("SYSTEMROOT");

		filev = (char *)malloc(strlen(systemroot) + 7 + strlen(file) + 1);
		strcpy(filev, systemroot);
		strcat(filev, "\\fonts\\");
		strcat(filev, file);
	} else {
		/* absolute path */
		filev = strdup(file);
	}

	geo(geov);
}

#else /* assume POSIX */

void
LayerText::font(const char *file)
{
	free(filev);

	if (*file != '/') {
		/* relative path */
		filev = (char *)malloc(sizeof(FONT_PATH) + strlen(file));
		strcpy(filev, FONT_PATH);
		strcat(filev, file);
	} else {
		/* absolute path */
		filev = strdup(file);
	}

	geo(geov);
}

#endif

void
LayerText::style_osc(LayerText *obj, lo_arg **argv)
{
	int style = TTF_STYLE_NORMAL;

	for (char *p = &argv[0]->s; *p; p++) {
		switch (*p) {
		case 'b':
			style |= TTF_STYLE_BOLD;
			break;
		case 'i':
			style |= TTF_STYLE_ITALIC;
			break;
		case 'u':
			style |= TTF_STYLE_UNDERLINE;
			break;
		default:
			break;
		}
	}

	obj->style(style);
}

void
LayerText::frame(SDL_Surface *target)
{
	SDL_Surface *use_surf = surf_alpha ? : surf;
	SDL_Rect dst_rect = {geov.x, geov.y, use_surf->w, use_surf->h};

	SDL_BlitSurface(use_surf, NULL, target, &dst_rect);
}

LayerText::~LayerText()
{
	unregister_method(style_osc_id);
	unregister_method(font_osc_id);
	unregister_method(text_osc_id);
	unregister_method(color_osc_id);

	SDL_FREESURFACE_SAFE(surf);
	SDL_FREESURFACE_SAFE(surf_alpha);

	TTF_CLOSEFONT_SAFE(ttf_font);
}
