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
    queue_delete(NULL); // check if it won't crash
}

void test_queue_main(void)
{
    test_queue_create();
    test_queue_delete();
}
