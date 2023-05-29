#include <stdlib.h>

#include "watchdog.h"
#include "logger.h"


/**
 * Creates new thread with a watchdog.
 * @param thread - thread identifier
 * @param th_fun - thread function
 * @return 0 on success, else -1
 */
int watchdog_create_thread(pthread_t* thread, void* (*th_fun)(void*), pthread_t* wd_thread, void* (*watchdog_func) (void*)){
    WDCommunication * wdc = malloc(sizeof(*wdc));
    if(wdc == NULL){
        logger_write("Watchdog allocation error", LOG_ERROR);
        return -1;
    }

    *wdc = (WDCommunication){.mutex = PTHREAD_MUTEX_INITIALIZER,
                                .signal_cv = PTHREAD_COND_INITIALIZER,
                                .monitored_thread = *thread
                                };

    if(pthread_create(thread, NULL, th_fun, wdc) != 0){
        logger_write("Monitored thread create error", LOG_ERROR);
        goto error_handler;
    }
    if(pthread_create(wd_thread, NULL, watchdog_func, wdc) != 0){
        logger_write("Watchdog thread create error", LOG_ERROR);
        goto error_handler;
    }
    return 0;
    
    error_handler:
        pthread_mutex_destroy(&wdc->mutex);
        pthread_cond_destroy(&wdc->signal_cv);
        free(wdc);
        return -1;
}

/**
 * Sends signal to watchdog.
 * @param wdc - mutex and cond variable used to communicate with threads watchdog.
 */
void watchdog_send_signal(WDCommunication* wdc){
    pthread_mutex_lock(&wdc->mutex);
    pthread_cond_signal(&wdc->signal_cv);
    pthread_mutex_unlock(&wdc->mutex);
}
