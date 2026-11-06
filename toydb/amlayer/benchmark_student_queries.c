/*
 * benchmark_student_queries.c
 *
 * Measures Student roll_no equality-query performance on:
 *   1. Incremental index
 *   2. Bulk-loaded index (100% fill)
 *   3. Bulk-loaded index (80% fill)
 *   4. Post-population index
 *
 * Metrics:
 *   - Total query execution time
 *   - Total logical page accesses
 *   - Average logical page accesses per query
 *   - Successful and failed lookups
 *
 * Run after building all four indexes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "pf.h"
#include "am.h"

#define MAX_LINE 2048
#define MAX_STUDENTS 30000
#define KEY_SIZE 10

typedef struct {
    int roll_no;
    char char_key[KEY_SIZE];
} StudentKey;

typedef struct {
    const char *method;
    const char *filename;
    char attrType;
    int attrLength;
} IndexConfig;

typedef struct {
    double elapsed_seconds;
    unsigned long total_page_accesses;
    unsigned long cache_hits;
    unsigned long cache_misses;
    int successful_queries;
    int failed_queries;
    int query_errors;
} QueryResult;


/* ---------------------------------------------------------
   Locate the Student dataset.
   --------------------------------------------------------- */

static FILE *open_student_file(const char *path)
{
    FILE *fp;

    if (path != NULL) {
        fp = fopen(path, "r");
        if (fp != NULL)
            return fp;
    }

    fp = fopen("../../data/student.txt", "r");

    if (fp != NULL)
        return fp;

    fp = fopen("../data/student.txt", "r");

    if (fp != NULL)
        return fp;

    fp = fopen("student.txt", "r");

    return fp;
}


/* ---------------------------------------------------------
   Load roll numbers from student.txt.
   --------------------------------------------------------- */

static int load_student_keys(
    const char *path,
    StudentKey **keys_out
)
{
    FILE *fp;
    char line[MAX_LINE];
    char field[64];
    char *separator;
    char *end;
    long roll;
    int count = 0;

    StudentKey *keys;

    fp = open_student_file(path);

    if (fp == NULL) {
        perror("Could not open student.txt");
        return -1;
    }

    keys = (StudentKey *)calloc(
        MAX_STUDENTS,
        sizeof(StudentKey)
    );

    if (keys == NULL) {
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {

        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0')
            continue;

        separator = strchr(line, ';');

        if (separator == NULL)
            continue;

        if ((size_t)(separator - line) >= sizeof(field)) {
            printf("Invalid roll number field.\n");
            free(keys);
            fclose(fp);
            return -1;
        }

        memcpy(field, line, (size_t)(separator - line));
        field[separator - line] = '\0';

        errno = 0;
        roll = strtol(field, &end, 10);

        /* Skip header or malformed rows. */
        if (errno != 0 || end == field || *end != '\0')
            continue;

        if (roll < 0 || roll > 2147483647L) {
            printf("Roll number outside supported integer range.\n");
            free(keys);
            fclose(fp);
            return -1;
        }

        if (count >= MAX_STUDENTS) {
            printf("Maximum student count exceeded.\n");
            free(keys);
            fclose(fp);
            return -1;
        }

        keys[count].roll_no = (int)roll;

        snprintf(
            keys[count].char_key,
            sizeof(keys[count].char_key),
            "%010d",
            keys[count].roll_no
        );

        count++;
    }

    fclose(fp);

    *keys_out = keys;

    return count;
}


/* ---------------------------------------------------------
   Run equality queries on one index.
   --------------------------------------------------------- */

static int run_queries(
    IndexConfig *config,
    StudentKey *keys,
    int num_keys,
    QueryResult *result
)
{
    int fd;
    int i;
    int scan_desc;
    int record_id;
    int error;

    clock_t start;
    clock_t end;

    PF_IOStats stats;

    memset(result, 0, sizeof(QueryResult));

    fd = PF_OpenFile((char *)config->filename);

    if (fd < 0) {
        printf(
            "ERROR: Could not open index file %s (error %d)\n",
            config->filename,
            fd
        );
        return -1;
    }

    /*
     * Exclude index opening from query statistics.
     */
    PF_ResetStats();

    start = clock();

    for (i = 0; i < 100; i++) {

        char *key;

        if (config->attrType == 'i')
            key = (char *)&keys[i].roll_no;
        else
            key = keys[i].char_key;

        /*
         * Open an equality scan for the current roll number.
         */
        scan_desc = AM_OpenIndexScan(
            fd,
            config->attrType,
            config->attrLength,
            EQUAL,
            key
        );

        if (scan_desc < 0) {
            result->query_errors++;
            continue;
        }

        /*
         * Retrieve the matching record reference.
         */
        record_id = AM_FindNextEntry(scan_desc);

        if (record_id == AME_EOF) {
            result->failed_queries++;
        }
        else if (record_id < 0) {
            result->query_errors++;
        }
        else {
            result->successful_queries++;
        }

        error = AM_CloseIndexScan(scan_desc);

        if (error != AME_OK)
            result->query_errors++;
    }

    end = clock();

    /*
     * Capture statistics before closing the index.
     */
    PF_GetStats(&stats);

    result->elapsed_seconds =
        (double)(end - start) / CLOCKS_PER_SEC;

    /*
     * Every PF_GetThisPage request is counted as
     * one logical page access.
     */
    result->total_page_accesses = stats.logical_reads;

    result->cache_hits = stats.cache_hits;
    result->cache_misses = stats.cache_misses;

    PF_CloseFile(fd);

    return 0;
}


/* ---------------------------------------------------------
   Print one benchmark result.
   --------------------------------------------------------- */

static void print_result(
    IndexConfig *config,
    QueryResult *result,
    int num_queries
)
{
    double average_accesses = 0.0;

    if (num_queries > 0) {
        average_accesses =
            (double)result->total_page_accesses / num_queries;
    }

    printf("\nMethod: %s\n", config->method);

    printf("Index file                 : %s\n",
           config->filename);

    printf("Queries executed           : %d\n",
           num_queries);

    printf("Successful queries         : %d\n",
           result->successful_queries);

    printf("Failed queries             : %d\n",
           result->failed_queries);

    printf("Query errors               : %d\n",
           result->query_errors);

    printf("Total query time           : %.6f seconds\n",
           result->elapsed_seconds);

    printf("Total logical page accesses: %lu\n",
           result->total_page_accesses);

    printf("Average page accesses/query: %.4f\n",
           average_accesses);

    printf("Cache hits                 : %lu\n",
           result->cache_hits);

    printf("Cache misses               : %lu\n",
           result->cache_misses);
}


/* ---------------------------------------------------------
   Main program.
   --------------------------------------------------------- */

int main(int argc, char *argv[])
{
    StudentKey *keys = NULL;
    int num_students;

    int i;

    QueryResult results[4];
    memset(results, 0, sizeof(results));

    IndexConfig indexes[4] = {

        {
            "Incremental insertion",
            "idx_student_inc.1",
            'c',
            KEY_SIZE
        },

        {
            "Bulk loading (100% fill)",
            "idx_student_bulk100.2",
            'c',
            KEY_SIZE
        },

        {
            "Bulk loading (80% fill)",
            "idx_student_bulk80.3",
            'c',
            KEY_SIZE
        },

        {
            "Post-population index construction",
            "student.0",
            'i',
            sizeof(int)
        }
    };

    const char *student_path = NULL;

    if (argc > 2) {
        printf("Usage: %s [path/to/student.txt]\n", argv[0]);
        return 1;
    }

    if (argc == 2)
        student_path = argv[1];

    PF_Init();

    num_students = load_student_keys(
        student_path,
        &keys
    );

    if (num_students <= 0) {
        printf("Could not load Student records.\n");
        return 1;
    }

    printf("\n");
    printf("============================================================\n");
    printf("       STUDENT B+ TREE QUERY PERFORMANCE BENCHMARK\n");
    printf("============================================================\n");

    printf("Dataset             : student.txt\n");
    printf("Number of queries   : %d\n", num_students);
    printf("Query type          : Equality search on roll_no\n");

    printf("\nRunning queries on all indexes...\n");

    for (i = 0; i < 4; i++) {

        printf("\nTesting: %s\n", indexes[i].method);

        if (run_queries(
                &indexes[i],
                keys,
                num_students,
                &results[i]
            ) != 0) {

            printf(
                "Skipping %s because its index could not be opened.\n",
                indexes[i].method
            );

            continue;
        }

        print_result(
            &indexes[i],
            &results[i],
            num_students
        );
    }

    printf("\n");
    printf("============================================================\n");
    printf("                 COMPARISON SUMMARY\n");
    printf("============================================================\n");

    printf(
        "%-38s | %12s | %14s | %14s\n",
        "Construction method",
        "Time (sec)",
        "Total pages",
        "Avg/query"
    );

    printf(
        "--------------------------------------------------------------------------------------\n"
    );

    for (i = 0; i < 4; i++) {

        if (results[i].successful_queries == 0 &&
            results[i].failed_queries == 0 &&
            results[i].query_errors == 0) {
            continue;
        }

        printf(
            "%-38s | %12.6f | %14lu | %14.4f\n",
            indexes[i].method,
            results[i].elapsed_seconds,
            results[i].total_page_accesses,
            (double)results[i].total_page_accesses /
                num_students
        );
    }

    printf("============================================================\n");

    printf(
        "\nNote: Page accesses are measured using PF logical reads.\n"
    );

    printf(
        "The query workload consists of equality lookups for all "
        "roll numbers in student.txt.\n"
    );

    printf(
        "A failed lookup indicates that a roll number was not found.\n"
    );

    free(keys);

    return 0;
}
