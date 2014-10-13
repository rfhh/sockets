#ifndef __DAS_INET_SYNC_H__
#define __DAS_INET_SYNC_H__


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


/*
 * Make connection to a name server at a well-known inet host;port.
 * Send an ID, #hosts and my inet;port.
 * Receive the inet;port of all (other) hosts.
 */

#ifdef __cplusplus
extern "C" {
#endif

int  das_inet_connect_to_server(const struct sockaddr *addr, size_t size,
				const char *id);

/* Sends platform address to administration server:
      <platform id, port, IP address>
*/
int  das_inet_sync_send(int me, int nhosts, const struct sockaddr *server,
			size_t size, const char *call_id);
int  das_inet_sync_rcve(int socktype,
			struct sockaddr_storage *remote_addrs,
		       	size_t n_addr);

int  das_inet_sync_init(int *argc, char *argv[]);
void das_inet_sync_end(void);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: tabstop=8 shiftwidth=4
 */
