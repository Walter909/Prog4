cmake_minimum_required(VERSION 3.24)
project(Prog4 C)

set(CMAKE_C_STANDARD 23)

add_executable(Prog4 libDisk.c libDisk.h libTinyFS.c libTinyFS.h Errors.h tinyFSDemo.c)
