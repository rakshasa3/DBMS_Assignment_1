#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "course_store.h"

/* PF functions used by this module */
extern int PF_CreateFile(char *fname);
extern int PF_OpenFile(char *fname);
extern int PF_CloseFile(int fd);
extern int PF_AllocPage(int fd, int *page_num, char **page_buf);
extern int PF_GetThisPage(int fd, int page_num, char **page_buf);
extern int PF_UnfixPage(int fd, int page_num, int dirty);

static void strip_newline(char *line)
{
    line[strcspn(line, "\r\n")] = '\0';
}

static int extract_course_code(const char *line,
                               char code[COURSE_CODE_LENGTH])
{
    const char *separator;
    size_t length;
    size_t i;

    separator = strchr(line, ';');

    if (separator == NULL)
        return -1;

    length = (size_t)(separator - line);

    if (length == 0 || length >= COURSE_CODE_LENGTH)
        return -1;

    memset(code, ' ', COURSE_CODE_LENGTH);

    for (i = 0; i < length; i++)
        code[i] = line[i];

    return 0;
}

int CourseStoreBuild(const char *input_path,
                     const char *database_path,
                     int *total_records)
{
    FILE *input;
    int fd;
    int error;
    int rid = 0;
    int page_num = -1;
    int page_count = 0;
    char line[COURSE_LINE_LENGTH + 32];
    char code[COURSE_CODE_LENGTH];
    char *page_buf = NULL;
    CoursePageHeader *header;
    CourseRecord record;
    CourseRecord *records;

    input = fopen(input_path, "r");

    if (input == NULL) {
        perror("Opening course input");
        return -1;
    }

    /*
     * Remove an old output file if present.
     * Ensure it is not currently open in another process.
     */
    remove((char *)database_path);

    error = PF_CreateFile((char *)database_path);

    if (error != PFE_OK) {
        fclose(input);
        return error;
    }

    fd = PF_OpenFile((char *)database_path);

    if (fd < 0) {
        fclose(input);
        return fd;
    }

    while (fgets(line, sizeof(line), input) != NULL) {
        strip_newline(line);

        if (line[0] == '\0')
            continue;

        if (strncmp(line, "Database dummy", 14) == 0)
            continue;

        if (extract_course_code(line, code) != 0) {
            fprintf(stderr, "Invalid course record: %s\n", line);
            error = -1;
            goto cleanup;
        }

        if (page_count == 0 ||
            header->record_count >= COURSE_RECORDS_PER_PAGE) {

            if (page_count > 0) {
                error = PF_UnfixPage(fd, page_num, TRUE);
                if (error != PFE_OK)
                    goto cleanup;
            }

            error = PF_AllocPage(fd, &page_num, &page_buf);

            if (error != PFE_OK)
                goto cleanup;

            memset(page_buf, 0, PF_PAGE_SIZE);

            header = (CoursePageHeader *)page_buf;
            header->magic = COURSE_PAGE_MAGIC;
            header->record_count = 0;

            page_count++;
        }

        record.rid = rid;

        memcpy(record.course_code, code, COURSE_CODE_LENGTH);

        memset(record.source_line, 0, COURSE_LINE_LENGTH);
        strncpy(record.source_line, line, COURSE_LINE_LENGTH - 1);

        records = (CourseRecord *)
            (page_buf + sizeof(CoursePageHeader));

        records[header->record_count] = record;
        header->record_count++;

        rid++;
    }

    if (page_count > 0) {
        error = PF_UnfixPage(fd, page_num, TRUE);

        if (error != PFE_OK)
            goto cleanup;
    }

    *total_records = rid;
    error = PFE_OK;

cleanup:
    fclose(input);

    if (PF_CloseFile(fd) != PFE_OK && error == PFE_OK)
        error = -1;

    return error;
}

int CourseStoreGet(int file_desc, int rid, CourseRecord *record)
{
    int page_num;
    int slot;
    int error;
    char *page_buf;
    CoursePageHeader *header;
    CourseRecord *records;

    if (rid < 0 || record == NULL)
        return -1;

    page_num = rid / COURSE_RECORDS_PER_PAGE;
    slot = rid % COURSE_RECORDS_PER_PAGE;

    error = PF_GetThisPage(file_desc, page_num, &page_buf);

    if (error != PFE_OK)
        return error;

    header = (CoursePageHeader *)page_buf;

    if (header->magic != COURSE_PAGE_MAGIC ||
        slot >= header->record_count) {

        PF_UnfixPage(file_desc, page_num, FALSE);
        return -1;
    }

    records = (CourseRecord *)
        (page_buf + sizeof(CoursePageHeader));

    *record = records[slot];

    error = PF_UnfixPage(file_desc, page_num, FALSE);

    return error;
}
