#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pf.h"
#include "pftypes.h"

int main()
{
    FILE *input;

    int fd;
    int pagenum;
    int bytes_read;

    char buffer[PF_PAGE_SIZE];
    char *pagebuf;

    input = fopen("../../data/courses.txt", "rb");

    if (input == NULL) {
        perror("courses.txt");
        return 1;
    }

    PF_Init();

    PF_CreateFile("courses.db");

    fd = PF_OpenFile("courses.db");

    if (fd < 0) {
        PF_PrintError("Open failed");
        return 1;
    }

    while (1) {

        memset(buffer, 0, PF_PAGE_SIZE);

        bytes_read = fread(
            buffer,
            1,
            PF_PAGE_SIZE,
            input
        );

        if (bytes_read == 0)
            break;

        if (PF_AllocPage(
                fd,
                &pagenum,
                &pagebuf
            ) != PFE_OK) {

            PF_PrintError("Page allocation failed");
            return 1;
        }

        memcpy(
            pagebuf,
            buffer,
            PF_PAGE_SIZE
        );

        PF_UnfixPage(fd, pagenum, TRUE);
    }

    fclose(input);

    PF_CloseFile(fd);

    printf("Courses successfully loaded into PF database.\n");

    return 0;
}
