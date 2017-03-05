cmake_minimum_required (VERSION 2.6)
project (Quiet)
include(CheckLibraryExists)
include(CheckIncludeFiles)
include(CheckCCompilerFlag)

set(QUIET_PROFILES_LOCATION "${CMAKE_INSTALL_PREFIX}/share/quiet/")
add_definitions(-DQUIET_PROFILES_LOCATION="${QUIET_PROFILES_LOCATION}quiet-profiles.json")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DQUIET_DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-no_pie")
else()
    if(EMSCRIPTEN)
    elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
      check_c_compiler_flag("-Ofast" HAVE_O_FAST)
      if(HAVE_O_FAST)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -g3")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Ofast -flto")
      else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g")
      endif()
    else()
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g")
    endif()
endif()

set(CMAKE_MACOSX_RPATH 1)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpedantic -Wall -D_XOPEN_SOURCE=700")
set(SRCFILES src/demodulator.c src/modulator.c src/utility.c src/decoder.c src/encoder.c src/profile.c src/error.c)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

set(CORE_DEPENDENCIES liquid jansson m)

set(CMAKE_THREAD_PREFER_PTHREAD ON)
find_package(Threads)
if (CMAKE_USE_PTHREADS_INIT)
  add_definitions(-DRING_BLOCKING=1)
  add_definitions(-DQUIET_PTHREAD_ERROR=1)
  set(SRCFILES ${SRCFILES} src/ring_blocking.c)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
  set(CORE_DEPENDENCIES ${CORE_DEPENDENCIES} ${CMAKE_THREAD_LIBS_INIT})
else()
  add_definitions(-DRING_BLOCKING=0)
  add_definitions(-DQUIET_PTHREAD_ERROR=0)
  set(SRCFILES ${SRCFILES} src/ring.c)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
endif()

find_path(liquid_include NAMES liquid/liquid.h)
find_library(liquid_library NAMES liquid)
find_package_handle_standard_args(Liquid DEFAULT_MSG liquid_library liquid_include)
mark_as_advanced(liquid_include liquid_library)
if(Liquid_FOUND)
    add_library(liquid UNKNOWN IMPORTED)
    set_target_properties(liquid PROPERTIES IMPORTED_LOCATION "${liquid_library}" INTERFACE_INCLUDE_DIRECTORIES "${liquid_include}")
    check_library_exists(liquid flexframesync_set_header_len,flexframegen_set_header_len,ofdmflexframesync_set_header_len,ofdmflexframegen_set_header_len,resamp_rrrf_execute_output_block,liquid_pack_array_block "" LIQUID_DEVEL_FUNCTIONS)
    if(NOT LIQUID_DEVEL_FUNCTIONS)
      unset(Liquid_FOUND CACHE)
        message(FATAL_ERROR "
libquiet requires the devel branch of libliquid but
  found a different branch installed
fetch the devel branch using
    git clone https://github.com/quiet/liquid-dsp.git -b devel --single-branch
and install it before continuing")
    endif()
else()
    message(FATAL_ERROR "
libquiet requires libliquid but cannot find it
fetch the devel branch using
    git clone https://github.com/quiet/liquid-dsp.git -b devel --single-branch
and install it before continuing")
endif()

find_path(jansson_include NAMES jansson.h)
find_library(jansson_library NAMES jansson)
find_package_handle_standard_args(Jansson DEFAULT_MSG jansson_library jansson_include)
mark_as_advanced(jansson_include jansson_library)
if(Jansson_FOUND)
    add_library(jansson UNKNOWN IMPORTED)
    set_target_properties(jansson PROPERTIES IMPORTED_LOCATION "${jansson_library}" INTERFACE_INCLUDE_DIRECTORIES "${jansson_include}")
else()
    message(FATAL_ERROR "
libquiet requires libjansson but cannot find it
fetch the latest jansson release from
    http://www.digip.org/jansson/
and install it before continuing")
endif()

find_library(PORTAUDIO portaudio)
if (PORTAUDIO)
  set(SRCFILES ${SRCFILES} src/portaudio_encoder.c src/portaudio_decoder.c)
  add_definitions(-DQUIET_PORTAUDIO=1)
  set(CORE_DEPENDENCIES ${CORE_DEPENDENCIES} portaudio)
else()
    unset(PORTAUDIO CACHE)
    message(WARNING "

libquiet could not find portaudio. this will prevent libquiet
from building its sound streaming programs. you can get portaudio from
    http://www.portaudio.com/download.html

")
endif()


add_library(quiet_static ${SRCFILES})
target_link_libraries(quiet_static ${CORE_DEPENDENCIES})
set_target_properties(quiet_static PROPERTIES OUTPUT_NAME "quiet")
add_library(quiet SHARED ${SRCFILES})
target_link_libraries(quiet ${CORE_DEPENDENCIES})
set_target_properties(quiet PROPERTIES INSTALL_RPATH "@rpath/libquiet.dylib")

add_custom_target(lib DEPENDS quiet quiet_static)

add_custom_target(quiet-h ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/include/quiet.h ${CMAKE_BINARY_DIR}/include/quiet.h)

if (PORTAUDIO)
  add_custom_target(quiet-portaudio-h ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/include/quiet-portaudio.h ${CMAKE_BINARY_DIR}/include/quiet-portaudio.h)
endif()

add_custom_target(quiet-profiles ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/quiet-profiles.json ${CMAKE_BINARY_DIR}/share/quiet-profiles.json)

set(buildable_programs "")

find_library(SNDFILE sndfile)
if (SNDFILE)
    add_executable(quiet_encode_file programs/encode_file.c)
    target_link_libraries(quiet_encode_file quiet_static sndfile ${CMAKE_THREAD_LIBS_INIT})
    set(buildable_programs ${buildable_programs} quiet_encode_file)

    add_executable(quiet_decode_file programs/decode_file.c)
    target_link_libraries(quiet_decode_file quiet_static sndfile ${CMAKE_THREAD_LIBS_INIT})
    set(buildable_programs ${buildable_programs} quiet_decode_file)
else()
    unset(SNDFILE CACHE)
    message(WARNING "

libquiet could not find libsndfile. this will prevent libquiet
from building its file generating programs. you can get libsndfile from
    http://www.mega-nerd.com/libsndfile/#Download

")
endif()

if (PORTAUDIO)
    add_executable(quiet_encode_soundcard programs/encode_soundcard.c)
    target_link_libraries(quiet_encode_soundcard quiet_static ${CMAKE_THREAD_LIBS_INIT})
    set(buildable_programs ${buildable_programs} quiet_encode_soundcard)

    add_executable(quiet_decode_soundcard programs/decode_soundcard.c)
    target_link_libraries(quiet_decode_soundcard quiet_static ${CMAKE_THREAD_LIBS_INIT})
    set(buildable_programs ${buildable_programs} quiet_decode_soundcard)
endif()

add_custom_target(programs DEPENDS ${buildable_programs})

install(FILES ${CMAKE_BINARY_DIR}/share/quiet-profiles.json DESTINATION ${QUIET_PROFILES_LOCATION})
install(DIRECTORY ${CMAKE_BINARY_DIR}/include/ DESTINATION include)
install(TARGETS ${buildable_programs}
                quiet quiet_static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

add_custom_target(docs COMMAND cldoc generate -- --output docs/html --language=c ${CMAKE_BINARY_DIR}/include/quiet.h DEPENDS quiet-h)

add_executable(integration_test_runner EXCLUDE_FROM_ALL tests/integration.c)
target_link_libraries(integration_test_runner quiet_static)
set_target_properties(integration_test_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "tests")
add_test(NAME integration_test WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests" COMMAND integration_test_runner)

add_custom_target(test-profiles COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/tests/test-profiles.json ${CMAKE_BINARY_DIR}/tests/test-profiles.json)
add_custom_target(cp-test-profiles DEPENDS test-profiles)
add_dependencies(integration_test_runner cp-test-profiles)

set(TEST_RUNNERS integration_test_runner)

if (CMAKE_USE_PTHREADS_INIT)
  add_executable(test_ring_blocking EXCLUDE_FROM_ALL tests/ring_blocking.c src/ring_blocking.c)
  target_link_libraries(test_ring_blocking ${CMAKE_THREAD_LIBS_INIT})
  set_target_properties(test_ring_blocking PROPERTIES RUNTIME_OUTPUT_DIRECTORY "tests")
  add_test(NAME ring_blocking_test WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests" COMMAND test_ring_blocking)
  set(TEST_RUNNERS ${TEST_RUNNERS} test_ring_blocking)
endif()

add_custom_target(test_runners DEPENDS ${TEST_RUNNERS})
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS test_runners)
enable_testing()
