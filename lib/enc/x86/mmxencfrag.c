/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2009                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id: dsp_mmx.c 14579 2008-03-12 06:42:40Z xiphmont $

 ********************************************************************/
#include <stddef.h>
#include "x86enc.h"

#if defined(OC_X86_ASM)

unsigned oc_enc_frag_sad_mmxext(const unsigned char *_src,
 const unsigned char *_ref,int _ystride){
  ptrdiff_t ystride3;
  ptrdiff_t ret;
  __asm__ __volatile__(
    /*Load the first 4 rows of each block.*/
    "movq (%[src]),%%mm0\n\t"
    "movq (%[ref]),%%mm1\n\t"
    "movq (%[src],%[ystride]),%%mm2\n\t"
    "movq (%[ref],%[ystride]),%%mm3\n\t"
    "lea (%[ystride],%[ystride],2),%[ystride3]\n\t"
    "movq (%[src],%[ystride],2),%%mm4\n\t"
    "movq (%[ref],%[ystride],2),%%mm5\n\t"
    "movq (%[src],%[ystride3]),%%mm6\n\t"
    "movq (%[ref],%[ystride3]),%%mm7\n\t"
    /*Compute their SADs and add them in %%mm0*/
    "psadbw %%mm1,%%mm0\n\t"
    "psadbw %%mm3,%%mm2\n\t"
    "lea (%[src],%[ystride],4),%[src]\n\t"
    "paddw %%mm2,%%mm0\n\t"
    "lea (%[ref],%[ystride],4),%[ref]\n\t"
    /*Load the next 3 rows as registers become available.*/
    "movq (%[src]),%%mm2\n\t"
    "movq (%[ref]),%%mm3\n\t"
    "psadbw %%mm5,%%mm4\n\t"
    "psadbw %%mm7,%%mm6\n\t"
    "paddw %%mm4,%%mm0\n\t"
    "movq (%[ref],%[ystride]),%%mm5\n\t"
    "movq (%[src],%[ystride]),%%mm4\n\t"
    "paddw %%mm6,%%mm0\n\t"
    "movq (%[ref],%[ystride],2),%%mm7\n\t"
    "movq (%[src],%[ystride],2),%%mm6\n\t"
    /*Start adding their SADs to %%mm0*/
    "psadbw %%mm3,%%mm2\n\t"
    "psadbw %%mm5,%%mm4\n\t"
    "paddw %%mm2,%%mm0\n\t"
    "psadbw %%mm7,%%mm6\n\t"
    /*Load last row as registers become available.*/
    "movq (%[src],%[ystride3]),%%mm2\n\t"
    "movq (%[ref],%[ystride3]),%%mm3\n\t"
    /*And finish adding up their SADs.*/
    "paddw %%mm4,%%mm0\n\t"
    "psadbw %%mm3,%%mm2\n\t"
    "paddw %%mm6,%%mm0\n\t"
    "paddw %%mm2,%%mm0\n\t"
    "movd %%mm0,%[ret]\n\t"
    :[ret]"=r"(ret),[ystride3]"=&r"(ystride3)
    :[src]"%r"(_src),[ref]"r"(_ref),[ystride]"r"((ptrdiff_t)_ystride)
  );
  return (unsigned)ret;
}

unsigned oc_enc_frag_sad_thresh_mmxext(const unsigned char *_src,
 const unsigned char *_ref,int _ystride,unsigned _thresh){
  /*Early termination is for suckers.*/
  return oc_enc_frag_sad_mmxext(_src,_ref,_ystride);
}

/*Assumes the first two rows of %[ref1] and %[ref2] are in %%mm0...%%mm3, the
   first two rows of %[src] are in %%mm4,%%mm5, and {1}x8 is in %%mm7.
  We pre-load the next two rows of data as registers become available.*/
#define OC_SAD2_LOOP \
 "#OC_SAD2_LOOP\n\t" \
 /*We want to compute (%%mm0+%%mm1>>1) on unsigned bytes without overflow, but \
    pavgb computes (%%mm0+%%mm1+1>>1). \
   The latter is exactly 1 too large when the low bit of two corresponding \
    bytes is only set in one of them. \
   Therefore we pxor the operands, pand to mask out the low bits, and psubb to \
    correct the output of pavgb.*/ \
 "movq %%mm0,%%mm6\n\t" \
 "lea (%[ref1],%[ystride],2),%[ref1]\n\t" \
 "pxor %%mm1,%%mm0\n\t" \
 "pavgb %%mm1,%%mm6\n\t" \
 "lea (%[ref2],%[ystride],2),%[ref2]\n\t" \
 "movq %%mm2,%%mm1\n\t" \
 "pand %%mm7,%%mm0\n\t" \
 "pavgb %%mm3,%%mm2\n\t" \
 "pxor %%mm3,%%mm1\n\t" \
 "movq (%[ref2],%[ystride]),%%mm3\n\t" \
 "psubb %%mm0,%%mm6\n\t" \
 "movq (%[ref1]),%%mm0\n\t" \
 "pand %%mm7,%%mm1\n\t" \
 "psadbw %%mm6,%%mm4\n\t" \
 "movd %[ret],%%mm6\n\t" \
 "psubb %%mm1,%%mm2\n\t" \
 "movq (%[ref2]),%%mm1\n\t" \
 "lea (%[src],%[ystride],2),%[src]\n\t" \
 "psadbw %%mm2,%%mm5\n\t" \
 "movq (%[ref1],%[ystride]),%%mm2\n\t" \
 "paddw %%mm4,%%mm5\n\t" \
 "movq (%[src]),%%mm4\n\t" \
 "paddw %%mm5,%%mm6\n\t" \
 "movq (%[src],%[ystride]),%%mm5\n\t" \
 "movd %%mm6,%[ret]\n\t" \

/*Same as above, but does not pre-load the next two rows.*/
#define OC_SAD2_TAIL \
 "#OC_SAD2_TAIL\n\t" \
 "movq %%mm0,%%mm6\n\t" \
 "pavgb %%mm1,%%mm0\n\t" \
 "pxor %%mm1,%%mm6\n\t" \
 "movq %%mm2,%%mm1\n\t" \
 "pand %%mm7,%%mm6\n\t" \
 "pavgb %%mm3,%%mm2\n\t" \
 "pxor %%mm3,%%mm1\n\t" \
 "psubb %%mm6,%%mm0\n\t" \
 "pand %%mm7,%%mm1\n\t" \
 "psadbw %%mm0,%%mm4\n\t" \
 "psubb %%mm1,%%mm2\n\t" \
 "movd %[ret],%%mm6\n\t" \
 "psadbw %%mm2,%%mm5\n\t" \
 "paddw %%mm4,%%mm5\n\t" \
 "paddw %%mm5,%%mm6\n\t" \
 "movd %%mm6,%[ret]\n\t" \

unsigned oc_enc_frag_sad2_thresh_mmxext(const unsigned char *_src,
 const unsigned char *_ref1,const unsigned char *_ref2,int _ystride,
 unsigned _thresh){
  ptrdiff_t ret;
  __asm__ __volatile__(
    "movq (%[ref1]),%%mm0\n\t"
    "movq (%[ref2]),%%mm1\n\t"
    "movq (%[ref1],%[ystride]),%%mm2\n\t"
    "movq (%[ref2],%[ystride]),%%mm3\n\t"
    "xor %[ret],%[ret]\n\t"
    "movq (%[src]),%%mm4\n\t"
    "pxor %%mm7,%%mm7\n\t"
    "pcmpeqb %%mm6,%%mm6\n\t"
    "movq (%[src],%[ystride]),%%mm5\n\t"
    "psubb %%mm6,%%mm7\n\t"
    OC_SAD2_LOOP
    OC_SAD2_LOOP
    OC_SAD2_LOOP
    OC_SAD2_TAIL
    :[ret]"=&r"(ret)
    :[src]"r"(_src),[ref1]"%r"(_ref1),[ref2]"r"(_ref2),
     [ystride]"r"((ptrdiff_t)_ystride)
  );
  return (unsigned)ret;
}

void oc_enc_frag_sub_mmx(ogg_int16_t _residue[64],
 const unsigned char *_src,const unsigned char *_ref,int _ystride){
  int i;
  __asm__ __volatile__("pxor %%mm7,%%mm7\n\t"::);
  for(i=4;i-->0;){
    __asm__ __volatile__(
      /*mm0=[src]*/
      "movq (%[src]),%%mm0\n\t"
      /*mm1=[ref]*/
      "movq (%[ref]),%%mm1\n\t"
      /*mm4=[src+ystride]*/
      "movq (%[src],%[ystride]),%%mm4\n\t"
      /*mm5=[ref+ystride]*/
      "movq (%[ref],%[ystride]),%%mm5\n\t"
      /*Compute [src]-[ref].*/
      "movq %%mm0,%%mm2\n\t"
      "punpcklbw %%mm7,%%mm0\n\t"
      "movq %%mm1,%%mm3\n\t"
      "punpckhbw %%mm7,%%mm2\n\t"
      "punpcklbw %%mm7,%%mm1\n\t"
      "punpckhbw %%mm7,%%mm3\n\t"
      "psubw %%mm1,%%mm0\n\t"
      "psubw %%mm3,%%mm2\n\t"
      /*Compute [src+ystride]-[ref+ystride].*/
      "movq %%mm4,%%mm1\n\t"
      "punpcklbw %%mm7,%%mm4\n\t"
      "movq %%mm5,%%mm3\n\t"
      "punpckhbw %%mm7,%%mm1\n\t"
      "lea (%[src],%[ystride],2),%[src]\n\t"
      "punpcklbw %%mm7,%%mm5\n\t"
      "lea (%[ref],%[ystride],2),%[ref]\n\t"
      "punpckhbw %%mm7,%%mm3\n\t"
      "psubw %%mm5,%%mm4\n\t"
      "psubw %%mm3,%%mm1\n\t"
      /*Write the answer out.*/
      "movq %%mm0,0x00(%[residue])\n\t"
      "movq %%mm2,0x08(%[residue])\n\t"
      "movq %%mm4,0x10(%[residue])\n\t"
      "movq %%mm1,0x18(%[residue])\n\t"
      "lea 0x20(%[residue]),%[residue]\n\t"
      :
      :[residue]"r"(_residue),[src]"r"(_src),[ref]"r"(_ref),
       [ystride]"r"((ptrdiff_t)_ystride)
      :"memory"
    );
  }
}

void oc_enc_frag_sub_128_mmx(ogg_int16_t _residue[64],
 const unsigned char *_src,int _ystride){
  ptrdiff_t ystride3;
  __asm__ __volatile__(
    /*mm0=[src]*/
    "movq (%[src]),%%mm0\n\t"
    /*mm1=[src+ystride]*/
    "movq (%[src],%[ystride]),%%mm1\n\t"
    /*mm6={-1}x4*/
    "pcmpeqw %%mm6,%%mm6\n\t"
    /*mm2=[src+2*ystride]*/
    "movq (%[src],%[ystride],2),%%mm2\n\t"
    /*[ystride3]=3*[ystride]*/
    "lea (%[ystride],%[ystride],2),%[ystride3]\n\t"
    /*mm6={1}x4*/
    "psllw $15,%%mm6\n\t"
    /*mm3=[src+3*ystride]*/
    "movq (%[src],%[ystride3]),%%mm3\n\t"
    /*mm6={128}x4*/
    "psrlw $8,%%mm6\n\t"
    /*mm7=0*/
    "pxor %%mm7,%%mm7\n\t"
    /*[src]=[src]+4*[ystride]*/
    "lea (%[src],%[ystride],4),%[src]\n\t"
    /*Compute [src]-128 and [src+ystride]-128*/
    "movq %%mm0,%%mm4\n\t"
    "punpcklbw %%mm7,%%mm0\n\t"
    "movq %%mm1,%%mm5\n\t"
    "punpckhbw %%mm7,%%mm4\n\t"
    "psubw %%mm6,%%mm0\n\t"
    "punpcklbw %%mm7,%%mm1\n\t"
    "psubw %%mm6,%%mm4\n\t"
    "punpckhbw %%mm7,%%mm5\n\t"
    "psubw %%mm6,%%mm1\n\t"
    "psubw %%mm6,%%mm5\n\t"
    /*Write the answer out.*/
    "movq %%mm0,0x00(%[residue])\n\t"
    "movq %%mm4,0x08(%[residue])\n\t"
    "movq %%mm1,0x10(%[residue])\n\t"
    "movq %%mm5,0x18(%[residue])\n\t"
    /*mm0=[src+4*ystride]*/
    "movq (%[src]),%%mm0\n\t"
    /*mm1=[src+5*ystride]*/
    "movq (%[src],%[ystride]),%%mm1\n\t"
    /*Compute [src+2*ystride]-128 and [src+3*ystride]-128*/
    "movq %%mm2,%%mm4\n\t"
    "punpcklbw %%mm7,%%mm2\n\t"
    "movq %%mm3,%%mm5\n\t"
    "punpckhbw %%mm7,%%mm4\n\t"
    "psubw %%mm6,%%mm2\n\t"
    "punpcklbw %%mm7,%%mm3\n\t"
    "psubw %%mm6,%%mm4\n\t"
    "punpckhbw %%mm7,%%mm5\n\t"
    "psubw %%mm6,%%mm3\n\t"
    "psubw %%mm6,%%mm5\n\t"
    /*Write the answer out.*/
    "movq %%mm2,0x20(%[residue])\n\t"
    "movq %%mm4,0x28(%[residue])\n\t"
    "movq %%mm3,0x30(%[residue])\n\t"
    "movq %%mm5,0x38(%[residue])\n\t"
    /*mm2=[src+6*ystride]*/
    "movq (%[src],%[ystride],2),%%mm2\n\t"
    /*mm3=[src+7*ystride]*/
    "movq (%[src],%[ystride3]),%%mm3\n\t"
    /*Compute [src+4*ystride]-128 and [src+5*ystride]-128*/
    "movq %%mm0,%%mm4\n\t"
    "punpcklbw %%mm7,%%mm0\n\t"
    "movq %%mm1,%%mm5\n\t"
    "punpckhbw %%mm7,%%mm4\n\t"
    "psubw %%mm6,%%mm0\n\t"
    "punpcklbw %%mm7,%%mm1\n\t"
    "psubw %%mm6,%%mm4\n\t"
    "punpckhbw %%mm7,%%mm5\n\t"
    "psubw %%mm6,%%mm1\n\t"
    "psubw %%mm6,%%mm5\n\t"
    /*Write the answer out.*/
    "movq %%mm0,0x40(%[residue])\n\t"
    "movq %%mm4,0x48(%[residue])\n\t"
    "movq %%mm1,0x50(%[residue])\n\t"
    "movq %%mm5,0x58(%[residue])\n\t"
    /*Compute [src+6*ystride]-128 and [src+7*ystride]-128*/
    "movq %%mm2,%%mm4\n\t"
    "punpcklbw %%mm7,%%mm2\n\t"
    "movq %%mm3,%%mm5\n\t"
    "punpckhbw %%mm7,%%mm4\n\t"
    "psubw %%mm6,%%mm2\n\t"
    "punpcklbw %%mm7,%%mm3\n\t"
    "psubw %%mm6,%%mm4\n\t"
    "punpckhbw %%mm7,%%mm5\n\t"
    "psubw %%mm6,%%mm3\n\t"
    "psubw %%mm6,%%mm5\n\t"
    /*Write the answer out.*/
    "movq %%mm2,0x60(%[residue])\n\t"
    "movq %%mm4,0x68(%[residue])\n\t"
    "movq %%mm3,0x70(%[residue])\n\t"
    "movq %%mm5,0x78(%[residue])\n\t"
    :[ystride3]"=&r"(ystride3)
    :[residue]"r"(_residue),[src]"r"(_src),[ystride]"r"((ptrdiff_t)_ystride)
    :"memory"
  );
}

void oc_enc_frag_copy2_mmxext(unsigned char *_dst,
 const unsigned char *_src1,const unsigned char *_src2,int _ystride){
  ptrdiff_t ystride3;
  __asm__ __volatile__(
    /*Load the first 3 rows.*/
    "movq (%[src1]),%%mm0\n\t"
    "movq (%[src2]),%%mm1\n\t"
    "movq (%[src1],%[ystride]),%%mm2\n\t"
    "movq (%[src2],%[ystride]),%%mm3\n\t"
    "pxor %%mm7,%%mm7\n\t"
    "movq (%[src1],%[ystride],2),%%mm4\n\t"
    "pcmpeqb %%mm6,%%mm6\n\t"
    "movq (%[src2],%[ystride],2),%%mm5\n\t"
    /*mm7={1}x8.*/
    "psubb %%mm6,%%mm7\n\t"
    /*ystride3=ystride*3.*/
    "lea (%[ystride],%[ystride],2),%[ystride3]\n\t"
    /*Start averaging %%mm0 and %%mm1 into %%mm6.*/
    "movq %%mm0,%%mm6\n\t"
    "pxor %%mm1,%%mm0\n\t"
    "pavgb %%mm1,%%mm6\n\t"
    /*%%mm1 is free, start averaging %%mm3 into %%mm2 using %%mm1.*/
    "movq %%mm2,%%mm1\n\t"
    "pand %%mm7,%%mm0\n\t"
    "pavgb %%mm3,%%mm2\n\t"
    "pxor %%mm3,%%mm1\n\t"
    /*%%mm3 is free.*/
    "psubb %%mm0,%%mm6\n\t"
    /*%%mm0 is free, start loading the next row.*/
    "movq (%[src1],%[ystride3]),%%mm0\n\t"
    /*Start averaging %%mm5 and %%mm4 using %%mm3.*/
    "movq %%mm4,%%mm3\n\t"
    /*%%mm6 (row 0) is done; write it out.*/
    "movq %%mm6,(%[dst])\n\t"
    "pand %%mm7,%%mm1\n\t"
    "pavgb %%mm5,%%mm4\n\t"
    "psubb %%mm1,%%mm2\n\t"
    /*%%mm1 is free, continue loading the next row.*/
    "movq (%[src2],%[ystride3]),%%mm1\n\t"
    "pxor %%mm5,%%mm3\n\t"
    /*Advance %[src1]*/
    "lea (%[src1],%[ystride],4),%[src1]\n\t"
    /*%%mm2 (row 1) is done; write it out.*/
    "movq %%mm2,(%[dst],%[ystride])\n\t"
    "pand %%mm7,%%mm3\n\t"
    /*Start loading the next row.*/
    "movq (%[src1]),%%mm2\n\t"
    "psubb %%mm3,%%mm4\n\t"
    /*Advance %[src2]*/
    "lea (%[src2],%[ystride],4),%[src2]\n\t"
    /*%%mm4 (row 2) is done; write it out.*/
    "movq %%mm4,(%[dst],%[ystride],2)\n\t"
    /*Continue loading the next row.*/
    "movq (%[src2]),%%mm3\n\t"
    /*Start averaging %%mm0 and %%mm1 into %%mm6.*/
    "movq %%mm0,%%mm6\n\t"
    "pxor %%mm1,%%mm0\n\t"
    /*Start loading the next row.*/
    "movq (%[src1],%[ystride]),%%mm4\n\t"
    "pavgb %%mm1,%%mm6\n\t"
    /*%%mm1 is free; start averaging %%mm3 into %%mm2 using %%mm1.*/
    "movq %%mm2,%%mm1\n\t"
    "pand %%mm7,%%mm0\n\t"
    /*Continue loading the next row.*/
    "movq (%[src2],%[ystride]),%%mm5\n\t"
    "pavgb %%mm3,%%mm2\n\t"
    "pxor %%mm3,%%mm1\n\t"
    /*%%mm3 is free.*/
    "psubb %%mm0,%%mm6\n\t"
    /*%%mm0 is free, start loading the next row.*/
    "movq (%[src1],%[ystride],2),%%mm0\n\t"
    /*Start averaging %%mm5 into %%mm4 using %%mm3.*/
    "movq %%mm4,%%mm3\n\t"
    /*%%mm6 (row 3) is done; write it out.*/
    "movq %%mm6,(%[dst],%[ystride3])\n\t"
    "pand %%mm7,%%mm1\n\t"
    "pavgb %%mm5,%%mm4\n\t"
    /*Advance %[dst]*/
    "lea (%[dst],%[ystride],4),%[dst]\n\t"
    "psubb %%mm1,%%mm2\n\t"
    /*%%mm1 is free; continue loading the next row.*/
    "movq (%[src2],%[ystride],2),%%mm1\n\t"
    "pxor %%mm5,%%mm3\n\t"
    /*%%mm2 (row 4) is done; write it out.*/
    "movq %%mm2,(%[dst])\n\t"
    "pand %%mm7,%%mm3\n\t"
    /*Start loading the next row.*/
    "movq (%[src1],%[ystride3]),%%mm2\n\t"
    "psubb %%mm3,%%mm4\n\t"
    /*Start averaging %%mm0 and %%mm1 into %%mm6.*/
    "movq %%mm0,%%mm6\n\t"
    /*Continue loading the next row.*/
    "movq (%[src2],%[ystride3]),%%mm3\n\t"
    /*%%mm4 (row 5) is done; write it out.*/
    "movq %%mm4,(%[dst],%[ystride])\n\t"
    "pxor %%mm1,%%mm0\n\t"
    "pavgb %%mm1,%%mm6\n\t"
    /*%%mm4 is free; start averaging %%mm3 into %%mm2 using %%mm4.*/
    "movq %%mm2,%%mm4\n\t"
    "pand %%mm7,%%mm0\n\t"
    "pavgb %%mm3,%%mm2\n\t"
    "pxor %%mm3,%%mm4\n\t"
    "psubb %%mm0,%%mm6\n\t"
    "pand %%mm7,%%mm4\n\t"
    /*%%mm6 (row 6) is done, write it out.*/
    "movq %%mm6,(%[dst],%[ystride],2)\n\t"
    "psubb %%mm4,%%mm2\n\t"
    /*%%mm2 (row 7) is done, write it out.*/
    "movq %%mm2,(%[dst],%[ystride3])\n\t"
    :[ystride3]"=&r"(ystride3)
    :[dst]"r"(_dst),[src1]"%r"(_src1),[src2]"r"(_src2),
     [ystride]"r"((ptrdiff_t)_ystride)
    :"memory"
  );
}

#endif
