/* Include standard headers FIRST */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Prevent am.h from redeclaring malloc/calloc using preprocessor macros */
#define malloc system_malloc_override
#define calloc system_calloc_override

#include "pf.h"
#include "am.h"
#include "testam.h"

/* Undefine macros so rest of the file uses standard library functions */
#undef malloc
#undef calloc

void load_data_from_file(char *data_filename, int fileDesc, char attrType, int attrLength) {
    char filepath[256];
    FILE *fp;
    int recId;
    char strVal[256];
    int intVal;

    /* Build relative path to ../data directory */
    snprintf(filepath, sizeof(filepath), "../data/%s", data_filename);

    fp = fopen(filepath, "r");
    if (fp == NULL) {
        printf("Error: Could not open dataset file at %s\n", filepath);
        exit(1);
    }

    printf("Loading data from %s...\n", filepath);

    if (attrType == 'c') {
        /* Read lines containing: <recId> <stringKey> */
        while (fscanf(fp, "%d %s", &recId, strVal) == 2) {
            padstring(strVal, attrLength);
            xAM_InsertEntry(fileDesc, attrType, attrLength, strVal, recId);
        }
    } else if (attrType == 'i') {
        /* Read lines containing: <recId> <integerKey> */
        while (fscanf(fp, "%d %d", &recId, &intVal) == 2) {
            xAM_InsertEntry(fileDesc, attrType, attrLength, (char *)&intVal, recId);
        }
    }

    fclose(fp);
    printf("Successfully loaded dataset from %s\n", filepath);
}

void padstring(char *str, int len) {
    int i;
    for (i = strlen(str); i < len; i++) {
        str[i] = ' ';
    }
    str[len] = '\0';
}

void xAM_CreateIndex(char *fileName, int indexNo, char attrType, int attrLength) {
    int errval;
    if ((errval = AM_CreateIndex(fileName, indexNo, attrType, attrLength)) != AME_OK) {
        printf("AM_CreateIndex(%s, %d, %c, %d) failed: %d\n", fileName, indexNo, attrType, attrLength, errval);
        exit(1);
    }
}

void xAM_DestroyIndex(char *fileName, int indexNo) {
    int errval;
    if ((errval = AM_DestroyIndex(fileName, indexNo)) != AME_OK) {
        printf("AM_DestroyIndex(%s, %d) failed: %d\n", fileName, indexNo, errval);
        exit(1);
    }
}

int xPF_OpenFile(char *fileName) {
    int fileDesc;
    if ((fileDesc = PF_OpenFile(fileName)) < 0) {
        printf("PF_OpenFile(%s) failed: %d\n", fileName, fileDesc);
        exit(1);
    }
    return fileDesc;
}

void xPF_CloseFile(int fileDesc) {
    int errval;
    if ((errval = PF_CloseFile(fileDesc)) != PFE_OK) {
        printf("PF_CloseFile(%d) failed: %d\n", fileDesc, errval);
        exit(1);
    }
}

void xAM_InsertEntry(int fileDesc, char attrType, int attrLength, char *value, int recId) {
    int errval;
    if ((errval = AM_InsertEntry(fileDesc, attrType, attrLength, value, recId)) != AME_OK) {
        printf("AM_InsertEntry(%d, %c, %d, ..., %d) failed: %d\n", fileDesc, attrType, attrLength, recId, errval);
        exit(1);
    }
}

void xAM_DeleteEntry(int fileDesc, char attrType, int attrLength, char *value, int recId) {
    int errval;
    if ((errval = AM_DeleteEntry(fileDesc, attrType, attrLength, value, recId)) != AME_OK) {
        printf("AM_DeleteEntry(%d, %c, %d, ..., %d) failed: %d\n", fileDesc, attrType, attrLength, recId, errval);
        exit(1);
    }
}

int xAM_OpenIndexScan(int fileDesc, char attrType, int attrLength, int op, char *value) {
    int scanDesc;
    if ((scanDesc = AM_OpenIndexScan(fileDesc, attrType, attrLength, op, value)) < 0) {
        printf("AM_OpenIndexScan(%d, %c, %d, %d, ...) failed: %d\n", fileDesc, attrType, attrLength, op, scanDesc);
        exit(1);
    }
    return scanDesc;
}

int xAM_FindNextEntry(int scanDesc) {
    return AM_FindNextEntry(scanDesc);
}

void xAM_CloseIndexScan(int scanDesc) {
    int errval;
    if ((errval = AM_CloseIndexScan(scanDesc)) != AME_OK) {
        printf("AM_CloseIndexScan(%d) failed: %d\n", scanDesc, errval);
        exit(1);
    }
}
