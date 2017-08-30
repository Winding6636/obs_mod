project(win-bgtexture)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")

set(win-bgtexture_SOURCES
	bgtexture.cpp)

add_executable(win-bgtexture WIN32
	${win-bgtexture_SOURCES})
target_link_libraries(win-bgtexture
	libobs)
define_graphic_modules(win-bgtexture)
