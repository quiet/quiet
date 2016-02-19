cmake_minimum_required (VERSION 2.6)
project (Quiet)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wpedantic -Werror -Wall -D_XOPEN_SOURCE=700")
set(SRCFILES src/demodulator.c src/modulator.c src/utility.c src/decoder.c src/encoder.c src/profile.c)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_custom_target(quiet-h ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/include/quiet.h ${CMAKE_BINARY_DIR}/include/quiet.h)

add_custom_target(quiet-profiles ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/profiles.json ${CMAKE_BINARY_DIR}/share/quiet-profiles.json)

add_library(quiet_static ${SRCFILES})
target_link_libraries(quiet_static liquid jansson)
add_library(quiet MODULE ${SRCFILES})
target_link_libraries(quiet liquid jansson)

add_custom_target(lib DEPENDS quiet quiet_static)

add_executable(encode_file programs/encode_file.c)
target_link_libraries(encode_file quiet_static sndfile)

add_executable(decode_file programs/decode_file.c)
target_link_libraries(decode_file quiet_static sndfile)

add_executable(encode_soundcard programs/encode_soundcard.c)
target_link_libraries(encode_soundcard quiet_static portaudio)

add_executable(decode_soundcard programs/decode_soundcard.c)
target_link_libraries(decode_soundcard quiet_static portaudio)

add_custom_target(programs DEPENDS encode_file decode_file encode_soundcard decode_soundcard)

add_custom_target(quiet-js COMMAND emcc -O2 $<TARGET_FILE:quiet_static> -o quiet-emscripten.js -s EXPORTED_FUNCTIONS=\"['_quiet_decoder_recv', '_quiet_decoder_create', '_quiet_decoder_max_len', '_quiet_decoder_flush', '_quiet_decoder_readbuf', '_quiet_decoder_destroy', '_quiet_encoder_emit', '_quiet_encoder_create', '_quiet_encoder_destroy', '_quiet_encoder_set_payload', '_quiet_encoder_profile_str', '_quiet_decoder_profile_str', '_quiet_encoder_opt_set_sample_rate', '_quiet_decoder_opt_set_sample_rate', '_quiet_encoder_clamp_frame_len']\" -lliquid -lfec -ljansson)

add_executable(integration_test_runner EXCLUDE_FROM_ALL tests/integration.c)
target_link_libraries(integration_test_runner quiet_static)
set_target_properties(integration_test_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "tests")
add_test(NAME integration_test WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests" COMMAND integration_test_runner)

add_custom_target(test-profiles COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/tests/test-profiles.json ${CMAKE_BINARY_DIR}/tests/test-profiles.json)
add_custom_target(cp-test-profiles DEPENDS test-profiles)
add_dependencies(integration_test_runner cp-test-profiles)

add_custom_target(test_runners DEPENDS integration_test_runner)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS test_runners)
enable_testing()
