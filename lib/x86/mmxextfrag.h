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

#if !defined(_x86_mmxextfrag_H)
# define _x86_mmxextfrag_H (1)
# include <stddef.h>
# include "x86int.h"

#if defined(OC_X86_ASM)

/*Copies an 8x8 block of pixels from _src1 and _src2 to _dst, assuming _ystride
   bytes between rows, taking the average of the two sources.*/
#define OC_FRAG_COPY2_MMXEXT(_dst,_src1,_src2,_ystride) \
  do{ \
    const unsigned char *cpysrc1; \
    const unsigned char *cpysrc2; \
    const unsigned char *cpydst; \
    ptrdiff_t            ystride3; \
    cpysrc1=(_src1); \
    cpysrc2=(_src2); \
    cpydst=(_dst); \
    __asm__ __volatile__( \
      "movq (%[src1]),%%mm0\n\t" \
      "movq (%[src2]),%%mm2\n\t" \
      "pcmpeqb %%mm7,%%mm7\n\t" \
      "movq (%[src1],%[ystride]),%%mm1\n\t" \
      "movq (%[src2],%[ystride]),%%mm3\n\t" \
      "lea (%[ystride],%[ystride],2),%[ystride3]\n\t" \
      "pxor %%mm7,%%mm0\n\t" \
      "pxor %%mm7,%%mm2\n\t" \
      "pxor %%mm7,%%mm1\n\t" \
      "pxor %%mm7,%%mm3\n\t" \
      "pavgb %%mm2,%%mm0\n\t" \
      "pavgb %%mm3,%%mm1\n\t" \
      "movq (%[src1],%[ystride],2),%%mm2\n\t" \
      "movq (%[src2],%[ystride],2),%%mm4\n\t" \
      "movq (%[src1],%[ystride3]),%%mm3\n\t" \
      "movq (%[src2],%[ystride3]),%%mm5\n\t" \
      "pxor %%mm7,%%mm0\n\t" \
      "pxor %%mm7,%%mm1\n\t" \
      "lea (%[src1],%[ystride],4),%[src1]\n\t" \
      "lea (%[src2],%[ystride],4),%[src2]\n\t" \
      "pxor %%mm7,%%mm2\n\t" \
      "pxor %%mm7,%%mm4\n\t" \
      "pxor %%mm7,%%mm3\n\t" \
      "pxor %%mm7,%%mm5\n\t" \
      "pavgb %%mm4,%%mm2\n\t" \
      "pavgb %%mm5,%%mm3\n\t" \
      "pxor %%mm7,%%mm2\n\t" \
      "pxor %%mm7,%%mm3\n\t" \
      "movq %%mm0,(%[dst])\n\t" \
      "movq %%mm1,(%[dst],%[ystride])\n\t" \
      "movq %%mm2,(%[dst],%[ystride],2)\n\t" \
      "movq %%mm3,(%[dst],%[ystride3])\n\t" \
      "movq (%[src1]),%%mm0\n\t" \
      "movq (%[src2]),%%mm2\n\t" \
      "movq (%[src1],%[ystride]),%%mm1\n\t" \
      "movq (%[src2],%[ystride]),%%mm3\n\t" \
      "pxor %%mm7,%%mm0\n\t" \
      "pxor %%mm7,%%mm2\n\t" \
      "pxor %%mm7,%%mm1\n\t" \
      "pxor %%mm7,%%mm3\n\t" \
      "pavgb %%mm2,%%mm0\n\t" \
      "pavgb %%mm3,%%mm1\n\t" \
      "movq (%[src1],%[ystride],2),%%mm2\n\t" \
      "movq (%[src2],%[ystride],2),%%mm4\n\t" \
      "movq (%[src1],%[ystride3]),%%mm3\n\t" \
      "movq (%[src2],%[ystride3]),%%mm5\n\t" \
      "pxor %%mm7,%%mm0\n\t" \
      "pxor %%mm7,%%mm1\n\t" \
      "lea (%[dst],%[ystride],4),%[dst]\n\t" \
      "pxor %%mm7,%%mm2\n\t" \
      "pxor %%mm7,%%mm4\n\t" \
      "pxor %%mm7,%%mm3\n\t" \
      "pxor %%mm7,%%mm5\n\t" \
      "pavgb %%mm4,%%mm2\n\t" \
      "pavgb %%mm5,%%mm3\n\t" \
      "movq %%mm0,(%[dst])\n\t" \
      "pxor %%mm7,%%mm2\n\t" \
      "pxor %%mm7,%%mm3\n\t" \
      "movq %%mm1,(%[dst],%[ystride])\n\t" \
      "movq %%mm2,(%[dst],%[ystride],2)\n\t" \
      "movq %%mm3,(%[dst],%[ystride3])\n\t" \
      :[dst]"+r"(cpydst),[src1]"+%r"(cpysrc1),[src2]"+r"(cpysrc2), \
       [ystride3]"=&r"(ystride3) \
      :[ystride]"r"((ptrdiff_t)_ystride) \
      :"memory" \
    ); \
  } \
  while(0)

/*Copies a 16x8 block of pixels from _src1 and _src2 to _dst, assuming _ystride
   bytes between rows, taking the average of the two sources.*/
#define OC_FRAGX2_COPY2_MMXEXT(_dst,_src1,_src2,_ystride) \
  do{ \
    const unsigned char *cpysrc1; \
    const unsigned char *cpysrc2; \
    const unsigned char *cpydst; \
    int                  i; \
    cpysrc1=(_src1); \
    cpysrc2=(_src2); \
    cpydst=(_dst); \
    __asm__ __volatile__("pcmpeqb %%mm7,%%mm7\n\t"::); \
    for(i=0;i<8;i+=2){ \
      __asm__ __volatile__( \
        "movq (%[src1]),%%mm0\n\t" \
        "movq 8(%[src1]),%%mm1\n\t" \
        "movq (%[src2]),%%mm2\n\t" \
        "movq 8(%[src2]),%%mm3\n\t" \
        "movq (%[src1],%[ystride]),%%mm4\n\t" \
        "movq 8(%[src1],%[ystride]),%%mm5\n\t" \
        "pxor %%mm7,%%mm0\n\t" \
        "pxor %%mm7,%%mm1\n\t" \
        "pxor %%mm7,%%mm2\n\t" \
        "pxor %%mm7,%%mm3\n\t" \
        "lea (%[src1],%[ystride],2),%[src1]\n\t" \
        "pavgb %%mm2,%%mm0\n\t" \
        "pavgb %%mm3,%%mm1\n\t" \
        "movq (%[src2],%[ystride]),%%mm2\n\t" \
        "movq 8(%[src2],%[ystride]),%%mm3\n\t" \
        "pxor %%mm7,%%mm0\n\t" \
        "pxor %%mm7,%%mm1\n\t" \
        "pxor %%mm7,%%mm4\n\t" \
        "pxor %%mm7,%%mm5\n\t" \
        "pxor %%mm7,%%mm2\n\t" \
        "pxor %%mm7,%%mm3\n\t" \
        "lea (%[src2],%[ystride],2),%[src2]\n\t" \
        "pavgb %%mm4,%%mm2\n\t" \
        "pavgb %%mm5,%%mm3\n\t" \
        "movq %%mm0,(%[dst])\n\t" \
        "pxor %%mm7,%%mm2\n\t" \
        "pxor %%mm7,%%mm3\n\t" \
        "movq %%mm1,8(%[dst])\n\t" \
        "movq %%mm2,(%[dst],%[ystride])\n\t" \
        "movq %%mm3,8(%[dst],%[ystride])\n\t" \
        "lea (%[dst],%[ystride],2),%[dst]\n\t" \
        :[dst]"+r"(cpydst),[src1]"+%r"(cpysrc1),[src2]"+r"(cpysrc2) \
        :[ystride]"r"((ptrdiff_t)_ystride) \
        :"memory" \
      ); \
    } \
  } \
  while(0)

# endif
#endif
