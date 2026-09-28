#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "process.h"
typedef enum { FCFS, SJF, RR } Algorithm;
typedef struct { size_t process; int64_t start, end; } TimelineSlice;
/* SIZE_MAX process index denotes an idle interval. */
typedef struct {
    Process *processes;
    size_t count;
    TimelineSlice *timeline;
    size_t slices, capacity, switches;
    int64_t finish, idle;
    double avg_waiting, avg_turnaround, avg_response;
} ScheduleResult;
int schedule(const Process *input, size_t count, Algorithm algorithm, int64_t quantum,
             ScheduleResult *out, char *error, size_t error_size);
void result_destroy(ScheduleResult *r);
const char *algorithm_name(Algorithm a);
#endif
