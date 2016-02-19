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

set(SRCFILES src/demodulator.c src/modulator.c src/utility.c src/decoder.c src/encoder.c src/profile.c)

add_library(quiet_static ${SRCFILES})
target_link_libraries(quiet_static liquid jansson)
add_library(quiet MODULE ${SRCFILES})
target_link_libraries(quiet liquid jansson)

add_executable(encode_file EXCLUDE_FROM_ALL programs/encode_file.c)
target_link_libraries(encode_file quiet_static sndfile)

add_executable(decode_file EXCLUDE_FROM_ALL programs/decode_file.c)
target_link_libraries(decode_file quiet_static sndfile)

add_executable(encode_soundcard EXCLUDE_FROM_ALL programs/encode_soundcard.c)
target_link_libraries(encode_soundcard quiet_static portaudio)

add_executable(decode_soundcard EXCLUDE_FROM_ALL programs/decode_soundcard.c)
target_link_libraries(decode_soundcard quiet_static portaudio)

add_custom_target(programs DEPENDS encode_file decode_file encode_soundcard decode_soundcard)

add_executable(test_integration tests/integration.c)
target_link_libraries(test_integration quiet_static)
add_test(NAME integration_test COMMAND test_integration)

add_custom_command(OUTPUT test-profiles.json
    COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/tests/test-profiles.json ${CMAKE_BINARY_DIR}/test-profiles.json)
add_custom_target(cp-test-profiles DEPENDS test-profiles.json)
add_dependencies(test_integration cp-test-profiles)

enable_testing()
