#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "libTinyFS.h"
#include "libDisk.h"
#include "Errors.h"

//Global FS state/metadata
int isMounted = 0;
int currentOpendisk = 0;
int fileInodeNumbers = 0;

struct dynamic_file_table *fileHead = NULL;

int nextFreeInode(char* bitmap){
    //returns the bNum of the next free Inode from 1 - midpoint

}

int nextFreeDataBlock(char* bitmap){
    //returns the bNum of the next free Inode from midpoint - (end - 1)

}

int doesNotExistOnDisk(char*name,int disk,int FStotalBlockCount){
    char buffer[BLOCK_SIZE];
    int i;
    //should read the root Inode on last bNum and check all the name inode pairs
    if(readBlock(disk,FStotalBlockCount-1, buffer) == READBLK_FAILED){
        return DOES_NOT_EXIST;
    }

    //read every 8 bytes for every 16 bytes
    for(i = 0; i < BLOCK_SIZE; i+=16){
        // Extract the first 8 bytes
        unsigned char bytes[8];
        memcpy(bytes, buffer + i, 8);

        // Convert the bytes to a string
        char str[9]; // Plus 1 for null terminator
        memcpy(str, bytes, 8);
        str[8] = '\0';

        // Convert the string
        int result = strcmp(str, name);
        if(result == 0){
            //it exists
            return 0;
        }
    }

    return DOES_NOT_EXIST;
}

int alreadyOpen(char* name, struct dynamic_file_table *head){
    struct dynamic_file_table *current = head;

    // Traverse the dynamic resource table
    while (current != NULL) {
        if (strcmp(current->filename, name) == 0) {
            // File with the given name is already open
            return -1;
        }
        current = current->next;
    }
    return 0;
}

void clearDynamicResourceTable(struct dynamic_file_table *head) {
    struct dynamic_file_table *current = head;
    struct dynamic_file_table *next;

    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
}

void addFileToDynamicResourceTable(char *filename, int inodeNumber, fileDescriptor fd, long size, int fp) {
    // Create a new entry
    struct dynamic_file_table *newEntry = malloc(sizeof(struct dynamic_file_table));
    newEntry->filename = filename;
    newEntry->inodeNumber = inodeNumber;
    newEntry->fd = fd;
    newEntry->size = size;
    newEntry->fp = fp;
    newEntry->next = NULL;

    if (fileHead == NULL) {
        // The linked list is empty, so set the new entry as the head
        fileHead = newEntry;
    } else {
        // Traverse the linked list to find the last node
        struct dynamic_file_table *current = fileHead;
        while (current->next != NULL) {
            current = current->next;
        }

        // Link the new entry to the last node
        current->next = newEntry;
    }
}

char* computebitmap(int disk, int start, int end){
    //creates a bitmap from start bNum to end bNum on current disk
    // Allocate memory for the bitmap size
    int bitmapSize = end - start + 1;
    char* bitmap = malloc(bitmapSize * sizeof(char));
    if (bitmap == NULL) {
        return NULL; // Memory allocation failed
    }

    //for checking if free block
    char *check = calloc(BLOCK_SIZE, sizeof(char));
    if(check == NULL){
        free(check);
        return NULL;
    }
    //buffer we read blocks into
    char *buffer = calloc(BLOCK_SIZE, sizeof(char));
    if(buffer == NULL){
        free(buffer);
        return NULL;
    }
    //hardcode to superblock to be allocated space
    for(int i = start; i <= end; i++){
        if(readBlock(disk,i,buffer) == READBLK_FAILED){
            free(bitmap);
            free(check);
            free(buffer);
            return NULL;
        }

        int result = memcmp(buffer,check,BLOCK_SIZE);

        //both all 0's so it is a free block otherwise not free block
        if(result == 0){
            bitmap[i - start] = 'F';
        } else{
            bitmap[i - start] = 'A';
        }
    }

    free(check);
    free(buffer);

    return bitmap;
}

/* Makes an empty TinyFS file system of size nBytes on the file specified by
 * ‘filename’. This function should use the emulated disk library to open the
 * specified file, and upon success, format the file to be mountable. This
 * includes initializing all data to 0x00, setting magic numbers, initializing
 * and writing the superblock and other metadata, etc. Must return a specified
 * success/error code. */
int tfs_mkfs(char *filename, int nBytes){

    struct stat fileStat;
    int disk;
    int i;
    if((disk = openDisk(filename,nBytes)) == OPENDISK_FAILED){
        return TFS_MKFS_FAILED;
    }

    int blockCount = nBytes/BLOCK_SIZE;
    int midpoint = blockCount / 2;
    int end = blockCount - 1;

    //for formatting the file system
    unsigned char *buffer = calloc(BLOCK_SIZE, sizeof(char));
    if(buffer == NULL){
        return TFS_MKFS_FAILED;
    }

    //0 out the disk
    for(i = 0 ; i < blockCount;i++){
        writeBlock(disk, i, buffer);
    }

    //root Inode at bNum -> 1 - midpoint
    // Use the stat function to retrieve file information
    if (stat(filename, &fileStat) == -1) {
        free(buffer);
        return TFS_MKFS_FAILED;
    }

    //Root Directory Inode structure at bNum 1
    struct Inode inode;
    unsigned char *inodepadding = calloc(164, sizeof(char));
    if(buffer == NULL){
        return TFS_MKFS_FAILED;
    }
    inode.fileSize = fileStat.st_size;
    inode.inodeNumber = 1;
    inode.accessControl = 0755;
    inode.referenceCount = 0;
    inode.fileType = "Directory";
    inode.accessTime = fileStat.st_atimespec;
    inode.modifiedTime = fileStat.st_mtimespec;
    inode.creationTime = fileStat.st_ctimespec;
    //Last bNum is for root directory data block that stores name - inode pairs
    inode.datablock = end;
    memcpy(inode.padding, inodepadding, sizeof(inode.padding));
    writeBlock(disk,1,&inode);

    //Superblock structure at bNum 0
    //magic number
    struct Superblock sb;
    sb.magicNumber = 0x5A;
    sb.blockCount = blockCount;
    //create the inode block sector and data block sectors bitmaps for superblock
    // Compute the inode bitmap and copy it to the superblock
    sb.inodebitmap = malloc(sizeof(char) * midpoint);
    char* inodeBitmap = computebitmap(disk, 1, midpoint);
    memcpy(sb.inodebitmap, inodeBitmap, midpoint);
    free(inodeBitmap);

    // Compute the free data block bitmap and copy it to the superblock
    sb.freedatablockbitmap = malloc(sizeof(char) * blockCount - midpoint - 1);
    char* freeDataBlockBitmap = computebitmap(disk, midpoint + 1, end);
    memcpy(sb.freedatablockbitmap, freeDataBlockBitmap, blockCount - midpoint - 1);
    free(freeDataBlockBitmap);

    //write superblock onto disk
    writeBlock(disk, 0, &sb);
    free(buffer);
    free(inodepadding);
    free(sb.inodebitmap);
    free(sb.freedatablockbitmap);

    return 0; //Success
}

/* tfs_mount(char *filename) “mounts” a TinyFS file system located within ‘filename’.
 * tfs_unmount(void) “unmounts” the currently mounted file system. As part of the mount
 * operation, tfs_mount should verify the file system is the correct type. Only one file
 * system may be mounted at a time. Use tfs_unmount to cleanly unmount the currently
 * mounted file system. Must return a specified success/error code. */
int tfs_mount(char *filename){
    if(isMounted != 0){
        perror("Cannot Mount multiple file systems!");
        return MOUNT_FAILED;
    }

    //Checking file system integrity from superblock
    int disk;
    if((disk = openDisk(filename,0)) == OPENDISK_FAILED){
        return MOUNT_FAILED;
    }

    // Read the superblock from the disk
    char superBlockBuffer[BLOCK_SIZE];
    if (readBlock(disk, 0, superBlockBuffer) != 0) {
        return MOUNT_FAILED;
    }

    // Verify the file system type using the magic number
    if (superBlockBuffer[0] != 0x5A) {
        return MOUNT_FAILED;
    }

    // Mark the file system as mounted
    isMounted = 1;
    currentOpendisk = disk;

    return 0;
}

int tfs_unmount(void){
    if(isMounted != 1){
        perror("A file system must be mounted first!");
        return UNMOUNT_FAILED;
    }

    //close the disk of the current mounted file
    closeDisk(currentOpendisk);

    //Unmark as mounted
    isMounted = 0;

    //clear dynamic resource table
    clearDynamicResourceTable(fileHead);

    return 0;
}

/* Opens a file for reading and writing on the currently mounted file system.
 * Creates a dynamic resource table entry for the file (the structure that tracks open files,
 * the internal file pointer, etc.), and returns a file descriptor (integer) that can be
 * used to reference this file while the filesystem is mounted. */
fileDescriptor tfs_open(char *name){
        fileDescriptor fd;

        //check name is not too long
        if(strlen(name) > 8){
            perror("Filename trying to be open is too long");
            return TFS_OPEN_FAILED;
        }

        //Check if file is not already opened in current FS
        if(alreadyOpen(name,fileHead) == -1){
            perror("File already opened or does not exist on FS");
            return TFS_OPEN_FAILED;
        }

        //read in superblock bitmap
        struct Superblock sb;
        if(readBlock(currentOpendisk,0,&sb) == READBLK_FAILED){
            perror("Superblock corrupted");
            return TFS_OPEN_FAILED;
        }

        //Check if file exists in FS
        if(doesNotExistOnDisk(name,currentOpendisk,sb.blockCount) == DOES_NOT_EXIST){
            //create Inode and add on disk
            struct Inode inode;
            inode.fileSize = 0;
            inode.inodeNumber = fileInodeNumbers++;
            inode.accessControl = 0755;
            inode.referenceCount = 0;
            inode.fileType = "File";
            inode.datablock = nextFreeDataBlock(sb.freedatablockbitmap);

            //allocate empty datablock on disk

            //add name inode pair to root directory data block

            //update the superblock bitmaps

        }

        if((fd = open(name,O_RDWR)) == -1){
                return TFS_OPEN_FAILED;
        }

        //read bNum of where file inode is in FS
        struct Inode inode;
        readBlock(currentOpendisk,
                  nextFreeInode(sb.inodebitmap),
                  &inode);

        //add entry to dynamic resource table to show it is open
        addFileToDynamicResourceTable(name,inode.inodeNumber,fd,inode.fileSize,0);

        return fd;
}

/* Closes the file and removes dynamic resource table entry */
int tfs_close(fileDescriptor FD){

    return  0;
}

/* Writes buffer ‘buffer’ of size ‘size’, which represents an entire file’s contents, to
 * the file described by ‘FD’. Sets the file pointer to 0 (the start of file) when done.
 * Returns success/error codes. */
int tfs_write(fileDescriptor FD, char *buffer, int size){

}

/* deletes a file and marks its blocks as free on disk. */
int tfs_delete(fileDescriptor FD);

/* reads one byte from the file and copies it to ‘buffer’, using the current file
 * pointer location and incrementing it by one upon success. If the file pointer
 * is already at the end of the file then tfs_readByte() should return an error
 * and not increment the file pointer. */
int tfs_readByte(fileDescriptor FD, char *buffer);

/* change the file pointer location to offset (absolute). Returns success/error codes.*/
int tfs_seek(fileDescriptor FD, int offset);
