#include "libTinyFS.h"
#include "libDisk.h"
#include "tinyFS_errno.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h> 

fileDescriptorTableEntry **fileDescriptorTable = NULL;
int activeDisk = 0;
int maxNumberOfFiles = 0;

/* Makes a blank TinyFS file system of size nBytes on the unix file
specified by ‘filename’. This function should use the emulated disk
library to open the specified unix file, and upon success, format the
file to be a mountable disk. This includes initializing all data to 0x00,
setting magic numbers, initializing and writing the superblock and
inodes, etc. Must return a specified success/error code. */

int tfs_mkfs(char *filename, int nBytes) {
    // Check for valid size parameters first
    if (nBytes < 0 || nBytes > MAX_BYTES) {
        printf("File system size out of range\n");
        return FS_CREATION_ERROR;
    }

    // Calculate total blocks and check if they're insufficient
    int totalBlocks = (nBytes / BLOCKSIZE) - 1;
    if (totalBlocks < 3) {
        printf("File system size too small\n");
        return FS_CREATION_ERROR;
    }

    // Attempt to create the disk and verify successful creation
    int diskID = openDisk(filename, nBytes);
    if (diskID < 0) {
        printf("Failed to create disk\n");
        return FS_CREATION_ERROR;
    }

    // Calculate maximum number of files supported
    int fileLimit = totalBlocks / 2;
    if (fileLimit < 1) {
        printf("Not enough blocks for metadata\n");
        return FS_CREATION_ERROR;
    }

    // Initialize super block
    char *superBlock = (char *)malloc(BLOCKSIZE);

    if (!superBlock) {
        printf("Memory allocation failed\n");
        return FS_CREATION_ERROR;
    }

    memset(superBlock, 0, BLOCKSIZE);
    superBlock[BLOCK_NUMBER_OFFSET] = 1;
    superBlock[MAGIC_NUMBER_OFFSET] = MAGIC_NUMBER;
    uint32_t firstFreeBlock = 2;  // Start of free blocks after superblock and root directory
    *((uint32_t *)(superBlock + 2)) = firstFreeBlock;
    memcpy(superBlock + SUPER_MAX_NUM_FILES_OFFSET, &fileLimit, sizeof(int));

    // Write super block to disk
    int result = writeBlock(diskID, 0, superBlock);
    free(superBlock);  // Free immediately after use
    if (result < 0) {
        printf("Error writing super block to disk\n");
        return FS_CREATION_ERROR;
    }

    // Initialize all other blocks
    for (int i = 1; i <= totalBlocks; i++) {
        char *blockData = (char *)malloc(BLOCKSIZE);
        if (!blockData) {
            printf("Memory allocation failed for block %d\n", i);
            continue; // Proceed to attempt next block allocation
        }
        memset(blockData, 0, BLOCKSIZE);
        blockData[BLOCK_NUMBER_OFFSET] = (i < totalBlocks) ? 4 : 0; // Mark last block differently if needed
        blockData[MAGIC_NUMBER_OFFSET] = MAGIC_NUMBER;
        uint32_t nextBlock = (i < totalBlocks) ? i + 1 : 0;
        *((uint32_t *)(blockData + 2)) = nextBlock;

        result = writeBlock(diskID, i, blockData);
        free(blockData); // Free immediately after use
        if (result < 0) {
            printf("Failed to write block %d to disk\n", i);
            return FS_CREATION_ERROR;
        }
    }
    return 1;
}

/* tfs_mount(char *diskname) “mounts” a TinyFS file system located within
‘diskname’. tfs_unmount(void) “unmounts” the currently mounted file
system. As part of the mount operation, tfs_mount should verify the file
system is the correct type. In tinyFS, only one file system may be
mounted at a time. Use tfs_unmount to cleanly unmount the currently
mounted file system. Must return a specified success/error code. */

int tfs_mount(char *diskname) {

    // Check if there is already a disk mounted
    if (activeDisk != 0) {
        printf("A disk is already mounted, unmount current\ndisk to mount a new disk\n");
        return FS_MOUNT_ERROR;
    }

    // Attempt to open the disk specified by 'diskname'
    activeDisk = openDisk(diskname, 0);
    if (activeDisk == -1) {
        activeDisk = 0;
        printf("Could not open disk\n");
        return FS_MOUNT_ERROR;
    }

    // Read the super block to fetch fs metadata
    char *superData = (char *)malloc(BLOCKSIZE);
    int success = readBlock(activeDisk, SUPER_BLOCK, superData);
    if (success < 0) {
        printf("Issue with super block read when mounting disk\n");
        return FS_MOUNT_ERROR;
    }

    // Retrieve the maximum number of files supported by this file system from the super block
    memcpy(&maxNumberOfFiles, superData + SUPER_MAX_NUM_FILES_OFFSET, sizeof(int));


    char *data = (char *)malloc(BLOCKSIZE * sizeof(char));
    int i = 0;

    // Check every block in the disk to validate type and magic number
    while (readBlock(activeDisk, i, data) < 0) {
        if (data[BLOCK_NUMBER_OFFSET] <= 0 || data[BLOCK_NUMBER_OFFSET] > 4) {
            printf("Invalid block type\n");
            return FS_MOUNT_ERROR;
        }

        if (data[MAGIC_NUMBER_OFFSET] != MAGIC_NUMBER) {
            printf("Invalid magic number\n");
            return FS_MOUNT_ERROR;
        }
    }

    // Allocate memory
    fileDescriptorTable = (fileDescriptorTableEntry **)malloc(maxNumberOfFiles * sizeof(fileDescriptorTableEntry *));
    if (fileDescriptorTable == NULL) {
        printf("Could not allocate memory for open file table\n");
        return FS_MOUNT_ERROR;
    }

    for (i = 0; i < maxNumberOfFiles; i++) {
        fileDescriptorTable[i] = NULL;
    }

    free(data);
    return activeDisk;
}

int tfs_unmount(void) {
    // Check if there is an active disk to unmount
    if (activeDisk == 0) {
        printf("No disk to unmount\n");
        return FS_UNMOUNT_ERROR;
    }
    activeDisk = 0;

    // Iterate through the file descriptor table to free any open file descriptors
    for (int i = 0; i < maxNumberOfFiles; i++) {
        if (fileDescriptorTable[i] != NULL) {
            free(fileDescriptorTable[i]);
            fileDescriptorTable[i] = NULL;
        }
    }

    // Free memory
    free(fileDescriptorTable);
    fileDescriptorTable = NULL;

    return 1;
}

int getTimestamp(char *buffer, size_t bufferSize) {
    // Retrieve the current time
    time_t now;
    time(&now);

    // Convert the time_t value to local time
    struct tm *localTime = localtime(&now);

    // Format time ex: 2024-06-03 11:44:48
    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", localTime);

    return 1;
}

int tfs_readFileInfo(fileDescriptor fileDescriptor) {

    // Check if the file descriptor corresponds to an open file
    if (fileDescriptorTable[fileDescriptor] == NULL) {
        printf("File is not open. Cannot read file info\n");
        return FILE_OPEN_ERROR;
    }

    // Allocate memory to read the inode data associated with the file descriptor
    char *inodeBuffer = (char *)malloc(BLOCKSIZE);
    int success = readBlock(activeDisk, fileDescriptorTable[fileDescriptor]->inodeNumber, inodeBuffer);
    if (success < 0) {
        printf("Invalid pointer to inode block\n");
        return FILE_READ_ERROR;
    }

    // Allocate memory to store file name and timestamps
    char *fileName = (char *)malloc(MAX_FILE_NAME_SIZE);
    int fileSize;
    char *created = (char *)malloc(TIMESTAMP_BUFFER_SIZE);
    char *modified = (char *)malloc(TIMESTAMP_BUFFER_SIZE);
    char *accessed = (char *)malloc(TIMESTAMP_BUFFER_SIZE);

    // Copy file metadata from the inode into local variables
    memcpy(fileName, inodeBuffer + INODE_FILE_NAME_OFFSET, MAX_FILE_NAME_SIZE);
    memcpy(&fileSize, inodeBuffer + INODE_FILE_SIZE_OFFSET, sizeof(int));
    memcpy(created, inodeBuffer + INODE_CR8_TIME_STAMP_OFFSET, TIMESTAMP_BUFFER_SIZE);
    memcpy(modified, inodeBuffer + INODE_MOD_TIME_STAMP_OFFSET, TIMESTAMP_BUFFER_SIZE);
    memcpy(accessed, inodeBuffer + INODE_ACC_TIME_STAMP_OFFSET, TIMESTAMP_BUFFER_SIZE);

    // Display the file information
    printf("\n%s Information:", fileName);
    printf("\nFile Size: %d\n", fileSize);
    printf("Created: %s\n", created);
    printf("Modified: %s\n", modified);
    printf("Accessed: %s\n\n", accessed);

    // Free memory
    free(fileName);
    free(created);
    free(modified);
    free(accessed);
    return 1;
}

/* Creates or Opens a file for reading and writing on the currently
mounted file system. Creates a dynamic resource table entry for the file,
and returns a file descriptor (integer) that can be used to reference
this entry while the filesystem is mounted. */

fileDescriptor tfs_openFile(char *name) {

    // Allocate memory to read the super block data
    char *superData = (char *)malloc(BLOCKSIZE);

    // Read the super block to access file system metadata
    int success = readBlock(activeDisk, SUPER_BLOCK, superData);
    if (success < 0) {
        printf("Issue with super block read when opening file\n");
        return FILE_OPEN_ERROR;
    }

    // Retrieve the head of the inode list from the super block
    int inodeHead;
    memcpy(&inodeHead, superData + IB_OFFSET, sizeof(int));

    // If there are existing inodes, search for the file
    if (inodeHead != 0) {
        int inodeCurrent = inodeHead;
        char *inodeBuffer = (char *)malloc(BLOCKSIZE);
        char fileName[MAX_FILE_NAME_SIZE];

        // Loop through inodes to find the file
        while (1) {
            success = readBlock(activeDisk, inodeCurrent, inodeBuffer);
            if (success < 0) {
                printf("Invalid pointer to inode block\n");
                return FILE_OPEN_ERROR;
            }

            // Compare the file name stored in the inode with the provided name
            memcpy(fileName, inodeBuffer + INODE_FILE_NAME_OFFSET, MAX_FILE_NAME_SIZE * sizeof(char));
            if (strcmp(fileName, name) == 0) {
                // Check if the file is already open
                for (int i = 0; i < maxNumberOfFiles; i++) {
                    if (fileDescriptorTable[i] != NULL && fileDescriptorTable[i]->inodeNumber == inodeCurrent) {
                        printf("File is already open\n");
                        return FILE_OPEN_ERROR;
                    }
                }

                // Allocate a new entry for the open file table
                fileDescriptorTableEntry *newEntry = (fileDescriptorTableEntry *)malloc(sizeof(fileDescriptorTableEntry));

                if (newEntry == NULL) {
                    printf("Could not allocate memory for new open file table entry\n");
                    return FILE_OPEN_ERROR;
                }

                // Initialize the new entry and add it to the file descriptor table
                newEntry->filePointer = 0;
                int currentFileDescriptor = 0;
                while (fileDescriptorTable[currentFileDescriptor] != NULL) {
                    currentFileDescriptor++;
                }
                newEntry->inodeNumber = inodeCurrent;
                fileDescriptorTable[currentFileDescriptor] = newEntry;

                // Update the access time in the inode
                char *timeStampBuffer = (char *)malloc(TIMESTAMP_BUFFER_SIZE);
                getTimestamp(timeStampBuffer, TIMESTAMP_BUFFER_SIZE);
                memset(inodeBuffer + INODE_ACC_TIME_STAMP_OFFSET, 0, TIMESTAMP_BUFFER_SIZE);
                memcpy(inodeBuffer + INODE_ACC_TIME_STAMP_OFFSET, timeStampBuffer, TIMESTAMP_BUFFER_SIZE);
                int writeSuccess = writeBlock(activeDisk, inodeCurrent, inodeBuffer);
                if (writeSuccess < 0) {
                    printf("Issue with inode block write when opening file\n");
                    return FILE_OPEN_ERROR;
                }
                return currentFileDescriptor;
            }
            memset(fileName, 0, MAX_FILE_NAME_SIZE * sizeof(char));

            // Move to the next inode if the current one is not the target
            if (inodeBuffer[INODE_NEXT_INODE_OFFSET] == 0) {
                break;
            }
            memcpy(&inodeCurrent, inodeBuffer + INODE_NEXT_INODE_OFFSET, sizeof(int));
            memset(inodeBuffer, 0, BLOCKSIZE);
        }
        free(inodeBuffer);
    }

    // Check if there are free blocks available to create a new file
    int freeBlockHead;
    memcpy(&freeBlockHead, superData + FB_OFFSET, sizeof(int));
    if (freeBlockHead == 0) {
        printf("No free blocks\n");
        return NO_SPACE_LEFT;
    }

    // Prepare to create a new file if it does not exist
    char *freeBlockData = (char *)malloc(BLOCKSIZE);
    success = readBlock(activeDisk, freeBlockHead, freeBlockData);
    if (success < 0) {
        printf("Invalid pointer to free block\n");
        return FILE_OPEN_ERROR;
    }

    // Update the free block list and set up a new inode for the file
    int nextFreeBlock;
    memcpy(&nextFreeBlock, freeBlockData + FREE_NEXT_BLOCK_OFFSET, sizeof(int));
    memcpy(superData + FB_OFFSET, &nextFreeBlock, sizeof(int));
    int newInodeBlockNum = freeBlockHead;
    freeBlockData[BLOCK_NUMBER_OFFSET] = INODE_BLOCK_TYPE;
    freeBlockData[MAGIC_NUMBER_OFFSET] = MAGIC_NUMBER;
    memcpy(&inodeHead, superData + IB_OFFSET, sizeof(int));
    memcpy(freeBlockData + INODE_NEXT_INODE_OFFSET, &inodeHead, sizeof(int));
    memcpy(superData + IB_OFFSET, &newInodeBlockNum, sizeof(int));

    // Initialize the new inode with file details and timestamps
    int fileSize = 0;
    memcpy(freeBlockData + INODE_FILE_SIZE_OFFSET, &fileSize, sizeof(int));
    int dataBlockPointer = 0;
    memcpy(freeBlockData + INODE_DATA_BLOCK_OFFSET, &dataBlockPointer, sizeof(int));
    memset(freeBlockData + INODE_FILE_NAME_OFFSET, 0, MAX_FILE_NAME_SIZE * sizeof(char));
    memcpy(freeBlockData + INODE_FILE_NAME_OFFSET, name, strlen(name) * sizeof(char));
    char *timeStampBuffer = (char *)malloc(TIMESTAMP_BUFFER_SIZE);
    getTimestamp(timeStampBuffer, TIMESTAMP_BUFFER_SIZE);
    memcpy(freeBlockData + INODE_CR8_TIME_STAMP_OFFSET, timeStampBuffer, TIMESTAMP_BUFFER_SIZE);
    memcpy(freeBlockData + INODE_MOD_TIME_STAMP_OFFSET, timeStampBuffer, TIMESTAMP_BUFFER_SIZE);
    memcpy(freeBlockData + INODE_ACC_TIME_STAMP_OFFSET, timeStampBuffer, TIMESTAMP_BUFFER_SIZE);
    
    // Write the updated super block and the new inode block
    int writeSuccess = writeBlock(activeDisk, SUPER_BLOCK, superData);
    if (writeSuccess < 0) {
        printf("Issue with super block write when opening file\n");
        return FILE_OPEN_ERROR;
    }
    writeSuccess = writeBlock(activeDisk, newInodeBlockNum, freeBlockData);
    if (writeSuccess < 0) {
        printf("Issue with inode block write when opening file\n");
        return FILE_OPEN_ERROR;
    }

    // Create a new entry in the file descriptor table for the new file
    fileDescriptorTableEntry *newEntry = (fileDescriptorTableEntry *)malloc(sizeof(fileDescriptorTableEntry));
    if (newEntry == NULL) {
        printf("Could not allocate memory for new open file table entry\n");
        return FILE_OPEN_ERROR;
    }
    newEntry->filePointer = 0;
    int currentFileDescriptor = 0;
    while (fileDescriptorTable[currentFileDescriptor] != NULL) {
        currentFileDescriptor++;
    }
    newEntry->inodeNumber = newInodeBlockNum;
    fileDescriptorTable[currentFileDescriptor] = newEntry;

    // Free memory
    free(superData);
    free(freeBlockData);
    free(timeStampBuffer);

    // Return file descriptor of the newly opened or found file
    return currentFileDescriptor;
}

/* Closes the file, de-allocates all system resources, and removes table
entry */

int tfs_closeFile(fileDescriptor fileDescriptor) {
    // Check if the file descriptor is valid
    if (fileDescriptorTable[fileDescriptor] == NULL) {
        printf("Invalid file descriptor. Cannot close file\n");
        return FILE_BAD_DESCRIPTOR;
    }

    // Check if a disk is mounted before attempting to close the file
    if (activeDisk == 0) {
        printf("No disk mounted. Cannot close file\n");
        return FILE_CLOSE_ERROR;
    }

    // Validate the range of the file descriptor
    if (fileDescriptor < 0 || fileDescriptor >= maxNumberOfFiles) {
        printf("Invalid file descriptor. Cannot close file\n");
        return FILE_BAD_DESCRIPTOR;
    }

    // Free memory
    free(fileDescriptorTable[fileDescriptor]);
    fileDescriptorTable[fileDescriptor] = NULL;
    
    return 1;
}

int deallocateBlock(int blockNum) {
    char *data = (char *)malloc(BLOCKSIZE);
    int success = readBlock(activeDisk, blockNum, data);
    if (success < 0) {
        printf("Invalid pointer to block\n");
        return DEALLOCATION_ERROR;
    }
    memset(data, 0, BLOCKSIZE);
    data[BLOCK_NUMBER_OFFSET] = FREE_BLOCK_TYPE;
    data[MAGIC_NUMBER_OFFSET] = MAGIC_NUMBER;
    char *superData = (char *)malloc(BLOCKSIZE);
    success = readBlock(activeDisk, SUPER_BLOCK, superData);
    if (success < 0) {
        printf("Issue with super block read when deallocating block\n");
        return DEALLOCATION_ERROR;
    }
    int freeBlockHead;
    memcpy(&freeBlockHead, superData + FB_OFFSET, sizeof(int));
    memcpy(data + FREE_NEXT_BLOCK_OFFSET, &freeBlockHead, sizeof(int));
    memcpy(superData + FB_OFFSET, &blockNum, sizeof(int));
    int writeSuccess = writeBlock(activeDisk, SUPER_BLOCK, superData);
    if (writeSuccess < 0) {
        printf("Issue with super block write when deallocating block\n");
        return DEALLOCATION_ERROR;
    }
    writeSuccess = writeBlock(activeDisk, blockNum, data);
    if (writeSuccess < 0) {
        printf("Issue with free block write when deallocating block\n");
        return DEALLOCATION_ERROR;
    }
    return 1;
}

/* Writes buffer ‘buffer’ of size ‘size’, which represents an entire
file’s content, to the file system. Previous content (if any) will be
completely lost. Sets the file pointer to 0 (the start of file) when
done. Returns success/error codes. */

int tfs_writeFile(fileDescriptor fileDescriptor, char *buffer, int size) {
    // Check if there is a disk mounted before attempting to write
    if (activeDisk == 0) {
        printf("Error: No disk mounted. Cannot find file. (writeFile)\n");
        return FS_MOUNT_ERROR;
    }

    // Retrieve the file descriptor table entry to get file-specific data
    fileDescriptorTableEntry *fileDescriptorEntry = fileDescriptorTable[fileDescriptor];
    if (fileDescriptorEntry == NULL) {
        printf("Error: File has not been opened. (writeFile)\n");
        return FILE_BAD_DESCRIPTOR;
    }

    // Read the inode for the file from the file system's super block
    int fileInode = fileDescriptorEntry->inodeNumber;
    char *superData = (char *)malloc(BLOCKSIZE * sizeof(char));
    int success = readBlock(activeDisk, SUPER_BLOCK, superData);
    if (success < 0) {
        free(superData);
        printf("Error: Issue with super block read. (writeFile)\n");
        return FILE_READ_ERROR;
    }

    // Read the inode block of the file to access file-specific metadata
    char *inodeBuffer = (char *)malloc(BLOCKSIZE * sizeof(char));
    success = readBlock(activeDisk, fileInode, inodeBuffer);
    if (success < 0) {
        free(superData);
        free(inodeBuffer);
        printf("Error: Issue with inode read. (writeFile)\n");
        return FILE_READ_ERROR;
    }

    // Determine the current size of the file from the inode
    int currentFileSize;
    memcpy(&currentFileSize, inodeBuffer + INODE_FILE_SIZE_OFFSET, sizeof(int));

    // Calculate the number of data blocks needed based on the size parameter
    int dataBlock;
    memcpy(&dataBlock, inodeBuffer + INODE_DATA_BLOCK_OFFSET, sizeof(int));
    int blocksNeeded = size / USEABLE_DATA_SIZE + (size % USEABLE_DATA_SIZE > 0 ? 1 : 0);
    
    // Deallocate existing data blocks if the file already contains data
    int bufferPointer = 0;
    int remainingBytes = size;
    if (currentFileSize != 0) {
        int blocksToDeallocate = currentFileSize / USEABLE_DATA_SIZE + (size % USEABLE_DATA_SIZE > 0 ? 1 : 0);
        for (int i = 0; i < blocksToDeallocate; i++) {
            char *dataBuffer = (char *)malloc(BLOCKSIZE * sizeof(char));
            success = readBlock(activeDisk, dataBlock, dataBuffer);
            if (success < 0) {
                free(inodeBuffer);
                free(superData);
                printf("Error: Data block could not be read. (writeFile)\n");
                return FILE_READ_ERROR;
            }
            int nextBlock;
            memcpy(&nextBlock, dataBuffer + DATA_NEXT_BLOCK_OFFSET, sizeof(int));
            success = deallocateBlock(dataBlock);
            if (success < 0) {
                free(inodeBuffer);
                free(superData);
                printf("Error: Could not deallocate data block. (writeFile)\n");
                return DEALLOCATION_ERROR;
            }
            dataBlock = nextBlock;
            free(dataBuffer);
        }
    }

    // Allocate new data blocks and write the buffer to these blocks
    int freeBlock;
    memcpy(&freeBlock, superData + FB_OFFSET, sizeof(int));
    int dataExtentHead = freeBlock;
    char *freeBuffer = (char *)malloc(BLOCKSIZE * sizeof(char));
    while (blocksNeeded != 0) {
        success = readBlock(activeDisk, freeBlock, freeBuffer);
        if (success < 0) {
            free(inodeBuffer);
            free(superData);
            free(freeBuffer);
            printf("Error: Free block could not be read. (writeFile)\n");
            return FILE_READ_ERROR;
        }
        freeBuffer[BLOCK_NUMBER_OFFSET] = DATA_BLOCK_TYPE;
        int writeBufferSize = (remainingBytes >= USEABLE_DATA_SIZE ? USEABLE_DATA_SIZE : remainingBytes) * sizeof(char);
        memcpy(freeBuffer + DATA_BLOCK_DATA_OFFSET, buffer + bufferPointer, writeBufferSize);
        bufferPointer = bufferPointer + writeBufferSize;
        remainingBytes = remainingBytes - writeBufferSize;
        int dataBlock = freeBlock;
        memcpy(&freeBlock, freeBuffer + FREE_NEXT_BLOCK_OFFSET, sizeof(int));
        blocksNeeded--;
        if (blocksNeeded == 0) {
            int zero = 0;
            memcpy(freeBuffer + DATA_NEXT_BLOCK_OFFSET, &zero, sizeof(int));
        }
        success = writeBlock(activeDisk, dataBlock, freeBuffer);
        if (success < 0) {
            free(inodeBuffer);
            free(superData);
            free(freeBuffer);
            printf("Error: Free block could not be written to. (writeFile)\n");
            return FILE_WRITE_ERROR;
        }
        if (freeBlock == 0 && blocksNeeded != 0) {
            break;
        }
    }

    // Update the super block to reflect the new state of free blocks
    memcpy(superData + FB_OFFSET, &freeBlock, sizeof(int));
    success = writeBlock(activeDisk, SUPER_BLOCK, superData);
    if (success < 0) {
        free(inodeBuffer);
        free(superData);
        free(freeBuffer);
        printf("Error: Super block could not be updated. (writeFile)\n");
        return FILE_WRITE_ERROR;
    }

    // Update inode with the new file size and data block head
    int finalSize = size - remainingBytes;
    memcpy(inodeBuffer + INODE_FILE_SIZE_OFFSET, &finalSize, sizeof(int));
    memcpy(inodeBuffer + INODE_DATA_BLOCK_OFFSET, &dataExtentHead, sizeof(int));

    // Update the inode modification timestamp
    char *timeStampBuffer = (char *)malloc(TIMESTAMP_BUFFER_SIZE);
    getTimestamp(timeStampBuffer, TIMESTAMP_BUFFER_SIZE);
    memcpy(inodeBuffer + INODE_MOD_TIME_STAMP_OFFSET, timeStampBuffer, TIMESTAMP_BUFFER_SIZE);
    free(timeStampBuffer);

    // Write the updated inode back to the disk
    success = writeBlock(activeDisk, fileInode, inodeBuffer);
    if (success < 0) {
        free(inodeBuffer);
        free(superData);
        free(freeBuffer);
        printf("Error: Inode block could not be updated. (writeFile)\n");
        return FILE_WRITE_ERROR;
    }

    // Reset the file descriptor's file pointer to the beginning
    fileDescriptorEntry->filePointer = 0;
    free(inodeBuffer);
    free(superData);
    free(freeBuffer);

    // Check if all necessary blocks were successfully allocated and written
    if (blocksNeeded > 0) {
        printf("Error: No free blocks. Incomplete write (writeFile)\n");
        return FILE_WRITE_ERROR;
    }
    return 1;
}

/* deletes a file and marks its blocks as free on disk. */

int tfs_deleteFile(fileDescriptor fileDescriptor) {

    // Validate file descriptor
    if (fileDescriptorTable[fileDescriptor] == NULL) {
        printf("invalid File Descriptor. Cannot delete file\n");
        return FILE_BAD_DESCRIPTOR;
    }

     // Retrieve inode to delete
    int inodeToDelete = fileDescriptorTable[fileDescriptor]->inodeNumber;

    // Read the super block to get inode information
    char *superData = (char *)malloc(BLOCKSIZE);
    int success = readBlock(activeDisk, SUPER_BLOCK, superData);
    if (success < 0) {
        printf("Issue with super block read when deleting file\n");
        return FILE_DELETE_ERROR;
    }

    // Find the inode in the inode list
    int currentInode;
    char *currentInodeBuffer = (char *)malloc(BLOCKSIZE);
    memcpy(&currentInode, superData + IB_OFFSET, sizeof(int));
    success = readBlock(activeDisk, currentInode, currentInodeBuffer);
    if (success < 0) {
        printf("Invalid pointer to inode block\n");
        return FILE_DELETE_ERROR;
    }

    // Check if the first inode is the one to delete
    if (currentInode == inodeToDelete) {
        // Update super block to point to next inode
        memcpy(superData + IB_OFFSET, currentInodeBuffer + INODE_NEXT_INODE_OFFSET, sizeof(int));
        int writeSuccess = writeBlock(activeDisk, SUPER_BLOCK, superData);
        if (writeSuccess < 0) {
            printf("Issue with super block write when deleting file\n");
            return FILE_DELETE_ERROR;
        }
    } else {
        // Traverse inode list to find the target inode
        int nextInode;
        memcpy(&nextInode, currentInodeBuffer + INODE_NEXT_INODE_OFFSET, sizeof(int));
        while (nextInode != inodeToDelete) {
            success = readBlock(activeDisk, nextInode, currentInodeBuffer);
            if (success < 0) {
                printf("Invalid pointer to inode block\n");
                return FILE_DELETE_ERROR;
            }
            currentInode = nextInode;
            memcpy(&nextInode, currentInodeBuffer + INODE_NEXT_INODE_OFFSET, sizeof(int));
        }

        // Update the previous inode to skip the deleted inode
        char *nextinodeBuffer = (char *)malloc(BLOCKSIZE);
        success = readBlock(activeDisk, nextInode, nextinodeBuffer);
        if (success < 0) {
            printf("Invalid pointer to inode block\n");
            return FILE_DELETE_ERROR;
        }
        int inodeAfterToDelete;
        memcpy(&inodeAfterToDelete, nextinodeBuffer + INODE_NEXT_INODE_OFFSET, sizeof(int));
        memcpy(currentInodeBuffer + INODE_NEXT_INODE_OFFSET, &inodeAfterToDelete, sizeof(int));
        int writeSuccess = writeBlock(activeDisk, currentInode, currentInodeBuffer);
        if (writeSuccess < 0) {
            printf("Issue with inode block write when deleting file\n");
            return FILE_DELETE_ERROR;
        }
        free(nextinodeBuffer);
    }

    // Free all data blocks associated with the inode
    success = readBlock(activeDisk, inodeToDelete, currentInodeBuffer);
    if (success < 0) {
        printf("Invalid pointer to inode block\n");
        return FILE_DELETE_ERROR;
    }
    int dataBlockPointer;
    memcpy(&dataBlockPointer, currentInodeBuffer + INODE_DATA_BLOCK_OFFSET, sizeof(int));
    if (dataBlockPointer != 0) {
        char *dataBlock = (char *)malloc(BLOCKSIZE);
        while (1) {
            success = readBlock(activeDisk, dataBlockPointer, dataBlock);
            if (success < 0) {
                printf("Invalid pointer to data block\n");
                return FILE_DELETE_ERROR;
            }
            int nextDataBlockPointer;
            memcpy(&nextDataBlockPointer, dataBlock + DATA_NEXT_BLOCK_OFFSET, sizeof(int));
            deallocateBlock(dataBlockPointer);
            dataBlockPointer = nextDataBlockPointer;
            if (dataBlockPointer == 0) {
                break;
            }
        }
        free(dataBlock);
    }

    // Free memory
    deallocateBlock(inodeToDelete);
    tfs_closeFile(fileDescriptor);
    free(superData);
    free(currentInodeBuffer);
    return 1;
}

/* reads one byte from the file and copies it to buffer, using the
current file pointer location and incrementing it by one upon success.
If the file pointer is already past the end of the file then
tfs_readByte() should return an error and not increment the file pointer.
*/

int tfs_readByte(fileDescriptor fileDescriptor, char *buffer) {

    // Check if a disk is mounted
    if (activeDisk == INT_NULL) {
        printf("Error: No disk mounted. Cannot find file. (readByte)\n");
        return FS_MOUNT_ERROR;
    }

    // Retrieve the file descriptor entry
    fileDescriptorTableEntry *fileDescriptorEntry = fileDescriptorTable[fileDescriptor];
    if (fileDescriptorEntry == NULL) {
        printf("Error: File has not been opened. (readByte)\n");
        return FILE_BAD_DESCRIPTOR;
    }
    int fileInode = fileDescriptorEntry->inodeNumber;
    int filePointer = fileDescriptorEntry->filePointer;

    // Read the inode block associated with the file descriptor
    char *inodeBuffer = (char *)malloc(BLOCKSIZE * sizeof(char));
    int success = readBlock(activeDisk, fileInode, inodeBuffer);
    if (success < 0) {
        free(inodeBuffer);
        printf("Error: Issue with inode read. (readByte)\n");
        return FILE_READ_ERROR;
    }

     // Extract file size and data block pointer
    int currentFileSize;
    memcpy(&currentFileSize, inodeBuffer + INODE_FILE_SIZE_OFFSET, sizeof(int));
    int dataBlock;
    memcpy(&dataBlock, inodeBuffer + INODE_DATA_BLOCK_OFFSET, sizeof(int));
    if (filePointer >= currentFileSize) {
        free(inodeBuffer);
        printf("\nError: File pointer out of bounds, EOF. (readByte)\n");
        return BLOCK_READ_ERROR;
    }

     // Read the correct data block based on the file pointer
    int blockNumber = filePointer / USEABLE_DATA_SIZE;
    int byteNumber = filePointer % USEABLE_DATA_SIZE;
    char *blockData = (char *)malloc(BLOCKSIZE * sizeof(char));
    success = readBlock(activeDisk, dataBlock, blockData);
    if (success < 0) {
        free(inodeBuffer);
        free(blockData);
        printf("Error: Issue with data read. (readByte)\n");
        return FILE_READ_ERROR;
    }
    while (blockNumber != 0) {
        memcpy(&dataBlock, blockData + DATA_NEXT_BLOCK_OFFSET, sizeof(int));
        success = readBlock(activeDisk, dataBlock, blockData);
        if (success < 0) {
            free(inodeBuffer);
            free(blockData);
            printf("Error: Issue with data read. (readByte)\n");
            return FILE_READ_ERROR;
        }
        blockNumber--;
    }

     // Read byte into buffer
    memcpy(buffer, blockData + DATA_BLOCK_DATA_OFFSET + byteNumber, sizeof(char));

    // Increment file pointer
    tfs_seek(fileDescriptor, 1);

    // Update access timestamp in inode
    char *timeStampBuffer = (char *)malloc(TIMESTAMP_BUFFER_SIZE);
    getTimestamp(timeStampBuffer, TIMESTAMP_BUFFER_SIZE);
    memcpy(inodeBuffer + INODE_ACC_TIME_STAMP_OFFSET, timeStampBuffer, TIMESTAMP_BUFFER_SIZE);
    free(timeStampBuffer);

    // Write updated inode data back to disk
    success = writeBlock(activeDisk, fileInode, inodeBuffer);
    if (success < 0) {
        free(inodeBuffer);
        free(blockData);
        printf("Error: Inode block could not be updated. (readByte)\n");
        return FILE_WRITE_ERROR;
    }

    // Free memory
    free(inodeBuffer);
    free(blockData);

    return 1;
}

/* change the file pointer location to offset (absolute). Returns
success/error codes.*/

int tfs_seek(int descriptor, int offset) {
    // Check if there is a disk mounted before attempting to seek
    if (activeDisk == INT_NULL) {
        printf("Error: No disk mounted. Cannot perform seek operation. (seek)\n");
        return FS_MOUNT_ERROR;
    }

    // Retrieve the file descriptor entry from the file descriptor table
    fileDescriptorTableEntry *entry = fileDescriptorTable[descriptor];
    if (entry == NULL) {
        printf("Error: File descriptor not found or file not opened. (seek)\n");
        return FILE_BAD_DESCRIPTOR;
    }

    // Calculate the new file pointer position by adding the offset
    int newFilePointer = entry->filePointer + offset;

    // Update the file pointer in the file descriptor entry
    entry->filePointer = newFilePointer;

    // Return the updated file pointer position
    return newFilePointer;
}

int tfs_readdir() {
    // Check if a disk is mounted
    if (activeDisk == INT_NULL) {
        printf("Error: No disk mounted. Cannot perform directory read. (readdir)\n");
        return FS_MOUNT_ERROR;
    }

    // Allocate and read super block data
    char *superBlockData = (char *)malloc(BLOCKSIZE);
    if (superBlockData == NULL) {
        printf("Memory allocation failure for super block data.\n");
        return MEM_ALLOC_FAILURE; // Define this error code accordingly
    }

    int readStatus = readBlock(activeDisk, SUPER_BLOCK, superBlockData);
    if (readStatus < 0) {
        free(superBlockData);
        printf("Error: Issue with super block read. (readdir)\n");
        return FILE_READ_ERROR;
    }

    // Extract the head of the inode list from the super block
    int inodeIndex;
    memcpy(&inodeIndex, superBlockData + IB_OFFSET, sizeof(int));
    free(superBlockData);  // Free super block data after use

    printf("\nFILE SYSTEM:\nroot directory:\n");

    // Iterate through inode list and print file names
    while (inodeIndex != 0) {
        char *inodeBuffer = (char *)malloc(BLOCKSIZE);
        if (inodeBuffer == NULL) {
            printf("Memory allocation failure for inode data.\n");
            return MEM_ALLOC_FAILURE; // Define this error code accordingly
        }

        readStatus = readBlock(activeDisk, inodeIndex, inodeBuffer);
        if (readStatus < 0) {
            free(inodeBuffer);
            printf("Error: Issue with inode block read. (readdir)\n");
            return FILE_READ_ERROR;
        }

        // Extract the file name from the inode block
        char fileName[MAX_FILE_NAME_SIZE];
        memcpy(fileName, inodeBuffer + INODE_FILE_NAME_OFFSET, MAX_FILE_NAME_SIZE);
        printf("%s\n", fileName);
        
        // Move to the next inode
        memcpy(&inodeIndex, inodeBuffer + INODE_NEXT_INODE_OFFSET, sizeof(int));
        free(inodeBuffer);  // Free inode data after use
    }

    printf("\n");

    return 1;
}

int tfs_rename(int fd, char *newName) {

    // Check if the new name is within the allowable length limit
    if (strlen(newName) >= MAX_FILE_NAME_SIZE) {
        printf("Error: File name is too long, cannot be supported. (rename)\n");
        return FILE_RENAME_ERROR;
    }

    // Check if a disk is mounted
    if (activeDisk == INT_NULL) {
        printf("Error: No disk mounted. Cannot find file. (rename)\n");
        return FS_MOUNT_ERROR;
    }

    // Retrieve the file descriptor table entry
    fileDescriptorTableEntry *descriptorEntry = fileDescriptorTable[fd];
    if (descriptorEntry == NULL) {
        printf("Error: File has not been opened. (rename)\n");
        return FILE_BAD_DESCRIPTOR;
    }

    int inodeIndex = descriptorEntry->inodeNumber;
    char *inodeBuffer = (char *)malloc(BLOCKSIZE * sizeof(char));

    // Read the inode block
    int readStatus = readBlock(activeDisk, inodeIndex, inodeBuffer);
    if (readStatus < 0) {
        free(inodeBuffer);
        printf("Error: Issue with inode block read. (rename)\n");
        return FILE_READ_ERROR;
    }

    // Clear and set new file name in the inode block
    memset(inodeBuffer + INODE_FILE_NAME_OFFSET, 0, MAX_FILE_NAME_SIZE * sizeof(char));
    memcpy(inodeBuffer + INODE_FILE_NAME_OFFSET, newName, strlen(newName) * sizeof(char));

    // Update modification timestamp
    char *timestamp = (char *)malloc(TIMESTAMP_BUFFER_SIZE);
    getTimestamp(timestamp, TIMESTAMP_BUFFER_SIZE);
    memcpy(inodeBuffer + INODE_MOD_TIME_STAMP_OFFSET, timestamp, TIMESTAMP_BUFFER_SIZE);
    free(timestamp);

    // Write the updated inode block back to disk
    int writeStatus = writeBlock(activeDisk, inodeIndex, inodeBuffer);
    if (writeStatus < 0) {
        free(inodeBuffer);
        printf("Error: Issue with inode block write. (rename)\n");
        return FILE_WRITE_ERROR;
    }

    // Free memory
    free(inodeBuffer);

    return 1;
}