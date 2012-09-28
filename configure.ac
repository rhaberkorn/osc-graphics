#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([OSC Graphics], [0.1],
	[robin.haberkorn@googlemail.com],
	[osc-graphics],
	[https://github.com/rhaberkorn/osc-graphics])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

chuckdir=$datarootdir/$PACKAGE_TARNAME/chuck
AC_SUBST(chuckdir)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

# Checks for libraries.
AC_CHECK_PROG(SDL_CONFIG, sdl-config, sdl-config)
if [[ x$SDL_CONFIG = x ]]; then
	AC_MSG_ERROR([Required sdl-config tool missing!])
fi
CFLAGS="$CFLAGS `$SDL_CONFIG --cflags`"
LIBS="$LIBS `$SDL_CONFIG --libs`"

PKG_CHECK_MODULES(SDL_IMAGE, [SDL_image], [
	CFLAGS="$CFLAGS $SDL_IMAGE_CFLAGS"
	LIBS="$LIBS $SDL_IMAGE_LIBS"
], [
	AC_CHECK_LIB(SDL_image, IMG_Load, , [
		AC_MSG_ERROR([Required libSDL_image missing!])
	])
	AC_CHECK_HEADERS([SDL_image.h], , [
		AC_MSG_ERROR([Required libSDL_image header missing!])
	])
])

PKG_CHECK_MODULES(SDL_GFX, [SDL_gfx], [
	CFLAGS="$CFLAGS $SDL_GFX_CFLAGS"
	LIBS="$LIBS $SDL_GFX_LIBS"
], [
	AC_CHECK_LIB(SDL_gfx, boxRGBA, , [
		AC_MSG_ERROR([Required libSDL_gfx missing!])
	])
	AC_CHECK_HEADERS([SDL_gfxPrimitives.h SDL_gfxBlitFunc.h \
			  SDL_rotozoom.h SDL_framerate.h], , [
		AC_MSG_ERROR([Required libSDL_gfx headers missing!])
	])
])

PKG_CHECK_MODULES(LIBVLC, [libvlc >= 1.1.10], [
	CFLAGS="$CFLAGS $LIBVLC_CFLAGS"
	LIBS="$LIBS $LIBVLC_LIBS"
])

PKG_CHECK_MODULES(LIBLO, [liblo], [
	CFLAGS="$CFLAGS $LIBLO_CFLAGS"
	LIBS="$LIBS $LIBLO_LIBS"
])

# FIXME: MinGW, need -lws2_32

AC_CHECK_LIB([m], [ceilf])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

AC_CHECK_HEADERS([bsd/sys/queue.h])
AM_CONDITIONAL(NEED_COMPAT, [test $ac_cv_header_bsd_sys_queue_h = no])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE

# Checks for library functions.
AC_CHECK_FUNCS([atexit strdup])

CXXFLAGS="$CXXFLAGS $CFLAGS"

AC_CONFIG_FILES([Makefile src/Makefile chuck/Makefile])
AC_CONFIG_FILES([chuck/lib.ck])
AC_OUTPUT
