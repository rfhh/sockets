/*
 * Make connection to a name server at a well-known inet host;port.
 * Send an ID, #hosts and my inet;port.
 * Receive the inet;port of all (other) hosts.
 */

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/select.h>
#include <limits.h>

#if defined __sun && defined __SVR4
/* Why does Solaris omit prototypes? */
int gethostname(char *name, int namelen);
#endif

#include "das_inet_sync.h"


#define LIN_BACKOFF   100
#define EXP_BACKOFF   600

static int		das_srv_socket;
static int		das_srv_port = -1;
static int	       *srv_sockets = NULL;
static int		n_srv_sockets = 0;
static FILE	       *das_srv_file;
static char	       *das_srv_name = NULL;
static char	       *das_srv_location = "/usr/local/sitedep/reserve/server_host";
static char	       *das_prog_preset_id;
static int		das_n_hosts;
static int		das_max_trial = -1;
static int		das_inet_inited = 0;
static char	       *argv_0;


static char *
hostname(void)
{
    static char	name[HOST_NAME_MAX];
    static int	first_pass = 1;

    if (first_pass) {
	(void)gethostname(name, HOST_NAME_MAX);
	first_pass = 0;
    }

    return name;
}


static int
str2int(const char *str, int *n)
{
    int		res;

    if (str == NULL) {
	errno = EINVAL;
	return 0;
    }

    if (str[0] == '-') {		/* Strange behaviour on -pan.... */
	res = -str2int(str + 1, n);
	*n = - *n;
	return res;
    }

    if (sscanf(str, "%d", n) != 1) {
	errno = EINVAL;
	return 0;
    }

    return 1;
}


static char *
login_name(void)
{
    struct passwd *pswd;

    pswd = getpwuid(getuid());
    return pswd->pw_name;
}


/*
 * Use an asymmetric protocol to establish connections between all pairs
 * of hosts. Each host is a server to all lower-numbered hosts and is a
 * client to all higher-numbered hosts. The panda_adm file contains the
 * server connection.
 */


static void
backoff(int restart)
{
    static int	sleep_time;

    if (restart) {
	sleep_time = 0;
	return;
    }

    (void)sleep(sleep_time);

    if (sleep_time < LIN_BACKOFF) {
	sleep_time++;
    } else if (sleep_time < EXP_BACKOFF) {
	sleep_time *= 2;
    }
}


int
das_inet_connect_to_server(const struct sockaddr *addr, size_t size,
			   const char *id)
{
    int		r;
    int		so;
    sa_family_t	family = ((struct sockaddr_in *)addr)->sin_family;

    backoff(1);
    do {
	so = socket(family, SOCK_STREAM, IPPROTO_TCP);
	if (so == -1) {
	    return -1;
	}
	r = connect(so, addr, size);
	if (r == -1) {
	    if (errno == EINTR) {
		close(so);
	    } else if (errno == ECONNREFUSED) {
		close(so);
		backoff(0);
	    } else {
		return -1;
	    }
#ifndef NDEBUG
	    printf("%s: renew connect call to %s socket %d\n",
		   hostname(), id, so);
#endif
	}
    } while (r == -1);

    return so;
}



/* Sends platform address to administration server:
      <platform id, port, IP address>
*/
int
das_inet_sync_send(int me, int nhosts, const struct sockaddr *server,
		   size_t size, const char *call_id)
{
    struct hostent *das_srv_host;
    struct sockaddr_in	das_server;
    char *prog_id;
    char *white;
    char       *key = NULL;
    char       *das_prog_id;
    char       *prog;
    char       *login;
#define IPV6_TEXT_SIZE	(8 * (4 + 1))
    char	server_name[IPV6_TEXT_SIZE];

    if (das_prog_preset_id == NULL) {
	key = getenv("PANDA_HOST_PORT_KEY");
	if (key == NULL) {
	    login = login_name();
	    if (argv_0 == NULL) {
		prog = "<unknown executable>";
	    } else {
		prog  = strchr(argv_0, '/');
		if (prog == NULL) {
		    prog = argv_0;
		} else {
		    prog++;
		}
	    }
	    das_prog_id = malloc(strlen(login) + strlen(prog) + 20);
	    strcpy(das_prog_id, login);
	    strcat(das_prog_id, "-");
	    strcat(das_prog_id, prog);
	} else {
	    das_prog_id = malloc(strlen(key) + 1);
	    strcpy(das_prog_id, key);
	}
    } else {
	das_prog_id = das_prog_preset_id;
    }

    prog_id = malloc(strlen(das_prog_id) + strlen(call_id) + 2);
    sprintf(prog_id, "%s.%s", das_prog_id, call_id);
    while ((white = strpbrk(prog_id, " \t\n")) != NULL) {
	*white = '-';
    }

    das_n_hosts = nhosts;

    das_srv_host = gethostbyname(das_srv_name);
    if (!das_srv_host) {
	errno = h_errno;
	return -1;
    }

    /* create address; system will fill in the port */
    memset(&das_server, 0, sizeof(struct sockaddr_in));
    memcpy(&das_server.sin_addr, das_srv_host->h_addr, das_srv_host->h_length);
    das_server.sin_family = AF_INET;
    das_server.sin_port = htons(das_srv_port);

    das_srv_socket = das_inet_connect_to_server((struct sockaddr *)&das_server,
					        sizeof das_server,
						"WAN id server");
    if (das_srv_socket < 0) {
	return -1;
    }

    n_srv_sockets++;
    srv_sockets = realloc(srv_sockets, n_srv_sockets * sizeof(int));
    srv_sockets[n_srv_sockets - 1] = das_srv_socket;

    das_srv_file = fdopen(das_srv_socket, "a+");
    if (das_srv_file == NULL) {
	return -1;
    }

    /* write internet address and port to server socket */
#ifdef NEVER
    fprintf(stdout, "%s: send to %s: %s %d %d ",
	    hostname(), inet_ntoa(das_server.sin_addr), prog_id, me, nhosts);
    fprintf(stdout, "%d %s\n",
	    ntohs(server->sin_port), inet_ntop(server->sin_addr));
#endif

    if (server == NULL) {
	if (fprintf(das_srv_file, "%s %d %d %d %s\n",
		    prog_id, me, nhosts, 0, "0.0.0.0") <= 0){
	    return -1;
	}
    } else {
	in_port_t port;

	switch (server->sa_family) {
	case AF_INET:
	    // inet_ntop(family, &in4->sin_addr, server_name, size);
		inet_ntop(server->sa_family, &((struct sockaddr_in*)server)->sin_addr, server_name, sizeof server_name);
	    port = ntohs(((struct sockaddr_in *)server)->sin_port);
	    break;
	case AF_INET6:
	    // inet_ntop(family, &in6->sin6_addr, server_name, size);
		inet_ntop(server->sa_family, &((struct sockaddr_in6*)server)->sin6_addr, server_name, sizeof server_name);
	    port = ntohs(((struct sockaddr_in6 *)server)->sin6_port);
	    break;
	default:
	    errno = EPROTONOSUPPORT;
	    return -1;
	}
	if (fprintf(das_srv_file, "%s %d %d %d %s\n",
		    prog_id, me, nhosts,
		    port, server_name) <= 0){
	    return -1;
	}
    }
    fflush(das_srv_file);
    
    free(prog_id);

    return 0;
}



int
das_inet_sync_rcve(int socktype,
		   struct sockaddr_storage *remote_addrs,
		   size_t n_addr)
{
    char	name[HOST_NAME_MAX];
    char	caller[HOST_NAME_MAX];
    int		i;
    int		pid;
    short unsigned	port;
#ifndef bsdi
    fd_set	the_rd_set;
    fd_set	the_except_set;
#endif
    fd_set     *rd_set;
    fd_set     *except_set;
    int		n_fds = das_srv_socket + 1;
    int		r;
    int		n_trial;
    int		partner_trial;
    struct timeval timeout;
    int		max_trial;
    int		partner_max_trial = -1;
    char	fmt[256];

#ifdef bsdi
    rd_set     = FD_ALLOC(n_fds);
    except_set = FD_ALLOC(n_fds);
#else
    rd_set     = &the_rd_set;
    except_set = &the_except_set;
#endif

    if (remote_addrs != NULL && n_addr < das_n_hosts) {
	errno = EOVERFLOW;
	return -1;
    }

    if (gethostname(caller, HOST_NAME_MAX - 1) != 0) {
	errno = h_errno;
	return -1;
    }

    FD_ZERO(rd_set);
    FD_ZERO(except_set);

    /* Read all addresses of other hosts */

    n_trial = 0;

    if (das_max_trial == -1) {
	max_trial = 100 + 20 * das_n_hosts;
    } else {
	max_trial = das_max_trial;
    }
    if (partner_max_trial == -1) {
	partner_max_trial = 5;
    }

    sprintf(fmt, "%%d %%hu %%%ds\n", HOST_NAME_MAX - 1);
    for (i = 0; i < das_n_hosts;){
	FD_SET(das_srv_socket, rd_set);
	FD_SET(das_srv_socket, except_set);
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	r = select(n_fds, rd_set, NULL, except_set, &timeout);
	if (r == -1 && errno != EINTR) {
	    return -1;
	}

	if (r == 0 || (r == -1 && errno == EINTR)) {
	    if (errno != EINTR && n_trial++ == max_trial) {
		return -1;
	    }
	    continue;
	}

	if (FD_ISSET(das_srv_socket, except_set)) {
	    fprintf(stderr, "exception on server connection");
	    errno = EBADMSG;
	    return -1;
	}

	partner_trial = 0;

	if (FD_ISSET(das_srv_socket, rd_set)) {

	    if (feof(das_srv_file)) {
		return -1;
	    }

	    memset(name, 0, sizeof(name));

	    while (1) {
		r = fscanf(das_srv_file, fmt, &pid, &port, name);
		if (r == 3){
		    break;
		}
		if (r == -1 && errno == EINTR) {
		    if (partner_trial++ >= partner_max_trial) {
			errno = ETIMEDOUT;
			return -1;
		    }
		    fprintf(stderr, "%s: Warning: hickup receiving wan partner %d inet address\n", caller, i);
		    sleep(1);
		} else {
		    return -1;
		}
	    }

	    i++;

	    if (remote_addrs != NULL) {
		struct sockaddr *a = (struct sockaddr *)&remote_addrs[pid];
		struct addrinfo *res;
		struct addrinfo hints;

		memset(a, 0, sizeof remote_addrs[pid]);

		memset(&hints, 0, sizeof hints);
		hints.ai_socktype = socktype;

		int ret = 0;
		int attempts = 5;
		while (attempts > 0 && (ret = getaddrinfo(name, NULL, &hints, &res)) != 0) {
			fprintf(stderr, "Cannot getaddrinfo: \"%s\" %d\n", name, ret);
			--attempts;
			usleep(50*1000);
		}
		if (ret != 0) {
			abort();
		}
		struct addrinfo *r;
		for (r = res; r != NULL; r = r->ai_next) {
		    memcpy(a, r->ai_addr, r->ai_addrlen);
		}
		freeaddrinfo(res);

		struct sockaddr_in *in = (struct sockaddr_in *)a;
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)a;
		switch (a->sa_family) {
		case AF_INET:
		    in->sin_port = ntohs(port);
		    break;
		case AF_INET6:
		    in6->sin6_port = ntohs(port);
		    break;
		default:
		    fprintf(stderr, "Unsupported address family\n");
		    break;
		}
	    }
	}
    }

    for (i = 0; i < n_srv_sockets; i++) {
	close(srv_sockets[i]);
    }
    n_srv_sockets = 0;

    return 0;
}


int
das_inet_sync_init(int *argc, char *argv[])
{
    int		i;
    int		new_argc;
    int		port = -1;
    const char *das_srv_port_env;
#define DEFAULT_PORT	3456

    if (das_inet_inited++ > 0) {
	return 0;
    }

    das_srv_name = getenv("DAS_SYNC_SERVER");
    das_srv_port_env = getenv("DAS_SYNC_PORT");
    if (das_srv_port_env != NULL) {
	if (! str2int(das_srv_port_env, &das_srv_port)) {
	    fprintf(stderr, "DAS_SYNC_PORT must be an int, not \"%s\"\n",
		    das_srv_port_env);
	    return -1;
	}
    }

    new_argc = 1;
    for (i = 1; i < *argc; i++) {
	if (0) {
	} else if (strcmp(argv[i], "-das_max_trial") == 0 ||
		   strcmp(argv[i], "-das-max-trial") == 0) {
	    if (++i >= *argc || ! str2int(argv[i], &das_max_trial)) {
		fprintf(stderr, "%s <max_trial>: "
				"need an int, not \"%s\"\n",
				argv[i-1], argv[i]);
		return -1;
	    }
	} else if (strcmp(argv[i], "-das_sync_port") == 0 ||
		   strcmp(argv[i], "-das-sync-port") == 0) {
	    if (++i >= *argc || ! str2int(argv[i], &das_srv_port)) {
		fprintf(stderr, "%s <port>: "
				"need an int, not \"%s\"\n",
				argv[i-1], argv[i]);
		return -1;
	    }
	} else if (strcmp(argv[i], "-das_sync_server") == 0 ||
		   strcmp(argv[i], "-das-sync-server") == 0) {
	    if (++i >= *argc) {
		fprintf(stderr, "%s <server>: need an argument\n",
				argv[i-1]);
		return -1;
	    }
	    das_srv_name = argv[i];
	} else if (strcmp(argv[i], "-das_sync_id") == 0 ||
		   strcmp(argv[i], "-das-sync-id") == 0) {
	    if (++i >= *argc) {
		fprintf(stderr, "%s <id>: need an argument\n",
				argv[i-1]);
		return -1;
	    }
	    das_prog_preset_id = argv[i];
	} else {
	    argv[new_argc++] = argv[i];
	}
    }
    *argc = new_argc;
    argv[new_argc] = NULL;

    argv_0 = argv[0];

    if (das_srv_name == NULL) {
#include <sys/param.h>
	FILE *das_srv_file;
	char	fmt[256];

	das_srv_file = fopen(das_srv_location, "r");
	if (das_srv_file == NULL) {
	    return -1;
	}
	das_srv_name = malloc(HOST_NAME_MAX);
	sprintf(fmt, " %%%ds ", HOST_NAME_MAX - 1);

	if (fscanf(das_srv_file, fmt, das_srv_name) != 1) {
	    return -1;
	}

	if (fscanf(das_srv_file, "%d ", &port) != 1) {
	    return -1;
	}
    }

    if (das_srv_port == -1) {
	if (port == -1) {
	    das_srv_port = DEFAULT_PORT;
	} else {
	    das_srv_port = port;
	}
    }

    return 0;
}


void
das_inet_sync_end(void)
{
    if (--das_inet_inited > 0) {
	return;
    }
}

/*
 * vim: tabstop=8 shiftwidth=4
 */
