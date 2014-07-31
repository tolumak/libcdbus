set(USE_DBUS_SYSTEM_BUS true CACHE BOOL "Use D-Bus System Bus")

if($ENV{DBUS_SESSION_BUS})
set(USE_DBUS_SYSTEM_BUS false CACHE BOOL "Use D-Bus System Bus" FORCE)
else($ENV{DBUS_SESSION_BUS})
set(USE_DBUS_SYSTEM_BUS true CACHE BOOL "Use D-Bus System Bus")
endif($ENV{DBUS_SESSION_BUS})

macro(add_cdbus_object SRCS OBJECT XML)
string(REPLACE "/" "_" _object ${OBJECT})
add_custom_command(OUTPUT ${_object}.c ${_object}.h
			   COMMAND ${PROJECT_SOURCE_DIR}/xml2cdbus.py ${XML}
			   DEPENDS ${PROJECT_SOURCE_DIR}/xml2cdbus.py ${XML})
list(APPEND ${SRCS} ${_object}.c)
endmacro(add_cdbus_object)

include_directories(${CMAKE_SYSROOT}/usr/include/dbus-1.0/)
include_directories(${CMAKE_SYSROOT}/usr/lib/dbus-1.0/include)
include_directories(${CMAKE_SYSROOT}/usr/lib64/dbus-1.0/include)
