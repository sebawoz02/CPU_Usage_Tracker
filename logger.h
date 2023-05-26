
#ifndef CPU_USAGE_TRACKER_LOGGER_H
#define CPU_USAGE_TRACKER_LOGGER_H
#include <signal.h>

#include "queue.h"

typedef struct log_line log_line_t;

typedef struct Logger logger_t;

typedef enum
{
    LOG_INFO    = 0,
    LOG_WARNING = 1,
    LOG_ERROR   = 2
} log_level_t;

int logger_init(void);

void logger_write(const char* msg, log_level_t log_level);

void destroy_logger(void);

#endif //CPU_USAGE_TRACKER_LOGGER_H
