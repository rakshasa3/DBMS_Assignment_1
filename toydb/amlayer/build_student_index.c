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
    int total_records = 0;

    int data_fd;
    int index_fd;
    int rid;
    int error;

    clock_t start, end;
    double elapsed;

    StudentRecord record;

    const char *input_path;

    input_path = (argc >= 2) ? argv[1] : "../student.txt";

    if (argc > 2) {
        printf("Usage: %s [student.txt]\n", argv[0]);
        return 1;
    }

    PF_Init();
    PF_ResetStats();

    start = clock();

    printf("Approach 2: Post-population B+ Tree construction\n");

    /*
     * Phase 1:
     * Store the entire Student file in PF format.
     */
    printf("Building PF student data file...\n");

    error = StudentStoreBuild(input_path,
                              STUDENT_DATA_FILE,
                              &total_records);

    if (error != PFE_OK) {
        printf("StudentStoreBuild failed: %d\n", error);
        return 1;
    }

    printf("Stored %d student records.\n", total_records);

    /*
     * Open the completed PF student data file.
     */
    data_fd = PF_OpenFile(STUDENT_DATA_FILE);

    if (data_fd < 0) {
        PF_PrintError();
        return 1;
    }

    /*
     * Phase 2:
     * Create an empty B+ Tree index.
     */
    remove(STUDENT_INDEX_FILE);

    error = AM_CreateIndex(STUDENT_RELATION,
                           STUDENT_INDEX_NO,
                           STUDENT_ATTR_TYPE,
                           STUDENT_ATTR_LENGTH);

    if (error != AME_OK) {
        printf("AM_CreateIndex failed: %d\n", error);
        PF_CloseFile(data_fd);
        return 1;
    }

    index_fd = PF_OpenFile(STUDENT_INDEX_FILE);

    if (index_fd < 0) {
        PF_PrintError();
        PF_CloseFile(data_fd);
        return 1;
    }

    /*
     * Phase 3:
     * Scan all stored records and insert their roll numbers.
     */
    printf("Scanning student data and constructing B+ Tree...\n");

    for (rid = 0; rid < total_records; rid++) {

        error = StudentStoreGet(data_fd, rid, &record);

        if (error != PFE_OK) {
            printf("Could not read student RID %d: %d\n",
                   rid, error);

            PF_CloseFile(index_fd);
            PF_CloseFile(data_fd);

            return 1;
        }

        error = AM_InsertEntry(index_fd,
                               STUDENT_ATTR_TYPE,
                               STUDENT_ATTR_LENGTH,
                               (char *)&record.roll_no,
                               record.rid);

        if (error != AME_OK) {
            printf("AM_InsertEntry failed for roll number %d: %d\n",
                   record.roll_no, error);

            PF_CloseFile(index_fd);
            PF_CloseFile(data_fd);

            return 1;
        }

        if ((rid + 1) % 1000 == 0)
            printf("Indexed %d student records...\n", rid + 1);
    }

    PF_CloseFile(index_fd);
    PF_CloseFile(data_fd);

    PF_GetStats(&stats);

    end = clock();
    elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\n========== POST-POPULATION INDEXING ==========\n");

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

    printf("===============================================\n");

    return 0;
}
