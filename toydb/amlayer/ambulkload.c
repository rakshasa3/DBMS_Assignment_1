#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"
#include "am.h"

static int g_attrType;
static int g_attrLength;

/* Key Comparison Function for qsort */
static int compare_entries(const void *a, const void *b) {
    const BulkEntry *ea = (const BulkEntry *)a;
    const BulkEntry *eb = (const BulkEntry *)b;

    if (g_attrType == 'i') {
        int valA = atoi(ea->key);
        int valB = atoi(eb->key);
        return (valA - valB);
    } else if (g_attrType == 'f') {
        float valA = (float)atof(ea->key);
        float valB = (float)atof(eb->key);
        if (valA < valB) return -1;
        if (valA > valB) return 1;
        return 0;
    } else {
        return strncmp(ea->key, eb->key, g_attrLength);
    }
}

/* Bottom-Up Bulk Load Implementation */
int AM_BulkLoad(int fileDesc, int attrType, int attrLength, BulkEntry *entries, int numEntries, float fillFactor) {
    int i;
    int leafPageNum, prevLeafPageNum = -1;
    char *leafBuf;
    AM_LEAFHEADER *leafHdr;
    
    int *currentLevelPages = NULL;
    char **currentLevelKeys = NULL;
    int currentLevelCount = 0;
    
    if (numEntries <= 0) return AME_OK;
    if (fillFactor <= 0.0f || fillFactor > 1.0f) fillFactor = 1.0f;

    g_attrType = attrType;
    g_attrLength = attrLength;

    /* Phase 1: Sort entries in-memory */
    qsort(entries, numEntries, sizeof(BulkEntry), compare_entries);

    /* Determine capacity per leaf node based on fillFactor */
    int maxKeysPerLeaf = (PF_PAGE_SIZE - AM_sl) / (attrLength + AM_ss);
    int targetKeysPerLeaf = (int)(maxKeysPerLeaf * fillFactor);
    if (targetKeysPerLeaf < 1) targetKeysPerLeaf = 1;

    /* Phase 2: Create Leaf Pages Bottom-Up */
    int numLeafPages = (numEntries + targetKeysPerLeaf - 1) / targetKeysPerLeaf;
    currentLevelPages = (int *)malloc(numLeafPages * sizeof(int));
    currentLevelKeys = (char **)malloc(numLeafPages * sizeof(char *));

    int entryIdx = 0;
    for (i = 0; i < numLeafPages; i++) {
        if (PF_AllocPage(fileDesc, &leafPageNum, &leafBuf) != PFE_OK) {
            return AME_PF;
        }

        leafHdr = (AM_LEAFHEADER *)leafBuf;
        leafHdr->pageType = 'l';
        leafHdr->nextLeafPage = AM_NULL_PAGE;
        leafHdr->numKeys = 0;
        leafHdr->attrLength = attrLength;
        leafHdr->maxKeys = maxKeysPerLeaf;

        /* Link previous leaf to current leaf */
        if (prevLeafPageNum != -1) {
            char *prevBuf;
            if (PF_GetThisPage(fileDesc, prevLeafPageNum, &prevBuf) == PFE_OK) {
                AM_LEAFHEADER *prevHdr = (AM_LEAFHEADER *)prevBuf;
                prevHdr->nextLeafPage = leafPageNum;
                PF_UnfixPage(fileDesc, prevLeafPageNum, 1);
            }
        }

        /* Pack keys into current leaf node */
        int keysToPack = targetKeysPerLeaf;
        if (entryIdx + keysToPack > numEntries) {
            keysToPack = numEntries - entryIdx;
        }

        /* Record boundary key for upper index level */
        currentLevelKeys[i] = (char *)malloc(attrLength + 1);
        memcpy(currentLevelKeys[i], entries[entryIdx].key, attrLength);
        currentLevelKeys[i][attrLength] = '\0';
        currentLevelPages[i] = leafPageNum;

        int k;
        for (k = 0; k < keysToPack; k++) {
            AM_LEAFHEADER *lHdr = (AM_LEAFHEADER *)leafBuf;
            char *keyPtr = leafBuf + AM_sl + lHdr->numKeys * attrLength;
            short *recPtr = (short *)(leafBuf + PF_PAGE_SIZE - (lHdr->numKeys + 1) * AM_ss);

            memcpy(keyPtr, entries[entryIdx].key, attrLength);
            recPtr[0] = (short)entries[entryIdx].pageNum;
            recPtr[1] = (short)entries[entryIdx].slotNum;

            lHdr->numKeys++;
            entryIdx++;
        }

        PF_UnfixPage(fileDesc, leafPageNum, 1);
        prevLeafPageNum = leafPageNum;
    }
    currentLevelCount = numLeafPages;

    /* Phase 3: Recursively Build Parent Index Levels */
    while (currentLevelCount > 1) {
        int maxKeysPerInt = (PF_PAGE_SIZE - AM_sint) / (attrLength + sizeof(int));
        int targetKeysPerInt = (int)(maxKeysPerInt * fillFactor);
        if (targetKeysPerInt < 1) targetKeysPerInt = 1;

        int parentCount = (currentLevelCount + targetKeysPerInt - 1) / targetKeysPerInt;
        int *nextLevelPages = (int *)malloc(parentCount * sizeof(int));
        char **nextLevelKeys = (char **)malloc(parentCount * sizeof(char *));

        int childIdx = 0;
        for (i = 0; i < parentCount; i++) {
            int intPageNum;
            char *intBuf;
            if (PF_AllocPage(fileDesc, &intPageNum, &intBuf) != PFE_OK) {
                return AME_PF;
            }

            AM_INTHEADER *intHdr = (AM_INTHEADER *)intBuf;
            intHdr->pageType = 'i';
            intHdr->numKeys = 0;
            intHdr->maxKeys = maxKeysPerInt;
            intHdr->attrLength = attrLength;

            int keysToPack = targetKeysPerInt;
            if (childIdx + keysToPack > currentLevelCount) {
                keysToPack = currentLevelCount - childIdx;
            }

            nextLevelKeys[i] = (char *)malloc(attrLength + 1);
            memcpy(nextLevelKeys[i], currentLevelKeys[childIdx], attrLength);
            nextLevelKeys[i][attrLength] = '\0';
            nextLevelPages[i] = intPageNum;

            int k;
            for (k = 0; k < keysToPack; k++) {
                char *keyPtr = intBuf + AM_sint + k * attrLength;
                int *ptrPtr = (int *)(intBuf + AM_sint + maxKeysPerInt * attrLength + k * sizeof(int));

                memcpy(keyPtr, currentLevelKeys[childIdx], attrLength);
                *ptrPtr = currentLevelPages[childIdx];
                intHdr->numKeys++;
                childIdx++;
            }

            PF_UnfixPage(fileDesc, intPageNum, 1);
        }

        for (i = 0; i < currentLevelCount; i++) free(currentLevelKeys[i]);
        free(currentLevelKeys);
        free(currentLevelPages);

        currentLevelKeys = nextLevelKeys;
        currentLevelPages = nextLevelPages;
        currentLevelCount = parentCount;
    }

    /* Set final root page in file header */
    char *hdrBuf;
    if (PF_GetFirstPage(fileDesc, &leafPageNum, &hdrBuf) == PFE_OK) {
        int *rootPtr = (int *)(hdrBuf + sizeof(int));
        *rootPtr = currentLevelPages[0];
        PF_UnfixPage(fileDesc, leafPageNum, 1);
    }

    free(currentLevelKeys[0]);
    free(currentLevelKeys);
    free(currentLevelPages);

    return AME_OK;
}
