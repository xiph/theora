INCLUDES = -I$(top_srcdir)/include

lib_LTLIBRARIES = libtheora.la

if THEORA_SUPPORT_ENCODE
encoder_sources = dct_encode.c encode.c encoder_toplevel.c
else
encoder_sources = encoder_disabled.c
endif

libtheora_la_SOURCES = \
	blockmap.c \
	comment.c \
	dct.c \
	dct_decode.c \
	decode.c \
	frarray.c \
	frinit.c \
	huffman.c \
	idct.c \
	mcomp.c \
	misc_common.c \
	pb.c \
	pp.c \
	quant.c \
	reconstruct.c \
	scan.c \
	toplevel.c \
	$(encoder_sources)

noinst_HEADERS = \
	block_inline.h \
	encoder_internal.h \
	encoder_lookup.h \
	huffman.h \
	hufftables.h \
	pp.h \
	quant_lookup.h \
	toplevel.h \
	toplevel_lookup.h

libtheora_la_CFLAGS = $(OGG_CFLAGS)
libtheora_la_LDFLAGS = -version-info @V_LIB_CURRENT@:@V_LIB_REVISION@:@V_LIB_AGE@
libtheora_la_LIBS = $(OGG_LIBS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@" LDFLAGS="-lefence"	

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
