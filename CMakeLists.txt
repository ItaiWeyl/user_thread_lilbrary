cmake_minimum_required(VERSION 3.22)
project(Ex2)

set(CMAKE_CXX_STANDARD 11)

add_executable(Ex2 main.cpp
        demo_singInt_handler.c
        uthreads.h
        thread.cpp
        thread.h)
