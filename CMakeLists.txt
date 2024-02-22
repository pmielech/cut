cmake_minimum_required(VERSION 3.26)
project(cut C)
set(CMAKE_C_STANDARD 11)


set(MY_DOWNLOAD_PATH "${PROJECT_SOURCE_DIR}/include")
set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/lib")
add_library(pthread STATIC IMPORTED)
INSTALL (
        DIRECTORY ${CMAKE_SOURCE_DIR}/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h*")

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(cut main.c)
target_link_libraries(cut ${CURSES_LIBRARIES})

