#ifndef _UNLINK_SHMEM_H
#define _UNLINK_SHMEM_H

struct arguments_t {
	const char *shm_path;
};

extern struct arguments_t arguments;

extern const char *program_invocation_name;

#endif
