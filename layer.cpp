#include <string.h>
#include <bsd/sys/queue.h>

#include <SDL.h>

#include "osc_graphics.h"
#include "layer.h"

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
