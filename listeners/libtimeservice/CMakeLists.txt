project(libtimeservice
	VERSION 1.0.0
	LANGUAGES C
)

# ''Packages''.

find_package(Threads REQUIRED)
if(NOT THREADS_FOUND)
        message(FATAL_ERROR "Threads not found")
endif()

add_compile_options(
	-Wstrict-prototypes -Wmissing-prototypes -Wbad-function-cast
)

# ''Source files''.

set(SRC
	../listenercbo/src/CListenerCBO.c
	../listenercbo/src/memscpy.h
	atime.c
	time_msg.h
)

add_library(timeservice SHARED ${SRC})

# ''Library version''.

set_target_properties(timeservice PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ''Headers and dependencies''.

# The binary depends on the MINK-IDLs
add_dependencies(timeservice listenercbo_MINKHEADERS)

target_include_directories(timeservice
	PRIVATE ../listenercbo/include
	PRIVATE ../listenercbo/idl
)

target_link_libraries(timeservice
	PRIVATE minkadaptor
)

# ''Install targets''.

install(TARGETS timeservice
	LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}"
)
