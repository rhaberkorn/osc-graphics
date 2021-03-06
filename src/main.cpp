#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_framerate.h>

/* HACK: older SDL_gfx versions define GFX_ALPHA_ADJUST in the header */
#define GFX_ALPHA_ADJUST \
	static __attribute__((unused)) GFX_ALPHA_ADJUST
#include <SDL_gfxBlitFunc.h>
#undef GFX_ALPHA_ADJUST

#include "osc_graphics.h"
#include "osc_server.h"
#include "recorder.h"

#include "layer.h"
#include "layer_box.h"
#include "layer_text.h"
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
extern "C" {

int main(int argc, char **argv);
static void cleanup(void);

}

SDL_Surface *screen;

OSCServer	osc_server;
static Recorder	recorder;
LayerList	layers;

int config_dump_osc = 0;
int config_framerate = DEFAULT_FRAMERATE;

void
rgba_blit_with_alpha(SDL_Surface *src_surf, SDL_Surface *dst_surf, Uint8 alpha)
{
	if (alpha == SDL_ALPHA_TRANSPARENT) {
		SDL_FillRect(dst_surf, NULL,
			     SDL_MapRGBA(dst_surf->format,
					 0, 0, 0, SDL_ALPHA_TRANSPARENT));
		return;
	}

	Uint8 *src = (Uint8 *)src_surf->pixels;
	Uint8 *dst = (Uint8 *)dst_surf->pixels;
	SDL_PixelFormat *fmt = src_surf->format;

	int inc = fmt->BytesPerPixel;
	int len = src_surf->w * src_surf->h;

	SDL_MAYBE_LOCK(src_surf);
	SDL_MAYBE_LOCK(dst_surf);

	GFX_DUFFS_LOOP4({
		register Uint32 pixel;
		register int a;

		pixel = *(Uint32 *)src;
		a = ((pixel & fmt->Amask) >> fmt->Ashift) << fmt->Aloss;
		a = (a*alpha)/SDL_ALPHA_OPAQUE;
		a = (a << fmt->Aloss) << fmt->Ashift;
		pixel &= ~fmt->Amask;
		pixel |= a;
		*(Uint32 *)dst = pixel;

		src += inc;
		dst += inc;
	}, len)

	SDL_MAYBE_UNLOCK(dst_surf);
	SDL_MAYBE_UNLOCK(src_surf);
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

	parse_options(argc, argv,
		      port, sdl_flags, show_cursor,
		      width, height, bpp, config_framerate);

	if (SDL_Init(SDL_INIT_VIDEO)) {
		SDL_ERROR("SDL_Init");
		return EXIT_FAILURE;
	}
	atexit(cleanup);

	SDL_WM_SetCaption("OSC Graphics", NULL);

	screen = SDL_SetVideoMode(width, height, bpp, sdl_flags);
	if (screen == NULL) {
		SDL_ERROR("SDL_SetVideoMode");
		return EXIT_FAILURE;
	}

#if DEFAULT_SDL_FLAGS & SDL_HWSURFACE
	if (!(screen->flags & SDL_HWSURFACE))
		WARNING_MSG("Hardware surfaces not available!");
#endif

	SDL_ShowCursor(show_cursor);

	osc_server.open(port);

	recorder.register_methods();

	REGISTER_LAYER(LayerImage);
	REGISTER_LAYER(LayerVideo);
	REGISTER_LAYER(LayerBox);
	REGISTER_LAYER(LayerText);

	osc_server.start();

	SDL_initFramerate(&fpsm);
	SDL_setFramerate(&fpsm, config_framerate);

	for (;;) {
		sdl_process_events();

		layers.render(screen);

		recorder.record(screen);

		SDL_Flip(screen);
		SDL_framerateDelay(&fpsm);
	}

	/* never reached */
	return EXIT_FAILURE;
}

static void
cleanup(void)
{
	SDL_Quit();
}

#if defined(__WIN32__) && defined(main)
#undef main
/*
 * Define a main() symbol, so that libmingw32 won't define it and libSDLmain's
 * WinMain() is not called since that would initiate stdio redirection.
 * Instead we call libSDLmain's console_main() which in turn calls
 * the main() defined above which was renamed to SDL_main by SDL CFLAGS.
 */

extern "C" int console_main(int argc, char *argv[]);

extern "C" int
main(int argc, char *argv[])
{
	return console_main(argc, argv);
}

#endif
