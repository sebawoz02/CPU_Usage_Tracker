
#ifndef CPU_USAGE_TRACKER_ANALYZER_H
#define CPU_USAGE_TRACKER_ANALYZER_H

#include <stddef.h>
#include "CPURawStats.h"

double analyzer_analyze(uint64_t* restrict prev_total, uint64_t* restrict prev_idle, Stats data);
void analyzer_update_prev(uint64_t* restrict prev_total, uint64_t* restrict prev_idle, CPURawStats data, size_t no_cpus);

#endif //CPU_USAGE_TRACKER_ANALYZER_H
