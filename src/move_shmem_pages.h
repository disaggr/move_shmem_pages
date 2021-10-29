#ifndef _MOVE_SHMEM_PAGES_H
#define _MOVE_SHMEM_PAGES_H

#include <sys/types.h>

struct arguments_t {
	const char *shm_path;
	int node_id;
	off_t start_off;
	off_t end_off;
};

extern struct arguments_t arguments;

extern const char *program_invocation_name;

#endif
