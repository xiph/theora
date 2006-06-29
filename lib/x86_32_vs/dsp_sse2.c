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
#include "perf_helper.h"

#if 1
//These are to let me selectively enable the C versions, these are needed
#define DSP_OP_AVG(a,b) ((((int)(a)) + ((int)(b)))/2)
#define DSP_OP_DIFF(a,b) (((int)(a)) - ((int)(b)))
#define DSP_OP_ABS_DIFF(a,b) abs((((int)(a)) - ((int)(b))))
#endif



static perf_info sub8x8_sse2_perf;
static perf_info sub8x8_128_sse2_perf;


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
	//PERF_BLOCK_START();
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

	//PERF_BLOCK_END("sub8x8 sse2", sub8x8_sse2_perf, 10000);


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
	PERF_BLOCK_START();
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

	PERF_BLOCK_END("sub8x8_128 sse2", sub8x8_128_sse2_perf, 10000);

 
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
        /*  
            //DON'T USE THESE, THEY AREN'T EQUIVALENT, THEY ADD 1 TO THE SUM
            pavgw       xmm1, xmm3
            pavgw       xmm2, xmm4
         */
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
        /*  
            //DON'T USE THESE, THEY AREN'T EQUIVALENT, THEY ADD 1 TO THE SUM
            pavgw       xmm1, xmm3
            pavgw       xmm2, xmm4
         */
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
        /*  
            //DON'T USE THESE, THEY AREN'T EQUIVALENT, THEY ADD 1 TO THE SUM
            pavgw       xmm1, xmm3
            pavgw       xmm2, xmm4
         */
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
        /*  
            //DON'T USE THESE, THEY AREN'T EQUIVALENT, THEY ADD 1 TO THE SUM
            pavgw       xmm1, xmm3
            pavgw       xmm2, xmm4
         */
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
  ogg_uint32_t SadValue;


  __asm {

    align       16
    mov         ebx, Src1
    mov         ecx, Src2

    pxor        xmm0, xmm0

    /* Load all the data */
    movq      xmm1, QWORD PTR [ebx]
    movq      xmm2, QWORD PTR [ecx]

    /* abs_diff(a,b) = (a-b)|(b-a) */
    movdqa      xmm3, xmm1
    psubusb     xmm1, xmm2
    psubusb     xmm2, xmm3
    por         xmm1, xmm2

    /* Extend to int16 */
    punpcklbw   xmm1, xmm0

    /* Shift each block of 64bits right by 32 so they align for adding */
    movdqa      xmm2, xmm1
    psrlq       xmm1, 32
    paddw       xmm2, xmm1

    /* Shift 16 to align again  and add */
    movdqa      xmm3, xmm2
    psrlq       xmm2, 16
    paddw       xmm3, xmm2

    /* Shift the whole register so the 2 results line up in the lowest 16bits */
    movdqa      xmm1, xmm3
    psrldq      xmm3, 8

    /* eax = max(SadValue, SadValue1) */
    psubusw     xmm1, xmm3
    paddw       xmm1, xmm3

    movd         eax, xmm1
    and         eax, 0xffff
    mov         SadValue, eax

  }

  return SadValue;


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

    static __declspec(align(16)) unsigned int temp_regs[8];
    static unsigned int* const temp_reg_ptr = temp_regs;
    static unsigned int* const temp_reg_result_ptr = &temp_regs[4];
 
    ogg_uint32_t SadValue;

    /* TODO::: It may not be worth contracting to 8 bit in the middle 
                The conversion back and forth possibly outweighs the saving */


    __asm {
        align       16
        
        /* Setup the paramters */
        mov         ebx, Src1
        mov         ecx, Src2
        mov         edx, stride
        lea         eax, [edx + edx*2]
        mov         edi, temp_reg_ptr
        mov         esi, temp_reg_result_ptr

        /* Read the first 4 iterations */
        movq      xmm0, QWORD PTR [ebx]
        movq      xmm4, QWORD PTR [ebx + edx]
        movq      xmm2, QWORD PTR [ebx + edx*2]
        movq      xmm6, QWORD PTR [ebx + eax]

        movq      xmm1, QWORD PTR [ecx]
        movq      xmm5, QWORD PTR [ecx + edx]
        movq      xmm3, QWORD PTR [ecx + edx*2]
        movq      xmm7, QWORD PTR [ecx + eax]

        /* Consolidate the results from 8 registers of 8x16bits to 4 of 16x8bits */
            movdqa      [edi], xmm7 /* Save xmm7 */
            pxor        xmm7, xmm7

            /* Expand everything to 16 bits */
            punpcklbw   xmm0, xmm7
            punpcklbw   xmm1, xmm7
            punpcklbw   xmm2, xmm7
            punpcklbw   xmm3, xmm7
            punpcklbw   xmm4, xmm7
            punpcklbw   xmm5, xmm7
            punpcklbw   xmm6, xmm7

            /* Now merge the first 3 */
            packuswb    xmm0, xmm4
            packuswb    xmm1, xmm5
            packuswb    xmm2, xmm6

            /* Restore xmm7 for the final merge into xmm3 */
            movdqa      xmm6, [edi]
            punpcklbw   xmm6, xmm7
            packuswb    xmm3, xmm6

        /* Duplicate all the registers */
        movdqa      xmm4, xmm0
        movdqa      xmm5, xmm1
        movdqa      xmm6, xmm2
        movdqa      xmm7, xmm3

        /* result = abs_diff(a,b) = (a-b)|(b-a) */
        psubusb     xmm0, xmm1
        psubusb     xmm2, xmm3

        psubusb     xmm1, xmm4
        psubusb     xmm3, xmm6

        por         xmm0, xmm1
        por         xmm2, xmm3

        /* Expand the 32x8bits in 2 registers to 32x16bits in 4 registers */
        pxor        xmm7, xmm7

        movdqa      xmm1, xmm0
        movdqa      xmm3, xmm2
        
        punpcklbw   xmm0, xmm7
        punpckhbw   xmm1, xmm7
        punpcklbw   xmm2, xmm7
        punpckhbw   xmm3, xmm7

        /* Add them up and then xmm0 contains the 8x16bit SadValue array*/
        paddw       xmm0, xmm1
        paddw       xmm2, xmm3
        paddw       xmm0, xmm2

        /* Save xmm0 for later so we can use all 8 registers again in the memread */
        /* push        xmm0 */
        movdqa      [esi], xmm0

        /* Advance the read pointers */
        lea         ebx, [ebx + edx*4]
        lea         ecx, [ecx + edx*4]

        /* ----- Repeat of above for the second sad array ------ */

        /* Read the first 4 iterations */
        movq      xmm0, QWORD PTR [ebx]
        movq      xmm4, QWORD PTR [ebx + edx]
        movq      xmm2, QWORD PTR [ebx + edx*2]
        movq      xmm6, QWORD PTR [ebx + eax]

        movq      xmm1, QWORD PTR [ecx]
        movq      xmm5, QWORD PTR [ecx + edx]
        movq      xmm3, QWORD PTR [ecx + edx*2]
        movq      xmm7, QWORD PTR [ecx + eax]

        /* Consolidate the results from 8 registers of 8x16bits to 4 of 16x8bits */
            movdqa      [edi], xmm7 /* Save xmm7 */
            pxor        xmm7, xmm7

            /* Expand everything to 16 bits */
            punpcklbw   xmm0, xmm7
            punpcklbw   xmm1, xmm7
            punpcklbw   xmm2, xmm7
            punpcklbw   xmm3, xmm7
            punpcklbw   xmm4, xmm7
            punpcklbw   xmm5, xmm7
            punpcklbw   xmm6, xmm7

            /* Now merge the first 3 */
            packuswb    xmm0, xmm4
            packuswb    xmm1, xmm5
            packuswb    xmm2, xmm6

            /* Restore xmm7 for the final merge into xmm3 */
            movdqa      xmm6, [edi]
            punpcklbw   xmm6, xmm7
            packuswb    xmm3, xmm6


        /* Duplicate all the registers */
        movdqa      xmm4, xmm0
        movdqa      xmm5, xmm1
        movdqa      xmm6, xmm2
        movdqa      xmm7, xmm3

        /* result = abs_diff(a,b) = (a-b)|(b-a) */
        psubusb     xmm0, xmm1
        psubusb     xmm2, xmm3

        psubusb     xmm1, xmm4
        psubusb     xmm3, xmm6

        por         xmm0, xmm1
        por         xmm2, xmm3

        /* Expand the 32x8bits in 2 registers to 32x16bits in 4 registers */
        pxor        xmm7, xmm7

        movdqa      xmm1, xmm0
        movdqa      xmm3, xmm2
        
        punpcklbw   xmm0, xmm7
        punpckhbw   xmm1, xmm7
        punpcklbw   xmm2, xmm7
        punpckhbw   xmm3, xmm7

        /* Add them up and then xmm0 contains the 8x16bit SadValue array*/
        paddw       xmm0, xmm1
        paddw       xmm2, xmm3
        paddw       xmm0, xmm2

        /* --------------- End of repeat ---------- */

        /* Restore the save sadarray - then xmm0 has sad1, and xmm1 has sad2*/
        /* pop         xmm1 */
        movdqa      xmm1, [esi]

                /* Find the maximum sad value */

        /* 
            Eliminate sad values from each array if they are not max.
            If any posistion in xmm1 was greater than the one in xmm0
            It's value is now in xmm0.
         */
        psubusw     xmm0, xmm1
        paddw       xmm0, xmm1

        /* reduce from 8 possibles to 4 with a shift-max */
        movdqa      xmm1, xmm0
        psrlq		xmm0, 32

        psubusw     xmm0, xmm1
        paddw       xmm0, xmm1

        /* reduce from 4 to 2 with another shift-max */
        movdqa      xmm1, xmm0
        psrlq		xmm0, 16

        psubusw     xmm0, xmm1
        paddw       xmm0, xmm1
        /* reduce to final value with another full register shift-max */
        movdqa      xmm1, xmm0
        psrldq      xmm0, 8

        psubusw     xmm0, xmm1
        paddw       xmm0, xmm1

        /* Put it in the return variable */
        movd         eax, xmm0
        and         eax, 0xffff
        mov         SadValue, eax


    };

    return SadValue;
 

#endif
}

static ogg_uint32_t sad8x8__sse2 (unsigned char *ptr1, ogg_uint32_t stride1,
		       	    unsigned char *ptr2, ogg_uint32_t stride2)
{

#if 0
  ogg_uint32_t  i;
  ogg_uint32_t  sad = 0;

  PERF_BLOCK_START();

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

  PERF_BLOCK_END("sad8x8 C - ", perf_sad8x8_time, perf_sad8x8_count,perf_sad8x8_min, 50000);

  return sad;
#elif 1
  ogg_uint32_t  DiffVal;

  __asm {
    align  16

    mov         eax, ptr1
    mov         ebx, ptr2


    mov         ecx, stride1
    mov         edx, stride2

    pxor      xmm2, xmm2 /* Result */    
    pxor      xmm3, xmm3

    mov         edi, 4

loop_start:
        movq      xmm0, QWORD PTR [eax]
        movq      xmm1, QWORD PTR [eax + ecx]


        movq      xmm4, QWORD PTR [ebx]
        movq      xmm5, QWORD PTR [ebx + edx]

        /* Absolute difference */
        movq        xmm6, xmm0
        movq        xmm7, xmm1
        psubusb     xmm0, xmm4
        psubusb     xmm1, xmm5
        psubusb     xmm4, xmm6
        psubusb     xmm5, xmm7
        por         xmm0, xmm4
        por         xmm1, xmm5

        /* Expand to 16 bits */
        punpcklbw   xmm0, xmm3
        punpcklbw   xmm1, xmm3

        /* Accumulate */
        paddw       xmm0, xmm1
        paddw       xmm2, xmm0

        lea         eax, [eax + 2*ecx]
        lea         ebx, [ebx + 2*edx]
        sub     edi, 1
        jnz     loop_start

        
    /*---------------------------*/


    /* Add the items in the result */
    movdqa      xmm0, xmm2
    psrlq       xmm2, 32

    paddw       xmm0, xmm2


    movdqa      xmm2, xmm0
    psrlq       xmm0, 16

    paddw       xmm2, xmm0

    movdqa      xmm0, xmm2
    psrldq      xmm2, 8
    paddw       xmm0, xmm2

    /* Put it in the return variable */

    movd        eax, xmm0
    and         eax, 0xffff
    mov         DiffVal, eax


  };

    return DiffVal;
   
 


#else

   

  ogg_uint32_t  DiffVal;

 PERF_BLOCK_START();
  __asm {
    align  16

    mov         eax, ptr1
    mov         ebx, ptr2


    mov         ecx, stride1
    mov         edx, stride2


    lea         edi, [ecx + ecx*2]
    lea         esi, [edx + edx*2]

    pxor      xmm2, xmm2 /* Result */    
    pxor      xmm3, xmm3

    /* Iteration 1-4 */

        /*Read 2 lots of 8 bytes from each */
        movq      xmm0, QWORD PTR [eax]
        movq      xmm1, QWORD PTR [eax + ecx]

        movq      xmm4, QWORD PTR [ebx]
        movq      xmm5, QWORD PTR [ebx + edx]

        /* Absolute difference */
        movq        xmm6, xmm0
        movq        xmm7, xmm1
        psubusb     xmm0, xmm4
        psubusb     xmm1, xmm5
        psubusb     xmm4, xmm6
        psubusb     xmm5, xmm7
        por         xmm0, xmm4
        por         xmm1, xmm5

        /* Expand to 16 bits */
        punpcklbw   xmm0, xmm3
        punpcklbw   xmm1, xmm3

        /* Accumulate */
        paddw       xmm0, xmm1
        paddw       xmm2, xmm0

        /* ----- half ----- */

        /*Read second 2 lots of 8 bytes from each */
        movq      xmm0, QWORD PTR [eax + ecx * 2]
        movq      xmm1, QWORD PTR [eax + edi]


        movq      xmm4, QWORD PTR [ebx + edx * 2]
        movq      xmm5, QWORD PTR [ebx + esi]

        /* Absolute difference */
        movq        xmm6, xmm0
        movq        xmm7, xmm1
        psubusb     xmm0, xmm4
        psubusb     xmm1, xmm5
        psubusb     xmm4, xmm6
        psubusb     xmm5, xmm7
        por         xmm0, xmm4
        por         xmm1, xmm5

        /* Expand to 16 bits */
        punpcklbw   xmm0, xmm3
        punpcklbw   xmm1, xmm3

        /* Accumulate */
        paddw       xmm0, xmm1
        paddw       xmm2, xmm0

    /* Advance read ptrs */
    lea     eax, [eax + ecx*4]
    lea     ebx, [ebx + edx*4]


    /* Iteration 5-8 */

        /*Read 2 lots of 8 bytes from each */
        movq      xmm0, QWORD PTR [eax]
        movq      xmm1, QWORD PTR [eax + ecx]

        movq      xmm4, QWORD PTR [ebx]
        movq      xmm5, QWORD PTR [ebx + edx]

        /* Absolute difference */
        movq        xmm6, xmm0
        movq        xmm7, xmm1
        psubusb     xmm0, xmm4
        psubusb     xmm1, xmm5
        psubusb     xmm4, xmm6
        psubusb     xmm5, xmm7
        por         xmm0, xmm4
        por         xmm1, xmm5

        /* Expand to 16 bits */
        punpcklbw   xmm0, xmm3
        punpcklbw   xmm1, xmm3

        /* Accumulate */
        paddw       xmm0, xmm1
        paddw       xmm2, xmm0

        /* ----- half ----- */

        /*Read second 2 lots of 8 bytes from each */
        movq      xmm0, QWORD PTR [eax + ecx * 2]
        movq      xmm1, QWORD PTR [eax + edi]


        movq      xmm4, QWORD PTR [ebx + edx * 2]
        movq      xmm5, QWORD PTR [ebx + esi]

        /* Absolute difference */
        movq        xmm6, xmm0
        movq        xmm7, xmm1
        psubusb     xmm0, xmm4
        psubusb     xmm1, xmm5
        psubusb     xmm4, xmm6
        psubusb     xmm5, xmm7
        por         xmm0, xmm4
        por         xmm1, xmm5

        /* Expand to 16 bits */
        punpcklbw   xmm0, xmm3
        punpcklbw   xmm1, xmm3

        /* Accumulate */
        paddw       xmm0, xmm1
        paddw       xmm2, xmm0
        
    /*---------------------------*/

    /* Load the address of temp */
    //mov         edx, temp_ptr

    /* Add the items in the result */
    movdqa      xmm0, xmm2
    psrlq       xmm2, 32

    paddw       xmm0, xmm2


    movdqa      xmm2, xmm0
    psrlq       xmm0, 16

    paddw       xmm2, xmm0

    movdqa      xmm0, xmm2
    psrldq      xmm2, 8
    paddw       xmm0, xmm2

    /* Put it in the return variable */

    movd        eax, xmm0
    and         eax, 0xffff
    mov         DiffVal, eax


  };

 PERF_BLOCK_END("sad8x8 sse2 - ", perf_sad8x8_time, perf_sad8x8_count,perf_sad8x8_min, 50000);
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
  TH_DEBUG("enabling accelerated x86_32 sse2 dsp functions.\n");
  funcs->sub8x8 = sub8x8__sse2;
  funcs->sub8x8_128 = sub8x8_128__sse2;
  //funcs->sub8x8avg2 = sub8x8avg2__sse2;
  //funcs->row_sad8 = row_sad8__sse2;
  //funcs->col_sad8x8 = col_sad8x8__sse2;
  
  
  /* The mmx versions are faster right now */
  //funcs->sad8x8 = sad8x8__sse2;
  //funcs->sad8x8_thres = sad8x8_thres__sse2;
 
  
  /* -------------- Not written --------- */
  //funcs->sad8x8_xy2_thres = sad8x8_xy2_thres__sse2;
  //funcs->intra8x8_err = intra8x8_err__sse2;
  //funcs->inter8x8_err = inter8x8_err__sse2;
  //funcs->inter8x8_err_xy2 = inter8x8_err_xy2__sse2;

  ClearPerfData(&sub8x8_sse2_perf);
  ClearPerfData(&sub8x8_128_sse2_perf);



  

}

