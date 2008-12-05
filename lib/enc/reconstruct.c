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
  last mod: $Id$

 ********************************************************************/

#include "codec_internal.h"

static void copy8x8__c (const unsigned char *src,
			unsigned char *dest,
			unsigned int stride)
{
  int j;
  for ( j = 8; j ; --j ){
    ((ogg_uint32_t*)dest)[0] = ((ogg_uint32_t*)src)[0];
    ((ogg_uint32_t*)dest)[1] = ((ogg_uint32_t*)src)[1];
    src+=stride;
    dest+=stride;
  }
}

static void copy8x8_half__c (const unsigned char *src1,
			     const unsigned char *src2, 
			     unsigned char *dest,
			     unsigned int stride)
{
  int j;

  for (j = 8; j; --j){
    dest[0] = ((int)src1[0] + (int)src2[0]) >> 1;
    dest[1] = ((int)src1[1] + (int)src2[1]) >> 1;
    dest[2] = ((int)src1[2] + (int)src2[2]) >> 1;
    dest[3] = ((int)src1[3] + (int)src2[3]) >> 1;
    dest[4] = ((int)src1[4] + (int)src2[4]) >> 1;
    dest[5] = ((int)src1[5] + (int)src2[5]) >> 1;
    dest[6] = ((int)src1[6] + (int)src2[6]) >> 1;
    dest[7] = ((int)src1[7] + (int)src2[7]) >> 1;

    src1+=stride;
    src2+=stride;
    dest+=stride;
  }
}


static void recon8x8__c (unsigned char *ReconPtr, 
			 const ogg_int16_t *ChangePtr, ogg_uint32_t LineStep)
{
  ogg_uint32_t i;

  for (i = 8; i; i--){
    ReconPtr[0] = clamp255(ReconPtr[0] + ChangePtr[0]);
    ReconPtr[1] = clamp255(ReconPtr[1] + ChangePtr[1]);
    ReconPtr[2] = clamp255(ReconPtr[2] + ChangePtr[2]);
    ReconPtr[3] = clamp255(ReconPtr[3] + ChangePtr[3]);
    ReconPtr[4] = clamp255(ReconPtr[4] + ChangePtr[4]);
    ReconPtr[5] = clamp255(ReconPtr[5] + ChangePtr[5]);
    ReconPtr[6] = clamp255(ReconPtr[6] + ChangePtr[6]);
    ReconPtr[7] = clamp255(ReconPtr[7] + ChangePtr[7]);

    ChangePtr += 8;
    ReconPtr += LineStep;
  }
}

void dsp_recon_init (DspFunctions *funcs, ogg_uint32_t cpu_flags)
{
  funcs->copy8x8 = copy8x8__c;
  funcs->copy8x8_half = copy8x8_half__c;
  funcs->recon8x8 = recon8x8__c;
#if defined(USE_ASM)
  if (cpu_flags & OC_CPU_X86_MMX) {
    dsp_mmx_recon_init(funcs);
  }
#endif
}
