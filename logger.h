
#ifndef CPU_USAGE_TRACKER_LOGGER_H
#define CPU_USAGE_TRACKER_LOGGER_H
#include <signal.h>

#include "queue.h"

typedef struct log_line log_line_t;

typedef struct Logger logger_t;

typedef enum
{
    LOG_DEBUG   = 0,
    LOG_INFO    = 1,
    LOG_WARNING = 2,
    LOG_ERROR   = 3,
    LOG_FATAL   = 4,
} log_level_t;

void logger_init(void);

void logger_write(const char* msg, log_level_t log_level);

void destroy_logger(void);

#endif //CPU_USAGE_TRACKER_LOGGER_H
