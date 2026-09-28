#include "csv_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
static char *trim(char *p) {
    char *end;
    while (isspace((unsigned char)*p)) ++p;
    end = p + strlen(p);
    while (end > p && isspace((unsigned char)end[-1])) --end;
    *end = 0; return p;
}
static int number(const char *p, int64_t *out) {
    int64_t n = 0;
    if (!*p) return 0;
    while (*p) {
        int digit = *p++ - '0';
        if (digit < 0 || digit > 9 || n > (INT64_MAX - digit) / 10) return 0;
        n = n * 10 + digit;
    }
    *out = n; return 1;
}
int read_processes(const char *path, Process **items, size_t *count, char *error, size_t error_size) {
    FILE *f = fopen(path, "rb");
    Process *data = NULL;
    size_t n = 0, capacity = 0, line_no = 0;
    const char *reason = NULL;
    int ch;
    *items = NULL; *count = 0;
    if (!f) { snprintf(error, error_size, "Cannot open CSV: %s", path); return 0; }
    while ((ch = fgetc(f)) != EOF) {
        char line[512], *pid, *arrival, *burst;
        size_t len = 0, i;
        int invalid = 0;
        Process p = {0};
        ++line_no;
        do {
            if (ch == '\n') break;
            if (!ch) invalid = 1;
            if (len < sizeof(line)-1) line[len++] = (char)ch; else invalid = 1;
        } while ((ch = fgetc(f)) != EOF);
        line[len] = 0;
        if (invalid) { reason = "record too long or contains NUL"; break; }
        pid = trim(line);
        arrival = strchr(pid, ',');
        if (!arrival) { reason = "expected three CSV fields"; break; }
        *arrival++ = 0;
        burst = strchr(arrival, ',');
        if (!burst) { reason = "expected three CSV fields"; break; }
        *burst++ = 0;
        pid = trim(pid); arrival = trim(arrival); burst = trim(burst);
        if (line_no == 1 && !strcmp(pid,"pid") && !strcmp(arrival,"arrival") && !strcmp(burst,"burst")) continue;
        if (line_no == 1) { reason = "required header: pid,arrival,burst"; break; }
        if (!*pid || strlen(pid) > 31) { reason = "PID must contain 1..31 characters"; break; }
        for (i = 0; pid[i]; ++i) {
            unsigned char c = (unsigned char)pid[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) break;
        }
        if (pid[i]) { reason = "PID allows ASCII letters, digits, underscore and hyphen"; break; }
        if (!number(arrival, &p.arrival) || !number(burst, &p.burst) || !p.burst) { reason = "arrival must be >= 0 and burst > 0, within int64 range"; break; }
        for (i = 0; i < n; ++i) if (!strcmp(data[i].pid, pid)) break;
        if (i != n) { reason = "duplicate PID"; break; }
        if (n == MAX_PROCESSES) { reason = "process limit exceeded (10000)"; break; }
        if (n == capacity) {
            size_t next = capacity ? capacity * 2 : 16;
            Process *grown = realloc(data, next * sizeof(*data));
            if (!grown) { reason = "allocation failed"; break; }
            data = grown; capacity = next;
        }
        strcpy(p.pid, pid); p.remaining = p.burst; p.first_run = -1;
        data[n++] = p;
    }
    if (ferror(f)) reason = "CSV read failed";
    if (fclose(f) != 0) reason = "CSV close failed";
    if (!reason && !n) reason = "empty input: at least one process required";
    if (reason) { snprintf(error, error_size, "Line %zu: %s", line_no ? line_no : 1, reason); free(data); return 0; }
    *items = data; *count = n; return 1;
}
