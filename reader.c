#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"

#include "reader.h"

/**
 * Counts cpus in /proc/stat.
 * @return Number of cores. 0 if an error occured
 */
size_t reader_get_no_cpus(void)
{
    bool was_error = true;
    size_t buff_size = 1024;
    char* buffer = malloc(buff_size);
    if(buffer == NULL)
    {
        perror("reader_get_no_cpus - buffer allocation error\n");
        return 0;
    }
    while (was_error)
    {
        FILE* file = fopen("/proc/stat", "r");
        if(file == NULL)
            continue;
        fread(buffer, sizeof(char), buff_size, file);
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
                return 0;
            }
        }
        fclose(file);
    }
    // Read from buffer - Count the occurence of word cpu ( should be no_cores + 1)
    bool found;
    size_t cpus = 0;
    const char word[3] = "cpu";
    const size_t word_len = 3;
    for(size_t i = 0; i <= strlen(buffer)- word_len; i++)
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
