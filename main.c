#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "queue.h"
#include "reader.h"
#include "analyzer.h"
#include "logger.h"

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

/**
 * Reader thread function
 */
static void* reader_func(void* args)
{
    sigset_t threadMask;
    sigemptyset(&threadMask);
    sigaddset(&threadMask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &threadMask, NULL);

    while(1)
    {
        // Produce
        CPURawStats data = reader_load_data(g_no_cpus);
        // Add to the buffer
        if(queue_enqueue(g_reader_analyzer_queue, &data) != 0)
        {
            logger_write("Reader error while adding data to the buffer", LOG_ERROR);
            return NULL;
        }
        logger_write("READER - new data to analyze sent", LOG_INFO);

        if(g_termination_req == 1)
            break;

        logger_write("READER - goes to sleep", LOG_INFO);
        sleep(1);   // Wait one second
    }
    return NULL;
}

/**
 * Analyzer thread function
 * Responsible for calculating the percentage of CPU usage from the prepared structure and
 * sending the results to the printer.
 */
static void* analyzer_func(void* args)
{
    CPURawStats* data = malloc(sizeof(Stats)*(g_no_cpus+1));

    bool first_iter = true;

    uint64_t* prev_total = malloc(sizeof(uint64_t)*(g_no_cpus+1));
    uint64_t* prev_idle = malloc(sizeof(uint64_t)*(g_no_cpus+1));

    if(prev_total == NULL || prev_idle == NULL)
    {
        logger_write("Allocation error", LOG_ERROR);
        // One of the pointers is possibly not NULL, free(ptr) - If ptr is NULL, no operation is performed.
        free(prev_idle);
        free(prev_total);
        return NULL;
    }
    sigset_t threadMask;
    sigemptyset(&threadMask);
    sigaddset(&threadMask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &threadMask, NULL);

    while(g_termination_req == 0)
    {
        // Pop from buffer
        // Queue structure is thread safe
        if (queue_dequeue(g_reader_analyzer_queue, data) != 0)
        {
            logger_write("Analyzer error while removing data from the buffer", LOG_ERROR);
            free(prev_total);
            free(prev_idle);
            return NULL;
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
            double* to_print = malloc(sizeof(double)*(g_no_cpus+1));
            //Total
            to_print[0] =  analyzer_analyze(&prev_total[0], &prev_idle[0], data->total);

            // Cores
            for (size_t j = 0; j < g_no_cpus; ++j)
                to_print[j+1] =  analyzer_analyze(&prev_total[j+1], &prev_idle[j+1], data->cpus[j]);

            // Send to print
            if(queue_enqueue(g_analyzer_printer_queue, &to_print) != 0)
            {
                logger_write("Analyzer error while adding data to the buffer", LOG_ERROR);
                free(prev_total);
                free(prev_idle);
                return NULL;
            }
            logger_write("ANALYZER - new data to print sent", LOG_INFO);
        }
        free(data->cpus);
    }
    // Cleanup
    free(data);
    free(prev_total);
    free(prev_idle);
    return NULL;
}

/**
 * Printer thread function.
 * Responsible for displaying prepared data in the terminal.
 */
static void* printer_func(void* args)
{
    // Only main thread needs to set the flag
    sigset_t threadMask;
    sigemptyset(&threadMask);
    sigaddset(&threadMask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &threadMask, NULL);

    system("clear");

    // printf("\t\t\033[3;33m*** CUT - CPU Usage Tracker ~ Sebastian Wozniak ***\033[0m\n");  // print here using tput
    double *to_print;
    while(g_termination_req == 0)
    {
        size_t i;
        // Remove from buffer
        if (queue_dequeue(g_analyzer_printer_queue, &to_print) != 0)
        {
            logger_write("Printer error while removing data from the buffer", LOG_ERROR);
            return NULL;
        }
        logger_write("PRINTER - new data to print received", LOG_INFO);

        // Print
        //system("tput cup 1 0");  // - Better than clear, but it's buggy when terminal window is too small
        system("clear");
        printf("\t\t\033[3;33m*** CUT - CPU Usage Tracker ~ Sebastian Wozniak ***\033[0m\n");
        printf("TOTAL:\t ╠");
        size_t pr = (size_t) to_print[0];
        for (i = 0; i < pr; i++)
            printf("▒");

        for (i = 0; i < 100 - pr; i++)
            printf("-");

        printf("╣ %.1f%% \n", to_print[0]);

        for (size_t j = 1; j < g_no_cpus + 1; j++)
        {
            printf("\033[0;%zumcpu%zu:\t ╠", 31 + ((j - 1) % 6), j);
            pr = (size_t) to_print[j];
            for (i = 0; i < pr; i++)
                printf("▒");

            for (i = 0; i < 100 - pr; i++)
                printf("-");

            printf("╣ %.1f%% \n", to_print[j]);
        }
        printf("\033[0m");

        free(to_print);
    }
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
    destroy_logger();
}

// SIGTERM sets termination flag which tells all the threads to clean data and exit
static void signal_handler(int signum)
{
    g_termination_req = 1;
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
        destroy_logger();
        return -1;
    }
    g_reader_analyzer_queue = queue_create_new(10, sizeof(Stats)*(g_no_cpus+1));
    if(g_reader_analyzer_queue == NULL)
    {
        logger_write("Create new queue error", LOG_ERROR);
        destroy_logger();
        return -1;
    }
    g_analyzer_printer_queue = queue_create_new(10, sizeof(double)*(g_no_cpus+1));
    if(g_analyzer_printer_queue == NULL)
    {
        queue_delete(g_reader_analyzer_queue);
        logger_write("Create new queue error", LOG_ERROR);
        destroy_logger();
        return -1;
    }

    // Create Reader thread
    if(pthread_create(&g_reader_th, NULL, reader_func, NULL) != 0)
    {
        thread_join_create_error("Failed to create reader thread");
        return -1;
    }
    logger_write("MAIN - Reader thread created", LOG_STARTUP);
    // Create Analyzer thread
    if(pthread_create(&g_analyzer_th, NULL, analyzer_func, NULL) != 0)
    {
        thread_join_create_error("Failed to create analyzer thread");
        return -1;
    }
    logger_write("MAIN - Analyzer thread created", LOG_STARTUP);
    // Create Printer thread
    if(pthread_create(&g_printer_th, NULL, printer_func, NULL) != 0)
    {
        thread_join_create_error("Failed to create analyzer thread");
        return -1;
    }
    logger_write("MAIN - Printer thread created", LOG_STARTUP);

    if(pthread_join(g_reader_th, NULL) != 0)
    {
        thread_join_create_error("Failed to join printer thread");
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
        thread_join_create_error("Failed to join reader thread");
        return -1;
    }
    logger_write("Printer thread finished", LOG_WARNING);

    printf("exit\n");

    // Cleanup data and destroy logger
    queues_cleanup();
    logger_write("Closing program", LOG_INFO);
    destroy_logger();

    return 0;
}
