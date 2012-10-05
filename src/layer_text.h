#ifndef __LAYER_TEXT_H
#define __LAYER_TEXT_H

#include <string.h>
#include <stdlib.h>

#include <SDL.h>
#include <SDL_ttf.h>

#include "osc_graphics.h"
#include "osc_server.h"
#include "layer.h"

class LayerText : public Layer {
	TTF_Font	*ttf_font;

	SDL_Surface	*surf_alpha;	/* with per-surface alpha */
	SDL_Surface	*surf;		/* original text (possibly scaled) */

	char		*textv;
	char		*filev;
	SDL_Color	colorv;
	SDL_Rect	geov;
	float		alphav;

public:
	LayerText(const char *name, SDL_Rect geo, float opacity,
		  SDL_Color color, const char *text, const char *file);

	static CtorInfo ctor_info;
	static Layer *
	ctor_osc(const char *name, SDL_Rect geo, float opacity, lo_arg **argv)
	{
		SDL_Color color = {
			(Uint8)argv[0]->i, (Uint8)argv[1]->i, (Uint8)argv[2]->i
		};
		return new LayerText(name, geo, opacity, color,
				     &argv[3]->s, &argv[4]->s);
	}

	~LayerText();

	void frame(SDL_Surface *target);

private:
	void geo(SDL_Rect geo);
	void alpha(float opacity);

	void color(SDL_Color color);
	OSCServer::MethodHandlerId *color_osc_id;
	static void
	color_osc(LayerText *obj, lo_arg **argv)
	{
		SDL_Color color = {
			(Uint8)argv[0]->i, (Uint8)argv[1]->i, (Uint8)argv[2]->i
		};
		obj->color(color);
	}

	inline void
	text(const char *text)
	{
		free(textv);
		textv = strdup(text);

		color(colorv);
	}
	OSCServer::MethodHandlerId *text_osc_id;
	static void
	text_osc(LayerText *obj, lo_arg **argv)
	{
		obj->text(&argv[0]->s);
	}

	void font(const char *file);
	OSCServer::MethodHandlerId *font_osc_id;
	static void
	font_osc(LayerText *obj, lo_arg **argv)
	{
		obj->font(&argv[0]->s);
	}

	inline void
	style(int style)
	{
		TTF_SetFontStyle(ttf_font, style);

		color(colorv);
	}
	OSCServer::MethodHandlerId *style_osc_id;
	static void style_osc(LayerText *obj, lo_arg **argv);
};

#endif