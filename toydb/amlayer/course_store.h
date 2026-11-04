#ifndef COURSE_STORE_H
#define COURSE_STORE_H

#include "../pflayer/pf.h"

#define COURSE_CODE_LENGTH 8
#define COURSE_LINE_LENGTH 96
#define COURSE_PAGE_MAGIC 0x43525345

typedef struct {
    int rid;
    char course_code[COURSE_CODE_LENGTH];
    char source_line[COURSE_LINE_LENGTH];
} CourseRecord;

typedef struct {
    int magic;
    int record_count;
} CoursePageHeader;

#define COURSE_STORE_PAGE_SIZE 4096

#define COURSE_RECORDS_PER_PAGE \
    ((COURSE_STORE_PAGE_SIZE - sizeof(CoursePageHeader)) / sizeof(CourseRecord))

int CourseStoreBuild(const char *input_path,
                     const char *database_path,
                     int *total_records);

int CourseStoreGet(int file_desc, int rid, CourseRecord *record);

#endif
