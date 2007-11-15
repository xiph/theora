/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id: reconstruct.c,v 1.6 2003/12/03 08:59:41 arc Exp $

 ********************************************************************/

#include "codec_internal.h"


static const unsigned __int64 V128 = 0x8080808080808080;

static void copy8x8__mmx (unsigned char *src,
	                unsigned char *dest,
	                unsigned int stride)
{

    //Is this even the fastest way to do this?
    __asm {
        align 16        

        mov         eax, src
        mov         ebx, dest
        mov         ecx, stride

        lea		    edi, [ecx + ecx * 2]
        movq		mm0, [eax]
        movq		mm1, [eax + ecx]
        movq		mm2, [eax + ecx * 2]
        movq		mm3, [eax + edi]
        lea		    eax, [eax + ecx * 4]
        movq		[ebx], mm0
        movq		[ebx + ecx], mm1
        movq		[ebx + ecx * 2], mm2
        movq		[ebx + edi], mm3
        lea		    ebx, [ebx + ecx * 4]
        movq		mm0, [eax]
        movq		mm1, [eax + ecx]
        movq		mm2, [eax + ecx * 2]
        movq		mm3, [eax + edi]
        movq		[ebx], mm0
        movq		[ebx + ecx], mm1
        movq		[ebx + ecx * 2], mm2
        movq		[ebx + edi], mm3

    };

}

static void recon8x8__mmx (unsigned char *ReconPtr, 
			   ogg_int16_t *ChangePtr, ogg_uint32_t LineStep)
{

    __asm {

        align 16

        mov         eax, ReconPtr
        mov         ebx, ChangePtr
        mov         ecx, LineStep
    
        pxor		mm0, mm0
        lea		    edi, [128 + ebx]

    loop_start:
        movq		mm2, [eax]

        movq		mm4, [ebx]
        movq		mm3, mm2
        movq		mm5, [8 + ebx]

        punpcklbw	mm2, mm0
        paddsw		mm2, mm4
        punpckhbw	mm3, mm0
        paddsw		mm3, mm5
        packuswb	mm2, mm3
        lea		    ebx, [16 + ebx]
        cmp		    ebx, edi

        movq		[eax], mm2

        lea		    eax, [eax + ecx]
        jc		    loop_start

    };
}

void dsp_mmx_recon_init(DspFunctions *funcs)
{
  TH_DEBUG("enabling accelerated x86_32 mmx recon functions.\n");
  funcs->copy8x8 = copy8x8__mmx;
  funcs->recon8x8 = recon8x8__mmx;
}

