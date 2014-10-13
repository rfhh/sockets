#ifndef __das_route_h__
#define __das_route_h__

#define DAS_ROUTE_MAX_ROUTE_LEN   16
#define DAS_ROUTE_HOST_NAME_MAX   64

struct das_route_info {
    unsigned      rt_skip;
    unsigned      rt_length;
    unsigned char rt_route[DAS_ROUTE_MAX_ROUTE_LEN];
};


struct das_route_host {
    char host_name[DAS_ROUTE_HOST_NAME_MAX];
};


struct das_route_header {
    int      hdr_big_endian;
    unsigned hdr_nr_hosts;
};


int das_route_open(char *route_file);

int das_route_query(char *src, char *dest, struct das_route_info *route);

int das_route_close(void);

#endif /* __das_route_h__ */
