#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "libTinyFS.h"
#include "libDisk.h"
#include "Errors.h"

//Global FS state/metadata
int isMounted = 0;
int currentOpendisk = 0;
char* currentDiskName = "";
int fileInodeNumbers = 10;

fileDescriptor fd = 0;

struct dynamic_file_table *fileHead = NULL;
struct FileInfo rootDirectoryFiles[OPEN_MAX];

int getInodeNumberFromRootDirectory(char* name){
    int i;

    for(i = 0; i < OPEN_MAX;i++){
        if (strcmp(rootDirectoryFiles[i].filename, "") == 0) {
            break;
        }
        else if(strcmp(rootDirectoryFiles[i].filename,name) == 0){
            return rootDirectoryFiles[i].inodeNumber;
        }
    }

    return NOT_FOUND_ROOT_DIRECTORY;
}

int getInodeLocation(int currentdisk, int inodeNum, int blkCount){
    int i;
    struct Inode *inode = malloc(sizeof(struct Inode));

    for(i = 0; i < blkCount; i++){
        readBlock(currentdisk,i,inode);
        if (inode->inodeNumber==inodeNum) {
            free(inode);
            return i;
        }
    }

    free(inode);
    return NO_INODE_FOUND;
}

void addToRootDirectoryDataBlock(int currendisk, int blockCount, char* name, int inodeNumber){
    //create FileInfo and write on the last bNum the name and inode
    struct FileInfo fp;
    int i;

    strncpy(fp.filename,name,8);
    fp.inodeNumber = inodeNumber;

    //add to block at first free position
    for(i = 0; i < OPEN_MAX;i++){
        if (strlen(rootDirectoryFiles[i].filename) == 0) {
            // Found an empty slot
            rootDirectoryFiles[i] = fp;
            break;
        }
    }

    writeBlock(currendisk,blockCount-1,rootDirectoryFiles);
}

void removeFromRootDirectoryDataBlock(int currendisk, int blockCount,char *filename){

    int i;
    for(i = 0; i < OPEN_MAX;i++){
        if (strcmp(rootDirectoryFiles[i].filename, "") == 0) {
            break;
        }
        else if (strcmp(rootDirectoryFiles[i].filename,filename) == 0) {
            // Found file to remove
            int j;
            for (j = i; j < OPEN_MAX - 1; j++) {
                // Shift elements to the left to fill the gap
                rootDirectoryFiles[j] = rootDirectoryFiles[j + 1];
            }
            break;
        }
    }

    writeBlock(currendisk,blockCount-1,rootDirectoryFiles);
}

int nextFreeBlock(char* bitmap){
    int i;
    for(i = 0; i < strlen(bitmap); i++){
        if(bitmap[i] == 'F'){
            return i;
        }
    }

    return NO_FREE_BLOCKS_LEFT;
}

int doesNotExistOnDisk(char*name,int disk,int FStotalBlockCount){
    struct FileInfo filesOnDisk[OPEN_MAX];
    int i;
    //should read the root directory datablock last bNum and check all the name inode pairs
    if(readBlock(disk,FStotalBlockCount-1, filesOnDisk) == READBLK_FAILED){
        return DOES_NOT_EXIST;
    }

    //check if file already exists on disk by checking the root directory entries
    for(i = 0; i < OPEN_MAX; i++){
        int result;
        if ((result = strcmp(filesOnDisk[i].filename,name)) == 0){
            return result;
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
            return ALREADY_OPEN;
        }
        current = current->next;
    }

    return NOT_ALREADY_OPEN;
}

struct dynamic_file_table *findFileInTable(fileDescriptor FD,struct dynamic_file_table *head){
    struct dynamic_file_table *current = head;

    // Traverse the dynamic resource table
    while (current != NULL) {
        if (current->fd == FD) {
            // File with the given name is already open
            return current;
        }
        current = current->next;
    }

    return NULL;
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

void removefromDynamicResourceTable(struct dynamic_file_table *head, fileDescriptor FD){
    struct dynamic_file_table *current = head;

    // Handle the case where the value to remove is the head
    if (current != NULL && current->fd == FD) {
        fileHead = current->next;  // Update the head pointer
        free(current);  // Free the memory occupied by the current node
        return;
    }

    while(current->next!=NULL){
        if (current->next != NULL && current->next->fd == FD) {
            struct dynamic_file_table *nodeToRemove = current->next;
            current->next = nodeToRemove->next;  // Update the pointers to bypass the node to remove
            free(nodeToRemove);  // Free the memory occupied by the node to remove
            return;
        }
        current = current->next;
    }
}

void addFileToDynamicResourceTable(char *filename,int inodebNum, fileDescriptor filesfd, int fp) {
    // Create a new entry
    struct dynamic_file_table *newEntry = malloc(sizeof(struct dynamic_file_table));
    newEntry->filename = filename;
    newEntry->inodebNum = inodebNum;
    newEntry->fd = filesfd;
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
    struct Inode *inode = malloc(sizeof(struct Inode));
    inode->fileSize = fileStat.st_size;
    inode->inodeNumber = 1;
    inode->accessControl = 0755;
    inode->referenceCount = 0;
    inode->fileType = "Directory";
    inode->creationTime = fileStat.st_ctimespec;
    //Last bNum is for root directory data block that stores name - inode pairs
    inode->startDatablock = end;
    inode->endDatablock = -1;
    writeBlock(disk,1,inode);

    //Superblock structure at bNum 0
    //magic number
    struct Superblock *sb = malloc(sizeof(struct Superblock));
    sb->magicNumber = 0x5A;
    sb->blockCount = blockCount;
    //create the inode block sector and data block sectors bitmaps for superblock
    // Compute the inode bitmap and copy it to the superblock
    char* inodeBitmap = computebitmap(disk, 1, midpoint);
    strncpy(sb->inodebitmap, inodeBitmap, strlen(inodeBitmap));
    free(inodeBitmap);

    // Compute the free data block bitmap and copy it to the superblock
    char* dataBlockBitmap = computebitmap(disk, midpoint + 1, end);
    strncpy(sb->freedatablockbitmap, dataBlockBitmap, strlen(dataBlockBitmap));
    free(dataBlockBitmap);

    //write superblock onto disk
    writeBlock(disk, 0, sb);
    free(buffer);
    free(inode);
    free(sb);
    closeDisk(disk);

    return 0; //Success
}

/* tfs_mount(char *filename) “mounts” a TinyFS file system located within ‘filename’.
 * tfs_unmount(void) “unmounts” the currently mounted file system. As part of the mount
 * operation, tfs_mount should verify the file system is the correct type. Only one file
 * system may be mounted at a time. Use tfs_unmount to cleanly unmount the currently
 * mounted file system. Must return a specified success/error code. */
int tfs_mount(char *filename){
    int disk;

    if(filename == NULL){
        filename = DEFAULT_DISK_NAME;
    }

    currentDiskName = filename;

    //opens disk for reading
    if((disk = openDisk(filename,0)) == OPENDISK_FAILED){
        return MOUNT_FAILED;
    }
    currentOpendisk = disk;

    if(isMounted != 0){
        perror("Cannot Mount multiple file systems!");
        return MOUNT_FAILED;
    }

    // Read the superblock from the disk
    struct Superblock *sb = malloc(sizeof(struct Superblock));
    if (readBlock(currentOpendisk, 0, sb) != 0) {
        return MOUNT_FAILED;
    }

    // Verify the file system type using the magic number
    if (sb->magicNumber != 0x5A) {
        return MOUNT_FAILED;
    }

    // Mark the file system as mounted
    isMounted = 1;
    free(sb);

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
        struct Inode *inode = malloc(sizeof(struct Inode));
        struct Superblock *sb = malloc(sizeof (struct Superblock));
        int inodeLocation = -1;

        //check name is not too long
        if(strlen(name) > 8){
            perror("Filename trying to be open is too long");
            return TFS_OPEN_FAILED;
        }

        //Check if file is not already opened in current FS
        if(alreadyOpen(name,fileHead) == ALREADY_OPEN){
            perror("File already opened or does not exist on FS");
            return TFS_OPEN_FAILED;
        }

        //read in superblock bitmap
        if(readBlock(currentOpendisk,0,sb) == READBLK_FAILED){
            perror("Superblock corrupted");
            return TFS_OPEN_FAILED;
        }

        //Check if file exists in FS root directory if not create and add file
        if(doesNotExistOnDisk(name,currentOpendisk,sb->blockCount) == DOES_NOT_EXIST){
            struct timespec current_time;

            if (clock_gettime(CLOCK_REALTIME, &current_time) == -1) {
                perror("clock_gettime failed");
                return TFS_OPEN_FAILED;
            }
            //create Inode and add a data block on disk
            inode->fileSize = 0;
            inode->inodeNumber = fileInodeNumbers++;
            inode->accessControl = 0755;
            inode->referenceCount = 0;
            inode->fileType = "File";
            //allocate empty datablock on disk
            inode->startDatablock = nextFreeBlock(sb->freedatablockbitmap);
            inode->endDatablock = -1;
            inode->creationTime = current_time;

            //allocate placeholder data for where the files data should go to keep superblock bitmaps up to date
            inodeLocation = nextFreeBlock(sb->inodebitmap)+1;
            writeBlock(currentOpendisk,inodeLocation,inode);
            writeBlock(currentOpendisk, strlen(sb->inodebitmap)+inode->startDatablock+1,"Placeholder");

            //add name inode pair to root directory data block
            addToRootDirectoryDataBlock(currentOpendisk,sb->blockCount,name,inode->inodeNumber);

            //update the superblock bitmaps
            char* inodes = computebitmap(currentOpendisk,1,sb->blockCount/2);
            char* datablocks = computebitmap(currentOpendisk,(sb->blockCount/2) + 1, sb->blockCount-1);
            int inodelength = sb->blockCount/2;
            int datablocklength = (sb->blockCount-1) - ((sb->blockCount/2) + 1) + 1;
            strncpy(sb->inodebitmap, inodes, inodelength);
            strncpy(sb->freedatablockbitmap, datablocks, datablocklength);
            writeBlock(currentOpendisk,0,sb);
            free(inodes);
            free(datablocks);
        }
        else {
            //find inodeLocation with the inode number from root Directory
            int inodeNum = getInodeNumberFromRootDirectory(name);
            //get inode Location on disks
            inodeLocation = getInodeLocation(currentOpendisk,inodeNum,sb->blockCount);
        }

        //add entry to dynamic resource table to show it is open
        fd++;
        addFileToDynamicResourceTable(name,inodeLocation,fd,0);
        free(inode);
        free(sb);

        return fd;
}

/* Closes the file and removes dynamic resource table entry */
int tfs_close(fileDescriptor FD){
    //remove the file from the dynamic resource table
    removefromDynamicResourceTable(fileHead,FD);

    //free up the fd number
    fd--;

    return  0;
}

/* Writes buffer ‘buffer’ of size ‘size’, which represents an entire file’s contents, to
 * the file described by ‘FD’. Sets the file pointer to 0 (the start of file) when done.
 * Returns success/error codes. */
int tfs_write(fileDescriptor FD, char *buffer, int size){

    //read in superblock
    struct Superblock *sb = malloc(sizeof (struct Superblock));
    if(readBlock(currentOpendisk,0,sb) == READBLK_FAILED){
        perror("Superblock corrupted");
        return TFS_DELETE_FAILED;
    }

    //with fd find file info resource
    struct dynamic_file_table* entry = findFileInTable(FD,fileHead);
    if(entry==NULL){
        return TFS_WRITE_FAILED;
    }

    //read in inode at bNum
    struct Inode *inode = malloc(sizeof (struct Inode));
    if(readBlock(currentOpendisk,entry->inodebNum,inode) == READBLK_FAILED){
        return TFS_DELETE_FAILED;
    }

    // Check if writing exceeds the available space
    if (size > strlen(sb->freedatablockbitmap) *BLOCK_SIZE) {
        perror("Trying to write too much, try to disk");
        free(sb);
        free(inode);
        return TFS_WRITE_FAILED;
    }

    // Use Current fp to update inode endblock and write buffer onto disk
    char *data = calloc(BLOCK_SIZE,sizeof(char));
    int offset = inode->startDatablock + (entry->fp / BLOCK_SIZE);
    readBlock(currentOpendisk,offset,data);
    strncpy(data,buffer,size);
    writeBlock(currentOpendisk,1 + strlen(sb->inodebitmap) + offset,data);

    //mark the continguos skipped blocks as owned by inode with some placeholder data
    for(int i = inode->startDatablock+1; i < offset; i++){
        writeBlock(currentOpendisk,1 + strlen(sb->inodebitmap)+i,"Placeholder");
    }

    //update necessay inode metaday
    if(inode->endDatablock < offset){
        inode->endDatablock = offset;
    }
    inode->fileSize= inode->endDatablock;
    writeBlock(currentOpendisk,entry->inodebNum,inode);

    // Reset the file pointer to the start of the file
    entry->fp = 0;

    //update superblock bitmaps
    char* inodes = computebitmap(currentOpendisk,1,sb->blockCount/2);
    char* datablocks = computebitmap(currentOpendisk,(sb->blockCount/2) + 1, sb->blockCount-1);
    int inodelength = sb->blockCount/2;
    int datablocklength = (sb->blockCount-1) - ((sb->blockCount/2) + 1) + 1;
    strncpy(sb->inodebitmap, inodes, inodelength);
    strncpy(sb->freedatablockbitmap, datablocks, datablocklength);
    writeBlock(currentOpendisk,0,sb);
    free(inodes);
    free(datablocks);

    free(sb);
    free(inode);
    free(data);
    return 0;
}

/* reads one byte from the file and copies it to ‘buffer’, using the current file
 * pointer location and incrementing it by one upon success. If the file pointer
 * is already at the end of the file then tfs_readByte() should return an error
 * and not increment the file pointer. */
int tfs_readByte(fileDescriptor FD, char *buffer){


    //read in superblock
    struct Superblock *sb = malloc(sizeof (struct Superblock));
    if(readBlock(currentOpendisk,0,sb) == READBLK_FAILED){
        perror("Superblock corrupted");
        return TFS_READ_BYTE_FAILED;
    }

    // Find the file in the file table using the file descriptor
    struct dynamic_file_table* entry = findFileInTable(FD, fileHead);

    //read in inode at bNum
    struct Inode *inode = malloc(sizeof (struct Inode));
    if(readBlock(currentOpendisk,entry->inodebNum,inode) == READBLK_FAILED){
        return TFS_READ_BYTE_FAILED;
    }

    // Check if the file pointer is already at the end of the file
    int fpblock = entry->fp/BLOCK_SIZE;
    if (fpblock == inode->endDatablock && entry->fp == BLOCK_SIZE) {
        // File pointer is at or beyond the end of the file, return error
        return TFS_READ_BYTE_FAILED;
    }

    //read current byte from current fp
    char data[BLOCK_SIZE];
    if(readBlock(currentOpendisk,1+strlen(sb->inodebitmap)+fpblock,data) == READBLK_FAILED){
        return TFS_READ_BYTE_FAILED;
    }

    *buffer = data[entry->fp % BLOCK_SIZE];

    // Increment the file pointer by one
    entry->fp++;

    free(sb);
    free(inode);

    return 0; // Success
}


/* deletes a file and marks its blocks as free on disk. */
int tfs_delete(fileDescriptor FD){

    //read in superblock
    struct Superblock *sb = malloc(sizeof (struct Superblock));
    if(readBlock(currentOpendisk,0,sb) == READBLK_FAILED){
        perror("Superblock corrupted");
        return TFS_DELETE_FAILED;
    }

    //remove from resource table if open already
    struct dynamic_file_table* entry = findFileInTable(FD,fileHead);
    if(entry ==NULL){
        return TFS_DELETE_FAILED;
    }

    //read in inode
    struct Inode *inode = malloc(sizeof (struct Inode));
    if(readBlock(currentOpendisk,entry->inodebNum,inode) == READBLK_FAILED){
        return TFS_DELETE_FAILED;
    }

    //for clearing blocks
    char *data = calloc(sizeof(char), BLOCK_SIZE);

    //clear inode on disk
    writeBlock(currentOpendisk, entry->inodebNum, data);

    //clear all the datablocks inode owns on disk
    for(int i = inode->startDatablock; i <= inode->endDatablock; i++){
        writeBlock(currentOpendisk, i + strlen(sb->inodebitmap) + 1, data);
    }

    //remove file from root directory data block then from resource table
    removeFromRootDirectoryDataBlock(currentOpendisk,sb->blockCount,entry->filename);

    removefromDynamicResourceTable(fileHead,FD);
    //free up filedescriptor
    fd--;

    //update the superblock bit maps
    char* inodes = computebitmap(currentOpendisk,1,sb->blockCount/2);
    char* datablocks = computebitmap(currentOpendisk,(sb->blockCount/2) + 1, sb->blockCount-1);
    int inodelength = sb->blockCount/2;
    int datablocklength = (sb->blockCount-1) - ((sb->blockCount/2) + 1) + 1;
    strncpy(sb->inodebitmap, inodes, inodelength);
    strncpy(sb->freedatablockbitmap, datablocks, datablocklength);
    writeBlock(currentOpendisk,0,sb);
    free(inodes);
    free(datablocks);
    free(data);

    return  0;
}

/* change the file pointer location to offset (absolute). Returns success/error codes.*/
int tfs_seek(fileDescriptor FD, int offset){
    //update the file pointer to offset
    struct dynamic_file_table* entry = findFileInTable(FD,fileHead);

    entry->fp = offset;

    return 0;
}

/* prints the file’s creation time as string */
void tfs_stat(fileDescriptor FD){
    //read the inode the fd belongs then print the creation time
    struct dynamic_file_table* entry = findFileInTable(FD,fileHead);

    if(entry==NULL){
        return;
    }

    //read in inode
    struct Inode *inode = malloc(sizeof (struct Inode));
    if(readBlock(currentOpendisk,entry->inodebNum,inode) == READBLK_FAILED){
        return;
    }
    char* timestampStr = ctime((const time_t *) &inode->creationTime);

    printf("Timestamp: %s", timestampStr);
}

/* prints all the files in the root directory on the disk */
void tfs_readdir(){
    int i;

    for(i = 0; i < OPEN_MAX;i++){
        if (strcmp(rootDirectoryFiles[i].filename, "") == 0) {
            break;
        }
        printf("%s \n",rootDirectoryFiles[i].filename);
    }
}

/* Renames a file.  New name should be passed in. */
void tfs_rename(char *oldName,char* newName){
    //update directory name of file
    int i;

    for(i = 0; i < OPEN_MAX;i++){
        if (strcmp(rootDirectoryFiles[i].filename, "") == 0) {
            break;
        }
        else if(strcmp(rootDirectoryFiles[i].filename,oldName) == 0){
            strncpy(oldName,newName,strlen(newName));
        }
    }
}

