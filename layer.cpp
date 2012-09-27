#include <string.h>
#include <bsd/sys/queue.h>

#include <SDL.h>

#include "osc_graphics.h"
#include "osc_server.h"
#include "layer.h"

Layer::Layer(const char *_name) : mutex(SDL_CreateMutex()), name(strdup(_name))
{
	geo_osc_id = register_method("geo", GEO_TYPES, geo_osc);
	alpha_osc_id = register_method("alpha", "f", alpha_osc);
}

void
LayerList::insert(int pos, Layer *layer)
{
	Layer *cur, *prev = NULL;

	lock();

	LIST_FOREACH(cur, &head, layers) {
		if (!pos--)
			break;
		prev = cur;
	}

	if (prev)
		LIST_INSERT_AFTER(prev, layer, layers);
	else
		LIST_INSERT_HEAD(&head, layer, layers);

	unlock();
}

void
LayerList::delete_layer(Layer *layer)
{
	lock();
	LIST_REMOVE(layer, layers);
	unlock();

	/* layer is guaranteed not to be rendered */
	delete layer;
}

void
LayerList::render(SDL_Surface *target)
{
	SDL_FillRect(target, NULL, SDL_MapRGB(target->format, 0, 0, 0));

	lock();

	Layer *cur;
	LIST_FOREACH(cur, &head, layers) {
		cur->lock();
		cur->frame(target);
		cur->unlock();
	}

	unlock();
}

Layer::~Layer()
{
	unregister_method(alpha_osc_id);
	unregister_method(geo_osc_id);

	free(name);
	SDL_DestroyMutex(mutex);
}
