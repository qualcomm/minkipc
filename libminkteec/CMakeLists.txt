project(libminkteec
	VERSION 1.0.0
	LANGUAGES C
)

if (BUILD_UNITTEST)
	add_subdirectory(tests/gp_test_client)
endif()

add_compile_options(
	-Wstrict-prototypes -Wmissing-prototypes -Wbad-function-cast
)

# ''Packages''.

find_package(Threads REQUIRED)
if(NOT THREADS_FOUND)
	message(FATAL_ERROR "Threads not found")
endif()

# ''Build IDL Headers''.

set (IDLS
	idl/CGPAppClient
	idl/IClientEnv
	idl/ICredentials
	idl/IGPAppClient
	idl/IGPSession
	idl/IIO
	idl/IWait
)

foreach(IDL ${IDLS})
	foreach(VARIANT "" "--skel" )
		if (VARIANT)
			set(INVOKE _invoke)
		else()
			set(INVOKE "")
		endif()

		set(IDL_OUT ${CMAKE_CURRENT_SOURCE_DIR}/${IDL}${INVOKE}.h)
		set(INPUT ${CMAKE_CURRENT_SOURCE_DIR}/${IDL}.idl)

		add_custom_command(
			OUTPUT  ${IDL_OUT}
			DEPENDS ${INPUT}
			COMMAND ${MINKIDLC_BIN_DIR}/idlc ${INPUT} -o ${IDL_OUT} ${VARIANT}
		)

		list(APPEND IDL_OUT_LIST ${IDL_OUT})
	endforeach()
endforeach()

# A custom target to build the MINK-HEADERS which is always out of date.
add_custom_target(
        minkteec_MINKHEADERS ALL
        DEPENDS ${IDL_OUT_LIST}
)

# ''Source files''.

set(SRC
	src/tee_client_api.c
	src/mink_teec.c
	src/CWait.c
)

add_library(minkteec SHARED ${SRC})

# The binary depends on the MINK-IDLs
add_dependencies(minkteec minkteec_MINKHEADERS)

# ''Library pkg-config file and version''.

set(libminkteectgt minkteec)

configure_file(minkteec.pc.in minkteec.pc @ONLY)

set_target_properties(minkteec PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ''Headers and dependencies''.

target_include_directories(minkteec
	PUBLIC include
	PRIVATE idl
	PRIVATE src
)

target_link_libraries(minkteec
	PRIVATE minkadaptor
)

# ''Install targets''.

install(TARGETS minkteec
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(DIRECTORY include/
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/minkipc"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/minkteec.pc"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)
