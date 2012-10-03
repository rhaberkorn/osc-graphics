AM_CXXFLAGS = -Wall
AM_CPPFLAGS =
if NEED_COMPAT
AM_CPPFLAGS += -I@top_srcdir@/compat
endif

# HACK: link with gcc, without libstdc++ but with libsupc++
CXXLD = @CC@
LDADD = -lsupc++

bin_PROGRAMS = osc-graphics
osc_graphics_SOURCES = main.cpp osc_graphics.h \
		       osc_server.cpp osc_server.h \
		       recorder.cpp recorder.h \
		       layer.cpp layer.h \
		       layer_box.cpp layer_box.h \
		       layer_text.cpp layer_text.h \
		       layer_image.cpp layer_image.h \
		       layer_video.cpp layer_video.h
