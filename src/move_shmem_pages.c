
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
	start_off: 0,
	end_off: -1};

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

	// determine amount of pages to move
	size_t num_pages = st.st_size / sysconf(_SC_PAGESIZE);
	if (st.st_size % sysconf(_SC_PAGESIZE) > 0)
		num_pages += 1;

	// sanitize inputs
	if (arguments.start_off < 0)
		arguments.start_off = 0;
	if (arguments.end_off < arguments.start_off && arguments.end_off >= 0)
		arguments.end_off = arguments.start_off;

	// if on page boundary, round down. this is the expected behaviour.
	off_t end_off = arguments.end_off;
	if (end_off % sysconf(_SC_PAGESIZE) == 0 && end_off > arguments.start_off)
		end_off -= 1;

	int start_page = arguments.start_off / sysconf(_SC_PAGESIZE);
	int end_page = num_pages - 1;
	if (arguments.end_off >= 0)
		end_page = end_off / sysconf(_SC_PAGESIZE);

	int move_page_count = end_page - start_page + 1;

	// determine placement
	void **pages = malloc(sizeof(*pages) * move_page_count);
	int *nodes = malloc(sizeof(*nodes) * move_page_count);
	int *status = malloc(sizeof(*status) * move_page_count);

	if (pages == NULL || nodes == NULL || status == NULL)
	{
		fprintf(stderr, "%s: error: failed to allocate memory: %s\n",
			program_invocation_name, strerror(errno));
		return 1;
	}

	size_t i;
	for (i = start_page; i <= end_page; ++i)
	{
		pages[i - start_page] = shmem + i * sysconf(_SC_PAGESIZE);
		nodes[i - start_page] = arguments.node_id;
		status[i - start_page] = 0;
		// touch the page once, to make move_pages happy
		(void)*(volatile int*)pages[i - start_page];
	}

	res = move_pages(0, move_page_count, pages, nodes, status, MPOL_MF_MOVE_ALL);
	if (res != 0)
	{
		fprintf(stderr, "%s: error: %s: failed to move pages: %s\n",
			program_invocation_name, arguments.shm_path, strerror(errno));
		return 1;
	}

	// print placement information
	int region_start = 0;
	res = 0;
	for (i = 0; i < move_page_count; ++i)
	{
		if (status[i] != status[region_start])
		{
			printf("  0x%012lx ... 0x%012lx",
				(off_t)(pages[region_start] - shmem),
				(off_t)(pages[i - 1] + sysconf(_SC_PAGESIZE) - 1 - shmem));
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
	printf("  0x%012lx ... 0x%012lx",
		(off_t)(pages[region_start] - shmem),
		(off_t)(pages[move_page_count - 1] + sysconf(_SC_PAGESIZE) - 1 - shmem));
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
