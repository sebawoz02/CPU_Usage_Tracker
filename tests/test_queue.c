#include <assert.h>
#include <stdlib.h>

#include "../reader.h"
#include "../queue.h"
#include "test_queue.h"

/*
 * TESTS:
 * - Create new queue
 * - Delete queue
 * - Full/empty on the empty queue
 * - Enqueue
 * - Dequeue
 * - Multiple enqueue and dequeue
 * - Behaviour when enqueueing and dequeue structure used in program
 */
static void test_queue_create(void);
static void test_queue_delete(void);
static void test_queue_full_empty_corrupted(void);
static void test_queue_dequeue(void);
static void test_queue_enqueue(void);
static void test_queue_multiple_enqueue_dequeue(void);
static void test_queue_with_cpurawstats(void);


static void test_queue_create(void)
{
    Queue* q;

    assert(queue_create_new(0, sizeof(int)) == NULL);

    assert(queue_create_new(3, 0) == NULL);

    assert(queue_create_new(0, 0) == NULL);

    q = queue_create_new(3, sizeof(int));
    assert(q != NULL);
    queue_delete(q);
}

static void test_queue_delete(void)
{
    queue_delete(NULL);
}

static void test_queue_full_empty_corrupted(void){
    Queue* q = queue_create_new(1, 1);
    assert(q != NULL);

    assert(queue_is_corrupted(NULL)==true);
    assert(queue_is_empty(NULL)==false);
    assert(queue_is_full(NULL)==false);

    assert(queue_is_full(q) == false);
    assert(queue_is_empty(q) == true);
    assert(queue_is_corrupted(q) == false);

    queue_delete(q);
}

static void test_queue_enqueue(void){
    size_t value_to_insert = 45;
    register const size_t no_elems = 10;
    register const size_t elem_size = sizeof(int);

    Queue* const q = queue_create_new(no_elems, elem_size);
    assert(q != NULL);

    assert(queue_enqueue(NULL, &value_to_insert) != 0);
    assert(queue_enqueue(q, NULL) != 0);
    assert(queue_enqueue(NULL, NULL) != 0);

    assert(queue_is_empty(q));
    assert(!queue_is_full(q));

    assert(queue_enqueue(q, &value_to_insert) == 0);

    assert((volatile int)value_to_insert == 45);

    assert(!queue_is_empty(q));
    assert(!queue_is_full(q));

    queue_delete(q);
}

static void test_queue_dequeue(void){
    size_t value_to_insert = 45;
    register const size_t no_elems = 10;
    register const size_t elem_size = sizeof(size_t);
    size_t val = 0;

    Queue* const q = queue_create_new(no_elems, elem_size);

    assert(q != NULL);
    assert(!queue_is_corrupted(q));
    assert(queue_is_empty(q));
    assert(!queue_is_full(q));

    assert(queue_enqueue(q, &value_to_insert) == 0);
    assert(!queue_is_empty(q));
    assert(!queue_is_full(q));

    assert(queue_dequeue(NULL, &val) != 0);
    assert(queue_dequeue(NULL, NULL) != 0);
    assert(queue_dequeue(q, NULL) != 0);

    assert(queue_dequeue(q, &val) == 0);

    assert(val == value_to_insert);

    assert(queue_is_empty(q));
    assert(!queue_is_full(q));

    queue_delete(q);
}

static void test_queue_multiple_enqueue_dequeue(void){
    size_t* values_to_insert = malloc(sizeof(size_t)*4);
    values_to_insert[0] = 45;
    values_to_insert[1] = 2;
    values_to_insert[2] = 23;
    values_to_insert[3] = 15;
    register const size_t no_elems = 10;
    register const size_t elem_size = sizeof(size_t);
    size_t val = 0;

    Queue* q = queue_create_new(no_elems, elem_size);
    assert(q != NULL);

    assert(queue_enqueue(q, &values_to_insert[0])==0);
    assert(queue_enqueue(q, &values_to_insert[1])==0);

    assert(!queue_is_full(q));
    assert(!queue_is_empty(q));

    assert(queue_enqueue(q, &values_to_insert[2])==0);
    assert(queue_enqueue(q, &values_to_insert[3])==0);

    assert(!queue_is_full(q));
    assert(!queue_is_empty(q));

    assert(queue_dequeue(q, &val) == 0);
    assert(val==45);
    assert(queue_dequeue(q, &val) == 0);
    assert(val==2);

    assert(!queue_is_full(q));
    assert(!queue_is_empty(q));

    assert(queue_dequeue(q, &val) == 0);
    assert(val==23);
    assert(queue_dequeue(q, &val) == 0);
    assert(val==15);

    assert(!queue_is_full(q));
    assert(queue_is_empty(q));

    queue_delete(q);
    free(values_to_insert);
}

static void test_queue_with_cpurawstats(void){
    size_t cpus = reader_get_no_cpus();
    CPURawStats stats = reader_load_data(cpus);
    Queue* q = queue_create_new(2, sizeof(stats));
    assert(q != NULL);
    CPURawStats dequeued_stats;

    assert(queue_enqueue(q, &stats) == 0);

    stats = reader_load_data(cpus);

    assert(queue_enqueue(q, &stats) == 0);

    assert(queue_is_full(q));

    assert(queue_dequeue(q, &dequeued_stats) == 0);
    free(dequeued_stats.cpus);

    assert(queue_dequeue(q, &dequeued_stats) == 0);

    free(dequeued_stats.cpus);

    assert(queue_is_empty(q));

    queue_delete(q);
}

void test_queue_main(void)
{
    test_queue_create();
    test_queue_delete();
    test_queue_full_empty_corrupted();
    test_queue_enqueue();
    test_queue_dequeue();
    test_queue_multiple_enqueue_dequeue();
    test_queue_with_cpurawstats();
}
