#include <math.h>

#include <SDL.h>
#include <SDL_gfxPrimitives.h>

#include "osc_graphics.h"
#include "layer_box.h"

void
LayerBox::geo(SDL_Rect geo)
{
	x1 = geo.x;
	y1 = geo.y;
	x2 = geo.x + geo.w;
	y2 = geo.y + geo.h;
}

void
LayerBox::alpha(float opacity)
{
	a = (Uint8)ceilf(opacity*SDL_ALPHA_OPAQUE);
}

void
LayerBox::frame(SDL_Surface *target)
{
	boxRGBA(target, x1, y1, x2 ? : target->w, y2 ? : target->h,
		r, g, b, a);
}
