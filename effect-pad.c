#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>

#define NARRAY(ARRAY) \
	(sizeof(ARRAY) / sizeof(ARRAY[0]))

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

#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480

static SDL_Surface *screen;

static inline SDL_Surface *
effect_fire_init(void)
{
	SDL_Surface *surface;
	static SDL_Color colors[256];

	surface = SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_SRCCOLORKEY,
				       screen->w, screen->h, 8, 0, 0, 0, 0);
	if (surface == NULL) {
		SDL_ERROR("SDL_CreateRGBSurface");
		exit(EXIT_FAILURE);
	}

	if (SDL_SetColorKey(surface, SDL_SRCCOLORKEY, 0)) {
		SDL_ERROR("SDL_SetColorKey");
		SDL_FreeSurface(surface);
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < 32; i++) {
		/* black to blue, 32 values */
		colors[i].b = i << 1;

		/* blue to red, 32 values */
		colors[i + 32].r = i << 3;
		colors[i + 32].b =  64 - (i << 1);

		/*red to yellow, 32 values */
		colors[i + 64].r = 255;
		colors[i + 64].g = i << 3;

		/* yellow to white, 162 */
		colors[i + 96].r = 255;
		colors[i + 96].g = 255;
		colors[i + 96].b = i << 2;
		colors[i + 128].r = 255;
		colors[i + 128].g = 255;
		colors[i + 128].b = 64 + (i << 2);
		colors[i + 160].r = 255;
		colors[i + 160].g = 255;
		colors[i + 160].b = 128 + (i << 2);
		colors[i + 192].r = 255;
		colors[i + 192].g = 255;
		colors[i + 192].b = 192 + i;
		colors[i + 224].r = 255;
		colors[i + 224].g = 255;
		colors[i + 224].b = 224 + i;
	}

	SDL_SetPalette(surface, SDL_LOGPAL | SDL_PHYSPAL, colors,
		       0, NARRAY(colors)); 

	return surface;
}

static inline void
effect_fire_update(SDL_Surface *surface)
{
	int mouse_x, mouse_y;

	SDL_MAYBE_LOCK(surface);

	if (SDL_GetMouseState(&mouse_x, &mouse_y) & SDL_BUTTON(1)) {
		Uint8 *p = (Uint8 *)surface->pixels;
		float r = (float)rand()/RAND_MAX;

		p[mouse_y*surface->pitch + mouse_x] = r > .3 ? 255 : 0;
	}

	for (Uint8 *p = (Uint8 *)surface->pixels + surface->pitch*surface->h - 1;
	     p >= (Uint8 *)surface->pixels + surface->pitch;
	     p--) {
		Uint16 acc;

		if (!((p - (Uint8 *)surface->pixels) % surface->pitch)) {
			/* left edge */
			acc = (Uint16)p[0] + p[1];
			if (acc/2 > p[-surface->pitch])
				p[-surface->pitch] = (acc + p[-surface->pitch])/3;
		} else if (!((p - (Uint8 *)surface->pixels + 1) % surface->pitch)) {
			/* right edge */
			acc = (Uint16)p[-1] + p[0];
			if (acc/2 > p[-surface->pitch])
				p[-surface->pitch] = (acc + p[-surface->pitch])/3;
		} else {
			/* somewhere inbetween */
			acc = (Uint16)p[-1] + p[0] + p[1];
			if (acc/3 > p[-surface->pitch])
				p[-surface->pitch] = (acc + p[-surface->pitch])/4;
		}

		/* decay */
		if (p[-surface->pitch] > 0)
			p[-surface->pitch]--;
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
	SDL_Surface *fire_surface;

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

	fire_surface = effect_fire_init();

	for (;;) {
		process_events();

		SDL_FillRect(screen, NULL,
			     SDL_MapRGB(screen->format, 0, 0, 0));

		effect_fire_update(fire_surface);
		SDL_BlitSurface(fire_surface, NULL, screen, NULL);

		SDL_Flip(screen);
		SDL_Delay(50);
	}

	/* never reached */
	return EXIT_FAILURE;
}
