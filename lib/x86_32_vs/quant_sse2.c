/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2005                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id: quant.c 11442 2006-05-27 17:28:08Z giles $

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "codec_internal.h"
#include "quant_lookup.h"

#include "perf_helper.h"

static unsigned __int64 perf_quant_time;
static unsigned __int64 perf_quant_min;
static unsigned __int64 perf_quant_count;


void quantize__sse2( PB_INSTANCE *pbi,
               ogg_int16_t * DCT_block,
               Q_LIST_ENTRY * quantized_list){

#if 1
  ogg_uint32_t  i;              /* Row index */
  Q_LIST_ENTRY  val;            /* Quantised value. */

  ogg_int32_t * FquantRoundPtr = pbi->fquant_round;
  ogg_int32_t * FquantCoeffsPtr = pbi->fquant_coeffs;
  ogg_int32_t * FquantZBinSizePtr = pbi->fquant_ZbSize;
  ogg_int16_t * DCT_blockPtr = DCT_block;
  ogg_uint32_t * ZigZagPtr = (ogg_uint32_t *)pbi->zigzag_index;
  ogg_int32_t temp;

  PERF_BLOCK_START();
  /* Set the quantized_list to default to 0 */
  memset( quantized_list, 0, 64 * sizeof(Q_LIST_ENTRY) );

  /* Note that we add half divisor to effect rounding on positive number */
  for( i = 0; i < VFRAGPIXELS; i++) {
    /* Column 0  */
    if ( DCT_blockPtr[0] >= FquantZBinSizePtr[0] ) {
      temp = FquantCoeffsPtr[0] * ( DCT_blockPtr[0] + FquantRoundPtr[0] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[0]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[0] <= -FquantZBinSizePtr[0] ) {
      temp = FquantCoeffsPtr[0] *
        ( DCT_blockPtr[0] - FquantRoundPtr[0] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[0]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 1 */
    if ( DCT_blockPtr[1] >= FquantZBinSizePtr[1] ) {
      temp = FquantCoeffsPtr[1] *
        ( DCT_blockPtr[1] + FquantRoundPtr[1] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[1]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[1] <= -FquantZBinSizePtr[1] ) {
      temp = FquantCoeffsPtr[1] *
        ( DCT_blockPtr[1] - FquantRoundPtr[1] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[1]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 2 */
    if ( DCT_blockPtr[2] >= FquantZBinSizePtr[2] ) {
      temp = FquantCoeffsPtr[2] *
        ( DCT_blockPtr[2] + FquantRoundPtr[2] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[2]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[2] <= -FquantZBinSizePtr[2] ) {
      temp = FquantCoeffsPtr[2] *
        ( DCT_blockPtr[2] - FquantRoundPtr[2] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[2]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 3 */
    if ( DCT_blockPtr[3] >= FquantZBinSizePtr[3] ) {
      temp = FquantCoeffsPtr[3] *
        ( DCT_blockPtr[3] + FquantRoundPtr[3] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[3]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[3] <= -FquantZBinSizePtr[3] ) {
      temp = FquantCoeffsPtr[3] *
        ( DCT_blockPtr[3] - FquantRoundPtr[3] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[3]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 4 */
    if ( DCT_blockPtr[4] >= FquantZBinSizePtr[4] ) {
      temp = FquantCoeffsPtr[4] *
        ( DCT_blockPtr[4] + FquantRoundPtr[4] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[4]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[4] <= -FquantZBinSizePtr[4] ) {
      temp = FquantCoeffsPtr[4] *
        ( DCT_blockPtr[4] - FquantRoundPtr[4] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[4]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 5 */
    if ( DCT_blockPtr[5] >= FquantZBinSizePtr[5] ) {
      temp = FquantCoeffsPtr[5] *
        ( DCT_blockPtr[5] + FquantRoundPtr[5] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[5]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[5] <= -FquantZBinSizePtr[5] ) {
      temp = FquantCoeffsPtr[5] *
        ( DCT_blockPtr[5] - FquantRoundPtr[5] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[5]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 6 */
    if ( DCT_blockPtr[6] >= FquantZBinSizePtr[6] ) {
      temp = FquantCoeffsPtr[6] *
        ( DCT_blockPtr[6] + FquantRoundPtr[6] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[6]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[6] <= -FquantZBinSizePtr[6] ) {
      temp = FquantCoeffsPtr[6] *
        ( DCT_blockPtr[6] - FquantRoundPtr[6] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[6]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 7 */
    if ( DCT_blockPtr[7] >= FquantZBinSizePtr[7] ) {
      temp = FquantCoeffsPtr[7] *
        ( DCT_blockPtr[7] + FquantRoundPtr[7] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[7]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[7] <= -FquantZBinSizePtr[7] ) {
      temp = FquantCoeffsPtr[7] *
        ( DCT_blockPtr[7] - FquantRoundPtr[7] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[7]] = ( val < -511 ) ? -511 : val;
    }

    FquantRoundPtr += 8;
    FquantCoeffsPtr += 8;
    FquantZBinSizePtr += 8;
    DCT_blockPtr += 8;
    ZigZagPtr += 8;
  }

  PERF_BLOCK_END("quantize C", perf_quant_time, perf_quant_count, perf_quant_min, 20000);

#else
static __declspec(align(16)) unsigned short Some511s[8] = { 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF };
static unsigned short* Some511sPtr = Some511s;

static __declspec(align(16)) unsigned char temp[128];
static unsigned char* temp_ptr = temp;


  ogg_int32_t * FquantRoundPtr = pbi->fquant_round;     /* These are not aligned for now */
  ogg_int32_t * FquantCoeffsPtr = pbi->fquant_coeffs;   /* These are not aligned for now */
  ogg_int32_t * FquantZBinSizePtr = pbi->fquant_ZbSize; /* These are not aligned for now */
  ogg_int16_t * DCT_blockPtr = DCT_block;
  ogg_uint32_t * ZigZagPtr = (ogg_uint32_t *)pbi->zigzag_index;
  

  //PERF_BLOCK_START();
 

  __asm {
        align           16
        
        mov     edi, temp_ptr      
        mov     esi, DCT_blockPtr
        mov     eax, FquantRoundPtr
        mov     edx, FquantCoeffsPtr
        mov     ecx, Some511sPtr
        
        movdqa  xmm7, [ecx]
        pcmpeqw xmm6, xmm6      /* All 1's */
        mov     ecx, FquantZBinSizePtr

            push    ebx
            mov     ebx, 8/* Loop counter */
           
            

        /* Set to 0, might be better to do twice as many modq's than to unaligned write? */

        /* 128 bytes worth of 0's */
        //movdqu  [edi], xmm0
        //movdqu  [edi + 16], xmm0
        //movdqu  [edi + 32], xmm0
        //movdqu  [edi + 48], xmm0
        //movdqu  [edi + 64], xmm0
        //movdqu  [edi + 80], xmm0
        //movdqu  [edi + 96], xmm0
        //movdqu  [edi + 112], xmm0

    read_loop_start:
        pxor    xmm0, xmm0

        /* Read all 8x16 bitsof the dct block */
        movdqa  xmm1, [esi]


        /* Load 8x32bits of the rounding values */
        movdqa      xmm3, [eax]
        movdqa      xmm4, [eax + 16]


        /* Shrnk them back to 16 bits */
        packssdw    xmm3, xmm4


        /* Load 8x32bits of the coeff values */
        movdqa      xmm4, [edx]
        movdqa      xmm5, [edx + 16]

        /* Shirnk the coeffs back to 16 bits */
        packssdw    xmm4, xmm5


        /* Add the rounding to the dct in one register */
        movdqa      xmm2, xmm1
        paddd       xmm1, xmm3

        /* Subtract in another */
        psubd       xmm2, xmm3

        /* Multiply both suma nd diff by the coeffs, keeping only the high word 
            (since in the C code we shift right by 16 bits ) */
        pmulhw      xmm1, xmm4
        pmulhw      xmm2, xmm4      /* TODO::: In the subtraction, have too do the round by adding 65535 */


        /* Now need to do the gt checks and mask in the appropriate result */
            /* Check the summed results to see if any are over 511 */

                /* Duplicate the summed values */
                movdqa      xmm5, xmm1

                /* Compare each word to 511, any that are >511 will have their word set to all 1s */
                pcmpgtw     xmm5, xmm7

                /* CHECK INSERTED CODE, to save reloading xmm7, assumes xmm3 was not holding a value at this point */
                movdqa      xmm3, xmm7

                /* Use the mask created to make a register with 511 in every place the original sum was >511 and 0 elsewhere */
                pand        xmm3, xmm5

                /* Flip the bits in the mask */
                pxor        xmm5, xmm6

                /* Now register has all the values that were less than or eq to 511 intact, but every where else is 0 */
                pand        xmm1, xmm5

                /* Now combine all the vals lt or eq to 511, from the original sums, with the register that has 511
                        in all the places where the value was >511. This effectively performs an upper bound clipping.
                        So every value that was >511 is now set to 511 */
                por         xmm1, xmm3

            /* Now do similar for the differences, to clip any value less than -511 */
                /* Duplicate the differenced values */
                movdqa      xmm5, xmm2

               
                /* Subtract the 511s from 0 to get -511's */

                psubw       xmm0, xmm7

                /* See if -511 is greater than the value. If it is that word is all 1's. This is effectively
                    a check to see if the value is less or equal to -511. Since the operation is the same for the
                    equal case, it doesn't matter that we check for less or equal rather than just less than */


                movdqa      xmm3, xmm0          /* mm3 is now a duplicate of the -511's */
                pcmpgtw     xmm0, xmm5      

                /* Create a mask on the -511s */
                pand        xmm3, xmm0

                /* Flip the bits in the mask */
                pxor        xmm0, xmm6

                /* Mask the values in the difference register */
                pand        xmm2, xmm0

                /* Combine them together */
                por         xmm2, xmm3



        /* By here, xmm1 contains the clipped 8 values of the sum, and xmm2 the clipped 8 values of the difference */

        
        pxor        xmm0, xmm0
        //mov         eax, FquantZBinSizePtr

        /* Load 8x32bits of the fquantzbin values */
        movdqa      xmm3, [ecx]
        movdqa      xmm4, [ecx + 16]


        /* Shrnk them back to 16 bits */
        packssdw    xmm3, xmm4

        /* Load the DCT Block values 8x16 bits again */
        movdqa      xmm4, [esi]

        /* Find -Fqauntzbin by subtract it from 0 */
        psubw       xmm0, xmm3

        /* Check if fquantzbin is greater dct_block value. if it's not, then use the summed register value */
        pcmpgtw     xmm3, xmm4


        /* Flip the mask */
        pxor        xmm3, xmm6

        /* Copy the mask to save for later */
        movdqa      xmm5, xmm3

        /* And the summed value regsiter */
        pand        xmm1, xmm3


        /* Check if -fquantzbin is greater than dct_block. If it is, use the difference register value */
        pcmpgtw     xmm0, xmm4

        /* Or the mask with the other mask, so we have the combination, of all those using the sum and all those
                using the difference, everything else will be set to zero later */
        por         xmm5, xmm0

        /* And the difference register to mask the appropriate values */
        pand        xmm2, xmm0

        /* Merge together the selected summed and differenced values */
        por         xmm1, xmm2

        /* Zero out everything that wasn't selected by the sum mask or the difference mask */
        pand        xmm1, xmm5



        /* Write these values out to the temp_space, after all 8 loops, 64 x 16 bit values are written,
            later we can apply the zigzag write all at once */
        movdqa      [edi], xmm1


        /* Increment the pointer */
        add         edi, 16     /* Temp output by 16 bytes */
        add         esi, 16     /* DCT_Block by 16 bytes */
        add         edx, 32     /* Fquantcoeffs by 32 bytes */
        add         eax, 32     /* fquant round ptr by 32 bytes */
        add         ecx, 32     /* fquant zbin by 32 bytes */

    /* Update the loop variable */
    sub         ebx, 1
    jnz         read_loop_start
        


        /* Now read through the temp output space and write using the zigpag pointer values */
        

        mov     edx, quantized_list
        mov     esi, ZigZagPtr
        mov     ebx, 8
        
        /* Put the temp output back to the start of the block */
        sub     edi, 128

    write_loop_start:
        mov     ax, WORD PTR [edi]
        mov     ecx, [esi]
        mov     WORD PTR [edx + ecx*2], ax

        mov     ax, WORD PTR [edi+2]
        mov     ecx, [esi+4]
        mov     WORD PTR [edx + ecx*2], ax

        mov     ax, WORD PTR [edi+4]
        mov     ecx, [esi+8]
        mov     WORD PTR [edx + ecx*2], ax

        mov     ax, WORD PTR [edi+6]
        mov     ecx, [esi+12]
        mov     WORD PTR [edx + ecx*2], ax


        mov     ax, WORD PTR [edi+8]
        mov     ecx, [esi+16]
        mov     WORD PTR [edx + ecx*2], ax

        mov     ax, WORD PTR [edi+10]
        mov     ecx, [esi+20]
        mov     WORD PTR [edx + ecx*2], ax

        mov     ax, WORD PTR [edi+12]
        mov     ecx, [esi+24]
        mov     WORD PTR [edx + ecx*2], ax

        mov     ax, WORD PTR [edi+14]
        mov     ecx, [esi+28]
        mov     WORD PTR [edx + ecx*2], ax

            /* Advance all the pointer */
            add     esi, 32
            add     edi, 16

    /* Check the loop counter */
    sub         ebx, 1
    jnz         write_loop_start


    /* Restore ebx */
    pop         ebx

  }

  //PERF_BLOCK_END("quantize sse2", perf_quant_time, perf_quant_count, perf_quant_min, 20000);

#endif
}


void dsp_sse2_quant_init(DspFunctions *funcs)
{
#ifndef USE_NO_SSE2
  TH_DEBUG("enabling accelerated x86_32 sse2 quant functions.\n");
  perf_quant_time = 0;
  perf_quant_min = -1;
  perf_quant_count = 0;
  funcs->quantize = quantize__sse2;

#endif
  //funcs->copy8x8 = copy8x8__sse2;
  //funcs->recon_intra8x8 = recon_intra8x8__sse2;
  //funcs->recon_inter8x8 = recon_inter8x8__sse2;
  //funcs->recon_inter8x8_half = recon_inter8x8_half__sse2;
}