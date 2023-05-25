#include <assert.h>
#include <stdlib.h>

#include "test_reader.h"
#include "../reader.h"

static void test_reader_get_no_cpus(void);

static void test_reader_get_no_cpus(void){
    // AMD Ryzen 7 4800HS - 1 socket * 8 cores per socket * 2 threads per core = 16 CPU(s)
    assert(reader_get_no_cpus() == 16);
}

static void test_reader_load_data(void)
{
    CPURawStats data = reader_load_data(reader_get_no_cpus()); // Just checking if it won't crash
    free(data.cpus);
}

void test_reader_main(void){
    test_reader_get_no_cpus();
    test_reader_load_data();
}
