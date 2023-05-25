#include <stdio.h>

#include "test_queue.h"
#include "test_reader.h"


int main(void)
{
    printf("Testing queue...");
    test_queue_main();
    printf("SUCCESS\n");
    printf("Testing reader...");
    test_reader_main();
    printf("SUCCESS\n");
    return 0;
}
