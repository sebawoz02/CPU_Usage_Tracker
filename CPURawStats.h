
#ifndef CPU_USAGE_TRACKER_CPURAWSTATS_H
#define CPU_USAGE_TRACKER_CPURAWSTATS_H

#include "stdint.h"

typedef struct Stats{
    uint32_t user;
    uint32_t nice;
    uint32_t system;
    uint32_t idle;
    uint32_t iowait;
    uint32_t irq;
    uint32_t sortirq;
    uint32_t steal;
}Stats;

typedef struct CPURawStats{
    Stats total;
    Stats* cpus;
} CPURawStats;


#endif //CPU_USAGE_TRACKER_CPURAWSTATS_H
