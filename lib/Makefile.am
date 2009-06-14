INCLUDES = -I$(top_srcdir)/include
AM_CFLAGS = $(OGG_CFLAGS) $(CAIRO_CFLAGS)

EXTRA_DIST = \
	cpu.c \
	enc/x86/dsp_sse2.c \
	enc/x86/mmxencfrag.c \
	enc/x86/mmxfdct.c \
	enc/x86/sse2fdct.c \
	enc/x86/x86enc.c \
	enc/x86/x86enc.h \
	enc/x86_32_vs/dsp_mmx.c \
	enc/x86_32_vs/fdct_mmx.c \
	enc/x86_32_vs/recon_mmx.c \
	enc/analyize.c \
	enc/encinfo.c \
	enc/encode.c \
	enc/rate.c \
	enc/tokenize.c \
	dec/x86/mmxfrag.c \
	dec/x86/mmxfrag.h \
	dec/x86/mmxidct.c \
	dec/x86/mmxloop.h \
	dec/x86/mmxstate.c \
	dec/x86/x86int.h \
	dec/x86/x86state.c \
	dec/x86_vc

lib_LTLIBRARIES = libtheoradec.la libtheoraenc.la libtheora.la

if THEORA_DISABLE_ENCODE
encoder_uniq_sources = \
	enc/encoder_disabled.c

encoder_sources = \
	$(encoder_uniq_sources)
else
encoder_uniq_x86_sources = \
	enc/x86/mmxencfrag.c \
	enc/x86/mmxfdct.c \
	enc/x86/x86enc.c

encoder_uniq_x86_64_sources = \
	enc/x86/sse2fdct.c

encoder_shared_x86_sources = \
	dec/x86/mmxfrag.c \
	dec/x86/mmxidct.c \
	dec/x86/mmxstate.c \
	dec/x86/x86state.c

encoder_shared_x86_64_sources =

if CPU_x86_64
encoder_uniq_arch_sources = \
 $(encoder_uniq_x86_sources) \
 $(encoder_uniq_x86_64_sources)
encoder_shared_arch_sources = \
 $(encoder_shared_x86_sources) \
 $(encoder_shared_x86_64_sources)
else
if CPU_x86_32
encoder_uniq_arch_sources = $(encoder_uniq_x86_sources)
encoder_shared_arch_sources = $(encoder_shared_x86_sources)
else
encoder_uniq_arch_sources =
encoder_shared_arch_sources =
endif
endif

encoder_uniq_sources = \
	enc/analyze.c \
	enc/fdct.c \
	enc/encfrag.c \
	enc/encapiwrapper.c \
	enc/encinfo.c \
	enc/encode.c \
	enc/enquant.c \
	enc/huffenc.c \
	enc/mathops.c \
	enc/mcenc.c \
	enc/rate.c \
	enc/tokenize.c \
	$(encoder_uniq_arch_sources)

encoder_sources = \
	dec/apiwrapper.c \
	dec/fragment.c \
	dec/idct.c \
	dec/internal.c \
	dec/state.c \
	dec/quant.c \
	$(encoder_shared_arch_sources) \
	$(encoder_uniq_sources)

endif

decoder_x86_sources = \
	dec/x86/mmxidct.c \
	dec/x86/mmxfrag.c \
	dec/x86/mmxstate.c \
	dec/x86/x86state.c
if CPU_x86_64
decoder_arch_sources = $(decoder_x86_sources)
else
if CPU_x86_32
decoder_arch_sources = $(decoder_x86_sources)
else
decoder_arch_sources =
endif
endif

decoder_sources = \
	dec/apiwrapper.c \
	dec/bitpack.c \
	dec/decapiwrapper.c \
	dec/decinfo.c \
	dec/decode.c \
	dec/dequant.c \
	dec/fragment.c \
	dec/huffdec.c \
	dec/idct.c \
	dec/info.c \
	dec/internal.c \
	dec/quant.c \
	dec/state.c \
	$(decoder_arch_sources)

noinst_HEADERS = \
	cpu.h \
	internal.h \
	enc/enquant.h \
	enc/huffenc.h \
	enc/mathops.h \
	enc/modedec.h \
	dec/apiwrapper.h \
	dec/bitpack.h \
	dec/dct.h \
	dec/decint.h \
	dec/dequant.h \
	dec/huffdec.h \
	dec/huffman.h \
	dec/ocintrin.h \
	dec/quant.h \
	dec/x86/x86int.h

libtheoradec_la_SOURCES = \
	$(decoder_sources) \
	Version_script-dec theoradec.exp
libtheoradec_la_LDFLAGS = \
  -version-info @THDEC_LIB_CURRENT@:@THDEC_LIB_REVISION@:@THDEC_LIB_AGE@ \
  @THEORADEC_LDFLAGS@ @CAIRO_LIBS@

libtheoraenc_la_SOURCES = \
	$(encoder_sources) \
	Version_script-enc theoraenc.exp
libtheoraenc_la_LDFLAGS = \
  -version-info @THENC_LIB_CURRENT@:@THENC_LIB_REVISION@:@THENC_LIB_AGE@ \
  @THEORAENC_LDFLAGS@ $(OGG_LIBS)

libtheora_la_SOURCES = \
	$(decoder_sources) \
	$(encoder_uniq_sources) \
	Version_script theora.exp
libtheora_la_LDFLAGS = \
  -version-info @TH_LIB_CURRENT@:@TH_LIB_REVISION@:@TH_LIB_AGE@ \
  @THEORA_LDFLAGS@ @CAIRO_LIBS@ $(OGG_LIBS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@" 

profile:
	$(MAKE) all CFLAGS="@PROFILE@"

# contstruct various symbol export list files
.def.exp : defexp.awk
	awk -f defexp.awk $< > $@
