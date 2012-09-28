AM_CXXFLAGS = -Wall
AM_CPPFLAGS =

if NEED_COMPAT
AM_CPPFLAGS += -I@top_srcdir@/compat
endif

bin_PROGRAMS = osc-graphics
osc_graphics_SOURCES = main.cpp osc_graphics.h \
		       osc_server.cpp osc_server.h \
		       layer.cpp layer.h \
		       layer_box.cpp layer_box.h \
		       layer_image.cpp layer_image.h \
		       layer_video.cpp layer_video.h

osc_graphics_LDFLAGS = -nodefaultlibs
osc_graphics_LDADD = -lc -lsupc++ -lgcc -lgcc_eh
