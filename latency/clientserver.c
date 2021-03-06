#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
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
    size_t      n = 100000;
    size_t      size = 1;
    int         options = 0;
    const char *if_name = NULL;
	int			client = -1;

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
		} else if (strcmp(argv[i], "-h") == 0) {
			printf("Options:\n");
			printf("    -i <interface>\n");
			printf("    -s                run as server\n");
			printf("    -c                run as client\n");
			printf("    -das-sync-server <registry host>\n");
        } else if (options == 0 && scanf(argv[i], "%zd", &n) == 1) {
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
		fprintf(stderr, "sleep(1) to allow server to post accept\n");
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

		for (size_t i = 0; i < n; i++) {
			das_time_get(&t_start);
			if (hmr_tcp_write_fully(fd, buffer, size) == -1) {
				error("Cannot write msg %zd", i);
			}
			if (hmr_tcp_read_fully(fd, buffer, size) == -1) {
				error("Cannot read msg %zd", i);
			}
			das_time_get(&t_stop);
			roundtrip[i] = t_stop - t_start;
		}

		for (size_t i = 0; i < n; i++) {
			printf("%.9lf\n", das_time_t2d(&roundtrip[i]));
		}

		free(roundtrip);

	} else {
		int client_fd = hmr_tcp_socket_accept(fd);
	   	if (client_fd == -1) {
			error("tcp accept");
		}

		for (size_t i = 0; i < n; i++) {
			if (hmr_tcp_read_fully(client_fd, buffer, size) == -1) {
				error("Cannot read msg %d", i);
			}
			if (hmr_tcp_write_fully(client_fd, buffer, size) == -1) {
				error("Cannot write msg %d", i);
			}
		}

		usleep(10000);
		hmr_tcp_socket_clear(client_fd);
	}

	// usleep(10000);
	hmr_tcp_socket_clear(fd);

	free(buffer);

    return 0;
}
