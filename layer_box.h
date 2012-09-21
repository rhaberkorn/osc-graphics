#ifndef __LAYER_BOX_H
#define __LAYER_BOX_H

#include <SDL.h>

#include "osc_graphics.h"
#include "layer.h"

class LayerBox : public Layer {
	Sint16 x1, y1, x2, y2;
	Uint8 r, g, b, a;

public:
	LayerBox(const char *name, SDL_Rect geo, float opacity,
		 SDL_Color color) : Layer(name)
	{
		LayerBox::geo(geo);
		LayerBox::color(color);
		LayerBox::alpha(opacity);
	}

	void geo(SDL_Rect geo);
	void alpha(float opacity);

	void
	color(SDL_Color color)
	{
		r = color.r;
		g = color.g;
		b = color.b;
	}


	void frame(SDL_Surface *target);
};

#endif