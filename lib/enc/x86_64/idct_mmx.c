/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2008                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id$

 ********************************************************************/

#include "codec_internal.h"

#if defined(USE_ASM)

#define MaskOffset 0        // 4 masks come in order low word to high
#define CosineOffset 32     // 7 cosines come in order pi/16 * (1 ... 7)
#define EightOffset 88
#define IdctAdjustBeforeShift 8

ogg_uint16_t idctconstants[(4+7+1) * 4] = {
    65535,     0,     0,     0,     0, 65535,     0,     0,
        0,     0, 65535,     0,     0,     0,     0, 65535,
    64277, 64277, 64277, 64277, 60547, 60547, 60547, 60547,
    54491, 54491, 54491, 54491, 46341, 46341, 46341, 46341,
    36410, 36410, 36410, 36410, 25080, 25080, 25080, 25080,
    12785, 12785, 12785, 12785,     8,     8,     8,     8,
};

/**************************************************************************************
 *
 *      Routine:        BeginIDCT
 *
 *      Description:    The Macro does IDct on 4 1-D Dcts
 *
 *      Input:          None
 *
 *      Output:         None
 *
 *      Return:         None
 *
 *      Special Note:   None
 *
 *      Error:          None
 *
 ***************************************************************************************
 */

#define MtoSTR(s) #s

#define BeginIDCT "#BeginIDCT\n"    \
                                    \
    "   movq    "I(3)",%%mm2\n"   \
                                    \
    "   movq    "C(3)",%%mm6\n"   \
    "   movq    %%mm2,%%mm4\n"     \
    "   movq    "J(5)",%%mm7\n"   \
    "   pmulhw  %%mm6,%%mm4\n"     \
    "   movq    "C(5)",%%mm1\n"   \
    "   pmulhw  %%mm7,%%mm6\n"     \
    "   movq    %%mm1,%%mm5\n"     \
    "   pmulhw  %%mm2,%%mm1\n"     \
    "   movq    "I(1)",%%mm3\n"   \
    "   pmulhw  %%mm7,%%mm5\n"     \
    "   movq    "C(1)",%%mm0\n"   \
    "   paddw   %%mm2,%%mm4\n"     \
    "   paddw   %%mm7,%%mm6\n"     \
    "   paddw   %%mm1,%%mm2\n"     \
    "   movq    "J(7)",%%mm1\n"   \
    "   paddw   %%mm5,%%mm7\n"     \
    "   movq    %%mm0,%%mm5\n"     \
    "   pmulhw  %%mm3,%%mm0\n"     \
    "   paddsw  %%mm7,%%mm4\n"     \
    "   pmulhw  %%mm1,%%mm5\n"     \
    "   movq    "C(7)",%%mm7\n"   \
    "   psubsw  %%mm2,%%mm6\n"     \
    "   paddw   %%mm3,%%mm0\n"     \
    "   pmulhw  %%mm7,%%mm3\n"     \
    "   movq    "I(2)",%%mm2\n"   \
    "   pmulhw  %%mm1,%%mm7\n"     \
    "   paddw   %%mm1,%%mm5\n"     \
    "   movq    %%mm2,%%mm1\n"     \
    "   pmulhw  "C(2)",%%mm2\n"   \
    "   psubsw  %%mm5,%%mm3\n"     \
    "   movq    "J(6)",%%mm5\n"   \
    "   paddsw  %%mm7,%%mm0\n"     \
    "   movq    %%mm5,%%mm7\n"     \
    "   psubsw  %%mm4,%%mm0\n"     \
    "   pmulhw  "C(2)",%%mm5\n"   \
    "   paddw   %%mm1,%%mm2\n"     \
    "   pmulhw  "C(6)",%%mm1\n"   \
    "   paddsw  %%mm4,%%mm4\n"     \
    "   paddsw  %%mm0,%%mm4\n"     \
    "   psubsw  %%mm6,%%mm3\n"     \
    "   paddw   %%mm7,%%mm5\n"     \
    "   paddsw  %%mm6,%%mm6\n"     \
    "   pmulhw  "C(6)",%%mm7\n"   \
    "   paddsw  %%mm3,%%mm6\n"     \
    "   movq    %%mm4,"I(1)"\n"   \
    "   psubsw  %%mm5,%%mm1\n"     \
    "   movq    "C(4)",%%mm4\n"   \
    "   movq    %%mm3,%%mm5\n"     \
    "   pmulhw  %%mm4,%%mm3\n"     \
    "   paddsw  %%mm2,%%mm7\n"     \
    "   movq    %%mm6,"I(2)"\n"   \
    "   movq    %%mm0,%%mm2\n"     \
    "   movq    "I(0)",%%mm6\n"   \
    "   pmulhw  %%mm4,%%mm0\n"     \
    "   paddw   %%mm3,%%mm5\n"     \
    "\n"                            \
    "   movq    "J(4)",%%mm3\n"   \
    "   psubsw  %%mm1,%%mm5\n"     \
    "   paddw   %%mm0,%%mm2\n"     \
    "   psubsw  %%mm3,%%mm6\n"     \
    "   movq    %%mm6,%%mm0\n"     \
    "   pmulhw  %%mm4,%%mm6\n"     \
    "   paddsw  %%mm3,%%mm3\n"     \
    "   paddsw  %%mm1,%%mm1\n"     \
    "   paddsw  %%mm0,%%mm3\n"     \
    "   paddsw  %%mm5,%%mm1\n"     \
    "   pmulhw  %%mm3,%%mm4\n"     \
    "   paddsw  %%mm0,%%mm6\n"     \
    "   psubsw  %%mm2,%%mm6\n"     \
    "   paddsw  %%mm2,%%mm2\n"     \
    "   movq    "I(1)",%%mm0\n"   \
    "   paddsw  %%mm6,%%mm2\n"     \
    "   paddw   %%mm3,%%mm4\n"     \
    "   psubsw  %%mm1,%%mm2\n"     \
    "#end BeginIDCT\n"
// end BeginIDCT macro (38 cycles).


// Two versions of the end of the idct depending on whether we're feeding
// into a transpose or dividing the final results by 16 and storing them.

/**************************************************************************************
 *
 *      Routine:        RowIDCT
 *
 *      Description:    The Macro does 1-D IDct on 4 Rows
 *
 *      Input:          None
 *
 *      Output:         None
 *
 *      Return:         None
 *
 *      Special Note:   None
 *
 *      Error:          None
 *
 ***************************************************************************************
 */

// RowIDCT gets ready to transpose.

#define RowIDCT "#RowIDCT\n"                             \
    BeginIDCT                                           \
    "\n"                                                \
    "   movq    "I(2)",%%mm3\n"  /* r3 = D. */           \
    "   psubsw  %%mm7,%%mm4\n"    /* r4 = E. = E - G */   \
    "   paddsw  %%mm1,%%mm1\n"    /* r1 = H. + H. */      \
    "   paddsw  %%mm7,%%mm7\n"    /* r7 = G + G */        \
    "   paddsw  %%mm2,%%mm1\n"    /* r1 = R1 = A.. + H. */\
    "   paddsw  %%mm4,%%mm7\n"    /* r7 = G. = E + G */   \
    "   psubsw  %%mm3,%%mm4\n"    /* r4 = R4 = E. - D. */ \
    "   paddsw  %%mm3,%%mm3\n"                            \
    "   psubsw  %%mm5,%%mm6\n"    /* r6 = R6 = F. - B.. */\
    "   paddsw  %%mm5,%%mm5\n"                            \
    "   paddsw  %%mm4,%%mm3\n"    /* r3 = R3 = E. + D. */ \
    "   paddsw  %%mm6,%%mm5\n"    /* r5 = R5 = F. + B.. */\
    "   psubsw  %%mm0,%%mm7\n"    /* r7 = R7 = G. - C. */ \
    "   paddsw  %%mm0,%%mm0\n"                            \
    "   movq    %%mm1,"I(1)"\n"  /* save R1 */           \
    "   paddsw  %%mm7,%%mm0\n"    /* r0 = R0 = G. + C. */ \
    "#end RowIDCT"									

// end RowIDCT macro (8 + 38 = 46 cycles)


/**************************************************************************************
 *
 *      Routine:        ColumnIDCT
 *
 *      Description:    The Macro does 1-D IDct on 4 columns
 *
 *      Input:          None
 *
 *      Output:         None
 *
 *      Return:         None
 *
 *      Special Note:   None
 *
 *      Error:          None
 *
 ***************************************************************************************
 */
// Column IDCT normalizes and stores final results.

#define ColumnIDCT "#ColumnIDCT\n"                          \
    BeginIDCT                                               \
    "\n"                                                    \
    "   paddsw  "Eight",%%mm2\n"                             \
    "   paddsw  %%mm1,%%mm1\n"        /* r1 = H. + H. */      \
    "   paddsw  %%mm2,%%mm1\n"        /* r1 = R1 = A.. + H. */\
    "   psraw   ""$4"",%%mm2\n"      /* r2 = NR2 */          \
    "   psubsw  %%mm7,%%mm4\n"        /* r4 = E. = E - G */   \
    "   psraw   ""$4"",%%mm1\n"      /* r1 = NR1 */          \
    "   movq    "I(2)",%%mm3\n"  /* r3 = D. */               \
    "   paddsw  %%mm7,%%mm7\n"        /* r7 = G + G */        \
    "   movq    %%mm2,"I(2)"\n"  /* store NR2 at I2 */       \
    "   paddsw  %%mm4,%%mm7\n"        /* r7 = G. = E + G */   \
    "   movq    %%mm1,"I(1)"\n"  /* store NR1 at I1 */       \
    "   psubsw  %%mm3,%%mm4\n"        /* r4 = R4 = E. - D. */ \
    "   paddsw  "Eight",%%mm4\n"                             \
    "   paddsw  %%mm3,%%mm3\n"        /* r3 = D. + D. */      \
    "   paddsw  %%mm4,%%mm3\n"        /* r3 = R3 = E. + D. */ \
    "   psraw   ""$4"",%%mm4\n"      /* r4 = NR4 */          \
    "   psubsw  %%mm5,%%mm6\n"        /* r6 = R6 = F. - B.. */\
    "   psraw   ""$4"",%%mm3\n"      /* r3 = NR3 */          \
    "   paddsw  "Eight",%%mm6\n"                             \
    "   paddsw  %%mm5,%%mm5\n"        /* r5 = B.. + B.. */    \
    "   paddsw  %%mm6,%%mm5\n"        /* r5 = R5 = F. + B.. */\
    "   psraw   ""$4"",%%mm6\n"      /* r6 = NR6 */          \
    "   movq    %%mm4,"J(4)"\n"  /* store NR4 at J4 */       \
    "   psraw   ""$4"",%%mm5\n"      /* r5 = NR5 */          \
    "   movq    %%mm3,"I(3)"\n"  /* store NR3 at I3 */       \
    "   psubsw  %%mm0,%%mm7\n"        /* r7 = R7 = G. - C. */ \
    "   paddsw  "Eight",%%mm7\n"                             \
    "   paddsw  %%mm0,%%mm0\n"        /* r0 = C. + C. */      \
    "   paddsw  %%mm7,%%mm0\n"        /* r0 = R0 = G. + C. */ \
    "   psraw   ""$4"",%%mm7\n"      /* r7 = NR7 */          \
    "   movq    %%mm6,"J(6)"\n"  /* store NR6 at J6 */       \
    "   psraw   ""$4"",%%mm0\n"      /* r0 = NR0 */          \
    "   movq    %%mm5,"J(5)"\n"  /* store NR5 at J5 */       \
    "   movq    %%mm7,"J(7)"\n"  /* store NR7 at J7 */       \
    "   movq    %%mm0,"I(0)"\n"  /* store NR0 at I0 */       \
    "#end ColumnIDCT\n"					   

// end ColumnIDCT macro (38 + 19 = 57 cycles)

/**************************************************************************************
 *
 *      Routine:        Transpose
 *
 *      Description:    The Macro does two 4x4 transposes in place.
 *
 *      Input:          None
 *
 *      Output:         None
 *
 *      Return:         None
 *
 *      Special Note:   None
 *
 *      Error:          None
 *
 ***************************************************************************************
 */

/* Following macro does two 4x4 transposes in place.

  At entry (we assume):

    r0 = a3 a2 a1 a0
    I(1) = b3 b2 b1 b0
    r2 = c3 c2 c1 c0
    r3 = d3 d2 d1 d0

    r4 = e3 e2 e1 e0
    r5 = f3 f2 f1 f0
    r6 = g3 g2 g1 g0
    r7 = h3 h2 h1 h0

   At exit, we have:

    I(0) = d0 c0 b0 a0
    I(1) = d1 c1 b1 a1
    I(2) = d2 c2 b2 a2
    I(3) = d3 c3 b3 a3

    J(4) = h0 g0 f0 e0
    J(5) = h1 g1 f1 e1
    J(6) = h2 g2 f2 e2
    J(7) = h3 g3 f3 e3

   I(0) I(1) I(2) I(3)  is the transpose of r0 I(1) r2 r3.
   J(4) J(5) J(6) J(7)  is the transpose of r4 r5 r6 r7.

   Since r1 is free at entry, we calculate the Js first. */


#define Transpose "#Transpose\n"           \
    "   movq        %%mm4,%%mm1\n"            \
    "   punpcklwd   %%mm5,%%mm4\n"            \
    "   movq        %%mm0,"I(0)"\n"          \
    "   punpckhwd   %%mm5,%%mm1\n"            \
    "   movq        %%mm6,%%mm0\n"            \
    "   punpcklwd   %%mm7,%%mm6\n"            \
    "   movq        %%mm4,%%mm5\n"            \
    "   punpckldq   %%mm6,%%mm4\n"            \
    "   punpckhdq   %%mm6,%%mm5\n"            \
    "   movq        %%mm1,%%mm6\n"            \
    "   movq        %%mm4,"J(4)"\n"          \
    "   punpckhwd   %%mm7,%%mm0\n"            \
    "   movq        %%mm5,"J(5)"\n"          \
    "   punpckhdq   %%mm0,%%mm6\n"            \
    "   movq        "I(0)",%%mm4\n"          \
    "   punpckldq   %%mm0,%%mm1\n"            \
    "   movq        "I(1)",%%mm5\n"          \
    "   movq        %%mm4,%%mm0\n"            \
    "   movq        %%mm6,"J(7)"\n"          \
    "   punpcklwd   %%mm5,%%mm0\n"            \
    "   movq        %%mm1,"J(6)"\n"          \
    "   punpckhwd   %%mm5,%%mm4\n"            \
    "   movq        %%mm2,%%mm5\n"            \
    "   punpcklwd   %%mm3,%%mm2\n"            \
    "   movq        %%mm0,%%mm1\n"            \
    "   punpckldq   %%mm2,%%mm0\n"            \
    "   punpckhdq   %%mm2,%%mm1\n"            \
    "   movq        %%mm4,%%mm2\n"            \
    "   movq        %%mm0,"I(0)"\n"          \
    "   punpckhwd   %%mm3,%%mm5\n"            \
    "   movq        %%mm1,"I(1)"\n"          \
    "   punpckhdq   %%mm5,%%mm4\n"            \
    "   punpckldq   %%mm5,%%mm2\n"            \
                                            \
    "   movq        %%mm4,"I(3)"\n"          \
                                            \
    "   movq        %%mm2,"I(2)"\n"          \
    "#end Transpose\n"			    
// end Transpose macro (19 cycles).

/**************************************************************************************
 *
 *      Routine:        MMX_idct
 *
 *      Description:    Perform IDCT on a 8x8 block
 *
 *      Input:          Pointer to input and output buffer
 *
 *      Output:         None
 *
 *      Return:         None
 *
 *      Special Note:   The input coefficients are in ZigZag order
 *
 *      Error:          None
 *
 ***************************************************************************************
 */
void IDctSlow__mmx(const ogg_int16_t *in,
		   const ogg_int16_t *q,
		   ogg_int16_t *out ) {

#   define MID(M,I)     MtoSTR(M+(I)*8)"(%[c])"
#   define M(I)         MID( MaskOffset , I )
#   define C(I)         MID( CosineOffset , I-1 )
#   define Eight        MID(EightOffset,0)

    /* eax = quantized input */
    /* esi = quantization table */
    /* edx = destination (= idct buffer) */
    /* ecx = idctconstants */


    __asm__ __volatile__ (
    "# dequantize, de-zigzag\n"			  
    "movq   (%[i]), %%mm0\n"
    "pmullw (%[q]), %%mm0\n"     /* r0 = 03 02 01 00 */
    "movq   16(%[i]), %%mm1\n"
    "pmullw 16(%[q]), %%mm1\n"   /* r1 = 13 12 11 10 */
    "movq   "M(0)", %%mm2\n"     /* r2 = __ __ __ FF */
    "movq   %%mm0, %%mm3\n"       /* r3 = 03 02 01 00 */
    "movq   8(%[i]), %%mm4\n"
    "psrlq  $16, %%mm0\n"        /* r0 = __ 03 02 01 */
    "pmullw 8(%[q]), %%mm4\n"    /* r4 = 07 06 05 04 */
    "pand   %%mm2, %%mm3\n"       /* r3 = __ __ __ 00 */
    "movq   %%mm0, %%mm5\n"       /* r5 = __ 03 02 01 */
    "movq   %%mm1, %%mm6\n"       /* r6 = 13 12 11 10 */
    "pand   %%mm2, %%mm5\n"       /* r5 = __ __ __ 01 */
    "psllq  $32, %%mm6\n"        /* r6 = 11 10 __ __ */
    "movq   "M(3)", %%mm7\n"     /* r7 = FF __ __ __ */
    "pxor   %%mm5, %%mm0\n"       /* r0 = __ 03 02 __ */
    "pand   %%mm6, %%mm7\n"       /* r7 = 11 __ __ __ */
    "por    %%mm3, %%mm0\n"       /* r0 = __ 03 02 00 */
    "pxor   %%mm7, %%mm6\n"       /* r6 = __ 10 __ __ */
    "por    %%mm7, %%mm0\n"       /* r0 = 11 03 02 00 = R0 */
    "movq   "M(3)", %%mm7\n"     /* r7 = FF __ __ __ */
    "movq   %%mm4, %%mm3\n"       /* r3 = 07 06 05 04 */
    "movq   %%mm0, (%[o])\n"     /* write R0 = r0 */
    "pand   %%mm2, %%mm3\n"       /* r3 = __ __ __ 04 */
    "movq   32(%[i]), %%mm0\n"
    "psllq  $16, %%mm3\n"        /* r3 = __ __ 04 __ */
    "pmullw 32(%[q]), %%mm0\n"   /* r0 = 23 22 21 20 */
    "pand   %%mm1, %%mm7\n"       /* r7 = 13 __ __ __ */
    "por    %%mm3, %%mm5\n"       /* r5 = __ __ 04 01 */
    "por    %%mm6, %%mm7\n"       /* r7 = 13 10 __ __ */
    "movq   24(%[i]), %%mm3\n"
    "por    %%mm5, %%mm7\n"       /* r7 = 13 10 04 01 = R1 */
    "pmullw 24(%[q]), %%mm3\n"   /* r3 = 17 16 15 14 */
    "psrlq  $16, %%mm4\n"        /* r4 = __ 07 06 05 */
    "movq   %%mm7, 16(%[o])\n"   /* write R1 = r7 */
    "movq   %%mm4, %%mm5\n"       /* r5 = __ 07 06 05 */
    "movq   %%mm0, %%mm7\n"       /* r7 = 23 22 21 20 */
    "psrlq  $16, %%mm4\n"        /* r4 = __ __ 07 06 */
    "psrlq  $48, %%mm7\n"        /* r7 = __ __ __ 23 */
    "movq   %%mm2, %%mm6\n"       /* r6 = __ __ __ FF */
    "pand   %%mm2, %%mm5\n"       /* r5 = __ __ __ 05 */
    "pand   %%mm4, %%mm6\n"       /* r6 = __ __ __ 06 */
    "movq   %%mm7, 80(%[o])\n"   /* partial R9 = __ __ __ 23 */
    "pxor   %%mm6, %%mm4\n"       /* r4 = __ __ 07 __ */
    "psrlq  $32, %%mm1\n"        /* r1 = __ __ 13 12 */
    "por    %%mm5, %%mm4\n"       /* r4 = __ __ 07 05 */
    "movq   "M(3)", %%mm7\n"     /* r7 = FF __ __ __ */
    "pand   %%mm2, %%mm1\n"       /* r1 = __ __ __ 12 */
    "movq   48(%[i]), %%mm5\n"
    "psllq  $16, %%mm0\n"        /* r0 = 22 21 20 __ */
    "pmullw 48(%[q]), %%mm5\n"   /* r5 = 33 32 31 30 */
    "pand   %%mm0, %%mm7\n"       /* r7 = 22 __ __ __ */
    "movq   %%mm1, 64(%[o])\n"   /* partial R8 = __ __ __ 12 */
    "por    %%mm4, %%mm7\n"       /* r7 = 22 __ 07 05 */
    "movq   %%mm3, %%mm4\n"       /* r4 = 17 16 15 14 */
    "pand   %%mm2, %%mm3\n"       /* r3 = __ __ __ 14 */
    "movq   "M(2)", %%mm1\n"     /* r1 = __ FF __ __ */
    "psllq  $32, %%mm3\n"        /* r3 = __ 14 __ __ */
    "por    %%mm3, %%mm7\n"       /* r7 = 22 14 07 05 = R2 */
    "movq   %%mm5, %%mm3\n"       /* r3 = 33 32 31 30 */
    "psllq  $48, %%mm3\n"        /* r3 = 30 __ __ __ */
    "pand   %%mm0, %%mm1\n"       /* r1 = __ 21 __ __ */
    "movq   %%mm7, 32(%[o])\n"   /* write R2 = r7 */
    "por    %%mm3, %%mm6\n"       /* r6 = 30 __ __ 06 */
    "movq   "M(1)", %%mm7\n"     /* r7 = __ __ FF __ */
    "por    %%mm1, %%mm6\n"       /* r6 = 30 21 __ 06 */
    "movq   56(%[i]), %%mm1\n"
    "pand   %%mm4, %%mm7\n"       /* r7 = __ __ 15 __ */
    "pmullw 56(%[q]), %%mm1\n"   /* r1 = 37 36 35 34 */
    "por    %%mm6, %%mm7\n"       /* r7 = 30 21 15 06 = R3 */
    "pand   "M(1)", %%mm0\n"     /* r0 = __ __ 20 __ */
    "psrlq  $32, %%mm4\n"        /* r4 = __ __ 17 16 */
    "movq   %%mm7, 48(%[o])\n"   /* write R3 = r7 */
    "movq   %%mm4, %%mm6\n"       /* r6 = __ __ 17 16 */
    "movq   "M(3)", %%mm7\n"     /* r7 = FF __ __ __ */
    "pand   %%mm2, %%mm4\n"       /* r4 = __ __ __ 16 */
    "movq   "M(1)", %%mm3\n"     /* r3 = __ __ FF __ */
    "pand   %%mm1, %%mm7\n"       /* r7 = 37 __ __ __ */
    "pand   %%mm5, %%mm3\n"       /* r3 = __ __ 31 __ */
    "por    %%mm4, %%mm0\n"       /* r0 = __ __ 20 16 */
    "psllq  $16, %%mm3\n"        /* r3 = __ 31 __ __ */
    "por    %%mm0, %%mm7\n"       /* r7 = 37 __ 20 16 */
    "movq   "M(2)", %%mm4\n"     /* r4 = __ FF __ __ */
    "por    %%mm3, %%mm7\n"       /* r7 = 37 31 20 16 = R4 */
    "movq   80(%[i]), %%mm0\n"
    "movq   %%mm4, %%mm3\n"       /* r3 = __ __ FF __ */
    "pmullw 80(%[q]), %%mm0\n"   /* r0 = 53 52 51 50 */
    "pand   %%mm5, %%mm4\n"       /* r4 = __ 32 __ __ */
    "movq   %%mm7, 8(%[o])\n"    /* write R4 = r7 */
    "por    %%mm4, %%mm6\n"       /* r6 = __ 32 17 16 */
    "movq   %%mm3, %%mm4\n"       /* r4 = __ FF __ __ */
    "psrlq  $16, %%mm6\n"        /* r6 = __ __ 32 17 */
    "movq   %%mm0, %%mm7\n"       /* r7 = 53 52 51 50 */
    "pand   %%mm1, %%mm4\n"       /* r4 = __ 36 __ __ */
    "psllq  $48, %%mm7\n"        /* r7 = 50 __ __ __ */
    "por    %%mm4, %%mm6\n"       /* r6 = __ 36 32 17 */
    "movq   88(%[i]), %%mm4\n"
    "por    %%mm6, %%mm7\n"       /* r7 = 50 36 32 17 = R5 */
    "pmullw 88(%[q]), %%mm4\n"   /* r4 = 57 56 55 54 */
    "psrlq  $16, %%mm3\n"        /* r3 = __ __ FF __ */
    "movq   %%mm7, 24(%[o])\n"   /* write R5 = r7 */
    "pand   %%mm1, %%mm3\n"       /* r3 = __ __ 35 __ */
    "psrlq  $48, %%mm5\n"        /* r5 = __ __ __ 33 */
    "pand   %%mm2, %%mm1\n"       /* r1 = __ __ __ 34 */
    "movq   104(%[i]), %%mm6\n"
    "por    %%mm3, %%mm5\n"       /* r5 = __ __ 35 33 */
    "pmullw 104(%[q]), %%mm6\n"  /* r6 = 67 66 65 64 */
    "psrlq  $16, %%mm0\n"        /* r0 = __ 53 52 51 */
    "movq   %%mm4, %%mm7\n"       /* r7 = 57 56 55 54 */
    "movq   %%mm2, %%mm3\n"       /* r3 = __ __ __ FF */
    "psllq  $48, %%mm7\n"        /* r7 = 54 __ __ __ */
    "pand   %%mm0, %%mm3\n"       /* r3 = __ __ __ 51 */
    "pxor   %%mm3, %%mm0\n"       /* r0 = __ 53 52 __ */
    "psllq  $32, %%mm3\n"        /* r3 = __ 51 __ __ */
    "por    %%mm5, %%mm7\n"       /* r7 = 54 __ 35 33 */
    "movq   %%mm6, %%mm5\n"       /* r5 = 67 66 65 64 */
    "pand   "M(1)", %%mm6\n"     /* r6 = __ __ 65 __ */
    "por    %%mm3, %%mm7\n"       /* r7 = 54 51 35 33 = R6 */
    "psllq  $32, %%mm6\n"        /* r6 = 65 __ __ __ */
    "por    %%mm1, %%mm0\n"       /* r0 = __ 53 52 34 */
    "movq   %%mm7, 40(%[o])\n"   /* write R6 = r7 */
    "por    %%mm6, %%mm0\n"       /* r0 = 65 53 52 34 = R7 */
    "movq   120(%[i]), %%mm7\n"
    "movq   %%mm5, %%mm6\n"       /* r6 = 67 66 65 64 */
    "pmullw 120(%[q]), %%mm7\n"  /* r7 = 77 76 75 74 */
    "psrlq  $32, %%mm5\n"        /* r5 = __ __ 67 66 */
    "pand   %%mm2, %%mm6\n"       /* r6 = __ __ __ 64 */
    "movq   %%mm5, %%mm1\n"       /* r1 = __ __ 67 66 */
    "movq   %%mm0, 56(%[o])\n"   /* write R7 = r0 */
    "pand   %%mm2, %%mm1\n"       /* r1 = __ __ __ 66 */
    "movq   112(%[i]), %%mm0\n"
    "movq   %%mm7, %%mm3\n"       /* r3 = 77 76 75 74 */
    "pmullw 112(%[q]), %%mm0\n"  /* r0 = 73 72 71 70 */
    "psllq  $16, %%mm3\n"        /* r3 = 76 75 74 __ */
    "pand   "M(3)", %%mm7\n"     /* r7 = 77 __ __ __ */
    "pxor   %%mm1, %%mm5\n"       /* r5 = __ __ 67 __ */
    "por    %%mm5, %%mm6\n"       /* r6 = __ __ 67 64 */
    "movq   %%mm3, %%mm5\n"       /* r5 = 76 75 74 __ */
    "pand   "M(3)", %%mm5\n"     /* r5 = 76 __ __ __ */
    "por    %%mm1, %%mm7\n"       /* r7 = 77 __ __ 66 */
    "movq   96(%[i]), %%mm1\n"
    "pxor   %%mm5, %%mm3\n"       /* r3 = __ 75 74 __ */
    "pmullw 96(%[q]), %%mm1\n"   /* r1 = 63 62 61 60 */
    "por    %%mm3, %%mm7\n"       /* r7 = 77 75 74 66 = R15 */
    "por    %%mm5, %%mm6\n"       /* r6 = 76 __ 67 64 */
    "movq   %%mm0, %%mm5\n"       /* r5 = 73 72 71 70 */
    "movq   %%mm7, 120(%[o])\n"  /* store R15 = r7 */
    "psrlq  $16, %%mm5\n"        /* r5 = __ 73 72 71 */
    "pand   "M(2)", %%mm5\n"     /* r5 = __ 73 __ __ */
    "movq   %%mm0, %%mm7\n"       /* r7 = 73 72 71 70 */
    "por    %%mm5, %%mm6\n"       /* r6 = 76 73 67 64 = R14 */
    "pand   %%mm2, %%mm0\n"       /* r0 = __ __ __ 70 */
    "pxor   %%mm0, %%mm7\n"       /* r7 = 73 72 71 __ */
    "psllq  $32, %%mm0\n"        /* r0 = __ 70 __ __ */
    "movq   %%mm6, 104(%[o])\n"  /* write R14 = r6 */
    "psrlq  $16, %%mm4\n"        /* r4 = __ 57 56 55 */
    "movq   72(%[i]), %%mm5\n"
    "psllq  $16, %%mm7\n"        /* r7 = 72 71 __ __ */
    "pmullw 72(%[q]), %%mm5\n"   /* r5 = 47 46 45 44 */
    "movq   %%mm7, %%mm6\n"       /* r6 = 72 71 __ __ */
    "movq   "M(2)", %%mm3\n"     /* r3 = __ FF __ __ */
    "psllq  $16, %%mm6\n"        /* r6 = 71 __ __ __ */
    "pand   "M(3)", %%mm7\n"     /* r7 = 72 __ __ __ */
    "pand   %%mm1, %%mm3\n"       /* r3 = __ 62 __ __ */
    "por    %%mm0, %%mm7\n"       /* r7 = 72 70 __ __ */
    "movq   %%mm1, %%mm0\n"       /* r0 = 63 62 61 60 */
    "pand   "M(3)", %%mm1\n"     /* r1 = 63 __ __ __ */
    "por    %%mm3, %%mm6\n"       /* r6 = 71 62 __ __ */
    "movq   %%mm4, %%mm3\n"       /* r3 = __ 57 56 55 */
    "psrlq  $32, %%mm1\n"        /* r1 = __ __ 63 __ */
    "pand   %%mm2, %%mm3\n"       /* r3 = __ __ __ 55 */
    "por    %%mm1, %%mm7\n"       /* r7 = 72 70 63 __ */
    "por    %%mm3, %%mm7\n"       /* r7 = 72 70 63 55 = R13 */
    "movq   %%mm4, %%mm3\n"       /* r3 = __ 57 56 55 */
    "pand   "M(1)", %%mm3\n"     /* r3 = __ __ 56 __ */
    "movq   %%mm5, %%mm1\n"       /* r1 = 47 46 45 44 */
    "movq   %%mm7, 88(%[o])\n"   /* write R13 = r7 */
    "psrlq  $48, %%mm5\n"        /* r5 = __ __ __ 47 */
    "movq   64(%[i]), %%mm7\n"
    "por    %%mm3, %%mm6\n"       /* r6 = 71 62 56 __ */
    "pmullw 64(%[q]), %%mm7\n"   /* r7 = 43 42 41 40 */
    "por    %%mm5, %%mm6\n"       /* r6 = 71 62 56 47 = R12 */
    "pand   "M(2)", %%mm4\n"     /* r4 = __ 57 __ __ */
    "psllq  $32, %%mm0\n"        /* r0 = 61 60 __ __ */
    "movq   %%mm6, 72(%[o])\n"   /* write R12 = r6 */
    "movq   %%mm0, %%mm6\n"       /* r6 = 61 60 __ __ */
    "pand   "M(3)", %%mm0\n"     /* r0 = 61 __ __ __ */
    "psllq  $16, %%mm6\n"        /* r6 = 60 __ __ __ */
    "movq   40(%[i]), %%mm5\n"
    "movq   %%mm1, %%mm3\n"       /* r3 = 47 46 45 44 */
    "pmullw 40(%[q]), %%mm5\n"   /* r5 = 27 26 25 24 */
    "psrlq  $16, %%mm1\n"        /* r1 = __ 47 46 45 */
    "pand   "M(1)", %%mm1\n"     /* r1 = __ __ 46 __ */
    "por    %%mm4, %%mm0\n"       /* r0 = 61 57 __ __ */
    "pand   %%mm7, %%mm2\n"       /* r2 = __ __ __ 40 */
    "por    %%mm1, %%mm0\n"       /* r0 = 61 57 46 __ */
    "por    %%mm2, %%mm0\n"       /* r0 = 61 57 46 40 = R11 */
    "psllq  $16, %%mm3\n"        /* r3 = 46 45 44 __ */
    "movq   %%mm3, %%mm4\n"       /* r4 = 46 45 44 __ */
    "movq   %%mm5, %%mm2\n"       /* r2 = 27 26 25 24 */
    "movq   %%mm0, 112(%[o])\n"  /* write R11 = r0 */
    "psrlq  $48, %%mm2\n"        /* r2 = __ __ __ 27 */
    "pand   "M(2)", %%mm4\n"     /* r4 = __ 45 __ __ */
    "por    %%mm2, %%mm6\n"       /* r6 = 60 __ __ 27 */
    "movq   "M(1)", %%mm2\n"     /* r2 = __ __ FF __ */
    "por    %%mm4, %%mm6\n"       /* r6 = 60 45 __ 27 */
    "pand   %%mm7, %%mm2\n"       /* r2 = __ __ 41 __ */
    "psllq  $32, %%mm3\n"        /* r3 = 44 __ __ __ */
    "por    80(%[o]), %%mm3\n"   /* r3 = 44 __ __ 23 */
    "por    %%mm2, %%mm6\n"       /* r6 = 60 45 41 27 = R10 */
    "movq   "M(3)", %%mm2\n"     /* r2 = FF __ __ __ */
    "psllq  $16, %%mm5\n"        /* r5 = 26 25 24 __ */
    "movq   %%mm6, 96(%[o])\n"   /* store R10 = r6 */
    "pand   %%mm5, %%mm2\n"       /* r2 = 26 __ __ __ */
    "movq   "M(2)", %%mm6\n"     /* r6 = __ FF __ __ */
    "pxor   %%mm2, %%mm5\n"       /* r5 = __ 25 24 __ */
    "pand   %%mm7, %%mm6\n"       /* r6 = __ 42 __ __ */
    "psrlq  $32, %%mm2\n"        /* r2 = __ __ 26 __ */
    "pand   "M(3)", %%mm7\n"     /* r7 = 43 __ __ __ */
    "por    %%mm2, %%mm3\n"       /* r3 = 44 __ 26 23 */
    "por    64(%[o]), %%mm7\n"   /* r7 = 43 __ __ 12 */
    "por    %%mm3, %%mm6\n"       /* r6 = 44 42 26 23 = R9 */
    "por    %%mm5, %%mm7\n"       /* r7 = 43 25 24 12 = R8 */
    "movq   %%mm6, 80(%[o])\n"   /* store R9 = r6 */
    "movq   %%mm7, 64(%[o])\n"   /* store R8 = r7 */
    
    /* 123c  ( / 64 coeffs  < 2c / coeff) */

/* Done w/dequant + descramble + partial transpose; now do the idct itself. */

#   define I( K)    MtoSTR((K*16))"(%[o])"
#   define J( K)    MtoSTR(((K - 4)*16)+8)"(%[o])"

    RowIDCT         /* 46 c */
    Transpose       /* 19 c */

#   undef I
#   undef J
#   define I( K)    MtoSTR((K*16)+64)"(%[o])"
#   define J( K)    MtoSTR(((K-4)*16)+72)"(%[o])"

    RowIDCT         /* 46 c */
    Transpose       /* 19 c */

#   undef I
#   undef J
#   define I( K)    MtoSTR((K * 16))"(%[o])"
#   define J( K)    I( K)

    ColumnIDCT      /* 57 c */

#   undef I
#   undef J
#   define I( K)    MtoSTR((K*16)+8)"(%[o])"
#   define J( K)    I( K)

    ColumnIDCT      /* 57 c */

#   undef I
#   undef J
    /* 368 cycles  ( / 64 coeff  <  6 c / coeff) */

    "emms\n"
    :
    :[i]"r"(in),[q]"r"(q),[o]"r"(out),[c]"r"(idctconstants)
   );
}

/**************************************************************************************
 *
 *      Routine:        MMX_idct10
 *
 *      Description:    Perform IDCT on a 8x8 block with at most 10 nonzero coefficients
 *
 *      Input:          Pointer to input and output buffer
 *
 *      Output:         None
 *
 *      Return:         None
 *
 *      Special Note:   The input coefficients are in transposed ZigZag order
 *
 *      Error:          None
 *
 ***************************************************************************************
 */
/* --------------------------------------------------------------- */
// This macro does four 4-sample one-dimensional idcts in parallel.  Inputs
// 4 thru 7 are assumed to be zero.
#define BeginIDCT_10 "#BeginIDCT_10\n"  \
    "   movq    "I(3)",%%mm2\n"          \
                                        \
    "   movq    "C(3)",%%mm6\n"          \
    "   movq    %%mm2,%%mm4\n"            \
                                        \
    "   movq    "C(5)",%%mm1\n"          \
    "   pmulhw  %%mm6,%%mm4\n"            \
                                        \
    "   movq    "I(1)",%%mm3\n"          \
    "   pmulhw  %%mm2,%%mm1\n"            \
                                        \
    "   movq    "C(1)",%%mm0\n"          \
    "   paddw   %%mm2,%%mm4\n"            \
                                        \
    "   pxor    %%mm6,%%mm6\n"            \
    "   paddw   %%mm1,%%mm2\n"            \
                                        \
    "   movq    "I(2)",%%mm5\n"          \
    "   pmulhw  %%mm3,%%mm0\n"            \
                                        \
    "   movq    %%mm5,%%mm1\n"            \
    "   paddw   %%mm3,%%mm0\n"            \
                                        \
    "   pmulhw  "C(7)",%%mm3\n"          \
    "   psubsw  %%mm2,%%mm6\n"            \
                                        \
    "   pmulhw  "C(2)",%%mm5\n"          \
    "   psubsw  %%mm4,%%mm0\n"            \
                                        \
    "   movq    "I(2)",%%mm7\n"          \
    "   paddsw  %%mm4,%%mm4\n"            \
                                        \
    "   paddw   %%mm5,%%mm7\n"            \
    "   paddsw  %%mm0,%%mm4\n"            \
                                        \
    "   pmulhw  "C(6)",%%mm1\n"          \
    "   psubsw  %%mm6,%%mm3\n"            \
                                        \
    "   movq    %%mm4,"I(1)"\n"          \
    "   paddsw  %%mm6,%%mm6\n"            \
                                        \
    "   movq    "C(4)",%%mm4\n"          \
    "   paddsw  %%mm3,%%mm6\n"            \
                                        \
    "   movq    %%mm3,%%mm5\n"            \
    "   pmulhw  %%mm4,%%mm3\n"            \
                                        \
    "   movq    %%mm6,"I(2)"\n"          \
    "   movq    %%mm0,%%mm2\n"            \
                                        \
    "   movq    "I(0)",%%mm6\n"          \
    "   pmulhw  %%mm4,%%mm0\n"            \
                                        \
    "   paddw   %%mm3,%%mm5\n"            \
    "   paddw   %%mm0,%%mm2\n"            \
                                        \
    "   psubsw  %%mm1,%%mm5\n"            \
    "   pmulhw  %%mm4,%%mm6\n"            \
                                        \
    "   paddw   "I(0)",%%mm6\n"          \
    "   paddsw  %%mm1,%%mm1\n"            \
                                        \
    "   movq    %%mm6,%%mm4\n"            \
    "   paddsw  %%mm5,%%mm1\n"            \
                                        \
    "   psubsw  %%mm2,%%mm6\n"            \
    "   paddsw  %%mm2,%%mm2\n"            \
                                        \
    "   movq    "I(1)",%%mm0\n"          \
    "   paddsw  %%mm6,%%mm2\n"            \
                                        \
    "   psubsw  %%mm1,%%mm2\n"            \
    "#end BeginIDCT_10\n"
// end BeginIDCT_10 macro (25 cycles).

#define RowIDCT_10 "#RowIDCT_10\n"                           \
    BeginIDCT_10                                            \
    "\n"                                                    \
    "   movq    "I(2)",%%mm3\n"  /* r3 = D. */               \
    "   psubsw  %%mm7,%%mm4\n"        /* r4 = E. = E - G */   \
    "   paddsw  %%mm1,%%mm1\n"        /* r1 = H. + H. */      \
    "   paddsw  %%mm7,%%mm7\n"        /* r7 = G + G */        \
    "   paddsw  %%mm2,%%mm1\n"        /* r1 = R1 = A.. + H. */\
    "   paddsw  %%mm4,%%mm7\n"        /* r7 = G. = E + G */   \
    "   psubsw  %%mm3,%%mm4\n"        /* r4 = R4 = E. - D. */ \
    "   paddsw  %%mm3,%%mm3\n"                                \
    "   psubsw  %%mm5,%%mm6\n"        /* r6 = R6 = F. - B.. */\
    "   paddsw  %%mm5,%%mm5\n"                                \
    "   paddsw  %%mm4,%%mm3\n"        /* r3 = R3 = E. + D. */ \
    "   paddsw  %%mm6,%%mm5\n"        /* r5 = R5 = F. + B.. */\
    "   psubsw  %%mm0,%%mm7\n"        /* r7 = R7 = G. - C. */ \
    "   paddsw  %%mm0,%%mm0\n"                                \
    "   movq    %%mm1,"I(1)"\n"  /* save R1 */               \
    "   paddsw  %%mm7,%%mm0\n"        /* r0 = R0 = G. + C. */ \
    "#end RowIDCT_10\n"									     
// end RowIDCT macro (8 + 38 = 46 cycles)

// Column IDCT normalizes and stores final results.

#define ColumnIDCT_10 "#ColumnIDCT_10\n"               \
    BeginIDCT_10                                        \
    "\n"                                                \
    "   paddsw  "Eight",%%mm2\n"                         \
    "   paddsw  %%mm1,%%mm1\n"    /* r1 = H. + H. */      \
    "   paddsw  %%mm2,%%mm1\n"    /* r1 = R1 = A.. + H. */\
    "   psraw   ""$4"",%%mm2\n"      /* r2 = NR2 */      \
    "   psubsw  %%mm7,%%mm4\n"    /* r4 = E. = E - G */   \
    "   psraw   ""$4"",%%mm1\n"      /* r1 = NR1 */      \
    "   movq    "I(2)",%%mm3\n"  /* r3 = D. */           \
    "   paddsw  %%mm7,%%mm7\n"    /* r7 = G + G */        \
    "   movq    %%mm2,"I(2)"\n"  /* store NR2 at I2 */   \
    "   paddsw  %%mm4,%%mm7\n"    /* r7 = G. = E + G */   \
    "   movq    %%mm1,"I(1)"\n"  /* store NR1 at I1 */   \
    "   psubsw  %%mm3,%%mm4\n"    /* r4 = R4 = E. - D. */ \
    "   paddsw  "Eight",%%mm4\n"                         \
    "   paddsw  %%mm3,%%mm3\n"    /* r3 = D. + D. */      \
    "   paddsw  %%mm4,%%mm3\n"    /* r3 = R3 = E. + D. */ \
    "   psraw   ""$4"",%%mm4\n"      /* r4 = NR4 */      \
    "   psubsw  %%mm5,%%mm6\n"    /* r6 = R6 = F. - B.. */\
    "   psraw   ""$4"",%%mm3\n"      /* r3 = NR3 */      \
    "   paddsw  "Eight",%%mm6\n"                         \
    "   paddsw  %%mm5,%%mm5\n"    /* r5 = B.. + B.. */    \
    "   paddsw  %%mm6,%%mm5\n"    /* r5 = R5 = F. + B.. */\
    "   psraw   ""$4"",%%mm6\n"      /* r6 = NR6 */      \
    "   movq    %%mm4,"J(4)"\n"  /* store NR4 at J4 */   \
    "   psraw   ""$4"",%%mm5\n"      /* r5 = NR5 */      \
    "   movq    %%mm3,"I(3)"\n"  /* store NR3 at I3 */   \
    "   psubsw  %%mm0,%%mm7\n"    /* r7 = R7 = G. - C. */ \
    "   paddsw  "Eight",%%mm7\n"                         \
    "   paddsw  %%mm0,%%mm0\n"    /* r0 = C. + C. */      \
    "   paddsw  %%mm7,%%mm0\n"    /* r0 = R0 = G. + C. */ \
    "   psraw   ""$4"",%%mm7\n"      /* r7 = NR7 */      \
    "   movq    %%mm6,"J(6)"\n"  /* store NR6 at J6 */   \
    "   psraw   ""$4"",%%mm0\n"      /* r0 = NR0 */      \
    "   movq    %%mm5,"J(5)"\n"  /* store NR5 at J5 */   \
                                                        \
    "   movq    %%mm7,"J(7)"\n"  /* store NR7 at J7 */   \
                                                        \
    "   movq    %%mm0,"I(0)"\n"  /* store NR0 at I0 */   \
    "#end ColumnIDCT_10\n"								

// end ColumnIDCT macro (38 + 19 = 57 cycles)
/* --------------------------------------------------------------- */


/* --------------------------------------------------------------- */
/* IDCT 10 */
void IDct10__mmx( const ogg_int16_t *in,
		  const ogg_int16_t *q,
		  ogg_int16_t *out ) {

    __asm__ __volatile__ (

    "movq   (%[i]), %%mm0\n"
    "pmullw (%[q]), %%mm0\n"     /* r0 = 03 02 01 00 */
    "movq   16(%[i]), %%mm1\n"
    "pmullw 16(%[q]), %%mm1\n"   /* r1 = 13 12 11 10 */
    "movq   "M(0)", %%mm2\n"     /* r2 = __ __ __ FF */
    "movq   %%mm0, %%mm3\n"       /* r3 = 03 02 01 00 */
    "movq   8(%[i]), %%mm4\n"
    "psrlq  $16, %%mm0\n"        /* r0 = __ 03 02 01 */
    "pmullw 8(%[q]), %%mm4\n"    /* r4 = 07 06 05 04 */
    "pand   %%mm2, %%mm3\n"       /* r3 = __ __ __ 00 */
    "movq   %%mm0, %%mm5\n"       /* r5 = __ 03 02 01 */
    "pand   %%mm2, %%mm5\n"       /* r5 = __ __ __ 01 */
    "psllq  $32, %%mm1\n"        /* r1 = 11 10 __ __ */
    "movq   "M(3)", %%mm7\n"     /* r7 = FF __ __ __ */
    "pxor   %%mm5, %%mm0\n"       /* r0 = __ 03 02 __ */
    "pand   %%mm1, %%mm7\n"       /* r7 = 11 __ __ __ */
    "por    %%mm3, %%mm0\n"       /* r0 = __ 03 02 00 */
    "pxor   %%mm7, %%mm1\n"       /* r1 = __ 10 __ __ */
    "por    %%mm7, %%mm0\n"       /* r0 = 11 03 02 00 = R0 */
    "movq   %%mm4, %%mm3\n"       /* r3 = 07 06 05 04 */
    "movq   %%mm0, (%[o])\n"     /* write R0 = r0 */
    "pand   %%mm2, %%mm3\n"       /* r3 = __ __ __ 04 */
    "psllq  $16, %%mm3\n"        /* r3 = __ __ 04 __ */
    "por    %%mm3, %%mm5\n"       /* r5 = __ __ 04 01 */
    "por    %%mm5, %%mm1\n"       /* r1 = __ 10 04 01 = R1 */
    "psrlq  $16, %%mm4\n"        /* r4 = __ 07 06 05 */
    "movq   %%mm1, 16(%[o])\n"   /* write R1 = r1 */
    "movq   %%mm4, %%mm5\n"       /* r5 = __ 07 06 05 */
    "psrlq  $16, %%mm4\n"        /* r4 = __ __ 07 06 */
    "movq   %%mm2, %%mm6\n"       /* r6 = __ __ __ FF */
    "pand   %%mm2, %%mm5\n"       /* r5 = __ __ __ 05 */
    "pand   %%mm4, %%mm6\n"       /* r6 = __ __ __ 06 */
    "pxor   %%mm6, %%mm4\n"       /* r4 = __ __ 07 __ */
    "por    %%mm5, %%mm4\n"       /* r4 = __ __ 07 05 */
    "movq   %%mm4, 32(%[o])\n"   /* write R2 = r4 */
    "movq   %%mm6, 48(%[o])\n"   /* write R3 = r6 */

#   define I( K)    MtoSTR((K*16))"(%[o])"
#   define J( K)    MtoSTR(((K - 4) * 16)+8)"(%[o])"

    RowIDCT_10      /* 33 c */
    Transpose       /* 19 c */

#   undef I
#   undef J

#   define I( K)    MtoSTR((K * 16))"(%[o])"
#   define J( K)    I( K)

    ColumnIDCT_10       /* 44 c */

#   undef I
#   undef J
#   define I( K)    MtoSTR((K * 16)+8)"(%[o])"
#   define J( K)    I( K)

    ColumnIDCT_10       /* 44 c */

#   undef I
#   undef J

    "emms\n"
    :
    :[i]"r"(in),[q]"r"(q),[o]"r"(out),[c]"r"(idctconstants)
    );
}

/**************************************************************************************
 *
 *      Routine:        MMX_idct3
 *
 *      Description:    Perform IDCT on a 8x8 block with at most 3 nonzero coefficients
 *
 *      Input:          Pointer to input and output buffer
 *
 *      Output:         None
 *
 *      Return:         None
 *
 *      Special Note:   Only works for three nonzero coefficients.
 *
 *      Error:          None
 *
 ***************************************************************************************
 */
/***************************************************************************************
    In IDCT 3, we are dealing with only three Non-Zero coefficients in the 8x8 block.
    In the case that we work in the fashion RowIDCT -> ColumnIDCT, we only have to
    do 1-D row idcts on the first two rows, the rest six rows remain zero anyway.
    After row IDCTs, since every column could have nonzero coefficients, we need do
    eight 1-D column IDCT. However, for each column, there are at most two nonzero
    coefficients, coefficient 0 and coefficient 1. Same for the coefficents for the
    two 1-d row idcts. For this reason, the process of a 1-D IDCT is simplified

    from a full version:

    A = (C1 * I1) + (C7 * I7)       B = (C7 * I1) - (C1 * I7)
    C = (C3 * I3) + (C5 * I5)       D = (C3 * I5) - (C5 * I3)
    A. = C4 * (A - C)               B. = C4 * (B - D)
    C. = A + C                      D. = B + D

    E = C4 * (I0 + I4)              F = C4 * (I0 - I4)
    G = (C2 * I2) + (C6 * I6)       H = (C6 * I2) - (C2 * I6)
    E. = E - G
    G. = E + G

    A.. = F + A.                    B.. = B. - H
    F.  = F - A.                    H.  = B. + H

    R0 = G. + C.    R1 = A.. + H.   R3 = E. + D.    R5 = F. + B..
    R7 = G. - C.    R2 = A.. - H.   R4 = E. - D.    R6 = F. - B..

    To:


    A = (C1 * I1)                   B = (C7 * I1)
    C = 0                           D = 0
    A. = C4 * A                     B. = C4 * B
    C. = A                          D. = B

    E = C4 * I0                     F = E
    G = 0                           H = 0
    E. = E
    G. = E

    A.. = E + A.                    B.. = B.
    F.  = E - A.                    H.  = B.

    R0 = E + A      R1 = E + A. + B.    R3 = E + B      R5 = E - A. + B.
    R7 = E - A      R2 = E + A. - B.    R4 = E - B      R6 = F - A. - B.

******************************************************************************************/

#define RowIDCT_3 "#RowIDCT_3\n"\
    "   movq        "I(1)",%%mm7\n"  /* r7 = I1                      */  \
    "   movq        "C(1)",%%mm0\n"  /* r0 = C1                      */  \
    "   movq        "C(7)",%%mm3\n"  /* r3 = C7                      */  \
    "   pmulhw      %%mm7,%%mm0\n"    /* r0 = C1 * I1 - I1            */  \
    "   pmulhw      %%mm7,%%mm3\n"    /* r3 = C7 * I1 = B, D.         */  \
    "   movq        "I(0)",%%mm6\n"  /* r6 = I0                      */  \
    "   movq        "C(4)",%%mm4\n"  /* r4 = C4                      */  \
    "   paddw       %%mm7,%%mm0\n"    /* r0 = C1 * I1 = A, C.         */  \
    "   movq        %%mm6,%%mm1\n"    /* make a copy of I0            */  \
    "   pmulhw      %%mm4,%%mm6\n"    /* r2 = C4 * I0 - I0            */  \
    "   movq        %%mm0,%%mm2\n"    /* make a copy of A             */  \
    "   movq        %%mm3,%%mm5\n"    /* make a copy of B             */  \
    "   pmulhw      %%mm4,%%mm2\n"    /* r2 = C4 * A - A              */  \
    "   pmulhw      %%mm4,%%mm5\n"    /* r5 = C4 * B - B              */  \
    "   paddw       %%mm1,%%mm6\n"    /* r2 = C4 * I0 = E, F          */  \
    "   movq        %%mm6,%%mm4\n"    /* r4 = E                       */  \
    "   paddw       %%mm0,%%mm2\n"    /* r2 = A.                      */  \
    "   paddw       %%mm3,%%mm5\n"    /* r5 = B.                      */  \
    "   movq        %%mm6,%%mm7\n"    /* r7 = E                       */  \
    "   movq        %%mm5,%%mm1\n"    /* r1 = B.                      */  \
    /*  r0 = A      */   \
    /*  r3 = B      */   \
    /*  r2 = A.     */   \
    /*  r5 = B.     */   \
    /*  r6 = E      */   \
    /*  r4 = E      */   \
    /*  r7 = E      */   \
    /*  r1 = B.     */   \
    "   psubw       %%mm2,%%mm6\n"    /* r6 = E - A.                  */  \
    "   psubw       %%mm3,%%mm4\n"    /* r4 = E - B ----R4            */  \
    "   psubw       %%mm0,%%mm7\n"    /* r7 = E - A ----R7            */  \
    "   paddw       %%mm2,%%mm2\n"    /* r2 = A. + A.                 */  \
    "   paddw       %%mm3,%%mm3\n"    /* r3 = B + B                   */  \
    "   paddw       %%mm0,%%mm0\n"    /* r0 = A + A                   */  \
    "   paddw       %%mm6,%%mm2\n"    /* r2 = E + A.                  */  \
    "   paddw       %%mm4,%%mm3\n"    /* r3 = E + B ----R3            */  \
    "   psubw       %%mm1,%%mm2\n"    /* r2 = E + A. - B. ----R2      */  \
    "   psubw       %%mm5,%%mm6\n"    /* r6 = E - A. - B. ----R6      */  \
    "   paddw       %%mm1,%%mm1\n"    /* r1 = B. + B.                 */  \
    "   paddw       %%mm5,%%mm5\n"    /* r5 = B. + B.                 */  \
    "   paddw       %%mm7,%%mm0\n"    /* r0 = E + A ----R0            */  \
    "   paddw       %%mm2,%%mm1\n"    /* r1 = E + A. + B. -----R1     */  \
    "   movq        %%mm1,"I(1)"\n"  /* save r1                      */  \
    "   paddw       %%mm6,%%mm5\n"    /* r5 = E - A. + B. -----R5     */  \
    "#end RowIDCT_3\n"
//End of RowIDCT_3

#define ColumnIDCT_3 "#ColumnIDCT_3\n"\
    "   movq        "I(1)",%%mm7\n"  /* r7 = I1                      */  \
    "   movq        "C(1)",%%mm0\n"  /* r0 = C1                      */  \
    "   movq        "C(7)",%%mm3\n"  /* r3 = C7                      */  \
    "   pmulhw      %%mm7,%%mm0\n"    /* r0 = C1 * I1 - I1            */  \
    "   pmulhw      %%mm7,%%mm3\n"    /* r3 = C7 * I1 = B, D.         */  \
    "   movq        "I(0)",%%mm6\n"  /* r6 = I0                      */  \
    "   movq        "C(4)",%%mm4\n"  /* r4 = C4                      */  \
    "   paddw       %%mm7,%%mm0\n"    /* r0 = C1 * I1 = A, C.         */  \
    "   movq        %%mm6,%%mm1\n"    /* make a copy of I0            */  \
    "   pmulhw      %%mm4,%%mm6\n"    /* r2 = C4 * I0 - I0            */  \
    "   movq        %%mm0,%%mm2\n"    /* make a copy of A             */  \
    "   movq        %%mm3,%%mm5\n"    /* make a copy of B             */  \
    "   pmulhw      %%mm4,%%mm2\n"    /* r2 = C4 * A - A              */  \
    "   pmulhw      %%mm4,%%mm5\n"    /* r5 = C4 * B - B              */  \
    "   paddw       %%mm1,%%mm6\n"    /* r2 = C4 * I0 = E, F          */  \
    "   movq        %%mm6,%%mm4\n"    /* r4 = E                       */  \
    "   paddw       "Eight",%%mm6\n" /* +8 for shift                 */  \
    "   paddw       "Eight",%%mm4\n" /* +8 for shift                 */  \
    "   paddw       %%mm0,%%mm2\n"    /* r2 = A.                      */  \
    "   paddw       %%mm3,%%mm5\n"    /* r5 = B.                      */  \
    "   movq        %%mm6,%%mm7\n"    /* r7 = E                       */  \
    "   movq        %%mm5,%%mm1\n"    /* r1 = B.                      */  \
/*  r0 = A      */   \
/*  r3 = B      */   \
/*  r2 = A.     */   \
/*  r5 = B.     */   \
/*  r6 = E      */   \
/*  r4 = E      */   \
/*  r7 = E      */   \
/*  r1 = B.     */   \
    "   psubw       %%mm2,%%mm6\n"    /* r6 = E - A.                  */  \
    "   psubw       %%mm3,%%mm4\n"    /* r4 = E - B ----R4            */  \
    "   psubw       %%mm0,%%mm7\n"    /* r7 = E - A ----R7            */  \
    "   paddw       %%mm2,%%mm2\n"    /* r2 = A. + A.                 */  \
    "   paddw       %%mm3,%%mm3\n"    /* r3 = B + B                   */  \
    "   paddw       %%mm0,%%mm0\n"    /* r0 = A + A                   */  \
    "   paddw       %%mm6,%%mm2\n"    /* r2 = E + A.                  */  \
    "   paddw       %%mm4,%%mm3\n"    /* r3 = E + B ----R3            */  \
    "   psraw        $4,%%mm4\n"     /* shift                        */  \
    "   movq        %%mm4,"J(4)"\n"  /* store R4 at J4               */  \
    "   psraw       $4,%%mm3\n"      /* shift                        */  \
    "   movq        %%mm3,"I(3)"\n"  /* store R3 at I3               */  \
    "   psubw       %%mm1,%%mm2\n"    /* r2 = E + A. - B. ----R2      */  \
    "   psubw       %%mm5,%%mm6\n"    /* r6 = E - A. - B. ----R6      */  \
    "   paddw       %%mm1,%%mm1\n"    /* r1 = B. + B.                 */  \
    "   paddw       %%mm5,%%mm5\n"    /* r5 = B. + B.                 */  \
    "   paddw       %%mm7,%%mm0\n"    /* r0 = E + A ----R0            */  \
    "   paddw       %%mm2,%%mm1\n"    /* r1 = E + A. + B. -----R1     */  \
    "   psraw       $4,%%mm7\n"      /* shift                        */  \
    "   psraw       $4,%%mm2\n"      /* shift                        */  \
    "   psraw       $4,%%mm0\n"      /* shift                        */  \
    "   psraw       $4,%%mm1\n"      /* shift                        */  \
    "   movq        %%mm7,"J(7)"\n"  /* store R7 to J7               */  \
    "   movq        %%mm0,"I(0)"\n"  /* store R0 to I0               */  \
    "   movq        %%mm1,"I(1)"\n"  /* store R1 to I1               */  \
    "   movq        %%mm2,"I(2)"\n"  /* store R2 to I2               */  \
    "   movq        %%mm1,"I(1)"\n"  /* save r1                      */  \
    "   paddw       %%mm6,%%mm5\n"    /* r5 = E - A. + B. -----R5     */  \
    "   psraw       $4,%%mm5\n"      /* shift                        */  \
    "   movq        %%mm5,"J(5)"\n"  /* store R5 at J5               */  \
    "   psraw       $4,%%mm6\n"      /* shift                        */  \
    "   movq        %%mm6,"J(6)"\n"  /* store R6 at J6               */  \
    "#end ColumnIDCT_3\n"
//End of ColumnIDCT_3

void IDct3__mmx( const ogg_int16_t *in,
		 const ogg_int16_t *q,
		 ogg_int16_t *out ) {

    __asm__ __volatile__ (

    "movq   (%[i]), %%mm0\n"     
    "pmullw (%[q]), %%mm0\n"     /* r0 = 03 02 01 00 */
    "movq   "M(0)", %%mm2\n"     /* r2 = __ __ __ FF */
    "movq   %%mm0, %%mm3\n"       /* r3 = 03 02 01 00 */
    "psrlq  $16, %%mm0\n"        /* r0 = __ 03 02 01 */
    "pand   %%mm2, %%mm3\n"       /* r3 = __ __ __ 00 */
    "movq   %%mm0, %%mm5\n"       /* r5 = __ 03 02 01 */
    "pand   %%mm2, %%mm5\n"       /* r5 = __ __ __ 01 */
    "pxor   %%mm5, %%mm0\n"       /* r0 = __ 03 02 __ */
    "por    %%mm3, %%mm0\n"       /* r0 = __ 03 02 00 */
    "movq   %%mm0, (%[o])\n"     /* write R0 = r0 */
    "movq   %%mm5, 16(%[o])\n"   /* write R1 = r5 */

/* Done partial transpose; now do the idct itself. */

#   define I( K)    MtoSTR((K*16))"(%[o])"
#   define J( K)    MtoSTR(((K - 4)*16)+8)"(%[o])"

    RowIDCT_3       /* 33 c */
    Transpose       /* 19 c */

#   undef I
#   undef J

#   define I( K)    MtoSTR((K * 16))"(%[o])"
#   define J( K)    I( K)

    ColumnIDCT_3    /* 44 c */

#   undef I
#   undef J
#   define I( K)    MtoSTR((K*16)+8)"(%[o])"
#   define J( K)    I( K)
    
    ColumnIDCT_3    /* 44 c */
    
#   undef I
#   undef J
    
    "emms\n"
    :
    :[i]"r"(in),[q]"r"(q),[o]"r"(out),[c]"r"(idctconstants)
    );
    
}

/* install our implementation in the function table */
void dsp_mmx_idct_init(DspFunctions *funcs)
{
  funcs->IDctSlow = IDctSlow__mmx;
  funcs->IDct10 = IDct10__mmx;
  funcs->IDct3 = IDct3__mmx;
}

#endif /* USE_ASM */


