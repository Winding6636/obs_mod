project(bgtexture)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")

if(MSVC)
	set(bgtexture_PLATFORM_DEPS
		w32-pthreads)
endif()

set(bgtexture_SOURCES
	${bgtexture_PLATFORM_SOURCES}
	bgtexture.c)

add_library(bgtexture MODULE
	${bgtexture_SOURCES})

target_link_libraries(bgtexture
	${bgtexture_PLATFORM_DEPS}
	libobs)

install_obs_plugin_with_data(bgtexture data)
