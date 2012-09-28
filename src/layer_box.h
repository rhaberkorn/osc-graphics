#ifndef __LAYER_BOX_H
#define __LAYER_BOX_H

#include <SDL.h>

#include "osc_graphics.h"
#include "osc_server.h"
#include "layer.h"

#define COLOR_TYPES "iii" /* r, g, b */

class LayerBox : public Layer {
	Sint16 x1, y1, x2, y2;
	Uint8 r, g, b, a;

public:
	LayerBox(const char *name, SDL_Rect geo, float opacity,
		 SDL_Color color);

	static CtorInfo ctor_info;
	static Layer *
	ctor_osc(const char *name, SDL_Rect geo, float opacity, lo_arg **argv)
	{
		SDL_Color color = {
			(Uint8)argv[0]->i, (Uint8)argv[1]->i, (Uint8)argv[2]->i
		};
		return new LayerBox(name, geo, opacity, color);
	}

	~LayerBox();

	void frame(SDL_Surface *target);

private:
	void geo(SDL_Rect geo);
	void alpha(float opacity);

	inline void
	color(SDL_Color color)
	{
		r = color.r;
		g = color.g;
		b = color.b;
	}
	OSCServer::MethodHandlerId *color_osc_id;
	static void
	color_osc(LayerBox *obj, lo_arg **argv)
	{
		SDL_Color color = {
			(Uint8)argv[0]->i, (Uint8)argv[1]->i, (Uint8)argv[2]->i
		};
		obj->color(color);
	}
};

#endif