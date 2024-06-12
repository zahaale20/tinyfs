#ifndef libDisk_h
#define libDisk_h
#define BLOCKSIZE 256
#include <stdio.h>

typedef struct Disk Disk;
struct Disk {
    int diskNumber;
    int nBytes;
    char *filename;
    Disk *next;
    FILE *filePointer;
};

extern int diskCounter;
extern Disk *diskListHead;

int openDisk(char *filename, int nBytes);
int closeDisk(int disk);
int readBlock(int disk, int bNum, void *block);
int writeBlock(int disk, int bNum, void *block);
#endif
