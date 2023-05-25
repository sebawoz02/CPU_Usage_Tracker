
#ifndef CPU_USAGE_TRACKER_QUEUE_H
#define CPU_USAGE_TRACKER_QUEUE_H

#include <stddef.h>

typedef struct Queue Queue;

Queue* queue_create_new(size_t capacity, size_t data_size);
void queue_delete(Queue* q);

#endif //CPU_USAGE_TRACKER_QUEUE_H
