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
#include <stddef.h>
#include "x86enc.h"

#if defined(OC_X86_ASM)

static int find_nonzero__sse2(ogg_int16_t *q, int in){
  int ret,tmp,tmp2;

  __asm__ (
	   ".p2align 4 \n"
	   "movd      %[in],%%xmm0\n"
	   "punpcklwd %%xmm0,%%xmm0\n"
	   "punpcklwd %%xmm0,%%xmm0\n"
	   "punpcklwd %%xmm0,%%xmm0\n"
	   
	   "movdqu    64(%[quant]),%%xmm1\n"
	   "pcmpgtw   %%xmm0,%%xmm1\n"
	   "movdqu    80(%[quant]),%%xmm2\n"
	   "pcmpgtw   %%xmm0,%%xmm2\n"
	   "packsswb  %%xmm2,%%xmm1\n"

	   "movdqu    96(%[quant]),%%xmm2\n"
	   "pcmpgtw   %%xmm0,%%xmm2\n"
	   "movdqu    112(%[quant]),%%xmm3\n"
	   "pcmpgtw   %%xmm0,%%xmm3\n"
	   "packsswb  %%xmm3,%%xmm2\n"

	   "pmovmskb  %%xmm1,%[ret]\n"
	   "pmovmskb  %%xmm2,%[tmp]\n"
	   "shl       $16,%[tmp]\n"
	   "or        %[tmp],%[ret]\n"
	   "bsr       %[ret],%[ret]\n"
	   "jz        %=1f\n"
	   "add       $33,%[ret]\n"
	   "jmp       %=3f\n"

	   "%=1:\n"
	   "movdqu    (%[quant]),%%xmm1\n"
	   "pcmpgtw   %%xmm0,%%xmm1\n"
	   "movdqu    16(%[quant]),%%xmm2\n"
	   "pcmpgtw   %%xmm0,%%xmm2\n"
	   "packsswb  %%xmm2,%%xmm1\n"

	   "movdqu    32(%[quant]),%%xmm2\n"
	   "pcmpgtw   %%xmm0,%%xmm2\n"
	   "movdqu    48(%[quant]),%%xmm3\n"
	   "pcmpgtw   %%xmm0,%%xmm3\n"
	   "packsswb  %%xmm3,%%xmm2\n"

	   "pmovmskb  %%xmm1,%[ret]\n"
	   "pmovmskb  %%xmm2,%[tmp]\n"
	   "shl       $16,%[tmp]\n"
	   "or        %[tmp],%[ret]\n"
	   "bsr       %[ret],%[ret]\n"
	   "jz        %=2f\n"
	   "inc       %[ret]\n"
	   "jmp       %=3f\n"

	   "%=2:\n"
	   "xor       %[ret],%[ret]\n"

	   "%=3:\n"

	   :[ret]"=&r"(ret),[tmp]"=&r"(tmp),[tmp2]"=&r"(tmp2)
	   :[quant]"r"(q),[in]"r"(in)
	   );

  return ret;
}

void dsp_sse2_init(DspFunctions *funcs)
{
  funcs->find_nonzero = find_nonzero__sse2;
}

#endif /* USE_ASM */
