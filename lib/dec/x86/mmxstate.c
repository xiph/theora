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

static const __attribute__((aligned(8),used)) ogg_int64_t V3=
 0x0003000300030003LL; 
static const __attribute__((aligned(8),used)) ogg_int64_t V4=
 0x0004000400040004LL; 
static const __attribute__((aligned(8),used)) ogg_int64_t V100=
 0x0100010001000100LL;
  
#if defined(__APPLE__)
#define MANGLE(x) "_"#x
#else
#define MANGLE(x) #x
#endif

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

static void loop_filter_v_mmx(unsigned char *_pix,int _ystride,int *_bv){
  _pix-=_ystride*2;
  
  __asm__ __volatile__(
    "pxor %%mm0,%%mm0\n"  	/* mm0 = 0 */
    "movq (%0),%%mm7\n"	/* mm7 = _pix[0..8] */
    "lea (%1,%1,2),%%esi\n"	/* esi = _ystride*3 */
    "movq (%0,%%esi),%%mm4\n" /* mm4 = _pix[0..8]+_ystride*3] */
    "movq %%mm7,%%mm6\n"	/* mm6 = _pix[0..8] */
    "punpcklbw %%mm0,%%mm6\n" /* expand unsigned _pix[0..3] to 16 bits */
    "movq %%mm4,%%mm5\n"
    "punpckhbw %%mm0,%%mm7\n" /* expand unsigned _pix[4..8] to 16 bits */
    "punpcklbw %%mm0,%%mm4\n" /* expand other arrays too */
    "punpckhbw %%mm0,%%mm5\n"
    "psubw %%mm4,%%mm6\n" /* mm6 = mm6 - mm4 */
    "psubw %%mm5,%%mm7\n" /* mm7 = mm7 - mm5 */
    			/* mm7:mm6 = _p[0]-_p[_ystride*3] */
    "movq (%0,%1),%%mm4\n"   /* mm4 = _pix[0..8+_ystride] */
    "movq %%mm4,%%mm5\n"
    "movq (%0,%1,2),%%mm2\n" /* mm2 = _pix[0..8]+_ystride*2] */
    "movq %%mm2,%%mm3\n"
    "movq %%mm2,%%mm1\n" //ystride*2
    "punpckhbw %%mm0,%%mm5\n"
    "punpcklbw %%mm0,%%mm4\n" 
    "punpckhbw %%mm0,%%mm3\n"
    "punpcklbw %%mm0,%%mm2\n"
    "psubw %%mm5,%%mm3\n" 
    "psubw %%mm4,%%mm2\n" 
    			/* mm3:mm2 = (_pix[_ystride*2]-_pix[_ystride]); */
    "PMULLW "MANGLE(V3)",%%mm3\n" 		/* *3 */
    "PMULLW "MANGLE(V3)",%%mm2\n" 		/* *3 */
    "paddw %%mm7,%%mm3\n"   /* highpart */
    "paddw %%mm6,%%mm2\n"/* lowpart of _pix[0]-_pix[_ystride*3]+3*(_pix[_ystride*2]-_pix[_ystride]);  */
    "paddw "MANGLE(V4)",%%mm3\n"  /* add 4 */
    "paddw "MANGLE(V4)",%%mm2\n"  /* add 4 */
    "psraw $3,%%mm3\n"  /* >>3 f coefs high */
    "psraw $3,%%mm2\n"  /* >>3 f coefs low */
    "paddw "MANGLE(V100)",%%mm3\n"  /* add 256 */
    "paddw "MANGLE(V100)",%%mm2\n"  /* add 256 */

    " pextrw $0,%%mm2,%%esi\n"  /* In MM4:MM0 we have f coefs (16bits) */
    " pextrw $1,%%mm2,%%edi\n"  /* now perform MM7:MM6 = *(_bv+ f) */
    " pinsrw $0,(%2,%%esi,4),%%mm6\n"
    " pinsrw $1,(%2,%%edi,4),%%mm6\n"

    " pextrw $2,%%mm2,%%esi\n"
    " pextrw $3,%%mm2,%%edi\n"
    " pinsrw $2,(%2,%%esi,4),%%mm6\n"
    " pinsrw $3,(%2,%%edi,4),%%mm6\n"

    " pextrw $0,%%mm3,%%esi\n" 
    " pextrw $1,%%mm3,%%edi\n"
    " pinsrw $0,(%2,%%esi,4),%%mm7\n"
    " pinsrw $1,(%2,%%edi,4),%%mm7\n"

    " pextrw $2,%%mm3,%%esi\n"
    " pextrw $3,%%mm3,%%edi\n"
    " pinsrw $2,(%2,%%esi,4),%%mm7\n"
    " pinsrw $3, (%2,%%edi,4),%%mm7\n"   //MM7 MM6   f=*(_bv+(f+4>>3));

    "paddw %%mm6,%%mm4\n"  /* (_pix[_ystride]+f); */
    "paddw %%mm7,%%mm5\n"  /* (_pix[_ystride]+f); */
    "movq %%mm1,%%mm2\n"
    "punpcklbw %%mm0,%%mm1\n"
    "punpckhbw %%mm0,%%mm2\n" //[ystride*2]
    "psubw %%mm6,%%mm1\n" /* (_pix[_ystride*2]-f); */
    "psubw %%mm7,%%mm2\n" /* (_pix[_ystride*2]-f); */
    "packuswb %%mm2,%%mm1\n"
    "packuswb %%mm5,%%mm4\n"
    "movq %%mm1,(%0,%1,2)\n" /* _pix[_ystride*2]= */
    "movq %%mm4,(%0,%1)\n" /* _pix[_ystride]= */
    "emms\n"
    : 
    : "r" (_pix), "r" (_ystride), "r" (_bv)
    : "esi", "edi" , "memory"
  );

}



#define OC_LOOP_H_4x4 \
__asm__ __volatile__( \
"lea (%1,%1,2),%%esi\n"	 /* esi = _ystride*3 */  \
"movd (%0), %%mm0\n"		/* 0 0 0 0 3 2 1 0 */ \
"movd (%0,%1),%%mm1\n"    	/* 0 0 0 0 7 6 5 4 */ \
"movd (%0,%1,2),%%mm2\n"  	/* 0 0 0 0 b a 9 8 */ \
"movd (%0,%%esi),%%mm3\n" 	/* 0 0 0 0 f e d c */ \
"punpcklbw %%mm1,%%mm0\n" 	/* mm0 = 7 3 6 2 5 1 4 0 */ \
"punpcklbw %%mm3,%%mm2\n" 	/* mm2 = f b e a d 9 c 8 */ \
"movq %%mm0,%%mm1\n"	 	/* mm1 = 7 3 6 2 5 1 4 0 */ \
"punpcklwd %%mm2,%%mm1\n"	/* mm1 = d 9 5 1 c 8 4 0 */ \
"punpckhwd %%mm2,%%mm0\n"	/* mm0 = f b 7 3 e a 6 2 */ \
"pxor %%mm7,%%mm7\n" \
"movq %%mm1,%%mm5\n" 		/* mm5 = d 9 5 1 c 8 4 0 */ \
"punpckhbw %%mm7,%%mm5\n"	/* mm5 = 0 d 0 9 0 5 0 1 = pix[1]*/ \
"punpcklbw %%mm7,%%mm1\n"	/* mm1 = 0 c 0 8 0 4 0 0 = pix[0]*/ \
"movq %%mm0,%%mm3\n" 		/* mm3 = f b 7 3 e a 6 2 */ \
"punpckhbw %%mm7,%%mm3\n"	/* mm3 = 0 f 0 b 0 7 0 3 = pix[3]*/ \
"punpcklbw %%mm7,%%mm0\n"    	/* mm0 = 0 e 0 a 0 6 0 2 = pix[2]*/ \
 \
"psubw %%mm3,%%mm1\n"		/* mm1 = pix[0]-pix[3] mm1 - mm3 */ \
"movq %%mm0,%%mm7\n"		/* mm7 = pix[2]*/ \
"psubw %%mm5,%%mm0\n" 		/* mm0 = pix[2]-pix[1] mm0 - mm5*/ \
"PMULLW "MANGLE(V3)",%%mm0\n" 		/* *3 */ \
"paddw %%mm0,%%mm1\n" 		/* mm1 has f[0] ... f[4]*/ \
"paddw "MANGLE(V4)",%%mm1\n"  /* add 4 */ \
"psraw $3,%%mm1\n"  	/* >>3 */ \
"paddw "MANGLE(V100)",%%mm1\n"  /* add 256 */ \
" pextrw $0,%%mm1,%%esi\n"  /* In MM1 we have 4 f coefs (16bits) */ \
" pextrw $1,%%mm1,%%edi\n"  /* now perform MM4 = *(_bv+ f) */ \
" pinsrw $0,(%2,%%esi,4),%%mm4\n" \
" pextrw $2,%%mm1,%%esi\n" \
" pinsrw $1,(%2,%%edi,4),%%mm4\n" \
" pextrw $3,%%mm1,%%edi\n" \
" pinsrw $2,(%2,%%esi,4),%%mm4\n" \
" pinsrw $3,(%2,%%edi,4),%%mm4\n" /* new f vals loaded */ \
"pxor %%mm0,%%mm0\n" \
" paddw %%mm4,%%mm5\n"	/*(_pix[1]+f);*/ \
" psubw %%mm4,%%mm7\n" /* (_pix[2]-f); */ \
" packuswb %%mm0,%%mm5\n" /* mm5 = x x x x newpix1 */ \
" packuswb %%mm0,%%mm7\n" /* mm7 = x x x x newpix2 */  \
" punpcklbw %%mm7,%%mm5\n" /* 2 1 2 1 2 1 2 1 */ \
" movd %%mm5,%%eax\n" /* eax = newpix21 */ \
" movw %%ax,1(%0)\n" \
" psrlq $32,%%mm5\n" /* why is so big stall here ? */ \
" shrl $16,%%eax\n" \
" lea 1(%0,%1,2),%%edi\n" \
" movw %%ax,1(%0,%1,1)\n" \
" movd %%mm5,%%eax\n"  /* eax = newpix21 high part */ \
" lea (%1,%1,2),%%esi\n" \
" movw %%ax,(%%edi)\n" \
" shrl $16,%%eax\n" \
" movw %%ax,1(%0,%%esi)\n" \
" emms\n" \
: \
: "r" (_pix), "r" (_ystride), "r" (_bv) \
: "esi", "edi" , "memory", "eax" \
); \

/* this code implements loop_filter_h
   data are striped p0 p1 p2 p3 ... p0 p1 p2 p3 ...
   in order to load all (four) p0's to one register we must transpose
   the values in four mmx regs. When halfs is done we repeat for rest.
  
TODO: some instruction stalls can be avoided

*/

static void loop_filter_h_mmx(unsigned char *_pix,int _ystride,int *_bv){
  _pix-=2;
  OC_LOOP_H_4x4
  _pix+=_ystride*4;
  OC_LOOP_H_4x4
}

/*Apply the loop filter to a given set of fragment rows in the given plane.
  The filter may be run on the bottom edge, affecting pixels in the next row of
   fragments, so this row also needs to be available.
  _bv:        The bounding values array.
  _refi:      The index of the frame buffer to filter.
  _pli:       The color plane to filter.
  _fragy0:    The Y coordinate of the first fragment row to filter.
  _fragy_end: The Y coordinate of the fragment row to stop filtering at.*/
  
/*  we copy whole function because mmx routines will be inlined 4 times 
    also _bv pointer should not be added with 256 because then we can use
    non negative index in MMX code and we get rid of sign extension instructions
*/

void oc_state_loop_filter_frag_rows_mmx(oc_theora_state *_state,int *_bv,
 int _refi,int _pli,int _fragy0,int _fragy_end){
  th_img_plane  *iplane;
  oc_fragment_plane *fplane;
  oc_fragment       *frag_top;
  oc_fragment       *frag0;
  oc_fragment       *frag;
  oc_fragment       *frag_end;
  oc_fragment       *frag0_end;
  oc_fragment       *frag_bot;
  iplane=_state->ref_frame_bufs[_refi]+_pli;
  fplane=_state->fplanes+_pli;
  /*The following loops are constructed somewhat non-intuitively on purpose.
    The main idea is: if a block boundary has at least one coded fragment on
     it, the filter is applied to it.
    However, the order that the filters are applied in matters, and VP3 chose
     the somewhat strange ordering used below.*/
  frag_top=_state->frags+fplane->froffset;
  frag0=frag_top+_fragy0*fplane->nhfrags;
  frag0_end=frag0+(_fragy_end-_fragy0)*fplane->nhfrags;
  frag_bot=_state->frags+fplane->froffset+fplane->nfrags;
  while(frag0<frag0_end){
    frag=frag0;
    frag_end=frag+fplane->nhfrags;
    while(frag<frag_end){
      if(frag->coded){
        if(frag>frag0){
          loop_filter_h_mmx(frag->buffer[_refi],iplane->ystride,_bv);
        }
        if(frag0>frag_top){
          loop_filter_v_mmx(frag->buffer[_refi],iplane->ystride,_bv);
        }
        if(frag+1<frag_end&&!(frag+1)->coded){
          loop_filter_h_mmx(frag->buffer[_refi]+8,iplane->ystride,_bv);
        }
        if(frag+fplane->nhfrags<frag_bot&&!(frag+fplane->nhfrags)->coded){
          loop_filter_v_mmx((frag+fplane->nhfrags)->buffer[_refi],
           iplane->ystride,_bv);
        }
      }
      frag++;
    }
    frag0+=fplane->nhfrags;
  }
}

#endif
