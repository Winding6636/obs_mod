project(obs-ffmpeg-audio-filters)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")

option(BUILD_FFMPEG_AUDIO_FILTERS "Build FFMPEG audio filters" ON)

if(BUILD_FFMPEG_AUDIO_FILTERS)
    if(MSVC)
	set(obs-ffmpeg-audio-filters_PLATFORM_DEPS
	    w32-pthreads)
    endif()

    find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)
    include_directories(${FFMPEG_INCLUDE_DIRS})

    set(obs-ffmpeg-audio-filters_HEADERS
	headers/obs-ffmpeg-audio-filters.h
	headers/obs-ffmpeg-audio-filters-compat.h)

    set(obs-ffmpeg-audio-filters_SOURCES
	obs-ffmpeg-audio-filters.c
	ffmpeg_acompressor.c)
	#ffmpeg_acrusher.c)
	#ffmpeg_adelay.c
	#ffmpeg_aecho.c
	#ffmpeg_aemphasis.c
	#ffmpeg_afftfilt.c
	#ffmpeg_agate.c
	#ffmpeg_alimiter.c
	#ffmpeg_allpass.c
	#ffmpeg_anequalizer.c
	#ffmpeg_aphaser.c
	#ffmpeg_apulsator.c
	#ffmpeg_bandpass.c
	#ffmpeg_bandreject.c
	#ffmpeg_bass.c
	#ffmpeg_chorus.c
	#ffmpeg_compand.c
	#ffmpeg_crystalizer.c
	#ffmpeg_dcshift.c
	#ffmpeg_dynaudnorm.c
	#ffmpeg_equalizer.c
	#ffmpeg_extrastereo.c
	#ffmpeg_firequalizer.c
	#ffmpeg_flanger.c
	#ffmpeg_highpass.c
	#ffmpeg_loudnorm.c
	#ffmpeg_lowpass.c
	#ffmpeg_rubberband.c
	#ffmpeg_sidechaincompress.c
	#ffmpeg_sidechaingate.c
	#ffmpeg_stereotools.c
	#ffmpeg_stereowiden.c
	#ffmpeg_treble.c
	#ffmpeg_tremolo.c
	#ffmpeg_vibrato.c)

    #if (BUILD_TESTS)
	#list (APPEND obs-ffmpeg-audio-filters_SOURCES
	    #ffmpeg_acopy.c
	    #ffmpeg_aeval.c
	    #ffmpeg_biquad.c)
	#message(STATUS "FFMPEG Audio Filter eval will be built.")
	#message(STATUS "FFMPEG Audio Copy will be built.")
	#message(STATUS "FFMPEG Audio Filter biquad will be built.")
    #endif()

    configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/headers/obs-ffmpeg-audio-filters-config.h.in"
	"${CMAKE_BINARY_DIR}/config/obs-ffmpeg-audio-filters-config.h")
    list (APPEND obs-ffmpeg-audio-filters_HEADERS
	"${CMAKE_BINARY_DIR}/config/obs-ffmpeg-audio-filters-config.h")

    add_library(obs-ffmpeg-audio-filters MODULE
	${obs-ffmpeg-audio-filters_HEADERS}
	${obs-ffmpeg-audio-filters_SOURCES})

    target_link_libraries(obs-ffmpeg-audio-filters
	libobs
	media-playback
	${obs-ffmpeg-audio-filters_PLATFORM_DEPS}
	${FFMPEG_LIBRARIES})

    install_obs_plugin_with_data(obs-ffmpeg-audio-filters data)
endif()
