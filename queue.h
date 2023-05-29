
#ifndef CPU_USAGE_TRACKER_QUEUE_H
#define CPU_USAGE_TRACKER_QUEUE_H

#include <stddef.h>
#include <stdbool.h>

typedef struct Queue Queue; // Forward declaration

Queue* queue_create_new(size_t capacity, size_t data_size);
void queue_delete(Queue* q);

bool queue_is_full(const Queue * q);
bool queue_is_empty(const Queue* q);
bool queue_is_corrupted(const Queue* q);

int queue_enqueue(Queue* restrict q, void* restrict elem, u_int8_t timeout);
int queue_dequeue(Queue* restrict q, void* restrict elem, u_int8_t timeout);

#endif //CPU_USAGE_TRACKER_QUEUE_H
