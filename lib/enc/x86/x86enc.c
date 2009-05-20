/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: x86state.c 15675 2009-02-06 09:43:27Z tterribe $

 ********************************************************************/
#include "x86enc.h"

#if defined(OC_X86_ASM)

#include "../../cpu.c"

void oc_enc_vtable_init_x86(CP_INSTANCE *_cpi){
  ogg_uint32_t cpu_flags;
  cpu_flags=oc_cpu_flags_get();
  oc_enc_vtable_init_c(_cpi);
  if(cpu_flags&OC_CPU_X86_MMX){
    _cpi->opt_vtable.frag_sub=oc_enc_frag_sub_mmx;
    _cpi->opt_vtable.frag_sub_128=oc_enc_frag_sub_128_mmx;
    _cpi->opt_vtable.frag_copy=oc_frag_copy_mmx;
    _cpi->opt_vtable.frag_recon_intra=oc_frag_recon_intra_mmx;
    _cpi->opt_vtable.frag_recon_inter=oc_frag_recon_inter_mmx;
    _cpi->opt_vtable.fdct8x8=oc_enc_fdct8x8_mmx;
    _cpi->opt_vtable.dequant_idct8x8=oc_dequant_idct8x8_mmx;
    _cpi->opt_vtable.enc_loop_filter=oc_enc_loop_filter_mmx;
    _cpi->opt_vtable.restore_fpu=oc_restore_fpu_mmx;
  }
  if(cpu_flags&OC_CPU_X86_MMXEXT){
    _cpi->opt_vtable.frag_sad=oc_enc_frag_sad_mmxext;
    _cpi->opt_vtable.frag_sad_thresh=oc_enc_frag_sad_thresh_mmxext;
    _cpi->opt_vtable.frag_sad2_thresh=oc_enc_frag_sad2_thresh_mmxext;
    _cpi->opt_vtable.frag_satd_thresh=oc_enc_frag_satd_thresh_mmxext;
    _cpi->opt_vtable.frag_satd2_thresh=oc_enc_frag_satd2_thresh_mmxext;
    _cpi->opt_vtable.frag_intra_satd=oc_enc_frag_intra_satd_mmxext;
    _cpi->opt_vtable.frag_copy2=oc_enc_frag_copy2_mmxext;
  }
  if(cpu_flags&OC_CPU_X86_SSE2){
# if defined(OC_X86_64_ASM)
    _cpi->opt_vtable.fdct8x8=oc_enc_fdct8x8_x86_64sse2;
# endif
  }
}
#endif
