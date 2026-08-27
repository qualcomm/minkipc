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

if(MINKTEEC_DIRECT_QCOMTEE)
	set(MINKTEEC_DIRECT_QCOMTEE_VALUE 1)
	set(pc_req_private "qcomtee")

	# Enable CMAKE_ASM_COMPILE
	enable_language(ASM)

	# FindQCOMTEE.cmake and FindQCBOR.cmake live with libminkadaptor
	list(APPEND CMAKE_MODULE_PATH
		"${CMAKE_SOURCE_DIR}/libminkadaptor/cmake"
	)

	set(QCOMTEE_DIR_HINT "" CACHE PATH "Hint path for QCOMTEE directory")
	find_package(QCOMTEE REQUIRED)
	if(NOT QCOMTEE_FOUND)
		message(FATAL_ERROR "QCOMTEE not found")
	endif()

	set(QCBOR_DIR_HINT "" CACHE PATH "Hint path for QCBOR directory")
	find_package(QCBOR REQUIRED)
	if(NOT QCBOR_FOUND)
		message(WARNING "QCBOR not found. Falling back to libcbor.")
	endif()
else()
	set(MINKTEEC_DIRECT_QCOMTEE_VALUE 0)
	set(pc_req_private "minkadaptor")
endif()

configure_file(include/minkteec_config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/minkteec_config.h @ONLY
)

# ''Build IDL Headers''.

if(NOT MINKTEEC_DIRECT_QCOMTEE)
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
endif()

# ''Source files''.

set(SRC
	src/tee_client_api.c
	src/mink_params.c
)

if(MINKTEEC_DIRECT_QCOMTEE)
	list(APPEND SRC
		src/gp_teec.c
		src/gp_wait.c
		src/transport/supplicant.c
	)

	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch32|arm)$")
		list(APPEND SRC src/transport/syscall32.S)
	else()
		list(APPEND SRC src/transport/syscall.S)
	endif()
else()
	list(APPEND SRC
		src/mink_teec.c
		src/CWait.c
	)
endif()

add_library(minkteec SHARED ${SRC})

# The binary depends on the MINK-IDLs, which the direct backend does not build.
if(NOT MINKTEEC_DIRECT_QCOMTEE)
	add_dependencies(minkteec minkteec_MINKHEADERS)
endif()

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
	PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
	PRIVATE idl
	PRIVATE src
)

if(MINKTEEC_DIRECT_QCOMTEE)
	target_include_directories(minkteec
		PRIVATE ${QCOMTEE_INCLUDE_DIRS}
	)

	# supplicant_start() and the mink_recv_* stubs are byte identical copies
	# of symbols libminkadaptor also exports. A process that ends up with
	# both libraries loaded - optee-client links minkadaptor directly and
	# minkteec through libckteec - would otherwise see two definitions of
	# each and let the dynamic linker's insertion order decide which one
	# wins. Hiding this library's copies removes the ambiguity; hidden
	# symbols still link fine within the library. Applied per source rather
	# than through C_VISIBILITY_PRESET so that the public TEEC_* API stays
	# exported. The assembly stubs carry their own .hidden directives, since
	# -fvisibility does not reach the assembler.
	set_source_files_properties(src/transport/supplicant.c
		PROPERTIES COMPILE_OPTIONS "-fvisibility=hidden"
	)

	if(QCBOR_FOUND)
		target_link_libraries(minkteec
			PRIVATE ${QCOMTEE_LIBRARIES}
			PRIVATE ${QCBOR_LIBRARIES}
			PRIVATE ${CMAKE_THREAD_LIBS_INIT}
		)
	else()
		target_link_libraries(minkteec
			PRIVATE ${QCOMTEE_LIBRARIES}
			PRIVATE cbor
			PRIVATE ${CMAKE_THREAD_LIBS_INIT}
		)
	endif()
else()
	target_link_libraries(minkteec
		PRIVATE minkadaptor
	)
endif()

# ''Install targets''.

install(TARGETS minkteec
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(DIRECTORY include
	DESTINATION ${CMAKE_INSTALL_PREFIX}
	PATTERN "minkteec_config.h.in" EXCLUDE
)

# The generated backend-selection header must travel with tee_client_api.h.
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/minkteec_config.h"
	DESTINATION "${CMAKE_INSTALL_PREFIX}/include"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/minkteec.pc"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)
