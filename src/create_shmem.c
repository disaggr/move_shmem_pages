
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "create_shmem.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

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

	// create shmem
	int fd = shm_open(arguments.shm_path, O_CREAT | O_EXCL | O_RDWR, 0777);
	if (fd == -1)
	{
		fprintf(stderr, "%s: error: %s: %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}

	// truncate to a non-zero size
	int res = ftruncate(fd, NUM_PAGES * sysconf(_SC_PAGESIZE));
	if (res != 0)
	{
		fprintf(stderr, "%s: error: %s: unable to truncate to %d pages: %s\n",
			program_invocation_name, arguments.shm_path, NUM_PAGES,
			strerror(errno));
		return 1;
	}

	// map pages and fill with data to materialize
	void *shmem = mmap(NULL, NUM_PAGES * sysconf(_SC_PAGESIZE), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (shmem == MAP_FAILED)
	{
		fprintf(stderr, "%s: error: %s: failed to map: %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}

	memset(shmem, '#', NUM_PAGES * sysconf(_SC_PAGESIZE));

	return 0;
}
