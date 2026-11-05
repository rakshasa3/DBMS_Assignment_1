#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rm.h"

int RM_CreateFile(char *fileName) {
    return PF_CreateFile(fileName);
}

int RM_DestroyFile(char *fileName) {
    return PF_DestroyFile(fileName);
}

int RM_OpenFile(char *fileName) {
    return PF_OpenFile(fileName);
}

int RM_CloseFile(int fileDesc) {
    return PF_CloseFile(fileDesc);
}

/* Compacts page data region to eliminate internal fragmentation caused by deletions */
int RM_CompactPage(char *pageBuf) {
    SlottedPageHeader *hdr = (SlottedPageHeader *)pageBuf;
    SlotEntry *slots = (SlotEntry *)(pageBuf + sizeof(SlottedPageHeader));
    
    char tempBuf[PF_PAGE_SIZE];
    int newFreeSpaceOffset = PF_PAGE_SIZE;
    int i;

    /* Copy active records to the end of tempBuf */
    for (i = 0; i < hdr->numSlots; i++) {
        if (slots[i].offset > 0 && slots[i].length > 0) {
            newFreeSpaceOffset -= slots[i].length;
            memcpy(tempBuf + newFreeSpaceOffset, pageBuf + slots[i].offset, slots[i].length);
            slots[i].offset = newFreeSpaceOffset;
        }
    }

    /* Copy re-packed record data back into pageBuf */
    if (newFreeSpaceOffset < PF_PAGE_SIZE) {
        memcpy(pageBuf + newFreeSpaceOffset, tempBuf + newFreeSpaceOffset, PF_PAGE_SIZE - newFreeSpaceOffset);
    }
    
    hdr->freeSpaceOffset = newFreeSpaceOffset;
    return 0;
}

/* Inserts a variable-length record into the first page with sufficient free space */
int RM_InsertRecord(int fileDesc, const char *recordData, int recordLen, RID *rid) {
    int pageNum = -1;
    char *pageBuf = NULL;
    int errVal;
    int targetSlot = -1;
    SlottedPageHeader *hdr = NULL;
    SlotEntry *slots = NULL;
    int requiredSpace;

    /* Find existing page or allocate a new one */
    errVal = PF_GetFirstPage(fileDesc, &pageNum, &pageBuf);
    while (errVal == PFE_OK) {
        hdr = (SlottedPageHeader *)pageBuf;
        slots = (SlotEntry *)(pageBuf + sizeof(SlottedPageHeader));

        /* Check if a tombstone slot can be reused */
        int i;
        targetSlot = -1;
        for (i = 0; i < hdr->numSlots; i++) {
            if (slots[i].offset == SLOT_TOMBSTONE || slots[i].offset == SLOT_UNUSED) {
                targetSlot = i;
                break;
            }
        }

        requiredSpace = (targetSlot != -1) ? recordLen : (recordLen + sizeof(SlotEntry));
        if (hdr->freeBytes >= requiredSpace) {
            break; /* Suitable page found */
        }

        int prevPage = pageNum;
        errVal = PF_GetNextPage(fileDesc, &pageNum, &pageBuf);
        PF_UnfixPage(fileDesc, prevPage, 0);
    }

    /* Allocate new page if no existing page has enough free space */
    if (errVal != PFE_OK) {
        errVal = PF_AllocPage(fileDesc, &pageNum, &pageBuf);
        if (errVal != PFE_OK) return errVal;

        hdr = (SlottedPageHeader *)pageBuf;
        hdr->numSlots = 0;
        hdr->freeSpaceOffset = PF_PAGE_SIZE;
        hdr->freeBytes = PF_PAGE_SIZE - sizeof(SlottedPageHeader);
        slots = (SlotEntry *)(pageBuf + sizeof(SlottedPageHeader));
        targetSlot = -1;
    }

    /* Determine slot index */
    if (targetSlot == -1) {
        targetSlot = hdr->numSlots;
        hdr->numSlots++;
        hdr->freeBytes -= sizeof(SlotEntry);
    }

    /* Compact page if contiguous free space is smaller than record length */
    int contiguousFree = hdr->freeSpaceOffset - (sizeof(SlottedPageHeader) + hdr->numSlots * sizeof(SlotEntry));
    if (contiguousFree < recordLen) {
        RM_CompactPage(pageBuf);
    }

    /* Place record data */
    hdr->freeSpaceOffset -= recordLen;
    memcpy(pageBuf + hdr->freeSpaceOffset, recordData, recordLen);
    
    slots[targetSlot].offset = hdr->freeSpaceOffset;
    slots[targetSlot].length = recordLen;
    hdr->freeBytes -= recordLen;

    rid->pageNum = pageNum;
    rid->slotNum = targetSlot;

    PF_UnfixPage(fileDesc, pageNum, 1); /* Mark dirty */
    return PFE_OK;
}

/* Delete record by setting a tombstone flag */
int RM_DeleteRecord(int fileDesc, RID rid) {
    char *pageBuf = NULL;
    int errVal = PF_GetThisPage(fileDesc, rid.pageNum, &pageBuf);
    if (errVal != PFE_OK) return errVal;

    SlottedPageHeader *hdr = (SlottedPageHeader *)pageBuf;
    SlotEntry *slots = (SlotEntry *)(pageBuf + sizeof(SlottedPageHeader));

    if (rid.slotNum < 0 || rid.slotNum >= hdr->numSlots) {
        PF_UnfixPage(fileDesc, rid.pageNum, 0);
        return -1;
    }

    if (slots[rid.slotNum].offset > 0) {
        hdr->freeBytes += slots[rid.slotNum].length;
        slots[rid.slotNum].offset = SLOT_TOMBSTONE;
        slots[rid.slotNum].length = 0;
    }

    PF_UnfixPage(fileDesc, rid.pageNum, 1);
    return PFE_OK;
}

/* Retrieve record data by RID */
int RM_GetRecord(int fileDesc, RID rid, char *recordData, int *recordLen) {
    char *pageBuf = NULL;
    int errVal = PF_GetThisPage(fileDesc, rid.pageNum, &pageBuf);
    if (errVal != PFE_OK) return errVal;

    SlottedPageHeader *hdr = (SlottedPageHeader *)pageBuf;
    SlotEntry *slots = (SlotEntry *)(pageBuf + sizeof(SlottedPageHeader));

    if (rid.slotNum < 0 || rid.slotNum >= hdr->numSlots || slots[rid.slotNum].offset <= 0) {
        PF_UnfixPage(fileDesc, rid.pageNum, 0);
        return -1;
    }

    *recordLen = slots[rid.slotNum].length;
    memcpy(recordData, pageBuf + slots[rid.slotNum].offset, *recordLen);
    recordData[*recordLen] = '\0';

    PF_UnfixPage(fileDesc, rid.pageNum, 0);
    return PFE_OK;
}

/* Initialize sequential scan */
int RM_OpenScan(int fileDesc, RM_ScanDesc *scanDesc) {
    scanDesc->fileDesc = fileDesc;
    scanDesc->currentPageNum = -1;
    scanDesc->currentSlotNum = -1;
    return PFE_OK;
}

/* Advance sequential scan to next active record */
int RM_GetNextRecord(RM_ScanDesc *scanDesc, RID *rid, char *recordData, int *recordLen) {
    char *pageBuf = NULL;
    int errVal;

    if (scanDesc->currentPageNum == -1) {
        errVal = PF_GetFirstPage(scanDesc->fileDesc, &scanDesc->currentPageNum, &pageBuf);
        scanDesc->currentSlotNum = 0;
    } else {
        errVal = PF_GetThisPage(scanDesc->fileDesc, scanDesc->currentPageNum, &pageBuf);
    }

    while (errVal == PFE_OK) {
        SlottedPageHeader *hdr = (SlottedPageHeader *)pageBuf;
        SlotEntry *slots = (SlotEntry *)(pageBuf + sizeof(SlottedPageHeader));

        while (scanDesc->currentSlotNum < hdr->numSlots) {
            int slotIdx = scanDesc->currentSlotNum++;
            if (slots[slotIdx].offset > 0) {
                rid->pageNum = scanDesc->currentPageNum;
                rid->slotNum = slotIdx;
                *recordLen = slots[slotIdx].length;
                memcpy(recordData, pageBuf + slots[slotIdx].offset, *recordLen);
                recordData[*recordLen] = '\0';
                PF_UnfixPage(scanDesc->fileDesc, scanDesc->currentPageNum, 0);
                return PFE_OK;
            }
        }

        int prevPage = scanDesc->currentPageNum;
        errVal = PF_GetNextPage(scanDesc->fileDesc, &scanDesc->currentPageNum, &pageBuf);
        PF_UnfixPage(scanDesc->fileDesc, prevPage, 0);
        scanDesc->currentSlotNum = 0;
    }

    return PFE_EOF;
}

int RM_CloseScan(RM_ScanDesc *scanDesc) {
    scanDesc->currentPageNum = -1;
    scanDesc->currentSlotNum = -1;
    return PFE_OK;
}