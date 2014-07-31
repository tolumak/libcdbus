cmake_minimum_required(VERSION 2.6)
project(libcdbus)

include(version.cmake)
include(log.cmake)
include(dbus.cmake)

# Options
set(BUILD_TEST_APP NO CACHE BOOL "Build test app")

configure_file (
  "config.h.in"
  "config.h"
)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})

# Libutils

set(SRCS libcdbus.c list.c)

version_file_c(SRCS)

add_library(cdbus SHARED ${SRCS})
version_add_dependencies(cdbus)
target_link_libraries(cdbus dbus-1)
install(TARGETS cdbus DESTINATION usr/lib)

if (BUILD_TEST_APP)
# Test exectutable
set(TEST_SRCS test.c fr_sise_test.c)
add_cdbus_object(TEST_SRCS fr/sise/test ${PROJECT_SOURCE_DIR}/test_introspect.xml)
add_executable(test-service ${TEST_SRCS})
target_link_libraries(test-service cdbus dbus-1)
endif (BUILD_TEST_APP)
