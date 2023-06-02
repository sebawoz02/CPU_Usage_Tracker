#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <stdatomic.h>

#include "queue.h"
#include "reader.h"
#include "analyzer.h"
#include "logger.h"
#include "watchdog.h"

// SIGNAL HANDLER
// volatile sig_atomic_t can be used to communicate only with a handler running in the same thread, it does not support multithreaded execution .
// C11 states that the use of the signal function in a multithreaded program is undefined behavior
// This solution succeeds on platforms where the atomic_bool type is always lock-free
// (the operation on such a variable will be performed in its entirety, without being interrupted by other threads.)
// The ATOMIC_BOOL_LOCK_FREE macro may have a value of 0, indicating that the type is never lock-free; a value of 1,
// indicating that the type is sometimes lock-free; or a value of 2, indicating that the type is always lock-free.
// Progran uses atomics when the availability of a lock-free atomic type can be determined at compile time; otherwise, it uses volatile sig_atomic_t.
#if ATOMIC_BOOL_LOCK_FREE == 0 || ATOMIC_BOOL_LOCK_FREE == 1
typedef volatile sig_atomic_t flag_type;
#define compare_flag(a, b) a == b
#else
typedef atomic_bool flag_type;
#define compare_flag(a, b) atomic_load(&g_termination_flag) == b
#endif

static flag_type g_termination_flag = ATOMIC_VAR_INIT(0);

// Reader - Analyzer : Producer - Consumer problem
static Queue* g_reader_analyzer_queue;

// Analyzer - Printer : Producer - Consumer problem
static Queue* g_analyzer_printer_queue;

// Number of cpus
static size_t g_no_cpus;

// Watchdog flag to make sure only one watchdog can execute exit() function which is not thread-safe
static atomic_flag g_wd_flag = ATOMIC_FLAG_INIT;


// SIGTERM sets termination flag which tells all the threads to clean data and exit
static void signal_handler(int signum)
{
    if(signum == SIGTERM)
        g_termination_flag = 1;
}


/**
 * Reader thread function
 */
static void* reader_func(void* args)
{
    WDCommunication * wdc = (WDCommunication *) args;
    while(1)
    {
        // Produce
        CPURawStats data = reader_load_data(g_no_cpus);
        // Add to the buffer
        if(queue_enqueue(g_reader_analyzer_queue, &data, 2) != QSUCCESS)
        {
            logger_write("Reader error while adding data to the buffer", LOG_ERROR);
            pthread_exit(NULL);
        }
        logger_write("READER - new data to analyze sent", LOG_INFO);

        if(compare_flag(g_termination_flag, 1))
            break;

        logger_write("READER - goes to sleep", LOG_INFO);
        watchdog_send_signal(wdc);

        // sleep 1 second
        struct timespec sleepTime;
        sleepTime.tv_sec = 1;
        sleepTime.tv_nsec = 0;
        nanosleep(&sleepTime, NULL);
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
    WDCommunication* wdc = (WDCommunication *) args;
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
    while(compare_flag(g_termination_flag, 0))
    {
        // Pop from buffer
        // Queue structure is thread safe
        if (queue_dequeue(g_reader_analyzer_queue, data, 2) != QSUCCESS)
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
            UsagePercentage to_print;
            to_print.cores_pr = malloc(sizeof(double)*(g_no_cpus));
            //Total
            to_print.total_pr =  analyzer_analyze(&prev_total[0], &prev_idle[0], data->total);

            // Cores
            for (size_t j = 0; j < g_no_cpus; ++j)
                to_print.cores_pr[j] =  analyzer_analyze(&prev_total[j+1], &prev_idle[j+1], data->cpus[j]);

            // Send to print
            if(queue_enqueue(g_analyzer_printer_queue, &to_print, 2) != QSUCCESS)
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
    WDCommunication * wdc = (WDCommunication *) args;
    // system func - there should not be any problems related to thread safety as long as there are no other threads attempting to call system concurrently.
    system("clear");
    // printf("\t\t\033[3;33m*** CUT - CPU Usage Tracker ~ Sebastian Wozniak ***\033[0m\n");  // print here using tput
    UsagePercentage* to_print = malloc(sizeof(double)*(g_no_cpus+1));
    if(to_print == NULL)
    {
        logger_write("Allocation error in printer thread", LOG_ERROR);
        pthread_exit(NULL);
    }
    while(compare_flag(g_termination_flag, 0))
    {
        size_t i;
        // Remove from buffer
        if (queue_dequeue(g_analyzer_printer_queue, to_print, 2) != QSUCCESS)
        {
            logger_write("Printer error while removing data from the buffer", LOG_ERROR);
            break;
        }
        logger_write("PRINTER - new data to print received", LOG_INFO);

        // Print
        // system("tput cup 1 0");  // - Better than clear, but it's buggy when terminal window is too small
        system("clear");
        printf("\t\t\033[3;33m*** CUT - CPU Usage Tracker ~ Sebastian Wozniak ***\033[0m\n"); // print here using clear
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
    WDCommunication* wdc = (WDCommunication *) args;
    struct timespec timeout;
    struct timeval now;

    pthread_mutex_lock(&wdc->mutex);
    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + 2;  // Timeout set to 2 seconds
    timeout.tv_nsec = now.tv_usec * 1000;

    while(compare_flag(g_termination_flag, 0))
    {
        // Wait 2 seconds for signal
        int result = pthread_cond_timedwait(&wdc->signal_cv, &wdc->mutex, &timeout);
        if (result != 0 && compare_flag(g_termination_flag, 0))
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
            if(!atomic_flag_test_and_set(&g_wd_flag))
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
    pthread_exit(NULL);
}

/**
 * Frees every element that is currently in the queue and destroys queues.
 */
static void queues_cleanup(void)
{
    CPURawStats to_free_1;
    while(!queue_is_empty(g_reader_analyzer_queue))
    {
        queue_dequeue(g_reader_analyzer_queue, &to_free_1, 2);
        free(to_free_1.cpus);
    }
    UsagePercentage to_free_2;
    while(!queue_is_empty(g_analyzer_printer_queue))
    {
        queue_dequeue(g_analyzer_printer_queue,&to_free_2, 2);
        free(to_free_2.cores_pr);
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
    pthread_t reader_th;
    pthread_t analyzer_th;
    pthread_t printer_th;

    if(signal(SIGTERM, signal_handler)== SIG_ERR)
        return EXIT_FAILURE;
    // Create logger
    if(logger_init() == LINIT_ERROR)
    {
        perror("Logger init error");
        return EXIT_FAILURE;
    }

    // Assign global variables
    g_no_cpus = reader_get_no_cpus();
    if(g_no_cpus == 0)
    {
        logger_write("Error while getting information about no cores", LOG_ERROR);
        logger_destroy();
        return EXIT_FAILURE;
    }
    g_reader_analyzer_queue = queue_create_new(10, sizeof(Stats)*(g_no_cpus+1));
    if(g_reader_analyzer_queue == NULL)
    {
        logger_write("Create new queue error", LOG_ERROR);
        logger_destroy();
        return EXIT_FAILURE;
    }
    g_analyzer_printer_queue = queue_create_new(10, sizeof(double)*(g_no_cpus+1));
    if(g_analyzer_printer_queue == NULL)
    {
        queue_delete(g_reader_analyzer_queue);
        logger_write("Create new queue error", LOG_ERROR);
        logger_destroy();
        return EXIT_FAILURE;
    }
    pthread_t watchdogs[3];

    // Create Reader thread
    if(watchdog_create_thread(&reader_th, reader_func, &watchdogs[0], watchdog_func) != 0)
    {
        thread_join_create_error("Failed to create reader thread");
        return EXIT_FAILURE;
    }
    logger_write("MAIN - Reader thread created", LOG_STARTUP);
    // Create Analyzer thread
    if(watchdog_create_thread(&analyzer_th, analyzer_func, &watchdogs[1], watchdog_func) != 0)
    {
        thread_join_create_error("Failed to create analyzer thread");
        return EXIT_FAILURE;
    }
    logger_write("MAIN - Analyzer thread created", LOG_STARTUP);
    // Create Printer thread
    if(watchdog_create_thread(&printer_th, printer_func, &watchdogs[2], watchdog_func) != 0)
    {
        thread_join_create_error("Failed to create printer thread");
        return EXIT_FAILURE;
    }
    logger_write("MAIN - Printer thread created", LOG_STARTUP);

    if(pthread_join(reader_th, NULL) != 0)
    {
        thread_join_create_error("Failed to join reader thread");
        return EXIT_FAILURE;
    }
    logger_write("Reader thread finished", LOG_WARNING);
    if(pthread_join(analyzer_th, NULL) != 0)
    {
        thread_join_create_error("Failed to join analyzer thread");
        return EXIT_FAILURE;
    }
    logger_write("Analyzer thread finished", LOG_WARNING);
    if(pthread_join(printer_th, NULL) != 0)
    {
        thread_join_create_error("Failed to join printer thread");
        return EXIT_FAILURE;
    }
    logger_write("Printer thread finished", LOG_WARNING);

    for(size_t i = 0; i < 3; i++)
    {
        if(pthread_join(watchdogs[i], NULL) != 0){
            thread_join_create_error("Watchdog thread join error");
            return EXIT_FAILURE;
        }
    }

    printf("exit\n");

    // Cleanup data and destroy logger
    queues_cleanup();
    logger_write("Closing program", LOG_INFO);
    logger_destroy();

    return EXIT_SUCCESS;
}
