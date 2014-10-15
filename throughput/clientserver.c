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
    int         n = 1000;
    size_t      size = 256000;
	int			concurrent = 1;
	int			window = 32;
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
		} else if (i < argc - 1 && strcmp(argv[i], "-p") == 0) {
			i++;
			if (sscanf(argv[i], "%d", &concurrent) != 1) {
				error("-p <concurrent buffers> must be an int, not \"%s\"", argv[i]);
			}
		} else if (i < argc - 1 && strcmp(argv[i], "-w") == 0) {
			i++;
			if (sscanf(argv[i], "%d", &window) != 1) {
				error("-w <window size> must be an int, not \"%s\"", argv[i]);
			}
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

	int bufsize = (int)size;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof bufsize) == -1) {
		error("cannot set SO_SNDBUF");
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof bufsize) == -1) {
		error("cannot set SO_RCVBUF");
	}

	if (client) {
		sleep(1);	// allow accept
		if (hmr_tcp_socket_connect(fd, (const struct sockaddr *)&remote_addr[0]) == -1) {
			error("tcp connect");
		}

		das_time_t      t_start;
		das_time_t      t_stop;
		struct bunch {
			size_t		ticks;
			das_time_t	time;
		};
		struct bunch	   *roundtrip = malloc(n * sizeof *roundtrip);
		if (roundtrip == NULL) {
			error("malloc roundtrip");
		}

		size_t acked = 0;
		das_time_get(&t_start);
		for (int i = 0; i < n; i++) {
			if (hmr_tcp_write_fully(fd, buffer, size) == -1) {
				error("Cannot write msg %d", i);
			}
			if (i - acked > window) {
				size_t prev = acked;
				if (hmr_tcp_read_fully(fd, &acked, sizeof acked) == -1) {
					error("Cannot read msg %d", i);
				}
				das_time_get(&t_stop);
				roundtrip[i].ticks = acked - prev;
				roundtrip[i].time = t_stop - t_start;
				das_time_get(&t_start);
				fprintf(stderr, "Acked %zd\n", acked);
			}
		}

		// usleep(10000);
		hmr_tcp_socket_clear(fd);

		for (int i = 0; i < n; i++) {
			printf("%zd %.9lf\n", roundtrip[i].ticks, das_time_t2d(&roundtrip[i].time));
		}

		free(roundtrip);
		free(buffer);

	} else {
		int client_fd = hmr_tcp_socket_accept(fd);
	   	if (client_fd == -1) {
			error("tcp accept");
		}

		size_t count = 0;
		for (int i = 0; i < n; i++) {
			if (hmr_tcp_read_fully(client_fd, buffer, size) == -1) {
				error("Cannot read msg %d", i);
			}
			count++;
			if ((count % window) == 0) {
				if (hmr_tcp_write_fully(client_fd, &count, sizeof count) == -1) {
					error("Cannot write msg %d", i);
				}
				fprintf(stderr, "sent ack %zd\n", count);
			}
		}
	}

    return 0;
}
