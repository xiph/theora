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

#include "codec_internal.h"

#if defined(USE_ASM)

static const __attribute__ ((aligned(8),used)) ogg_int64_t V128 = 0x8080808080808080LL;

static void copy8x8__mmx (const unsigned char *src,
                          unsigned char *dest,
                          ogg_uint32_t stride)
{
  __asm__ __volatile__ (
    "  .balign 16                      \n\t"

    "  lea         (%2, %2, 2), %%rdi  \n\t"

    "  movq        (%1), %%mm0         \n\t"
    "  movq        (%1, %2), %%mm1     \n\t"
    "  movq        (%1, %2, 2), %%mm2  \n\t"
    "  movq        (%1, %%rdi), %%mm3  \n\t"

    "  lea         (%1, %2, 4), %1     \n\t" 

    "  movq        %%mm0, (%0)         \n\t"
    "  movq        %%mm1, (%0, %2)     \n\t"
    "  movq        %%mm2, (%0, %2, 2)  \n\t"
    "  movq        %%mm3, (%0, %%rdi)  \n\t"

    "  lea         (%0, %2, 4), %0     \n\t" 

    "  movq        (%1), %%mm0         \n\t"
    "  movq        (%1, %2), %%mm1     \n\t"
    "  movq        (%1, %2, 2), %%mm2  \n\t"
    "  movq        (%1, %%rdi), %%mm3  \n\t"

    "  movq        %%mm0, (%0)         \n\t"
    "  movq        %%mm1, (%0, %2)     \n\t"
    "  movq        %%mm2, (%0, %2, 2)  \n\t"
    "  movq        %%mm3, (%0, %%rdi)  \n\t"
      : "+a" (dest)
      : "c" (src),
        "d" ((unsigned long)stride)
      : "memory", "rdi"
  );
}

static void recon8x8__mmx (unsigned char *ReconPtr, 
			   const ogg_int16_t *ChangePtr, 
			   ogg_uint32_t LineStep)
{
  __asm__ __volatile__ (
    "  .balign 16                      \n\t"

    "  pxor        %%mm0, %%mm0        \n\t"
    "  lea         128(%1), %%rdi      \n\t"

    "1:                                \n\t"
    "  movq        (%0), %%mm2         \n\t" /* (+3 misaligned) 8 reference pixels */

    "  movq        (%1), %%mm4         \n\t" /* first 4 changes */
    "  movq        %%mm2, %%mm3        \n\t"
    "  movq        8(%1), %%mm5        \n\t" /* last 4 changes */
    "  punpcklbw   %%mm0, %%mm2        \n\t" /* turn first 4 refs into positive 16-bit #s */
    "  paddsw      %%mm4, %%mm2        \n\t" /* add in first 4 changes */
    "  punpckhbw   %%mm0, %%mm3        \n\t" /* turn last 4 refs into positive 16-bit #s */
    "  paddsw      %%mm5, %%mm3        \n\t" /* add in last 4 changes */

    "  packuswb    %%mm3, %%mm2        \n\t" /* pack result to unsigned 8-bit values */
    "  lea         16(%1), %1          \n\t" /* next row of changes */
    "  cmp         %%rdi, %1           \n\t" /* are we done? */

    "  movq        %%mm2, (%0)         \n\t" /* store result */

    "  lea         (%0, %2), %0        \n\t" /* next row of output */
    "  jc          1b                  \n\t"
      : "+r" (ReconPtr)
      : "r" (ChangePtr),
        "r" ((unsigned long)LineStep)
      : "memory", "rdi"
  );
}

void dsp_mmx_recon_init(DspFunctions *funcs)
{
  funcs->copy8x8 = copy8x8__mmx;
  funcs->recon8x8 = recon8x8__mmx;
}

#endif /* USE_ASM */
