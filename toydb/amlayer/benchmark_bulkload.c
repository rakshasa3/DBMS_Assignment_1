#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"
#include "am.h"

#define MAX_ENTRIES 5000
#define KEY_SIZE 32

/* PF function declarations */
extern int PF_OpenFile(char *fileName);
extern int PF_CloseFile(int fileDesc);
extern int PF_GetFirstPage(int fileDesc, int *pageNum, char **pageBuf);
extern int PF_GetNextPage(int fileDesc, int *pageNum, char **pageBuf);
extern int PF_UnfixPage(int fileDesc, int pageNum, int dirty);

/* Helper to count total pages in index file */
static int count_index_pages(int fd) {
    int pageNum;
    char *buf;
    int count = 0;
    int err = PF_GetFirstPage(fd, &pageNum, &buf);
    while (err == PFE_OK) {
        count++;
        int prev = pageNum;
        err = PF_GetNextPage(fd, &pageNum, &buf);
        PF_UnfixPage(fd, prev, 0);
    }
    return count;
}

int main(void) {
    FILE *fp;
    char line[1024];
    BulkEntry entries[MAX_ENTRIES];
    int numEntries = 0;
    int i, fd;

    PF_Init();

    /* 1. Load data from courses.txt */
    fp = fopen("../../data/courses.txt", "r");
    if (!fp) fp = fopen("../data/courses.txt", "r");
    if (!fp) fp = fopen("courses.txt", "r");
    if (!fp) {
        printf("Error: Could not find courses.txt\n");
        return 1;
    }

    while (fgets(line, sizeof(line), fp) && numEntries < MAX_ENTRIES) {
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) {
            line[len-1] = '\0';
            len--;
        }
        if (len == 0) continue;

        memset(entries[numEntries].key, 0, sizeof(entries[numEntries].key));
        strncpy(entries[numEntries].key, line, KEY_SIZE - 1);
        entries[numEntries].pageNum = (numEntries / 20) + 1;
        entries[numEntries].slotNum = (numEntries % 20);
        numEntries++;
    }
    fclose(fp);

    printf("\n=========================================================================\n");
    printf("           B+ TREE BULK-LOADING VS INCREMENTAL BENCHMARK                 \n");
    printf("=========================================================================\n");
    printf("Total Keys Loaded: %d (Key Length: %d Bytes)\n\n", numEntries, KEY_SIZE);

    /* 2. Incremental Insertion */
    PF_ResetStats();

    AM_DestroyIndex("idx_inc", 1);
    if (AM_CreateIndex("idx_inc", 1, 'c', KEY_SIZE) != AME_OK) {
        printf("Error creating incremental index\n");
        return 1;
    }

    fd = PF_OpenFile("idx_inc.1");
    if (fd < 0) {
        printf("Error opening incremental index\n");
        return 1;
    }

    for (i = 0; i < numEntries; i++) {
        char val[KEY_SIZE];
        memset(val, 0, sizeof(val));
        strncpy(val, entries[i].key, KEY_SIZE - 1);
        AM_InsertEntry(fd, 'c', KEY_SIZE, val, (int)entries[i].pageNum);
    }

    int incPages = count_index_pages(fd);
    int incReads = (int)PFstats.physical_reads;
    int incWrites = (int)PFstats.physical_writes;
    PF_CloseFile(fd);

    /* 3. Bulk Load (100% Fill Factor) */
    PF_ResetStats();

    AM_DestroyIndex("idx_bulk100", 2);
    if (AM_CreateIndex("idx_bulk100", 2, 'c', KEY_SIZE) != AME_OK) {
        printf("Error creating bulk 100%% index\n");
        return 1;
    }

    fd = PF_OpenFile("idx_bulk100.2");
    if (fd < 0) {
        printf("Error opening bulk 100%% index\n");
        return 1;
    }

    AM_BulkLoad(fd, 'c', KEY_SIZE, entries, numEntries, 1.0f);
    int bulk100Pages = count_index_pages(fd);
    int bulk100Reads = (int)PFstats.physical_reads;
    int bulk100Writes = (int)PFstats.physical_writes;
    PF_CloseFile(fd);

    /* 4. Bulk Load (80% Fill Factor) */
    PF_ResetStats();

    AM_DestroyIndex("idx_bulk80", 3);
    if (AM_CreateIndex("idx_bulk80", 3, 'c', KEY_SIZE) != AME_OK) {
        printf("Error creating bulk 80%% index\n");
        return 1;
    }

    fd = PF_OpenFile("idx_bulk80.3");
    if (fd < 0) {
        printf("Error opening bulk 80%% index\n");
        return 1;
    }

    AM_BulkLoad(fd, 'c', KEY_SIZE, entries, numEntries, 0.8f);
    int bulk80Pages = count_index_pages(fd);
    int bulk80Reads = (int)PFstats.physical_reads;
    int bulk80Writes = (int)PFstats.physical_writes;
    PF_CloseFile(fd);

    /* 5. Output Summary Table */
    printf("+--------------------------------------+------------+------------+------------+\n");
    printf("| Construction Method                  | Total Pages| Disk Reads | Disk Writes|\n");
    printf("+--------------------------------------+------------+------------+------------+\n");
    printf("| Incremental Insertion (AM_Insert)    | %10d | %10d | %10d |\n", incPages, incReads, incWrites);
    printf("| Bottom-Up Bulk Load (FillFactor 1.0) | %10d | %10d | %10d |\n", bulk100Pages, bulk100Reads, bulk100Writes);
    printf("| Bottom-Up Bulk Load (FillFactor 0.8) | %10d | %10d | %10d |\n", bulk80Pages, bulk80Reads, bulk80Writes);
    printf("+--------------------------------------+------------+------------+------------+\n\n");

    AM_DestroyIndex("idx_inc", 1);
    AM_DestroyIndex("idx_bulk100", 2);
    AM_DestroyIndex("idx_bulk80", 3);

    return 0;
}