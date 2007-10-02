#ifdef USE_ASM

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

#endif /* USE_ASM */

