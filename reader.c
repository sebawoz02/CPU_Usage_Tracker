#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"

#include "reader.h"


static char* reader_load_to_buffer(void);

/**
 * Puts data from /proc/stat into buffer.
 * @return buffer with data from /proc/stat. NULL if allocation error occurred
 */
static char* reader_load_to_buffer(void)
{
    bool was_error = true;
    size_t buff_size = 1024;
    size_t bytes_read = 0;
    char* buffer = malloc(buff_size);
    if(buffer == NULL)
    {
        perror("reader_get_no_cpus - buffer allocation error\n");
        return NULL;
    }
    while (was_error)
    {
        FILE* file = fopen("/proc/stat", "r");
        if(file == NULL)
            continue;
        bytes_read = fread(buffer, sizeof(char), buff_size, file);
        was_error = ferror(file) || !feof(file);

        // End of the file not reached or stream error
        if(was_error)
        {
            buff_size *= 2;
            free(buffer);
            buffer = malloc(buff_size);
            if(buffer == NULL)
            {
                perror("reader_get_no_cpus - buffer allocation error\n");
                fclose(file);
                return NULL;
            }
        }
        fclose(file);
    }
    buffer[bytes_read] = '\0';  // Add null character to mark the end of buffer data
    return buffer;
}


/**
 * Counts cpus in /proc/stat.
 * @return Number of cores. 0 if an error occured
 */
size_t reader_get_no_cpus(void)
{
    char* buffer = reader_load_to_buffer();
    if(buffer == NULL){
        return 0;
    }
    // Read from buffer - Count the occurence of word cpu ( should be no_cores + 1)
    bool found;
    size_t cpus = 0;
    const char word[3] = "cpu";
    const size_t word_len = 3;
    const size_t buf_len = strlen(buffer);
    for(size_t i = 0; i <= buf_len - word_len; i++)
    {
        found = true;
        for (size_t j = 0; j < word_len; ++j) {
            if(buffer[i+j] != word[j])
            {
                found = false;
                break;
            }
        }
        if(found == 1)
            cpus++;
    }

    free(buffer);
    return cpus - 1;
}

/**
 * Reads data from /proc/stat and stores it in a structure.
 * @param no_cpus - num of cpus to load
 * @return Pointer to the CPURawStats structure with loaded data of main cpu and no_cpus cores.
 */
CPURawStats reader_load_data(size_t const no_cpus)
{
    CPURawStats data;
    data.cpus = malloc(sizeof(Stats)*no_cpus);

    char* buffer = reader_load_to_buffer();
    // divide into lines
    char* line = strtok(buffer, "\n");
    size_t cpu_num = 0;
    int tmp;
    while (line != NULL)
    {
        if(strstr(line, "cpu")!=NULL) {
            if(cpu_num == 0)
            {
                sscanf(line, "cpu %d %d %d %d %d %d %d %d", &(data.total.user), &(data.total.nice),
                       &(data.total.system), &(data.total.idle), &(data.total.iowait),
                       &(data.total.irq), &(data.total.sortirq), &(data.total.steal));
            }
            else{
                sscanf(line, "cpu%d %d %d %d %d %d %d %d %d", &tmp, &(data.cpus[cpu_num - 1].user), &(data.cpus[cpu_num - 1].nice),
                       &(data.cpus[cpu_num - 1].system), &(data.cpus[cpu_num - 1].idle), &(data.cpus[cpu_num - 1].iowait),
                       &(data.cpus[cpu_num - 1].irq), &(data.cpus[cpu_num - 1].sortirq), &(data.cpus[cpu_num - 1].steal));
            }
            if (cpu_num == no_cpus) break;
            cpu_num++;
        } else break;
        line = strtok(NULL, "\n");
    }
    free(buffer);
    return data;
}
