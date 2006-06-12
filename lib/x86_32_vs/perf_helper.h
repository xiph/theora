#ifdef WIN32

#include <windows.h>
#include <stdio.h>
static unsigned __int64 perf_start_time[4096];
static unsigned __int64 perf_temp;
static unsigned long depth = 0;

/*
//typedef struct {
//    unsigned __int64 sum;
//    unsigned __int64 count;
//    unsigned __int64 min;
//
//} perf_info;
*/

extern unsigned __int64 GetCPUTime();
#define PERF_DATA_ON
#ifdef PERF_DATA_ON


#define PERF_BLOCK_START()  perf_start_time[depth++] = GetCPUTime();

#define PERF_BLOCK_END(s, x, y, l, z)                                                               \
        perf_temp = (GetCPUTime() - perf_start_time[--depth]);                                      \
        (l) = ((l) > perf_temp) ? perf_temp : (l);                                                  \
        x += perf_temp;                                                                             \
        (y)++;                                                                                      \
  if (((y) % (z)) == 0)                                                                             \
  {                                                                                                 \
    printf(s " - %lld from %lld iterations -- @%lld cycles -- min(%lld)\n", x, y, (x) / (y), l);    \
  }                

#else
#define PERF_BLOCK_START()
#define PERF_BLOCK_END(s, x, y, l, z)

#endif




#endif