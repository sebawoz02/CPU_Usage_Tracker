#include <stdlib.h>
#include <stdio.h>

#include "logger.h"


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

    }
    return NULL;
}

/**
 * Creates logger_cv instance. If one instance already exists no action performed.
 * Singleton design pattern.
 * @param queue - queue used as log messages buffer
 * @param term_flag - pointer to termination flag that will tell logger to quit
 * @return pointer to logger thread. NULL if error occured
 */
pthread_t* logger_init(Queue* queue, sig_atomic_t* term_flag)
{
    if(logger_instance == NULL){
        g_buffer = queue;
        logger_instance = malloc(sizeof(logger_t));
        logger_instance->terminate = false;
        if(pthread_cond_init(&logger_instance->less, NULL)!=0){
            perror("Logger cv init error");
            free(logger_instance);
            return NULL;
        }
        if(pthread_cond_init(&logger_instance->more, NULL)!=0){
            perror("Logger cv init error");
            free(logger_instance);
            return NULL;
        }
        if (pthread_create(&logger_instance->log_thread, NULL, logger_func, NULL) != 0){
            perror("Logger thread init error");
            free(logger_instance);
            return NULL;
        }
    }
    return &logger_instance->log_thread;
}
