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

/*MMX acceleration of complete fragment reconstruction algorithm.
  Originally written by Rudolf Marek.*/
#include "x86int.h"
#include "../internal.h"

#if defined(USE_ASM)


static const __attribute__((aligned(8),used)) int OC_FZIG_ZAGMMX[64]={
   0, 8, 1, 2, 9,16,24,17,
  10, 3,32,11,18,25, 4,12,
   5,26,19,40,33,34,41,48,
  27, 6,13,20,28,21,14, 7,
  56,49,42,35,43,50,57,36,
  15,22,29,30,23,44,37,58,
  51,59,38,45,52,31,60,53,
  46,39,47,54,61,62,55,63
};



void oc_state_frag_recon_mmx(oc_theora_state *_state,const oc_fragment *_frag,
 int _pli,ogg_int16_t _dct_coeffs[128],int _last_zzi,int _ncoefs,
 ogg_uint16_t _dc_iquant,const ogg_uint16_t _ac_iquant[64]){
  ogg_int16_t  __attribute__((aligned(8))) res_buf[64];
  int dst_framei;
  int dst_ystride;
  int zzi;
  int ci;
  /*_last_zzi is subtly different from an actual count of the number of
     coefficients we decoded for this block.
    It contains the value of zzi BEFORE the final token in the block was
     decoded.
    In most cases this is an EOB token (the continuation of an EOB run from a
     previous block counts), and so this is the same as the coefficient count.
    However, in the case that the last token was NOT an EOB token, but filled
     the block up with exactly 64 coefficients, _last_zzi will be less than 64.
    Provided the last token was not a pure zero run, the minimum value it can
     be is 46, and so that doesn't affect any of the cases in this routine.
    However, if the last token WAS a pure zero run of length 63, then _last_zzi
     will be 1 while the number of coefficients decoded is 64.
    Thus, we will trigger the following special case, where the real
     coefficient count would not.
    Note also that a zero run of length 64 will give _last_zzi a value of 0,
     but we still process the DC coefficient, which might have a non-zero value
     due to DC prediction.
    Although convoluted, this is arguably the correct behavior: it allows us to
     dequantize fewer coefficients and use a smaller transform when the block
     ends with a long zero run instead of a normal EOB token.
    It could be smarter... multiple separate zero runs at the end of a block
     will fool it, but an encoder that generates these really deserves what it
     gets.
    Needless to say we inherited this approach from VP3.*/
  /*Special case only having a DC component.*/
  if(_last_zzi<2){
    ogg_int16_t p;
    /*Why is the iquant product rounded in this case and no others?
      Who knows.*/
    p=(ogg_int16_t)((ogg_int32_t)_frag->dc*_dc_iquant+15>>5);
    /*for(ci=0;ci<64;ci++)res_buf[ci]=p;*/
    /*This could also be done with MMX 2.*/
    __asm__ __volatile__(
     "  movzwl    %1,   %%eax\n\t"
     "  movd   %%eax,   %%mm0\n\t" /* XXXX XXXX 0000 AAAA */
     "  movq   %%mm0,   %%mm1\n\t" /* XXXX XXXX 0000 AAAA */
     "  pslld    $16,   %%mm1\n\t" /* XXXX XXXX AAAA 0000 */
     "  por    %%mm0,   %%mm1\n\t" /* XXXX XXXX AAAA AAAA */
     "  movq   %%mm1,   %%mm0\n\t" /* XXXX XXXX AAAA AAAA */
     "  psllq    $32,   %%mm1\n\t" /* AAAA AAAA 0000 0000 */
     "  por    %%mm1,   %%mm0\n\t" /* AAAA AAAA AAAA AAAA */
     "  movq   %%mm0,    (%0)\n\t"
     "  movq   %%mm0,   8(%0)\n\t"
     "  movq   %%mm0,  16(%0)\n\t"
     "  movq   %%mm0,  24(%0)\n\t"
     "  movq   %%mm0,  32(%0)\n\t"
     "  movq   %%mm0,  40(%0)\n\t"
     "  movq   %%mm0,  48(%0)\n\t"
     "  movq   %%mm0,  56(%0)\n\t"
     "  movq   %%mm0,  64(%0)\n\t"
     "  movq   %%mm0,  72(%0)\n\t"
     "  movq   %%mm0,  80(%0)\n\t"
     "  movq   %%mm0,  88(%0)\n\t"
     "  movq   %%mm0,  96(%0)\n\t"
     "  movq   %%mm0, 104(%0)\n\t"
     "  movq   %%mm0, 112(%0)\n\t"
     "  movq   %%mm0, 120(%0)\n\t"
     :
     :"r" (res_buf),
      "r" (p)
     :"memory"
    );
  }
  else{
    /*Then, fill in the remainder of the coefficients with 0's, and perform
       the iDCT.*/
    /*First zero the buffer.*/
    /*On K7, etc., this could be replaced with movntq and sfence.*/
    __asm__ __volatile__(
     "  pxor %%mm0,   %%mm0\n\t"
     "  movq %%mm0,    (%0)\n\t"
     "  movq %%mm0,   8(%0)\n\t"
     "  movq %%mm0,  16(%0)\n\t"
     "  movq %%mm0,  24(%0)\n\t"
     "  movq %%mm0,  32(%0)\n\t"
     "  movq %%mm0,  40(%0)\n\t"
     "  movq %%mm0,  48(%0)\n\t"
     "  movq %%mm0,  56(%0)\n\t"
     "  movq %%mm0,  64(%0)\n\t"
     "  movq %%mm0,  72(%0)\n\t"
     "  movq %%mm0,  80(%0)\n\t"
     "  movq %%mm0,  88(%0)\n\t"
     "  movq %%mm0,  96(%0)\n\t"
     "  movq %%mm0, 104(%0)\n\t"
     "  movq %%mm0, 112(%0)\n\t"
     "  movq %%mm0, 120(%0)\n\t"
     :
     :"r" (res_buf)
     :"memory"
    );
    res_buf[0]=(ogg_int16_t)((ogg_int32_t)_frag->dc*_dc_iquant);
    /*This is planned to be rewritten in MMX.*/
    for(zzi=1;zzi<_ncoefs;zzi++){
      int ci;
      ci=OC_FZIG_ZAG[zzi];
      res_buf[OC_FZIG_ZAGMMX[zzi]]=(ogg_int16_t)((ogg_int32_t)_dct_coeffs[zzi]*
       _ac_iquant[ci]);
    }
    if(_last_zzi<10){
      oc_idct8x8_10_mmx(res_buf);
    }
    else{
      oc_idct8x8_mmx(res_buf);
    }
  }
  /*Fill in the target buffer.*/
  dst_framei=_state->ref_frame_idx[OC_FRAME_SELF];
  dst_ystride=_state->ref_frame_bufs[dst_framei][_pli].ystride;
  /*For now ystride values in all ref frames assumed to be equal.*/
  if(_frag->mbmode==OC_MODE_INTRA){
    oc_frag_recon_intra(_state,_frag->buffer[dst_framei],dst_ystride,res_buf);
  }
  else{
    int ref_framei;
    int ref_ystride;
    int mvoffset0;
    int mvoffset1;
    ref_framei=_state->ref_frame_idx[OC_FRAME_FOR_MODE[_frag->mbmode]];
    ref_ystride=_state->ref_frame_bufs[ref_framei][_pli].ystride;
    if(oc_state_get_mv_offsets(_state,&mvoffset0,&mvoffset1,_frag->mv[0],
     _frag->mv[1],ref_ystride,_pli)>1){
      oc_frag_recon_inter2(_state,_frag->buffer[dst_framei],dst_ystride,
       _frag->buffer[ref_framei]+mvoffset0,ref_ystride,
       _frag->buffer[ref_framei]+mvoffset1,ref_ystride,res_buf);
    }
    else{
      oc_frag_recon_inter(_state,_frag->buffer[dst_framei],dst_ystride,
       _frag->buffer[ref_framei]+mvoffset0,ref_ystride,res_buf);
    }
  }
  oc_restore_fpu(_state);
}

/*Copies the fragments specified by the lists of fragment indices from one
   frame to another.
  _fragis:    A pointer to a list of fragment indices.
  _nfragis:   The number of fragment indices to copy.
  _dst_frame: The reference frame to copy to.
  _src_frame: The reference frame to copy from.
  _pli:       The color plane the fragments lie in.*/
void oc_state_frag_copy_mmx(const oc_theora_state *_state,const int *_fragis,
 int _nfragis,int _dst_frame,int _src_frame,int _pli){
  const int *fragi;
  const int *fragi_end;
  int        dst_framei;
  int        dst_ystride;
  int        src_framei;
  int        src_ystride;
  dst_framei=_state->ref_frame_idx[_dst_frame];
  src_framei=_state->ref_frame_idx[_src_frame];
  dst_ystride=_state->ref_frame_bufs[dst_framei][_pli].ystride;
  src_ystride=_state->ref_frame_bufs[src_framei][_pli].ystride;
  fragi_end=_fragis+_nfragis;
  for(fragi=_fragis;fragi<fragi_end;fragi++){
    oc_fragment   *frag;
    unsigned char *dst;
    unsigned char *src;
    frag=_state->frags+*fragi;
    dst=frag->buffer[dst_framei];
    src=frag->buffer[src_framei];
#if (defined(__amd64__) || defined(__x86_64__))
    __asm__ __volatile__(
     "  lea         (%3, %3, 2), %%rsi   \n\t"  /* esi=src_stride*3 */
     "  movq        (%1),        %%mm0   \n\t"  /* src */
     "  lea         (%2, %2, 2), %%rdi   \n\t"  /* edi=dst_stride*3 */
     "  movq        (%1, %3),    %%mm1   \n\t"  /* src+1x stride */
     "  movq        (%1, %3, 2), %%mm2   \n\t"  /* src+2x stride */
     "  movq        (%1, %%rsi), %%mm3   \n\t"  /* src+3x stride */
     "  movq        %%mm0,       (%0)    \n\t"  /* dst */
     "  movq        %%mm1,       (%0, %2)\n\t"  /* dst+dst_stride */
     "  lea         (%1,%3,4),   %1      \n\t"  /* pointer to next 4 */
     "  movq        %%mm2,       (%0, %2, 2)      \n\t"  /*dst+2x dst_stride */
     "  movq        %%mm3,       (%0, %%rdi)      \n\t"  /* 3x */
     "  lea         (%0,%2,4),   %0      \n\t"  /* pointer to next 4 */
     "  movq        (%1),        %%mm0   \n\t"  /* src */
     "  movq        (%1, %3),    %%mm1   \n\t"  /* src+1x stride */
     "  movq        (%1, %3, 2), %%mm2   \n\t"  /* src+2x stride */
     "  movq        (%1, %%rsi), %%mm3   \n\t"  /* src+3x stride */
     "  movq        %%mm0,       (%0)    \n\t"  /* dst */
     "  movq        %%mm1,       (%0, %2)\n\t"  /* dst+dst_stride */
     "  movq        %%mm2,       (%0, %2, 2)     \n\t"  /* dst+2x dst_stride */
     "  movq        %%mm3,       (%0, %%rdi)     \n\t"  /* 3x */
     :"+r" (dst) /* 0 */
     :"r" (src),  /* 1 */
      "r" ((long)dst_ystride), /* 2 */
      "r" ((long)src_ystride) /* 3 */
     :"memory", "rsi","rdi"
    );
  }
#else
    __asm__ __volatile__(
     "  lea         (%3, %3, 2), %%esi   \n\t"  /* esi=src_stride*3 */
     "  movq        (%1),        %%mm0   \n\t"  /* src */
     "  lea         (%2, %2, 2), %%edi   \n\t"  /* edi=dst_stride*3 */
     "  movq        (%1, %3),    %%mm1   \n\t"  /* src+1x stride */
     "  movq        (%1, %3, 2), %%mm2   \n\t"  /* src+2x stride */
     "  movq        (%1, %%esi), %%mm3   \n\t"  /* src+3x stride */
     "  movq        %%mm0,       (%0)    \n\t"  /* dst */
     "  movq        %%mm1,       (%0, %2)\n\t"  /* dst+dst_stride */
     "  lea         (%1,%3,4),   %1      \n\t"  /* pointer to next 4 */
     "  movq        %%mm2,       (%0, %2, 2)      \n\t"  /*dst+2x dst_stride */
     "  movq        %%mm3,       (%0, %%edi)      \n\t"  /* 3x */
     "  lea         (%0,%2,4),   %0      \n\t"  /* pointer to next 4 */
     "  movq        (%1),        %%mm0   \n\t"  /* src */
     "  movq        (%1, %3),    %%mm1   \n\t"  /* src+1x stride */
     "  movq        (%1, %3, 2), %%mm2   \n\t"  /* src+2x stride */
     "  movq        (%1, %%esi), %%mm3   \n\t"  /* src+3x stride */
     "  movq        %%mm0,       (%0)    \n\t"  /* dst */
     "  movq        %%mm1,       (%0, %2)\n\t"  /* dst+dst_stride */
     "  movq        %%mm2,       (%0, %2, 2)     \n\t"  /* dst+2x dst_stride */
     "  movq        %%mm3,       (%0, %%edi)     \n\t"  /* 3x */
     :"+r" (dst) /* 0 */
     :"r" (src),  /* 1 */
      "r" (dst_ystride), /* 2 */
      "r" (src_ystride) /* 3 */
     :"memory", "esi","edi"
    );
  }
#endif
  /*This needs to be removed when decode specific functions are implemented:*/
  __asm__ __volatile__("emms\n\t");
}
#endif
