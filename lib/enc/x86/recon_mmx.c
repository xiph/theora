/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id: recon_mmx.c 14579 2008-03-12 06:42:40Z xiphmont $

 ********************************************************************/

#include "../codec_internal.h"
#include <stddef.h>

#if defined(USE_ASM)

/*TODO: This is basically oc_state_frag_copy_mmx() without the enclosing loop.
  Seems like one of these two should share the other's code.*/
static void oc_copy8x8_mmx(const unsigned char *_src,unsigned char *_dst,
 ogg_uint32_t _ystride){
  ptrdiff_t esi;
  __asm__ __volatile__(
    /*src+0*src_ystride*/
    "movq (%[src]),%%mm0\n\t"
    /*esi=src_ystride*3*/
    "lea (%[ystride],%[ystride],2),%[s]\n\t"
    /*src+1*src_ystride*/
    "movq (%[src],%[ystride]),%%mm1\n\t"
    /*src+2*src_ystride*/
    "movq (%[src],%[ystride],2),%%mm2\n\t"
    /*src+3*src_ystride*/
    "movq (%[src],%[s]),%%mm3\n\t"
    /*dst+0*dst_ystride*/
    "movq %%mm0,(%[dst])\n\t"
    /*dst+1*dst_ystride*/
    "movq %%mm1,(%[dst],%[ystride])\n\t"
    /*Pointer to next 4.*/
    "lea (%[src],%[ystride],4),%[src]\n\t" 
    /*dst+2*dst_ystride*/
    "movq %%mm2,(%[dst],%[ystride],2)\n\t"
    /*dst+3*dst_ystride*/
    "movq %%mm3,(%[dst],%[s])\n\t"
    /*Pointer to next 4.*/
    "lea (%[dst],%[ystride],4),%[dst]\n\t" 
    /*src+0*src_ystride*/
    "movq (%[src]),%%mm0\n\t"
    /*src+1*src_ystride*/
    "movq (%[src],%[ystride]),%%mm1\n\t"
    /*src+2*src_ystride*/
    "movq (%[src],%[ystride],2),%%mm2\n\t"
    /*src+3*src_ystride*/
    "movq (%[src],%[s]),%%mm3\n\t"
    /*dst+0*dst_ystride*/
    "movq %%mm0,(%[dst])\n\t"
    /*dst+1*dst_ystride*/
    "movq %%mm1,(%[dst],%[ystride])\n\t"
    /*dst+2*dst_ystride*/
    "movq %%mm2,(%[dst],%[ystride],2)\n\t"
    /*dst+3*dst_ystride*/
    "movq %%mm3,(%[dst],%[s])\n\t"
    :[s]"=&S"(esi)
    :[dst]"r"(_dst),[src]"r"(_src),[ystride]"r"((ptrdiff_t)_ystride)
    :"memory"
  );
}

/*TODO: There isn't much penalty to just re-using
   oc_frag_recon_inter_mmx() from the decoder here; we should do that.*/
static void oc_recon8x8_mmx(unsigned char *_dst,const ogg_int16_t *_residue,
 ogg_uint32_t _ystride){
  ptrdiff_t s;
  int       i;
  /*Zero mm0.*/
  __asm__ __volatile__("pxor %%mm0,%%mm0\n\t"::);
  for(i=8;i-->0;){
    __asm__ __volatile__(
      /*Load mm2 with _src*/
      "movq (%[dst]),%%mm2\n\t"
      /*Load mm4 with low part of residue.*/
      "movq (%[res]),%%mm4\n\t"
      /*Load mm5 with high part of residue.*/
      "movq 8(%[res]),%%mm5\n\t"
      /*Copy mm2 to mm3.*/
      "movq %%mm2,%%mm3\n\t"
      /*Expand low part of _src to 16 bits.*/
      "punpcklbw %%mm0,%%mm2\n\t"
      /*Expand high part of _src to 16 bits.*/
      "punpckhbw %%mm0,%%mm3\n\t"
      /*Add low part with low part of residue.*/
      "paddsw %%mm4,%%mm2\n\t"
      /*High with high.*/
      "paddsw %%mm5,%%mm3\n\t"
      /*Pack and saturate to mm2.*/
      "packuswb %%mm3,%%mm2\n\t"
      /*_residue+=16*/
      "lea 16(%[res]),%[res]\n\t"
      /*Put mm2 to dest.*/
      "movq %%mm2,(%[dst])\n\t"
      /*_dst+=_dst_ystride*/
      "lea (%[dst],%[ystride]),%[dst]\n\t"
      :[dst]"+r"(_dst),[res]"+r"(_residue)
      :[ystride]"r"((ptrdiff_t)_ystride)
      :"memory"
    );
  }
}

void dsp_mmx_recon_init(DspFunctions *_funcs){
  _funcs->copy8x8=oc_copy8x8_mmx;
  _funcs->recon8x8=oc_recon8x8_mmx;
}

#endif /* USE_ASM */
