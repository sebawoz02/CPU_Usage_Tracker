
#ifndef CPU_USAGE_TRACKER_ANALYZER_H
#define CPU_USAGE_TRACKER_ANALYZER_H

#include <stddef.h>
#include "CPURawStats.h"

// CPU usage in % prepared by analyzer for printer
typedef struct Usage_percentage{
    double total_pr;
    double* cores_pr;
} usage_percentage_t;

double analyzer_analyze(uint64_t* restrict prev_total, uint64_t* restrict prev_idle, stats_t data);
void analyzer_update_prev(uint64_t* restrict prev_total, uint64_t* restrict prev_idle, CPURawStats_t data, size_t no_cpus);

#endif //CPU_USAGE_TRACKER_ANALYZER_H
