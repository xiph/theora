#ifdef USE_ASM

__asm {
   pushfl
   pushfl
   popl          eax
   movl          eax,ebx
   xorl   0x200000,eax
   pushl         eax
   popfl
   pushfl
   popl          eax
   popfl
}

#endif /* USE_ASM */
