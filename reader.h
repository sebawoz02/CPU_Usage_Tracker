
#ifndef CPU_USAGE_TRACKER_READER_H
#define CPU_USAGE_TRACKER_READER_H

#include <stddef.h>
#include "CPURawStats.h"

size_t reader_get_no_cpus(void);

CPURawStats_t reader_load_data(size_t no_cpus);

#endif //CPU_USAGE_TRACKER_READER_H
