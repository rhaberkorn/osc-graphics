#include <math.h>

#include <SDL.h>
#include <SDL_gfxPrimitives.h>

#include "osc_graphics.h"
#include "layer_box.h"

LayerBox::LayerBox(const char *name, SDL_Rect geo, float opacity,
		   SDL_Color color) : Layer(name)
{
	color_osc_id = register_method("color", "iii",
				       (OSCServer::MethodHandlerCb)color_osc);

	LayerBox::geo(geo);
	LayerBox::color(color);
	LayerBox::alpha(opacity);
}

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

LayerBox::~LayerBox()
{
	unregister_method(color_osc_id);
}
