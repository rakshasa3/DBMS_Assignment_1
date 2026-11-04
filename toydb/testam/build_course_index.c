#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pf.h"
#include "am.h"
#include "course_store.h"

#define COURSE_RELATION "courses"
#define COURSE_INDEX_NO 0
#define COURSE_INDEX_FILE "courses.0"

#define COURSE_ATTR_TYPE 'c'
#define COURSE_ATTR_LENGTH COURSE_CODE_LENGTH

int main(int argc, char *argv[])
{
    int total_records = 0;
    int data_fd;
    int index_fd;
    int rid;
    int error;
    CourseRecord record;

    if (argc != 2) {
        printf("Usage: %s <courses.txt>\n", argv[0]);
        return 1;
    }

    PF_Init();

    printf("Building structured course data file...\n");

    error = CourseStoreBuild(argv[1], "courses.data",
                             &total_records);

    if (error != PFE_OK) {
        printf("Course data build failed: %d\n", error);
        return 1;
    }

    printf("Stored %d course records.\n", total_records);

    data_fd = PF_OpenFile("courses.data");

    if (data_fd < 0) {
        PF_PrintError("Opening courses.data");
        return 1;
    }

    /* Remove an old index file before creating a fresh index. */
    remove(COURSE_INDEX_FILE);

    printf("Creating B+ tree index...\n");

    error = AM_CreateIndex(COURSE_RELATION,
                           COURSE_INDEX_NO,
                           COURSE_ATTR_TYPE,
                           COURSE_ATTR_LENGTH);

    if (error != AME_OK) {
        printf("AM_CreateIndex failed: %d\n", error);
        PF_CloseFile(data_fd);
        return 1;
    }

    index_fd = PF_OpenFile(COURSE_INDEX_FILE);

    if (index_fd < 0) {
        PF_PrintError("Opening courses.0");
        PF_CloseFile(data_fd);
        return 1;
    }

    printf("Inserting course codes into B+ tree...\n");

    for (rid = 0; rid < total_records; rid++) {

        error = CourseStoreGet(data_fd, rid, &record);

        if (error != PFE_OK) {
            printf("Could not read course RID %d: %d\n",
                   rid, error);
            PF_CloseFile(index_fd);
            PF_CloseFile(data_fd);
            return 1;
        }

        error = AM_InsertEntry(index_fd,
                               COURSE_ATTR_TYPE,
                               COURSE_ATTR_LENGTH,
                               record.course_code,
                               record.rid);

        if (error != AME_OK) {
            printf("AM_InsertEntry failed at RID %d: %d\n",
                   rid, error);
            PF_CloseFile(index_fd);
            PF_CloseFile(data_fd);
            return 1;
        }

        if ((rid + 1) % 500 == 0)
            printf("Indexed %d records...\n", rid + 1);
    }

    PF_CloseFile(index_fd);
    PF_CloseFile(data_fd);

    printf("\nIndex construction completed.\n");
    printf("Total records indexed: %d\n", total_records);
    printf("Data file: courses.data\n");
    printf("Index file: courses.0\n");

    return 0;
}
