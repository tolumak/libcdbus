cmake_minimum_required(VERSION 2.6)
project(libcdbus)

# Options
set(LOG_LEVEL 4 CACHE STRING "Log level")

configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${CMAKE_MV2_ENV_SYSROOT}/usr/include/dbus-1.0/)
include_directories(${CMAKE_MV2_ENV_SYSROOT}/usr/lib/dbus-1.0/include)
set(SRCS libcdbus.c list.c)

add_library(cdbus SHARED ${SRCS})
target_link_libraries(cdbus dbus-1)


# Test exectutable
add_custom_command(OUTPUT org_gnome_ServiceName.c
			  COMMAND ${PROJECT_SOURCE_DIR}/xml2cdbus.py ${PROJECT_SOURCE_DIR}/test_introspect.xml
			  DEPENDS ${PROJECT_SOURCE_DIR}/xml2cdbus.py ${PROJECT_SOURCE_DIR}/test_introspect.xml)

set(TEST_SRCS main.c org_gnome_ServiceName.c)

add_executable(test ${TEST_SRCS})
target_link_libraries(test cdbus)
