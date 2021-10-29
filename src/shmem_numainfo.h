#ifndef _SHMEM_NUMAINFO_H
#define _SHMEM_NUMAINFO_H

struct arguments_t {
	const char *shm_path;
};

extern struct arguments_t arguments;

extern const char *program_invocation_name;

#endif
