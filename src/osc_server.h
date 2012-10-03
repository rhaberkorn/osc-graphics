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

		MethodHandlerId(const char *_types, const char *_path,
				void *_data = NULL) :
			       types(strdup(_types)), path(strdup(_path)),
			       data(_data) {}
		~MethodHandlerId()
		{
			free(types);
			free(path);
		}
	};

private:
	void add_method_v(MethodHandlerId **hnd, const char *types,
			  lo_method_handler handler, void *data,
			  const char *fmt, va_list ap)
			 __attribute__((format(printf, 6, 0)));

public:
	typedef void (*MethodHandlerCb)(Layer *obj, lo_arg **argv);
	typedef Layer *(*CtorHandlerCb)(const char *name, SDL_Rect geo,
					float alpha, lo_arg **argv);

	OSCServer() : server(NULL) {}
	~OSCServer();

	void open(const char *port);

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
		  __attribute__((format(printf, 6, 7)))
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
		  __attribute__((format(printf, 5, 6)))
	{
		va_list ap;
		va_start(ap, fmt);
		add_method_v(NULL, types, handler, data, fmt, ap);
		va_end(ap);
	}

	void del_method(const char *types, const char *fmt, ...)
		       __attribute__((format(printf, 3, 4)));
	inline void
	del_method(MethodHandlerId *hnd)
	{
		del_method(hnd->types, "%s", hnd->path);
		delete hnd;
	}

	void register_layer(const char *name, const char *types,
			    CtorHandlerCb ctor_cb);

	MethodHandlerId *register_method(Layer *layer, const char *method,
					 const char *types,
					 MethodHandlerCb method_cb);
	void unregister_method(MethodHandlerId *hnd);
};

#define GEO_TYPES	"iiii"			/* x, y, width, height */
#define NEW_LAYER_TYPES	"is" GEO_TYPES "f"	/* position, name, GEO, alpha */

#define COLOR_TYPES	"iii"			/* r, g, b */

#endif
