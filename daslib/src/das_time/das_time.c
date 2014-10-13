/*
 * Copyright 1998-1999 Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the DASLIB distribution.
 */
 
#include <stdio.h>

#include "das_time.h"

// #define MEGA	(1 << 20)
#define MEGA	1000000

#define HOST_MHZ_DEFAULT 1000.0

static double das_time_host_mhz = HOST_MHZ_DEFAULT * MEGA;

void
das_time_init(int *argc, char **argv)
{
#if defined(__linux)
    /* code borrowed from Panda 4.0 */
#   define LINE_SIZE       512
    char line[LINE_SIZE];
    FILE *f;

    das_time_host_mhz = -1.0;
 
    f = fopen("/proc/cpuinfo", "r");
    if (f != NULL) {
        while (! feof(f)) {
            fgets(line, LINE_SIZE, f);
            if (sscanf(line, " cpu MHz : %lf", &das_time_host_mhz) == 1) {
                break;
            }
        }
	if (das_time_host_mhz == -1.0) {
	    while (! feof(f)) {
		fgets(line, LINE_SIZE, f);
		if (sscanf(line, " bogomips : %lf", &das_time_host_mhz) == 1) {
		    break;
		}
	    }
	}
        fclose(f);
    }
    if (das_time_host_mhz < 0.0) {
        das_time_host_mhz = HOST_MHZ_DEFAULT;
        fprintf(stderr,
                "Cannot find /proc/cpuinfo, assume clock speed = %.1f MHz\n",
                das_time_host_mhz);
    }

    das_time_host_mhz *= MEGA;
#else
#  error "OS not supported"
#endif
}


void
das_time_end(void)
{
}


double das_time_t2d(const das_time_p t)
{
    return *t / das_time_host_mhz;
}


void das_time_d2t(das_time_p t, double d)
{
    *t = (das_time_t)(d * das_time_host_mhz);
}
