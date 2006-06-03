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
 ********************************************************************/


#include <stdlib.h>

#include "codec_internal.h"
#include "dsp.h"

#if 0
//These are to let me selectively enable the C versions, these are needed
#define DSP_OP_AVG(a,b) ((((int)(a)) + ((int)(b)))/2)
#define DSP_OP_DIFF(a,b) (((int)(a)) - ((int)(b)))
#define DSP_OP_ABS_DIFF(a,b) abs((((int)(a)) - ((int)(b))))
#endif


//static const ogg_int64_t V128 = 0x0080008000800080LL;

static __declspec(align(16)) const unsigned int V128_8x16bits[4] = { 0x00800080, 0x00800080, 0x00800080, 0x00800080 };
static const unsigned int* V128_8x16bitsPtr = V128_8x16bits;

static void sub8x8__sse2 (unsigned char *FiltPtr, unsigned char *ReconPtr,
                  ogg_int16_t *DctInputPtr, ogg_uint32_t PixelsPerLine,
                  ogg_uint32_t ReconPixelsPerLine) 
{

    //Make non-zero to use the C-version
#if 0
  int i;

  /* For each block row */
  for (i=8; i; i--) {
    DctInputPtr[0] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[0], ReconPtr[0]);
    DctInputPtr[1] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[1], ReconPtr[1]);
    DctInputPtr[2] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[2], ReconPtr[2]);
    DctInputPtr[3] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[3], ReconPtr[3]);
    DctInputPtr[4] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[4], ReconPtr[4]);
    DctInputPtr[5] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[5], ReconPtr[5]);
    DctInputPtr[6] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[6], ReconPtr[6]);
    DctInputPtr[7] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[7], ReconPtr[7]);

    /* Start next row */
    FiltPtr += PixelsPerLine;
    ReconPtr += ReconPixelsPerLine;
    DctInputPtr += 8;
  }
#else

    __asm {
        align 16

        pxor		xmm0, xmm0	

        mov     eax, FiltPtr
        mov     ebx, ReconPtr
        mov     edi, DctInputPtr
        mov     ecx, PixelsPerLine
        mov     edx, ReconPixelsPerLine

        /* ITERATION 1 */
        movq    xmm1, QWORD PTR [eax]     ;       /*  xmm1_L = FiltPtr[0..7] */
        movq    xmm2, QWORD PTR [ebx]     ;       /*  xmm2_L = ReconPtr[0..7] */
        punpcklbw   xmm1, xmm0  ;       /*  xmm1 = INT16(FiltPtr[0..7]) */
        punpcklbw   xmm2, xmm0  ;       /*  xmm2 = INT16(ReconPtr[0..7]) */
        psubw       xmm1, xmm2  ;       /*  xmm1 -= xmm2 {int16wise} */



        /* Increment pointers */
        add		eax, ecx		
        add		ebx, edx

        /* ITERATION 2 */
        movq    xmm2, QWORD PTR [eax]     
        movq    xmm3, QWORD PTR [ebx]     
        punpcklbw   xmm2, xmm0  
        punpcklbw   xmm3, xmm0  
        psubw       xmm2, xmm3  



        /* Increment pointers */
        add		eax, ecx		
        add		ebx, edx

         /* ITERATION 3 */
        movq    xmm3, QWORD PTR [eax]     
        movq    xmm4, QWORD PTR [ebx]     
        punpcklbw   xmm3, xmm0  
        punpcklbw   xmm4, xmm0  
        psubw       xmm3, xmm4  



        /* Increment pointers */
        add		eax, ecx		
        add		ebx, edx

        /* ITERATION 4 */
        movq    xmm4, QWORD PTR [eax]     
        movq    xmm5, QWORD PTR [ebx]     
        punpcklbw   xmm4, xmm0  
        punpcklbw   xmm5, xmm0  
        psubw       xmm4, xmm5  


        /* Write out the first 4 iterations */
        movdqa      [edi], xmm1
        movdqa      [edi + 16], xmm2
        movdqa      [edi + 32], xmm3
        movdqa      [edi + 48], xmm4

        add         edi, 64


        /* Increment pointers */
        add		eax, ecx		
        add		ebx, edx


        /* ---------------------- Repeat of above -------------------- */

        /* ITERATION 1 */
        movq    xmm1, QWORD PTR [eax]     ;       /*  xmm1_L = FiltPtr[0..7] */
        movq    xmm2, QWORD PTR [ebx]     ;       /*  xmm2_L = ReconPtr[0..7] */
        punpcklbw   xmm1, xmm0  ;       /*  xmm1 = INT16(FiltPtr[0..7]) */
        punpcklbw   xmm2, xmm0  ;       /*  xmm2 = INT16(ReconPtr[0..7]) */
        psubw       xmm1, xmm2  ;       /*  xmm1 -= xmm2 {int16wise} */



        /* Increment pointers */
        add		eax, ecx		
        add		ebx, edx

        /* ITERATION 2 */
        movq    xmm2, QWORD PTR [eax]     
        movq    xmm3, QWORD PTR [ebx]     
        punpcklbw   xmm2, xmm0  
        punpcklbw   xmm3, xmm0  
        psubw       xmm2, xmm3  



        /* Increment pointers */
        add		eax, ecx		
        add		ebx, edx

         /* ITERATION 3 */
        movq    xmm3, QWORD PTR [eax]     
        movq    xmm4, QWORD PTR [ebx]     
        punpcklbw   xmm3, xmm0  
        punpcklbw   xmm4, xmm0  
        psubw       xmm3, xmm4  



        /* Increment pointers */
        add		eax, ecx		
        add		ebx, edx

        /* ITERATION 4 */
        movq    xmm4, QWORD PTR [eax]     
        movq    xmm5, QWORD PTR [ebx]     
        punpcklbw   xmm4, xmm0  
        punpcklbw   xmm5, xmm0  
        psubw       xmm4, xmm5  


        /* Write out the first 4 iterations */
        movdqa      [edi], xmm1
        movdqa      [edi + 16], xmm2
        movdqa      [edi + 32], xmm3
        movdqa      [edi + 48], xmm4


    };


#endif
}

static void sub8x8_128__sse2 (unsigned char *FiltPtr, ogg_int16_t *DctInputPtr,
                      ogg_uint32_t PixelsPerLine) 
{

#if 0
  int i;
  /* For each block row */
  for (i=8; i; i--) {
    /* INTRA mode so code raw image data */
    /* We convert the data to 8 bit signed (by subtracting 128) as
       this reduces the internal precision requirments in the DCT
       transform. */
    DctInputPtr[0] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[0], 128);
    DctInputPtr[1] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[1], 128);
    DctInputPtr[2] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[2], 128);
    DctInputPtr[3] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[3], 128);
    DctInputPtr[4] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[4], 128);
    DctInputPtr[5] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[5], 128);
    DctInputPtr[6] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[6], 128);
    DctInputPtr[7] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[7], 128);

    /* Start next row */
    FiltPtr += PixelsPerLine;
    DctInputPtr += 8;
  }

#else
    __asm {
        align 16
        
        pxor		xmm0, xmm0	
        mov         edx, V128_8x16bitsPtr
        movdqa      xmm7, [edx]

        /* Setup the parameters */
        mov         esi, FiltPtr
        mov         edi, DctInputPtr
        mov         eax, PixelsPerLine
        lea         ebx, [eax + eax*2]      ;   /* ebx = 3 * PixelsPerLine */

        /* 
            Read the first 4 lots of 8x8bits from FiltPtr into the 
             low 64 bits of the registers. Then expand out into
             8x16bits to fill all 128 bits of the register        
        */
        movq        xmm1, QWORD PTR [esi]
        movq        xmm2, QWORD PTR [esi + eax]
        movq        xmm3, QWORD PTR [esi + eax * 2]
        movq        xmm4, QWORD PTR [esi + ebx]

        punpcklbw   xmm1, xmm0
        punpcklbw   xmm2, xmm0
        punpcklbw   xmm3, xmm0
        punpcklbw   xmm4, xmm0

        /* Subtract 128 16bitwise and write*/
        psubw       xmm1, xmm7
        movdqa      [edi], xmm1
        psubw       xmm2, xmm7
        movdqa      [edi + 16], xmm2
        psubw       xmm3, xmm7
        movdqa      [edi + 32], xmm3
        psubw       xmm4, xmm7
        movdqa      [edi + 48], xmm4




        /* Advance the source and dest pointer for the next 4 iterations */
        lea         esi, [esi + eax * 4]
        add         edi, 64




        /* Repeat of above for second round */
        movq        xmm1, QWORD PTR [esi]
        movq        xmm2, QWORD PTR [esi + eax]
        movq        xmm3, QWORD PTR [esi + eax * 2]
        movq        xmm4, QWORD PTR [esi + ebx]

        punpcklbw   xmm1, xmm0
        punpcklbw   xmm2, xmm0
        punpcklbw   xmm3, xmm0
        punpcklbw   xmm4, xmm0

        /* Subtract 128 16bitwise and write*/
        psubw       xmm1, xmm7
        movdqa      [edi], xmm1
        psubw       xmm2, xmm7
        movdqa      [edi + 16], xmm2
        psubw       xmm3, xmm7
        movdqa      [edi + 32], xmm3
        psubw       xmm4, xmm7
        movdqa      [edi + 48], xmm4

    };

 
#endif
}




static void sub8x8avg2__sse2 (unsigned char *FiltPtr, unsigned char *ReconPtr1,
                     unsigned char *ReconPtr2, ogg_int16_t *DctInputPtr,
                     ogg_uint32_t PixelsPerLine,
                     ogg_uint32_t ReconPixelsPerLine) 
{

#if 0
  int i;

  /* For each block row */
  for (i=8; i; i--) {
    DctInputPtr[0] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[0], DSP_OP_AVG (ReconPtr1[0], ReconPtr2[0]));
    DctInputPtr[1] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[1], DSP_OP_AVG (ReconPtr1[1], ReconPtr2[1]));
    DctInputPtr[2] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[2], DSP_OP_AVG (ReconPtr1[2], ReconPtr2[2]));
    DctInputPtr[3] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[3], DSP_OP_AVG (ReconPtr1[3], ReconPtr2[3]));
    DctInputPtr[4] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[4], DSP_OP_AVG (ReconPtr1[4], ReconPtr2[4]));
    DctInputPtr[5] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[5], DSP_OP_AVG (ReconPtr1[5], ReconPtr2[5]));
    DctInputPtr[6] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[6], DSP_OP_AVG (ReconPtr1[6], ReconPtr2[6]));
    DctInputPtr[7] = (ogg_int16_t) DSP_OP_DIFF (FiltPtr[7], DSP_OP_AVG (ReconPtr1[7], ReconPtr2[7]));

    /* Start next row */
    FiltPtr += PixelsPerLine;
    ReconPtr1 += ReconPixelsPerLine;
    ReconPtr2 += ReconPixelsPerLine;
    DctInputPtr += 8;
  }
#else
    __asm {
        align 16

        pxor        xmm0, xmm0

        /* Setup input params */
        mov         eax, ReconPtr1
        mov         ebx, ReconPtr2
        mov         ecx, PixelsPerLine
        mov         edx, ReconPixelsPerLine
        mov         esi, FiltPtr
        mov         edi, DctInputPtr


        /* ITERATION 1&2 */

        /* Read 2 iterations worth of the input arrays */
        movq        xmm1, QWORD PTR [eax]
        movq        xmm2, QWORD PTR [eax + edx]
        movq        xmm3, QWORD PTR [ebx]
        movq        xmm4, QWORD PTR [ebx + edx]
        movq        xmm5, QWORD PTR [esi]
        movq        xmm6, QWORD PTR [esi + ecx]

        /* Extend out to int16's */
        punpcklbw   xmm1, xmm0
        punpcklbw   xmm2, xmm0
        punpcklbw   xmm3, xmm0
        punpcklbw   xmm4, xmm0
        punpcklbw   xmm5, xmm0
        punpcklbw   xmm6, xmm0

        /* Average ReconPtr1 and 2 */
        paddw       xmm1, xmm3
        paddw       xmm2, xmm4
        psrlw       xmm1, 1
        psrlw       xmm2, 1

        /* Do Result = FilterPtr[i] - avg(ReconPtr[i], ReconPtr[i]) */
        psubw       xmm5, xmm1
        psubw       xmm6, xmm2

        /* Write out two iterations worth */
        movdqa      [edi], xmm5
        movdqa      [edi + 16], xmm6

        /* Update pointers */
        lea         eax, [eax + edx*2]
        lea         ebx, [ebx + edx*2]
        lea         esi, [esi + ecx*2]
        add         edi, 32


        /* ITERATION 3&4 */

        /* Read 2 iterations worth of the input arrays */
        movq        xmm1, QWORD PTR [eax]
        movq        xmm2, QWORD PTR [eax + edx]
        movq        xmm3, QWORD PTR [ebx]
        movq        xmm4, QWORD PTR [ebx + edx]
        movq        xmm5, QWORD PTR [esi]
        movq        xmm6, QWORD PTR [esi + ecx]

        /* Extend out to int16's */
        punpcklbw   xmm1, xmm0
        punpcklbw   xmm2, xmm0
        punpcklbw   xmm3, xmm0
        punpcklbw   xmm4, xmm0
        punpcklbw   xmm5, xmm0
        punpcklbw   xmm6, xmm0

        /* Average ReconPtr1 and 2 */
        paddw       xmm1, xmm3
        paddw       xmm2, xmm4
        psrlw       xmm1, 1
        psrlw       xmm2, 1

        /* Do Result = FilterPtr[i] - avg(ReconPtr[i], ReconPtr[i]) */
        psubw       xmm5, xmm1
        psubw       xmm6, xmm2

        /* Write out two iterations worth */
        movdqa      [edi], xmm5
        movdqa      [edi + 16], xmm6

        /* Update pointers */
        lea         eax, [eax + edx*2]
        lea         ebx, [ebx + edx*2]
        lea         esi, [esi + ecx*2]
        add         edi, 32


        
        /* ITERATION 5&6 */

        /* Read 2 iterations worth of the input arrays */
        movq        xmm1, QWORD PTR [eax]
        movq        xmm2, QWORD PTR [eax + edx]
        movq        xmm3, QWORD PTR [ebx]
        movq        xmm4, QWORD PTR [ebx + edx]
        movq        xmm5, QWORD PTR [esi]
        movq        xmm6, QWORD PTR [esi + ecx]

        /* Extend out to int16's */
        punpcklbw   xmm1, xmm0
        punpcklbw   xmm2, xmm0
        punpcklbw   xmm3, xmm0
        punpcklbw   xmm4, xmm0
        punpcklbw   xmm5, xmm0
        punpcklbw   xmm6, xmm0

        /* Average ReconPtr1 and 2 */
        paddw       xmm1, xmm3
        paddw       xmm2, xmm4
        psrlw       xmm1, 1
        psrlw       xmm2, 1

        /* Do Result = FilterPtr[i] - avg(ReconPtr[i], ReconPtr[i]) */
        psubw       xmm5, xmm1
        psubw       xmm6, xmm2

        /* Write out two iterations worth */
        movdqa      [edi], xmm5
        movdqa      [edi + 16], xmm6

        /* Update pointers */
        lea         eax, [eax + edx*2]
        lea         ebx, [ebx + edx*2]
        lea         esi, [esi + ecx*2]
        add         edi, 32
        



        /* ITERATION 7&8 */

        /* Read 2 iterations worth of the input arrays */
        movq        xmm1, QWORD PTR [eax]
        movq        xmm2, QWORD PTR [eax + edx]
        movq        xmm3, QWORD PTR [ebx]
        movq        xmm4, QWORD PTR [ebx + edx]
        movq        xmm5, QWORD PTR [esi]
        movq        xmm6, QWORD PTR [esi + ecx]

        /* Extend out to int16's */
        punpcklbw   xmm1, xmm0
        punpcklbw   xmm2, xmm0
        punpcklbw   xmm3, xmm0
        punpcklbw   xmm4, xmm0
        punpcklbw   xmm5, xmm0
        punpcklbw   xmm6, xmm0

        /* Average ReconPtr1 and 2 */
        paddw       xmm1, xmm3
        paddw       xmm2, xmm4
        psrlw       xmm1, 1
        psrlw       xmm2, 1

        /* Do Result = FilterPtr[i] - avg(ReconPtr[i], ReconPtr[i]) */
        psubw       xmm5, xmm1
        psubw       xmm6, xmm2

        /* Write out two iterations worth */
        movdqa      [edi], xmm5
        movdqa      [edi + 16], xmm6

        /* Update pointers */
        //lea         eax, [eax + edx*2]
        //lea         ebx, [ebx + edx*2]
        //lea         esi, [esi + ecx*2]
        //add         edi, 32
};        

 

 
#endif
}

static ogg_uint32_t row_sad8__sse2 (unsigned char *Src1, unsigned char *Src2)
{

#if 0
  ogg_uint32_t SadValue;
  ogg_uint32_t SadValue1;

  SadValue    = DSP_OP_ABS_DIFF (Src1[0], Src2[0]) + 
	        DSP_OP_ABS_DIFF (Src1[1], Src2[1]) +
	        DSP_OP_ABS_DIFF (Src1[2], Src2[2]) +
	        DSP_OP_ABS_DIFF (Src1[3], Src2[3]);

  SadValue1   = DSP_OP_ABS_DIFF (Src1[4], Src2[4]) + 
	        DSP_OP_ABS_DIFF (Src1[5], Src2[5]) +
	        DSP_OP_ABS_DIFF (Src1[6], Src2[6]) +
	        DSP_OP_ABS_DIFF (Src1[7], Src2[7]);

  SadValue = ( SadValue > SadValue1 ) ? SadValue : SadValue1;

  return SadValue;

#else
  ogg_uint32_t MaxSad;

  
  __asm {
    align       16
    mov         ebx, Src1
    mov         ecx, Src2


    pxor		mm6, mm6		;	/* zero out mm6 for unpack */
    pxor		mm7, mm7		;	/* zero out mm7 for unpack */
    movq		mm0, [ebx]		;	/* take 8 bytes */
    movq		mm1, [ecx]		;	

    movq		mm2, mm0		;	
    psubusb		mm0, mm1		;	/* A - B */
    psubusb		mm1, mm2		;	/* B - A */
    por		mm0, mm1		;	/* and or gives abs difference */

    movq		mm1, mm0		;	

    punpcklbw		mm0, mm6		;	/* ; unpack low four bytes to higher precision */
    punpckhbw		mm1, mm7		;	/* ; unpack high four bytes to higher precision */

    movq		mm2, mm0		;	
    movq		mm3, mm1		;	
    psrlq		mm2, 32		;	/* fold and add */
    psrlq		mm3, 32		;	
    paddw		mm0, mm2		;	
    paddw		mm1, mm3		;	
    movq		mm2, mm0		;	
    movq		mm3, mm1		;	
    psrlq		mm2, 16		;	
    psrlq		mm3, 16		;	
    paddw		mm0, mm2		;	
    paddw		mm1, mm3		;	

    psubusw		mm1, mm0		;	
    paddw		mm1, mm0		;	/* mm1 = max(mm1, mm0) */
    movd		eax, mm1		;

    and         eax, 0xffff
    mov         MaxSad, eax
  };
   return MaxSad;
  
  
  
 

#endif
}




static ogg_uint32_t col_sad8x8__sse2 (unsigned char *Src1, unsigned char *Src2,
		                    ogg_uint32_t stride)
{

#if 0
  ogg_uint32_t SadValue[8] = {0,0,0,0,0,0,0,0};
  ogg_uint32_t SadValue2[8] = {0,0,0,0,0,0,0,0};
  ogg_uint32_t MaxSad = 0;
  ogg_uint32_t i;

  for ( i = 0; i < 4; i++ ){
    SadValue[0] += abs(Src1[0] - Src2[0]);
    SadValue[1] += abs(Src1[1] - Src2[1]);
    SadValue[2] += abs(Src1[2] - Src2[2]);
    SadValue[3] += abs(Src1[3] - Src2[3]);
    SadValue[4] += abs(Src1[4] - Src2[4]);
    SadValue[5] += abs(Src1[5] - Src2[5]);
    SadValue[6] += abs(Src1[6] - Src2[6]);
    SadValue[7] += abs(Src1[7] - Src2[7]);
    
    Src1 += stride;
    Src2 += stride;
  }

  for ( i = 0; i < 4; i++ ){
    SadValue2[0] += abs(Src1[0] - Src2[0]);
    SadValue2[1] += abs(Src1[1] - Src2[1]);
    SadValue2[2] += abs(Src1[2] - Src2[2]);
    SadValue2[3] += abs(Src1[3] - Src2[3]);
    SadValue2[4] += abs(Src1[4] - Src2[4]);
    SadValue2[5] += abs(Src1[5] - Src2[5]);
    SadValue2[6] += abs(Src1[6] - Src2[6]);
    SadValue2[7] += abs(Src1[7] - Src2[7]);
    
    Src1 += stride;
    Src2 += stride;
  }
    
  for ( i = 0; i < 8; i++ ){
    if ( SadValue[i] > MaxSad )
      MaxSad = SadValue[i];
    if ( SadValue2[i] > MaxSad )
      MaxSad = SadValue2[i];
  }
    
  return MaxSad;
#else
  ogg_uint32_t MaxSad;


    __asm {
        align       16
        mov         ebx, Src1
        mov         ecx, Src2

        pxor		mm3, mm3		;	/* zero out mm3 for unpack */
        pxor		mm4, mm4		;	/* mm4 low sum */
        pxor		mm5, mm5		;	/* mm5 high sum */
        pxor		mm6, mm6		;	/* mm6 low sum */
        pxor		mm7, mm7		;	/* mm7 high sum */
        mov		edi, 4		;	/* 4 rows */
        label_1:				;	
        movq		mm0, [ebx]		;	/* take 8 bytes */
        movq		mm1, [ecx]		;	/* take 8 bytes */

        movq		mm2, mm0		;	
        psubusb		mm0, mm1		;	/* A - B */
        psubusb		mm1, mm2		;	/* B - A */
        por		mm0, mm1		;	/* and or gives abs difference */
        movq		mm1, mm0		;	

        punpcklbw		mm0, mm3		;	/* unpack to higher precision for accumulation */
        paddw		mm4, mm0		;	/* accumulate difference... */
        punpckhbw		mm1, mm3		;	/* unpack high four bytes to higher precision */
        paddw		mm5, mm1		;	/* accumulate difference... */
        add		ebx, stride		;	/* Inc pointer into the new data */
        add		ecx, stride		;	/* Inc pointer into the new data */

        dec		edi		;	
        jnz		label_1		;	

        mov		edi, 4		;	/* 4 rows */
        label_2:				;	
        movq		mm0, [ebx]		;	/* take 8 bytes */
        movq		mm1, [ecx]		;	/* take 8 bytes */

        movq		mm2, mm0		;	
        psubusb		mm0, mm1		;	/* A - B */
        psubusb		mm1, mm2		;	/* B - A */
        por		mm0, mm1		;	/* and or gives abs difference */
        movq		mm1, mm0		;	

        punpcklbw		mm0, mm3		;	/* unpack to higher precision for accumulation */
        paddw		mm6, mm0		;	/* accumulate difference... */
        punpckhbw		mm1, mm3		;	/* unpack high four bytes to higher precision */
        paddw		mm7, mm1		;	/* accumulate difference... */
        add		ebx, stride		;	/* Inc pointer into the new data */
        add		ecx, stride		;	/* Inc pointer into the new data */

        dec		edi		;	
        jnz		label_2		;	

        psubusw		mm7, mm6		;	
        paddw		mm7, mm6		;	/* mm7 = max(mm7, mm6) */
        psubusw		mm5, mm4		;	
        paddw		mm5, mm4		;	/* mm5 = max(mm5, mm4) */
        psubusw		mm7, mm5		;	
        paddw		mm7, mm5		;	/* mm7 = max(mm5, mm7) */
        movq		mm6, mm7		;	
        psrlq		mm6, 32		;	
        psubusw		mm7, mm6		;	
        paddw		mm7, mm6		;	/* mm7 = max(mm5, mm7) */
        movq		mm6, mm7		;	
        psrlq		mm6, 16		;	
        psubusw		mm7, mm6		;	
        paddw		mm7, mm6		;	/* mm7 = max(mm5, mm7) */
        movd		eax, mm7		;	
        and		    eax, 0xffff		;

        mov         MaxSad, eax
    };

    return MaxSad;


#endif
}

static ogg_uint32_t sad8x8__sse2 (unsigned char *ptr1, ogg_uint32_t stride1,
		       	    unsigned char *ptr2, ogg_uint32_t stride2)
{

#if 0
  ogg_uint32_t  i;
  ogg_uint32_t  sad = 0;

  for (i=8; i; i--) {
    sad += DSP_OP_ABS_DIFF(ptr1[0], ptr2[0]);
    sad += DSP_OP_ABS_DIFF(ptr1[1], ptr2[1]);
    sad += DSP_OP_ABS_DIFF(ptr1[2], ptr2[2]);
    sad += DSP_OP_ABS_DIFF(ptr1[3], ptr2[3]);
    sad += DSP_OP_ABS_DIFF(ptr1[4], ptr2[4]);
    sad += DSP_OP_ABS_DIFF(ptr1[5], ptr2[5]);
    sad += DSP_OP_ABS_DIFF(ptr1[6], ptr2[6]);
    sad += DSP_OP_ABS_DIFF(ptr1[7], ptr2[7]);

    /* Step to next row of block. */
    ptr1 += stride1;
    ptr2 += stride2;
  }

  return sad;
#else
  ogg_uint32_t  DiffVal;

  __asm {
    align  16

    mov         ebx, ptr1
    mov         edx, ptr2

    pxor		mm6, mm6		;	/* zero out mm6 for unpack */
    pxor		mm7, mm7		;	/* mm7 contains the result */
    
    ; /* ITERATION 1 */
    movq		mm0, [ebx]		;	/* take 8 bytes */
    movq		mm1, [edx]		;	
    movq		mm2, mm0		;	

    psubusb		mm0, mm1		;	/* A - B */
    psubusb		mm1, mm2		;	/* B - A */
    por		mm0, mm1		;	/* and or gives abs difference */
    movq		mm1, mm0		;	

    punpcklbw		mm0, mm6		;	/* unpack to higher precision for accumulation */
    paddw		mm7, mm0		;	/* accumulate difference... */
    punpckhbw		mm1, mm6		;	/* unpack high four bytes to higher precision */
    add		ebx, stride1		;	/* Inc pointer into the new data */
    paddw		mm7, mm1		;	/* accumulate difference... */
    add		edx, stride2		;	/* Inc pointer into ref data */

    ; /* ITERATION 2 */
    movq		mm0, [ebx]		;	/* take 8 bytes */
    movq		mm1, [edx]		;	
    movq		mm2, mm0		;	

    psubusb		mm0, mm1		;	/* A - B */
    psubusb		mm1, mm2		;	/* B - A */
    por		mm0, mm1		;	/* and or gives abs difference */
    movq		mm1, mm0		;	

    punpcklbw		mm0, mm6		;	/* unpack to higher precision for accumulation */
    paddw		mm7, mm0		;	/* accumulate difference... */
    punpckhbw		mm1, mm6		;	/* unpack high four bytes to higher precision */
    add		ebx, stride1		;	/* Inc pointer into the new data */
    paddw		mm7, mm1		;	/* accumulate difference... */
    add		edx, stride2		;	/* Inc pointer into ref data */


    ; /* ITERATION 3 */
    movq		mm0, [ebx]		;	/* take 8 bytes */
    movq		mm1, [edx]		;	
    movq		mm2, mm0		;	

    psubusb		mm0, mm1		;	/* A - B */
    psubusb		mm1, mm2		;	/* B - A */
    por		mm0, mm1		;	/* and or gives abs difference */
    movq		mm1, mm0		;	

    punpcklbw		mm0, mm6		;	/* unpack to higher precision for accumulation */
    paddw		mm7, mm0		;	/* accumulate difference... */
    punpckhbw		mm1, mm6		;	/* unpack high four bytes to higher precision */
    add		ebx, stride1		;	/* Inc pointer into the new data */
    paddw		mm7, mm1		;	/* accumulate difference... */
    add		edx, stride2		;	/* Inc pointer into ref data */

    ; /* ITERATION 4 */
    movq		mm0, [ebx]		;	/* take 8 bytes */
    movq		mm1, [edx]		;	
    movq		mm2, mm0		;	

    psubusb		mm0, mm1		;	/* A - B */
    psubusb		mm1, mm2		;	/* B - A */
    por		mm0, mm1		;	/* and or gives abs difference */
    movq		mm1, mm0		;	

    punpcklbw		mm0, mm6		;	/* unpack to higher precision for accumulation */
    paddw		mm7, mm0		;	/* accumulate difference... */
    punpckhbw		mm1, mm6		;	/* unpack high four bytes to higher precision */
    add		ebx, stride1		;	/* Inc pointer into the new data */
    paddw		mm7, mm1		;	/* accumulate difference... */
    add		edx, stride2		;	/* Inc pointer into ref data */


    ; /* ITERATION 5 */
    movq		mm0, [ebx]		;	/* take 8 bytes */
    movq		mm1, [edx]		;	
    movq		mm2, mm0		;	

    psubusb		mm0, mm1		;	/* A - B */
    psubusb		mm1, mm2		;	/* B - A */
    por		mm0, mm1		;	/* and or gives abs difference */
    movq		mm1, mm0		;	

    punpcklbw		mm0, mm6		;	/* unpack to higher precision for accumulation */
    paddw		mm7, mm0		;	/* accumulate difference... */
    punpckhbw		mm1, mm6		;	/* unpack high four bytes to higher precision */
    add		ebx, stride1		;	/* Inc pointer into the new data */
    paddw		mm7, mm1		;	/* accumulate difference... */
    add		edx, stride2		;	/* Inc pointer into ref data */


    ; /* ITERATION 6 */
    movq		mm0, [ebx]		;	/* take 8 bytes */
    movq		mm1, [edx]		;	
    movq		mm2, mm0		;	

    psubusb		mm0, mm1		;	/* A - B */
    psubusb		mm1, mm2		;	/* B - A */
    por		mm0, mm1		;	/* and or gives abs difference */
    movq		mm1, mm0		;	

    punpcklbw		mm0, mm6		;	/* unpack to higher precision for accumulation */
    paddw		mm7, mm0		;	/* accumulate difference... */
    punpckhbw		mm1, mm6		;	/* unpack high four bytes to higher precision */
    add		ebx, stride1		;	/* Inc pointer into the new data */
    paddw		mm7, mm1		;	/* accumulate difference... */
    add		edx, stride2		;	/* Inc pointer into ref data */


    ; /* ITERATION 7 */
    movq		mm0, [ebx]		;	/* take 8 bytes */
    movq		mm1, [edx]		;	
    movq		mm2, mm0		;	

    psubusb		mm0, mm1		;	/* A - B */
    psubusb		mm1, mm2		;	/* B - A */
    por		mm0, mm1		;	/* and or gives abs difference */
    movq		mm1, mm0		;	

    punpcklbw		mm0, mm6		;	/* unpack to higher precision for accumulation */
    paddw		mm7, mm0		;	/* accumulate difference... */
    punpckhbw		mm1, mm6		;	/* unpack high four bytes to higher precision */
    add		ebx, stride1		;	/* Inc pointer into the new data */
    paddw		mm7, mm1		;	/* accumulate difference... */
    add		edx, stride2		;	/* Inc pointer into ref data */



    ; /* ITERATION 8 */
    movq		mm0, [ebx]		;	/* take 8 bytes */
    movq		mm1, [edx]		;	
    movq		mm2, mm0		;	

    psubusb		mm0, mm1		;	/* A - B */
    psubusb		mm1, mm2		;	/* B - A */
    por		mm0, mm1		;	/* and or gives abs difference */
    movq		mm1, mm0		;	

    punpcklbw		mm0, mm6		;	/* unpack to higher precision for accumulation */
    paddw		mm7, mm0		;	/* accumulate difference... */
    punpckhbw		mm1, mm6		;	/* unpack high four bytes to higher precision */
    add		ebx, stride1		;	/* Inc pointer into the new data */
    paddw		mm7, mm1		;	/* accumulate difference... */
    add		edx, stride2		;	/* Inc pointer into ref data */



    ; /* ------ */

    movq		mm0, mm7		;	
    psrlq		mm7, 32		;	
    paddw		mm7, mm0		;	
    movq		mm0, mm7		;	
    psrlq		mm7, 16		;	
    paddw		mm7, mm0		;	
    movd		eax, mm7		;	
    and		    eax, 0xffff		;	

    mov         DiffVal, eax
  };

  return DiffVal;

 

#endif
}

static ogg_uint32_t sad8x8_thres__sse2 (unsigned char *ptr1, ogg_uint32_t stride1,
		       		  unsigned char *ptr2, ogg_uint32_t stride2, 
			   	  ogg_uint32_t thres)
{
#if 0
  ogg_uint32_t  i;
  ogg_uint32_t  sad = 0;

  for (i=8; i; i--) {
    sad += DSP_OP_ABS_DIFF(ptr1[0], ptr2[0]);
    sad += DSP_OP_ABS_DIFF(ptr1[1], ptr2[1]);
    sad += DSP_OP_ABS_DIFF(ptr1[2], ptr2[2]);
    sad += DSP_OP_ABS_DIFF(ptr1[3], ptr2[3]);
    sad += DSP_OP_ABS_DIFF(ptr1[4], ptr2[4]);
    sad += DSP_OP_ABS_DIFF(ptr1[5], ptr2[5]);
    sad += DSP_OP_ABS_DIFF(ptr1[6], ptr2[6]);
    sad += DSP_OP_ABS_DIFF(ptr1[7], ptr2[7]);

    if (sad > thres )
      break;

    /* Step to next row of block. */
    ptr1 += stride1;
    ptr2 += stride2;
  }

  return sad;
#else
  return sad8x8__sse2 (ptr1, stride1, ptr2, stride2);
#endif
}


static ogg_uint32_t sad8x8_xy2_thres__sse2 (unsigned char *SrcData, ogg_uint32_t SrcStride,
		                      unsigned char *RefDataPtr1,
			              unsigned char *RefDataPtr2, ogg_uint32_t RefStride,
			              ogg_uint32_t thres)
{
#if 0
  ogg_uint32_t  i;
  ogg_uint32_t  sad = 0;

  for (i=8; i; i--) {
    sad += DSP_OP_ABS_DIFF(SrcData[0], DSP_OP_AVG (RefDataPtr1[0], RefDataPtr2[0]));
    sad += DSP_OP_ABS_DIFF(SrcData[1], DSP_OP_AVG (RefDataPtr1[1], RefDataPtr2[1]));
    sad += DSP_OP_ABS_DIFF(SrcData[2], DSP_OP_AVG (RefDataPtr1[2], RefDataPtr2[2]));
    sad += DSP_OP_ABS_DIFF(SrcData[3], DSP_OP_AVG (RefDataPtr1[3], RefDataPtr2[3]));
    sad += DSP_OP_ABS_DIFF(SrcData[4], DSP_OP_AVG (RefDataPtr1[4], RefDataPtr2[4]));
    sad += DSP_OP_ABS_DIFF(SrcData[5], DSP_OP_AVG (RefDataPtr1[5], RefDataPtr2[5]));
    sad += DSP_OP_ABS_DIFF(SrcData[6], DSP_OP_AVG (RefDataPtr1[6], RefDataPtr2[6]));
    sad += DSP_OP_ABS_DIFF(SrcData[7], DSP_OP_AVG (RefDataPtr1[7], RefDataPtr2[7]));

    if ( sad > thres )
      break;

    /* Step to next row of block. */
    SrcData += SrcStride;
    RefDataPtr1 += RefStride;
    RefDataPtr2 += RefStride;
  }

  return sad;
#else
  ogg_uint32_t  DiffVal;

  __asm {
    align 16

        mov     ebx, SrcData
        mov     ecx, RefDataPtr1
        mov     edx, RefDataPtr2


    pcmpeqd		mm5, mm5		;	/* fefefefefefefefe in mm5 */
    paddb		mm5, mm5		;	
				    ;	
    pxor		mm6, mm6		;	/* zero out mm6 for unpack */
    pxor		mm7, mm7		;	/* mm7 contains the result */
    mov		edi, 8		;	/* 8 rows */
    loop_start:				;	
    movq		mm0, [ebx]		;	/* take 8 bytes */

    movq		mm2, [ecx]		;	
    movq		mm3, [edx]		;	/* take average of mm2 and mm3 */
    movq		mm1, mm2		;	
    pand		mm1, mm3		;	
    pxor		mm3, mm2		;	
    pand		mm3, mm5		;	
    psrlq		mm3, 1		;	
    paddb		mm1, mm3		;	

    movq		mm2, mm0		;	

    psubusb		mm0, mm1		;	/* A - B */
    psubusb		mm1, mm2		;	/* B - A */
    por		mm0, mm1		;	/* and or gives abs difference */
    movq		mm1, mm0		;	

    punpcklbw		mm0, mm6		;	/* unpack to higher precision for accumulation */
    paddw		mm7, mm0		;	/* accumulate difference... */
    punpckhbw		mm1, mm6		;	/* unpack high four bytes to higher precision */
    add		ebx, SrcStride		;	/* Inc pointer into the new data */
    paddw		mm7, mm1		;	/* accumulate difference... */
    add		ecx, RefStride		;	/* Inc pointer into ref data */
    add		edx, RefStride		;	/* Inc pointer into ref data */

    dec		edi		;	
    jnz		loop_start		;	

    movq		mm0, mm7		;	
    psrlq		mm7, 32		;	
    paddw		mm7, mm0		;	
    movq		mm0, mm7		;	
    psrlq		mm7, 16		;	
    paddw		mm7, mm0		;	
    movd		eax, mm7		;	
    and		eax, 0xffff		;	

    mov DiffVal, eax
  };

  return DiffVal;

 

#endif
}

static ogg_uint32_t intra8x8_err__sse2 (unsigned char *DataPtr, ogg_uint32_t Stride)
{
#if 0
  ogg_uint32_t  i;
  ogg_uint32_t  XSum=0;
  ogg_uint32_t  XXSum=0;

  for (i=8; i; i--) {
     /* Examine alternate pixel locations. */
     XSum += DataPtr[0];
     XXSum += DataPtr[0]*DataPtr[0];
     XSum += DataPtr[1];
     XXSum += DataPtr[1]*DataPtr[1];
     XSum += DataPtr[2];
     XXSum += DataPtr[2]*DataPtr[2];
     XSum += DataPtr[3];
     XXSum += DataPtr[3]*DataPtr[3];
     XSum += DataPtr[4];
     XXSum += DataPtr[4]*DataPtr[4];
     XSum += DataPtr[5];
     XXSum += DataPtr[5]*DataPtr[5];
     XSum += DataPtr[6];
     XXSum += DataPtr[6]*DataPtr[6];
     XSum += DataPtr[7];
     XXSum += DataPtr[7]*DataPtr[7];

     /* Step to next row of block. */
     DataPtr += Stride;
   }

   /* Compute population variance as mis-match metric. */
   return (( (XXSum<<6) - XSum*XSum ) );
#else
  ogg_uint32_t  XSum;
  ogg_uint32_t  XXSum;

  __asm {
    align 16

        mov     ecx, DataPtr

    pxor		mm5, mm5		;	
    pxor		mm6, mm6		;	
    pxor		mm7, mm7		;	
    mov		edi, 8		;	
    loop_start:		
    movq		mm0, [ecx]		;	/* take 8 bytes */
    movq		mm2, mm0		;	

    punpcklbw		mm0, mm6		;	
    punpckhbw		mm2, mm6		;	

    paddw		mm5, mm0		;	
    paddw		mm5, mm2		;	

    pmaddwd		mm0, mm0		;	
    pmaddwd		mm2, mm2		;	
				    ;	
    paddd		mm7, mm0		;	
    paddd		mm7, mm2		;	

    add		ecx, Stride		;	/* Inc pointer into src data */

    dec		edi		;	
    jnz		loop_start		;	

    movq		mm0, mm5		;	
    psrlq		mm5, 32		;	
    paddw		mm5, mm0		;	
    movq		mm0, mm5		;	
    psrlq		mm5, 16		;	
    paddw		mm5, mm0		;	
    movd		edi, mm5		;	
    movsx		edi, di		;	
    mov		eax, edi		;	

    movq		mm0, mm7		;	
    psrlq		mm7, 32		;	
    paddd		mm7, mm0		;	
    movd		ebx, mm7		;	

        mov         XSum, eax
        mov         XXSum, ebx;

  };
    /* Compute population variance as mis-match metric. */
    return (( (XXSum<<6) - XSum*XSum ) );

 

#endif
}

static ogg_uint32_t inter8x8_err__sse2 (unsigned char *SrcData, ogg_uint32_t SrcStride,
		                 unsigned char *RefDataPtr, ogg_uint32_t RefStride)
{

#if 0
  ogg_uint32_t  i;
  ogg_uint32_t  XSum=0;
  ogg_uint32_t  XXSum=0;
  ogg_int32_t   DiffVal;

  for (i=8; i; i--) {
    DiffVal = DSP_OP_DIFF (SrcData[0], RefDataPtr[0]);
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    DiffVal = DSP_OP_DIFF (SrcData[1], RefDataPtr[1]);
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    DiffVal = DSP_OP_DIFF (SrcData[2], RefDataPtr[2]);
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    DiffVal = DSP_OP_DIFF (SrcData[3], RefDataPtr[3]);
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;
        
    DiffVal = DSP_OP_DIFF (SrcData[4], RefDataPtr[4]);
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;
        
    DiffVal = DSP_OP_DIFF (SrcData[5], RefDataPtr[5]);
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;
        
    DiffVal = DSP_OP_DIFF (SrcData[6], RefDataPtr[6]);
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;
        
    DiffVal = DSP_OP_DIFF (SrcData[7], RefDataPtr[7]);
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;
        
    /* Step to next row of block. */
    SrcData += SrcStride;
    RefDataPtr += RefStride;
  }

  /* Compute and return population variance as mis-match metric. */
  return (( (XXSum<<6) - XSum*XSum ));
#else
  ogg_uint32_t  XSum;
  ogg_uint32_t  XXSum;


  __asm {
    align 16

        mov     ecx, SrcData
        mov     edx, RefDataPtr

    pxor		mm5, mm5		;	
    pxor		mm6, mm6		;	
    pxor		mm7, mm7		;	
    mov		edi, 8		;	
    loop_start:				;	
    movq		mm0, [ecx]		;	/* take 8 bytes */
    movq		mm1, [edx]		;	
    movq		mm2, mm0		;	
    movq		mm3, mm1		;	

    punpcklbw		mm0, mm6		;	
    punpcklbw		mm1, mm6		;	
    punpckhbw		mm2, mm6		;	
    punpckhbw		mm3, mm6		;	

    psubsw		mm0, mm1		;	
    psubsw		mm2, mm3		;	

    paddw		mm5, mm0		;	
    paddw		mm5, mm2		;	

    pmaddwd		mm0, mm0		;	
    pmaddwd		mm2, mm2		;	
				    ;	
    paddd		mm7, mm0		;	
    paddd		mm7, mm2		;	

    add		ecx, SrcStride		;	/* Inc pointer into src data */
    add		edx, RefStride		;	/* Inc pointer into ref data */

    dec		edi		;	
    jnz		loop_start		;	

    movq		mm0, mm5		;	
    psrlq		mm5, 32		;	
    paddw		mm5, mm0		;	
    movq		mm0, mm5		;	
    psrlq		mm5, 16		;	
    paddw		mm5, mm0		;	
    movd		edi, mm5		;	
    movsx		edi, di		;	
    mov		eax, edi		;	

    movq		mm0, mm7		;	
    psrlq		mm7, 32		;	
    paddd		mm7, mm0		;	
    movd		ebx, mm7		;	

        mov     XSum, eax
        mov     XXSum, ebx

  };

  /* Compute and return population variance as mis-match metric. */
  return (( (XXSum<<6) - XSum*XSum ));

 
#endif
}

static ogg_uint32_t inter8x8_err_xy2__sse2 (unsigned char *SrcData, ogg_uint32_t SrcStride,
		                     unsigned char *RefDataPtr1,
				     unsigned char *RefDataPtr2, ogg_uint32_t RefStride)
{
#if 0
  ogg_uint32_t  i;
  ogg_uint32_t  XSum=0;
  ogg_uint32_t  XXSum=0;
  ogg_int32_t   DiffVal;

  for (i=8; i; i--) {
    DiffVal = DSP_OP_DIFF(SrcData[0], DSP_OP_AVG (RefDataPtr1[0], RefDataPtr2[0]));
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    DiffVal = DSP_OP_DIFF(SrcData[1], DSP_OP_AVG (RefDataPtr1[1], RefDataPtr2[1]));
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    DiffVal = DSP_OP_DIFF(SrcData[2], DSP_OP_AVG (RefDataPtr1[2], RefDataPtr2[2]));
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    DiffVal = DSP_OP_DIFF(SrcData[3], DSP_OP_AVG (RefDataPtr1[3], RefDataPtr2[3]));
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    DiffVal = DSP_OP_DIFF(SrcData[4], DSP_OP_AVG (RefDataPtr1[4], RefDataPtr2[4]));
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    DiffVal = DSP_OP_DIFF(SrcData[5], DSP_OP_AVG (RefDataPtr1[5], RefDataPtr2[5]));
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    DiffVal = DSP_OP_DIFF(SrcData[6], DSP_OP_AVG (RefDataPtr1[6], RefDataPtr2[6]));
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    DiffVal = DSP_OP_DIFF(SrcData[7], DSP_OP_AVG (RefDataPtr1[7], RefDataPtr2[7]));
    XSum += DiffVal;
    XXSum += DiffVal*DiffVal;

    /* Step to next row of block. */
    SrcData += SrcStride;
    RefDataPtr1 += RefStride;
    RefDataPtr2 += RefStride;
  }

  /* Compute and return population variance as mis-match metric. */
  return (( (XXSum<<6) - XSum*XSum ));
#else
  ogg_uint32_t XSum;
  ogg_uint32_t XXSum;

  __asm {
    align 16

        mov ebx, SrcData
        mov ecx, RefDataPtr1
        mov edx, RefDataPtr2

    pcmpeqd		mm4, mm4		;	/* fefefefefefefefe in mm4 */
    paddb		mm4, mm4		;	
    pxor		mm5, mm5		;	
    pxor		mm6, mm6		;	
    pxor		mm7, mm7		;	
    mov		edi, 8		;	
    loop_start:				;	
    movq		mm0, [ebx]		;	/* take 8 bytes */

    movq		mm2, [ecx]		;	
    movq		mm3, [edx]		;	/* take average of mm2 and mm3 */
    movq		mm1, mm2		;	
    pand		mm1, mm3		;	
    pxor		mm3, mm2		;	
    pand		mm3, mm4		;	
    psrlq		mm3, 1		;	
    paddb		mm1, mm3		;	

    movq		mm2, mm0		;	
    movq		mm3, mm1		;	

    punpcklbw		mm0, mm6		;	
    punpcklbw		mm1, mm6		;	
    punpckhbw		mm2, mm6		;	
    punpckhbw		mm3, mm6		;	

    psubsw		mm0, mm1		;	
    psubsw		mm2, mm3		;	

    paddw		mm5, mm0		;	
    paddw		mm5, mm2		;	

    pmaddwd		mm0, mm0		;	
    pmaddwd		mm2, mm2		;	
				    ;	
    paddd		mm7, mm0		;	
    paddd		mm7, mm2		;	

    add		ebx, SrcStride		;	/* Inc pointer into src data */
    add		ecx, RefStride		;	/* Inc pointer into ref data */
    add		edx, RefStride		;	/* Inc pointer into ref data */

    dec		edi		;	
    jnz		loop_start		;	

    movq		mm0, mm5		;	
    psrlq		mm5, 32		;	
    paddw		mm5, mm0		;	
    movq		mm0, mm5		;	
    psrlq		mm5, 16		;	
    paddw		mm5, mm0		;	
    movd		edi, mm5		;	
    movsx		edi, di		;	
    mov         XSum, edi   ; /* movl		eax, edi		;	Modified for vc to resuse eax*/

    movq		mm0, mm7		;	
    psrlq		mm7, 32		;	
    paddd		mm7, mm0		;	
    movd        XXSum, mm7 ; /*movd		eax, mm7		; Modified for vc to reuse eax */
  };

    return (( (XXSum<<6) - XSum*XSum ));

#endif
}



void dsp_sse2_init(DspFunctions *funcs)
{
  TH_DEBUG("enabling accelerated x86_32 mmx dsp functions.\n");
  funcs->sub8x8 = sub8x8__sse2;
  funcs->sub8x8_128 = sub8x8_128__sse2;
  funcs->sub8x8avg2 = sub8x8avg2__sse2;
  //funcs->row_sad8 = row_sad8__sse2;
  //funcs->col_sad8x8 = col_sad8x8__sse2;
  //funcs->sad8x8 = sad8x8__sse2;
  //funcs->sad8x8_thres = sad8x8_thres__sse2;
  //funcs->sad8x8_xy2_thres = sad8x8_xy2_thres__sse2;
  //funcs->intra8x8_err = intra8x8_err__sse2;
  //funcs->inter8x8_err = inter8x8_err__sse2;
  //funcs->inter8x8_err_xy2 = inter8x8_err_xy2__sse2;
}

