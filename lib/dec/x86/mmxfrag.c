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

/*MMX acceleration of fragment reconstruction for motion compensation.
  Originally written by Rudolf Marek.*/
#include "x86int.h"

#if defined(USE_ASM)

static const __attribute__((aligned(8),used)) ogg_int64_t OC_V128=
 0x0080008000800080LL;

void oc_frag_recon_intra_mmx(unsigned char *_dst,int _dst_ystride,
 const ogg_int16_t *_residue){
  int i;
  for(i=8;i-->0;){
    __asm__ __volatile__(
      /*Set mm0 to 0x0080008000800080.*/
      "movq %[OC_V128],%%mm0\n\t"
      /*First four input values*/
      "movq (%[residue]),%%mm2\n\t"
      /*Set mm1=mm0.*/
      "movq %%mm0,%%mm1\n\t"
      /*Next four input values.*/
      "movq 8(%[residue]),%%mm3\n\t"
      /*Add 128 and saturate to 16 bits.*/
      "paddsw %%mm3,%%mm1\n\t"
      /*_residue+=16*/
      "lea 0x10(%[residue]),%[residue]\n\t"
      /*Add 128 and saturate to 16 bits.*/
      "paddsw %%mm2,%%mm0\n\t"
      /*Pack saturate with next(high) four values.*/
      "packuswb %%mm1,%%mm0\n\t"
      /*Writeback.*/
      "movq %%mm0,(%[dst])\n\t"
      /*_dst+=_dst_ystride*/
      "lea  (%[dst],%[dst_ystride]),%[dst]\n\t"
      :[dst]"+r"(_dst),[residue]"+r"(_residue)
      :[dst_ystride]"r"((long)_dst_ystride),[OC_V128]"m"(OC_V128)
      :"memory"
    );
  }
}

void oc_frag_recon_inter_mmx(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src,int _src_ystride,const ogg_int16_t *_residue){
  int i;
  /*Zero mm0.*/
  __asm__ __volatile__("pxor %%mm0,%%mm0\n\t"::);
  for(i=8;i-->0;){
    __asm__ __volatile__(
      /*Load mm2 with _src*/
      "movq (%[src]),%%mm2\n\t"
      /*Copy mm2 to mm3.*/
      "movq %%mm2,%%mm3\n\t"
      /*Expand high part of _src to 16 bits.*/
      "punpckhbw %%mm0,%%mm2\n\t"
      /*Expand low part of _src to 16 bits.*/
      "punpcklbw %%mm0,%%mm3\n\t"
      /*Add low part with low part of residue.*/
      "paddsw (%[residue]),%%mm3\n\t"
      /*High with high.*/
      "paddsw 8(%[residue]),%%mm2\n\t"
      /*Pack and saturate to mm3.*/
      "packuswb %%mm2,%%mm3\n\t"
      /*_src+=_src_ystride*/
      "lea (%[src],%[src_ystride]),%[src]\n\t"
      /*_residue+=16*/
      "lea 0x10(%[residue]),%[residue]\n\t"
      /*Put mm3 to dest.*/
      "movq %%mm3,(%[dst])\n\t"
      /*_dst+=_dst_ystride*/
      "lea (%[dst],%[dst_ystride]),%[dst]\n\t"
      :[dst]"+r"(_dst),[src]"+r"(_src),[residue]"+r"(_residue)
      :[dst_ystride]"r"((long)_dst_ystride),
       [src_ystride]"r"((long)_src_ystride)
      :"memory"
    );
  }
}

#if defined(__amd64__)||defined(__x86_64__)

void oc_frag_recon_inter2_mmx(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src1,int _src1_ystride,const unsigned char *_src2,
 int _src2_ystride,const ogg_int16_t *_residue){
  int i;
  __asm__ __volatile__(
    /*Zero mm0.*/
    "pxor %%mm0,%%mm0\n\t"
    /*Load mm2 with _src1.*/
    "movq (%[src1]),%%mm2\n\t"
    :[src1]"+r"(_src1)
    :
  );
  for(i=8;i-->0;){
    __asm__ __volatile__(
     /*Packed _src2.*/ 
     "movq (%[src2]),%%mm4\n\t"
     /*Copy packed src1 to mm3.*/
     "movq %%mm2,%%mm3\n\t"
     /*Copy packed src2 to mm5.*/
     "movq %%mm4,%%mm5\n\t"
     /*Expand low part of src1 to mm2.*/
     "punpcklbw %%mm0,%%mm2\n\t"
     /*Expand Low part of src2 to mm4.*/
     "punpcklbw %%mm0,%%mm4\n\t"
     /*_src1+=_src1_ystride*/
     "lea (%[src1],%[src1_ystride]),%[src1]\n\t"
     /*Expand high part of src1 to mm3.*/
     "punpckhbw %%mm0,%%mm3\n\t"
     /*Expand high part of src2 to mm5.*/
     "punpckhbw %%mm0,%%mm5\n\t"
     /*Add low parts of src1 and src2.*/
     "paddsw %%mm2,%%mm4\n\t"
     /*Add high parts of src1 and src2.*/
     "paddsw %%mm3,%%mm5\n\t"
     /*_src2+=_src2_ystride.*/
     "lea (%[src2],%[src2_ystride]),%[src2]\n\t"
     /*Load mm2 with _src1.*/
     "movq (%[src1]),%%mm2\n\t"
     /*Shift logical 1 to right o 2 dolu.*/
     "psrlw $1,%%mm4\n\t"
     /*Shift logical 1 to right.*/
     "psrlw $1,%%mm5\n\t"
     /*Add low parts wwith low parts.*/
     "paddsw (%[residue]),%%mm4\n\t"
     /*Add highparts with high.*/
     "paddsw 8(%[residue]),%%mm5\n\t"
     /*Pack saturate high to low.*/
     "packuswb %%mm5,%%mm4\n\t"
     /*_residue+=16.*/
     "lea 0x10(%[residue]),%[residue]\n\t"
     /*Write to dst.*/
     "movq %%mm4,(%[dst])\n\t"
     /*_dst+=_dst_ystride*/
     "lea (%[dst],%[dst_ystride]),%[dst]\n\t"
     :[dst]"+r"(_dst),[residue]"+r"(_residue),
      [src1]"+r"(_src1),[src2]"+r"(_src2)
     :[dst_ystride]"r"((long)_dst_ystride),
      [src1_ystride]"r"((long)_src1_ystride),
      [src2_ystride]"r"((long)_src2_ystride)
     :"memory"
    );
  }
}

#else

void oc_frag_recon_inter2_mmx(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src1,int _src1_ystride,const unsigned char *_src2,
 int _src2_ystride,const ogg_int16_t *_residue){
  long a;
  int  i;
  __asm__ __volatile__(
    /*Zero mm0.*/
    "pxor %%mm0,%%mm0\n\t"
    /*Load mm2 with _src1.*/
    "movq (%[src1]),%%mm2\n\t"
    :[src1]"+r"(_src1)
    :
  );
  for(i=8;i-->0;){
    __asm__ __volatile__(
     /*Packed _src2.*/ 
     "movq (%[src2]),%%mm4\n\t"
     /*Copy packed src1 to mm3.*/
     "movq %%mm2,%%mm3\n\t"
     /*Copy packed src2 to mm5.*/
     "movq %%mm4,%%mm5\n\t"
     /*eax=_src1_ystride*/
     "mov %[src1_ystride],%[a]\n\t"
     /*Expand low part of src1 to mm2.*/
     "punpcklbw %%mm0,%%mm2\n\t"
     /*Expand Low part of src2 to mm4.*/
     "punpcklbw %%mm0,%%mm4\n\t"
     /*_src1+=_src1_ystride*/
     "lea (%[src1],%[a]),%[src1]\n\t"
     /*Expand high part of src1 to mm3.*/
     "punpckhbw %%mm0,%%mm3\n\t"
     /*Expand high part of src2 to mm5.*/
     "punpckhbw %%mm0,%%mm5\n\t"
     /*eax=_src2_ystride*/
     "mov %[src2_ystride],%[a]\n\t"
     /*Add low parts of src1 and src2.*/
     "paddsw %%mm2,%%mm4\n\t"
     /*Add high parts of src1 and src2.*/
     "paddsw %%mm3,%%mm5\n\t"
     /*_src2+=_src2_ystride.*/
     "lea (%[src2],%[a]),%[src2]\n\t"
     /*Load mm2 with _src1.*/
     "movq (%[src1]),%%mm2\n\t"
     /*Shift logical 1 to right o 2 dolu.*/
     "psrlw $1,%%mm4\n\t"
     /*Shift logical 1 to right.*/
     "psrlw $1,%%mm5\n\t"
     /*Add low parts wwith low parts.*/
     "paddsw (%[residue]),%%mm4\n\t"
     /*Add highparts with high.*/
     "paddsw 8(%[residue]),%%mm5\n\t"
     /*eax=_dst_ystride.*/
     "mov %[dst_ystride],%[a]\n\t"
     /*Pack saturate high to low.*/
     "packuswb %%mm5,%%mm4\n\t"
     /*_residue+=16.*/
     "lea 0x10(%[residue]),%[residue]\n\t"
     /*Write to dst.*/
     "movq %%mm4,(%[dst])\n\t"
     /*_dst+=_dst_ystride*/
     "lea (%[dst],%[a]),%[dst]\n\t"
     :[a]"=&a"(a),[dst]"+r"(_dst),[residue]"+r"(_residue),
      [src1]"+r"(_src1),[src2]"+r"(_src2)
     :[dst_ystride]"m"((long)_dst_ystride),
      [src1_ystride]"m"((long)_src1_ystride),
      [src2_ystride]"m"((long)_src2_ystride)
     :"memory"
    );
  }
}

#endif

void oc_restore_fpu_mmx(void){
  __asm__ __volatile__("emms\n\t");
}
#endif
