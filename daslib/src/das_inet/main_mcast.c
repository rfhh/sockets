#include <stdio.h>
#include <stdlib.h>

#include "daslib.h"

int
main(int argc, char *argv[])
{
    int		i;
    int		option;
    int		me;
    int		nhosts;

    das_inet_sync_init(&argc, argv);

    for (i = 1; i < argc; i++) {
	if (0) {
	} else if (option == 0) {
	    me = atoi(argv[i]);
	    option++;
	} else if (option == 1) {
	    nhosts = atoi(argv[i]);
	    option++;
	} else {
	    printf("No such option %s\n", argv[i]);
	}
    }

    if (option != 2) {
	printf("Usage: %s <host> <nhosts>\n", argv[0]);
	exit(55);
    }

    das_inet_sync_send(me, nhosts, NULL, "first sync");
    das_inet_sync_rcve(NULL, 0);

    das_inet_sync_send(me, nhosts + 1, NULL, "second sync");
    if (me == 0) {
	das_inet_sync_send(nhosts, nhosts + 1, NULL, "second sync");
    }
    das_inet_sync_rcve(NULL, 0);

    printf("%2d: done\n", me);

    return 0;
}

/*
 * vim: tabstop=8 shiftwidth=4
 */
