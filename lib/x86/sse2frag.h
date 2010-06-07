/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2009                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: mmxfrag.c 16503 2009-08-22 18:14:02Z giles $

 ********************************************************************/

#if !defined(_x86_sse2frag_H)
# define _x86_sse2frag_H (1)
# include <stddef.h>
# include "x86int.h"

#if defined(OC_X86_ASM)

/*Copies a 16x8 block of pixels from _src1 and _src2 to _dst, assuming _ystride
   bytes between rows, taking the average of the two sources.*/
# define OC_FRAGX2_COPY2_SSE2(_dst,_src1,_src2,_ystride) \
  do{ \
    const unsigned char *cpysrc1; \
    const unsigned char *cpysrc2; \
    const unsigned char *cpydst; \
    ptrdiff_t            ystride3; \
    cpysrc1=(_src1); \
    cpysrc2=(_src2); \
    cpydst=(_dst); \
    __asm__ __volatile__( \
      "movdqu (%[src1]),%%xmm0\n\t" \
      "movdqu (%[src2]),%%xmm2\n\t" \
      "pcmpeqb %%xmm7,%%xmm7\n\t" \
      "movdqu (%[src1],%[ystride]),%%xmm1\n\t" \
      "movdqu (%[src2],%[ystride]),%%xmm3\n\t" \
      "lea (%[ystride],%[ystride],2),%[ystride3]\n\t" \
      "pxor %%xmm7,%%xmm0\n\t" \
      "pxor %%xmm7,%%xmm2\n\t" \
      "pxor %%xmm7,%%xmm1\n\t" \
      "pxor %%xmm7,%%xmm3\n\t" \
      "pavgb %%xmm2,%%xmm0\n\t" \
      "pavgb %%xmm3,%%xmm1\n\t" \
      "movdqu (%[src1],%[ystride],2),%%xmm2\n\t" \
      "movdqu (%[src2],%[ystride],2),%%xmm4\n\t" \
      "movdqu (%[src1],%[ystride3]),%%xmm3\n\t" \
      "movdqu (%[src2],%[ystride3]),%%xmm5\n\t" \
      "pxor %%xmm7,%%xmm0\n\t" \
      "pxor %%xmm7,%%xmm1\n\t" \
      "lea (%[src1],%[ystride],4),%[src1]\n\t" \
      "lea (%[src2],%[ystride],4),%[src2]\n\t" \
      "pxor %%xmm7,%%xmm2\n\t" \
      "pxor %%xmm7,%%xmm4\n\t" \
      "pxor %%xmm7,%%xmm3\n\t" \
      "pxor %%xmm7,%%xmm5\n\t" \
      "pavgb %%xmm4,%%xmm2\n\t" \
      "pavgb %%xmm5,%%xmm3\n\t" \
      "pxor %%xmm7,%%xmm2\n\t" \
      "pxor %%xmm7,%%xmm3\n\t" \
      "movdqa %%xmm0,(%[dst])\n\t" \
      "movdqa %%xmm1,(%[dst],%[ystride])\n\t" \
      "movdqa %%xmm2,(%[dst],%[ystride],2)\n\t" \
      "movdqa %%xmm3,(%[dst],%[ystride3])\n\t" \
      "movdqu (%[src1]),%%xmm0\n\t" \
      "movdqu (%[src2]),%%xmm2\n\t" \
      "movdqu (%[src1],%[ystride]),%%xmm1\n\t" \
      "movdqu (%[src2],%[ystride]),%%xmm3\n\t" \
      "pxor %%xmm7,%%xmm0\n\t" \
      "pxor %%xmm7,%%xmm2\n\t" \
      "pxor %%xmm7,%%xmm1\n\t" \
      "pxor %%xmm7,%%xmm3\n\t" \
      "pavgb %%xmm2,%%xmm0\n\t" \
      "pavgb %%xmm3,%%xmm1\n\t" \
      "movdqu (%[src1],%[ystride],2),%%xmm2\n\t" \
      "movdqu (%[src2],%[ystride],2),%%xmm4\n\t" \
      "movdqu (%[src1],%[ystride3]),%%xmm3\n\t" \
      "movdqu (%[src2],%[ystride3]),%%xmm5\n\t" \
      "pxor %%xmm7,%%xmm0\n\t" \
      "pxor %%xmm7,%%xmm1\n\t" \
      "lea (%[dst],%[ystride],4),%[dst]\n\t" \
      "pxor %%xmm7,%%xmm2\n\t" \
      "pxor %%xmm7,%%xmm4\n\t" \
      "pxor %%xmm7,%%xmm3\n\t" \
      "pxor %%xmm7,%%xmm5\n\t" \
      "pavgb %%xmm4,%%xmm2\n\t" \
      "pavgb %%xmm5,%%xmm3\n\t" \
      "movdqa %%xmm0,(%[dst])\n\t" \
      "pxor %%xmm7,%%xmm2\n\t" \
      "pxor %%xmm7,%%xmm3\n\t" \
      "movdqa %%xmm1,(%[dst],%[ystride])\n\t" \
      "movdqa %%xmm2,(%[dst],%[ystride],2)\n\t" \
      "movdqa %%xmm3,(%[dst],%[ystride3])\n\t" \
      :[dst]"+r"(cpydst),[src1]"+%r"(cpysrc1),[src2]"+r"(cpysrc2), \
       [ystride3]"=&r"(ystride3) \
      :[ystride]"r"((ptrdiff_t)_ystride) \
      :"memory" \
    ); \
  } \
  while(0)

# endif
#endif
