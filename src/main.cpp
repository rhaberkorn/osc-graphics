#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_framerate.h>

#include "osc_graphics.h"
#include "osc_server.h"

#include "layer.h"
#include "layer_box.h"
#include "layer_image.h"
#include "layer_video.h"

/*
 * Default values
 */
#define DEFAULT_SCREEN_WIDTH	640		/* pixels */
#define DEFAULT_SCREEN_HEIGHT	480		/* pixels */
#define DEFAULT_SCREEN_BPP	32		/* bits */
#define DEFAULT_SDL_FLAGS	(SDL_HWSURFACE | SDL_DOUBLEBUF)
#define DEFAULT_SHOW_CURSOR	SDL_ENABLE
#define DEFAULT_FRAMERATE	20		/* Hz */
#define DEFAULT_PORT		"7770"		/* port number/service/UNIX socket */

#define BOOL2STR(X) \
	((X) ? "on" : "off")

/*
 * Declarations
 */
SDL_Surface *screen;

OSCServer osc_server;
LayerList layers;

int config_dump_osc = 0;

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

static void
print_help(void)
{
	printf("%s (v%s)\n"
	       "\n"
	       "Usage: osc-server [-h] [-p <port>] [-f] [-c] "
				 "[-W <width>] [-H <height>] "
				 "[-B <bpp>] [-F <framerate>]\n"
	       "Options:\n"
	       "\t-h                 Show this help\n"
	       "\t-p <port>          Listen on port <port> (default: %s)\n"
	       "\t-f                 Toggle fullscreen (default: %s)\n"
	       "\t-c                 Toggle cursor displaying (default: %s)\n"
	       "\t-W <width>         Set screen width (default: %d)\n"
	       "\t-H <height>        Set screen height (default: %d)\n"
	       "\t-B <bpp>           Set screen Bits per Pixel (default: %d)\n"
	       "\t-F <framerate>     Set framerate in Hz (default: %d)\n"
	       "\n"
	       "Homepage: <%s>\n"
	       "E-Mail: <%s>\n",
	       PACKAGE_NAME, PACKAGE_VERSION,
	       DEFAULT_PORT,
	       BOOL2STR(DEFAULT_SDL_FLAGS & SDL_FULLSCREEN),
	       BOOL2STR(DEFAULT_SHOW_CURSOR),
	       DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT,
	       DEFAULT_SCREEN_BPP,
	       DEFAULT_FRAMERATE,
	       PACKAGE_URL, PACKAGE_BUGREPORT);
}

static inline void
parse_options(int argc, char **argv,
	      const char *&port, Uint32 &flags, int &show_cursor,
	      int &width, int &height, int &bpp, int &framerate)
{
	for (int i = 1; i < argc; i++) {
		if (strlen(argv[i]) != 2 || argv[i][0] != '-')
			goto error;

		switch (argv[i][1]) {
		case 'h':
			print_help();
			exit(EXIT_SUCCESS);
		case 'p':
			if (++i == argc)
				goto error;
			port = argv[i];
			break;
		case 'f':
			flags ^= SDL_FULLSCREEN;
			break;
		case 'c':
			show_cursor = !show_cursor;
			break;
		case 'W':
			if (++i == argc)
				goto error;
			width = atoi(argv[i]);
			break;
		case 'H':
			if (++i == argc)
				goto error;
			height = atoi(argv[i]);
			break;
		case 'B':
			if (++i == argc)
				goto error;
			bpp = atoi(argv[i]);
			break;
		case 'F':
			if (++i == argc)
				goto error;
			framerate = atoi(argv[i]);
			break;
		default:
			goto error;
		}
	}

	return;

error:
	print_help();
	exit(EXIT_FAILURE);
}

#define REGISTER_LAYER(CLASS) \
	osc_server.register_layer(CLASS::ctor_info.name, \
				  CLASS::ctor_info.types, \
				  CLASS::ctor_osc)

int
main(int argc, char **argv)
{
	FPSmanager fpsm;

	const char *port	= DEFAULT_PORT;
	Uint32 sdl_flags	= DEFAULT_SDL_FLAGS;
	int show_cursor		= DEFAULT_SHOW_CURSOR;
	int width		= DEFAULT_SCREEN_WIDTH;
	int height		= DEFAULT_SCREEN_HEIGHT;
	int bpp			= DEFAULT_SCREEN_BPP;
	int framerate		= DEFAULT_FRAMERATE;

	parse_options(argc, argv,
		      port, sdl_flags, show_cursor,
		      width, height, bpp, framerate);

#ifdef __WIN32__
	/* disable SDL's stdio redirect */
	freopen("CON", "w", stdout);
	freopen("CON", "w", stderr);
#endif

	if (SDL_Init(SDL_INIT_VIDEO)) {
		SDL_ERROR("SDL_Init");
		return EXIT_FAILURE;
	}
	atexit(SDL_Quit);

	SDL_WM_SetCaption("OSC Graphics", NULL);

	screen = SDL_SetVideoMode(width, height, bpp, sdl_flags);
	if (screen == NULL) {
		SDL_ERROR("SDL_SetVideoMode");
		return EXIT_FAILURE;
	}

#if DEFAULT_SDL_FLAGS & SDL_HWSURFACE
	if (!(screen->flags & SDL_HWSURFACE))
		fprintf(stderr, "Warning: Hardware surfaces not available!\n");
#endif

	SDL_ShowCursor(show_cursor);

	osc_server.open(port);

	REGISTER_LAYER(LayerImage);
	REGISTER_LAYER(LayerVideo);
	REGISTER_LAYER(LayerBox);

	osc_server.start();

	SDL_initFramerate(&fpsm);
	SDL_setFramerate(&fpsm, framerate);

	for (;;) {
		sdl_process_events();

		layers.render(screen);

		SDL_Flip(screen);
		SDL_framerateDelay(&fpsm);
	}

	/* never reached */
	return EXIT_FAILURE;
}
