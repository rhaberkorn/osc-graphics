#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_framerate.h>
#include <SDL_rotozoom.h>
#include <SDL_gfxPrimitives.h>

#include <vlc/vlc.h>

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

static SDL_Surface *image_surface = NULL;

static void
effect_image_change(const char *file)
{
	if (image_surface != NULL)
		SDL_FreeSurface(image_surface);

	if (file == NULL) {
		image_surface = NULL;
		return;
	}

	image_surface = IMG_Load(file);
	if (image_surface == NULL) {
		SDL_IMAGE_ERROR("IMG_Load");
		exit(EXIT_FAILURE);
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
}

static struct EffectVideoCtx {
	libvlc_instance_t *vlcinst;
	libvlc_media_player_t *mp;

	SDL_Surface *surf;
	SDL_mutex *mutex;
} effect_video_ctx;

static void *
effect_video_lock(void *data, void **p_pixels)
{
	struct EffectVideoCtx *ctx = data;

	SDL_LockMutex(ctx->mutex);
	SDL_LockSurface(ctx->surf);
	*p_pixels = ctx->surf->pixels;

	return NULL; /* picture identifier, not needed here */
}

static void
effect_video_unlock(void *data, void *id, void *const *p_pixels)
{
    struct EffectVideoCtx *ctx = data;

    SDL_UnlockSurface(ctx->surf);
    SDL_UnlockMutex(ctx->mutex);

    assert(id == NULL); /* picture identifier, not needed here */
}

static void
effect_video_display(void *data __attribute__((unused)), void *id)
{
	/* VLC wants to display the video */
	assert(id == NULL);
}

static inline void
effect_video_init(void)
{
	char const *vlc_argv[] = {
		"--no-audio", /* skip any audio track */
		"--no-xlib", /* tell VLC to not use Xlib */
	};

	effect_video_ctx.surf = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h,
						     16, 0x001f, 0x07e0, 0xf800, 0);
	effect_video_ctx.mutex = SDL_CreateMutex();

	effect_video_ctx.vlcinst = libvlc_new(NARRAY(vlc_argv), vlc_argv);
}

static void
effect_video_change(const char *file, SDL_Color *key)
{
	libvlc_media_t *m;

	if (effect_video_ctx.mp != NULL)
		libvlc_media_player_release(effect_video_ctx.mp);

	if (file == NULL) {
		effect_video_ctx.mp = NULL;
		return;
	}

	if (key == NULL)
		SDL_SetColorKey(effect_video_ctx.surf, 0, 0);
	else
		SDL_SetColorKey(effect_video_ctx.surf, SDL_SRCCOLORKEY,
				SDL_MapRGB(effect_video_ctx.surf->format,
					   key->r, key->g, key->b));

	m = libvlc_media_new_location(effect_video_ctx.vlcinst, file);
	effect_video_ctx.mp = libvlc_media_player_new_from_media(m);
	libvlc_media_release(m);

	libvlc_video_set_callbacks(effect_video_ctx.mp,
				   effect_video_lock, effect_video_unlock, effect_video_display,
				   &effect_video_ctx);
	libvlc_video_set_format(effect_video_ctx.mp, "RV16",
				effect_video_ctx.surf->w,
				effect_video_ctx.surf->h,
				effect_video_ctx.surf->w*2);
	libvlc_media_player_play(effect_video_ctx.mp);
}

static SDL_Color effect_bg_color = {0, 0, 0};

static inline void
effect_bg_change(SDL_Color color)
{
	effect_bg_color = color;
}

static inline void
process_events(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			if (event.key.keysym.mod & KMOD_LALT) {
				switch (event.key.keysym.sym) {
				case SDLK_0:
					effect_bg_change((SDL_Color){0, 0, 0});
					break;
				case SDLK_1:
					effect_bg_change((SDL_Color){255, 0, 0});
					break;
				case SDLK_2:
					effect_bg_change((SDL_Color){0, 255, 0});
					break;
				case SDLK_3:
					effect_bg_change((SDL_Color){0, 0, 255});
					break;
				default:
					break;
				}
			} else if (event.key.keysym.mod & KMOD_RCTRL) {
				switch (event.key.keysym.sym) {
				case SDLK_0:
					effect_video_change(NULL, NULL);
					break;
				case SDLK_1:
					effect_video_change("v4l2://", &(SDL_Color){0, 0, 0});
					break;
				case SDLK_2:
					effect_video_change("/mnt/data/movies/Godzilla.-.1967.-.Frankenstein.jagt.Godzillas.Sohn.KHPP.avi",
							    &(SDL_Color){255, 255, 255});
					break;
				default:
					break;
				}
			} else {
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

				case SDLK_ESCAPE:
					exit(EXIT_SUCCESS);

				case SDLK_0:
					effect_image_change(NULL);
					break;
				case SDLK_1:
					effect_image_change("image_1.jpg");
					break;
				case SDLK_2:
					effect_image_change("image_2.png");
					break;

				default:
					break;
				}
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

	SDL_initFramerate(&fpsm);
	SDL_setFramerate(&fpsm, FRAMERATE);

	effect_video_init();

	for (;;) {
		process_events();

		SDL_FillRect(screen, NULL,
			     SDL_MapRGB(screen->format,
					effect_bg_color.r,
					effect_bg_color.g,
					effect_bg_color.b));

		if (effect_video_ctx.mp != NULL) {
			SDL_LockMutex(effect_video_ctx.mutex);
			SDL_BlitSurface(effect_video_ctx.surf, NULL, screen, NULL);
			SDL_UnlockMutex(effect_video_ctx.mutex);
		}

		if (image_surface != NULL)
			SDL_BlitSurface(image_surface, NULL, screen, NULL);

		SDL_Flip(screen);
		SDL_framerateDelay(&fpsm);
	}

	/* never reached */
	return EXIT_FAILURE;
}
