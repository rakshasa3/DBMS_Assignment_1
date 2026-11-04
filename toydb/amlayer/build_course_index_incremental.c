#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "am.h"
#include "course_store.h"

#define COURSE_RELATION "courses"
#define COURSE_INDEX_NO 0
#define COURSE_INDEX_FILE "courses.0"
#define COURSE_DATA_FILE "courses.data"

#define COURSE_ATTR_TYPE 'c'
#define COURSE_ATTR_LENGTH COURSE_CODE_LENGTH

PF_IOStats stats;

int main(int argc, char *argv[])
{
    FILE *input;
    char line[COURSE_LINE_LENGTH + 32];

    int index_fd;
    int error;
    int total_records = 0;

    CourseStoreWriter writer;
    CourseRecord record;

    clock_t start, end;
    double elapsed;

    const char *input_path;

    input_path = (argc >= 2) ? argv[1] : "courses.txt";

    if (argc > 2) {
        printf("Usage: %s [courses.txt]\n", argv[0]);
        return 1;
    }

    PF_Init();
    PF_ResetStats();
    start = clock();

    input = fopen(input_path, "r");

    if (input == NULL) {
        perror("Opening course input");
        return 1;
    }

    start = clock();

    printf("Approach 2: Incremental B+ tree construction\n");

    /* Create and open the data file for incremental writing. */
    error = CourseStoreOpenWriter(&writer, COURSE_DATA_FILE);

    if (error != PFE_OK) {
        printf("CourseStoreOpenWriter failed: %d\n", error);
        fclose(input);
        return 1;
    }

    /* Create a fresh B+ tree index. */
    remove(COURSE_INDEX_FILE);

    error = AM_CreateIndex(COURSE_RELATION,
                           COURSE_INDEX_NO,
                           COURSE_ATTR_TYPE,
                           COURSE_ATTR_LENGTH);

    if (error != AME_OK) {
        printf("AM_CreateIndex failed: %d\n", error);
        CourseStoreCloseWriter(&writer);
        fclose(input);
        return 1;
    }

    index_fd = PF_OpenFile(COURSE_INDEX_FILE);

    if (index_fd < 0) {
        PF_PrintError("Opening course index");
        CourseStoreCloseWriter(&writer);
        fclose(input);
        return 1;
    }

    printf("Reading and indexing course records...\n");

    while (fgets(line, sizeof(line), input) != NULL) {

        /* Skip blank lines. */
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0')
            continue;

        /* Skip the input header, if present. */
        if (strncmp(line, "Database dummy", 14) == 0)
            continue;

        /*
         * Append the record to the data file.
         * The returned record contains its assigned RID and course key.
         */
        error = CourseStoreAppend(&writer, line, &record);

        if (error != PFE_OK) {
            printf("CourseStoreAppend failed at record %d: %d\n",
                   total_records, error);

            CourseStoreCloseWriter(&writer);
            PF_CloseFile(index_fd);
            fclose(input);
            return 1;
        }

        /*
         * Immediately insert this record's key into the B+ tree.
         */
        error = AM_InsertEntry(index_fd,
                               COURSE_ATTR_TYPE,
                               COURSE_ATTR_LENGTH,
                               record.course_code,
                               record.rid);

        if (error != AME_OK) {
            printf("AM_InsertEntry failed at RID %d: %d\n",
                   record.rid, error);

            CourseStoreCloseWriter(&writer);
            PF_CloseFile(index_fd);
            fclose(input);
            return 1;
        }

        total_records++;

        if (total_records % 500 == 0)
            printf("Indexed %d records...\n", total_records);
    }

    fclose(input);

    /* Flush and close the data writer. */
    error = CourseStoreCloseWriter(&writer);

    if (error != PFE_OK) {
        printf("CourseStoreCloseWriter failed: %d\n", error);
        PF_CloseFile(index_fd);
        return 1;
    }

    PF_CloseFile(index_fd);
    PF_GetStats(&stats);

    printf("\nPF I/O Statistics:\n");
    printf("Logical reads:   %lu\n", stats.logical_reads);
    printf("Logical writes:  %lu\n", stats.logical_writes);
    printf("Physical reads:  %lu\n", stats.physical_reads);
    printf("Physical writes: %lu\n", stats.physical_writes);
    printf("Cache hits:      %lu\n", stats.cache_hits);
    printf("Cache misses:    %lu\n", stats.cache_misses);

    end = clock();
    elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\nIncremental construction completed.\n");
    printf("Total records indexed: %d\n", total_records);
    printf("Data file: %s\n", COURSE_DATA_FILE);
    printf("Index file: %s\n", COURSE_INDEX_FILE);
    printf("Elapsed CPU time: %.6f seconds\n", elapsed);

    return 0;
}
