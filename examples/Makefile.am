## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

INCLUDES = -I$(top_srcdir)/include 

noinst_PROGRAMS = encoder_example dump_video $(BUILDABLE_EXAMPLES)

# possible contents of BUILDABLE_EXAMPLES:
EXTRA_PROGRAMS = player_example

CFLAGS = $(SDL_CFLAGS)
LDADD = ../lib/libtheora.la -logg 

dump_video_SOURCES = dump_video.c
dump_video_LDADD = $(LDADD)

player_example_SOURCES = player_example.c
player_example_LDADD = $(LDADD) $(SDL_LIBS) -lvorbis -lm

encoder_example_SOURCES = encoder_example.c
EXTRA_encoder_example_SOURCES = getopt.c getopt1.c getopt.h
encoder_example_LDADD = $(GETOPT_OBJS) $(LDADD) -lvorbisenc -lvorbis -lm
encoder_example_DEPENDENCIES = $(GETOPT_OBJS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@ $(CFLAGS)"

profile:
	$(MAKE) all CFLAGS="@PROFILE@ $(CFLAGS)"

