/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2003                *
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

static const __attribute__((aligned(8),used)) ogg_int64_t V128=
 0x0080008000800080LL;

void oc_frag_recon_intra_mmx(unsigned char *_dst,int _dst_ystride,
 const ogg_int16_t *_residue){
  __asm__ __volatile__(
   "  mov          $0x7, %%ecx  \n\t" /* 8x loop */
   "  .p2align 4                \n\t"
   "1:movq           %3, %%mm0  \n\t" /* Set mm0 to 0x0080008000800080 */
   "  movq         (%1), %%mm2  \n\t" /* First four input values */
   "  movq        %%mm0, %%mm1  \n\t" /* Set mm1 == mm0 */
   "  movq        8(%1), %%mm3  \n\t" /* Next four input values */
   "  decl      %%ecx           \n\t" /* dec counter */
   "  paddsw      %%mm3, %%mm1  \n\t" /* add+128 and saturate to 16bit */
   "  lea      0x10(%1), %1     \n\t" /*_residuo+16 */
   "  paddsw      %%mm2, %%mm0  \n\t" /* add+128 and saturate to 16bit   */
   "  packuswb    %%mm1, %%mm0  \n\t" /* pack saturate with next(high) four values */
   "  movq      %%mm0, (%0)     \n\t" /* writeback */
   "  lea         (%0,%2), %0   \n\t" /*_dst+_dst_ystride */
   "  jns 1b                    \n\t" /* loop */
   :"+r" (_dst)
   :"r" (_residue),
    "r" ((long)_dst_ystride),
    "m" (V128)
   :"memory", "ecx", "cc"
  );
}

void oc_frag_recon_inter_mmx(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src,int _src_ystride,const ogg_int16_t *_residue){
  int i;
  __asm__ __volatile__(
   "  movl         $0x7,   %%eax   \n\t" /* 8x loop */
   "  pxor         %%mm0,  %%mm0   \n\t" /* zero mm0  */
   "  .p2align 4                   \n\t"
   "1: movq        (%4),   %%mm2   \n\t" /* load mm2 with _src */
   "  movq         %%mm2,  %%mm3   \n\t" /* copy mm2 to mm3 */
   "  punpckhbw    %%mm0,  %%mm2   \n\t" /* expand high part of _src to 16 bits */
   "  punpcklbw    %%mm0,  %%mm3   \n\t" /* expand low part of _src to 16 bits */
   "  paddsw       (%1),   %%mm3   \n\t" /* add low part with low part of residue */
   "  paddsw       8(%1),  %%mm2   \n\t" /* high with high */
   "  packuswb     %%mm2,  %%mm3   \n\t" /* pack and saturate to mm3 */
   "  lea         (%4,%3), %4      \n\t" /* _src+_src_ystride */
   "  lea         0x10(%1), %1     \n\t" /* _residuo+16 */
   "  movq        %%mm3,   (%0)    \n\t" /* put mm3 to dest */
   "  lea         (%0,%2),%0       \n\t" /* _dst+_dst_ystride */
   "  decl        %%eax            \n\t" /* dec counter */
   "  jns         1b               \n\t" /* loop */
   :"+r" (_dst)
   :"r" (_residue), 
    "r" ((long)_dst_ystride),
    "r" ((long)_src_ystride),
    "r" (_src)
   :"memory", "eax", "cc"
  );
}

#if (defined(__amd64__) ||  defined(__x86_64__))

void oc_frag_recon_inter2_mmx(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src1,int _src1_ystride,const unsigned char *_src2,
 int _src2_ystride,const ogg_int16_t *_residue){

  __asm__ __volatile__(
   "  movl         $0x7,   %%eax   \n\t" /* 8x loop */
   "  pxor         %%mm0,  %%mm0   \n\t" /* zero mm0 */
   "  movq         (%4),   %%mm2   \n\t" /* load mm2 with _src1 */
   "  .p2align 4                   \n\t"
   "1:movq         (%6),   %%mm4   \n\t" /* packed SRC2 */ 
   "  movq         %%mm2,  %%mm3   \n\t" /* copy to mm3 */
   "  movq         %%mm4,  %%mm5   \n\t" /* copy packed src2 to mm5 */
   "  punpcklbw    %%mm0,  %%mm2   \n\t" /* expand low part of src1 to mm2 */
   "  punpcklbw    %%mm0,  %%mm4   \n\t" /* low part expand of src2 to mm4 */
   "  lea          (%4,%3), %4     \n\t" /*  _src1+_src1_ystride */
   "  punpckhbw    %%mm0,  %%mm3   \n\t" /* expand high part of src1 to mm3 */
   "  punpckhbw    %%mm0,  %%mm5   \n\t" /* high part expand of src2 to mm5 */
   "  paddsw       %%mm2,  %%mm4   \n\t" /* add low parts of src1 and src2 */
   "  paddsw       %%mm3,  %%mm5   \n\t" /* add high parts of src1 and src2 */
   "  lea          (%6,%5), %6     \n\t" /* _src2+_src2_ystride */  
   "  movq         (%4), %%mm2     \n\t" /* load mm2 with _src1 */
   "  psrlw        $1,     %%mm4   \n\t" /* shift logical 1 to right o 2 dolu */
   "  psrlw        $1,     %%mm5   \n\t" /* shift logical 1 to right */
   "  paddsw       (%1),   %%mm4   \n\t" /* add low parts wwith low parts */
   "  paddsw       8(%1),  %%mm5   \n\t" /* add highparts with high */
   "  packuswb     %%mm5,  %%mm4   \n\t" /* pack saturate high to low */
   "  lea          0x10(%1), %1    \n\t" /* _residuo+16 */
   "  movq         %%mm4, (%0)     \n\t" /* write to src */
   "  decl         %%eax           \n\t"
   "  lea          (%0,%2), %0     \n\t" /* _dst+_dst_ystride */
   "  jns          1b\n\t"
   :"+r" (_dst) /* 0 */
   :"r" (_residue), /* 1 */
    "r" ((long)_dst_ystride), /* 2 */
    "r" ((long)_src1_ystride), /* 3 */
    "r" (_src1), /* 4 */
    "r" ((long)_src2_ystride), /* 5 */
    "r" (_src2) /* 6 */
   : "memory", "cc", "eax"
  );
}
#else

void oc_frag_recon_inter2_mmx(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src1,int _src1_ystride,const unsigned char *_src2,
 int _src2_ystride,const ogg_int16_t *_residue){
  int i;
  __asm__ __volatile__(
   "  movl         $0x7,   %7      \n\t" /* 8x loop */
   "  pxor         %%mm0,  %%mm0   \n\t" /* zero mm0 */
   "  movq         (%4),   %%mm2   \n\t" /* load mm2 with _src1 */
   "  .p2align 4                   \n\t"
   "1: movq        (%6),   %%mm4   \n\t" /* packed SRC2 */ 
   "  movq         %%mm2,  %%mm3   \n\t" /* copy to mm3 */
   "  movq         %%mm4,  %%mm5   \n\t" /* copy packed src2 to mm5 */
   "  mov          %3,     %%eax   \n\t"
   "  punpcklbw    %%mm0,  %%mm2   \n\t" /* expand low part of src1 to mm2 */
   "  punpcklbw    %%mm0,  %%mm4   \n\t" /* low part expand of src2 to mm4 */
   "  lea          (%4,%%eax), %4  \n\t" /*  _src1+_src1_ystride */
   "  punpckhbw    %%mm0,  %%mm3   \n\t" /* expand high part of src1 to mm3 */
   "  punpckhbw    %%mm0,  %%mm5   \n\t" /* high part expand of src2 to mm5 */
   "  mov          %5,     %%eax   \n\t"
   "  paddsw       %%mm2,  %%mm4   \n\t" /* add low parts of src1 and src2 */
   "  paddsw       %%mm3,  %%mm5   \n\t" /* add high parts of src1 and src2 */
   "  lea          (%6,%%eax), %6  \n\t" /* _src2+_src2_ystride */  
   "  movq         (%4), %%mm2     \n\t" /* load mm2 with _src1 */
   "  psrlw        $1,     %%mm4   \n\t" /* shift logical 1 to right o 2 dolu */
   "  psrlw        $1,     %%mm5   \n\t" /* shift logical 1 to right */
   "  paddsw       (%1),   %%mm4   \n\t" /* add low parts wwith low parts */
   "  paddsw       8(%1),  %%mm5   \n\t" /* add highparts with high */
   "  packuswb     %%mm5,  %%mm4   \n\t" /* pack saturate high to low */
   "  lea          0x10(%1), %1    \n\t" /* _residuo+16 */
   "  movq         %%mm4, (%0)     \n\t" /* write to src */
   "  decl         %7              \n\t"
   "  lea          (%0,%2), %0     \n\t" /* _dst+_dst_ystride */
   "  jns          1b\n\t"
   :"+r" (_dst) /* 0 */
   :"r" (_residue), /* 1 */
    "r" (_dst_ystride), /* 2 */
    "m" (_src1_ystride), /* 3 */
    "r" (_src1), /* 4 */
    "m" (_src2_ystride), /* 5 */
    "r" (_src2), /* 6 */
    "m" (i)
   :"memory", "eax", "cc"
  );
}

#endif

void oc_restore_fpu_mmx(void){
  __asm__ __volatile__(
   "  emms    \n\t" /* pack with next(high) four values */
  );
}
#endif
