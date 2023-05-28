#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>

#include "queue.h"
#include "reader.h"
#include "analyzer.h"
#include "logger.h"
#include "watchdog.h"

// Signal handling
static volatile sig_atomic_t g_termination_req = 0;

// Reader - Analyzer : Producer - Consumer problem
static Queue* g_reader_analyzer_queue;
static pthread_t g_reader_th;
static pthread_t g_analyzer_th;

// Analyzer - Printer : Producer - Consumer problem
static Queue* g_analyzer_printer_queue;
static pthread_t g_printer_th;

static size_t g_no_cpus;    // number of cpus


// SIGTERM sets termination flag which tells all the threads to clean data and exit
static void signal_handler(int signum)
{
    (void)signum;
    g_termination_req = 1;
}

/**
 * Reader thread function
 */
static void* reader_func(void* args)
{
    wd_communication_t* wdc = (wd_communication_t*) args;
    while(1)
    {
        // Produce
        CPURawStats data = reader_load_data(g_no_cpus);
        // Add to the buffer
        if(queue_enqueue(g_reader_analyzer_queue, &data) != 0)
        {
            logger_write("Reader error while adding data to the buffer", LOG_ERROR);
            pthread_exit(NULL);
        }
        logger_write("READER - new data to analyze sent", LOG_INFO);

        if(g_termination_req == 1)
            break;

        logger_write("READER - goes to sleep", LOG_INFO);
        watchdog_send_signal(wdc);
        sleep(1);   // Wait one second
    }
    pthread_exit(NULL);
}

/**
 * Analyzer thread function
 * Responsible for calculating the percentage of CPU usage from the prepared structure and
 * sending the results to the printer.
 */
static void* analyzer_func(void* args)
{
    wd_communication_t* wdc = (wd_communication_t*) args;
    CPURawStats* data = malloc(sizeof(Stats)*(g_no_cpus+1));

    bool first_iter = true;

    uint64_t* prev_total = calloc(g_no_cpus+1 ,sizeof(uint64_t));
    uint64_t* prev_idle = calloc(g_no_cpus+1 ,sizeof(uint64_t));

    if(prev_total == NULL || prev_idle == NULL)
    {
        logger_write("Allocation error", LOG_ERROR);
        // One of the pointers is possibly not NULL, free(ptr) - If ptr is NULL, no operation is performed.
        free(prev_idle);
        free(prev_total);
        pthread_exit(NULL);
    }
    while(g_termination_req == 0)
    {
        // Pop from buffer
        // Queue structure is thread safe
        if (queue_dequeue(g_reader_analyzer_queue, data) != 0)
        {
            logger_write("Analyzer error while removing data from the buffer", LOG_ERROR);
            break;
        }
        logger_write("ANALYZER - new data to analyze received", LOG_INFO);

        // Consume / Analyze
        if (first_iter)
        {
            analyzer_update_prev(prev_total, prev_idle, *data, g_no_cpus);
            first_iter = false;
        }
        else
        {
            Usage_percentage to_print;
            to_print.cores_pr = malloc(sizeof(double)*(g_no_cpus));
            //Total
            to_print.total_pr =  analyzer_analyze(&prev_total[0], &prev_idle[0], data->total);

            // Cores
            for (size_t j = 0; j < g_no_cpus; ++j)
                to_print.cores_pr[j] =  analyzer_analyze(&prev_total[j+1], &prev_idle[j+1], data->cpus[j]);

            // Send to print
            if(queue_enqueue(g_analyzer_printer_queue, &to_print) != 0)
            {
                logger_write("Analyzer error while adding data to the buffer", LOG_ERROR);
                break;
            }
            logger_write("ANALYZER - new data to print sent", LOG_INFO);
        }
        free(data->cpus);
        watchdog_send_signal(wdc);
    }
    // Cleanup
    free(data);
    free(prev_total);
    free(prev_idle);
    pthread_exit(NULL);
}

/**
 * Printer thread function.
 * Responsible for displaying prepared data in the terminal.
 */
static void* printer_func(void* args)
{
    wd_communication_t* wdc = (wd_communication_t*) args;
    system("clear");
    printf("\t\t\033[3;33m*** CUT - CPU Usage Tracker ~ Sebastian Wozniak ***\033[0m\n");  // print here using tput
    Usage_percentage* to_print = malloc(sizeof(double)*(g_no_cpus+1));
    if(to_print == NULL)
    {
        logger_write("Allocation error in printer thread", LOG_ERROR);
        pthread_exit(NULL);
    }
    while(g_termination_req == 0)
    {
        size_t i;
        // Remove from buffer
        if (queue_dequeue(g_analyzer_printer_queue, to_print) != 0)
        {
            logger_write("Printer error while removing data from the buffer", LOG_ERROR);
            break;
        }
        logger_write("PRINTER - new data to print received", LOG_INFO);

        // Print
        system("tput cup 1 0");  // - Better than clear, but it's buggy when terminal window is too small
        //system("clear");
        //printf("\t\t\033[3;33m*** CUT - CPU Usage Tracker ~ Sebastian Wozniak ***\033[0m\n"); // print here using clear
        printf("TOTAL:\t ╠");
        size_t pr = (size_t) to_print->total_pr;
        for (i = 0; i < pr; i++)
            printf("▒");

        for (i = 0; i < 100 - pr; i++)
            printf("-");

        printf("╣ %.1f%% \n", to_print->total_pr);

        for (size_t j = 0; j < g_no_cpus; j++)
        {
            printf("\033[0;%zumcpu%zu:\t ╠", 31 + (j % 6), j+1);
            pr = (size_t) to_print->cores_pr[j];
            for (i = 0; i < pr; i++)
                printf("▒");

            for (i = 0; i < 100 - pr; i++)
                printf("-");

            printf("╣ %.1f%% \n", to_print->cores_pr[j]);
        }
        printf("\033[0m");
        free(to_print->cores_pr);

        watchdog_send_signal(wdc);
    }
    free(to_print);
    pthread_exit(NULL);
}

/**
 * Watchdog thread uses passed as parameters mutex and condition variable to communicate with one thread.
 * After not receiving any signal for 2 seconds he assumes that the thread is jammed and terminates the program.
 */
static void* watchdog_func(void* args)
{
    wd_communication_t* wdc = (wd_communication_t*) args;
    struct timespec timeout;
    struct timeval now;

    pthread_mutex_lock(&wdc->mutex);
    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + 2;  // Timeout set to 2 seconds
    timeout.tv_nsec = now.tv_usec * 1000;

    while(g_termination_req == 0)
    {
        // Wait 2 seconds for signal
        int result = pthread_cond_timedwait(&wdc->signal_cv, &wdc->mutex, &timeout);
        if (result != 0 && g_termination_req == 0)
        {
            char message[100];
            strcat(message,"Watchdog got no signal from thread: ");
            char th_id[128];
            sprintf(th_id, "%ld", wdc->monitored_thread);
            strcat(message, th_id);
            strcat(message, "\0");
            logger_write(message, LOG_ERROR);
            // Program termination
            perror("WATCHDOG GOT NO SIGNAL FROM THREAD - killing process");
            exit(1);

        } else
        {   // Timeout reset
            gettimeofday(&now, NULL);
            timeout.tv_sec = now.tv_sec + 2;
            timeout.tv_nsec = now.tv_usec * 1000;
        }
    }
    pthread_mutex_destroy(&wdc->mutex);
    pthread_cond_destroy(&wdc->signal_cv);
    free(wdc);
    return NULL;
}

/**
 * Frees every element that is currently in the queue and destroys queues.
 */
static void queues_cleanup(void)
{
    CPURawStats to_free_1;
    while(!queue_is_empty(g_reader_analyzer_queue))
    {
        queue_dequeue(g_reader_analyzer_queue, &to_free_1);
        free(to_free_1.cpus);
    }
    double* to_free_2;
    while(!queue_is_empty(g_analyzer_printer_queue))
    {
        queue_dequeue(g_analyzer_printer_queue,&to_free_2);
        free(to_free_2);
    }
    queue_delete(g_reader_analyzer_queue);
    queue_delete(g_analyzer_printer_queue);
}

static void thread_join_create_error(const char* msg)
{
    logger_write(msg, LOG_ERROR);
    queues_cleanup();
    logger_destroy();
}

int main(void)
{
    signal(SIGTERM, signal_handler);
    // Create logger
    if(logger_init() == -1)
    {
        perror("Logger init error");
        return -1;
    }

    // Assign global variables
    g_no_cpus = reader_get_no_cpus();
    if(g_no_cpus == 0)
    {
        logger_write("Error while getting information about no cores", LOG_ERROR);
        logger_destroy();
        return -1;
    }
    g_reader_analyzer_queue = queue_create_new(10, sizeof(Stats)*(g_no_cpus+1));
    if(g_reader_analyzer_queue == NULL)
    {
        logger_write("Create new queue error", LOG_ERROR);
        logger_destroy();
        return -1;
    }
    g_analyzer_printer_queue = queue_create_new(10, sizeof(double)*(g_no_cpus+1));
    if(g_analyzer_printer_queue == NULL)
    {
        queue_delete(g_reader_analyzer_queue);
        logger_write("Create new queue error", LOG_ERROR);
        logger_destroy();
        return -1;
    }
    pthread_t watchdogs[3];

    // Create Reader thread
    if(watchdog_create_thread(&g_reader_th, reader_func, &watchdogs[0],watchdog_func) != 0)
    {
        thread_join_create_error("Failed to create reader thread");
        return -1;
    }
    logger_write("MAIN - Reader thread created", LOG_STARTUP);
    // Create Analyzer thread
    if(watchdog_create_thread(&g_analyzer_th, analyzer_func, &watchdogs[1],watchdog_func) != 0)
    {
        thread_join_create_error("Failed to create analyzer thread");
        return -1;
    }
    logger_write("MAIN - Analyzer thread created", LOG_STARTUP);
    // Create Printer thread
    if(watchdog_create_thread(&g_printer_th, printer_func, &watchdogs[2], watchdog_func) != 0)
    {
        thread_join_create_error("Failed to create printer thread");
        return -1;
    }
    logger_write("MAIN - Printer thread created", LOG_STARTUP);

    if(pthread_join(g_reader_th, NULL) != 0)
    {
        thread_join_create_error("Failed to join reader thread");
        return -1;
    }
    logger_write("Reader thread finished", LOG_WARNING);
    if(pthread_join(g_analyzer_th, NULL) != 0)
    {
        thread_join_create_error("Failed to join analyzer thread");
        return -1;
    }
    logger_write("Analyzer thread finished", LOG_WARNING);
    if(pthread_join(g_printer_th, NULL) != 0)
    {
        thread_join_create_error("Failed to join printer thread");
        return -1;
    }
    logger_write("Printer thread finished", LOG_WARNING);

    for(size_t i = 0; i < 3; i++)
    {
        if(pthread_join(watchdogs[i], NULL) != 0){
            thread_join_create_error("Watchdog thread join error");
            return -1;
        }
    }

    printf("exit\n");

    // Cleanup data and destroy logger
    queues_cleanup();
    logger_write("Closing program", LOG_INFO);
    logger_destroy();

    return 0;
}
