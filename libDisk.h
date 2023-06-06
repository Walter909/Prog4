//
// Created by Walter Toriola on 5/31/23.
//

#ifndef PROG4_LIBDISK_H
#define PROG4_LIBDISK_H

#define BLOCK_SIZE 256
#define DEFAULT_DISK_SIZE 10240
#define DEFAULT_DISK_NAME "tinyFSDisk"

int writeBlock(int disk, int bNum, void *block);

int readBlock(int disk, int bNum, void *block);

void closeDisk(int disk);

int openDisk(char *filename, int nbytes);

#endif //PROG4_LIBDISK_H


