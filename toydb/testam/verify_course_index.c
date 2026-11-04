#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pf.h"
#include "am.h"
#include "course_store.h"

#define COURSE_INDEX_FILE "courses.0"
#define COURSE_DATA_FILE "courses.data"
#define COURSE_ATTR_TYPE 'c'
#define COURSE_ATTR_LENGTH COURSE_CODE_LENGTH

static void make_key(const char *text, char key[COURSE_CODE_LENGTH])
{
    memset(key, ' ', COURSE_CODE_LENGTH);
    memcpy(key, text, strlen(text));
}

static int search_course(int index_fd, int data_fd,
                         const char *course_code)
{
    char key[COURSE_CODE_LENGTH];
    int scan_fd;
    int rid;
    int found = 0;
    CourseRecord record;

    make_key(course_code, key);

    scan_fd = AM_OpenIndexScan(index_fd, COURSE_ATTR_TYPE,
                               COURSE_ATTR_LENGTH, EQUAL, key);

    if (scan_fd < 0) {
        printf("Could not open search scan: %d\n", scan_fd);
        return -1;
    }

    while ((rid = AM_FindNextEntry(scan_fd)) != AME_EOF) {
        if (rid < 0) {
            printf("Index search error: %d\n", rid);
            AM_CloseIndexScan(scan_fd);
            return -1;
        }

        if (CourseStoreGet(data_fd, rid, &record) != PFE_OK) {
            printf("Could not retrieve RID %d\n", rid);
            AM_CloseIndexScan(scan_fd);
            return -1;
        }

        printf("Found: RID=%d, Course=%s\n",
               rid, record.course_code);

        found++;
    }

    AM_CloseIndexScan(scan_fd);
    return found;
}

int main(void)
{
    int index_fd;
    int data_fd;
    int scan_fd;
    int rid;
    int count = 0;
    int errors = 0;
    int seen[10000] = {0};
    CourseRecord record;

    PF_Init();

    index_fd = PF_OpenFile(COURSE_INDEX_FILE);

    if (index_fd < 0) {
        PF_PrintError("Opening course index");
        return 1;
    }

    data_fd = PF_OpenFile(COURSE_DATA_FILE);

    if (data_fd < 0) {
        PF_PrintError("Opening course data");
        PF_CloseFile(index_fd);
        return 1;
    }

    printf("Searching for an existing course:\n");

    if (search_course(index_fd, data_fd, "HS 702") != 1)
        errors++;

    printf("\nSearching for a missing course:\n");

    if (search_course(index_fd, data_fd, "ZZ 999") != 0)
        errors++;

    printf("\nVerifying all indexed records:\n");

    scan_fd = AM_OpenIndexScan(index_fd, COURSE_ATTR_TYPE,
                               COURSE_ATTR_LENGTH, EQUAL, NULL);

    if (scan_fd < 0) {
        printf("Could not open full index scan: %d\n", scan_fd);
        PF_CloseFile(data_fd);
        PF_CloseFile(index_fd);
        return 1;
    }

    while ((rid = AM_FindNextEntry(scan_fd)) != AME_EOF) {

        if (rid < 0) {
            printf("Full scan error: %d\n", rid);
            errors++;
            break;
        }

        if (rid >= 10000 || seen[rid]) {
            printf("Invalid or duplicate RID: %d\n", rid);
            errors++;
            continue;
        }

        seen[rid] = 1;

        if (CourseStoreGet(data_fd, rid, &record) != PFE_OK) {
            printf("Cannot retrieve RID %d\n", rid);
            errors++;
            continue;
        }

        if (record.rid != rid) {
            printf("RID mismatch: index=%d, data=%d\n",
                   rid, record.rid);
            errors++;
        }

        count++;
    }

    AM_CloseIndexScan(scan_fd);

    printf("\nTotal records found by index scan: %d\n", count);

    if (count != 4268) {
        printf("Expected 4268 records.\n");
        errors++;
    }

    if (errors == 0)
        printf("\nPASS: Course index verification successful.\n");
    else
        printf("\nFAIL: %d verification problem(s).\n", errors);

    PF_CloseFile(data_fd);
    PF_CloseFile(index_fd);

    return errors == 0 ? 0 : 1;
}
