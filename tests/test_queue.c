#include "assert.h"

#include "../queue.h"
#include "test_queue.h"

/*
 * TESTS:
 * - Create new queue
 * - Delete queue
 * - Full/empty on the empty queue
 * - Enqueue
 * - Enqueue to the full queue
 * - Dequeue
 * - Dequeue from the empty queue
 * - Multiple enqueue and dequeue
 */
static void test_queue_create(void);
static void test_queue_delete(void);
static void test_queue_full_empty_corrupted(void);

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

void test_queue_main(void)
{
    test_queue_create();
    test_queue_delete();
    test_queue_full_empty_corrupted();
}


