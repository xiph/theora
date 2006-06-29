

#include "codec_internal.h"
#include "dsp.h"
#include "cpu.h"

#include "perf_helper.h"

//static __declspec(align(16)) const unsigned int PixMask[4] = { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF };
//static const unsigned int* PixMaskPtr = PixMask;
//
//static __declspec(align(16)) const unsigned int TripleMask[4] = { 0x0000FFFF, 0xFFFF0000, 0x0000FFFF, 0xFFFF0000 };
//static const unsigned int* PTripleMaskPtr = TripleMask;



static perf_info filter_horiz_perf;

static void FilterHoriz__sse2(unsigned char * PixelPtr,
                        ogg_int32_t LineLength,
                        ogg_int32_t *BoundingValuePtr){

#if 0
  
  

  ogg_int32_t j;
  ogg_int32_t FiltVal;
  PERF_BLOCK_START();
  for ( j = 0; j < 8; j++ ){
    FiltVal =
      ( PixelPtr[0] ) -
      ( PixelPtr[1] * 3 ) +
      ( PixelPtr[2] * 3 ) -
      ( PixelPtr[3] );

    FiltVal = *(BoundingValuePtr+((FiltVal + 4) >> 3));

    PixelPtr[1] = clamp255(PixelPtr[1] + FiltVal);
    PixelPtr[2] = clamp255(PixelPtr[2] - FiltVal);

    PixelPtr += LineLength;
    
  }
  PERF_BLOCK_END("filter horiz C", filter_horiz_perf, 10000);

#else
    static __declspec(align(16)) unsigned char temp[128];
    static unsigned char* temp_ptr = temp; 

  PERF_BLOCK_START();
    __asm {
        align           16
        mov             esi, PixelPtr
        mov             edi, temp_ptr
        mov             ecx, BoundingValuePtr

        push            ebp
        push            ebx
        mov             ebp, LineLength
        mov             ebx, 8
        
        /* It */     
    loop_start:
        movzx           eax, BYTE PTR [esi]
        mov             [edi+64], ax

        movzx           edx, BYTE PTR [esi+1]
        mov             [edi+66], dx
        sub             eax, edx
        add             edx, edx
        sub             eax, edx

        movzx           edx, BYTE PTR [esi+2]
        mov             [edi+68], dx
        add             eax, edx
        add             edx, edx
        add             eax, edx

        movzx           edx, BYTE PTR [esi+3]
        mov             [edi+70], dx
        sub             eax, edx

        add             eax, 4
        sar             eax, 3
        sal             eax, 2

        mov             eax, [eax + ecx]

        mov             WORD PTR [edi], 0
        mov             [edi + 2], ax
        neg             ax
        mov             [edi + 4], ax
        mov             WORD PTR [edi + 6], 0

        

        add             edi, 8
        add             esi, ebp

        sub             ebx, 1
        jnz     loop_start

        sub             edi, 64
        shl             ebp, 3
        sub             esi, ebp
        shr             ebp, 3

        movdqa          xmm1, [edi]
        movdqa          xmm2, [edi + 16]
        movdqa          xmm3, [edi + 32]
        movdqa          xmm4, [edi + 48]


        movdqa          xmm5, [edi + 64]
        movdqa          xmm6, [edi + 80]
        movdqa          xmm7, [edi + 96]
        movdqa          xmm0, [edi + 112]

        paddsw            xmm1, xmm5
        paddsw            xmm2, xmm6
        paddsw            xmm3, xmm7
        paddsw            xmm4, xmm0

        packuswb        xmm1, xmm1
        movdqa          [edi], xmm1
        packuswb        xmm2, xmm2
        movdqa          [edi + 16], xmm2
        packuswb        xmm3, xmm3
        movdqa          [edi + 32], xmm3
        packuswb        xmm4, xmm4

        movdqa          [edi + 48], xmm4



        mov             ebx, 4
    write_loop_start:
        mov             eax, [edi]
        mov             edx, [edi + 4]
        mov             [esi], eax
        mov             [esi + ebp], edx

        lea             esi, [esi + 2*ebp]
        add             edi, 16

        sub             ebx, 1
        jnz     write_loop_start



        pop             ebx
        pop             ebp
    }
    
	PERF_BLOCK_END("filter horiz sse2", filter_horiz_perf, 10000);
#endif
}

static void FilterVert__sse2(unsigned char * PixelPtr,
                ogg_int32_t LineLength,
                ogg_int32_t *BoundingValuePtr){
  ogg_int32_t j;
  ogg_int32_t FiltVal;

  /* the math was correct, but negative array indicies are forbidden
     by ANSI/C99 and will break optimization on several modern
     compilers */

  PixelPtr -= 2*LineLength;

  for ( j = 0; j < 8; j++ ) {
    FiltVal = ( (ogg_int32_t)PixelPtr[0] ) -
      ( (ogg_int32_t)PixelPtr[LineLength] * 3 ) +
      ( (ogg_int32_t)PixelPtr[2 * LineLength] * 3 ) -
      ( (ogg_int32_t)PixelPtr[3 * LineLength] );

    FiltVal = *(BoundingValuePtr+((FiltVal + 4) >> 3));

    PixelPtr[LineLength] = clamp255(PixelPtr[LineLength] + FiltVal);
    PixelPtr[2 * LineLength] = clamp255(PixelPtr[2*LineLength] - FiltVal);

    PixelPtr ++;
  }
}

void dsp_sse2_dct_decode_init(DspFunctions *funcs)
{
  TH_DEBUG("enabling accelerated x86_32 mmx dsp functions.\n");

  ClearPerfData(&filter_horiz_perf);
  funcs->FilterHoriz = FilterHoriz__sse2;

}