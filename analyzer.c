#include "analyzer.h"

double analyzer_analyze(uint64_t* restrict prev_total, uint64_t* restrict prev_idle, const Stats data)
{
    double percentage;
    uint64_t idle, non_idle, totald, idled;

    idle = data.idle + data.iowait;
    non_idle = data.user + data.nice + data.system + data.irq +
               data.sortirq + data.steal;
    totald = idle + non_idle - *prev_total;
    idled = idle - *prev_idle;

    percentage = totald != 0 ? (double) ((totald - idled) * 100 / totald) : 0;

    *prev_total = idle + non_idle;
    *prev_idle = idle;

    return percentage;
}

void analyzer_update_prev(uint64_t* restrict prev_total, uint64_t* restrict prev_idle, const CPURawStats data, const size_t no_cpus)
{
    uint32_t idle, non_idle;

    idle = data.total.idle + data.total.iowait;
    non_idle = data.total.user + data.total.nice + data.total.system + data.total.irq +
               data.total.sortirq + data.total.steal;
    prev_total[0] = idle + non_idle;
    prev_idle[0] = idle;
    for (size_t j = 0; j < no_cpus; j++)
    {
        idle = data.cpus[j].idle + data.cpus[j].iowait;
        non_idle = data.cpus[j].user + data.cpus[j].nice + data.cpus[j].system + data.cpus[j].irq +
                   data.cpus[j].sortirq + data.cpus[j].steal;

        prev_total[j+1] = idle + non_idle;
        prev_idle[j+1] = idle;
    }
}
