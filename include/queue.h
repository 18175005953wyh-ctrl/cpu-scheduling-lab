#ifndef QUEUE_H
#define QUEUE_H
#include <stddef.h>
typedef struct { size_t *items, capacity, front, size; } Queue;
int queue_init(Queue *q, size_t capacity);
void queue_destroy(Queue *q);
int queue_push(Queue *q, size_t item);
int queue_pop(Queue *q, size_t *item);
#endif
