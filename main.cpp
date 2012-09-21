#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_framerate.h>

#include <lo/lo.h>

#include "osc_graphics.h"

#include "layer.h"
#include "layer_box.h"
#include "layer_image.h"
#include "layer_video.h"

#define GEO_TYPES	"iiii"			/* x, y, width, height */
#define NEW_LAYER_TYPES	"is" GEO_TYPES "f"	/* position, name, GEO, alpha */
#define COLOR_TYPES	"iii"			/* r, g, b */

/*
 * Default values
 */
#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480
#define FRAMERATE	20 /* Hz */

/*
 * Declarations
 */
SDL_Surface *screen;

static LayerList layers;

static int config_dump_osc = 0;

static void
lo_server_thread_add_method_v(lo_server_thread server, const char *types,
		       	      lo_method_handler handler, void *data,
		       	      const char *fmt, ...)
{
	char buf[255];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	lo_server_thread_add_method(server, buf, types, handler, data);
	va_end(ap);
}

static void
lo_server_thread_del_method_v(lo_server_thread server, const char *types,
			      const char *fmt, ...)
{
	char buf[255];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	lo_server_thread_del_method(server, buf, types);
	va_end(ap);
}

static void
osc_error(int num, const char *msg, const char *path)
{
	fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, msg);
}

/* catch any incoming messages and display them. returning 1 means that the
 * message has not been fully handled and the server should try other methods */
static int
osc_generic_handler(const char *path, const char *types, lo_arg **argv,
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

static inline char *
get_layer_name_from_path(const char *path)
{
	/* path is /layer/[name]/... */
	char *name = strdup(path + 7);
	*strchr(name, '/') = '\0';

	return name;
}

static int
osc_layer_delete(const char *path, const char *types, lo_arg **argv,
		 int argc, void *data, void *user_data)
{
	lo_server_thread server = user_data;
	char *name;

	name = get_layer_name_from_path(path);
	layers.delete_by_name(name);
	free(name);

	lo_server_thread_del_method(server, path, types);

	return 1;
}

static inline void
osc_add_layer_delete(lo_server_thread server, const char *name,
		     lo_method_handler custom_delete)
{
	if (custom_delete)
		lo_server_thread_add_method_v(server, "", custom_delete, server,
				              "/layer/%s/delete", name);
	lo_server_thread_add_method_v(server, "", osc_layer_delete, server,
			              "/layer/%s/delete", name);
}

static int
osc_image_geo(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data, void *user_data)
{
	LayerImage *ctx = (LayerImage *)user_data;

	ctx->lock();
	ctx->geo((SDL_Rect){argv[0]->i, argv[1]->i, argv[2]->i, argv[3]->i});
	ctx->unlock();
	return 0;
}

static int
osc_image_alpha(const char *path, const char *types, lo_arg **argv,
	        int argc, void *data, void *user_data)
{
	LayerImage *ctx = (LayerImage *)user_data;

	ctx->lock();
	ctx->alpha(argv[0]->f);
	ctx->unlock();
	return 0;
}

static int
osc_image_file(const char *path, const char *types, lo_arg **argv,
	       int argc, void *data, void *user_data)
{
	LayerImage *ctx = (LayerImage *)user_data;

	ctx->lock();
	ctx->file(&argv[0]->s);
	ctx->unlock();
	return 0;
}

static int
osc_image_delete(const char *path, const char *types, lo_arg **argv,
		 int argc, void *data, void *user_data)
{
	lo_server_thread server = (lo_server_thread)user_data;
	char *name;

	name = get_layer_name_from_path(path);

	lo_server_thread_del_method_v(server, GEO_TYPES, "/layer/%s/geo", name);
	lo_server_thread_del_method_v(server, "f", "/layer/%s/alpha", name);
	lo_server_thread_del_method_v(server, "s", "/layer/%s/file", name);

	free(name);

	lo_server_thread_del_method(server, path, types);

	return 1;
}

static int
osc_image_new(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data, void *user_data)
{
	lo_server_thread server = (lo_server_thread)user_data;
	SDL_Rect geo = {
		(Sint16)argv[2]->i, (Sint16)argv[3]->i,
		(Uint16)argv[4]->i, (Uint16)argv[5]->i
	};

	LayerImage *ctx = new LayerImage(&argv[1]->s, geo, argv[6]->f, &argv[7]->s);

	layers.insert(argv[0]->i, ctx);

	lo_server_thread_add_method_v(server, GEO_TYPES, osc_image_geo, ctx,
			              "/layer/%s/geo", &argv[1]->s);
	lo_server_thread_add_method_v(server, "f", osc_image_alpha, ctx,
			              "/layer/%s/alpha", &argv[1]->s);
	lo_server_thread_add_method_v(server, "s", osc_image_file, ctx,
			              "/layer/%s/file", &argv[1]->s);

	osc_add_layer_delete(server, &argv[1]->s, osc_image_delete);

	return 0;
}

static int
osc_video_geo(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data, void *user_data)
{
	LayerVideo *ctx = (LayerVideo *)user_data;

	ctx->lock();
	ctx->geo((SDL_Rect){argv[0]->i, argv[1]->i, argv[2]->i, argv[3]->i});
	ctx->unlock();
	return 0;
}

static int
osc_video_alpha(const char *path, const char *types, lo_arg **argv,
	        int argc, void *data, void *user_data)
{
	LayerVideo *ctx = (LayerVideo *)user_data;

	ctx->lock();
	ctx->alpha(argv[0]->f);
	ctx->unlock();
	return 0;
}

static int
osc_video_url(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data, void *user_data)
{
	LayerVideo *ctx = (LayerVideo *)user_data;

	ctx->lock();
	ctx->url(&argv[0]->s);
	ctx->unlock();
	return 0;
}

static int
osc_video_rate(const char *path, const char *types, lo_arg **argv,
	       int argc, void *data, void *user_data)
{
	LayerVideo *ctx = (LayerVideo *)user_data;

	ctx->lock();
	ctx->rate(argv[0]->f);
	ctx->unlock();
	return 0;
}

static int
osc_video_position(const char *path, const char *types, lo_arg **argv,
		   int argc, void *data, void *user_data)
{
	LayerVideo *ctx = (LayerVideo *)user_data;

	ctx->lock();
	ctx->position(argv[0]->f);
	ctx->unlock();
	return 0;
}

static int
osc_video_paused(const char *path, const char *types, lo_arg **argv,
		 int argc, void *data, void *user_data)
{
	LayerVideo *ctx = (LayerVideo *)user_data;

	ctx->lock();
	ctx->paused(argv[0]->i);
	ctx->unlock();
	return 0;
}

static int
osc_video_delete(const char *path, const char *types, lo_arg **argv,
		 int argc, void *data, void *user_data)
{
	lo_server_thread server = (lo_server_thread)user_data;
	char *name;

	name = get_layer_name_from_path(path);

	lo_server_thread_del_method_v(server, GEO_TYPES, "/layer/%s/geo", name);
	lo_server_thread_del_method_v(server, "f", "/layer/%s/alpha", name);
	lo_server_thread_del_method_v(server, "s", "/layer/%s/url", name);
	lo_server_thread_del_method_v(server, "f", "/layer/%s/rate", name);
	lo_server_thread_del_method_v(server, "f", "/layer/%s/position", name);
	lo_server_thread_del_method_v(server, "i", "/layer/%s/paused", name);

	free(name);

	lo_server_thread_del_method(server, path, types);

	return 1;
}

static int
osc_video_new(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data, void *user_data)
{
	lo_server_thread server = (lo_server_thread)user_data;
	SDL_Rect geo = {
		(Sint16)argv[2]->i, (Sint16)argv[3]->i,
		(Uint16)argv[4]->i, (Uint16)argv[5]->i
	};

	LayerVideo *ctx = new LayerVideo(&argv[1]->s, geo, argv[6]->f, &argv[7]->s);

	layers.insert(argv[0]->i, ctx);

	lo_server_thread_add_method_v(server, GEO_TYPES, osc_video_geo, ctx,
			       	      "/layer/%s/geo", &argv[1]->s);
	lo_server_thread_add_method_v(server, "f", osc_video_alpha, ctx,
			       	      "/layer/%s/alpha", &argv[1]->s);
	lo_server_thread_add_method_v(server, "s", osc_video_url, ctx,
			              "/layer/%s/url", &argv[1]->s);
	lo_server_thread_add_method_v(server, "f", osc_video_rate, ctx,
			              "/layer/%s/rate", &argv[1]->s);
	lo_server_thread_add_method_v(server, "f", osc_video_position, ctx,
			              "/layer/%s/position", &argv[1]->s);
	lo_server_thread_add_method_v(server, "i", osc_video_paused, ctx,
			              "/layer/%s/paused", &argv[1]->s);
	osc_add_layer_delete(server, &argv[1]->s, osc_video_delete);

	return 0;
}

static int
osc_box_geo(const char *path, const char *types, lo_arg **argv,
	    int argc, void *data, void *user_data)
{
	LayerBox *ctx = (LayerBox *)user_data;

	ctx->lock();
	ctx->geo((SDL_Rect){argv[0]->i, argv[1]->i, argv[2]->i, argv[3]->i});
	ctx->unlock();
	return 0;
}

static int
osc_box_alpha(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data, void *user_data)
{
	LayerBox *ctx = (LayerBox *)user_data;

	ctx->lock();
	ctx->alpha(argv[0]->f);
	ctx->unlock();
	return 0;
}

static int
osc_box_color(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data, void *user_data)
{
	LayerBox *ctx = (LayerBox *)user_data;

	ctx->lock();
	ctx->color((SDL_Color){argv[0]->i, argv[1]->i, argv[2]->i});
	ctx->unlock();
	return 0;
}

static int
osc_box_delete(const char *path, const char *types, lo_arg **argv,
	       int argc, void *data, void *user_data)
{
	lo_server_thread server = (lo_server_thread)user_data;
	char *name;

	name = get_layer_name_from_path(path);

	lo_server_thread_del_method_v(server, GEO_TYPES, "/layer/%s/geo", name);
	lo_server_thread_del_method_v(server, "f", "/layer/%s/alpha", name);
	lo_server_thread_del_method_v(server, COLOR_TYPES, "/layer/%s/color", name);

	free(name);

	lo_server_thread_del_method(server, path, types);

	return 1;
}

static int
osc_box_new(const char *path, const char *types, lo_arg **argv,
	    int argc, void *data, void *user_data)
{
	lo_server_thread server = (lo_server_thread)user_data;
	SDL_Rect geo = {
		(Sint16)argv[2]->i, (Sint16)argv[3]->i,
		(Uint16)argv[4]->i, (Uint16)argv[5]->i
	};
	SDL_Color color = {
		(Uint8)argv[7]->i, (Uint8)argv[8]->i, (Uint8)argv[9]->i
	};

	LayerBox *ctx = new LayerBox(&argv[1]->s, geo, argv[6]->f, color);

	layers.insert(argv[0]->i, ctx);

	lo_server_thread_add_method_v(server, GEO_TYPES, osc_box_geo, ctx,
			       	      "/layer/%s/geo", &argv[1]->s);
	lo_server_thread_add_method_v(server, "f", osc_box_alpha, ctx,
			       	      "/layer/%s/alpha", &argv[1]->s);
	lo_server_thread_add_method_v(server, COLOR_TYPES, osc_box_color, ctx,
			       	      "/layer/%s/color", &argv[1]->s);

	osc_add_layer_delete(server, &argv[1]->s, osc_box_delete);

	return 0;
}

static inline lo_server_thread
osc_init(const char *port)
{
	lo_server_thread server = lo_server_thread_new(port, osc_error);

	lo_server_thread_add_method(server, NULL, NULL, osc_generic_handler, NULL);

	lo_server_thread_add_method(server, "/layer/new/image", NEW_LAYER_TYPES "s",
			     	    osc_image_new, server);
	lo_server_thread_add_method(server, "/layer/new/video", NEW_LAYER_TYPES "s",
			     	    osc_video_new, server);
	lo_server_thread_add_method(server, "/layer/new/box", NEW_LAYER_TYPES COLOR_TYPES,
			     	    osc_box_new, server);

	return server;
}

static inline void
sdl_process_events(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) {
			case SDLK_F11:
				if (!SDL_WM_ToggleFullScreen(screen)) {
					SDL_ERROR("SDL_WM_ToggleFullScreen");
					exit(EXIT_FAILURE);
				}
				break;

			case SDLK_F10:
				SDL_ShowCursor(!SDL_ShowCursor(SDL_QUERY));
				break;

			case SDLK_F9:
				config_dump_osc ^= 1;
				break;

			case SDLK_ESCAPE:
				exit(EXIT_SUCCESS);

			default:
				break;
			}
			break;

		case SDL_QUIT:
			exit(EXIT_SUCCESS);

		default:
			break;
		}
	}
}

int
main(int argc, char **argv)
{
	lo_server_thread osc_server;
	FPSmanager fpsm;

	osc_server = osc_init("7770");
	if (!osc_server)
		return EXIT_FAILURE;

	if (SDL_Init(SDL_INIT_VIDEO)) {
		SDL_ERROR("SDL_Init");
		return EXIT_FAILURE;
	}
	atexit(SDL_Quit);

	SDL_WM_SetCaption("OSC Graphics", NULL);

	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32,
				  SDL_HWSURFACE | SDL_DOUBLEBUF);
	if (screen == NULL) {
		SDL_ERROR("SDL_SetVideoMode");
		return EXIT_FAILURE;
	}

	lo_server_thread_start(osc_server);

	SDL_initFramerate(&fpsm);
	SDL_setFramerate(&fpsm, FRAMERATE);

	for (;;) {
		sdl_process_events();

		layers.render(screen);

		SDL_Flip(screen);
		SDL_framerateDelay(&fpsm);
	}

	/* never reached */
	return EXIT_FAILURE;
}
