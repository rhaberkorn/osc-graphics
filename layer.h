#ifndef __HAVE_LAYER_H
#define __HAVE_LAYER_H

#include <string.h>

#include <SDL.h>
#include <SDL_thread.h>

class Layer {
	SDL_mutex *mutex;

public:
	Layer *next;

	char *name;

	Layer(const char *name)
	{
		mutex = SDL_CreateMutex();
		Layer::name = strdup(name);
	}
	~Layer()
	{
		free(name);
		SDL_DestroyMutex(mutex);
	}

	inline void
	lock()
	{
		SDL_LockMutex(mutex);
	}
	inline void
	unlock()
	{
		SDL_UnlockMutex(mutex);
	}

	virtual void geo(SDL_Rect geo) = 0;
	virtual void alpha(float opacity) = 0;

	virtual void frame(SDL_Surface *target) = 0;
};

class LayerList {
	Layer *head;

	SDL_mutex *mutex;

	inline void
	lock()
	{
		SDL_LockMutex(mutex);
	}
	inline void
	unlock()
	{
		SDL_UnlockMutex(mutex);
	}

public:
	LayerList() : head(NULL)
	{
		mutex = SDL_CreateMutex();
	}
	~LayerList()
	{
		SDL_DestroyMutex(mutex);
	}

	void insert(int pos, Layer *layer);
	bool delete_by_name(const char *name);
	void render(SDL_Surface *target);
};

#endif