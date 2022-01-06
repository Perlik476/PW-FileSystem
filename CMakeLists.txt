cmake_minimum_required(VERSION 3.8)
project(MIMUW-FORK C)

set(CMAKE_CXX_STANDARD "17")
set(CMAKE_C_STANDARD "11")
set(CMAKE_C_FLAGS "-g -Wall -Wextra -Wno-sign-compare")

add_library(err src/err.c)
add_library(HashMap src/HashMap.c)
add_library(Tree src/Tree.c)
add_library(path_utils src/path_utils.c)
add_executable(main src/main.c)
target_link_libraries(main Tree HashMap err pthread path_utils)

install(TARGETS DESTINATION src)
