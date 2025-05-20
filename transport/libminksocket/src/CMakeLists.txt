cmake_minimum_required(VERSION 3.13)
project(libminksocket LANGUAGES C
                      VERSION 1.0.0
                      DESCRIPTION "minksocket library")

add_library(minksocket SHARED)
include(GNUInstallDirs)
set_target_properties(minksocket PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER sock/minkipc.h)
configure_file(minksocket.pc.in minksocket.pc @ONLY)
# target_include_directories(minksocket PRIVATE .)
install(TARGETS minksocket
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_BINARY_DIR}/minksocket.pc
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)


set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(minksocket
  PRIVATE MinkHeaders
  PRIVATE Threads::Threads
)

if(OFFTARGET)
  target_link_options(minksocket BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address)
endif()

target_sources(minksocket
  PUBLIC
    sock/LinkCredentials.h
    sock/Utils.h
    sock/VmOsal.h
    sock/fdwrapper.h
    sock/minkipc.h
    sock/minksocket.h
    sock/msforwarder.h
    sock/xtzdCredentials.h
)

target_sources(minksocket
  PRIVATE
    ext/itoa.c
    ext/strlcpy.c
    sock/CredentialsAdapter.c
    sock/CredentialsAdapter.h
    sock/LinkCredentials.c
    sock/Marshalling.c
    sock/Marshalling.h
    sock/ObjectTable.c
    sock/ObjectTable.h
    sock/ObjectTableMT.h
    sock/Profiling.c
    sock/Profiling.h
    sock/SockAgnostic.c
    sock/SockAgnostic.h
    sock/ThreadPool.c
    sock/ThreadPool.h
    sock/Utils.c
    sock/minkipc.c
    sock/minksocket.c
    sock/msforwarder.c
    sock/xtzdCredentials.c
)

option(VNDR_UNIXSOCK "Allow connecting over UNIX protocol" ON)
if(VNDR_UNIXSOCK)
  add_compile_definitions(VNDR_UNIXSOCK=1)
  target_sources(minksocket PRIVATE sock/SockUNIX.c)
endif()

option(PLATFORM_CUSTOMIZED "Configure to allow sharing memory with remote VMs" OFF)
if(PLATFORM_CUSTOMIZED)
  add_compile_definitions(PLATFORM_CUSTOMIZED=1)
  target_sources(minksocket PRIVATE vmosal/VmOsalCustomize.c)
else()
  target_sources(minksocket PRIVATE vmosal/VmOsalUnix.c)
endif()

target_include_directories(minksocket PUBLIC include/)
target_include_directories(minksocket PUBLIC kernel/)
set(IDL_DIR ${PROJECT_BINARY_DIR}/idl)
target_include_directories(minksocket PUBLIC ${IDL_DIR}/)
target_include_directories(minksocket PUBLIC sock/)

file(DOWNLOAD
  https://github.com/quic/mink-idl-compiler/releases/download/v0.2.0/idlc
  ${IDL_DIR}/minkidl
)

file(CHMOD ${IDL_DIR}/minkidl PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)

set(IDLS ITAccessPermissions)

# add the custom commands for each document
foreach(IDL ${IDLS})
  foreach( LANG "" "--cpp" )
    if (LANG)
      set(EXT hpp)
    else()
      set(EXT h)
    endif()
    foreach( VARIANT "" "--skel" )
      if (VARIANT)
        set(INVOKE _invoke)
      else()
        set(INVOKE "")
      endif()
      set(RESULT  ${IDL_DIR}/${IDL}${INVOKE}.${EXT} )
      set(INPUT   ${PROJECT_SOURCE_DIR}/idl/${IDL}.idl )
      # message("OUTPUT = ${RESULT}")
      add_custom_command(
        OUTPUT  ${RESULT}
        DEPENDS ${INPUT}
        COMMAND ${IDL_DIR}/minkidl ${INPUT} -o ${RESULT} ${LANG} ${VARIANT}
      )
      # build a list of all the results
      list(APPEND RESULTS  ${RESULT} )
    endforeach()
  endforeach()
endforeach()

target_sources(minksocket PRIVATE ${RESULTS})
