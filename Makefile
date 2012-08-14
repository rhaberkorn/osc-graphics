CC := gcc

SDL_CFLAGS := $(shell sdl-config --cflags)
SDL_LDFLAGS := $(shell sdl-config --libs)

CFLAGS := -std=c99 -Wall -g -O0 \
	  $(SDL_CFLAGS)
LDFLAGS := $(SDL_LDFLAGS)

all : effect-pad

effect-pad : effect-pad.o
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	$(RM) *.o effect-pad{,.exe}
