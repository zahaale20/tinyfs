#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libTinyFS.h"
#include "tinyFS_errno.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

void printInHexadecimal(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }
        size_t bytesRead;
    unsigned long offset = 0;
    unsigned char buffer[16];

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        printf("%08lx: ", offset);
        for (size_t i = 0; i < 16; ++i) {
            if (i < bytesRead) {
                printf("%02x ", buffer[i]);
            } else {
                printf("   ");
            }
            if (i == 7) {
                printf(" ");
            }
        }
        printf(" |");
        for (size_t i = 0; i < bytesRead; ++i) {
            if (isprint(buffer[i])) {
                printf("%c", buffer[i]);
            } else {
                printf(".");
            }
        }
        printf("|\n");
        offset += bytesRead;
    }
    fclose(file);
}

int main() {
    char *btcwhitepaper = (char *)malloc(sizeof(char)*169);
    memcpy(btcwhitepaper, "A purely peer-to-peer version of electronic cash would allow online payments to be sent directly from one party to another without going through a financial institution.", 169);
    int status;
    fileDescriptor fd1, fd2, fd3, fd4, fd5, fd6, fd7, fd8;

    // The program attempts to mount test.dsk but fails as the file doesn't exist
    if (tfs_mount("test.dsk") < 0) {
        perror("test.dsk not found, creating new disk");
        tfs_mkfs("test.dsk", 10240);
        if (tfs_mount("test.dsk") < 0) {
            perror("Failed to mount disk");
            return 1;
        }
    }
    printf("Initial mounting phase completed\n");


    // : The program opens eight files (file1 to file8) and prints their file descriptors.
    fd1 = tfs_openFile("file1");
    fd2 = tfs_openFile("file2");
    fd3 = tfs_openFile("file3");
    fd4 = tfs_openFile("file4");
    fd5 = tfs_openFile("file5");
    fd6 = tfs_openFile("file6");
    fd7 = tfs_openFile("file7");
    fd8 = tfs_openFile("file8");
    printf("File descriptors: \n%d, %d, %d, %d, %d, %d, %d, %d\n", fd1, fd2, fd3, fd4, fd5, fd6, fd7, fd8);
    
    // The string "I am sentient!" is written to file1
    printf("\nWriting to file1\n");
    if (tfs_writeFile(fd1, "Iam sentiend!", strlen("I am sentiend!")) < 0) {
        return -1;
    }
    printf("Wrote 'I am sentient!' to file1\n");


    // The first character 'I' is read from file1, and the file pointer is now at position 1.
    printf("\nReading first character of file1\n");
    char *oneByte = (char *)malloc(sizeof(char));
    if (tfs_readByte(fd1, oneByte) < 0) {
        printf("Read 1\n");
        return -1;
    }
    printf("%c\n", *oneByte);
    printf("\nFile pointer of file1 is now at 1\n");

    printf("\nSeeking file pointer for last chracter\n");
    if(tfs_seek(fd1, 11) < 0) {
        return -1;
    }
    //The file pointer is moved to the last character, which is then read and displayed as '!'.
    printf("\nReading last character of file1\n");
    if (tfs_readByte(fd1, oneByte) < 0) {
        printf("Read 2\n");
        return -1;
    }
    printf("%c\n", *oneByte);

    printf("\nWriting to file1 again\n");
    if(tfs_writeFile(fd1, "hello", strlen("hello")) < 0) {
        return -1;
    }
    printf("Wrote 'hello' to file1\n");


    // Continuous read to verify content and writing a large text to another file
    printf("Reading all of file1\n");
    while(tfs_readByte(fd1, oneByte) > 0) {
        printf("%c", *oneByte);
    }
    if (tfs_writeFile(fd4, btcwhitepaper, 169) < 0) {
        return -1;
    }
    //The Bitcoin whitepaper text is written to file4. The first 48 characters are read and displayed.
    printf("\nWrote btcwhitepaper to file4.\n");

    // Simulating reading operations to verify file content
    int it = 48;
    while(it != 0) {
        tfs_readByte(fd4, oneByte);
        printf("%c", *oneByte);
        it--;
    }
    // The file pointer for file4 is reset to the beginning, and the first character 'r' is read.
    printf("\nSeeking file pointer to first block\n");
    if(tfs_seek(fd4, 0) < 0) {
        return -1;
    }
    tfs_readByte(fd4, oneByte);
    printf("%c\n", *oneByte);

    // Writing and then checking if the seek function handles boundary conditions correctly
    // The string "abc" is written to file4, and attempting to read past this new length results in an EOF error, confirming data overwriting.
    printf("\nWriting to file4 again\n");
    if(tfs_writeFile(fd4, "abc", strlen("abc")) < 0) {
        return -1;
    }
    printf("Wrote 'abc' to file4\n");
    printf("\nSeeking file pointer past new write length\n");
    if(tfs_seek(fd4, 5) < 0) {
        return -1;
    }
    if(tfs_readByte(fd4, oneByte) < 0) {
        printf("Data overwritten correctly\n");
    }

    // attempt to rename file2 to mainfile.txt fails due to length constraints. Renaming it to main.c succeeds.
    printf("\nAttempting to rename file2 to mainfile.txt\n");
    if (tfs_rename(fd2, "mainfile.txt") < 0) {
        printf("Renaming to mainfile.txt failed\n");
        printf("Attempting to rename file2 to main.c\n");
        if (tfs_rename(fd2, "main.c") > 0) {
            printf("Renamed file2 to main.c\n\n");
        }
        else {
            printf("Renaming to main.c failed\n");
        }
    }
    else {
        printf("\nRenaming to mainfile.txt succeeded unexpectedly\n");
    }

    // The program tries to reopen file1 while it's already open, which fails. After closing file1, reopening it succeeds.
    if (tfs_openFile("file1") < 0) {
        printf("Opening file1 failed as expected, since it is already open\n");
    }

    if (tfs_closeFile(fd1) < 0) {
        printf("Closing file1 failed unexpectedly\n");
        return 1;
    }
    printf("Closed file1\n");
    printf("Attempting to open file1 again...\n");
    fd1 = tfs_openFile("file1");
    if (fd1 < 0) {
        printf("Opening file1 after closing failed unexpectedly\n");
        return 1;
    }
    printf("Opened file1 successfully\n");

    // The current files in the root directory are listed.
    printf("\nCurrent Working Directory...\n");
    if(tfs_readdir() < 0) {
        return -1;
    }

    tfs_deleteFile(fd3);
    printf("Test delete\n");
    // file3 is deleted and the updated directory listing is shown
    printf("\nCurrent Working Directory...\n");
    if(tfs_readdir() < 0) {
        return -1;
    }

    printf("Retrieving file info...\n");
    // Metadata for file1 is displayed, including size, creation, modification, and access times.
    tfs_readFileInfo(fd1);
    tfs_readFileInfo(fd2);
    tfs_readFileInfo(fd3);
    tfs_readFileInfo(fd4);
    tfs_readFileInfo(fd5);
    tfs_readFileInfo(fd6);
    tfs_readFileInfo(fd7);
    tfs_readFileInfo(fd8);

    //File descriptors remain the same after deleting and reopening file1
    printf("File descriptors before delete: \n%d, %d, %d, %d, %d, %d, %d, %d\n", fd1, fd2, fd3, fd4, fd5, fd6, fd7, fd8);
    if (tfs_deleteFile(fd1) < 0) {
        printf("Deleting file1 failed\n");
        return 1;
    }
    fd1 = tfs_openFile("file1");
    if (fd1 < 0) {
        printf("Opening file1 after deleting failed unexpectedly\n");
        return 1;
    }
    printf("File descriptors after delete and reopen (should be the same): \n%d, %d, %d, %d, %d, %d, %d, %d\n", fd1, fd2, fd3, fd4, fd5, fd6, fd7, fd8);


    // Testing unmounting and remounting the file system, for persistence
    if (tfs_unmount() < 0) {
        printf("Unmounting failed\n");
        return 1;
    }
    if (tfs_mount("test.dsk") < 0) {
        printf("Remounting failed\n");
        return 1;
    }
    
    // Metadata for file4 is displayed.
    fd4 = tfs_openFile("file4");
    if (tfs_deleteFile(fd4) < 0) {
        printf("Opening file4 after 2nd mount failed unexpectedly\n");
        return 1;
    }
    //The demo concludes by unmounting and deleting test.dsk, confirming successful completion of all operations.
    printf("Demo completed\n");
    status = remove("test.dsk");
    if (status == 0) {
        printf("File deleted successfully!\n");
    } 
    else {
        printf("Error deleting the file\n");
    }
    return 0;
}