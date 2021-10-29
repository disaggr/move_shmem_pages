
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "shmem_numainfo.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <numaif.h>
#include <stdint.h>

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

	// open existing shmem
	int fd = shm_open(arguments.shm_path, O_RDONLY, 0);
	if (fd == -1)
	{
		fprintf(stderr, "%s: error: %s: %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}

	// determine shmem size
	struct stat st;
	int res = fstat(fd, &st);
	if (res != 0)
	{
		fprintf(stderr, "%s: error: %s: could not stat: %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}

	// map memory
	void *shmem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (shmem == MAP_FAILED)
	{
		fprintf(stderr, "%s: error: %s: failed to map: %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}
	if (((uintptr_t)shmem) % sysconf(_SC_PAGESIZE) > 0)
	{
		fprintf(stderr, "%s: error: %s: map not page aligned.\n",
			program_invocation_name, arguments.shm_path);
		return 1;
	}

	// print shmem size
	printf("%s: 0x%08zx bytes\n", arguments.shm_path, st.st_size);
	if (st.st_size == 0)
		return 0;

	// determine placement
	size_t num_pages = st.st_size / sysconf(_SC_PAGESIZE);
	if (st.st_size % sysconf(_SC_PAGESIZE) > 0)
		num_pages += 1;

	void **pages = malloc(sizeof(*pages) * num_pages);
	int *status = malloc(sizeof(*status) * num_pages);

	if (pages == NULL || status == NULL)
	{
		fprintf(stderr, "%s: error: failed to allocate memory: %s\n",
			program_invocation_name, strerror(errno));
		return 1;
	}

	size_t i;
	for (i = 0; i < num_pages; ++i)
	{
		pages[i] = shmem + i * sysconf(_SC_PAGESIZE);
		status[i] = 0;
		// touch the page once, to make move_pages happy
		(void)*(volatile int*)pages[i];
	}

	res = move_pages(0, num_pages, pages, NULL, status, 0);
	if (res != 0)
	{
		fprintf(stderr, "%s: error: %s: failed to determine placement: %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}

	// print placement information
	int region_start = 0;
	for (i = 1; i < num_pages; ++i)
	{
		if (status[i] != status[region_start])
		{
			printf("  0x%08lx ... 0x%08lx\t%i",
				(uintptr_t)pages[region_start],
				(uintptr_t)pages[i - 1] + sysconf(_SC_PAGESIZE) - 1,
				status[region_start]);
			if (status[region_start] < 0)
			{
				printf(" (%s)", strerror(-status[region_start]));
			}
			printf("\n");
			region_start = i;
		}
	}
	printf("  0x%08lx ... 0x%08lx\t%i",
		(uintptr_t)pages[region_start],
		(uintptr_t)pages[num_pages - 1] + sysconf(_SC_PAGESIZE) - 1,
		status[region_start]);
	if (status[region_start] < 0)
	{
		printf(" (%s)", strerror(-status[region_start]));
	}
	printf("\n");

	return 0;
}
