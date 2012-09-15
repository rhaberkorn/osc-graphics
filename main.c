#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_framerate.h>
#include <SDL_rotozoom.h>
#include <SDL_gfxPrimitives.h>

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

static struct layer_image *layer_image_new(const char *file);
static void layer_image_change(struct layer_image *ctx, const char *file);
static void layer_image_frame_cb(void *data, SDL_Surface *target);
static void layer_image_free_cb(void *data);

static struct layer_video *layer_video_new(const char *file, SDL_Color *key);
static void layer_video_change(struct layer_video *ctx,
			       const char *file, SDL_Color *key);
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

static int
osc_image_new(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data,
	      void *user_data __attribute__((unused)))
{
	struct layer_image *ctx = layer_image_new(&argv[2]->s);

	layer_insert(argv[0]->i, &argv[1]->s, ctx,
		     layer_image_frame_cb, layer_image_free_cb);

	return 0;
}

static int
osc_video_new(const char *path, const char *types, lo_arg **argv,
	      int argc, void *data,
	      void *user_data __attribute__((unused)))
{
	struct layer_video *ctx = layer_video_new(&argv[2]->s, NULL);

	layer_insert(argv[0]->i, &argv[1]->s, ctx,
		     layer_video_frame_cb, layer_video_free_cb);

	return 0;
}

static int
osc_rect_new(const char *path, const char *types, lo_arg **argv,
	     int argc, void *data,
	     void *user_data __attribute__((unused)))
{
	SDL_Rect rect = {
		(Sint16)argv[2]->i, (Sint16)argv[3]->i,
		(Uint16)argv[4]->i, (Uint16)argv[5]->i
	};
	SDL_Color color = {(Uint8)argv[6]->i, (Uint8)argv[7]->i, (Uint8)argv[8]->i};

	struct layer_rect *ctx = layer_rect_new(rect, color);

	layer_insert(argv[0]->i, &argv[1]->s, ctx,
		     layer_rect_frame_cb, layer_rect_free_cb);

	return 0;
}

static inline lo_server
osc_init(const char *port)
{
	lo_server server = lo_server_new(port, osc_error);

	lo_server_add_method(server, NULL, NULL, osc_generic_handler, NULL);

	lo_server_add_method(server, "/layer/image/new", "iss",
			     osc_image_new, NULL);
	lo_server_add_method(server, "/layer/video/new", "iss",
			     osc_video_new, NULL);
	lo_server_add_method(server, "/layer/rect/new", "isiiiiiii",
			     osc_rect_new, NULL);

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
	SDL_Surface *surf;
};

static struct layer_image *
layer_image_new(const char *file)
{
	struct layer_image *ctx = malloc(sizeof(struct layer_image));

	if (ctx) {
		memset(ctx, 0, sizeof(*ctx));
		layer_image_change(ctx, file);
	}

	return ctx;
}

static void
layer_image_change(struct layer_image *ctx, const char *file)
{
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
}

static void
layer_image_frame_cb(void *data, SDL_Surface *target)
{
	struct layer_image *ctx = data;

	if (ctx->surf)
		SDL_BlitSurface(ctx->surf, NULL, target, NULL);
}

static void
layer_image_free_cb(void *data)
{
	struct layer_image *ctx = data;

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
	SDL_LockSurface(ctx->surf);
	*p_pixels = ctx->surf->pixels;

	return NULL; /* picture identifier, not needed here */
}

static void
layer_video_unlock_cb(void *data, void *id, void *const *p_pixels)
{
	struct layer_video *ctx = data;

	assert(id == NULL); /* picture identifier, not needed here */

	SDL_UnlockSurface(ctx->surf);
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

	ctx->surf = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h,
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
				ctx->surf->w*2);

	libvlc_media_player_play(ctx->mp);
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

		FOREACH_LAYER(cur)
			cur->frame_cb(cur->data, screen);

		SDL_Flip(screen);
		SDL_framerateDelay(&fpsm);
	}

	/* never reached */
	return EXIT_FAILURE;
}
