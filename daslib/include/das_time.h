#ifndef __DAS_LIB_DAS_TIME_H__
#define __DAS_LIB_DAS_TIME_H__

#ifdef __GNUC__

#include <inttypes.h>

typedef uint64_t	das_time_t, *das_time_p;

#define DAS_TIME_RDTSC(b, c)					\
    __asm __volatile(						\
		       ".byte 0xf; .byte 0x31"			\
		      : "=a" (b), "=d" (c))

static inline void
das_time_get(das_time_p t)
{
	uint32_t b;
	uint32_t c;
    DAS_TIME_RDTSC(b, c);
	*t = (uint64_t)b | ((uint64_t)c << 32);
}

double das_time_t2d(const das_time_p t);
void   das_time_d2t(das_time_p t, double d);

void   das_time_init(int *argc, char **argv);
void   das_time_end(void);

#else
#error Need GNU C for asm and long long
#endif

#endif
