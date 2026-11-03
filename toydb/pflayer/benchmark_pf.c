#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pf.h"
#include "pftypes.h"

#define PAGE_COUNT 64
#define BUFFER_SIZE 8
#define OPERATIONS 2000

PF_IOStats stats;


/* ---------------------------------
   WORKLOAD FUNCTION
   --------------------------------- */

void run_workload(fd, write_percentage)
int fd;
int write_percentage;
{
    int i;
    int page_no;
    int operation;

    int original_value;

    char *pagebuf;

    for (i = 0; i < OPERATIONS; i++) {

        page_no = rand() % PAGE_COUNT;

        operation = rand() % 100;

        if (PF_GetThisPage(fd, page_no, &pagebuf) != PFE_OK) {

            PF_PrintError("Page read failed");
            exit(1);
        }

        if (operation < write_percentage) {

            /*
               Mark page dirty without permanently
               changing the original dataset.
            */

            memcpy(&original_value, pagebuf, sizeof(int));

            ((int *)pagebuf)[0]++;

            /* Restore original contents */
            memcpy(pagebuf, &original_value, sizeof(int));

            if (PF_UnfixPage(fd, page_no, TRUE) != PFE_OK) {

                PF_PrintError("Page write failed");
                exit(1);
            }

        }
        else {

            if (PF_UnfixPage(fd, page_no, FALSE) != PFE_OK) {

                PF_PrintError("Page unfix failed");
                exit(1);
            }
        }
    }
}


/* ---------------------------------
   MAIN FUNCTION
   --------------------------------- */

int main()
{
    int policies[2] = {PF_LRU, PF_MRU};

    int percentages[5] = {0, 25, 50, 75, 100};

    int i, j;

    int fd;

    char *policy_name;

    PF_Init();

    /*
       Configure buffer pool before opening files.
    */

    if (PF_SetBufferSize(BUFFER_SIZE) != PFE_OK) {

        PF_PrintError("Buffer configuration failed");
        return 1;
    }

    printf("Database: courses.db\n");
    printf("Buffer pool size: %d\n", BUFFER_SIZE);
    printf("Page count: %d\n", PAGE_COUNT);
    printf("Operations: %d\n\n", OPERATIONS);

    printf("Policy,WritePercentage,"
           "LogicalReads,LogicalWrites,"
           "PhysicalReads,PhysicalWrites,"
           "CacheHits,CacheMisses\n");

    for (i = 0; i < 2; i++) {

        for (j = 0; j < 5; j++) {

            /*
               Select correct replacement policy.
            */

            if (policies[i] == PF_LRU)
                policy_name = "LRU";
            else
                policy_name = "MRU";

            printf("Running %s with %d%% writes...\n",
                   policy_name, percentages[j]);

            /*
               Open file using the correct policy.
            */

            fd = PF_OpenFileWithPolicy(
                "courses.db",
                policies[i]
            );

            if (fd < 0) {

                PF_PrintError("Opening benchmark file");
                return 1;
            }

            /*
               Reset statistics before workload.
            */

            PF_ResetStats();

            /*
               Same random sequence for fair comparison.
            */

            srand(12345);

            /*
               Execute workload.
            */

            run_workload(fd, percentages[j]);

            /*
               Close file to flush dirty pages.
            */

            if (PF_CloseFile(fd) != PFE_OK) {

                PF_PrintError("Closing benchmark file");
                return 1;
            }

            /*
               Collect statistics after closing.
            */

            PF_GetStats(&stats);

            printf("%s,%d,%lu,%lu,%lu,%lu,%lu,%lu\n",

                policy_name,

                percentages[j],

                stats.logical_reads,
                stats.logical_writes,

                stats.physical_reads,
                stats.physical_writes,

                stats.cache_hits,
                stats.cache_misses
            );

            fflush(stdout);
        }
    }

    printf("\nBenchmark completed successfully.\n");

    return 0;
}
