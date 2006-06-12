#include "perf_helper.h"
unsigned __int64 GetCPUTime()
{
    unsigned long upper;
    unsigned long lower;
    unsigned __int64 ret;
    __asm {
        align 16
        RDTSC
        mov     upper, edx
        mov     lower, eax
    }

    ret = upper;
    ret <<= 32;
    ret += lower;
    return ret;


}
