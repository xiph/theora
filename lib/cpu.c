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

  function:
  last mod: $Id: mcomp.c,v 1.8 2003/12/03 08:59:41 arc Exp $

 ********************************************************************/

#include "cpu.h"

ogg_uint32_t cpu_flags = 0;

#if 1
static ogg_uint32_t cpu_get_flags (void)
{
  ogg_uint32_t eax, ebx, ecx, edx;
  ogg_uint32_t flags;

#define cpuid(op,eax,ebx,ecx,edx)      \
  asm volatile ("pushl %%ebx   \n\t"   \
                "cpuid         \n\t"   \
                "movl %%ebx,%1 \n\t"   \
                "popl %%ebx"           \
              : "=a" (eax),            \
                "=r" (ebx),            \
                "=c" (ecx),            \
                "=d" (edx)             \
              : "a" (op)               \
              : "cc")

  asm volatile ("pushfl              \n\t"
                "pushfl              \n\t"
                "popl %0             \n\t"
                "movl %0,%1          \n\t"
                "xorl $0x200000,%0   \n\t"
                "pushl %0            \n\t"
                "popfl               \n\t"
                "pushfl              \n\t"
                "popl %0             \n\t"
                "popfl"
              : "=r" (eax),
                "=r" (ebx)
              :
              : "cc");
         
  if (eax == ebx)             /* no cpuid */
    return 0;

  cpuid(0, eax, ebx, ecx, edx);

  if (ebx == 0x756e6547 &&
      edx == 0x49656e69 &&
      ecx == 0x6c65746e) {
    /* intel */

  inteltest:
    cpuid(1, eax, ebx, ecx, edx);
    if ((edx & 0x00800000) == 0)
      return 0;
    flags = CPU_X86_MMX;
    if (edx & 0x02000000)
      flags |= CPU_X86_MMXEXT | CPU_X86_SSE;
    if (edx & 0x04000000)
      flags |= CPU_X86_SSE2;
    return flags;
  } else if (ebx == 0x68747541 &&
             edx == 0x69746e65 &&
             ecx == 0x444d4163) {
    /* AMD */
    cpuid(0x80000000, eax, ebx, ecx, edx);
    if ((unsigned)eax < 0x80000001)
      goto inteltest;
    cpuid(0x80000001, eax, ebx, ecx, edx);
    if ((edx & 0x00800000) == 0)
      return 0;
    flags = CPU_X86_MMX;
    if (edx & 0x80000000)
      flags |= CPU_X86_3DNOW;
    if (edx & 0x00400000)
      flags |= CPU_X86_MMXEXT;
    return flags;
  }
  else {
    /* implement me */
  }

  return flags;
}
#else
static ogg_uint32_t cpu_get_flags (void) {
  return 0;
}
#endif

void cpu_init () 
{
  cpu_flags = cpu_get_flags();
}
