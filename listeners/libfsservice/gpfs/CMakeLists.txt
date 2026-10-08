project(libgpfsservice
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

set(GPFS_SRC
	../../listenercbo/src/CListenerCBO.c
	../../listenercbo/src/memscpy.h
	../cmn/helper.c
	gpfs_msg.h
	gpfs_main.c
	gpfs.h
	gpfs.c
)

add_library(gpfsservice SHARED ${GPFS_SRC})

# ''Headers and dependencies''.

# The binary depends on the MINK-IDLs
add_dependencies(gpfsservice listenercbo_MINKHEADERS)

target_include_directories(gpfsservice
	PRIVATE ../cmn
	PRIVATE ../../listenercbo/include
	PRIVATE ../../listenercbo/idl
)

target_link_libraries(gpfsservice
	PRIVATE minkadaptor
)

if(${USE_GLIB})
	find_package(PkgConfig)
	pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0>=2.10)
	include_directories(BEFORE ${GLIB2_INCLUDE_DIRS})
	target_link_libraries(gpfsservice PRIVATE PkgConfig::GLIB2)
endif()

# ''Library version''.

set_target_properties(gpfsservice PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ''Install targets''.

install(TARGETS gpfsservice
	LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}"
)
