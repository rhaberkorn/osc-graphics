#include <stdarg.h>
#include <stdio.h>

#include <SDL.h>

#include <lo/lo.h>

#include "osc_graphics.h"
#include "layer.h"

#include "osc_server.h"

/*
 * liblo callbacks
 */
extern "C" {

static void error_handler(int num, const char *msg, const char *path);
static int generic_handler(const char *path, const char *types, lo_arg **argv,
			   int argc, void *data, void *user_data);

static int dtor_generic_handler(const char *path, const char *types,
		       		lo_arg **argv, int argc,
		       		void *data, void *user_data);
static int ctor_generic_handler(const char *path, const char *types,
		       		lo_arg **argv, int argc,
		       		void *data, void *user_data);
static int method_generic_handler(const char *path, const char *types,
		       		  lo_arg **argv, int argc,
		       		  void *data, void *user_data);

}

extern LayerList layers;

static void
error_handler(int num, const char *msg, const char *path)
{
	fprintf(stderr, "liblo server error %d in path %s: %s\n",
		num, path, msg);
}

/* catch any incoming messages and display them. returning 1 means that the
 * message has not been fully handled and the server should try other methods */
static int
generic_handler(const char *path, const char *types, lo_arg **argv,
		int argc, void *data,
		void *user_data __attribute__((unused)))
{
	if (!config_dump_osc)
		return 1;

	printf("path: <%s>\n", path);
	for (int i = 0; i < argc; i++) {
		printf("arg %d '%c' ", i, types[i]);
		lo_arg_pp((lo_type)types[i], argv[i]);
		printf("\n");
	}
	printf("\n");

	return 1;
}

OSCServer::OSCServer(const char *port)
{
	server = lo_server_thread_new(port, error_handler);

	add_method(NULL, generic_handler, NULL, NULL);
}

void
OSCServer::add_method_v(MethodHandlerId **hnd, const char *types,
			lo_method_handler handler, void *data,
			const char *fmt, va_list ap)
{
	char buf[255];

	if (fmt)
		vsnprintf(buf, sizeof(buf), fmt, ap);
	lo_server_thread_add_method(server, fmt ? buf : NULL, types,
				    handler, data);

	if (hnd)
		*hnd = new MethodHandlerId(types, buf, data);
}

void
OSCServer::del_method(const char *types, const char *fmt, ...)
{
	char buf[255];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	lo_server_thread_del_method(server, buf, types);
	va_end(ap);
}

static int
dtor_generic_handler(const char *path,
		     const char *types __attribute__((unused)),
		     lo_arg **argv __attribute__((unused)),
		     int argc __attribute__((unused)),
		     void *data __attribute__((unused)),
		     void *user_data)
{
	Layer *layer = (Layer *)user_data;

	/* FIXME: double-linked list allows more effecient layer delete */
	layers.delete_by_name(layer->name);

	osc_server.del_method("", "%s", path);

	return 0;
}

static int
ctor_generic_handler(const char *path __attribute__((unused)),
		     const char *types __attribute__((unused)),
		     lo_arg **argv, int argc,
		     void *data __attribute__((unused)),
		     void *user_data)
{
	OSCServer::CtorHandlerCb const_cb = (OSCServer::CtorHandlerCb)user_data;
	Layer *layer;

	SDL_Rect geo = {
		(Sint16)argv[2]->i, (Sint16)argv[3]->i,
		(Uint16)argv[4]->i, (Uint16)argv[5]->i
	};

	layer = const_cb(&argv[1]->s, geo, argv[6]->f, argv + 7);
	layers.insert(argv[0]->i, layer);

	osc_server.add_method("", dtor_generic_handler, layer,
			      "/layer/%s/delete", layer->name);

	return 0;
}

void
OSCServer::register_layer(const char *name, const char *types,
			  CtorHandlerCb ctor_cb)
{
	char buf[255];

	snprintf(buf, sizeof(buf), "%s%s", NEW_LAYER_TYPES, types);
	add_method(buf, ctor_generic_handler, (void *)ctor_cb,
		   "/layer/new/%s", name);
}

struct OscMethodDefaultCtx {
	Layer				*layer;
	OSCServer::MethodHandlerCb	method_cb;
};

static int
method_generic_handler(const char *path __attribute__((unused)),
		       const char *types __attribute__((unused)),
		       lo_arg **argv, int argc,
		       void *data __attribute__((unused)),
		       void *user_data)
{
	struct OscMethodDefaultCtx *ctx = (struct OscMethodDefaultCtx *)user_data;

	ctx->layer->lock();
	ctx->method_cb(ctx->layer, argv);
	ctx->layer->unlock();

	return 0;
}

OSCServer::MethodHandlerId *
OSCServer::register_method(Layer *layer, const char *method, const char *types,
			   MethodHandlerCb method_cb)
{
	MethodHandlerId *hnd;
	struct OscMethodDefaultCtx *ctx = new struct OscMethodDefaultCtx;

	ctx->layer = layer;
	ctx->method_cb = method_cb;

	add_method(&hnd, types, method_generic_handler, ctx,
		   "/layer/%s/%s", layer->name, method);

	return hnd;
}

void
OSCServer::unregister_method(MethodHandlerId *hnd)
{
	delete (struct OscMethodDefaultCtx *)hnd->data;
	del_method(hnd);
}

OSCServer::~OSCServer()
{
	lo_server_thread_free(server);
}
