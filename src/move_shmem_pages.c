
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "move_shmem_pages.h"

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
	shm_path: NULL,
	node_id: -1,
	start_off: 0x0,
	end_off: 0x0};

void
usage ()
{
	fprintf(stderr, "usage: %s <path> <node> [start] [end]\n",
		program_invocation_name);
}

int
main (int argc, char *argv[])
{
	// parse arguments
	if (argc < 3 || argc > 5)
	{
		usage();
		return 2;
	}

	// argument 1: path to shmem
	arguments.shm_path = argv[1];

	// argument 2: node id
	int errsv = errno;
	errno = 0;
	arguments.node_id = strtol(argv[2], NULL, 0);
	if (errno != 0)
	{
		fprintf(stderr, "%s: error: %s: %s\n",
			program_invocation_name, argv[2], strerror(errno));
		usage();
		return 1;
	}
	errno = errsv;

	// argument 3: start offset
	if (argc > 3)
	{
		errsv = errno;
		errno = 0;
		arguments.start_off = strtoll(argv[3], NULL, 0);
		if (errno != 0)
		{
			fprintf(stderr, "%s: error: %s: %s\n",
				program_invocation_name, argv[3], strerror(errno));
			usage();
			return 1;
		}
		errno = errsv;
	}
	
	// argument 4: end offset
	if (argc > 4)
	{
		errsv = errno;
		errno = 0;
		arguments.end_off = strtoll(argv[4], NULL, 0);
		if (errno != 0)
		{
			fprintf(stderr, "%s: error: %s: %s\n",
				program_invocation_name, argv[4], strerror(errno));
			usage();
			return 1;
		}
		errno = errsv;
	}

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

	// determine placement
	size_t num_pages = st.st_size / sysconf(_SC_PAGESIZE);
	if (st.st_size % sysconf(_SC_PAGESIZE) > 0)
		num_pages += 1;

	void **pages = malloc(sizeof(*pages) * num_pages);
	int *nodes = malloc(sizeof(*nodes) * num_pages);
	int *status = malloc(sizeof(*status) * num_pages);

	if (pages == NULL || nodes == NULL || status == NULL)
	{
		fprintf(stderr, "%s: error: failed to allocate memory: %s\n",
			program_invocation_name, strerror(errno));
		return 1;
	}

	size_t i;
	for (i = 0; i < num_pages; ++i)
	{
		pages[i] = shmem + i * sysconf(_SC_PAGESIZE);
		nodes[i] = arguments.node_id;
		status[i] = 0;
		// touch the page once, to make move_pages happy
		(void)*(volatile int*)pages[i];
	}

	int first_page = arguments.start_off / sysconf(_SC_PAGESIZE);
	int last_page = num_pages - 1;
	if (arguments.end_off > 0)
	{
		last_page = arguments.end_off / sysconf(_SC_PAGESIZE);
		if (arguments.end_off % sysconf(_SC_PAGESIZE))
			last_page += 1;
	}

	res = move_pages(0,
		last_page - first_page + 1,
		pages + first_page,
		nodes + first_page,
		status + first_page, 0);
	if (res != 0)
	{
		fprintf(stderr, "%s: error: %s: failed to move pages: %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}

	// print placement information
	int region_start = first_page;
	res = 0;
	for (i = first_page; i < last_page + 1; ++i)
	{
		if (status[i] != status[region_start])
		{
			printf("  0x%08lx ... 0x%08lx",
				(uintptr_t)pages[region_start],
				(uintptr_t)pages[i - 1] + sysconf(_SC_PAGESIZE) - 1);
			if (status[region_start] == arguments.node_id)
			{
				printf("\tOK");
			}
			else
			{
				printf("\t%i (%s)",
					status[region_start],
					strerror(-status[region_start]));
				res = 1;
			}
			printf("\n");
			region_start = i;
		}
	}
	printf("  0x%08lx ... 0x%08lx",
		(uintptr_t)pages[region_start],
		(uintptr_t)pages[last_page] + sysconf(_SC_PAGESIZE) - 1);
	if (status[region_start] == arguments.node_id)
	{
		printf("\tOK");
	}
	else
	{
		printf("\t%i (%s)",
			status[region_start],
			strerror(-status[region_start]));
		res = 1;
	}
	printf("\n");

	return res;
}
