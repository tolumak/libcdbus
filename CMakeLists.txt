cmake_minimum_required(VERSION 2.6)
project(libcdbus)

include(../common/defaults.cmake)
include(../common/version.cmake)
include(../common/log.cmake)
include(../common/dbus.cmake)

# Options
set(BUILD_TEST_APP NO CACHE BOOL "Build test app")

configure_file (
  "${PROJECT_SOURCE_DIR}/../common/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/../common)
include_directories(${CMAKE_SISE_ENV_SYSROOT}/usr/include/dbus-1.0/)
include_directories(${CMAKE_SISE_ENV_SYSROOT}/usr/lib/dbus-1.0/include)

set(SRCS libcdbus.c ../common/list.c)

version_file_c(SRCS)

add_library(cdbus SHARED ${SRCS})
version_add_dependencies(cdbus)
target_link_libraries(cdbus dbus-1)
install(TARGETS cdbus DESTINATION usr/lib)

if (BUILD_TEST_APP)
# Test exectutable
set(TEST_SRCS main.c fr_sise_test.c)
add_cdbus_object(TEST_SRCS fr/sise/test ${PROJECT_SOURCE_DIR}/test_introspect.xml)
add_executable(test ${TEST_SRCS})
target_link_libraries(test cdbus)
endif (BUILD_TEST_APP)
