#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pf.h"
#include "pftypes.h"

#define OPERATIONS_PER_PASS 5000
#define WORKLOAD_PASSES 3

static PF_IOStats stats;

/*
 * Run one workload pass.
 * Each operation selects a random page and performs a read or write.
 * For writes, the first integer is changed and then restored. This marks
 * the page dirty so the buffer manager must eventually write it, while
 * keeping the file's original contents unchanged.
 */
static int run_workload(int fd, int page_count, int write_percentage)
{
    int i;
    int page_no;
    int operation;
    int original_value;
    char *pagebuf;

    for (i = 0; i < OPERATIONS_PER_PASS; i++) {
        page_no = rand() % page_count;
        operation = rand() % 100;

        if (PF_GetThisPage(fd, page_no, &pagebuf) != PFE_OK) {
            PF_PrintError("PF_GetThisPage failed");
            return -1;
        }

        if (operation < write_percentage) {
            memcpy(&original_value, pagebuf, sizeof(int));

            ((int *)pagebuf)[0]++;

            memcpy(pagebuf, &original_value, sizeof(int));

            if (PF_UnfixPage(fd, page_no, TRUE) != PFE_OK) {
                PF_PrintError("PF_UnfixPage failed after write");
                return -1;
            }
        } else {
            if (PF_UnfixPage(fd, page_no, FALSE) != PFE_OK) {
                PF_PrintError("PF_UnfixPage failed after read");
                return -1;
            }
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int buffer_size;
    int page_count;
    int policies[2];
    int percentages[5];
    int i, j, pass;
    int fd;
    int close_result;
    const char *filename;
    const char *policy_name;

    if (argc < 3 || argc > 4) {
        fprintf(stderr,
                "Usage: %s <buffer_size> <page_count> [filename]\n",
                argv[0]);
        fprintf(stderr,
                "Example: %s 8 64 courses.data\n",
                argv[0]);
        return 1;
    }

    buffer_size = atoi(argv[1]);
    page_count = atoi(argv[2]);
    filename = (argc == 4) ? argv[3] : "courses.data";

    if (buffer_size <= 0 || page_count <= 0) {
        fprintf(stderr, "Error: buffer_size and page_count must be positive.\n");
        return 1;
    }

    policies[0] = PF_LRU;
    policies[1] = PF_MRU;

    percentages[0] = 0;
    percentages[1] = 25;
    percentages[2] = 50;
    percentages[3] = 75;
    percentages[4] = 100;

    PF_Init();

    /*
     * Configure the pool before opening the file or allocating buffer
     * frames. PF_SetBufferSize must be called before frames are allocated.
     */
    if (PF_SetBufferSize(buffer_size) != PFE_OK) {
        PF_PrintError("PF_SetBufferSize failed");
        return 1;
    }

    fprintf(stderr, "Benchmark file: %s\n", filename);
    fprintf(stderr, "Buffer pool size: %d\n", buffer_size);
    fprintf(stderr, "Pages accessed: %d\n", page_count);
    fprintf(stderr, "Operations per pass: %d\n", OPERATIONS_PER_PASS);
    fprintf(stderr, "Workload passes: %d\n\n", WORKLOAD_PASSES);

    /*
     * CSV header. Keep benchmark progress messages on stderr so stdout
     * can be redirected directly to a CSV file.
     */
    printf("BufferSize,Policy,WritePercentage,Passes,"
           "LogicalReads,LogicalWrites,PhysicalReads,PhysicalWrites,"
           "CacheHits,CacheMisses\n");

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 5; j++) {

            policy_name = (policies[i] == PF_LRU) ? "LRU" : "MRU";

            fprintf(stderr, "Running %s with %d%% writes...\n",
                    policy_name, percentages[j]);

            fd = PF_OpenFileWithPolicy((char *)filename, policies[i]);

            if (fd < 0) {
                PF_PrintError("PF_OpenFileWithPolicy failed");
                return 1;
            }

            PF_ResetStats();

            /*
             * Reset the random seed for every configuration so LRU and
             * MRU receive the same sequence of page accesses and operation
             * choices for a given write percentage.
             */
            srand(12345);

            for (pass = 0; pass < WORKLOAD_PASSES; pass++) {
                if (run_workload(fd, page_count, percentages[j]) != 0) {
                    PF_CloseFile(fd);
                    return 1;
                }
            }

            /*
             * Close before reading the statistics so that writes caused
             * by flushing dirty pages during close are included.
             */
            close_result = PF_CloseFile(fd);

            if (close_result != PFE_OK) {
                PF_PrintError("PF_CloseFile failed");
                return 1;
            }

            PF_GetStats(&stats);

            printf("%d,%s,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu\n",
                   buffer_size,
                   policy_name,
                   percentages[j],
                   WORKLOAD_PASSES,
                   stats.logical_reads,
                   stats.logical_writes,
                   stats.physical_reads,
                   stats.physical_writes,
                   stats.cache_hits,
                   stats.cache_misses);
        }
    }

    fprintf(stderr, "\nBenchmark completed successfully.\n");

    return 0;
}
