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
	Layer *cur, **prev;

	lock();

	SLIST_FOREACH_PREVPTR(cur, prev, &head, layers)
		if (!pos--)
			break;

	SLIST_NEXT(layer, layers) = cur;
	*prev = layer;

	unlock();
}

bool
LayerList::delete_by_name(const char *name)
{
	Layer *cur, **prev;

	lock();

	SLIST_FOREACH_PREVPTR(cur, prev, &head, layers)
		if (!strcmp(cur->name, name)) {
			*prev = SLIST_NEXT(cur, layers);
			delete cur;
			break;
		}

	unlock();

	return cur == NULL;
}

void
LayerList::render(SDL_Surface *target)
{
	SDL_FillRect(target, NULL, SDL_MapRGB(target->format, 0, 0, 0));

	lock();

	Layer *cur;
	SLIST_FOREACH(cur, &head, layers) {
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
