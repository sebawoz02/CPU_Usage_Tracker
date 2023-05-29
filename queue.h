
#ifndef CPU_USAGE_TRACKER_QUEUE_H
#define CPU_USAGE_TRACKER_QUEUE_H

#include <stddef.h>
#include <stdbool.h>

typedef enum{
    QSUCCESS = 0,
    QTIMEOUT = 1,
    QERROR = 2
}QueueErrorCode;

typedef struct Queue Queue; // Forward declaration

Queue* queue_create_new(size_t capacity, size_t data_size);
void queue_delete(Queue* q);

bool queue_is_full(const Queue * q);
bool queue_is_empty(const Queue* q);
bool queue_is_corrupted(const Queue* q);

QueueErrorCode queue_enqueue(Queue* restrict q, void* restrict elem, u_int8_t timeout);
QueueErrorCode queue_dequeue(Queue* restrict q, void* restrict elem, u_int8_t timeout);

#endif //CPU_USAGE_TRACKER_QUEUE_H
