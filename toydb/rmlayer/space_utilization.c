#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rm.h"

int main(void) {
    FILE *fp;
    char line[1024];
    int totalRecords = 0;
    long totalRawBytes = 0;
    int fileDesc;
    RID rid;
    int pageCount = 0;
    int pNum;
    char *pBuf;
    int errVal;
    long slottedTotalAllocated;
    double slottedUtil;
    int maxLens[3] = {128, 256, 512};
    int i;

    PF_Init();

    /* 1. Slotted-Page Storage Setup */
    RM_DestroyFile("courses_slotted.db");
    if (RM_CreateFile("courses_slotted.db") != PFE_OK) {
        printf("Error creating courses_slotted.db\n");
        return 1;
    }

    fileDesc = RM_OpenFile("courses_slotted.db");
    if (fileDesc < 0) {
        printf("Error opening courses_slotted.db\n");
        return 1;
    }

    /* Locate and parse courses.txt */
    fp = fopen("../../data/courses.txt", "r");
    if (!fp) {
        fp = fopen("../data/courses.txt", "r");
    }
    if (!fp) {
        fp = fopen("courses.txt", "r");
    }
    if (!fp) {
        printf("Error: Could not find courses.txt\n");
        RM_CloseFile(fileDesc);
        return 1;
    }

    while (fgets(line, sizeof(line), fp)) {
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) {
            line[len-1] = '\0';
            len--;
        }
        if (len == 0) continue;

        totalRecords++;
        totalRawBytes += len;

        if (RM_InsertRecord(fileDesc, line, len, &rid) != PFE_OK) {
            printf("Error inserting record #%d\n", totalRecords);
        }
    }
    fclose(fp);

    /* Count actual slotted pages allocated */
    errVal = PF_GetFirstPage(fileDesc, &pNum, &pBuf);
    while (errVal == PFE_OK) {
        pageCount++;
        int prevPage = pNum;
        errVal = PF_GetNextPage(fileDesc, &pNum, &pBuf);
        PF_UnfixPage(fileDesc, prevPage, 0);
    }

    RM_CloseFile(fileDesc);

    slottedTotalAllocated = (long)pageCount * PF_PAGE_SIZE;
    slottedUtil = ((double)totalRawBytes / (double)slottedTotalAllocated) * 100.0;

    /* 2. Print Summary Comparison Table */
    printf("\n=========================================================================\n");
    printf("                  OBJECTIVE 2: SPACE UTILIZATION COMPARISON              \n");
    printf("=========================================================================\n");
    printf("Total Records Loaded : %d\n", totalRecords);
    printf("Total Raw Data Size  : %ld bytes\n\n", totalRawBytes);

    printf("+---------------------------------------+---------------+-----------------+\n");
    printf("| Storage Model                         | Allocated (B) | Utilization (%%) |\n");
    printf("+---------------------------------------+---------------+-----------------+\n");
    printf("| Slotted-Page (Dynamic Variable-Length)| %13ld | %14.2f%% |\n", slottedTotalAllocated, slottedUtil);

    for (i = 0; i < 3; i++) {
        int L = maxLens[i];
        int recsPerPage = PF_PAGE_SIZE / L;
        int staticPages = (totalRecords + recsPerPage - 1) / recsPerPage;
        long staticTotalAllocated = (long)staticPages * PF_PAGE_SIZE;
        double staticUtil = ((double)totalRawBytes / (double)staticTotalAllocated) * 100.0;

        printf("| Static Fixed-Length (MaxLen = %3d B) | %13ld | %14.2f%% |\n", L, staticTotalAllocated, staticUtil);
    }
    printf("+---------------------------------------+---------------+-----------------+\n\n");

    RM_DestroyFile("courses_slotted.db");
    return 0;
}
