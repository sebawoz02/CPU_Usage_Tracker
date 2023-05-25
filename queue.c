#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "queue.h"

#define QUEUE_MAGIC_NUMBER (int64_t) 0xdeadbeef


struct Queue {
    uint64_t magic;
    pthread_mutex_t mutex;  // thread safe dequeue and enqueue

    size_t tail;
    size_t head;

    size_t cur_no_elements;
    size_t capacity;

    size_t elem_size;
    uint8_t buffer[];
};

/**
 * Creates a new queue.
 * @param capacity - max no elements in the queue
 * @param data_size - size of one element
 * @return Pointer to the newly created queue.
 */
Queue* queue_create_new(const size_t capacity, const size_t data_size)
{
    Queue* q;

    if(capacity == 0){
        return NULL;
    }
    if(data_size == 0){
        return NULL;
    }

    q = malloc(sizeof(*q) + (data_size*capacity));
    if(q == NULL){
        return NULL;
    }

    if(pthread_mutex_init(&q->mutex, NULL)!=0)
    {
        perror("Queue mutex init error\n");
        free(q);
        return NULL;
    }

    q->tail = 0;
    q->head = 0;
    q->cur_no_elements = 0;
    q->magic = QUEUE_MAGIC_NUMBER;
    q->capacity = capacity;
    q->elem_size = data_size;

    return q;
}

/**
 * Deletes the queue and frees memory.
 * @param q - queue to delete
 */
void queue_delete(Queue* q)
{
    if(q==NULL)
        return;
    pthread_mutex_destroy(&q->mutex);
    free(q);
}

/**
 * Determines whether the queue is full.
 * @param q - queue
 * @return True if full else false.
 */
bool queue_is_full(const Queue * q){
    if(q == NULL)
        return false;
    if(queue_is_corrupted(q))
        return false;

    if(q->cur_no_elements == q->capacity)
        return true;
    return false;
}

/**
 * Determines whether the queue is empty.
 * @param q - queue
 * @return True if empty else false.
 */
bool queue_is_empty(const Queue* q){
    if(q == NULL)
        return false;
    if(queue_is_corrupted(q))
        return false;

    if(q->cur_no_elements == 0)
        return true;
    return false;
}

/**
 * Determines whether the queue is corrupted.
 * @param q - queue
 * @return True if corrupted else false.
 */
bool queue_is_corrupted(const Queue* q){
    if (q == NULL)
        return true;
    return q->magic != QUEUE_MAGIC_NUMBER;
}


/**
 * Adds new element to the queue.
 * @param q - queue
 * @param elem - element to add
 * @return 0 if added successfully else -1.
 */
int queue_enqueue(Queue* restrict const q, void* restrict const elem){

    if(q == NULL)
        return -1;
    if(elem == NULL)
        return -1;
    if(queue_is_full(q))
        return -1;

    pthread_mutex_lock(&q->mutex);

    uint8_t* const ptr = &q->buffer[q->head*q->elem_size];
    memcpy(ptr, elem, q->elem_size);

    q->cur_no_elements++;
    q->head = (q->head + 1) % q->capacity;

    pthread_mutex_unlock(&q->mutex);

    return 0;
}

/**
 * Removes element from the queue.
 * @param q - queue
 * @param elem - element to delete
 * @return 0 if removed successfully else -1.
 */
int queue_dequeue(Queue* restrict const q, void* restrict elem){
    if(q == NULL)
        return -1;
    if(elem == NULL)
        return -1;
    if(queue_is_corrupted(q))
        return -1;
    if(queue_is_empty(q))
        return -1;

    pthread_mutex_lock(&q->mutex);

    uint8_t * const ptr = &q->buffer[q->tail * q->elem_size];
    memcpy(elem, ptr, q->elem_size);

    q->cur_no_elements--;
    q->tail = (q->tail + 1) % q->capacity;

    pthread_mutex_unlock(&q->mutex);

    return 0;
}
