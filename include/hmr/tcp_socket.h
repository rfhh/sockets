#ifndef HMR_TCP_SOCKET_H__
#define HMR_TCP_SOCKET_H__

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <inttypes.h>

int hmr_tcp_socket(const char *interface_name);
int hmr_tcp_socket_accept(int server_fd);
int hmr_tcp_socket_connect(int fd, const struct sockaddr *server);
int hmr_tcp_socket_clear(int fd);
int hmr_tcp_exchange_addr(int fd,
						  struct sockaddr_storage *remote_addr,
						  size_t me, size_t n,
						  int *argc, char *argv[]);

int hmr_socket_set_nonblocking(int fd);

#include <errno.h>

static inline ssize_t
hmr_tcp_read_fully(int fd, void *data, size_t len)
{
    size_t      rd = 0;

    while (rd < len) {
        ssize_t r = read(fd, (char *)data + rd, len - rd);
        if (r == -1) {
			if (errno == EAGAIN) {
				// Or poll for POLLIN?
				errno = 0;
				continue;
			}
            if (errno == EINTR) {
                errno = 0;
                continue;
            }

            // retain errno
            return -1;
        } else if (r == 0) {
            errno = ENOLINK;      // how do I signal EOF?
            return -1;
        }

        rd += r;
    }

    return rd;
}


static inline ssize_t
hmr_tcp_write_fully(int fd, const void *data, size_t len)
{
    size_t      rd = 0;

    while (rd < len) {
        ssize_t r = write(fd, (const char *)data + rd, len - rd);
        if (r == -1) {
			if (errno == EAGAIN) {
				// Or poll for POLLOUT?
				errno = 0;
				continue;
			}

            if (errno == EINTR) {
                errno = 0;
                continue;
            }

            // retain errno
            return -1;
        }

        rd += r;
    }

    return rd;
}

#endif
