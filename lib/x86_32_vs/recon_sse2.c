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

#include "codec_internal.h"
#include "dsp.h"
#include "cpu.h"

static const unsigned int V128x16[4] = { 0x80808080, 0x80808080, 0x80808080, 0x80808080 };
static const unsigned int* V128x16Ptr = V128x16;

static void copy8x8__sse2 (unsigned char *src,
	                unsigned char *dest,
	                unsigned int stride)
{
#if 0
  int j;
  for ( j = 0; j < 8; j++ ){
    ((ogg_uint32_t*)dest)[0] = ((ogg_uint32_t*)src)[0];
    ((ogg_uint32_t*)dest)[1] = ((ogg_uint32_t*)src)[1];
    src+=stride;
    dest+=stride;
  }

#else

    /*

            @src
            <----    stride    ---->
    0       FFFF FFFF .... .... ....
    1       FFFF FFFF .... .... ....
    ...
    7       FFFF FFFF .... .... ....


            @dest
            <----    stride    ---->
    0       TTTT TTTT .... .... ....
    1       TTTT TTTT .... .... ....
    ...
    7       TTTT TTTT .... .... ....


    */
    __asm {
        align 16

        /* Load the parameters into the general registers */
        mov         eax, src
        mov         ebx, dest
        mov         ecx, stride

        /* edi = 3*stride */
        /* edx = 5*stride */
        /* edi = 7*stride */
        lea		    edi, [ecx + ecx * 2]
        lea         edx, [ecx + ecx * 4]
        lea         esi, [ecx + edi * 2]

        /* 
            TODO::: If we can somehow ensure each addressed element of src 
            and dest, were 16 byte aligned could maybe use movdqa which might be
            faster. That requires that the base pointer is aligned,
            and that the stride is a multiple of 16
            */

        /* Load all 8 registers */
        movq      xmm0, QWORD PTR [eax]
        movq      xmm1, QWORD PTR [eax + ecx]
        movq      xmm2, QWORD PTR [eax + ecx * 2]
        movq      xmm3, QWORD PTR [eax + edi]

        movq      xmm4, QWORD PTR [eax + ecx * 4]
        movq      xmm5, QWORD PTR [eax + edx]
        movq      xmm6, QWORD PTR [eax + edi * 2]
        movq      xmm7, QWORD PTR [eax + esi]


        /* Write out all 8 registers */
        movq      QWORD PTR [ebx], xmm0
        movq      QWORD PTR [ebx + ecx], xmm1
        movq      QWORD PTR [ebx + ecx * 2], xmm2
        movq      QWORD PTR [ebx + edi], xmm3

        movq      QWORD PTR [ebx + ecx * 4], xmm4
        movq      QWORD PTR [ebx + edx], xmm5
        movq      QWORD PTR [ebx + edi * 2], xmm6
        movq      QWORD PTR [ebx + esi], xmm7



    };

#endif
}

static void recon_intra8x8__sse2 (unsigned char *ReconPtr, ogg_int16_t *ChangePtr,
		      ogg_uint32_t LineStep)
{

#if 0
  ogg_uint32_t i;

  for (i = 8; i; i--){
    /* Convert the data back to 8 bit unsigned */
    /* Saturate the output to unsigend 8 bit values */
    ReconPtr[0] = clamp255( ChangePtr[0] + 128 );
    ReconPtr[1] = clamp255( ChangePtr[1] + 128 );
    ReconPtr[2] = clamp255( ChangePtr[2] + 128 );
    ReconPtr[3] = clamp255( ChangePtr[3] + 128 );
    ReconPtr[4] = clamp255( ChangePtr[4] + 128 );
    ReconPtr[5] = clamp255( ChangePtr[5] + 128 );
    ReconPtr[6] = clamp255( ChangePtr[6] + 128 );
    ReconPtr[7] = clamp255( ChangePtr[7] + 128 );

    ReconPtr += LineStep;
    ChangePtr += 8;
  }

#else

    /*
        @ChangePtr
        <--- 8 int16's --->
    0   HLHL HLHL HLHL HLHL
    ...
    7   HLHL HLHL HLHL HLHL

    
        @ReconPtr
        <----- LineStep ------->
    0   CCCC CCCC .... .... .... 
    ...
    7   CCCC CCCC .... .... .... 
    */

    __asm {

        align 16

        mov     eax, ReconPtr
        mov     ebx, ChangePtr
        mov     ecx, LineStep
        mov     edx, V128x16Ptr

        /* Check whether we can use movdqa for 16 byte alignment */

        movdqu      xmm7, [edx]
        /* 8 lots of int16 per register on the first mov */
        /* Then packs those 8 + another 8 down to 16x 8 bits */
        /* Loads the data in only 4 iterations into different registers */
        /* Maybe just make all the loads offsetted adress and no lea? */
        
        /* Iteration 1 - xmm0 */
        movdqu      xmm0, [ebx]
        packsswb    xmm0, [ebx + 16]
        pxor        xmm0, xmm7
        lea         ebx, [ebx + 32]

        /* Iteration 2 - xmm1*/
        movdqu      xmm1, [ebx]
        packsswb    xmm1, [ebx + 16]
        pxor        xmm1, xmm7
        lea         ebx, [ebx + 32]

        /* Iteration 3 - xmm2 */
        movdqu      xmm2, [ebx]
        packsswb    xmm2, [ebx + 16]
        pxor        xmm2, xmm7
        lea         ebx, [ebx + 32]

        /* Iteration 4 - xmm3 */
        movdqu      xmm3, [ebx]
        packsswb    xmm3, [ebx + 16]
        pxor        xmm3, xmm7
        /* lea         ebx, [ebx + 16] */


        /* Output the data - lower bits, then shift then low bits again */

        /* Iteration 1 - xmm0 */
        movq        QWORD PTR [eax], xmm0
        psrldq      xmm0, 8
        movq        QWORD PTR [eax + ecx], xmm0
        lea         eax, [eax + ecx * 2]
        
        /* Iteration 2 - xmm1 */
        movq        QWORD PTR [eax], xmm1
        psrldq      xmm1, 8
        movq        QWORD PTR [eax + ecx], xmm1
        lea         eax, [eax + ecx * 2]

        /* Iteration 3 - xmm2 */
        movq        QWORD PTR [eax], xmm2
        psrldq      xmm2, 8
        movq        QWORD PTR [eax + ecx], xmm2
        lea         eax, [eax + ecx * 2]

        /* Iteration 4 - xmm3 */
        movq        QWORD PTR [eax], xmm3
        psrldq      xmm3, 8
        movq        QWORD PTR [eax + ecx], xmm3
        /* lea         eax, [eax + ecx]*/


    };

#endif
}

static void recon_inter8x8__sse2 (unsigned char *ReconPtr, unsigned char *RefPtr,
		      ogg_int16_t *ChangePtr, ogg_uint32_t LineStep)
{
  ogg_uint32_t i;

  for (i = 8; i; i--){
    ReconPtr[0] = clamp255(RefPtr[0] + ChangePtr[0]);
    ReconPtr[1] = clamp255(RefPtr[1] + ChangePtr[1]);
    ReconPtr[2] = clamp255(RefPtr[2] + ChangePtr[2]);
    ReconPtr[3] = clamp255(RefPtr[3] + ChangePtr[3]);
    ReconPtr[4] = clamp255(RefPtr[4] + ChangePtr[4]);
    ReconPtr[5] = clamp255(RefPtr[5] + ChangePtr[5]);
    ReconPtr[6] = clamp255(RefPtr[6] + ChangePtr[6]);
    ReconPtr[7] = clamp255(RefPtr[7] + ChangePtr[7]);

    ChangePtr += 8;
    ReconPtr += LineStep;
    RefPtr += LineStep;
  }
}

static void recon_inter8x8_half__sse2 (unsigned char *ReconPtr, unsigned char *RefPtr1,
		           unsigned char *RefPtr2, ogg_int16_t *ChangePtr,
			   ogg_uint32_t LineStep)
{
  ogg_uint32_t  i;

  for (i = 8; i; i--){
    ReconPtr[0] = clamp255((((int)RefPtr1[0] + (int)RefPtr2[0]) >> 1) + ChangePtr[0] );
    ReconPtr[1] = clamp255((((int)RefPtr1[1] + (int)RefPtr2[1]) >> 1) + ChangePtr[1] );
    ReconPtr[2] = clamp255((((int)RefPtr1[2] + (int)RefPtr2[2]) >> 1) + ChangePtr[2] );
    ReconPtr[3] = clamp255((((int)RefPtr1[3] + (int)RefPtr2[3]) >> 1) + ChangePtr[3] );
    ReconPtr[4] = clamp255((((int)RefPtr1[4] + (int)RefPtr2[4]) >> 1) + ChangePtr[4] );
    ReconPtr[5] = clamp255((((int)RefPtr1[5] + (int)RefPtr2[5]) >> 1) + ChangePtr[5] );
    ReconPtr[6] = clamp255((((int)RefPtr1[6] + (int)RefPtr2[6]) >> 1) + ChangePtr[6] );
    ReconPtr[7] = clamp255((((int)RefPtr1[7] + (int)RefPtr2[7]) >> 1) + ChangePtr[7] );

    ChangePtr += 8;
    ReconPtr += LineStep;
    RefPtr1 += LineStep;
    RefPtr2 += LineStep;
  }
}


void dsp_sse2_recon_init(DspFunctions *funcs)
{
  TH_DEBUG("enabling accelerated x86_32 sse2 recon functions.\n");
  funcs->copy8x8 = copy8x8__sse2;
  funcs->recon_intra8x8 = recon_intra8x8__sse2;
  funcs->recon_inter8x8 = recon_inter8x8__sse2;
  funcs->recon_inter8x8_half = recon_inter8x8_half__sse2;
}

