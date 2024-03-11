cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_STANDARD 99)
project(CpuUsageTracker VERSION 1.0)

set(FLAGS -Weverything -g)

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

include_directories(${CURSES_INCLUDE_DIR})
include_directories(${THREADS_INCLUDE_DIR})

set(LIBS ${CURSES_LIBRARIES} ${THREADS_LIBRARIES})

add_executable(${PROJECT_NAME} main.c)

target_link_libraries(${PROJECT_NAME} ${LIBS})

