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
/*CPU capability detection for x86 processors.
  Originally written by Rudolf Marek.*/

#include "cpu.h"


ogg_uint32_t oc_cpu_flags_get(void){
  ogg_uint32_t flags = 0;
  ogg_uint32_t eax;
  ogg_uint32_t ebx;
  ogg_uint32_t ecx;
  ogg_uint32_t edx;
#if defined(USE_ASM)
#if (defined(__amd64__) || defined(__x86_64__))
# define cpuid(_op,_eax,_ebx,_ecx,_edx) \
  __asm__ __volatile__( \
   "push %%rbx\n\t" \
   "cpuid\n\t" \
   "movl %%ebx,%1\n\t" \
   "pop  %%rbx\n\t" \
   :"=a" (_eax), \
    "=r" (_ebx), \
    "=c" (_ecx), \
    "=d" (_edx) \
   :"a" (_op) \
   :"cc" \
  )
#else
# define cpuid(_op,_eax,_ebx,_ecx,_edx) \
  __asm__ __volatile__( \
   "pushl %%ebx\n\t" \
   "cpuid\n\t" \
   "movl  %%ebx,%1\n\t" \
   "popl  %%ebx\n\t" \
   :"=a" (_eax), \
    "=r" (_ebx), \
    "=c" (_ecx), \
    "=d" (_edx) \
   :"a" (_op) \
   :"cc" \
  )
  __asm__ __volatile__(
   "pushfl\n\t"
   "pushfl\n\t"
   "popl          %0\n\t"
   "movl          %0,%1\n\t"
   "xorl   $0x200000,%0\n\t"
   "pushl         %0\n\t"
   "popfl\n\t"
   "pushfl\n\t"
   "popl          %0\n\t"
   "popfl\n\t"
   :"=r" (eax),
    "=r" (ebx)
   :
   :"cc"
  );
  /*No cpuid.*/
  if(eax==ebx)return 0;
#endif
  cpuid(0,eax,ebx,ecx,edx);
  if(ebx==0x756e6547&&edx==0x49656e69&&ecx==0x6c65746e){
    /*Intel:*/
inteltest:
    cpuid(1,eax,ebx,ecx,edx);
    if((edx&0x00800000)==0)return 0;
    flags=OC_CPU_X86_MMX;
    if(edx&0x02000000)flags|=OC_CPU_X86_MMXEXT|OC_CPU_X86_SSE;
    if(edx&0x04000000)flags|=OC_CPU_X86_SSE2;
  }
  else if(ebx==0x68747541&&edx==0x69746e65&&ecx==0x444d4163){
    /*AMD:*/
    cpuid(0x80000000,eax,ebx,ecx,edx);
    if(eax<0x80000001)goto inteltest;
    cpuid(0x80000001,eax,ebx,ecx,edx);
    if((edx&0x00800000)==0)return 0;
    flags=OC_CPU_X86_MMX;
    if(edx&0x80000000)flags|=OC_CPU_X86_3DNOW;
    if(edx&0x40000000)flags|=OC_CPU_X86_3DNOWEXT;
    if(edx&0x00400000)flags|=OC_CPU_X86_MMXEXT;
  }
  else{
    /*Implement me.*/
    flags=0;
  }
  
#ifdef DEBUG
  if (flags) {
    TH_DEBUG("vectorized instruction sets supported:");
    if (flags & OC_CPU_X86_MMX)      TH_DEBUG(" mmx");
    if (flags & OC_CPU_X86_MMXEXT)   TH_DEBUG(" mmxext");
    if (flags & OC_CPU_X86_SSE)      TH_DEBUG(" sse");
    if (flags & OC_CPU_X86_SSE2)     TH_DEBUG(" sse2");
    if (flags & OC_CPU_X86_3DNOW)    TH_DEBUG(" 3dnow");
    if (flags & OC_CPU_X86_3DNOWEXT) TH_DEBUG(" 3dnowext");
    TH_DEBUG("\n");
  }
#endif
#endif
  
  return flags;
}

