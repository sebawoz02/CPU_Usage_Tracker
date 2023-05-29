
#ifndef CPU_USAGE_TRACKER_WATCHDOG_H
#define CPU_USAGE_TRACKER_WATCHDOG_H

#include <pthread.h>

typedef struct Watchdog_communication{
    pthread_t monitored_thread;
    pthread_mutex_t mutex;
    pthread_cond_t signal_cv;
} WDCommunication;

int watchdog_create_thread(pthread_t* thread, void* (*th_fun)(void*), pthread_t* wd_thread, void* (*watchdog_func) (void*));

void watchdog_send_signal(WDCommunication * wdc);


#endif //CPU_USAGE_TRACKER_WATCHDOG_H
