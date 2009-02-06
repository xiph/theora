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

#include <stdlib.h>
#include <string.h>
#include "codec_internal.h"
#include "../cpu.c"

#define DSP_OP_AVG(a,b) ((((int)(a)) + ((int)(b)))/2)
#define DSP_OP_DIFF(a,b) (((int)(a)) - ((int)(b)))
#define DSP_OP_ABS_DIFF(a,b) abs((((int)(a)) - ((int)(b))))

static void set8x8__c (unsigned char val, unsigned char *ptr,
		       ogg_uint32_t PixelsPerLine){
  /* For each block row */
  memset(ptr,val,8);
  ptr+=PixelsPerLine;
  memset(ptr,val,8);
  ptr+=PixelsPerLine;
  memset(ptr,val,8);
  ptr+=PixelsPerLine;
  memset(ptr,val,8);
  ptr+=PixelsPerLine;
  memset(ptr,val,8);
  ptr+=PixelsPerLine;
  memset(ptr,val,8);
  ptr+=PixelsPerLine;
  memset(ptr,val,8);
  ptr+=PixelsPerLine;
  memset(ptr,val,8);
}

static void sub8x8__c (const unsigned char *FiltPtr, const unsigned char *ReconPtr,
		       ogg_int16_t *DctInputPtr, ogg_uint32_t PixelsPerLine){
  int i;

  /* For each block row */
  for (i=8; i; i--) {
    DctInputPtr[0] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[0], ReconPtr[0]);
    DctInputPtr[1] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[1], ReconPtr[1]);
    DctInputPtr[2] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[2], ReconPtr[2]);
    DctInputPtr[3] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[3], ReconPtr[3]);
    DctInputPtr[4] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[4], ReconPtr[4]);
    DctInputPtr[5] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[5], ReconPtr[5]);
    DctInputPtr[6] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[6], ReconPtr[6]);
    DctInputPtr[7] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[7], ReconPtr[7]);

    /* Start next row */
    FiltPtr += PixelsPerLine;
    ReconPtr += PixelsPerLine;
    DctInputPtr += 8;
  }
}

static void sub8x8_128__c (const unsigned char *FiltPtr, ogg_int16_t *DctInputPtr,
			   ogg_uint32_t PixelsPerLine) {
  int i;
  /* For each block row */
  for (i=8; i; i--) {
    /* INTRA mode so code raw image data */
    /* We convert the data to 8 bit signed (by subtracting 128) as
       this reduces the internal precision requirments in the DCT
       transform. */
    DctInputPtr[0] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[0], 128);
    DctInputPtr[1] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[1], 128);
    DctInputPtr[2] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[2], 128);
    DctInputPtr[3] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[3], 128);
    DctInputPtr[4] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[4], 128);
    DctInputPtr[5] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[5], 128);
    DctInputPtr[6] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[6], 128);
    DctInputPtr[7] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[7], 128);

    /* Start next row */
    FiltPtr += PixelsPerLine;
    DctInputPtr += 8;
  }
}

static ogg_uint32_t sad8x8__c (const unsigned char *ptr1, 
			       const unsigned char *ptr2, 
			       ogg_uint32_t stride)
{
  ogg_uint32_t  i;
  ogg_uint32_t  sad = 0;

  for (i=8; i; i--) {
    sad += DSP_OP_ABS_DIFF(ptr1[0], ptr2[0]);
    sad += DSP_OP_ABS_DIFF(ptr1[1], ptr2[1]);
    sad += DSP_OP_ABS_DIFF(ptr1[2], ptr2[2]);
    sad += DSP_OP_ABS_DIFF(ptr1[3], ptr2[3]);
    sad += DSP_OP_ABS_DIFF(ptr1[4], ptr2[4]);
    sad += DSP_OP_ABS_DIFF(ptr1[5], ptr2[5]);
    sad += DSP_OP_ABS_DIFF(ptr1[6], ptr2[6]);
    sad += DSP_OP_ABS_DIFF(ptr1[7], ptr2[7]);

    /* Step to next row of block. */
    ptr1 += stride;
    ptr2 += stride;
  }

  return sad;
}

static ogg_uint32_t sad8x8_thres__c (const unsigned char *ptr1, 
				     const unsigned char *ptr2, 
				     ogg_uint32_t stride, 
				     ogg_uint32_t thres)
{
  ogg_uint32_t  i;
  ogg_uint32_t  sad = 0;

  for (i=8; i; i--) {
    sad += DSP_OP_ABS_DIFF(ptr1[0], ptr2[0]);
    sad += DSP_OP_ABS_DIFF(ptr1[1], ptr2[1]);
    sad += DSP_OP_ABS_DIFF(ptr1[2], ptr2[2]);
    sad += DSP_OP_ABS_DIFF(ptr1[3], ptr2[3]);
    sad += DSP_OP_ABS_DIFF(ptr1[4], ptr2[4]);
    sad += DSP_OP_ABS_DIFF(ptr1[5], ptr2[5]);
    sad += DSP_OP_ABS_DIFF(ptr1[6], ptr2[6]);
    sad += DSP_OP_ABS_DIFF(ptr1[7], ptr2[7]);

    if (sad > thres )
      break;

    /* Step to next row of block. */
    ptr1 += stride;
    ptr2 += stride;
  }

  return sad;
}

static ogg_uint32_t sad8x8_xy2_thres__c (const unsigned char *SrcData, 
					 const unsigned char *RefDataPtr1,
					 const unsigned char *RefDataPtr2, 
					 ogg_uint32_t Stride,
					 ogg_uint32_t thres)
{
  ogg_uint32_t  i;
  ogg_uint32_t  sad = 0;

  for (i=8; i; i--) {
    sad += DSP_OP_ABS_DIFF(SrcData[0], DSP_OP_AVG (RefDataPtr1[0], RefDataPtr2[0]));
    sad += DSP_OP_ABS_DIFF(SrcData[1], DSP_OP_AVG (RefDataPtr1[1], RefDataPtr2[1]));
    sad += DSP_OP_ABS_DIFF(SrcData[2], DSP_OP_AVG (RefDataPtr1[2], RefDataPtr2[2]));
    sad += DSP_OP_ABS_DIFF(SrcData[3], DSP_OP_AVG (RefDataPtr1[3], RefDataPtr2[3]));
    sad += DSP_OP_ABS_DIFF(SrcData[4], DSP_OP_AVG (RefDataPtr1[4], RefDataPtr2[4]));
    sad += DSP_OP_ABS_DIFF(SrcData[5], DSP_OP_AVG (RefDataPtr1[5], RefDataPtr2[5]));
    sad += DSP_OP_ABS_DIFF(SrcData[6], DSP_OP_AVG (RefDataPtr1[6], RefDataPtr2[6]));
    sad += DSP_OP_ABS_DIFF(SrcData[7], DSP_OP_AVG (RefDataPtr1[7], RefDataPtr2[7]));

    if ( sad > thres )
      break;

    /* Step to next row of block. */
    SrcData += Stride;
    RefDataPtr1 += Stride;
    RefDataPtr2 += Stride;
  }

  return sad;
}

static void nop (void) { /* NOP */ }

void dsp_init(DspFunctions *funcs)
{
  funcs->save_fpu = nop;
  funcs->restore_fpu = nop;
  funcs->set8x8 = set8x8__c;
  funcs->sub8x8 = sub8x8__c;
  funcs->sub8x8_128 = sub8x8_128__c;
  funcs->sad8x8 = sad8x8__c;
  funcs->sad8x8_thres = sad8x8_thres__c;
  funcs->sad8x8_xy2_thres = sad8x8_xy2_thres__c;
}

void dsp_static_init(DspFunctions *funcs)
{
  ogg_uint32_t cpuflags;

  cpuflags = oc_cpu_flags_get ();
  dsp_init (funcs);

  dsp_recon_init (funcs, cpuflags);
  dsp_dct_init (funcs, cpuflags);
#if defined(USE_ASM)
  if (cpuflags & OC_CPU_X86_MMX) {
    dsp_mmx_init(funcs);
  }
# ifndef WIN32
  /* This is implemented for win32 yet */
  if (cpuflags & OC_CPU_X86_MMXEXT) {
    dsp_mmxext_init(funcs);
  }
# endif
#endif
}

