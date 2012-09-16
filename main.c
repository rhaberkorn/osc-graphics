#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_framerate.h>
#include <SDL_rotozoom.h>
#include <SDL_gfxPrimitives.h>
#include <SDL_gfxBlitFunc.h>

#include <vlc/vlc.h>

#include <lo/lo.h>

/*
 * Macros
 */
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

/*
 * Default values
 */
#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480
#define FRAMERATE	20 /* Hz */

/*
 * Declarations
 */
typedef void (*layer_frame_cb_t)(void *data, SDL_Surface *target);
typedef void (*layer_free_cb_t)(void *data);

static int layer_insert(int pos, const char *name, void *data,
			layer_frame_cb_t frame_cb, layer_free_cb_t free_cb);
static int layer_delete_by_name(const char *name);

static struct layer_image *layer_image_new(const char *file);
static void layer_image_change(struct layer_image *ctx, const char *file);
static void layer_image_alpha(struct layer_image *ctx, float opacity);
static void layer_image_frame_cb(void *data, SDL_Surface *target);
static void layer_image_free_cb(void *data);

static struct layer_video *layer_video_new(const char *file, SDL_Color *key);
static void layer_video_change(struct layer_video *ctx,
			       const char *file, SDL_Color *key);
static void layer_video_alpha(struct layer_video *ctx, float opacity);
static void layer_video_frame_cb(void *data, SDL_Surface *target);
static void layer_video_free_cb(void *data);

static struct layer_rect *layer_rect_new(SDL_Rect rect, SDL_Color color);
static void layer_rect_change(struct layer_rect *ctx,
			      SDL_Rect rect, SDL_Color color);
static void layer_rect_frame_cb(void *data, SDL_Surface *target);
static void layer_rect_free_cb(void *data);

static SDL_Surface *screen;

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
    int i;

    printf("path: <%s>\n", path);
    for (i=0; i<argc; i++) {
	printf("arg %d '%c' ", i, types[i]);
	lo_arg_pp(types[i], argv[i]);
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
	lo_server server = user_data;
	char *name;

	name = get_layer_name_from_path(path);
	layer_delete_by_name(name);
	free(name);

	lo_server_del_method(server, path, types);

	return 1;
}

static inline void
osc_add_layer_delete(lo_server server, const char *name,
		     lo_method_handler custom_delete)
{
	char path[255];

	snprintf(path, sizeof(path), "/layer/%s/delete", name);

	if (custom_delete)
		lo_server_add_method(server, path, "", custom_delete, server);
	lo_server_add_method(server, path, "", osc_layer_delete, server);
}

static int
osc_image_alpha(const char *path, const char *types, lo_arg **argv,
	        int argc, void *data, void *user_data)
{
	struct layer_image *ctx = user_data;

	layer_image_alpha(ctx, argv[0]->f);
	return 0;
}

static int
osc_image_delete(const char *path, const char *types, lo_arg **argv,
		 int argc, void *data, void *user_data)
{
	lo_server server = user_data;
	char buf[255], *name;

	name = get_layer_name_from_path(path);
	snprintf(buf, sizeof(buf), "/layer/%s/alpha", name);
	lo_server_del_method(server, buf, "f");
	free(name);

	lo_server_del_method(server, path, types);

	return 1;
}

static int
osc_image_new(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data, void *user_data)
{
	lo_server server = user_data;
	char buf[255];
	struct layer_image *ctx = layer_image_new(&argv[2]->s);

	if (layer_insert(argv[0]->i, &argv[1]->s, ctx,
			 layer_image_frame_cb, layer_image_free_cb))
		return 0;

	snprintf(buf, sizeof(buf), "/layer/%s/alpha", &argv[1]->s);
	lo_server_add_method(server, buf, "f", osc_image_alpha, ctx);
	osc_add_layer_delete(server, &argv[1]->s, osc_image_delete);

	return 0;
}

static int
osc_video_alpha(const char *path, const char *types, lo_arg **argv,
	        int argc, void *data, void *user_data)
{
	struct layer_video *ctx = user_data;

	layer_video_alpha(ctx, argv[0]->f);
	return 0;
}

static int
osc_video_delete(const char *path, const char *types, lo_arg **argv,
		 int argc, void *data, void *user_data)
{
	lo_server server = user_data;
	char buf[255], *name;

	name = get_layer_name_from_path(path);
	snprintf(buf, sizeof(buf), "/layer/%s/alpha", name);
	lo_server_del_method(server, buf, "f");
	free(name);

	lo_server_del_method(server, path, types);

	return 1;
}

static int
osc_video_new(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data, void *user_data)
{
	lo_server server = user_data;
	char buf[255];
	struct layer_video *ctx = layer_video_new(&argv[2]->s, NULL);

	if (layer_insert(argv[0]->i, &argv[1]->s, ctx,
			 layer_video_frame_cb, layer_video_free_cb))
		return 0;

	snprintf(buf, sizeof(buf), "/layer/%s/alpha", &argv[1]->s);
	lo_server_add_method(server, buf, "f", osc_video_alpha, ctx);
	osc_add_layer_delete(server, &argv[1]->s, osc_video_delete);

	return 0;
}

static int
osc_rect_new(const char *path, const char *types, lo_arg **argv,
	     int argc, void *data, void *user_data)
{
	lo_server server = user_data;
	SDL_Rect rect = {
		(Sint16)argv[2]->i, (Sint16)argv[3]->i,
		(Uint16)argv[4]->i, (Uint16)argv[5]->i
	};
	SDL_Color color = {(Uint8)argv[6]->i, (Uint8)argv[7]->i, (Uint8)argv[8]->i};

	struct layer_rect *ctx = layer_rect_new(rect, color);

	if (layer_insert(argv[0]->i, &argv[1]->s, ctx,
			 layer_rect_frame_cb, layer_rect_free_cb))
		return 0;

	osc_add_layer_delete(server, &argv[1]->s, NULL);

	return 0;
}

static inline lo_server
osc_init(const char *port)
{
	lo_server server = lo_server_new(port, osc_error);

	lo_server_add_method(server, NULL, NULL, osc_generic_handler, NULL);

	lo_server_add_method(server, "/layer/new/image", "iss",
			     osc_image_new, server);
	lo_server_add_method(server, "/layer/new/video", "iss",
			     osc_video_new, server);
	lo_server_add_method(server, "/layer/new/rect", "isiiiiiii",
			     osc_rect_new, server);

	return server;
}

static inline void
osc_process_events(lo_server server)
{
	while (lo_server_recv_noblock(server, 0));
}

/*
 * Image layers
 */
struct layer_image {
	SDL_Surface	*surf_alpha;	/* with per-surface alpha */
	SDL_Surface	*surf;

	float		alpha;
};

static struct layer_image *
layer_image_new(const char *file)
{
	struct layer_image *ctx = malloc(sizeof(struct layer_image));

	if (ctx) {
		memset(ctx, 0, sizeof(*ctx));
		ctx->alpha = 1.;
		layer_image_change(ctx, file);
	}

	return ctx;
}

static void
layer_image_change(struct layer_image *ctx, const char *file)
{
	if (ctx->surf_alpha) {
		SDL_FreeSurface(ctx->surf_alpha);
		ctx->surf_alpha = NULL;
	}

	if (ctx->surf) {
		SDL_FreeSurface(ctx->surf);
		ctx->surf = NULL;
	}

	if (!file || !*file)
		return;

	ctx->surf = IMG_Load(file);
	if (!ctx->surf) {
		SDL_IMAGE_ERROR("IMG_Load");
		exit(EXIT_FAILURE);
	}

	if (ctx->surf->w != screen->w || ctx->surf->h != screen->h) {
		SDL_Surface *new;

		new = zoomSurface(ctx->surf,
				  (double)screen->w/ctx->surf->w,
				  (double)screen->h/ctx->surf->h,
				  SMOOTHING_ON);
		SDL_FreeSurface(ctx->surf);
		ctx->surf = new;
	}

	layer_image_alpha(ctx, ctx->alpha);
}

#if 0

static inline void
rgba_blit_with_alpha(SDL_Surface *src_surf, SDL_Surface *dst_surf, Uint8 alpha)
{
	SDL_FillRect(dst_surf, NULL,
		     SDL_MapRGBA(dst_surf->format,
		     	     	 0, 0, 0, SDL_ALPHA_TRANSPARENT));
	SDL_gfxBlitRGBA(src_surf, NULL, dst_surf, NULL);
	SDL_gfxMultiplyAlpha(dst_surf, alpha);
}

#else

static inline void
rgba_blit_with_alpha(SDL_Surface *src_surf, SDL_Surface *dst_surf, Uint8 alpha)
{
	Uint8 *src = src_surf->pixels;
	Uint8 *dst = dst_surf->pixels;
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

#endif

static void
layer_image_alpha(struct layer_image *ctx, float opacity)
{
	Uint8 alpha = (Uint8)ceilf(opacity*SDL_ALPHA_OPAQUE);
	ctx->alpha = opacity;

	if (!ctx->surf)
		return;

	if (!ctx->surf->format->Amask) {
		if (alpha == SDL_ALPHA_OPAQUE)
			SDL_SetAlpha(ctx->surf, 0, 0);
		else
			SDL_SetAlpha(ctx->surf, SDL_SRCALPHA | SDL_RLEACCEL, alpha);

		return;
	}

	if (alpha == SDL_ALPHA_OPAQUE) {
		if (ctx->surf_alpha) {
			SDL_FreeSurface(ctx->surf_alpha);
			ctx->surf_alpha = NULL;
		}
		return;
	}

	if (!ctx->surf_alpha) {
		ctx->surf_alpha = SDL_CreateRGBSurface(ctx->surf->flags,
						       ctx->surf->w, ctx->surf->h,
						       ctx->surf->format->BitsPerPixel,
						       ctx->surf->format->Rmask,
						       ctx->surf->format->Gmask,
						       ctx->surf->format->Bmask,
						       ctx->surf->format->Amask);
	}

	if (alpha == SDL_ALPHA_TRANSPARENT) {
		SDL_FillRect(ctx->surf_alpha, NULL,
			     SDL_MapRGBA(ctx->surf_alpha->format,
			     	     	 0, 0, 0, SDL_ALPHA_TRANSPARENT));
	} else {
		rgba_blit_with_alpha(ctx->surf, ctx->surf_alpha, alpha);
	}
}

static void
layer_image_frame_cb(void *data, SDL_Surface *target)
{
	struct layer_image *ctx = data;

	if (ctx->surf)
		SDL_BlitSurface(ctx->surf_alpha ? : ctx->surf,
				NULL, target, NULL);
}

static void
layer_image_free_cb(void *data)
{
	struct layer_image *ctx = data;

	if (ctx->surf_alpha)
		SDL_FreeSurface(ctx->surf_alpha);
	if (ctx->surf)
		SDL_FreeSurface(ctx->surf);

	free(ctx);
}

/*
 * Video layer
 */
struct layer_video {
	libvlc_instance_t *vlcinst;
	libvlc_media_player_t *mp;

	SDL_Surface *surf;
	SDL_mutex *mutex;
};

static void *
layer_video_lock_cb(void *data, void **p_pixels)
{
	struct layer_video *ctx = data;

	SDL_LockMutex(ctx->mutex);
	SDL_MAYBE_LOCK(ctx->surf);
	*p_pixels = ctx->surf->pixels;

	return NULL; /* picture identifier, not needed here */
}

static void
layer_video_unlock_cb(void *data, void *id, void *const *p_pixels)
{
	struct layer_video *ctx = data;

	assert(id == NULL); /* picture identifier, not needed here */

	SDL_MAYBE_UNLOCK(ctx->surf);
	SDL_UnlockMutex(ctx->mutex);
}

static void
layer_video_display_cb(void *data __attribute__((unused)), void *id)
{
	/* VLC wants to display the video */
	assert(id == NULL); /* picture identifier, not needed here */
}

static struct layer_video *
layer_video_new(const char *file, SDL_Color *key)
{
	char const *vlc_argv[] = {
		"--no-audio",	/* skip any audio track */
		"--no-xlib"	/* tell VLC to not use Xlib */
	};
	struct layer_video *ctx = malloc(sizeof(struct layer_video));

	if (!ctx)
		return NULL;

	ctx->surf = SDL_CreateRGBSurface(SDL_HWSURFACE, screen->w, screen->h,
					 16, 0x001f, 0x07e0, 0xf800, 0);
	ctx->mutex = SDL_CreateMutex();

	ctx->vlcinst = libvlc_new(NARRAY(vlc_argv), vlc_argv);
	ctx->mp = NULL;

	layer_video_change(ctx, file, key);

	return ctx;
}

static void
layer_video_change(struct layer_video *ctx, const char *file, SDL_Color *key)
{
	libvlc_media_t *m;

	if (ctx->mp) {
		libvlc_media_player_release(ctx->mp);
		ctx->mp = NULL;
	}

	if (!file || !*file)
		return;

	if (key) {
		SDL_SetColorKey(ctx->surf, SDL_SRCCOLORKEY,
				SDL_MapRGB(ctx->surf->format,
					   key->r, key->g, key->b));
	} else {
		SDL_SetColorKey(ctx->surf, 0, 0);
	}

	m = libvlc_media_new_location(ctx->vlcinst, file);
	ctx->mp = libvlc_media_player_new_from_media(m);
	libvlc_media_release(m);

	libvlc_video_set_callbacks(ctx->mp, layer_video_lock_cb,
				   layer_video_unlock_cb, layer_video_display_cb,
				   ctx);
	libvlc_video_set_format(ctx->mp, "RV16",
				ctx->surf->w,
				ctx->surf->h,
				ctx->surf->w*ctx->surf->format->BytesPerPixel);

	libvlc_media_player_play(ctx->mp);
}

static void
layer_video_alpha(struct layer_video *ctx, float opacity)
{
	Uint8 alpha = (Uint8)ceilf(opacity*SDL_ALPHA_OPAQUE);

	SDL_LockMutex(ctx->mutex);
	if (alpha == SDL_ALPHA_OPAQUE)
		SDL_SetAlpha(ctx->surf, 0, 0);
	else
		SDL_SetAlpha(ctx->surf, SDL_SRCALPHA | SDL_RLEACCEL, alpha);
	SDL_UnlockMutex(ctx->mutex);
}

static void
layer_video_frame_cb(void *data, SDL_Surface *target)
{
	struct layer_video *ctx = data;

	if (ctx->mp) {
		SDL_LockMutex(ctx->mutex);
		SDL_BlitSurface(ctx->surf, NULL, target, NULL);
		SDL_UnlockMutex(ctx->mutex);
	}
}

static void
layer_video_free_cb(void *data)
{
	struct layer_video *ctx = data;

	if (ctx->mp)
		libvlc_media_player_release(ctx->mp);
	libvlc_release(ctx->vlcinst);
	SDL_DestroyMutex(ctx->mutex);
	SDL_FreeSurface(ctx->surf);

	free(ctx);
}

/*
 * Rectangle layer
 */
struct layer_rect {
	SDL_Rect	rect;
	SDL_Color	color;
};

static struct layer_rect *
layer_rect_new(SDL_Rect rect, SDL_Color color)
{
	struct layer_rect *ctx = malloc(sizeof(struct layer_rect));

	if (ctx)
		layer_rect_change(ctx, rect, color);

	return ctx;
}

static void
layer_rect_change(struct layer_rect *ctx, SDL_Rect rect, SDL_Color color)
{
	ctx->rect = rect;
	ctx->color = color;
}

static void
layer_rect_frame_cb(void *data, SDL_Surface *target)
{
	struct layer_rect *ctx = data;
	SDL_Rect *dstrect = &ctx->rect;

	if (!dstrect->x && !dstrect->y && !dstrect->w && !dstrect->h)
		dstrect = NULL;

	SDL_FillRect(target, dstrect,
		     SDL_MapRGB(target->format,
				ctx->color.r, ctx->color.g, ctx->color.b));
}

static void
layer_rect_free_cb(void *data)
{
	struct layer_rect *ctx = data;

	free(ctx);
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

struct layer {
	struct layer *next;

	char *name;

	void			*data;
	layer_frame_cb_t	frame_cb;
	layer_free_cb_t		free_cb;
};
static struct layer layers_head = {NULL};

#define FOREACH_LAYER(VAR) \
	for (struct layer *VAR = layers_head.next; VAR; VAR = VAR->next)

static int
layer_insert(int pos, const char *name, void *data,
	     layer_frame_cb_t frame_cb, layer_free_cb_t free_cb)
{
	struct layer *new, *cur;

	new = malloc(sizeof(struct layer));
	if (!new)
		return -1;

	new->name = strdup(name);
	if (!new->name) {
		free(new);
		return -1;
	}
	new->data = data;
	new->frame_cb = frame_cb;
	new->free_cb = free_cb;

	for (cur = &layers_head; cur->next && pos; cur = cur->next, pos--);
	new->next = cur->next;
	cur->next = new;

	return 0;
}

static int
layer_delete_by_name(const char *name)
{
	for (struct layer *cur = &layers_head; cur->next; cur = cur->next) {
		if (!strcmp(cur->next->name, name)) {
			struct layer *l = cur->next;

			cur->next = l->next;

			l->free_cb(l->data);
			free(l->name);
			free(l);

			return 0;
		}
	}

	return -1;
}

int
main(int argc, char **argv)
{
	lo_server osc_server;
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

	SDL_initFramerate(&fpsm);
	SDL_setFramerate(&fpsm, FRAMERATE);

	for (;;) {
		sdl_process_events();
		osc_process_events(osc_server);

		SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));

		FOREACH_LAYER(cur)
			cur->frame_cb(cur->data, screen);

		SDL_Flip(screen);
		SDL_framerateDelay(&fpsm);
	}

	/* never reached */
	return EXIT_FAILURE;
}
