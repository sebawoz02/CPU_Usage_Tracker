#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "queue.h"

#define QUEUE_MAGIC_NUMBER (int64_t) 0xdeadbeef


/**
 *  QUEUE STRUCTURE IS DESIGNED TO BE USED IN PRODUCER-CONSUMER PROBLEM.
 *  ALL OPERATIONS ON BUFFER ARE THREAD SAFE,  PROTECTED BY MUTEX AND CONDITION VARIABLES
 *  QUEUE CAN STORE ALL DATA TYPES.
 */
struct Queue {
    pthread_cond_t less_cv;     // 48B - signals if there is fewer data in queue now
    pthread_cond_t more_cv;     //48B - signals if there is more data in queue now
    pthread_mutex_t mutex;  // 40B - mutex used to protect dequeue and enqueue is created with the structure
    uint64_t magic;     // 8B

    size_t tail;    // 8B
    size_t head;     // 8B

    size_t cur_no_elements; // 8B
    size_t capacity;    // 8B

    size_t elem_size;   // 8B
    uint8_t buffer[];   // Fixed size - FAM
};

/**
 * Creates a new queue.
 * @param capacity - max no elements in the queue
 * @param data_size - size of one element
 * @return Pointer to the newly created queue.
 */
Queue* queue_create_new(const size_t capacity, const size_t data_size)
{
    if(capacity == 0)
        return NULL;

    if(data_size == 0)
        return NULL;

    Queue* const q = calloc(1, sizeof(*q) + (data_size*capacity));  // Flexible Array Member
    if(q == NULL)
        return NULL;

    if(pthread_mutex_init(&q->mutex, NULL)!=0)
    {
        perror("Queue mutex init error\n");
        free(q);
        return NULL;
    }
    if(pthread_cond_init(&q->less_cv, NULL)!=0)
    {
        perror("Queue cond variable init error\n");
        pthread_mutex_destroy(&q->mutex);
        free(q);
        return NULL;
    }
    if(pthread_cond_init(&q->more_cv, NULL)!=0)
    {
        perror("Queue cond variable init error\n");
        pthread_mutex_destroy(&q->mutex);
        pthread_cond_destroy(&q->less_cv);
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
    pthread_cond_destroy(&q->less_cv);
    pthread_cond_destroy(&q->more_cv);
    free(q);
}

/**
 * Determines whether the queue is full.
 * @param q - queue
 * @return True if full else false.
 */
bool queue_is_full(const Queue * q)
{
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
bool queue_is_empty(const Queue* q)
{
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
bool queue_is_corrupted(const Queue* q)
{
    if (q == NULL)
        return true;
    return q->magic != QUEUE_MAGIC_NUMBER;
}


/**
 * Adds new element to the queue. When queue is full condition variable is used to wait for another thread to remove data.
 * data from queue.
 * @param q - queue
 * @param elem - element to add
 * @return 0 if added successfully else -1.
 */
int queue_enqueue(Queue* restrict const q, void* restrict const elem)
{
    if(q == NULL)
        return -1;
    if(elem == NULL)
        return -1;

    pthread_mutex_lock(&q->mutex);
    while(queue_is_full(q))
        pthread_cond_wait(&q->less_cv, &q->mutex);

    uint8_t* const ptr = &q->buffer[q->head*q->elem_size];
    memcpy(ptr, elem, q->elem_size);

    q->cur_no_elements++;
    q->head = (q->head + 1) % q->capacity;

    pthread_cond_signal(&q->more_cv);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

/**
 * Removes element from the queue. When queue is empty condition variable is used to wait for another thread to insert data.
 * @param q - queue
 * @param elem - element to delete
 * @return 0 if removed successfully else -1.
 */
int queue_dequeue(Queue* restrict const q, void* restrict elem)
{
    if(q == NULL)
        return -1;
    if(elem == NULL)
        return -1;
    if(queue_is_corrupted(q))
        return -1;

    pthread_mutex_lock(&q->mutex);
    while (queue_is_empty(q))
        pthread_cond_wait(&q->more_cv, &q->mutex);

    uint8_t * const ptr = &q->buffer[q->tail * q->elem_size];
    memcpy(elem, ptr, q->elem_size);

    q->cur_no_elements--;
    q->tail = (q->tail + 1) % q->capacity;

    pthread_cond_signal(&q->less_cv);
    pthread_mutex_unlock(&q->mutex);

    return 0;
}
