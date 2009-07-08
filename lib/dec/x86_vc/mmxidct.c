/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id$

 ********************************************************************/

/*MMX acceleration of Theora's iDCT.
  Originally written by Rudolf Marek, based on code from On2's VP3.*/
#include "x86int.h"
#include "../dct.h"

#if defined(OC_X86_ASM)

/*These are offsets into the table of constants below.*/
/*7 rows of cosines, in order: pi/16 * (1 ... 7).*/
#define OC_COSINE_OFFSET (0)
/*A row of 8's.*/
#define OC_EIGHT_OFFSET  (56)



/*A table of constants used by the MMX routines.*/
static const __declspec(align(16))ogg_uint16_t
 OC_IDCT_CONSTS[(7+1)*4]={
  (ogg_uint16_t)OC_C1S7,(ogg_uint16_t)OC_C1S7,
  (ogg_uint16_t)OC_C1S7,(ogg_uint16_t)OC_C1S7,
  (ogg_uint16_t)OC_C2S6,(ogg_uint16_t)OC_C2S6,
  (ogg_uint16_t)OC_C2S6,(ogg_uint16_t)OC_C2S6,
  (ogg_uint16_t)OC_C3S5,(ogg_uint16_t)OC_C3S5,
  (ogg_uint16_t)OC_C3S5,(ogg_uint16_t)OC_C3S5,
  (ogg_uint16_t)OC_C4S4,(ogg_uint16_t)OC_C4S4,
  (ogg_uint16_t)OC_C4S4,(ogg_uint16_t)OC_C4S4,
  (ogg_uint16_t)OC_C5S3,(ogg_uint16_t)OC_C5S3,
  (ogg_uint16_t)OC_C5S3,(ogg_uint16_t)OC_C5S3,
  (ogg_uint16_t)OC_C6S2,(ogg_uint16_t)OC_C6S2,
  (ogg_uint16_t)OC_C6S2,(ogg_uint16_t)OC_C6S2,
  (ogg_uint16_t)OC_C7S1,(ogg_uint16_t)OC_C7S1,
  (ogg_uint16_t)OC_C7S1,(ogg_uint16_t)OC_C7S1,
      8,    8,    8,    8
};

/*38 cycles*/
#define OC_IDCT_BEGIN __asm{ \
  __asm movq mm2,OC_I(3) \
  __asm movq mm6,OC_C(3) \
  __asm movq mm4,mm2 \
  __asm movq mm7,OC_J(5) \
  __asm pmulhw mm4,mm6 \
  __asm movq mm1,OC_C(5) \
  __asm pmulhw mm6,mm7 \
  __asm movq mm5,mm1 \
  __asm pmulhw mm1,mm2 \
  __asm movq mm3,OC_I(1) \
  __asm pmulhw mm5,mm7 \
  __asm movq mm0,OC_C(1) \
  __asm paddw mm4,mm2 \
  __asm paddw mm6,mm7 \
  __asm paddw mm2,mm1 \
  __asm movq mm1,OC_J(7) \
  __asm paddw mm7,mm5 \
  __asm movq mm5,mm0 \
  __asm pmulhw mm0,mm3 \
  __asm paddw mm4,mm7 \
  __asm pmulhw mm5,mm1 \
  __asm movq mm7,OC_C(7) \
  __asm psubw mm6,mm2 \
  __asm paddw mm0,mm3 \
  __asm pmulhw mm3,mm7 \
  __asm movq mm2,OC_I(2) \
  __asm pmulhw mm7,mm1 \
  __asm paddw mm5,mm1 \
  __asm movq mm1,mm2 \
  __asm pmulhw mm2,OC_C(2) \
  __asm psubw mm3,mm5 \
  __asm movq mm5,OC_J(6) \
  __asm paddw mm0,mm7 \
  __asm movq mm7,mm5 \
  __asm psubw mm0,mm4 \
  __asm pmulhw mm5,OC_C(2) \
  __asm paddw mm2,mm1 \
  __asm pmulhw mm1,OC_C(6) \
  __asm paddw mm4,mm4 \
  __asm paddw mm4,mm0 \
  __asm psubw mm3,mm6 \
  __asm paddw mm5,mm7 \
  __asm paddw mm6,mm6 \
  __asm pmulhw mm7,OC_C(6) \
  __asm paddw mm6,mm3 \
  __asm movq OC_I(1),mm4 \
  __asm psubw mm1,mm5 \
  __asm movq mm4,OC_C(4) \
  __asm movq mm5,mm3 \
  __asm pmulhw mm3,mm4 \
  __asm paddw mm7,mm2 \
  __asm movq OC_I(2),mm6 \
  __asm movq mm2,mm0 \
  __asm movq mm6,OC_I(0) \
  __asm pmulhw mm0,mm4 \
  __asm paddw mm5,mm3 \
  __asm movq mm3,OC_J(4) \
  __asm psubw mm5,mm1 \
  __asm paddw mm2,mm0 \
  __asm psubw mm6,mm3 \
  __asm movq mm0,mm6 \
  __asm pmulhw mm6,mm4 \
  __asm paddw mm3,mm3 \
  __asm paddw mm1,mm1 \
  __asm paddw mm3,mm0 \
  __asm paddw mm1,mm5 \
  __asm pmulhw mm4,mm3 \
  __asm paddw mm6,mm0 \
  __asm psubw mm6,mm2 \
  __asm paddw mm2,mm2 \
  __asm movq mm0,OC_I(1) \
  __asm paddw mm2,mm6 \
  __asm paddw mm4,mm3 \
  __asm psubw mm2,mm1 \
}

/*38+8=46 cycles.*/
#define OC_ROW_IDCT __asm{ \
  OC_IDCT_BEGIN \
  /*r3=D'*/ \
  __asm  movq mm3,OC_I(2) \
  /*r4=E'=E-G*/ \
  __asm  psubw mm4,mm7 \
  /*r1=H'+H'*/ \
  __asm  paddw mm1,mm1 \
  /*r7=G+G*/ \
  __asm  paddw mm7,mm7 \
  /*r1=R1=A''+H'*/ \
  __asm  paddw mm1,mm2 \
  /*r7=G'=E+G*/ \
  __asm  paddw mm7,mm4 \
  /*r4=R4=E'-D'*/ \
  __asm  psubw mm4,mm3 \
  __asm  paddw mm3,mm3 \
  /*r6=R6=F'-B''*/ \
  __asm  psubw mm6,mm5 \
  __asm  paddw mm5,mm5 \
  /*r3=R3=E'+D'*/ \
  __asm  paddw mm3,mm4 \
  /*r5=R5=F'+B''*/ \
  __asm  paddw mm5,mm6 \
  /*r7=R7=G'-C'*/ \
  __asm  psubw mm7,mm0 \
  __asm  paddw mm0,mm0 \
  /*Save R1.*/ \
  __asm  movq OC_I(1),mm1 \
  /*r0=R0=G.+C.*/ \
  __asm  paddw mm0,mm7 \
}

/*The following macro does two 4x4 transposes in place.
  At entry, we assume:
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

  I(0) I(1) I(2) I(3) is the transpose of r0 I(1) r2 r3.
  J(4) J(5) J(6) J(7) is the transpose of r4  r5  r6 r7.

  Since r1 is free at entry, we calculate the Js first.*/
/*19 cycles.*/
#define OC_TRANSPOSE __asm{ \
  __asm movq mm1,mm4 \
  __asm punpcklwd mm4,mm5 \
  __asm movq OC_I(0),mm0 \
  __asm punpckhwd mm1,mm5 \
  __asm movq mm0,mm6 \
  __asm punpcklwd mm6,mm7 \
  __asm movq mm5,mm4 \
  __asm punpckldq mm4,mm6 \
  __asm punpckhdq mm5,mm6 \
  __asm movq mm6,mm1 \
  __asm movq OC_J(4),mm4 \
  __asm punpckhwd mm0,mm7 \
  __asm movq OC_J(5),mm5 \
  __asm punpckhdq mm6,mm0 \
  __asm movq mm4,OC_I(0) \
  __asm punpckldq mm1,mm0 \
  __asm movq mm5,OC_I(1) \
  __asm movq mm0,mm4 \
  __asm movq OC_J(7),mm6 \
  __asm punpcklwd mm0,mm5 \
  __asm movq OC_J(6),mm1 \
  __asm punpckhwd mm4,mm5 \
  __asm movq mm5,mm2 \
  __asm punpcklwd mm2,mm3 \
  __asm movq mm1,mm0 \
  __asm punpckldq mm0,mm2 \
  __asm punpckhdq mm1,mm2 \
  __asm movq mm2,mm4 \
  __asm movq OC_I(0),mm0 \
  __asm punpckhwd mm5,mm3 \
  __asm movq OC_I(1),mm1 \
  __asm punpckhdq mm4,mm5 \
  __asm punpckldq mm2,mm5 \
  __asm movq OC_I(3),mm4 \
  __asm movq OC_I(2),mm2 \
}

/*38+19=57 cycles.*/
#define OC_COLUMN_IDCT __asm{ \
  OC_IDCT_BEGIN \
  __asm paddw mm2,OC_8 \
  /*r1=H'+H'*/ \
  __asm paddw mm1,mm1 \
  /*r1=R1=A''+H'*/ \
  __asm paddw mm1,mm2 \
  /*r2=NR2*/ \
  __asm psraw mm2,4 \
  /*r4=E'=E-G*/ \
  __asm psubw mm4,mm7 \
  /*r1=NR1*/ \
  __asm psraw mm1,4 \
  /*r3=D'*/ \
  __asm movq mm3,OC_I(2) \
  /*r7=G+G*/ \
  __asm paddw mm7,mm7 \
  /*Store NR2 at I(2).*/ \
  __asm movq OC_I(2),mm2 \
  /*r7=G'=E+G*/ \
  __asm paddw mm7,mm4 \
  /*Store NR1 at I(1).*/ \
  __asm movq OC_I(1),mm1 \
  /*r4=R4=E'-D'*/ \
  __asm psubw mm4,mm3 \
  __asm paddw mm4,OC_8 \
  /*r3=D'+D'*/ \
  __asm paddw mm3,mm3 \
  /*r3=R3=E'+D'*/ \
  __asm paddw mm3,mm4 \
  /*r4=NR4*/ \
  __asm psraw mm4,4 \
  /*r6=R6=F'-B''*/ \
  __asm psubw mm6,mm5 \
  /*r3=NR3*/ \
  __asm psraw mm3,4 \
  __asm paddw mm6,OC_8 \
  /*r5=B''+B''*/ \
  __asm paddw mm5,mm5 \
  /*r5=R5=F'+B''*/ \
  __asm paddw mm5,mm6 \
  /*r6=NR6*/ \
  __asm psraw mm6,4 \
  /*Store NR4 at J(4).*/ \
  __asm movq OC_J(4),mm4 \
  /*r5=NR5*/ \
  __asm psraw mm5,4 \
  /*Store NR3 at I(3).*/ \
  __asm movq OC_I(3),mm3 \
  /*r7=R7=G'-C'*/ \
  __asm psubw mm7,mm0 \
  __asm paddw mm7,OC_8 \
  /*r0=C'+C'*/ \
  __asm paddw mm0,mm0 \
  /*r0=R0=G'+C'*/ \
  __asm paddw mm0,mm7 \
  /*r7=NR7*/ \
  __asm psraw mm7,4 \
  /*Store NR6 at J(6).*/ \
  __asm movq OC_J(6),mm6 \
  /*r0=NR0*/ \
  __asm psraw mm0,4 \
  /*Store NR5 at J(5).*/ \
  __asm movq OC_J(5),mm5 \
  /*Store NR7 at J(7).*/ \
  __asm movq OC_J(7),mm7 \
  /*Store NR0 at I(0).*/ \
  __asm movq OC_I(0),mm0 \
}

#define OC_MID(_m,_i) [CONSTS+_m+(_i)*8]
#define OC_C(_i)      OC_MID(OC_COSINE_OFFSET,_i-1)
#define OC_8          OC_MID(OC_EIGHT_OFFSET,0)

static void oc_idct8x8_slow(ogg_int16_t _y[64]){
  /*This routine accepts an 8x8 matrix, but in partially transposed form.
    Every 4x4 block is transposed.*/
  __asm{
#define CONSTS eax
#define Y edx
    mov CONSTS,offset OC_IDCT_CONSTS
    mov Y,_y
#define OC_I(_k)      [Y+_k*16]
#define OC_J(_k)      [Y+(_k-4)*16+8]
    OC_ROW_IDCT
    OC_TRANSPOSE
#undef  OC_I
#undef  OC_J
#define OC_I(_k)      [Y+(_k*16)+64]
#define OC_J(_k)      [Y+(_k-4)*16+72]
    OC_ROW_IDCT
    OC_TRANSPOSE
#undef  OC_I
#undef  OC_J
#define OC_I(_k)      [Y+_k*16]
#define OC_J(_k)      OC_I(_k)
    OC_COLUMN_IDCT
#undef  OC_I
#undef  OC_J
#define OC_I(_k)      [Y+_k*16+8]
#define OC_J(_k)      OC_I(_k)
    OC_COLUMN_IDCT
#undef  OC_I
#undef  OC_J
#undef  CONSTS
#undef  Y
  }
}

/*25 cycles.*/
#define OC_IDCT_BEGIN_10 __asm{ \
  __asm movq mm2,OC_I(3) \
  __asm nop \
  __asm movq mm6,OC_C(3) \
  __asm movq mm4,mm2 \
  __asm movq mm1,OC_C(5) \
  __asm pmulhw mm4,mm6 \
  __asm movq mm3,OC_I(1) \
  __asm pmulhw mm1,mm2 \
  __asm movq mm0,OC_C(1) \
  __asm paddw mm4,mm2 \
  __asm pxor mm6,mm6 \
  __asm paddw mm2,mm1 \
  __asm movq mm5,OC_I(2) \
  __asm pmulhw mm0,mm3 \
  __asm movq mm1,mm5 \
  __asm paddw mm0,mm3 \
  __asm pmulhw mm3,OC_C(7) \
  __asm psubw mm6,mm2 \
  __asm pmulhw mm5,OC_C(2) \
  __asm psubw mm0,mm4 \
  __asm movq mm7,OC_I(2) \
  __asm paddw mm4,mm4 \
  __asm paddw mm7,mm5 \
  __asm paddw mm4,mm0 \
  __asm pmulhw mm1,OC_C(6) \
  __asm psubw mm3,mm6 \
  __asm movq OC_I(1),mm4 \
  __asm paddw mm6,mm6 \
  __asm movq mm4,OC_C(4) \
  __asm paddw mm6,mm3 \
  __asm movq mm5,mm3 \
  __asm pmulhw mm3,mm4 \
  __asm movq OC_I(2),mm6 \
  __asm movq mm2,mm0 \
  __asm movq mm6,OC_I(0) \
  __asm pmulhw mm0,mm4 \
  __asm paddw mm5,mm3 \
  __asm paddw mm2,mm0 \
  __asm psubw mm5,mm1 \
  __asm pmulhw mm6,mm4 \
  __asm paddw mm6,OC_I(0) \
  __asm paddw mm1,mm1 \
  __asm movq mm4,mm6 \
  __asm paddw mm1,mm5 \
  __asm psubw mm6,mm2 \
  __asm paddw mm2,mm2 \
  __asm movq mm0,OC_I(1) \
  __asm paddw mm2,mm6 \
  __asm psubw mm2,mm1 \
  __asm nop \
}

/*25+8=33 cycles.*/
#define OC_ROW_IDCT_10 __asm{ \
  OC_IDCT_BEGIN_10 \
  /*r3=D'*/ \
   __asm movq mm3,OC_I(2) \
  /*r4=E'=E-G*/ \
   __asm psubw mm4,mm7 \
  /*r1=H'+H'*/ \
   __asm paddw mm1,mm1 \
  /*r7=G+G*/ \
   __asm paddw mm7,mm7 \
  /*r1=R1=A''+H'*/ \
   __asm paddw mm1,mm2 \
  /*r7=G'=E+G*/ \
   __asm paddw mm7,mm4 \
  /*r4=R4=E'-D'*/ \
   __asm psubw mm4,mm3 \
   __asm paddw mm3,mm3 \
  /*r6=R6=F'-B''*/ \
   __asm psubw mm6,mm5 \
   __asm paddw mm5,mm5 \
  /*r3=R3=E'+D'*/ \
   __asm paddw mm3,mm4 \
  /*r5=R5=F'+B''*/ \
   __asm paddw mm5,mm6 \
  /*r7=R7=G'-C'*/ \
   __asm psubw mm7,mm0 \
   __asm paddw mm0,mm0 \
  /*Save R1.*/ \
   __asm movq OC_I(1),mm1 \
  /*r0=R0=G'+C'*/ \
   __asm paddw mm0,mm7 \
}

/*25+19=44 cycles'*/
#define OC_COLUMN_IDCT_10 __asm{ \
  OC_IDCT_BEGIN_10 \
  __asm paddw mm2,OC_8 \
  /*r1=H'+H'*/ \
  __asm paddw mm1,mm1 \
  /*r1=R1=A''+H'*/ \
  __asm paddw mm1,mm2 \
  /*r2=NR2*/ \
  __asm psraw mm2,4 \
  /*r4=E'=E-G*/ \
  __asm psubw mm4,mm7 \
  /*r1=NR1*/ \
  __asm psraw mm1,4 \
  /*r3=D'*/ \
  __asm movq mm3,OC_I(2) \
  /*r7=G+G*/ \
  __asm paddw mm7,mm7 \
  /*Store NR2 at I(2).*/ \
  __asm movq OC_I(2),mm2 \
  /*r7=G'=E+G*/ \
  __asm paddw mm7,mm4 \
  /*Store NR1 at I(1).*/ \
  __asm movq OC_I(1),mm1 \
  /*r4=R4=E'-D'*/ \
  __asm psubw mm4,mm3 \
  __asm paddw mm4,OC_8 \
  /*r3=D'+D'*/ \
  __asm paddw mm3,mm3 \
  /*r3=R3=E'+D'*/ \
  __asm paddw mm3,mm4 \
  /*r4=NR4*/ \
  __asm psraw mm4,4 \
  /*r6=R6=F'-B''*/ \
  __asm psubw mm6,mm5 \
  /*r3=NR3*/ \
  __asm psraw mm3,4 \
  __asm paddw mm6,OC_8 \
  /*r5=B''+B''*/ \
  __asm paddw mm5,mm5 \
  /*r5=R5=F'+B''*/ \
  __asm paddw mm5,mm6 \
  /*r6=NR6*/ \
  __asm psraw mm6,4 \
  /*Store NR4 at J(4).*/ \
  __asm movq OC_J(4),mm4 \
  /*r5=NR5*/ \
  __asm psraw mm5,4 \
  /*Store NR3 at I(3).*/ \
  __asm movq OC_I(3),mm3 \
  /*r7=R7=G'-C'*/ \
  __asm psubw mm7,mm0 \
  __asm paddw mm7,OC_8 \
  /*r0=C'+C'*/ \
  __asm paddw mm0,mm0 \
  /*r0=R0=G'+C'*/ \
  __asm paddw mm0,mm7 \
  /*r7=NR7*/ \
  __asm psraw mm7,4 \
  /*Store NR6 at J(6).*/ \
  __asm movq OC_J(6),mm6 \
  /*r0=NR0*/ \
  __asm psraw mm0,4 \
  /*Store NR5 at J(5).*/ \
  __asm movq OC_J(5),mm5 \
  /*Store NR7 at J(7).*/ \
  __asm movq OC_J(7),mm7 \
  /*Store NR0 at I(0).*/ \
  __asm movq OC_I(0),mm0 \
}

static void oc_idct8x8_10(ogg_int16_t _y[64]){
  __asm{
#define CONSTS eax
#define Y edx
    mov CONSTS,offset OC_IDCT_CONSTS
    mov Y,_y
#define OC_I(_k) [Y+_k*16]
#define OC_J(_k) [Y+(_k-4)*16+8]
    /*Done with dequant, descramble, and partial transpose.
      Now do the iDCT itself.*/
    OC_ROW_IDCT_10
    OC_TRANSPOSE
#undef  OC_I
#undef  OC_J
#define OC_I(_k) [Y+_k*16]
#define OC_J(_k) OC_I(_k)
    OC_COLUMN_IDCT_10
#undef  OC_I
#undef  OC_J
#define OC_I(_k) [Y+_k*16+8]
#define OC_J(_k) OC_I(_k)
    OC_COLUMN_IDCT_10
#undef  OC_I
#undef  OC_J
#undef  CONSTS
#undef  Y
  }
}

/*This table has been modified from OC_FZIG_ZAG by baking a 4x4 transpose into
   each quadrant of the destination.*/
static const unsigned char OC_FZIG_ZAG_MMX[64]={
   0, 8, 1, 2, 9,16,24,17,
  10, 3,32,11,18,25, 4,12,
   5,26,19,40,33,34,41,48,
  27, 6,13,20,28,21,14, 7,
  56,49,42,35,43,50,57,36,
  15,22,29,30,23,44,37,58,
  51,59,38,45,52,31,60,53,
  46,39,47,54,61,62,55,63
};

/*Performs an inverse 8x8 Type-II DCT transform.
  The input is assumed to be scaled by a factor of 4 relative to orthonormal
   version of the transform.
  _y: The buffer to store the result in.
      This must not be the same as _x.
  _x: The input coefficients.*/
void oc_dequant_idct8x8_mmx(ogg_int16_t _y[64],const ogg_int16_t _x[64],
 int _last_zzi,int _ncoefs,ogg_uint16_t _dc_quant,
 const ogg_uint16_t _ac_quant[64]){
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
    /*Note that this value must be unsigned, to keep the __asm__ block from
       sign-extending it when it puts it in a register.*/
    ogg_uint16_t p;
    /*We round this dequant product (and not any of the others) because there's
       no iDCT rounding.*/
    p=(ogg_int16_t)(_x[0]*(ogg_int32_t)_dc_quant+15>>5);
    /*Fill _y with p.*/
    __asm{
#define Y eax
#define P ecx
      mov Y,_y
      movd P,p
      /*mm0=0000 0000 0000 AAAA*/
      movd mm0,P
      /*mm0=0000 0000 AAAA AAAA*/
      punpcklwd mm0,mm0
      /*mm0=AAAA AAAA AAAA AAAA*/
      punpckldq mm0,mm0
      movq [Y],mm0
      movq [8+Y],mm0
      movq [16+Y],mm0
      movq [24+Y],mm0
      movq [32+Y],mm0
      movq [40+Y],mm0
      movq [48+Y],mm0
      movq [56+Y],mm0
      movq [64+Y],mm0
      movq [72+Y],mm0
      movq [80+Y],mm0
      movq [88+Y],mm0
      movq [96+Y],mm0
      movq [104+Y],mm0
      movq [112+Y],mm0
      movq [120+Y],mm0
#undef Y
#undef P
    }
  }
  else{
    int zzi;
    /*First zero the buffer.*/
    /*On K7, etc., this could be replaced with movntq and sfence.*/
    __asm{
#define Y eax
      mov Y,_y
      pxor mm0,mm0
      movq [Y],mm0
      movq [8+Y],mm0
      movq [16+Y],mm0
      movq [24+Y],mm0
      movq [32+Y],mm0
      movq [40+Y],mm0
      movq [48+Y],mm0
      movq [56+Y],mm0
      movq [64+Y],mm0
      movq [72+Y],mm0
      movq [80+Y],mm0
      movq [88+Y],mm0
      movq [96+Y],mm0
      movq [104+Y],mm0
      movq [112+Y],mm0
      movq [120+Y],mm0
#undef Y
    }
    /*Dequantize the coefficients.*/
    _y[0]=(ogg_int16_t)(_x[0]*(int)_dc_quant);
    for(zzi=1;zzi<_ncoefs;zzi++){
      _y[OC_FZIG_ZAG_MMX[zzi]]=(ogg_int16_t)(_x[zzi]*(int)_ac_quant[zzi]);
    }
    /*Then perform the iDCT.*/
    if(_last_zzi<10)oc_idct8x8_10(_y);
    else oc_idct8x8_slow(_y);
  }
}

#endif
