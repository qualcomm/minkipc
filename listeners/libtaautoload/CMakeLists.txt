project(libtaautoload
	VERSION 1.0.0
	LANGUAGES CXX
)

# ''Build IDL Headers''.

set (IDLS
	idl/CRegisterTABufCBO
	idl/IClientEnv
	idl/ICredentials
	idl/IIO
	idl/IRegisterTABufCBO
	idl/IRequestTABuffer
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
        taautoload_MINKHEADERS ALL
        DEPENDS ${IDL_OUT_LIST}
)

# ''Source files''.

set(SRC
	src/TaAutoLoad.cpp
	src/taImageReader.cpp
	src/dmaMemPool.cpp
	src/CRequestTABuffer.cpp
)

add_library(taautoload SHARED ${SRC})

# The binary depends on the MINK-IDLs
add_dependencies(taautoload taautoload_MINKHEADERS)

# ''Library version''.

set_target_properties(taautoload PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ''Headers and dependencies''.

target_include_directories(taautoload
	PRIVATE idl
	PRIVATE src
	PRIVATE ${CMAKE_SOURCE_DIR}/listeners/listenercbo/include
)

target_link_libraries(taautoload
	PRIVATE minkadaptor
)

# ''Install targets''.

install(TARGETS taautoload
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
