#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>

#include "queue.h"
#include "CPURawStats.h"
#include "reader.h"
#include "analyzer.h"

// Reader - Analyzer : Producer - Consumer problem
static Queue* g_reader_analyzer_queue;
static sem_t g_ra_sem_full;
static sem_t g_ra_sem_empty;
static pthread_t g_reader_th;
static pthread_t g_analyzer_th;

// Analyzer - Printer : Producer - Consumer problem
static Queue* g_analyzer_printer_queue;
static sem_t g_ap_sem_full;
static sem_t g_ap_sem_empty;
static pthread_t g_printer_th;

static size_t g_no_cpus;

/**
 * Reader thread function
 */
static void* reader_func(void* args)
{
    while(1)
    {
        // Produce
        CPURawStats data = reader_load_data(g_no_cpus);
        // Add to the buffer
        sem_wait(&g_ra_sem_empty);
        // Queue structure is thread safe
        if(queue_enqueue(g_reader_analyzer_queue, &data) != 0)
        {
            perror("Reader error while adding data to the buffer");
            pthread_exit(NULL);
        }
        sem_post(&g_ra_sem_full);
        sleep(1);   // Wait one second
    }
}

/**
 * Analyzer thread function
 * Responsible for calculating the percentage of CPU usage from the prepared structure and
 * sending the results to the printer.
 */
static void* analyzer_func(void* args)
{
    CPURawStats data;

    bool first_iter = true;

    uint64_t* prev_total = malloc(sizeof(uint64_t)*(g_no_cpus+1));
    uint64_t* prev_idle = malloc(sizeof(uint64_t)*(g_no_cpus+1));

    if(prev_total == NULL || prev_idle == NULL)
    {
        perror("Allocation error");
        // One of the pointers is possibly not NULL, free(ptr) - If ptr is NULL, no operation is performed.
        free(prev_idle);
        free(prev_total);
        pthread_exit(NULL);
    }
    while(1)
    {
        // Pop from buffer
        sem_wait(&g_ra_sem_full);
        // Queue structure is thread safe
        if (queue_dequeue(g_reader_analyzer_queue, &data) != 0)
        {
            perror("Analyzer error while removing data from the buffer");
            free(prev_total);
            free(prev_idle);
            pthread_exit(NULL);
        }
        sem_post(&g_ra_sem_empty);

        // Consume / Analyze
        if (first_iter)
        {
            analyzer_update_prev(prev_total, prev_idle, data, g_no_cpus);
            first_iter = false;
        }
        else
        {
            double* to_print = malloc(sizeof(double)*(g_no_cpus+1));
            //Total
            to_print[0] =  analyzer_analyze(&prev_total[0], &prev_idle[0], data.total);

            // Cores
            for (size_t j = 0; j < g_no_cpus; ++j)
                to_print[j+1] =  analyzer_analyze(&prev_total[j+1], &prev_idle[j+1], data.cpus[j]);

            // Send to print
            sem_wait(&g_ap_sem_empty);
            if(queue_enqueue(g_analyzer_printer_queue, &to_print) != 0)
            {
                perror("Analyzer error while adding data to the buffer");
                free(prev_total);
                free(prev_idle);
                pthread_exit(NULL);
            }
            sem_post(&g_ap_sem_full);
        }
        free(data.cpus);
    }
}

/**
 * Printer thread function.
 * Responsible for displaying prepared data in the terminal.
 */
static void* printer_func(void* args) {
    system("clear");

    // printf("\t\t\033[3;33m*** CUT - CPU Usage Tracker ~ Sebastian Wozniak ***\033[0m\n");
    double *to_print;
    while(1){
        size_t i;
        // Remove from buffer
        sem_wait(&g_ap_sem_full);
        if (queue_dequeue(g_analyzer_printer_queue, &to_print) != 0) {
            perror("Printer error while removing data from the buffer");
            pthread_exit(NULL);
        }
        sem_post(&g_ap_sem_empty);

        // Print
        // system("tput cup 1 0");  - Better than clear but its buggy when terminal is too small
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
            printf("\033[0;%zumcpu%zu:\t ╠", 31 + ((j - 1) % 6), j - 1);
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
}

/**
 * Destroys all created semaphores.
 */
static void destroy_semaphores(void){
    sem_destroy(&g_ra_sem_empty);
    sem_destroy(&g_ra_sem_full);
    sem_destroy(&g_ap_sem_empty);
    sem_destroy(&g_ap_sem_full);
}

/**
 * Frees every element that is currently in the queue and destoroys queues.
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
}

static void thread_join_create_error(const char* msg){
    perror(msg);
    queues_cleanup();
    destroy_semaphores();
}



int main(void)
{
    // Reader - Analyzer semaphores
    if(sem_init(&g_ra_sem_empty, 0, 10) != 0)
    {
        perror("Reader - Analyzer semaphore init error");
        return -1;
    }
    if(sem_init(&g_ra_sem_full, 0, 0) != 0)
    {
        perror("Reader - Analyzer semaphore init error");
        sem_destroy(&g_ra_sem_empty);
        return -1;
    }

    // Analyzer - Printer semaphores
    if(sem_init(&g_ap_sem_empty, 0, 10) != 0)
    {
        perror("Analyzer - Printer semaphore init error");
        destroy_semaphores();
        return -1;
    }
    if(sem_init(&g_ap_sem_full, 0, 0) != 0)
    {
        perror("Analyzer - Printer semaphore init error");
        destroy_semaphores();
        return -1;
    }

    // Assign global variables
    g_no_cpus = reader_get_no_cpus();
    if(g_no_cpus == 0)
    {
        perror("Error while getting information about no cores");
        return -1;
    }
    g_reader_analyzer_queue = queue_create_new(10, sizeof(Stats)*(g_no_cpus+1));
    if(g_reader_analyzer_queue == NULL)
    {
        perror("Create new queue error");
        return -1;
    }
    g_analyzer_printer_queue = queue_create_new(10, sizeof(double)*(g_no_cpus+1));
    if(g_analyzer_printer_queue == NULL)
    {
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

    system("clean");
    printf("exit\n");

    queues_cleanup();
    destroy_semaphores();

    return 0;
}
