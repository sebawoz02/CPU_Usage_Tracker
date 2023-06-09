
#ifndef CPU_USAGE_TRACKER_LOGGER_H
#define CPU_USAGE_TRACKER_LOGGER_H

#include "queue.h"

typedef enum
{
    LOG_INFO    = 0,
    LOG_WARNING = 1,
    LOG_ERROR   = 2,
    LOG_STARTUP = 3,
    LOG_DEBUG = 4
} log_level_t;

typedef enum
{
    LINIT_SUCCESS = 0,
    LINIT_ERROR = 1
} LoggerErrorCode;

LoggerErrorCode logger_init(void);

void logger_write(const char* msg, log_level_t log_level);

void logger_destroy(void);

#endif //CPU_USAGE_TRACKER_LOGGER_H
