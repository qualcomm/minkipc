project(libminkadaptor
	VERSION 0.1.0
	LANGUAGES C ASM
)

if (BUILD_UNITTEST)
	add_subdirectory(tests/smcinvoke_client)
endif()

add_compile_options(
	-Wstrict-prototypes -Wmissing-prototypes -Wbad-function-cast
	-Wswitch-default
)

# ''Packages''.

find_package(Threads REQUIRED)
if(NOT THREADS_FOUND)
	message(FATAL_ERROR "Threads not found")
endif()

# Append modules directory.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(QCOMTEE_DIR_HINT "" CACHE PATH "Hint path for QCOMTEE directory")

find_package(QCOMTEE REQUIRED)
if(NOT QCOMTEE_FOUND)
	message(FATAL_ERROR "QCOMTEE not found")
endif()

include_directories(${QCOMTEE_INCLUDE_DIRS})

# ''Source files''.

set(SRC
	src/syscall.S
	src/supplicant.c
	src/mink_adaptor.c
)

add_library(minkadaptor SHARED ${SRC})

# ''Library pkg-config file and version''.

set(libminkadaptortgt minkadaptor)

configure_file(minkadaptor.pc.in minkadaptor.pc @ONLY)

set_target_properties(minkadaptor PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ''Headers and dependencies''.

target_include_directories(minkadaptor
	PUBLIC include
	PRIVATE src
)

target_link_libraries(minkadaptor
	PRIVATE MinkHeaders
	PRIVATE ${QCOMTEE_LIBRARIES}
	PRIVATE ${CMAKE_THREAD_LIBS_INIT}
)

# ''Install targets''.

install(TARGETS minkadaptor
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(DIRECTORY include
	DESTINATION ${CMAKE_INSTALL_PREFIX}
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/minkadaptor.pc"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)
