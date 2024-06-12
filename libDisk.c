#include "libDisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int diskCounter = 1;
Disk *diskListHead = NULL;

int openDisk(char *filename, int nBytes) {
    FILE *fp = NULL;
    int fileSize = 0;
    Disk *newDisk = NULL;
    char *filenameCopy = NULL;

    if (nBytes == 0) {
        if ((fp = fopen(filename, "r+")) == NULL) {
            printf("The file should have existed but was not found. (LibDisk.c)\n");
            return -1;
        }

        fseek(fp, 0, SEEK_END);
        fileSize = ftell(fp);

        if (fileSize % BLOCKSIZE != 0) {
            printf("File size is not a multiple of the block size. (LibDisk.c)\n");
            fclose(fp);
            return -1;
        }

        fseek(fp, 0, SEEK_SET);
        nBytes = fileSize;

    } else {
        if (nBytes < BLOCKSIZE) {
            printf("The number of bytes must be at least the size of a block. (LibDisk.c)\n");
            return -1;
        }

        if (nBytes % BLOCKSIZE != 0) {
            nBytes -= nBytes % BLOCKSIZE;
        }

        if ((fp = fopen(filename, "w")) == NULL) {
            printf("An error occurred while opening the file. (LibDisk.c)\n");
            return -1;
        }

        char zero = 0;
        for (int i = 0; i < nBytes; i++) {
            fwrite(&zero, sizeof(char), 1, fp);
        }
    }

    if ((newDisk = malloc(sizeof(Disk))) == NULL) {
        printf("Failed to allocate memory for the new disk. (LibDisk.c)\n");
        fclose(fp);
        return -1;
    }

    if ((filenameCopy = malloc(strlen(filename) + 1)) == NULL) {
        printf("Failed to allocate memory for the filename. (LibDisk.c)\n");
        free(newDisk);
        fclose(fp);
        return -1;
    }

    strcpy(filenameCopy, filename);

    newDisk->diskNumber = diskCounter++;
    newDisk->filename = filenameCopy;
    newDisk->nBytes = nBytes;
    newDisk->next = diskListHead;
    newDisk->filePointer = fp;
    diskListHead = newDisk;

    return newDisk->diskNumber;
}

int closeDisk(int disk) {
    Disk *currentDisk = diskListHead;
    Disk *previousDisk = NULL;

    while (currentDisk != NULL) {
        if (currentDisk->diskNumber == disk) {
            if (fclose(currentDisk->filePointer) != 0) {
                printf("An error occurred while closing the file. (LibDisk.c)\n");
                return -1;
            }
            if (previousDisk == NULL) {
                diskListHead = currentDisk->next;
            } else {
                previousDisk->next = currentDisk->next;
            }
            free(currentDisk->filename);
            free(currentDisk);
            return 0;
        }
        previousDisk = currentDisk;
        currentDisk = currentDisk->next;
    }

    printf("The specified disk was not found. (LibDisk.c)\n");
    return -1;
}

int readBlock(int disk, int bNum, void *block) {
    Disk *currentDisk = diskListHead;

    while (currentDisk != NULL) {
        if (currentDisk->diskNumber == disk) {
            if (bNum < 0 || bNum >= currentDisk->nBytes / BLOCKSIZE) {
                printf("The block number is out of range. (LibDisk.c)\n");
                return -1;
            }
            FILE *fp = currentDisk->filePointer;
            if (fseek(fp, bNum * BLOCKSIZE, SEEK_SET) != 0) {
                printf("An error occurred while seeking to the position. (LibDisk.c)\n");
                return -1;
            }
            if (fread(block, sizeof(char), BLOCKSIZE, fp) != BLOCKSIZE) {
                printf("An error occurred while reading the block. (LibDisk.c)\n");
                return -1;
            }
            return 0;
        }
        currentDisk = currentDisk->next;
    }

    printf("The specified disk was not found. (LibDisk.c)\n");
    return -1;
}

int writeBlock(int disk, int bNum, void *block) {
    Disk *currentDisk = diskListHead;

    while (currentDisk != NULL) {
        if (currentDisk->diskNumber == disk) {
            if (bNum < 0 || bNum >= currentDisk->nBytes / BLOCKSIZE) {
                printf("The block number is out of range. (LibDisk.c)\n");
                return -1;
            }
            FILE *fp = currentDisk->filePointer;
            if (fseek(fp, bNum * BLOCKSIZE, SEEK_SET) != 0) {
                printf("An error occurred while seeking to the position. (LibDisk.c)\n");
                return -1;
            }
            if (fwrite(block, sizeof(char), BLOCKSIZE, fp) != BLOCKSIZE) {
                printf("An error occurred while writing the block. (LibDisk.c)\n");
                return -1;
            }
            return 0;
        }
        currentDisk = currentDisk->next;
    }

    printf("The specified disk was not found. (LibDisk.c)\n");
    return -1;
}
