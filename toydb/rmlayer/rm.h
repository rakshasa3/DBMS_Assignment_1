#ifndef RM_H
#define RM_H

#include "pf.h"

#define SLOT_UNUSED -1
#define SLOT_TOMBSTONE -2

//slot entry struct
typedef struct SlotEntry{
    int offset;
    int length;
} SlotEntry;

//slotted page header struct
typedef struct SlottedPageHeader{
    int numSlots;
    int freeSpaceOffset;
    int freeBytes;
} SlottedPageHeader;

//Record Identifier (RID) struct
typedef struct RID{
    int pageNum;
    int slotNum;
} RID;

//Scan descriptor struct
typedef struct RM_ScanDesc{
    int fileDesc;
    int currentPageNum;
    int currentSlotNum;
} RM_ScanDesc;

//function prototypes

//file functions
int RM_CreateFile(char *fileName);
int RM_DestroyFile(char *fileName);
int RM_OpenFile(char *fileName);
int RM_CloseFile(int fileDesc);

//record functions
int RM_InsertRecord(int fileDesc, const char *recordData, int recordLen, RID *rid);
int RM_DeleteRecord(int fileDesc, RID rid);
int RM_GetRecord(int fileDesc, RID rid, char *recordData, int *recordLen);
int RM_CompactPage(char *pageBuf);

//scan functions
int RM_OpenScan(int fileDesc,RM_ScanDesc *scanDesc);
int RM_GetNextRecord(RM_ScanDesc *scanDesc, RID *rid, char *recordData, int *recordLen);
int RM_CloseScan(RM_ScanDesc *scanDesc);

#endif