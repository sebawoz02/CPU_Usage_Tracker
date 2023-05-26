#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "logger.h"

#define LOGGER_MSG_MAX_SIZE 256
#define LOGGER_BUFFER_CAPACITY 128

struct log_line{
    log_level_t log_level;
    char message[LOGGER_MSG_MAX_SIZE + 1];
};

struct Logger{
    pthread_cond_t less;
    pthread_cond_t more;
    pthread_t log_thread;
    bool terminate;
};

static logger_t* logger_instance = NULL;
static Queue* g_buffer;

static void* logger_func(void* args)
{
    while(!logger_instance->terminate || !queue_is_empty(g_buffer))
    {
        return NULL;
    }
    return NULL;
}

/**
 * Creates logger_cv instance. If one instance already exists no action performed.
 */
void logger_init(void)
{
    if(logger_instance == NULL){
        g_buffer = queue_create_new(LOGGER_BUFFER_CAPACITY, sizeof(log_line_t));
        logger_instance = malloc(sizeof(logger_t));
        logger_instance->terminate = false;
        if(pthread_cond_init(&logger_instance->less, NULL)!=0){
            perror("Logger cv init error");
            free(logger_instance);
            return;
        }
        if(pthread_cond_init(&logger_instance->more, NULL)!=0){
            perror("Logger cv init error");
            free(logger_instance);
            return;
        }
        if (pthread_create(&logger_instance->log_thread, NULL, logger_func, NULL) != 0){
            perror("Logger thread init error");
            free(logger_instance);
            return;
        }
    }
}

/**
 * Stops current logger thread.
 */
void destroy_logger(void){
    if(logger_instance != NULL){
        logger_instance->terminate = true;
        pthread_join(logger_instance->log_thread, NULL);
        free(logger_instance);
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

