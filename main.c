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

enum Effects {
	EFFECT_FIRE,
	EFFECT_BLOPS,
	EFFECT_ERASOR
};
static enum Effects currentEffect = EFFECT_FIRE;

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

	if (currentEffect == EFFECT_FIRE &&
	    (SDL_GetMouseState(&mouse_x, &mouse_y) & SDL_BUTTON(1)))
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

struct EffectBlop {
	struct EffectBlop *next;

	Sint16		x;
	Sint16		y;
	Sint16		radius;

	SDL_Color	color;
	Uint8		alpha;
};
static struct EffectBlop effect_blops_list = {NULL};

static void
effect_blops_create(Uint16 x, Uint16 y, SDL_Color color)
{
	struct EffectBlop *tail = &effect_blops_list;

	while (tail->next != NULL)
		tail = tail->next;

	tail->next = malloc(sizeof(struct EffectBlop));
	tail = tail->next;

	tail->next = NULL;
	tail->x = (Sint16)x;
	tail->y = (Sint16)y;
	tail->radius = 1;
	tail->color = color;
	tail->alpha = SDL_ALPHA_OPAQUE;
}

static inline void
effect_blops_update(void)
{
	struct EffectBlop *next;

	for (struct EffectBlop *cur = &effect_blops_list;
	     cur != NULL && cur->next != NULL;
	     cur = next) {
		next = cur->next;

		filledCircleRGBA(screen, next->x, next->y, next->radius,
				 next->color.r, next->color.g,
				 next->color.b, next->alpha);
		if (next->radius > 1)
			aacircleRGBA(screen, next->x, next->y, next->radius,
				     next->color.r, next->color.g,
				     next->color.b, next->alpha);

		next->radius++;
		if (next->alpha > 10)
			next->alpha -= 3;
		else
			next->alpha = 0;

		if (next->alpha == SDL_ALPHA_TRANSPARENT) {
			cur->next = next->next;
			free(next);
			next = cur;
		}
	}
}

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

static SDL_Surface *effect_erasor_surface, *effect_erasor_applysurf;

static inline void
effect_erasor_init(void)
{
	effect_erasor_surface = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA,
						     screen->w, screen->h, 16,
						     0x0003, 0x000C, 0x0030, 0xFF00);

	effect_erasor_applysurf = SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_SRCALPHA,
						       screen->w, screen->h, 32,
						       0x000000FF, 0x0000FF00,
						       0x00FF0000, 0xFF000000);
}

static inline void
effect_erasor_draw(int x, int y, Uint8 intensity)
{
	filledCircleRGBA(effect_erasor_surface, x, y, 5,
			 0, 0, 0, intensity);

}

static void
effect_erasor_apply(void)
{
	SDL_MAYBE_LOCK(effect_erasor_surface);
	SDL_MAYBE_LOCK(effect_erasor_applysurf);

	for (int i = 0; i < effect_erasor_surface->w*effect_erasor_surface->h; i++) {
		Uint8 erasor_alpha;
		SDL_Color dst_color;

		SDL_GetRGBA(((Uint16 *)effect_erasor_surface->pixels)[i],
			    effect_erasor_surface->format,
			    &dst_color.r, &dst_color.g,
			    &dst_color.b, &erasor_alpha);
		SDL_GetRGBA(((Uint32 *)effect_erasor_applysurf->pixels)[i],
			    effect_erasor_applysurf->format,
			    &dst_color.r, &dst_color.g,
			    &dst_color.b, &dst_color.unused);

		if (dst_color.unused > erasor_alpha)
			dst_color.unused -= erasor_alpha;
		else
			dst_color.unused = 0;

		((Uint32 *)effect_erasor_applysurf->pixels)[i] =
			SDL_MapRGBA(effect_erasor_applysurf->format,
				    dst_color.r, dst_color.g,
				    dst_color.b, dst_color.unused);
	}

	SDL_MAYBE_UNLOCK(effect_erasor_applysurf);
	SDL_MAYBE_UNLOCK(effect_erasor_surface);
}

static SDL_Color effect_bg_color = {0, 0, 0};

static inline void
effect_bg_change(SDL_Color color)
{
	effect_bg_color = color;
}

static SDL_Color effect_generic_color = {0, 0, 0};

static inline void
effect_generic_set_color(SDL_Color color)
{
	effect_generic_color = color;
}

static SDL_Color effect_generic_colors[256];

static inline void
effect_generic_init(void)
{
	memset(effect_generic_colors, 0, sizeof(effect_generic_colors));

	for (int i = 0; i < 32; i++) {
		/* black to blue, 32 values */
		effect_generic_colors[i].b = i << 1;

		/* blue to red, 32 values */
		effect_generic_colors[i + 32].r = i << 3;
		effect_generic_colors[i + 32].b = 64 - (i << 1);

		/*red to yellow, 32 values */
		effect_generic_colors[i + 64].r = 255;
		effect_generic_colors[i + 64].g = i << 3;

		/* yellow to white, 162 */
		effect_generic_colors[i + 96].r = 255;
		effect_generic_colors[i + 96].g = 255;
		effect_generic_colors[i + 96].b = i << 2;
		effect_generic_colors[i + 128].r = 255;
		effect_generic_colors[i + 128].g = 255;
		effect_generic_colors[i + 128].b = 64 + (i << 2);
		effect_generic_colors[i + 160].r = 255;
		effect_generic_colors[i + 160].g = 255;
		effect_generic_colors[i + 160].b = 128 + (i << 2);
		effect_generic_colors[i + 192].r = 255;
		effect_generic_colors[i + 192].g = 255;
		effect_generic_colors[i + 192].b = 192 + i;
		effect_generic_colors[i + 224].r = 255;
		effect_generic_colors[i + 224].g = 255;
		effect_generic_colors[i + 224].b = 224 + i;
	}
}

static SDL_Color
effect_generic_get_next_color(void)
{
	static int curColor = 0;
	SDL_Color ret = effect_generic_colors[curColor];

	curColor++;
	curColor %= NARRAY(effect_generic_colors);

	return ret;
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
			} else if (event.key.keysym.mod & KMOD_LCTRL) {
				switch (event.key.keysym.sym) {
				case SDLK_0:
					effect_generic_set_color((SDL_Color){0, 0, 0});
					break;
				case SDLK_1:
					effect_generic_set_color((SDL_Color){255, 0, 0});
					break;
				case SDLK_2:
					effect_generic_set_color((SDL_Color){0, 255, 0});
					break;
				case SDLK_3:
					effect_generic_set_color((SDL_Color){0, 0, 255});
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

				case SDLK_f:
					currentEffect = EFFECT_FIRE;
					break;
				case SDLK_b:
					currentEffect = EFFECT_BLOPS;
					break;
				case SDLK_e:
					currentEffect = EFFECT_ERASOR;
					break;

				default:
					break;
				}
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
			switch (currentEffect) {
			case EFFECT_BLOPS:
				effect_blops_create(event.button.x, event.button.y,
						    event.button.button == SDL_BUTTON_LEFT
							? effect_generic_color
							: effect_generic_get_next_color());
				break;
			case EFFECT_ERASOR:
				effect_erasor_draw(event.button.x, event.button.y, 128);
				break;
			default:
				break;
			}
			break;

		case SDL_MOUSEMOTION:
			switch (currentEffect) {
			case EFFECT_BLOPS:
				if (event.motion.state & (SDL_BUTTON(1) | SDL_BUTTON(3)))
					effect_blops_create(event.motion.x, event.motion.y,
							    event.motion.state & SDL_BUTTON(1)
								? effect_generic_color
								: effect_generic_get_next_color());
				break;
			case EFFECT_ERASOR:
				if (event.motion.state & SDL_BUTTON(1))
					effect_erasor_draw(event.motion.x, event.motion.y, 128);
				break;
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

	effect_generic_init();
	effect_video_init();
	fire_surface = effect_fire_init();
	effect_erasor_init();

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

		if (image_surface != NULL) {
#if 0
			SDL_FillRect(effect_erasor_applysurf, NULL,
				     SDL_MapRGBA(effect_erasor_applysurf->format,
						 0, 0, 0, SDL_ALPHA_OPAQUE));
#endif

			SDL_BlitSurface(image_surface, NULL, screen, NULL);

#if 0
			effect_erasor_apply();
			SDL_BlitSurface(effect_erasor_applysurf, NULL, screen, NULL);
#endif
		}

		effect_fire_update(fire_surface);
		SDL_BlitSurface(fire_surface, NULL, screen, NULL);

		effect_blops_update();

		SDL_Flip(screen);
		SDL_framerateDelay(&fpsm);
	}

	/* never reached */
	return EXIT_FAILURE;
}
