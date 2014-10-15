#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <poll.h>
#include <unistd.h>
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
    size_t      n = 1000;
    size_t      size = 256 << 10;
	size_t		buffer_size = 0;
	int			window = 32;
    int         options = 0;
    const char *if_name = NULL;
	int			client = -1;
	int			nonblocking = 1;
#define MEGA	(1 << 20)

	das_time_init(&argc, argv);

    for (int i = 1; i < argc; i++) {
        if (0) {
        } else if (i < argc -1 && strcmp(argv[i], "-i") == 0) {
            i++;
            if_name = argv[i];
		} else if (strcmp(argv[i], "-s") == 0) {
			client = 0;
		} else if (strcmp(argv[i], "-c") == 0) {
			client = 1;
		} else if (i < argc - 1 && strcmp(argv[i], "-w") == 0) {
			i++;
			if (sscanf(argv[i], "%d", &window) != 1) {
				error("-w <window size> must be an int, not \"%s\"", argv[i]);
			}
		} else if (i < argc - 1 && strcmp(argv[i], "-b") == 0) {
			i++;
			if (sscanf(argv[i], "%zd", &buffer_size) != 1) {
				error("-p <buffer size> must be an int, not \"%s\"", argv[i]);
			}
		} else if (strcmp(argv[i], "-B") == 0) {
			nonblocking = 0;
		} else if (strcmp(argv[i], "-h") == 0) {
			printf("Options:\n");
			printf("    -i <interface>\n");
			printf("    -s                run as server\n");
			printf("    -c                run as client\n");
			printf("    -w <window size>\n");
			printf("    -b <buffer size>\n");
			printf("    -das-sync-server <registry host>\n");
        } else if (options == 0 && sscanf(argv[i], "%zd", &n) == 1) {
            options++;
        } else if (options == 1 && sscanf(argv[i], "%zd", &size) == 1) {
            options++;
        }
    }

	if (client == -1) {
		error("specify -s (server) or -c (client)");
	}
	if (if_name == NULL) {
		error("specify -i <interface>");
	}

	if (buffer_size == 0) {
		buffer_size = size;
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

	int bufsize = (int)buffer_size;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof bufsize) == -1) {
		error("cannot set SO_SNDBUF");
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof bufsize) == -1) {
		error("cannot set SO_RCVBUF");
	}

	das_time_t t_start_outer;
	das_time_t t_stop_outer;

	if (client) {

		fprintf(stderr, "sleep(1) to allow server to post accept\n");
		sleep(1);	// allow accept

		if (hmr_tcp_socket_connect(fd, (const struct sockaddr *)&remote_addr[0]) == -1) {
			if (errno == EINPROGRESS) {
				struct pollfd fds = {
					fd,
					POLLIN | POLLOUT,
					POLLIN | POLLOUT,
				};
				if (poll(&fds, 1, -1) == -1) {
					error("poll");
				}
			} else {
				error("tcp connect");
			}
		}

		if (nonblocking && hmr_socket_set_nonblocking(fd) == -1) {
			error("set nonblocking");
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
		size_t n_ack = 0;
		das_time_get(&t_start_outer);
		das_time_get(&t_start);
		for (size_t i = 0; i < n; i++) {
			if (hmr_tcp_write_fully(fd, buffer, size) == -1) {
				error("Cannot write msg %zd", i);
			}
			if (i - acked > window) {
				size_t prev = acked;
				if (hmr_tcp_read_fully(fd, &acked, sizeof acked) == -1) {
					error("Cannot read msg %d", n_ack);
				}
				das_time_get(&t_stop);
				roundtrip[n_ack].ticks = acked - prev;
				roundtrip[n_ack].time = t_stop - t_start;
				das_time_get(&t_start);
				// fprintf(stderr, "Acked[%zd] %zd\n", n_ack, acked);
				n_ack++;
			}
		}
		while (acked < n) {
			size_t prev = acked;
			if (hmr_tcp_read_fully(fd, &acked, sizeof acked) == -1) {
				error("Cannot read msg %d", n_ack);
			}
			das_time_get(&t_stop);
			roundtrip[n_ack].ticks = acked - prev;
			roundtrip[n_ack].time = t_stop - t_start;
			das_time_get(&t_start);
			// fprintf(stderr, "Acked[%zd] %zd\n", n_ack, acked);
			n_ack++;
		}
		das_time_get(&t_stop_outer);

		for (size_t i = 0; i < n_ack; i++) {
			printf("%zd %.9lf %.3lfMb/s\n", roundtrip[i].ticks, das_time_t2d(&roundtrip[i].time), CHAR_BIT * roundtrip[i].ticks * size / (MEGA * das_time_t2d(&roundtrip[i].time)));
		}

		free(roundtrip);

	} else {
		int client_fd = hmr_tcp_socket_accept(fd);
	   	if (client_fd == -1) {
			error("tcp accept");
		}

		if (nonblocking && hmr_socket_set_nonblocking(client_fd) == -1) {
			error("set nonblocking");
		}

		das_time_get(&t_start_outer);
		for (size_t i = 0; i < n; i++) {
			if (hmr_tcp_read_fully(client_fd, buffer, size) == -1) {
				error("Cannot read msg %zd", i);
			}
			if ((i % (window / 2)) == 0) {
				if (hmr_tcp_write_fully(client_fd, &i, sizeof i) == -1) {
					error("Cannot write msg %d", i);
				}
				// fprintf(stderr, "sent ack %zd\n", count);
			}
		}
		if ((n % (window / 2)) != 0) {
			if (hmr_tcp_write_fully(client_fd, &n, sizeof n) == -1) {
				error("Cannot write msg %zd", n);
			}
			// fprintf(stderr, "sent ack %zd\n", count);
		}
		das_time_get(&t_stop_outer);

		usleep(10000);
		hmr_tcp_socket_clear(client_fd);
	}

	t_stop_outer -= t_start_outer;
	printf("Total time %.9lf data %zd * %.3lfMB = %.3lfMB throughput %.3lfMb/s\n",
		   das_time_t2d(&t_stop_outer),
		   n, (1.0 * size) / MEGA, (1.0 * size * n) / MEGA,
		   (CHAR_BIT * size * n) / (MEGA * das_time_t2d(&t_stop_outer)));

	// usleep(10000);
	hmr_tcp_socket_clear(fd);

	free(buffer);

    return 0;
}
