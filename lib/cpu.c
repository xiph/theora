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
  last mod: $Id$

 ********************************************************************/

#include "cpu.h"

void
cpuid(ogg_int32_t op, ogg_uint32_t *out_eax, ogg_uint32_t *out_ebx, ogg_uint32_t *out_ecx, ogg_uint32_t *out_edx)
{
#ifdef USE_ASM
# if defined(__x86_64__)
  asm volatile ("pushq %%rbx   \n\t"
                "cpuid         \n\t"
                "movl %%ebx,%1 \n\t"
                "popq %%rbx"        
              : "=a" (*eax),         
                "=r" (*ebx),         
                "=c" (*ecx),         
                "=d" (*edx)          
              : "a" (op)            
              : "cc");
# elif defined(__i386__)
  asm volatile ("pushl %%ebx   \n\t"
                "cpuid         \n\t"
                "movl %%ebx,%1 \n\t"
                "popl %%ebx"        
              : "=a" (*eax),         
                "=r" (*ebx),         
                "=c" (*ecx),         
                "=d" (*edx)          
              : "a" (op)            
              : "cc");
# elif defined(WIN32)
    ogg_uint32_t my_eax, my_ebx, my_ecx, my_edx;
    __asm {
        //push   ebx
        mov     eax, op
        cpuid
        mov     my_eax, eax
        mov     my_ebx, ebx
        mov     my_ecx, ecx
        mov     my_edx, edx
        


    };

    *out_eax = my_eax;
    *out_ebx = my_ebx;
    *out_ecx = my_ecx;
    *out_edx = my_edx; 
# endif

#endif
}

#if defined(USE_ASM) && (defined(__i386__) || defined(__x86_64__) || defined(WIN32))

static ogg_uint32_t cpu_get_flags (void)
{
  ogg_uint32_t eax, ebx, ecx, edx;
  ogg_uint32_t flags = 0;

  /* check for cpuid support on i386 */
#if defined(__i386__)
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
#endif

  /*cpuid(0, &eax, &ebx, &ecx, &edx); */
  /* Intel */
  cpuid(1, &eax, &ebx, &ecx, &edx);
  if ((edx & 0x00800000) == 0)
    return 0;
  flags |= CPU_X86_MMX;
  if (edx & 0x02000000)
    flags |= CPU_X86_MMXEXT | CPU_X86_SSE;
  if (edx & 0x04000000)
    flags |= CPU_X86_SSE2;

  /* AMD */
  cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
  if(eax >= 0x80000001) {
    cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
    if ((edx & 0x00800000) != 0) {
      flags |= CPU_X86_MMX;
      if (edx & 0x80000000)
        flags |= CPU_X86_3DNOW;
      if (edx & 0x40000000)
        flags |= CPU_X86_3DNOWEXT;
      if (edx & 0x00400000)
        flags |= CPU_X86_MMXEXT;
    }
  }

  return flags;
}

#else /* no supported cpu architecture */

static ogg_uint32_t cpu_get_flags (void) {
  return 0;
}

#endif /* USE_ASM */

ogg_uint32_t cpu_init (void)
{
  ogg_uint32_t cpu_flags = cpu_get_flags();

  if (cpu_flags) {
    TH_DEBUG("vectorized instruction sets supported:");
    if (cpu_flags & CPU_X86_MMX)      TH_DEBUG(" mmx");
    if (cpu_flags & CPU_X86_MMXEXT)   TH_DEBUG(" mmxext");
    if (cpu_flags & CPU_X86_SSE)      TH_DEBUG(" sse");
    if (cpu_flags & CPU_X86_SSE2)     TH_DEBUG(" sse2");
    if (cpu_flags & CPU_X86_3DNOW)    TH_DEBUG(" 3dnow");
    if (cpu_flags & CPU_X86_3DNOWEXT) TH_DEBUG(" 3dnowext");
    TH_DEBUG("\n");
  }

  return cpu_flags;
}
