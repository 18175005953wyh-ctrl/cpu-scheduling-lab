#include "queue.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
int queue_init(Queue *q, size_t capacity) {
    memset(q, 0, sizeof(*q));
    if (!capacity || capacity > SIZE_MAX / sizeof(*q->items)) return 0;
    q->items = malloc(capacity * sizeof(*q->items));
    if (!q->items) return 0;
    q->capacity = capacity; return 1;
}
void queue_destroy(Queue *q) { free(q->items); memset(q, 0, sizeof(*q)); }
int queue_push(Queue *q, size_t item) {
    if (q->size == q->capacity) return 0;
    q->items[(q->front + q->size) % q->capacity] = item; ++q->size; return 1;
}
int queue_pop(Queue *q, size_t *item) {
    if (!q->size) return 0;
    *item = q->items[q->front]; q->front = (q->front + 1) % q->capacity; --q->size; return 1;
}
