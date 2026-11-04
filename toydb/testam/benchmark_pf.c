#include <stdio.h>
#include <stdlib.h>

#include "course_store.h"

/* PF functions used by this benchmark. */
extern int PF_OpenFile(char *fileName);
extern int PF_CloseFile(int fileDesc);
extern int PF_GetThisPage(int fileDesc, int pageNum, char **pageBuf);
extern int PF_UnfixPage(int fileDesc, int pageNum, int dirty);
extern int PF_SetBufferSize(int numberOfFrames);
extern int PF_SetReplacementPolicy(int policy);
extern void PF_GetStats(PF_IOStats *stats);

#define TOTAL_RECORDS 4268
#define OPERATIONS 20000

/*
 * A fixed seed makes the access sequence repeatable.
 * The same sequence is used for LRU and MRU.
 */
static unsigned int random_state = 123456789;

static unsigned int next_random(void)
{
    random_state = random_state * 1103515245u + 12345u;
    return (random_state / 65536u) % 32768u;
}

static int run_benchmark(int policy,
                         int buffer_size,
                         int write_percentage,
                         int page_count)
{
    int fd;
    int i;
    int page_num;
    int dirty;
    int status;
    char *page_buf;
    unsigned char saved_byte;
    PF_IOStats stats;

    /* Configure the buffer manager before opening the data file. */
    status = PF_SetBufferSize(buffer_size);
    if (status != PFE_OK) {
        fprintf(stderr, "Could not set buffer size to %d\n", buffer_size);
        return -1;
    }

    status = PF_SetReplacementPolicy(policy);
    if (status != PFE_OK) {
        fprintf(stderr, "Could not set replacement policy\n");
        return -1;
    }

    fd = PF_OpenFile("courses.data");
    if (fd < 0) {
        fprintf(stderr, "Could not open courses.data\n");
        return -1;
    }

    /* Exclude file-open activity from the measured workload. */
    PF_ResetStats();

    random_state = 123456789u;

    for (i = 0; i < OPERATIONS; i++) {
        page_num = (int)(next_random() % (unsigned int)page_count);

        status = PF_GetThisPage(fd, page_num, &page_buf);
        if (status != PFE_OK) {
            fprintf(stderr, "PF_GetThisPage failed for page %d\n",
                    page_num);
            PF_CloseFile(fd);
            return -1;
        }

        dirty = ((int)(next_random() % 100u) < write_percentage);

        if (dirty) {
            /*
             * Mark the page dirty without changing its contents.
             * This exercises dirty-page write-back without changing
             * the course records stored in the file.
             */
            saved_byte = (unsigned char)page_buf[0];
            page_buf[0] = (char)(saved_byte ^ 0x01);
            page_buf[0] = (char)saved_byte;
        }

        status = PF_UnfixPage(fd, page_num, dirty);
        if (status != PFE_OK) {
            fprintf(stderr, "PF_UnfixPage failed for page %d\n",
                    page_num);
            PF_CloseFile(fd);
            return -1;
        }
    }

    /*
     * Capture workload statistics before closing the file.
     * Closing may flush dirty pages and add physical writes.
     */
    PF_GetStats(&stats);

    printf("%s,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu\n",
           (policy == PF_LRU) ? "LRU" : "MRU",
           buffer_size,
           write_percentage,
           stats.logical_reads,
           stats.logical_writes,
           stats.physical_reads,
           stats.physical_writes,
           stats.cache_hits,
           stats.cache_misses);

    status = PF_CloseFile(fd);
    if (status != PFE_OK) {
        fprintf(stderr, "PF_CloseFile failed\n");
        return -1;
    }

    return 0;
}

int main(void)
{
    int page_count;
    int policies[2];
    int buffer_sizes[3];
    int write_percentages[5];
    int p;
    int b;
    int w;

    PF_Init();

    page_count =
        (TOTAL_RECORDS + COURSE_RECORDS_PER_PAGE - 1) /
        COURSE_RECORDS_PER_PAGE;

    policies[0] = PF_LRU;
    policies[1] = PF_MRU;

    buffer_sizes[0] = 5;
    buffer_sizes[1] = 10;
    buffer_sizes[2] = 20;

    write_percentages[0] = 0;
    write_percentages[1] = 25;
    write_percentages[2] = 50;
    write_percentages[3] = 75;
    write_percentages[4] = 100;

    printf("Policy,BufferSize,WritePercentage,"
           "LogicalReads,LogicalWrites,"
           "PhysicalReads,PhysicalWrites,"
           "CacheHits,CacheMisses\n");

    for (p = 0; p < 2; p++) {
        for (b = 0; b < 3; b++) {
            for (w = 0; w < 5; w++) {
                if (run_benchmark(policies[p],
                                  buffer_sizes[b],
                                  write_percentages[w],
                                  page_count) != 0) {
                    return 1;
                }
            }
        }
    }

    return 0;
}
