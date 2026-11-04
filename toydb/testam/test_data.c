#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define malloc system_malloc_override
#define calloc system_calloc_override

#include "pf.h"
#include "am.h"
#include "testam.h"

#undef malloc
#undef calloc

#undef RELNAME
#define RELNAME "courserel"

#define ATTR_LEN 20

void load_and_index_courses(char *relname, int fileDesc) {
    char filepath[256];
    FILE *fp;
    int recId = 0;
    char line[512];
    char courseCode[256];

    snprintf(filepath, sizeof(filepath), "../../../data/courses.txt");

    fp = fopen(filepath, "r");
    if (fp == NULL) {
        printf("Error: Could not open dataset file at %s\n", filepath);
        exit(1);
    }

    printf("Loading course dataset line-by-line from: %s\n", filepath);

    /* Read full lines instead of single words */
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* Strip trailing newline or carriage return */
        line[strcspn(line, "\r\n")] = '\0';

        /* Skip empty lines */
        if (strlen(line) == 0) continue;

        /* Truncate line to fit in B+ tree key fixed length */
        strncpy(courseCode, line, ATTR_LEN - 1);
        courseCode[ATTR_LEN - 1] = '\0';

        /* Pad remaining spaces */
        padstring(courseCode, ATTR_LEN);

        /* Insert into B+ tree index */
        xAM_InsertEntry(fileDesc, 'c', ATTR_LEN, courseCode, recId);
        
        if (recId % 100 == 0) {
            printf("Indexed %d records... (Latest: '%.20s')\n", recId, courseCode);
        }
        recId++;
    }

    fclose(fp);
    printf("\nSuccessfully indexed %d course records.\n\n", recId);
}

int main() {
    int fileDesc;
    int scanDesc;
    int recId;
    int scannedCount = 0;

    printf("=== Initializing Course Database Index Test ===\n");

    /* Create and open index */
    xAM_CreateIndex(RELNAME, 0, 'c', ATTR_LEN);
    fileDesc = xPF_OpenFile("courserel.0");

    /* Populate dataset */
    load_and_index_courses(RELNAME, fileDesc);

    /* Perform scan */
    printf("--- Opening Index Scan ---\n");
    scanDesc = xAM_OpenIndexScan(fileDesc, 'c', ATTR_LEN, EQUAL, NULL);

    while ((recId = xAM_FindNextEntry(scanDesc)) != AME_EOF) {
        scannedCount++;
    }

    printf("Scan completed! Total retrieved records: %d\n", scannedCount);

    /* Cleanup */
    xAM_CloseIndexScan(scanDesc);
    xPF_CloseFile(fileDesc);
    xAM_DestroyIndex(RELNAME, 0);

    printf("=== Test Completed Successfully ===\n");
    return 0;
}
