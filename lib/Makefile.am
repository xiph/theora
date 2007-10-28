INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/lib -I$(top_srcdir)/lib/dec -I$(top_srcdir)/lib/enc

EXTRA_DIST = \
        enc/x86_32/dct_decode_mmx.c \
        enc/x86_32/dsp_mmx.c \
        enc/x86_32/dsp_mmxext.c \
        enc/x86_32/recon_mmx.c \
        enc/x86_32/fdct_mmx.c \
        enc/x86_32/idct_mmx.c \
        enc/x86_64/dsp_mmx.c \
        enc/x86_64/dsp_mmxext.c \
        enc/x86_64/recon_mmx.c \
        enc/x86_64/fdct_mmx.c \
        enc/x86_64/idct_mmx.c \
        enc/x86_32_vs/dsp_mmx.c \
        enc/x86_32_vs/fdct_mmx.c \
        enc/x86_32_vs/recon_mmx.c \
        enc/dct_encode.c \
        enc/encode.c \
        enc/encoder_toplevel.c

lib_LTLIBRARIES = libtheora.la

if THEORA_DISABLE_ENCODE
encoder_sources = \
	enc/encoder_disabled.c
else
encoder_sources = \
	enc/dct_encode.c \
	enc/encode.c \
	enc/encoder_huffman.c \
	enc/encoder_idct.c \
	enc/encoder_toplevel.c \
	enc/encoder_quant.c \
	enc/blockmap.c \
	enc/dct.c \
	enc/dct_decode.c \
	enc/frarray.c \
	enc/frinit.c \
	enc/mcomp.c \
	enc/misc_common.c \
	enc/pb.c \
	enc/pp.c \
	enc/reconstruct.c \
	enc/scan.c \
	enc/dsp.c

if CPU_x86_64
enc_arch_dir = enc/x86_64
encoder_arch_sources= \
	$(enc_arch_dir)/dct_decode_mmx.c \
	$(enc_arch_dir)/dsp_mmx.c \
	$(enc_arch_dir)/dsp_mmxext.c \
	$(enc_arch_dir)/recon_mmx.c \
	$(enc_arch_dir)/idct_mmx.c \
	$(enc_arch_dir)/fdct_mmx.c
else
if CPU_x86_32
enc_arch_dir = enc/x86_32
encoder_arch_sources= \
	$(enc_arch_dir)/dct_decode_mmx.c \
	$(enc_arch_dir)/dsp_mmx.c \
	$(enc_arch_dir)/dsp_mmxext.c \
	$(enc_arch_dir)/recon_mmx.c \
	$(enc_arch_dir)/idct_mmx.c \
	$(enc_arch_dir)/fdct_mmx.c
endif
endif

endif

decoder_sources = \
	dec/apiwrapper.c \
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
	dec/state.c

if CPU_x86_64
decoder_x86_sources = \
	dec/x86/mmxidct.c \
	dec/x86/mmxfrag.c \
	dec/x86/mmxstate.c \
	dec/x86/x86state.c
else
if CPU_x86_32
decoder_x86_sources = \
	dec/x86/mmxidct.c \
	dec/x86/mmxfrag.c \
	dec/x86/mmxstate.c \
	dec/x86/x86state.c
endif
endif

libtheora_la_SOURCES = \
  cpu.c \
	$(decoder_x86_sources) \
	$(decoder_sources) \
	$(encoder_arch_sources) \
	$(encoder_sources) \
  Version_script

noinst_HEADERS = \
	cpu.h \
	internal.h \
	enc/block_inline.h \
	enc/codec_internal.h \
	enc/encoder_lookup.h \
	enc/encoder_huffman.h \
	enc/hufftables.h \
	enc/pp.h \
	enc/quant_lookup.h \
	enc/toplevel_lookup.h \
	enc/dsp.h \
	dec/apiwrapper.h \
	dec/dct.h \
	dec/decint.h \
	dec/dequant.h \
	dec/enquant.h \
	dec/huffdec.h \
	dec/huffman.h \
	dec/idct.h \
	dec/ocintrin.h \
	dec/quant.h \
	dec/x86/x86int.h

libtheora_la_CFLAGS = $(OGG_CFLAGS)
libtheora_la_LDFLAGS = -version-info @TH_LIB_CURRENT@:@TH_LIB_REVISION@:@TH_LIB_AGE@ @THEORA_LDFLAGS@
libtheora_la_LIBADD = $(OGG_LIBS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@" 

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
