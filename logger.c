#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>

#include "logger.h"

#define LOGGER_MSG_MAX_SIZE 255 // 256-th is null terminator
#define LOGGER_BUFFER_CAPACITY 128

typedef struct log_line{
    log_level_t log_level;
    char message[LOGGER_MSG_MAX_SIZE + 1];
}log_line_t;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
typedef struct Logger{
    pthread_t log_thread;   // 8B
    atomic_bool term_flag; // 1B
     // 7B padding
} Logger;
#pragma GCC diagnostic pop

static Logger* logger_instance = NULL;
static atomic_flag g_logger_initialized = ATOMIC_FLAG_INIT;
static Queue* g_buffer;


/**
 * Creates new for the new log file with a timestamp.
 * @param fileName - pointer where to save new log file name
 */
static void createLogFileName(char* fileName)
{
    time_t rawTime;
    struct tm* timeInfo = malloc(sizeof(*timeInfo));

    time(&rawTime);
    localtime_r(&rawTime, timeInfo);

    strftime(fileName, 256, "log_%Y%m%d_%H%M%S.txt", timeInfo);
    free(timeInfo);
}

// Logger thread func - appends logs to the file
static void* logger_func(void* args)
{
    (void)args;
    char filename[256];

    createLogFileName(filename);
    log_line_t* new_log = malloc(sizeof(log_line_t));
    if(new_log == NULL){
        perror("Allocation error in logger thread");
        pthread_exit(NULL);
    }

    while(atomic_load(&logger_instance->term_flag) == false || !queue_is_empty(g_buffer))
    {
        QueueErrorCode ret = QTIMEOUT;
        while (atomic_load(&logger_instance->term_flag) == false && ret == QTIMEOUT)
            ret = queue_dequeue(g_buffer, new_log, 2);  // CHECK every 2 seconds if termination flag is not up

        char prefix[32];
        switch (new_log->log_level) {
            case LOG_INFO:
                strcpy(prefix, "[INFO]\t");
                break;
            case LOG_WARNING:
                strcpy(prefix, "[WARNING]");
                break;
            case LOG_ERROR:
                strcpy(prefix, "[ERROR]\t");
                break;
            case LOG_STARTUP:
                strcpy(prefix, "[STARTUP]");
                break;
            case LOG_DEBUG:
                strcpy(prefix, "[DEBUG]\t");
                break;
        }
        time_t currentTime;
        struct tm* localTime = malloc(sizeof(*localTime));
        char dateTime[20];
        currentTime = time(NULL);
        localtime_r(&currentTime, localTime);
        strftime(dateTime, sizeof(dateTime), "%Y-%m-%d %H:%M:%S", localTime);

        FILE* log_file = fopen(filename, "a+");
        if(log_file == NULL)
        {
            perror("Logger failed to create new file.");
            pthread_exit(NULL);
        }
        fprintf(log_file, "[%s]", dateTime);
        fprintf(log_file, "%s\t", prefix);
        fprintf(log_file, "%s\n", new_log->message);
        fclose(log_file);
        free(localTime);
    }
    free(new_log);
    pthread_exit(NULL);
}

/**
 * Creates logger thread. If one thread is already running no action performed.
 * @return return LINIT_SUCCESS on success, else LINIT_ERROR
 */
LoggerErrorCode logger_init(void)
{
    if(atomic_flag_test_and_set(&g_logger_initialized) == 0)
    {
        g_buffer = queue_create_new(LOGGER_BUFFER_CAPACITY, sizeof(log_line_t));
        logger_instance = malloc(sizeof(Logger));
        *logger_instance = (Logger){
            .term_flag = ATOMIC_VAR_INIT(0)
        };
        if (pthread_create(&logger_instance->log_thread, NULL, logger_func, NULL) != 0)
        {
            perror("Logger thread init error");
            free(logger_instance);
            atomic_flag_clear(&g_logger_initialized);
            return LINIT_ERROR;
        }
        return LINIT_SUCCESS;
    }
    return LINIT_ERROR;
}

/**
 * Stops current logger thread.
 */
void logger_destroy(void)
{
    if(logger_instance != NULL)
    {
        atomic_store(&logger_instance->term_flag, true);
        pthread_join(logger_instance->log_thread, NULL);
        free(logger_instance);
        queue_delete(g_buffer);
    }
}

/**
 * Sends log msg to the logger thread.
 * @param msg - log message
 * @param log_level - importance of log
 */
void logger_write(const char* msg, log_level_t log_level)
{
    if(logger_instance == NULL)
        return;
    if(atomic_load(&logger_instance->term_flag) != false)     // Logger is closed for receiving new messages
        return;
    if(g_buffer == NULL)
        return;
    log_line_t new_log;
    if(strlen(msg) > LOGGER_MSG_MAX_SIZE)
    {
        strncpy(new_log.message, msg, LOGGER_MSG_MAX_SIZE);
        new_log.message[LOGGER_MSG_MAX_SIZE] = '\0';
    } else{
        strcpy(new_log.message, msg);
    }

    new_log.log_level = log_level;
    queue_enqueue(g_buffer, &new_log, 2);
}
