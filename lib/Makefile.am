AUTOMAKE_OPTIONS = foreign

INCLUDES = -I$(top_srcdir)/include

lib_LTLIBRARIES = libtheora.la

libtheora_la_SOURCES = encode.c hufftables.h quant_lookup.h \
	encoder_internal.h idct.c reconstruct.c block_inline.h \
	encoder_lookup.h mcomp.c scan.c blockmap.c misc_common.c \
	dct.c frarray.c pb.c dct_decode.c frinit.c pp.c dct_encode.c \
	huffman.c pp.h toplevel.c decode.c huffman.h quant.c \
	comment.c toplevel_lookup.h mcomp.h

libtheora_la_LDFLAGS = -version-info @V_LIB_CURRENT@:@V_LIB_REVISION@:@V_LIB_AGE@

CLEANFILES = $(lib_LTLIBRARIES)

debug:
	$(MAKE) all CFLAGS="@DEBUG@" LDFLAGS="-lefence"	

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
