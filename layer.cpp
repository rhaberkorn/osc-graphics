#include <string.h>

#include <SDL.h>

#include "osc_graphics.h"
#include "layer.h"

#define FOREACH_LAYER(VAR) \
	for (Layer *VAR = head; VAR; VAR = VAR->next)

void
LayerList::insert(int pos, Layer *layer)
{
	Layer *cur, *prev = NULL;

	lock();

	for (cur = head; cur && pos; prev = cur, cur = cur->next, pos--);

	layer->next = cur;
	if (prev)
		prev->next = layer;
	else
		head = layer;

	unlock();
}

bool
LayerList::delete_by_name(const char *name)
{
	Layer *prev = NULL;

	lock();

	FOREACH_LAYER(cur) {
		if (!strcmp(cur->name, name)) {
			if (prev)
				prev->next = cur->next;
			else
				head = cur->next;
			delete cur;

			unlock();
			return false;
		}

		prev = cur;
	}

	unlock();

	return true;
}

void
LayerList::render(SDL_Surface *target)
{
	SDL_FillRect(target, NULL, SDL_MapRGB(target->format, 0, 0, 0));

	lock();

	FOREACH_LAYER(cur) {
		cur->lock();
		cur->frame(target);
		cur->unlock();
	}

	unlock();
}
