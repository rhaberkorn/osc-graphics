#ifndef __HAVE_LAYER_H
#define __HAVE_LAYER_H

#include <string.h>
#include <bsd/sys/queue.h>

#include <SDL.h>

#include <lo/lo.h>

#include "osc_graphics.h"
#include "osc_server.h"

extern OSCServer osc_server;

class Layer : public Mutex {
public:
	/*
	 * Every derived class must have a static CtorInfo struct "ctor_info"
	 * and a static "ctor_osc" method
	 */
	struct CtorInfo {
		const char *name;
		const char *types;
	};

	LIST_ENTRY(Layer) layers;

	char *name;

	Layer(const char *name);
	virtual ~Layer();

	/*
	 * Frame render method
	 */
	virtual void frame(SDL_Surface *target) = 0;

protected:
	inline OSCServer::MethodHandlerId *
	register_method(const char *method, const char *types,
			OSCServer::MethodHandlerCb method_cb)
	{
		return osc_server.register_method(this, method, types, method_cb);
	}
	inline void
	unregister_method(OSCServer::MethodHandlerId *hnd)
	{
		osc_server.unregister_method(hnd);
	}

	/*
	 * Default methods
	 */
	virtual void geo(SDL_Rect geo) = 0;
	virtual void alpha(float opacity) = 0;

private:
	/*
	 * OSC handler methods
	 */
	OSCServer::MethodHandlerId *geo_osc_id;
	static void
	geo_osc(Layer *obj, lo_arg **argv)
	{
		SDL_Rect geo = {
			(Sint16)argv[0]->i, (Sint16)argv[1]->i,
			(Uint16)argv[2]->i, (Uint16)argv[3]->i
		};
		obj->geo(geo);
	}
	OSCServer::MethodHandlerId *alpha_osc_id;
	static void
	alpha_osc(Layer *obj, lo_arg **argv)
	{
		obj->alpha(argv[0]->f);
	}
};

class LayerList : Mutex {
	LIST_HEAD(layers_head, Layer) head;

public:
	LayerList() : Mutex()
	{
		LIST_INIT(&head);
	}
	~LayerList();

	void insert(int pos, Layer *layer);
	void delete_layer(Layer *layer);
	void render(SDL_Surface *target);
};

#endif