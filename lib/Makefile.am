INCLUDES = -I$(top_srcdir)/include
AM_CFLAGS = $(OGG_CFLAGS) $(CAIRO_CFLAGS)

EXTRA_DIST = \
	cpu.c \
        enc/x86/fdct_mmx.c \
        enc/x86/fdct_sse2.c \
        enc/x86/mmxenc.c \
        enc/x86/mmxencfrag.c \
        enc/x86/x86enc.c \
        enc/x86_32_vs/dsp_mmx.c \
        enc/x86_32_vs/fdct_mmx.c \
        enc/dct_encode.c \
        enc/encode.c \
        enc/encoder_toplevel.c \
        dec/x86_vc

lib_LTLIBRARIES = libtheoradec.la libtheoraenc.la libtheora.la

if THEORA_DISABLE_ENCODE
encoder_sources = \
	enc/encapiwrapper.c \
	enc/encoder_disabled.c
else
encoder_uniq_x86_sources = \
	enc/x86/mmxenc.c \
	enc/x86/mmxencfrag.c \
	enc/x86/mmxfdct.c \
	enc/x86/x86enc.c

encoder_uniq_x86_64_sources = \
	enc/x86/sse2fdct.c

encoder_shared_x86_sources = \
	dec/x86/mmxfrag.c \
	dec/x86/mmxidct.c

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
	enc/dct.c \
	enc/dct_decode.c \
	enc/dct_encode.c \
	enc/encfrag.c \
	enc/encapiwrapper.c \
	enc/encode.c \
	enc/encoder_toplevel.c \
	enc/encoder_quant.c \
	enc/frarray.c \
	enc/frinit.c \
	enc/huffenc.c \
	enc/mathops.c \
	enc/mcenc.c \
	enc/mode.c \
	$(encoder_uniq_arch_sources)

encoder_sources = \
	dec/fragment.c \
	dec/idct.c \
	dec/internal.c \
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
	enc/codec_internal.h \
	enc/encoder_huffman.h \
	enc/encoder_lookup.h \
	enc/enquant.h \
	enc/hufftables.h \
	enc/mathops.h \
	enc/mode_select.h \
	enc/quant_lookup.h \
	enc/toplevel_lookup.h \
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
	Version_script-dec
libtheoradec_la_LDFLAGS = \
  -version-info @THDEC_LIB_CURRENT@:@THDEC_LIB_REVISION@:@THDEC_LIB_AGE@ \
  @THEORADEC_LDFLAGS@ @CAIRO_LIBS@

libtheoraenc_la_SOURCES = \
	$(encoder_sources) \
	Version_script-enc
libtheoraenc_la_LDFLAGS = \
  -version-info @THENC_LIB_CURRENT@:@THENC_LIB_REVISION@:@THENC_LIB_AGE@ \
  @THEORAENC_LDFLAGS@ $(OGG_LIBS)

libtheora_la_SOURCES = \
	$(decoder_sources) \
	$(encoder_uniq_sources) \
	Version_script
libtheora_la_LDFLAGS = \
  -version-info @TH_LIB_CURRENT@:@TH_LIB_REVISION@:@TH_LIB_AGE@ \
  @THEORA_LDFLAGS@ @CAIRO_LIBS@ $(OGG_LIBS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@" 

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
