cmake_minimum_required(VERSION 3.19)
project(MinkHeaders)

# Include common path definitions before using them
include(GNUInstallDirs)

# INTERFACE libraries don't compile any code and (generally) are not copied into
# the build tree.
add_library(${PROJECT_NAME} INTERFACE)

# IDL files will be generated in the build tree
set(IDL_DIR ${PROJECT_BINARY_DIR}/idl)

# For building, include dirs which are part of source AND in the build tree
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${IDL_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

file(DOWNLOAD
  https://github.com/quic/mink-idl-compiler/releases/download/v0.2.0/idlc
  ${IDL_DIR}/minkidl
)

# Give minkidl compiler R/W/X permissions
file(CHMOD ${IDL_DIR}/minkidl PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)

set(IDLS IObject IOpener IModule ICredentials)

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
      set_property(SOURCE ${RESULT} PROPERTY GENERATED ON)
      # build a list of all the results
      list(APPEND RESULTS  ${RESULT} )
    endforeach()
  endforeach()
endforeach()

# Trigger the creation of files by installing the generated files into the build area.
target_sources(${PROJECT_NAME} PRIVATE ${RESULTS})
