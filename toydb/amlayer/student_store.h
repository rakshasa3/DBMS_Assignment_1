#ifndef STUDENT_STORE_H
#define STUDENT_STORE_H

#include "../pflayer/pf.h"

#define STUDENT_PAGE_MAGIC 0x53545544
#define STUDENT_LINE_LENGTH 128

typedef struct {
    int rid;
    int roll_no;
    char source_line[STUDENT_LINE_LENGTH];
} StudentRecord;

typedef struct {
    int magic;
    int record_count;
} StudentPageHeader;

#define STUDENT_RECORDS_PER_PAGE \
    ((PF_PAGE_SIZE - sizeof(StudentPageHeader)) / sizeof(StudentRecord))

typedef struct {
    int file_desc;
    int page_num;
    int record_count;
    int records_in_page;
    char *page_buf;
} StudentStoreWriter;

int StudentStoreOpenWriter(StudentStoreWriter *writer,
                           const char *database_path);

int StudentStoreAppend(StudentStoreWriter *writer,
                       const char *line,
                       StudentRecord *record);

int StudentStoreCloseWriter(StudentStoreWriter *writer);

int StudentStoreBuild(const char *input_path,
                      const char *database_path,
                      int *total_records);

int StudentStoreGet(int file_desc,
                    int rid,
                    StudentRecord *record);

int StudentExtractRollNo(const char *line, int *roll_no);

#endif
