#ifndef _PID_NUMAINFO_H
#define _PID_NUMAINFO_H

#include <sys/types.h>

struct arguments_t {
	pid_t pid;
};

extern struct arguments_t arguments;

extern const char *program_invocation_name;

#endif
