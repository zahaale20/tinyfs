#ifndef libTinyFS_h
#define libTinyFS_h

/* The default size of the disk and file system block */
#define BLOCKSIZE 256
/* Your program should use a 10240 Byte disk size giving you 40 blocks
total. This is a default size. You must be able to support different
possible values */
#define DEFAULT_DISK_SIZE 10240
/* use this name for a default emulated disk file name */
#define DEFAULT_DISK_NAME "tinyFSDisk"
/* use as a special type to keep track of files */
typedef int fileDescriptor;

#define MAX_BYTES 2147483647
#define USEABLE_DATA_SIZE 250
#define MAGIC_NUMBER 0x44
#define BLOCK_NUMBER_OFFSET 0
#define MAGIC_NUMBER_OFFSET 1
#define TIMESTAMP_BUFFER_SIZE 25
#define SUPER_BLOCK_TYPE 1
#define SUPER_BLOCK 0
#define FB_OFFSET 2
#define IB_OFFSET 6
#define SUPER_MAX_NUM_FILES_OFFSET 10
#define INODE_BLOCK_TYPE 2
#define INODE_NEXT_INODE_OFFSET 2
#define INODE_FILE_SIZE_OFFSET 6
#define INODE_DATA_BLOCK_OFFSET 10
#define INODE_FILE_NAME_OFFSET 14
#define INODE_CR8_TIME_STAMP_OFFSET 23
#define INODE_MOD_TIME_STAMP_OFFSET 48
#define INODE_ACC_TIME_STAMP_OFFSET 73
#define FREE_BLOCK_TYPE 4
#define FREE_NEXT_BLOCK_OFFSET 2
#define DATA_BLOCK_TYPE 3
#define DATA_NEXT_BLOCK_OFFSET 2
#define DATA_BLOCK_DATA_OFFSET 6
#define MAX_FILE_NAME_SIZE 9
#define INT_NULL 0
#define BEGINNING_OF_FILE 0


typedef struct fileDescriptorTableEntry {
    int inodeNumber;
    int filePointer;
} fileDescriptorTableEntry;

int tfs_mkfs(char* filename, int nBytes);
int tfs_mount(char* diskname);
int tfs_unmount(void);
fileDescriptor tfs_openFile(char* name);
int tfs_closeFile(fileDescriptor FD);
int tfs_writeFile(fileDescriptor FD, char* buffer, int size);
int tfs_deleteFile(fileDescriptor FD);
int tfs_readByte(fileDescriptor FD, char* buffer);
int tfs_seek(fileDescriptor FD, int offset);
int tfs_rename(fileDescriptor FD, char* newName);
int tfs_readdir();
int tfs_readFileInfo(fileDescriptor FD);

#endif