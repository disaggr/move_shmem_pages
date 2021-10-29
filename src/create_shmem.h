#ifndef _CREATE_SHMEM_H
#define _CREATE_SHMEM_H

#define NUM_PAGES 1024

struct arguments_t {
	const char *shm_path;
};

extern struct arguments_t arguments;

extern const char *program_invocation_name;

#endif
