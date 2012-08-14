CC := gcc

SDL_CFLAGS := $(shell sdl-config --cflags)
SDL_LDFLAGS := $(shell sdl-config --libs)

SDL_IMAGE_CFLAGS := $(shell pkg-config SDL_image --cflags)
SDL_IMAGE_LDFLAGS := $(shell pkg-config SDL_image --libs)

CFLAGS := -std=c99 -Wall -g -O0 \
	  $(SDL_CFLAGS) $(SDL_IMAGE_CFLAGS)
LDFLAGS := $(SDL_LDFLAGS) $(SDL_IMAGE_LDFLAGS)

all : effect-pad

effect-pad : effect-pad.o
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	$(RM) *.o effect-pad{,.exe}
