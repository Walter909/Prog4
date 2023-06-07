//
// Created by Walter Toriola on 6/1/23.
//

#ifndef PROG4_LIBTINYFS_H
#define PROG4_LIBTINYFS_H

#include <sys/stat.h>

typedef int fileDescriptor;

struct dynamic_file_table{
    char *filename;
    int inodeNumber;
    fileDescriptor fd;
    long size;
    int fp;
    struct dynamic_file_table *next;
};

struct Superblock {
    char magicNumber;
    int blockCount;
    char *freedatablockbitmap;
    char *inodebitmap;
    char padding[1];  // Placeholder for padding
};

struct Inode {
    off_t fileSize;
    int inodeNumber;
    int accessControl;
    int referenceCount;
    char* fileType;
    struct timespec creationTime;
    //data block bNum location
    int datablock;
    char padding[164];  // Add padding array to make the struct 256 bytes
};

int tfs_mkfs(char *filename, int nbytes);
int tfs_mount(char *diskname);
int tfs_unmount(void);
fileDescriptor tfs_open(char *name);
int tfs_close(fileDescriptor FD);
int tfs_write(fileDescriptor FD, char *buffer, int size);
int tfs_delete(fileDescriptor FD);
int tfs_readByte(fileDescriptor FD, char *buffer);
int tfs_seek(fileDescriptor FD, int offset);

#endif //PROG4_LIBTINYFS_H
