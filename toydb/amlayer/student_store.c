#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "student_store.h"

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


/*
 * The first semicolon-separated field is the student roll number.
 */
int StudentExtractRollNo(const char *line, int *roll_no)
{
    char field[32];
    const char *separator;
    size_t length;
    char *end;
    long value;

    if (line == NULL || roll_no == NULL)
        return -1;

    separator = strchr(line, ';');

    if (separator == NULL)
        return -1;

    length = (size_t)(separator - line);

    if (length == 0 || length >= sizeof(field))
        return -1;

    memcpy(field, line, length);
    field[length] = '\0';

    errno = 0;
    value = strtol(field, &end, 10);

    if (errno != 0 || *end != '\0' ||
        value < 0 || value > 2147483647L)
        return -1;

    *roll_no = (int)value;

    return 0;
}


/*
 * Create a new PF student data file.
 */
int StudentStoreOpenWriter(StudentStoreWriter *writer,
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
 * Append one student record to the PF data file.
 * Returns the assigned RID and roll number.
 */
int StudentStoreAppend(StudentStoreWriter *writer,
                       const char *line,
                       StudentRecord *record)
{
    int error;
    int roll_no;

    char clean_line[STUDENT_LINE_LENGTH];
    StudentPageHeader *header;
    StudentRecord *records;

    if (writer == NULL || line == NULL || record == NULL ||
        writer->file_desc < 0)
        return -1;

    strncpy(clean_line, line, sizeof(clean_line) - 1);
    clean_line[sizeof(clean_line) - 1] = '\0';

    strip_newline(clean_line);

    if (clean_line[0] == '\0')
        return -1;

    if (strncmp(clean_line, "Database dummy", 14) == 0)
        return -1;

    if (StudentExtractRollNo(clean_line, &roll_no) != 0)
        return -1;

    /*
     * Allocate a new PF page when necessary.
     */
    if (writer->page_buf == NULL ||
        writer->records_in_page >= STUDENT_RECORDS_PER_PAGE) {

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

        header = (StudentPageHeader *)writer->page_buf;

        header->magic = STUDENT_PAGE_MAGIC;
        header->record_count = 0;

        writer->records_in_page = 0;
    }

    /*
     * Assign the record ID and construct the stored record.
     */
    record->rid = writer->record_count;
    record->roll_no = roll_no;

    memset(record->source_line, 0, STUDENT_LINE_LENGTH);

    strncpy(record->source_line,
            clean_line,
            STUDENT_LINE_LENGTH - 1);

    header = (StudentPageHeader *)writer->page_buf;

    records = (StudentRecord *)
        (writer->page_buf + sizeof(StudentPageHeader));

    records[writer->records_in_page] = *record;

    writer->records_in_page++;
    writer->record_count++;

    header->record_count = writer->records_in_page;

    return PFE_OK;
}


/*
 * Flush the final page and close the PF file.
 */
int StudentStoreCloseWriter(StudentStoreWriter *writer)
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
 * Build the complete PF student data file first.
 * This is used by the post-population approach.
 */
int StudentStoreBuild(const char *input_path,
                      const char *database_path,
                      int *total_records)
{
    FILE *input;
    char line[STUDENT_LINE_LENGTH];

    int error;
    int count = 0;

    StudentStoreWriter writer;
    StudentRecord record;

    if (total_records == NULL)
        return -1;

    input = fopen(input_path, "r");

    if (input == NULL) {
        perror("Opening student input");
        return -1;
    }

    error = StudentStoreOpenWriter(&writer, database_path);

    if (error != PFE_OK) {
        fclose(input);
        return error;
    }

    while (fgets(line, sizeof(line), input) != NULL) {

        strip_newline(line);

        if (line[0] == '\0')
            continue;

        if (strncmp(line, "Database dummy", 14) == 0)
            continue;

        error = StudentStoreAppend(&writer, line, &record);

        if (error != PFE_OK) {
            fprintf(stderr,
                    "Invalid student record at input record %d: %s\n",
                    count, line);

            StudentStoreCloseWriter(&writer);
            fclose(input);

            return error;
        }

        count++;
    }

    fclose(input);

    error = StudentStoreCloseWriter(&writer);

    if (error != PFE_OK)
        return error;

    *total_records = count;

    return PFE_OK;
}


/*
 * Read a stored student record using its RID.
 */
int StudentStoreGet(int file_desc,
                    int rid,
                    StudentRecord *record)
{
    int page_num;
    int slot;
    int error;

    char *page_buf;

    StudentPageHeader *header;
    StudentRecord *records;

    if (rid < 0 || record == NULL)
        return -1;

    page_num = rid / STUDENT_RECORDS_PER_PAGE;
    slot = rid % STUDENT_RECORDS_PER_PAGE;

    error = PF_GetThisPage(file_desc, page_num, &page_buf);

    if (error != PFE_OK)
        return error;

    header = (StudentPageHeader *)page_buf;

    if (header->magic != STUDENT_PAGE_MAGIC ||
        slot >= header->record_count) {

        PF_UnfixPage(file_desc, page_num, FALSE);
        return -1;
    }

    records = (StudentRecord *)
        (page_buf + sizeof(StudentPageHeader));

    *record = records[slot];

    error = PF_UnfixPage(file_desc, page_num, FALSE);

    return error;
}
