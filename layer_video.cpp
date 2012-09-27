#include <assert.h>
#include <math.h>

#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_rotozoom.h>

#include <vlc/vlc.h>
#include <vlc/libvlc_version.h>

#include "osc_graphics.h"
#include "layer_video.h"

Layer::CtorInfo LayerVideo::ctor_info = {"video", "s" /* url */};

libvlc_instance_t *LayerVideo::vlcinst = NULL;

/*
 * libvlc callbacks
 */
extern "C" {

static void *lock_cb(void *data, void **p_pixels);
static void unlock_cb(void *data, void *id, void *const *p_pixels);
static void display_cb(void *data, void *id);

}

LayerVideo::LayerVideo(const char *name, SDL_Rect geo, float opacity,
		       const char *url) :
		      Layer(name), mp(NULL), surf(NULL),
		      mutex(SDL_CreateMutex())
{
	/* static initialization */
	if (!vlcinst) {
		static char const *vlc_argv[] = {
			"--no-audio",	/* skip any audio track */
			"--no-xlib",	/* tell VLC to not use Xlib */
			"--no-osd"	/* no text on video */
		};

		vlcinst = libvlc_new(NARRAY(vlc_argv), vlc_argv);
	}

	url_osc_id = register_method("url", "s",
				     (OSCServer::MethodHandlerCb)url_osc);
	rate_osc_id = register_method("rate", "f",
				      (OSCServer::MethodHandlerCb)rate_osc);
	position_osc_id = register_method("position", "f",
					  (OSCServer::MethodHandlerCb)position_osc);
	paused_osc_id = register_method("paused", "i",
					(OSCServer::MethodHandlerCb)paused_osc);

	LayerVideo::geo(geo);
	LayerVideo::alpha(opacity);
	LayerVideo::rate(1.);
	LayerVideo::paused(true);

	LayerVideo::url(url);
}

void
LayerVideo::geo(SDL_Rect geo)
{
	if (!geo.x && !geo.y && !geo.w && !geo.h)
		geov = (SDL_Rect){0, 0, screen->w, screen->h};
	else
		geov = geo;
}

#if LIBVLC_VERSION_INT < LIBVLC_VERSION(2,0,0,0)

/*
 * libvlc_video_get_size() cannot be used before playback has started and
 * libvlc_media_get_tracks_info() is broken in libVLC v1.x.x so we just
 * use the screen size on those versions (results in unnecessary scaling).
 */
static inline void
media_get_video_size(libvlc_media_t *media __attribute__((unused)),
		     unsigned int &width, unsigned int &height)
{
	width = screen->w;
	height = screen->h;
}

#else

static inline void
media_get_video_size(libvlc_media_t *media,
		     unsigned int &width, unsigned int &height)
{
	libvlc_media_track_info_t *tracks = NULL;
	int num_tracks;

	width = height = 0;

	libvlc_media_parse(media);

	num_tracks = libvlc_media_get_tracks_info(media, &tracks);
	for (int i = 0; i < num_tracks; i++) {
		if (tracks[i].i_type == libvlc_track_video) {
			width = tracks[i].u.video.i_width;
			height = tracks[i].u.video.i_height;
			break;
		}
	}

	free(tracks);

	assert(width && height);
}

#endif

static void *
lock_cb(void *data, void **p_pixels)
{
	LayerVideo *video = (LayerVideo *)data;

	*p_pixels = video->lock_surf();

	return NULL; /* picture identifier, not needed here */
}

static void
unlock_cb(void *data, void *id __attribute__((unused)),
	  void *const *p_pixels)
{
	LayerVideo *video = (LayerVideo *)data;

	video->unlock_surf();
}

static void
display_cb(void *data __attribute__((unused)),
	   void *id __attribute__((unused)))
{
	/* VLC wants to display the video */
}

void
LayerVideo::url(const char *url)
{
	libvlc_media_t *m;
	unsigned int width, height;

	SDL_FREESURFACE_SAFE(surf);

	if (mp) {
		libvlc_media_player_release(mp);
		mp = NULL;
	}

	if (!url || !*url)
		return;

	m = libvlc_media_new_location(vlcinst, url);
	mp = libvlc_media_player_new_from_media(m);
	media_get_video_size(m, width, height);
	libvlc_media_release(m);

	/*
	 * Cannot change buffer dimensions on the fly, so we let
	 * libVLC render into a statically sized buffer and do the resizing
	 * on our own.
	 * We use the original video size so libVLC does not have to do
	 * unnecessary scaling.
	 */
	surf = SDL_CreateRGBSurface(SDL_HWSURFACE, width, height,
				    16, 0x001f, 0x07e0, 0xf800, 0);

	libvlc_video_set_callbacks(mp, lock_cb, unlock_cb, display_cb, this);
	libvlc_video_set_format(mp, "RV16", surf->w, surf->h, surf->pitch);

	rate(ratev);
	paused(pausedv);
}

void
LayerVideo::alpha(float opacity)
{
	alphav = opacity;
}

void
LayerVideo::rate(float rate)
{
	ratev = rate;

	if (mp)
		libvlc_media_player_set_rate(mp, rate);
}

void
LayerVideo::position(float position)
{
	if (mp)
		libvlc_media_player_set_position(mp, position);
}

void
LayerVideo::paused(bool paused)
{
	pausedv = paused;

	if (!mp)
		return;

#if 0
	libvlc_media_player_set_pause(mp, paused);
#else
	int playing = libvlc_media_player_is_playing(mp);
	if (playing && paused)
		libvlc_media_player_pause(mp);
	else if (!playing && !paused)
		libvlc_media_player_play(mp);
#endif
}

void
LayerVideo::frame(SDL_Surface *target)
{
	Uint8 alpha = (Uint8)ceilf(alphav*SDL_ALPHA_OPAQUE);

	if (!surf)
		return;

	if (surf->w != geov.w || surf->h != geov.h) {
		SDL_Surface *surf_scaled;

		SDL_LockMutex(mutex);
		surf_scaled = zoomSurface(surf,
					  (double)geov.w/surf->w,
					  (double)geov.h/surf->h,
					  SMOOTHING_ON);
		SDL_UnlockMutex(mutex);

		if (alpha < SDL_ALPHA_OPAQUE)
			SDL_SetAlpha(surf_scaled, SDL_SRCALPHA | SDL_RLEACCEL, alpha);

		SDL_BlitSurface(surf_scaled, NULL, target, &geov);
		SDL_FreeSurface(surf_scaled);
	} else {
		if (alpha == SDL_ALPHA_OPAQUE)
			SDL_SetAlpha(surf, 0, 0);
		else
			SDL_SetAlpha(surf, SDL_SRCALPHA | SDL_RLEACCEL, alpha);

		SDL_LockMutex(mutex);
		SDL_BlitSurface(surf, NULL, target, &geov);
		SDL_UnlockMutex(mutex);
	}
}

LayerVideo::~LayerVideo()
{
	unregister_method(url_osc_id);
	unregister_method(rate_osc_id);
	unregister_method(position_osc_id);
	unregister_method(paused_osc_id);

	if (mp)
		libvlc_media_player_release(mp);
	libvlc_release(vlcinst);
	SDL_DestroyMutex(mutex);
	if (surf)
		SDL_FreeSurface(surf);
}
