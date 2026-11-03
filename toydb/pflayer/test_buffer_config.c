#include <stdio.h>
#include "pf.h"
#include "pftypes.h"

int main()
{
    int fd;
    int i, pagenum;
    char *pagebuf;

    PF_Init();

    if (PF_SetBufferSize(5) != PFE_OK) {
        PF_PrintError("Buffer configuration failed");
        return 1;
    }

    PF_CreateFile("buffer_test.db");

    fd = PF_OpenFile("buffer_test.db");

    for (i = 0; i < 10; i++) {

        if (PF_AllocPage(fd, &pagenum, &pagebuf) != PFE_OK) {
            PF_PrintError("Allocation failed");
            return 1;
        }

        sprintf(pagebuf, "Page number %d", i);

        PF_UnfixPage(fd, pagenum, TRUE);
    }

    printf("Successfully allocated 10 pages "
           "with a buffer pool of 5 pages.\n");

    PF_CloseFile(fd);

    PF_DestroyFile("buffer_test.db");

    return 0;
}
