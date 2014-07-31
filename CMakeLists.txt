cmake_minimum_required(VERSION 2.6)
project(libcdbus)

include($ENV{SISE_ENV_APPS_ROOT}/common/defaults.cmake)
include($ENV{SISE_ENV_APPS_ROOT}/common/version.cmake)
include($ENV{SISE_ENV_APPS_ROOT}/common/log.cmake)
include($ENV{SISE_ENV_APPS_ROOT}/common/dbus.cmake)

# Options
set(BUILD_TEST_APP NO CACHE BOOL "Build test app")

configure_file (
  "${PROJECT_SOURCE_DIR}/../common/config.h.in"
  "${PROJECT_BINARY_DIR}/common-config.h"
)

# Libutils
set(LIBUTILS_PTHREAD_LOCK ON FORCE)
add_subdirectory($ENV{SISE_ENV_APPS_ROOT}/libutils libutils)
include_directories(${libutils_SOURCE_DIR})
include_directories(${libutils_BINARY_DIR})


include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})
include_directories($ENV{SISE_ENV_APPS_ROOT}/common)


set(SRCS libcdbus.c)

version_file_c(SRCS)

add_library(cdbus SHARED ${SRCS})
version_add_dependencies(cdbus)
target_link_libraries(cdbus dbus-1 utils)
install(TARGETS cdbus DESTINATION usr/lib)

if (BUILD_TEST_APP)
# Test exectutable
set(TEST_SRCS test.c fr_sise_test.c)
add_cdbus_object(TEST_SRCS fr/sise/test ${PROJECT_SOURCE_DIR}/test_introspect.xml)
add_executable(test-service ${TEST_SRCS})
target_link_libraries(test-service cdbus dbus-1)
endif (BUILD_TEST_APP)
