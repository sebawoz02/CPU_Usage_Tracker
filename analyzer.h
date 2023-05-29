
#ifndef CPU_USAGE_TRACKER_ANALYZER_H
#define CPU_USAGE_TRACKER_ANALYZER_H

#include <stddef.h>
#include "CPURawStats.h"

// CPU usage in % prepared by analyzer for printer
typedef struct UsagePercentage{
    double total_pr;
    double* cores_pr;
} UsagePercentage;

double analyzer_analyze(uint64_t* restrict prev_total, uint64_t* restrict prev_idle, Stats data);
void analyzer_update_prev(uint64_t* restrict prev_total, uint64_t* restrict prev_idle, CPURawStats data, size_t no_cpus);

#endif //CPU_USAGE_TRACKER_ANALYZER_H
