#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "am.h"
#include "student_store.h"

#define STUDENT_RELATION "student"
#define STUDENT_INDEX_NO 0
#define STUDENT_INDEX_FILE "student.0"
#define STUDENT_DATA_FILE "student.data"

#define STUDENT_ATTR_TYPE 'i'
#define STUDENT_ATTR_LENGTH sizeof(int)

PF_IOStats stats;

int main(int argc, char *argv[])
{
    FILE *input;
    char line[STUDENT_LINE_LENGTH];

    int index_fd;
    int error;
    int total_records = 0;

    StudentStoreWriter writer;
    StudentRecord record;

    clock_t start, end;
    double elapsed;

    const char *input_path;

    input_path = (argc >= 2) ? argv[1] : "../student.txt";

    if (argc > 2) {
        printf("Usage: %s [student.txt]\n", argv[0]);
        return 1;
    }

    PF_Init();
    PF_ResetStats();

    start = clock();

    input = fopen(input_path, "r");

    if (input == NULL) {
        perror("Opening student input");
        return 1;
    }

    printf("Approach 1: Incremental B+ Tree construction\n");

    /*
     * Create the PF student data file.
     */
    error = StudentStoreOpenWriter(&writer, STUDENT_DATA_FILE);

    if (error != PFE_OK) {
        printf("StudentStoreOpenWriter failed: %d\n", error);
        fclose(input);
        return 1;
    }

    /*
     * Create a fresh B+ Tree index.
     */
    remove(STUDENT_INDEX_FILE);

    error = AM_CreateIndex(STUDENT_RELATION,
                           STUDENT_INDEX_NO,
                           STUDENT_ATTR_TYPE,
                           STUDENT_ATTR_LENGTH);

    if (error != AME_OK) {
        printf("AM_CreateIndex failed: %d\n", error);
        StudentStoreCloseWriter(&writer);
        fclose(input);
        return 1;
    }

    index_fd = PF_OpenFile(STUDENT_INDEX_FILE);

    if (index_fd < 0) {
        PF_PrintError();
        StudentStoreCloseWriter(&writer);
        fclose(input);
        return 1;
    }

    printf("Reading and indexing student records...\n");

    while (fgets(line, sizeof(line), input) != NULL) {

        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0')
            continue;

        if (strncmp(line, "Database dummy", 14) == 0)
            continue;

        /*
         * Store the arriving student record.
         * The function assigns its RID and extracts roll_no.
         */
        error = StudentStoreAppend(&writer, line, &record);

        if (error != PFE_OK) {
            printf("StudentStoreAppend failed at record %d: %d\n",
                   total_records, error);

            StudentStoreCloseWriter(&writer);
            PF_CloseFile(index_fd);
            fclose(input);

            return 1;
        }

        /*
         * Insert the roll number immediately.
         *
         * AM_InsertEntry performs the standard top-down
         * B+ Tree insertion and handles node splits.
         */
        error = AM_InsertEntry(index_fd,
                               STUDENT_ATTR_TYPE,
                               STUDENT_ATTR_LENGTH,
                               (char *)&record.roll_no,
                               record.rid);

        if (error != AME_OK) {
            printf("AM_InsertEntry failed for roll number %d: %d\n",
                   record.roll_no, error);

            StudentStoreCloseWriter(&writer);
            PF_CloseFile(index_fd);
            fclose(input);

            return 1;
        }

        total_records++;

        if (total_records % 1000 == 0)
            printf("Indexed %d student records...\n", total_records);
    }

    fclose(input);

    error = StudentStoreCloseWriter(&writer);

    if (error != PFE_OK) {
        printf("StudentStoreCloseWriter failed: %d\n", error);
        PF_CloseFile(index_fd);
        return 1;
    }

    PF_CloseFile(index_fd);

    PF_GetStats(&stats);

    end = clock();
    elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\n========== INCREMENTAL INDEXING ==========\n");

    printf("Total records indexed: %d\n", total_records);

    printf("\nPF I/O Statistics:\n");
    printf("Logical reads:   %lu\n", stats.logical_reads);
    printf("Logical writes:  %lu\n", stats.logical_writes);
    printf("Physical reads:  %lu\n", stats.physical_reads);
    printf("Physical writes: %lu\n", stats.physical_writes);
    printf("Cache hits:      %lu\n", stats.cache_hits);
    printf("Cache misses:    %lu\n", stats.cache_misses);

    printf("\nElapsed CPU time: %.6f seconds\n", elapsed);

    printf("Data file: %s\n", STUDENT_DATA_FILE);
    printf("Index file: %s\n", STUDENT_INDEX_FILE);

    printf("==========================================\n");

    return 0;
}
