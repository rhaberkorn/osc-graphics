CC := gcc

SDL_CFLAGS := $(shell sdl-config --cflags)
SDL_LDFLAGS := $(shell sdl-config --libs)

SDL_IMAGE_CFLAGS := $(shell pkg-config SDL_image --cflags)
SDL_IMAGE_LDFLAGS := $(shell pkg-config SDL_image --libs)

SDL_GFX_CFLAGS := $(shell pkg-config SDL_gfx --cflags)
SDL_GFX_LDFLAGS := $(shell pkg-config SDL_gfx --libs)

LIBVLC_CFLAGS := $(shell pkg-config libvlc --cflags)
LIBVLC_LDFLAGS := $(shell pkg-config libvlc --libs)

LIBLO_CFLAGS := $(shell pkg-config liblo --cflags)
LIBLO_LDFLAGS := $(shell pkg-config liblo --libs)

CFLAGS := -std=c99 -Wall -g -O0 \
	  $(SDL_CFLAGS) $(SDL_IMAGE_CFLAGS) $(SDL_GFX_CFLAGS) \
	  $(LIBVLC_CFLAGS) $(LIBLO_CFLAGS)
LDFLAGS := -lm \
	   $(SDL_LDFLAGS) $(SDL_IMAGE_LDFLAGS) $(SDL_GFX_LDFLAGS) \
	   $(LIBVLC_LDFLAGS) $(LIBLO_LDFLAGS)

all : osc-graphics

osc-graphics : main.o
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	$(RM) *.o osc-graphics{,.exe}
