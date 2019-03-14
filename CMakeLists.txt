cmake_minimum_required(VERSION 3.13)
project(nm_otool)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(src)

add_executable(nm_otool
        src/nm.c
        src/nmp.h
        src/ofile.c
        src/ofilep.h
        src/otool.c
        src/otoolp.h
        README.md)
