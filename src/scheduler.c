#include "scheduler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef struct { size_t index; int64_t arrival; } Arrival;
static int order(const void *a, const void *b) {
    const Arrival *x = a, *y = b;
    if (x->arrival != y->arrival) return x->arrival < y->arrival ? -1 : 1;
    return x->index < y->index ? -1 : x->index != y->index;
}
const char *algorithm_name(Algorithm a) { return a == FCFS ? "FCFS" : a == SJF ? "SJF" : "RR"; }
void result_destroy(ScheduleResult *r) { free(r->processes); free(r->timeline); memset(r, 0, sizeof(*r)); }
static int append(ScheduleResult *r, size_t p, int64_t start, int64_t end) {
    if (r->slices && r->timeline[r->slices-1].process == p) { r->timeline[r->slices-1].end = end; return 1; }
    if (r->slices == r->capacity) {
        size_t next = r->capacity ? r->capacity * 2 : 16;
        TimelineSlice *grown = realloc(r->timeline, next * sizeof(*grown));
        if (!grown) return 0;
        r->timeline = grown; r->capacity = next;
    }
    if (r->slices && p != SIZE_MAX && r->timeline[r->slices-1].process != SIZE_MAX) ++r->switches;
    r->timeline[r->slices].process = p;
    r->timeline[r->slices].start = start; r->timeline[r->slices++].end = end;
    return 1;
}
int schedule(const Process *input, size_t n, Algorithm algorithm, int64_t quantum,
             ScheduleResult *r, char *error, size_t error_size) {
    Arrival *sorted = NULL;
    size_t i, completed = 0, next = 0;
    int64_t now = 0, total = 0, latest = 0;
    const char *reason = NULL;
    (void)quantum;
    memset(r, 0, sizeof(*r));
    if (!input || !n || n > MAX_PROCESSES || (algorithm != FCFS && algorithm != SJF)) { reason = "Invalid scheduling configuration"; goto fail; }
    for (i = 0; i < n; ++i) {
        if (input[i].arrival < 0 || input[i].burst <= 0 || total > INT64_MAX - input[i].burst) { reason = "Invalid process times or time overflow"; goto fail; }
        total += input[i].burst;
        if (input[i].arrival > latest) latest = input[i].arrival;
    }
    if (latest > INT64_MAX - total) { reason = "Time overflow: latest arrival plus total burst exceeds int64"; goto fail; }
    r->processes = malloc(n * sizeof(*input)); sorted = malloc(n * sizeof(*sorted));
    if (!r->processes || !sorted) { reason = "Allocation failed"; goto fail; }
    memcpy(r->processes, input, n * sizeof(*input)); r->count = n;
    for (i = 0; i < n; ++i) {
        r->processes[i].remaining = input[i].burst; r->processes[i].first_run = -1; r->processes[i].completion = 0;
        sorted[i].index = i; sorted[i].arrival = input[i].arrival;
    }
    qsort(sorted, n, sizeof(*sorted), order);
    while (completed < n) {
        size_t selected = SIZE_MAX;
        int64_t arrival = INT64_MAX;
        Process *p;
        if (algorithm == FCFS) selected = sorted[next++].index;
        else {
            for (i = 0; i < n; ++i) {
                Process *candidate = &r->processes[i];
                if (!candidate->remaining) continue;
                if (candidate->arrival < arrival) arrival = candidate->arrival;
                if (candidate->arrival > now) continue;
                if (selected == SIZE_MAX || candidate->burst < r->processes[selected].burst ||
                    (candidate->burst == r->processes[selected].burst && candidate->arrival < r->processes[selected].arrival)) selected = i;
            }
            if (selected == SIZE_MAX) {
                if (!append(r, SIZE_MAX, now, arrival)) { reason = "Allocation failed"; goto fail; }
                r->idle += arrival - now; now = arrival; continue;
            }
        }
        p = &r->processes[selected];
        if (p->arrival > now) {
            if (!append(r, SIZE_MAX, now, p->arrival)) { reason = "Allocation failed"; goto fail; }
            r->idle += p->arrival - now; now = p->arrival;
        }
        p->first_run = now;
        if (!append(r, selected, now, now + p->burst)) { reason = "Allocation failed"; goto fail; }
        now += p->burst; p->remaining = 0; p->completion = now; ++completed;
    }
    r->finish = now;
    for (i = 0; i < n; ++i) {
        Process *p = &r->processes[i];
        int64_t turnaround = p->completion - p->arrival;
        r->avg_turnaround += (double)turnaround / (double)n;
        r->avg_waiting += (double)(turnaround - p->burst) / (double)n;
        r->avg_response += (double)(p->first_run - p->arrival) / (double)n;
    }
    free(sorted); return 1;
fail:
    snprintf(error, error_size, "%s", reason); free(sorted); result_destroy(r); return 0;
}
