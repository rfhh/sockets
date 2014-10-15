#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <hmr/tcp_socket.h>
#include <das_time.h>


static int
error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (errno != 0) {
        fprintf(stderr, ": %s", strerror(errno));
    }
    fprintf(stderr, "\n");

    exit(33);
}



int
main(int argc, char *argv[])
{
    int         n = 1000000;
    size_t      size = 1;
    int         options = 0;
    const char *if_name = NULL;
	int			client = -1;

    for (int i = 1; i < argc; i++) {
        if (0) {
        } else if (i < argc -1 && strcmp(argv[i], "-i") == 0) {
            i++;
            if_name = argv[i];
		} else if (strcmp(argv[i], "-s") == 0) {
			client = 0;
		} else if (strcmp(argv[i], "-c") == 0) {
			client = 1;
        } else if (options == 0) {
            if (sscanf(argv[i], "%d", &n) != 1) {
                error("<nmsg> must be an int, not \"%s\"", argv[i]);
            }
            options++;
        } else if (options == 1) {
            if (sscanf(argv[i], "%zd", &size) != 1) {
                error("<size> must be an int, not \"%s\"", argv[i]);
            }
            options++;
        }
    }

	if (client == -1) {
		error("specify -s (server) or -c (client)");
	}

	int fd = hmr_tcp_socket(if_name);
	if (fd == -1) {
		error("hmr_tcp_socket");
	}

	struct sockaddr_storage remote_addr[2];
	if (hmr_tcp_exchange_addr(fd, remote_addr, client ? 1 : 0, 2, &argc, argv) == -1) {
		error("tcp exchange_addr");
	}

    void       *buffer = malloc(size);
    if (buffer == NULL) {
        error("Cannot allocate buffer");
    }

	if (client) {
		sleep(1);	// allow accept
		if (hmr_tcp_socket_connect(fd, (const struct sockaddr *)&remote_addr[0]) == -1) {
			error("tcp connect");
		}

		das_time_t      t_start;
		das_time_t      t_stop;
		das_time_t	   *roundtrip = malloc(n * sizeof *roundtrip);
		if (roundtrip == NULL) {
			error("malloc roundtrip");
		}

		for (int i = 0; i < n; i++) {
			das_time_get(&t_start);
			if (hmr_tcp_write_fully(fd, buffer, size) == -1) {
				error("Cannot write msg %d", i);
			}
			if (hmr_tcp_read_fully(fd, buffer, size) == -1) {
				error("Cannot read msg %d", i);
			}
			das_time_get(&t_stop);
			roundtrip[i] = t_stop - t_start;
		}

		// usleep(10000);
		hmr_tcp_socket_clear(fd);

		for (int i = 0; i < n; i++) {
			printf("%.9lf\n", das_time_t2d(&roundtrip[i]));
		}

		free(roundtrip);

	} else {
		int client_fd = hmr_tcp_socket_accept(fd);
	   	if (client_fd == -1) {
			error("tcp accept");
		}

		for (int i = 0; i < n; i++) {
			if (hmr_tcp_read_fully(client_fd, buffer, size) == -1) {
				error("Cannot read msg %d", i);
			}
			if (hmr_tcp_write_fully(client_fd, buffer, size) == -1) {
				error("Cannot write msg %d", i);
			}
		}
	}

	free(buffer);

    return 0;
}
