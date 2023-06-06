//
// Created by Walter Toriola on 6/4/23.
//
#include "libTinyFS.h"

int main(int argc, char *argv[]) {
    // Program code goes here
    tfs_mkfs("/Users/walte/CLionProjects/Prog4/testFile",4096);

    tfs_mount("/Users/walte/CLionProjects/Prog4/testFile");

    tfs_unmount();

    tfs_mount("/Users/walte/CLionProjects/Prog4/testFile");

    tfs_open("/Users/walte/CLionProjects/Prog4/target");

    return 0; // Return 0 to indicate successful program execution
}