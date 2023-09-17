cmake_minimum_required(VERSION 3.26)
project(cut C)
include_directories(cut include)
set(CMAKE_C_STANDARD 11)

add_executable(cut main.c)
