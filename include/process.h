#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    char pid[32];
    int64_t arrival, burst, remaining, first_run, completion;
} Process;
#define MAX_PROCESSES 10000
#endif
