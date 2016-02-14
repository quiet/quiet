cmake_minimum_required (VERSION 2.6)
project (Quiet)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wpedantic -Werror -Wall -D_XOPEN_SOURCE=700")

if(APPLE)
    set(CMAKE_SKIP_BUILD_RPATH  false)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH false)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH true)
    set(CMAKE_MACOSX_RPATH on)
endif()

add_library(quiet src/encode.c)
target_link_libraries(quiet liquid jansson)
add_library(quiet_shared MODULE src/encode.c)
target_link_libraries(quiet_shared liquid jansson)

add_executable(encodefile programs/encodefile.c)
target_link_libraries(encodefile quiet sndfile)

add_executable(decodefile programs/decodefile.c)
target_link_libraries(decodefile quiet sndfile)
