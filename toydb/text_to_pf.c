#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pflayer/pf.h"

#define INPUT_CHUNK_SIZE PF_PAGE_SIZE

int main(int argc, char *argv[])
{
    FILE *input;
    int fd;
    int page_no;
    int status;
    int total_pages = 0;
    char *pagebuf;
    size_t bytes_read;

    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s input.txt output.data\n",
                argv[0]);
        return 1;
    }

    input = fopen(argv[1], "rb");

    if (input == NULL) {
        perror("Cannot open input file");
        return 1;
    }

    status = PF_CreateFile(argv[2]);

    if (status != PFE_OK) {
        fprintf(stderr,
                "PF_CreateFile failed: %d\n",
                status);
        fclose(input);
        return 1;
    }

    fd = PF_OpenFile(argv[2]);

    if (fd < 0) {
        fprintf(stderr,
                "PF_OpenFile failed: %d\n",
                fd);
        fclose(input);
        return 1;
    }

    while (1) {
        status = PF_AllocPage(fd, &page_no, &pagebuf);

        if (status != PFE_OK) {
            fprintf(stderr,
                    "PF_AllocPage failed: %d\n",
                    status);
            break;
        }

        memset(pagebuf, 0, PF_PAGE_SIZE);

        bytes_read = fread(
            pagebuf,
            1,
            INPUT_CHUNK_SIZE,
            input
        );

        if (bytes_read == 0) {
            PF_UnfixPage(fd, page_no, 0);
            break;
        }

        status = PF_UnfixPage(fd, page_no, 1);

        if (status != PFE_OK) {
            fprintf(stderr,
                    "PF_UnfixPage failed: %d\n",
                    status);
            break;
        }

        total_pages++;

        if (bytes_read < INPUT_CHUNK_SIZE) {
            break;
        }
    }

    fclose(input);

    status = PF_CloseFile(fd);

    if (status != PFE_OK) {
        fprintf(stderr,
                "PF_CloseFile failed: %d\n",
                status);
        return 1;
    }

    printf("Pages created: %d\n", total_pages);
    printf("Output file: %s\n", argv[2]);

    return 0;
}
