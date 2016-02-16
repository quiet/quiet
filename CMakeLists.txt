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

set(SRCFILES src/demodulator.c src/modulator.c src/utility.c src/decode.c src/encode.c src/profile.c)

add_library(quiet ${SRCFILES})
target_link_libraries(quiet liquid jansson)
add_library(quiet_shared MODULE ${SRCFILES})
target_link_libraries(quiet_shared liquid jansson)

add_executable(encodefile EXCLUDE_FROM_ALL programs/encodefile.c)
target_link_libraries(encodefile quiet sndfile)

add_executable(decodefile EXCLUDE_FROM_ALL programs/decodefile.c)
target_link_libraries(decodefile quiet sndfile)

add_custom_target(programs DEPENDS encodefile decodefile)

add_executable(test_integration tests/integration.c)
target_link_libraries(test_integration quiet)
add_test(NAME integration_test COMMAND test_integration)

enable_testing()
