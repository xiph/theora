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

  function: SSE2 implementation of the Theora iDCT
  last mod: $Id: idct_sse2.c 11513 2006-06-04 09:46:34Z illiminable $

 ********************************************************************/

#include <string.h>
#include "codec_internal.h"
#include "quant_lookup.h"

#define IdctAdjustBeforeShift 8

/* cos(n*pi/16) or sin(8-n)*pi/16) */
#define xC1S7 64277
#define xC2S6 60547
#define xC3S5 54491
#define xC4S4 46341
#define xC5S3 36410
#define xC6S2 25080
#define xC7S1 12785



static void dequant_slow__sse2( ogg_int16_t * dequant_coeffs,
                   ogg_int16_t * quantized_list,
                   ogg_int32_t * DCT_block) 
{
#if 1
  int i;
  for(i=0;i<64;i++)
    DCT_block[dezigzag_index[i]] = quantized_list[i] * dequant_coeffs[i];
#else

    static __declspec(align(16)) ogg_int32_t temp_block[64];
    static ogg_int32_t* temp_block_ptr = temp_block;
    static ogg_int32_t* zigzag_ptr = dezigzag_index;

    /*      quantized list is not aligned */


    __asm {
        align       16

        mov     edi, DCT_block          /* int32 */
        mov     edx, zigzag_ptr          /* int32 */
        mov     esi, quantized_list     /* int16 */
        mov     ebx, dequant_coeffs     /* int16 */
        mov     eax, temp_block_ptr

        /* 16 Iterations at a time  */
        mov         ecx, 4      /* 4 lots of 16 */

        calc_loop_start:
            /* Read 16x16 bits of quatized_list and dequant_coeffs */
            movdqu      xmm1, [esi]
            movdqu      xmm5, [esi + 16]

            movdqa      xmm2, [ebx]
            movdqa      xmm6, [ebx + 16]

            /* Make a copy of xmm1 and xmm5 */
            movdqa      xmm7, xmm1
            movdqa      xmm0, xmm5

            /* Multiply */
            pmullw      xmm1, xmm2
            pmulhw      xmm2, xmm7

            pmullw      xmm5, xmm6
            pmulhw      xmm6, xmm0

            /* Interleave the multiplicataion results */
            movdqa      xmm0, xmm1
            punpcklwd   xmm1, xmm2      /* Now the low 4 x 32 bits */
            punpckhwd   xmm0, xmm2      /* The high 4x32 bits */

            movdqa      xmm2, xmm5
            punpcklwd   xmm5, xmm6
            punpckhwd   xmm2, xmm6

            /* Write the 16x32 bits of output to temp space */
            movdqa      [eax], xmm1
            movdqa      [eax + 16], xmm0
            movdqa      [eax + 32], xmm5
            movdqa      [eax + 48], xmm2

            /* Update the pointers */
            add         esi, 32
            add         ebx, 32
            add         eax, 64

        /* Loop check */
        sub         ecx, 1
        jnz         calc_loop_start

    /* Restore the pointer to the start of the temp buffer */
    sub         eax, 256


    /* Now follow the pattern to write - can't use simd */
    mov         ebx, 8

    write_loop_start:
        mov         ecx         , [edx]
        mov         esi         , [eax]
        mov         [edi + ecx*4] , esi
        mov         ecx         , [edx + 4]
        mov         esi         , [eax + 4]
        mov         [edi + ecx*4] , esi
        mov         ecx         , [edx + 8]
        mov         esi         , [eax + 8]
        mov         [edi + ecx*4] , esi
        mov         ecx         , [edx + 12]
        mov         esi         , [eax + 12]
        mov         [edi + ecx*4] , esi

        mov         ecx         , [edx + 16]
        mov         esi         , [eax + 16]
        mov         [edi + ecx*4] , esi
        mov         ecx         , [edx + 20]
        mov         esi         , [eax + 20]
        mov         [edi + ecx*4] , esi
        mov         ecx         , [edx + 24]
        mov         esi         , [eax + 24]
        mov         [edi + ecx*4] , esi
        mov         ecx         , [edx + 28]
        mov         esi         , [eax + 28]
        mov         [edi + ecx*4] , esi

        /* Update the pointers */
        add         eax, 32
        add         edx, 32

    /* Check the loop */
    sub         ebx, 1
    jnz         write_loop_start

    };
#endif
}



void IDctSlow__sse2(  Q_LIST_ENTRY * InputData,
                ogg_int16_t *QuantMatrix,
                ogg_int16_t * OutputData ) {
  __declspec(align(16)) ogg_int32_t IntermediateData[64];
  ogg_int32_t * ip = IntermediateData;
  ogg_int16_t * op = OutputData;

  ogg_int32_t _A, _B, _C, _D, _Ad, _Bd, _Cd, _Dd, _E, _F, _G, _H;
  ogg_int32_t _Ed, _Gd, _Add, _Bdd, _Fd, _Hd;
  ogg_int32_t t1, t2;

  int loop;

  dequant_slow__sse2( QuantMatrix, InputData, IntermediateData);

  /* Inverse DCT on the rows now */
  for ( loop = 0; loop < 8; loop++){
    /* Check for non-zero values */
    if ( ip[0] | ip[1] | ip[2] | ip[3] | ip[4] | ip[5] | ip[6] | ip[7] ) {
      t1 = (xC1S7 * ip[1]);
      t2 = (xC7S1 * ip[7]);
      t1 >>= 16;
      t2 >>= 16;
      _A = t1 + t2;

      t1 = (xC7S1 * ip[1]);
      t2 = (xC1S7 * ip[7]);
      t1 >>= 16;
      t2 >>= 16;
      _B = t1 - t2;

      t1 = (xC3S5 * ip[3]);
      t2 = (xC5S3 * ip[5]);
      t1 >>= 16;
      t2 >>= 16;
      _C = t1 + t2;

      t1 = (xC3S5 * ip[5]);
      t2 = (xC5S3 * ip[3]);
      t1 >>= 16;
      t2 >>= 16;
      _D = t1 - t2;

      t1 = (xC4S4 * (ogg_int16_t)(_A - _C));
      t1 >>= 16;
      _Ad = t1;

      t1 = (xC4S4 * (ogg_int16_t)(_B - _D));
      t1 >>= 16;
      _Bd = t1;


      _Cd = _A + _C;
      _Dd = _B + _D;

      t1 = (xC4S4 * (ogg_int16_t)(ip[0] + ip[4]));
      t1 >>= 16;
      _E = t1;

      t1 = (xC4S4 * (ogg_int16_t)(ip[0] - ip[4]));
      t1 >>= 16;
      _F = t1;

      t1 = (xC2S6 * ip[2]);
      t2 = (xC6S2 * ip[6]);
      t1 >>= 16;
      t2 >>= 16;
      _G = t1 + t2;

      t1 = (xC6S2 * ip[2]);
      t2 = (xC2S6 * ip[6]);
      t1 >>= 16;
      t2 >>= 16;
      _H = t1 - t2;


      _Ed = _E - _G;
      _Gd = _E + _G;

      _Add = _F + _Ad;
      _Bdd = _Bd - _H;

      _Fd = _F - _Ad;
      _Hd = _Bd + _H;

      /* Final sequence of operations over-write original inputs. */
      ip[0] = (ogg_int16_t)((_Gd + _Cd )   >> 0);
      ip[7] = (ogg_int16_t)((_Gd - _Cd )   >> 0);

      ip[1] = (ogg_int16_t)((_Add + _Hd )  >> 0);
      ip[2] = (ogg_int16_t)((_Add - _Hd )  >> 0);

      ip[3] = (ogg_int16_t)((_Ed + _Dd )   >> 0);
      ip[4] = (ogg_int16_t)((_Ed - _Dd )   >> 0);

      ip[5] = (ogg_int16_t)((_Fd + _Bdd )  >> 0);
      ip[6] = (ogg_int16_t)((_Fd - _Bdd )  >> 0);

    }

    ip += 8;                    /* next row */
  }

  ip = IntermediateData;

  for ( loop = 0; loop < 8; loop++){
    /* Check for non-zero values (bitwise or faster than ||) */
    if ( ip[0 * 8] | ip[1 * 8] | ip[2 * 8] | ip[3 * 8] |
         ip[4 * 8] | ip[5 * 8] | ip[6 * 8] | ip[7 * 8] ) {

      t1 = (xC1S7 * ip[1*8]);
      t2 = (xC7S1 * ip[7*8]);
      t1 >>= 16;
      t2 >>= 16;
      _A = t1 + t2;

      t1 = (xC7S1 * ip[1*8]);
      t2 = (xC1S7 * ip[7*8]);
      t1 >>= 16;
      t2 >>= 16;
      _B = t1 - t2;

      t1 = (xC3S5 * ip[3*8]);
      t2 = (xC5S3 * ip[5*8]);
      t1 >>= 16;
      t2 >>= 16;
      _C = t1 + t2;

      t1 = (xC3S5 * ip[5*8]);
      t2 = (xC5S3 * ip[3*8]);
      t1 >>= 16;
      t2 >>= 16;
      _D = t1 - t2;

      t1 = (xC4S4 * (ogg_int16_t)(_A - _C));
      t1 >>= 16;
      _Ad = t1;

      t1 = (xC4S4 * (ogg_int16_t)(_B - _D));
      t1 >>= 16;
      _Bd = t1;


      _Cd = _A + _C;
      _Dd = _B + _D;

      t1 = (xC4S4 * (ogg_int16_t)(ip[0*8] + ip[4*8]));
      t1 >>= 16;
      _E = t1;

      t1 = (xC4S4 * (ogg_int16_t)(ip[0*8] - ip[4*8]));
      t1 >>= 16;
      _F = t1;

      t1 = (xC2S6 * ip[2*8]);
      t2 = (xC6S2 * ip[6*8]);
      t1 >>= 16;
      t2 >>= 16;
      _G = t1 + t2;

      t1 = (xC6S2 * ip[2*8]);
      t2 = (xC2S6 * ip[6*8]);
      t1 >>= 16;
      t2 >>= 16;
      _H = t1 - t2;

      _Ed = _E - _G;
      _Gd = _E + _G;

      _Add = _F + _Ad;
      _Bdd = _Bd - _H;

      _Fd = _F - _Ad;
      _Hd = _Bd + _H;

      _Gd += IdctAdjustBeforeShift;
      _Add += IdctAdjustBeforeShift;
      _Ed += IdctAdjustBeforeShift;
      _Fd += IdctAdjustBeforeShift;

      /* Final sequence of operations over-write original inputs. */
      op[0*8] = (ogg_int16_t)((_Gd + _Cd )   >> 4);
      op[7*8] = (ogg_int16_t)((_Gd - _Cd )   >> 4);

      op[1*8] = (ogg_int16_t)((_Add + _Hd )  >> 4);
      op[2*8] = (ogg_int16_t)((_Add - _Hd )  >> 4);

      op[3*8] = (ogg_int16_t)((_Ed + _Dd )   >> 4);
      op[4*8] = (ogg_int16_t)((_Ed - _Dd )   >> 4);

      op[5*8] = (ogg_int16_t)((_Fd + _Bdd )  >> 4);
      op[6*8] = (ogg_int16_t)((_Fd - _Bdd )  >> 4);
    }else{
      op[0*8] = 0;
      op[7*8] = 0;
      op[1*8] = 0;
      op[2*8] = 0;
      op[3*8] = 0;
      op[4*8] = 0;
      op[5*8] = 0;
      op[6*8] = 0;
    }

    ip++;                       /* next column */
    op++;
  }
}

/************************
  x  x  x  x  0  0  0  0
  x  x  x  0  0  0  0  0
  x  x  0  0  0  0  0  0
  x  0  0  0  0  0  0  0
  0  0  0  0  0  0  0  0
  0  0  0  0  0  0  0  0
  0  0  0  0  0  0  0  0
  0  0  0  0  0  0  0  0
*************************/

static void dequant_slow10__sse2( ogg_int16_t * dequant_coeffs,
                     ogg_int16_t * quantized_list,
                     ogg_int32_t * DCT_block){
  int i;
  memset(DCT_block,0, 128);
  for(i=0;i<10;i++)
    DCT_block[dezigzag_index[i]] = quantized_list[i] * dequant_coeffs[i];

}

void IDct10__sse2( Q_LIST_ENTRY * InputData,
             ogg_int16_t *QuantMatrix,
             ogg_int16_t * OutputData ){
  ogg_int32_t IntermediateData[64];
  ogg_int32_t * ip = IntermediateData;
  ogg_int16_t * op = OutputData;

  ogg_int32_t _A, _B, _C, _D, _Ad, _Bd, _Cd, _Dd, _E, _F, _G, _H;
  ogg_int32_t _Ed, _Gd, _Add, _Bdd, _Fd, _Hd;
  ogg_int32_t t1, t2;

  int loop;

  dequant_slow10__sse2( QuantMatrix, InputData, IntermediateData);

  /* Inverse DCT on the rows now */
  for ( loop = 0; loop < 4; loop++){
    /* Check for non-zero values */
    if ( ip[0] | ip[1] | ip[2] | ip[3] ){
      t1 = (xC1S7 * ip[1]);
      t1 >>= 16;
      _A = t1;

      t1 = (xC7S1 * ip[1]);
      t1 >>= 16;
      _B = t1 ;

      t1 = (xC3S5 * ip[3]);
      t1 >>= 16;
      _C = t1;

      t2 = (xC5S3 * ip[3]);
      t2 >>= 16;
      _D = -t2;


      t1 = (xC4S4 * (ogg_int16_t)(_A - _C));
      t1 >>= 16;
      _Ad = t1;

      t1 = (xC4S4 * (ogg_int16_t)(_B - _D));
      t1 >>= 16;
      _Bd = t1;


      _Cd = _A + _C;
      _Dd = _B + _D;

      t1 = (xC4S4 * ip[0] );
      t1 >>= 16;
      _E = t1;

      _F = t1;

      t1 = (xC2S6 * ip[2]);
      t1 >>= 16;
      _G = t1;

      t1 = (xC6S2 * ip[2]);
      t1 >>= 16;
      _H = t1 ;


      _Ed = _E - _G;
      _Gd = _E + _G;

      _Add = _F + _Ad;
      _Bdd = _Bd - _H;

      _Fd = _F - _Ad;
      _Hd = _Bd + _H;

      /* Final sequence of operations over-write original inputs. */
      ip[0] = (ogg_int16_t)((_Gd + _Cd )   >> 0);
      ip[7] = (ogg_int16_t)((_Gd - _Cd )   >> 0);

      ip[1] = (ogg_int16_t)((_Add + _Hd )  >> 0);
      ip[2] = (ogg_int16_t)((_Add - _Hd )  >> 0);

      ip[3] = (ogg_int16_t)((_Ed + _Dd )   >> 0);
      ip[4] = (ogg_int16_t)((_Ed - _Dd )   >> 0);

      ip[5] = (ogg_int16_t)((_Fd + _Bdd )  >> 0);
      ip[6] = (ogg_int16_t)((_Fd - _Bdd )  >> 0);

    }

    ip += 8;                    /* next row */
  }

  ip = IntermediateData;

  for ( loop = 0; loop < 8; loop++) {
    /* Check for non-zero values (bitwise or faster than ||) */
    if ( ip[0 * 8] | ip[1 * 8] | ip[2 * 8] | ip[3 * 8] ) {

      t1 = (xC1S7 * ip[1*8]);
      t1 >>= 16;
      _A = t1 ;

      t1 = (xC7S1 * ip[1*8]);
      t1 >>= 16;
      _B = t1 ;

      t1 = (xC3S5 * ip[3*8]);
      t1 >>= 16;
      _C = t1 ;

      t2 = (xC5S3 * ip[3*8]);
      t2 >>= 16;
      _D = - t2;


      t1 = (xC4S4 * (ogg_int16_t)(_A - _C));
      t1 >>= 16;
      _Ad = t1;

      t1 = (xC4S4 * (ogg_int16_t)(_B - _D));
      t1 >>= 16;
      _Bd = t1;


      _Cd = _A + _C;
      _Dd = _B + _D;

      t1 = (xC4S4 * ip[0*8]);
      t1 >>= 16;
      _E = t1;
      _F = t1;

      t1 = (xC2S6 * ip[2*8]);
      t1 >>= 16;
      _G = t1;

      t1 = (xC6S2 * ip[2*8]);
      t1 >>= 16;
      _H = t1;


      _Ed = _E - _G;
      _Gd = _E + _G;

      _Add = _F + _Ad;
      _Bdd = _Bd - _H;

      _Fd = _F - _Ad;
      _Hd = _Bd + _H;

      _Gd += IdctAdjustBeforeShift;
      _Add += IdctAdjustBeforeShift;
      _Ed += IdctAdjustBeforeShift;
      _Fd += IdctAdjustBeforeShift;

      /* Final sequence of operations over-write original inputs. */
      op[0*8] = (ogg_int16_t)((_Gd + _Cd )   >> 4);
      op[7*8] = (ogg_int16_t)((_Gd - _Cd )   >> 4);

      op[1*8] = (ogg_int16_t)((_Add + _Hd )  >> 4);
      op[2*8] = (ogg_int16_t)((_Add - _Hd )  >> 4);

      op[3*8] = (ogg_int16_t)((_Ed + _Dd )   >> 4);
      op[4*8] = (ogg_int16_t)((_Ed - _Dd )   >> 4);

      op[5*8] = (ogg_int16_t)((_Fd + _Bdd )  >> 4);
      op[6*8] = (ogg_int16_t)((_Fd - _Bdd )  >> 4);
    }else{
      op[0*8] = 0;
      op[7*8] = 0;
      op[1*8] = 0;
      op[2*8] = 0;
      op[3*8] = 0;
      op[4*8] = 0;
      op[5*8] = 0;
      op[6*8] = 0;
    }

    ip++;                       /* next column */
    op++;
  }
}

/***************************
  x   0   0  0  0  0  0  0
  0   0   0  0  0  0  0  0
  0   0   0  0  0  0  0  0
  0   0   0  0  0  0  0  0
  0   0   0  0  0  0  0  0
  0   0   0  0  0  0  0  0
  0   0   0  0  0  0  0  0
  0   0   0  0  0  0  0  0
**************************/

void IDct1__sse2( Q_LIST_ENTRY * InputData,
            ogg_int16_t *QuantMatrix,
            ogg_int16_t * OutputData ){
  int loop;

  ogg_int16_t  OutD;

  OutD=(ogg_int16_t) ((ogg_int32_t)(InputData[0]*QuantMatrix[0]+15)>>5);

  for(loop=0;loop<64;loop++)
    OutputData[loop]=OutD;

}


void dsp_sse2_idct_init (DspFunctions *funcs)
{
    /* TODO::: Match function order */
  funcs->dequant_slow = dequant_slow__sse2;
  funcs->IDct1 = IDct1__sse2;
  funcs->IDct10 = IDct10__sse2;
  funcs->dequant_slow10 = dequant_slow10__sse2;
  funcs->IDctSlow = IDctSlow__sse2;
  funcs->dequant_slow = dequant_slow__sse2;

}