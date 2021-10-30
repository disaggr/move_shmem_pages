#ifndef _CREATE_SHMEM_H
#define _CREATE_SHMEM_H

#include <stddef.h>

#define DEFAULT_NUM_PAGES 1024

struct arguments_t {
	const char *shm_path;
	size_t size;
};

extern struct arguments_t arguments;

extern const char *program_invocation_name;

#endif
