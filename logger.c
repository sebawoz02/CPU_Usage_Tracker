#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "logger.h"

#define LOGGER_MSG_MAX_SIZE 255 //256-th is null terminator
#define LOGGER_BUFFER_CAPACITY 128

struct log_line{
    log_level_t log_level;
    char message[LOGGER_MSG_MAX_SIZE + 1];
};

struct Logger{
    // 7 byte alignment
    pthread_cond_t less;
    pthread_cond_t more;
    pthread_t log_thread;
    sig_atomic_t terminate;
};

static logger_t* logger_instance = NULL;
static sig_atomic_t g_logger_initialized = 0;
static Queue* g_buffer;


/**
 * Creates new for the new log file with a timestamp.
 * @param fileName - pointer where to save new log file name
 */
static void createLogFileName(char* fileName) {
    time_t rawTime;
    struct tm* timeInfo;

    time(&rawTime);
    timeInfo = localtime(&rawTime);

    strftime(fileName, 256, "../logs/log_%Y%m%d_%H%M%S.txt", timeInfo);
}

// Logger thread func - appends logs to the file
static void* logger_func(void* args)
{
    char filename[256];

    createLogFileName(filename);
    FILE* log_file = fopen(filename, "a+");
    if(log_file == NULL)
    {
        perror("Logger failed to create new file.");
        return NULL;
    }
    log_line_t* new_log = malloc(sizeof(log_line_t));
    pthread_mutex_t* mut = queue_get_mutex(g_buffer);

    while(logger_instance->terminate == 0 || !queue_is_empty(g_buffer))
    {
        pthread_mutex_lock(mut);
        while (queue_is_empty(g_buffer))
            pthread_cond_wait(&logger_instance->more, mut);
        queue_dequeue(g_buffer, new_log);
        pthread_cond_signal(&logger_instance->less);
        pthread_mutex_unlock(mut);

        char prefix[32];
        switch (new_log->log_level) {
            case LOG_INFO:
                strcpy(prefix, "[INFO]");
                break;
            case LOG_WARNING:
                strcpy(prefix, "[WARNING]");
                break;
            case LOG_ERROR:
                strcpy(prefix, "[ERROR]");
                break;
            case LOG_STARTUP:
                strcpy(prefix, "[STARTUP]");
                break;
            case LOG_DEBUG:
                strcpy(prefix, "[DEBUG]");
                break;
        }
        time_t currentTime;
        struct tm *localTime;
        char dateTime[20];
        // Pobranie obecnego czasu
        currentTime = time(NULL);
        // Konwersja czasu na lokalny czas
        localTime = localtime(&currentTime);
        // Formatowanie daty i godziny
        strftime(dateTime, sizeof(dateTime), "%Y-%m-%d %H:%M:%S", localTime);

        fprintf(log_file, "%s\t", prefix);
        fprintf(log_file, "[%s]\t", dateTime);
        fprintf(log_file, "%s\n", new_log->message);
    }
    fclose(log_file);
    free(new_log);
    return NULL;
}

/**
 * Creates logger_cv instance. If one instance already exists no action performed.
 * @return return 0 on success, else -1
 */
int logger_init(void)
{
    if(g_logger_initialized == 0){
        g_logger_initialized = 1;
        g_buffer = queue_create_new(LOGGER_BUFFER_CAPACITY, sizeof(log_line_t));
        logger_instance = malloc(sizeof(logger_t));
        logger_instance->terminate = 0;
        if(pthread_cond_init(&logger_instance->less, NULL)!=0){
            perror("Logger cv init error");
            goto exit_error;
        }
        if(pthread_cond_init(&logger_instance->more, NULL)!=0){
            perror("Logger cv init error");
            goto exit_error;
        }
        if (pthread_create(&logger_instance->log_thread, NULL, logger_func, NULL) != 0){
            perror("Logger thread init error");
            goto exit_error;
        }
        return 0;

        exit_error:
        free(logger_instance);
        g_logger_initialized = 0;
        return -1;
    }
    return -1;
}

/**
 * Stops current logger thread.
 */
void destroy_logger(void){
    if(logger_instance != NULL){
        logger_instance->terminate = 1;
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
void logger_write(const char* msg, log_level_t log_level){
    if(logger_instance == NULL)
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

    pthread_mutex_t* mut = queue_get_mutex(g_buffer);
    pthread_mutex_lock(mut);
    while(queue_is_full(g_buffer))
        pthread_cond_wait(&logger_instance->less, mut);
    queue_enqueue(g_buffer, &new_log);
    pthread_cond_signal(&logger_instance->more);
    pthread_mutex_unlock(mut);
}

