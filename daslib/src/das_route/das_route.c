#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

#include "das_route.h"

#if !defined(DEFAULT_ROUTE_FILE)
#define  DEFAULT_ROUTE_FILE   "/usr/local/sitedep/routes.myri"
#endif

static int     route_fd = -1;
static void *  route_addr = 0;
static size_t  route_size = 0;

static unsigned nr_hosts;

static struct das_route_header *route_hdr;
static struct das_route_host   *route_hosts;
static unsigned char           *route_ptr;
static unsigned char         ***route_pairs;


int
das_route_open(char *route_file)
{
    unsigned char *route_limit;
    struct stat filestat;
    unsigned i;
    unsigned j;

    if (route_file == 0) {
	route_file = getenv("MYRINET_ROUTES");
    }
    if (route_file == 0) {
	route_file = DEFAULT_ROUTE_FILE;
    }

    route_fd = open(route_file, O_RDONLY, 0);
    if (route_fd == -1) {
	fprintf(stderr,
		"DAS: could not open Myrinet routing file %s\n", route_file);
	return -1;
    }

#ifndef JAVA_HACK
    if (fstat(route_fd, &filestat) == -1) {
	return -1;
    }
#else
    if (__fxstat(_STAT_VER, route_fd, &filestat) == -1) {
	fprintf(stderr,
		"DAS: could not stat Myrinet routing file %s\n", route_file);
	return -1;
    }
#endif
    
    route_size = (size_t) filestat.st_size;
    
    /* mmap() implementations seem to be incompatible enough
     * to prefer using standard io here.
     */
    route_addr = malloc(route_size);
    if (route_addr == NULL) {
	fprintf(stderr,
		"DAS: could not allocate Myrinet routing table for %s\n",
		route_file);
	return -1;
    }
    if (read(route_fd, (unsigned char *) route_addr, route_size) != route_size){
	fprintf(stderr, "DAS: could not read Myrinet routing table from %s\n",
		route_file);
	return -1;
    }

    /* The file format is as follows:
     * 1. header
     * 2. list of host structures (list length in header)
     * 3. nr_hosts * nr_hosts (length, route) pairs, ordered by
     *    numeric host id.  A hosts numeric host id is simply
     *    its position in the list of host structures.
     */
    
    /* TO DO: check endianness */

    route_hdr = (struct das_route_header *) route_addr;
    nr_hosts = route_hdr->hdr_nr_hosts;
    route_hosts = (struct das_route_host *) (route_hdr + 1);
    route_ptr   = (unsigned char *) &route_hosts[nr_hosts];
    route_limit = (unsigned char *) route_addr + route_size;

    route_pairs = malloc(nr_hosts * sizeof(unsigned char **));
    if (route_pairs == 0) {
	fprintf(stderr,
		"DAS: could not allocate Myrinet routing table for %s\n",
		route_file);
	return -1;
    }
    for (i = 0; i < nr_hosts; i++) {
	route_pairs[i] = malloc(nr_hosts * sizeof(unsigned char *));
	if (route_pairs[i] == 0) {
	    fprintf(stderr,
		    "DAS: could not allocate Myrinet routing entry for %s\n",
		    route_file);
	    return -1;
	}
	for (j = 0; j < nr_hosts; j++) {
	    if (route_ptr >= route_limit) {
	        fprintf(stderr, "DAS: malformed routing file %s (i %d j %d nr_hosts %d route_hdr %p route_size %d route_ptr %p route_limit %p)\n",
			route_file, i, j, nr_hosts,
			route_hdr, route_size, route_ptr, route_limit);
		return -1;
	    }
	    route_pairs[i][j] = route_ptr;
	    route_ptr += (*route_ptr) + 1;
	}
    }

    return 0;
}


static int
find_host(char *hostname)
{
    unsigned i;

    for (i = 0; i < route_hdr->hdr_nr_hosts; i++) {
	if (strcmp(route_hosts[i].host_name, hostname) == 0) {
	    return i;
	}
    }
    
    return -1;
}


int
das_route_query(char *src, char *dst, struct das_route_info *route)
{
    unsigned char *path;
    unsigned char *len;
    int src_id;
    int dst_id;
    unsigned i;
    int rem;

    src_id = find_host(src);
    dst_id = find_host(dst);
    
    if (src_id == -1 || dst_id == -1) {
	return -1;
    }

    len = route_pairs[src_id][dst_id];
    path = len + 1;

    route->rt_length = *len;
    rem = route->rt_length % 4;
    route->rt_skip = (rem == 0 ? 0 : 4 - rem);
    for (i = 0; i < route->rt_length; i++) {
	route->rt_route[i] = path[i];
    }

    return 0;
}


int
das_route_close(void)
{
    unsigned i;

    for (i = 0; i < nr_hosts; i++) {
	free(route_pairs[i]);
    }
    free(route_pairs);

    free(route_addr);
    route_addr = NULL;

    if (close(route_fd) == -1) {
	return -1;
    }

    return 0;
}
