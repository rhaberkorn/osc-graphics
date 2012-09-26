#ifndef __OSC_SERVER_H
#define __OSC_SERVER_H

#include <string.h>
#include <stdarg.h>

#include <SDL.h>

#include <lo/lo.h>

#include "osc_graphics.h"

class Layer;

class OSCServer {
	lo_server_thread server;

public:
	struct MethodHandlerId {
		char	*types;
		char	*path;
		void	*data;

		MethodHandlerId(const char *types, const char *path,
				void *d = NULL) : data(d)
		{
			MethodHandlerId::types = strdup(types);
			MethodHandlerId::path = strdup(path);
		}
		~MethodHandlerId()
		{
			free(types);
			free(path);
		}
	};

private:
	void add_method_v(MethodHandlerId **hnd, const char *types,
			  lo_method_handler handler, void *data,
			  const char *fmt, va_list ap);

public:
	typedef void (*MethodHandlerCb)(Layer *obj, lo_arg **argv);
	typedef Layer *(*ConstructorHandlerCb)(const char *name, SDL_Rect geo,
					       float alpha, lo_arg **argv);

	OSCServer(const char *port);
	~OSCServer();

	inline void
	start()
	{
		lo_server_thread_start(server);
	}
	inline void
	stop()
	{
		lo_server_thread_stop(server);
	}

	inline void
	add_method(MethodHandlerId **hnd, const char *types,
		   lo_method_handler handler, void *data,
		   const char *fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		add_method_v(hnd, types, handler, data, fmt, ap);
		va_end(ap);
	}
	inline void
	add_method(const char *types,
		   lo_method_handler handler, void *data,
		   const char *fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		add_method_v(NULL, types, handler, data, fmt, ap);
		va_end(ap);
	}

	void del_method(const char *types, const char *fmt, ...);
	inline void
	del_method(MethodHandlerId *hnd)
	{
		del_method(hnd->types, "%s", hnd->path);
		delete hnd;
	}

	MethodHandlerId *register_method(Layer *layer, const char *method,
					 const char *types,
					 MethodHandlerCb method_cb);
	void unregister_method(MethodHandlerId *hnd);
};

#define GEO_TYPES	"iiii"			/* x, y, width, height */
#define NEW_LAYER_TYPES	"is" GEO_TYPES "f"	/* position, name, GEO, alpha */

#endif