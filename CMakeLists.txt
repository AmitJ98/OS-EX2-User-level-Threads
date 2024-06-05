cmake_minimum_required(VERSION 3.22)
project(Os_Ex2)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(Os_Ex2
        demo_singInt_handler.c
        thread.cpp
        thread.h
        uthreads.cpp
        uthreads.h)
