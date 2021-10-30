
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
	shm_path: NULL,
	size: 0};

void
usage ()
{
	fprintf(stderr, "usage: %s <path> [size]\n",
		program_invocation_name);
}

int
main (int argc, char *argv[])
{
	// parse arguments
	if (argc < 2 || argc > 3)
	{
		usage();
		return 2;
	}

	// argument 1: path to shmem
	arguments.shm_path = argv[1];

	// argument 2: size
	arguments.size = DEFAULT_NUM_PAGES * sysconf(_SC_PAGESIZE);
	if (argc > 2)
	{
		int errsv = errno;
		errno = 0;
		arguments.size = strtoll(argv[2], NULL, 0);
		if (errno != 0)
		{
			fprintf(stderr, "%s: error: %s: %s\n",
				program_invocation_name, argv[2], strerror(errno));
			usage();
			return 1;
		}
		errno = errsv;
	}

	// create shmem
	int fd = shm_open(arguments.shm_path, O_CREAT | O_EXCL | O_RDWR, 0777);
	if (fd == -1)
	{
		fprintf(stderr, "%s: error: %s: %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}

	// truncate to a non-zero size
	int res = ftruncate(fd, arguments.size);
	if (res != 0)
	{
		fprintf(stderr, "%s: error: %s: unable to truncate to 0x%zu bytes: %s\n",
			program_invocation_name, arguments.shm_path, arguments.size,
			strerror(errno));
		return 1;
	}

	// map pages and fill with data to materialize
	void *shmem = mmap(NULL, arguments.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (shmem == MAP_FAILED)
	{
		fprintf(stderr, "%s: error: %s: failed to map: %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}

	memset(shmem, '0', arguments.size);

	return 0;
}
