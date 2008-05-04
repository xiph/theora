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

 CPU capability detection for x86 processors.
  Originally written by Rudolf Marek.

 function:
  last mod: $Id$

 ********************************************************************/

#include "cpu.h"

#if !defined(USE_ASM)
ogg_uint32_t oc_cpu_flags_get(void){
  return 0;
}
#else

# if !defined(_MSC_VER)
#  if defined(__amd64__)||defined(__x86_64__)
/*On x86-64, gcc seems to be able to figure out how to save %rbx for us when
   compiling with -fPIC.*/
#   define cpuid(_op,_eax,_ebx,_ecx,_edx) \
  __asm__ __volatile__( \
   "cpuid\n\t" \
   :[eax]"=a"(_eax),[ebx]"=b"(_ebx),[ecx]"=c"(_ecx),[edx]"=d"(_edx) \
   :"a"(_op) \
   :"cc" \
  )
#  else
/*On x86-32, not so much.*/
#   define cpuid(_op,_eax,_ebx,_ecx,_edx) \
  __asm__ __volatile__( \
   "xchgl %%ebx,%[ebx]\n\t" \
   "cpuid\n\t" \
   "xchgl %%ebx,%[ebx]\n\t" \
   :[eax]"=a"(_eax),[ebx]"=r"(_ebx),[ecx]"=c"(_ecx),[edx]"=d"(_edx) \
   :"a"(_op) \
   :"cc" \
  )
#  endif
# else
/*Why does MSVC need this complicated rigamarole?
  At this point I honestly do not care.*/

/*Visual C cpuid helper function.
  For VS2005 we could as well use the _cpuid builtin, but that wouldn't work
   for VS2003 users, so we do it in inline assembler.*/
static void oc_cpuid_helper(ogg_uint32_t _cpu_info[4],ogg_uint32_t _op){
  _asm{
    mov eax,[_op]
    mov esi,_cpu_info
    cpuid
    mov [esi+0],eax
    mov [esi+4],ebx
    mov [esi+8],ecx
    mov [esi+12],edx
  }
}

#  define cpuid(_op,_eax,_ebx,_ecx,_edx) \
  do{ \
    ogg_uint32_t cpu_info[4]; \
    oc_cpuid_helper(cpu_info,_op); \
    (_eax) = cpu_info[0]; \
    (_ebx) = cpu_info[1]; \
    (_ecx) = cpu_info[2]; \
    (_edx) = cpu_info[3]; \
  }while(0)

static void oc_detect_cpuid_helper(ogg_uint32_t *_eax,ogg_uint32_t *_ebx){
  _asm{
    pushfd
    pushfd
    pop eax
    mov ebx,eax
    xor eax,200000h
    push eax
    popfd
    pushfd
    pop eax
    popfd
    mov [_eax],eax
    mov [_ebx],ebx
  }
}
# endif

ogg_uint32_t oc_cpu_flags_get(void){
  ogg_uint32_t flags;
  ogg_uint32_t eax;
  ogg_uint32_t ebx;
  ogg_uint32_t ecx;
  ogg_uint32_t edx;
# if !defined(__amd64__)&&!defined(__x86_64__)
  /*Not all x86-32 chips support cpuid, so we have to check.*/
#  if !defined(_MSC_VER)
  __asm__ __volatile__(
   "pushfl\n\t"
   "pushfl\n\t"
   "popl %[a]\n\t"
   "movl %[a],%[b]\n\t"
   "xorl $0x200000,%[a]\n\t"
   "pushl %[a]\n\t"
   "popfl\n\t"
   "pushfl\n\t"
   "popl %[a]\n\t"
   "popfl\n\t"
   :[a]"=r"(eax),[b]"=r"(ebx)
   :
   :"cc"
  );
#  else
  oc_detect_cpuid_helper(&eax,&ebx);
#  endif
  /*No cpuid.*/
  if(eax==ebx)return 0;
# endif
  cpuid(0,eax,ebx,ecx,edx);
  /*         l e t n          I e n i          u n e G*/
  if(ecx==0x6C65746E&&edx==0x49656E69&&ebx==0x756E6547||
   /*      6 8 x M          T e n i          u n e G*/
   ecx==0x3638784D&&edx==0x54656E69&&ebx==0x756E6547){
    /*Intel, Transmeta (tested with Crusoe TM5800):*/
    cpuid(1,eax,ebx,ecx,edx);
    /*If there isn't even MMX, give up.*/
    if(!(edx&0x00800000))return 0;
    flags=OC_CPU_X86_MMX;
    if(edx&0x02000000)flags|=OC_CPU_X86_MMXEXT|OC_CPU_X86_SSE;
    if(edx&0x04000000)flags|=OC_CPU_X86_SSE2;
    if(ecx&0x00000001)flags|=OC_CPU_X86_PNI;
  }
  /*              D M A c          i t n e          h t u A*/
  else if(ecx==0x444D4163&&edx==0x69746E65&&ebx==0x68747541||
   /*      C S N            y b   e          d o e G*/
   ecx==0x43534e20&&edx==0x79622065&&ebx==0x646f6547){
    /*AMD, Geode:*/
    cpuid(0x80000000,eax,ebx,ecx,edx);
    if(eax<0x80000001){
      /*No extended functions supported.
        Use normal cpuid flags.*/
      cpuid(1,eax,ebx,ecx,edx);
      /*If there isn't even MMX, give up.*/
      if(!(edx&0x00800000))return 0;
      flags=OC_CPU_X86_MMX;
      if(edx&0x02000000)flags|=OC_CPU_X86_MMXEXT|OC_CPU_X86_SSE;
    }
    else{
      cpuid(0x80000001,eax,ebx,ecx,edx);
      /*If there isn't even MMX, give up.*/
      if(!(edx&0x00800000))return 0;
      flags=OC_CPU_X86_MMX;
      if(edx&0x80000000)flags|=OC_CPU_X86_3DNOW;
      if(edx&0x40000000)flags|=OC_CPU_X86_3DNOWEXT;
      if(edx&0x00400000)flags|=OC_CPU_X86_MMXEXT;
      /*Also check for SSE.*/
      cpuid(1,eax,ebx,ecx,edx);
      if(edx&0x02000000)flags|=OC_CPU_X86_SSE;
    }
    if(edx&0x04000000)flags|=OC_CPU_X86_SSE2;
    if(ecx&0x00000001)flags|=OC_CPU_X86_PNI;
  }
  /*              s l u a          H r u a          t n e C*/
  else if(ecx==0x736C7561&&edx==0x48727561&&ebx==0x746E6543){
    /*VIA:*/
    /*The C7 (and later?) processors support Intel-like cpuid info.*/
    /*The C3-2 (Nehemiah) cores appear to, as well.*/
    cpuid(1,eax,ebx,ecx,edx);
    if(edx&0x00800000){
      flags=OC_CPU_X86_MMX;
      if(edx&0x02000000)flags|=OC_CPU_X86_MMXEXT|OC_CPU_X86_SSE;
      if(edx&0x04000000)flags|=OC_CPU_X86_SSE2;
      if(ecx&0x00000001)flags|=OC_CPU_X86_PNI;
    }
    else flags=0;
    /*The (non-Nehemiah) C3 processors support AMD-like cpuid info.
      We need to check this even if the Intel test succeeds to pick up 3dnow!
       support on these processors.*/
    /*TODO: How about earlier chips?*/
    cpuid(0x80000001,eax,ebx,ecx,edx);
    if(edx&0x00800000)flags|=OC_CPU_X86_MMX;
    if(edx&0x80000000)flags|=OC_CPU_X86_3DNOW;
  }
  else{
    /*Implement me.*/
    flags=0;
  }
# if defined(DEBUG)
  if(flags){
    TH_DEBUG("vectorized instruction sets supported:");
    if(flags&OC_CPU_X86_MMX)TH_DEBUG(" mmx");
    if(flags&OC_CPU_X86_MMXEXT)TH_DEBUG(" mmxext");
    if(flags&OC_CPU_X86_SSE)TH_DEBUG(" sse");
    if(flags&OC_CPU_X86_SSE2)TH_DEBUG(" sse2");
    if(flags&OC_CPU_X86_3DNOW)TH_DEBUG(" 3dnow");
    if(flags&OC_CPU_X86_3DNOWEXT)TH_DEBUG(" 3dnowext");
    TH_DEBUG("\n");
  }
# endif
  return flags;
}
#endif
