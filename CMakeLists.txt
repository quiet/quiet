cmake_minimum_required (VERSION 2.6)
project (Quiet)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wpedantic -Werror -Wall")

add_library(encode encode.c encode.h)
target_link_libraries(encode liquid jansson)

add_executable(encodefile encodefile.c)
target_link_libraries(encodefile encode sndfile)

add_executable(decodefile decodefile.c)
target_link_libraries(decodefile encode sndfile)
