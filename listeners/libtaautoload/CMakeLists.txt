project(libtaautoload
	VERSION 1.0.0
	LANGUAGES CXX
)

# ''Source files''.

set(SRC
	src/TaAutoLoad.cpp
	src/taImageReader.cpp
	src/dmaMemPool.cpp
	src/CRequestTABuffer.cpp
)

add_library(taautoload SHARED ${SRC})

# ''Library version''.

set_target_properties(taautoload PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ''Headers and dependencies''.

target_include_directories(taautoload
	PRIVATE idl
	PRIVATE src
)

target_link_libraries(taautoload
	PRIVATE minkadaptor
)

# ''Install targets''.

install(TARGETS taautoload
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
