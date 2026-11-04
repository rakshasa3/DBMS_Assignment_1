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

/*
 * Open a new course database for incremental writing.
 */
int CourseStoreOpenWriter(CourseStoreWriter *writer,
                          const char *database_path)
{
    int error;

    if (writer == NULL || database_path == NULL)
        return -1;

    writer->file_desc = -1;
    writer->page_num = -1;
    writer->record_count = 0;
    writer->records_in_page = 0;
    writer->page_buf = NULL;

    /* Remove an old database file, if present. */
    remove((char *)database_path);

    error = PF_CreateFile((char *)database_path);

    if (error != PFE_OK)
        return error;

    writer->file_desc = PF_OpenFile((char *)database_path);

    if (writer->file_desc < 0)
        return writer->file_desc;

    return PFE_OK;
}

/*
 * Append one course record to the database.
 */
int CourseStoreAppend(CourseStoreWriter *writer,
                      const char *line,
                      CourseRecord *record)
{
    int error;
    char code[COURSE_CODE_LENGTH];
    char clean_line[COURSE_LINE_LENGTH + 32];
    CoursePageHeader *header;
    CourseRecord *records;

    if (writer == NULL || line == NULL || record == NULL ||
        writer->file_desc < 0)
        return -1;

    /* Make a local copy so the caller's line is not modified. */
    strncpy(clean_line, line, sizeof(clean_line) - 1);
    clean_line[sizeof(clean_line) - 1] = '\0';
    strip_newline(clean_line);

    if (clean_line[0] == '\0')
        return -1;

    if (strncmp(clean_line, "Database dummy", 14) == 0)
        return -1;

    if (extract_course_code(clean_line, code) != 0)
        return -1;

    /*
     * Allocate a page when there is no current page or the current
     * page has reached its capacity.
     */
    if (writer->page_buf == NULL ||
        writer->records_in_page >= COURSE_RECORDS_PER_PAGE) {

        /* Release the previous page before allocating another one. */
        if (writer->page_buf != NULL) {
            error = PF_UnfixPage(writer->file_desc,
                                 writer->page_num,
                                 TRUE);

            if (error != PFE_OK)
                return error;

            writer->page_buf = NULL;
        }

        error = PF_AllocPage(writer->file_desc,
                             &writer->page_num,
                             &writer->page_buf);

        if (error != PFE_OK)
            return error;

        memset(writer->page_buf, 0, PF_PAGE_SIZE);

        header = (CoursePageHeader *)writer->page_buf;
        header->magic = COURSE_PAGE_MAGIC;
        header->record_count = 0;

        writer->records_in_page = 0;
    }

    /* Construct the record using the existing on-disk format. */
    record->rid = writer->record_count;

    memcpy(record->course_code, code, COURSE_CODE_LENGTH);

    memset(record->source_line, 0, COURSE_LINE_LENGTH);
    strncpy(record->source_line,
            clean_line,
            COURSE_LINE_LENGTH - 1);

    /* Store the record immediately after the page header. */
    header = (CoursePageHeader *)writer->page_buf;

    records = (CourseRecord *)
        (writer->page_buf + sizeof(CoursePageHeader));

    records[writer->records_in_page] = *record;

    writer->records_in_page++;
    writer->record_count++;

    header->record_count = writer->records_in_page;

    return PFE_OK;
}

/*
 * Flush the final page and close the database.
 */
int CourseStoreCloseWriter(CourseStoreWriter *writer)
{
    int error;
    int close_error;

    if (writer == NULL || writer->file_desc < 0)
        return -1;

    error = PFE_OK;

    if (writer->page_buf != NULL) {
        error = PF_UnfixPage(writer->file_desc,
                             writer->page_num,
                             TRUE);

        writer->page_buf = NULL;
    }

    close_error = PF_CloseFile(writer->file_desc);

    writer->file_desc = -1;
    writer->page_num = -1;
    writer->records_in_page = 0;

    if (error != PFE_OK)
        return error;

    if (close_error != PFE_OK)
        return close_error;

    return PFE_OK;
}

/*
 * Existing post-population builder.
 */
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
    CoursePageHeader *header = NULL;
    CourseRecord record;
    CourseRecord *records;

    input = fopen(input_path, "r");

    if (input == NULL) {
        perror("Opening course input");
        return -1;
    }

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

/*
 * Existing record lookup. The RID-to-page mapping is unchanged.
 */
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
