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

pkgdatadir=$datadir/$PACKAGE_TARNAME
AC_SUBST(pkgdatadir)

chuckdir=$pkgdatadir/chuck
AC_SUBST(chuckdir)

# Checks for programs.
LT_INIT
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_SED

# Checks for libraries.
AC_CHECK_PROG(SDL_CONFIG, sdl-config, sdl-config)
if [[ x$SDL_CONFIG = x ]]; then
	AC_MSG_ERROR([Required sdl-config tool missing!])
fi
# Don't add to CFLAGS to prevent libtool wrappers being built with
# -Dmain=SDL_main on MinGW
#CFLAGS="$CFLAGS `$SDL_CONFIG --cflags`"
CPPFLAGS="$CPPFLAGS `$SDL_CONFIG --cflags`"
LIBS="$LIBS `$SDL_CONFIG --libs`"

# On Windows, SDL adds -mwindows to LIBS
case $build_os in
*mingw*) LIBS="$LIBS -mconsole"
esac

PKG_CHECK_MODULES(SDL_IMAGE, [SDL_image], [
	CFLAGS="$CFLAGS $SDL_IMAGE_CFLAGS"
	CPPFLAGS="$CPPFLAGS $SDL_IMAGE_CFLAGS"
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
	CPPFLAGS="$CPPFLAGS $SDL_GFX_CFLAGS"
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

AC_CHECK_LIB(SDL_ttf, TTF_Init, , [
	AC_MSG_ERROR([Required libSDL_ttf missing!])
])
AC_CHECK_HEADERS([SDL_ttf.h], , [
	AC_MSG_ERROR([Required libSDL_ttf header missing!])
])

case $build_os in
*mingw*) LIBS="$LIBS -lShlwapi"
esac

PKG_CHECK_MODULES(FFMPEG, [libavcodec libavformat libavutil libswscale], [
	CFLAGS="$CFLAGS $FFMPEG_CFLAGS"
	CPPFLAGS="$CPPFLAGS $FFMPEG_CFLAGS"
	LIBS="$LIBS $FFMPEG_LIBS"
])

PKG_CHECK_MODULES(LIBVLC, [libvlc >= 1.1.10], [
	CFLAGS="$CFLAGS $LIBVLC_CFLAGS"
	CPPFLAGS="$CPPFLAGS $LIBVLC_CFLAGS"
	LIBS="$LIBS $LIBVLC_LIBS"
])

PKG_CHECK_MODULES(LIBLO, [liblo], [
	CFLAGS="$CFLAGS $LIBLO_CFLAGS"
	CPPFLAGS="$CPPFLAGS $LIBLO_CFLAGS"
	LIBS="$LIBS $LIBLO_LIBS"
])

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

#
# Config options
#
AC_ARG_WITH(font-path,
	AS_HELP_STRING([--with-font-path=PATH],
		       [Specify base directory for selecting TTF fonts [default=/usr/share/fonts/truetype/]]),
	[FONT_PATH=$withval], [FONT_PATH=/usr/share/fonts/truetype/])
AC_DEFINE_UNQUOTED([FONT_PATH], ["$FONT_PATH"], [TTF fonts base directory])

CXXFLAGS="$CXXFLAGS $CFLAGS"

AC_CONFIG_FILES([Makefile src/Makefile chuck/Makefile])
AC_OUTPUT
