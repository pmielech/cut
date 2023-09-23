cmake_minimum_required(VERSION 3.26)
project(cut C)
set(CMAKE_C_STANDARD 11)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/include)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(cut main.c)
target_link_libraries(cut ${CURSES_LIBRARIES})

