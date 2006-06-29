#ifdef WIN32

#include <windows.h>
#include <stdio.h>
static unsigned __int64 perf_start_time[4096];
static unsigned __int64 perf_temp;
static unsigned long depth = 0;


typedef struct {
    unsigned __int64 sum;
    unsigned __int64 count;
    unsigned __int64 min;


} perf_info;


extern unsigned __int64 GetCPUTime();
extern void ClearPerfData(perf_info* inoutData);
#define PERF_DATA_ON
#ifdef PERF_DATA_ON


#define PERF_BLOCK_START()  perf_start_time[depth++] = GetCPUTime();

#define PERF_BLOCK_END(s, perf, z)                                                               \
        perf_temp = (GetCPUTime() - perf_start_time[--depth]);                                      \
        (perf.min) = ((perf.min) > perf_temp) ? perf_temp : (perf.min);                                                  \
        perf.sum += perf_temp;                                                                             \
        (perf.count)++;                                                                                      \
  if (((perf.count) % (z)) == 0)                                                                             \
  {                                                                                                 \
    printf(s " - %lld from %lld iterations -- @%lld cycles -- min(%lld)\n", perf.sum, perf.count, (perf.sum) / (perf.count), perf.min);    \
  }                

#else
#define PERF_BLOCK_START()
#define PERF_BLOCK_END(s, perf, z)

#endif




#endif