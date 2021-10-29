
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "unlink_shmem.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>

struct arguments_t arguments = {
	shm_path: NULL};

void
usage ()
{
	fprintf(stderr, "usage: %s <path>\n",
		program_invocation_name);
}

int
main (int argc, char *argv[])
{
	// parse arguments
	if (argc < 2 || argc > 2)
	{
		usage();
		return 2;
	}

	// argument 1: path to shmem
	arguments.shm_path = argv[1];

	// unlink existing shmem
	int res = shm_unlink(arguments.shm_path);
	if (res != 0)
	{
		fprintf(stderr, "%s: error: %s: could not unlink:  %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}

	return 0;
}
