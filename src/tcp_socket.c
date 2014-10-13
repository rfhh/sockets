
#include <sys/utsname.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <netpacket/packet.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include <das_inet_sync.h>

#include <hmr/tcp_socket.h>


static int
fill_addr(struct sockaddr_in *addr, const char *interface_name, in_port_t port)
{
    addr->sin_family = AF_INET;
    if (interface_name == NULL) {
        addr->sin_addr.s_addr = INADDR_ANY;
    } else {
        struct ifaddrs *ifap;
        const struct ifaddrs *ifa;

        if (getifaddrs(&ifap) == -1) {
            return -1;
        }

        for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr->sa_family == AF_INET &&
                    strcmp(interface_name, ifa->ifa_name) == 0) {
                    memcpy(addr, ifa->ifa_addr, sizeof *addr);
                break;
            }
        }

        freeifaddrs(ifap);
    }
    addr->sin_port = htons(port);

    return 0;
}


int
hmr_tcp_socket(const char *interface_name)
{
    struct sockaddr_in addr = { 0 };

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        goto error;
    }

    if (fill_addr(&addr, interface_name, 0) == -1) {
        goto error;
    }

    if (0 && interface_name != NULL) {
        struct ifreq interface;

        strncpy(interface.ifr_ifrn.ifrn_name, interface_name, IFNAMSIZ);
        if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, &interface, sizeof interface) == -1) {
            goto error;
        }
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) == -1) {
        return -1;
    }

    int one = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one) == -1) {
        goto error;
    }

	return fd;

error:
	return -1;
}


/**
 * ---------------------------------------------------------------------------
 *
 * Utility functions for easily setting up a socket connection
 *
 * ---------------------------------------------------------------------------
 */

int
hmr_tcp_socket_accept(int server_fd)
{
    int         fd = -1;

    struct linger linger = { 1, 0 };
    if (setsockopt(server_fd, SOL_SOCKET, SO_LINGER, &linger, sizeof linger) == -1) {
        goto error;
    }

    if (listen(server_fd, 5) != 0) {
        goto error;
    }

    socklen_t   size;
    struct sockaddr_in addr;

    size = sizeof addr;
    do {
        fd = accept(server_fd, (struct sockaddr *)&addr, &size);
        if (fd == -1) {
            if (errno != EINTR) {
                return -1;
            }
            errno = 0;
        }
    } while (fd == -1);

    fprintf(stderr, "Accept connection: my fd %d, peer %s/%" PRId16 "\n",
            fd, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    return fd;

error:
	if (fd != -1) {
		close(fd);
	}

	return -1;
}


int
hmr_tcp_socket_connect(int fd, const struct sockaddr *server)
{
    while (connect(fd, server, sizeof *server) == -1) {
        if (errno == EINTR) {
            errno = 0;
        } else {
            goto error;
        }
    }

    return fd;

error:
    if (fd != -1) {
        close(fd);
    }

    return -1;
}


int
hmr_tcp_socket_clear(int fd)
{
    shutdown(fd, SHUT_RDWR);
    close(fd);

    return 0;
}


int
hmr_tcp_exchange_addr(int fd,
					  struct sockaddr_storage *remote_addr,
					  size_t me, size_t n,
					  int *argc, char *argv[])
{
	const char *registry_key = "0000 socket latency";
	for (int i = 1; i < *argc; i++) {
		if (i < *argc - 1 && strcmp(argv[i], "-k") == 0) {
			registry_key = argv[i];
		}
	}

	struct sockaddr addr;
	socklen_t addrlen = sizeof addr;
	if (getsockname(fd, &addr, &addrlen) != 0) {
		return -1;
	}
	struct sockaddr_in *in = (struct sockaddr_in *)&addr;
	if (in->sin_addr.s_addr == INADDR_ANY) {
		struct utsname uts;

		if (uname(&uts) == -1) {
			return -1;
		}
		in->sin_addr.s_addr = inet_addr(uts.nodename);
	}

	if (das_inet_sync_init(argc, argv) == -1) {
		return -1;
	}
	if (das_inet_sync_send(me, n, &addr, addrlen, registry_key) == -1) {
		return -1;
	}
	if (das_inet_sync_rcve(SOCK_STREAM, remote_addr, n) == -1) {
		return -1;
	}

	return 0;
}
