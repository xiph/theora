## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = dump_video dump_psnr libtheora_info \
	$(BUILDABLE_EXAMPLES)

# possible contents of BUILDABLE_EXAMPLES:
EXTRA_PROGRAMS = player_example encoder_example png2theora tiff2theora
EXTRA_DIST = encoder_example_ffmpeg

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = $(OGG_CFLAGS)
LDADD = ../lib/libtheora.la $(OGG_LIBS)
LDADDDEC = ../lib/libtheoradec.la $(OGG_LIBS)
LDADDENC = ../lib/libtheoraenc.la ../lib/libtheoradec.la $(OGG_LIBS)

dump_video_SOURCES = dump_video.c
EXTRA_dump_video_SOURCES = getopt.c getopt1.c getopt.h
dump_video_LDADD = $(GETOPT_OBJS) $(LDADDDEC) $(COMPAT_LIBS)

dump_psnr_SOURCES = dump_psnr.c
EXTRA_dump_psnr_SOURCES = getopt.c getopt1.c getopt.h
dump_psnr_LDADD = $(GETOPT_OBJS) $(LDADDDEC) -lm

libtheora_info_SOURCES = libtheora_info.c
libtheora_info_LDADD = $(LDADDENC)

player_example_SOURCES = player_example.c
player_example_CFLAGS = $(SDL_CFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS)
player_example_LDADD = $(LDADDDEC) $(SDL_LIBS) $(VORBIS_LIBS) $(OSS_LIBS) -lm

encoder_example_SOURCES = encoder_example.c
EXTRA_encoder_example_SOURCES = getopt.c getopt1.c getopt.h
encoder_example_CFLAGS = $(OGG_CFLAGS) $(VORBIS_CFLAGS)
encoder_example_LDADD = $(GETOPT_OBJS) $(LDADDENC) $(VORBIS_LIBS) $(VORBISENC_LIBS) -lm

png2theora_SOURCES = png2theora.c
png2theora_CFLAGS = $(OGG_CFLAGS) $(PNG_CFLAGS)
png2theora_LDADD = $(GETOPT_OBJS) $(LDADDENC) $(PNG_LIBS) -lm

tiff2theora_SOURCES = tiff2theora.c
tiff2theora_CFLAGS = $(OGG_CFLAGS) $(TIFF_CFLAGS)
tiff2theora_LDADD = $(GETOPT_OBJS) $(LDADDENC) $(TIFF_LIBS) -lm

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"

