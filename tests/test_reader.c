#include <assert.h>

#include "test_reader.h"
#include "../reader.h"

static void test_reader_get_no_cpus(void);

static void test_reader_get_no_cpus(void){
    // AMD Ryzen 7 4800HS - 1 socket * 8 cores per socket * 2 threads per core = 16 CPU(s)
    assert(reader_get_no_cpus() == 16);
}

void test_reader_main(void){
    test_reader_get_no_cpus();
}
