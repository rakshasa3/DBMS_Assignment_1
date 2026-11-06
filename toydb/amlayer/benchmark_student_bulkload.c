#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pf.h"
#include "am.h"

#define STUDENT_FILE_1 "../../data/student.txt"
#define STUDENT_FILE_2 "../data/student.txt"
#define STUDENT_FILE_3 "student.txt"

#define KEY_SIZE 10
#define MAX_LINE 2048

#define MAX_ENTRIES 30000

/* PF function declarations */
extern int PF_OpenFile(char *fileName);
extern int PF_CloseFile(int fileDesc);
extern int PF_GetFirstPage(int fileDesc, int *pageNum, char **pageBuf);
extern int PF_GetNextPage(int fileDesc, int *pageNum, char **pageBuf);
extern int PF_UnfixPage(int fileDesc, int pageNum, int dirty);


/* Statistics for each experiment */
typedef struct {
    int pages;
    unsigned long logicalReads;
    unsigned long logicalWrites;
    unsigned long physicalReads;
    unsigned long physicalWrites;
    unsigned long cacheHits;
    unsigned long cacheMisses;
} BenchmarkStats;


static void save_stats(BenchmarkStats *result, int fd)
{
    int pageCount = 0;
    int pageNum;
    int previousPage;
    char *pageBuf;

    int error;

    /*
     * Count index pages.
     */
    error = PF_GetFirstPage(fd, &pageNum, &pageBuf);

    while (error == PFE_OK) {

        pageCount++;

        previousPage = pageNum;

        error = PF_GetNextPage(fd, &pageNum, &pageBuf);

        PF_UnfixPage(fd, previousPage, FALSE);
    }

    result->pages = pageCount;

    /*
     * Capture PF statistics.
     */
    result->logicalReads = PFstats.logical_reads;
    result->logicalWrites = PFstats.logical_writes;

    result->physicalReads = PFstats.physical_reads;
    result->physicalWrites = PFstats.physical_writes;

    result->cacheHits = PFstats.cache_hits;
    result->cacheMisses = PFstats.cache_misses;
}


/*
 * Find student.txt regardless of whether the executable
 * is launched from toydb or toydb/amlayer.
 */
static FILE *open_student_file(void)
{
    FILE *fp;

    fp = fopen(STUDENT_FILE_1, "r");

    if (fp != NULL)
        return fp;

    fp = fopen(STUDENT_FILE_2, "r");

    if (fp != NULL)
        return fp;

    fp = fopen(STUDENT_FILE_3, "r");

    return fp;
}


/*
 * Extract roll number from the first semicolon-separated field.
 *
 * Store it as a zero-padded 10-character string.
 * Example:
 *
 * 949981 -> 0000949981
 *
 * This allows the existing character-key comparator
 * to preserve numeric ordering.
 */
static int extract_roll_number(const char *line, char *key)
{
    char field[32];
    const char *separator;

    size_t length;
    char *end;

    long rollNo;

    separator = strchr(line, ';');

    if (separator == NULL)
        return -1;

    length = (size_t)(separator - line);

    if (length == 0 || length >= sizeof(field))
        return -1;

    memcpy(field, line, length);
    field[length] = '\0';

    errno = 0;

    rollNo = strtol(field, &end, 10);

    if (errno != 0 || *end != '\0' ||
        rollNo < 0 || rollNo > 9999999999L)
        return -1;

    sprintf(key, "%010ld", rollNo);

    return 0;
}


/*
 * Load all student roll numbers into memory.
 */
static int load_student_entries(BulkEntry **entriesOut)
{
    FILE *fp;

    char line[MAX_LINE];
    char key[KEY_SIZE + 1];

    BulkEntry *entries;

    int count = 0;
    int lineNumber = 0;

    fp = open_student_file();

    if (fp == NULL) {
        printf("ERROR: Could not find student.txt\n");
        printf("Expected location: ../data/student.txt relative to toydb\n");
        return -1;
    }

    entries = (BulkEntry *)calloc(
        MAX_ENTRIES,
        sizeof(BulkEntry)
    );

    if (entries == NULL) {
        printf("Memory allocation failed.\n");
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {

        lineNumber++;

        /*
         * Remove newline characters.
         */
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0')
            continue;

        /*
         * Skip header.
         */
        if (strncmp(line, "Database dummy", 14) == 0)
            continue;

        if (count >= MAX_ENTRIES) {
            printf("Maximum student entry limit exceeded.\n");
            free(entries);
            fclose(fp);
            return -1;
        }

        /*
         * Extract roll number.
         */
        if (extract_roll_number(line, key) != 0) {
            printf("Invalid roll number at line %d: %s\n",
                   lineNumber, line);

            free(entries);
            fclose(fp);
            return -1;
        }

        memset(entries[count].key, 0, sizeof(entries[count].key));

        memcpy(entries[count].key, key, KEY_SIZE);

        /*
         * Synthetic record reference, following the structure
         * of the original benchmark.
         *
         * pageNum + slotNum uniquely identify an input record.
         */
        entries[count].pageNum = (count / 20) + 1;
        entries[count].slotNum = count % 20;

        count++;
    }

    fclose(fp);

    *entriesOut = entries;

    return count;
}


/*
 * Print results.
 */
static void print_result(const char *method,
                         BenchmarkStats *stats)
{
    printf("| %-34s | %8d | %10lu | %11lu | %12lu | %13lu |\n",
           method,
           stats->pages,
           stats->logicalReads,
           stats->logicalWrites,
           stats->physicalReads,
           stats->physicalWrites);
}


int main(void)
{
    BulkEntry *entries = NULL;

    int numEntries;

    int fd;
    int error;
    int i;

    BenchmarkStats incremental;
    BenchmarkStats bulk100;
    BenchmarkStats bulk80;

    PF_Init();

    /*
     * 1. Read Student dataset.
     */
    numEntries = load_student_entries(&entries);

    if (numEntries <= 0) {
        printf("No student records loaded.\n");
        return 1;
    }

    printf("\n");
    printf("============================================================\n");
    printf("       STUDENT B+ TREE BULK LOADING BENCHMARK\n");
    printf("============================================================\n");

    printf("Dataset       : student.txt\n");
    printf("Key field     : roll_no\n");
    printf("Key type      : Character (zero-padded numeric string)\n");
    printf("Key length    : %d bytes\n", KEY_SIZE);
    printf("Total records : %d\n", numEntries);

    /*
     * 2. Incremental insertion.
     */
    printf("\nRunning incremental insertion...\n");

    PF_ResetStats();

    AM_DestroyIndex("idx_student_inc", 1);

    error = AM_CreateIndex(
        "idx_student_inc",
        1,
        'c',
        KEY_SIZE
    );

    if (error != AME_OK) {
        printf("Could not create incremental index: %d\n", error);
        free(entries);
        return 1;
    }

    fd = PF_OpenFile("idx_student_inc.1");

    if (fd < 0) {
        printf("Could not open incremental index.\n");
        free(entries);
        return 1;
    }

    for (i = 0; i < numEntries; i++) {

        char key[KEY_SIZE + 1];

        memset(key, 0, sizeof(key));

        memcpy(key, entries[i].key, KEY_SIZE);

        error = AM_InsertEntry(
            fd,
            'c',
            KEY_SIZE,
            key,
            i + 1
        );

        if (error != AME_OK) {
            printf("Incremental insertion failed at record %d: %d\n",
                   i, error);

            PF_CloseFile(fd);
            free(entries);
            return 1;
        }
    }

    save_stats(&incremental, fd);

    PF_CloseFile(fd);


    /*
     * 3. Bulk loading with 100% fill factor.
     */
    printf("Running bulk loading (100%% fill factor)...\n");

    PF_ResetStats();

    AM_DestroyIndex("idx_student_bulk100", 2);

    error = AM_CreateIndex(
        "idx_student_bulk100",
        2,
        'c',
        KEY_SIZE
    );

    if (error != AME_OK) {
        printf("Could not create bulk 100 index: %d\n", error);
        free(entries);
        return 1;
    }

    fd = PF_OpenFile("idx_student_bulk100.2");

    if (fd < 0) {
        printf("Could not open bulk 100 index.\n");
        free(entries);
        return 1;
    }

    error = AM_BulkLoad(
        fd,
        'c',
        KEY_SIZE,
        entries,
        numEntries,
        1.0f
    );

    if (error != AME_OK) {
        printf("Bulk loading 100%% failed: %d\n", error);
        PF_CloseFile(fd);
        free(entries);
        return 1;
    }

    save_stats(&bulk100, fd);

    PF_CloseFile(fd);


    /*
     * 4. Bulk loading with 80% fill factor.
     */
    printf("Running bulk loading (80%% fill factor)...\n");

    PF_ResetStats();

    AM_DestroyIndex("idx_student_bulk80", 3);

    error = AM_CreateIndex(
        "idx_student_bulk80",
        3,
        'c',
        KEY_SIZE
    );

    if (error != AME_OK) {
        printf("Could not create bulk 80 index: %d\n", error);
        free(entries);
        return 1;
    }

    fd = PF_OpenFile("idx_student_bulk80.3");

    if (fd < 0) {
        printf("Could not open bulk 80 index.\n");
        free(entries);
        return 1;
    }

    error = AM_BulkLoad(
        fd,
        'c',
        KEY_SIZE,
        entries,
        numEntries,
        0.8f
    );

    if (error != AME_OK) {
        printf("Bulk loading 80%% failed: %d\n", error);
        PF_CloseFile(fd);
        free(entries);
        return 1;
    }

    save_stats(&bulk80, fd);

    PF_CloseFile(fd);


    /*
     * 5. Print benchmark results.
     */
    printf("\n");
    printf("================================================================================================================\n");
    printf("                                      STUDENT INDEX RESULTS\n");
    printf("================================================================================================================\n");

    printf("+------------------------------------+----------+------------+-------------+--------------+---------------+\n");

    printf("| Construction Method                |   Pages  | Logical R  | Logical W   | Physical R   | Physical W    |\n");

    printf("+------------------------------------+----------+------------+-------------+--------------+---------------+\n");

    print_result("Incremental insertion", &incremental);

    print_result("Bulk loading (100% fill)", &bulk100);

    print_result("Bulk loading (80% fill)", &bulk80);

    printf("+------------------------------------+----------+------------+-------------+--------------+---------------+\n");


    /*
     * 6. Cache statistics.
     */
    printf("\nCACHE STATISTICS\n");

    printf("+------------------------------------+------------+-------------+\n");
    printf("| Construction Method                | Cache Hits | Cache Misses|\n");
    printf("+------------------------------------+------------+-------------+\n");

    printf("| Incremental insertion              | %10lu | %11lu |\n",
           incremental.cacheHits,
           incremental.cacheMisses);

    printf("| Bulk loading (100%% fill)           | %10lu | %11lu |\n",
           bulk100.cacheHits,
           bulk100.cacheMisses);

    printf("| Bulk loading (80%% fill)            | %10lu | %11lu |\n",
           bulk80.cacheHits,
           bulk80.cacheMisses);

    printf("+------------------------------------+------------+-------------+\n");


    /*
     * 7. Clean up.
     */
    free(entries);

    AM_DestroyIndex("idx_student_inc", 1);
    AM_DestroyIndex("idx_student_bulk100", 2);
    AM_DestroyIndex("idx_student_bulk80", 3);

    printf("\nStudent benchmark completed.\n");

    return 0;
}
