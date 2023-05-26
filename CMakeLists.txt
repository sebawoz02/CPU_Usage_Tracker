cmake_minimum_required(VERSION 3.22.1)
project(CPU_Usage_Tracker C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_COMPILER "clang")
set(CMAKE_C_FLAGS "-Weverything -Wno-declaration-after-statement -Wno-unused-parameter -pthread")

add_library(reader reader.h reader.c)
add_library(analyzer analyzer.h analyzer.c)
add_library(queue queue.h queue.c)

add_executable(CUT main.c)
add_executable(test tests/test_main.c tests/test_queue.h tests/test_queue.c tests/test_reader.c tests/test_reader.h)

target_link_libraries(CUT PRIVATE reader)
target_link_libraries(CUT PRIVATE queue)
target_link_libraries(CUT PRIVATE analyzer)

target_link_libraries(test PRIVATE reader)
target_link_libraries(test PRIVATE queue)
