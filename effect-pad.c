#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_framerate.h>
#include <SDL_rotozoom.h>

//#define EFFECT_FIRE_COLORKEYED

#define NARRAY(ARRAY) \
	(sizeof(ARRAY) / sizeof((ARRAY)[0]))

#define SDL_MAYBE_LOCK(SURFACE) do {		\
	if (SDL_MUSTLOCK(SURFACE))		\
		SDL_LockSurface(SURFACE);	\
} while (0)

#define SDL_MAYBE_UNLOCK(SURFACE) do {		\
	if (SDL_MUSTLOCK(SURFACE))		\
		SDL_UnlockSurface(SURFACE);	\
} while (0)

#define SDL_ERROR(FMT, ...) do {					\
	fprintf(stderr, "%s(%d): " FMT ": %s\n",			\
		__FILE__, __LINE__, ##__VA_ARGS__, SDL_GetError());	\
} while (0)

#define SDL_IMAGE_ERROR(FMT, ...) do {					\
	fprintf(stderr, "%s(%d): " FMT ": %s\n",			\
		__FILE__, __LINE__, ##__VA_ARGS__, IMG_GetError());	\
} while (0)

#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480
#define FRAMERATE	20 /* Hz */

static SDL_Surface *screen;

static SDL_Color colors[256];

#define FIRE_DECAY(C) \
	((C) > 0 ? (C) - 1 : 0)

static inline SDL_Surface *
effect_fire_init(void)
{
	Uint32 flags = SDL_HWSURFACE;
	SDL_Surface *surface;

#ifdef EFFECT_FIRE_COLORKEYED
	surface = SDL_CreateRGBSurface(flags, screen->w, screen->h, 8,
				       0, 0, 0, 0);
#else
	flags |= SDL_SRCALPHA;
	surface = SDL_CreateRGBSurface(flags, screen->w, screen->h, 32,
				       0x000000FF, 0x0000FF00,
				       0x00FF0000, 0xFF000000);
#endif
	if (surface == NULL) {
		SDL_ERROR("SDL_CreateRGBSurface");
		exit(EXIT_FAILURE);
	}

#ifdef EFFECT_FIRE_COLORKEYED
	if (SDL_SetColorKey(surface, SDL_SRCCOLORKEY, 0)) {
		SDL_ERROR("SDL_SetColorKey");
		SDL_FreeSurface(surface);
		exit(EXIT_FAILURE);
	}
#endif

	memset(colors, 0, sizeof(colors));

	for (int i = 0; i < 32; i++) {
#ifdef EFFECT_FIRE_COLORKEYED
		/* black to blue, 32 values */
		colors[i].b = i << 1;
#else
		/* transparent to blue, 32 values */
		colors[i].b = 255;
		colors[i].unused = i << 3;
#endif

		/* blue to red, 32 values */
		colors[i + 32].r = i << 3;
		colors[i + 32].b = 64 - (i << 1);
		colors[i + 32].unused = SDL_ALPHA_OPAQUE;

		/*red to yellow, 32 values */
		colors[i + 64].r = 255;
		colors[i + 64].g = i << 3;
		colors[i + 64].unused = SDL_ALPHA_OPAQUE;

		/* yellow to white, 162 */
		colors[i + 96].r = 255;
		colors[i + 96].g = 255;
		colors[i + 96].b = i << 2;
		colors[i + 96].unused = SDL_ALPHA_OPAQUE;
		colors[i + 128].r = 255;
		colors[i + 128].g = 255;
		colors[i + 128].b = 64 + (i << 2);
		colors[i + 128].unused = SDL_ALPHA_OPAQUE;
		colors[i + 160].r = 255;
		colors[i + 160].g = 255;
		colors[i + 160].b = 128 + (i << 2);
		colors[i + 160].unused = SDL_ALPHA_OPAQUE;
		colors[i + 192].r = 255;
		colors[i + 192].g = 255;
		colors[i + 192].b = 192 + i;
		colors[i + 192].unused = SDL_ALPHA_OPAQUE;
		colors[i + 224].r = 255;
		colors[i + 224].g = 255;
		colors[i + 224].b = 224 + i;
		colors[i + 224].unused = SDL_ALPHA_OPAQUE;
	}

#ifdef EFFECT_FIRE_COLORKEYED
	SDL_SetPalette(surface, SDL_LOGPAL | SDL_PHYSPAL, colors,
		       0, NARRAY(colors));
#else
	SDL_FillRect(surface, NULL,
		     SDL_MapRGBA(surface->format,
		     	     	 colors[0].r, colors[0].g,
		     	     	 colors[0].b, colors[0].unused));
#endif

	return surface;
}

#ifdef EFFECT_FIRE_COLORKEYED

static inline Uint8
effect_fire_get_color(SDL_Surface *surface, int x, int y)
{
	return ((Uint8 *)surface->pixels)[surface->w*y + x];
}

static inline void
effect_fire_set_color(SDL_Surface *surface, int x, int y, Uint8 color)
{
	((Uint8 *)surface->pixels)[surface->w*y + x] = color;
}

#else

static inline Uint8
effect_fire_get_color(SDL_Surface *surface, int x, int y)
{
	SDL_Color color;

	/* alpha-channel surface */
	SDL_GetRGBA(((Uint32 *)surface->pixels)[surface->w*y + x],
		    surface->format, &color.r, &color.g, &color.b, &color.unused);
	for (Uint8 i = 0; i < NARRAY(colors); i++)
		if (!memcmp(colors + i, &color, sizeof(color)))
			return i;

	/* shouldn't be reached */
	return 0;
}

static inline void
effect_fire_set_color(SDL_Surface *surface, int x, int y, Uint8 color)
{
	((Uint32 *)surface->pixels)[surface->w*y + x] =
		SDL_MapRGBA(surface->format,
			    colors[color].r, colors[color].g,
			    colors[color].b, colors[color].unused);
}

#endif

static inline void
effect_fire_update(SDL_Surface *surface)
{
	int mouse_x, mouse_y;

	SDL_MAYBE_LOCK(surface);

	if (SDL_GetMouseState(&mouse_x, &mouse_y) & SDL_BUTTON(1))
		effect_fire_set_color(surface, mouse_x, mouse_y,
				      (float)rand()/RAND_MAX > .3 ? 255 : 0);

	for (int y = surface->h - 1; y > 0; y--) {
		Uint16	acc;
		Uint8	color;

		/* right edge */
		acc = (Uint16)effect_fire_get_color(surface, surface->w - 2, y)
		    + effect_fire_get_color(surface, surface->w - 1, y);
		color = effect_fire_get_color(surface, surface->w - 1, y - 1);
		if (acc/2 > color)
			color = (Uint8)((acc + color)/3);
		effect_fire_set_color(surface, surface->w - 1, y - 1,
				      FIRE_DECAY(color));

		/* center */
		for (int x = surface->w - 2; x > 0; x--) {
			acc = (Uint16)effect_fire_get_color(surface, x - 1, y)
			    + effect_fire_get_color(surface, x, y)
			    + effect_fire_get_color(surface, x + 1, y);
			color = effect_fire_get_color(surface, x, y - 1);
			if (acc/3 > color)
				color = (Uint8)((acc + color)/4);
			effect_fire_set_color(surface, x, y - 1, FIRE_DECAY(color));
		}

		/* left edge */
		acc = (Uint16)effect_fire_get_color(surface, 0, y)
		    + effect_fire_get_color(surface, 1, y);
		color = effect_fire_get_color(surface, 0, y - 1);
		if (acc/2 > color)
			color = (Uint8)((acc + color)/3);
		effect_fire_set_color(surface, 0, y - 1, FIRE_DECAY(color));
	}

	SDL_MAYBE_UNLOCK(surface);
}

static inline void
process_events(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) {
			case SDLK_f:
				if (!SDL_WM_ToggleFullScreen(screen)) {
					SDL_ERROR("SDL_WM_ToggleFullScreen");
					exit(EXIT_FAILURE);
				}
				break;

			case SDLK_m:
				SDL_ShowCursor(!SDL_ShowCursor(SDL_QUERY));
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
	FPSmanager	fpsm;

	SDL_Surface	*fire_surface;
	SDL_Surface	*image_surface;

	if (SDL_Init(SDL_INIT_VIDEO)) {
		SDL_ERROR("SDL_Init");
		return EXIT_FAILURE;
	}
	atexit(SDL_Quit);

	SDL_WM_SetCaption("Effect Pad", NULL);

	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32,
				  SDL_HWSURFACE | SDL_DOUBLEBUF);
	if (screen == NULL) {
		SDL_ERROR("SDL_SetVideoMode");
		return EXIT_FAILURE;
	}

	SDL_initFramerate(&fpsm);
	SDL_setFramerate(&fpsm, FRAMERATE);

	image_surface = IMG_Load("image_1.jpg");
	if (image_surface == NULL) {
		SDL_IMAGE_ERROR("IMG_Load");
		return EXIT_FAILURE;
	}

	if (image_surface->w != screen->w || image_surface->h != screen->h) {
		SDL_Surface *new;

		new = zoomSurface(image_surface,
				  (double)screen->w/image_surface->w,
				  (double)screen->h/image_surface->h,
				  SMOOTHING_ON);
		SDL_FreeSurface(image_surface);
		image_surface = new;
	}

	fire_surface = effect_fire_init();

	for (;;) {
		process_events();

		SDL_FillRect(screen, NULL,
			     SDL_MapRGB(screen->format, 0, 0, 0));

		SDL_BlitSurface(image_surface, NULL, screen, NULL);

		effect_fire_update(fire_surface);
		SDL_BlitSurface(fire_surface, NULL, screen, NULL);

		SDL_Flip(screen);
		SDL_framerateDelay(&fpsm);
	}

	/* never reached */
	return EXIT_FAILURE;
}
