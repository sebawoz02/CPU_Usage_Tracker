cmake_minimum_required(VERSION 3.22.1)
project(CPU_Usage_Tracker C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_COMPILER "clang")
set(CMAKE_C_FLAGS "-Weverything")

add_executable(CUT main.c queue.c)
add_executable(test tests/test_main.c tests/test_queue.h tests/test_queue.c queue.c)
