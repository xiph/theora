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
  last mod: $Id: dsp_mmxext.c 14579 2008-03-12 06:42:40Z xiphmont $

 ********************************************************************/

#include <stdlib.h>

#include "codec_internal.h"
#include "dsp.h"

#if defined(USE_ASM)

static ogg_uint32_t sad8x8__mmxext (const unsigned char *ptr1, const unsigned char *ptr2, 
				    ogg_uint32_t stride)
{
  ogg_uint32_t  DiffVal;

  __asm__ __volatile__ (
    "  .balign 16                   \n\t"
    "  pxor %%mm7, %%mm7            \n\t" 	/* mm7 contains the result */

    ".rept 7                        \n\t"
    "  movq (%1), %%mm0             \n\t"	/* take 8 bytes */
    "  movq (%2), %%mm1             \n\t"
    "  psadbw %%mm1, %%mm0          \n\t"
    "  add %3, %1                   \n\t"	/* Inc pointer into the new data */
    "  paddw %%mm0, %%mm7           \n\t"	/* accumulate difference... */
    "  add %3, %2                   \n\t"	/* Inc pointer into ref data */
    ".endr                          \n\t"

    "  movq (%1), %%mm0             \n\t"	/* take 8 bytes */
    "  movq (%2), %%mm1             \n\t"
    "  psadbw %%mm1, %%mm0          \n\t"
    "  paddw %%mm0, %%mm7           \n\t"	/* accumulate difference... */
    "  movd %%mm7, %0               \n\t"

     : "=r" (DiffVal),
       "+r" (ptr1), 
       "+r" (ptr2) 
     : "r" ((unsigned long)stride)
     : "memory"
  );

  return DiffVal;
}

static ogg_uint32_t sad8x8_thres__mmxext (const unsigned char *ptr1, const unsigned char *ptr2, 
					  ogg_uint32_t stride, ogg_uint32_t thres)
{
  ogg_uint32_t  DiffVal;

  __asm__ __volatile__ (
    "  .balign 16                   \n\t"
    "  pxor %%mm7, %%mm7            \n\t" 	/* mm7 contains the result */

    ".rept 8                        \n\t"
    "  movq (%1), %%mm0             \n\t"	/* take 8 bytes */
    "  movq (%2), %%mm1             \n\t"
    "  psadbw %%mm1, %%mm0          \n\t"
    "  add %3, %1                   \n\t"	/* Inc pointer into the new data */
    "  paddw %%mm0, %%mm7           \n\t"	/* accumulate difference... */
    "  add %3, %2                   \n\t"	/* Inc pointer into ref data */
    ".endr                          \n\t"

    "  movd %%mm7, %0               \n\t"

     : "=r" (DiffVal),
       "+r" (ptr1), 
       "+r" (ptr2) 
     : "r" ((unsigned long)stride)
     : "memory"
  );

  return DiffVal;
}

static ogg_uint32_t sad8x8_xy2_thres__mmxext (const unsigned char *SrcData, const unsigned char *RefDataPtr1,
                                              const unsigned char *RefDataPtr2, ogg_uint32_t Stride,
                                              ogg_uint32_t thres)
{
  ogg_uint32_t  DiffVal;

  __asm__ __volatile__ (
    "  .balign 16                   \n\t"
    "  pxor %%mm7, %%mm7            \n\t" 	/* mm7 contains the result */
    ".rept 8                        \n\t"
    "  movq (%1), %%mm0             \n\t"	/* take 8 bytes */
    "  movq (%2), %%mm1             \n\t"
    "  movq (%3), %%mm2             \n\t"
    "  pavgb %%mm2, %%mm1           \n\t"
    "  psadbw %%mm1, %%mm0          \n\t"

    "  add %4, %1                   \n\t"	/* Inc pointer into the new data */
    "  paddw %%mm0, %%mm7           \n\t"	/* accumulate difference... */
    "  add %4, %2                   \n\t"	/* Inc pointer into ref data */
    "  add %4, %3                   \n\t"	/* Inc pointer into ref data */
    ".endr                          \n\t"

    "  movd %%mm7, %0               \n\t"
     : "=m" (DiffVal),
       "+r" (SrcData), 
       "+r" (RefDataPtr1), 
       "+r" (RefDataPtr2) 
     : "r" ((unsigned long)Stride)
     : "memory"
  );

  return DiffVal;
}
		
void dsp_mmxext_init(DspFunctions *funcs)
{
  funcs->sad8x8 = sad8x8__mmxext;
  funcs->sad8x8_thres = sad8x8_thres__mmxext;
  funcs->sad8x8_xy2_thres = sad8x8_xy2_thres__mmxext;
}

#endif /* USE_ASM */
