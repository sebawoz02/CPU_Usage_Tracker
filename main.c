#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "queue.h"
#include "reader.h"
#include "analyzer.h"
#include "logger.h"

// Signal handling
static volatile sig_atomic_t g_termination_req = 0;

// Reader - Analyzer : Producer - Consumer problem
static Queue* g_reader_analyzer_queue;
static pthread_cond_t g_ra_more_cv; // signals that there is more data in queue now
static pthread_cond_t g_ra_less_cv; // signals that there is fewer data in queue now
static pthread_t g_reader_th;
static pthread_t g_analyzer_th;


// Analyzer - Printer : Producer - Consumer problem
static Queue* g_analyzer_printer_queue;
static pthread_cond_t g_ap_more_cv;
static pthread_cond_t g_ap_less_cv;
static pthread_t g_printer_th;


static size_t g_no_cpus;


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
        pthread_mutex_t* mut = queue_get_mutex(g_reader_analyzer_queue);
        pthread_mutex_lock(mut);
        while (queue_is_full(g_reader_analyzer_queue))
            pthread_cond_wait(&g_ra_less_cv, mut);  // wait for more space if needed

        assert(!queue_is_full(g_reader_analyzer_queue));

        if(queue_enqueue(g_reader_analyzer_queue, &data) != 0)
        {
            perror("Reader error while adding data to the buffer");
            return NULL;
        }

        pthread_cond_signal(&g_ra_more_cv);
        pthread_mutex_unlock(mut);

        if(g_termination_req == 1)
            break;

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
        perror("Allocation error");
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
        pthread_mutex_t* mut = queue_get_mutex(g_reader_analyzer_queue);
        pthread_mutex_lock(mut);
        while (queue_is_empty(g_reader_analyzer_queue))
            pthread_cond_wait(&g_ra_more_cv, mut);

        assert(!queue_is_empty(g_reader_analyzer_queue));
        // Queue structure is thread safe
        if (queue_dequeue(g_reader_analyzer_queue, data) != 0)
        {
            perror("Analyzer error while removing data from the buffer");
            free(prev_total);
            free(prev_idle);
            return NULL;
        }

        pthread_cond_signal(&g_ra_less_cv);
        pthread_mutex_unlock(mut);

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
            mut = queue_get_mutex(g_analyzer_printer_queue);
            pthread_mutex_lock(mut);
            while (queue_is_full(g_analyzer_printer_queue)) // wait for more space
                pthread_cond_wait(&g_ap_less_cv, mut);

            assert(!queue_is_full(g_analyzer_printer_queue));
            if(queue_enqueue(g_analyzer_printer_queue, &to_print) != 0)
            {
                perror("Analyzer error while adding data to the buffer");
                free(prev_total);
                free(prev_idle);
                return NULL;
            }
            pthread_cond_signal(&g_ap_more_cv);
            pthread_mutex_unlock(mut);
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
static void* printer_func(void* args) {
    // Only main thread needs to set the flag
    sigset_t threadMask;
    sigemptyset(&threadMask);
    sigaddset(&threadMask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &threadMask, NULL);

    system("clear");

    // printf("\t\t\033[3;33m*** CUT - CPU Usage Tracker ~ Sebastian Wozniak ***\033[0m\n");
    double *to_print;
    while(g_termination_req == 0){
        size_t i;
        // Remove from buffer
        pthread_mutex_t* mut = queue_get_mutex(g_analyzer_printer_queue);
        pthread_mutex_lock(mut);
        while (queue_is_empty(g_analyzer_printer_queue))
            pthread_cond_wait(&g_ap_more_cv, mut);
        if (queue_dequeue(g_analyzer_printer_queue, &to_print) != 0) {
            perror("Printer error while removing data from the buffer");
            return NULL;
        }
        pthread_cond_signal(&g_ap_less_cv);
        pthread_mutex_unlock(mut);

        // Print
        //system("tput cup 1 0");  // - Better than clear but its buggy when terminal window is too small
        system("clear");
        printf("\t\t\033[3;33m*** CUT - CPU Usage Tracker ~ Sebastian Wozniak ***\033[0m\n");
        printf("TOTAL:\t ╠");
        size_t pr = (size_t) to_print[0];
        for (i = 0; i < pr; i++) {
            printf("▒");
        }
        for (i = 0; i < 100 - pr; i++) {
            printf("-");
        }
        printf("╣ %.1f%% \n", to_print[0]);

        for (size_t j = 1; j < g_no_cpus + 1; j++) {
            printf("\033[0;%zumcpu%zu:\t ╠", 31 + ((j - 1) % 6), j);
            pr = (size_t) to_print[j];
            for (i = 0; i < pr; i++) {
                printf("▒");
            }
            for (i = 0; i < 100 - pr; i++) {
                printf("-");
            }
            printf("╣ %.1f%% \n", to_print[j]);
        }
        printf("\033[0m");

        free(to_print);
    }
    return NULL;
}

/**
 * Destroys all created condition variables.
 */
static void destroy_cv(void){
    pthread_cond_destroy(&g_ra_less_cv);
    pthread_cond_destroy(&g_ra_more_cv);
    pthread_cond_destroy(&g_ap_less_cv);
    pthread_cond_destroy(&g_ap_more_cv);
}

/**
 * Frees every element that is currently in the queue and destroys queues.
 */
static void queues_cleanup(void){
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

static void thread_join_create_error(const char* msg){
    perror(msg);
    queues_cleanup();
    destroy_cv();
}

static void signal_handler(int signum)
{
    g_termination_req = 1;
}


int main(void)
{
    signal(SIGTERM, signal_handler);

    // Reader - Analyzer semaphores
    if(pthread_cond_init(&g_ra_more_cv, NULL) != 0)
    {
        perror("Reader - Analyzer cv init error");
        return -1;
    }
    if(pthread_cond_init(&g_ra_less_cv, NULL) != 0)
    {
        perror("Reader - Analyzer cv init error");
        pthread_cond_destroy(&g_ra_more_cv);
        return -1;
    }

    // Analyzer - Printer semaphores
    if(pthread_cond_init(&g_ap_more_cv, NULL) != 0)
    {
        perror("Analyzer - Printer cv init error");
        destroy_cv();
        return -1;
    }
    if(pthread_cond_init(&g_ap_less_cv, NULL) != 0)
    {
        perror("Analyzer - Printer cv init error");
        destroy_cv();
        return -1;
    }

    // Assign global variables
    g_no_cpus = reader_get_no_cpus();
    if(g_no_cpus == 0)
    {
        perror("Error while getting information about no cores");
        destroy_cv();
        return -1;
    }
    g_reader_analyzer_queue = queue_create_new(10, sizeof(Stats)*(g_no_cpus+1));
    if(g_reader_analyzer_queue == NULL)
    {
        perror("Create new queue error");
        destroy_cv();
        return -1;
    }
    g_analyzer_printer_queue = queue_create_new(10, sizeof(double)*(g_no_cpus+1));
    if(g_analyzer_printer_queue == NULL)
    {
        queue_delete(g_reader_analyzer_queue);
        destroy_cv();
        perror("Create new queue error");
        return -1;
    }
    // Create Reader thread
    if(pthread_create(&g_reader_th, NULL, reader_func, NULL) != 0) {
        thread_join_create_error("Failed to create reader thread");
        return -1;
    }
    // Create Analyzer thread
    if(pthread_create(&g_analyzer_th, NULL, analyzer_func, NULL) != 0) {
        thread_join_create_error("Failed to create analyzer thread");
        return -1;
    }
    // Create Printer thread
    if(pthread_create(&g_printer_th, NULL, printer_func, NULL) != 0) {
        thread_join_create_error("Failed to create analyzer thread");
        return -1;
    }

    if(pthread_join(g_reader_th, NULL) != 0) {
        thread_join_create_error("Failed to join printer thread");
        return -1;
    }
    if(pthread_join(g_analyzer_th, NULL) != 0) {
        thread_join_create_error("Failed to join analyzer thread");
        return -1;
    }
    if(pthread_join(g_printer_th, NULL) != 0) {
        thread_join_create_error("Failed to join reader thread");
        return -1;
    }

    printf("exit\n");

    queues_cleanup();
    destroy_cv();

    return 0;
}
