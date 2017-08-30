
add_subdirectory(bgtexture)

if(WIN32)
	add_subdirectory(win)
endif()

if(APPLE AND UNIX)
	add_subdirectory(unix)
endif()
