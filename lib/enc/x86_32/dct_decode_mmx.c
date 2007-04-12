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

#include <stdlib.h>

#include "codec_internal.h"

#if defined(USE_ASM)

static const __attribute__((aligned(8),used)) ogg_int64_t V3= 0x0003000300030003LL;
static const __attribute__((aligned(8),used)) ogg_int64_t V804= 0x0804080408040804LL;

#if defined(__APPLE__)
#define MANGLE(x) "_"#x
#else
#define MANGLE(x) #x
#endif

static void FilterHoriz__mmx(unsigned char * PixelPtr,
                        ogg_int32_t LineLength,
                        ogg_int16_t *BoundingValuePtr){

#define OC_LOOP_H_4x4                                                   \
    __asm__ __volatile__(                                               \
    "lea (%1,%1,2),%%esi\n"     /* esi = ystride*3 */                   \
    "movd (%0), %%mm0\n"        /* 0 0 0 0 3 2 1 0 */                   \
    "movd (%0,%1),%%mm1\n"      /* 0 0 0 0 7 6 5 4 */                   \
    "movd (%0,%1,2),%%mm2\n"    /* 0 0 0 0 b a 9 8 */                   \
    "movd (%0,%%esi),%%mm3\n"   /* 0 0 0 0 f e d c */                   \
    "punpcklbw %%mm1,%%mm0\n"   /* mm0 = 7 3 6 2 5 1 4 0 */             \
    "punpcklbw %%mm3,%%mm2\n"   /* mm2 = f b e a d 9 c 8 */             \
    "movq %%mm0,%%mm1\n"        /* mm1 = 7 3 6 2 5 1 4 0 */             \
    "punpcklwd %%mm2,%%mm1\n"   /* mm1 = d 9 5 1 c 8 4 0 */             \
    "punpckhwd %%mm2,%%mm0\n"   /* mm0 = f b 7 3 e a 6 2 */             \
    "pxor %%mm7,%%mm7\n"                                                \
    "movq %%mm1,%%mm5\n"        /* mm5 = d 9 5 1 c 8 4 0 */             \
    "punpckhbw %%mm7,%%mm5\n"   /* mm5 = 0 d 0 9 0 5 0 1 = pix[1]*/     \
    "punpcklbw %%mm7,%%mm1\n"   /* mm1 = 0 c 0 8 0 4 0 0 = pix[0]*/     \
    "movq %%mm0,%%mm3\n"        /* mm3 = f b 7 3 e a 6 2 */             \
    "punpckhbw %%mm7,%%mm3\n"   /* mm3 = 0 f 0 b 0 7 0 3 = pix[3]*/     \
    "punpcklbw %%mm7,%%mm0\n"       /* mm0 = 0 e 0 a 0 6 0 2 = pix[2]*/ \
                                                                        \
    "psubw %%mm3,%%mm1\n"       /* mm1 = pix[0]-pix[3] mm1 - mm3 */     \
    "movq %%mm0,%%mm7\n"        /* mm7 = pix[2]*/                       \
    "psubw %%mm5,%%mm0\n"       /* mm0 = pix[2]-pix[1] mm0 - mm5*/      \
    "PMULLW "MANGLE(V3)",%%mm0\n" /* *3 */                              \
    "paddw %%mm0,%%mm1\n"         /* mm1 has f[0] ... f[4]*/            \
    "paddw "MANGLE(V804)",%%mm1\n"/* add 4 */ /* add 256 after shift */ \
    "psraw $3,%%mm1\n"          /* >>3 */                               \
    " pextrw $0,%%mm1,%%esi\n"  /* In MM1 we have 4 f coefs (16bits) */ \
    " pextrw $1,%%mm1,%%edi\n"  /* now perform MM4 = *(_bv+ f) */       \
    " pinsrw $0,(%2,%%esi,2),%%mm4\n"                                   \
    " pextrw $2,%%mm1,%%esi\n"                                          \
    " pinsrw $1,(%2,%%edi,2),%%mm4\n"                                   \
    " pextrw $3,%%mm1,%%edi\n"                                          \
    " pinsrw $2,(%2,%%esi,2),%%mm4\n"                                   \
    " pinsrw $3,(%2,%%edi,2),%%mm4\n" /* new f vals loaded */           \
    "pxor %%mm0,%%mm0\n"                                                \
    " paddw %%mm4,%%mm5\n"      /*(pix[1]+f);*/                         \
    " psubw %%mm4,%%mm7\n"      /* (pix[2]-f); */                       \
    " packuswb %%mm0,%%mm5\n"   /* mm5 = x x x x newpix1 */             \
    " packuswb %%mm0,%%mm7\n"   /* mm7 = x x x x newpix2 */             \
    " punpcklbw %%mm7,%%mm5\n"  /* 2 1 2 1 2 1 2 1 */                   \
    " movd %%mm5,%%eax\n"       /* eax = newpix21 */                    \
    " movw %%ax,1(%0)\n"                                                \
    " psrlq $32,%%mm5\n"        /* why is so big stall here ? */        \
    " shrl $16,%%eax\n"                                                 \
    " lea 1(%0,%1,2),%%edi\n"                                           \
    " movw %%ax,1(%0,%1,1)\n"                                           \
    " movd %%mm5,%%eax\n"       /* eax = newpix21 high part */          \
    " lea (%1,%1,2),%%esi\n"                                            \
    " movw %%ax,(%%edi)\n"                                              \
    " shrl $16,%%eax\n"                                                 \
    " movw %%ax,1(%0,%%esi)\n"                                          \
    :                                                                   \
    : "r" (PixelPtr), "r" (LineLength), "r" (BoundingValuePtr-256)      \
    : "esi", "edi" , "memory", "eax"                                    \
    );

    OC_LOOP_H_4x4
    PixelPtr += LineLength*4;
    OC_LOOP_H_4x4
    __asm__ __volatile__("emms\n");
}

static void FilterVert__mmx(unsigned char * PixelPtr,
                ogg_int32_t LineLength,
                ogg_int16_t *BoundingValuePtr){
    __asm__ __volatile__(
    "pxor %%mm0,%%mm0\n"        /* mm0 = 0 */
    "movq (%0),%%mm7\n"         /* mm7 = pix[0..7] */
    "lea (%1,%1,2),%%esi\n"     /* esi = ystride*3 */
    "movq (%0,%%esi),%%mm4\n"   /* mm4 = pix[0..7+ystride*3] */
    "movq %%mm7,%%mm6\n"        /* mm6 = pix[0..7] */
    "punpcklbw %%mm0,%%mm6\n"   /* expand unsigned pix[0..3] to 16 bits */
    "movq %%mm4,%%mm5\n"
    "punpckhbw %%mm0,%%mm7\n"   /* expand unsigned pix[4..7] to 16 bits */
    "punpcklbw %%mm0,%%mm4\n"   /* expand other arrays too */
    "punpckhbw %%mm0,%%mm5\n"
    "psubw %%mm4,%%mm6\n"       /* mm6 = mm6 - mm4 */
    "psubw %%mm5,%%mm7\n"       /* mm7 = mm7 - mm5 */
                /* mm7:mm6 = _p[0]-_p[ystride*3] */
    "movq (%0,%1),%%mm4\n"      /* mm4 = pix[0..7+ystride] */
    "movq %%mm4,%%mm5\n"
    "movq (%0,%1,2),%%mm2\n"    /* mm2 = pix[0..7+ystride*2] */
    "movq %%mm2,%%mm3\n"
    "movq %%mm2,%%mm1\n"        //ystride*2
    "punpckhbw %%mm0,%%mm5\n"
    "punpcklbw %%mm0,%%mm4\n"
    "punpckhbw %%mm0,%%mm3\n"
    "punpcklbw %%mm0,%%mm2\n"
    "psubw %%mm5,%%mm3\n"
    "psubw %%mm4,%%mm2\n"
                /* mm3:mm2 = (pix[ystride*2]-pix[ystride]); */
    "PMULLW "MANGLE(V3)",%%mm3\n"    /* *3 */
    "PMULLW "MANGLE(V3)",%%mm2\n"    /* *3 */
    "paddw %%mm7,%%mm3\n"            /* highpart */
    "paddw %%mm6,%%mm2\n"            /* lowpart of pix[0]-pix[ystride*3]+3*(pix[ystride*2]-pix[ystride]);  */
    "paddw "MANGLE(V804)",%%mm3\n"   /* add 4 */ /* add 256 after shift */
    "paddw "MANGLE(V804)",%%mm2\n"   /* add 4 */ /* add 256 after shift */
    "psraw $3,%%mm3\n"               /* >>3 f coefs high */
    "psraw $3,%%mm2\n"               /* >>3 f coefs low */

    " pextrw $0,%%mm2,%%esi\n"  /* In MM3:MM2 we have f coefs (16bits) */
    " pextrw $1,%%mm2,%%edi\n"  /* now perform MM7:MM6 = *(_bv+ f) */
    " pinsrw $0,(%2,%%esi,2),%%mm6\n"
    " pinsrw $1,(%2,%%edi,2),%%mm6\n"

    " pextrw $2,%%mm2,%%esi\n"
    " pextrw $3,%%mm2,%%edi\n"
    " pinsrw $2,(%2,%%esi,2),%%mm6\n"
    " pinsrw $3,(%2,%%edi,2),%%mm6\n"

    " pextrw $0,%%mm3,%%esi\n"
    " pextrw $1,%%mm3,%%edi\n"
    " pinsrw $0,(%2,%%esi,2),%%mm7\n"
    " pinsrw $1,(%2,%%edi,2),%%mm7\n"

    " pextrw $2,%%mm3,%%esi\n"
    " pextrw $3,%%mm3,%%edi\n"
    " pinsrw $2,(%2,%%esi,2),%%mm7\n"
    " pinsrw $3,(%2,%%edi,2),%%mm7\n"   //MM7 MM6   f=*(_bv+(f+4>>3));

    "paddw %%mm6,%%mm4\n"       /* (pix[ystride]+f); */
    "paddw %%mm7,%%mm5\n"       /* (pix[ystride]+f); */
    "movq %%mm1,%%mm2\n"
    "punpcklbw %%mm0,%%mm1\n"
    "punpckhbw %%mm0,%%mm2\n"   //[ystride*2]
    "psubw %%mm6,%%mm1\n"       /* (pix[ystride*2]-f); */
    "psubw %%mm7,%%mm2\n"       /* (pix[ystride*2]-f); */
    "packuswb %%mm2,%%mm1\n"
    "packuswb %%mm5,%%mm4\n"
    "movq %%mm1,(%0,%1,2)\n"    /* pix[ystride*2]= */
    "movq %%mm4,(%0,%1)\n"      /* pix[ystride]= */
    "emms\n"
    :
    : "r" (PixelPtr-2*LineLength), "r" (LineLength), "r" (BoundingValuePtr-256)
    : "esi", "edi" , "memory"
    );
}

/* install our implementation in the function table */
void dsp_mmx_dct_decode_init(DspFunctions *funcs)
{
  TH_DEBUG("enabling accelerated x86_32 mmx dct decode functions.\n");
  funcs->FilterVert = FilterVert__mmx;
  funcs->FilterHoriz = FilterHoriz__mmx;
}

#endif /* USE_ASM */
