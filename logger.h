
#ifndef CPU_USAGE_TRACKER_LOGGER_H
#define CPU_USAGE_TRACKER_LOGGER_H
#include <signal.h>

#include "queue.h"

typedef struct Logger logger_t;

pthread_t* logger_init(Queue* queue, sig_atomic_t* term_flag);

void destroy_logger(void);

#endif //CPU_USAGE_TRACKER_LOGGER_H
