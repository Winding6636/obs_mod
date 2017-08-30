project(unix-bgcolor)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")

find_library(COCOA Cocoa)
include_directories(${COCOA})

add_definitions(-fobjc-arc)

set(unix-bgcolor_SOURCES
	bgcolor.mm)

add_executable(unix_bgcolor
	${unix-bgcolor_SOURCES})
target_link_libraries(unix_bgcolor
	libobs
	${COCOA})
define_graphic_modules(unix_bgcolor)
