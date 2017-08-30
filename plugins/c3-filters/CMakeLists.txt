project(obs-c3-filters)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/obs-c3-filters-config.h.in"
	"${CMAKE_BINARY_DIR}/plugins/obs-c3-filters/config/obs-c3-filters-config.h")

set(obs-c3-filters_config_HEADERS
	"${CMAKE_BINARY_DIR}/plugins/obs-c3-filters/config/obs-c3-filters-config.h")
include_directories(${LIBSPEEXDSP_INCLUDE_DIRS}
	"${CMAKE_BINARY_DIR}/plugins/obs-c3-filters/config")

set(obs-c3-filters_SOURCES
	obs-c3-filters.c
	star-filter.c)

add_library(obs-c3-filters MODULE
	${obs-c3-filters_SOURCES}
	${obs-c3-filters_config_HEADERS})
target_link_libraries(obs-c3-filters
	libobs)

install_obs_plugin_with_data(obs-c3-filters data)
