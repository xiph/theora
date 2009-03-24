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
  last mod: $Id: dsp_mmx.c 14579 2008-03-12 06:42:40Z xiphmont $

 ********************************************************************/

#include <stdlib.h>

#include "../codec_internal.h"
#include "../dsp.h"

#if defined(USE_ASM)

static const __attribute__ ((aligned(8),used)) ogg_int64_t V128 = 0x0080008000800080LL;

#define DSP_OP_AVG(a,b) ((((int)(a)) + ((int)(b)))/2)
#define DSP_OP_DIFF(a,b) (((int)(a)) - ((int)(b)))
#define DSP_OP_ABS_DIFF(a,b) abs((((int)(a)) - ((int)(b))))

static void sub8x8__mmx (const unsigned char *FiltPtr, const unsigned char *ReconPtr,
                         ogg_int16_t *DctInputPtr, ogg_uint32_t PixelsPerLine)
{
  __asm__ __volatile__ (
    "  .p2align 4                   \n\t"

    "  pxor        %%mm7, %%mm7     \n\t"

    ".rept 8                        \n\t"
    "  movq        (%0), %%mm0      \n\t" /* mm0 = FiltPtr */
    "  movq        (%1), %%mm1      \n\t" /* mm1 = ReconPtr */
    "  movq        %%mm0, %%mm2     \n\t" /* dup to prepare for up conversion */
    "  movq        %%mm1, %%mm3     \n\t" /* dup to prepare for up conversion */
    /* convert from UINT8 to INT16 */
    "  punpcklbw   %%mm7, %%mm0     \n\t" /* mm0 = INT16(FiltPtr) */
    "  punpcklbw   %%mm7, %%mm1     \n\t" /* mm1 = INT16(ReconPtr) */
    "  punpckhbw   %%mm7, %%mm2     \n\t" /* mm2 = INT16(FiltPtr) */
    "  punpckhbw   %%mm7, %%mm3     \n\t" /* mm3 = INT16(ReconPtr) */
    /* start calculation */
    "  psubw       %%mm1, %%mm0     \n\t" /* mm0 = FiltPtr - ReconPtr */
    "  psubw       %%mm3, %%mm2     \n\t" /* mm2 = FiltPtr - ReconPtr */
    "  movq        %%mm0,  (%2)     \n\t" /* write answer out */
    "  movq        %%mm2, 8(%2)     \n\t" /* write answer out */
    /* Increment pointers */
    "  add         $16, %2          \n\t"
    "  add         %3, %0           \n\t"
    "  add         %3, %1           \n\t"
    ".endr                          \n\t"

     : "+r" (FiltPtr),
       "+r" (ReconPtr),
       "+r" (DctInputPtr)

     : "r" ((unsigned long)PixelsPerLine)
     : "memory"
  );
}

static void sub8x8_128__mmx (const unsigned char *FiltPtr, ogg_int16_t *DctInputPtr,
                             ogg_uint32_t PixelsPerLine)
{

  __asm__ __volatile__ (
    "  .p2align 4                   \n\t"

    "  pxor        %%mm7, %%mm7     \n\t"
    "  movq        %[V128], %%mm1   \n\t"

    ".rept 8                        \n\t"
    "  movq        (%0), %%mm0      \n\t" /* mm0 = FiltPtr */
    "  movq        %%mm0, %%mm2     \n\t" /* dup to prepare for up conversion */
    /* convert from UINT8 to INT16 */
    "  punpcklbw   %%mm7, %%mm0     \n\t" /* mm0 = INT16(FiltPtr) */
    "  punpckhbw   %%mm7, %%mm2     \n\t" /* mm2 = INT16(FiltPtr) */
    /* start calculation */
    "  psubw       %%mm1, %%mm0     \n\t" /* mm0 = FiltPtr - 128 */
    "  psubw       %%mm1, %%mm2     \n\t" /* mm2 = FiltPtr - 128 */
    "  movq        %%mm0,  (%1)     \n\t" /* write answer out */
    "  movq        %%mm2, 8(%1)     \n\t" /* write answer out */
    /* Increment pointers */
    "  add         $16, %1           \n\t"
    "  add         %2, %0           \n\t"
    ".endr                          \n\t"

     : "+r" (FiltPtr),
       "+r" (DctInputPtr)
     : "r" ((unsigned long)PixelsPerLine),
       [V128] "m" (V128)
     : "memory"
  );
}

static void restore_fpu (void)
{
  __asm__ __volatile__ (
    "  emms                         \n\t"
  );
}

void dsp_mmx_init(DspFunctions *funcs)
{
  funcs->restore_fpu = restore_fpu;
  funcs->sub8x8 = sub8x8__mmx;
  funcs->sub8x8_128 = sub8x8_128__mmx;
}

#endif /* USE_ASM */
