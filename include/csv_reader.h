#ifndef CSV_READER_H
#define CSV_READER_H
#include "process.h"
int read_processes(const char *path, Process **items, size_t *count, char *error, size_t error_size);
#endif
