#include "csv_reader.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
static void help(void) {
    puts("CPU Scheduling Lab\nUsage: scheduler FILE --algorithm fcfs|sjf|rr [--quantum N]\n       scheduler FILE --compare --quantum N\nCSV header: pid,arrival,burst\nRR requires a positive decimal quantum. Algorithm names are lowercase.");
}
static int quantum_value(const char *s, int64_t *out) {
    int64_t n = 0;
    if (!*s) return 0;
    for (; *s; ++s) {
        int d = *s - '0';
        if (d < 0 || d > 9 || n > (INT64_MAX - d) / 10) return 0;
        n = n * 10 + d;
    }
    *out = n; return n > 0;
}
static void row(Algorithm a, const ScheduleResult *r) {
    printf("%-9s %11.2f %14.2f %12.2f %8zu %8" PRId64 " %8" PRId64 "\n", algorithm_name(a), r->avg_waiting, r->avg_turnaround, r->avg_response, r->switches, r->finish, r->idle);
}
static void detail(Algorithm a, const ScheduleResult *r) {
    size_t i;
    printf("\n%s timeline (half-open intervals; IDLE is CPU idle):\n", algorithm_name(a));
    for (i = 0; i < r->slices; ++i) {
        const TimelineSlice *s = &r->timeline[i];
        printf("[ %" PRId64 ", %" PRId64 " ) %s\n", s->start, s->end, s->process == SIZE_MAX ? "<IDLE>" : r->processes[s->process].pid);
    }
    puts("PID Arrival Burst First Completion Waiting Turnaround Response");
    for (i = 0; i < r->count; ++i) {
        const Process *p = &r->processes[i];
        int64_t turnaround = p->completion - p->arrival;
        printf("%s %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 "\n", p->pid, p->arrival, p->burst, p->first_run, p->completion, turnaround-p->burst, turnaround, p->first_run-p->arrival);
    }
}
int main(int argc, char **argv) {
    const char *path = NULL, *name = NULL;
    int comparison = 0, seen_quantum = 0, i, result = 1;
    int64_t quantum = 0;
    Algorithm algorithm = FCFS;
    Process *input = NULL;
    size_t count = 0;
    char error[256];
    ScheduleResult runs[3] = {{0}};
    if (argc == 2 && !strcmp(argv[1], "--help")) { help(); return 0; }
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--compare") && !comparison) comparison = 1;
        else if (!strcmp(argv[i], "--algorithm") && !name && i+1 < argc) name = argv[++i];
        else if (!strcmp(argv[i], "--quantum") && !seen_quantum && i+1 < argc) {
            seen_quantum = 1;
            if (!quantum_value(argv[++i], &quantum)) { fprintf(stderr, "Error: quantum must be a positive decimal int64.\n"); return 1; }
        } else if (argv[i][0] != '-' && !path) path = argv[i];
        else { fprintf(stderr, "Error: unknown, duplicate or incomplete argument: %s\n", argv[i]); return 1; }
    }
    if (!path || (comparison && name) || (!comparison && !name)) { help(); return 1; }
    if (name) {
        if (!strcmp(name,"fcfs")) algorithm = FCFS;
        else if (!strcmp(name,"sjf")) algorithm = SJF;
        else if (!strcmp(name,"rr")) algorithm = RR;
        else { fprintf(stderr,"Error: algorithm must be lowercase fcfs, sjf or rr.\n"); return 1; }
    }
    if ((comparison || algorithm == RR) && !seen_quantum) { fprintf(stderr,"Error: RR requires --quantum.\n"); return 1; }
    if (!comparison && algorithm != RR && seen_quantum) { fprintf(stderr,"Error: --quantum is only for RR or comparison.\n"); return 1; }
    if (!read_processes(path, &input, &count, error, sizeof(error))) { fprintf(stderr,"Error: %s\n",error); return 1; }
    for (i = 0; i < (comparison ? 3 : 1); ++i) {
        if (!schedule(input, count, comparison ? (Algorithm)i : algorithm, quantum, &runs[i], error, sizeof(error))) { fprintf(stderr,"Error: %s\n",error); goto done; }
    }
    puts("Switches count direct transitions between different processes; idle transitions excluded.");
    if (comparison || algorithm == RR) printf("RR quantum: %" PRId64 "\n", quantum);
    puts("Algorithm Avg Waiting Avg Turnaround Avg Response Switches   Finish     Idle");
    for (i = 0; i < (comparison ? 3 : 1); ++i) row(comparison ? (Algorithm)i : algorithm, &runs[i]);
    for (i = 0; i < (comparison ? 3 : 1); ++i) detail(comparison ? (Algorithm)i : algorithm, &runs[i]);
    result = ferror(stdout) ? 1 : 0;
done:
    for (i = 0; i < 3; ++i) result_destroy(&runs[i]);
    free(input); return result;
}
