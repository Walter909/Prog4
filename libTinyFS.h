//
// Created by Walter Toriola on 6/1/23.
//

#ifndef PROG4_LIBTINYFS_H
#define PROG4_LIBTINYFS_H

#include <sys/stat.h>

#define OPEN_MAX 100
#define FILE_DATABLOCKS 5

typedef int fileDescriptor;

struct dynamic_file_table{
    char *filename;
    int inodebNum;
    fileDescriptor fd;
    int fp;
    struct dynamic_file_table *next;
};

struct Superblock {
    char magicNumber;
    int blockCount;
    char freedatablockbitmap[100];
    char inodebitmap[100];
    char padding[54];  //Placeholder for padding
};

struct Inode {
    int fileSize;
    int inodeNumber;
    int accessControl;
    int referenceCount;
    char *fileType;
    struct timespec creationTime;
    //data block bNum location
    int startDatablock;
    int endDatablock;
    //char padding[124];  Add padding array to make the struct 256 bytes
};

struct FileInfo {
    char filename[8];
    int inodeNumber;
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
void tfs_stat(fileDescriptor FD);
void tfs_readdir();
void tfs_rename(char *oldName,char* newName);

#endif //PROG4_LIBTINYFS_H
