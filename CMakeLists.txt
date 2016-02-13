cmake_minimum_required (VERSION 2.6)
project (Quiet)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wpedantic -Werror -Wall")

add_library(quiet src/encode.c include/quiet.h)
target_link_libraries(quiet liquid jansson)
add_library(quiet_shared SHARED src/encode.c)
target_link_libraries(quiet_shared liquid jansson)

add_executable(encodefile programs/encodefile.c)
target_link_libraries(encodefile quiet sndfile)

add_executable(decodefile programs/decodefile.c)
target_link_libraries(decodefile quiet sndfile)
