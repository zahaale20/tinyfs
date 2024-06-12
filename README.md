# Tiny Filesystem

## Authors:
- Alex Zaharia
- Triet Tran

## TinyFS Implementation: Leveraging Linked Lists
TinyFS is a lightweight file system optimized for efficient data insertion using linked lists, which offers a balance between functionality and system performance. The choice to use linked lists allows for rapid data insertion, though deletions may be slower compared to other data structures. This design decision supports our goal of optimizing system operations for scenarios where write performance is critical.

## Enhancing TinyFS: Advanced Features
Our TinyFS implementation incorporates several advanced features:
- **Timestamps**: For tracking when files are created, modified, and accessed. This feature enhances file management by providing historical data integrity.
- **File renaming capabilities**: Users can rename files, which improves overall file management and organization.
- **Directory listing**: Enhances navigation and file management by allowing users to view lists of files and directories.

## Demonstration of Functionality
We have demonstrated that these features work through various tests:
- **Timestamps**: Each file operation updates the relevant timestamps, which we then display using the `tfs_readFileInfo` function.
- **File Renaming**: We successfully change file names and verify the changes through subsequent directory listings.
- **Directory Listing**: Our tests confirm that after file operations, the directory listing accurately reflects the current state of the file system.

## Limitations and Bugs
TinyFS is designed for specific use cases and thus, while stable and reliable within its scope, it does not include more complex features found in larger file systems such as hierarchical directory structures or built-in compression. Known issues include:
- **Slow Deletion**: Due to the linked list structure, deleting files, especially in a large filesystem, can be slower than in systems that use more sophisticated data structures.
- **Single Mount at a Time**: TinyFS can only mount one filesystem at a time, limiting its use in environments where multiple filesystem access is necessary.

## Running the Demo
To see TinyFS in action, use the following commands:
```bash
make clean
make
./tinyFSDemo