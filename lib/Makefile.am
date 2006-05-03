INCLUDES = -I$(top_srcdir)/include

EXTRA_DIST = Version_script.in \
	x86_32/dsp_mmx.c \
	x86_32/dsp_mmxext.c \
	x86_32/recon_mmx.c \
	x86_32/fdct_mmx.c \
	x86_64/dsp_mmx.c \
	x86_64/dsp_mmxext.c \
	x86_64/recon_mmx.c \
	x86_64/fdct_mmx.c

lib_LTLIBRARIES = libtheora.la

if THEORA_DISABLE_ENCODE
encoder_sources = encoder_disabled.c
else
encoder_sources = dct_encode.c encode.c encoder_toplevel.c
endif

if CPU_x86_64
arch_dir = x86_64
arch_sources= \
	$(arch_dir)/dsp_mmx.c \
	$(arch_dir)/dsp_mmxext.c \
	$(arch_dir)/recon_mmx.c \
	$(arch_dir)/fdct_mmx.c
else
if CPU_x86_32
arch_dir = x86_32
arch_sources= \
	$(arch_dir)/dsp_mmx.c \
	$(arch_dir)/dsp_mmxext.c \
	$(arch_dir)/recon_mmx.c \
	$(arch_dir)/fdct_mmx.c
endif
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
	cpu.c \
	dsp.c \
  $(arch_sources) \
	$(encoder_sources)

noinst_HEADERS = \
	block_inline.h \
	codec_internal.h \
	encoder_lookup.h \
	huffman.h \
	hufftables.h \
	pp.h \
	quant_lookup.h \
	toplevel.h \
	toplevel_lookup.h \
	cpu.h \
	dsp.h

libtheora_la_CFLAGS = $(OGG_CFLAGS)
libtheora_la_LDFLAGS = -version-info @V_LIB_CURRENT@:@V_LIB_REVISION@:@V_LIB_AGE@ @SHLIB_VERSION_ARG@
libtheora_la_LIBADD = $(OGG_LIBS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@" LDFLAGS="-lefence"	

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
