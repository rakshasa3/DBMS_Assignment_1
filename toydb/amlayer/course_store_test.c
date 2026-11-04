#include <stdio.h>
#include <stdlib.h>

#include "course_store.h"
#include "../pflayer/pf.h"

extern int PF_OpenFile();
extern int PF_CloseFile();

int main(int argc, char *argv[])
{
    int total_records;
    int fd;
    int test_rids[3];
    int i;

    CourseRecord record;

    if (argc != 3) {
        printf("Usage: %s <courses.txt> <database.db>\n", argv[0]);
        return 1;
    }

    if (CourseStoreBuild(
            argv[1],
            argv[2],
            &total_records
        ) != 0) {

        printf("Course database build failed.\n");
        return 1;
    }

    PF_Init();

    fd = PF_OpenFile(argv[2]);

    if (fd < 0) {
        printf("Could not reopen course database: %d\n", fd);
        return 1;
    }

    test_rids[0] = 0;
    test_rids[1] = total_records / 2;
    test_rids[2] = total_records - 1;

    printf("\nTesting record retrieval:\n");

    for (i = 0; i < 3; i++) {

        if (CourseStoreGet(fd, test_rids[i], &record) != 0) {
            printf("Failed to retrieve RID %d\n", test_rids[i]);
            PF_CloseFile(fd);
            return 1;
        }

        printf(
            "RID: %d | Course: %.8s | Source: %s\n",
            record.rid,
            record.course_code,
            record.source_line
        );

        if (record.rid != test_rids[i]) {
            printf("RID mismatch!\n");
            PF_CloseFile(fd);
            return 1;
        }
    }

    PF_CloseFile(fd);

    printf("\nCourse storage test passed.\n");

    return 0;
}
