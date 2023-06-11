//
// Created by Walter Toriola on 6/4/23.
//

#include "libTinyFS.h"

int main(int argc, char *argv[]) {
    // Program code goes here
    tfs_mkfs("/Users/walte/CLionProjects/Prog4/testFile",4096);

    tfs_mount("/Users/walte/CLionProjects/Prog4/testFile");

    int fd1 = tfs_open("target1");

    int fd2 = tfs_open("target2");

    tfs_seek(fd1,600);

    tfs_write(fd1,"Wassup",6);

    tfs_write(fd1,"Should not fail",11);

    tfs_write(fd2,"Should not fail",11);

    char data[1];
    tfs_readByte(fd1,data);

    tfs_stat(fd1);

    tfs_delete(fd1);

    tfs_readdir();

    tfs_rename("target2","File1");

    tfs_readdir();

    tfs_close(fd1);

    tfs_unmount();

    return 0; // Return 0 to indicate successful program execution
}